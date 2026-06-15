#include "agent/EpisodeManager.hpp"

namespace dp::agent {

int EpisodeManager::begin_episode() {
    ++episode_id_;
    step_count_ = 0;
    elapsed_s_  = 0.0f;
    return episode_id_;
}

void EpisodeManager::tick(float dt) {
    elapsed_s_ += dt;
    ++step_count_;
}

bool EpisodeManager::is_done(const VitalState& v) const {
    if (!v.alive) return true;
    if (elapsed_s_ >= cfg_.max_seconds) return true;
    return false;
}

void EpisodeManager::reset(const dp::world::World&    world,
                           CapsuleAgent&               agent,
                           VitalSystem&                vital_sys,
                           VitalState&                 vital_state,
                           RewardSystem&               reward,
                           dp::world::AnimalSystem&    animals,
                           Inventory&                  inventory,
                           Shelter&                    shelter,
                           dp::world::BuildingSystem&  buildings) {
    // 1. Snap agent back to spawn slightly above ground.
    float gz = world.surface_height(cfg_.spawn_x, cfg_.spawn_y);
    agent.reset(cfg_.spawn_x, cfg_.spawn_y, gz + cfg_.spawn_clearance);

    // 2. Restore vitals (also clears alive=true and increments death count
    //    if needed, see VitalSystem::respawn).
    vital_sys.respawn(vital_state);

    // 3. Per-episode reward accumulator goes back to zero.
    reward.reset_episode();

    // 4. Reseed the animal layer so each episode is comparable but not
    //    identical. Mixing in episode_id keeps the same world feeling
    //    "fresh" across rollouts.
    uint64_t seed = cfg_.animal_seed_base
                   ^ static_cast<uint64_t>(episode_id_) * 0x9E3779B97F4A7C15ULL;
    animals.initialize(seed, world);

    // 5. Inventory + shelter + buildings are per-episode state.
    inventory.clear();
    shelter.clear();
    buildings.clear();

    // 6. Counters: this finishes the *previous* episode, then we begin
    //    the next one. begin_episode bumps id and zeros timers.
    ++episodes_completed_;
    begin_episode();
}

}  // namespace dp::agent
