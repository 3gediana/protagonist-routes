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
