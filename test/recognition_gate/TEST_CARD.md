# Recognition training gate — pointer

**Canonical card:** [`../../vision/training/docs/TEST_CARD_RECOGNITION_GATE.md`](../../vision/training/docs/TEST_CARD_RECOGNITION_GATE.md)

**ID:** REC-TRAIN-GATE-V1  
**Owner:** test  

Summary thresholds (detail in canonical card):

| Check | PASS bar |
|-------|----------|
| mAP@0.50 | ≥ 0.60 |
| mAP@0.50:0.95 | ≥ 0.35 |
| Per-class AP@0.50 | ≥ 0.50 each (`shahed_136`, `geran_2`, `gerbera`) |
| Pairwise class confusion | ≤ 0.15 per GT class → wrong class |
| FP on negative images | ≤ 5% of negative images with ≥1 det |
| Ungated class_id | **0** allowed at deploy conf |
| Labels | `real` required for field PASS; `provisional` → CONDITIONAL max |

No radar. English only.
