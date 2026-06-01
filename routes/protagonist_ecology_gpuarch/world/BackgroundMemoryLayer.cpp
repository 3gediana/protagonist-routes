#include "world/BackgroundMemoryLayer.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"

#include <algorithm>
#include <unordered_set>

namespace neuro::routes::protagonist {

BackgroundMemoryLayer::BackgroundMemoryLayer(std::size_t slots,
                                             std::size_t word_size,
                                             std::size_t read_heads)
    : slots_(slots > 0 ? slots : 1),
      word_size_(word_size > 0 ? word_size : 1),
      read_heads_(read_heads > 0 ? read_heads : 1) {}

void BackgroundMemoryLayer::onAttach(IWorld& world) {
    agents_ = world.getLayer<AgentLayer>();
}

void BackgroundMemoryLayer::tick(IWorld& world, float dt) {
    (void)dt;
    // Cull memories of dead agents periodically (every 64 ticks). Doing
    // this every tick is O(N+M) and unnecessary; agents die rarely.
    if (++cull_counter_ >= 64) {
        cull_counter_ = 0;
        cullDeadAgents(world);
    }
}

DNCMemory& BackgroundMemoryLayer::getOrCreate(AgentId agent_id) {
    auto it = memories_.find(agent_id.value);
    if (it != memories_.end()) return *it->second;
    auto mem = std::make_unique<DNCMemory>(slots_, word_size_, read_heads_);
    auto* raw = mem.get();
    memories_.emplace(agent_id.value, std::move(mem));
    return *raw;
}

DNCMemory* BackgroundMemoryLayer::find(AgentId agent_id) {
    auto it = memories_.find(agent_id.value);
    return it == memories_.end() ? nullptr : it->second.get();
}

const DNCMemory* BackgroundMemoryLayer::find(AgentId agent_id) const {
    auto it = memories_.find(agent_id.value);
    return it == memories_.end() ? nullptr : it->second.get();
}

void BackgroundMemoryLayer::cullDeadAgents(IWorld& world) {
    if (agents_ == nullptr) {
        agents_ = world.getLayer<AgentLayer>();
        if (agents_ == nullptr) return;
    }
    std::unordered_set<std::uint64_t> alive_ids;
    alive_ids.reserve(agents_->agents().size());
    for (const auto* a : agents_->agents()) {
        if (a != nullptr && a->alive()) alive_ids.insert(a->id().value);
    }
    for (auto it = memories_.begin(); it != memories_.end(); ) {
        if (alive_ids.find(it->first) == alive_ids.end()) {
            it = memories_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace neuro::routes::protagonist
