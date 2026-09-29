#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>

namespace actprove::capture {

/// Source role for dual EO paths (nose vs ground). Log / Frame meta only in V1.
/// Not part of locked DetectionMsg wire — cam_id stays EO=0 for both.
enum class SourceRole : uint8_t {
  NoseEo = 0,
  GroundEo = 1,
};

enum class PixelFormat : uint8_t {
  Unknown = 0,
  Gray8,
  RGB8,
  BGR8,
  YUYV,
  MJPEG,
};

enum class MemoryKind : uint8_t {
  None = 0,
  Host,
  CudaDevice,
  Nvmm,
};

enum class CameraRole : uint8_t {
  Generic = 0,
  Search = 1,
  Tele = 2,
};

/// NVMM/CUDA buffer + PPS mid-exposure meta.
struct Frame {
  void* device_ptr{nullptr};   // CUDA device pointer (or NVMM mapped)
  size_t bytes{0};
  size_t stride_bytes{0};
  PixelFormat pixel_format{PixelFormat::Unknown};
  MemoryKind memory_kind{MemoryKind::None};
  CameraRole camera_role{CameraRole::Generic};
  // Keeps copied V4L2 frames alive after the driver buffer is re-queued.
  std::shared_ptr<std::vector<uint8_t>> host_owner;
  double t_pps{0.0};           // mid-exposure, PPS domain
  float exposure_us{0.0f};
  float gain{0.0f};
  uint16_t w{0};
  uint16_t h{0};
  uint64_t seq{0};
  uint8_t cam_id{0};           // EO=0 THERMAL=1
  uint8_t source_role{0};      // SourceRole: nose_eo=0, ground_eo=1 (meta/log only)
};

}  // namespace actprove::capture
