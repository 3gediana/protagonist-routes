#pragma once

#include "core/interfaces/IWorldLayer.h"

#include <functional>
#include <memory>
#include <vector>

namespace neuro {

class Agent;

// Optional hook: when set, AgentLayer splits its tick() into 4 phases:
//   1) parallel  decideUntilDense() per agent (perception + brain.prepareInputs)
//   2) batched   hook(world, dt, batched_agents) — single call, runner folds
//                all batched_agents' dense matmul into one GPU/SIMD pass
//   3) parallel  decideAfterDense() per agent (brain.finalizeOutputs / write outputs)
//   4) serial    publish + applyCapabilities + metabolism + death (unchanged)
//
// If the hook is null, AgentLayer keeps its single-call decide() behaviour.
//
// `batched_agents` contains only agents whose decideUntilDense() returned true
// (i.e. their brain implements IBatchableBrain and they are deciding this tick).
using BatchedDecisionHook = std::function<void(class IWorld& world,
                                                float dt,
                                                const std::vector<Agent*>& batched_agents)>;

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

    void setBatchedDecisionHook(BatchedDecisionHook hook);
    bool hasBatchedDecisionHook() const;

private:
    class DecisionExecutor;

    void tickBatched(IWorld& world, float dt);

    float death_threshold_;
    float base_metabolism_;
    std::size_t decision_threads_;
    std::unique_ptr<DecisionExecutor> decision_executor_;
    std::vector<std::unique_ptr<Agent>> agents_;
    BatchedDecisionHook batched_decision_hook_{};
};

}  // namespace neuro
