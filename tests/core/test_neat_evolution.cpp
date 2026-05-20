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

TEST(NeatEvolution, ExportImportStatePreservesNextReproduction) {
    auto make_evolution = []() {
        return NeatEvolution(
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
    };

    auto build_population = [](NeatEvolution& evolution) {
        std::mt19937 rng(42);
        auto& tracker = evolution.innovationTracker();
        tracker.beginGeneration();
        std::vector<std::unique_ptr<IGenome>> population;
        population.push_back(NeatGenome::minimal(GenomeId{1}, 5, 2, tracker, rng));
        population.push_back(NeatGenome::minimal(GenomeId{2}, 5, 2, tracker, rng));
        population.push_back(NeatGenome::minimal(GenomeId{3}, 5, 2, tracker, rng));
        population.push_back(NeatGenome::minimal(GenomeId{4}, 5, 2, tracker, rng));
        return population;
    };

    auto make_evaluated = [](NeatEvolution& evolution, std::vector<std::unique_ptr<IGenome>>& population) {
        std::vector<IGenome*> raw;
        for (auto& genome : population) {
            raw.push_back(genome.get());
        }
        const auto species = evolution.assignSpecies(raw);
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
        return evaluated;
    };

    auto evolution_a = make_evolution();
    auto population_a = build_population(evolution_a);
    auto evaluated_a = make_evaluated(evolution_a, population_a);
    auto state = evolution_a.exportState();

    auto evolution_b = make_evolution();
    evolution_b.importState(state);
    auto population_b = build_population(evolution_b);
    auto evaluated_b = make_evaluated(evolution_b, population_b);

    std::vector<IGenome*> raw_a;
    std::vector<IGenome*> raw_b;
    for (auto& genome : population_a) raw_a.push_back(genome.get());
    for (auto& genome : population_b) raw_b.push_back(genome.get());

    auto next_a = evolution_a.reproduce(evaluated_a, raw_a);
    auto next_b = evolution_b.reproduce(evaluated_b, raw_b);
    ASSERT_EQ(next_a.size(), next_b.size());
    for (std::size_t i = 0; i < next_a.size(); ++i) {
        const auto* neat_a = dynamic_cast<const NeatGenome*>(next_a[i].get());
        const auto* neat_b = dynamic_cast<const NeatGenome*>(next_b[i].get());
        ASSERT_NE(neat_a, nullptr);
        ASSERT_NE(neat_b, nullptr);
        EXPECT_EQ(neat_a->id(), neat_b->id());
        EXPECT_EQ(neat_a->nodeCount(), neat_b->nodeCount());
        EXPECT_EQ(neat_a->connectionCount(), neat_b->connectionCount());
        ASSERT_EQ(neat_a->connections().size(), neat_b->connections().size());
        for (std::size_t c = 0; c < neat_a->connections().size(); ++c) {
            EXPECT_EQ(neat_a->connections()[c].from, neat_b->connections()[c].from);
            EXPECT_EQ(neat_a->connections()[c].to, neat_b->connections()[c].to);
            EXPECT_FLOAT_EQ(neat_a->connections()[c].weight, neat_b->connections()[c].weight);
            EXPECT_EQ(neat_a->connections()[c].enabled, neat_b->connections()[c].enabled);
            EXPECT_EQ(neat_a->connections()[c].innovation_id, neat_b->connections()[c].innovation_id);
        }
    }
}
