#include "core/brain/fixed/FixedTopologyGenome.h"
#include "core/evolution/ClassicGA.h"

#include <gtest/gtest.h>

using neuro::ClassicGA;
using neuro::EvaluatedIndividual;
using neuro::FixedTopologyGenome;
using neuro::GenomeId;
using neuro::IGenome;

TEST(ClassicGA, ReproducePreservesPopulationSize) {
    std::vector<std::unique_ptr<IGenome>> population;
    population.push_back(std::make_unique<FixedTopologyGenome>(GenomeId{1}, 5, 10, 2));
    population.push_back(std::make_unique<FixedTopologyGenome>(GenomeId{2}, 5, 10, 2));
    population.push_back(std::make_unique<FixedTopologyGenome>(GenomeId{3}, 5, 10, 2));
    population.push_back(std::make_unique<FixedTopologyGenome>(GenomeId{4}, 5, 10, 2));

    std::vector<IGenome*> raw;
    for (auto& genome : population) {
        raw.push_back(genome.get());
    }

    std::vector<EvaluatedIndividual> evaluated = {
        {GenomeId{1}, 10.0, neuro::kInvalidSpeciesId, population[0]->complexity()},
        {GenomeId{2}, 20.0, neuro::kInvalidSpeciesId, population[1]->complexity()},
        {GenomeId{3}, 30.0, neuro::kInvalidSpeciesId, population[2]->complexity()},
        {GenomeId{4}, 40.0, neuro::kInvalidSpeciesId, population[3]->complexity()},
    };

    ClassicGA ga(0.25, 2, 0.8, 0.05, 0.5, 123);
    auto next = ga.reproduce(evaluated, raw);

    ASSERT_EQ(next.size(), population.size());
    for (const auto& genome : next) {
        EXPECT_NE(genome->id().value, 0u);
        EXPECT_EQ(genome->complexity(), population[0]->complexity());
    }
}
