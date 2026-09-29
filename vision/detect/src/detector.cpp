#include "detect/detector.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace actprove::detect {
namespace {

uint8_t clamp_u8(int value) {
  return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

void read_rgb(const capture::Frame& frame, int x, int y,
              uint8_t& r, uint8_t& g, uint8_t& b) {
  const auto* src = static_cast<const uint8_t*>(frame.device_ptr);
  const size_t default_bpp = frame.pixel_format == capture::PixelFormat::YUYV ? 2u :
                             frame.pixel_format == capture::PixelFormat::Gray8 ? 1u : 3u;
  const size_t stride = frame.stride_bytes ? frame.stride_bytes :
      static_cast<size_t>(frame.w) * default_bpp;
  const uint8_t* row = src + static_cast<size_t>(y) * stride;
  switch (frame.pixel_format) {
    case capture::PixelFormat::RGB8: {
      const uint8_t* p = row + static_cast<size_t>(x) * 3;
      r = p[0]; g = p[1]; b = p[2];
      break;
    }
    case capture::PixelFormat::BGR8: {
      const uint8_t* p = row + static_cast<size_t>(x) * 3;
      b = p[0]; g = p[1]; r = p[2];
      break;
    }
    case capture::PixelFormat::Gray8:
      r = g = b = row[x];
      break;
    case capture::PixelFormat::YUYV: {
      const size_t pair = static_cast<size_t>(x / 2) * 4;
      const int yy = row[pair + (x % 2 ? 2 : 0)];
      const int u = row[pair + 1] - 128;
      const int v = row[pair + 3] - 128;
      const int c = std::max(0, yy - 16);
      r = clamp_u8((298 * c + 409 * v + 128) >> 8);
      g = clamp_u8((298 * c - 100 * u - 208 * v + 128) >> 8);
      b = clamp_u8((298 * c + 516 * u + 128) >> 8);
      break;
    }
    default:
      r = g = b = 0;
  }
}

bool preprocess(const capture::Frame& frame, int dst_w, int dst_h,
                std::vector<float>& tensor, float& scale,
                float& pad_x, float& pad_y) {
  if (frame.memory_kind != capture::MemoryKind::Host || !frame.device_ptr ||
      frame.w == 0 || frame.h == 0 ||
      frame.pixel_format == capture::PixelFormat::MJPEG ||
      frame.pixel_format == capture::PixelFormat::Unknown) return false;
  scale = std::min(static_cast<float>(dst_w) / frame.w,
                   static_cast<float>(dst_h) / frame.h);
  const int scaled_w = std::max(1, static_cast<int>(std::round(frame.w * scale)));
  const int scaled_h = std::max(1, static_cast<int>(std::round(frame.h * scale)));
  pad_x = 0.5f * (dst_w - scaled_w);
  pad_y = 0.5f * (dst_h - scaled_h);
  const size_t plane = static_cast<size_t>(dst_w) * dst_h;
  tensor.assign(3 * plane, 114.0f / 255.0f);
  for (int dy = 0; dy < scaled_h; ++dy) {
    const int sy = std::min<int>(frame.h - 1, static_cast<int>(dy / scale));
    for (int dx = 0; dx < scaled_w; ++dx) {
      const int sx = std::min<int>(frame.w - 1, static_cast<int>(dx / scale));
      uint8_t r, g, b;
      read_rgb(frame, sx, sy, r, g, b);
      const int ox = static_cast<int>(pad_x) + dx;
      const int oy = static_cast<int>(pad_y) + dy;
      if (ox < 0 || ox >= dst_w || oy < 0 || oy >= dst_h) continue;
      const size_t p = static_cast<size_t>(oy) * dst_w + ox;
      tensor[p] = r / 255.0f;
      tensor[plane + p] = g / 255.0f;
      tensor[2 * plane + p] = b / 255.0f;
    }
  }
  return true;
}

float iou(const Box& a, const Box& b) {
  const float x1 = std::max(a.x, b.x);
  const float y1 = std::max(a.y, b.y);
  const float x2 = std::min(a.x + a.w, b.x + b.w);
  const float y2 = std::min(a.y + a.h, b.y + b.h);
  const float intersection = std::max(0.0f, x2 - x1) * std::max(0.0f, y2 - y1);
  const float total = a.w * a.h + b.w * b.h - intersection;
  return total > 0.0f ? intersection / total : 0.0f;
}

}  // namespace

Detector::Detector(std::string engine_path, bool simulate)
    : engine_path_(std::move(engine_path)), simulate_(simulate) {}

bool Detector::load() {
  if (simulate_) {
    ready_ = true;
    return true;
  }
  engine_ = std::make_unique<TrtEngine>(engine_path_);
  ready_ = engine_->build_or_load();
  return ready_;
}

bool Detector::infer(const capture::Frame& in, DetectionBatch& out) {
  if (!ready_) return false;
  out = {};
  out.t_pps = in.t_pps;
  out.seq = in.seq;
  out.cam_id = in.cam_id;
  out.src_w = in.w;
  out.src_h = in.h;
  if (simulate_) return true;

  const auto& input_shape = engine_->input_shape();
  if (input_shape.size() != 4 || input_shape[1] != 3) return false;
  const int input_h = static_cast<int>(input_shape[2]);
  const int input_w = static_cast<int>(input_shape[3]);
  std::vector<float> tensor;
  float scale = 1.0f, pad_x = 0.0f, pad_y = 0.0f;
  if (!preprocess(in, input_w, input_h, tensor, scale, pad_x, pad_y)) return false;
  std::vector<float> raw;
  if (!engine_->infer(tensor, raw)) return false;

  const auto& shape = engine_->output_shape();
  if (shape.size() != 3 || shape[0] != 1) return false;
  const bool channel_first = shape[1] < shape[2];
  const int attrs = static_cast<int>(channel_first ? shape[1] : shape[2]);
  const int predictions = static_cast<int>(channel_first ? shape[2] : shape[1]);
  if (attrs < 5 || predictions <= 0 ||
      raw.size() < static_cast<size_t>(attrs) * predictions) return false;
  auto value = [&](int prediction, int attr) {
    return channel_first
        ? raw[static_cast<size_t>(attr) * predictions + prediction]
        : raw[static_cast<size_t>(prediction) * attrs + attr];
  };

  std::vector<Box> candidates;
  candidates.reserve(static_cast<size_t>(predictions));
  for (int p = 0; p < predictions; ++p) {
    int class_id = 0;
    float confidence = value(p, 4);
    for (int c = 5; c < attrs; ++c) {
      if (value(p, c) > confidence) {
        confidence = value(p, c);
        class_id = c - 4;
      }
    }
    if (confidence < conf_thresh_) continue;
    const float cx = value(p, 0);
    const float cy = value(p, 1);
    const float width = value(p, 2);
    const float height = value(p, 3);
    float x = (cx - 0.5f * width - pad_x) / scale;
    float y = (cy - 0.5f * height - pad_y) / scale;
    float w = width / scale;
    float h = height / scale;
    x = std::clamp(x, 0.0f, static_cast<float>(in.w));
    y = std::clamp(y, 0.0f, static_cast<float>(in.h));
    w = std::clamp(w, 0.0f, static_cast<float>(in.w) - x);
    h = std::clamp(h, 0.0f, static_cast<float>(in.h) - y);
    if (w < 1.0f || h < 1.0f) continue;
    // DetectionMsg ICD is full-sensor pixel xywh, not normalized 0–1.
    // Normalized boxes fail Tracking lock geometry (min area / min side).
    candidates.push_back({x, y, w, h, confidence, static_cast<uint32_t>(class_id)});
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const Box& a, const Box& b) { return a.conf > b.conf; });
  std::vector<Box> kept;
  kept.reserve(MAX_DET);
  for (const Box& candidate : candidates) {
    bool suppressed = false;
    for (const Box& prior : kept) {
      if (candidate.class_id == prior.class_id && iou(candidate, prior) > nms_iou_) {
        suppressed = true;
        break;
      }
    }
    if (!suppressed) kept.push_back(candidate);
    if (kept.size() == MAX_DET) break;
  }
  out.n = static_cast<uint32_t>(kept.size());
  std::copy(kept.begin(), kept.end(), out.boxes);
  return true;
}

}  // namespace actprove::detect
