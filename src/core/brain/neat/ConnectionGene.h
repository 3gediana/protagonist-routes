#pragma once

#include "core/brain/neat/NodeGene.h"
#include "core/types/Aliases.h"

namespace neuro {

struct ConnectionGene {
    NodeId from{0};
    NodeId to{0};
    float weight{0.0f};
    bool enabled{true};
    InnovationId innovation_id{kInvalidInnovationId};
};

}  // namespace neuro
