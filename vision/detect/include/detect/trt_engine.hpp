#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace actprove::detect {

class TrtEngine {
 public:
  explicit TrtEngine(std::string path);
  ~TrtEngine();
  bool build_or_load();
  bool infer(const std::vector<float>& host_input, std::vector<float>& host_output);
  size_t input_bytes() const { return input_bytes_; }
  size_t output_bytes() const { return output_bytes_; }
  const std::vector<int64_t>& input_shape() const { return input_shape_; }
  const std::vector<int64_t>& output_shape() const { return output_shape_; }
 private:
  class Impl;
  std::string path_;
  std::unique_ptr<Impl> impl_;
  size_t input_bytes_{0};
  size_t output_bytes_{0};
  std::vector<int64_t> input_shape_;
  std::vector<int64_t> output_shape_;
  bool ready_{false};
};

}  // namespace actprove::detect
