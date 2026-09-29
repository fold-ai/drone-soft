# Orin deploy

Two mutually exclusive stacks:

| Stack | Units | FC access | Use |
|-------|-------|-----------|-----|
| **Companion (intercept)** | `actprove-companion` + `actprove-vision` | UART TELEM2 | Lock / Takeover / GUIDED |
| **Passive (camera only)** | `actprove-passive-vision` + logger | none | Bench detect, no commands |

Companion install: `install_companion.sh` and `docs/bringup/orin_companion.md`.  
Passive install: `README_PASSIVE.md`.
