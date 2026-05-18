#pragma once
//
// core/interfaces/IRenderer.h
//
// Abstract renderer. Implemented in the viz/ layer with raylib in M2.
// Lives in core/interfaces so non-viz code can hold a pointer / nullptr
// for it (interactive runtime owns one; headless runtime always nullptr).
//
// Renderers are PASSIVE: the runtime drives them via beginFrame/draw/end.
// They never mutate world state.

namespace neuro {

class IWorld;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // Called once at startup, after the window/device is ready.
    virtual void initialize() = 0;

    // Called once at shutdown.
    virtual void shutdown() = 0;

    // Per-frame hooks.
    virtual void beginFrame() = 0;
    virtual void draw(const IWorld& world) = 0;
    virtual void endFrame() = 0;

    // Is the window asking to close (X clicked, ESC pressed)?
    virtual bool shouldClose() const = 0;
};

}  // namespace neuro
