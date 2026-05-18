#include "core/world/World2D.h"

#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorldLayer.h"

#include <algorithm>
#include <memory>

namespace neuro {

World2D::World2D(WorldId id, Vec2 size)
    : id_(id),
      tick_(0),
      size_(size),
      bus_(std::make_unique<events::EventBus>()) {}

World2D::~World2D() = default;

WorldId World2D::id() const {
    return id_;
}

Tick World2D::currentTick() const {
    return tick_;
}

Vec2 World2D::size() const {
    return size_;
}

events::EventBus& World2D::eventBus() {
    return *bus_;
}

void World2D::addLayer(std::unique_ptr<IWorldLayer> layer) {
    layer->onAttach(*this);
    layers_.push_back(std::move(layer));
    std::stable_sort(layers_.begin(), layers_.end(), [](const auto& lhs, const auto& rhs) {
        return lhs->tickOrder() < rhs->tickOrder();
    });
}

IWorldLayer* World2D::findLayer(const std::string& name) {
    for (auto& layer : layers_) {
        if (name == layer->layerName()) {
            return layer.get();
        }
    }
    return nullptr;
}

const IWorldLayer* World2D::findLayer(const std::string& name) const {
    for (const auto& layer : layers_) {
        if (name == layer->layerName()) {
            return layer.get();
        }
    }
    return nullptr;
}

void World2D::tick(float dt) {
    bus_->publish(events::Event{events::EventType::TickStarted,
                                events::TickStarted{tick_},
                                tick_});
    bus_->flush();

    for (auto& layer : layers_) {
        layer->tick(*this, dt);
    }

    bus_->publish(events::Event{events::EventType::TickEnded,
                                events::TickEnded{tick_},
                                tick_});
    bus_->flush();
    ++tick_;
}

}  // namespace neuro
