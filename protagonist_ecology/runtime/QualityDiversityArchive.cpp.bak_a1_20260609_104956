#include "runtime/QualityDiversityArchive.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <random>

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

void QualityDiversityArchive::initCvt(int num_centroids,
                                      int samples_per_centroid,
                                      int kmeans_iters,
                                      std::uint32_t seed) {
    cvt_enabled_ = false;
    centroids_.clear();
    cells_.clear();
    const int k = std::max(1, num_centroids);
    const int spc = std::max(1, samples_per_centroid);
    const int iters = std::max(1, kmeans_iters);

    // Step 1 (Vassiliades 2017 Alg. 1): generate k*spc uniform-random
    // samples in normalised [0,1]^3 feature space. We use the same
    // feature ranges as grid mode (defined by bin_size * max_bins per
    // axis) so the two modes describe the same behaviour space.
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);
    const int total_samples = k * spc;
    std::vector<std::array<float, 3>> samples(total_samples);
    for (auto& s : samples) {
        s[0] = uni(rng);
        s[1] = uni(rng);
        s[2] = uni(rng);
    }

    // Step 2: pick k initial centroids by random sampling (k-means++
    // would be better but uniform pick is standard in Vassiliades 2017
    // and works fine for low-dim BD).
    std::vector<std::array<float, 3>> centroids(k);
    {
        std::uniform_int_distribution<int> idx_dist(0, total_samples - 1);
        for (int i = 0; i < k; ++i) {
            centroids[i] = samples[idx_dist(rng)];
        }
    }

    // Step 3: Lloyd's k-means. Each iteration: assign each sample to
    // nearest centroid, then move each centroid to the mean of its
    // assigned samples.
    std::vector<int> assign(total_samples, 0);
    for (int iter = 0; iter < iters; ++iter) {
        // Assign
        for (int s_i = 0; s_i < total_samples; ++s_i) {
            const auto& p = samples[s_i];
            float best_d = std::numeric_limits<float>::infinity();
            int best_k = 0;
            for (int c_i = 0; c_i < k; ++c_i) {
                const auto& c = centroids[c_i];
                const float dx = p[0] - c[0];
                const float dy = p[1] - c[1];
                const float dz = p[2] - c[2];
                const float d = dx * dx + dy * dy + dz * dz;
                if (d < best_d) {
                    best_d = d;
                    best_k = c_i;
                }
            }
            assign[s_i] = best_k;
        }
        // Update centroids = mean of assigned samples
        std::vector<std::array<float, 3>> sum(k, {0.0f, 0.0f, 0.0f});
        std::vector<int> count(k, 0);
        for (int s_i = 0; s_i < total_samples; ++s_i) {
            const int c = assign[s_i];
            sum[c][0] += samples[s_i][0];
            sum[c][1] += samples[s_i][1];
            sum[c][2] += samples[s_i][2];
            ++count[c];
        }
        for (int c_i = 0; c_i < k; ++c_i) {
            if (count[c_i] > 0) {
                const float n = static_cast<float>(count[c_i]);
                centroids[c_i] = {sum[c_i][0] / n, sum[c_i][1] / n, sum[c_i][2] / n};
            }
            // Empty centroids stay put (rare with spc>=100).
        }
    }

    centroids_ = std::move(centroids);
    cvt_enabled_ = true;
}

std::array<float, 3> QualityDiversityArchive::normaliseBd(
    const BehaviorDescriptor& bd) const {
    // Feature range per axis = bin_size * max_bins (matches grid mode).
    // Clamp to [0, 1] so out-of-range BDs collapse to the boundary cell.
    const float chop_max = static_cast<float>(chops_bin_size_) * static_cast<float>(max_bins_);
    const float wc_max   = static_cast<float>(workcraft_bin_size_) * static_cast<float>(max_bins_);
    // Hunt axis: when hunt_bin_size_=0 or hunt_max_bins_=1 the axis is
    // collapsed in grid mode; we still need a sensible range for CVT
    // normalisation, so fall back to (bin_size=5, max=3) ie [0, 15] when
    // disabled, matching v17's default range.
    const float hunt_max = (hunt_bin_size_ > 0 && hunt_max_bins_ > 1)
        ? static_cast<float>(hunt_bin_size_) * static_cast<float>(hunt_max_bins_)
        : 15.0f;
    const float wc_raw = static_cast<float>(bd.build_count + bd.craft_count);
    auto clamp01 = [](float v) {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    };
    return {
        clamp01(chop_max > 0.0f ? static_cast<float>(bd.chop_count) / chop_max : 0.0f),
        clamp01(wc_max   > 0.0f ? wc_raw / wc_max : 0.0f),
        clamp01(hunt_max > 0.0f ? static_cast<float>(bd.hunt_count) / hunt_max : 0.0f),
    };
}

QualityDiversityArchive::CellKey
QualityDiversityArchive::cellOf(const BehaviorDescriptor& bd) const {
    if (cvt_enabled_ && !centroids_.empty()) {
        // CVT mode: argmin distance to centroid (Vassiliades 2017 Alg. 3
        // line 14). The centroid index is the cell key.
        const auto p = normaliseBd(bd);
        float best_d = std::numeric_limits<float>::infinity();
        int best_k = 0;
        for (std::size_t i = 0; i < centroids_.size(); ++i) {
            const auto& c = centroids_[i];
            const float dx = p[0] - c[0];
            const float dy = p[1] - c[1];
            const float dz = p[2] - c[2];
            const float d = dx * dx + dy * dy + dz * dz;
            if (d < best_d) {
                best_d = d;
                best_k = static_cast<int>(i);
            }
        }
        return best_k;
    }
    // Grid mode (v14-v17): discretise BD into integer bins and pack
    // (x, y, z) into a single int key. Packing order matches the layout
    // x * (max_bins * hunt_max_bins) + y * hunt_max_bins + z so different
    // (x, y, z) tuples never collide.
    int x = std::min(max_bins_ - 1, std::max(0, bd.chop_count / chops_bin_size_));
    int y_raw = bd.build_count + bd.craft_count;
    int y = std::min(max_bins_ - 1, std::max(0, y_raw / workcraft_bin_size_));
    int z = 0;
    if (hunt_max_bins_ > 1 && hunt_bin_size_ > 0) {
        z = std::min(hunt_max_bins_ - 1, std::max(0, bd.hunt_count / hunt_bin_size_));
    }
    return x * (max_bins_ * hunt_max_bins_) + y * hunt_max_bins_ + z;
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
