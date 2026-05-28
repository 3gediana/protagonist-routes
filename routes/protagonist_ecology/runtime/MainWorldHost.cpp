#include "runtime/MainWorldHost.h"

#include "runtime/ProtagonistEcologyConfig.h"
#include "runtime/ProtagonistWorldFactory.h"

#include "core/world/World2D.h"

namespace neuro::routes::protagonist {

MainWorldHost::MainWorldHost(const ProtagonistEcologyConfig& route)
    : world_(createBootstrapWorld(route)) {
    // Intentionally empty: createBootstrapWorld already wires every
    // layer the protagonist runtime expects. We hold onto the
    // instance for the host's lifetime so it can be ticked without
    // ever being torn down — that is the LAYER6_THREE_WORLDS_SPEC
    // § 1.3 "永不 reset" invariant in practice.
}

MainWorldHost::~MainWorldHost() = default;

bool MainWorldHost::tick(SimTimeSeconds dt_seconds) {
    if (dt_seconds <= 0.0) {
        return false;
    }
    if (world_ == nullptr) {
        return false;
    }
    world_->tick(static_cast<float>(dt_seconds));
    ++tick_call_count_;
    return true;
}

SimTimeSeconds MainWorldHost::simTimeSeconds() const {
    if (world_ == nullptr) {
        return 0.0;
    }
    return world_->currentTimeSeconds();
}

}  // namespace neuro::routes::protagonist
