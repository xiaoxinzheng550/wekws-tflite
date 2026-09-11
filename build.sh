#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
default_build_dir="${project_dir}/build"
build_dir="${BUILD_DIR:-${default_build_dir}}"
portaudio_dir="${project_dir}/third_party/portaudio"
build_mode="all"

if [[ "${1:-}" == "clean" ]]; then
  if [[ "$#" -ne 1 ]]; then
    echo "Usage: ./build.sh clean" >&2
    echo "Optional: BUILD_DIR=/absolute/path ./build.sh clean" >&2
    exit 2
  fi
  if [[ -e "${build_dir}" ]]; then
    resolved_build_dir="$(realpath "${build_dir}")"
    case "${resolved_build_dir}" in
      /|"${project_dir}")
        echo "Error: refusing to remove unsafe build directory: ${resolved_build_dir}" >&2
        exit 2
        ;;
    esac
    echo "Removing generated build directory: ${resolved_build_dir}"
    cmake -E remove_directory "${resolved_build_dir}"
  else
    echo "Build directory does not exist; nothing to clean: ${build_dir}"
  fi
  exit 0
fi

case "${1:-}" in
  kws)
    build_mode="kws"
    shift
    ;;
  kws_stream)
    build_mode="kws_stream"
    shift
    ;;
  ""|-*)
    ;;
  *)
    echo "Usage: ./build.sh [kws|kws_stream|clean] [CMake options...]" >&2
    exit 2
    ;;
esac

if [[ "${build_mode}" == "kws" ]]; then
  stream_enabled=false
else
  stream_enabled=true
fi

# CMake caches absolute source paths. If the repository was moved together
# with an old build directory, discard only the default generated directory
# and configure it again at the new location.
cache_file="${build_dir}/CMakeCache.txt"
if [[ -f "${cache_file}" ]]; then
  cached_source="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "${cache_file}" | head -n 1)"
  if [[ -n "${cached_source}" && "${cached_source}" != "${project_dir}" ]]; then
    if [[ "${build_dir}" != "${default_build_dir}" ]]; then
      echo "Error: BUILD_DIR contains a CMake cache for another source tree:" >&2
      echo "  cached:  ${cached_source}" >&2
      echo "  current: ${project_dir}" >&2
      echo "Choose a new BUILD_DIR or remove that generated directory." >&2
      exit 1
    fi
    echo "Source path changed: ${cached_source} -> ${project_dir}"
    echo "Recreating generated build directory: ${build_dir}"
    cmake -E remove_directory "${build_dir}"
  fi
fi

for arg in "$@"; do
  case "${arg}" in
    -DWEKWS_PORTAUDIO_SOURCE_DIR=*)
      portaudio_dir="${arg#*=}"
      ;;
  esac
done

if [[ "${stream_enabled}" == true && ! -f "${portaudio_dir}/CMakeLists.txt" ]]; then
  echo "Error: local PortAudio source not found: ${portaudio_dir}" >&2
  echo "This build script never downloads PortAudio." >&2
  echo "Restore third_party/portaudio or pass -DWEKWS_PORTAUDIO_SOURCE_DIR=/absolute/path." >&2
  exit 1
fi

if [[ "${stream_enabled}" == true ]]; then
  cmake -S "${project_dir}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DWEKWS_PORTAUDIO_SOURCE_DIR="${portaudio_dir}" \
    "$@" \
    -DWEKWS_BUILD_STREAM=ON
else
  cmake -S "${project_dir}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    "$@" \
    -DWEKWS_BUILD_STREAM=OFF
fi

case "${build_mode}" in
  kws)
    cmake --build "${build_dir}" --target kws_main --parallel
    ;;
  kws_stream)
    cmake --build "${build_dir}" --target stream_kws_main --parallel
    ;;
  all)
    cmake --build "${build_dir}" --parallel
    ;;
esac

echo "Built binaries under ${build_dir}/bin"
