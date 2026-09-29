#pragma once

#include "iface/box_message.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace actprove::iface {

// Bounded single-producer/single-consumer queue for detections inside one
// process. This is deliberately not advertised as cross-process IPC.
//
// Producer writes the complete DetectionMsg before publishing write_ with
// release semantics. Consumer acquires write_ before reading the slot. When
// full, publish() fails and increments dropped_ instead of overwriting unread
// data.
class BoxPublisher {
 public:
  static constexpr std::size_t kCapacity = 8;

  bool publish(const DetectionMsg& msg);
  bool try_read(DetectionMsg& out);

  std::size_t approximate_size() const;
  std::uint64_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

 private:
  std::array<DetectionMsg, kCapacity> slots_{};
  alignas(64) std::atomic<std::size_t> write_{0};
  alignas(64) std::atomic<std::size_t> read_{0};
  std::atomic<std::uint64_t> dropped_{0};
};

}  // namespace actprove::iface
