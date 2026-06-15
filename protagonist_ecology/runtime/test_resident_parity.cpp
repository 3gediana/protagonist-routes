// Parity + traffic test for the cohort-resident GPU inference refactor.
//
// Validates that the new uploadCohort()/forwardResident() path is numerically
// identical (within float rounding) to the legacy forward() that re-uploads all
// weights every tick, and to the CPU reference forwardCpu(). Also quantifies the
// host->device (H2D) byte traffic each path incurs over T ticks, which is the
// whole point of the refactor (weights are lifetime-constant, so they should be
// uploaded once per cohort, not once per tick).
//
// Builds/links against neural_eco::route_protagonist. Runs anywhere: if no CUDA
// device is present, the GPU paths are skipped and only the CPU reference plus
// the analytical traffic model are reported (so the test still compiles + runs
// in CI without a GPU; full numeric parity is exercised on the GPU box).

#include "brain/BatchedDenseGpu.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <span>
#include <vector>

namespace {

using neuro::routes::protagonist::BatchedDenseGpu;

constexpr std::size_t kBatch = 48;   // agents in cohort
constexpr std::size_t kIn = 64;      // input_dim
constexpr std::size_t kHid = 96;     // hidden_dim
constexpr std::size_t kAct = 24;     // action_dim
constexpr std::size_t kIface = 16;   // interface_dim
constexpr std::size_t kTicks = 200;  // simulated lifetime ticks
constexpr float kTol = 1e-4f;

float maxAbsDiff(const std::vector<float>& a, const std::vector<float>& b) {
    float m = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        m = std::max(m, std::fabs(a[i] - b[i]));
    }
    return m;
}

}  // namespace

int main() {
    std::mt19937 rng(20260601u);
    std::uniform_real_distribution<float> w(-0.3f, 0.3f);
    std::uniform_real_distribution<float> a01(0.05f, 0.95f);

    const std::size_t n_w_ih = kBatch * kIn * kHid;
    const std::size_t n_w_ha = kBatch * kHid * kAct;
    const std::size_t n_w_hi = kBatch * kHid * kIface;
    const std::size_t n_b_h = kBatch * kHid;
    const std::size_t n_b_a = kBatch * kAct;
    const std::size_t n_b_i = kBatch * kIface;

    // Lifetime-constant genome tensors (same for all ticks).
    std::vector<float> w_ih(n_w_ih), w_ha(n_w_ha), w_hi(n_w_hi);
    std::vector<float> b_h(n_b_h), b_a(n_b_a), b_i(n_b_i);
    std::vector<float> h_alpha(kBatch), a_alpha(kBatch);
    std::vector<float> addend(n_b_h);
    for (auto& x : w_ih) x = w(rng);
    for (auto& x : w_ha) x = w(rng);
    for (auto& x : w_hi) x = w(rng);
    for (auto& x : b_h) x = w(rng);
    for (auto& x : b_a) x = w(rng);
    for (auto& x : b_i) x = w(rng);
    for (auto& x : addend) x = w(rng);
    for (auto& x : h_alpha) x = a01(rng);
    for (auto& x : a_alpha) x = a01(rng);

    // Per-tick inputs (changing observations).
    std::vector<std::vector<float>> inputs(kTicks, std::vector<float>(kBatch * kIn));
    for (auto& tick : inputs)
        for (auto& x : tick) x = w(rng);

    // Initial plastic state, shared starting point for all three runs.
    std::vector<float> h0(n_b_h), a0(n_b_a);
    for (auto& x : h0) x = w(rng);
    for (auto& x : a0) x = w(rng);

    // ---- CPU reference state ----
    // IMPORTANT: do NOT pre-run the CPU trajectory to completion here. The CPU
    // reference must be advanced IN LOCKSTEP with the GPU paths inside the tick
    // loop below, so every comparison is between the same tick t. (Pre-running
    // it would compare the GPU's per-tick state against the CPU's *final*
    // tick-(T-1) state, which spuriously reports a huge max|Δ|.)
    std::vector<float> hC = h0, aC = a0, iC(n_b_i, 0.0f);

    bool gpu = BatchedDenseGpu::supported();
    std::printf("[parity] CUDA device present: %s\n", gpu ? "yes" : "no");

    int failures = 0;
    if (gpu) {
        BatchedDenseGpu legacy;
        BatchedDenseGpu resident;
        if (!legacy.ready() || !resident.ready()) {
            std::printf("[parity] GPU handle not ready; skipping GPU parity.\n");
        } else {
            std::vector<float> hA = h0, aA = a0, iA(n_b_i, 0.0f);
            std::vector<float> hB = h0, aB = a0, iB(n_b_i, 0.0f);

            bool uploaded = resident.uploadCohort(kBatch, kIn, kHid, kAct, kIface,
                                                  w_ih, b_h, w_ha, b_a, w_hi, b_i);
            if (!uploaded) {
                std::printf("[parity] FAIL: uploadCohort returned false\n");
                ++failures;
            }
            float worst_AB = 0.0f, worst_BC = 0.0f, worst_AC = 0.0f;
            for (std::size_t t = 0; t < kTicks && uploaded; ++t) {
                // Advance the CPU reference one tick so all three paths are at
                // tick t when compared.
                BatchedDenseGpu::forwardCpu(kBatch, kIn, kHid, kAct, kIface,
                                            inputs[t], w_ih, b_h, w_ha, b_a, w_hi, b_i,
                                            addend, h_alpha, a_alpha, hC, aC, iC);
                bool okA = legacy.forward(kBatch, kIn, kHid, kAct, kIface,
                                          inputs[t], w_ih, b_h, w_ha, b_a, w_hi, b_i,
                                          addend, h_alpha, a_alpha, hA, aA, iA);
                bool okB = resident.forwardResident(kBatch, kIn, kHid, kAct, kIface,
                                                    inputs[t], addend, h_alpha, a_alpha,
                                                    hB, aB, iB);
                if (!okA || !okB) {
                    std::printf("[parity] FAIL tick %zu: forward okA=%d okB=%d\n", t, okA, okB);
                    ++failures;
                    break;
                }
                worst_AB = std::max({worst_AB, maxAbsDiff(hA, hB), maxAbsDiff(aA, aB), maxAbsDiff(iA, iB)});
                worst_BC = std::max({worst_BC, maxAbsDiff(hB, hC), maxAbsDiff(aB, aC), maxAbsDiff(iB, iC)});
                worst_AC = std::max({worst_AC, maxAbsDiff(hA, hC), maxAbsDiff(aA, aC), maxAbsDiff(iA, iC)});
            }
            std::printf("[parity] max|Δ| resident-vs-legacy = %.3e\n", worst_AB);
            std::printf("[parity] max|Δ| resident-vs-cpu    = %.3e\n", worst_BC);
            std::printf("[parity] max|Δ| legacy-vs-cpu      = %.3e\n", worst_AC);
            if (worst_AB > kTol) { std::printf("[parity] FAIL: resident != legacy\n"); ++failures; }
            if (worst_BC > kTol) { std::printf("[parity] FAIL: resident != cpu\n"); ++failures; }
        }
    } else {
        std::printf("[parity] GPU absent: numeric parity exercised on GPU box only.\n");
        // Sanity: CPU run produced finite output (advance the full trajectory).
        for (std::size_t t = 0; t < kTicks; ++t) {
            BatchedDenseGpu::forwardCpu(kBatch, kIn, kHid, kAct, kIface,
                                        inputs[t], w_ih, b_h, w_ha, b_a, w_hi, b_i,
                                        addend, h_alpha, a_alpha, hC, aC, iC);
        }
        float s = 0.0f;
        for (float x : hC) s += x;
        if (!std::isfinite(s)) { std::printf("[parity] FAIL: CPU output non-finite\n"); ++failures; }
    }

    // ---- H2D traffic model (bytes) ----
    const double bf = 4.0;  // sizeof(float)
    const double W = bf * (double)(n_w_ih + n_w_ha + n_w_hi + n_b_h + n_b_a + n_b_i);
    const double per_tick_changing =
        bf * (double)(kBatch * kIn          // inputs
                      + n_b_h               // addend
                      + 2 * kBatch          // alphas
                      + n_b_h + n_b_a);     // hidden + action state upload
    const double legacy_h2d = (double)kTicks * (W + per_tick_changing);
    const double resident_h2d = W + (double)kTicks * per_tick_changing;
    std::printf("[traffic] weights/cohort      = %.2f MB\n", W / 1e6);
    std::printf("[traffic] changing/tick       = %.2f MB\n", per_tick_changing / 1e6);
    std::printf("[traffic] legacy   H2D / %zu ticks = %.1f MB\n", kTicks, legacy_h2d / 1e6);
    std::printf("[traffic] resident H2D / %zu ticks = %.1f MB\n", kTicks, resident_h2d / 1e6);
    std::printf("[traffic] H2D reduction       = %.1f%% (%.1fx)\n",
                100.0 * (1.0 - resident_h2d / legacy_h2d), legacy_h2d / resident_h2d);

    if (failures == 0) {
        std::printf("[parity] RESULT: PASS\n");
        return 0;
    }
    std::printf("[parity] RESULT: FAIL (%d)\n", failures);
    return 1;
}
