#pragma once
// ADIS16470-class IMU sync contract for Navigation PASS.
// Owner: Navigation (+ Hardware / Integration for pins).
//
// LOCKED POLICY (V1):
//   - Mode: free-run + DRDY HW-timestamped on PPS-disciplined Orin clock.
//   - Cam FSYNC (30/60 Hz) must NEVER drive ADIS SYNC.
//   - Soft stamp (clock_gettime on SPI done) = REJECT for Nav PASS.
//   - Optional later: high-rate SYNC 200–2000 Hz from same PPS timebase.
//
// Carrier note (2026-09-16): Forecr DSBOARD-ORNX SPI0 @ 3.3 V (DF11) ACCEPT;
// DRDY→HTE GPIO still FAIL/HOLD pending FAE. Soft SPI REJECT.

#include <cstdint>

namespace navigation {

enum class ImuSyncMode : uint8_t {
  kFreeRunDrdyHwTs = 0,  // V1 required
  kHighRateSyncPps = 1,  // allowed alternate
  kCamFsyncSync = 2,     // FORBIDDEN
  kSoftStamp = 3,        // FORBIDDEN for PASS
};

struct AdisSample {
  double t_pps = 0.0;  // HW timestamp in PPS domain (required)
  float gyro_rps[3]{};
  float accel_mps2[3]{};
  uint32_t seq = 0;
  bool hw_timestamped = false;
};

inline bool adis_mode_acceptable(ImuSyncMode m) {
  return m == ImuSyncMode::kFreeRunDrdyHwTs || m == ImuSyncMode::kHighRateSyncPps;
}

inline bool adis_sample_acceptable(const AdisSample& s) {
  return s.hw_timestamped && s.t_pps > 0.0;
}

}  // namespace navigation
