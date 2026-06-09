#!/bin/bash
# =====================================================================
# D5BU-FateOS Build & Run Toolchain
# This script is designed to run inside the Debian WSL environment.
# It automates compilation, rootfs assembly, packaging, and emulation.
# =====================================================================

# Exit immediately if any command fails, or if an undefined variable is used
set -euo pipefail

# Define variables
WORKSPACE_DIR="/mnt/c/Users/d5bu/OneDrive/Desktop/Projects/FATEOS"
BUILD_DIR="/tmp/fateos-build"
BUSYBOX_VER="1.36.1"
BUSYBOX_URL="https://busybox.net/downloads/busybox-${BUSYBOX_VER}.tar.bz2"

echo "============================================="
echo "  Starting D5BU-FateOS Build Process"
echo "============================================="

# 1. Ensure output and build directories exist
mkdir -p "${WORKSPACE_DIR}/dist"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# 2. Download and Extract BusyBox
if [ ! -f "busybox-${BUSYBOX_VER}.tar.bz2" ]; then
    echo "[-] Downloading BusyBox v${BUSYBOX_VER}..."
    wget -q --show-progress "${BUSYBOX_URL}"
else
    echo "[+] BusyBox archive already downloaded."
fi

if [ ! -d "busybox-${BUSYBOX_VER}" ]; then
    echo "[-] Extracting BusyBox..."
    tar -xf "busybox-${BUSYBOX_VER}.tar.bz2"
fi

# 3. Configure and Compile BusyBox (Statically Linked)
cd "busybox-${BUSYBOX_VER}"

# Create default configuration if .config doesn't exist
if [ ! -f ".config" ]; then
    echo "[-] Generating default BusyBox config..."
    make defconfig
    
    echo "[-] Configuring BusyBox to compile as a static binary..."
    # A static binary has no external library dependencies (no libc.so required).
    # This allows BusyBox to run directly on the kernel without any additional files!
    sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
    
    echo "[-] Disabling incompatible tc utility (Traffic Control)..."
    sed -i 's/CONFIG_TC=y/# CONFIG_TC is not set/' .config
    sed -i 's/CONFIG_FEATURE_TC_INGRESS=y/# CONFIG_FEATURE_TC_INGRESS is not set/' .config
fi

echo "[-] Compiling BusyBox (this takes about 10 seconds)..."
make -j$(nproc)

echo "[-] Installing BusyBox to temporary _install folder..."
# 'make install' creates the folder _install containing bin/, sbin/, and symlinks
make install

# 4. Assemble the Root Filesystem (rootfs)
echo "[-] Creating rootfs directory structure..."
ROOTFS="${BUILD_DIR}/rootfs"
rm -rf "${ROOTFS}"
mkdir -p "${ROOTFS}"

# Copy compiled BusyBox files (bin, sbin, usr)
cp -av "${BUILD_DIR}/busybox-${BUSYBOX_VER}/_install/." "${ROOTFS}/"

# Create essential system mount point folders
# These must exist so our /init script can mount the virtual filesystems on them.
mkdir -p "${ROOTFS}/proc"
mkdir -p "${ROOTFS}/sys"
mkdir -p "${ROOTFS}/dev"
mkdir -p "${ROOTFS}/etc"
mkdir -p "${ROOTFS}/tmp"

# Create essential device nodes
# Linux needs /dev/console to print output and /dev/null for redirection
# BEFORE the virtual devtmpfs filesystem is fully mounted by init.
sudo mknod -m 600 "${ROOTFS}/dev/console" c 5 1
sudo mknod -m 666 "${ROOTFS}/dev/null" c 1 3

# Copy our custom /init script
echo "[-] Copying custom /init script and welcome banner..."
cp "${WORKSPACE_DIR}/src/init" "${ROOTFS}/init"
chmod +x "${ROOTFS}/init"

# Copy the welcome banner
cp "${WORKSPACE_DIR}/src/welcome.txt" "${ROOTFS}/welcome.txt"

# 5. Pack the Root Filesystem into initramfs.cpio.gz
echo "[-] Packing rootfs into compressed initramfs..."
cd "${ROOTFS}"
# Find all files, bundle them as a 'newc' format cpio archive, and compress with gzip
find . -print0 | cpio --null -ov --format=newc | gzip -9 > "${BUILD_DIR}/initramfs.cpio.gz"

# Copy the finished initramfs to the workspace
cp "${BUILD_DIR}/initramfs.cpio.gz" "${WORKSPACE_DIR}/dist/initramfs.cpio.gz"

# 6. Locate and copy the precompiled Linux kernel
echo "[-] Copying precompiled Linux kernel from Debian host..."
# Find the installed kernel image in /boot/ (installed via apt linux-image-amd64)
KERNEL_PATH=$(find /boot -name "vmlinuz-*" | head -n 1)

if [ -z "${KERNEL_PATH}" ]; then
    echo "[!] Error: No precompiled Linux kernel found in /boot."
    echo "    Make sure 'linux-image-amd64' package is installed."
    exit 1
fi

echo "[+] Found kernel: ${KERNEL_PATH}"
cp "${KERNEL_PATH}" "${WORKSPACE_DIR}/dist/vmlinuz"

echo "============================================="
echo "  Build Completed Successfully!"
echo "  Files created in: ${WORKSPACE_DIR}/dist"
echo "  - vmlinuz (Linux Kernel)"
echo "  - initramfs.cpio.gz (Custom Root Filesystem)"
echo "============================================="
echo ""
echo "To boot D5BU-FateOS inside QEMU now, run:"
echo "qemu-system-x86_64 -kernel dist/vmlinuz -initrd dist/initramfs.cpio.gz -nographic -append \"console=ttyS0\""
echo ""
