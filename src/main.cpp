// Neural Eco - Main entry point
//
// CLI dispatcher: parses --mode and routes to the appropriate runtime.
//
// Usage:
//   neural-eco --hello
//   neural-eco --mode headless    [--config configs/default.toml]
//   neural-eco --mode interactive [--config configs/default.toml]

#include "core/config/Config.h"
#include "core/logging/Logger.h"
#include "runtime/headless/HeadlessRuntime.h"
#ifdef NEURAL_ECO_HAS_VIZ
  #include "runtime/interactive/InteractiveRuntime.h"
#endif

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

struct CliArgs {
    std::string mode = "headless";              // "headless" | "interactive"
    std::string config_path = "configs/default.toml";
    bool        hello = false;
    bool        help  = false;
};

void printUsage(std::ostream& os) {
    os << "Neural Eco - A neuroevolution ecosystem.\n\n"
       << "Usage:\n"
       << "  neural-eco --hello\n"
       << "  neural-eco [--mode {headless|interactive}] [--config <path>]\n\n"
       << "Options:\n"
       << "  --hello                Print a hello banner and exit (smoke test).\n"
       << "  --mode <mode>          Runtime mode (default: headless).\n"
       << "  --config <path>        Path to TOML config (default: configs/default.toml).\n"
       << "  -h, --help             Show this help.\n";
}

CliArgs parseArgs(int argc, char** argv) {
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "--hello") {
            args.hello = true;
        } else if (a == "-h" || a == "--help") {
            args.help = true;
        } else if (a == "--mode" && i + 1 < argc) {
            args.mode = argv[++i];
        } else if (a == "--config" && i + 1 < argc) {
            args.config_path = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << a << "\n\n";
            args.help = true;
        }
    }
    return args;
}

}  // namespace

int main(int argc, char** argv) {
    const CliArgs args = parseArgs(argc, argv);

    if (args.hello) {
        std::cout << "Neural Eco v0.1 - Hello\n";
        return 0;
    }
    if (args.help) {
        printUsage(std::cout);
        return 0;
    }

    // ----- Config -----
    neuro::config::Config config;
    try {
        config = neuro::config::Config::loadFromFile(args.config_path);
    } catch (const std::exception& e) {
        std::cerr << "Config error: " << e.what() << "\n";
        return 2;
    }

    // ----- Logger -----
    neuro::log::Options log_opts;
    log_opts.level = config.getOr<std::string>("runtime.log_level", "info");
    log_opts.console = true;
    // For M0 we keep the log file path simple. M1 will add timestamp substitution.
    if (auto path = config.tryGet<std::string>("runtime.log_file"); path && !path->empty()) {
        // Replace {timestamp} placeholder with a no-op for now (M0).
        std::string p = *path;
        auto pos = p.find("{timestamp}");
        if (pos != std::string::npos) p.replace(pos, 11, "M0");
        log_opts.log_file = p;
    }
    neuro::log::initialize(log_opts);

    LOG_INFO("Neural Eco starting (mode={}, config={})",
             args.mode, args.config_path);

    // ----- Runtime dispatch -----
    int rc = 0;
    try {
        if (args.mode == "headless") {
            neuro::runtime::HeadlessRuntime rt(std::move(config));
            rt.initialize();
            rc = rt.run();
        } else if (args.mode == "interactive") {
#ifdef NEURAL_ECO_HAS_VIZ
            neuro::runtime::InteractiveRuntime rt(std::move(config));
            rt.initialize();
            rc = rt.run();
#else
            LOG_ERROR("Interactive mode requested but viz was not built. "
                      "Reconfigure CMake with NEURAL_ECO_BUILD_VIZ=ON.");
            rc = 3;
#endif
        } else {
            LOG_ERROR("Unknown --mode '{}'. Use 'headless' or 'interactive'.",
                      args.mode);
            rc = 4;
        }
    } catch (const std::exception& e) {
        LOG_CRITICAL("Unhandled exception: {}", e.what());
        rc = 1;
    }

    LOG_INFO("Neural Eco exiting (rc={})", rc);
    neuro::log::shutdown();
    return rc;
}
