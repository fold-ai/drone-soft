#include "iface/box_publisher.hpp"

#include <algorithm>

namespace actprove::iface {

bool BoxPublisher::publish(const DetectionMsg& msg) {
  const std::size_t write = write_.load(std::memory_order_relaxed);
  const std::size_t read = read_.load(std::memory_order_acquire);
  if (write - read >= kCapacity) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  slots_[write % kCapacity] = msg;
  write_.store(write + 1, std::memory_order_release);
  return true;
}

bool BoxPublisher::try_read(DetectionMsg& out) {
  const std::size_t read = read_.load(std::memory_order_relaxed);
  const std::size_t write = write_.load(std::memory_order_acquire);
  if (read == write) return false;
  out = slots_[read % kCapacity];
  read_.store(read + 1, std::memory_order_release);
  return true;
}

std::size_t BoxPublisher::approximate_size() const {
  const std::size_t write = write_.load(std::memory_order_acquire);
  const std::size_t read = read_.load(std::memory_order_acquire);
  return std::min(write - read, kCapacity);
}

// Exported C API for scaffold linking
extern "C" int actprove_box_pub_smoke() {
  BoxPublisher p;
  DetectionMsg m{};
  m.t_pps = 0;
  m.n = 0;
  return p.publish(m) ? 0 : 1;
}

}  // namespace actprove::iface
