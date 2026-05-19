#include "core/observer/SpeciesObserver.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <vector>

namespace neuro {

namespace {

struct SpeciesSnapshot {
    SpeciesId id{kInvalidSpeciesId};
    std::size_t members{0};
    std::size_t living{0};
    double fitness_sum{0.0};
    double best_fitness{0.0};
};

}  // namespace

SpeciesObserver::SpeciesObserver(const std::filesystem::path& output_file)
    : output_file_(output_file),
      phylo_file_(output_file.parent_path() / "phylo_tree.jsonl"),
      world_(nullptr),
      subscription_id_(0) {}

SpeciesObserver::~SpeciesObserver() {
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
    if (phylo_stream_.is_open()) {
        phylo_stream_.flush();
        phylo_stream_.close();
    }
}

void SpeciesObserver::onAttach(IWorld& world) {
    world_ = &world;
    std::filesystem::create_directories(output_file_.parent_path());

    const bool exists = std::filesystem::exists(output_file_);
    stream_.open(output_file_, std::ios::out | (exists ? std::ios::app : std::ios::trunc));
    if (!stream_.is_open()) {
        throw std::runtime_error("Failed to open species observer output file");
    }

    const bool phylo_exists = std::filesystem::exists(phylo_file_);
    phylo_stream_.open(phylo_file_, std::ios::out | (phylo_exists ? std::ios::app : std::ios::trunc));
    if (!phylo_stream_.is_open()) {
        throw std::runtime_error("Failed to open phylo tree output file");
    }

    subscription_id_ = world.eventBus().subscribe(events::EventType::GenerationEnded,
        [this](const events::Event& event) {
            const auto& payload = event.payloadAs<events::GenerationEnded>();
            writeCurrentGenerationSnapshot(payload.gen);
        });
}

void SpeciesObserver::onDetach(IWorld& world) {
    // Write extinction events for any remaining species
    for (auto& [id, record] : phylo_records_) {
        if (!record.extinct) {
            record.extinct = true;
            record.death_gen = record.birth_gen + static_cast<Generation>(record.total_generations);
            writePhyloTreeRecord(record, "species_extinct");
        }
    }

    if (subscription_id_ != 0) {
        world.eventBus().unsubscribe(subscription_id_);
        subscription_id_ = 0;
    }
    world_ = nullptr;
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
    if (phylo_stream_.is_open()) {
        phylo_stream_.flush();
        phylo_stream_.close();
    }
}

const char* SpeciesObserver::name() const {
    return "species_observer";
}

const std::unordered_map<std::uint64_t, SpeciesRecord>& SpeciesObserver::phyloRecords() const {
    return phylo_records_;
}

void SpeciesObserver::detectSpeciesBirthExtinction(
    Generation generation,
    const std::unordered_set<std::uint64_t>& current_species) {

    // Detect new species (birth events)
    for (const auto sid : current_species) {
        if (phylo_records_.find(sid) == phylo_records_.end()) {
            SpeciesRecord record;
            record.id = SpeciesId{sid};
            record.birth_gen = generation;
            record.extinct = false;

            // Determine parent: closest species ID from previous generation
            std::uint64_t best_parent = 0;
            if (!prev_gen_species_.empty()) {
                std::uint64_t min_diff = std::numeric_limits<std::uint64_t>::max();
                for (const auto pid : prev_gen_species_) {
                    const auto diff = sid > pid ? sid - pid : pid - sid;
                    if (diff < min_diff && diff > 0) {
                        min_diff = diff;
                        best_parent = pid;
                    }
                }
            }
            record.parent_id = best_parent > 0 ? SpeciesId{best_parent} : kInvalidSpeciesId;

            phylo_records_[sid] = record;
            writePhyloTreeRecord(record, "species_born");
        }
        auto& rec = phylo_records_[sid];
        rec.total_generations++;
    }

    // Detect extinct species
    for (const auto sid : prev_gen_species_) {
        if (current_species.find(sid) == current_species.end()) {
            auto& rec = phylo_records_[sid];
            if (!rec.extinct) {
                rec.extinct = true;
                rec.death_gen = generation;
                writePhyloTreeRecord(rec, "species_extinct");
            }
        }
    }

    prev_gen_species_ = current_species;
}

void SpeciesObserver::writePhyloTreeRecord(const SpeciesRecord& record, const char* event_type) {
    if (!phylo_stream_.is_open()) return;

    phylo_stream_ << "{\"event\":\"" << event_type
                  << "\",\"id\":" << record.id.value
                  << ",\"parent_id\":" << (record.parent_id == kInvalidSpeciesId ? -1 : static_cast<std::int64_t>(record.parent_id.value))
                  << ",\"birth_gen\":" << record.birth_gen
                  << ",\"death_gen\":" << (record.extinct ? static_cast<std::int64_t>(record.death_gen) : -1)
                  << ",\"peak_fitness\":" << record.peak_fitness
                  << ",\"peak_size\":" << record.peak_size
                  << ",\"total_gens\":" << record.total_generations
                  << "}\n";
    phylo_stream_.flush();
}

void SpeciesObserver::writeCurrentGenerationSnapshot(Generation generation) {
    if (world_ == nullptr) {
        throw std::runtime_error("SpeciesObserver is not attached to a world");
    }

    const auto* agent_layer = world_->getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        throw std::runtime_error("SpeciesObserver requires AgentLayer");
    }

    std::map<SpeciesId, SpeciesSnapshot> snapshots;
    std::unordered_set<std::uint64_t> current_species_ids;

    for (const Agent* agent : agent_layer->agents()) {
        const SpeciesId species_id = agent->speciesId();
        if (species_id == kInvalidSpeciesId) {
            continue;
        }

        auto& snapshot = snapshots[species_id];
        snapshot.id = species_id;
        snapshot.members += 1;
        snapshot.living += agent->alive() ? 1u : 0u;
        snapshot.fitness_sum += agent->fitness();
        snapshot.best_fitness = snapshot.members == 1
            ? agent->fitness()
            : std::max(snapshot.best_fitness, agent->fitness());

        current_species_ids.insert(static_cast<std::uint64_t>(species_id.value));
    }

    // Update peak stats in phylo records
    for (const auto& [species_id, snapshot] : snapshots) {
        auto it = phylo_records_.find(static_cast<std::uint64_t>(species_id.value));
        if (it != phylo_records_.end()) {
            auto& rec = it->second;
            if (snapshot.best_fitness > rec.peak_fitness) {
                rec.peak_fitness = snapshot.best_fitness;
            }
            if (snapshot.members > rec.peak_size) {
                rec.peak_size = snapshot.members;
            }
        }
    }

    // Detect birth/extinction events
    detectSpeciesBirthExtinction(generation, current_species_ids);

    // Write species_alive events with current peak stats
    for (const auto& [sid, record] : phylo_records_) {
        if (!record.extinct) {
            writePhyloTreeRecord(record, "species_alive");
        }
    }

    // Write standard snapshot
    stream_ << "{\"generation\":" << generation
            << ",\"species_count\":" << snapshots.size()
            << ",\"species\":[";

    bool first = true;
    for (const auto& [species_id, snapshot] : snapshots) {
        if (!first) {
            stream_ << ',';
        }
        first = false;
        const double avg_fitness = snapshot.members == 0
            ? 0.0
            : snapshot.fitness_sum / static_cast<double>(snapshot.members);
        stream_ << "{\"id\":" << species_id.value
                << ",\"members\":" << snapshot.members
                << ",\"living\":" << snapshot.living
                << ",\"avg_fitness\":" << avg_fitness
                << ",\"best_fitness\":" << snapshot.best_fitness
                << '}';
    }

    stream_ << "]}\n";
    stream_.flush();
}

}  // namespace neuro
