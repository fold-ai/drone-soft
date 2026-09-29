cmake_minimum_required(VERSION 3.18)
project(actprove_host_contract LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(host_chain_smoke
  host_chain_smoke.cpp
  ../../tracking/src/tracker.cpp
  ../../navigation/src/heading_guidance.cpp
  ../../navigation/src/miss_geometry.cpp
  ../../onboard/safety_gates/src/mission_sm.c
)
target_include_directories(host_chain_smoke PRIVATE
  ../../interfaces/include
  ../../interfaces/mavlink
  ../../tracking/include
  ../../navigation/include
  ../../onboard/safety_gates/include
)
target_compile_options(host_chain_smoke PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME host_chain_smoke COMMAND host_chain_smoke)
