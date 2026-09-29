#include "logging/blackbox.hpp"
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace actprove::logging {

static std::string stamp_id() {
  using namespace std::chrono;
  const auto t = system_clock::now();
  const auto tt = system_clock::to_time_t(t);
  std::tm tm{};
  localtime_r(&tt, &tm);
  std::ostringstream os;
  os << std::put_time(&tm, "%Y%m%d_%H%M%S") << "_run";
  return os.str();
}

BlackboxWriter::BlackboxWriter(BlackboxConfig cfg) : cfg_(std::move(cfg)) {}
BlackboxWriter::~BlackboxWriter() {
  std::lock_guard<std::mutex> lock(mu_);
  if (meta_.is_open()) meta_.close();
}

bool BlackboxWriter::open_run() {
  const std::string id = cfg_.run_id.empty() ? stamp_id() : cfg_.run_id;
  run_dir_ = cfg_.root + "/" + id;
  std::error_code ec;
  fs::create_directories(run_dir_, ec);
  if (ec) return false;
  meta_.open(run_dir_ + "/meta.json");
  if (!meta_) return false;
  meta_ << "{\"run_id\":\"" << id
        << "\",\"schema\":\"logging/schema/blackbox_streams.json\""
        << ",\"capacity_note\":\"NVMe >=1-2h V1 rates\"}\n";
  meta_.flush();
  return true;
}

void BlackboxWriter::write_jsonl(const std::string& stream, const std::string& line) {
  std::lock_guard<std::mutex> lock(mu_);
  std::ofstream out(run_dir_ + "/" + stream + ".jsonl", std::ios::app);
  out << line << "\n";
}

void BlackboxWriter::write_meta(const std::string& json) {
  std::lock_guard<std::mutex> lock(mu_);
  if (meta_.is_open()) {
    meta_ << json << "\n";
    meta_.flush();
  }
}

void BlackboxWriter::write_camera_ts(const CameraTsRecord& r) {
  std::ostringstream os;
  os << "{\"measurement_epoch_ns\":" << r.measurement_epoch_ns
     << ",\"publish_time_ns\":" << r.publish_time_ns
     << ",\"seq\":" << r.seq
     << ",\"cam_id\":" << static_cast<int>(r.cam_id)
     << ",\"exposure_us\":" << r.exposure_us << "}";
  write_jsonl("camera_ts", os.str());
}

void BlackboxWriter::write_track(const TrackRecord& r) {
  std::ostringstream os;
  os << "{\"track_id\":" << r.track_id
     << ",\"measurement_epoch_ns\":" << r.measurement_epoch_ns
     << ",\"publish_time_ns\":" << r.publish_time_ns
     << ",\"lock_quality\":" << r.lock_quality
     << ",\"state\":" << static_cast<int>(r.state)
     << ",\"cam_id\":" << static_cast<int>(r.cam_id) << "}";
  write_jsonl("tracks", os.str());
}

void BlackboxWriter::write_ownship(const OwnShipRecord& r) {
  std::ostringstream os;
  os << "{\"measurement_epoch_ns\":" << r.measurement_epoch_ns
     << ",\"airspeed_mps\":" << r.airspeed_mps
     << ",\"heading_rad\":" << r.heading_rad
     << ",\"alt_m\":" << r.alt_m << "}";
  write_jsonl("ownship", os.str());
}

void BlackboxWriter::write_mavlink_setpoint(const MavlinkSetpointRecord& r) {
  std::ostringstream os;
  os << "{\"publish_time_ns\":" << r.publish_time_ns
     << ",\"vx\":" << r.vx << ",\"vy\":" << r.vy << ",\"vz\":" << r.vz
     << ",\"yaw\":" << r.yaw << ",\"type_mask\":" << r.type_mask << "}";
  write_jsonl("mavlink_tx", os.str());
}

void BlackboxWriter::write_bda(const BdaRecord& r) {
  std::ostringstream os;
  os << "{\"publish_time_ns\":" << r.publish_time_ns
     << ",\"cmd\":\"" << r.cmd << "\""
     << ",\"mission_state\":\"" << r.mission_state << "\""
     << ",\"video_source\":\"" << r.video_source << "\"}";
  write_jsonl("bda", os.str());
}

void BlackboxWriter::write_gcs_event(const GcsEventRecord& r) {
  std::ostringstream os;
  os << "{\"publish_time_ns\":" << r.publish_time_ns
     << ",\"event\":\"" << r.event << "\""
     << ",\"video_source\":\"" << r.video_source << "\"}";
  write_jsonl("gcs_events", os.str());
}

}  // namespace actprove::logging
