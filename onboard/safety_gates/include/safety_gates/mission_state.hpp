#pragma once
#include <cstdint>
#include <string_view>

namespace actprove::safety {

enum class MissionState : uint8_t {
  BOOT = 0,
  SEARCH = 1,
  LOCK = 2,
  CLOSE = 3,
  ABORT = 4,
  RTB = 5,
  FTS = 6,
};

inline constexpr std::string_view to_string(MissionState s) {
  switch (s) {
    case MissionState::BOOT: return "BOOT";
    case MissionState::SEARCH: return "SEARCH";
    case MissionState::LOCK: return "LOCK";
    case MissionState::CLOSE: return "CLOSE";
    case MissionState::ABORT: return "ABORT";
    case MissionState::RTB: return "RTB";
    case MissionState::FTS: return "FTS";
  }
  return "UNKNOWN";
}

enum class Cmd : uint8_t {
  NONE = 0,
  WORK = 1,
  ABORT = 2,
  RTB = 3,
  FTS = 4,
};

}  // namespace actprove::safety
