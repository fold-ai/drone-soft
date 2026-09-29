# Perception → Tracking (wire twin of DetectionMsg)
float64 t_pps
uint64 seq
uint8 cam_id
uint16 src_w
uint16 src_h
uint32 n
float32[] box_x
float32[] box_y
float32[] box_w
float32[] box_h
float32[] conf
uint32[] class_id
# n <= 32; arrays length == n
