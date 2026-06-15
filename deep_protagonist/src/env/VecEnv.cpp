// SPDX: D-078: VecEnv impl. Parallel env.step() via std::async.
//
// Threading model: each step_all() launches n_envs std::async tasks
// (launch::async => OS thread or thread pool, impl-defined). On Windows
// MSVC this maps to ThreadPool. Cost is ~5-15us per task spawn + join,
// dominated by env.step() which is much heavier (~100-200us at 30Hz
// scale).
//
// IMPORTANT: Environment instances do NOT share state. Each holds its
// own world_, plants_, animals_, resources_, agent_, vitals_, etc.
// libtorch is only touched in PPO::sample_actions_batch (main thread),
// not inside env.step(). So thread-safety is naturally satisfied.

#include "env/VecEnv.hpp"

#include <spdlog/spdlog.h>

#include <future>

namespace dp::env {

VecEnv::VecEnv(const EnvConfig& base_cfg, int n_envs, uint64_t seed_xor)
    : base_cfg_(base_cfg), seed_xor_(seed_xor) {
    if (n_envs < 1) n_envs = 1;
    envs_.reserve(n_envs);
    for (int i = 0; i < n_envs; ++i) {
        EnvConfig cfg = base_cfg;
        // Give each env a distinct world. Reusing base seed XOR'd with
        // index keeps reproducibility stable across runs given same base.
        cfg.world.seed = base_cfg.world.seed
                         ^ (seed_xor + static_cast<uint64_t>(i) * 0x9E3779B97F4A7C15ULL);
        cfg.instance_seed_xor = base_cfg.instance_seed_xor
                              + static_cast<uint64_t>(i) * 0xBF58476D1CE4E5B9ULL;
        envs_.emplace_back(std::make_unique<Environment>(cfg));
    }
    spdlog::info("VecEnv built: n_envs={}", n_envs);
}

void VecEnv::reset_all(std::vector<dp::agent::ObservationBuilder::Vector>& obs_batch) {
    obs_batch.resize(envs_.size());
    for (size_t i = 0; i < envs_.size(); ++i) {
        envs_[i]->reset(obs_batch[i]);
    }
}

void VecEnv::reset(int idx, dp::agent::ObservationBuilder::Vector& obs_out) {
    envs_.at(idx)->reset(obs_out);
}

void VecEnv::reseed(int idx, uint64_t new_world_seed,
                    dp::agent::ObservationBuilder::Vector& obs_out) {
    // Rebuild this slot's Environment from scratch with a fresh world seed so
    // terrain + resource/plant scatter are re-rolled (animals already reseed
    // per episode inside EpisodeManager::reset, but terrain/resources are
    // fixed at construction -> without this the policy only ever sees n_envs
    // distinct maps and memorizes their food layout = overfit).
    EnvConfig cfg = base_cfg_;
    cfg.world.seed = new_world_seed;
    cfg.instance_seed_xor = base_cfg_.instance_seed_xor
                          ^ (new_world_seed * 0xBF58476D1CE4E5B9ULL);
    envs_.at(idx) = std::make_unique<Environment>(cfg);
    envs_[idx]->reset(obs_out);
}

void VecEnv::step_all(const std::vector<dp::agent::AgentAction>& actions,
                      std::vector<StepResult>& results) {
    const int n = static_cast<int>(envs_.size());
    if (static_cast<int>(actions.size()) != n) {
        spdlog::error("VecEnv::step_all: actions.size()={} != n_envs={}",
                      actions.size(), n);
        return;
    }
    results.resize(n);

    // Parallel dispatch via std::async. Each future runs env.step()
    // independently. We collect all futures then join (wait .get()).
    std::vector<std::future<StepResult>> futs;
    futs.reserve(n);
    for (int i = 0; i < n; ++i) {
        Environment* e = envs_[i].get();
        const dp::agent::AgentAction* a = &actions[i];
        futs.emplace_back(std::async(std::launch::async,
            [e, a]() { return e->step(*a); }));
    }
    for (int i = 0; i < n; ++i) {
        results[i] = futs[i].get();
    }
}

} // namespace dp::env
