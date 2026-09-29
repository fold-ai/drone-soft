# tools/ — System stack offline utilities

| Tool | Role |
|------|------|
| [`latency_probe/`](latency_probe/) | Pass/fail vs camera→Orin (30–40 ms) and E2E (50–80 ms) |
| [`log_replay/`](log_replay/) | Validate black-box run folder against ICD fields |
| [`extrinsics_calib/`](extrinsics_calib/) | EO↔IMU lever-arm YAML stub (Navigation owns process) |
| [`first_test_hdmi_preview.py`](first_test_hdmi_preview.py) | HDMI HUD for JAI/Aravis or webcam: balloon/drone box + LOCK |
| [`detection_ipc.py`](detection_ipc.py) | Python APD1 datagram (same as C++ companion IPC) |

```bash
python3 tools/latency_probe/latency_probe.py --demo
python3 tools/log_replay/log_replay.py /path/to/run
python3 tools/extrinsics_calib/extrinsics_calib.py
python3 tools/verify_local_passive.py
python3 tools/detection_ipc.py
# On Orin with HDMI: python3 tools/first_test_hdmi_preview.py --target balloon
```

Stdlib Python 3; optional `numpy` / `PyYAML`.
