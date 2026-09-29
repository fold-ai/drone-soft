#pragma once
/* Physical C2 / companion map — V1. Video is not on these UARTs. */

#ifdef __cplusplus
extern "C" {
#endif

/* Pixhawk / Cube class */
#define AP_TELEM1_ROLE        "RFD900x_C2"   /* 900 MHz air↔GCS */
#define AP_TELEM2_ROLE        "ORIN_UART"    /* onboard MAVLink peer */

#define AP_TELEM_BAUD         57600u
#define AP_RFD900_AIR_BPS     64000u         /* ~64 kbps air data rate */

#define AP_SYSID_VEHICLE      1u
#define AP_COMPID_ORIN        191u           /* MAV_COMP_ID_ONBOARD_COMPUTER */
#define AP_SYSID_GCS          255u

/* Near-field digital video: separate 1.2 or 2.4 GHz RF — not MAVLink, not TELEM* */

#ifdef __cplusplus
}
#endif
