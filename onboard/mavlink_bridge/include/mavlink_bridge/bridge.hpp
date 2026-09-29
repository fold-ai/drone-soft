#pragma once
// Orin ↔ FC UART MAVLink on TELEM2 — common.xml v1 only.
// TELEM1 = RFD900x GCS C2. USB is NOT primary. Video NOT on MAVLink.
// ICD: docs/icd/mavlink_v1.md · constants: interfaces/mavlink/

#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace actprove::mavlink_bridge {

struct BridgeConfig {
  std::string port{"/dev/ttyTHS1"};  // Orin UART wired to FC TELEM2
  int baud{57600};                   // match FC SERIALn; AP_TELEM_BAUD
  uint8_t sysid{1};
  uint8_t compid{191};               // MAV_COMP_ID_ONBOARD_COMPUTER
  uint8_t target_sysid{1};
  uint8_t target_compid{1};
  bool smoke{false};
  // Destructive flight termination is disabled in normal/test builds.
  bool allow_flight_termination{false};
};

/// SET_POSITION_TARGET_LOCAL_NED — velocity (optionally yaw).
struct LocalNedSetpoint {
  float vx{0.f};
  float vy{0.f};
  float vz{0.f};   // NED down-positive / body-down when frame is BODY_*
  float yaw{0.f};  // desired heading rad (ignored when type_mask says so)
  uint16_t type_mask{0};
  uint8_t coordinate_frame{1};  // MAV_FRAME_LOCAL_NED
};

enum class RxEventType : uint8_t {
  Heartbeat,
  CommandLong,
  CommandAck,
  RcChannels,
};

struct RxEvent {
  RxEventType type{RxEventType::Heartbeat};
  uint8_t sysid{0};
  uint8_t compid{0};
  uint16_t command{0};
  float param1{0.0f};
  uint8_t result{0};
  uint32_t custom_mode{0};
  uint8_t base_mode{0};
  uint8_t mav_type{0};
  uint8_t autopilot{0};
  uint8_t rc_count{0};
  uint16_t rc_raw[18]{};
};

class Bridge {
 public:
  explicit Bridge(BridgeConfig cfg);
  ~Bridge();

  bool open();
  void close();

  void send_heartbeat();
  void send_mission_state(uint8_t state);  // NAMED_VALUE_INT MISSION_STATE 0..6
  void send_setpoint(const LocalNedSetpoint& sp);
  void send_set_mode(uint32_t custom_mode, uint8_t base_mode);
  void send_command_long(uint16_t command, float param1, float param2 = 0.f,
                         float param3 = 0.f, float param4 = 0.f,
                         float param5 = 0.f, float param6 = 0.f,
                         float param7 = 0.f);
  void request_data_stream(uint8_t stream_id, uint16_t rate_hz, bool start = true);

  void send_work();   // USER_1 param1=1
  void send_abort();  // USER_1 param1=2 — ≠ FTS
  void send_rtb();    // NAV_RETURN_TO_LAUNCH 20
  void send_fts();    // guarded; no-op unless allow_flight_termination=true

  // Operator BDA (GCS C2 on TELEM1 may originate; Orin bridge accepts/forwards).
  // Forward into safety_gates as exact strings TARGET_DESTROYED / MISS/REATTACK.
  void send_target_destroyed();  // USER_1 param1=3 → TARGET_DESTROYED → RTB
  void send_miss();              // USER_1 param1=4 → MISS/REATTACK → ABORT
  void send_reattack();          // USER_1 param1=4 (V1 same as miss; frozen ICD has no param1=5)

  /// Non-blocking read + MAVLink v1 parse.
  int spin_once();
  size_t ingest_bytes(const uint8_t* data, size_t size);
  void set_rx_handler(std::function<void(const RxEvent&)> handler);

  double peer_hb_age_s() const;
  bool peer_armed() const;
  uint32_t peer_custom_mode() const;

 private:
  class Impl;
  bool send_frame(uint8_t msgid, const uint8_t* payload, uint8_t payload_len,
                  uint8_t crc_extra);
  BridgeConfig cfg_;
  std::unique_ptr<Impl> impl_;
  int fd_{-1};
  double peer_hb_age_s_{1e9};
};

}  // namespace actprove::mavlink_bridge
