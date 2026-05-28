#pragma once

#include "core/types/Aliases.h"

#include <cstddef>
#include <memory>

namespace neuro {
class World2D;
}

namespace neuro::routes::protagonist {

class ProtagonistEcologyConfig;

// Layer 6 R3 stage 4 (LAYER6_THREE_WORLDS_SPEC § 1.3 + § 2.5,
// PRODUCTION_WORLD_LIFECYCLE_SPEC invariant #1): persistent main-world
// host. Owns a single World2D that is created once at host
// construction and NEVER reset between training generations. This
// realizes the "主世界永不 reset" invariant.
//
// Scope of this commit (matches the spec § 6 priority-2 goal "MainWorld
// 不 reset 但内存中"):
//   - own one World2D instance via createBootstrapWorld
//   - tick(dt) advances physics without ever recreating the world
//   - the underlying World2D itself tracks the cumulative sim_time
//     and tick counter, so monotonicity falls out automatically
//
// Out of scope (explicitly deferred per spec § 6 priority-3 "序列化到 disk"):
//   - serialize the World2D state to disk
//   - reload after process restart
//
// Out of scope (R3 stage 5 and beyond, to be wired in follow-up commits):
//   - automatic agent spawning from borderland→main_world promotion
//     (callers can pull world() and seed agents themselves for now)
//   - in-world reproduction loop
//   - fitness-window snapshots
//
// The host is intentionally tiny so that it can be unit-tested
// without dragging in the rest of the training runtime.
class MainWorldHost {
public:
    // Constructs a fresh persistent main world from `route`. The
    // ProtagonistEcologyConfig must outlive this host only for the
    // duration of the constructor; the host does not store the config
    // (it stashes everything it needs into the World2D directly via
    // createBootstrapWorld).
    explicit MainWorldHost(const ProtagonistEcologyConfig& route);
    ~MainWorldHost();

    MainWorldHost(const MainWorldHost&) = delete;
    MainWorldHost& operator=(const MainWorldHost&) = delete;
    MainWorldHost(MainWorldHost&&) = delete;
    MainWorldHost& operator=(MainWorldHost&&) = delete;

    // Advance the persistent main world by dt_seconds of simulated
    // time. Negative / zero dt is a no-op (returns false). The world
    // is NEVER recreated by this call; subsequent calls always
    // continue from the prior state — that is the "永不 reset"
    // invariant. Returns true if the tick actually ran.
    bool tick(SimTimeSeconds dt_seconds);

    // Cumulative simulated seconds advanced via tick(). Sourced from
    // the underlying World2D's own monotonic clock so we don't risk
    // double-bookkeeping. Returns 0 if the world failed to construct.
    SimTimeSeconds simTimeSeconds() const;

    // Number of successful tick() calls (dt_seconds > 0) that have
    // run against the world.
    std::size_t tickCallCount() const { return tick_call_count_; }

    // Direct access to the owned World2D for callers that need to
    // inspect or interact with layers (e.g. tests, future migration
    // hooks). Stable for the host's lifetime.
    World2D* world() { return world_.get(); }
    const World2D* world() const { return world_.get(); }

private:
    std::unique_ptr<World2D> world_;
    std::size_t tick_call_count_{0};
};

}  // namespace neuro::routes::protagonist
