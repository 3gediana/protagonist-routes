// SPDX: D-078: VecEnv. N parallel Environment instances stepped via std::async.
// Goal: scale GPU/CPU utilisation for PPO training. Each env has its own
// seed, world, animals, plants - they share NOTHING. step_all() launches
// N std::async tasks that run env.step() in parallel.
//
// Usage:
//   VecEnv venv(base_cfg, 8, base_seed);
//   std::vector<Vector> obs_batch;
//   venv.reset_all(obs_batch);   // obs_batch.size() == 8
//   ...
//   std::vector<AgentAction> actions = ppo.sample_actions_batch(obs_batch, ...);
//   std::vector<StepResult> results;
//   venv.step_all(actions, results);
//   for (int i = 0; i < 8; ++i) {
//       if (results[i].done) venv.reset(i, obs_batch[i]);
//       else                 obs_batch[i] = results[i].obs;
//   }
#pragma once

#include "env/Environment.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace dp::env {

class VecEnv {
public:
    // n_envs >= 1. Each env_i is constructed with EnvConfig where world.seed
    // = base_cfg.world.seed + i*OFFSET. Same physics, different worlds.
    VecEnv(const EnvConfig& base_cfg, int n_envs, uint64_t seed_xor = 0xC0FFEEULL);

    int n_envs() const { return static_cast<int>(envs_.size()); }
    Environment& env(int i) { return *envs_[i]; }
    const Environment& env(int i) const { return *envs_[i]; }

    // Reset all envs into obs_batch[0..n_envs-1]. Resizes obs_batch.
    void reset_all(std::vector<dp::agent::ObservationBuilder::Vector>& obs_batch);

    // Reset a single env after done. Writes into obs_out.
    void reset(int idx, dp::agent::ObservationBuilder::Vector& obs_out);

    // Domain randomization: rebuild env idx with a brand-new world seed
    // (fresh terrain + resource/plant layout), then reset. Used to stop the
    // policy memorizing a fixed handful of maps (overfit fix, 2026-06-01).
    void reseed(int idx, uint64_t new_world_seed,
                dp::agent::ObservationBuilder::Vector& obs_out);

    // Step all envs in parallel. actions.size() must == n_envs(). Resizes
    // results to n_envs and fills it.
    void step_all(const std::vector<dp::agent::AgentAction>& actions,
                  std::vector<StepResult>& results);

private:
    std::vector<std::unique_ptr<Environment>> envs_;
    EnvConfig base_cfg_;     // template config for domain-rand rebuilds
    uint64_t  seed_xor_{0};  // per-index decorrelation offset
};

} // namespace dp::env
