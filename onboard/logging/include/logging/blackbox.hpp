#pragma once
// NVMe black-box logger — ≥1–2 h continuous soak target.
// Schema: camera_ts, tracks, own-ship, mavlink setpoints (ICD + G1 layout).

#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>

namespace actprove::logging {

struct BlackboxConfig {
  std::string root{"/data/blackbox"};  // Orin NVMe mount
  std::string run_id;                  // empty → auto YYYYMMDD_HHMMSS_run
  std::size_t rotate_bytes{1u << 20};  // 1 MiB segment hint (scaffold)
};

/// Record kinds written as JSONL streams under the run folder.
struct CameraTsRecord {
  int64_t measurement_epoch_ns{0};
  int64_t publish_time_ns{0};
  uint64_t seq{0};
  uint8_t cam_id{0};
  double exposure_us{0.0};
};

struct TrackRecord {
  uint32_t track_id{0};
  int64_t measurement_epoch_ns{0};  // last real measurement (coast keeps epoch)
  int64_t publish_time_ns{0};
  float lock_quality{0.f};
  uint8_t state{0};
  uint8_t cam_id{0};
};

struct OwnShipRecord {
  int64_t measurement_epoch_ns{0};
  float airspeed_mps{0.f};
  float heading_rad{0.f};
  float alt_m{0.f};
};

struct MavlinkSetpointRecord {
  int64_t publish_time_ns{0};
  float vx{0.f};
  float vy{0.f};
  float vz{0.f};
  float yaw{0.f};
  uint16_t type_mask{0};
};

/// Operator BDA command log (GCS / bridge → safety_gates).
struct BdaRecord {
  int64_t publish_time_ns{0};
  std::string cmd;           // TARGET_DESTROYED | MISS | REATTACK (UX)
  std::string mission_state; // BOOT|SEARCH|LOCK|CLOSE|ABORT|RTB|FTS
  std::string video_source;  // nose | ground
};

/// Optional GCS UI / C2 events (e.g. video source toggle).
struct GcsEventRecord {
  int64_t publish_time_ns{0};
  std::string event;         // e.g. video_source_changed
  std::string video_source;  // nose | ground
};

class BlackboxWriter {
 public:
  explicit BlackboxWriter(BlackboxConfig cfg);
  ~BlackboxWriter();

  bool open_run();
  const std::string& run_dir() const { return run_dir_; }

  void write_jsonl(const std::string& stream, const std::string& line);
  void write_meta(const std::string& json);

  void write_camera_ts(const CameraTsRecord& r);
  void write_track(const TrackRecord& r);
  void write_ownship(const OwnShipRecord& r);
  void write_mavlink_setpoint(const MavlinkSetpointRecord& r);
  void write_bda(const BdaRecord& r);
  void write_gcs_event(const GcsEventRecord& r);

 private:
  BlackboxConfig cfg_;
  std::string run_dir_;
  std::ofstream meta_;
  std::mutex mu_;
};

}  // namespace actprove::logging
