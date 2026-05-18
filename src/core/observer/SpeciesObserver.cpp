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
      world_(nullptr),
      subscription_id_(0) {}

SpeciesObserver::~SpeciesObserver() {
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
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

    subscription_id_ = world.eventBus().subscribe(events::EventType::GenerationEnded,
        [this](const events::Event& event) {
            const auto& payload = event.payloadAs<events::GenerationEnded>();
            writeCurrentGenerationSnapshot(payload.gen);
        });
}

void SpeciesObserver::onDetach(IWorld& world) {
    if (subscription_id_ != 0) {
        world.eventBus().unsubscribe(subscription_id_);
        subscription_id_ = 0;
    }
    world_ = nullptr;
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
}

const char* SpeciesObserver::name() const {
    return "species_observer";
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
    }

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
