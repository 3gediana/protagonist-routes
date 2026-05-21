#include "world/MemoryTrialLayer.h"

#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro::routes::protagonist {

namespace {

Vec2 clampToWorld(Vec2 position, Vec2 world_size) {
    position.x = std::clamp(position.x, 0.0f, world_size.x);
    position.y = std::clamp(position.y, 0.0f, world_size.y);
    return position;
}

bool shouldTrackAgent(const Agent& agent) {
    return dynamic_cast<const ProtagonistBrain*>(&agent.brain()) != nullptr;
}

}  // namespace

MemoryTrialLayer::MemoryTrialLayer(const MemoryTrialConfig& config, std::uint32_t seed)
    : config_(config),
      rng_(seed == 0 ? std::random_device{}() : seed) {}

void MemoryTrialLayer::onAttach(IWorld& world) {
    agents_ = world.getLayer<AgentLayer>();
    world_size_ = world.size();
    cue_anchor_ = Vec2{world_size_.x * 0.50f, world_size_.y * 0.18f};
    hub_anchor_ = Vec2{world_size_.x * 0.50f, world_size_.y * 0.46f};
    left_target_ = Vec2{world_size_.x * 0.33f, world_size_.y * 0.78f};
    right_target_ = Vec2{world_size_.x * 0.67f, world_size_.y * 0.78f};

    std::uniform_int_distribution<int> cue_dist(0, 1);
    correct_channel_ = static_cast<std::size_t>(cue_dist(rng_));
    cue_active_ = true;
    success_count_ = 0;
    failure_count_ = 0;
    agent_states_.clear();
}

void MemoryTrialLayer::tick(IWorld& world, float dt) {
    (void)dt;
    if (agents_ == nullptr) {
        return;
    }

    cue_active_ = world.currentTimeSeconds() < config_.cue_duration_seconds;
    const bool deadline_passed =
        world.currentTimeSeconds() >= (config_.cue_duration_seconds + config_.choice_deadline_seconds);

    for (auto* agent : agents_->agents()) {
        auto* concrete = dynamic_cast<Agent*>(agent);
        if (concrete == nullptr || !concrete->alive() || !shouldTrackAgent(*concrete)) {
            continue;
        }

        auto& state = agent_states_[concrete->id().value];
        if (cue_active_) {
            placeAtCue(*concrete);
            continue;
        }

        if (!state.sent_to_hub) {
            placeAtHub(*concrete);
            state.sent_to_hub = true;
        }

        if (state.resolved) {
            continue;
        }

        if (deadline_passed) {
            concrete->addFitness(-static_cast<double>(config_.failure_fitness_penalty));
            state.resolved = true;
            ++failure_count_;
            continue;
        }

        const bool in_left = Vec2::distanceSquared(concrete->position(), left_target_)
            <= config_.target_radius * config_.target_radius;
        const bool in_right = Vec2::distanceSquared(concrete->position(), right_target_)
            <= config_.target_radius * config_.target_radius;
        if (!in_left && !in_right) {
            continue;
        }

        const std::size_t chosen_channel = in_left ? 0u : 1u;
        if (chosen_channel == correct_channel_) {
            concrete->addFitness(static_cast<double>(config_.success_fitness_bonus));
            ++success_count_;
        } else {
            concrete->addFitness(-static_cast<double>(config_.failure_fitness_penalty));
            ++failure_count_;
        }
        state.resolved = true;
    }
}

std::array<float, 2> MemoryTrialLayer::cueFor(Vec2 position) const {
    std::array<float, 2> cue{0.0f, 0.0f};
    if (!cue_active_) {
        return cue;
    }
    if (Vec2::distanceSquared(position, cue_anchor_) > config_.cue_radius * config_.cue_radius) {
        return cue;
    }
    cue[correct_channel_] = 1.0f;
    return cue;
}

void MemoryTrialLayer::placeAtCue(Agent& agent) const {
    agent.setPosition(clampToWorld(cue_anchor_ + stableOffset(agent.id().value, 1.25f), world_size_));
    agent.setVelocity(kZeroVec2);
}

void MemoryTrialLayer::placeAtHub(Agent& agent) const {
    agent.setPosition(clampToWorld(hub_anchor_ + stableOffset(agent.id().value, 1.5f), world_size_));
    agent.setVelocity(kZeroVec2);
}

Vec2 MemoryTrialLayer::stableOffset(std::uint64_t agent_value, float radius) const {
    const float angle = static_cast<float>((agent_value * 2654435761u) % 6283u) / 1000.0f;
    return Vec2{std::cos(angle) * radius, std::sin(angle) * radius};
}

}  // namespace neuro::routes::protagonist
