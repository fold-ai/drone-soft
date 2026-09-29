#pragma once
/* V1 SAFETY mission state machine — owner SAFETY.
 * States from interfaces/mavlink/mission_state.h (ap_mission_state_t).
 * Uplink cmd strings: WORK / ABORT / RTB / FTS / TARGET_DESTROYED /
 * MISS/REATTACK (mavlink_v1_cmds.h).
 * Source of truth: V1_PLAN §6. Mission = see/hold/commanded miss — NOT kill.
 * LAND_SOFT is an ABORT submode (not a new enum value).
 */

#include <stdint.h>

#include "mission_state.h"
#include "mavlink_v1_cmds.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Default timeouts (seconds). T_link matches AP_LOST_LINK_RTB_S. */
#define AP_SM_DEFAULT_T_BOOT_S   30.0
#define AP_SM_DEFAULT_T_SEARCH_S 60.0
#define AP_SM_DEFAULT_T_COAST_S   2.0
#define AP_SM_DEFAULT_T_LINK_S    ((double)AP_LOST_LINK_RTB_S)

typedef struct ap_mission_sm_config {
  double t_boot_s;   /* BOOT BIT-wait timeout → ABORT */
  double t_search_s; /* SEARCH without lock → ABORT */
  double t_coast_s;  /* lost-box coast LOCK/CLOSE → SEARCH */
  double t_link_s;   /* no heartbeat → RTB (never auto-FTS) */
} ap_mission_sm_config_t;

typedef struct ap_mission_sm_inputs {
  int heartbeat_ok;           /* Orin HEARTBEAT present this tick */
  int bit_ok;                 /* FCS/sensors BIT OK */
  int corridor_loaded;        /* engagement corridor loaded */
  int track_box_valid;        /* stable track + box */
  int geometry_ok_for_close;  /* geometry OK to enter CLOSE */
  int miss_hold_done;         /* commanded-miss hold complete */
  int hard_fail;              /* irrecoverable geo/energy → FTS */
  int operator_lock_request;  /* SEARCH + box + corridor → LOCK (manual) */
  int test_recover;           /* latch at SEARCH entry; post-CLOSE → LAND_SOFT */
} ap_mission_sm_inputs_t;

typedef struct ap_mission_sm_fts_outputs {
  int throttle_idle;    /* assert throttle idle */
  int surfaces_fixed;   /* freeze control surfaces */
  int fuel_cut_relay;   /* assert fuel-cut relay — NOT explosive */
} ap_mission_sm_fts_outputs_t;

typedef struct ap_mission_sm {
  ap_mission_sm_config_t cfg;
  ap_mission_state_t state;
  double state_enter_s;       /* time of last state entry */
  double last_hb_ok_s;        /* last time heartbeat_ok was true */
  double lost_box_since_s;    /* when track_box_valid went false; <0 if valid */
  double last_now_s;          /* last tick time */
  int have_time;              /* set after first tick */
  int engagement_cleared;     /* ABORT cleared engagement; WORK may re-enter */
  int work_pending;           /* WORK seen; applied on tick when eligible */
  int land_soft_active;       /* ABORT submode: soft-land (test recover) */
  int test_recover_armed;     /* latched test_recover at WORK→SEARCH entry */
  char reason[96];
} ap_mission_sm_t;

/* Fill config with defaults (safe to pass NULL to init for defaults). */
void ap_mission_sm_config_defaults(ap_mission_sm_config_t *cfg);

/* Init / conceptual ground power-cycle (only exit from FTS for sim/bench). */
void ap_mission_sm_init(ap_mission_sm_t *sm, const ap_mission_sm_config_t *cfg);
void ap_mission_sm_reset(ap_mission_sm_t *sm);

/* Periodic evaluation. now_s is monotonic seconds (PPS or host clock). */
void ap_mission_sm_tick(ap_mission_sm_t *sm, double now_s,
                        const ap_mission_sm_inputs_t *in);

/* Ground cmds: exact strings WORK, ABORT, RTB, FTS, TARGET_DESTROYED,
 * MISS/REATTACK.
 * Returns 0 if accepted or intentionally ignored per state rules;
 * -1 if cmd string unknown. */
int ap_mission_sm_handle_command(ap_mission_sm_t *sm, const char *cmd);

ap_mission_state_t ap_mission_sm_state(const ap_mission_sm_t *sm);
const char *ap_mission_sm_state_name(const ap_mission_sm_t *sm);
const char *ap_mission_sm_reason(const ap_mission_sm_t *sm);

/* 1 while ABORT submode LAND_SOFT (test recover after CLOSE miss). */
int ap_mission_sm_land_soft_active(const ap_mission_sm_t *sm);

void ap_mission_sm_fts_outputs(const ap_mission_sm_t *sm,
                               ap_mission_sm_fts_outputs_t *out);

#ifdef __cplusplus
}
#endif
