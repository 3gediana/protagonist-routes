#pragma once

#include "core/interfaces/IAgentCapability.h"

#include <cstddef>

namespace neuro::routes::protagonist {

// Layer 5 #9: decodes the brain's last `interfaceDim()` outputs into a
// DNCMemoryInterface and steps the agent's DNC. Background NEAT brain
// is sized so the trailing slots feed this capability — earlier slots
// drive Move/Hunt/Attack/Flee as before.
//
// Consumed outputs: DNCMemory::interfaceDim(word_size, read_heads).
class BackgroundMemoryWriteCapability final : public IAgentCapability {
public:
    BackgroundMemoryWriteCapability(std::size_t word_size, std::size_t read_heads);

    std::size_t consumedOutputs() const override { return interface_dim_; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "bg_memory_write"; }

private:
    std::size_t word_size_;
    std::size_t read_heads_;
    std::size_t interface_dim_;
};

}  // namespace neuro::routes::protagonist
