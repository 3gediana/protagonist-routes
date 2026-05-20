#include "core/reward/FoodReward.h"

#include "core/agent/Agent.h"
#include "core/world/layers/signal/SignalLayer.h"
#include "core/world/layers/signal/SignalEmitHistory.h"
#include "core/interfaces/IWorld.h"

namespace neuro {

FoodReward::FoodReward(double weight)
    : weight_(weight) {}

double FoodReward::evaluate(const IAgent& agent, const IWorld& world, float) const {
    if (const auto* concrete = dynamic_cast<const Agent*>(&agent)) {
        const double plant = static_cast<double>(concrete->plantFoodThisTick());
        const double meat = static_cast<double>(concrete->meatFoodThisTick());
        const double total = plant + meat;
        const double diet = static_cast<double>(concrete->dietPreference());
        const double match = total > 0.0
            ? (((1.0 - diet) * plant) + (diet * meat)) / total
            : 0.0;
        double base = total > 0.0
            ? total * (0.5 + 0.75 * match)
            : static_cast<double>(concrete->foodEatenThisTick());
        // Signal-guided foraging bonus: eating near a signal gives extra reward
        // This creates the closed loop: signal → approach → eat → reward
        if (base > 0.0) {
            const auto* signal = world.getLayer<SignalLayer>();
            if (signal) {
                const auto emit_records = signal->history().query(agent.position(), 10.0f, 4.0, world.currentTimeSeconds());
                float signal_strength = 0.0f;
                for (const auto& emit : emit_records) {
                    if (emit.source != agent.id()) {
                        signal_strength += emit.intensity;
                    }
                }
                if (signal_strength > 0.1f) {
                    base += std::min(1.0f, 0.5f + 0.5f * signal_strength);  // bonus for eating food guided by signal
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
