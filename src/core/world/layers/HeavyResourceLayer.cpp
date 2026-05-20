#include "core/world/layers/HeavyResourceLayer.h"
#include "core/world/layers/AgentLayer.h"
#include "core/agent/Agent.h"
#include "core/interfaces/IWorld.h"

#include <cmath>
#include <random>

namespace neuro {

HeavyResourceLayer::HeavyResourceLayer(std::size_t count,
                                       float amount,
                                       std::size_t required_workers,
                                       std::size_t required_ticks,
                                       float collect_radius,
                                       std::uint32_t seed)
    : count_(count),
      amount_(amount),
      required_workers_(required_workers),
      required_ticks_(required_ticks),
      collect_radius_(collect_radius),
      rng_(seed == 0 ? std::random_device{}() : seed),
      world_size_(0.0f, 0.0f) {}

void HeavyResourceLayer::onAttach(IWorld& world) {
    world_size_ = world.size();
    resources_.clear();
    resources_.reserve(count_);

    auto* agent_layer = world.getLayer<AgentLayer>();
    const std::size_t pop_size = agent_layer ? agent_layer->agents().size() : 24;

    std::uniform_real_distribution<float> x_dist(0.0f, world_size_.x);
    std::uniform_real_distribution<float> y_dist(0.0f, world_size_.y);

    for (std::size_t i = 0; i < count_; ++i) {
        resources_.push_back(HeavyResource{
            {x_dist(rng_), y_dist(rng_)},
            amount_,
            required_workers_,
            0,
            0,
            required_ticks_,
            true,
        });
    }
}

void HeavyResourceLayer::tick(IWorld& world, float dt) {
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    const auto& agents = agent_layer->agents();

    for (auto& resource : resources_) {
        if (!resource.available) continue;

        std::size_t nearby_count = 0;
        for (const auto* agent : agents) {
            if (!agent->alive()) continue;
            const float dx = agent->position().x - resource.position.x;
            const float dy = agent->position().y - resource.position.y;
            if (dx * dx + dy * dy <= collect_radius_ * collect_radius_) {
                nearby_count++;
            }
        }

        resource.current_workers = nearby_count;

        if (nearby_count >= resource.required_workers) {
            resource.ticks_with_workers += static_cast<std::size_t>(dt);
            if (resource.ticks_with_workers >= resource.required_ticks) {
                const float share = resource.amount / static_cast<float>(nearby_count);
                for (auto* agent : agents) {
                    if (!agent->alive()) continue;
                    const float dx = agent->position().x - resource.position.x;
                    const float dy = agent->position().y - resource.position.y;
                    if (dx * dx + dy * dy <= collect_radius_ * collect_radius_) {
                        auto* concrete = dynamic_cast<Agent*>(agent);
                        if (concrete) {
                            const float gained = share * (1.15f - 0.3f * concrete->dietPreference());
                            concrete->setEnergy(concrete->energy() + gained);
                            concrete->notePlantFood(gained);
                        }
                    }
                }
                resource.available = false;
                resource.ticks_with_workers = 0;
                ++gen_completed_;
            }
        } else {
            resource.ticks_with_workers = 0;
        }
    }
}

}  // namespace neuro
