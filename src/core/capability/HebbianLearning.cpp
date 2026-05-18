#include "core/capability/HebbianLearning.h"

#include "core/agent/Agent.h"
#include "core/brain/neat/NeatBrain.h"
#include "core/brain/neat/NeatGenome.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IBrain.h"

namespace neuro {

HebbianLearning::HebbianLearning(float learning_rate)
    : learning_rate_(learning_rate) {}

std::size_t HebbianLearning::consumedOutputs() const {
    return 0;
}

void HebbianLearning::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    (void)world;
    (void)outputs;

    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr) return;

    auto* neat_brain = dynamic_cast<NeatBrain*>(&concrete->brain());
    if (neat_brain == nullptr) return;

    neat_brain->hebbianUpdate(learning_rate_ * dt);
}

const char* HebbianLearning::name() const {
    return "hebbian_learning";
}

}  // namespace neuro
