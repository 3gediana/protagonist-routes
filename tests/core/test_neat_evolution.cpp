#include "core/brain/neat/NeatGenome.h"
#include "core/evolution/NeatEvolution.h"

#include <gtest/gtest.h>

#include <random>

using neuro::EvaluatedIndividual;
using neuro::GenomeId;
using neuro::IGenome;
using neuro::NeatEvolution;
using neuro::NeatGenome;

TEST(NeatEvolution, ReproducePreservesPopulationSizeAndGenomeType) {
    NeatEvolution evolution(
        0.25,
        2,
        0.8,
        0.05,
        0.2,
        0.2,
        0.1,
        0.01,
        0.5,
        3.0,
        1.0,
        1.0,
        0.4,
        0.1f,
        5.0f,
        123);

    std::mt19937 rng(42);
    auto& tracker = evolution.innovationTracker();
    tracker.beginGeneration();

    std::vector<std::unique_ptr<IGenome>> population;
    population.push_back(NeatGenome::minimal(GenomeId{1}, 5, 2, tracker, rng));
    population.push_back(NeatGenome::minimal(GenomeId{2}, 5, 2, tracker, rng));
    population.push_back(NeatGenome::minimal(GenomeId{3}, 5, 2, tracker, rng));
    population.push_back(NeatGenome::minimal(GenomeId{4}, 5, 2, tracker, rng));

    std::vector<IGenome*> raw;
    for (auto& genome : population) {
        raw.push_back(genome.get());
    }

    const auto species = evolution.assignSpecies(raw);
    ASSERT_FALSE(species.empty());

    std::vector<EvaluatedIndividual> evaluated;
    for (std::size_t i = 0; i < population.size(); ++i) {
        neuro::SpeciesId species_id = neuro::kInvalidSpeciesId;
        for (const auto& group : species) {
            if (std::find(group.members.begin(), group.members.end(), population[i]->id()) != group.members.end()) {
                species_id = group.id;
                break;
            }
        }
        evaluated.push_back(EvaluatedIndividual{
            population[i]->id(),
            10.0 + static_cast<double>(i),
            species_id,
            population[i]->complexity(),
        });
    }

    auto next = evolution.reproduce(evaluated, raw);
    ASSERT_EQ(next.size(), population.size());
    for (const auto& genome : next) {
        EXPECT_NE(dynamic_cast<const NeatGenome*>(genome.get()), nullptr);
        EXPECT_NE(genome->id().value, 0u);
    }
}
