#include "core/snapshot/SnapshotIO.h"

#include "core/logging/Logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace neuro {

namespace {

std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y%m%d-%H%M%S");
    return oss.str();
}

}  // namespace

bool SnapshotIO::save(const std::filesystem::path& snapshot_dir,
                      const std::vector<std::unique_ptr<IGenome>>& population,
                      const InnovationTracker& tracker,
                      Generation generation,
                      const std::string& brain_type,
                      const std::string& label) {
    std::filesystem::create_directories(snapshot_dir);

    SnapshotMeta meta;
    meta.timestamp = currentTimestamp();
    meta.generation = generation;
    meta.population_size = population.size();
    meta.brain_type = brain_type;
    meta.label = label;

    if (!writeMeta(snapshot_dir / "meta.toml", meta)) {
        LOG_ERROR("SnapshotIO: failed to write meta");
        return false;
    }

    if (!writePopulation(snapshot_dir / "population.bin", population, tracker, generation)) {
        LOG_ERROR("SnapshotIO: failed to write population");
        return false;
    }

    LOG_INFO("SnapshotIO: saved snapshot at gen {} to {}", generation, snapshot_dir.string());
    return true;
}

bool SnapshotIO::load(const std::filesystem::path& snapshot_dir,
                      std::vector<std::unique_ptr<IGenome>>& population,
                      InnovationTracker& tracker,
                      Generation& generation) {
    if (!readPopulation(snapshot_dir / "population.bin", population, tracker, generation)) {
        LOG_ERROR("SnapshotIO: failed to read population from {}", snapshot_dir.string());
        return false;
    }

    LOG_INFO("SnapshotIO: loaded snapshot from gen {} ({} genomes) from {}",
             generation, population.size(), snapshot_dir.string());
    return true;
}

SnapshotMeta SnapshotIO::readMeta(const std::filesystem::path& snapshot_dir) {
    SnapshotMeta meta;
    const auto meta_file = snapshot_dir / "meta.toml";
    if (!std::filesystem::exists(meta_file)) return meta;

    std::ifstream in(meta_file);
    if (!in.is_open()) return meta;

    std::string line;
    while (std::getline(in, line)) {
        if (line.find("generation") != std::string::npos) {
            const auto pos = line.find('=');
            if (pos != std::string::npos) {
                meta.generation = static_cast<Generation>(std::stoul(line.substr(pos + 1)));
            }
        } else if (line.find("population_size") != std::string::npos) {
            const auto pos = line.find('=');
            if (pos != std::string::npos) {
                meta.population_size = std::stoul(line.substr(pos + 1));
            }
        } else if (line.find("brain_type") != std::string::npos) {
            const auto pos = line.find('=');
            if (pos != std::string::npos) {
                auto val = line.substr(pos + 1);
                // Trim quotes and whitespace
                val.erase(0, val.find_first_not_of(" \t\""));
                val.erase(val.find_last_not_of(" \t\"") + 1);
                meta.brain_type = val;
            }
        } else if (line.find("timestamp") != std::string::npos) {
            const auto pos = line.find('=');
            if (pos != std::string::npos) {
                auto val = line.substr(pos + 1);
                val.erase(0, val.find_first_not_of(" \t\""));
                val.erase(val.find_last_not_of(" \t\"") + 1);
                meta.timestamp = val;
            }
        } else if (line.find("label") != std::string::npos) {
            const auto pos = line.find('=');
            if (pos != std::string::npos) {
                auto val = line.substr(pos + 1);
                val.erase(0, val.find_first_not_of(" \t\""));
                val.erase(val.find_last_not_of(" \t\"") + 1);
                meta.label = val;
            }
        }
    }
    return meta;
}

std::vector<std::filesystem::path> SnapshotIO::listSnapshots(const std::filesystem::path& base_dir) {
    std::vector<std::filesystem::path> result;
    if (!std::filesystem::exists(base_dir)) return result;

    for (const auto& entry : std::filesystem::directory_iterator(base_dir)) {
        if (entry.is_directory() && std::filesystem::exists(entry.path() / "meta.toml")) {
            result.push_back(entry.path());
        }
    }
    return result;
}

bool SnapshotIO::writeMeta(const std::filesystem::path& meta_file, const SnapshotMeta& meta) {
    std::ofstream out(meta_file, std::ios::out | std::ios::trunc);
    if (!out.is_open()) return false;

    out << "timestamp = \"" << meta.timestamp << "\"\n"
        << "generation = " << meta.generation << "\n"
        << "population_size = " << meta.population_size << "\n"
        << "brain_type = \"" << meta.brain_type << "\"\n"
        << "label = \"" << meta.label << "\"\n";
    out.close();
    return true;
}

bool SnapshotIO::writePopulation(const std::filesystem::path& pop_file,
                                 const std::vector<std::unique_ptr<IGenome>>& population,
                                 const InnovationTracker& tracker,
                                 Generation generation) {
    std::ofstream out(pop_file, std::ios::out | std::ios::binary);
    if (!out.is_open()) return false;

    // Write generation
    out.write(reinterpret_cast<const char*>(&generation), sizeof(generation));

    // Write InnovationTracker state
    const auto next_innov = tracker.lastInnovationId();
    const auto next_node = tracker.lastNodeId();
    out.write(reinterpret_cast<const char*>(&next_innov), sizeof(next_innov));
    out.write(reinterpret_cast<const char*>(&next_node), sizeof(next_node));

    // Write population size
    const std::uint64_t pop_size = population.size();
    out.write(reinterpret_cast<const char*>(&pop_size), sizeof(pop_size));

    // Write each genome
    for (const auto& genome_ptr : population) {
        const auto* neat = dynamic_cast<const NeatGenome*>(genome_ptr.get());
        if (!neat) continue;

        // GenomeId
        const auto gid_val = neat->id().value;
        out.write(reinterpret_cast<const char*>(&gid_val), sizeof(std::uint64_t));

        // input_dim, output_dim
        const auto in_dim = neat->inputDim();
        const auto out_dim = neat->outputDim();
        out.write(reinterpret_cast<const char*>(&in_dim), sizeof(in_dim));
        out.write(reinterpret_cast<const char*>(&out_dim), sizeof(out_dim));

        // decision_interval, immunity_gain_rate
        const float di = neat->decisionInterval();
        const float igr = neat->immunityGainRate();
        out.write(reinterpret_cast<const char*>(&di), sizeof(di));
        out.write(reinterpret_cast<const char*>(&igr), sizeof(igr));

        // Nodes
        const std::uint64_t node_count = neat->nodes().size();
        out.write(reinterpret_cast<const char*>(&node_count), sizeof(node_count));
        for (const auto& node : neat->nodes()) {
            out.write(reinterpret_cast<const char*>(&node.id), sizeof(node.id));
            const int type_int = static_cast<int>(node.type);
            out.write(reinterpret_cast<const char*>(&type_int), sizeof(type_int));
            out.write(reinterpret_cast<const char*>(&node.bias), sizeof(node.bias));
            out.write(reinterpret_cast<const char*>(&node.response), sizeof(node.response));
            out.write(reinterpret_cast<const char*>(&node.time_constant), sizeof(node.time_constant));
        }

        // Connections
        const std::uint64_t conn_count = neat->connections().size();
        out.write(reinterpret_cast<const char*>(&conn_count), sizeof(conn_count));
        for (const auto& conn : neat->connections()) {
            out.write(reinterpret_cast<const char*>(&conn.from), sizeof(conn.from));
            out.write(reinterpret_cast<const char*>(&conn.to), sizeof(conn.to));
            out.write(reinterpret_cast<const char*>(&conn.weight), sizeof(conn.weight));
            out.write(reinterpret_cast<const char*>(&conn.enabled), sizeof(conn.enabled));
            out.write(reinterpret_cast<const char*>(&conn.innovation_id.value), sizeof(std::uint64_t));
        }
    }

    out.close();
    return true;
}

bool SnapshotIO::readPopulation(const std::filesystem::path& pop_file,
                               std::vector<std::unique_ptr<IGenome>>& population,
                               InnovationTracker& tracker,
                               Generation& generation) {
    std::ifstream in(pop_file, std::ios::in | std::ios::binary);
    if (!in.is_open()) return false;

    // Read generation
    in.read(reinterpret_cast<char*>(&generation), sizeof(generation));

    // Read InnovationTracker state
    std::uint64_t next_innov_val;
    std::uint32_t next_node_val;
    in.read(reinterpret_cast<char*>(&next_innov_val), sizeof(next_innov_val));
    in.read(reinterpret_cast<char*>(&next_node_val), sizeof(next_node_val));
    tracker = InnovationTracker(next_innov_val, next_node_val);

    // Read population size
    std::uint64_t pop_size;
    in.read(reinterpret_cast<char*>(&pop_size), sizeof(pop_size));

    population.clear();
    population.reserve(pop_size);

    for (std::uint64_t i = 0; i < pop_size; ++i) {
        // GenomeId
        std::uint64_t gid;
        in.read(reinterpret_cast<char*>(&gid), sizeof(gid));

        // input_dim, output_dim
        std::size_t in_dim, out_dim;
        in.read(reinterpret_cast<char*>(&in_dim), sizeof(in_dim));
        in.read(reinterpret_cast<char*>(&out_dim), sizeof(out_dim));

        // decision_interval, immunity_gain_rate
        float di, igr;
        in.read(reinterpret_cast<char*>(&di), sizeof(di));
        in.read(reinterpret_cast<char*>(&igr), sizeof(igr));

        // Nodes
        std::uint64_t node_count;
        in.read(reinterpret_cast<char*>(&node_count), sizeof(node_count));
        std::vector<NodeGene> nodes(node_count);
        for (auto& node : nodes) {
            in.read(reinterpret_cast<char*>(&node.id), sizeof(node.id));
            int type_int;
            in.read(reinterpret_cast<char*>(&type_int), sizeof(type_int));
            node.type = static_cast<NodeGeneType>(type_int);
            in.read(reinterpret_cast<char*>(&node.bias), sizeof(node.bias));
            in.read(reinterpret_cast<char*>(&node.response), sizeof(node.response));
            in.read(reinterpret_cast<char*>(&node.time_constant), sizeof(node.time_constant));
        }

        // Connections
        std::uint64_t conn_count;
        in.read(reinterpret_cast<char*>(&conn_count), sizeof(conn_count));
        std::vector<ConnectionGene> conns(conn_count);
        for (auto& conn : conns) {
            in.read(reinterpret_cast<char*>(&conn.from), sizeof(conn.from));
            in.read(reinterpret_cast<char*>(&conn.to), sizeof(conn.to));
            in.read(reinterpret_cast<char*>(&conn.weight), sizeof(conn.weight));
            in.read(reinterpret_cast<char*>(&conn.enabled), sizeof(conn.enabled));
            in.read(reinterpret_cast<char*>(&conn.innovation_id.value), sizeof(std::uint64_t));
        }

        auto genome = std::make_unique<NeatGenome>(
            GenomeId{gid}, in_dim, out_dim,
            std::move(nodes), std::move(conns), di);
        genome->setImmunityGainRate(igr);
        population.push_back(std::move(genome));
    }

    in.close();
    return true;
}

}  // namespace neuro
