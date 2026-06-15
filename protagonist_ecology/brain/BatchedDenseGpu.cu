#include <cuda_runtime.h>

#include <cstddef>
#include <cmath>
#include <new>

namespace {

__device__ float activateValue(float x) {
    return tanhf(x);
}

// Each block.y picks an agent in the batch; threads inside a block run hidden_dim units.
__global__ void batchedHiddenKernel(const float* __restrict__ inputs,
                                    const float* __restrict__ weights,
                                    const float* __restrict__ biases,
                                    const float* __restrict__ addend,
                                    float* __restrict__ hidden_io,
                                    const float* __restrict__ alphas,
                                    int input_dim,
                                    int hidden_dim) {
    const int a = static_cast<int>(blockIdx.y);
    const int h = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    if (h >= hidden_dim) return;

    const int input_off = a * input_dim;
    const int weight_off = a * input_dim * hidden_dim;
    const int bias_off = a * hidden_dim;
    const int hidden_off = a * hidden_dim;

    float sum = biases[bias_off + h];
    for (int i = 0; i < input_dim; ++i) {
        sum += inputs[input_off + i] * weights[weight_off + i * hidden_dim + h];
    }
    float target = activateValue(sum);
    if (addend != nullptr) {
        target += addend[hidden_off + h];
    }
    const float alpha = alphas[a];
    const int idx = hidden_off + h;
    hidden_io[idx] += alpha * (target - hidden_io[idx]);
}

__global__ void batchedOutputKernel(const float* __restrict__ hidden_io,
                                    const float* __restrict__ weights,
                                    const float* __restrict__ biases,
                                    float* __restrict__ outputs_io,
                                    const float* __restrict__ alphas,
                                    int hidden_dim,
                                    int action_dim) {
    const int a = static_cast<int>(blockIdx.y);
    const int o = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    if (o >= action_dim) return;

    const int hidden_off = a * hidden_dim;
    const int weight_off = a * hidden_dim * action_dim;
    const int bias_off = a * action_dim;
    const int out_off = a * action_dim;

    float sum = biases[bias_off + o];
    for (int h = 0; h < hidden_dim; ++h) {
        sum += hidden_io[hidden_off + h] * weights[weight_off + h * action_dim + o];
    }
    const float target = activateValue(sum);
    const float alpha = alphas[a];
    const int idx = out_off + o;
    outputs_io[idx] += alpha * (target - outputs_io[idx]);
}

__global__ void batchedInterfaceKernel(const float* __restrict__ hidden_io,
                                       const float* __restrict__ weights,
                                       const float* __restrict__ biases,
                                       float* __restrict__ interface_out,
                                       int hidden_dim,
                                       int interface_dim) {
    const int a = static_cast<int>(blockIdx.y);
    const int k = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    if (k >= interface_dim) return;

    const int hidden_off = a * hidden_dim;
    const int weight_off = a * hidden_dim * interface_dim;
    const int bias_off = a * interface_dim;
    const int iface_off = a * interface_dim;

    float sum = biases[bias_off + k];
    for (int h = 0; h < hidden_dim; ++h) {
        sum += hidden_io[hidden_off + h] * weights[weight_off + h * interface_dim + k];
    }
    interface_out[iface_off + k] = sum;
}

bool ensureBuffer(float*& ptr, std::size_t& capacity, std::size_t needed) {
    if (needed <= capacity) return true;
    if (ptr != nullptr) {
        cudaFree(ptr);
        ptr = nullptr;
        capacity = 0;
    }
    if (needed == 0) return true;
    if (cudaMalloc(reinterpret_cast<void**>(&ptr), needed * sizeof(float)) != cudaSuccess) {
        ptr = nullptr;
        capacity = 0;
        return false;
    }
    capacity = needed;
    return true;
}

void freeBuffer(float*& ptr, std::size_t& capacity) {
    if (ptr != nullptr) {
        cudaFree(ptr);
        ptr = nullptr;
    }
    capacity = 0;
}

}  // namespace

struct BatchedDenseGpuCudaHandle {
    cudaStream_t stream{nullptr};

    float* d_inputs{nullptr};                 std::size_t cap_inputs{0};
    float* d_weights_ih{nullptr};             std::size_t cap_weights_ih{0};
    float* d_bias_h{nullptr};                 std::size_t cap_bias_h{0};
    float* d_weights_ha{nullptr};             std::size_t cap_weights_ha{0};
    float* d_bias_a{nullptr};                 std::size_t cap_bias_a{0};
    float* d_weights_hi{nullptr};             std::size_t cap_weights_hi{0};
    float* d_bias_i{nullptr};                 std::size_t cap_bias_i{0};
    float* d_hidden_addend{nullptr};          std::size_t cap_hidden_addend{0};
    float* d_hidden{nullptr};                 std::size_t cap_hidden{0};
    float* d_outputs{nullptr};                std::size_t cap_outputs{0};
    float* d_interface{nullptr};              std::size_t cap_interface{0};
    float* d_hidden_alphas{nullptr};          std::size_t cap_hidden_alphas{0};
    float* d_action_alphas{nullptr};          std::size_t cap_action_alphas{0};

    // --- cohort-resident weights (2026-06-01 GPU-arch refactor) ---
    // The genome weights/biases are constant over an individual's lifetime, so
    // uploadCohort() loads them once into d_weights_*/d_bias_* and forwardResident()
    // reuses them every tick (only inputs + plastic state change). These remember
    // which cohort dims are currently resident so a mismatched forwardResident()
    // call safely refuses instead of reading stale weights.
    bool        weights_resident{false};
    std::size_t res_batch{0};
    std::size_t res_input_dim{0};
    std::size_t res_hidden_dim{0};
    std::size_t res_action_dim{0};
    std::size_t res_interface_dim{0};
};

namespace {
// Launch the 3 dense kernels (hidden -> action + interface) on the handle's
// stream using whatever is currently in d_weights_*/d_bias_*/d_inputs/d_hidden.
// Shared by both the legacy full-upload forward and the resident-weights path.
bool launchDenseKernels(BatchedDenseGpuCudaHandle* handle,
                        std::size_t batch_size,
                        std::size_t input_dim,
                        std::size_t hidden_dim,
                        std::size_t action_dim,
                        std::size_t interface_dim,
                        bool has_addend) {
    constexpr int block_size = 128;
    auto grid = [](std::size_t n) {
        return dim3(static_cast<unsigned int>((n + block_size - 1) / block_size));
    };
    const dim3 hidden_grid(grid(hidden_dim).x, static_cast<unsigned int>(batch_size));
    const dim3 output_grid(grid(action_dim).x, static_cast<unsigned int>(batch_size));
    const dim3 interface_grid(grid(interface_dim).x, static_cast<unsigned int>(batch_size));

    batchedHiddenKernel<<<hidden_grid, block_size, 0, handle->stream>>>(
        handle->d_inputs, handle->d_weights_ih, handle->d_bias_h,
        (has_addend ? handle->d_hidden_addend : nullptr),
        handle->d_hidden, handle->d_hidden_alphas,
        static_cast<int>(input_dim), static_cast<int>(hidden_dim));
    batchedOutputKernel<<<output_grid, block_size, 0, handle->stream>>>(
        handle->d_hidden, handle->d_weights_ha, handle->d_bias_a,
        handle->d_outputs, handle->d_action_alphas,
        static_cast<int>(hidden_dim), static_cast<int>(action_dim));
    batchedInterfaceKernel<<<interface_grid, block_size, 0, handle->stream>>>(
        handle->d_hidden, handle->d_weights_hi, handle->d_bias_i,
        handle->d_interface,
        static_cast<int>(hidden_dim), static_cast<int>(interface_dim));
    return cudaGetLastError() == cudaSuccess;
}
}  // namespace

extern "C" bool batchedDenseGpuCudaSupported() {
    int device_count = 0;
    return cudaGetDeviceCount(&device_count) == cudaSuccess && device_count > 0;
}

extern "C" BatchedDenseGpuCudaHandle* batchedDenseGpuCudaCreate() {
    if (!batchedDenseGpuCudaSupported()) return nullptr;
    auto* handle = new (std::nothrow) BatchedDenseGpuCudaHandle{};
    if (handle == nullptr) return nullptr;
    if (cudaStreamCreate(&handle->stream) != cudaSuccess) {
        delete handle;
        return nullptr;
    }
    return handle;
}

extern "C" void batchedDenseGpuCudaDestroy(BatchedDenseGpuCudaHandle* handle) {
    if (handle == nullptr) return;
    if (handle->stream != nullptr) {
        cudaStreamDestroy(handle->stream);
        handle->stream = nullptr;
    }
    freeBuffer(handle->d_inputs, handle->cap_inputs);
    freeBuffer(handle->d_weights_ih, handle->cap_weights_ih);
    freeBuffer(handle->d_bias_h, handle->cap_bias_h);
    freeBuffer(handle->d_weights_ha, handle->cap_weights_ha);
    freeBuffer(handle->d_bias_a, handle->cap_bias_a);
    freeBuffer(handle->d_weights_hi, handle->cap_weights_hi);
    freeBuffer(handle->d_bias_i, handle->cap_bias_i);
    freeBuffer(handle->d_hidden_addend, handle->cap_hidden_addend);
    freeBuffer(handle->d_hidden, handle->cap_hidden);
    freeBuffer(handle->d_outputs, handle->cap_outputs);
    freeBuffer(handle->d_interface, handle->cap_interface);
    freeBuffer(handle->d_hidden_alphas, handle->cap_hidden_alphas);
    freeBuffer(handle->d_action_alphas, handle->cap_action_alphas);
    delete handle;
}

extern "C" bool batchedDenseGpuCudaForward(BatchedDenseGpuCudaHandle* handle,
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
                                           float* interface_out) {
    if (handle == nullptr || batch_size == 0) return false;

    // The legacy path re-uploads weights itself; whatever was cohort-resident is
    // now potentially stale (dims/contents may differ), so invalidate residency.
    handle->weights_resident = false;

    const std::size_t n_inputs = batch_size * input_dim;
    const std::size_t n_weights_ih = batch_size * input_dim * hidden_dim;
    const std::size_t n_bias_h = batch_size * hidden_dim;
    const std::size_t n_weights_ha = batch_size * hidden_dim * action_dim;
    const std::size_t n_bias_a = batch_size * action_dim;
    const std::size_t n_weights_hi = batch_size * hidden_dim * interface_dim;
    const std::size_t n_bias_i = batch_size * interface_dim;
    const std::size_t n_hidden = batch_size * hidden_dim;
    const std::size_t n_outputs = batch_size * action_dim;
    const std::size_t n_interface = batch_size * interface_dim;

    if (!ensureBuffer(handle->d_inputs, handle->cap_inputs, n_inputs)
        || !ensureBuffer(handle->d_weights_ih, handle->cap_weights_ih, n_weights_ih)
        || !ensureBuffer(handle->d_bias_h, handle->cap_bias_h, n_bias_h)
        || !ensureBuffer(handle->d_weights_ha, handle->cap_weights_ha, n_weights_ha)
        || !ensureBuffer(handle->d_bias_a, handle->cap_bias_a, n_bias_a)
        || !ensureBuffer(handle->d_weights_hi, handle->cap_weights_hi, n_weights_hi)
        || !ensureBuffer(handle->d_bias_i, handle->cap_bias_i, n_bias_i)
        || !ensureBuffer(handle->d_hidden, handle->cap_hidden, n_hidden)
        || (hidden_addend != nullptr && !ensureBuffer(handle->d_hidden_addend, handle->cap_hidden_addend, n_bias_h))
        || !ensureBuffer(handle->d_outputs, handle->cap_outputs, n_outputs)
        || !ensureBuffer(handle->d_interface, handle->cap_interface, n_interface)
        || !ensureBuffer(handle->d_hidden_alphas, handle->cap_hidden_alphas, batch_size)
        || !ensureBuffer(handle->d_action_alphas, handle->cap_action_alphas, batch_size)) {
        return false;
    }

    auto upload = [&](float* dst, const float* src, std::size_t count) -> bool {
        if (count == 0) return true;
        return cudaMemcpyAsync(dst, src, count * sizeof(float), cudaMemcpyHostToDevice, handle->stream) == cudaSuccess;
    };
    auto download = [&](float* dst, const float* src, std::size_t count) -> bool {
        if (count == 0) return true;
        return cudaMemcpyAsync(dst, src, count * sizeof(float), cudaMemcpyDeviceToHost, handle->stream) == cudaSuccess;
    };

    if (!upload(handle->d_inputs, inputs, n_inputs)
        || !upload(handle->d_weights_ih, weights_in_hidden, n_weights_ih)
        || !upload(handle->d_bias_h, hidden_bias, n_bias_h)
        || !upload(handle->d_weights_ha, weights_hidden_action, n_weights_ha)
        || !upload(handle->d_bias_a, action_bias, n_bias_a)
        || !upload(handle->d_weights_hi, weights_hidden_interface, n_weights_hi)
        || !upload(handle->d_bias_i, interface_bias, n_bias_i)
        || !upload(handle->d_hidden, hidden_io, n_hidden)
        || (hidden_addend != nullptr && !upload(handle->d_hidden_addend, hidden_addend, n_bias_h))
        || !upload(handle->d_outputs, outputs_io, n_outputs)
        || !upload(handle->d_hidden_alphas, hidden_alphas, batch_size)
        || !upload(handle->d_action_alphas, action_alphas, batch_size)) {
        return false;
    }

    // Shared kernel launch (identical to the resident path) so the two forwards
    // can never numerically drift.
    if (!launchDenseKernels(handle, batch_size, input_dim, hidden_dim,
                            action_dim, interface_dim, hidden_addend != nullptr)) {
        return false;
    }

    if (!download(hidden_io, handle->d_hidden, n_hidden)
        || !download(outputs_io, handle->d_outputs, n_outputs)
        || !download(interface_out, handle->d_interface, n_interface)) {
        return false;
    }

    return cudaStreamSynchronize(handle->stream) == cudaSuccess;
}

// === cohort-resident path (2026-06-01 GPU-arch refactor) ===================
// Upload the lifetime-constant genome tensors (weights ih/ha/hi + biases) ONCE.
// forwardResident() then reuses them every tick without re-uploading ~14 MB.
extern "C" bool batchedDenseGpuCudaUploadCohort(BatchedDenseGpuCudaHandle* handle,
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
                                                const float* interface_bias) {
    if (handle == nullptr || batch_size == 0) return false;
    handle->weights_resident = false;

    const std::size_t n_weights_ih = batch_size * input_dim * hidden_dim;
    const std::size_t n_bias_h     = batch_size * hidden_dim;
    const std::size_t n_weights_ha = batch_size * hidden_dim * action_dim;
    const std::size_t n_bias_a     = batch_size * action_dim;
    const std::size_t n_weights_hi = batch_size * hidden_dim * interface_dim;
    const std::size_t n_bias_i     = batch_size * interface_dim;

    if (!ensureBuffer(handle->d_weights_ih, handle->cap_weights_ih, n_weights_ih)
        || !ensureBuffer(handle->d_bias_h, handle->cap_bias_h, n_bias_h)
        || !ensureBuffer(handle->d_weights_ha, handle->cap_weights_ha, n_weights_ha)
        || !ensureBuffer(handle->d_bias_a, handle->cap_bias_a, n_bias_a)
        || !ensureBuffer(handle->d_weights_hi, handle->cap_weights_hi, n_weights_hi)
        || !ensureBuffer(handle->d_bias_i, handle->cap_bias_i, n_bias_i)) {
        return false;
    }

    auto up = [&](float* dst, const float* src, std::size_t count) -> bool {
        if (count == 0) return true;
        return cudaMemcpyAsync(dst, src, count * sizeof(float),
                               cudaMemcpyHostToDevice, handle->stream) == cudaSuccess;
    };
    if (!up(handle->d_weights_ih, weights_in_hidden, n_weights_ih)
        || !up(handle->d_bias_h, hidden_bias, n_bias_h)
        || !up(handle->d_weights_ha, weights_hidden_action, n_weights_ha)
        || !up(handle->d_bias_a, action_bias, n_bias_a)
        || !up(handle->d_weights_hi, weights_hidden_interface, n_weights_hi)
        || !up(handle->d_bias_i, interface_bias, n_bias_i)) {
        return false;
    }
    if (cudaStreamSynchronize(handle->stream) != cudaSuccess) return false;

    handle->weights_resident   = true;
    handle->res_batch          = batch_size;
    handle->res_input_dim      = input_dim;
    handle->res_hidden_dim     = hidden_dim;
    handle->res_action_dim     = action_dim;
    handle->res_interface_dim  = interface_dim;
    return true;
}

// Per-tick forward reusing cohort-resident weights. Uploads only the small
// changing tensors (inputs / addend / alphas / plastic state), launches the
// kernels against the resident d_weights_*, and downloads state back.
extern "C" bool batchedDenseGpuCudaForwardResident(BatchedDenseGpuCudaHandle* handle,
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
                                                   float* interface_out) {
    if (handle == nullptr || batch_size == 0) return false;
    // Refuse if the resident cohort does not match this call's dims/size, so we
    // never silently multiply against stale weights.
    if (!handle->weights_resident
        || handle->res_batch != batch_size
        || handle->res_input_dim != input_dim
        || handle->res_hidden_dim != hidden_dim
        || handle->res_action_dim != action_dim
        || handle->res_interface_dim != interface_dim) {
        return false;
    }

    const std::size_t n_inputs    = batch_size * input_dim;
    const std::size_t n_bias_h    = batch_size * hidden_dim;
    const std::size_t n_hidden    = batch_size * hidden_dim;
    const std::size_t n_outputs   = batch_size * action_dim;
    const std::size_t n_interface = batch_size * interface_dim;

    if (!ensureBuffer(handle->d_inputs, handle->cap_inputs, n_inputs)
        || !ensureBuffer(handle->d_hidden, handle->cap_hidden, n_hidden)
        || !ensureBuffer(handle->d_outputs, handle->cap_outputs, n_outputs)
        || !ensureBuffer(handle->d_interface, handle->cap_interface, n_interface)
        || (hidden_addend != nullptr && !ensureBuffer(handle->d_hidden_addend, handle->cap_hidden_addend, n_bias_h))
        || !ensureBuffer(handle->d_hidden_alphas, handle->cap_hidden_alphas, batch_size)
        || !ensureBuffer(handle->d_action_alphas, handle->cap_action_alphas, batch_size)) {
        return false;
    }

    auto up = [&](float* dst, const float* src, std::size_t count) -> bool {
        if (count == 0) return true;
        return cudaMemcpyAsync(dst, src, count * sizeof(float),
                               cudaMemcpyHostToDevice, handle->stream) == cudaSuccess;
    };
    auto down = [&](float* dst, const float* src, std::size_t count) -> bool {
        if (count == 0) return true;
        return cudaMemcpyAsync(dst, src, count * sizeof(float),
                               cudaMemcpyDeviceToHost, handle->stream) == cudaSuccess;
    };

    // Only the changing tensors move across PCIe each tick (weights stay resident).
    if (!up(handle->d_inputs, inputs, n_inputs)
        || !up(handle->d_hidden, hidden_io, n_hidden)
        || !up(handle->d_outputs, outputs_io, n_outputs)
        || (hidden_addend != nullptr && !up(handle->d_hidden_addend, hidden_addend, n_bias_h))
        || !up(handle->d_hidden_alphas, hidden_alphas, batch_size)
        || !up(handle->d_action_alphas, action_alphas, batch_size)) {
        return false;
    }

    if (!launchDenseKernels(handle, batch_size, input_dim, hidden_dim,
                            action_dim, interface_dim, hidden_addend != nullptr)) {
        return false;
    }

    if (!down(hidden_io, handle->d_hidden, n_hidden)
        || !down(outputs_io, handle->d_outputs, n_outputs)
        || !down(interface_out, handle->d_interface, n_interface)) {
        return false;
    }
    return cudaStreamSynchronize(handle->stream) == cudaSuccess;
}
