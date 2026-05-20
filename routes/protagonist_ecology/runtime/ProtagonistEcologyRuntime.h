#pragma once

#include "core/config/Config.h"

#include <atomic>
#include <memory>

namespace neuro {
class World2D;
}

namespace neuro::routes::protagonist {

class ProtagonistEcologyRuntime {
public:
    explicit ProtagonistEcologyRuntime(config::Config config);
    ~ProtagonistEcologyRuntime();

    ProtagonistEcologyRuntime(const ProtagonistEcologyRuntime&) = delete;
    ProtagonistEcologyRuntime& operator=(const ProtagonistEcologyRuntime&) = delete;

    void initialize();
    int run();
    void stop();

    bool running() const { return running_.load(); }

private:
    int runTraining();

    config::Config config_;
    std::unique_ptr<World2D> world_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    bool initialized_{false};
};

}  // namespace neuro::routes::protagonist
