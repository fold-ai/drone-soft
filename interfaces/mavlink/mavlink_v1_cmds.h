#pragma once
/* V1 C2 uplink — common.xml MAVLink v1 only. Names frozen with #safety: no aliases. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MAV_CMD from common.xml */
#define AP_MAV_CMD_NAV_RETURN_TO_LAUNCH   20u
#define AP_MAV_CMD_DO_SET_MODE            176u
#define AP_MAV_CMD_DO_FLIGHTTERMINATION   185u
#define AP_MAV_CMD_USER_1                 31000u

/* ArduCopter custom_mode values (SET_MODE / DO_SET_MODE param2) */
#define AP_COPTER_MODE_STABILIZE  0u
#define AP_COPTER_MODE_ALT_HOLD   2u
#define AP_COPTER_MODE_GUIDED     4u
#define AP_COPTER_MODE_LOITER     5u
#define AP_COPTER_MODE_RTL        6u
#define AP_COPTER_MODE_LAND       9u

#define AP_MAV_MODE_FLAG_CUSTOM_MODE_ENABLED 1u
#define AP_MAV_MODE_FLAG_SAFETY_ARMED        128u

#define AP_MSG_SET_MODE              11u
#define AP_MSG_RC_CHANNELS           65u
#define AP_MSG_REQUEST_DATA_STREAM   66u
#define AP_MAV_DATA_STREAM_RC_CHANNELS 3u
#define AP_MAV_DATA_STREAM_EXTRA1      10u
#define AP_MAV_FRAME_LOCAL_NED         1u
#define AP_MAV_FRAME_BODY_NED          8u
#define AP_MAV_FRAME_BODY_OFFSET_NED   9u
/* Ignore pos + accel + yaw + yaw_rate; send body velocity only. */
#define AP_TYPEMASK_BODY_VEL          0x0DC7u

/* COMMAND_LONG param1 when command == MAV_CMD_USER_1 */
#define AP_UPLINK_WORK              1u  /* enter/continue mission */
#define AP_UPLINK_ABORT             2u  /* end engagement only — not FTS */
#define AP_UPLINK_TARGET_DESTROYED  3u  /* BDA: clear engagement → RTB (not LAND_SOFT) */
#define AP_UPLINK_MISS_REATTACK     4u  /* BDA: clear CLOSE; allow WORK / re-cue */
#define AP_UPLINK_MISS              AP_UPLINK_MISS_REATTACK  /* bridge send_miss() */
#define AP_UPLINK_REATTACK          AP_UPLINK_MISS_REATTACK  /* bridge send_reattack(); V1 same as miss — no param1=5 */

/* Frozen uplink names (documentation / UI / logs — use these strings) */
#define AP_UPLINK_NAME_WORK              "WORK"
#define AP_UPLINK_NAME_ABORT             "ABORT"
#define AP_UPLINK_NAME_RTB               "RTB"
#define AP_UPLINK_NAME_FTS               "FTS"
#define AP_UPLINK_NAME_TARGET_DESTROYED  "TARGET_DESTROYED"
#define AP_UPLINK_NAME_MISS_REATTACK     "MISS/REATTACK"
#define AP_UPLINK_NAME_MISS              "MISS/REATTACK"  /* UX "MISS" → same frozen string */
#define AP_UPLINK_NAME_REATTACK          "MISS/REATTACK"  /* UX "REATTACK" → same frozen string */

/* SET_POSITION_TARGET_LOCAL_NED (84) — heading + climb / commanded miss */
#define AP_MSG_SET_POSITION_TARGET_LOCAL_NED 84u
/* Typical type_mask: ignore pos, accel, force, yaw_rate — use vx,vy,vz,yaw (confirm PX4 mask bits in bridge). */

/* Lost-link: seconds without Orin HEARTBEAT before FC RTB */
#define AP_LOST_LINK_RTB_S 3u

static inline uint16_t ap_cmd_for_uplink_rtb(void) {
  return (uint16_t)AP_MAV_CMD_NAV_RETURN_TO_LAUNCH;
}

static inline uint16_t ap_cmd_for_uplink_fts(void) {
  return (uint16_t)AP_MAV_CMD_DO_FLIGHTTERMINATION;
}

#ifdef __cplusplus
}
#endif
