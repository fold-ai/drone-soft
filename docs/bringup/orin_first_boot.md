# Orin NX First Boot

**Goal:** JetPack up, SSH, NVMe mounted, PPS GPIO visible, UART TELEM2 present.  
**Owner:** System stack + hardware

## Procedure

1. Flash JetPack (Orin NX 16GB) per NVIDIA / Forecr carrier docs.
2. First login: set hostname `actprove-orin`, enable SSH, disable WiFi-as-C2 (debug SSH only).
3. Mount NVMe for black-box:
   ```bash
   sudo mkfs.ext4 /dev/nvme0n1p1   # once
   sudo mkdir -p /data/blackbox
   echo '/dev/nvme0n1p1 /data/blackbox ext4 defaults,noatime 0 2' | sudo tee -a /etc/fstab
   sudo mount /data/blackbox
   df -h /data/blackbox
   ```
4. Confirm UART for FC **TELEM2** (Orin side):
   ```bash
   ls -l /dev/ttyTHS* /dev/ttyUSB* 2>/dev/null
   # Expect harness to TELEM2; baud 57600 (match FC SERIALn)
   ```
5. Confirm PPS input (pending Forecr FAE pin name):
   ```bash
   ls -l /dev/pps0 2>/dev/null || echo "PPS device TBD — FAE blocker"
   ```
6. Install deps: CUDA, TensorRT, Jetson Multimedia API (Argus), cmake, git.
7. Sync repo to `/opt/drone-soft`.
8. Build onboard scaffold:
   ```bash
   cmake -S /opt/drone-soft/onboard -B /opt/drone-soft/onboard/build
   cmake --build /opt/drone-soft/onboard/build -j$(nproc)
   ```
9. Run smoke **before** camera bring-up:
   - `docs/bringup/logger_smoke.md`
   - `docs/bringup/mavlink_telem2_check.md`
   - then `./onboard/bringup/scripts/start_order.sh`

## ELP USB3 camera bring-up

The selected AR0234 cameras are UVC/V4L2 devices; they do not use Argus.

```bash
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video0 --list-formats-ext

cmake -S vision -B vision/build -DSOFT_NO_JETPACK=OFF
cmake --build vision/build -j$(nproc)

# Works with SEARCH alone; TELE may be disconnected during the first test.
./vision/build/dual_capture_bench --search /dev/video0 --tele /dev/video1

# Real TensorRT inference after the engine has been built on this Jetson.
./vision/build/dual_detect_node --search /dev/video0 --tele /dev/video1 \
  --engine vision/models/engines/yolov8n_fp16.engine --frames 0
```

Use `--frames N` for a bounded bench run. The systemd passive observer uses
`--frames 0` and relies on `SIGTERM` for a clean stop.

Keep `pixel_format: YUYV` for the current deterministic preprocessor. Confirm
both cameras enumerate on separate USB3 paths with `lsusb -t`; if 1080p60 on
two streams is unstable, validate 1080p30 before changing the inference path.

**Hold:** do not buy/attach Forecr carrier production unit until Integration PASS on PPS/FSYNC/DRDY→HTE.
