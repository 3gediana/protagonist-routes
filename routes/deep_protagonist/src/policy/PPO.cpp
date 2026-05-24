#include "policy/PPO.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace dp::policy {

// ============================================================================
// Network
// ============================================================================
ActorCriticImpl::ActorCriticImpl() {
    fc1         = register_module("fc1",         torch::nn::Linear(OBS_DIM, HIDDEN));
    fc2         = register_module("fc2",         torch::nn::Linear(HIDDEN,  HIDDEN));
    cont_mean   = register_module("cont_mean",   torch::nn::Linear(HIDDEN,  CONT_DIM));
    disc_logits = register_module("disc_logits", torch::nn::Linear(HIDDEN,  DISC_DIM));
    value       = register_module("value",       torch::nn::Linear(HIDDEN,  1));

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
    orthogonal_(fc1,         std::sqrt(2.0f));
    orthogonal_(fc2,         std::sqrt(2.0f));
    orthogonal_(cont_mean,   0.01f);   // small final-layer
    orthogonal_(disc_logits, 0.01f);
    orthogonal_(value,       1.0f);
}

std::tuple<torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor>
ActorCriticImpl::forward(const torch::Tensor& obs) {
    auto h = torch::relu(fc1(obs));
    h      = torch::relu(fc2(h));
    auto cm = cont_mean(h);
    auto cs = log_std.expand({obs.size(0), CONT_DIM});
    auto dl = disc_logits(h);
    auto v  = value(h).squeeze(-1);
    return {cm, cs, dl, v};
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

    spdlog::info("PPO init on device={}, params={}",
                 (device_.is_cuda() ? "CUDA" : "CPU"),
                 [&]() {
                     int64_t n = 0;
                     for (auto& p : net_->parameters()) n += p.numel();
                     return n;
                 }());
}

dp::agent::AgentAction PPO::sample_action(
    const dp::agent::ObservationBuilder::Vector& obs,
    std::vector<float>& out_cont,
    std::vector<float>& out_disc,
    float& out_log_prob,
    float& out_value)
{
    constexpr int O = ActorCriticImpl::OBS_DIM;
    constexpr int C = ActorCriticImpl::CONT_DIM;
    constexpr int D = ActorCriticImpl::DISC_DIM;

    torch::NoGradGuard ng;
    auto t_obs = torch::from_blob(
        const_cast<float*>(obs.data()), {1, O}, torch::kFloat32)
        .clone()
        .to(device_);
    net_->eval();
    auto [cm, cs, dl, v] = net_->forward(t_obs);

    auto std_  = cs.exp();                       // [1, C]
    auto eps   = torch::randn_like(cm);
    auto cont  = cm + std_ * eps;                // reparam sample
    auto cont_log_prob =
        (-0.5f * ((cont - cm) / std_).pow(2)
         - cs
         - 0.5f * std::log(2.0f * static_cast<float>(M_PI)))
            .sum(1);                             // [1]

    // Discrete: independent Bernoulli per channel.
    auto probs = torch::sigmoid(dl);             // [1, D]
    auto u     = torch::rand_like(probs);
    auto disc  = (u < probs).to(torch::kFloat32);
    // Bernoulli log-prob with logits (numerically stable).
    auto disc_log_prob = -torch::nn::functional::binary_cross_entropy_with_logits(
        dl, disc,
        torch::nn::functional::BinaryCrossEntropyWithLogitsFuncOptions()
            .reduction(torch::kNone))
        .sum(1);                                 // [1]

    auto log_prob = cont_log_prob + disc_log_prob;

    auto cont_cpu = cont.to(torch::kCPU).contiguous();
    auto disc_cpu = disc.to(torch::kCPU).contiguous();
    out_cont.assign(cont_cpu.data_ptr<float>(),
                    cont_cpu.data_ptr<float>() + C);
    out_disc.assign(disc_cpu.data_ptr<float>(),
                    disc_cpu.data_ptr<float>() + D);
    out_log_prob = log_prob.item<float>();
    out_value    = v.item<float>();

    // Map cont -> AgentAction. move/yaw can be unbounded; CapsuleAgent
    // normalises move and accepts any yaw_rate (we still scale yaw_rate
    // down so a freshly-init policy doesn't spin like a top).
    dp::agent::AgentAction a{};
    a.move_x   = std::tanh(out_cont[0]);
    a.move_y   = std::tanh(out_cont[1]);
    a.yaw_rate = std::tanh(out_cont[2]) * 3.0f;    // up to 3 rad/s
    a.eat                  = out_disc[0]  > 0.5f;
    a.drink                = out_disc[1]  > 0.5f;
    a.attack               = out_disc[2]  > 0.5f;
    a.collect              = out_disc[3]  > 0.5f;
    a.place_shelter        = out_disc[4]  > 0.5f;
    a.craft_spear          = out_disc[5]  > 0.5f;
    a.sleep                = out_disc[6]  > 0.5f;
    a.craft_grass_dress    = out_disc[7]  > 0.5f;
    a.craft_fur_cloak      = out_disc[8]  > 0.5f;
    a.wear_clothes         = out_disc[9]  > 0.5f;
    a.place_blueprint      = out_disc[10] > 0.5f;
    a.cycle_building_type  = out_disc[11] > 0.5f;
    a.deposit_to_site      = out_disc[12] > 0.5f;
    a.plant_seed           = out_disc[13] > 0.5f;
    a.water_plot           = out_disc[14] > 0.5f;
    a.feed_tame            = out_disc[15] > 0.5f;
    return a;
}

PPO::UpdateStats PPO::update(const std::vector<Transition>& rollout,
                             float last_value)
{
    constexpr int O = ActorCriticImpl::OBS_DIM;
    constexpr int C = ActorCriticImpl::CONT_DIM;
    constexpr int D = ActorCriticImpl::DISC_DIM;

    const int N = static_cast<int>(rollout.size());
    if (N == 0) return {};

    // ---------- pack rollout into flat host tensors ----------
    std::vector<float> obs_flat;       obs_flat.reserve(N * O);
    std::vector<float> cont_flat;      cont_flat.reserve(N * C);
    std::vector<float> disc_flat;      disc_flat.reserve(N * D);
    std::vector<float> rew(N), val(N), lp(N);
    std::vector<float> done(N);
    for (int i = 0; i < N; ++i) {
        const auto& tr = rollout[i];
        obs_flat.insert(obs_flat.end(), tr.obs.begin(), tr.obs.end());
        cont_flat.insert(cont_flat.end(), tr.cont_act.begin(), tr.cont_act.end());
        disc_flat.insert(disc_flat.end(), tr.disc_act.begin(), tr.disc_act.end());
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
    auto t_disc  = torch::from_blob(disc_flat.data(), {N, D}, torch::kFloat32).clone().to(device_);
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
            auto b_lpold = t_lpold.index_select(0, t_idx);
            auto b_adv   = t_adv.index_select(0,   t_idx);
            auto b_ret   = t_ret.index_select(0,   t_idx);

            auto [cm, cs, dl, v] = net_->forward(b_obs);
            auto std_ = cs.exp();
            auto cont_lp =
                (-0.5f * ((b_cont - cm) / std_).pow(2)
                 - cs
                 - 0.5f * std::log(2.0f * static_cast<float>(M_PI))).sum(1);
            auto disc_lp =
                -torch::nn::functional::binary_cross_entropy_with_logits(
                    dl, b_disc,
                    torch::nn::functional::BinaryCrossEntropyWithLogitsFuncOptions()
                        .reduction(torch::kNone)).sum(1);
            auto lp_new = cont_lp + disc_lp;

            // Continuous Gaussian entropy + Bernoulli entropy summed over channels.
            auto cont_ent = (cs + 0.5f
                             * std::log(2.0f * static_cast<float>(M_PI) * static_cast<float>(M_E)))
                            .sum(1);
            auto p = torch::sigmoid(dl);
            auto eps = 1e-7f;
            auto disc_ent = (-(p * (p + eps).log()
                              + (1.0f - p) * (1.0f - p + eps).log())).sum(1);
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

}  // namespace dp::policy
