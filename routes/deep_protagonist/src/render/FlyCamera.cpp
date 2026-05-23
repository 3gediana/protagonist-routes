#include "render/FlyCamera.hpp"

#include <raymath.h>

#include <cmath>

namespace dp::render {

FlyCamera::FlyCamera() {
    cam_.position   = Vector3{ 64.0f, 64.0f, 60.0f };
    cam_.up         = Vector3{ 0.0f, 0.0f, 1.0f };  // Z is up
    cam_.fovy       = 70.0f;
    cam_.projection = CAMERA_PERSPECTIVE;
    sync_target();
}

Vector3 FlyCamera::compute_forward(float yaw, float pitch) {
    return Vector3{
        std::cos(pitch) * std::cos(yaw),
        std::cos(pitch) * std::sin(yaw),
        std::sin(pitch)
    };
}

Vector3 FlyCamera::forward() const {
    return compute_forward(yaw_, pitch_);
}

void FlyCamera::sync_target() {
    Vector3 fwd = forward();
    cam_.target = Vector3Add(cam_.position, fwd);
    cam_.up = Vector3{ 0.0f, 0.0f, 1.0f };
}

void FlyCamera::set_yaw_pitch(float yaw, float pitch) {
    yaw_ = yaw;
    pitch_ = pitch;
    sync_target();
}

void FlyCamera::set_target(Vector3 t) {
    Vector3 d = Vector3Subtract(t, cam_.position);
    float len = Vector3Length(d);
    if (len < 1e-6f) return;
    d = Vector3Scale(d, 1.0f / len);
    pitch_ = std::asin(d.z);
    yaw_   = std::atan2(d.y, d.x);
    sync_target();
}

void FlyCamera::apply_mouse_delta(Vector2 m) {
    // Mouse right (m.x > 0) turns the view right.
    // In Z-up with math-CCW yaw, "right turn" = yaw decreases.
    yaw_   -= m.x * mouse_sensitivity_;
    pitch_ -= m.y * mouse_sensitivity_;
    constexpr float kHalfPi = 1.5707963f;
    if (pitch_ >  kHalfPi - 0.01f) pitch_ =  kHalfPi - 0.01f;
    if (pitch_ < -kHalfPi + 0.01f) pitch_ = -kHalfPi + 0.01f;
}

void FlyCamera::update(float dt) {
    apply_mouse_delta(GetMouseDelta());

    Vector3 fwd = forward();
    // Right = forward x up (Z-up, right-handed). Equals
    //   ( cos(p)*sin(y), -cos(p)*cos(y), 0 ) projected to ground.
    Vector3 right = Vector3{ std::sin(yaw_), -std::cos(yaw_), 0.0f };

    float speed = speed_ * (IsKeyDown(KEY_LEFT_CONTROL) ? 4.0f : 1.0f);
    Vector3 move = Vector3{ 0, 0, 0 };
    if (IsKeyDown(KEY_W)) move = Vector3Add(move, fwd);
    if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, fwd);
    if (IsKeyDown(KEY_D)) move = Vector3Add(move, right);
    if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, right);
    if (IsKeyDown(KEY_SPACE))      move.z += 1.0f;
    if (IsKeyDown(KEY_LEFT_SHIFT)) move.z -= 1.0f;
    if (Vector3Length(move) > 0.0f) {
        move = Vector3Scale(Vector3Normalize(move), speed * dt);
        cam_.position = Vector3Add(cam_.position, move);
    }

    sync_target();
}

}  // namespace dp::render
