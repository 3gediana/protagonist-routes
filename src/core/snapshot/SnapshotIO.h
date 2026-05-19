#pragma once

#include "core/brain/neat/NeatGenome.h"
#include "core/brain/neat/InnovationTracker.h"
#include "core/types/Aliases.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace neuro {

struct SnapshotMeta {
    std::string timestamp;
    Generation generation{0};
    std::size_t population_size{0};
    std::string brain_type;
    std::string label;
};

struct PopulationSnapshot {
    std::vector<NeatGenome> genomes;
    InnovationTracker innovation_tracker;
    Generation generation{0};
};

class SnapshotIO {
public:
    // Save a population snapshot to disk
    static bool save(const std::filesystem::path& snapshot_dir,
                     const std::vector<std::unique_ptr<IGenome>>& population,
                     const InnovationTracker& tracker,
                     Generation generation,
                     const std::string& brain_type,
                     const std::string& label = "");

    // Load a population snapshot from disk
    static bool load(const std::filesystem::path& snapshot_dir,
                     std::vector<std::unique_ptr<IGenome>>& population,
                     InnovationTracker& tracker,
                     Generation& generation);

    // Read only the metadata
    static SnapshotMeta readMeta(const std::filesystem::path& snapshot_dir);

    // List all snapshots in a directory
    static std::vector<std::filesystem::path> listSnapshots(const std::filesystem::path& base_dir);

private:
    static bool writeMeta(const std::filesystem::path& meta_file, const SnapshotMeta& meta);
    static bool writePopulation(const std::filesystem::path& pop_file,
                                const std::vector<std::unique_ptr<IGenome>>& population,
                                const InnovationTracker& tracker,
                                Generation generation);
    static bool readPopulation(const std::filesystem::path& pop_file,
                               std::vector<std::unique_ptr<IGenome>>& population,
                               InnovationTracker& tracker,
                               Generation& generation);
};

}  // namespace neuro
