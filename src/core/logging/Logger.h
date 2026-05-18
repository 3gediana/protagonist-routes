#pragma once
//
// core/logging/Logger.h
//
// Thin wrapper over spdlog. All other code in the project should use the
// LOG_* macros below, never spdlog directly. This lets us swap the backend
// later without touching call sites.
//
// Usage:
//   neuro::log::initialize({"data/runs/2026-05-18/log.txt", "info"});
//   LOG_INFO("Hello, gen={} pop={}", 1, 100);
//
// Levels: trace < debug < info < warn < error < critical < off

#include <filesystem>
#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace neuro::log {

struct Options {
    std::string             level    = "info";          // string for TOML friendliness
    bool                    console  = true;
    std::filesystem::path   log_file = {};              // empty = no file sink
    bool                    truncate = true;            // overwrite vs append
};

// Initialize the global logger. Idempotent: subsequent calls reconfigure
// the existing logger rather than crashing.
void initialize(const Options& opts);

// Shut down: flush and drop sinks.
void shutdown();

// Change level at runtime. Accepts the same strings as Options::level.
void setLevel(const std::string& level);

// Underlying spdlog handle (used internally by macros).
spdlog::logger& get();

}  // namespace neuro::log

// ---------------------------------------------------------------------------
// LOG_* macros — preferred entry points.
// ---------------------------------------------------------------------------
#define LOG_TRACE(...)    ::neuro::log::get().trace(__VA_ARGS__)
#define LOG_DEBUG(...)    ::neuro::log::get().debug(__VA_ARGS__)
#define LOG_INFO(...)     ::neuro::log::get().info(__VA_ARGS__)
#define LOG_WARN(...)     ::neuro::log::get().warn(__VA_ARGS__)
#define LOG_ERROR(...)    ::neuro::log::get().error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::neuro::log::get().critical(__VA_ARGS__)
