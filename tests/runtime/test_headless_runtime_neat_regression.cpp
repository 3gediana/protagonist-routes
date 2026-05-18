#include "runtime/headless/HeadlessRuntime.h"

#include "core/config/Config.h"

#include <gtest/gtest.h>

#include <filesystem>

TEST(HeadlessRuntime, NeatAcceptanceSeed44DoesNotCorruptGenome) {
    const auto temp_dir = std::filesystem::temp_directory_path() / "neural_eco_m3_regression_seed44";
    std::filesystem::create_directories(temp_dir);

    const auto toml = std::string{}
        + "[runtime]\n"
        + "mode = \"headless\"\n"
        + "random_seed = 44\n"
        + "log_level = \"error\"\n"
        + "\n"
        + "[world]\n"
        + "size = [80.0, 80.0]\n"
        + "\n"
        + "[world.resources]\n"
        + "count = 16\n"
        + "amount = 12.0\n"
        + "respawn_ticks = 20\n"
        + "\n"
        + "[population]\n"
        + "size = 24\n"
        + "input_dim = 5\n"
        + "output_dim = 2\n"
        + "\n"
        + "[brain]\n"
        + "type = \"neat\"\n"
        + "hidden_layer_size = 8\n"
        + "\n"
        + "[brain.ctrnn]\n"
        + "enabled = true\n"
        + "tau_min = 0.1\n"
        + "tau_max = 5.0\n"
        + "\n"
        + "[evolution]\n"
        + "generations = 100\n"
        + "episode_ticks = 80\n"
        + "\n"
        + "[evolution.selection]\n"
        + "elite_ratio = 0.25\n"
        + "tournament_size = 2\n"
        + "\n"
        + "[evolution.mutation]\n"
        + "weight_perturb_rate = 0.8\n"
        + "weight_reset_rate = 0.05\n"
        + "add_node_rate = 0.03\n"
        + "add_connection_rate = 0.05\n"
        + "toggle_connection_rate = 0.01\n"
        + "weight_perturb_sigma = 0.5\n"
        + "\n"
        + "[evolution.speciation]\n"
        + "enabled = true\n"
        + "compatibility_threshold = 3.0\n"
        + "c1_excess = 1.0\n"
        + "c2_disjoint = 1.0\n"
        + "c3_weight = 0.4\n"
        + "\n"
        + "[reward]\n"
        + "survival_weight = 1.0\n"
        + "food_weight = 10.0\n"
        + "intrinsic_weight = 1.5\n"
        + "\n"
        + "[energy]\n"
        + "initial = 40.0\n"
        + "base_metabolism = 0.01\n"
        + "decision_cost = 0.01\n"
        + "movement_cost_factor = 0.01\n"
        + "death_threshold = 0.0\n"
        + "\n"
        + "[agent]\n"
        + "max_speed = 1.5\n"
        + "eat_radius = 3.0\n"
        + "exploration_grid_size = 10\n"
        + "\n"
        + "[output]\n"
        + "runs_dir = \"" + temp_dir.generic_string() + "\"\n";

    auto config = neuro::config::Config::loadFromString(toml);
    neuro::runtime::HeadlessRuntime runtime(std::move(config));
    runtime.initialize();

    EXPECT_EQ(runtime.run(), 0);
}
