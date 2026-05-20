#include "core/capability/EmitSignalCapability.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/signal/SignalLayer.h"

#include <algorithm>
#include <vector>

namespace neuro {

EmitSignalCapability::EmitSignalCapability(std::size_t channels, float energy_cost)
    : channels_(channels), energy_cost_(energy_cost) {}

std::size_t EmitSignalCapability::consumedOutputs() const {
    return channels_;
}

void EmitSignalCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr) return;

    auto* signal_layer = world.getLayer<SignalLayer>();
    if (signal_layer == nullptr) return;

    bool any_active = false;
    float emitted_intensity = 0.0f;
    std::vector<float> signal(channels_, 0.0f);
    for (std::size_t i = 0; i < channels_ && i < outputs.size(); ++i) {
        const float intensity = std::clamp(outputs[i], 0.0f, 1.0f);
        if (intensity > 0.1f) {
            signal[i] = intensity;
            emitted_intensity += intensity;
            any_active = true;
        }
    }

    if (any_active) {
        signal_layer->emitWithHistory(agent.id(), agent.position(), signal, world.currentTick(), world.currentTimeSeconds());
        concrete->setEnergy(concrete->energy() - energy_cost_ * emitted_intensity * dt);
        for (std::size_t i = 0; i < channels_ && i < signal.size(); ++i) {
            if (signal[i] > 0.1f) {
                world.eventBus().publish(events::Event{events::EventType::AgentEmittedSignal,
                                                       events::AgentEmittedSignal{agent.id(), agent.position(), static_cast<std::uint8_t>(i), signal[i]},
                                                       world.currentTick(),
                                                       world.currentTimeSeconds()});
            }
        }
    }

    // Disease→signal linkage: infected agents auto-emit distress on last channel
    // This creates a natural "infection signal" that other agents can learn to avoid
    if (concrete->infected() && channels_ > 0) {
        std::vector<float> distress(channels_, 0.0f);
        distress[channels_ - 1] = concrete->infectionProgress() * 2.0f;  // intensity = infection severity
        signal_layer->emitWithHistory(agent.id(), agent.position(), distress, world.currentTick(), world.currentTimeSeconds());
        if (distress[channels_ - 1] > 0.01f) {
            world.eventBus().publish(events::Event{events::EventType::AgentEmittedSignal,
                                                   events::AgentEmittedSignal{agent.id(), agent.position(), static_cast<std::uint8_t>(channels_ - 1), distress[channels_ - 1]},
                                                   world.currentTick(),
                                                   world.currentTimeSeconds()});
        }
        // No extra cost for distress signal (it's involuntary)
    }
}

const char* EmitSignalCapability::name() const {
    return "emit_signal";
}

}  // namespace neuro
