cmake_minimum_required(VERSION 3.18)
project(actprove_onboard LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The C mission_sm is the single safety-policy implementation.  The former C++
# sketch is intentionally not linked into the documented onboard build.
add_library(safety_gates
  safety_gates/src/mission_sm.c
)
target_include_directories(safety_gates PUBLIC
  safety_gates/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../interfaces/mavlink
)
target_compile_options(safety_gates PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_executable(test_mission_sm safety_gates/tests/test_mission_sm.c)
target_link_libraries(test_mission_sm PRIVATE safety_gates)
add_test(NAME test_mission_sm COMMAND test_mission_sm)

add_library(mavlink_bridge
  mavlink_bridge/src/bridge.cpp
)
target_include_directories(mavlink_bridge PUBLIC
  mavlink_bridge/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../interfaces/mavlink
)

add_executable(mavlink_bridge_smoke
  mavlink_bridge/src/main_smoke.cpp
  mavlink_bridge/src/bridge.cpp
)
target_include_directories(mavlink_bridge_smoke PRIVATE mavlink_bridge/include)

add_executable(test_mavlink_parser mavlink_bridge/tests/test_mavlink_parser.cpp)
target_link_libraries(test_mavlink_parser PRIVATE mavlink_bridge)
add_test(NAME test_mavlink_parser COMMAND test_mavlink_parser)

add_library(time_sync
  time_sync/src/pps_ingest.cpp
)
target_include_directories(time_sync PUBLIC time_sync/include)
target_link_libraries(time_sync PUBLIC pthread)

add_library(logging
  logging/src/blackbox.cpp
)
target_include_directories(logging PUBLIC logging/include)

add_executable(logger_smoke logging/src/logger_smoke_main.cpp logging/src/blackbox.cpp)
target_include_directories(logger_smoke PRIVATE logging/include)

add_library(companion_core
  companion/src/rc_gates.cpp
  companion/src/image_homing.cpp
  companion/src/app.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../tracking/src/tracker.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../vision/iface/src/detection_ipc.cpp
)
target_include_directories(companion_core PUBLIC
  companion/include
  mavlink_bridge/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../interfaces/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../interfaces/mavlink
  ${CMAKE_CURRENT_SOURCE_DIR}/../tracking/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../vision/iface/include
)
target_link_libraries(companion_core PUBLIC mavlink_bridge)
target_compile_options(companion_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(actprove_companion companion/src/main.cpp)
target_link_libraries(actprove_companion PRIVATE companion_core)

add_executable(test_rc_gates companion/tests/test_rc_gates.cpp)
target_link_libraries(test_rc_gates PRIVATE companion_core)
add_test(NAME test_rc_gates COMMAND test_rc_gates)

add_executable(test_image_homing companion/tests/test_image_homing.cpp)
target_link_libraries(test_image_homing PRIVATE companion_core)
add_test(NAME test_image_homing COMMAND test_image_homing)

add_executable(test_companion_app companion/tests/test_companion_app.cpp)
target_link_libraries(test_companion_app PRIVATE companion_core)
add_test(NAME test_companion_app COMMAND test_companion_app)
