# Orin TensorRT build (later — on device)

Local pipeline exports **ONNX** under `vision/training/exports/`.  
**Do not** require TensorRT on the training workstation.

## After ONNX exists

On Jetson Orin (with TensorRT + CUDA matching Orin JetPack):

```bash
# Example — adjust paths / precision for deploy config
trtexec \
  --onnx=/path/to/smoke_e2.onnx \
  --saveEngine=/path/to/smoke_e2.engine \
  --fp16 \
  --workspace=4096
```

Or use Ultralytics on-device:

```bash
yolo export model=best.pt format=engine device=0 imgsz=640
```

## Notes

- Match `imgsz` / letterbox to `configs/detector_yolov8n.yaml` (deploy).
- Keep class order `{0: shahed_136, 1: geran_2, 2: gerbera}`.
- Weights stay on Orin / this repo — not in browser dashboards.
- See also `vision/cmake/FindTensorRT.cmake` and detect TRT stubs.
