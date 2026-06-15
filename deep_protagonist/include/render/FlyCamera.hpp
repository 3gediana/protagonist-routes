#pragma once

#include <raylib.h>

namespace dp::render {

// First-person fly camera: WASD to translate, mouse to look, Space/Shift to
// climb/descend. Wraps raylib's Camera3D so we still benefit from raylib's
// projection math.
//
// The pure math (mouse->yaw/pitch->forward) is exposed via apply_mouse_delta /
// forward() so it can be unit-tested without a real window.
class FlyCamera {
public:
    FlyCamera();

    void set_position(Vector3 pos) { cam_.position = pos; sync_target(); }
    void set_yaw_pitch(float yaw, float pitch);
    void set_target(Vector3 t);   // reverse-derives yaw/pitch from target

    // Update from raylib's live input (mouse + WASD). Frame time in seconds.
    void update(float dt);

    // ---- Test-friendly inputs (no raylib globals required) ----
    // Apply a mouse delta in pixels. Right-positive, down-positive (raylib
    // convention). Mouse right turns the view right (yaw decreases in our
    // Z-up CCW math convention).
    void apply_mouse_delta(Vector2 mouse_delta);
    Vector3 forward() const;

    Camera3D& raw() { return cam_; }
    const Camera3D& raw() const { return cam_; }

    float yaw()   const { return yaw_; }
    float pitch() const { return pitch_; }

    // Static math helper, easy to assert against.
    static Vector3 compute_forward(float yaw, float pitch);

private:
    Camera3D cam_;
    float yaw_   = 0.0f;   // radians, around world up (Z); 0 = face +X
    float pitch_ = 0.0f;   // radians, [-pi/2, pi/2]; positive = look up
    float speed_ = 12.0f;  // m/s baseline; held Ctrl = 4x
    float mouse_sensitivity_ = 0.0025f;

    void sync_target();
};

}  // namespace dp::render
