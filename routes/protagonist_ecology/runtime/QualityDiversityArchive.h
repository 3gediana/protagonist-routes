#pragma once
//
// routes/protagonist_ecology/runtime/QualityDiversityArchive.h
//
// L2.8 v14 ME-NEAT (MAP-Elites + NEAT). The single-fitness NEAT we used
// in v8-v13 collapses long-chain compositional behaviour: middle-of-chain
// stepping stones (e.g. "chop a lot + build 1 worksite") get out-fitnessed
// by lazy specialists ("chop only", "throw spam") and disappear. The
// literature (Mouret&Clune 2015 MAP-Elites; InstaDeep 2023 ICLR spotlight
// "Neuroevolution is a Competitive Alternative to RL for Skill Discovery";
// Cully 2018) shows quality-diversity preserves these stepping stones by
// binning the population by behaviour-descriptor cells and keeping one
// elite per cell. Each cell elite is a different "way to play", so the
// next generation can mutate from any cell - the chain chop -> deposit ->
// build -> craft is reachable as a sequence of cell-to-cell jumps.
//
// We use the 2D conservative descriptor (P1):
//   x = chop count (GoalType::ChopTree)
//   y = build+craft count (GoalType::BuildWorksite + GoalType::CraftWeapon)
//
// 5x5 grid (25 cells); population 32 fills about half. Larger BD spaces
// (3D + signal_emits) are deferred to a follow-up since signal_emits is
// near-zero at this stage and would leave 100+ empty cells.
//
// This archive is rebuilt every generation from the freshly evaluated
// population (per-generation MAP-Elites, not lifetime). It does not
// serialise across checkpoints; the goal is to shape the next-gen breed
// selection, not to persist a long-running QD run.

#include "core/types/Aliases.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <unordered_set>
#include <vector>

namespace neuro::routes::protagonist {

// One agent-genome's behavioural descriptor, accumulated across all
// episodes in a generation. Sourced from Agent::goalCompletionTotal(int).
struct BehaviorDescriptor {
    int chop_count = 0;          // GoalType::ChopTree     (idx 5)
    int build_count = 0;         // GoalType::BuildWorksite (idx 6)
    int craft_count = 0;         // GoalType::CraftWeapon   (idx 7)
    int hunt_count = 0;          // GoalType::Hunt          (idx 8)  [reserved for P2/P3]
    int signal_emits = 0;        // GoalType::EmitSignal    (idx 11) [reserved for P3]
};

class QualityDiversityArchive {
public:
    using CellKey = std::pair<int, int>;

    struct Cell {
        GenomeId elite_id{kInvalidGenomeId};
        double elite_fitness = -std::numeric_limits<double>::infinity();
    };

    // Configure the 2D grid. chops_bin_size and workcraft_bin_size give
    // discretisation granularity. With chop=20 and wc=2, the grid cells
    // cover (chops in [0,19], [20,39], [40,59], [60,79], [80+]) and
    // (workcraft in [0,1], [2,3], [4,5], [6,7], [8+]). 5x5 = 25 cells.
    QualityDiversityArchive(int chops_bin_size = 20,
                            int workcraft_bin_size = 2,
                            int max_bins = 5);

    void clear();

    // Submit one (genome, fitness, descriptor). If the cell is empty,
    // this becomes the cell elite; otherwise the higher-fitness one wins.
    void submit(GenomeId id, double fitness, const BehaviorDescriptor& bd);

    // For breeding selection: ids of all current cell elites (one per
    // filled cell).
    std::vector<GenomeId> eliteIds() const;
    std::unordered_set<GenomeId> eliteIdSet() const;

    // Uniformly sample one elite genome id (returns kInvalidGenomeId if
    // archive is empty).
    GenomeId uniformPickElite(std::mt19937& rng) const;

    // QD metrics for csv logging.
    std::size_t coverage() const { return cells_.size(); }   // # filled cells
    std::size_t maxCells() const { return static_cast<std::size_t>(max_bins_) * static_cast<std::size_t>(max_bins_); }
    double qdScore() const;       // sum of all cell elite fitnesses
    double maxFitness() const;    // single best fitness across all cells
    double meanCellFitness() const;

    // Read access (mainly for debug / metrics export).
    const std::map<CellKey, Cell>& cells() const { return cells_; }

private:
    CellKey cellOf(const BehaviorDescriptor& bd) const;

    int chops_bin_size_;
    int workcraft_bin_size_;
    int max_bins_;
    std::map<CellKey, Cell> cells_;
};

}  // namespace neuro::routes::protagonist
