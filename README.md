# D5BU-FateOS: Build Log & Architecture Walkthrough
*A student's lab notebook on building a custom Linux distribution from scratch.*

This repository documents my journey of building **D5BU-FateOS** (D5BU Fate Operating System), an ultra-lightweight Linux distribution that runs entirely in RAM. The goal of this project is to understand the absolute fundamentals of the Linux boot process, userspace initialization, and hardware emulation.

---

## Table of Contents
1. [Core Architecture & How Linux Boots](#1-core-architecture--how-linux-boots)
2. [Step 1: Setting up the Build Environment](#step-1-setting-up-the-build-environment)
3. [Step 2: Writing the PID 1 Init Script](#step-2-writing-the-pid-1-init-script)
4. [Step 3: Compiling BusyBox (The Userspace)](#step-3-compiling-busybox-the-userspace)
5. [Step 4: Assembling and Packaging the Rootfs (initramfs)](#step-4-assembling-and-packaging-the-rootfs-initramfs)
6. [Step 5: Extracting the Linux Kernel](#step-5-extracting-the-linux-kernel)
7. [Step 6: Booting D5BU-FateOS in QEMU](#step-6-booting-d5bu-fateos-in-qemu)
8. [Step 7: Networking & Internet Support](#step-7-networking--internet-support)

---

## 1. Core Architecture & How Linux Boots

Before writing a single line of code, I needed to understand what happens when a computer turns on. I learned that booting a Linux system is a relay race where control is passed between several layers:

1. **BIOS/UEFI (Firmware)**: The hardware firmware initialized when the PC starts. It performs hardware checks (POST) and loads the bootloader (like GRUB) from the boot device.
2. **Bootloader (e.g., GRUB)**: Reads its configuration files, loads the **Linux Kernel** (`vmlinuz`) and the **Initial RAM Disk** (`initramfs.cpio.gz`) into RAM, and then jumps into the kernel's entry point.
3. **Linux Kernel**: The core of the OS. It initializes the processor, memory, and essential hardware drivers (like disk controllers and serial ports). Once initialized, it mounts the temporary RAM disk (`initramfs`) as the root filesystem `/`.
4. **PID 1 (`/init`)**: The kernel searches the root filesystem for a program named `/init` and starts it as Process ID 1. If this file doesn't exist, is not executable, or exits, the kernel crashes with a **Kernel Panic**.
5. **Userspace (BusyBox & Shell)**: The `/init` script mounts virtual directories (`/proc`, `/sys`, `/dev`) and starts an interactive command-line shell (`sh`), allowing me to run commands.

```
+---------------+     +------------+     +----------------+     +---------------+     +----------------+
| Hardware/BIOS | --> | Bootloader | --> | Linux Kernel   | --> | PID 1 (/init) | --> | BusyBox Shell  |
| (Firmware)    |     | (GRUB)     |     | (vmlinuz)      |     | (Rootfs mount)|     | (User Console) |
+---------------+     +------------+     +----------------+     +---------------+     +----------------+
```

---

## Step 1: Setting up the Build Environment

Since I am developing on Windows, I need a native Linux building environment to compile Linux binaries. I used **WSL 2 (Windows Subsystem for Linux)** running **Debian GNU/Linux** as my build sandbox.

### 1. Installing Debian WSL
I ran this on my Windows host to download and install a clean Debian container without launching its GUI console:
```powershell
wsl --install -d Debian --no-launch
```

### 2. Installing Developer Tools
Inside the Debian WSL, I installed the core compilers, archive utilities, and emulation tools:
```bash
apt-get update && apt-get install -y \
    build-essential \
    cpio \
    qemu-system-x86 \
    linux-image-amd64 \
    bzip2 \
    wget
```
* **`build-essential`**: Installs `gcc` (GNU C Compiler) and `make` (compilation automation tool).
* **`cpio`**: An old-school archiving tool (similar to `tar`). The Linux kernel requires the RAM disk to be packaged in the `cpio` format.
* **`qemu-system-x86`**: An emulator that simulates x86_64 PC hardware so I can test my operating system inside my terminal.
* **`linux-image-amd64`**: Installs a precompiled Debian Linux kernel. We extract this kernel (`vmlinuz`) to boot D5BU-FateOS, saving compile time.
* **`bzip2` / `wget`**: Used to download and unpack the BusyBox source code.

---

## Step 2: Writing the PID 1 Init Script

I wrote a custom shell script `src/init` which will serve as Process ID 1. It is the absolute heart of D5BU-FateOS.

```bash
#!/bin/sh
# Mount virtual filesystems needed by Linux utilities.
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev 2>/dev/null || mount -t tmpfs none /dev

# Set up terminal multi-user/pty support
mkdir -p /dev/pts
mount -t devpts none /dev/pts

clear
cat /welcome.txt

echo "============================================="
echo "  D5BU-FateOS V1 (Ultra-Lightweight Edition) "
echo "  Kernel: $(uname -r)                        "
echo "============================================="
echo ""

# Start the interactive shell in an infinite loop
while true; do
    setsid cttyhack sh
    echo "Shell exited. Restarting shell..."
    sleep 1
done
```

### What I Learned About Virtual Filesystems:
* **`/proc`**: A pseudo-filesystem. It doesn't exist on disk; it is a direct window into the kernel's RAM. Commands like `top` or `ps` read `/proc` to get process info.
* **`/sys`**: A window into the kernel's hardware model. It allows configuring drivers and checking device properties.
* **`/dev`**: Contains device files. For example, writing to `/dev/stdout` prints to the screen. In V1, we mount `devtmpfs` here so the kernel automatically creates nodes for keyboards, consoles, and disks.
* **`setsid` & `cttyhack`**: These utilities from Busybox are crucial. `cttyhack` detects which terminal device is active (e.g., serial or display screen) and attaches it to the shell. `setsid` launches the shell in a new session, which enables job control (such as using `Ctrl+C` to cancel a command).
* **The Loop**: Wrapping the shell in `while true` ensures that if I type `exit` or the shell crashes, it simply restarts instead of crashing the operating system.

---

## Step 3: Compiling BusyBox (The Userspace)

Linux is just a kernel; it has no `ls`, `cd`, or shell out of the box. Instead of compiling hundreds of separate GNU utilities (which would take hours and create a huge OS), I used **BusyBox**, known as the *"Swiss Army Knife of Embedded Linux"*. It combines tiny versions of over 300 UNIX utilities into a single, compact executable.

### 1. Static Linking
I configured BusyBox to compile as a **statically linked** executable.
* **Dynamic Linking (Default)**: Binaries depend on shared C library files (like `/lib/libc.so.6`) installed on the host OS. If those files are missing in my rootfs, the shell won't start.
* **Static Linking**: The compiler packages all standard library functions directly inside the `busybox` binary itself. The binary becomes slightly larger (~2 MB) but is completely self-contained and has **zero dependencies**.

### 2. Compilation Commands
```bash
# Extract source code
tar -xf busybox-1.36.1.tar.bz2
cd busybox-1.36.1

# Generate a default configuration file (.config)
make defconfig

# Edit the configuration to enable static building programmatically
sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config

# Compile Busybox using all CPU cores
make -j$(nproc)

# Install BusyBox into a directory structure
make install
```
This compilation creates a folder named `_install/` containing:
* `bin/` and `sbin/`: containing the single `busybox` binary and hundreds of symlinks (like `ls -> busybox`, `cat -> busybox`).

---

## Step 4: Assembling and Packaging the Rootfs (initramfs)

Next, I need to bundle my `/init` script, welcome text, and the BusyBox files into a single RAM filesystem image (`initramfs.cpio.gz`).

### 1. Creating the Directory Tree
I created a directory named `rootfs/` and assembled the files:
```bash
mkdir -p rootfs/{proc,sys,dev,etc,tmp}
cp -a busybox-1.36.1/_install/. rootfs/
cp src/init rootfs/init
cp src/welcome.txt rootfs/welcome.txt
```

### 2. Creating Dev Console Nodes
Before `devtmpfs` mounts `/dev`, the kernel needs an initial console node to wire up standard input, output, and error. I created it manually:
```bash
sudo mknod -m 600 rootfs/dev/console c 5 1
sudo mknod -m 666 rootfs/dev/null c 1 3
```
* `c 5 1`: Tells Linux this is a character device with major number 5 and minor number 1 (which corresponds to `/dev/console`).

### 3. Creating the Archive
I used `cpio` (copy-in-copy-out) and `gzip` to compress the filesystem:
```bash
cd rootfs
find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
```
* **`--format=newc`**: The SVR4 portable format header, which is the exact format the Linux kernel expects for an initramfs archive.

---

## Step 5: Extracting the Linux Kernel

Instead of waiting for hours to compile a kernel, I copied the precompiled Linux kernel `vmlinuz` from the Debian host's `/boot` directory:
```bash
KERNEL_PATH=$(find /boot -name "vmlinuz-*" | head -n 1)
cp "${KERNEL_PATH}" dist/vmlinuz
```
The file `vmlinuz` is a compressed Linux kernel executable that can be booted by x86_64 processors.

---

## Step 6: Booting D5BU-FateOS in QEMU

With `vmlinuz` (the brain) and `initramfs.cpio.gz` (the body) ready, I booted D5BU-FateOS using the QEMU emulator:

```bash
qemu-system-x86_64 \
    -kernel dist/vmlinuz \
    -initrd dist/initramfs.cpio.gz \
    -nographic \
    -append "console=ttyS0"
```

### Parameter Breakdown:
* **`-kernel dist/vmlinuz`**: Tells QEMU to load our Linux kernel directly into the virtual machine's RAM.
* **`-initrd dist/initramfs.cpio.gz`**: Tells QEMU to load our RAM filesystem archive.
* **`-nographic`**: Disables the QEMU graphical window and redirects virtual hardware output straight to my terminal screen.
* **`-append "console=ttyS0"`**: Passes a boot parameter to the Linux kernel. It tells the kernel to print all boot logs and start the shell on the serial console (`ttyS0`), which feeds directly into our active terminal.

When I run this command, the kernel boots in less than **0.2 seconds**, executes my `/init` script, displays my ASCII art, and drops me into a fully functioning D5BU-FateOS command-line shell!

---

## Step 7: Networking & Internet Support

Once I had a basic shell booting from the ISO, my next goal was to enable network and internet access inside the RAM disk. I wanted the system to auto-detect a network interface, obtain an IP address via DHCP, and ping external servers.

### 1. The Challenge: Modular Kernel Drivers
When I first ran the custom OS, it printed:
`[!] No network interface card detected.`
Although the host kernel (`vmlinuz`) detected the virtual Intel PCI ethernet card, it didn't initialize it. I learned that Linux kernels don't compile every hardware driver directly into the main kernel file (which would make it massive). Instead:
* Common drivers are built as **Loadable Kernel Modules (LKMs)** with a `.ko` (Kernel Object) extension.
* They are stored under `/lib/modules/$(uname -r)/` and loaded into RAM on demand.
* Since our custom initramfs didn't have `/lib/modules/` or any network driver files, the kernel couldn't activate the network card!

### 2. Copying and Decompressing Drivers
In Debian, kernel modules are compressed using XZ format (`.ko.xz`). Since the standard `insmod` tool in BusyBox requires raw, uncompressed binaries to load modules easily, I updated `build.sh` to extract the drivers from the host, copy them, and decompress them:

```bash
# copy e1000 (Intel NIC) and virtio-net (virtual NIC) with its dependencies
MODULES=(
    "kernel/drivers/net/ethernet/intel/e1000/e1000.ko.xz"
    "kernel/net/core/failover.ko.xz"
    "kernel/drivers/net/net_failover.ko.xz"
    "kernel/drivers/net/virtio_net.ko.xz"
)
for mod in "${MODULES[@]}"; do
    cp "/lib/modules/${KERNEL_VER}/${mod}" "rootfs/lib/modules/"
    xz -d "rootfs/lib/modules/$(basename ${mod})"
done
```

This populates our RAM disk with `e1000.ko` and `virtio_net.ko` under `/lib/modules/`.

### 3. Loading Modules at Boot (init)
I updated `/init` to dynamically insert the network drivers using `insmod` before executing network scans:
```bash
echo "[-] Loading network driver modules..."
insmod /lib/modules/failover.ko
insmod /lib/modules/net_failover.ko
insmod /lib/modules/virtio_net.ko
insmod /lib/modules/e1000.ko
```
Once the modules are loaded, the kernel initializes the network card, creating the interface node `eth0`.

### 4. DHCP & Routing Configuration
With `eth0` activated, I used BusyBox's `udhcpc` (DHCP client) to request configuration parameters from the network's DHCP server.
`udhcpc` coordinates with a helper script `/etc/udhcpc.script` which dynamically configures:
1. **IP Address**: Assigns the leased IP address to the interface:
   ```bash
   ifconfig eth0 10.0.2.15 netmask 255.255.255.0 up
   ```
2. **Default Gateway**: Deletes any stale routes and adds the virtual router gateway:
   ```bash
   route add default gw 10.0.2.2 dev eth0
   ```
3. **DNS Resolution**: Populates `/etc/resolv.conf` so names can be translated to IPs:
   ```bash
   echo "nameserver 10.0.2.3" > /etc/resolv.conf
   ```

### 5. Verification
When booting D5BU-FateOS in QEMU, the system successfully initializes the interface and leases an IP:
```
[-] Loading network driver modules...
[    7.324162] e1000: Intel(R) PRO/1000 Network Driver
[    7.681516] e1000 0000:00:03.0 eth0: Intel(R) PRO/1000 Network Connection
[-] Initializing network loopback...
[+] Found network interface: eth0
[-] Requesting IP address via DHCP in the background...
[udhcpc] Bound interface eth0 to IP: 10.0.2.15
[udhcpc] Setting default gateway to: 10.0.2.2
[udhcpc] Writing DNS configurations...
```
I verified outbound connectivity by running pings inside the shell:
```bash
~ # ping -c 4 8.8.8.8
4 packets transmitted, 4 packets received, 0% packet loss

~ # ping -c 4 google.com
4 packets transmitted, 4 packets received, 0% packet loss
```
Internet, routing, and DNS are fully operational!

---

# Project Attribution
Built by D5BU on 2026-06-09. Last updated on 2026-06-11.
