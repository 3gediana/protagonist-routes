#include "core/logging/Logger.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <mutex>
#include <vector>

namespace neuro::log {

namespace {

constexpr const char* kLoggerName = "neuro";

std::mutex                       g_mutex;
std::shared_ptr<spdlog::logger>  g_logger;

spdlog::level::level_enum parseLevel(const std::string& s) {
    if (s == "trace")    return spdlog::level::trace;
    if (s == "debug")    return spdlog::level::debug;
    if (s == "info")     return spdlog::level::info;
    if (s == "warn"
     || s == "warning")  return spdlog::level::warn;
    if (s == "error")    return spdlog::level::err;
    if (s == "critical") return spdlog::level::critical;
    if (s == "off")      return spdlog::level::off;
    return spdlog::level::info;
}

std::shared_ptr<spdlog::logger> makeLogger(const Options& opts) {
    std::vector<spdlog::sink_ptr> sinks;

    if (opts.console) {
        auto cs = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        cs->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        sinks.push_back(std::move(cs));
    }

    if (!opts.log_file.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(opts.log_file.parent_path(), ec);
        auto fs = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            opts.log_file.string(), opts.truncate);
        fs->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        sinks.push_back(std::move(fs));
    }

    auto logger = std::make_shared<spdlog::logger>(
        kLoggerName, sinks.begin(), sinks.end());
    logger->set_level(parseLevel(opts.level));
    logger->flush_on(spdlog::level::warn);
    return logger;
}

}  // namespace

void initialize(const Options& opts) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_logger = makeLogger(opts);
    // Register so spdlog::get(kLoggerName) works elsewhere if needed.
    spdlog::drop(kLoggerName);
    spdlog::register_logger(g_logger);
}

void shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_logger) {
        g_logger->flush();
        spdlog::drop(kLoggerName);
        g_logger.reset();
    }
}

void setLevel(const std::string& level) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_logger) g_logger->set_level(parseLevel(level));
}

spdlog::logger& get() {
    // Hot path: no lock. Initialization races are caller's responsibility
    // (call initialize() once at startup before any LOG_*).
    if (!g_logger) {
        // Lazy fallback so log calls before initialize() don't crash.
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_logger) {
            Options defaults{};
            g_logger = makeLogger(defaults);
            spdlog::register_logger(g_logger);
        }
    }
    return *g_logger;
}

}  // namespace neuro::log
