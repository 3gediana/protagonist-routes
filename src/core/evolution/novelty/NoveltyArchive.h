#pragma once

#include <cstddef>
#include <vector>

namespace neuro {

struct BehaviorCharacterization {
    float final_x;
    float final_y;
    float total_distance;
    float avg_energy;
    float visited_cells_ratio;
    float food_eaten;
    float damage_dealt;
    float signals_emitted;

    std::vector<float> toVector() const;
    static float distance(const BehaviorCharacterization& a, const BehaviorCharacterization& b);
};

class NoveltyArchive {
public:
    explicit NoveltyArchive(std::size_t k_neighbors = 15, float novelty_threshold = 0.3f);

    float novelty(const BehaviorCharacterization& bc) const;
    void add(const BehaviorCharacterization& bc);
    void clear();

    std::size_t size() const { return archive_.size(); }

private:
    std::size_t k_neighbors_;
    float novelty_threshold_;
    std::vector<BehaviorCharacterization> archive_;
};

}  // namespace neuro
