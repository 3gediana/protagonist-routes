#pragma once
//
// runtime/interactive/InteractiveRuntime.h
//
// Demo runtime. Owns a window via raylib (added in M2), drives the world
// at a fixed frame rate, and routes user input to interventions.
//
// M0: skeleton only. M2 fills in raylib initialization, render loop,
// hot keys, and panel widgets.

#include "core/config/Config.h"
#include "core/events/EventBus.h"

#include <atomic>
#include <memory>

namespace neuro::runtime {

class InteractiveRuntime {
public:
    explicit InteractiveRuntime(config::Config config);
    ~InteractiveRuntime();

    InteractiveRuntime(const InteractiveRuntime&) = delete;
    InteractiveRuntime& operator=(const InteractiveRuntime&) = delete;

    void initialize();
    int  run();
    void stop();

    bool running() const { return running_.load(); }

private:
    config::Config        config_;
    events::EventBus      bus_;
    std::atomic<bool>     running_{false};
    std::atomic<bool>     stop_requested_{false};
    bool                  initialized_{false};
};

}  // namespace neuro::runtime
