#include "brain/BatchedDenseGpu.h"

#include <cmath>
#include <utility>

namespace neuro::routes::protagonist {

namespace {
inline float activate(float x) { return std::tanh(x); }
}  // namespace

#if defined(NEURAL_ECO_HAS_PROTAGONIST_CUDA)
extern "C" {
struct BatchedDenseGpuCudaHandle;
bool batchedDenseGpuCudaSupported();
BatchedDenseGpuCudaHandle* batchedDenseGpuCudaCreate();
void batchedDenseGpuCudaDestroy(BatchedDenseGpuCudaHandle* handle);
bool batchedDenseGpuCudaForward(BatchedDenseGpuCudaHandle* handle,
                                std::size_t batch_size,
                                std::size_t input_dim,
                                std::size_t hidden_dim,
                                std::size_t action_dim,
                                std::size_t interface_dim,
                                const float* inputs,
                                const float* weights_in_hidden,
                                const float* hidden_bias,
                                const float* weights_hidden_action,
                                const float* action_bias,
                                const float* weights_hidden_interface,
                                const float* interface_bias,
                                const float* hidden_addend,
                                const float* hidden_alphas,
                                const float* action_alphas,
                                float* hidden_io,
                                float* outputs_io,
                                float* interface_out);
bool batchedDenseGpuCudaUploadCohort(BatchedDenseGpuCudaHandle* handle,
                                     std::size_t batch_size,
                                     std::size_t input_dim,
                                     std::size_t hidden_dim,
                                     std::size_t action_dim,
                                     std::size_t interface_dim,
                                     const float* weights_in_hidden,
                                     const float* hidden_bias,
                                     const float* weights_hidden_action,
                                     const float* action_bias,
                                     const float* weights_hidden_interface,
                                     const float* interface_bias);
bool batchedDenseGpuCudaForwardResident(BatchedDenseGpuCudaHandle* handle,
                                        std::size_t batch_size,
                                        std::size_t input_dim,
                                        std::size_t hidden_dim,
                                        std::size_t action_dim,
                                        std::size_t interface_dim,
                                        const float* inputs,
                                        const float* hidden_addend,
                                        const float* hidden_alphas,
                                        const float* action_alphas,
                                        float* hidden_io,
                                        float* outputs_io,
                                        float* interface_out);
}

struct BatchedDenseGpu::Impl {
    explicit Impl(BatchedDenseGpuCudaHandle* in_handle) : handle(in_handle) {}
    ~Impl() {
        if (handle != nullptr) batchedDenseGpuCudaDestroy(handle);
    }
    Impl(Impl&& other) noexcept : handle(std::exchange(other.handle, nullptr)) {}
    Impl& operator=(Impl&& other) noexcept {
        if (this != &other) {
            if (handle != nullptr) batchedDenseGpuCudaDestroy(handle);
            handle = std::exchange(other.handle, nullptr);
        }
        return *this;
    }
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    BatchedDenseGpuCudaHandle* handle{nullptr};
};
#else
struct BatchedDenseGpu::Impl {};
#endif

BatchedDenseGpu::BatchedDenseGpu() {
#if defined(NEURAL_ECO_HAS_PROTAGONIST_CUDA)
    if (!supported()) return;
    auto* handle = batchedDenseGpuCudaCreate();
    if (handle != nullptr) {
        impl_ = std::make_unique<Impl>(handle);
    }
#endif
}

BatchedDenseGpu::~BatchedDenseGpu() = default;
BatchedDenseGpu::BatchedDenseGpu(BatchedDenseGpu&&) noexcept = default;
BatchedDenseGpu& BatchedDenseGpu::operator=(BatchedDenseGpu&&) noexcept = default;

bool BatchedDenseGpu::supported() {
#if defined(NEURAL_ECO_HAS_PROTAGONIST_CUDA)
    return batchedDenseGpuCudaSupported();
#else
    return false;
#endif
}

bool BatchedDenseGpu::ready() const {
#if defined(NEURAL_ECO_HAS_PROTAGONIST_CUDA)
    return impl_ != nullptr && impl_->handle != nullptr;
#else
    return false;
#endif
}

bool BatchedDenseGpu::forward(std::size_t batch_size,
                              std::size_t input_dim,
                              std::size_t hidden_dim,
                              std::size_t action_dim,
                              std::size_t interface_dim,
                              std::span<const float> inputs,
                              std::span<const float> weights_in_hidden,
                              std::span<const float> hidden_bias,
                              std::span<const float> weights_hidden_action,
                              std::span<const float> action_bias,
                              std::span<const float> weights_hidden_interface,
                              std::span<const float> interface_bias,
                              std::span<const float> hidden_addend,
                              std::span<const float> hidden_alphas,
                              std::span<const float> action_alphas,
                              std::span<float> hidden_io,
                              std::span<float> outputs_io,
                              std::span<float> interface_out) {
#if defined(NEURAL_ECO_HAS_PROTAGONIST_CUDA)
    if (!ready()) return false;
    return batchedDenseGpuCudaForward(impl_->handle,
                                      batch_size,
                                      input_dim,
                                      hidden_dim,
                                      action_dim,
                                      interface_dim,
                                      inputs.data(),
                                      weights_in_hidden.data(),
                                      hidden_bias.data(),
                                      weights_hidden_action.data(),
                                      action_bias.data(),
                                      weights_hidden_interface.data(),
                                      interface_bias.data(),
                                      hidden_addend.empty() ? nullptr : hidden_addend.data(),
                                      hidden_alphas.data(),
                                      action_alphas.data(),
                                      hidden_io.data(),
                                      outputs_io.data(),
                                      interface_out.data());
#else
    (void)batch_size; (void)input_dim; (void)hidden_dim; (void)action_dim; (void)interface_dim;
    (void)inputs; (void)weights_in_hidden; (void)hidden_bias;
    (void)weights_hidden_action; (void)action_bias;
    (void)weights_hidden_interface; (void)interface_bias;
    (void)hidden_addend;
    (void)hidden_alphas; (void)action_alphas;
    (void)hidden_io; (void)outputs_io; (void)interface_out;
    return false;
#endif
}

bool BatchedDenseGpu::uploadCohort(std::size_t batch_size,
                                   std::size_t input_dim,
                                   std::size_t hidden_dim,
                                   std::size_t action_dim,
                                   std::size_t interface_dim,
                                   std::span<const float> weights_in_hidden,
                                   std::span<const float> hidden_bias,
                                   std::span<const float> weights_hidden_action,
                                   std::span<const float> action_bias,
                                   std::span<const float> weights_hidden_interface,
                                   std::span<const float> interface_bias) {
#if defined(NEURAL_ECO_HAS_PROTAGONIST_CUDA)
    if (!ready()) return false;
    return batchedDenseGpuCudaUploadCohort(impl_->handle,
                                           batch_size, input_dim, hidden_dim,
                                           action_dim, interface_dim,
                                           weights_in_hidden.data(),
                                           hidden_bias.data(),
                                           weights_hidden_action.data(),
                                           action_bias.data(),
                                           weights_hidden_interface.data(),
                                           interface_bias.data());
#else
    (void)batch_size; (void)input_dim; (void)hidden_dim; (void)action_dim; (void)interface_dim;
    (void)weights_in_hidden; (void)hidden_bias; (void)weights_hidden_action;
    (void)action_bias; (void)weights_hidden_interface; (void)interface_bias;
    return false;
#endif
}

bool BatchedDenseGpu::forwardResident(std::size_t batch_size,
                                      std::size_t input_dim,
                                      std::size_t hidden_dim,
                                      std::size_t action_dim,
                                      std::size_t interface_dim,
                                      std::span<const float> inputs,
                                      std::span<const float> hidden_addend,
                                      std::span<const float> hidden_alphas,
                                      std::span<const float> action_alphas,
                                      std::span<float> hidden_io,
                                      std::span<float> outputs_io,
                                      std::span<float> interface_out) {
#if defined(NEURAL_ECO_HAS_PROTAGONIST_CUDA)
    if (!ready()) return false;
    return batchedDenseGpuCudaForwardResident(impl_->handle,
                                              batch_size, input_dim, hidden_dim,
                                              action_dim, interface_dim,
                                              inputs.data(),
                                              hidden_addend.empty() ? nullptr : hidden_addend.data(),
                                              hidden_alphas.data(),
                                              action_alphas.data(),
                                              hidden_io.data(),
                                              outputs_io.data(),
                                              interface_out.data());
#else
    (void)batch_size; (void)input_dim; (void)hidden_dim; (void)action_dim; (void)interface_dim;
    (void)inputs; (void)hidden_addend; (void)hidden_alphas; (void)action_alphas;
    (void)hidden_io; (void)outputs_io; (void)interface_out;
    return false;
#endif
}

void BatchedDenseGpu::forwardCpu(std::size_t batch_size,
                                 std::size_t input_dim,
                                 std::size_t hidden_dim,
                                 std::size_t action_dim,
                                 std::size_t interface_dim,
                                 std::span<const float> inputs,
                                 std::span<const float> weights_in_hidden,
                                 std::span<const float> hidden_bias,
                                 std::span<const float> weights_hidden_action,
                                 std::span<const float> action_bias,
                                 std::span<const float> weights_hidden_interface,
                                 std::span<const float> interface_bias,
                                 std::span<const float> hidden_addend,
                                 std::span<const float> hidden_alphas,
                                 std::span<const float> action_alphas,
                                 std::span<float> hidden_io,
                                 std::span<float> outputs_io,
                                 std::span<float> interface_out) {
    const bool has_addend = !hidden_addend.empty();
    for (std::size_t a = 0; a < batch_size; ++a) {
        const float* inp = inputs.data() + a * input_dim;
        const float* w_ih = weights_in_hidden.data() + a * input_dim * hidden_dim;
        const float* b_h = hidden_bias.data() + a * hidden_dim;
        const float* w_ha = weights_hidden_action.data() + a * hidden_dim * action_dim;
        const float* b_a = action_bias.data() + a * action_dim;
        const float* w_hi = weights_hidden_interface.data() + a * hidden_dim * interface_dim;
        const float* b_i = interface_bias.data() + a * interface_dim;
        float* hid = hidden_io.data() + a * hidden_dim;
        float* out = outputs_io.data() + a * action_dim;
        float* iface = interface_out.data() + a * interface_dim;
        const float* addend = has_addend ? hidden_addend.data() + a * hidden_dim : nullptr;
        const float h_alpha = hidden_alphas[a];
        const float a_alpha = action_alphas[a];

        for (std::size_t h = 0; h < hidden_dim; ++h) {
            float sum = b_h[h];
            for (std::size_t i = 0; i < input_dim; ++i) {
                sum += inp[i] * w_ih[i * hidden_dim + h];
            }
            float target = activate(sum);
            if (addend != nullptr) {
                target += addend[h];
            }
            hid[h] += h_alpha * (target - hid[h]);
        }

        for (std::size_t o = 0; o < action_dim; ++o) {
            float sum = b_a[o];
            for (std::size_t h = 0; h < hidden_dim; ++h) {
                sum += hid[h] * w_ha[h * action_dim + o];
            }
            const float target = activate(sum);
            out[o] += a_alpha * (target - out[o]);
        }

        for (std::size_t k = 0; k < interface_dim; ++k) {
            float sum = b_i[k];
            for (std::size_t h = 0; h < hidden_dim; ++h) {
                sum += hid[h] * w_hi[h * interface_dim + k];
            }
            iface[k] = sum;
        }
    }
}

}  // namespace neuro::routes::protagonist
