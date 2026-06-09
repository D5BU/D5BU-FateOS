#!/bin/bash
set -euo pipefail
WORKSPACE_DIR="/mnt/c/Users/d5bu/OneDrive/Desktop/Projects/FATEOS"
BUILD_DIR="/tmp/fateos-build"
mkdir -p "${WORKSPACE_DIR}/dist"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"
wget -q --show-progress "https://busybox.net/downloads/busybox-1.36.1.tar.bz2"

tar -xf busybox-1.36.1.tar.bz2
cd busybox-1.36.1
