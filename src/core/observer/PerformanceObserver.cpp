#include "core/observer/PerformanceObserver.h"

#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"

#include <filesystem>
#include <stdexcept>

namespace neuro {

PerformanceObserver::PerformanceObserver(const std::filesystem::path& output_file)
    : output_file_(output_file),
      world_(nullptr),
      subscription_id_(0) {
    std::filesystem::create_directories(output_file_.parent_path());
    const bool needs_header = !std::filesystem::exists(output_file_);
    stream_.open(output_file_, std::ios::out | std::ios::app);
    if (!stream_.is_open()) {
        throw std::runtime_error("Failed to open performance observer output file");
    }
    if (needs_header) {
        stream_ << "generation,total_ms,eval_ms,repro_ms\n";
        stream_.flush();
    }
}

PerformanceObserver::~PerformanceObserver() {
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
}

void PerformanceObserver::onAttach(IWorld& world) {
    world_ = &world;
}

void PerformanceObserver::onDetach(IWorld& world) {
    (void)world;
    world_ = nullptr;
}

const char* PerformanceObserver::name() const {
    return "performance_observer";
}

void PerformanceObserver::markGenerationStart() {
    gen_start_ = Clock::now();
    eval_ms_ = 0.0;
    repro_ms_ = 0.0;
}

void PerformanceObserver::markEvaluationStart() {
    eval_start_ = Clock::now();
}

void PerformanceObserver::markEvaluationEnd() {
    const auto now = Clock::now();
    eval_ms_ = std::chrono::duration<double, std::milli>(now - eval_start_).count();
}

void PerformanceObserver::markReproductionStart() {
    repro_start_ = Clock::now();
}

void PerformanceObserver::markReproductionEnd() {
    const auto now = Clock::now();
    repro_ms_ = std::chrono::duration<double, std::milli>(now - repro_start_).count();
}

void PerformanceObserver::finishGeneration(Generation generation) {
    writeRecord(generation);
}

void PerformanceObserver::writeRecord(Generation generation) {
    const auto now = Clock::now();
    const double total_ms = std::chrono::duration<double, std::milli>(now - gen_start_).count();
    stream_ << generation << ','
            << total_ms << ','
            << eval_ms_ << ','
            << repro_ms_ << '\n';
    stream_.flush();
}

}  // namespace neuro
