#pragma once

#include "core/interfaces/IWorld.h"

#include <memory>
#include <vector>

namespace neuro {

namespace events { class EventBus; }

class World2D final : public IWorld {
public:
    World2D(WorldId id, Vec2 size);
    ~World2D() override;

    WorldId id() const override;
    Tick currentTick() const override;
    Vec2 size() const override;
    events::EventBus& eventBus() override;
    void addLayer(std::unique_ptr<IWorldLayer> layer) override;
    IWorldLayer* findLayer(const std::string& name) override;
    const IWorldLayer* findLayer(const std::string& name) const override;
    void tick(float dt) override;

private:
    WorldId id_;
    Tick tick_;
    Vec2 size_;
    std::unique_ptr<events::EventBus> bus_;
    std::vector<std::unique_ptr<IWorldLayer>> layers_;
};

}  // namespace neuro
