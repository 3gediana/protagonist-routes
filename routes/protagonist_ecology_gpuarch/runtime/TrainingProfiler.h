#pragma once

// TrainingProfiler · profile-only timer (B-stage L2 GPU decision support)
//
// Lightweight RAII timer + atomic accumulator to find the real CPU bottleneck
// of protagonist training. Used to decide if it is worth GPU-offloading
// brain forward (current implementation only offloads dense matmul and the
// host<->device sync overhead actually makes GPU build slower than CPU,
// see docx/STATUS.md s 4.5). Always-on, atomic-add overhead is negligible
// (~50 ns/call), kept in code so future runs can re-measure after
// reward/perception changes.
//
// Phases tracked:
//   kBrainForward         : ProtagonistBrain::forward (dense matmul + DNC step)
//   kWorldTick            : world.tick(dt) - all IWorldLayer ticks
//                           (AgentLayer = 100 invokes capability::apply
//                            which itself drives brain forward, so kWorldTick
//                            includes brain time; subtract kBrainForward for
//                            non-brain world cost)
//   kProtagonistReward    : per-tick protagonist reward computation loop
//   kBackgroundReward     : per-tick background-species reward computation
//
// Usage:
//   {
//       TrainingProfiler::Scope scope(TrainingProfiler::Phase::kBrainForward);
//       ... heavy code ...
//   }
//   ...
//   profiler.flushGeneration();   // log + reset

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace neuro::routes::protagonist {

class TrainingProfiler {
public:
    enum class Phase : std::size_t {
        kBrainForward = 0,
        kWorldTick = 1,
        kProtagonistReward = 2,
        kBackgroundReward = 3,
        kPhaseCount = 4
    };

    static TrainingProfiler& instance() {
        static TrainingProfiler inst;
        return inst;
    }

    class Scope {
    public:
        explicit Scope(Phase p)
            : phase_(p),
              start_(std::chrono::high_resolution_clock::now()) {}
        ~Scope() {
            const auto end = std::chrono::high_resolution_clock::now();
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
            TrainingProfiler::instance().counters_[static_cast<std::size_t>(phase_)]
                .fetch_add(static_cast<std::int64_t>(us), std::memory_order_relaxed);
        }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        Phase phase_;
        std::chrono::high_resolution_clock::time_point start_;
    };

    void resetGeneration() {
        for (auto& c : counters_) {
            c.store(0, std::memory_order_relaxed);
        }
    }

    std::int64_t getMicroseconds(Phase p) const {
        return counters_[static_cast<std::size_t>(p)].load(std::memory_order_relaxed);
    }

private:
    TrainingProfiler() = default;
    std::array<std::atomic<std::int64_t>,
               static_cast<std::size_t>(Phase::kPhaseCount)> counters_{};
};

}  // namespace neuro::routes::protagonist
