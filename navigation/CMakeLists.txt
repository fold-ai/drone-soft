cmake_minimum_required(VERSION 3.16)
project(actprove_navigation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(navigation
  src/heading_guidance.cpp
  src/miss_geometry.cpp
  src/soft_land_profile.cpp
  src/rtb_profile.cpp
  src/cue_heading.cpp
  src/bda_exit.cpp
)
target_include_directories(navigation PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../interfaces/include
)
target_compile_options(navigation PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_executable(navigation_smoke tests/smoke_test.cpp)
target_link_libraries(navigation_smoke PRIVATE navigation)
add_test(NAME navigation_smoke COMMAND navigation_smoke)
