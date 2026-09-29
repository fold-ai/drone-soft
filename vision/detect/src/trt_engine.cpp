#include "detect/trt_engine.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <utility>

#ifndef SOFT_NO_JETPACK
#include <NvInfer.h>
#include <cuda_runtime_api.h>
#endif

namespace actprove::detect {

class TrtEngine::Impl {
 public:
#ifndef SOFT_NO_JETPACK
  class Logger final : public nvinfer1::ILogger {
   public:
    void log(Severity severity, const char* msg) noexcept override {
      if (severity <= Severity::kWARNING) std::fprintf(stderr, "TensorRT: %s\n", msg);
    }
  } logger;

  nvinfer1::IRuntime* runtime{nullptr};
  nvinfer1::ICudaEngine* engine{nullptr};
  nvinfer1::IExecutionContext* context{nullptr};
  cudaStream_t stream{nullptr};
  std::vector<void*> bindings;
  int input_index{-1};
  int output_index{-1};
  std::string input_name;
  std::string output_name;
  void* input_device{nullptr};
  void* output_device{nullptr};

  ~Impl() {
    if (input_device) cudaFree(input_device);
    if (output_device) cudaFree(output_device);
    if (stream) cudaStreamDestroy(stream);
#if NV_TENSORRT_MAJOR >= 10
    delete context;
    delete engine;
    delete runtime;
#else
    if (context) context->destroy();
    if (engine) engine->destroy();
    if (runtime) runtime->destroy();
#endif
  }
#endif
};

namespace {
#ifndef SOFT_NO_JETPACK
size_t volume(const nvinfer1::Dims& dims) {
  size_t result = 1;
  for (int i = 0; i < dims.nbDims; ++i) {
    if (dims.d[i] <= 0) return 0;
    result *= static_cast<size_t>(dims.d[i]);
  }
  return result;
}

std::vector<int64_t> shape_of(const nvinfer1::Dims& dims) {
  std::vector<int64_t> shape;
  shape.reserve(dims.nbDims);
  for (int i = 0; i < dims.nbDims; ++i) shape.push_back(dims.d[i]);
  return shape;
}
#endif
}  // namespace

TrtEngine::TrtEngine(std::string path)
    : path_(std::move(path)), impl_(std::make_unique<Impl>()) {}

TrtEngine::~TrtEngine() = default;

bool TrtEngine::build_or_load() {
#ifdef SOFT_NO_JETPACK
  ready_ = false;
  return false;
#else
  std::ifstream file(path_, std::ios::binary);
  if (!file) return false;
  const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), {});
  if (bytes.empty()) return false;

  impl_->runtime = nvinfer1::createInferRuntime(impl_->logger);
  if (!impl_->runtime) return false;
  impl_->engine = impl_->runtime->deserializeCudaEngine(bytes.data(), bytes.size());
  if (!impl_->engine) return false;
  impl_->context = impl_->engine->createExecutionContext();
  if (!impl_->context) return false;

#if NV_TENSORRT_MAJOR >= 10
  for (int i = 0; i < impl_->engine->getNbIOTensors(); ++i) {
    const char* name = impl_->engine->getIOTensorName(i);
    if (!name) return false;
    const auto dims = impl_->engine->getTensorShape(name);
    if (volume(dims) == 0 ||
        impl_->engine->getTensorDataType(name) != nvinfer1::DataType::kFLOAT) return false;
    if (impl_->engine->getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT &&
        impl_->input_name.empty()) {
      impl_->input_name = name;
      input_shape_ = shape_of(dims);
      input_bytes_ = volume(dims) * sizeof(float);
    } else if (impl_->engine->getTensorIOMode(name) == nvinfer1::TensorIOMode::kOUTPUT &&
               impl_->output_name.empty()) {
      impl_->output_name = name;
      output_shape_ = shape_of(dims);
      output_bytes_ = volume(dims) * sizeof(float);
    }
  }
  if (impl_->input_name.empty() || impl_->output_name.empty() ||
      input_bytes_ == 0 || output_bytes_ == 0) return false;
#else
  impl_->bindings.assign(static_cast<size_t>(impl_->engine->getNbBindings()), nullptr);
  for (int i = 0; i < impl_->engine->getNbBindings(); ++i) {
    const auto dims = impl_->engine->getBindingDimensions(i);
    if (volume(dims) == 0 ||
        impl_->engine->getBindingDataType(i) != nvinfer1::DataType::kFLOAT) {
      return false;
    }
    if (impl_->engine->bindingIsInput(i) && impl_->input_index < 0) {
      impl_->input_index = i;
      input_shape_ = shape_of(dims);
      input_bytes_ = volume(dims) * sizeof(float);
    } else if (!impl_->engine->bindingIsInput(i) && impl_->output_index < 0) {
      impl_->output_index = i;
      output_shape_ = shape_of(dims);
      output_bytes_ = volume(dims) * sizeof(float);
    }
  }
  if (impl_->input_index < 0 || impl_->output_index < 0 ||
      input_bytes_ == 0 || output_bytes_ == 0) return false;
#endif
  if (cudaStreamCreate(&impl_->stream) != cudaSuccess ||
      cudaMalloc(&impl_->input_device, input_bytes_) != cudaSuccess ||
      cudaMalloc(&impl_->output_device, output_bytes_) != cudaSuccess) return false;
#if NV_TENSORRT_MAJOR >= 10
  if (!impl_->context->setTensorAddress(impl_->input_name.c_str(), impl_->input_device) ||
      !impl_->context->setTensorAddress(impl_->output_name.c_str(), impl_->output_device)) return false;
#else
  impl_->bindings[static_cast<size_t>(impl_->input_index)] = impl_->input_device;
  impl_->bindings[static_cast<size_t>(impl_->output_index)] = impl_->output_device;
#endif
  ready_ = true;
  return true;
#endif
}

bool TrtEngine::infer(const std::vector<float>& host_input,
                      std::vector<float>& host_output) {
#ifdef SOFT_NO_JETPACK
  (void)host_input;
  (void)host_output;
  return false;
#else
  if (!ready_ || host_input.size() * sizeof(float) != input_bytes_) return false;
  host_output.resize(output_bytes_ / sizeof(float));
  if (cudaMemcpyAsync(impl_->input_device, host_input.data(), input_bytes_,
                      cudaMemcpyHostToDevice, impl_->stream) != cudaSuccess) return false;
#if NV_TENSORRT_MAJOR >= 10
  if (!impl_->context->enqueueV3(impl_->stream)) return false;
#else
  if (!impl_->context->enqueueV2(impl_->bindings.data(), impl_->stream, nullptr)) return false;
#endif
  if (cudaMemcpyAsync(host_output.data(), impl_->output_device, output_bytes_,
                      cudaMemcpyDeviceToHost, impl_->stream) != cudaSuccess) return false;
  return cudaStreamSynchronize(impl_->stream) == cudaSuccess;
#endif
}

}  // namespace actprove::detect
