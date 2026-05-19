#pragma once

#include "core/interfaces/IObserver.h"
#include "core/types/Aliases.h"

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace neuro {

class IWorld;

struct SpeciesRecord {
    SpeciesId id{kInvalidSpeciesId};
    SpeciesId parent_id{kInvalidSpeciesId};
    Generation birth_gen{0};
    Generation death_gen{0};  // 0 = still alive
    bool extinct{false};
    double peak_fitness{0.0};
    std::size_t peak_size{0};
    std::size_t total_generations{0};
};

class SpeciesObserver final : public IObserver {
public:
    explicit SpeciesObserver(const std::filesystem::path& output_file);
    ~SpeciesObserver() override;

    void onAttach(IWorld& world) override;
    void onDetach(IWorld& world) override;
    const char* name() const override;

    // Access phylogenetic records (for F-TREE)
    const std::unordered_map<std::uint64_t, SpeciesRecord>& phyloRecords() const;

private:
    void writeCurrentGenerationSnapshot(Generation generation);
    void detectSpeciesBirthExtinction(Generation generation,
                                       const std::unordered_set<std::uint64_t>& current_species);
    void writePhyloTreeRecord(const SpeciesRecord& record, const char* event_type);

    std::filesystem::path output_file_;
    std::filesystem::path phylo_file_;
    std::ofstream stream_;
    std::ofstream phylo_stream_;
    IWorld* world_;
    events::SubscriptionId subscription_id_;

    // Phylogenetic tracking state
    std::unordered_set<std::uint64_t> prev_gen_species_;
    std::unordered_map<std::uint64_t, SpeciesRecord> phylo_records_;
};

}  // namespace neuro
