#pragma once

#include "core/interfaces/IRewardComponent.h"
#include "core/evolution/novelty/NoveltyArchive.h"

#include <memory>

namespace neuro {

class NoveltyReward final : public IRewardComponent {
public:
    NoveltyReward(double weight, std::shared_ptr<NoveltyArchive> archive);

    double evaluate(const IAgent& agent, const IWorld& world, float dt) const override;
    double weight() const override;
    const char* name() const override;

    void recordBehavior(const IAgent& agent);

private:
    double weight_;
    std::shared_ptr<NoveltyArchive> archive_;
};

}  // namespace neuro
