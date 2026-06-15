#include "core/config/Config.h"
#include "core/logging/Logger.h"
#include "runtime/ProtagonistEcologyRuntime.h"

#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace {

struct CliArgs {
    std::string config_path = "configs/protagonist_ecology_bootstrap.toml";
    bool help = false;
};

CliArgs parseArgs(int argc, char** argv) {
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        if ((a == "-h" || a == "--help")) {
            args.help = true;
        } else if (a == "--config" && i + 1 < argc) {
            args.config_path = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << a << "\n";
            args.help = true;
        }
    }
    return args;
}

void printUsage(std::ostream& os) {
    os << "Usage:\n"
       << "  neural-eco-protagonist [--config <path>]\n";
}

}  // namespace

int main(int argc, char** argv) {
    const CliArgs args = parseArgs(argc, argv);
    if (args.help) {
        printUsage(std::cout);
        return 0;
    }

    neuro::config::Config config;
    try {
        config = neuro::config::Config::loadFromFile(args.config_path);
    } catch (const std::exception& e) {
        std::cerr << "Config error: " << e.what() << "\n";
        return 2;
    }

    neuro::log::Options log_opts;
    log_opts.level = config.getOr<std::string>("runtime.log_level", "info");
    log_opts.console = true;
    neuro::log::initialize(log_opts);

    int rc = 0;
    try {
        neuro::routes::protagonist::ProtagonistEcologyRuntime runtime(std::move(config));
        runtime.initialize();
        rc = runtime.run();
    } catch (const std::exception& e) {
        LOG_CRITICAL("Unhandled exception: {}", e.what());
        rc = 1;
    }

    neuro::log::shutdown();
    return rc;
}
