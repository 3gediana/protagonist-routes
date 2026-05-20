#include "core/evolution/speciation/Speciation.h"

#include <stdexcept>

namespace neuro {

namespace {

struct WorkingSpecies {
    SpeciesId id{kInvalidSpeciesId};
    const NeatGenome* comparison_representative{nullptr};
    GenomeId representative_genome_id{kInvalidGenomeId};
    std::vector<GenomeId> members;
};

}  // namespace

Speciation::Speciation(CompatibilityDistance distance,
                       double compatibility_threshold,
                       SpeciesId next_species_id)
    : distance_(std::move(distance)),
      compatibility_threshold_(compatibility_threshold),
      next_species_id_(next_species_id) {}

std::vector<SpeciesGroup> Speciation::assign(const std::vector<const NeatGenome*>& genomes) {
    std::vector<WorkingSpecies> working_species;
    working_species.reserve(previous_species_.size() + genomes.size());
    for (const auto& previous : previous_species_) {
        working_species.push_back(WorkingSpecies{
            previous.id,
            previous.representative.get(),
            kInvalidGenomeId,
            {},
        });
    }

    for (const NeatGenome* genome : genomes) {
        if (genome == nullptr) {
            throw std::invalid_argument("Speciation::assign received null genome");
        }

        WorkingSpecies* matched_group = nullptr;
        for (auto& group : working_species) {
            if (group.comparison_representative == nullptr) {
                continue;
            }

            if (distance_.compute(*genome, *group.comparison_representative) <= compatibility_threshold_) {
                matched_group = &group;
                break;
            }
        }

        if (matched_group == nullptr) {
            working_species.push_back(WorkingSpecies{
                next_species_id_,
                genome,
                genome->id(),
                {genome->id()},
            });
            ++next_species_id_.value;
            continue;
        }

        if (matched_group->members.empty()) {
            matched_group->comparison_representative = genome;
            matched_group->representative_genome_id = genome->id();
        }
        matched_group->members.push_back(genome->id());
    }

    std::vector<SpeciesGroup> groups;
    groups.reserve(working_species.size());
    for (const auto& group : working_species) {
        if (group.members.empty()) {
            continue;
        }
        groups.push_back(SpeciesGroup{
            group.id,
            group.representative_genome_id,
            group.members,
        });
    }

    previous_species_.clear();
    previous_species_.reserve(groups.size());
    for (const auto& group : groups) {
        const auto genome_it = std::find_if(genomes.begin(), genomes.end(), [&](const NeatGenome* genome) {
            return genome != nullptr && genome->id() == group.representative_genome_id;
        });
        if (genome_it == genomes.end()) {
            throw std::runtime_error("Speciation representative genome not found");
        }

        previous_species_.push_back(PreviousSpecies{
            group.id,
            std::make_unique<NeatGenome>(**genome_it),
        });
    }

    return groups;
}

SpeciesId Speciation::lastSpeciesId() const {
    return next_species_id_.value == 0 ? kInvalidSpeciesId : SpeciesId{next_species_id_.value - 1};
}

double Speciation::compatibilityThreshold() const {
    return compatibility_threshold_;
}

SpeciationState Speciation::exportState() const {
    SpeciationState state;
    state.compatibility_threshold = compatibility_threshold_;
    state.next_species_id = next_species_id_;
    state.previous_species.reserve(previous_species_.size());
    for (const auto& previous : previous_species_) {
        if (previous.representative == nullptr) {
            continue;
        }
        state.previous_species.push_back(SpeciationPreviousSpeciesState{
            previous.id,
            *previous.representative,
        });
    }
    return state;
}

void Speciation::importState(SpeciationState state) {
    compatibility_threshold_ = state.compatibility_threshold;
    next_species_id_ = state.next_species_id;
    previous_species_.clear();
    previous_species_.reserve(state.previous_species.size());
    for (auto& previous : state.previous_species) {
        previous_species_.push_back(PreviousSpecies{
            previous.id,
            std::make_unique<NeatGenome>(std::move(previous.representative)),
        });
    }
}

void Speciation::adjustCompatibilityThreshold(std::size_t min_species,
                                              std::size_t max_species,
                                              std::size_t current_species,
                                              double step,
                                              double minimum_threshold) {
    if (step <= 0.0 || min_species > max_species) {
        return;
    }

    if (current_species < min_species) {
        compatibility_threshold_ = std::max(minimum_threshold, compatibility_threshold_ - step);
    } else if (current_species > max_species) {
        compatibility_threshold_ += step;
    }
}

}  // namespace neuro
