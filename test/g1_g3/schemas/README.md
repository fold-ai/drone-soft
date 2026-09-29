# Evidence schemas

JSON Schema (2020-12) for gate archives. Scorers validate `manifest.json` + `summary.json` (+ `sorties.jsonl` for G3) before TEST may write `PASS`.

| File | Artifact |
|------|----------|
| `run_manifest.schema.json` | `manifest.json` |
| `g1_summary.schema.json` | G1 `summary.json` |
| `g2_summary.schema.json` | G2 `summary.json` |
| `g3_summary.schema.json` | G3 campaign `summary.json` |
| `sortie.schema.json` | each line of G3 `sorties.jsonl` |

Detection wire format: `docs/icd/detection_msg.md` (not duplicated here).
