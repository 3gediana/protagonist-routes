#include "runtime/headless/HeadlessRuntime.h"

#include "core/config/Config.h"

#include <gtest/gtest.h>

#include <filesystem>

TEST(HeadlessRuntime, SmokeRunSingleGeneration) {
    const auto temp_dir = std::filesystem::temp_directory_path() / "neural_eco_m1_smoke";
    std::filesystem::create_directories(temp_dir);

    const auto toml = std::string{}
        + "[runtime]\n"
        + "mode = \"headless\"\n"
        + "random_seed = 42\n"
        + "log_level = \"error\"\n"
        + "\n"
        + "[world]\n"
        + "size = [50.0, 50.0]\n"
        + "\n"
        + "[world.resources]\n"
        + "count = 8\n"
        + "amount = 10.0\n"
        + "respawn_ticks = 10\n"
        + "\n"
        + "[population]\n"
        + "size = 8\n"
        + "input_dim = 5\n"
        + "output_dim = 2\n"
        + "\n"
        + "[brain]\n"
        + "hidden_layer_size = 6\n"
        + "\n"
        + "[evolution]\n"
        + "generations = 1\n"
        + "episode_ticks = 20\n"
        + "\n"
        + "[evolution.selection]\n"
        + "elite_ratio = 0.25\n"
        + "tournament_size = 2\n"
        + "\n"
        + "[evolution.mutation]\n"
        + "weight_perturb_rate = 0.8\n"
        + "weight_reset_rate = 0.05\n"
        + "weight_perturb_sigma = 0.5\n"
        + "\n"
        + "[reward]\n"
        + "survival_weight = 1.0\n"
        + "food_weight = 10.0\n"
        + "intrinsic_weight = 1.0\n"
        + "\n"
        + "[energy]\n"
        + "initial = 50.0\n"
        + "base_metabolism = 0.01\n"
        + "decision_cost = 0.01\n"
        + "movement_cost_factor = 0.01\n"
        + "death_threshold = 0.0\n"
        + "\n"
        + "[agent]\n"
        + "max_speed = 1.0\n"
        + "eat_radius = 3.0\n"
        + "exploration_grid_size = 10\n"
        + "\n"
        + "[output]\n"
        + "runs_dir = \"" + temp_dir.generic_string() + "\"\n";

    auto config = neuro::config::Config::loadFromString(toml);
    neuro::runtime::HeadlessRuntime runtime(std::move(config));
    runtime.initialize();

    EXPECT_EQ(runtime.run(), 0);

    bool found_fitness_csv = false;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(temp_dir)) {
        if (entry.is_regular_file() && entry.path().filename() == "fitness.csv") {
            found_fitness_csv = true;
            break;
        }
    }
    EXPECT_TRUE(found_fitness_csv);
}
