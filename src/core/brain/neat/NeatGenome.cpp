#include "core/brain/neat/NeatGenome.h"

#include "core/brain/neat/InnovationTracker.h"

#include <algorithm>
#include <stdexcept>

namespace neuro {

namespace {

float sampleWeight(std::mt19937& rng) {
    std::normal_distribution<float> dist(0.0f, 1.0f);
    return dist(rng);
}

float sampleTimeConstant(std::mt19937& rng, float min_value, float max_value) {
    std::uniform_real_distribution<float> dist(min_value, max_value);
    return dist(rng);
}

}  // namespace

NeatGenome::NeatGenome(GenomeId id,
                       std::size_t input_dim,
                       std::size_t output_dim,
                       std::vector<NodeGene> nodes,
                       std::vector<ConnectionGene> connections,
                       float decision_interval)
    : id_(id),
      input_dim_(input_dim),
      output_dim_(output_dim),
      nodes_(std::move(nodes)),
      connections_(std::move(connections)),
      decision_interval_(decision_interval) {
    if (input_dim_ == 0 || output_dim_ == 0) {
        throw std::invalid_argument("NeatGenome input/output dims must be > 0");
    }
    if (nodes_.empty()) {
        throw std::invalid_argument("NeatGenome requires at least one node");
    }
}

GenomeId NeatGenome::id() const {
    return id_;
}

std::unique_ptr<IGenome> NeatGenome::clone() const {
    return std::make_unique<NeatGenome>(*this);
}

std::size_t NeatGenome::complexity() const {
    return nodes_.size() + connections_.size();
}

void NeatGenome::setId(GenomeId id) {
    id_ = id;
}

std::size_t NeatGenome::inputDim() const {
    return input_dim_;
}

std::size_t NeatGenome::outputDim() const {
    return output_dim_;
}

std::size_t NeatGenome::nodeCount() const {
    return nodes_.size();
}

std::size_t NeatGenome::connectionCount() const {
    return connections_.size();
}

const std::vector<NodeGene>& NeatGenome::nodes() const {
    return nodes_;
}

const std::vector<ConnectionGene>& NeatGenome::connections() const {
    return connections_;
}

std::vector<NodeGene>& NeatGenome::nodes() {
    return nodes_;
}

std::vector<ConnectionGene>& NeatGenome::connections() {
    return connections_;
}

const NodeGene* NeatGenome::findNode(NodeId id) const {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(), [id](const NodeGene& node) {
        return node.id == id;
    });
    return it == nodes_.end() ? nullptr : &(*it);
}

NodeGene* NeatGenome::findNode(NodeId id) {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(), [id](const NodeGene& node) {
        return node.id == id;
    });
    return it == nodes_.end() ? nullptr : &(*it);
}

bool NeatGenome::hasConnection(NodeId from, NodeId to) const {
    return std::any_of(connections_.begin(), connections_.end(), [from, to](const ConnectionGene& connection) {
        return connection.from == from && connection.to == to;
    });
}

const ConnectionGene* NeatGenome::findConnection(InnovationId innovation_id) const {
    const auto it = std::find_if(connections_.begin(), connections_.end(), [innovation_id](const ConnectionGene& connection) {
        return connection.innovation_id == innovation_id;
    });
    return it == connections_.end() ? nullptr : &(*it);
}

std::vector<NodeId> NeatGenome::inputNodeIds() const {
    std::vector<NodeId> ids;
    ids.reserve(input_dim_);
    for (const auto& node : nodes_) {
        if (node.type == NodeGeneType::Input) {
            ids.push_back(node.id);
        }
    }
    return ids;
}

std::vector<NodeId> NeatGenome::outputNodeIds() const {
    std::vector<NodeId> ids;
    ids.reserve(output_dim_);
    for (const auto& node : nodes_) {
        if (node.type == NodeGeneType::Output) {
            ids.push_back(node.id);
        }
    }
    return ids;
}

float NeatGenome::decisionInterval() const {
    return decision_interval_;
}

void NeatGenome::setDecisionInterval(float interval) {
    decision_interval_ = interval;
}

void NeatGenome::setWeights(const std::vector<float>& weights) {
    std::size_t wi = 0;
    for (auto& connection : connections_) {
        if (connection.enabled && wi < weights.size()) {
            connection.weight = weights[wi++];
        }
    }
}

std::unique_ptr<NeatGenome> NeatGenome::minimal(GenomeId id,
                                                std::size_t input_dim,
                                                std::size_t output_dim,
                                                InnovationTracker& tracker,
                                                std::mt19937& rng,
                                                float time_constant_min,
                                                float time_constant_max,
                                                float decision_interval_min,
                                                float decision_interval_max) {
    if (input_dim == 0 || output_dim == 0) {
        throw std::invalid_argument("NeatGenome::minimal requires input/output dims > 0");
    }

    std::vector<NodeGene> nodes;
    nodes.reserve(input_dim + output_dim);

    std::vector<NodeId> input_nodes;
    std::vector<NodeId> output_nodes;
    input_nodes.reserve(input_dim);
    output_nodes.reserve(output_dim);

    for (std::size_t i = 0; i < input_dim; ++i) {
        const NodeId node_id = static_cast<NodeId>(i + 1);
        input_nodes.push_back(node_id);
        nodes.push_back(NodeGene{node_id, NodeGeneType::Input, 0.0f, 1.0f, 1.0f});
    }

    for (std::size_t o = 0; o < output_dim; ++o) {
        const NodeId node_id = static_cast<NodeId>(input_dim + o + 1);
        output_nodes.push_back(node_id);
        nodes.push_back(NodeGene{node_id,
                                 NodeGeneType::Output,
                                 sampleWeight(rng),
                                 1.0f,
                                 sampleTimeConstant(rng, time_constant_min, time_constant_max)});
    }

    tracker.ensureNextNodeIdAtLeast(static_cast<NodeId>(input_dim + output_dim + 1));

    std::vector<ConnectionGene> connections;
    connections.reserve(input_dim * output_dim);
    for (NodeId in_node : input_nodes) {
        for (NodeId out_node : output_nodes) {
            connections.push_back(ConnectionGene{
                in_node,
                out_node,
                sampleWeight(rng),
                true,
                tracker.nextConnectionInnovation(in_node, out_node),
            });
        }
    }

    std::uniform_real_distribution<float> interval_dist(decision_interval_min, decision_interval_max);
    const float decision_interval = interval_dist(rng);

    return std::make_unique<NeatGenome>(id, input_dim, output_dim, std::move(nodes), std::move(connections), decision_interval);
}

}  // namespace neuro
