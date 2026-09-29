cmake_minimum_required(VERSION 3.16)
project(actprove_tracking LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(actprove_tracking
  src/tracker.cpp
)
target_include_directories(actprove_tracking
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../interfaces/include
)

add_executable(tracking_smoke src/smoke_main.cpp)
target_link_libraries(tracking_smoke PRIVATE actprove_tracking)

add_executable(tracking_virtual_tests src/virtual_tests.cpp)
target_link_libraries(tracking_virtual_tests PRIVATE actprove_tracking)
