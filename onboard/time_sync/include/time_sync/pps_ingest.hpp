#pragma once
// PPS edge ingest stub — System stack owns path; Navigation owns IMU domain policy.
// Do NOT start cameras until PpsIngest reports disciplined (bringup start-order).

#include "time_sync/pps_clock.hpp"
#include <atomic>
#include <string>
#include <thread>

namespace actprove::time_sync {

class PpsIngest {
 public:
  struct Config {
    std::string pps_device{"/dev/pps0"};  // TODO(orin): Forecr FAE pin → device
    std::string gpio_label;               // TODO(orin): e.g. "PPS_IN"
    bool simulate{false};                 // host tests must explicitly opt in
  };

  explicit PpsIngest(Config cfg);
  ~PpsIngest();

  bool start();
  void stop();

  /// Seconds since process-local t0 of last edge (legacy API used by scaffold).
  double now_pps() const;
  uint64_t edge_count() const { return edges_.load(); }
  bool disciplined() const { return disciplined_.load(); }

  PpsClock& clock() { return clock_; }
  const PpsClock& clock() const { return clock_; }

 private:
  void loop();
  Config cfg_;
  PpsClock clock_;
  std::atomic<bool> run_{false};
  std::atomic<double> last_edge_pps_{0.0};
  std::atomic<uint64_t> edges_{0};
  std::atomic<bool> disciplined_{false};
  std::thread thr_;
};

double mid_exposure_pps(double sof_pps, double exposure_us);

}  // namespace actprove::time_sync
