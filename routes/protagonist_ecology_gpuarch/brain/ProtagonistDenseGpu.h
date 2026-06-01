#pragma once

#include <cstddef>
#include <memory>
#include <span>

namespace neuro::routes::protagonist {

class ProtagonistDenseGpu final {
public:
    ProtagonistDenseGpu(std::size_t input_dim,
                        std::size_t hidden_dim,
                        std::size_t action_dim,
                        std::size_t interface_dim,
                        std::span<const float> input_hidden_weights,
                        std::span<const float> hidden_bias,
                        std::span<const float> hidden_action_weights,
                        std::span<const float> action_bias,
                        std::span<const float> hidden_interface_weights,
                        std::span<const float> interface_bias);
    ~ProtagonistDenseGpu();

    ProtagonistDenseGpu(ProtagonistDenseGpu&& other) noexcept;
    ProtagonistDenseGpu& operator=(ProtagonistDenseGpu&& other) noexcept;

    ProtagonistDenseGpu(const ProtagonistDenseGpu&) = delete;
    ProtagonistDenseGpu& operator=(const ProtagonistDenseGpu&) = delete;

    bool ready() const;
    bool forward(std::span<const float> full_input,
                 float hidden_alpha,
                 float action_alpha,
                 std::span<float> hidden_io,
                 std::span<float> outputs_io,
                 std::span<float> interface_out);
    void reset();

    static bool supported();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace neuro::routes::protagonist
