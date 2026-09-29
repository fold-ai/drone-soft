# Perception V1 — Vision Architecture + Dataset Checklist

**Owner:** Perception (detect + box only)  
**Hardware:** Jetson Orin NX  
**Goal:** Small airborne object detector, 60 Hz, frame→box latency < 40 ms  
**Hot path:** C++ / CUDA / TensorRT  
**Out of scope:** PWM, IMU selection, proportional navigation, flight control

---

## 1. Repo layout (`vision/`)

```
vision/
├── README.md
├── CMakeLists.txt                 # Orin NX, CUDA, TensorRT, Argus/V4L2
├── cmake/
│   └── FindTensorRT.cmake
├── configs/
│   ├── capture_imx900.yaml        # exposure 0.2–0.5 ms, FPS, ROI
│   ├── capture_imx676.yaml
│   ├── detector_yolov8n.yaml      # input size, NMS, conf thresh
│   └── trt_build.yaml             # FP16/INT8, workspace, DLA off by default
├── capture/
│   ├── include/capture/
│   │   ├── frame.hpp              # NVMM/CUDA buffer + t_pps + meta
│   │   ├── argus_capture.hpp
│   │   └── v4l2_capture.hpp       # fallback / USB debug cams
│   └── src/
│       ├── argus_capture.cpp
│       ├── v4l2_capture.cpp
│       └── pps_stamp.cpp          # latch PPS ↔ frame mid-exposure
├── detect/
│   ├── include/detect/
│   │   ├── detector.hpp           # Frame in → DetectionBatch out
│   │   ├── trt_engine.hpp
│   │   └── preprocess.hpp         # CUDA letterbox / normalize
│   └── src/
│       ├── detector.cpp
│       ├── trt_engine.cpp
│       ├── preprocess.cu
│       └── postprocess.cu         # NMS on GPU where possible
├── iface/
│   ├── include/iface/
│   │   └── box_message.hpp        # wire format → Tracking
│   └── src/
│       └── box_publisher.cpp      # shared mem / ZMQ / DDS (TBD w/ Tracking)
├── tools/                         # Python only
│   ├── dataset/
│   │   ├── ingest_raw.py
│   │   ├── label_qc.py
│   │   └── split_sets.py
│   ├── train/
│   │   └── launch_train.py        # Ultralytics / custom export → ONNX
│   └── eval/
│       ├── metrics_pr.py
│       └── latency_bench.py
├── models/
│   ├── onnx/                      # export artifacts (git-lfs or artifact store)
│   └── engines/                   # Orin-built .engine (local only, not committed)
├── data/                          # symlinks / manifests, not raw video in git
│   └── manifests/
│       ├── train.txt
│       ├── val.txt
│       └── test.txt
├── logs/
│   └── schema.json                # canonical detection log schema
└── apps/
    ├── capture_bench              # FPS, drop, exposure verify
    ├── detect_node                # production: capture→detect→publish
    └── offline_replay             # bag/video + PPS mock → boxes
```

---

## 2. Capture path: Argus first, V4L2 fallback

### Decision: **libargus (Jetson Multimedia API) for production CSI**

| Criterion | Argus | V4L2 |
|-----------|-------|------|
| CSI + ISP on Orin | Native, NVMM zero-copy → CUDA | Often extra copies / ISP bypass quirks |
| Exposure / GS control | Sensor modes + Argus controls | Driver-dependent; fine for some GS boards |
| Timestamping | Frame metadata + sof/eof hooks | `V4L2_BUF_FLAG_TIMESTAMP_*`; harder to align mid-exposure |
| Debug / USB cams | No | Yes |

**Why Argus for V1**
- IMX900 / IMX676-class on Orin are CSI/MIPI → Argus is the supported zero-copy path into CUDA/TensorRT.
- Short exposure (0.2–0.5 ms) needs reliable sensor-mode + exposure APIs; Argus exposes them without fighting the ISP.
- NVMM → CUDA EGL/CUDA interop keeps the hot path off the CPU and under the 40 ms budget.

**When to use V4L2**
- Lab USB / HDMI grabbers, bring-up before CSI board lands.
- Vendor GS modules that only ship a V4L2 subdev (then pin zero-copy DMABUF → CUDA if available).

**Capture contract (`Frame`)**
- `buffer`: CUDA device pointer (or NVMM mapped)
- `t_pps`: PPS-aligned timestamp of **mid-exposure** (not dequeue time)
- `exposure_us`, `gain`, `w`, `h`, `seq`
- Drop policy: if detect backlog > 1 frame, drop oldest capture (never stall camera thread)

---

## 3. Network family + TensorRT plan

### Family: **tiny single-stage detector, not COCO-giant**
Primary candidate: **YOLOv8n / YOLOv10n / YOLO11n** class (n = nano), trained **from scratch or COCO-pretrained then heavily fine-tuned** on our airborne set — not used as a frozen COCO detector.

Alternates if recall on tiny blobs fails:
- Same nano YOLO **with P2 / high-res head** (stride-4) for distant quads
- **RTMDet-tiny** or **YOLOv8n-p2** export path

**Not for V1 hot path:** YOLOv8x/l, Detectron2, large transformers, multi-frame transformers.

### Input / classes
- Input: start **640×640** letterbox; if p95 latency > ~25 ms headroom needed, try **512** or ROI window around track seed later (Tracking owns track; Perception may accept optional ROI hint in V1.1)
- Classes V1: `quad`, `fixed_wing` (merge to `airborne` only if label budget is tight)
- Conf threshold + NMS tuned on val set for high recall at range (false positives OK if Tracking can gate)

### TensorRT plan
1. Train → export **ONNX** (opset stable for TRT 8.x/10.x on JetPack)
2. `trtexec` / builder API on **target Orin NX**:
   - **FP16** first (must hit <40 ms end-to-end)
   - **INT8** calibration on 500+ in-domain frames if FP16 is tight
3. Explicit batch=1, fixed shapes (no dynamic shape in V1)
4. CUDA preprocess (letterbox + normalize) fused before enqueue; GPU NMS or TRT EfficientNMS plugin
5. DLA: optional experiment only; default **GPU** for latency predictability
6. Warmup N frames at start; measure **frame→box** = mid-exposure stamp → box publish time

**Latency budget (60 Hz = 16.7 ms period; goal <40 ms pipeline latency is multi-frame OK as long as throughput ≥60 Hz and age <40 ms)**
- Capture dequeue + stamp: ~1–2 ms
- Preprocess: ~1–2 ms
- TRT infer: target ≤20–25 ms (nano @ 640 FP16 on Orin NX is typically in this band; verify on hardware)
- Post + publish: ~1–2 ms  
If infer alone >30 ms → drop to 512 or INT8 before changing family.

---

## 4. Dataset plan (≥1000 frames)

### Minimum bar
- **≥1000 labeled frames** with at least one target (quad or small plane)
- Held-out **test** never used for train/val tuning: ≥15% or ≥200 frames
- Val: ~15%; rest train
- Prefer **video-derived** frames with temporal diversity (not 1000 near-duplicates)

### Distance / size bins (label each frame or track segment)
| Bin | Approx target size (px long axis) | Intent |
|-----|-------------------------------------|--------|
| Near | >64 px | easy; sanity |
| Mid | 16–64 px | primary dogfight / chase |
| Far | 8–16 px | stretch; drives P2 head decision |
| Speck | <8 px | optional hard set; may be recall-limited |

Aim: **≥200 frames per Near/Mid/Far**; Speck as stretch.

### Scene / clutter mix
- Sky / clear blue
- Sky + clouds / sun glare
- Ground clutter (trees, buildings, horizon)
- Mixed sky–ground (target near horizon — hardest)
- Motion blur at 0.2–0.5 ms should be minimal; still capture some bank/yaw of *own* platform if gimbal/body motion exists

### Platforms / targets
- Quads: ≥2 airframes / silhouettes if possible
- Small planes / fixed-wing: if available; else synthetic + few real
- Orientations: top, side, oblique; approaching / crossing / fleeing

### Capture protocol (for dataset, not flight)
- Same GS camera class + exposure band as flight (0.2–0.5 ms)
- Log `t_pps` (or GPS time) with every frame even offline
- Raw or lightly compressed; store gain/exposure per frame
- Negative frames: ≥10% empty sky/ground (false-positive control)

### Labeling
- Boxes `xywh` absolute pixels + class + occluded flag
- QC pass: size sanity, no empty boxes, no duplicate IDs on same frame
- Manifest CSVs under `data/manifests/`

### Checklist (gate before “train V1”)
- [ ] ≥1000 positive frames labeled
- [ ] Near / Mid / Far quotas met
- [ ] ≥10% negatives
- [ ] Train / val / test split locked (by sequence, not random frame shuffle)
- [ ] Test held out; no leakage from same flight clip into train+test
- [ ] Exposure / camera metadata present
- [ ] Label QC report generated (`tools/dataset/label_qc.py`)

---

## 5. Metrics

### Detection quality (held-out test)
- **Precision / Recall** at IoU 0.5 (primary)
- Also report IoU 0.3 for speck-sized targets
- Per-class and micro-averaged
- Per distance bin (Near/Mid/Far)
- Optional: F1 at operating conf threshold chosen on val

### Runtime (Orin NX, production `detect_node`)
- **Latency frame→box:** p50, p95, max (ms), using `t_pps` → publish time
- **Throughput:** achieved Hz over 60 s soak
- **Dropped frames:** count + % (capture drops vs detect backlog drops)
- Capture jitter: inter-frame Δt vs 1/60 s

### Gate for V1 “good enough to hand Tracking”
- Throughput ≥ 58 Hz sustained
- Latency p95 < 40 ms
- Dropped frames < 1% in soak
- Recall Mid-bin ≥ agreed floor (set after first val pass; placeholder **≥0.85** at IoU 0.5)
- Precision not below floor that swamps Tracking (placeholder **≥0.5** at that recall point — tune with Tracking)

---

## 6. Log schema + Tracking handshake

### Detection log (JSONL or binary twin; schema in `vision/logs/schema.json`)

```json
{
  "t_pps": 1710000000.123456,
  "seq": 1842,
  "cam_id": 0,
  "src_w": 1920,
  "src_h": 1080,
  "latency_ms": 27.4,
  "dropped_before": 0,
  "detections": [
    {
      "box": {"x": 912.0, "y": 440.0, "w": 24.0, "h": 18.0},
      "conf": 0.87,
      "class": "quad",
      "class_id": 0
    }
  ]
}
```

- `box`: pixel `xywh`, origin top-left of the **full** sensor frame (not letterbox space)
- `t_pps`: **same** PPS time as the source frame (mid-exposure)
- `cam_id`: uint8 modality — EO=0, THERMAL=1 (required; dual-stream association)
- `src_w`, `src_h`: uint16 source frame size (keeps xywh unambiguous if bin/ROI later)
- Empty `detections` array = frame processed, no objects (still publish for sync)
- **MAX_DET = 32** (V1)

### Handshake → **Tracking system** (LOCKED with Tracking ack)
- Every processed frame emits one message: `(t_pps, seq, cam_id, src_w, src_h, detections[])`
- Tracking correlates on **`t_pps` only**; empty list still advances their coast/miss logic via seq + t_pps
- Transport: **lock-free SPSC shared-memory ring on Orin** primary; ZMQ IPC fallback
- Classes V1: `quad` + `fixed_wing` (bird/clutter later, not blocking)
- ROI hints from Tracking: **V1.1** (follow ROI for high-res/tile path) — not required for V1 bring-up
- Perception never sends PWM / guidance; boxes only
- Tracking publishes Track with measurement epoch = t_pps of frames that formed the update

### Wire struct (C++ sketch)

```cpp
struct Box {
  float x, y, w, h;
  float conf;
  uint32_t class_id;
};
static constexpr uint32_t MAX_DET = 32;
struct DetectionMsg {
  double   t_pps;      // seconds, PPS domain, mid-exposure
  uint64_t seq;
  uint8_t  cam_id;     // EO=0, THERMAL=1
  uint16_t src_w;
  uint16_t src_h;
  uint32_t n;
  Box      boxes[MAX_DET];
};
```

---

## V1 deliverable status

| Item | Status |
|------|--------|
| Repo layout | Specified |
| Capture path + rationale | Argus primary / V4L2 fallback |
| Network + TRT plan | Nano YOLO family + FP16→INT8 |
| Dataset plan + checklist | ≥1000 frames, bins, QC gates |
| Metrics | PR + latency p50/p95 + drops |
| Log schema + Tracking handshake | `t_pps` + box + conf + class |

**Contract status:** Tracking ack locked (MAX_DET=32, cam_id + src_w/src_h added). ROI hints deferred to V1.1.

**Next (when you say go):** scaffold `vision/` tree + `schema.json`.
