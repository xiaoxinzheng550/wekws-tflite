# PortAudio v19.7.0 still declares compatibility with very old CMake releases.
# CMake 4 removed that compatibility unless a minimum policy version is supplied.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

FetchContent_Declare(portaudio
  GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
  GIT_TAG        v19.7.0
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(portaudio)
