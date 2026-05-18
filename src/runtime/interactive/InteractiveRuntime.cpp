#include "runtime/interactive/InteractiveRuntime.h"

#include "core/logging/Logger.h"

#include <utility>

namespace neuro::runtime {

InteractiveRuntime::InteractiveRuntime(config::Config config)
    : config_(std::move(config)) {}

InteractiveRuntime::~InteractiveRuntime() {
    if (running_.load()) stop();
}

void InteractiveRuntime::initialize() {
    if (initialized_) return;

    LOG_INFO("InteractiveRuntime: initialize (config={})",
             config_.path().string());

    // M0 placeholder. M2 will:
    //   1. Create raylib window from config (viz.window_width/height)
    //   2. Load CJK font (data/fonts/...)
    //   3. Initialize i18n with config viz.language
    //   4. Construct renderer + UI panels
    //   5. Same world + population init as HeadlessRuntime

    initialized_ = true;
}

int InteractiveRuntime::run() {
    if (!initialized_) initialize();

    running_.store(true);
    stop_requested_.store(false);
    LOG_INFO("InteractiveRuntime: run starting (M0 stub - exits immediately)");

    // M0 stub: nothing to render, exit so the binary doesn't hang.
    LOG_INFO("InteractiveRuntime: run finished");
    running_.store(false);
    return 0;
}

void InteractiveRuntime::stop() {
    stop_requested_.store(true);
    LOG_INFO("InteractiveRuntime: stop requested");
}

}  // namespace neuro::runtime
