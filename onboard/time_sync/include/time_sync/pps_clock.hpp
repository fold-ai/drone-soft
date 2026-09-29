#pragma once
// PPS-disciplined clock types — ICD: docs/icd/time_sync.md
// Soft host now() is FORBIDDEN for sensor / track timestamps.
// TODO(orin): wire GNSS PPS GPIO / /dev/pps0 / HTE latch (Forecr FAE pin TBD).

#include <cstdint>
#include <atomic>
#include <optional>

namespace actprove::time_sync {

/// Absolute time in the PPS domain (nanoseconds since an epoch agreed at lock).
using PpsTimeNs = int64_t;

/// Frame / sample timestamp: mid-exposure (or DRDY edge) in PPS domain.
struct FrameTimestamp {
  PpsTimeNs measurement_epoch_ns{0};  // mid-exposure or DRDY — NOT dequeue time
  PpsTimeNs publish_time_ns{0};       // optional debug; never substitute for epoch
  uint64_t seq{0};
  bool pps_valid{false};
};

/// Disciplined PPS clock shared by capture, IMU, track, logger.
class PpsClock {
 public:
  struct Snapshot {
    PpsTimeNs last_edge_ns{0};
    uint64_t edge_count{0};
    bool disciplined{false};
  };

  void note_pps_edge(PpsTimeNs edge_ns) {
    last_edge_ns_.store(edge_ns, std::memory_order_release);
    edges_.fetch_add(1, std::memory_order_relaxed);
    disciplined_.store(true, std::memory_order_release);
  }

  Snapshot snapshot() const {
    return Snapshot{
        last_edge_ns_.load(std::memory_order_acquire),
        edges_.load(std::memory_order_relaxed),
        disciplined_.load(std::memory_order_acquire),
    };
  }

  bool ready_for_cameras() const {
    return disciplined_.load(std::memory_order_acquire) &&
           edges_.load(std::memory_order_relaxed) >= 1;
  }

  /// Interpolate now from last edge + monotonic offset.
  /// TODO(orin): replace with HTE / kernel pps_ktime once GPIO named.
  PpsTimeNs now_estimate_ns(PpsTimeNs mono_offset_ns) const {
    return last_edge_ns_.load(std::memory_order_acquire) + mono_offset_ns;
  }

 private:
  std::atomic<PpsTimeNs> last_edge_ns_{0};
  std::atomic<uint64_t> edges_{0};
  std::atomic<bool> disciplined_{false};
};

/// Mid-exposure latch helper (sof + 0.5 * exposure).
inline PpsTimeNs mid_exposure_ns(PpsTimeNs sof_ns, double exposure_us) {
  const auto half = static_cast<PpsTimeNs>(exposure_us * 500.0);  // us → ns / 2
  return sof_ns + half;
}

}  // namespace actprove::time_sync
