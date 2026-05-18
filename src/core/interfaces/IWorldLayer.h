#pragma once
//
// core/interfaces/IWorldLayer.h
//
// One physical / informational stratum of the world. The world is the sum
// of its layers. Layers are ticked in dependency order each simulation step.
//
// Layers (per stage):
//   M1  : AgentLayer, ResourceLayer
//   M5  : TerrainLayer (height + caves)
//   F-COMM   : SignalLayer
//   F-DISEASE: DiseaseLayer
//   F-DAY    : TimeOfDayLayer
//   F-TOOL   : ObjectLayer
//
// Adding a new world feature should mean adding a new IWorldLayer
// subclass; never adding a new method to IWorld itself.

#include <cstdint>

namespace neuro {

class IWorld;

class IWorldLayer {
public:
    virtual ~IWorldLayer() = default;

    // Stable identifier; used by IWorld::getLayer<L>() lookup.
    virtual const char* layerName() const = 0;

    // Called once when the layer is added to a world. Layer can resolve
    // dependencies on sibling layers via the world reference.
    virtual void onAttach(IWorld& world) = 0;

    // Called every tick. dt is the simulation step length.
    virtual void tick(IWorld& world, float dt) = 0;

    // Higher = ticked LATER. Default 0; agents tick at 100; rewards at 200.
    // Use small positive values for layers reading agent state.
    virtual int tickOrder() const { return 0; }
};

}  // namespace neuro
