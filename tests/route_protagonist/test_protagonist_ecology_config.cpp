#include "core/config/Config.h"
#include "runtime/ProtagonistEcologyConfig.h"

#include <gtest/gtest.h>

using neuro::config::Config;
using neuro::routes::protagonist::ProtagonistEcologyConfig;

namespace {

constexpr const char* kConfigSample = R"(
[runtime]
random_seed = 42
world_role = "train"
scenario_name = "config_parse_test"

[world]
size = [120.0, 120.0]

[world.signal]
enabled = false
channels = 4
grid_resolution = 17
decay_rate = 0.2
diffusion_rate = 0.3

[world.water]
direction_requires_visibility = true

[world.worksite]
direction_requires_visibility = true

[population.protagonist]
predator_warn_enabled = false
hunt_enabled = false
)";

}  // namespace

TEST(ProtagonistEcologyConfig, ParsesSignalSection) {
    const Config config = Config::loadFromString(kConfigSample);
    const auto route = ProtagonistEcologyConfig::fromConfig(config);

    EXPECT_FALSE(route.signal.enabled);
    EXPECT_EQ(route.signal.channels, 4u);
    EXPECT_EQ(route.signal.grid_resolution, 17u);
    EXPECT_FLOAT_EQ(route.signal.decay_rate, 0.2f);
    EXPECT_FLOAT_EQ(route.signal.diffusion_rate, 0.3f);
    EXPECT_TRUE(route.water.direction_requires_visibility);
    EXPECT_TRUE(route.worksite.direction_requires_visibility);
    EXPECT_FALSE(route.protagonist.predator_warn_enabled);
    EXPECT_FALSE(route.protagonist.hunt_enabled);
}

TEST(ProtagonistEcologyConfig, ParsesControlledEmitterFlags) {
    constexpr const char* kSample = R"(
[runtime]
random_seed = 7
world_role = "train"
scenario_name = "controlled_emitter_test"

[world]
size = [200.0, 200.0]

[world.signal]
enabled = true
controlled_emitter_enabled = true
controlled_emitter_period_seconds = 18.5
controlled_emitter_offset_radius = 27.0
controlled_emitter_offset_jitter = 11.0
controlled_emitter_randomize_position = true
controlled_emitter_food_radius = 7.0
controlled_emitter_food_count = 3
controlled_emitter_food_amount = 14.0
)";

    const Config config = Config::loadFromString(kSample);
    const auto route = ProtagonistEcologyConfig::fromConfig(config);

    EXPECT_TRUE(route.signal.controlled_emitter_enabled);
    EXPECT_DOUBLE_EQ(route.signal.controlled_emitter_period_seconds, 18.5);
    EXPECT_FLOAT_EQ(route.signal.controlled_emitter_offset_radius, 27.0f);
    EXPECT_FLOAT_EQ(route.signal.controlled_emitter_offset_jitter, 11.0f);
    EXPECT_TRUE(route.signal.controlled_emitter_randomize_position);
    EXPECT_FLOAT_EQ(route.signal.controlled_emitter_food_radius, 7.0f);
    EXPECT_EQ(route.signal.controlled_emitter_food_count, 3u);
    EXPECT_FLOAT_EQ(route.signal.controlled_emitter_food_amount, 14.0f);
}

TEST(ProtagonistEcologyConfig, ControlledEmitterDefaultsToOff) {
    constexpr const char* kSample = R"(
[runtime]
random_seed = 1
world_role = "train"
scenario_name = "controlled_emitter_default_test"

[world]
size = [120.0, 120.0]
)";

    const Config config = Config::loadFromString(kSample);
    const auto route = ProtagonistEcologyConfig::fromConfig(config);

    EXPECT_FALSE(route.signal.controlled_emitter_enabled);
    EXPECT_FALSE(route.signal.controlled_emitter_randomize_position);
    EXPECT_DOUBLE_EQ(route.signal.controlled_emitter_period_seconds, 30.0);
    EXPECT_GT(route.signal.controlled_emitter_food_amount, 0.0f);
}

// L6 R3 training audit (2026-05-25): goal_completion_rewards array
// expanded from 13 to 16 to add slot 13 (ThrowAttempt penalty); slots
// 14/15 reserved. Default value is 0 (off) so existing configs are
// unaffected.
TEST(ProtagonistEcologyConfig, GoalCompletionRewardsArrayHas16SlotsAndThrowDefaultsToZero) {
    constexpr const char* kSample = R"(
[runtime]
random_seed = 42
world_role = "train"
scenario_name = "throw_penalty_default_test"

[world]
size = [120.0, 120.0]
)";
    const Config config = Config::loadFromString(kSample);
    const auto route = ProtagonistEcologyConfig::fromConfig(config);

    // Compile-time array width.
    static_assert(std::tuple_size_v<decltype(route.goal_completion_rewards)> == 16u,
                  "goal_completion_rewards must have 16 slots (13 GoalType + ThrowAttempt + 2 reserved)");
    // Defaults: slot 8 (Hunt) = 10.0, slot 13 (ThrowAttempt) = 0.0 unless toml overrides.
    EXPECT_FLOAT_EQ(route.goal_completion_rewards[8], 10.0f);
    EXPECT_FLOAT_EQ(route.goal_completion_rewards[13], 0.0f);
    EXPECT_FLOAT_EQ(route.goal_completion_rewards[14], 0.0f);
    EXPECT_FLOAT_EQ(route.goal_completion_rewards[15], 0.0f);
}

// L6 R3 training audit: throw_attempt slot is bound to a toml key.
// Setting it to a negative value must round-trip into the config struct.
TEST(ProtagonistEcologyConfig, GoalCompletionRewardsThrowAttemptParsesNegativeFromToml) {
    constexpr const char* kSample = R"(
[runtime]
random_seed = 42
world_role = "train"
scenario_name = "throw_penalty_set_test"

[world]
size = [120.0, 120.0]

[reward.goal_completion]
throw_attempt = -0.5
)";
    const Config config = Config::loadFromString(kSample);
    const auto route = ProtagonistEcologyConfig::fromConfig(config);
    EXPECT_FLOAT_EQ(route.goal_completion_rewards[13], -0.5f);
    // Other slots untouched - hunt default still 10.0.
    EXPECT_FLOAT_EQ(route.goal_completion_rewards[8], 10.0f);
}
