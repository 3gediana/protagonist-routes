#pragma once

#include <cstdint>

namespace neuro {

enum class NodeGeneType {
    Input,
    Hidden,
    Output,
    Modulatory,
};

using NodeId = std::uint32_t;

struct NodeGene {
    NodeId id{0};
    NodeGeneType type{NodeGeneType::Hidden};
    float bias{0.0f};
    float response{1.0f};
    float time_constant{1.0f};
};

}  // namespace neuro
