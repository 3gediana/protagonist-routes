#include "policy/PPO.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numeric>
#include <random>

namespace dp::policy {

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

    hidden_ = torch::zeros({1, ActorCriticImpl::HIDDEN},
                           torch::TensorOptions().dtype(torch::kFloat32)
                                                 .device(device_));

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
    auto eps   = torch::randn_like(cm);
    auto cont  = cm + std_ * eps;                // reparam sample
    auto cont_log_prob =
        (-0.5f * ((cont - cm) / std_).pow(2)
         - cs
         - 0.5f * std::log(2.0f * static_cast<float>(M_PI)))
            .sum(1);                             // [1]

    // Discrete: 17-way categorical (16 triggers + NOOP). Multinomial
    // sample one index, log_prob = log p(a). Replaces independent
    // Bernoulli heads to break the "press every button" collapse.
    auto log_probs_d = torch::log_softmax(dl, /*dim=*/1);   // [1, 17]
    auto probs_d     = log_probs_d.exp();                    // [1, 17]
    auto idx         = probs_d.multinomial(1, /*replacement=*/true);  // [1, 1]
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
        default: /* NOOP */ break;
    }
    return a;
}

PPO::UpdateStats PPO::update(const std::vector<Transition>& rollout,
                             float last_value)
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

            auto [cm, cs, dl, v, h_out] = net_->forward_step(b_obs, b_hin);
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
            auto loss = policy_loss + cfg_.vf_coef * value_loss
                      - cfg_.ent_coef * entropy;

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
        }
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
    torch::load(net_, path);
    net_->to(device_);
    // Optimizer file is optional - if missing we keep a fresh one (this
    // is fine for evaluation-only loads).
    std::string opt_path = path + ".opt";
    if (std::filesystem::exists(opt_path)) {
        torch::load(*opt_, opt_path);
        spdlog::info("PPO checkpoint + optimizer loaded: {}", path);
    } else {
        spdlog::info("PPO checkpoint loaded (no optimizer file): {}", path);
    }
}

}  // namespace dp::policy
