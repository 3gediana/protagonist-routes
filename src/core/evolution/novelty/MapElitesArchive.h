#pragma once

#include "core/evolution/novelty/NoveltyArchive.h"
#include "core/types/Aliases.h"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace neuro {

// Stage 5 / P5 MAP-Elites archive.
//
// A discretized version of NoveltyArchive: the behavior space is partitioned
// into a regular grid (per-dimension bin count = bins_per_dim), and each cell
// stores the highest-fitness elite seen for that behavior signature.
//
// Compared to NoveltyArchive (which tracks all behaviors and computes
// k-NN distance), MAP-Elites maintains an O(bins_per_dim ^ behavior_dim)
// bounded grid and keeps only the local best per cell, so the archive is
// both diverse and quality-pressured.
//
// Behavior dimensions used here mirror BehaviorCharacterization (8 dims),
// but the archive itself is agnostic - any float-vector descriptor works
// as long as toVector() is stable.
struct MapElitesElite {
    GenomeId  genome_id{0};
    float     fitness = 0.0f;
    BehaviorCharacterization bc{};
};

class MapElitesArchive {
public:
    // bins_per_dim = how finely each behavior dimension is discretized.
    // value_min / value_max define the per-dimension normalization range
    // (descriptors outside this range get clamped into the edge cells).
    MapElitesArchive(std::size_t bins_per_dim,
                     float value_min,
                     float value_max);

    // Returns true if this submission was a new elite (either new cell or
    // strictly better fitness than the previous elite in this cell).
    bool considerCandidate(GenomeId id,
                           const BehaviorCharacterization& bc,
                           float fitness);

    std::size_t cellCount() const { return cells_.size(); }
    std::size_t coverage() const { return cells_.size(); }
    const std::unordered_map<std::uint64_t, MapElitesElite>& cells() const { return cells_; }
    void clear();

    // Diagnostic: best fitness across all cells.
    float bestFitness() const;

    // Diagnostic: average fitness across all occupied cells. 0 if empty.
    float avgFitness() const;

private:
    std::uint64_t cellKey(const BehaviorCharacterization& bc) const;

    std::size_t bins_per_dim_;
    float value_min_;
    float value_max_;
    // Key = packed cell index across all dims. Value = current elite.
    std::unordered_map<std::uint64_t, MapElitesElite> cells_;
};

}  // namespace neuro
