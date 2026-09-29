# Training pipeline (one-pager)

```
inbox → ingest → label → QC → split → train → eval → export → metrics.json
```

| Step | Command (from `vision/`) |
|------|--------------------------|
| Ingest | `.venv/bin/python training/scripts/ingest_inbox.py` |
| Init labels | `.venv/bin/python training/scripts/init_labels.py --empty` *(or provisional for smoke)* |
| Real boxes | Label Studio / CVAT / YOLO txt → `import_yolo_labels.py` / `import_labelstudio.py` |
| QC | `.venv/bin/python training/scripts/label_qc.py` **(FAILS full-frame)** |
| Split | `.venv/bin/python training/scripts/split_manifests.py --smoke-per-class 30` |
| Train | `.venv/bin/python training/scripts/train.py --epochs 30 --device cpu --name run1` |
| Eval | `.venv/bin/python training/scripts/eval.py --weights training/runs/run1/weights/best.pt --name run1` |
| ONNX | `.venv/bin/python training/scripts/export_onnx.py --weights ... --name run1` |
| Metrics | `.venv/bin/python training/scripts/write_metrics_json.py --run-dir training/runs/run1` |

**Smoke:** QC with `--allow-provisional` + train `--smoke --allow-provisional`.  
**Production:** QC without flag; real boxes required; gate card `TEST_CARD_RECOGNITION_GATE.md`.

Classes: `shahed_136=0`, `geran_2=1`, `gerbera=2`. Negatives: `docs/NEGATIVES.md`.
