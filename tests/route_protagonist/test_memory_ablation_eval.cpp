#include "core/config/Config.h"
#include "runtime/BootstrapPopulation.h"
#include "runtime/MemoryAblationEval.h"
#include "runtime/ProtagonistEcologyConfig.h"

#include <gtest/gtest.h>

using neuro::config::Config;
using neuro::routes::protagonist::MemoryAblationTaskKind;
using neuro::routes::protagonist::configureMemoryAblationTask;
using neuro::routes::protagonist::loadMemoryAblationSettings;
using neuro::routes::protagonist::protagonistSensoryDim;
using neuro::routes::protagonist::ProtagonistEcologyConfig;

namespace {

constexpr const char* kConfigSample = R"(
[runtime]
random_seed = 42
world_role = "train"
scenario_name = "memory_ablation_test"

[world]
size = [200.0, 200.0]

[world.signal]
enabled = true
channels = 2

[world.objects]
enabled = true
count = 24

[world.base]
enabled = true

[world.nursery]
enabled = true

[world.nursery_supply]
enabled = true

[world.campfire]
enabled = true
count = 2

[world.water]
enabled = true
source_count = 2

[world.survival]
enabled = true

[world.tree]
enabled = true
count = 4

[world.worksite]
enabled = true
count = 1

[world.intervention]
enabled = true

[world.progression]
enabled = true

[world.day_night]
enabled = true

[world.season]
enabled = true

[world.disease]
enabled = true

[population.protagonist]
action_dim = 13
memory_cognition_enabled = true

[population.background]
forager_count = 4
predator_count = 2

[memory_ablation]
enabled = true
episodes_per_task = 7
trace_enabled = true
)";

}  // namespace

TEST(MemoryAblationEval, ParsesSettingsSection) {
    const Config config = Config::loadFromString(kConfigSample);
    const auto settings = loadMemoryAblationSettings(config);

    EXPECT_TRUE(settings.enabled);
    EXPECT_EQ(settings.episodes_per_task, 7u);
    EXPECT_TRUE(settings.trace_enabled);
}

TEST(MemoryAblationEval, TaskConfigsPreserveCheckpointShape) {
    const Config config = Config::loadFromString(kConfigSample);
    const auto base = ProtagonistEcologyConfig::fromConfig(config);
    const auto base_dim = protagonistSensoryDim(base);

    for (const auto task : {
             MemoryAblationTaskKind::WaterReturn,
             MemoryAblationTaskKind::DangerAvoidance,
             MemoryAblationTaskKind::WorksiteReturn,
             MemoryAblationTaskKind::SignalResponse,
         }) {
        const auto route = configureMemoryAblationTask(base, task);
        EXPECT_EQ(protagonistSensoryDim(route), base_dim);
        EXPECT_EQ(route.protagonist.action_dim, base.protagonist.action_dim);
    }
}

TEST(MemoryAblationEval, TaskSpecificTweaksMatchIntent) {
    const Config config = Config::loadFromString(kConfigSample);
    const auto base = ProtagonistEcologyConfig::fromConfig(config);

    const auto water = configureMemoryAblationTask(base, MemoryAblationTaskKind::WaterReturn);
    EXPECT_EQ(water.water.source_count, 1u);
    EXPECT_EQ(water.resources.count, 0u);
    EXPECT_LT(water.survival.initial_hydration_ratio, base.survival.initial_hydration_ratio);
    EXPECT_FALSE(water.signal.protagonist_emit_enabled);
    EXPECT_FALSE(water.protagonist.hunt_enabled);
    EXPECT_TRUE(water.protagonist.goal_action_assist_enabled);
    EXPECT_TRUE(water.water.direction_requires_visibility);

    const auto danger = configureMemoryAblationTask(base, MemoryAblationTaskKind::DangerAvoidance);
    EXPECT_EQ(danger.background.forager_count, 0u);
    EXPECT_GT(danger.background.predator_count, 0u);
    EXPECT_FALSE(danger.signal.protagonist_emit_enabled);
    EXPECT_FALSE(danger.protagonist.predator_warn_enabled);

    const auto worksite = configureMemoryAblationTask(base, MemoryAblationTaskKind::WorksiteReturn);
    EXPECT_TRUE(worksite.isLiveRole());
    EXPECT_EQ(worksite.worksite.count, 1u);
    EXPECT_FALSE(worksite.nursery.enabled);
    EXPECT_FALSE(worksite.signal.protagonist_emit_enabled);
    EXPECT_FALSE(worksite.protagonist.hunt_enabled);
    EXPECT_TRUE(worksite.protagonist.goal_action_assist_enabled);
    EXPECT_TRUE(worksite.worksite.direction_requires_visibility);

    const auto signal = configureMemoryAblationTask(base, MemoryAblationTaskKind::SignalResponse);
    EXPECT_TRUE(signal.signal.enabled);
    EXPECT_EQ(signal.resources.count, 0u);
    EXPECT_EQ(signal.worksite.count, 0u);
}
