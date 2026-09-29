#pragma once
/* NAMED_VALUE_INT name = "MISSION_STATE" — source of truth: #safety §6 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AP_MISSION_STATE_NAME "MISSION_STATE"  /* application/log name */
#define AP_MISSION_STATE_WIRE_NAME "MISSION_ST" /* NAMED_VALUE_INT char[10] */

typedef enum ap_mission_state {
  AP_MISSION_BOOT   = 0,
  AP_MISSION_SEARCH = 1,
  AP_MISSION_LOCK   = 2,
  AP_MISSION_CLOSE  = 3,
  AP_MISSION_ABORT  = 4,
  AP_MISSION_RTB    = 5,
  AP_MISSION_FTS    = 6
} ap_mission_state_t;

#ifdef __cplusplus
}
#endif
