#include "core/capability/EmitSignalCapability.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/signal/SignalLayer.h"

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
    std::vector<float> signal(channels_, 0.0f);
    for (std::size_t i = 0; i < channels_ && i < outputs.size(); ++i) {
        signal[i] = std::max(0.0f, outputs[i]);
        if (signal[i] > 0.1f) any_active = true;
    }

    if (any_active) {
        signal_layer->emit(agent.position(), signal);
        concrete->setEnergy(concrete->energy() - energy_cost_ * dt);
    }
}

const char* EmitSignalCapability::name() const {
    return "emit_signal";
}

}  // namespace neuro
