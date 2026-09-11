cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(wekws_tflite_micro VERSION 0.1.0 LANGUAGES C CXX)

option(WEKWS_BUILD_STREAM "Build the live microphone demo" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-O3)
add_compile_definitions(TF_LITE_STATIC_MEMORY)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

if(APPLE AND WEKWS_BUILD_STREAM)
  include(portaudio)
endif()
include(tflite_micro)

add_subdirectory(frontend)
add_subdirectory(kws)
add_subdirectory(bin)
