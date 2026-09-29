#include "mavlink_bridge/bridge.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {
void crc_accumulate(uint8_t data, uint16_t& crc) {
  uint8_t tmp = data ^ static_cast<uint8_t>(crc & 0xff);
  tmp ^= static_cast<uint8_t>(tmp << 4);
  crc = static_cast<uint16_t>((crc >> 8) ^ (static_cast<uint16_t>(tmp) << 8) ^
                              (static_cast<uint16_t>(tmp) << 3) ^ (tmp >> 4));
}

std::vector<uint8_t> frame(uint8_t msgid, const uint8_t* payload,
                           uint8_t length, uint8_t crc_extra) {
  std::vector<uint8_t> out(static_cast<size_t>(length) + 8);
  out[0] = 0xfe;
  out[1] = length;
  out[2] = 7;
  out[3] = 1;
  out[4] = 1;
  out[5] = msgid;
  if (length) std::memcpy(out.data() + 6, payload, length);
  uint16_t crc = 0xffff;
  for (size_t i = 1; i < static_cast<size_t>(length) + 6; ++i) crc_accumulate(out[i], crc);
  crc_accumulate(crc_extra, crc);
  out[6 + length] = static_cast<uint8_t>(crc & 0xff);
  out[7 + length] = static_cast<uint8_t>(crc >> 8);
  return out;
}
}  // namespace

int main() {
  using namespace actprove::mavlink_bridge;
  BridgeConfig config;
  config.smoke = true;
  Bridge bridge(config);
  assert(bridge.open());

  int heartbeats = 0;
  int acknowledgements = 0;
  bridge.set_rx_handler([&](const RxEvent& event) {
    if (event.type == RxEventType::Heartbeat) ++heartbeats;
    if (event.type == RxEventType::CommandAck && event.command == 20 && event.result == 0) {
      ++acknowledgements;
    }
  });

  std::array<uint8_t, 9> heartbeat{};
  heartbeat[8] = 3;
  const auto heartbeat_frame = frame(0, heartbeat.data(), heartbeat.size(), 50);
  assert(bridge.ingest_bytes(heartbeat_frame.data(), 3) == 0);  // fragmented input
  assert(bridge.ingest_bytes(heartbeat_frame.data() + 3, heartbeat_frame.size() - 3) == 1);
  assert(heartbeats == 1);
  assert(bridge.peer_hb_age_s() < 1.0);

  auto bad = heartbeat_frame;
  bad.back() ^= 0xff;
  assert(bridge.ingest_bytes(bad.data(), bad.size()) == 0);
  assert(heartbeats == 1);

  std::array<uint8_t, 3> ack{};
  ack[0] = 20;
  ack[1] = 0;
  ack[2] = 0;
  const auto ack_frame = frame(77, ack.data(), ack.size(), 143);
  assert(bridge.ingest_bytes(ack_frame.data(), ack_frame.size()) == 1);
  assert(acknowledgements == 1);

  int rc_events = 0;
  uint16_t ch7 = 0;
  bridge.set_rx_handler([&](const RxEvent& event) {
    if (event.type == RxEventType::RcChannels) {
      ++rc_events;
      ch7 = event.rc_raw[6];
    }
  });
  std::array<uint8_t, 42> rc{};
  rc[4] = 12;
  rc[5 + 6 * 2] = 0xE8;  // CH7 = 2024
  rc[5 + 6 * 2 + 1] = 0x07;
  const auto rc_frame = frame(65, rc.data(), rc.size(), 118);
  assert(bridge.ingest_bytes(rc_frame.data(), rc_frame.size()) == 1);
  assert(rc_events == 1);
  assert(ch7 == 2024);
  return 0;
}
