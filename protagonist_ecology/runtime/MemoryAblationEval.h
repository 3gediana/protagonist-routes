#pragma once

#include "core/config/Config.h"
#include "runtime/ProtagonistEcologyConfig.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace neuro::routes::protagonist {

enum class MemoryAblationTaskKind : std::uint8_t {
    WaterReturn = 0,
    DangerAvoidance,
    WorksiteReturn,
    SignalResponse,
    // A3.S2-S4 communication semantics micro-tasks.
    // CommScoutWorker: controlled emitter "scout" beacons over food drops;
    // protagonist "worker" must follow the signal to reach the food. Scores
    // worker arrival count + total reward.
    CommScoutWorker,
    // CommDangerWarning: controlled emitter beacons "predator nearby" but
    // hides the predator from direct perception. Listener must use the
    // signal to flee. Scores death count + distance-from-emitter.
    CommDangerWarning,
    // CommBuildRequest: worksite needs materials; emitter beacons near the
    // worksite while protagonist carrying stick/stone must follow signal to
    // deposit. Scores deposit count + worksite completion events.
    CommBuildRequest,
};

struct MemoryAblationSettings {
    bool enabled{false};
    std::size_t episodes_per_task{4};
    bool trace_enabled{false};
};

struct MemoryAblationVariant {
    std::string_view name{};
    bool spatial_memory_writes_enabled{true};
    bool episodic_memory_writes_enabled{true};
    bool social_memory_writes_enabled{true};
    bool goal_manager_enabled{true};
};

MemoryAblationSettings loadMemoryAblationSettings(const config::Config& config);
std::string_view memoryAblationTaskName(MemoryAblationTaskKind task);
const std::array<MemoryAblationTaskKind, 7>& memoryAblationTasks();
const std::array<MemoryAblationVariant, 5>& memoryAblationVariants();
ProtagonistEcologyConfig configureMemoryAblationTask(const ProtagonistEcologyConfig& base,
                                                     MemoryAblationTaskKind task);

}  // namespace neuro::routes::protagonist
