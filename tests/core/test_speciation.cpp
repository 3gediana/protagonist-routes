#include "core/brain/neat/InnovationTracker.h"
#include "core/brain/neat/NeatGenome.h"
#include "core/evolution/speciation/CompatibilityDistance.h"
#include "core/evolution/speciation/Speciation.h"

#include <gtest/gtest.h>

#include <random>
#include <vector>

using neuro::CompatibilityDistance;
using neuro::GenomeId;
using neuro::InnovationTracker;
using neuro::NeatGenome;
using neuro::SpeciesId;
using neuro::Speciation;

TEST(Speciation, SimilarMinimalGenomesStayInSameSpecies) {
    InnovationTracker tracker(1, 1);
    std::mt19937 rng(42);

    auto genome_a = NeatGenome::minimal(GenomeId{1}, 2, 1, tracker, rng);
    auto clone = genome_a->clone();
    auto* genome_b = dynamic_cast<NeatGenome*>(clone.get());
    ASSERT_NE(genome_b, nullptr);
    genome_b->setId(GenomeId{2});

    Speciation speciation(CompatibilityDistance(1.0, 1.0, 0.4), 3.0);
    const std::vector<const NeatGenome*> genomes{genome_a.get(), genome_b};
    const auto groups = speciation.assign(genomes);

    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].members.size(), 2u);
}

TEST(Speciation, ExistingRepresentativesReuseSpeciesAcrossAssignments) {
    InnovationTracker tracker(1, 1);
    std::mt19937 rng(42);

    auto genome_a = NeatGenome::minimal(GenomeId{1}, 2, 1, tracker, rng);
    tracker.beginGeneration();
    auto genome_b = NeatGenome::minimal(GenomeId{2}, 2, 1, tracker, rng);

    CompatibilityDistance distance(1.0, 1.0, 0.4);
    Speciation speciation(distance, 3.0);
    speciation.assign(std::vector<const NeatGenome*>{genome_a.get(), genome_b.get()});

    auto genome_c = NeatGenome::minimal(GenomeId{3}, 2, 1, tracker, rng);
    auto genome_d = NeatGenome::minimal(GenomeId{4}, 2, 1, tracker, rng);
    genome_d->connections().front().weight += 100.0f;

    const auto groups = speciation.assign(std::vector<const NeatGenome*>{genome_c.get(), genome_d.get()});
    ASSERT_EQ(groups.size(), 2u);
    EXPECT_NE(groups[0].id, groups[1].id);
}

TEST(Speciation, ExportImportPreservesRepresentativesAndNextSpeciesId) {
    InnovationTracker tracker(1, 1);
    std::mt19937 rng(42);

    auto genome_a = NeatGenome::minimal(GenomeId{1}, 2, 1, tracker, rng);
    auto clone = genome_a->clone();
    auto* genome_b = dynamic_cast<NeatGenome*>(clone.get());
    ASSERT_NE(genome_b, nullptr);
    genome_b->setId(GenomeId{2});

    CompatibilityDistance distance(1.0, 1.0, 0.4);
    Speciation speciation(distance, 3.0);
    const auto first_groups = speciation.assign(std::vector<const NeatGenome*>{genome_a.get(), genome_b});
    ASSERT_EQ(first_groups.size(), 1u);

    auto exported = speciation.exportState();

    Speciation restored(distance, 0.5);
    restored.importState(exported);
    EXPECT_DOUBLE_EQ(restored.compatibilityThreshold(), 3.0);

    auto genome_c = genome_a->clone();
    auto* similar = dynamic_cast<NeatGenome*>(genome_c.get());
    ASSERT_NE(similar, nullptr);
    similar->setId(GenomeId{3});

    auto genome_d = NeatGenome::minimal(GenomeId{4}, 2, 1, tracker, rng);
    genome_d->connections().front().weight += 100.0f;

    const auto second_groups = restored.assign(std::vector<const NeatGenome*>{similar, genome_d.get()});
    ASSERT_EQ(second_groups.size(), 2u);
    EXPECT_EQ(second_groups[0].id, SpeciesId{1});
    EXPECT_EQ(second_groups[1].id, SpeciesId{2});
}
