# Runners

Offline scorers / checklists. No JetPack required for structural checks.

| Script | Purpose |
|--------|---------|
| `score_g1.py` | Score G1 archive: Hz, p95 latency, PPS flags |
| `score_g2.py` | Score G2: FC heading proof present, WORK-gated SEARCH, FTS holder |
| `score_g3.py` | Score G3 campaign: count valid sorties vs envelope |
| `check_fts_gate.py` | Exit non-zero if G2/G3 scheduled with unnamed FTS holder |

Usage (from repo root):

```bash
python3 test/g1_g3/runners/score_g1.py path/to/G1_.../
python3 test/g1_g3/runners/check_fts_gate.py G2 --fts-holder ""
```

PASS/FAIL marker files are written only after human TEST confirmation — scorers print a recommendation.
