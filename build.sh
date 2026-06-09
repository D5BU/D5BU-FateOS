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
make defconfig
sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
make -j$(nproc)
make install
ROOTFS="${BUILD_DIR}/rootfs"
mkdir -p "${ROOTFS}"
cp -a "${BUILD_DIR}/busybox-1.36.1/_install/." "${ROOTFS}/"
mkdir -p "${ROOTFS}"/{proc,sys,dev,etc,tmp}
sudo mknod -m 600 "${ROOTFS}/dev/console" c 5 1
sudo mknod -m 666 "${ROOTFS}/dev/null" c 1 3
cp "${WORKSPACE_DIR}/src/init" "${ROOTFS}/init"
chmod +x "${ROOTFS}/init"
cp "${WORKSPACE_DIR}/src/welcome.txt" "${ROOTFS}/welcome.txt"
cd "${ROOTFS}"
find . -print0 | cpio --null -ov --format=newc | gzip -9 > "${WORKSPACE_DIR}/dist/initramfs.cpio.gz"
cp /boot/vmlinuz-$(uname -r) "${WORKSPACE_DIR}/dist/vmlinuz"
