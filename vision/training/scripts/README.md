# Training scripts

Run these commands from the repository root.

## Export metrics

Export common Ultralytics/YOLO outputs (`results.csv`, `args.yaml`, and weights)
to the versioned JSON schema:

```bash
python3 vision/training/scripts/export_training_metrics.py \
  --run-dir vision/training/runs/<run-id>
```

The output defaults to
`vision/training/exports/metrics_<run-id>.json`. Use `--out` and optional
`--model`, `--epochs`, `--notes`, or `--git-sha` to override metadata. Missing
training files are represented with `null` placeholders. Create a valid sample
without a real run using:

```bash
python3 vision/training/scripts/export_training_metrics.py --demo
```

## Optional Supabase upload

The upload script is a dry-run by default and prints the three planned inserts:

```bash
python3 vision/training/scripts/upload_training_metrics_supabase.py \
  vision/training/exports/metrics_<run-id>.json --dry-run
```

For a protected server-side job only, set `SUPABASE_URL` and either
`SUPABASE_SERVICE_ROLE_KEY` or `SUPABASE_ANON_KEY`, then use `--execute`.
The `supabase` Python package is required for execution. Never put a service-role
key in source control, a browser, or a Next.js client.

See [`../docs/TRAINING_METRICS_SUPABASE.md`](../docs/TRAINING_METRICS_SUPABASE.md)
for the JSON schema and SQL sketch.
