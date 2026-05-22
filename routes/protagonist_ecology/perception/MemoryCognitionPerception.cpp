#include "perception/MemoryCognitionPerception.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "brain/ProtagonistBrain.h"

#include <algorithm>

namespace neuro::routes::protagonist {

MemoryCognitionPerception::MemoryCognitionPerception() {}

void MemoryCognitionPerception::sense(const IAgent& agent, const IWorld& world, std::span<float> output) const {
    if (output.size() < kDim) {
        return;
    }

    std::fill(output.begin(), output.end(), 0.0f);

    const auto* brain = dynamic_cast<const ProtagonistBrain*>(&agent.brain());
    if (brain == nullptr) {
        // Fallback if not protagonist
        return;
    }

    const auto& spatial = brain->spatialMemory();
    const auto& episodic = brain->episodicMemory();
    const auto& social = brain->socialMemory();
    const auto& goal = brain->goalMemory();
    const auto dnc = brain->dncSummary();
    const auto pos = agent.position();
    const auto now_time = world.currentTimeSeconds();

    // 1. Spatial queries (dx, dy normalized by world span)
    constexpr float kWorldSpan = 200.0f;

    auto sense_target = [&](Vec2 target, std::size_t out_offset) {
        if (target.x == 0.0f && target.y == 0.0f) {
            output[out_offset] = 0.0f;
            output[out_offset + 1] = 0.0f;
        } else {
            const Vec2 diff = target - pos;
            output[out_offset] = std::clamp(diff.x / kWorldSpan, -1.0f, 1.0f);
            output[out_offset + 1] = std::clamp(diff.y / kWorldSpan, -1.0f, 1.0f);
        }
    };

    const Vec2 water_target = spatial.findNearestWater(pos, 0.15f);
    const Vec2 food_target = spatial.findNearestFood(pos, 0.15f);
    const Vec2 tree_target = spatial.findNearestTree(pos, 0.15f);
    const Vec2 danger_target = spatial.findNearestDanger(pos, 0.15f);
    const Vec2 base_target = spatial.findNearestBase(pos, 0.15f);
    const Vec2 fire_target = spatial.findNearestFire(pos, 0.15f);
    const Vec2 worksite_target = spatial.findNearestWorksite(pos, 0.15f);
    const Vec2 stone_target = spatial.findNearestStone(pos, 0.15f);
    const Vec2 stick_target = spatial.findNearestStick(pos, 0.15f);

    sense_target(water_target, 0);
    sense_target(food_target, 2);
    sense_target(tree_target, 4);
    sense_target(danger_target, 6);
    sense_target(base_target, 8);
    sense_target(fire_target, 10);
    sense_target(worksite_target, 12);
    sense_target(stone_target, 14);
    sense_target(stick_target, 16);

    // 2. Episodic readback (audit § 4.2 fix)
    //
    // The old wiring exposed `topSalient(1)`, which under v3 training
    // mostly returned built_worksite / killed_prey / was_attacked /
    // saw_predator -- none of which are tied to the current four
    // fixed tasks (water / worksite / signal / danger). Per audit § 4.2
    // we now specialise dim 18-21 to `peer_signal_heard`, the one
    // episodic event whose pos / salience / recency the controller
    // actually needs to walk back to a signal source. When there is
    // no recent peer_signal_heard event these four dims stay 0,
    // which is the correct "I have no signal memory" signal.
    //
    // dim 18-19: dxdy to the most recent peer_signal_heard position
    // dim 20:    salience of that event (proxy for proximity at hear time)
    // dim 21:    recency in [0, 1] over a 60s episode-local window
    const auto recent_peer_signal = episodic.recentOfType("peer_signal_heard", 1);
    if (!recent_peer_signal.empty()) {
        const auto& evt = recent_peer_signal.front();
        sense_target(evt.pos, 18);
        output[20] = std::clamp(evt.salience, 0.0f, 1.0f);
        output[21] = std::clamp(1.0f - static_cast<float>((now_time - evt.time_seconds) / 60.0), 0.0f, 1.0f);
    }

    const auto recent_threat = episodic.recentOfType("was_attacked", 1);
    if (!recent_threat.empty()) {
        output[22] = std::clamp(recent_threat.front().salience, 0.0f, 1.0f);
    }
    const auto recent_attack = episodic.recentOfType("attacked_enemy", 1);
    if (!recent_attack.empty()) {
        output[23] = std::clamp(recent_attack.front().salience, 0.0f, 1.0f);
    }

    // 3. Social readback
    SocialRelationship best_ally{};
    bool has_ally = false;
    SocialRelationship nearest_predator{};
    bool has_predator = false;
    float best_predator_dist_sq = 0.0f;

    for (const auto& [_, rel] : social.relations()) {
        if (!rel.is_predator) {
            const float social_score = rel.cooperation_score + rel.communication_score;
            if (!has_ally || social_score > (best_ally.cooperation_score + best_ally.communication_score)) {
                best_ally = rel;
                has_ally = true;
            }
            continue;
        }

        const float dist_sq = Vec2::distanceSquared(pos, rel.last_seen_pos);
        if (!has_predator || dist_sq < best_predator_dist_sq) {
            nearest_predator = rel;
            best_predator_dist_sq = dist_sq;
            has_predator = true;
        }
    }

    if (has_ally) {
        sense_target(best_ally.last_seen_pos, 24);
        output[26] = std::clamp(best_ally.cooperation_score, 0.0f, 1.0f);
        output[27] = std::clamp(best_ally.communication_score, 0.0f, 1.0f);
    }
    if (has_predator) {
        sense_target(nearest_predator.last_seen_pos, 28);
        output[30] = 1.0f;
        output[31] = std::clamp(nearest_predator.hostility_score, 0.0f, 1.0f);
    }

    // 4. Goal states (audit § 4.1 cascade decoupling)
    //
    // Old wiring set dim 34-35 to goal_target dxdy computed via a
    // switch(goal) → spatial.findNearestX cascade. That made `no_spatial`
    // and `no_goal` ablation variants impossible to separate: turning
    // off spatial writes zeroed dim 0-17 AND dim 34-35 (because
    // findNearestX returned the empty Vec2 zero), and turning off
    // goal_manager also zeroed dim 32-35. So `no_spatial` was
    // double-counted as `no_goal` and vice versa.
    //
    // New wiring exposes only intrinsic GoalMemory state in dim 32-35,
    // so the four dims live or die exclusively with `goal_manager_enabled`:
    //
    // dim 32: goal kind index (normalized over GoalType::Count)
    // dim 33: commitment in [0, 1]
    // dim 34: completion progress in [0, 1]
    // dim 35: elapsed_seconds normalized over a 60s episode window
    //
    // Spatial-related direction signals remain on dim 0-17 where they
    // already lived. The controller can still combine "I am pursuing
    // FindWater" (dim 32) with "water is here" (dim 0-1) via its own
    // weights, but the perception output no longer pre-fuses them.
    //
    // (We intentionally drop the recent_signal_target lookup here too.
    // The audit added a peer_signal_heard episodic event whose readback
    // already lives on dim 18-21, which is the proper memory pipeline
    // for signal direction.)
    const float goal_idx = static_cast<float>(goal.currentGoal()) / static_cast<float>(GoalType::Count);
    output[32] = std::clamp(goal_idx, 0.0f, 1.0f);
    output[33] = std::clamp(goal.commitment(), 0.0f, 1.0f);
    output[34] = std::clamp(goal.completionProgress(), 0.0f, 1.0f);
    output[35] = std::clamp(static_cast<float>(goal.elapsedSeconds()) / 60.0f, 0.0f, 1.0f);

    // 5. DNC retrieval activity summary
    output[36] = std::clamp(dnc.usage_mean, 0.0f, 1.0f);
    output[37] = std::clamp(dnc.usage_max, 0.0f, 1.0f);
    output[38] = std::clamp(dnc.write_peak, 0.0f, 1.0f);
    output[39] = std::clamp(dnc.read_peak, 0.0f, 1.0f);
    output[40] = std::clamp(dnc.read_norm / 4.0f, 0.0f, 1.0f);
    output[41] = std::clamp(dnc.precedence_peak, 0.0f, 1.0f);
    output[42] = std::clamp(dnc.write_focus_slot, 0.0f, 1.0f);
    output[43] = std::clamp(dnc.read_focus_slot, 0.0f, 1.0f);
}

}  // namespace neuro::routes::protagonist
