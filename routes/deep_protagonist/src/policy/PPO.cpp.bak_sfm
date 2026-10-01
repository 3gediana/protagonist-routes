#include "policy/PPO.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <numeric>
#include <random>

namespace dp::policy {

namespace {
// decision_blueprint_nutrition §3a — first-layer warm-start surgery.
//
// torch::load(module, path) ADOPTS the checkpoint's tensor shapes (set_data)
// and so loads every bc_v9 weight correctly — including encoder.weight, which
// comes back at the OLD input width [HIDDEN, 567]. Every other tensor (GRU,
// heads, log_std) is dimension-unchanged and already correct. The only fix
// needed is to widen that first layer to the new obs width: copy the old
// columns into the leading slots and zero-init the trailing nutrition columns,
// so the warm-started policy is mathematically identical to bc_v9 until PPO
// learns to use the 2 new inputs. Returns true iff an expansion was applied.
bool expand_encoder_if_needed(ActorCritic& module) {
    torch::NoGradGuard ng;
    auto enc_w = module->encoder->weight;          // [HIDDEN, in]
    const int64_t cur_in = enc_w.size(1);
    const int64_t want_in = ActorCriticImpl::OBS_DIM;
    if (cur_in == want_in) return false;           // already correct width
    auto padded = torch::zeros({enc_w.size(0), want_in}, enc_w.options());
    const int64_t keep = std::min(cur_in, want_in);
    padded.narrow(1, 0, keep).copy_(enc_w.narrow(1, 0, keep));
    module->encoder->weight.set_data(padded);      // adopt widened [HIDDEN, 569]
    spdlog::info("warmstart surgery: encoder.weight [{}x{}] -> [{}x{}], "
                 "cols {}..{} zero-init (nutrition)",
                 enc_w.size(0), cur_in, enc_w.size(0), want_in,
                 keep, want_in - 1);
    return true;
}

// D-122 one-pot tech-tree — discrete-head warm-start surgery.
//
// torch::load adopts the checkpoint's disc_logits at its OLD class count
// (e.g. 18). We widen the categorical head to DISC_CATS (23): copy the old
// rows (weight) / entries (bias) into the leading slots and ZERO-init the 5
// new tech-tree rows (18..22). Zero rows => logit 0 for the new classes; they
// are ALSO masked to -inf by apply_disc_mask until their stage unlocks, so the
// warm-started champion is byte-identical to the pre-tech-tree policy on day 1
// and PPO grows the new rows from zero only after the stage opens. Returns
// true iff an expansion was applied.
bool expand_disc_logits_if_needed(ActorCritic& module) {
    torch::NoGradGuard ng;
    auto w = module->disc_logits->weight;          // [cats, HIDDEN]
    auto b = module->disc_logits->bias;            // [cats]
    const int64_t cur  = w.size(0);
    const int64_t want = ActorCriticImpl::DISC_CATS;
    if (cur == want) return false;                 // already correct height
    auto nw = torch::zeros({want, w.size(1)}, w.options());
    auto nb = torch::zeros({want}, b.options());
    const int64_t keep = std::min(cur, want);
    nw.narrow(0, 0, keep).copy_(w.narrow(0, 0, keep));
    nb.narrow(0, 0, keep).copy_(b.narrow(0, 0, keep));
    module->disc_logits->weight.set_data(nw);
    module->disc_logits->bias.set_data(nb);
    spdlog::info("warmstart surgery: disc_logits.weight [{}x{}] -> [{}x{}] "
                 "+ bias [{}]->[{}], rows {}..{} zero-init (tech-tree)",
                 cur, w.size(1), want, w.size(1), cur, want,
                 keep, want - 1);
    return true;
}

// D-123 SF: tolerant module loader. A pre-SF checkpoint has no `successor.*`
// parameters; plain torch::load reads every module param by name and THROWS on
// the first missing key, which would abort warm-starting an old champion (and
// the bc_v9 KL reference). Try the proven happy path first; on failure fall
// back to a selective archive read that adopts every key PRESENT (same set_data
// shape-adoption torch::load uses, so encoder/disc surgery still applies) and
// leaves missing ones (the ψ head) at their fresh init -- ψ is then converged
// by an sf_warmup phase (mirrors the D-101 critic-only warmup).
void load_module_tolerant(ActorCritic& module, const std::string& path) {
    try {
        torch::load(module, path);
        return;
    } catch (const std::exception& e) {
        spdlog::warn("torch::load('{}') failed ({}); selective load "
                     "(missing keys kept at fresh init)", path, e.what());
    }
    torch::serialize::InputArchive ar;
    ar.load_from(path);
    torch::NoGradGuard ng;
    for (auto& p : module->named_parameters()) {
        torch::Tensor t;
        if (ar.try_read(p.key(), t, /*is_buffer=*/false)) p.value().set_data(t);
    }
    for (auto& b : module->named_buffers()) {
        torch::Tensor t;
        if (ar.try_read(b.key(), t, /*is_buffer=*/true)) b.value().set_data(t);
    }
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
    // D-123 SF: successor-feature head ψ (HIDDEN -> SF_DIM). Sibling of value.
    successor   = register_module("successor",
                                  torch::nn::Linear(HIDDEN, SF_DIM));

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
    orthogonal_(successor,   1.0f);    // D-123 SF: same as value head
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

// D-123 SF: ψ(s) = linear read-out of the hidden state -> [B, SF_DIM].
// No internal detach: training passes h.detach() so only `successor` learns;
// self-tests may pass a grad-enabled h to exercise the head directly.
torch::Tensor ActorCriticImpl::psi(const torch::Tensor& h) {
    return successor(h);                                // [B, SF_DIM]
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

    // D-122 one-pot: lock every reserved tech-tree class whose curriculum gate
    // is OFF. These columns are masked to -inf each step (apply_disc_mask) so a
    // warm-started champion (whose new logit rows are also zero-init) is
    // behaviourally identical until the stage that teaches the action opens.
    if (!cfg_.allow_cook)       locked_cats_.push_back(ActorCriticImpl::COOK_IDX);
    if (!cfg_.allow_mine)       locked_cats_.push_back(ActorCriticImpl::MINE_IDX);
    if (!cfg_.allow_craft_axe)  locked_cats_.push_back(ActorCriticImpl::CRAFT_AXE_IDX);
    if (!cfg_.allow_craft_pick) locked_cats_.push_back(ActorCriticImpl::CRAFT_PICKAXE_IDX);
    if (!cfg_.allow_monument)   locked_cats_.push_back(ActorCriticImpl::BUILD_MONUMENT_IDX);
    if (!locked_cats_.empty())
        spdlog::info("D-122 one-pot: {} reserved tech-tree class(es) masked "
                     "(locked until stage unlock)", locked_cats_.size());

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
        case 18: a.cook                 = true; break;  // D-122 tech-tree (COOK_IDX)
        case 19: a.mine                 = true; break;  // D-122 tech-tree (MINE_IDX)
        case 20: a.craft_axe            = true; break;  // D-122 tech-tree (CRAFT_AXE_IDX)
        case 21: a.craft_pickaxe        = true; break;  // D-122 tech-tree (CRAFT_PICKAXE_IDX)
        case 22: a.build_monument       = true; break;  // D-122 tech-tree (BUILD_MONUMENT_IDX)
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
            case 18: a.cook                 = true; break;  // D-122 tech-tree (COOK_IDX)
            case 19: a.mine                 = true; break;  // D-122 tech-tree (MINE_IDX)
            case 20: a.craft_axe            = true; break;  // D-122 tech-tree (CRAFT_AXE_IDX)
            case 21: a.craft_pickaxe        = true; break;  // D-122 tech-tree (CRAFT_PICKAXE_IDX)
            case 22: a.build_monument       = true; break;  // D-122 tech-tree (BUILD_MONUMENT_IDX)
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

// D-122 early-warning helper. Pure arithmetic so it can be unit-tested without
// a network (see main.cpp --self-test). Mean advantage over the transitions
// that chose action `target`. The sign is the leading "monotone trend" signal:
// PPO's policy gradient moves a chosen action's log-prob in the direction of
// its advantage, so adv_mean(attack)>0 predicts hunting will rise, <0 predicts
// it gets extinguished -- readable long before the behavior itself emerges.
std::pair<float, int> PPO::mean_adv_for_action(
    const std::vector<float>&   adv,
    const std::vector<int64_t>& disc_act,
    int                         target)
{
    const std::size_t n = std::min(adv.size(), disc_act.size());
    double sum = 0.0;
    int    cnt = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (disc_act[i] == static_cast<int64_t>(target)) {
            sum += adv[i];
            ++cnt;
        }
    }
    if (cnt == 0) return {0.0f, 0};
    return {static_cast<float>(sum / cnt), cnt};
}

PPO::UpdateStats PPO::update(const std::vector<Transition>& rollout,
                             float last_value, bool critic_only,
                             bool sf_warmup)
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
    // D-122 early-warning: snapshot mean RAW advantage of the hunt(attack,idx2)
    // and cook(idx18) actions BEFORE normalization, so the sign keeps its
    // "is this action better/worse than the value baseline" meaning.
    auto [ew_adv_attack, ew_cnt_attack] = mean_adv_for_action(adv, disc_idx, /*attack*/2);
    auto [ew_adv_cook,   ew_cnt_cook]   = mean_adv_for_action(adv, disc_idx, /*cook*/18);
    // D-123 FULL-behavior panel: same pre-normalization snapshot, but for every
    // categorical class so a few hundred episodes predict which of ALL behaviors
    // rise vs get extinguished. Pure read-out of the SAME `adv`/`disc_idx`
    // already used above -> zero effect on the gradient/optimizer.
    std::array<float, ActorCriticImpl::DISC_CATS> ew_adv_all{};
    std::array<int,   ActorCriticImpl::DISC_CATS> ew_cnt_all{};
    for (int a = 0; a < ActorCriticImpl::DISC_CATS; ++a) {
        auto [m, c] = mean_adv_for_action(adv, disc_idx, a);
        ew_adv_all[a] = m;
        ew_cnt_all[a] = c;
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

    // ---------- D-123 SF: ψ TD target (host, fixed within this update) ----------
    // ψ(s) predicts the discounted future accumulation of each reward feature
    // φ. We train it with the vector Bellman target ψ(s_t) <- φ_t + γ(1-done)·
    // ψ(s_{t+1}). Build the bootstrap ψ(s_{i+1}) ONCE from a no-grad forward
    // over the whole rollout (same stale-hidden as the minibatch re-forward),
    // exactly mirroring how the value head uses precomputed returns. Skipped
    // (sf_enabled=false) if the caller never filled Transition.phi -> full
    // back-compat for legacy rollouts.
    constexpr int F = ActorCriticImpl::SF_DIM;
    const bool sf_enabled = (cfg_.sf_coef > 0.0f || sf_warmup)
                            && N > 0
                            && static_cast<int>(rollout[0].phi.size()) == F;
    torch::Tensor t_psiret;   // [N, F]; defined iff sf_enabled
    if (sf_enabled) {
        std::vector<float> phi_flat;
        phi_flat.reserve(static_cast<size_t>(N) * F);
        bool phi_ok = true;
        for (int i = 0; i < N; ++i) {
            if (static_cast<int>(rollout[i].phi.size()) != F) { phi_ok = false; break; }
            phi_flat.insert(phi_flat.end(), rollout[i].phi.begin(), rollout[i].phi.end());
        }
        if (phi_ok) {
            std::vector<float> psi_all(static_cast<size_t>(N) * F, 0.0f);
            {
                torch::NoGradGuard ng;
                auto fo    = net_->forward_step(t_obs, t_hin);
                auto h_all = std::get<4>(fo);
                auto psi   = net_->psi(h_all).to(torch::kCPU).contiguous();  // [N,F]
                std::copy(psi.data_ptr<float>(),
                          psi.data_ptr<float>() + psi_all.size(),
                          psi_all.data());
            }
            std::vector<float> psi_ret(static_cast<size_t>(N) * F, 0.0f);
            for (int i = 0; i < N; ++i) {
                const float nonterm = rollout[i].done ? 0.0f : 1.0f;
                const size_t base   = static_cast<size_t>(i) * F;
                // Bootstrap with next state's ψ; at the rollout boundary there is
                // no next state cached, so self-bootstrap (negligible: 1/N rows).
                const size_t nbase  = (i == N - 1) ? base
                                                   : static_cast<size_t>(i + 1) * F;
                for (int f = 0; f < F; ++f) {
                    psi_ret[base + f] = phi_flat[base + f]
                                      + cfg_.gamma * nonterm * psi_all[nbase + f];
                }
            }
            t_psiret = torch::from_blob(psi_ret.data(), {N, F}, torch::kFloat32)
                           .clone().to(device_);
        }
    }

    // ---------- PPO update ----------
    UpdateStats stats;
    stats.adv_attack = ew_adv_attack;  stats.cnt_attack = ew_cnt_attack;
    stats.adv_cook   = ew_adv_cook;    stats.cnt_cook   = ew_cnt_cook;
    stats.adv_by_action    = ew_adv_all;
    stats.cnt_by_action    = ew_cnt_all;
    stats.total_transitions = N;
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

            // D-123 SF: ψ TD loss on the DETACHED trunk. Because h_out is
            // detached, this gradient updates ONLY the `successor` head -> the
            // policy/value/encoder/GRU receive exactly zero ψ gradient (champion
            // is byte-stable). Target b_psiret is the fixed Bellman target above.
            torch::Tensor sf_loss;
            if (sf_enabled && t_psiret.defined()) {
                auto b_psiret = t_psiret.index_select(0, t_idx);   // [B, F]
                auto psi_pred = net_->psi(h_out.detach());          // [B, F]
                sf_loss = (psi_pred - b_psiret).pow(2).mean();
            }

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

            // D-123 SF ψ-only warmup: freeze policy/value/trunk, train just the
            // ψ head (analogous to D-101 critic_only). If φ was not provided
            // there is nothing to warm up, so skip the minibatch entirely.
            if (sf_warmup) {
                if (!sf_loss.defined()) continue;
                auto loss = cfg_.sf_coef * sf_loss;
                opt_->zero_grad();
                loss.backward();
                torch::nn::utils::clip_grad_norm_(net_->parameters(),
                                                  cfg_.max_grad_norm);
                opt_->step();
                stats.sf_loss += sf_loss.item<float>();
                ++total_minibatches;
                continue;
            }

            // D-101 critic warmup: value-only loss. value_loss does not flow
            // through the policy heads, so the BC policy is frozen exactly
            // while the critic learns V(s) for it.
            auto loss = critic_only
                ? (cfg_.vf_coef * value_loss)
                : (policy_loss + cfg_.vf_coef * value_loss
                   - cfg_.ent_coef * entropy);
            // Dead-action floor: penalize discrete actions whose batch-mean
            // probability has fallen below the floor so collapsed logits keep
            // a recoverable gradient. relu(floor - mean_p) is zero for any
            // healthy action, so this is inert until an action actually dies.
            if (!critic_only && cfg_.dead_action_floor > 0.0f && cfg_.dead_action_coef > 0.0f) {
                auto mean_p = log_p_d.exp().mean(0);  // [n_discrete]
                auto dead_pen = torch::relu(
                    torch::full_like(mean_p, cfg_.dead_action_floor) - mean_p).sum();
                loss = loss + cfg_.dead_action_coef * dead_pen;
            }
            if (kl_ref.defined()) loss = loss + cfg_.kl_ref_coef * kl_ref;
            // ψ TD loss rides along on the detached head (zero policy effect).
            if (sf_loss.defined()) loss = loss + cfg_.sf_coef * sf_loss;

            opt_->zero_grad();
            loss.backward();
            torch::nn::utils::clip_grad_norm_(net_->parameters(),
                                              cfg_.max_grad_norm);
            opt_->step();

            // Guardrail: the continuous log_std is a free, state-independent
            // parameter with NO upper bound. Under a warm-start finetune the
            // entropy bonus inflates it without limit (entropy 2.9 -> 70+),
            // which turns continuous control (move/turn) into pure noise and
            // erodes the cloned navigate/eat/drink behaviors -- the agent then
            // starves (deaths_food) while the bounded discrete head still fires
            // craft/milestone. This was the true cause of the v1/v2/v3 weather
            // warm-start collapses. Clamp to a sane exploration band every
            // step. The S5 champion sits at log(0.5)=-0.693.
            //
            // v6 update: a [-2.3, 0.5] band still let the (small) entropy bonus
            // pin log_std at the +0.5 ceiling (std=1.65) over ~1300 episodes;
            // that ceiling-level movement noise slowly eroded navigation
            // (r_alive 11 -> 7, episodes 6500 -> 4300 from ep ~1318). Cap the
            // ceiling just above the champion's own level (-0.65, std~=0.52) so
            // movement noise can never exceed the proven-healthy band -- it may
            // only get quieter (floor -2.3). The champion's -0.693 sits below
            // the ceiling and is untouched.
            {
                torch::NoGradGuard ng;
                net_->log_std.clamp_(-2.3f, -0.65f);
            }

            stats.policy_loss += policy_loss.item<float>();
            stats.value_loss  += value_loss.item<float>();
            stats.entropy     += entropy.item<float>();
            if (sf_loss.defined()) stats.sf_loss += sf_loss.item<float>();
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
        stats.sf_loss     /= total_minibatches;
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
    // torch::load adopts the checkpoint's shapes, so this fully loads bc_v9
    // (encoder comes back at its 567 input width). §3a widens only that first
    // layer to the current 569 obs width, zero-padding the new nutrition cols.
    // D-123 SF: tolerant load so a pre-SF checkpoint (no `successor.*`) still
    // warm-starts, with the ψ head left at fresh init for an sf_warmup phase.
    load_module_tolerant(net_, path);
    const bool enc_surgery  = expand_encoder_if_needed(net_);
    const bool disc_surgery = expand_disc_logits_if_needed(net_);  // D-122 one-pot
    const bool did_surgery  = enc_surgery || disc_surgery;
    net_->to(device_);
    if (did_surgery) {
        // The first-layer tensor was replaced; rebuild Adam over the new
        // parameter set (warm-start wants a fresh optimizer anyway).
        opt_ = std::make_shared<torch::optim::Adam>(
            net_->parameters(), torch::optim::AdamOptions(cfg_.lr));
    }
    // Optimizer file is optional - if missing we keep a fresh one (this is
    // fine for evaluation-only loads). After surgery the saved Adam state is
    // shape-incompatible, so we MUST keep the fresh optimizer just built.
    std::string opt_path = path + ".opt";
    if (did_surgery) {
        spdlog::info("PPO warm-started via surgery from {} (fresh optimizer)", path);
    } else if (std::filesystem::exists(opt_path)) {
        // D-123 SF: a pre-SF checkpoint's optimizer state has no entries for
        // the new psi (successor) head, so the saved Adam state is param-set
        // incompatible and torch::load throws. Fall back to a fresh optimizer
        // (params were still warm-started above) instead of aborting.
        try {
            torch::load(*opt_, opt_path);
            spdlog::info("PPO checkpoint + optimizer loaded: {}", path);
        } catch (const std::exception& e) {
            opt_ = std::make_shared<torch::optim::Adam>(
                net_->parameters(), torch::optim::AdamOptions(cfg_.lr));
            spdlog::warn("optimizer load from {} failed ({}); using fresh "
                         "optimizer (params warm-started, psi at fresh init)",
                         opt_path, e.what());
        }
    } else {
        spdlog::info("PPO checkpoint loaded (no optimizer file): {}", path);
    }
}

void PPO::load_reference(const std::string& path) {
    // §3a: the KL anchor must be the SAME bc_v9 widened to 569 so its outputs
    // match the warm-started policy exactly at step 0.
    ref_net_ = ActorCritic();
    load_module_tolerant(ref_net_, path);   // D-123 SF: tolerant (bc_v9 lacks ψ)
    expand_encoder_if_needed(ref_net_);
    expand_disc_logits_if_needed(ref_net_);  // D-122 one-pot: match policy head width
    ref_net_->to(device_);
    ref_net_->eval();
    for (auto& p : ref_net_->parameters()) p.set_requires_grad(false);
    has_ref_ = true;
    spdlog::info("decision_round_17: KL-reference policy loaded from {}", path);
}

}  // namespace dp::policy
