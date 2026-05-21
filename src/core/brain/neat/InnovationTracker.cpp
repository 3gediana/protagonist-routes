#include "core/brain/neat/InnovationTracker.h"

#include <functional>

namespace neuro {

InnovationTracker::InnovationTracker(std::uint64_t next_innovation_value, NodeId next_node_id)
    : next_innovation_(InnovationId{next_innovation_value}),
      next_node_id_(next_node_id) {}

InnovationId InnovationTracker::nextConnectionInnovation(NodeId from, NodeId to) {
    const EdgeKey key{from, to};
    if (const auto it = current_generation_cache_.find(key); it != current_generation_cache_.end()) {
        return it->second;
    }

    const InnovationId assigned = next_innovation_;
    current_generation_cache_.emplace(key, assigned);
    ++next_innovation_.value;
    return assigned;
}

NodeId InnovationTracker::nextNodeId() {
    return next_node_id_++;
}

void InnovationTracker::ensureNextNodeIdAtLeast(NodeId minimum_next_node_id) {
    if (next_node_id_ < minimum_next_node_id) {
        next_node_id_ = minimum_next_node_id;
    }
}

void InnovationTracker::beginGeneration() {
    current_generation_cache_.clear();
}

InnovationId InnovationTracker::lastInnovationId() const {
    return next_innovation_.value == 0 ? kInvalidInnovationId : InnovationId{next_innovation_.value - 1};
}

NodeId InnovationTracker::lastNodeId() const {
    return next_node_id_ == 0 ? 0 : next_node_id_ - 1;
}

InnovationTrackerState InnovationTracker::exportState() const {
    return InnovationTrackerState{
        next_innovation_.value,
        next_node_id_,
    };
}

void InnovationTracker::importState(const InnovationTrackerState& state) {
    next_innovation_ = InnovationId{state.next_innovation_value == 0 ? 1 : state.next_innovation_value};
    next_node_id_ = state.next_node_id == 0 ? 1 : state.next_node_id;
    current_generation_cache_.clear();
}

std::size_t InnovationTracker::EdgeKeyHasher::operator()(const EdgeKey& key) const noexcept {
    const std::size_t lhs = std::hash<NodeId>{}(key.from);
    const std::size_t rhs = std::hash<NodeId>{}(key.to);
    return lhs ^ (rhs + 0x9e3779b9u + (lhs << 6u) + (lhs >> 2u));
}

}  // namespace neuro
