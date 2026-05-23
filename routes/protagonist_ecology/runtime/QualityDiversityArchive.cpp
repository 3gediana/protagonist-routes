#include "runtime/QualityDiversityArchive.h"

#include <algorithm>
#include <iterator>
#include <limits>

namespace neuro::routes::protagonist {

QualityDiversityArchive::QualityDiversityArchive(int chops_bin_size,
                                                 int workcraft_bin_size,
                                                 int max_bins,
                                                 int hunt_bin_size,
                                                 int hunt_max_bins)
    : chops_bin_size_(std::max(1, chops_bin_size)),
      workcraft_bin_size_(std::max(1, workcraft_bin_size)),
      max_bins_(std::max(1, max_bins)),
      // hunt_bin_size_=0 is allowed; combined with hunt_max_bins_=1 it
      // forces every genome into the z=0 slice (2D backward-compat).
      hunt_bin_size_(std::max(0, hunt_bin_size)),
      hunt_max_bins_(std::max(1, hunt_max_bins)) {}

void QualityDiversityArchive::clear() {
    cells_.clear();
}

QualityDiversityArchive::CellKey
QualityDiversityArchive::cellOf(const BehaviorDescriptor& bd) const {
    int x = std::min(max_bins_ - 1, std::max(0, bd.chop_count / chops_bin_size_));
    int y_raw = bd.build_count + bd.craft_count;
    int y = std::min(max_bins_ - 1, std::max(0, y_raw / workcraft_bin_size_));
    // 3rd axis: when hunt_max_bins_ == 1 (or hunt_bin_size_ == 0) z is
    // always 0 -> archive degenerates to 2D, identical to v14-v16.
    int z = 0;
    if (hunt_max_bins_ > 1 && hunt_bin_size_ > 0) {
        z = std::min(hunt_max_bins_ - 1, std::max(0, bd.hunt_count / hunt_bin_size_));
    }
    return {x, y, z};
}

void QualityDiversityArchive::submit(GenomeId id,
                                     double fitness,
                                     const BehaviorDescriptor& bd) {
    const auto key = cellOf(bd);
    auto it = cells_.find(key);
    if (it == cells_.end() || fitness > it->second.elite_fitness) {
        cells_[key] = Cell{id, fitness};
    }
}

std::vector<GenomeId> QualityDiversityArchive::eliteIds() const {
    std::vector<GenomeId> out;
    out.reserve(cells_.size());
    for (const auto& [key, cell] : cells_) {
        if (cell.elite_id != kInvalidGenomeId) {
            out.push_back(cell.elite_id);
        }
    }
    return out;
}

std::unordered_set<GenomeId> QualityDiversityArchive::eliteIdSet() const {
    std::unordered_set<GenomeId> out;
    out.reserve(cells_.size());
    for (const auto& [key, cell] : cells_) {
        if (cell.elite_id != kInvalidGenomeId) {
            out.insert(cell.elite_id);
        }
    }
    return out;
}

GenomeId QualityDiversityArchive::uniformPickElite(std::mt19937& rng) const {
    if (cells_.empty()) {
        return kInvalidGenomeId;
    }
    std::uniform_int_distribution<std::size_t> dist(0, cells_.size() - 1);
    auto it = std::next(cells_.begin(), static_cast<std::ptrdiff_t>(dist(rng)));
    return it->second.elite_id;
}

double QualityDiversityArchive::qdScore() const {
    double sum = 0.0;
    for (const auto& [key, cell] : cells_) {
        sum += cell.elite_fitness;
    }
    return sum;
}

double QualityDiversityArchive::maxFitness() const {
    double best = -std::numeric_limits<double>::infinity();
    for (const auto& [key, cell] : cells_) {
        best = std::max(best, cell.elite_fitness);
    }
    return cells_.empty() ? 0.0 : best;
}

double QualityDiversityArchive::meanCellFitness() const {
    if (cells_.empty()) return 0.0;
    return qdScore() / static_cast<double>(cells_.size());
}

}  // namespace neuro::routes::protagonist
