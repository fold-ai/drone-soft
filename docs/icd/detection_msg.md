# DetectionMsg ICD (Perception → Tracking)

**Status:** LOCKED (Tracking ack)  
**Transport:** lock-free SPSC shared-memory ring on Orin (primary); ZMQ IPC fallback  
**Rate:** one message per processed frame (empty `n=0` still published)

## Wire struct

```cpp
struct Box {
  float x, y, w, h;   // pixel xywh, origin top-left of full sensor frame
  float conf;
  uint32_t class_id;  // 0=shahed_136, 1=geran_2, 2=gerbera (range-test)
};

static constexpr uint32_t MAX_DET = 32;

struct DetectionMsg {
  double   t_pps;     // seconds, PPS domain, mid-exposure of source frame
  uint64_t seq;       // capture sequence
  uint8_t  cam_id;    // Nose EO=0, THERMAL=1, GroundEo=2
  uint16_t src_w;     // source frame width
  uint16_t src_h;     // source frame height
  uint32_t n;         // 0..MAX_DET
  Box      boxes[MAX_DET];
};
```

## Correlation

- Tracking correlates on **`t_pps` only**.
- Empty list still advances coast/miss via `seq` + `t_pps`.
- Perception never sends PWM / guidance — boxes only.
- ROI hints from Tracking: **V1.1** (not required for V1).

## Schema twin

See `interfaces/schemas/detection_msg.json` and `vision/logs/schema.json`.

**V1 bring-up:** Nose EO `cam_id=0`; Ground EO (GCS USB) `cam_id=2`. Thermal deferred V1.1 (`cam_id=1`).

Range-test ClassId: 0=`shahed_136`, 1=`geran_2`, 2=`gerbera` (class-gated Lock).
