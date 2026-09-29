# FindTensorRT.cmake — JetPack TensorRT
find_path(TENSORRT_INCLUDE_DIR NvInfer.h
  HINTS /usr/include/x86_64-linux-gnu /usr/include/aarch64-linux-gnu /usr/include)
find_library(TENSORRT_LIBRARY nvinfer
  HINTS /usr/lib/aarch64-linux-gnu /usr/lib/x86_64-linux-gnu)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TensorRT DEFAULT_MSG TENSORRT_INCLUDE_DIR TENSORRT_LIBRARY)
if(TensorRT_FOUND AND NOT TARGET TensorRT::NvInfer)
  add_library(TensorRT::NvInfer UNKNOWN IMPORTED)
  set_target_properties(TensorRT::NvInfer PROPERTIES
    IMPORTED_LOCATION "${TENSORRT_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${TENSORRT_INCLUDE_DIR}")
endif()
