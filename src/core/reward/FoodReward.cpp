#include "core/reward/FoodReward.h"

#include "core/agent/Agent.h"
#include "core/world/layers/signal/SignalLayer.h"
#include "core/interfaces/IWorld.h"

namespace neuro {

FoodReward::FoodReward(double weight)
    : weight_(weight) {}

double FoodReward::evaluate(const IAgent& agent, const IWorld& world, float) const {
    if (const auto* concrete = dynamic_cast<const Agent*>(&agent)) {
        double base = static_cast<double>(concrete->foodEatenThisTick());
        // Signal-guided foraging bonus: eating near a signal gives extra reward
        // This creates the closed loop: signal → approach → eat → reward
        if (base > 0.0) {
            const auto* signal = world.getLayer<SignalLayer>();
            if (signal) {
                auto samples = signal->sample(agent.position(), 10.0f);
                float signal_strength = 0.0f;
                for (float s : samples) signal_strength += std::abs(s);
                if (signal_strength > 0.1f) {
                    base += 0.5;  // bonus for eating food guided by signal
                }
            }
        }
        return base;
    }
    return 0.0;
}

double FoodReward::weight() const {
    return weight_;
}

const char* FoodReward::name() const {
    return "food";
}

}  // namespace neuro
