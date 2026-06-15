#include "capability/BackgroundMemoryWriteCapability.h"

#include "brain/DNCMemory.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "world/BackgroundMemoryLayer.h"

#include <cmath>

namespace neuro::routes::protagonist {

namespace {

// Local copies of the activation helpers used by ProtagonistBrain.cpp's
// decodeInterface. Kept private to this TU so we don't have to expose
// the DNCMemory internals.
inline float sigmoidActivation(float x) {
    if (x >= 0.0f) {
        const float z = std::exp(-x);
        return 1.0f / (1.0f + z);
    }
    const float z = std::exp(x);
    return z / (1.0f + z);
}

inline float softplusActivation(float x) {
    if (x > 20.0f) return x;
    if (x < -20.0f) return std::exp(x);
    return std::log1p(std::exp(x));
}

inline void softmax3InPlace(float& a, float& b, float& c) {
    const float m = std::max({a, b, c});
    a = std::exp(a - m);
    b = std::exp(b - m);
    c = std::exp(c - m);
    const float sum = a + b + c;
    if (sum <= 1e-6f) {
        a = b = c = 1.0f / 3.0f;
        return;
    }
    a /= sum; b /= sum; c /= sum;
}

DNCMemoryInterface decodeInterface(std::span<const float> raw,
                                   std::size_t word_size,
                                   std::size_t read_heads) {
    DNCMemoryInterface s;
    s.write_key.resize(word_size, 0.0f);
    s.erase_vector.resize(word_size, 0.0f);
    s.write_vector.resize(word_size, 0.0f);
    s.free_gates.resize(read_heads, 0.0f);
    s.read_keys.resize(read_heads * word_size, 0.0f);
    s.read_strengths.resize(read_heads, 0.0f);
    s.read_modes.resize(read_heads * 3, 0.0f);

    std::size_t cur = 0;
    auto next = [&](float fb = 0.0f) -> float {
        if (cur >= raw.size()) return fb;
        return raw[cur++];
    };

    for (std::size_t i = 0; i < word_size; ++i) s.write_key[i] = std::tanh(next());
    s.write_strength = softplusActivation(next());
    for (std::size_t i = 0; i < word_size; ++i) s.erase_vector[i] = sigmoidActivation(next());
    for (std::size_t i = 0; i < word_size; ++i) s.write_vector[i] = std::tanh(next());
    for (std::size_t i = 0; i < read_heads; ++i) s.free_gates[i] = sigmoidActivation(next());
    s.allocation_gate = sigmoidActivation(next());
    s.write_gate = sigmoidActivation(next());
    for (std::size_t i = 0; i < read_heads * word_size; ++i) s.read_keys[i] = std::tanh(next());
    for (std::size_t i = 0; i < read_heads; ++i) s.read_strengths[i] = softplusActivation(next());
    for (std::size_t h = 0; h < read_heads; ++h) {
        float backward = next();
        float content = next();
        float forward = next();
        softmax3InPlace(backward, content, forward);
        s.read_modes[h * 3 + 0] = backward;
        s.read_modes[h * 3 + 1] = content;
        s.read_modes[h * 3 + 2] = forward;
    }
    return s;
}

}  // namespace

BackgroundMemoryWriteCapability::BackgroundMemoryWriteCapability(std::size_t word_size,
                                                                 std::size_t read_heads)
    : word_size_(word_size > 0 ? word_size : 1),
      read_heads_(read_heads > 0 ? read_heads : 1),
      interface_dim_(DNCMemory::interfaceDim(word_size_, read_heads_)) {}

void BackgroundMemoryWriteCapability::apply(IAgent& agent,
                                            IWorld& world,
                                            std::span<const float> outputs,
                                            float dt) {
    (void)dt;
    if (outputs.size() < interface_dim_) return;

    auto* layer = world.getLayer<BackgroundMemoryLayer>();
    if (layer == nullptr) return;

    auto& mem = layer->getOrCreate(agent.id());
    const auto interface_state = decodeInterface(outputs, word_size_, read_heads_);
    mem.step(interface_state);
}

}  // namespace neuro::routes::protagonist
