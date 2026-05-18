#include "core/observer/FitnessRecorder.h"

#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"

#include <filesystem>
#include <stdexcept>

namespace neuro {

FitnessRecorder::FitnessRecorder(const std::filesystem::path& output_file)
    : output_file_(output_file),
      subscription_id_(0) {}

FitnessRecorder::~FitnessRecorder() {
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
}

void FitnessRecorder::onAttach(IWorld& world) {
    std::filesystem::create_directories(output_file_.parent_path());
    const bool exists = std::filesystem::exists(output_file_);
    stream_.open(output_file_, std::ios::out | (exists ? std::ios::app : std::ios::trunc));
    if (!stream_.is_open()) {
        throw std::runtime_error("Failed to open fitness recorder output file");
    }

    if (!exists) {
        stream_ << "generation,min_fitness,avg_fitness,best_fitness\n";
    }
    subscription_id_ = world.eventBus().subscribe(events::EventType::GenerationEnded,
        [this](const events::Event& event) {
            const auto& payload = event.payloadAs<events::GenerationEnded>();
            stream_ << payload.gen << ','
                    << payload.min_fitness << ','
                    << payload.avg_fitness << ','
                    << payload.best_fitness << '\n';
            stream_.flush();
        });
}

void FitnessRecorder::onDetach(IWorld& world) {
    if (subscription_id_ != 0) {
        world.eventBus().unsubscribe(subscription_id_);
        subscription_id_ = 0;
    }
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
}

const char* FitnessRecorder::name() const {
    return "fitness_recorder";
}

}  // namespace neuro
