#include "capture/v4l2_capture.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <unistd.h>

#ifdef __linux__
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#endif

namespace actprove::capture {

extern double latch_mid_exposure_pps(double sof_pps, float exposure_us);

namespace {
#ifdef __linux__
bool xioctl(int fd, unsigned long request, void* arg) {
  int rc;
  do {
    rc = ::ioctl(fd, request, arg);
  } while (rc < 0 && errno == EINTR);
  return rc >= 0;
}

uint32_t requested_fourcc(const std::string& name) {
  if (name == "MJPG" || name == "MJPEG") return V4L2_PIX_FMT_MJPEG;
  if (name == "RGB3" || name == "RGB8") return V4L2_PIX_FMT_RGB24;
  if (name == "BGR3" || name == "BGR8") return V4L2_PIX_FMT_BGR24;
  if (name == "GREY" || name == "GRAY8") return V4L2_PIX_FMT_GREY;
  return V4L2_PIX_FMT_YUYV;
}

PixelFormat frame_format(uint32_t fourcc) {
  switch (fourcc) {
    case V4L2_PIX_FMT_YUYV: return PixelFormat::YUYV;
    case V4L2_PIX_FMT_MJPEG: return PixelFormat::MJPEG;
    case V4L2_PIX_FMT_RGB24: return PixelFormat::RGB8;
    case V4L2_PIX_FMT_BGR24: return PixelFormat::BGR8;
    case V4L2_PIX_FMT_GREY: return PixelFormat::Gray8;
    default: return PixelFormat::Unknown;
  }
}
#endif
}  // namespace

V4L2Capture::V4L2Capture(V4L2Config cfg) : cfg_(std::move(cfg)) {}
V4L2Capture::~V4L2Capture() { close(); }

bool V4L2Capture::open() {
  if (cfg_.simulate) return true;
#ifndef __linux__
  // V4L2 is Linux-only. macOS continues to use the Python/OpenCV demo path.
  return false;
#else
  fd_ = ::open(cfg_.device.c_str(), O_RDWR | O_NONBLOCK);
  if (fd_ < 0) return false;

  v4l2_capability caps{};
  if (!xioctl(fd_, VIDIOC_QUERYCAP, &caps)) {
    close();
    return false;
  }
  const uint32_t effective_caps = (caps.capabilities & V4L2_CAP_DEVICE_CAPS)
      ? caps.device_caps : caps.capabilities;
  if (!(effective_caps & V4L2_CAP_VIDEO_CAPTURE) ||
      !(effective_caps & V4L2_CAP_STREAMING)) {
    close();
    return false;
  }

  v4l2_format fmt{};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = cfg_.width;
  fmt.fmt.pix.height = cfg_.height;
  fmt.fmt.pix.pixelformat = requested_fourcc(cfg_.pixel_format);
  fmt.fmt.pix.field = V4L2_FIELD_ANY;
  if (!xioctl(fd_, VIDIOC_S_FMT, &fmt)) {
    close();
    return false;
  }
  negotiated_format_ = frame_format(fmt.fmt.pix.pixelformat);
  if (negotiated_format_ == PixelFormat::Unknown) {
    close();
    return false;
  }
  cfg_.width = static_cast<uint16_t>(fmt.fmt.pix.width);
  cfg_.height = static_cast<uint16_t>(fmt.fmt.pix.height);
  negotiated_stride_ = fmt.fmt.pix.bytesperline;

  v4l2_streamparm parm{};
  parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  parm.parm.capture.timeperframe.numerator = 1;
  parm.parm.capture.timeperframe.denominator = std::max(1, cfg_.fps);
  (void)xioctl(fd_, VIDIOC_S_PARM, &parm);  // Some UVC devices ignore FPS requests.

  v4l2_requestbuffers req{};
  req.count = static_cast<uint32_t>(std::clamp(cfg_.buffer_count, 2, 8));
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (!xioctl(fd_, VIDIOC_REQBUFS, &req) || req.count < 2) {
    close();
    return false;
  }

  buffers_.resize(req.count);
  for (uint32_t i = 0; i < req.count; ++i) {
    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    if (!xioctl(fd_, VIDIOC_QUERYBUF, &buf)) {
      close();
      return false;
    }
    void* mapped = ::mmap(nullptr, buf.length, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd_, static_cast<off_t>(buf.m.offset));
    if (mapped == MAP_FAILED) {
      close();
      return false;
    }
    buffers_[i] = {mapped, buf.length};
    if (!xioctl(fd_, VIDIOC_QBUF, &buf)) {
      close();
      return false;
    }
  }

  v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (!xioctl(fd_, VIDIOC_STREAMON, &type)) {
    close();
    return false;
  }
  return true;
#endif
}

void V4L2Capture::close() {
#ifdef __linux__
  if (fd_ >= 0 && !buffers_.empty()) {
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    (void)xioctl(fd_, VIDIOC_STREAMOFF, &type);
  }
  for (auto& buffer : buffers_) {
    if (buffer.start && buffer.start != MAP_FAILED) {
      ::munmap(buffer.start, buffer.length);
    }
  }
#endif
  buffers_.clear();
  if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

bool V4L2Capture::grab(Frame& out) {
  using clock = std::chrono::steady_clock;
  if (!cfg_.simulate) {
#ifndef __linux__
    return false;
#else
    if (fd_ < 0) return false;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    timeval timeout{2, 0};
    const int ready = ::select(fd_ + 1, &fds, nullptr, nullptr, &timeout);
    if (ready <= 0) return false;

    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (!xioctl(fd_, VIDIOC_DQBUF, &buf)) return false;
    if (buf.index >= buffers_.size()) return false;

    const size_t used = std::min<size_t>(buf.bytesused, buffers_[buf.index].length);
    auto owner = std::make_shared<std::vector<uint8_t>>(used);
    std::memcpy(owner->data(), buffers_[buf.index].start, used);
    const timeval ts = buf.timestamp;
    const double sof = ts.tv_sec > 0
        ? static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_usec) * 1e-6
        : std::chrono::duration<double>(clock::now().time_since_epoch()).count();

    const bool requeued = xioctl(fd_, VIDIOC_QBUF, &buf);
    if (!requeued) return false;

    out.host_owner = std::move(owner);
    out.device_ptr = out.host_owner->data();
    out.bytes = used;
    out.stride_bytes = negotiated_stride_;
    out.pixel_format = negotiated_format_;
    out.memory_kind = MemoryKind::Host;
    out.exposure_us = 0.0f;  // Filled when V4L2 exposure controls are queried later.
    out.gain = 0.0f;
    out.w = cfg_.width;
    out.h = cfg_.height;
    out.seq = ++seq_;
    out.cam_id = cfg_.cam_id;
    out.source_role = static_cast<uint8_t>(SourceRole::NoseEo);
    out.t_pps = latch_mid_exposure_pps(sof, out.exposure_us);
    return true;
#endif
  }

  const double sof = std::chrono::duration<double>(clock::now().time_since_epoch()).count();
  out.host_owner.reset();
  out.device_ptr = nullptr;
  out.bytes = 0;
  out.stride_bytes = 0;
  out.pixel_format = PixelFormat::Unknown;
  out.memory_kind = MemoryKind::None;
  out.exposure_us = 300.0f;
  out.gain = 1.0f;
  out.w = cfg_.width;
  out.h = cfg_.height;
  out.seq = ++seq_;
  out.cam_id = cfg_.cam_id;
  out.source_role = static_cast<uint8_t>(SourceRole::NoseEo);  // default; GroundEoCapture overrides
  out.t_pps = latch_mid_exposure_pps(sof, out.exposure_us);
  return true;
}

}  // namespace actprove::capture
