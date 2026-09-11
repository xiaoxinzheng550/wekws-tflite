set(TFLM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tflm" CACHE PATH
    "TensorFlow Lite Micro headers and prebuilt libraries")
set(TFLM_INCLUDE_DIR "${TFLM_ROOT}/include" CACHE PATH
    "TensorFlow Lite Micro include directory")
set(TFLM_LIBRARY "" CACHE FILEPATH
    "Override the TensorFlow Lite Micro static library")

if(NOT TFLM_LIBRARY)
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" WEKWS_PROCESSOR)
  if(APPLE AND WEKWS_PROCESSOR MATCHES "^(arm64|aarch64)$")
    set(TFLM_LIBRARY
        "${TFLM_ROOT}/lib/macos-arm64/libtensorflow-microlite.a")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
         WEKWS_PROCESSOR MATCHES "^(x86_64|amd64)$")
    set(TFLM_LIBRARY
        "${TFLM_ROOT}/lib/linux-x86_64/libtensorflow-microlite.a")
  else()
    message(FATAL_ERROR
      "No bundled TFLM library for ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}. "
      "Pass -DTFLM_LIBRARY=/absolute/path/libtensorflow-microlite.a")
  endif()
endif()

if(NOT EXISTS "${TFLM_INCLUDE_DIR}/tensorflow/lite/micro/micro_interpreter.h")
  message(FATAL_ERROR "TFLM headers not found under ${TFLM_INCLUDE_DIR}")
endif()
if(NOT EXISTS "${TFLM_LIBRARY}")
  message(FATAL_ERROR "TFLM static library not found: ${TFLM_LIBRARY}")
endif()

add_library(tflite_micro STATIC IMPORTED GLOBAL)
set_target_properties(tflite_micro PROPERTIES IMPORTED_LOCATION "${TFLM_LIBRARY}")
target_include_directories(tflite_micro INTERFACE
  "${TFLM_INCLUDE_DIR}"
  "${TFLM_INCLUDE_DIR}/gemmlowp")
target_link_libraries(tflite_micro INTERFACE m pthread)

message(STATUS "TFLM include: ${TFLM_INCLUDE_DIR}")
message(STATUS "TFLM library: ${TFLM_LIBRARY}")
