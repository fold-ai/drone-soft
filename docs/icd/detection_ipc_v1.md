# Passive detection IPC v1

Local transport: Unix datagram socket `/run/actprove/detections.sock`.
Publisher: `detect_node` or `dual_detect_node`. Consumer:
`detection_logger`. This interface has no control or flight-controller fields.

All multibyte fields are little-endian. A datagram is accepted only when its
size exactly matches `32 + n * 24` bytes and all numeric/geometry checks pass.

## Header — 32 bytes

| Offset | Type | Meaning |
|---:|---|---|
| 0 | char[4] | `APD1` |
| 4 | u16 | version = 1 |
| 6 | u16 | box count, 0–32 |
| 8 | f64 | measurement timestamp |
| 16 | u64 | frame sequence |
| 24 | u8 | camera id |
| 25 | u8 | reserved |
| 26 | u16 | source width |
| 28 | u16 | source height |
| 30 | u16 | reserved |

## Box — 24 bytes each

| Offset | Type | Meaning |
|---:|---|---|
| 0 | f32 | x, full-sensor pixels |
| 4 | f32 | y, full-sensor pixels |
| 8 | f32 | width, pixels |
| 12 | f32 | height, pixels |
| 16 | f32 | confidence, 0–1 |
| 20 | u32 | class id |

The subscriber rejects unknown magic/version, truncated or oversized packets,
non-finite values, negative geometry, confidence outside `[0,1]`, and boxes
outside the declared source dimensions. Unix datagrams preserve message
boundaries; no compiler struct layout is placed on the wire.

Backpressure policy is drop-newest at the publisher. `METRIC` output exposes
`ipc_sent` and `ipc_dropped`; camera inference is not blocked by a slow or
restarting logger.
