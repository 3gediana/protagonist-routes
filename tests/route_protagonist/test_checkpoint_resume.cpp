#include "core/config/Config.h"
#include "runtime/ProtagonistEcologyRuntime.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using neuro::config::Config;
using neuro::routes::protagonist::ProtagonistEcologyRuntime;

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::in | std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

std::vector<std::string> readDataLines(const std::filesystem::path& path) {
    std::ifstream stream(path);
    std::vector<std::string> lines;
    std::string line;
    bool first = true;
    while (std::getline(stream, line)) {
        if (first) {
            first = false;
            continue;
        }
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

std::vector<std::string> filterLinesByGeneration(const std::vector<std::string>& lines, std::size_t generation) {
    const auto prefix = std::to_string(generation) + ",";
    std::vector<std::string> filtered;
    for (const auto& line : lines) {
        if (line.rfind(prefix, 0) == 0) {
            filtered.push_back(line);
        }
    }
    return filtered;
}

std::filesystem::path onlyRunDir(const std::filesystem::path& runs_dir) {
    std::vector<std::filesystem::path> dirs;
    for (const auto& entry : std::filesystem::directory_iterator(runs_dir)) {
        if (entry.is_directory()) {
            dirs.push_back(entry.path());
        }
    }
    EXPECT_EQ(dirs.size(), 1u);
    return dirs.empty() ? runs_dir : dirs.front();
}

std::string makeConfig(const std::filesystem::path& runs_dir,
                       std::size_t generations,
                       std::size_t checkpoint_every,
                       std::string_view checkpoint_path = {}) {
    std::ostringstream toml;
    toml << "[runtime]\n";
    toml << "random_seed = 123\n";
    toml << "log_level = \"error\"\n";
    toml << "world_role = \"train\"\n";
    toml << "scenario_name = \"checkpoint_resume_test\"\n";
    if (!checkpoint_path.empty()) {
        toml << "resume_checkpoint_path = \"" << checkpoint_path << "\"\n";
    }

    toml << "\n[output]\n";
    toml << "runs_dir = \"" << runs_dir.generic_string() << "\"\n";

    toml << "\n[world]\n";
    toml << "size = [80.0, 80.0]\n";

    toml << "\n[world.resources]\n";
    toml << "count = 8\n";
    toml << "amount = 6.0\n";
    toml << "respawn_seconds = 4.0\n";

    toml << "\n[world.objects]\n";
    toml << "count = 8\n";

    toml << "\n[world.campfire]\n";
    toml << "count = 1\n";

    toml << "\n[world.water]\n";
    toml << "source_count = 1\n";

    toml << "\n[world.tree]\n";
    toml << "count = 3\n";

    toml << "\n[world.worksite]\n";
    toml << "count = 1\n";
    toml << "required_sticks = 2\n";
    toml << "required_stones = 1\n";
    toml << "assembly_rate = 0.2\n";

    toml << "\n[world.terrain]\n";
    toml << "enabled = false\n";

    toml << "\n[world.day_night]\n";
    toml << "enabled = false\n";

    toml << "\n[world.season]\n";
    toml << "enabled = false\n";

    toml << "\n[world.disease]\n";
    toml << "enabled = false\n";

    toml << "\n[world.intervention]\n";
    toml << "enabled = false\n";

    toml << "\n[world.progression]\n";
    toml << "enabled = false\n";

    toml << "\n[compute]\n";
    toml << "protagonist_dense_gpu_enabled = false\n";
    toml << "agent_decision_threads = 1\n";

    toml << "\n[simulation.time]\n";
    toml << "substep_dt = 0.2\n";
    toml << "episode_duration_seconds = 1.0\n";

    toml << "\n[population.protagonist]\n";
    toml << "count = 2\n";
    toml << "hidden_dim = 32\n";
    toml << "memory_cells = 8\n";
    toml << "memory_slots = 8\n";
    toml << "read_heads = 2\n";
    toml << "action_dim = 13\n";

    toml << "\n[population.background]\n";
    toml << "hidden_dim = 16\n";

    toml << "\n[[population.background.species]]\n";
    toml << "archetype = \"small_forager\"\n";
    toml << "count = 2\n";

    toml << "\n[[population.background.species]]\n";
    toml << "archetype = \"pack_hunter\"\n";
    toml << "count = 1\n";

    toml << "\n[evolution]\n";
    toml << "generations = " << generations << "\n";
    toml << "episodes_per_generation = 1\n";
    toml << "num_threads = 1\n";
    toml << "checkpoint_every = " << checkpoint_every << "\n";

    toml << "\n[evolution.selection]\n";
    toml << "elite_ratio = 0.5\n";
    toml << "tournament_size = 2\n";

    toml << "\n[evolution.mutation]\n";
    toml << "weight_perturb_rate = 0.5\n";
    toml << "weight_reset_rate = 0.02\n";
    toml << "weight_perturb_sigma = 0.3\n";

    toml << "\n[evolution.neat]\n";
    toml << "add_node_rate = 0.02\n";
    toml << "add_connection_rate = 0.05\n";
    toml << "toggle_connection_rate = 0.01\n";
    toml << "add_modulatory_rate = 0.0\n";
    toml << "compatibility_threshold = 3.0\n";

    return toml.str();
}

std::filesystem::path runTraining(const std::filesystem::path& runs_dir,
                                  std::size_t generations,
                                  std::size_t checkpoint_every,
                                  std::string_view checkpoint_path = {}) {
    std::filesystem::create_directories(runs_dir);
    auto config = Config::loadFromString(makeConfig(runs_dir, generations, checkpoint_every, checkpoint_path));
    ProtagonistEcologyRuntime runtime(std::move(config));
    EXPECT_EQ(runtime.run(), 0);
    return onlyRunDir(runs_dir);
}

}  // namespace

TEST(ProtagonistCheckpointResume, SavesBackgroundSpeciesStateAndManifest) {
    const auto temp_root = std::filesystem::temp_directory_path() / "neural_eco_protagonist_checkpoint_files";
    std::filesystem::remove_all(temp_root);
    const auto run_dir = runTraining(temp_root / "runs", 1, 1);
    const auto checkpoint_dir = run_dir / "checkpoint_gen0";

    ASSERT_TRUE(std::filesystem::exists(checkpoint_dir / "checkpoint_manifest.json"));
    ASSERT_TRUE(std::filesystem::exists(checkpoint_dir / "checkpoint_state.toml"));
    ASSERT_TRUE(std::filesystem::exists(checkpoint_dir / "best_genome_overall.bin"));
    ASSERT_TRUE(std::filesystem::exists(checkpoint_dir / "best_genome_latest.bin"));
    ASSERT_TRUE(std::filesystem::exists(checkpoint_dir / "background_species_0" / "meta.toml"));
    ASSERT_TRUE(std::filesystem::exists(checkpoint_dir / "background_species_1" / "meta.toml"));

    const auto manifest = readFile(checkpoint_dir / "checkpoint_manifest.json");
    EXPECT_NE(manifest.find("\"resume_supported\": \"full_population_state\""), std::string::npos);
    EXPECT_NE(manifest.find("\"background_species_resume\": true"), std::string::npos);
    EXPECT_NE(manifest.find("\"rng_state_resume\": true"), std::string::npos);
    EXPECT_NE(manifest.find("\"innovation_tracker_resume\": true"), std::string::npos);
    EXPECT_NE(manifest.find("\"speciation_state_resume\": true"), std::string::npos);
    EXPECT_NE(manifest.find("\"generation_index_resume\": true"), std::string::npos);

    const auto checkpoint_state = readFile(checkpoint_dir / "checkpoint_state.toml");
    EXPECT_NE(checkpoint_state.find("resume_generation = 1"), std::string::npos);
    EXPECT_NE(checkpoint_state.find("[[background_species]]"), std::string::npos);

    std::size_t background_genome_files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(checkpoint_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().parent_path().filename().string().starts_with("background_species_")
            && entry.path().filename().string().starts_with("genome_")) {
            ++background_genome_files;
        }
    }
    EXPECT_EQ(background_genome_files, 3u);
}

TEST(ProtagonistCheckpointResume, ResumeMatchesFullRunAtNextGeneration) {
    const auto temp_root = std::filesystem::temp_directory_path() / "neural_eco_protagonist_checkpoint_resume_compare";
    std::filesystem::remove_all(temp_root);

    const auto full_run_dir = runTraining(temp_root / "full", 2, 0);
    const auto partial_run_dir = runTraining(temp_root / "partial", 1, 1);
    const auto checkpoint_dir = partial_run_dir / "checkpoint_gen0";
    const auto resumed_run_dir = runTraining(temp_root / "resume", 2, 0, checkpoint_dir.generic_string());

    const auto full_evolution = readDataLines(full_run_dir / "evolution.csv");
    const auto resumed_evolution = readDataLines(resumed_run_dir / "evolution.csv");
    ASSERT_EQ(full_evolution.size(), 2u);
    ASSERT_EQ(resumed_evolution.size(), 1u);
    EXPECT_EQ(full_evolution[1], resumed_evolution[0]);

    const auto full_species = readDataLines(full_run_dir / "species_metrics.csv");
    const auto resumed_species = readDataLines(resumed_run_dir / "species_metrics.csv");
    EXPECT_EQ(filterLinesByGeneration(full_species, 1), resumed_species);
}
