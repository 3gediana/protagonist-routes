#include "core/agent/Agent.h"
#include "core/interfaces/IBrain.h"
#include "core/observer/ModuleMetricsObserver.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

class ModuleMetricsDummyBrain final : public neuro::IBrain {
public:
    std::size_t inputDim() const override { return 0; }
    std::size_t outputDim() const override { return 0; }
    std::span<const float> forward(std::span<const float> inputs, float dt) override {
        (void)inputs;
        (void)dt;
        return {};
    }
    void reset() override {}
    std::size_t parameterCount() const override { return 0; }
};

static std::string readLastNonEmptyModuleMetricsLine(const std::filesystem::path& csv_path) {
    std::ifstream file(csv_path);
    std::string line;
    std::string last_non_empty;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            last_non_empty = line;
        }
    }
    return last_non_empty;
}

static std::vector<std::string> splitModuleMetricsCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

TEST(ModuleMetricsObserver, AvgFoodEatenCapturesWithinGenerationSamples) {
    const auto temp_dir = std::filesystem::temp_directory_path() / "neural_eco_module_metrics_test";
    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    std::string last_line;
    {
        neuro::World2D world(neuro::WorldId{1}, {20.0f, 20.0f});
        auto agent_layer = std::make_unique<neuro::AgentLayer>(-1000.0f, 0.0f);
        auto* agent_layer_ptr = agent_layer.get();
        world.addLayer(std::move(agent_layer));

        neuro::ModuleMetricsObserver observer(temp_dir / "module_metrics.csv");
        observer.onAttach(world);

        auto agent = std::make_unique<neuro::Agent>(
            neuro::AgentId{1},
            neuro::GenomeId{1},
            std::make_unique<ModuleMetricsDummyBrain>(),
            neuro::Vec2{5.0f, 5.0f},
            50.0f);
        agent_layer_ptr->addAgent(std::move(agent), world);
        world.eventBus().flush();

        for (int tick = 1; tick <= 20; ++tick) {
            world.tick(1.0f);
            auto* concrete = agent_layer_ptr->agents().front();
            ASSERT_TRUE(concrete->alive());
            if (tick == 10) {
                concrete->noteFoodEaten(1.0f);
            }
            observer.onTick(world);
        }

        observer.onDetach(world);
        last_line = readLastNonEmptyModuleMetricsLine(temp_dir / "module_metrics.csv");
    }

    ASSERT_FALSE(last_line.empty());
    const auto fields = splitModuleMetricsCsvLine(last_line);
    ASSERT_EQ(fields.size(), 31u);
    EXPECT_NEAR(std::stod(fields[15]), 0.5, 1e-9);

    std::filesystem::remove_all(temp_dir);
}
