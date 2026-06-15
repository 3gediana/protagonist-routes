#include <cuda_runtime.h>

#include <cstddef>
#include <cmath>
#include <new>

namespace {

__device__ float activateValue(float x) {
    return tanhf(x);
}

__global__ void hiddenKernel(const float* input,
                             const float* weights,
                             const float* bias,
                             float* hidden,
                             int input_dim,
                             int hidden_dim,
                             float alpha) {
    const int h = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    if (h >= hidden_dim) {
        return;
    }

    float sum = bias[h];
    for (int i = 0; i < input_dim; ++i) {
        sum += input[i] * weights[i * hidden_dim + h];
    }

    const float target = activateValue(sum);
    hidden[h] += alpha * (target - hidden[h]);
}

__global__ void outputKernel(const float* hidden,
                             const float* weights,
                             const float* bias,
                             float* outputs,
                             int hidden_dim,
                             int action_dim,
                             float alpha) {
    const int o = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    if (o >= action_dim) {
        return;
    }

    float sum = bias[o];
    for (int h = 0; h < hidden_dim; ++h) {
        sum += hidden[h] * weights[h * action_dim + o];
    }

    const float target = activateValue(sum);
    outputs[o] += alpha * (target - outputs[o]);
}

__global__ void interfaceKernel(const float* hidden,
                                const float* weights,
                                const float* bias,
                                float* interface_out,
                                int hidden_dim,
                                int interface_dim) {
    const int i = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    if (i >= interface_dim) {
        return;
    }

    float sum = bias[i];
    for (int h = 0; h < hidden_dim; ++h) {
        sum += hidden[h] * weights[h * interface_dim + i];
    }

    interface_out[i] = sum;
}

bool allocateBuffer(float*& ptr, std::size_t count) {
    if (count == 0) {
        ptr = nullptr;
        return true;
    }
    return cudaMalloc(reinterpret_cast<void**>(&ptr), count * sizeof(float)) == cudaSuccess;
}

bool copyToDevice(float* dst, const float* src, std::size_t count) {
    if (count == 0) {
        return true;
    }
    return cudaMemcpy(dst, src, count * sizeof(float), cudaMemcpyHostToDevice) == cudaSuccess;
}

void freeBuffer(float*& ptr) {
    if (ptr != nullptr) {
        cudaFree(ptr);
        ptr = nullptr;
    }
}

}  // namespace

struct ProtagonistDenseGpuCudaHandle {
    std::size_t input_dim{0};
    std::size_t hidden_dim{0};
    std::size_t action_dim{0};
    std::size_t interface_dim{0};
    float* d_input{nullptr};
    float* d_hidden{nullptr};
    float* d_output{nullptr};
    float* d_interface{nullptr};
    float* d_input_hidden_weights{nullptr};
    float* d_hidden_bias{nullptr};
    float* d_hidden_action_weights{nullptr};
    float* d_action_bias{nullptr};
    float* d_hidden_interface_weights{nullptr};
    float* d_interface_bias{nullptr};
    cudaStream_t stream{nullptr};
};

extern "C" bool protagonistDenseGpuCudaSupported() {
    int device_count = 0;
    return cudaGetDeviceCount(&device_count) == cudaSuccess && device_count > 0;
}

extern "C" void protagonistDenseGpuCudaDestroy(ProtagonistDenseGpuCudaHandle* handle) {
    if (handle == nullptr) {
        return;
    }

    if (handle->stream != nullptr) {
        cudaStreamDestroy(handle->stream);
        handle->stream = nullptr;
    }

    freeBuffer(handle->d_input);
    freeBuffer(handle->d_hidden);
    freeBuffer(handle->d_output);
    freeBuffer(handle->d_interface);
    freeBuffer(handle->d_input_hidden_weights);
    freeBuffer(handle->d_hidden_bias);
    freeBuffer(handle->d_hidden_action_weights);
    freeBuffer(handle->d_action_bias);
    freeBuffer(handle->d_hidden_interface_weights);
    freeBuffer(handle->d_interface_bias);

    delete handle;
}

extern "C" ProtagonistDenseGpuCudaHandle* protagonistDenseGpuCudaCreate(std::size_t input_dim,
                                                                          std::size_t hidden_dim,
                                                                          std::size_t action_dim,
                                                                          std::size_t interface_dim,
                                                                          const float* input_hidden_weights,
                                                                          const float* hidden_bias,
                                                                          const float* hidden_action_weights,
                                                                          const float* action_bias,
                                                                          const float* hidden_interface_weights,
                                                                          const float* interface_bias) {
    if (!protagonistDenseGpuCudaSupported()) {
        return nullptr;
    }

    auto* handle = new (std::nothrow) ProtagonistDenseGpuCudaHandle{};
    if (handle == nullptr) {
        return nullptr;
    }

    handle->input_dim = input_dim;
    handle->hidden_dim = hidden_dim;
    handle->action_dim = action_dim;
    handle->interface_dim = interface_dim;

    const std::size_t input_hidden_count = input_dim * hidden_dim;
    const std::size_t hidden_action_count = hidden_dim * action_dim;
    const std::size_t hidden_interface_count = hidden_dim * interface_dim;

    if (cudaStreamCreate(&handle->stream) != cudaSuccess
        || !allocateBuffer(handle->d_input, input_dim)
        || !allocateBuffer(handle->d_hidden, hidden_dim)
        || !allocateBuffer(handle->d_output, action_dim)
        || !allocateBuffer(handle->d_interface, interface_dim)
        || !allocateBuffer(handle->d_input_hidden_weights, input_hidden_count)
        || !allocateBuffer(handle->d_hidden_bias, hidden_dim)
        || !allocateBuffer(handle->d_hidden_action_weights, hidden_action_count)
        || !allocateBuffer(handle->d_action_bias, action_dim)
        || !allocateBuffer(handle->d_hidden_interface_weights, hidden_interface_count)
        || !allocateBuffer(handle->d_interface_bias, interface_dim)
        || !copyToDevice(handle->d_input_hidden_weights, input_hidden_weights, input_hidden_count)
        || !copyToDevice(handle->d_hidden_bias, hidden_bias, hidden_dim)
        || !copyToDevice(handle->d_hidden_action_weights, hidden_action_weights, hidden_action_count)
        || !copyToDevice(handle->d_action_bias, action_bias, action_dim)
        || !copyToDevice(handle->d_hidden_interface_weights, hidden_interface_weights, hidden_interface_count)
        || !copyToDevice(handle->d_interface_bias, interface_bias, interface_dim)
        || cudaMemsetAsync(handle->d_hidden, 0, hidden_dim * sizeof(float), handle->stream) != cudaSuccess
        || cudaMemsetAsync(handle->d_output, 0, action_dim * sizeof(float), handle->stream) != cudaSuccess
        || cudaStreamSynchronize(handle->stream) != cudaSuccess) {
        protagonistDenseGpuCudaDestroy(handle);
        return nullptr;
    }

    return handle;
}

extern "C" bool protagonistDenseGpuCudaForward(ProtagonistDenseGpuCudaHandle* handle,
                                                 const float* full_input,
                                                 float hidden_alpha,
                                                 float action_alpha,
                                                 float* hidden_io,
                                                 float* outputs_io,
                                                 float* interface_out) {
    if (handle == nullptr || full_input == nullptr || hidden_io == nullptr || outputs_io == nullptr || interface_out == nullptr) {
        return false;
    }

    if (cudaMemcpyAsync(handle->d_input,
                        full_input,
                        handle->input_dim * sizeof(float),
                        cudaMemcpyHostToDevice,
                        handle->stream) != cudaSuccess) {
        return false;
    }

    constexpr int block_size = 256;
    const dim3 hidden_grid(static_cast<unsigned int>((handle->hidden_dim + block_size - 1) / block_size));
    const dim3 output_grid(static_cast<unsigned int>((handle->action_dim + block_size - 1) / block_size));
    const dim3 interface_grid(static_cast<unsigned int>((handle->interface_dim + block_size - 1) / block_size));

    hiddenKernel<<<hidden_grid, block_size, 0, handle->stream>>>(handle->d_input,
                                                                  handle->d_input_hidden_weights,
                                                                  handle->d_hidden_bias,
                                                                  handle->d_hidden,
                                                                  static_cast<int>(handle->input_dim),
                                                                  static_cast<int>(handle->hidden_dim),
                                                                  hidden_alpha);
    outputKernel<<<output_grid, block_size, 0, handle->stream>>>(handle->d_hidden,
                                                                  handle->d_hidden_action_weights,
                                                                  handle->d_action_bias,
                                                                  handle->d_output,
                                                                  static_cast<int>(handle->hidden_dim),
                                                                  static_cast<int>(handle->action_dim),
                                                                  action_alpha);
    interfaceKernel<<<interface_grid, block_size, 0, handle->stream>>>(handle->d_hidden,
                                                                        handle->d_hidden_interface_weights,
                                                                        handle->d_interface_bias,
                                                                        handle->d_interface,
                                                                        static_cast<int>(handle->hidden_dim),
                                                                        static_cast<int>(handle->interface_dim));

    if (cudaGetLastError() != cudaSuccess) {
        return false;
    }

    if (cudaMemcpyAsync(hidden_io,
                        handle->d_hidden,
                        handle->hidden_dim * sizeof(float),
                        cudaMemcpyDeviceToHost,
                        handle->stream) != cudaSuccess
        || cudaMemcpyAsync(outputs_io,
                           handle->d_output,
                           handle->action_dim * sizeof(float),
                           cudaMemcpyDeviceToHost,
                           handle->stream) != cudaSuccess
        || cudaMemcpyAsync(interface_out,
                           handle->d_interface,
                           handle->interface_dim * sizeof(float),
                           cudaMemcpyDeviceToHost,
                           handle->stream) != cudaSuccess) {
        return false;
    }

    return cudaStreamSynchronize(handle->stream) == cudaSuccess;
}

extern "C" void protagonistDenseGpuCudaReset(ProtagonistDenseGpuCudaHandle* handle) {
    if (handle == nullptr) {
        return;
    }

    if (handle->d_hidden != nullptr) {
        cudaMemsetAsync(handle->d_hidden, 0, handle->hidden_dim * sizeof(float), handle->stream);
    }
    if (handle->d_output != nullptr) {
        cudaMemsetAsync(handle->d_output, 0, handle->action_dim * sizeof(float), handle->stream);
    }
    cudaStreamSynchronize(handle->stream);
}
