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
const std::array<MemoryAblationTaskKind, 4>& memoryAblationTasks();
const std::array<MemoryAblationVariant, 5>& memoryAblationVariants();
ProtagonistEcologyConfig configureMemoryAblationTask(const ProtagonistEcologyConfig& base,
                                                     MemoryAblationTaskKind task);

}  // namespace neuro::routes::protagonist
