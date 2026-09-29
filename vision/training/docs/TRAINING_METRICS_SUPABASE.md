# Training metrics → Supabase

User creates project; PM fills env via secret form. **Never commit keys.**

Local pipeline always writes `training/runs/<name>/metrics.json`.  
Upload is optional and **no-ops** without env keys.

## Env (optional)

| Variable | Purpose |
|----------|---------|
| `SUPABASE_URL` or `TRAINING_SUPABASE_URL` | Project URL |
| `SUPABASE_SERVICE_KEY` / `SUPABASE_ANON_KEY` / `TRAINING_SUPABASE_KEY` | API key |

`scripts/write_metrics_json.py --upload` checks env and no-ops if missing.

## Tables (RLS on)

### `training_runs`

| Column | Type | Notes |
|--------|------|-------|
| `id` | text / uuid PK | `run_id` |
| `started_at` | timestamptz | ISO-8601 |
| `model` | text | e.g. `yolov8n` |
| `epochs` | int | |
| `git_sha` | text nullable | |
| `notes` | text | e.g. provisional smoke |

### `training_metrics`

| Column | Type | Notes |
|--------|------|-------|
| `run_id` | text FK | → training_runs.id |
| `split` | text | `val` / `test` |
| `map50` | float | |
| `map50_95` | float | |
| `per_class` | jsonb | `{shahed_136: {ap50, ap}, ...}` |
| `fp_on_negatives` | float nullable | null = N/A (no negatives) |

### `training_artifacts`

| Column | Type | Notes |
|--------|------|-------|
| `run_id` | text FK | |
| `kind` | text | `weights` \| `onnx` \| `log` |
| `storage_path` | text | local or object path |

## Local `metrics.json` schema

Emitted by `write_metrics_json.py` (and fields filled by `eval.py`):

```json
{
  "run_id": "smoke_e2_a1b2c3d4",
  "started_at": "2026-09-17T04:00:00+00:00",
  "model": "yolov8n",
  "epochs": 2,
  "git_sha": null,
  "notes": "smoke provisional labels",
  "split": "val",
  "map50": 0.0,
  "map50_95": 0.0,
  "per_class": {
    "shahed_136": {"ap50": null, "ap": null},
    "geran_2": {"ap50": null, "ap": null},
    "gerbera": {"ap50": null, "ap": null}
  },
  "fp_on_negatives": null,
  "fp_on_negatives_note": "N/A — no negatives in dataset (count=0)",
  "negatives_count": 0,
  "artifacts": {
    "weights_best": "vision/training/runs/smoke_e2/weights/best.pt",
    "weights_last": ".../last.pt",
    "onnx": "vision/training/exports/smoke_e2.onnx",
    "eval_summary": ".../eval_summary.json",
    "results_csv": ".../results.csv"
  },
  "training_runs": { "id": "...", "started_at": "...", "model": "yolov8n", "epochs": 2, "git_sha": null, "notes": "" },
  "training_metrics": { "run_id": "...", "split": "val", "map50": 0.0, "map50_95": 0.0, "per_class": {}, "fp_on_negatives": null },
  "training_artifacts": [
    { "run_id": "...", "kind": "weights", "storage_path": "..." }
  ]
}
```

## Dashboard

Vercel / Next under actprove-ops/cloud later — **weights stay in this repo / Orin**, not in browser.
