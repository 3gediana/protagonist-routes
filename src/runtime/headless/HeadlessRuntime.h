#pragma once
//
// runtime/headless/HeadlessRuntime.h
//
// Training-only runtime. No window, no rendering, no input. The world
// ticks as fast as the CPU allows; CSV / JSONL observers write progress
// to disk.
//
// M0: skeleton only — initialize / run / shutdown lifecycle.
// M1: actually drives the evolution loop with a fixed-topology population.

#include "core/config/Config.h"

#include <atomic>
#include <filesystem>

namespace neuro::runtime {

class HeadlessRuntime {
public:
    explicit HeadlessRuntime(config::Config config);
    ~HeadlessRuntime();

    HeadlessRuntime(const HeadlessRuntime&) = delete;
    HeadlessRuntime& operator=(const HeadlessRuntime&) = delete;

    // Set up world, population, observers. Idempotent.
    void initialize();

    // Block and run the training loop until completion or stop().
    // Returns 0 on success, non-zero on error (suitable for main()).
    int run();

    // Asynchronous stop request. Loop exits at the next tick boundary.
    void stop();

    // True if run() is currently executing.
    bool running() const { return running_.load(); }

private:
    config::Config        config_;
    std::filesystem::path run_dir_;
    std::atomic<bool>     running_{false};
    std::atomic<bool>     stop_requested_{false};
    bool                  initialized_{false};
};

}  // namespace neuro::runtime
