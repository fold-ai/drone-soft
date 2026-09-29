#include "iface/box_publisher.hpp"

#include <atomic>
#include <cassert>
#include <cstdint>
#include <thread>

using actprove::iface::BoxPublisher;
using actprove::iface::DetectionMsg;

int main() {
  BoxPublisher queue;
  DetectionMsg out{};
  assert(!queue.try_read(out));

  for (std::uint64_t seq = 1; seq <= BoxPublisher::kCapacity; ++seq) {
    DetectionMsg msg{};
    msg.seq = seq;
    assert(queue.publish(msg));
  }
  assert(queue.approximate_size() == BoxPublisher::kCapacity);

  DetectionMsg overflow{};
  overflow.seq = 99;
  assert(!queue.publish(overflow));
  assert(queue.dropped() == 1);

  for (std::uint64_t seq = 1; seq <= BoxPublisher::kCapacity; ++seq) {
    assert(queue.try_read(out));
    assert(out.seq == seq);
  }
  assert(!queue.try_read(out));

  constexpr std::uint64_t kMessages = 20000;
  BoxPublisher concurrent;
  std::atomic<bool> producer_done{false};
  std::thread producer([&] {
    for (std::uint64_t seq = 1; seq <= kMessages; ++seq) {
      DetectionMsg msg{};
      msg.seq = seq;
      while (!concurrent.publish(msg)) std::this_thread::yield();
    }
    producer_done.store(true, std::memory_order_release);
  });

  std::uint64_t expected = 1;
  while (!producer_done.load(std::memory_order_acquire) || concurrent.approximate_size() > 0) {
    if (!concurrent.try_read(out)) {
      std::this_thread::yield();
      continue;
    }
    assert(out.seq == expected++);
  }
  producer.join();
  assert(expected == kMessages + 1);
  return 0;
}
