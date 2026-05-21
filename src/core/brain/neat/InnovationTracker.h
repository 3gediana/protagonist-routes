#pragma once

#include "core/brain/neat/NodeGene.h"
#include "core/types/Aliases.h"

#include <cstdint>
#include <unordered_map>
#include <utility>

namespace neuro {

struct InnovationTrackerState {
    std::uint64_t next_innovation_value{1};
    NodeId next_node_id{1};
};

class InnovationTracker {
public:
    explicit InnovationTracker(std::uint64_t next_innovation_value = 1, NodeId next_node_id = 1);

    InnovationId nextConnectionInnovation(NodeId from, NodeId to);
    NodeId nextNodeId();
    void ensureNextNodeIdAtLeast(NodeId minimum_next_node_id);
    void beginGeneration();

    InnovationId lastInnovationId() const;
    NodeId lastNodeId() const;
    InnovationTrackerState exportState() const;
    void importState(const InnovationTrackerState& state);

private:
    struct EdgeKey {
        NodeId from;
        NodeId to;

        bool operator==(const EdgeKey& other) const noexcept {
            return from == other.from && to == other.to;
        }
    };

    struct EdgeKeyHasher {
        std::size_t operator()(const EdgeKey& key) const noexcept;
    };

    InnovationId next_innovation_;
    NodeId next_node_id_;
    std::unordered_map<EdgeKey, InnovationId, EdgeKeyHasher> current_generation_cache_;
};

}  // namespace neuro
