#include "agent/ProgressTracker.hpp"

namespace dp::agent {

const char* progress_stage_name(ProgressStage s) {
    switch (s) {
        case ProgressStage::Early: return "early";
        case ProgressStage::Mid:   return "mid";
        case ProgressStage::Late:  return "late";
    }
    return "?";
}

void ProgressTracker::on_collect(int count) {
    resources_collected_ += count;
    try_advance_();
}

void ProgressTracker::on_explore_step(float distance) {
    if (distance <= 0.0f) return;
    total_explored_ += distance;
    try_advance_();
}

void ProgressTracker::on_house_built() {
    ++houses_built_;
    try_advance_();
}

void ProgressTracker::try_advance_() {
    if (stage_ == ProgressStage::Early) {
        if (resources_collected_ >= mid_collect_threshold
            && total_explored_ >= mid_explore_threshold) {
            stage_ = ProgressStage::Mid;
            unlock_just_fired_ = true;
        }
    } else if (stage_ == ProgressStage::Mid) {
        if (houses_built_ >= late_house_threshold) {
            stage_ = ProgressStage::Late;
            unlock_just_fired_ = true;
        }
    }
}

bool ProgressTracker::poll_unlock() {
    bool fired = unlock_just_fired_;
    unlock_just_fired_ = false;
    return fired;
}

void ProgressTracker::reset() {
    stage_ = ProgressStage::Early;
    resources_collected_ = 0;
    total_explored_ = 0.0f;
    houses_built_ = 0;
    unlock_just_fired_ = false;
}

}  // namespace dp::agent
