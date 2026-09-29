/* Unit tests — SAFETY V1 acceptance paths for mission_sm. */
#include "safety_gates/mission_sm.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void expect_state(const ap_mission_sm_t *sm, ap_mission_state_t want,
                         const char *label) {
  if (ap_mission_sm_state(sm) != want) {
    fprintf(stderr, "FAIL %s: got %s (%d) want %d — %s\n", label,
            ap_mission_sm_state_name(sm), (int)ap_mission_sm_state(sm),
            (int)want, ap_mission_sm_reason(sm));
    g_fail = 1;
  } else {
    printf("PASS %s → %s\n", label, ap_mission_sm_state_name(sm));
  }
}

static void expect_true(int cond, const char *label) {
  if (!cond) {
    fprintf(stderr, "FAIL %s\n", label);
    g_fail = 1;
  } else {
    printf("PASS %s\n", label);
  }
}

static ap_mission_sm_inputs_t base_in(void) {
  ap_mission_sm_inputs_t in;
  memset(&in, 0, sizeof(in));
  in.heartbeat_ok = 1;
  in.bit_ok = 1;
  in.corridor_loaded = 1;
  return in;
}

/* Drive BOOT → SEARCH via WORK. */
static void to_search(ap_mission_sm_t *sm, ap_mission_sm_inputs_t *in,
                      double *t) {
  ap_mission_sm_tick(sm, *t, in);
  *t += 0.1;
  ap_mission_sm_handle_command(sm, "WORK");
  ap_mission_sm_tick(sm, *t, in);
  *t += 0.1;
}

/* Drive SEARCH → CLOSE (auto lock + geometry). */
static void to_close(ap_mission_sm_t *sm, ap_mission_sm_inputs_t *in,
                     double *t) {
  in->track_box_valid = 1;
  ap_mission_sm_tick(sm, *t, in);
  *t += 0.1;
  in->geometry_ok_for_close = 1;
  ap_mission_sm_tick(sm, *t, in);
  *t += 0.1;
}

/* WORK-only SEARCH entry from BOOT (requires corridor). */
static void test_work_only_search_entry(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_config_t cfg;
  ap_mission_sm_config_defaults(&cfg);
  ap_mission_sm_init(&sm, &cfg);

  ap_mission_sm_inputs_t in = base_in();
  ap_mission_sm_tick(&sm, 0.0, &in);
  expect_state(&sm, AP_MISSION_BOOT, "boot stays without WORK");

  /* Corridor missing: WORK must not enter SEARCH */
  in.corridor_loaded = 0;
  ap_mission_sm_handle_command(&sm, "WORK");
  ap_mission_sm_tick(&sm, 1.0, &in);
  expect_state(&sm, AP_MISSION_BOOT, "no SEARCH without corridor");

  in.corridor_loaded = 1;
  ap_mission_sm_tick(&sm, 2.0, &in);
  expect_state(&sm, AP_MISSION_SEARCH, "WORK + corridor → SEARCH");
}

/* Lost-link 3s → RTB, never auto-FTS. */
static void test_lost_link_rtb(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_config_t cfg;
  ap_mission_sm_config_defaults(&cfg);
  cfg.t_search_s = 120.0;
  ap_mission_sm_init(&sm, &cfg);

  ap_mission_sm_inputs_t in = base_in();
  ap_mission_sm_tick(&sm, 0.0, &in);
  ap_mission_sm_handle_command(&sm, "WORK");
  ap_mission_sm_tick(&sm, 0.1, &in);
  expect_state(&sm, AP_MISSION_SEARCH, "precondition SEARCH");

  in.heartbeat_ok = 0;
  ap_mission_sm_tick(&sm, 1.0, &in); /* 1s lost — still SEARCH */
  expect_state(&sm, AP_MISSION_SEARCH, "link loss < 3s stays");
  ap_mission_sm_tick(&sm, 3.2, &in); /* >= 3s since last hb at 0.1 */
  expect_state(&sm, AP_MISSION_RTB, "lost-link → RTB");

  ap_mission_sm_fts_outputs_t fts;
  ap_mission_sm_fts_outputs(&sm, &fts);
  expect_true(!fts.throttle_idle && !fts.fuel_cut_relay,
              "lost-link does not assert FTS outputs");
}

/* Lost-track → SEARCH; SEARCH timeout → ABORT. */
static void test_lost_track_search_abort(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_config_t cfg;
  ap_mission_sm_config_defaults(&cfg);
  cfg.t_coast_s = 2.0;
  cfg.t_search_s = 5.0;
  ap_mission_sm_init(&sm, &cfg);

  ap_mission_sm_inputs_t in = base_in();
  ap_mission_sm_tick(&sm, 0.0, &in);
  ap_mission_sm_handle_command(&sm, "WORK");
  ap_mission_sm_tick(&sm, 0.1, &in);

  in.track_box_valid = 1;
  ap_mission_sm_tick(&sm, 0.2, &in);
  expect_state(&sm, AP_MISSION_LOCK, "track → LOCK");

  in.track_box_valid = 0;
  ap_mission_sm_tick(&sm, 0.3, &in);
  expect_state(&sm, AP_MISSION_LOCK, "coast < T_coast");
  ap_mission_sm_tick(&sm, 2.5, &in);
  expect_state(&sm, AP_MISSION_SEARCH, "lost-box → SEARCH");

  /* Stay without box until T_search from SEARCH re-entry at 2.5 */
  ap_mission_sm_tick(&sm, 2.5 + 5.0, &in);
  expect_state(&sm, AP_MISSION_ABORT, "SEARCH timeout → ABORT");
}

/* FTS latch + outputs; reset only exit. */
static void test_fts_latch(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_init(&sm, NULL);

  ap_mission_sm_inputs_t in = base_in();
  ap_mission_sm_tick(&sm, 0.0, &in);
  ap_mission_sm_handle_command(&sm, "FTS");
  expect_state(&sm, AP_MISSION_FTS, "FTS cmd");

  ap_mission_sm_fts_outputs_t fts;
  ap_mission_sm_fts_outputs(&sm, &fts);
  expect_true(fts.throttle_idle && fts.surfaces_fixed && fts.fuel_cut_relay,
              "FTS outputs idle+surfaces+fuel-cut");

  ap_mission_sm_handle_command(&sm, "WORK");
  ap_mission_sm_handle_command(&sm, "ABORT");
  ap_mission_sm_handle_command(&sm, "RTB");
  ap_mission_sm_tick(&sm, 1.0, &in);
  expect_state(&sm, AP_MISSION_FTS, "FTS stays latched");

  ap_mission_sm_reset(&sm);
  expect_state(&sm, AP_MISSION_BOOT, "reset → BOOT (sim power cycle)");
}

/* No WORK in LOCK/CLOSE. */
static void test_no_work_in_lock_close(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_config_t cfg;
  ap_mission_sm_config_defaults(&cfg);
  ap_mission_sm_init(&sm, &cfg);

  ap_mission_sm_inputs_t in = base_in();
  ap_mission_sm_tick(&sm, 0.0, &in);
  ap_mission_sm_handle_command(&sm, "WORK");
  ap_mission_sm_tick(&sm, 0.1, &in);

  in.track_box_valid = 1;
  ap_mission_sm_tick(&sm, 0.2, &in);
  expect_state(&sm, AP_MISSION_LOCK, "in LOCK");

  ap_mission_sm_handle_command(&sm, "WORK");
  ap_mission_sm_tick(&sm, 0.3, &in);
  expect_state(&sm, AP_MISSION_LOCK, "WORK ignored in LOCK");

  in.geometry_ok_for_close = 1;
  ap_mission_sm_tick(&sm, 0.4, &in);
  expect_state(&sm, AP_MISSION_CLOSE, "geometry → CLOSE");

  ap_mission_sm_handle_command(&sm, "WORK");
  ap_mission_sm_tick(&sm, 0.5, &in);
  expect_state(&sm, AP_MISSION_CLOSE, "WORK ignored in CLOSE");
}

/* Happy path miss-hold → ABORT; hard_fail → FTS; BOOT BIT timeout. */
static void test_miss_hold_and_hard_fail(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_config_t cfg;
  ap_mission_sm_config_defaults(&cfg);
  cfg.t_boot_s = 5.0;
  ap_mission_sm_init(&sm, &cfg);

  ap_mission_sm_inputs_t in = base_in();
  ap_mission_sm_tick(&sm, 0.0, &in);
  ap_mission_sm_handle_command(&sm, "WORK");
  ap_mission_sm_tick(&sm, 0.1, &in);
  in.track_box_valid = 1;
  ap_mission_sm_tick(&sm, 0.2, &in);
  in.geometry_ok_for_close = 1;
  ap_mission_sm_tick(&sm, 0.3, &in);
  expect_state(&sm, AP_MISSION_CLOSE, "CLOSE");
  in.miss_hold_done = 1;
  ap_mission_sm_tick(&sm, 0.4, &in);
  expect_state(&sm, AP_MISSION_ABORT, "miss hold → ABORT");
  expect_true(!ap_mission_sm_land_soft_active(&sm),
              "miss hold without test_recover → no LAND_SOFT");

  /* Re-enter SEARCH via WORK after clear */
  in.miss_hold_done = 0;
  in.track_box_valid = 0;
  in.geometry_ok_for_close = 0;
  ap_mission_sm_handle_command(&sm, "WORK");
  ap_mission_sm_tick(&sm, 0.5, &in);
  expect_state(&sm, AP_MISSION_SEARCH, "ABORT+WORK → SEARCH");

  in.hard_fail = 1;
  ap_mission_sm_tick(&sm, 0.6, &in);
  expect_state(&sm, AP_MISSION_FTS, "hard_fail → FTS");

  /* BOOT BIT timeout */
  ap_mission_sm_reset(&sm);
  in = base_in();
  in.bit_ok = 0;
  ap_mission_sm_tick(&sm, 0.0, &in);
  ap_mission_sm_tick(&sm, 5.0, &in);
  expect_state(&sm, AP_MISSION_ABORT, "BOOT BIT timeout → ABORT");
}

/* RTB ignores WORK/ABORT; state names match schema. */
static void test_rtb_and_names(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_init(&sm, NULL);
  ap_mission_sm_inputs_t in = base_in();
  ap_mission_sm_tick(&sm, 0.0, &in);
  ap_mission_sm_handle_command(&sm, "WORK");
  ap_mission_sm_tick(&sm, 0.1, &in);
  ap_mission_sm_handle_command(&sm, "RTB");
  expect_state(&sm, AP_MISSION_RTB, "RTB cmd");
  ap_mission_sm_handle_command(&sm, "WORK");
  ap_mission_sm_handle_command(&sm, "ABORT");
  ap_mission_sm_tick(&sm, 0.2, &in);
  expect_state(&sm, AP_MISSION_RTB, "WORK/ABORT ignored in RTB");

  static const char *names[] = {"BOOT", "SEARCH", "LOCK", "CLOSE",
                                "ABORT", "RTB", "FTS"};
  for (int i = 0; i <= 6; i++) {
    sm.state = (ap_mission_state_t)i;
    expect_true(strcmp(ap_mission_sm_state_name(&sm), names[i]) == 0,
                names[i]);
  }
  expect_true(strcmp(AP_MISSION_STATE_NAME, "MISSION_STATE") == 0,
              "MISSION_STATE name");
  expect_true(AP_LOST_LINK_RTB_S == 3u, "AP_LOST_LINK_RTB_S == 3");
}

/* operator_lock_request SEARCH→LOCK with box+corridor. */
static void test_operator_lock_ok(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_config_t cfg;
  ap_mission_sm_config_defaults(&cfg);
  cfg.t_search_s = 120.0;
  ap_mission_sm_init(&sm, &cfg);

  ap_mission_sm_inputs_t in = base_in();
  double t = 0.0;
  to_search(&sm, &in, &t);
  expect_state(&sm, AP_MISSION_SEARCH, "op-lock precondition SEARCH");

  in.track_box_valid = 1;
  in.operator_lock_request = 1;
  ap_mission_sm_tick(&sm, t, &in);
  expect_state(&sm, AP_MISSION_LOCK, "operator_lock → LOCK");
  expect_true(strcmp(ap_mission_sm_reason(&sm), "operator_lock") == 0,
              "reason operator_lock");
}

/* operator_lock ignored without box / not in SEARCH / BOOT. */
static void test_operator_lock_ignored(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_config_t cfg;
  ap_mission_sm_config_defaults(&cfg);
  cfg.t_search_s = 120.0;
  ap_mission_sm_init(&sm, &cfg);

  ap_mission_sm_inputs_t in = base_in();
  double t = 0.0;

  /* BOOT: operator_lock ignored */
  in.operator_lock_request = 1;
  in.track_box_valid = 1;
  ap_mission_sm_tick(&sm, t, &in);
  t += 0.1;
  expect_state(&sm, AP_MISSION_BOOT, "operator_lock ignored in BOOT");

  in.operator_lock_request = 0;
  in.track_box_valid = 0;
  to_search(&sm, &in, &t);
  expect_state(&sm, AP_MISSION_SEARCH, "in SEARCH for ignore tests");

  /* SEARCH without box: ignore */
  in.operator_lock_request = 1;
  in.track_box_valid = 0;
  ap_mission_sm_tick(&sm, t, &in);
  t += 0.1;
  expect_state(&sm, AP_MISSION_SEARCH, "operator_lock ignored without box");

  /* Not in SEARCH (LOCK): operator_lock does not re-trigger / bypass */
  in.track_box_valid = 1;
  in.operator_lock_request = 0;
  ap_mission_sm_tick(&sm, t, &in);
  t += 0.1;
  expect_state(&sm, AP_MISSION_LOCK, "auto → LOCK");
  in.operator_lock_request = 1;
  ap_mission_sm_tick(&sm, t, &in);
  expect_state(&sm, AP_MISSION_LOCK, "operator_lock ignored when not SEARCH");
}

/* TEST_RECOVER armed: CLOSE miss → LAND_SOFT; lost-link does NOT → RTB. */
static void test_test_recover_land_soft(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_config_t cfg;
  ap_mission_sm_config_defaults(&cfg);
  cfg.t_search_s = 120.0;
  ap_mission_sm_init(&sm, &cfg);

  ap_mission_sm_inputs_t in = base_in();
  in.test_recover = 1; /* armed from GCS before WORK */
  double t = 0.0;
  to_search(&sm, &in, &t);
  expect_state(&sm, AP_MISSION_SEARCH, "TEST_RECOVER SEARCH");
  expect_true(sm.test_recover_armed == 1, "test_recover latched at SEARCH");

  /* Mid-CLOSE flip of test_recover must not disarm latch */
  to_close(&sm, &in, &t);
  expect_state(&sm, AP_MISSION_CLOSE, "TEST_RECOVER CLOSE");
  in.test_recover = 0;
  in.miss_hold_done = 1;
  ap_mission_sm_tick(&sm, t, &in);
  t += 0.1;
  expect_state(&sm, AP_MISSION_ABORT, "miss → ABORT LAND_SOFT");
  expect_true(ap_mission_sm_land_soft_active(&sm), "land_soft_active=1");
  expect_true(strcmp(ap_mission_sm_reason(&sm), "LAND_SOFT") == 0,
              "reason LAND_SOFT");

  /* Lost-link during LAND_SOFT must NOT → RTB */
  in.heartbeat_ok = 0;
  in.miss_hold_done = 0;
  ap_mission_sm_tick(&sm, t, &in);
  t += 4.0; /* > T_link */
  ap_mission_sm_tick(&sm, t, &in);
  expect_state(&sm, AP_MISSION_ABORT, "LAND_SOFT lost-link stays ABORT");
  expect_true(ap_mission_sm_land_soft_active(&sm),
              "land_soft still active after lost-link");
}

/* TEST_RECOVER off: lost-link still → RTB as before. */
static void test_test_recover_off_lost_link(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_config_t cfg;
  ap_mission_sm_config_defaults(&cfg);
  cfg.t_search_s = 120.0;
  ap_mission_sm_init(&sm, &cfg);

  ap_mission_sm_inputs_t in = base_in();
  in.test_recover = 0;
  double t = 0.0;
  to_search(&sm, &in, &t);
  to_close(&sm, &in, &t);
  in.miss_hold_done = 1;
  ap_mission_sm_tick(&sm, t, &in);
  t += 0.1;
  expect_state(&sm, AP_MISSION_ABORT, "miss → plain ABORT");
  expect_true(!ap_mission_sm_land_soft_active(&sm), "no LAND_SOFT");

  in.miss_hold_done = 0;
  in.heartbeat_ok = 0;
  ap_mission_sm_tick(&sm, t, &in);
  t += 4.0;
  ap_mission_sm_tick(&sm, t, &in);
  expect_state(&sm, AP_MISSION_RTB, "TEST_RECOVER off lost-link → RTB");
}

/* Explicit RTB cmd during LAND_SOFT → RTB OK. */
static void test_rtb_overrides_land_soft(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_config_t cfg;
  ap_mission_sm_config_defaults(&cfg);
  cfg.t_search_s = 120.0;
  ap_mission_sm_init(&sm, &cfg);

  ap_mission_sm_inputs_t in = base_in();
  in.test_recover = 1;
  double t = 0.0;
  to_search(&sm, &in, &t);
  to_close(&sm, &in, &t);
  in.miss_hold_done = 1;
  ap_mission_sm_tick(&sm, t, &in);
  t += 0.1;
  expect_true(ap_mission_sm_land_soft_active(&sm), "precondition LAND_SOFT");

  in.miss_hold_done = 0;
  ap_mission_sm_handle_command(&sm, "RTB");
  expect_state(&sm, AP_MISSION_RTB, "explicit RTB overrides LAND_SOFT");
  expect_true(!ap_mission_sm_land_soft_active(&sm),
              "land_soft cleared on RTB");
}


/* Operator BDA: TARGET_DESTROYED from CLOSE → RTB; land_soft stays 0. */
static void test_bda_destroyed_rtb(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_config_t cfg;
  ap_mission_sm_config_defaults(&cfg);
  cfg.t_search_s = 120.0;
  ap_mission_sm_init(&sm, &cfg);

  ap_mission_sm_inputs_t in = base_in();
  double t = 0.0;
  to_search(&sm, &in, &t);
  to_close(&sm, &in, &t);
  expect_state(&sm, AP_MISSION_CLOSE, "BDA destroyed precondition CLOSE");

  expect_true(ap_mission_sm_handle_command(&sm, AP_UPLINK_NAME_TARGET_DESTROYED) == 0,
              "TARGET_DESTROYED accepted");
  expect_state(&sm, AP_MISSION_RTB, "TARGET_DESTROYED → RTB");
  expect_true(!ap_mission_sm_land_soft_active(&sm),
              "TARGET_DESTROYED land_soft_active=0");
  expect_true(sm.engagement_cleared == 1, "TARGET_DESTROYED engagement cleared");
}

/* BDA destroyed with test_recover armed still → RTB (not LAND_SOFT). */
static void test_bda_destroyed_not_land_soft(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_config_t cfg;
  ap_mission_sm_config_defaults(&cfg);
  cfg.t_search_s = 120.0;
  ap_mission_sm_init(&sm, &cfg);

  ap_mission_sm_inputs_t in = base_in();
  in.test_recover = 1;
  double t = 0.0;
  to_search(&sm, &in, &t);
  expect_true(sm.test_recover_armed == 1, "test_recover armed for BDA");
  to_close(&sm, &in, &t);
  expect_state(&sm, AP_MISSION_CLOSE, "BDA+recover CLOSE");

  ap_mission_sm_handle_command(&sm, AP_UPLINK_NAME_TARGET_DESTROYED);
  expect_state(&sm, AP_MISSION_RTB, "BDA destroyed → RTB not LAND_SOFT");
  expect_true(!ap_mission_sm_land_soft_active(&sm),
              "BDA destroyed never LAND_SOFT");
}

/* MISS/REATTACK from CLOSE → ABORT cleared; WORK can re-enter SEARCH. */
static void test_bda_miss_reattack_abort_recue(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_config_t cfg;
  ap_mission_sm_config_defaults(&cfg);
  cfg.t_search_s = 120.0;
  ap_mission_sm_init(&sm, &cfg);

  ap_mission_sm_inputs_t in = base_in();
  double t = 0.0;
  to_search(&sm, &in, &t);
  to_close(&sm, &in, &t);

  expect_true(ap_mission_sm_handle_command(&sm, "MISS/REATTACK") == 0,
              "MISS/REATTACK accepted");
  expect_state(&sm, AP_MISSION_ABORT, "MISS/REATTACK → ABORT");
  expect_true(sm.engagement_cleared == 1, "MISS/REATTACK engagement cleared");
  expect_true(!ap_mission_sm_land_soft_active(&sm), "MISS/REATTACK no LAND_SOFT");

  in.track_box_valid = 0;
  in.geometry_ok_for_close = 0;
  ap_mission_sm_handle_command(&sm, "WORK");
  ap_mission_sm_tick(&sm, t, &in);
  expect_state(&sm, AP_MISSION_SEARCH, "MISS/REATTACK then WORK → SEARCH re-cue");
}

/* MISS/REATTACK does not declare kill / does not go FTS. */
static void test_bda_miss_reattack_not_kill_fts(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_config_t cfg;
  ap_mission_sm_config_defaults(&cfg);
  cfg.t_search_s = 120.0;
  ap_mission_sm_init(&sm, &cfg);

  ap_mission_sm_inputs_t in = base_in();
  double t = 0.0;
  to_search(&sm, &in, &t);
  to_close(&sm, &in, &t);
  ap_mission_sm_handle_command(&sm, AP_UPLINK_NAME_MISS_REATTACK);
  expect_state(&sm, AP_MISSION_ABORT, "MISS/REATTACK stays ABORT not FTS");
  expect_true(ap_mission_sm_state(&sm) != AP_MISSION_FTS, "MISS/REATTACK not FTS");

  ap_mission_sm_fts_outputs_t fts;
  ap_mission_sm_fts_outputs(&sm, &fts);
  expect_true(!fts.throttle_idle && !fts.fuel_cut_relay,
              "MISS/REATTACK does not assert FTS outputs");
}

/* Unknown cmd still -1; BDA macros match stamped freeze. */
static void test_bda_unknown_and_macros(void) {
  ap_mission_sm_t sm;
  ap_mission_sm_init(&sm, NULL);
  ap_mission_sm_inputs_t in = base_in();
  ap_mission_sm_tick(&sm, 0.0, &in);

  expect_true(ap_mission_sm_handle_command(&sm, "NOT_A_CMD") == -1,
              "unknown cmd → -1");
  expect_true(AP_UPLINK_TARGET_DESTROYED == 3u, "AP_UPLINK_TARGET_DESTROYED==3");
  expect_true(AP_UPLINK_MISS_REATTACK == 4u, "AP_UPLINK_MISS_REATTACK==4");
  expect_true(strcmp(AP_UPLINK_NAME_TARGET_DESTROYED, "TARGET_DESTROYED") == 0,
              "NAME TARGET_DESTROYED");
  expect_true(strcmp(AP_UPLINK_NAME_MISS_REATTACK, "MISS/REATTACK") == 0,
              "NAME MISS/REATTACK");
  /* Separate MISS / REATTACK / param1=5 must not exist. */
  expect_true(ap_mission_sm_handle_command(&sm, "MISS") == -1,
              "bare MISS unknown");
  expect_true(ap_mission_sm_handle_command(&sm, "REATTACK") == -1,
              "bare REATTACK unknown");
}

int main(void) {
  g_fail = 0;
  test_work_only_search_entry();
  test_lost_link_rtb();
  test_lost_track_search_abort();
  test_fts_latch();
  test_no_work_in_lock_close();
  test_miss_hold_and_hard_fail();
  test_rtb_and_names();
  test_operator_lock_ok();
  test_operator_lock_ignored();
  test_test_recover_land_soft();
  test_test_recover_off_lost_link();
  test_rtb_overrides_land_soft();
  test_bda_destroyed_rtb();
  test_bda_destroyed_not_land_soft();
  test_bda_miss_reattack_abort_recue();
  test_bda_miss_reattack_not_kill_fts();
  test_bda_unknown_and_macros();
  if (g_fail) {
    fprintf(stderr, "\nSOME TESTS FAILED\n");
    return 1;
  }
  printf("\nALL TESTS PASSED\n");
  return 0;
}
