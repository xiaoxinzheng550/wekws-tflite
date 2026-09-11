# PortAudio v19.7.0 still declares compatibility with very old CMake releases.
# CMake 4 removed that compatibility unless a minimum policy version is supplied.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

set(WEKWS_PORTAUDIO_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/portaudio" CACHE PATH
    "Path to a PortAudio source tree")

if(NOT EXISTS "${WEKWS_PORTAUDIO_SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "PortAudio source not found at ${WEKWS_PORTAUDIO_SOURCE_DIR}. "
    "Set -DWEKWS_PORTAUDIO_SOURCE_DIR=/absolute/path/to/portaudio")
endif()

set(PA_BUILD_STATIC ON CACHE BOOL "Build PortAudio static library" FORCE)
set(PA_BUILD_SHARED OFF CACHE BOOL "Build PortAudio shared library" FORCE)
set(PA_BUILD_TESTS OFF CACHE BOOL "Build PortAudio tests" FORCE)
set(PA_BUILD_EXAMPLES OFF CACHE BOOL "Build PortAudio examples" FORCE)

add_subdirectory(
  "${WEKWS_PORTAUDIO_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}/third_party/portaudio"
  EXCLUDE_FROM_ALL)
