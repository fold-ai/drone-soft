# V1 MAVLink message list (common.xml)

| Message | ID | Dir | Rate | V1 |
|---------|----|-----|------|----|
| HEARTBEAT | 0 | Orin↔FC↔GCS | 1 Hz | Y |
| SET_POSITION_TARGET_LOCAL_NED | 84 | Orin→FC | 10–20 Hz | Y |
| COMMAND_LONG WORK/ABORT/RTB/FTS | 76 | Orin\|GCS→FC | event | Y |
| COMMAND_ACK | 77 | FC→Orin,GCS | event | Y |
| NAMED_VALUE_INT MISSION_STATE | 252 | Orin→FC,GCS | change/2 Hz | Y |
| VFR_HUD | 74 | FC→Orin,GCS | 5 Hz | Y |
| BATTERY_STATUS | 147 | FC→Orin,GCS | 1 Hz | Y |
| SYS_STATUS | 1 | FC→Orin,GCS | 1 Hz | Y |
| GPS_RAW_INT | 24 | FC→GCS only | 1 Hz | Y |
| STATUSTEXT | 253 | any→GCS | event | N |

**TELEM1** = RFD900x C2 57600 / ~64 kbps · **TELEM2** = Orin UART · **video ≠ MAVLink**  
**Lost-link:** 3 s no Orin HEARTBEAT → RTB (not FTS)

**BDA (FROZEN):** USER_1 param1=3 `TARGET_DESTROYED` → RTB always; param1=4 `MISS/REATTACK` (no param1=5).
