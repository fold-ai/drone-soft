cmake_minimum_required(VERSION 3.18)
project(actprove_vision LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SOFT_NO_JETPACK "Build structural stubs without CUDA/Argus/TRT" ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_library(vision_capture
  capture/src/argus_capture.cpp
  capture/src/v4l2_capture.cpp
  capture/src/dual_camera_capture.cpp
  capture/src/ground_eo_capture.cpp
  capture/src/pps_stamp.cpp
)
target_include_directories(vision_capture PUBLIC capture/include)
target_compile_definitions(vision_capture PUBLIC $<$<BOOL:${SOFT_NO_JETPACK}>:SOFT_NO_JETPACK=1>)

add_library(vision_detect
  detect/src/detector.cpp
  detect/src/trt_engine.cpp
)
target_include_directories(vision_detect PUBLIC detect/include capture/include)
target_compile_definitions(vision_detect PUBLIC $<$<BOOL:${SOFT_NO_JETPACK}>:SOFT_NO_JETPACK=1>)

if(NOT SOFT_NO_JETPACK)
  enable_language(CUDA)
 find_package(CUDAToolkit REQUIRED)
  find_package(TensorRT REQUIRED)
  target_link_libraries(vision_detect PUBLIC TensorRT::NvInfer CUDA::cudart)
endif()

add_library(vision_iface
  iface/src/box_publisher.cpp
  iface/src/detection_ipc.cpp
)
target_include_directories(vision_iface PUBLIC
  iface/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../interfaces/include
)

enable_testing()
add_executable(test_box_publisher iface/tests/test_box_publisher.cpp)
target_link_libraries(test_box_publisher PRIVATE vision_iface)
add_test(NAME test_box_publisher COMMAND test_box_publisher)

add_executable(test_detection_ipc iface/tests/test_detection_ipc.cpp)
target_link_libraries(test_detection_ipc PRIVATE vision_iface)
add_test(NAME test_detection_ipc COMMAND test_detection_ipc)

add_executable(capture_bench apps/capture_bench/main.cpp)
target_link_libraries(capture_bench PRIVATE vision_capture)

add_executable(dual_capture_bench apps/dual_capture_bench/main.cpp)
target_link_libraries(dual_capture_bench PRIVATE vision_capture)

add_executable(detect_node apps/detect_node/main.cpp)
target_link_libraries(detect_node PRIVATE vision_capture vision_detect vision_iface)

add_executable(dual_detect_node apps/dual_detect_node/main.cpp)
target_link_libraries(dual_detect_node PRIVATE vision_capture vision_detect vision_iface)

add_executable(ground_eo_node apps/ground_eo_node/main.cpp)
target_link_libraries(ground_eo_node PRIVATE vision_capture vision_detect vision_iface)

add_executable(offline_replay apps/offline_replay/main.cpp)
target_link_libraries(offline_replay PRIVATE vision_detect vision_iface)

add_executable(detection_logger apps/detection_logger/main.cpp)
target_link_libraries(detection_logger PRIVATE vision_iface)
