#include "capability/ChooseBuildingTypeCapability.h"

#include "core/agent/Agent.h"

namespace neuro::routes::protagonist {

void ChooseBuildingTypeCapability::apply(IAgent& agent,
                                         IWorld& world,
                                         std::span<const float> outputs,
                                         float dt) {
    (void)world;
    (void)dt;
    if (outputs.size() < 4) return;

    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr) return;

    // Argmax over the 4 outputs. No softmax temperature; we only need the
    // ranking, and the brain's last-layer activation already shapes
    // magnitudes. Ties go to the lowest index (Wall) which matches the
    // worksite layer's seeded default.
    std::size_t best = 0;
    float best_val = outputs[0];
    for (std::size_t i = 1; i < 4; ++i) {
        if (outputs[i] > best_val) {
            best_val = outputs[i];
            best = i;
        }
    }
    concrete->setBuildingTypeChoice(static_cast<std::uint8_t>(best));
}

}  // namespace neuro::routes::protagonist
