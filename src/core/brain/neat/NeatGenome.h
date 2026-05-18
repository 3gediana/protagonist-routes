#pragma once

#include "core/brain/neat/ConnectionGene.h"
#include "core/brain/neat/NodeGene.h"
#include "core/interfaces/IGenome.h"

#include <cstddef>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

namespace neuro {

class InnovationTracker;

class NeatGenome final : public IGenome {
public:
    NeatGenome(GenomeId id,
               std::size_t input_dim,
               std::size_t output_dim,
               std::vector<NodeGene> nodes,
               std::vector<ConnectionGene> connections,
               float decision_interval = 1.0f);

    GenomeId id() const override;
    std::unique_ptr<IGenome> clone() const override;
    std::size_t complexity() const override;

    void setId(GenomeId id);

    std::size_t inputDim() const;
    std::size_t outputDim() const;
    std::size_t nodeCount() const;
    std::size_t connectionCount() const;

    const std::vector<NodeGene>& nodes() const;
    const std::vector<ConnectionGene>& connections() const;
    std::vector<NodeGene>& nodes();
    std::vector<ConnectionGene>& connections();

    const NodeGene* findNode(NodeId id) const;
    NodeGene* findNode(NodeId id);
    bool hasConnection(NodeId from, NodeId to) const;
    const ConnectionGene* findConnection(InnovationId innovation_id) const;

    std::vector<NodeId> inputNodeIds() const;
    std::vector<NodeId> outputNodeIds() const;

    float decisionInterval() const;
    void setDecisionInterval(float interval);
    void setWeights(const std::vector<float>& weights);

    static std::unique_ptr<NeatGenome> minimal(GenomeId id,
                                               std::size_t input_dim,
                                               std::size_t output_dim,
                                               InnovationTracker& tracker,
                                               std::mt19937& rng,
                                               float time_constant_min = 0.1f,
                                               float time_constant_max = 5.0f,
                                               float decision_interval_min = 1.0f,
                                               float decision_interval_max = 30.0f);

private:
    GenomeId id_;
    std::size_t input_dim_;
    std::size_t output_dim_;
    std::vector<NodeGene> nodes_;
    std::vector<ConnectionGene> connections_;
    float decision_interval_;
};

}  // namespace neuro
