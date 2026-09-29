#pragma once

#include "iface/box_message.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace actprove::iface {

// Stable little-endian datagram format. It avoids sending compiler-dependent
// struct padding across process boundaries.
std::vector<std::uint8_t> encode_detection_v1(const DetectionMsg& msg);
bool decode_detection_v1(const std::uint8_t* data, std::size_t size,
                         DetectionMsg& out);

class UnixDetectionPublisher {
 public:
  explicit UnixDetectionPublisher(std::string socket_path);
  ~UnixDetectionPublisher();

  bool open();
  void close();
  bool publish(const DetectionMsg& msg);
  std::uint64_t sent() const { return sent_; }
  std::uint64_t dropped() const { return dropped_; }

 private:
  std::string socket_path_;
  int fd_{-1};
  std::uint64_t sent_{0};
  std::uint64_t dropped_{0};
};

class UnixDetectionSubscriber {
 public:
  explicit UnixDetectionSubscriber(std::string socket_path);
  ~UnixDetectionSubscriber();

  bool open();
  void close();
  bool receive(DetectionMsg& out, int timeout_ms);

 private:
  std::string socket_path_;
  int fd_{-1};
};

}  // namespace actprove::iface
