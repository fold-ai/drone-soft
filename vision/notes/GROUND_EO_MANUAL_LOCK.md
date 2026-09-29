# Ground EO Manual Lock (GCS laptop V4L2)

## Role
Alternate video source for the **GCS laptop**: USB/V4L2 camera the operator aims.
**Not** Orin nose GMSL. Same box designate contract as nose EO → `DetectionMsg`.

## Config
- Primary: `configs/capture_ground_eo_usb.yaml`
  - `device: /dev/video0`
  - `fps: 30` (default)
  - **`cam_id: 2`** (`CamId::GroundEo` on DetectionMsg wire)
- Alias: `configs/capture_ground_eo_v4l2.yaml` (same values; prefer `_usb`)

## Capture / node
- `capture/include/capture/ground_eo_capture.hpp` + `src/ground_eo_capture.cpp`
- App stub: `apps/ground_eo_node` — `capture → detect` OR `--manual` designate → publish
- CMake target: `ground_eo_node`

## Wire vs meta
| Field | Where | Value |
|-------|-------|-------|
| `cam_id` | **DetectionMsg wire** | **2** (GroundEo) |
| `source_role` | Frame meta / log (optional) | `ground_eo` |

Do **not** publish Ground EO as `cam_id=0`. Nose EO remains `0`.

## GCS UI
Source toggle: **Nose EO** | **Ground EO** — see `docs/notes/GROUND_EO_CUE_AND_BDA.md`.
Manual Lock: click/drag box or confirm detector proposal on the selected feed.

## Class gate
Lock only if `class_id ∈ {shahed_136, geran_2, gerbera}` (or operator-forced among those three).
