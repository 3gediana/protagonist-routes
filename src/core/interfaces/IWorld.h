#pragma once
//
// core/interfaces/IWorld.h
//
// The world is a container of IWorldLayer instances + a clock + a bus.
// It does NOT define what kinds of layers exist; that is each layer's
// problem. New world features = new layers, not new IWorld methods.
//
// IWorld is intentionally MINIMAL. Code that needs e.g. terrain queries
// goes through getLayer<TerrainLayer>().queryHeight(p). This keeps IWorld
// stable across all phases.

#include "core/types/Aliases.h"
#include "core/types/Vec2.h"

#include <memory>
#include <string>

namespace neuro {

class IWorldLayer;
class IAgent;
namespace events { class EventBus; }

class IWorld {
public:
    virtual ~IWorld() = default;

    // ----- identity / clock -----------------------------------------------
    virtual WorldId id()         const = 0;
    virtual Tick    currentTick() const = 0;
    virtual SimTimeSeconds currentTimeSeconds() const = 0;

    // ----- world bounds ---------------------------------------------------
    // Continuous floating-point world. Bounds are [0, size.x] x [0, size.y].
    virtual Vec2 size()    const = 0;

    // ----- event bus ------------------------------------------------------
    virtual events::EventBus& eventBus() = 0;

    // ----- layers ---------------------------------------------------------
    // Adds a layer; takes ownership. Calls layer->onAttach(*this).
    virtual void addLayer(std::unique_ptr<IWorldLayer> layer) = 0;

    // Find a layer by name. Returns nullptr if not present.
    virtual IWorldLayer*       findLayer(const std::string& name)       = 0;
    virtual const IWorldLayer* findLayer(const std::string& name) const = 0;

    // Convenience: typed lookup. Implementation:
    //   template<class L> L* getLayer() {
    //     return dynamic_cast<L*>(findLayer(L::kName));
    //   }
    // (Subclasses define a static constexpr const char* kName.)
    template <class L>
    L* getLayer() {
        return dynamic_cast<L*>(findLayer(std::string{L::kName}));
    }
    template <class L>
    const L* getLayer() const {
        return dynamic_cast<const L*>(findLayer(std::string{L::kName}));
    }

    // ----- main loop hooks ------------------------------------------------
    // The runtime drives this; in headless mode it loops as fast as possible.
    virtual void tick(float dt) = 0;
};

}  // namespace neuro
