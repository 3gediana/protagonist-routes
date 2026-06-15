#pragma once

#include <cstdint>

namespace dp::agent {

// Three-stage progression that drives the blueprint's curriculum.
// Each stage unlocks new actions and abilities; the agent has to "earn"
// access to the next stage by hitting per-stage thresholds.
//
// Early: spawn defaults; forage, drink, hunt, basic crafting
// Mid:   unlock real construction (place blueprint, deposit materials)
// Late:  unlock farming, taming, animal pens
enum class ProgressStage : uint8_t {
    Early = 0,
    Mid   = 1,
    Late  = 2,
};

const char* progress_stage_name(ProgressStage s);

class ProgressTracker {
public:
    // Thresholds that gate Early -> Mid
    int   mid_collect_threshold  = 5;    // D-086: 30->5 (5 D-XXX runs with 30 threshold built ~7 total houses across 25M steps; lower threshold gives PPO more attempts at build chain per episode)
    float mid_explore_threshold  = 50.0f;   // D-086: 500->50 (same reason; Mid gate unlocks within first ~30s of episode instead of mid-late)

    // Threshold that gates Mid -> Late
    int   late_house_threshold = 1;   // buildings completed

    // Per-step event hooks
    void on_collect(int count = 1);
    void on_explore_step(float distance);
    void on_house_built();

    // Returns true on the tick the stage just advanced (rising edge).
    // Used by RewardSystem for the +50 unlock bonus.
    bool poll_unlock();

    ProgressStage stage() const { return stage_; }
    int   resources_collected() const { return resources_collected_; }
    float total_explored()      const { return total_explored_; }
    int   houses_built()        const { return houses_built_; }

    // Helpers to ask "is X unlocked this stage?"
    bool can_build()  const { return stage_ != ProgressStage::Early; }
    bool can_farm()   const { return stage_ == ProgressStage::Late; }
    bool can_tame()   const { return stage_ == ProgressStage::Late; }

    // EpisodeManager calls this on reset (progress persists across
    // episodes in *training* mode by convention, but the API is here so
    // an evaluation harness can wipe it for a clean run).
    void reset();

private:
    ProgressStage stage_              = ProgressStage::Early;
    int           resources_collected_ = 0;
    float         total_explored_      = 0.0f;
    int           houses_built_        = 0;
    bool          unlock_just_fired_   = false;

    void try_advance_();
};

}  // namespace dp::agent
