#pragma once
// Closed loop without I/O: detections + RC + FC heartbeat → GUIDED intent.
// main.cpp owns UART / IPC.

#include "companion/image_homing.hpp"
#include "companion/rc_gates.hpp"

#include "tracking/tracker.hpp"

#include <actprove/detection_msg.hpp>
#include <cstdint>
#include <string>

namespace actprove::companion {

struct CompanionConfig {
  RcGateConfig rc{};
  HomingConfig homing{};
  bool allow_any_class{true};
  bool require_armed{true};
  bool prefer_center{true};
  float min_conf{0.30f};
  uint32_t confirm_hits{2};
  float t_coast_s{0.45f};
  uint32_t guided_mode{15};  // ArduPlane GUIDED
  uint32_t manual_mode{5};   // FBWA — stabilized sticks to climb and land
  float lost_link_s{3.f};
};

struct PlaneAttitude {
  float roll_rad{0.f};
  float pitch_rad{0.f};
  float yaw_rad{0.f};
  float throttle{0.f};
  bool valid{false};
};

enum class CompanionPhase : uint8_t {
  Manual = 0,
  Search = 1,
  Close = 2,
};

struct CompanionStatus {
  CompanionPhase phase{CompanionPhase::Manual};
  RcIntent intent{RcIntent::Manual};
  bool lock_latched{false};
  bool armed{false};
  bool track_ok{false};
  bool setpoint_valid{false};
  bool want_guided{false};
  bool want_manual{false};
  bool want_rtl{false};
  bool have_yaw{false};
  uint8_t mission_state{0};
  uint32_t track_id{0};
  float roll_rad{0.f};
  float pitch_rad{0.f};
  float throttle{0.f};
  float range_est_m{0.f};
  float lock_quality{0.f};
};

class CompanionApp {
 public:
  explicit CompanionApp(CompanionConfig cfg = {});

  void on_detection(const actprove::DetectionMsg& det);
  void on_rc(const uint16_t* ch, uint8_t n);
  void on_fc_heartbeat(bool armed, uint32_t custom_mode, double now_s);
  void on_attitude(float roll_rad, float pitch_rad, float yaw_rad);
  void tick(double now_s);

  CompanionStatus status() const { return status_; }
  PlaneAttitude attitude() const;
  const actprove::TrackMsg& track() const { return tracker_.track(); }

  void reset();

 private:
  CompanionConfig cfg_;
  tracking::Tracker tracker_;
  RcGateState rc_{};
  CompanionStatus status_{};
  uint16_t src_w_{0};
  uint16_t src_h_{0};
  double last_fc_hb_s_{-1.0};
  uint32_t fc_mode_{0};
  float last_yaw_rad_{0.f};
  bool have_yaw_{false};
  bool searching_{false};
  double search_t0_s_{0.0};
  HomingOutput homing_{};

  actprove::DetectionMsg maybe_center(const actprove::DetectionMsg& det) const;
};

}  // namespace actprove::companion
