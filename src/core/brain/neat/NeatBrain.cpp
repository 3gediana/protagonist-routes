#include "core/brain/neat/NeatBrain.h"

#include "core/brain/neat/NeatGenome.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace neuro {

namespace {

float activate(float x) {
    return std::tanh(x);
}

}  // namespace

NeatBrain::NeatBrain(const NeatGenome& genome)
    : input_dim_(genome.inputDim()),
      output_dim_(genome.outputDim()),
      input_node_ids_(genome.inputNodeIds()),
      output_node_ids_(genome.outputNodeIds()),
      outputs_(output_dim_, 0.0f),
      connection_count_(genome.connectionCount()) {
    node_evals_.reserve(genome.nodeCount());
    for (const auto& node : genome.nodes()) {
        const std::size_t index = node_evals_.size();
        node_index_by_id_.emplace(node.id, index);
        node_evals_.push_back(NodeEval{
            node.type == NodeGeneType::Input,
            node.type == NodeGeneType::Output,
            node.type == NodeGeneType::Modulatory,
            node.bias,
            node.response,
            std::max(node.time_constant, 1.0e-3f),
            {},
            {},
        });
    }

    for (const auto& connection : genome.connections()) {
        if (!connection.enabled) {
            continue;
        }

        const auto source_it = node_index_by_id_.find(connection.from);
        const auto target_it = node_index_by_id_.find(connection.to);
        if (source_it == node_index_by_id_.end() || target_it == node_index_by_id_.end()) {
            throw std::runtime_error("NeatBrain connection references missing node");
        }

        const std::size_t source_idx = source_it->second;
        const std::size_t target_idx = target_it->second;

        if (node_evals_[source_idx].is_modulatory) {
            node_evals_[target_idx].modulatory_incoming.emplace_back(source_idx, connection.weight);
        } else {
            node_evals_[target_idx].incoming.emplace_back(source_idx, connection.weight);
        }
    }

    values_[0].assign(node_evals_.size(), 0.0f);
    values_[1].assign(node_evals_.size(), 0.0f);
}

std::size_t NeatBrain::inputDim() const {
    return input_dim_;
}

std::size_t NeatBrain::outputDim() const {
    return output_dim_;
}

std::span<const float> NeatBrain::forward(std::span<const float> inputs, float dt) {
    if (inputs.size() != input_dim_) {
        throw std::invalid_argument("NeatBrain input dim mismatch");
    }

    const float step = dt > 0.0f ? dt : 1.0f;
    auto& current = values_[active_buffer_index_];
    auto& next = values_[1 - active_buffer_index_];
    next = current;

    for (std::size_t i = 0; i < input_node_ids_.size(); ++i) {
        const auto node_it = node_index_by_id_.find(input_node_ids_[i]);
        if (node_it == node_index_by_id_.end()) {
            throw std::runtime_error("NeatBrain input node id missing from node table");
        }
        current[node_it->second] = inputs[i];
        next[node_it->second] = inputs[i];
    }

    for (std::size_t index = 0; index < node_evals_.size(); ++index) {
        const auto& node = node_evals_[index];
        if (node.is_input) {
            continue;
        }

        float modulation = 0.0f;
        for (const auto& [source_index, weight] : node.modulatory_incoming) {
            modulation += current[source_index] * weight;
        }
        const float mod_factor = 1.0f + std::tanh(modulation);

        float sum = 0.0f;
        for (const auto& [source_index, weight] : node.incoming) {
            sum += current[source_index] * weight * mod_factor;
        }

        const float activated = activate(node.bias + node.response * sum);
        const float alpha = std::clamp(step / node.time_constant, 0.0f, 1.0f);
        next[index] = current[index] + alpha * (-current[index] + activated);
        if (!std::isfinite(next[index])) next[index] = 0.0f;
    }

    active_buffer_index_ = 1 - active_buffer_index_;
    const auto& active = values_[active_buffer_index_];
    for (std::size_t o = 0; o < output_node_ids_.size(); ++o) {
        const auto node_it = node_index_by_id_.find(output_node_ids_[o]);
        if (node_it == node_index_by_id_.end()) {
            throw std::runtime_error("NeatBrain output node id missing from node table");
        }
        float val = active[node_it->second];
        outputs_[o] = std::isfinite(val) ? val : 0.0f;
    }

    return outputs_;
}

void NeatBrain::reset() {
    std::fill(values_[0].begin(), values_[0].end(), 0.0f);
    std::fill(values_[1].begin(), values_[1].end(), 0.0f);
    std::fill(outputs_.begin(), outputs_.end(), 0.0f);
    active_buffer_index_ = 0;
}

std::size_t NeatBrain::parameterCount() const {
    return node_evals_.size() + connection_count_;
}

void NeatBrain::hebbianUpdate(float learning_rate) {
    const auto& current = values_[active_buffer_index_];

    for (std::size_t index = 0; index < node_evals_.size(); ++index) {
        auto& node = node_evals_[index];
        if (node.is_input) continue;

        const float post = current[index];
        for (auto& [source_index, weight] : node.incoming) {
            const float pre = current[source_index];
            weight += learning_rate * pre * post;
            weight = std::clamp(weight, -5.0f, 5.0f);
        }
    }
}

std::vector<float> NeatBrain::currentWeights() const {
    std::vector<float> weights;
    for (const auto& node : node_evals_) {
        if (node.is_input) continue;
        for (const auto& [_, weight] : node.incoming) {
            weights.push_back(weight);
        }
    }
    return weights;
}

void NeatBrain::setWeights(const std::vector<float>& weights) {
    std::size_t wi = 0;
    for (auto& node : node_evals_) {
        if (node.is_input) continue;
        for (auto& [_, weight] : node.incoming) {
            if (wi < weights.size()) {
                weight = weights[wi++];
            }
        }
    }
}

}  // namespace neuro
