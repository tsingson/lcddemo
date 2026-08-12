#!/bin/bash
set -e

VENV_DIR="/Users/qinshen/go/zephyrproject/.venv"
PROJECT_DIR="/Users/qinshen/go/zephyrproject/lcddemo"
BUILD_DIR="$PROJECT_DIR/build_st7735s_lvgl"

# Clean
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Activate venv properly by setting PATH and VIRTUAL_ENV
export VIRTUAL_ENV="$VENV_DIR"
export PATH="$VENV_DIR/bin:$PATH"
export PYTHONHOME=""

cd "$PROJECT_DIR"

# Use west via python module
python -m west build \
  -b esp32_devkitc_esp32_procpu \
  -d "$BUILD_DIR" \
  -c DeveloperBuild \
  -- \
  -DSHIELD=esp32_devkitc_esp32_procpu_st7735s \
  -DCONFIG_FILE_SUFFIX=st7735s_lvgl

echo "Build complete: $BUILD_DIR"
