#include "perception/MemoryCognitionPerception.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/signal/SignalLayer.h"
#include "brain/ProtagonistBrain.h"

#include <algorithm>
#include <optional>

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
    constexpr float kRecentSignalRadius = 36.0f;
    constexpr SimTimeSeconds kRecentSignalWindowSeconds = 40.0;

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

    // 2. Episodic readback
    const auto salient_events = episodic.topSalient(1);
    if (!salient_events.empty()) {
        const auto& evt = salient_events.front();
        sense_target(evt.pos, 18);
        output[20] = std::clamp(evt.salience, 0.0f, 1.0f);
        output[21] = std::clamp(1.0f - static_cast<float>((now_time - evt.time_seconds) / 120.0), 0.0f, 1.0f);
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

    std::optional<Vec2> recent_signal_target;
    if (const auto* signal_layer = world.getLayer<SignalLayer>(); signal_layer != nullptr) {
        const auto recent_signals = signal_layer->history().query(
            pos,
            kRecentSignalRadius,
            kRecentSignalWindowSeconds,
            now_time);
        for (auto it = recent_signals.rbegin(); it != recent_signals.rend(); ++it) {
            if (it->source != agent.id()) {
                recent_signal_target = it->pos;
                break;
            }
        }
    }

    // 4. Goal states
    // Output current goal as a normalized index in [0, 1]
    const float goal_idx = static_cast<float>(goal.currentGoal()) / static_cast<float>(GoalType::Count);
    Vec2 goal_target{};
    switch (goal.currentGoal()) {
        case GoalType::Explore:
        case GoalType::Count:
            break;
        case GoalType::FindWater:
            goal_target = water_target;
            break;
        case GoalType::FindFood:
            goal_target = food_target;
            break;
        case GoalType::GatherStick:
            goal_target = stick_target;
            break;
        case GoalType::GatherStone:
            goal_target = stone_target;
            break;
        case GoalType::ChopTree:
            goal_target = tree_target;
            break;
        case GoalType::BuildWorksite:
            goal_target = worksite_target;
            break;
        case GoalType::CraftWeapon:
            goal_target = base_target;
            break;
        case GoalType::Hunt:
            if (has_predator) {
                goal_target = nearest_predator.last_seen_pos;
            } else if (!salient_events.empty()) {
                goal_target = salient_events.front().pos;
            }
            break;
        case GoalType::Flee:
            goal_target = danger_target;
            break;
        case GoalType::RespondSignal:
            if (recent_signal_target.has_value()) {
                goal_target = *recent_signal_target;
            } else if (has_ally) {
                goal_target = best_ally.last_seen_pos;
            } else if (!salient_events.empty()) {
                goal_target = salient_events.front().pos;
            }
            break;
        case GoalType::EmitSignal:
            if (has_predator) {
                goal_target = nearest_predator.last_seen_pos;
            } else if (has_ally) {
                goal_target = best_ally.last_seen_pos;
            }
            break;
        case GoalType::RestNearFire:
            goal_target = fire_target;
            break;
    }
    output[32] = std::clamp(goal_idx, 0.0f, 1.0f);
    output[33] = std::clamp(goal.commitment(), 0.0f, 1.0f);
    sense_target(goal_target, 34);

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
