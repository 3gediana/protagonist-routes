#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IBrain.h"
#include "core/observer/ComprehensiveMetrics.h"
#include "core/pathogen/Pathogen.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

class DummyBrain final : public neuro::IBrain {
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

std::string readLastNonEmptyDataLine(const std::filesystem::path& csv_path) {
    std::ifstream file(csv_path);
    std::string line;
    std::string last_non_empty;
    while (std::getline(file, line)) {
        if (!line.empty()) last_non_empty = line;
    }
    return last_non_empty;
}

std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

TEST(ComprehensiveMetrics, DiseaseStatsCaptureWithinGenerationInfections) {
    const auto temp_dir = std::filesystem::temp_directory_path() / "neural_eco_comprehensive_metrics_test";
    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    std::string last_line;
    {
        neuro::World2D world(neuro::WorldId{1}, {20.0f, 20.0f});
        auto agent_layer = std::make_unique<neuro::AgentLayer>(-1000.0f, 0.0f);
        auto* agent_layer_ptr = agent_layer.get();
        world.addLayer(std::move(agent_layer));

        neuro::ComprehensiveMetrics metrics(temp_dir);
        metrics.onAttach(world);

        auto agent = std::make_unique<neuro::Agent>(neuro::AgentId{1}, neuro::GenomeId{1}, std::make_unique<DummyBrain>(), neuro::Vec2{5.0f, 5.0f}, 50.0f);
        agent->setSpeciesId(neuro::SpeciesId{1});
        agent->attachPathogen(std::make_unique<neuro::Pathogen>(neuro::PathogenGenome(42)));
        agent_layer_ptr->addAgent(std::move(agent), world);
        world.eventBus().flush();

        world.tick(1.0f);

        auto* concrete = agent_layer_ptr->agents().front();
        ASSERT_TRUE(concrete->infected());
        ASSERT_NE(concrete->pathogen(), nullptr);

        concrete->setInfected(false);
        concrete->addImmuneMemory(neuro::PathogenGenome(42).strain());
        concrete->setImmune(true);

        world.eventBus().publish(neuro::events::Event{neuro::events::EventType::GenerationEnded,
                                                       neuro::events::GenerationEnded{0, 0.0, 0.0, 0.0},
                                                       world.currentTick()});
        world.eventBus().flush();

        last_line = readLastNonEmptyDataLine(temp_dir / "disease_arms_race.csv");
        metrics.onDetach(world);
    }

    ASSERT_FALSE(last_line.empty());

    const auto fields = splitCsvLine(last_line);
    ASSERT_EQ(fields.size(), 8u);
    EXPECT_GT(std::stod(fields[1]), 0.0);
    EXPECT_GT(std::stod(fields[2]), 0.0);
    EXPECT_GT(std::stod(fields[3]), 0.0);
    EXPECT_GT(std::stod(fields[7]), 0.0);
    std::filesystem::remove_all(temp_dir);
}
