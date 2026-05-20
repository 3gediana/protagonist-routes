#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Vec2.h"

#include <cstddef>
#include <vector>

namespace neuro {

using ObjectId = std::uint64_t;

enum class ObjectType : int {
    Stone = 0,
    Stick = 1,
    Spear = 2,
    Count = 3
};

struct MovableObject {
    ObjectId id;
    Vec2 pos;
    Vec2 velocity;
    float mass;
    ObjectType type;
    bool carried;       // true if an agent is holding this
    bool heavy;         // true if requires 2+ agents to carry
    std::size_t carriers_required;  // how many agents needed to pick up
};

class ObjectLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "objects";

    ObjectLayer(Vec2 world_size, std::size_t object_count, float damping = 0.9f);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;

    const std::vector<MovableObject>& objects() const { return objects_; }
    std::vector<MovableObject>& objects() { return objects_; }

    MovableObject* findById(ObjectId id);
    const MovableObject* findById(ObjectId id) const;

    std::vector<const MovableObject*> nearestObjects(Vec2 pos, float radius) const;

    ObjectId nextId() { return next_id_++; }

private:
    Vec2 world_size_;
    float damping_;
    ObjectId next_id_ = 1;
    std::vector<MovableObject> objects_;
};

}  // namespace neuro
