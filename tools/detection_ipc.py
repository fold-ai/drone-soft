"""Little-endian APD1 datagram used by dual_detect_node / companion.

Keep this packing identical to vision/iface/src/detection_ipc.cpp.
"""
from __future__ import annotations

import socket
import struct
from dataclasses import dataclass, field
from typing import List, Sequence, Tuple

HEADER_SIZE = 32
BOX_SIZE = 24
MAX_DET = 32
VERSION = 1


@dataclass
class Box:
    x: float
    y: float
    w: float
    h: float
    conf: float
    class_id: int = 0


@dataclass
class DetectionMsg:
    t_pps: float
    seq: int
    cam_id: int
    src_w: int
    src_h: int
    boxes: List[Box] = field(default_factory=list)


def encode_detection_v1(msg: DetectionMsg) -> bytes:
    boxes = msg.boxes[:MAX_DET]
    buf = bytearray(HEADER_SIZE + len(boxes) * BOX_SIZE)
    buf[0:4] = b"APD1"
    struct.pack_into("<H", buf, 4, VERSION)
    struct.pack_into("<H", buf, 6, len(boxes))
    struct.pack_into("<d", buf, 8, float(msg.t_pps))
    struct.pack_into("<Q", buf, 16, int(msg.seq))
    buf[24] = int(msg.cam_id) & 0xFF
    struct.pack_into("<H", buf, 26, int(msg.src_w))
    struct.pack_into("<H", buf, 28, int(msg.src_h))
    for i, box in enumerate(boxes):
        off = HEADER_SIZE + i * BOX_SIZE
        struct.pack_into("<f", buf, off + 0, float(box.x))
        struct.pack_into("<f", buf, off + 4, float(box.y))
        struct.pack_into("<f", buf, off + 8, float(box.w))
        struct.pack_into("<f", buf, off + 12, float(box.h))
        struct.pack_into("<f", buf, off + 16, float(box.conf))
        struct.pack_into("<I", buf, off + 20, int(box.class_id))
    return bytes(buf)


def decode_detection_v1(data: bytes) -> DetectionMsg:
    if len(data) < HEADER_SIZE or data[0:4] != b"APD1":
        raise ValueError("bad APD1 header")
    version, count = struct.unpack_from("<HH", data, 4)
    if version != VERSION or count > MAX_DET:
        raise ValueError("bad APD1 version/count")
    if len(data) != HEADER_SIZE + count * BOX_SIZE:
        raise ValueError("bad APD1 length")
    t_pps = struct.unpack_from("<d", data, 8)[0]
    seq = struct.unpack_from("<Q", data, 16)[0]
    cam_id = data[24]
    src_w, src_h = struct.unpack_from("<HH", data, 26)
    boxes: List[Box] = []
    for i in range(count):
        off = HEADER_SIZE + i * BOX_SIZE
        x, y, w, h, conf = struct.unpack_from("<fffff", data, off)
        class_id = struct.unpack_from("<I", data, off + 20)[0]
        boxes.append(Box(x, y, w, h, conf, class_id))
    return DetectionMsg(t_pps, seq, cam_id, src_w, src_h, boxes)


class UnixDetectionPublisher:
    def __init__(self, path: str) -> None:
        self.path = path
        self.fd = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        self.fd.setblocking(False)

    def publish(self, msg: DetectionMsg) -> bool:
        try:
            self.fd.sendto(encode_detection_v1(msg), self.path)
            return True
        except OSError:
            return False

    def close(self) -> None:
        self.fd.close()


def roundtrip_ok() -> None:
    original = DetectionMsg(
        t_pps=42.25,
        seq=77,
        cam_id=0,
        src_w=2448,
        src_h=2048,
        boxes=[Box(10.0, 20.0, 30.0, 40.0, 0.75, 11)],
    )
    decoded = decode_detection_v1(encode_detection_v1(original))
    assert decoded.seq == 77
    assert decoded.src_w == 2448
    assert abs(decoded.boxes[0].conf - 0.75) < 1e-6


if __name__ == "__main__":
    roundtrip_ok()
    print("detection_ipc roundtrip PASS")
