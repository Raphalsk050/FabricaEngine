#!/usr/bin/env bash
set -euo pipefail

CONFIG="Debug"
BUILD_DIR="engine/build-linux"
BUILD_TESTS="ON"
BUILD_SAMPLES="OFF"
GLFW_BACKEND="ON"
FETCH_GLFW="OFF"
TARGET=""
CLEAN="OFF"
PARALLEL=""

print_help() {
  cat <<EOF
Usage: scripts/build_linux.sh [options]

Options:
  --config <Debug|Release|RelWithDebInfo|MinSizeRel>
  --build-dir <path>
  --no-tests
  --build-samples
  --disable-glfw-backend
  --fetch-glfw
  --target <cmake-target>
  --parallel <n>
  --clean
  --help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --config)
      CONFIG="$2"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --no-tests)
      BUILD_TESTS="OFF"
      shift
      ;;
    --build-samples)
      BUILD_SAMPLES="ON"
      shift
      ;;
    --disable-glfw-backend)
      GLFW_BACKEND="OFF"
      shift
      ;;
    --fetch-glfw)
      FETCH_GLFW="ON"
      shift
      ;;
    --target)
      TARGET="$2"
      shift 2
      ;;
    --parallel)
      PARALLEL="$2"
      shift 2
      ;;
    --clean)
      CLEAN="ON"
      shift
      ;;
    --help)
      print_help
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      print_help
      exit 2
      ;;
  esac
done

if [[ "$CLEAN" == "ON" && -d "$BUILD_DIR" ]]; then
  rm -rf "$BUILD_DIR"
fi

configure_args=(
  -S engine
  -B "$BUILD_DIR"
  -DFABRICA_BUILD_TESTS="$BUILD_TESTS"
  -DFABRICA_BUILD_SAMPLES="$BUILD_SAMPLES"
  -DFABRICA_ENABLE_GLFW_WINDOW_BACKEND="$GLFW_BACKEND"
  -DFABRICA_FETCH_GLFW="$FETCH_GLFW"
  -DCMAKE_BUILD_TYPE="$CONFIG"
)

echo "[build_linux] Configure: cmake ${configure_args[*]}"
cmake "${configure_args[@]}"

build_args=(--build "$BUILD_DIR")
if [[ -n "$PARALLEL" ]]; then
  build_args+=(--parallel "$PARALLEL")
else
  build_args+=(--parallel)
fi

if [[ -n "$TARGET" ]]; then
  build_args+=(--target "$TARGET")
fi

echo "[build_linux] Build: cmake ${build_args[*]}"
cmake "${build_args[@]}"

if [[ "$BUILD_TESTS" == "ON" ]]; then
  test_args=(--test-dir "$BUILD_DIR" --output-on-failure)
  echo "[build_linux] Test: ctest ${test_args[*]}"
  ctest "${test_args[@]}"
fi

echo "[build_linux] Done"