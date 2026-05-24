#pragma once

#include "core/brain/neat/NodeGene.h"
#include "core/interfaces/IBrain.h"

#include <unordered_map>
#include <utility>
#include <vector>

namespace neuro {

class NeatGenome;

class NeatBrain final : public IBrain {
public:
    explicit NeatBrain(const NeatGenome& genome);

    std::size_t inputDim() const override;
    std::size_t outputDim() const override;
    std::span<const float> forward(std::span<const float> inputs, float dt) override;
    void reset() override;
    std::size_t parameterCount() const override;

    void hebbianUpdate(float learning_rate);
    std::vector<float> currentWeights() const;
    void setWeights(const std::vector<float>& weights);

    // Layer 8 Lamarckian inheritance: write the brain's current (possibly
    // Hebbian-updated) weights back into a NeatGenome. The genome must be
    // structurally identical to the one this brain was constructed from
    // (same nodes, same connections in the same order); typically this is
    // a clone of the source genome. Re-traverses connections in genome
    // order, matching them against the brain's incoming/modulatory lists
    // built in NeatBrain's constructor.
    void writeWeightsBackTo(NeatGenome& genome) const;

    std::size_t nodeCount() const { return node_evals_.size(); }
    std::size_t connectionCount() const { return connection_count_; }
    std::size_t hiddenCount() const {
        std::size_t h = 0;
        for (const auto& n : node_evals_) {
            if (!n.is_input && !n.is_output) ++h;
        }
        return h;
    }

private:
    struct NodeEval {
        bool is_input{false};
        bool is_output{false};
        bool is_modulatory{false};
        float bias{0.0f};
        float response{1.0f};
        float time_constant{1.0f};
        std::vector<std::pair<std::size_t, float>> incoming;
        std::vector<std::pair<std::size_t, float>> modulatory_incoming;
    };

    std::size_t input_dim_;
    std::size_t output_dim_;
    std::vector<NodeId> input_node_ids_;
    std::vector<NodeId> output_node_ids_;
    std::unordered_map<NodeId, std::size_t> node_index_by_id_;
    std::vector<NodeEval> node_evals_;
    std::vector<float> values_[2];
    std::size_t active_buffer_index_{0};
    std::vector<float> outputs_;
    std::size_t connection_count_{0};
};

}  // namespace neuro
