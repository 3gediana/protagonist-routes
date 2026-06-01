#include "policy/PPO.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numeric>
#include <random>

namespace dp::policy {

namespace {
// decision_blueprint_nutrition §3a — warm-start architecture surgery.
// Loads a checkpoint whose encoder.weight may have FEWER input columns than
// the current net (e.g. a 567-dim bc_v9 into a 569-dim nutrition net). Every
// tensor is matched by name: equal-shape tensors are copied verbatim; the
// encoder first-layer weight is zero-padded (old columns copied into the
// leading slots, the new nutrition columns left at zero) so the loaded policy
// is mathematically identical to the original until PPO learns to use the new
// inputs. Hidden/output layers and the GRU copy unchanged. Any genuine
// mismatch is left at init and logged. Returns true if a pad was applied.
bool surgery_load_into(ActorCritic& module,
                       const std::string& path,
                       torch::Device device) {
    torch::NoGradGuard ng;
    torch::serialize::InputArchive archive;
    archive.load_from(path);
    int copied = 0, padded = 0, missing = 0, mism = 0;
    auto handle = [&](const std::string& name, torch::Tensor dst, bool is_buf) {
        torch::Tensor src;
        if (!archive.try_read(name, src, is_buf)) { ++missing; return; }
        src = src.to(dst.device());
        if (src.sizes() == dst.sizes()) { dst.copy_(src); ++copied; return; }
        if (name == "encoder.weight" && src.dim() == 2 && dst.dim() == 2 &&
            src.size(0) == dst.size(0) && src.size(1) < dst.size(1)) {
            dst.zero_();
            dst.narrow(1, 0, src.size(1)).copy_(src);
            ++padded;
            spdlog::info("warmstart surgery: encoder.weight [{}x{}] -> [{}x{}], "
                         "cols {}..{} zero-init (nutrition)",
                         src.size(0), src.size(1), dst.size(0), dst.size(1),
                         src.size(1), dst.size(1) - 1);
            return;
        }
        ++mism;
        spdlog::warn("warmstart surgery: '{}' shape mismatch, left at init", name);
    };
    for (auto& p : module->named_parameters()) handle(p.key(), p.value(), false);
    for (auto& b : module->named_buffers())    handle(b.key(), b.value(), true);
    module->to(device);
    spdlog::info("warmstart surgery done: copied={} padded={} missing={} mismatch={}",
                 copied, padded, missing, mism);
    return padded > 0;
}
}  // namespace

// ============================================================================
// Network
// ============================================================================
ActorCriticImpl::ActorCriticImpl() {
    encoder     = register_module("encoder",
                                  torch::nn::Linear(OBS_DIM, HIDDEN));
    gru         = register_module("gru",
                                  torch::nn::GRUCell(HIDDEN, HIDDEN));
    cont_mean   = register_module("cont_mean",
                                  torch::nn::Linear(HIDDEN, CONT_DIM));
    disc_logits = register_module("disc_logits",
                                  torch::nn::Linear(HIDDEN, DISC_CATS));  // 17-way categorical
    value       = register_module("value",
                                  torch::nn::Linear(HIDDEN, 1));

    // log_std initialised to log(0.5) ≈ -0.693 (moderate exploration).
    log_std = register_parameter(
        "log_std",
        torch::full({CONT_DIM}, -0.693f, torch::kFloat32));

    // Orthogonal init for actor heads, smaller for value head (PPO standard).
    auto orthogonal_ = [](torch::nn::Linear& l, float gain) {
        torch::NoGradGuard g;
        torch::nn::init::orthogonal_(l->weight, gain);
        torch::nn::init::zeros_(l->bias);
    };
    orthogonal_(encoder,     std::sqrt(2.0f));
    orthogonal_(cont_mean,   0.01f);   // small final-layer
    orthogonal_(disc_logits, 0.01f);
    orthogonal_(value,       1.0f);
    // GRUCell weights are initialised by libtorch with reasonable defaults
    // (uniform Kaiming-ish). We leave them alone.
}

std::tuple<torch::Tensor, torch::Tensor, torch::Tensor,
           torch::Tensor, torch::Tensor>
ActorCriticImpl::forward_step(const torch::Tensor& obs,
                              const torch::Tensor& h_in) {
    auto enc = torch::relu(encoder(obs));               // [B, HIDDEN]
    auto h   = gru(enc, h_in);                          // [B, HIDDEN]
    auto cm  = cont_mean(h);                            // [B, CONT_DIM]
    auto cs  = log_std.expand({obs.size(0), CONT_DIM}); // [B, CONT_DIM]
    auto dl  = disc_logits(h);                          // [B, DISC_DIM]
    auto v   = value(h).squeeze(-1);                    // [B]
    return {cm, cs, dl, v, h};
}

// ============================================================================
// PPO
// ============================================================================
PPO::PPO(const PPOConfig& cfg)
    : cfg_(cfg),
      device_((cfg.use_cuda && torch::cuda::is_available())
              ? torch::Device(torch::kCUDA, 0)
              : torch::Device(torch::kCPU)),
      net_(ActorCritic())
{
    torch::manual_seed(cfg.seed);
    net_->to(device_);

    opt_ = std::make_shared<torch::optim::Adam>(
        net_->parameters(),
        torch::optim::AdamOptions(cfg.lr));

    hidden_ = torch::zeros({cfg.n_envs, ActorCriticImpl::HIDDEN},
                           torch::TensorOptions().dtype(torch::kFloat32)
                                                 .device(device_));

    // decision_round_17 / decision_blueprint_nutrition: action masking.
    //   mask_atk_fire  -> mask BOTH idx2(attack) and idx17(fire)  (old world)
    //   mask_fire_only -> mask ONLY idx17(fire); attack stays open (hunting)
    // mask_atk_fire takes precedence if both are set. apply_disc_mask() builds
    // the actual additive mask each call via functional ops (the round_17
    // CUDA-safe path); disc_mask_ is kept only as a static reference tensor.
    if (cfg_.mask_atk_fire || cfg_.mask_fire_only) {
        mask_enabled_ = true;
        mask_attack_  = cfg_.mask_atk_fire;  // fire-only => don't mask attack
        spdlog::info("action mask ON ({})",
                     mask_attack_ ? "idx2 attack + idx17 fire -> -inf"
                                  : "idx17 fire only (attack UNMASKED for hunting)");
    }

    spdlog::info("PPO init on device={}, params={}, obs_dim={}",
                 (device_.is_cuda() ? "CUDA" : "CPU"),
                 [&]() {
                     int64_t n = 0;
                     for (auto& p : net_->parameters()) n += p.numel();
                     return n;
                 }(),
                 ActorCriticImpl::OBS_DIM);
}

void PPO::reset_hidden() {
    torch::NoGradGuard ng;
    hidden_.zero_();
}

std::vector<float> PPO::current_hidden() const {
    auto cpu = hidden_.detach().to(torch::kCPU).contiguous();
    return std::vector<float>(cpu.data_ptr<float>(),
                              cpu.data_ptr<float>() + ActorCriticImpl::HIDDEN);
}

dp::agent::AgentAction PPO::sample_action(
    const dp::agent::ObservationBuilder::Vector& obs,
    std::vector<float>& out_cont,
    int&                out_disc_idx,
    float& out_log_prob,
    float& out_value)
{
    constexpr int O = ActorCriticImpl::OBS_DIM;
    constexpr int C = ActorCriticImpl::CONT_DIM;

    torch::NoGradGuard ng;
    auto t_obs = torch::from_blob(
        const_cast<float*>(obs.data()), {1, O}, torch::kFloat32)
        .clone()
        .to(device_);
    net_->eval();
    auto [cm, cs, dl, v, h_out] = net_->forward_step(t_obs, hidden_);
    hidden_ = h_out.detach();

    auto std_  = cs.exp();                       // [1, C]
    // D-101: deterministic eval uses the distribution mode (mean cont,
    // argmax disc) so a cloned/finetuned policy is measured without the
    // exploration noise. Training/rollout keeps the stochastic sampling.
    auto eps   = deterministic_ ? torch::zeros_like(cm) : torch::randn_like(cm);
    auto cont  = cm + std_ * eps;                // reparam sample
    auto cont_log_prob =
        (-0.5f * ((cont - cm) / std_).pow(2)
         - cs
         - 0.5f * std::log(2.0f * static_cast<float>(M_PI)))
            .sum(1);                             // [1]

    // Discrete: 17-way categorical (16 triggers + NOOP). Multinomial
    // sample one index, log_prob = log p(a). Replaces independent
    // Bernoulli heads to break the "press every button" collapse.
    auto dlm         = apply_disc_mask(dl);
    auto log_probs_d = torch::log_softmax(dlm, /*dim=*/1);   // [1, DISC_CATS]
    auto probs_d     = log_probs_d.exp();                    // [1, 17]
    auto idx         = deterministic_
                         ? probs_d.argmax(1, /*keepdim=*/true)
                         : probs_d.multinomial(1, /*replacement=*/true);  // [1, 1]
    auto disc_log_prob = log_probs_d.gather(1, idx).squeeze(1);       // [1]

    auto log_prob = cont_log_prob + disc_log_prob;

    auto cont_cpu = cont.to(torch::kCPU).contiguous();
    out_cont.assign(cont_cpu.data_ptr<float>(),
                    cont_cpu.data_ptr<float>() + C);
    out_disc_idx = static_cast<int>(idx.to(torch::kCPU).item<int64_t>());
    out_log_prob = log_prob.item<float>();
    out_value    = v.item<float>();

    // Map cont -> AgentAction. move/yaw can be unbounded; CapsuleAgent
    // normalises move and accepts any yaw_rate (we still scale yaw_rate
    // down so a freshly-init policy doesn't spin like a top).
    dp::agent::AgentAction a{};
    a.move_x   = std::tanh(out_cont[0]);
    a.move_y   = std::tanh(out_cont[1]);
    a.yaw_rate = std::tanh(out_cont[2]) * 3.0f;    // up to 3 rad/s
    // Categorical fan-out: exactly one trigger is set, or NOOP (idx=16).
    switch (out_disc_idx) {
        case  0: a.eat                  = true; break;
        case  1: a.drink                = true; break;
        case  2: a.attack               = true; break;
        case  3: a.collect              = true; break;
        case  4: a.place_shelter        = true; break;
        case  5: a.craft_spear          = true; break;
        case  6: a.sleep                = true; break;
        case  7: a.craft_grass_dress    = true; break;
        case  8: a.craft_fur_cloak      = true; break;
        case  9: a.wear_clothes         = true; break;
        case 10: a.place_blueprint      = true; break;
        case 11: a.cycle_building_type  = true; break;
        case 12: a.deposit_to_site      = true; break;
        case 13: a.plant_seed           = true; break;
        case 14: a.water_plot           = true; break;
        case 15: a.feed_tame            = true; break;
        case 17: a.tend_fire            = true; break;  // D-112 fire (TEND_FIRE_IDX)
        default: /* NOOP (16) */ break;
    }
    return a;
}

// ============================================================================
// D-078: VecEnv batched sampling.
// Single forward pass on [N, OBS_DIM] -> sample N actions. hidden_ is
// updated row-wise; out vectors are resized to N. Sequential CPU pack
// of N obs into a contiguous buffer keeps allocation cheap. For N=8
// this lifts GPU per-step utilisation since N parallel env steps are
// rolled into one batched policy forward pass.
// ============================================================================
void PPO::sample_actions_batch(
    const std::vector<dp::agent::ObservationBuilder::Vector>& obs_batch,
    std::vector<dp::agent::AgentAction>& actions_out,
    std::vector<std::vector<float>>& cont_acts_out,
    std::vector<int>& disc_idxs_out,
    std::vector<float>& log_probs_out,
    std::vector<float>& values_out)
{
    constexpr int O = ActorCriticImpl::OBS_DIM;
    constexpr int C = ActorCriticImpl::CONT_DIM;
    const int N = static_cast<int>(obs_batch.size());
    if (N == 0) return;

    actions_out.assign(N, dp::agent::AgentAction{});
    cont_acts_out.assign(N, std::vector<float>(C, 0.0f));
    disc_idxs_out.assign(N, 0);
    log_probs_out.assign(N, 0.0f);
    values_out.assign(N, 0.0f);

    // Pack [N, O] flat
    std::vector<float> flat(static_cast<size_t>(N) * O);
    for (int i = 0; i < N; ++i) {
        std::copy(obs_batch[i].begin(), obs_batch[i].end(),
                  flat.data() + static_cast<size_t>(i) * O);
    }
    torch::NoGradGuard ng;
    auto t_obs = torch::from_blob(flat.data(), {N, O}, torch::kFloat32)
                     .clone().to(device_);
    net_->eval();
    auto [cm, cs, dl, v, h_out] = net_->forward_step(t_obs, hidden_);
    hidden_ = h_out.detach();

    auto std_ = cs.exp();
    auto eps  = torch::randn_like(cm);
    auto cont = cm + std_ * eps;
    auto cont_log_prob =
        (-0.5f * ((cont - cm) / std_).pow(2)
         - cs
         - 0.5f * std::log(2.0f * static_cast<float>(M_PI)))
            .sum(1);  // [N]
    auto dlm         = apply_disc_mask(dl);
    auto log_probs_d = torch::log_softmax(dlm, /*dim=*/1);  // [N, DISC_CATS]
    auto probs_d     = log_probs_d.exp();
    auto idx         = probs_d.multinomial(1, /*replacement=*/true);  // [N, 1]
    auto disc_log_prob = log_probs_d.gather(1, idx).squeeze(1);       // [N]
    auto log_prob = cont_log_prob + disc_log_prob;

    auto cont_cpu = cont.to(torch::kCPU).contiguous();
    auto idx_cpu  = idx.to(torch::kCPU).contiguous();
    auto lp_cpu   = log_prob.to(torch::kCPU).contiguous();
    auto v_cpu    = v.to(torch::kCPU).contiguous();

    const float* cont_p = cont_cpu.data_ptr<float>();
    const int64_t* idx_p = idx_cpu.data_ptr<int64_t>();
    const float* lp_p   = lp_cpu.data_ptr<float>();
    const float* v_p    = v_cpu.data_ptr<float>();

    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < C; ++j) cont_acts_out[i][j] = cont_p[i*C + j];
        disc_idxs_out[i] = static_cast<int>(idx_p[i]);
        log_probs_out[i] = lp_p[i];
        values_out[i]    = v_p[i];

        dp::agent::AgentAction& a = actions_out[i];
        a.move_x   = std::tanh(cont_acts_out[i][0]);
        a.move_y   = std::tanh(cont_acts_out[i][1]);
        a.yaw_rate = std::tanh(cont_acts_out[i][2]) * 3.0f;
        switch (disc_idxs_out[i]) {
            case  0: a.eat                  = true; break;
            case  1: a.drink                = true; break;
            case  2: a.attack               = true; break;
            case  3: a.collect              = true; break;
            case  4: a.place_shelter        = true; break;
            case  5: a.craft_spear          = true; break;
            case  6: a.sleep                = true; break;
            case  7: a.craft_grass_dress    = true; break;
            case  8: a.craft_fur_cloak      = true; break;
            case  9: a.wear_clothes         = true; break;
            case 10: a.place_blueprint      = true; break;
            case 11: a.cycle_building_type  = true; break;
            case 12: a.deposit_to_site      = true; break;
            case 13: a.plant_seed           = true; break;
            case 14: a.water_plot           = true; break;
            case 15: a.feed_tame            = true; break;
            case 17: a.tend_fire            = true; break;  // D-112 fire (TEND_FIRE_IDX)
            default: /* NOOP (16) */ break;
        }
    }
}

std::vector<float> PPO::current_hidden_at(int env_idx) const {
    auto row = hidden_.index({env_idx}).detach().to(torch::kCPU).contiguous();
    return std::vector<float>(row.data_ptr<float>(),
                              row.data_ptr<float>() + ActorCriticImpl::HIDDEN);
}

void PPO::reset_hidden_at(int env_idx) {
    torch::NoGradGuard ng;
    hidden_.index({env_idx}).zero_();
}

PPO::UpdateStats PPO::update(const std::vector<Transition>& rollout,
                             float last_value, bool critic_only)
{
    constexpr int O = ActorCriticImpl::OBS_DIM;
    constexpr int C = ActorCriticImpl::CONT_DIM;
    constexpr int H = ActorCriticImpl::HIDDEN;

    const int N = static_cast<int>(rollout.size());
    if (N == 0) return {};

    // ---------- pack rollout into flat host tensors ----------
    // We use "stale-hidden" PPO (CleanRL-style): the hidden state passed
    // to forward during update is the one cached at rollout time. This
    // is slightly off-policy but is the simplest correct PPO+GRU.
    std::vector<float>   obs_flat;   obs_flat.reserve(N * O);
    std::vector<float>   cont_flat;  cont_flat.reserve(N * C);
    std::vector<int64_t> disc_idx(N);
    std::vector<float>   hin_flat;   hin_flat.reserve(N * H);
    std::vector<float>   rew(N), val(N), lp(N);
    std::vector<float>   done(N);
    for (int i = 0; i < N; ++i) {
        const auto& tr = rollout[i];
        obs_flat.insert(obs_flat.end(), tr.obs.begin(), tr.obs.end());
        cont_flat.insert(cont_flat.end(), tr.cont_act.begin(), tr.cont_act.end());
        disc_idx[i] = static_cast<int64_t>(tr.disc_act);
        hin_flat.insert(hin_flat.end(), tr.h_in.begin(), tr.h_in.end());
        rew[i]  = tr.reward;
        val[i]  = tr.value;
        lp[i]   = tr.log_prob;
        done[i] = tr.done ? 1.0f : 0.0f;
    }

    // ---------- GAE-λ on host ----------
    std::vector<float> adv(N, 0.0f), ret(N, 0.0f);
    float gae = 0.0f;
    for (int i = N - 1; i >= 0; --i) {
        float next_v = (i == N - 1) ? last_value : val[i + 1];
        float next_nonterm = (i == N - 1)
            ? (rollout[N - 1].done ? 0.0f : 1.0f)
            : (rollout[i].done     ? 0.0f : 1.0f);
        float delta = rew[i] + cfg_.gamma * next_v * next_nonterm - val[i];
        gae   = delta + cfg_.gamma * cfg_.lambda_ * next_nonterm * gae;
        adv[i] = gae;
        ret[i] = adv[i] + val[i];
    }
    // Normalise advantages
    {
        double mean = std::accumulate(adv.begin(), adv.end(), 0.0) / N;
        double sq   = 0.0;
        for (auto a : adv) sq += (a - mean) * (a - mean);
        float std_ = static_cast<float>(std::sqrt(sq / std::max(1, N - 1)) + 1e-8);
        for (auto& a : adv) a = (a - static_cast<float>(mean)) / std_;
    }

    auto t_obs   = torch::from_blob(obs_flat.data(),  {N, O}, torch::kFloat32).clone().to(device_);
    auto t_cont  = torch::from_blob(cont_flat.data(), {N, C}, torch::kFloat32).clone().to(device_);
    auto t_disc  = torch::from_blob(disc_idx.data(),  {N},    torch::kInt64).clone().to(device_);
    auto t_hin   = torch::from_blob(hin_flat.data(),  {N, H}, torch::kFloat32).clone().to(device_);
    auto t_lpold = torch::from_blob(lp.data(),        {N},    torch::kFloat32).clone().to(device_);
    auto t_adv   = torch::from_blob(adv.data(),       {N},    torch::kFloat32).clone().to(device_);
    auto t_ret   = torch::from_blob(ret.data(),       {N},    torch::kFloat32).clone().to(device_);

    // ---------- PPO update ----------
    UpdateStats stats;
    int total_minibatches = 0;
    std::vector<int> idx(N);
    std::iota(idx.begin(), idx.end(), 0);
    std::mt19937 rng(static_cast<uint32_t>(cfg_.seed) + N);

    net_->train();
    bool kl_stop = false;
    for (int e = 0; e < cfg_.epochs; ++e) {
        std::shuffle(idx.begin(), idx.end(), rng);
        for (int mb = 0; mb < N; mb += cfg_.minibatch_size) {
            int end = std::min(mb + cfg_.minibatch_size, N);
            std::vector<int64_t> sub(idx.begin() + mb, idx.begin() + end);
            auto t_idx = torch::from_blob(
                sub.data(), {static_cast<int64_t>(sub.size())},
                torch::kInt64).clone().to(device_);

            auto b_obs   = t_obs.index_select(0,   t_idx);
            auto b_cont  = t_cont.index_select(0,  t_idx);
            auto b_disc  = t_disc.index_select(0,  t_idx);
            auto b_hin   = t_hin.index_select(0,   t_idx);
            auto b_lpold = t_lpold.index_select(0, t_idx);
            auto b_adv   = t_adv.index_select(0,   t_idx);
            auto b_ret   = t_ret.index_select(0,   t_idx);

            auto [cm, cs, dl0, v, h_out] = net_->forward_step(b_obs, b_hin);
            auto dl   = apply_disc_mask(dl0);
            auto std_ = cs.exp();
            auto cont_lp =
                (-0.5f * ((b_cont - cm) / std_).pow(2)
                 - cs
                 - 0.5f * std::log(2.0f * static_cast<float>(M_PI))).sum(1);
            // Categorical log-prob: log_softmax(dl)[chosen_idx]
            auto log_p_d = torch::log_softmax(dl, /*dim=*/1);   // [B, 17]
            auto disc_lp = log_p_d.gather(1, b_disc.unsqueeze(1)).squeeze(1);  // [B]
            auto lp_new  = cont_lp + disc_lp;

            // Continuous Gaussian entropy + Categorical entropy.
            auto cont_ent = (cs + 0.5f
                             * std::log(2.0f * static_cast<float>(M_PI) * static_cast<float>(M_E)))
                            .sum(1);
            auto p_d      = log_p_d.exp();
            auto disc_ent = -(p_d * log_p_d).sum(1);            // [B]
            auto entropy  = (cont_ent + disc_ent).mean();

            auto ratio = (lp_new - b_lpold).exp();
            auto unclipped = ratio * b_adv;
            auto clipped   = ratio.clamp(1.0f - cfg_.clip_eps, 1.0f + cfg_.clip_eps) * b_adv;
            auto policy_loss = -torch::min(unclipped, clipped).mean();
            auto value_loss  = (v - b_ret).pow(2).mean();

            // decision_round_17 (S4 PPO C): KL-to-reference (pi || pi_ref=bc_v9)
            // anchor keeps the finetuned policy inside the BC basin so the
            // night PBRS can refine behaviour without the cloned policy drift
            // (round_17: attack 0->spam, nShel collapse). Analytic KL =
            // Gaussian(cont)+Categorical(disc); ref outputs are grad-free
            // constants so the gradient pulls CURRENT toward the reference.
            torch::Tensor kl_ref;
            if (has_ref_ && !critic_only && cfg_.kl_ref_coef > 0.0f) {
                torch::Tensor r_cm, r_cs, r_dl;
                {
                    torch::NoGradGuard g;
                    auto rt = ref_net_->forward_step(b_obs, b_hin);
                    r_cm = std::get<0>(rt);
                    r_cs = std::get<1>(rt);
                    r_dl = apply_disc_mask(std::get<2>(rt));
                }
                auto kl_cont = ((r_cs - cs)
                    + (torch::exp(2.0f * cs) + (cm - r_cm).pow(2))
                        / (2.0f * torch::exp(2.0f * r_cs))
                    - 0.5f).sum(1);
                auto r_log_p_d = torch::log_softmax(r_dl, /*dim=*/1);
                auto kl_disc = (p_d * (log_p_d - r_log_p_d)).sum(1);
                kl_ref = (kl_cont + kl_disc).mean();
            }

            // D-101 critic warmup: value-only loss. value_loss does not flow
            // through the policy heads, so the BC policy is frozen exactly
            // while the critic learns V(s) for it.
            auto loss = critic_only
                ? (cfg_.vf_coef * value_loss)
                : (policy_loss + cfg_.vf_coef * value_loss
                   - cfg_.ent_coef * entropy);
            if (kl_ref.defined()) loss = loss + cfg_.kl_ref_coef * kl_ref;

            opt_->zero_grad();
            loss.backward();
            torch::nn::utils::clip_grad_norm_(net_->parameters(),
                                              cfg_.max_grad_norm);
            opt_->step();

            stats.policy_loss += policy_loss.item<float>();
            stats.value_loss  += value_loss.item<float>();
            stats.entropy     += entropy.item<float>();
            stats.approx_kl   += (b_lpold - lp_new).mean().item<float>();
            stats.clip_frac   += ((ratio - 1.0f).abs() > cfg_.clip_eps)
                                   .to(torch::kFloat32)
                                   .mean()
                                   .item<float>();
            ++total_minibatches;
            // decision_round_17: target-KL early stop (CleanRL k3 estimator,
            // always >=0). Second guardrail vs cumulative drift out of basin.
            if (cfg_.target_kl > 0.0f) {
                float kl_mb = ((ratio - 1.0f) - (lp_new - b_lpold))
                                  .mean().item<float>();
                if (kl_mb > cfg_.target_kl) { kl_stop = true; break; }
            }
        }
        if (kl_stop) break;
    }
    if (total_minibatches > 0) {
        stats.policy_loss /= total_minibatches;
        stats.value_loss  /= total_minibatches;
        stats.entropy     /= total_minibatches;
        stats.approx_kl   /= total_minibatches;
        stats.clip_frac   /= total_minibatches;
    }
    return stats;
}

void PPO::save(const std::string& path) const {
    // Two-archive scheme: one file for the module, a sibling file for
    // the optimizer. libtorch's serialize::OutputArchive doesn't easily
    // mix modules + optimizers inside the same archive, and splitting
    // them is what most reference implementations do.
    torch::save(net_, path);
    torch::save(*opt_, path + ".opt");
    spdlog::info("PPO checkpoint saved: {}", path);
}

void PPO::load(const std::string& path) {
    bool did_surgery = false;
    try {
        torch::load(net_, path);
    } catch (const std::exception& e) {
        // decision_blueprint_nutrition §3a: a 567-dim bc_v9 won't load into a
        // 569-dim nutrition net by shape; fall back to first-layer surgery.
        spdlog::warn("plain load failed ({}); attempting warm-start surgery "
                     "(obs-dim expansion)", e.what());
        did_surgery = surgery_load_into(net_, path, device_);
    }
    net_->to(device_);
    // Optimizer file is optional - if missing we keep a fresh one (this
    // is fine for evaluation-only loads). After surgery the param shapes no
    // longer match the saved Adam state, so we MUST keep a fresh optimizer.
    std::string opt_path = path + ".opt";
    if (did_surgery) {
        spdlog::info("PPO warm-started via surgery from {} (fresh optimizer)", path);
    } else if (std::filesystem::exists(opt_path)) {
        torch::load(*opt_, opt_path);
        spdlog::info("PPO checkpoint + optimizer loaded: {}", path);
    } else {
        spdlog::info("PPO checkpoint loaded (no optimizer file): {}", path);
    }
}

void PPO::load_reference(const std::string& path) {
    ref_net_ = ActorCritic();
    try {
        torch::load(ref_net_, path);
    } catch (const std::exception& e) {
        // §3a: KL anchor must be the SAME bc_v9 surgically expanded to 569 so
        // its outputs match the warm-started policy exactly at step 0.
        spdlog::warn("KL-ref plain load failed ({}); warm-start surgery", e.what());
        surgery_load_into(ref_net_, path, device_);
    }
    ref_net_->to(device_);
    ref_net_->eval();
    for (auto& p : ref_net_->parameters()) p.set_requires_grad(false);
    has_ref_ = true;
    spdlog::info("decision_round_17: KL-reference policy loaded from {}", path);
}

}  // namespace dp::policy
