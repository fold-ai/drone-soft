# Tracking → Navigation / Safety (V1)
# measurement_epoch = PPS-domain time of frames that formed the update.
# Coast updates: keep epoch of last REAL measurement (not publish time).
# t_pps is the historical alias; both MUST be equal when both set.
std_msgs/Header header
uint32 track_id
float64 measurement_epoch
float64 t_pps
float64 t_publish_pps
uint8 state
uint8 lock_state
uint8 cam_id
float32 u
float32 v
float32 bbox_w
float32 bbox_h
float32 range_est_m
float32[2] bearing_ned_rad
float32 lock_quality
uint32 class_id
uint32 age_updates
float32 time_since_meas_s
uint8 handoff_phase
uint8 cue_source
bool onboard_owns
bool operator_lock_req
