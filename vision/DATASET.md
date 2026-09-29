# Perception V1 dataset — EO target photos (range test)

## Purpose
Class-gated Lock: train/eval YOLOv8n on user-supplied photos of range-test targets
so the detector can lock only allowed classes.

## V1 scope
- **EO images only** (thermal dataset = V1.1)
- **Primary classes (range test):**
  | id | name |
  |----|------|
  | 0 | `shahed_136` |
  | 1 | `geran_2` |
  | 2 | `gerbera` |
- Legacy optional (NOT in V1 range-test Lock gate): `quad`, `fixed_wing`
- Gate: Near/Mid/Far bins when sized; ≥10% negatives preferred; sequence-based split

## Inbox layout (user drop)

```
data/inbox/
  shahed_136/
  geran_2/
  gerbera/
  negative/       # optional empty sky/ground (no target)
  # legacy optional: quad/, fixed_wing/
```

Supported: `.jpg` `.jpeg` `.png` `.bmp` `.tif` `.tiff` `.webp`

## After ingest (`data/raw/`)

```
data/raw/
  images/...
  labels/<same stem>.txt   # YOLO: class_id cx cy w h (normalized)
  meta.jsonl               # path, class, cam_id=0, source
```

Unlabeled inbox photos get a **full-frame provisional box** of that class so training
can start; replace with real boxes via your labeler, then re-run `label_qc.py`.

## Checklist before train V1
- [x] Inbox classes ingested (see counts below)
- [ ] Near / Mid / Far representation when sized boxes exist
- [ ] ≥10% negatives (TBD)
- [x] Sequence-safe train/val/test (no clip leakage)
- [ ] `label_qc.py` PASS
- [ ] EO only (`cam_id=0` in meta)

## Inbox classes (ingested 2026-09-16)

| Folder | Class id | Label | Count (approx) |
|--------|----------|-------|---------------:|
| `vision/data/inbox/shahed_136/` | 0 | Shahed-136 | ~215 |
| `vision/data/inbox/geran_2/` | 1 | Geran-2 | ~251 |
| `vision/data/inbox/gerbera/` | 2 | Gerbera | ~252 |

See `vision/data/inbox/MANIFEST.md` and `configs/classes_range_v1.yaml`.
Negatives still TBD. Class-gated Lock only for `{0,1,2}`.
