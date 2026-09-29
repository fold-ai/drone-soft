#include "mavlink_bridge/bridge.hpp"
#include "mavlink_v1_cmds.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include <vector>

namespace actprove::mavlink_bridge {
namespace {

constexpr uint8_t kMavlinkV1Stx = 0xfe;
constexpr uint8_t kMsgHeartbeat = 0;
constexpr uint8_t kMsgSetMode = 11;
constexpr uint8_t kMsgRcChannels = 65;
constexpr uint8_t kMsgRequestDataStream = 66;
constexpr uint8_t kMsgSetPositionTargetLocalNed = 84;
constexpr uint8_t kMsgCommandLong = 76;
constexpr uint8_t kMsgCommandAck = 77;
constexpr uint8_t kMsgNamedValueInt = 252;
constexpr uint8_t kCrcHeartbeat = 50;
constexpr uint8_t kCrcSetMode = 89;
constexpr uint8_t kCrcRcChannels = 118;
constexpr uint8_t kCrcRequestDataStream = 148;
constexpr uint8_t kCrcSetPositionTargetLocalNed = 143;
constexpr uint8_t kCrcCommandLong = 152;
constexpr uint8_t kCrcCommandAck = 143;
constexpr uint8_t kCrcNamedValueInt = 44;

void crc_accumulate(uint8_t data, uint16_t& crc) {
  uint8_t tmp = data ^ static_cast<uint8_t>(crc & 0xff);
  tmp ^= static_cast<uint8_t>(tmp << 4);
  crc = static_cast<uint16_t>((crc >> 8) ^ (static_cast<uint16_t>(tmp) << 8) ^
                              (static_cast<uint16_t>(tmp) << 3) ^ (tmp >> 4));
}

uint8_t crc_extra_for(uint8_t msgid) {
  switch (msgid) {
    case kMsgHeartbeat: return kCrcHeartbeat;
    case kMsgSetMode: return kCrcSetMode;
    case kMsgRcChannels: return kCrcRcChannels;
    case kMsgRequestDataStream: return kCrcRequestDataStream;
    case kMsgSetPositionTargetLocalNed: return kCrcSetPositionTargetLocalNed;
    case kMsgCommandLong: return kCrcCommandLong;
    case kMsgCommandAck: return kCrcCommandAck;
    case kMsgNamedValueInt: return kCrcNamedValueInt;
    default: return 0;
  }
}

template <typename T>
void put_le(uint8_t* dst, size_t offset, T value) {
  static_assert(std::is_trivially_copyable<T>::value, "wire field must be POD");
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  std::memcpy(dst + offset, &value, sizeof(T));
#else
  const auto* src = reinterpret_cast<const uint8_t*>(&value);
  for (size_t i = 0; i < sizeof(T); ++i) dst[offset + i] = src[sizeof(T) - 1 - i];
#endif
}

template <typename T>
T get_le(const uint8_t* src, size_t offset) {
  T value{};
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  std::memcpy(&value, src + offset, sizeof(T));
#else
  auto* dst = reinterpret_cast<uint8_t*>(&value);
  for (size_t i = 0; i < sizeof(T); ++i) dst[i] = src[offset + sizeof(T) - 1 - i];
#endif
  return value;
}

bool set_baud(int fd, int baud) {
  speed_t speed;
  switch (baud) {
    case 9600: speed = B9600; break;
    case 57600: speed = B57600; break;
    case 115200: speed = B115200; break;
#ifdef B921600
    case 921600: speed = B921600; break;
#endif
    default: return false;
  }
  termios tio{};
  if (tcgetattr(fd, &tio) != 0) return false;
  cfmakeraw(&tio);
  cfsetispeed(&tio, speed);
  cfsetospeed(&tio, speed);
  tio.c_cflag |= (CLOCAL | CREAD);
#ifdef CRTSCTS
  tio.c_cflag &= ~CRTSCTS;
#endif
  return tcsetattr(fd, TCSANOW, &tio) == 0;
}

double monotonic_seconds() {
  return std::chrono::duration<double>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

uint32_t monotonic_millis() {
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}

}  // namespace

class Bridge::Impl {
 public:
  enum class ParseState : uint8_t {
    WaitStx, Len, Seq, Sysid, Compid, Msgid, Payload, CrcLow, CrcHigh,
  } state{ParseState::WaitStx};

  uint8_t length{0};
  uint8_t sequence{0};
  uint8_t sysid{0};
  uint8_t compid{0};
  uint8_t msgid{0};
  uint8_t payload_index{0};
  uint8_t received_crc_low{0};
  uint16_t crc{0xffff};
  std::array<uint8_t, 255> payload{};
  uint8_t tx_sequence{0};
  double last_heartbeat_s{-1.0};
  bool peer_armed{false};
  uint32_t peer_custom_mode{0};
  std::function<void(const RxEvent&)> handler;

  void reset() {
    state = ParseState::WaitStx;
    payload_index = 0;
    crc = 0xffff;
  }
};

Bridge::Bridge(BridgeConfig cfg)
    : cfg_(std::move(cfg)), impl_(std::make_unique<Impl>()) {}

Bridge::~Bridge() { close(); }

bool Bridge::open() {
  if (cfg_.smoke) {
    fd_ = -1;
    return true;
  }
  fd_ = ::open(cfg_.port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) {
    std::perror("mavlink_bridge open");
    return false;
  }
  if (!set_baud(fd_, cfg_.baud)) {
    std::fprintf(stderr, "mavlink_bridge: unsupported/failed baud %d\n", cfg_.baud);
    close();
    return false;
  }
  return true;
}

void Bridge::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool Bridge::send_frame(uint8_t msgid, const uint8_t* payload, uint8_t payload_len,
                        uint8_t crc_extra) {
  std::vector<uint8_t> frame(static_cast<size_t>(payload_len) + 8);
  frame[0] = kMavlinkV1Stx;
  frame[1] = payload_len;
  frame[2] = impl_->tx_sequence++;
  frame[3] = cfg_.sysid;
  frame[4] = cfg_.compid;
  frame[5] = msgid;
  if (payload_len) std::memcpy(frame.data() + 6, payload, payload_len);
  uint16_t crc = 0xffff;
  for (size_t i = 1; i < static_cast<size_t>(payload_len) + 6; ++i) {
    crc_accumulate(frame[i], crc);
  }
  crc_accumulate(crc_extra, crc);
  frame[6 + payload_len] = static_cast<uint8_t>(crc & 0xff);
  frame[7 + payload_len] = static_cast<uint8_t>(crc >> 8);
  if (cfg_.smoke) return true;
  if (fd_ < 0) return false;
  size_t offset = 0;
  while (offset < frame.size()) {
    const ssize_t n = ::write(fd_, frame.data() + offset, frame.size() - offset);
    if (n <= 0) return false;
    offset += static_cast<size_t>(n);
  }
  return true;
}

void Bridge::send_heartbeat() {
  std::array<uint8_t, 9> payload{};
  put_le<uint32_t>(payload.data(), 0, 0u);
  payload[4] = 18;  // MAV_TYPE_ONBOARD_CONTROLLER
  payload[5] = 8;   // MAV_AUTOPILOT_INVALID
  payload[6] = 0;
  payload[7] = 4;   // MAV_STATE_ACTIVE
  payload[8] = 3;   // MAVLink protocol version
  (void)send_frame(kMsgHeartbeat, payload.data(), payload.size(), kCrcHeartbeat);
}

void Bridge::send_mission_state(uint8_t state) {
  std::array<uint8_t, 18> payload{};
  put_le<uint32_t>(payload.data(), 0, monotonic_millis());
  put_le<int32_t>(payload.data(), 4, static_cast<int32_t>(state));
  constexpr char name[] = "MISSION_STATE";
  std::memcpy(payload.data() + 8, name, 10);  // MAVLink field is exactly 10 bytes.
  (void)send_frame(kMsgNamedValueInt, payload.data(), payload.size(), kCrcNamedValueInt);
}

void Bridge::send_setpoint(const LocalNedSetpoint& sp) {
  std::array<uint8_t, 53> payload{};
  put_le<uint32_t>(payload.data(), 0, monotonic_millis());
  put_le<float>(payload.data(), 16, sp.vx);
  put_le<float>(payload.data(), 20, sp.vy);
  put_le<float>(payload.data(), 24, sp.vz);
  put_le<float>(payload.data(), 40, sp.yaw);
  const uint16_t mask = sp.type_mask ? sp.type_mask : static_cast<uint16_t>(0x09c7);
  put_le<uint16_t>(payload.data(), 48, mask);
  payload[50] = cfg_.target_sysid;
  payload[51] = cfg_.target_compid;
  payload[52] = sp.coordinate_frame ? sp.coordinate_frame : static_cast<uint8_t>(1);
  (void)send_frame(kMsgSetPositionTargetLocalNed, payload.data(), payload.size(),
                   kCrcSetPositionTargetLocalNed);
}

void Bridge::send_command_long(uint16_t command, float param1, float param2,
                              float param3, float param4, float param5,
                              float param6, float param7) {
  std::array<uint8_t, 33> payload{};
  put_le<float>(payload.data(), 0, param1);
  put_le<float>(payload.data(), 4, param2);
  put_le<float>(payload.data(), 8, param3);
  put_le<float>(payload.data(), 12, param4);
  put_le<float>(payload.data(), 16, param5);
  put_le<float>(payload.data(), 20, param6);
  put_le<float>(payload.data(), 24, param7);
  put_le<uint16_t>(payload.data(), 28, command);
  payload[30] = cfg_.target_sysid;
  payload[31] = cfg_.target_compid;
  payload[32] = 0;
  (void)send_frame(kMsgCommandLong, payload.data(), payload.size(), kCrcCommandLong);
}

void Bridge::send_set_mode(uint32_t custom_mode, uint8_t base_mode) {
  std::array<uint8_t, 6> payload{};
  put_le<uint32_t>(payload.data(), 0, custom_mode);
  payload[4] = cfg_.target_sysid;
  payload[5] = base_mode;
  (void)send_frame(kMsgSetMode, payload.data(), payload.size(), kCrcSetMode);
  send_command_long(AP_MAV_CMD_DO_SET_MODE,
                    static_cast<float>(AP_MAV_MODE_FLAG_CUSTOM_MODE_ENABLED),
                    static_cast<float>(custom_mode));
}

void Bridge::request_data_stream(uint8_t stream_id, uint16_t rate_hz, bool start) {
  std::array<uint8_t, 6> payload{};
  put_le<uint16_t>(payload.data(), 0, rate_hz);
  payload[2] = cfg_.target_sysid;
  payload[3] = cfg_.target_compid;
  payload[4] = stream_id;
  payload[5] = start ? 1 : 0;
  (void)send_frame(kMsgRequestDataStream, payload.data(), payload.size(),
                   kCrcRequestDataStream);
}

void Bridge::send_work() {
  send_command_long(AP_MAV_CMD_USER_1, static_cast<float>(AP_UPLINK_WORK));
}
void Bridge::send_abort() {
  send_command_long(AP_MAV_CMD_USER_1, static_cast<float>(AP_UPLINK_ABORT));
}
void Bridge::send_rtb() {
  send_command_long(AP_MAV_CMD_NAV_RETURN_TO_LAUNCH, 0.0f);
}
void Bridge::send_fts() {
  if (!cfg_.allow_flight_termination) {
    std::fprintf(stderr, "mavlink_bridge: flight termination refused (disabled)\n");
    return;
  }
  send_command_long(AP_MAV_CMD_DO_FLIGHTTERMINATION, 1.0f);
}
void Bridge::send_target_destroyed() {
  send_command_long(AP_MAV_CMD_USER_1, static_cast<float>(AP_UPLINK_TARGET_DESTROYED));
}
void Bridge::send_miss() {
  send_command_long(AP_MAV_CMD_USER_1, static_cast<float>(AP_UPLINK_MISS_REATTACK));
}
void Bridge::send_reattack() { send_miss(); }

void Bridge::set_rx_handler(std::function<void(const RxEvent&)> handler) {
  impl_->handler = std::move(handler);
}

size_t Bridge::ingest_bytes(const uint8_t* data, size_t size) {
  size_t accepted = 0;
  for (size_t i = 0; i < size; ++i) {
    const uint8_t byte = data[i];
    switch (impl_->state) {
      case Impl::ParseState::WaitStx:
        if (byte == kMavlinkV1Stx) {
          impl_->crc = 0xffff;
          impl_->payload_index = 0;
          impl_->state = Impl::ParseState::Len;
        }
        break;
      case Impl::ParseState::Len:
        impl_->length = byte;
        crc_accumulate(byte, impl_->crc);
        impl_->state = Impl::ParseState::Seq;
        break;
      case Impl::ParseState::Seq:
        impl_->sequence = byte;
        crc_accumulate(byte, impl_->crc);
        impl_->state = Impl::ParseState::Sysid;
        break;
      case Impl::ParseState::Sysid:
        impl_->sysid = byte;
        crc_accumulate(byte, impl_->crc);
        impl_->state = Impl::ParseState::Compid;
        break;
      case Impl::ParseState::Compid:
        impl_->compid = byte;
        crc_accumulate(byte, impl_->crc);
        impl_->state = Impl::ParseState::Msgid;
        break;
      case Impl::ParseState::Msgid:
        impl_->msgid = byte;
        crc_accumulate(byte, impl_->crc);
        impl_->state = impl_->length ? Impl::ParseState::Payload : Impl::ParseState::CrcLow;
        break;
      case Impl::ParseState::Payload:
        impl_->payload[impl_->payload_index++] = byte;
        crc_accumulate(byte, impl_->crc);
        if (impl_->payload_index == impl_->length) impl_->state = Impl::ParseState::CrcLow;
        break;
      case Impl::ParseState::CrcLow:
        impl_->received_crc_low = byte;
        impl_->state = Impl::ParseState::CrcHigh;
        break;
      case Impl::ParseState::CrcHigh: {
        const uint8_t extra = crc_extra_for(impl_->msgid);
        if (extra) crc_accumulate(extra, impl_->crc);
        const uint16_t received = static_cast<uint16_t>(impl_->received_crc_low) |
                                  (static_cast<uint16_t>(byte) << 8);
        if (extra && received == impl_->crc) {
          RxEvent event{};
          event.sysid = impl_->sysid;
          event.compid = impl_->compid;
          bool emit = false;
          if (impl_->msgid == kMsgHeartbeat && impl_->length >= 9) {
            event.type = RxEventType::Heartbeat;
            event.custom_mode = get_le<uint32_t>(impl_->payload.data(), 0);
            event.mav_type = impl_->payload[4];
            event.autopilot = impl_->payload[5];
            event.base_mode = impl_->payload[6];
            impl_->last_heartbeat_s = monotonic_seconds();
            impl_->peer_custom_mode = event.custom_mode;
            impl_->peer_armed = (event.base_mode & AP_MAV_MODE_FLAG_SAFETY_ARMED) != 0;
            peer_hb_age_s_ = 0.0;
            emit = true;
          } else if (impl_->msgid == kMsgCommandLong && impl_->length >= 33) {
            event.type = RxEventType::CommandLong;
            event.param1 = get_le<float>(impl_->payload.data(), 0);
            event.command = get_le<uint16_t>(impl_->payload.data(), 28);
            emit = true;
          } else if (impl_->msgid == kMsgCommandAck && impl_->length >= 3) {
            event.type = RxEventType::CommandAck;
            event.command = get_le<uint16_t>(impl_->payload.data(), 0);
            event.result = impl_->payload[2];
            emit = true;
          } else if (impl_->msgid == kMsgRcChannels && impl_->length >= 42) {
            event.type = RxEventType::RcChannels;
            event.rc_count = impl_->payload[4];
            for (int i = 0; i < 18; ++i) {
              event.rc_raw[i] = get_le<uint16_t>(impl_->payload.data(), 5 + i * 2);
            }
            emit = true;
          }
          if (emit) {
            ++accepted;
            if (impl_->handler) impl_->handler(event);
          }
        }
        impl_->reset();
        break;
      }
    }
  }
  return accepted;
}

int Bridge::spin_once() {
  if (fd_ < 0) return 0;
  std::array<uint8_t, 512> buffer{};
  const ssize_t n = ::read(fd_, buffer.data(), buffer.size());
  if (n <= 0) return 0;
  (void)ingest_bytes(buffer.data(), static_cast<size_t>(n));
  return static_cast<int>(n);
}

double Bridge::peer_hb_age_s() const {
  if (impl_->last_heartbeat_s < 0.0) return 1e9;
  return std::max(0.0, monotonic_seconds() - impl_->last_heartbeat_s);
}

bool Bridge::peer_armed() const { return impl_->peer_armed; }

uint32_t Bridge::peer_custom_mode() const { return impl_->peer_custom_mode; }

}  // namespace actprove::mavlink_bridge
