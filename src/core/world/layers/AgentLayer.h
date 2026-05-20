#pragma once

#include "core/interfaces/IWorldLayer.h"

#include <memory>
#include <vector>

namespace neuro {

class Agent;

class AgentLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "agent";

    AgentLayer(float death_threshold, float base_metabolism, std::size_t decision_threads = 1);
    ~AgentLayer() override;

    const char* layerName() const override;
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override;

    void addAgent(std::unique_ptr<Agent> agent, IWorld& world);
    void clear();
    std::size_t size() const;
    std::size_t livingCount() const;
    std::vector<Agent*> agents();
    std::vector<const Agent*> agents() const;

private:
    class DecisionExecutor;

    float death_threshold_;
    float base_metabolism_;
    std::size_t decision_threads_;
    std::unique_ptr<DecisionExecutor> decision_executor_;
    std::vector<std::unique_ptr<Agent>> agents_;
};

}  // namespace neuro
