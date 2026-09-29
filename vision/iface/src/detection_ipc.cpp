#include "iface/detection_ipc.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace actprove::iface {
namespace {

constexpr std::size_t kHeaderSize = 32;
constexpr std::size_t kBoxSize = 24;
constexpr std::uint16_t kVersion = 1;

void put_u16(std::uint8_t* dst, std::uint16_t value) {
  dst[0] = static_cast<std::uint8_t>(value & 0xffu);
  dst[1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
}

void put_u32(std::uint8_t* dst, std::uint32_t value) {
  for (int i = 0; i < 4; ++i) dst[i] = static_cast<std::uint8_t>(value >> (8 * i));
}

void put_u64(std::uint8_t* dst, std::uint64_t value) {
  for (int i = 0; i < 8; ++i) dst[i] = static_cast<std::uint8_t>(value >> (8 * i));
}

std::uint16_t get_u16(const std::uint8_t* src) {
  return static_cast<std::uint16_t>(src[0]) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(src[1]) << 8);
}

std::uint32_t get_u32(const std::uint8_t* src) {
  std::uint32_t value = 0;
  for (int i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(src[i]) << (8 * i);
  return value;
}

std::uint64_t get_u64(const std::uint8_t* src) {
  std::uint64_t value = 0;
  for (int i = 0; i < 8; ++i) value |= static_cast<std::uint64_t>(src[i]) << (8 * i);
  return value;
}

void put_float(std::uint8_t* dst, float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  put_u32(dst, bits);
}

float get_float(const std::uint8_t* src) {
  const std::uint32_t bits = get_u32(src);
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

void put_double(std::uint8_t* dst, double value) {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  put_u64(dst, bits);
}

double get_double(const std::uint8_t* src) {
  const std::uint64_t bits = get_u64(src);
  double value = 0.0;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

bool fill_address(const std::string& path, sockaddr_un& address) {
  if (path.empty() || path.size() >= sizeof(address.sun_path)) return false;
  address = {};
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
#ifdef __APPLE__
  address.sun_len = static_cast<std::uint8_t>(
      offsetof(sockaddr_un, sun_path) + path.size() + 1);
#endif
  return true;
}

socklen_t address_size(const std::string& path) {
  return static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path.size() + 1);
}

}  // namespace

std::vector<std::uint8_t> encode_detection_v1(const DetectionMsg& msg) {
  const std::uint32_t count = std::min(msg.n, MAX_DET);
  std::vector<std::uint8_t> wire(kHeaderSize + count * kBoxSize, 0);
  wire[0] = 'A';
  wire[1] = 'P';
  wire[2] = 'D';
  wire[3] = '1';
  put_u16(wire.data() + 4, kVersion);
  put_u16(wire.data() + 6, static_cast<std::uint16_t>(count));
  put_double(wire.data() + 8, msg.t_pps);
  put_u64(wire.data() + 16, msg.seq);
  wire[24] = msg.cam_id;
  put_u16(wire.data() + 26, msg.src_w);
  put_u16(wire.data() + 28, msg.src_h);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::uint8_t* box = wire.data() + kHeaderSize + i * kBoxSize;
    put_float(box + 0, msg.boxes[i].x);
    put_float(box + 4, msg.boxes[i].y);
    put_float(box + 8, msg.boxes[i].w);
    put_float(box + 12, msg.boxes[i].h);
    put_float(box + 16, msg.boxes[i].conf);
    put_u32(box + 20, msg.boxes[i].class_id);
  }
  return wire;
}

bool decode_detection_v1(const std::uint8_t* data, std::size_t size,
                         DetectionMsg& out) {
  if (!data || size < kHeaderSize) return false;
  if (data[0] != 'A' || data[1] != 'P' || data[2] != 'D' || data[3] != '1') return false;
  if (get_u16(data + 4) != kVersion) return false;
  const std::uint16_t count = get_u16(data + 6);
  if (count > MAX_DET || size != kHeaderSize + count * kBoxSize) return false;

  DetectionMsg decoded{};
  decoded.t_pps = get_double(data + 8);
  decoded.seq = get_u64(data + 16);
  decoded.cam_id = data[24];
  decoded.src_w = get_u16(data + 26);
  decoded.src_h = get_u16(data + 28);
  decoded.n = count;
  if (!std::isfinite(decoded.t_pps) || decoded.t_pps < 0.0 ||
      decoded.src_w == 0 || decoded.src_h == 0) {
    return false;
  }
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::uint8_t* box = data + kHeaderSize + i * kBoxSize;
    decoded.boxes[i].x = get_float(box + 0);
    decoded.boxes[i].y = get_float(box + 4);
    decoded.boxes[i].w = get_float(box + 8);
    decoded.boxes[i].h = get_float(box + 12);
    decoded.boxes[i].conf = get_float(box + 16);
    decoded.boxes[i].class_id = get_u32(box + 20);
    if (!std::isfinite(decoded.boxes[i].x) || !std::isfinite(decoded.boxes[i].y) ||
        !std::isfinite(decoded.boxes[i].w) || !std::isfinite(decoded.boxes[i].h) ||
        !std::isfinite(decoded.boxes[i].conf) || decoded.boxes[i].x < 0.0f ||
        decoded.boxes[i].y < 0.0f || decoded.boxes[i].w < 0.0f ||
        decoded.boxes[i].h < 0.0f || decoded.boxes[i].conf < 0.0f ||
        decoded.boxes[i].conf > 1.0f ||
        decoded.boxes[i].x + decoded.boxes[i].w > decoded.src_w + 1.0f ||
        decoded.boxes[i].y + decoded.boxes[i].h > decoded.src_h + 1.0f) {
      return false;
    }
  }
  out = decoded;
  return true;
}

UnixDetectionPublisher::UnixDetectionPublisher(std::string socket_path)
    : socket_path_(std::move(socket_path)) {}

UnixDetectionPublisher::~UnixDetectionPublisher() { close(); }

bool UnixDetectionPublisher::open() {
  close();
  sockaddr_un address{};
  if (!fill_address(socket_path_, address)) return false;
  fd_ = ::socket(AF_UNIX, SOCK_DGRAM, 0);
  if (fd_ < 0) {
    std::perror("detection_ipc publisher socket");
    return false;
  }
  const int flags = ::fcntl(fd_, F_GETFL, 0);
  if (flags < 0 || ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) != 0) {
    close();
    return false;
  }
  return true;
}

void UnixDetectionPublisher::close() {
  if (fd_ >= 0) ::close(fd_);
  fd_ = -1;
}

bool UnixDetectionPublisher::publish(const DetectionMsg& msg) {
  if (fd_ < 0) return false;
  sockaddr_un address{};
  if (!fill_address(socket_path_, address)) return false;
  const auto wire = encode_detection_v1(msg);
  const ssize_t sent = ::sendto(fd_, wire.data(), wire.size(), MSG_DONTWAIT,
                                reinterpret_cast<const sockaddr*>(&address),
                                address_size(socket_path_));
  if (sent != static_cast<ssize_t>(wire.size())) {
    ++dropped_;
    return false;
  }
  ++sent_;
  return true;
}

UnixDetectionSubscriber::UnixDetectionSubscriber(std::string socket_path)
    : socket_path_(std::move(socket_path)) {}

UnixDetectionSubscriber::~UnixDetectionSubscriber() { close(); }

bool UnixDetectionSubscriber::open() {
  close();
  sockaddr_un address{};
  if (!fill_address(socket_path_, address)) return false;
  fd_ = ::socket(AF_UNIX, SOCK_DGRAM, 0);
  if (fd_ < 0) {
    std::perror("detection_ipc subscriber socket");
    return false;
  }
  (void)::unlink(socket_path_.c_str());
  if (::bind(fd_, reinterpret_cast<const sockaddr*>(&address),
             address_size(socket_path_)) != 0) {
    std::perror("detection_ipc bind");
    close();
    return false;
  }
  (void)::chmod(socket_path_.c_str(), 0660);
  return true;
}

void UnixDetectionSubscriber::close() {
  if (fd_ >= 0) ::close(fd_);
  fd_ = -1;
  if (!socket_path_.empty()) (void)::unlink(socket_path_.c_str());
}

bool UnixDetectionSubscriber::receive(DetectionMsg& out, int timeout_ms) {
  if (fd_ < 0) return false;
  pollfd descriptor{fd_, POLLIN, 0};
  const int ready = ::poll(&descriptor, 1, std::max(0, timeout_ms));
  if (ready <= 0 || !(descriptor.revents & POLLIN)) return false;
  std::array<std::uint8_t, kHeaderSize + MAX_DET * kBoxSize> buffer{};
  const ssize_t count = ::recv(fd_, buffer.data(), buffer.size(), 0);
  return count > 0 && decode_detection_v1(buffer.data(), static_cast<std::size_t>(count), out);
}

}  // namespace actprove::iface
