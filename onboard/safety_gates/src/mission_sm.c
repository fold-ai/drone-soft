/* V1 SAFETY mission state machine — see/hold/commanded miss, NOT kill.
 *
 * Forbidden V1 behaviors (enforced here):
 * - No combat-intercept state (enum stops at FTS=6; no INTERCEPT / LAND_SOFT enum).
 * - No terminal stick / impact-seeking guidance in CLOSE.
 * - No explosive FTS path — only idle + surfaces fixed + fuel-cut relay.
 * - Never auto-FTS on lost-link (lost-link → RTB only; LAND_SOFT exception holds).
 * - No SEARCH entry without WORK + corridor_loaded.
 * - No new-target hunt without corridor.
 * - WORK ignored in LOCK and CLOSE.
 */

#include "safety_gates/mission_sm.h"

#include <string.h>

static void set_reason(ap_mission_sm_t *sm, const char *r) {
  if (!sm || !r) {
    return;
  }
  strncpy(sm->reason, r, sizeof(sm->reason) - 1u);
  sm->reason[sizeof(sm->reason) - 1u] = '\0';
}

static void enter_state(ap_mission_sm_t *sm, ap_mission_state_t st, double now_s,
                        const char *reason) {
  sm->state = st;
  sm->state_enter_s = now_s;
  sm->lost_box_since_s = -1.0;
  /* Clear LAND_SOFT on any transition; caller may re-set for ABORT+LAND_SOFT. */
  sm->land_soft_active = 0;
  set_reason(sm, reason);
  if (st == AP_MISSION_ABORT) {
    sm->engagement_cleared = 1;
    sm->work_pending = 0;
  }
  if (st == AP_MISSION_SEARCH) {
    sm->engagement_cleared = 0;
    sm->work_pending = 0;
  }
  if (st == AP_MISSION_FTS || st == AP_MISSION_RTB || st == AP_MISSION_LOCK ||
      st == AP_MISSION_CLOSE) {
    sm->work_pending = 0;
  }
  /* FTS latch: outputs via getter; no explosive path exists. */
}

void ap_mission_sm_config_defaults(ap_mission_sm_config_t *cfg) {
  if (!cfg) {
    return;
  }
  cfg->t_boot_s = AP_SM_DEFAULT_T_BOOT_S;
  cfg->t_search_s = AP_SM_DEFAULT_T_SEARCH_S;
  cfg->t_coast_s = AP_SM_DEFAULT_T_COAST_S;
  cfg->t_link_s = AP_SM_DEFAULT_T_LINK_S;
}

void ap_mission_sm_init(ap_mission_sm_t *sm, const ap_mission_sm_config_t *cfg) {
  if (!sm) {
    return;
  }
  memset(sm, 0, sizeof(*sm));
  if (cfg) {
    sm->cfg = *cfg;
  } else {
    ap_mission_sm_config_defaults(&sm->cfg);
  }
  if (sm->cfg.t_link_s <= 0.0) {
    sm->cfg.t_link_s = AP_SM_DEFAULT_T_LINK_S;
  }
  if (sm->cfg.t_boot_s <= 0.0) {
    sm->cfg.t_boot_s = AP_SM_DEFAULT_T_BOOT_S;
  }
  if (sm->cfg.t_search_s <= 0.0) {
    sm->cfg.t_search_s = AP_SM_DEFAULT_T_SEARCH_S;
  }
  if (sm->cfg.t_coast_s <= 0.0) {
    sm->cfg.t_coast_s = AP_SM_DEFAULT_T_COAST_S;
  }
  sm->state = AP_MISSION_BOOT;
  sm->state_enter_s = 0.0;
  sm->last_hb_ok_s = 0.0;
  sm->last_now_s = 0.0;
  sm->lost_box_since_s = -1.0;
  sm->have_time = 0;
  sm->engagement_cleared = 1; /* BOOT: WORK allowed once BIT+corridor ready */
  sm->work_pending = 0;
  sm->land_soft_active = 0;
  sm->test_recover_armed = 0;
  set_reason(sm, "BOOT");
}

void ap_mission_sm_reset(ap_mission_sm_t *sm) {
  /* Conceptual ground power cycle — only allowed exit from latched FTS. */
  if (!sm) {
    return;
  }
  ap_mission_sm_config_t cfg = sm->cfg;
  ap_mission_sm_init(sm, &cfg);
}

static int try_enter_search(ap_mission_sm_t *sm, double now_s,
                            const ap_mission_sm_inputs_t *in, const char *why) {
  if (!in || !in->bit_ok) {
    set_reason(sm, "WORK held — BIT not OK");
    return 0;
  }
  if (!in->corridor_loaded) {
    set_reason(sm, "WORK held — corridor not loaded");
    return 0;
  }
  /* Arm TEST_RECOVER only at WORK→SEARCH (GCS before WORK); not mid-CLOSE. */
  sm->test_recover_armed = in->test_recover ? 1 : 0;
  enter_state(sm, AP_MISSION_SEARCH, now_s, why);
  return 1;
}

int ap_mission_sm_handle_command(ap_mission_sm_t *sm, const char *cmd) {
  if (!sm || !cmd) {
    return -1;
  }

  double now = sm->have_time ? sm->last_now_s : 0.0;

  /* Latched FTS ignores all uplink (exit = reset / power cycle only). */
  if (sm->state == AP_MISSION_FTS) {
    set_reason(sm, "FTS latched — cmd ignored");
    return 0;
  }

  if (strcmp(cmd, AP_UPLINK_NAME_FTS) == 0) {
    enter_state(sm, AP_MISSION_FTS, now, "FTS cmd");
    return 0;
  }

  /* Operator BDA: TARGET_DESTROYED → RTB (never LAND_SOFT). */
  if (strcmp(cmd, AP_UPLINK_NAME_TARGET_DESTROYED) == 0) {
    if (sm->state == AP_MISSION_BOOT) {
      set_reason(sm, "TARGET_DESTROYED ignored in BOOT");
      return 0;
    }
    if (sm->state == AP_MISSION_RTB) {
      set_reason(sm, "TARGET_DESTROYED ignored in RTB");
      return 0;
    }
    /* Accept in SEARCH / LOCK / CLOSE / ABORT. Always cruise RTB. */
    if (sm->state == AP_MISSION_SEARCH || sm->state == AP_MISSION_LOCK ||
        sm->state == AP_MISSION_CLOSE || sm->state == AP_MISSION_ABORT) {
      enter_state(sm, AP_MISSION_RTB, now, "TARGET_DESTROYED → RTB");
      sm->engagement_cleared = 1;
      return 0;
    }
    set_reason(sm, "TARGET_DESTROYED ignored");
    return 0;
  }

  /* Operator BDA: MISS/REATTACK → ABORT cleared (re-cue via WORK). Not kill. */
  if (strcmp(cmd, AP_UPLINK_NAME_MISS_REATTACK) == 0) {
    if (sm->state == AP_MISSION_BOOT) {
      set_reason(sm, "MISS/REATTACK ignored in BOOT");
      return 0;
    }
    if (sm->state == AP_MISSION_RTB) {
      set_reason(sm, "MISS/REATTACK ignored in RTB");
      return 0;
    }
    if (sm->state == AP_MISSION_SEARCH || sm->state == AP_MISSION_LOCK ||
        sm->state == AP_MISSION_CLOSE || sm->state == AP_MISSION_ABORT) {
      if (sm->state == AP_MISSION_ABORT) {
        sm->engagement_cleared = 1;
        sm->land_soft_active = 0;
        set_reason(sm, "MISS/REATTACK → ABORT");
        return 0;
      }
      enter_state(sm, AP_MISSION_ABORT, now, "MISS/REATTACK → ABORT");
      return 0;
    }
    set_reason(sm, "MISS/REATTACK ignored");
    return 0;
  }

  /* RTB preferred: WORK/ABORT ignored while in RTB. */
  if (sm->state == AP_MISSION_RTB) {
    if (strcmp(cmd, AP_UPLINK_NAME_WORK) == 0 ||
        strcmp(cmd, AP_UPLINK_NAME_ABORT) == 0) {
      set_reason(sm, "RTB preferred — WORK/ABORT ignored");
      return 0;
    }
    if (strcmp(cmd, AP_UPLINK_NAME_RTB) == 0) {
      set_reason(sm, "already RTB");
      return 0;
    }
    return -1;
  }

  if (strcmp(cmd, AP_UPLINK_NAME_RTB) == 0) {
    /* BOOT allowed cmds: WORK, FTS only. */
    if (sm->state == AP_MISSION_BOOT) {
      set_reason(sm, "RTB ignored in BOOT");
      return 0;
    }
    /* Explicit RTB overrides LAND_SOFT (auto lost-link does not). */
    enter_state(sm, AP_MISSION_RTB, now, "RTB cmd");
    return 0;
  }

  if (strcmp(cmd, AP_UPLINK_NAME_ABORT) == 0) {
    if (sm->state == AP_MISSION_BOOT) {
      set_reason(sm, "ABORT ignored in BOOT");
      return 0;
    }
    if (sm->state == AP_MISSION_ABORT) {
      sm->engagement_cleared = 1;
      set_reason(sm, "already ABORT — engagement cleared");
      return 0;
    }
    enter_state(sm, AP_MISSION_ABORT, now, "ABORT cmd");
    return 0;
  }

  if (strcmp(cmd, AP_UPLINK_NAME_WORK) == 0) {
    /* No WORK in LOCK or CLOSE. */
    if (sm->state == AP_MISSION_LOCK || sm->state == AP_MISSION_CLOSE) {
      set_reason(sm, "WORK ignored in LOCK/CLOSE");
      return 0;
    }
    if (sm->state == AP_MISSION_SEARCH) {
      set_reason(sm, "already SEARCH");
      return 0;
    }
    if (sm->state == AP_MISSION_BOOT || sm->state == AP_MISSION_ABORT) {
      sm->work_pending = 1;
      sm->engagement_cleared = 1;
      set_reason(sm, "WORK pending — need BIT+corridor on tick");
      return 0;
    }
    return 0;
  }

  return -1;
}

void ap_mission_sm_tick(ap_mission_sm_t *sm, double now_s,
                        const ap_mission_sm_inputs_t *in) {
  if (!sm || !in) {
    return;
  }

  if (!sm->have_time) {
    sm->have_time = 1;
    sm->state_enter_s = now_s;
    sm->last_hb_ok_s = now_s;
  }
  sm->last_now_s = now_s;

  if (in->heartbeat_ok) {
    sm->last_hb_ok_s = now_s;
  }

  /* --- FTS latched: terminal; no explosive path --- */
  if (sm->state == AP_MISSION_FTS) {
    set_reason(sm, "FTS latched");
    return;
  }

  /* hard_fail → FTS (never from lost-link alone); applies during LAND_SOFT */
  if (in->hard_fail) {
    enter_state(sm, AP_MISSION_FTS, now_s, "hard_fail → FTS");
    return;
  }

  /* Lost-link: T_link no heartbeat → RTB (NEVER auto-FTS).
   * Exception: LAND_SOFT ABORT submode — auto lost-link does not → RTB. */
  if (!in->heartbeat_ok &&
      (now_s - sm->last_hb_ok_s) >= sm->cfg.t_link_s) {
    int land_soft_hold =
        (sm->state == AP_MISSION_ABORT && sm->land_soft_active);
    if (!land_soft_hold) {
      if (sm->state != AP_MISSION_RTB) {
        enter_state(sm, AP_MISSION_RTB, now_s, "lost-link → RTB");
      } else {
        set_reason(sm, "RTB (lost-link)");
      }
      return;
    }
    /* Stay in ABORT/LAND_SOFT; fall through to ABORT case. */
  }

  switch (sm->state) {
    case AP_MISSION_BOOT: {
      if (!in->bit_ok && (now_s - sm->state_enter_s) >= sm->cfg.t_boot_s) {
        enter_state(sm, AP_MISSION_ABORT, now_s, "BOOT BIT timeout → ABORT");
        break;
      }
      if (sm->work_pending) {
        (void)try_enter_search(sm, now_s, in, "WORK → SEARCH");
      } else {
        set_reason(sm, in->bit_ok ? "BOOT BIT OK — waiting WORK"
                                  : "BOOT waiting BIT");
      }
      break;
    }

    case AP_MISSION_SEARCH: {
      if (!in->corridor_loaded) {
        /* No new-target hunt without corridor — fail-HOLD. */
        set_reason(sm, "SEARCH fail-HOLD — no corridor");
        break;
      }
      if ((now_s - sm->state_enter_s) >= sm->cfg.t_search_s) {
        enter_state(sm, AP_MISSION_ABORT, now_s, "SEARCH timeout → ABORT");
        break;
      }
      /* Manual lock (operator) or auto track+box — same gates. */
      if (in->track_box_valid) {
        if (in->operator_lock_request) {
          enter_state(sm, AP_MISSION_LOCK, now_s, "operator_lock");
        } else {
          enter_state(sm, AP_MISSION_LOCK, now_s, "track+box → LOCK");
        }
        break;
      }
      set_reason(sm, "SEARCH");
      break;
    }

    case AP_MISSION_LOCK: {
      if (!in->track_box_valid) {
        if (sm->lost_box_since_s < 0.0) {
          sm->lost_box_since_s = now_s;
        }
        if ((now_s - sm->lost_box_since_s) >= sm->cfg.t_coast_s) {
          enter_state(sm, AP_MISSION_SEARCH, now_s, "lost-box coast → SEARCH");
          break;
        }
        set_reason(sm, "LOCK coasting lost-box");
      } else {
        sm->lost_box_since_s = -1.0;
        if (in->geometry_ok_for_close) {
          enter_state(sm, AP_MISSION_CLOSE, now_s, "geometry OK → CLOSE");
          break;
        }
        set_reason(sm, "LOCK");
      }
      break;
    }

    case AP_MISSION_CLOSE: {
      /* Commanded miss only — never impact-seeking / terminal stick. */
      if (in->miss_hold_done) {
        if (sm->test_recover_armed) {
          enter_state(sm, AP_MISSION_ABORT, now_s, "LAND_SOFT");
          sm->land_soft_active = 1;
        } else {
          enter_state(sm, AP_MISSION_ABORT, now_s,
                      "miss-distance hold done → ABORT");
        }
        break;
      }
      if (!in->track_box_valid) {
        if (sm->lost_box_since_s < 0.0) {
          sm->lost_box_since_s = now_s;
        }
        if ((now_s - sm->lost_box_since_s) >= sm->cfg.t_coast_s) {
          enter_state(sm, AP_MISSION_SEARCH, now_s, "CLOSE coast → SEARCH");
          break;
        }
        set_reason(sm, "CLOSE coasting lost-box");
      } else {
        sm->lost_box_since_s = -1.0;
        set_reason(sm, "CLOSE commanded-miss hold");
      }
      break;
    }

    case AP_MISSION_ABORT: {
      if (sm->work_pending) {
        (void)try_enter_search(sm, now_s, in,
                               "ABORT clear + WORK → SEARCH");
        break;
      }
      if (sm->land_soft_active) {
        set_reason(sm, "LAND_SOFT");
      } else {
        set_reason(sm, "ABORT fail-HOLD — engagement cleared");
      }
      break;
    }

    case AP_MISSION_RTB: {
      set_reason(sm, "RTB in progress");
      break;
    }

    case AP_MISSION_FTS:
      break;
  }
}

ap_mission_state_t ap_mission_sm_state(const ap_mission_sm_t *sm) {
  return sm ? sm->state : AP_MISSION_BOOT;
}

const char *ap_mission_sm_state_name(const ap_mission_sm_t *sm) {
  if (!sm) {
    return "BOOT";
  }
  switch (sm->state) {
    case AP_MISSION_BOOT:
      return "BOOT";
    case AP_MISSION_SEARCH:
      return "SEARCH";
    case AP_MISSION_LOCK:
      return "LOCK";
    case AP_MISSION_CLOSE:
      return "CLOSE";
    case AP_MISSION_ABORT:
      return "ABORT";
    case AP_MISSION_RTB:
      return "RTB";
    case AP_MISSION_FTS:
      return "FTS";
    default:
      return "UNKNOWN";
  }
}

const char *ap_mission_sm_reason(const ap_mission_sm_t *sm) {
  return sm ? sm->reason : "";
}

int ap_mission_sm_land_soft_active(const ap_mission_sm_t *sm) {
  return (sm && sm->land_soft_active) ? 1 : 0;
}

void ap_mission_sm_fts_outputs(const ap_mission_sm_t *sm,
                               ap_mission_sm_fts_outputs_t *out) {
  if (!out) {
    return;
  }
  if (sm && sm->state == AP_MISSION_FTS) {
    /* Non-explosive FTS only — no warhead / explosive API exists. */
    out->throttle_idle = 1;
    out->surfaces_fixed = 1;
    out->fuel_cut_relay = 1;
  } else {
    out->throttle_idle = 0;
    out->surfaces_fixed = 0;
    out->fuel_cut_relay = 0;
  }
}
