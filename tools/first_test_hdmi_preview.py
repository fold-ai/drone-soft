#!/usr/bin/env python3
"""First-test HDMI HUD for JAI GOX-5103 + Kowa 50mm (or a UVC webcam).

Shows what Orin sees: candidate boxes and a LOCK hold. This is the bench
monitor path. DJI O4 / 3DR 433 cannot carry this overlay.

  DISPLAY=:0 python3 tools/first_test_hdmi_preview.py --target balloon
  python3 tools/first_test_hdmi_preview.py --webcam 0 --target drone

JAI is USB3 Vision (Aravis), not /dev/video0.
"""
from __future__ import annotations

import argparse
import math
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Sequence

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))

try:
    import cv2
    import numpy as np
except ImportError as exc:  # pragma: no cover
    raise SystemExit("install python3-opencv / numpy") from exc

from detection_ipc import Box, DetectionMsg, UnixDetectionPublisher  # noqa: E402


@dataclass
class Grab:
    gray: "np.ndarray"
    bgr: "np.ndarray"


class FrameSource:
    def read(self) -> Optional[Grab]:
        raise NotImplementedError

    def close(self) -> None:
        return


class WebcamSource(FrameSource):
    def __init__(self, index: int, width: int, height: int) -> None:
        self.cap = cv2.VideoCapture(index)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        if not self.cap.isOpened():
            raise SystemExit(f"cannot open webcam {index}")

    def read(self) -> Optional[Grab]:
        ok, frame = self.cap.read()
        if not ok or frame is None:
            return None
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        return Grab(gray=gray, bgr=frame)

    def close(self) -> None:
        self.cap.release()


class AravisSource(FrameSource):
    def __init__(self) -> None:
        try:
            gi = __import__("gi")
            gi.require_version("Aravis", "0.8")
            from gi.repository import Aravis
        except Exception as exc:  # pragma: no cover
            raise SystemExit(
                "JAI USB3 Vision needs Aravis: sudo apt-get install python3-gi gir1.2-aravis-0.8"
            ) from exc
        Aravis.update_device_list()
        if Aravis.get_n_devices() == 0:
            raise SystemExit("no USB3 Vision camera found (is the SuperSpeed cable plugged into USB3?)")
        self.Aravis = Aravis
        self.cam = Aravis.Camera.new(None)
        try:
            self.cam.set_pixel_format_from_string("Mono8")
        except Exception:
            pass
        self.stream = self.cam.create_stream(None, None)
        payload = self.cam.get_payload()
        for _ in range(8):
            self.stream.push_buffer(Aravis.Buffer.new_allocate(payload))
        self.cam.start_acquisition()
        print(f"aravis camera={self.cam.get_model_name()} payload={payload}", flush=True)

    def read(self) -> Optional[Grab]:
        buf = self.stream.timeout_pop_buffer(200000)
        if buf is None:
            return None
        try:
            data = buf.get_data()
            w = buf.get_image_width()
            h = buf.get_image_height()
            gray = np.frombuffer(data, dtype=np.uint8, count=w * h).reshape((h, w))
            bgr = cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR)
            return Grab(gray=gray, bgr=bgr)
        finally:
            self.stream.push_buffer(buf)

    def close(self) -> None:
        try:
            self.cam.stop_acquisition()
        except Exception:
            pass


def range_from_width(box_w_px: float, frame_w: int, known_w: float, hfov_deg: float) -> float:
    if box_w_px <= 1 or frame_w <= 1 or known_w <= 0:
        return float("nan")
    focal = (frame_w / 2.0) / math.tan(math.radians(hfov_deg) / 2.0)
    return (known_w * focal) / box_w_px


def _boxes_from_mask(mask: "np.ndarray", class_id: int, min_side: int, roundish: bool) -> List[Box]:
    h, w = mask.shape[:2]
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    boxes: List[Box] = []
    for contour in contours:
        x, y, bw, bh = cv2.boundingRect(contour)
        if bw < min_side or bh < min_side:
            continue
        if y + bh > int(h * 0.88):
            continue
        area = float(bw * bh)
        if area < 80 or area > 0.35 * w * h:
            continue
        ext = cv2.contourArea(contour) / area
        if ext < 0.25:
            continue
        if roundish:
            ratio = bw / float(bh)
            if ratio < 0.45 or ratio > 2.2:
                continue
        boxes.append(
            Box(
                x=float(x),
                y=float(y),
                w=float(bw),
                h=float(bh),
                conf=min(0.99, 0.35 + 0.5 * ext),
                class_id=class_id,
            )
        )
    return boxes


def find_airborne_blobs(gray: "np.ndarray", target: str) -> List[Box]:
    """One camera, sky only. class 0 = bright balloon, class 1 = dark UAV."""
    h, w = gray.shape[:2]
    blur = cv2.GaussianBlur(gray, (7, 7), 0)
    boxes: List[Box] = []
    want_balloon = target in ("balloon", "sky")
    want_drone = target in ("drone", "sky")
    if want_balloon:
        if float(np.mean(gray)) > 140:
            bright = cv2.threshold(blur, max(180, int(np.percentile(gray, 92))), 255, cv2.THRESH_BINARY)[1]
        else:
            bright = cv2.threshold(blur, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)[1]
        bright = cv2.morphologyEx(bright, cv2.MORPH_OPEN, np.ones((5, 5), np.uint8))
        boxes.extend(_boxes_from_mask(bright, 0, 14, True))
        # A white flying wing is bright and wide. Round balloons stay class 0.
        for box in _boxes_from_mask(bright, 1, 8, False):
            ratio = box.w / max(1.0, box.h)
            if 2.2 < ratio <= 8.0:
                boxes.append(box)
    if want_drone:
        sky = float(np.median(gray[: max(8, h // 3), :]))
        thresh = max(20, min(120, int(sky * 0.55)))
        dark = cv2.threshold(blur, thresh, 255, cv2.THRESH_BINARY_INV)[1]
        dark[int(h * 0.72) :, :] = 0
        dark = cv2.morphologyEx(dark, cv2.MORPH_OPEN, np.ones((5, 5), np.uint8))
        boxes.extend(_boxes_from_mask(dark, 1, 8, False))
    boxes.sort(key=lambda b: b.conf, reverse=True)
    return boxes[:8]


def yolo_boxes(model, bgr: "np.ndarray") -> List[Box]:
    results = model.predict(bgr, verbose=False, conf=0.25, imgsz=640)
    out: List[Box] = []
    if not results:
        return out
    xyxy = results[0].boxes.xyxy.cpu().numpy()
    conf = results[0].boxes.conf.cpu().numpy()
    cls = results[0].boxes.cls.cpu().numpy()
    for i in range(len(xyxy)):
        x1, y1, x2, y2 = xyxy[i]
        out.append(
            Box(
                x=float(x1),
                y=float(y1),
                w=float(x2 - x1),
                h=float(y2 - y1),
                conf=float(conf[i]),
                class_id=int(cls[i]),
            )
        )
    return out


def pick_lock(boxes: Sequence[Box], frame_w: int, frame_h: int) -> Optional[Box]:
    if not boxes:
        return None
    cx, cy = frame_w / 2.0, frame_h / 2.0
    best = None
    best_d = 1e18
    for box in boxes:
        if box.conf < 0.28:
            continue
        u = box.x + 0.5 * box.w
        v = box.y + 0.5 * box.h
        if v > 0.88 * frame_h:
            continue
        d = (u - cx) ** 2 + (v - cy) ** 2
        if d < best_d:
            best_d = d
            best = box
    return best


def draw_lock_square(vis: "np.ndarray", box: Box, color: tuple, thick: int) -> None:
    cx = int(box.x + 0.5 * box.w)
    cy = int(box.y + 0.5 * box.h)
    side = int(max(box.w, box.h, 28))
    half = side // 2
    x1, y1, x2, y2 = cx - half, cy - half, cx + half, cy + half
    arm = max(10, side // 5)
    for px, py, sx, sy in (
        (x1, y1, 1, 1),
        (x2, y1, -1, 1),
        (x1, y2, 1, -1),
        (x2, y2, -1, -1),
    ):
        cv2.line(vis, (px, py), (px + sx * arm, py), color, thick)
        cv2.line(vis, (px, py), (px, py + sy * arm), color, thick)
    cv2.drawMarker(vis, (cx, cy), color, cv2.MARKER_CROSS, max(16, side // 3), 1)


def draw_hud(
    bgr: "np.ndarray",
    boxes: List[Box],
    lock: Optional[Box],
    *,
    target: str,
    range_m: float,
    width_m: float,
    hits: int,
    source: str,
    fps: float,
) -> "np.ndarray":
    vis = bgr
    h, w = vis.shape[:2]
    cv2.drawMarker(vis, (w // 2, h // 2), (80, 80, 80), cv2.MARKER_CROSS, 24, 1)
    for box in boxes:
        p1 = (int(box.x), int(box.y))
        p2 = (int(box.x + box.w), int(box.y + box.h))
        cv2.rectangle(vis, p1, p2, (0, 180, 0), 1)
    locked = hits >= 2 and lock is not None
    if lock is not None:
        color = (0, 0, 255) if locked else (0, 165, 255)
        draw_lock_square(vis, lock, color, 3 if locked else 2)
        kind = "BALLOON" if lock.class_id == 0 else "WING"
        label = ("LOCK " if locked else "HOLD ") + kind
        cv2.putText(
            vis,
            label,
            (int(lock.x), max(24, int(lock.y) - 8)),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.9,
            color,
            2,
        )
    range_txt = "--" if not math.isfinite(range_m) else f"{range_m:.0f}m"
    size_txt = "--" if width_m <= 0 else f"{width_m:.2f}m"
    lines = [
        f"ACTPROVE first test  src={source}  target={target}",
        f"fps={fps:.1f}  hits={hits}  size={size_txt}  range~{range_txt}",
        "Square is the lock. Center crosshair is the nose. O4 is pilot only.",
    ]
    y = 28
    for line in lines:
        cv2.putText(vis, line, (16, y), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 0), 4)
        cv2.putText(vis, line, (16, y), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 1)
        y += 28
    return vis


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--webcam", type=int, default=-1, help="UVC index; omit for JAI/Aravis")
    parser.add_argument("--target", choices=("sky", "balloon", "drone"), default="sky")
    parser.add_argument("--headless", action="store_true", help="no HDMI window; still publishes IPC")
    parser.add_argument("--hfov", type=float, default=10.0)
    parser.add_argument("--known-width", type=float, default=0.0, help="override meters; balloon=1.2 wing=1.0")
    parser.add_argument("--yolo", default="", help="optional yolov8 .pt path")
    parser.add_argument("--ipc", default="", help="optional APD1 unix datagram path")
    parser.add_argument("--record", default="", help="optional mp4 path")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--display", default="ACTPROVE Lock HDMI")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    known = args.known_width
    if args.webcam >= 0:
        source: FrameSource = WebcamSource(args.webcam, args.width, args.height)
        source_name = f"webcam{args.webcam}"
    else:
        source = AravisSource()
        source_name = "aravis-jai"

    model = None
    if args.yolo:
        from ultralytics import YOLO

        model = YOLO(args.yolo)

    publisher = UnixDetectionPublisher(args.ipc) if args.ipc else None
    writer = None
    if not args.headless:
        cv2.namedWindow(args.display, cv2.WINDOW_NORMAL)
        try:
            cv2.setWindowProperty(args.display, cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)
        except Exception:
            pass

    seq = 0
    hits = 0
    last_lock: Optional[Box] = None
    window_t = time.time()
    window_n = 0
    fps = 0.0
    print(
        f"HDMI preview target={args.target} known_width={known}m hfov={args.hfov} "
        f"src={source_name}. q=quit  f=window",
        flush=True,
    )
    try:
        while True:
            grab = source.read()
            if grab is None:
                continue
            h, w = grab.gray.shape[:2]
            if model is not None:
                boxes = yolo_boxes(model, grab.bgr)
            else:
                boxes = find_airborne_blobs(grab.gray, args.target)
            lock = pick_lock(boxes, w, h)
            if lock is not None:
                hits = min(hits + 1, 30)
                last_lock = lock
            else:
                hits = max(0, hits - 1)
                if hits == 0:
                    last_lock = None
            width_m = known if known > 0 else (1.2 if last_lock is not None and last_lock.class_id == 0 else 1.0)
            range_m = (
                range_from_width(last_lock.w, w, width_m, args.hfov) if last_lock is not None else float("nan")
            )
            if not args.headless:
                vis = draw_hud(
                    grab.bgr,
                    boxes,
                    last_lock,
                    target=args.target,
                    range_m=range_m,
                    width_m=width_m,
                    hits=hits,
                    source=source_name,
                    fps=fps,
                )
                if writer is None and args.record:
                    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
                    writer = cv2.VideoWriter(args.record, fourcc, 20.0, (vis.shape[1], vis.shape[0]))
                if writer is not None:
                    writer.write(vis)
                cv2.imshow(args.display, vis)
                key = cv2.waitKey(1) & 0xFF
                if key in (ord("q"), 27):
                    break
                if key == ord("f"):
                    cv2.setWindowProperty(args.display, cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_NORMAL)
            if publisher is not None:
                seq += 1
                # One box: the sky target nearest the crosshair. Companion locks that.
                msg = DetectionMsg(
                    t_pps=time.time(),
                    seq=seq,
                    cam_id=0,
                    src_w=w,
                    src_h=h,
                    boxes=[last_lock] if last_lock is not None else [],
                )
                publisher.publish(msg)
            if args.headless:
                time.sleep(0.01)
            window_n += 1
            now = time.time()
            if now - window_t >= 1.0:
                fps = window_n / (now - window_t)
                window_t = now
                window_n = 0
    finally:
        source.close()
        if publisher is not None:
            publisher.close()
        if writer is not None:
            writer.release()
        cv2.destroyAllWindows()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
