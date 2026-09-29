# Labeling — real boxes for class-gated Lock

**Classes:** `shahed_136=0`, `geran_2=1`, `gerbera=2` only.  
**Policy:** provisional full-frame boxes are **smoke only**. Production QC **rejects** them.

## Dataset layout (training system)

```
vision/training/datasets/raw/
  images/<class>__<stem>.jpg
  labels/<same stem>.txt      # YOLO: class_id cx cy w h (normalized 0–1)
  meta.jsonl                  # provisional flags, source, cam_id=0
```

Inbox source (do not train from inbox directly):

```
vision/data/inbox/{shahed_136,geran_2,gerbera,negative}/
```

## Reject full-frame policy

A box is **junk / provisional** if:

- `w * h >= 0.95`, **or**
- `w >= 0.98` **and** `h >= 0.98`

`scripts/label_qc.py` **FAILS** these by default.  
`--allow-provisional` is **smoke only** — production gate must pass without it.

## Tooling options

### A) YOLO txt (simplest import)

1. Draw boxes in any editor that exports YOLO darknet txt.
2. One `.txt` per image, same stem as image (or source stem before `class__` prefix).
3. Import:

```bash
vision/.venv/bin/python vision/training/scripts/import_yolo_labels.py \
  --from-dir /path/to/yolo_labels/
```

### B) Label Studio

1. Create project; labeling config with RectangleLabels for `shahed_136`, `geran_2`, `gerbera`.
2. Import images from `datasets/raw/images/` (or inbox).
3. Export **YOLO** or **JSON**.
4. Import:

```bash
vision/.venv/bin/python vision/training/scripts/import_labelstudio.py \
  --yolo-dir /path/to/ls_yolo_export/
# or
vision/.venv/bin/python vision/training/scripts/import_labelstudio.py \
  --json /path/to/export.json
```

### C) CVAT

Export as YOLO 1.1 zip → unzip → `import_yolo_labels.py --from-dir ...` or `--from-zip`.

## Empty / unlabeled

```bash
vision/.venv/bin/python vision/training/scripts/init_labels.py --empty
```

Creates empty sidecars for real labeling. Negatives stay empty forever.

## QC

```bash
# Production (must PASS):
vision/.venv/bin/python vision/training/scripts/label_qc.py

# Smoke only:
vision/.venv/bin/python vision/training/scripts/label_qc.py --allow-provisional
```

Production gate **FAILS** without real boxes. See `TEST_CARD_RECOGNITION_GATE.md`.
