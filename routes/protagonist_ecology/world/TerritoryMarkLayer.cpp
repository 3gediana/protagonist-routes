#include "world/TerritoryMarkLayer.h"

#include "core/interfaces/IWorld.h"

#include <algorithm>
#include <cmath>

namespace neuro::routes::protagonist {

namespace {

// Kin threshold and fingerprint cosine -- identical measure to Kin /
// TerritorySocialPerception / SocialActionCapability so "ownership" here means
// exactly the same "close relative" relation used by the rest of Phase D/E.
constexpr float kAllyRelatedness = 0.6f;

float relatednessFromFingerprints(std::span<const float> a, std::span<const float> b) {
    if (a.empty() || a.size() != b.size()) {
        return 0.0f;
    }
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    if (na <= 1.0e-12f || nb <= 1.0e-12f) {
        return 0.0f;
    }
    return std::clamp(dot / std::sqrt(na * nb), 0.0f, 1.0f);
}

}  // namespace

TerritoryMarkLayer::TerritoryMarkLayer(float mark_radius,
                                       float decay_per_second,
                                       float merge_radius,
                                       float refresh_boost,
                                       std::size_t max_marks)
    : mark_radius_(mark_radius),
      decay_per_second_(decay_per_second),
      merge_radius_(merge_radius),
      refresh_boost_(refresh_boost),
      max_marks_(max_marks) {}

void TerritoryMarkLayer::tick(IWorld& /*world*/, float dt) {
    if (dt <= 0.0f || marks_.empty()) {
        return;
    }
    const float decay = decay_per_second_ * dt;
    for (auto& m : marks_) {
        m.strength -= decay;
    }
    marks_.erase(std::remove_if(marks_.begin(), marks_.end(),
                                [](const TerritoryMark& m) { return m.strength <= 1.0e-3f; }),
                 marks_.end());
}

bool TerritoryMarkLayer::placeOrRefresh(Vec2 pos, std::span<const float> owner_fp) {
    if (owner_fp.empty()) {
        return false;
    }

    // 1) Refresh the nearest mark I already own within the merge radius, rather
    //    than spamming duplicates on top of my own claim.
    const float merge_r2 = merge_radius_ * merge_radius_;
    TerritoryMark* best = nullptr;
    float best_d2 = merge_r2;
    for (auto& m : marks_) {
        if (relatednessFromFingerprints(owner_fp, m.owner_fingerprint) < kAllyRelatedness) {
            continue;
        }
        const float d2 = Vec2::distanceSquared(m.position, pos);
        if (d2 <= best_d2) {
            best_d2 = d2;
            best = &m;
        }
    }
    if (best != nullptr) {
        best->strength = std::min(1.0f, best->strength + refresh_boost_);
        best->position = best->position + (pos - best->position) * 0.25f;  // gentle re-center
        best->owner_fingerprint.assign(owner_fp.begin(), owner_fp.end());   // adopt freshest signer
        best->radius = mark_radius_;
        return true;
    }

    // 2) No owned mark nearby -> place a new one. If the store is full, recycle
    //    the weakest existing mark instead of unbounded growth.
    if (marks_.size() >= max_marks_) {
        auto weakest = std::min_element(
            marks_.begin(), marks_.end(),
            [](const TerritoryMark& a, const TerritoryMark& b) { return a.strength < b.strength; });
        if (weakest == marks_.end()) {
            return false;
        }
        weakest->position = pos;
        weakest->owner_fingerprint.assign(owner_fp.begin(), owner_fp.end());
        weakest->strength = 1.0f;
        weakest->radius = mark_radius_;
        return true;
    }

    TerritoryMark m;
    m.position = pos;
    m.owner_fingerprint.assign(owner_fp.begin(), owner_fp.end());
    m.strength = 1.0f;
    m.radius = mark_radius_;
    marks_.push_back(std::move(m));
    return true;
}

float TerritoryMarkLayer::ownedStrengthAt(Vec2 pos, std::span<const float> owner_fp) const {
    if (owner_fp.empty()) {
        return 0.0f;
    }
    float best = 0.0f;
    for (const auto& m : marks_) {
        if (Vec2::distanceSquared(m.position, pos) > m.radius * m.radius) {
            continue;
        }
        if (relatednessFromFingerprints(owner_fp, m.owner_fingerprint) < kAllyRelatedness) {
            continue;
        }
        best = std::max(best, m.strength);
    }
    return best;
}

float TerritoryMarkLayer::foreignStrengthAt(Vec2 pos, std::span<const float> owner_fp) const {
    float best = 0.0f;
    for (const auto& m : marks_) {
        if (Vec2::distanceSquared(m.position, pos) > m.radius * m.radius) {
            continue;
        }
        // Foreign == not owned by my clan. With no fingerprint, every mark is foreign.
        if (!owner_fp.empty() &&
            relatednessFromFingerprints(owner_fp, m.owner_fingerprint) >= kAllyRelatedness) {
            continue;
        }
        best = std::max(best, m.strength);
    }
    return best;
}

}  // namespace neuro::routes::protagonist
