#pragma once

#include "brain/DNCMemory.h"
#include "core/interfaces/IWorldLayer.h"
#include "core/types/Aliases.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace neuro {
class IWorld;
class AgentLayer;
}

namespace neuro::routes::protagonist {

// Layer 5 #9 (audit): per-agent DNC memory store for background species.
// Acts as a route-level extension point: BackgroundMemoryReadPerception
// fetches read_vectors as input, BackgroundMemoryWriteCapability decodes
// the brain's interface vector and steps the DNC. The NeatBrain itself
// remains untouched, so all existing dynamic_cast<NeatBrain*> sites
// (Hebbian, writeWeightsBackTo, ComprehensiveMetrics) keep working.
//
// Memory cost per agent at the default tiny config (slots=4, word=2,
// heads=1) is ~(slots*word + slots*3 + slots*slots + heads*slots +
// heads*word) floats = ~32 floats = 128 bytes. With 65 background
// agents that's ~8KB per world — well within the 3MB envelope flagged
// in the audit summary.
class BackgroundMemoryLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "protagonist_background_memory";

    BackgroundMemoryLayer(std::size_t slots, std::size_t word_size, std::size_t read_heads);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override { return 95; }  // before AgentLayer (100)

    std::size_t slotCount() const { return slots_; }
    std::size_t wordSize() const { return word_size_; }
    std::size_t readHeads() const { return read_heads_; }
    std::size_t readVectorDim() const { return word_size_ * read_heads_; }
    std::size_t interfaceDim() const { return DNCMemory::interfaceDim(word_size_, read_heads_); }

    // Returns the agent's DNC memory, creating it on first access.
    DNCMemory& getOrCreate(AgentId agent_id);
    // Returns nullptr if no memory has been created yet.
    DNCMemory* find(AgentId agent_id);
    const DNCMemory* find(AgentId agent_id) const;

    std::size_t agentMemoryCount() const { return memories_.size(); }

private:
    void cullDeadAgents(IWorld& world);

    std::size_t slots_;
    std::size_t word_size_;
    std::size_t read_heads_;
    std::unordered_map<std::uint64_t, std::unique_ptr<DNCMemory>> memories_;
    AgentLayer* agents_{nullptr};
    int cull_counter_{0};
};

}  // namespace neuro::routes::protagonist
