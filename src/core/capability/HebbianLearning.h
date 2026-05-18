#pragma once

#include "core/interfaces/IAgentCapability.h"

namespace neuro {

class HebbianLearning final : public IAgentCapability {
public:
    HebbianLearning(float learning_rate);

    std::size_t consumedOutputs() const override;
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override;

    float learningRate() const { return learning_rate_; }

private:
    float learning_rate_;
};

}  // namespace neuro
