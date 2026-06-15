#include "brain/ProtagonistDenseGpu.h"

#include <utility>

namespace neuro::routes::protagonist {

#if defined(NEURAL_ECO_HAS_PROTAGONIST_CUDA)
extern "C" {
struct ProtagonistDenseGpuCudaHandle;
bool protagonistDenseGpuCudaSupported();
ProtagonistDenseGpuCudaHandle* protagonistDenseGpuCudaCreate(std::size_t input_dim,
                                                             std::size_t hidden_dim,
                                                             std::size_t action_dim,
                                                             std::size_t interface_dim,
                                                             const float* input_hidden_weights,
                                                             const float* hidden_bias,
                                                             const float* hidden_action_weights,
                                                             const float* action_bias,
                                                             const float* hidden_interface_weights,
                                                             const float* interface_bias);
void protagonistDenseGpuCudaDestroy(ProtagonistDenseGpuCudaHandle* handle);
bool protagonistDenseGpuCudaForward(ProtagonistDenseGpuCudaHandle* handle,
                                    const float* full_input,
                                    float hidden_alpha,
                                    float action_alpha,
                                    float* hidden_io,
                                    float* outputs_io,
                                    float* interface_out);
void protagonistDenseGpuCudaReset(ProtagonistDenseGpuCudaHandle* handle);
}

struct ProtagonistDenseGpu::Impl {
    explicit Impl(ProtagonistDenseGpuCudaHandle* in_handle)
        : handle(in_handle) {}

    ~Impl() {
        if (handle != nullptr) {
            protagonistDenseGpuCudaDestroy(handle);
        }
    }

    Impl(Impl&& other) noexcept
        : handle(std::exchange(other.handle, nullptr)) {}

    Impl& operator=(Impl&& other) noexcept {
        if (this != &other) {
            if (handle != nullptr) {
                protagonistDenseGpuCudaDestroy(handle);
            }
            handle = std::exchange(other.handle, nullptr);
        }
        return *this;
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    ProtagonistDenseGpuCudaHandle* handle{nullptr};
};
#else
struct ProtagonistDenseGpu::Impl {};
#endif

ProtagonistDenseGpu::ProtagonistDenseGpu(std::size_t input_dim,
                                         std::size_t hidden_dim,
                                         std::size_t action_dim,
                                         std::size_t interface_dim,
                                         std::span<const float> input_hidden_weights,
                                         std::span<const float> hidden_bias,
                                         std::span<const float> hidden_action_weights,
                                         std::span<const float> action_bias,
                                         std::span<const float> hidden_interface_weights,
                                         std::span<const float> interface_bias) {
#if defined(NEURAL_ECO_HAS_PROTAGONIST_CUDA)
    if (!supported()) {
        return;
    }

    auto* handle = protagonistDenseGpuCudaCreate(input_dim,
                                                 hidden_dim,
                                                 action_dim,
                                                 interface_dim,
                                                 input_hidden_weights.data(),
                                                 hidden_bias.data(),
                                                 hidden_action_weights.data(),
                                                 action_bias.data(),
                                                 hidden_interface_weights.data(),
                                                 interface_bias.data());
    if (handle != nullptr) {
        impl_ = std::make_unique<Impl>(handle);
    }
#else
    (void)input_dim;
    (void)hidden_dim;
    (void)action_dim;
    (void)interface_dim;
    (void)input_hidden_weights;
    (void)hidden_bias;
    (void)hidden_action_weights;
    (void)action_bias;
    (void)hidden_interface_weights;
    (void)interface_bias;
#endif
}

ProtagonistDenseGpu::~ProtagonistDenseGpu() = default;
ProtagonistDenseGpu::ProtagonistDenseGpu(ProtagonistDenseGpu&& other) noexcept = default;
ProtagonistDenseGpu& ProtagonistDenseGpu::operator=(ProtagonistDenseGpu&& other) noexcept = default;

bool ProtagonistDenseGpu::ready() const {
#if defined(NEURAL_ECO_HAS_PROTAGONIST_CUDA)
    return impl_ != nullptr && impl_->handle != nullptr;
#else
    return false;
#endif
}

bool ProtagonistDenseGpu::forward(std::span<const float> full_input,
                                  float hidden_alpha,
                                  float action_alpha,
                                  std::span<float> hidden_io,
                                  std::span<float> outputs_io,
                                  std::span<float> interface_out) {
#if defined(NEURAL_ECO_HAS_PROTAGONIST_CUDA)
    if (!ready()) {
        return false;
    }
    return protagonistDenseGpuCudaForward(impl_->handle,
                                          full_input.data(),
                                          hidden_alpha,
                                          action_alpha,
                                          hidden_io.data(),
                                          outputs_io.data(),
                                          interface_out.data());
#else
    (void)full_input;
    (void)hidden_alpha;
    (void)action_alpha;
    (void)hidden_io;
    (void)outputs_io;
    (void)interface_out;
    return false;
#endif
}

void ProtagonistDenseGpu::reset() {
#if defined(NEURAL_ECO_HAS_PROTAGONIST_CUDA)
    if (ready()) {
        protagonistDenseGpuCudaReset(impl_->handle);
    }
#endif
}

bool ProtagonistDenseGpu::supported() {
#if defined(NEURAL_ECO_HAS_PROTAGONIST_CUDA)
    return protagonistDenseGpuCudaSupported();
#else
    return false;
#endif
}

}  // namespace neuro::routes::protagonist
