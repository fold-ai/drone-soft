cmake_minimum_required(VERSION 3.16)
project(safety_gates C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Repo interfaces/mavlink (mission_state.h, mavlink_v1_cmds.h)
set(AP_INTERFACES_MAVLINK "${CMAKE_CURRENT_SOURCE_DIR}/../../interfaces/mavlink")

add_library(safety_gates
  src/mission_sm.c
)
target_include_directories(safety_gates
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${AP_INTERFACES_MAVLINK}
)

enable_testing()
add_executable(test_mission_sm tests/test_mission_sm.c)
target_link_libraries(test_mission_sm PRIVATE safety_gates)
add_test(NAME test_mission_sm COMMAND test_mission_sm)
