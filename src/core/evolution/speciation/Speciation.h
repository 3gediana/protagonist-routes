#pragma once

#include "core/brain/neat/NeatGenome.h"
#include "core/evolution/speciation/CompatibilityDistance.h"
#include "core/types/Aliases.h"

#include <memory>
#include <vector>

namespace neuro {

struct SpeciesGroup {
    SpeciesId id{kInvalidSpeciesId};
    GenomeId representative_genome_id{kInvalidGenomeId};
    std::vector<GenomeId> members;
};

class Speciation {
public:
    Speciation(CompatibilityDistance distance,
               double compatibility_threshold,
               SpeciesId next_species_id = SpeciesId{1});

    std::vector<SpeciesGroup> assign(const std::vector<const NeatGenome*>& genomes);
    SpeciesId lastSpeciesId() const;
    double compatibilityThreshold() const;
    void adjustCompatibilityThreshold(std::size_t min_species,
                                      std::size_t max_species,
                                      std::size_t current_species,
                                      double step,
                                      double minimum_threshold = 0.25);

private:
    struct PreviousSpecies {
        SpeciesId id{kInvalidSpeciesId};
        std::unique_ptr<NeatGenome> representative;
    };

    CompatibilityDistance distance_;
    double compatibility_threshold_;
    SpeciesId next_species_id_;
    std::vector<PreviousSpecies> previous_species_;
};

}  // namespace neuro
