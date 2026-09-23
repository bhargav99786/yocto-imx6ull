# Custom i.MX6ULL Embedded Linux (Yocto Kirkstone) & Qt 5.15 HMI BSP

This repository contains the complete Yocto Project Build System configuration and custom layer (`meta-custom-hmi`) for the **NXP i.MX6ULL Custom HMI Platform** (derived from Forlinx OKMX6ULL-C architecture).

---

## Technical Features

- **Yocto Release**: Yocto **Kirkstone 4.0 LTS** (Linux Kernel 5.15 LTS + Poky).
- **Qt Framework**: **Qt 5.15.2 LTS** with Framebuffer (`linuxfb`), EGLFS (`eglfs`), and `libgpiod` character device GPIO integration.
- **4 to 5 Second Fast Boot**:
  - Zero-delay U-Boot (`CONFIG_BOOTDELAY=0`).
  - Silent kernel boot (`quiet loglevel=0`).
  - Early direct Framebuffer launch (`hmi-app.service` running `QT_QPA_PLATFORM=linuxfb` before `multi-user.target`).
- **Plug-and-Play AutoIP Networking**: Dual LAN (`eth0` & `eth1`) configured for automatic link-local (`169.254.x.x` IPv4LL) address assignment when connected directly to a PC/laptop, alongside standard DHCP router support.
- **SWUpdate A/B OTA Updates**: Dual-bank RootFS partitioning layout (`Boot`, `RootFS_A` 1.5GB, `RootFS_B` 1.5GB, `UserData` 500MB) with automatic bootlimit rollback protection.

---

## Repository & Layer Architecture

```
yocto-imx6ull/
├── README.md                  # Complete Build & Deployment Documentation
├── .gitignore                 # Excludes build caches (tmp, downloads, sstate-cache)
├── meta-custom-hmi/           # Custom BSP & Application Layer
│   ├── conf/layer.conf
│   ├── recipes-bsp/u-boot/    # U-Boot Fastboot (CONFIG_BOOTDELAY=0) & silent boot
│   ├── recipes-core/systemd/  # AutoIP (10-eth.network) plug-and-play network configuration
│   ├── recipes-graphics/qt5/  # Qt 5.15 integration settings
│   ├── recipes-hmi/hmi-app/   # Fast-boot Qt application auto-start launcher service
│   ├── recipes-kernel/linux/  # Device Tree (imx6ull-custom-hmi.dts) for RGB LCD, GT911 touch, RTC
│   ├── recipes-support/swupdate/ # SWUpdate OTA update package (.swu) & postupdate scripts
│   └── wic/imx-sdcard-ab.wks  # Dual-bank A/B SD card WIC partition table definition
├── build-hmi/
│   └── conf/
│       ├── local.conf         # Pre-configured Yocto build settings, Qt 5.15 specs, rm_work
│       └── bblayers.conf      # Configured layer paths (poky, meta-qt5, meta-freescale, meta-custom-hmi)
├── poky/                      # Core Yocto Kirkstone 4.0 LTS repository
├── meta-openembedded/         # OpenEmbedded utilities & recipes
├── meta-freescale/            # NXP i.MX BSP layer
├── meta-qt5/                  # Qt 5.15 LTS layer
└── meta-swupdate/             # SWUpdate OTA update layer
```

---

## 1. Host Prerequisites & Initial Setup

### Essential Host Packages (Ubuntu/Debian 20.04/22.04 LTS):
```bash
sudo apt update
sudo apt install -y gawk wget git diffstat unzip texinfo gcc build-essential \
chrpath socat cpio python3 python3-pip python3-pexpect xz-utils debianutils \
iputils-ping python3-git python3-jinja2 libegl1-mesa libsdl1.2-dev pylint xterm zstd
```

### System Inotify Limit (Recommended for Yocto):
To prevent BitBake watch errors across large layer sets, increase `fs.inotify.max_user_watches`:
```bash
sudo sysctl -w fs.inotify.max_user_watches=524288
echo "fs.inotify.max_user_watches=524288" | sudo tee /etc/sysctl.d/99-yocto-inotify.conf
```

---

### Step 1: Clone Required Yocto Layers
To automatically clone all required Yocto Kirkstone layer dependencies (`poky`, `meta-openembedded`, `meta-freescale`, `meta-qt5`, `meta-swupdate`), run:
```bash
./setup-layers.sh
```

### Step 2: Initialize the Yocto Environment
From the top-level repository folder:
```bash
source poky/oe-init-build-env build-hmi
```


### Step 2: Build the Core Image
```bash
bitbake core-image-minimal
```

### Output Artifacts:
Upon completion, the output SD card image will be generated at:
`build-hmi/tmp/deploy/images/imx6ullevk/core-image-minimal-imx6ullevk.wic`

---

## 3. Flashing the Image to SD Card

To program the generated image onto an SD Card for boot testing:

```bash
# Replace /dev/sdX with your SD Card block device (e.g., /dev/sdb)
sudo dd if=tmp/deploy/images/imx6ullevk/core-image-minimal-imx6ullevk.wic of=/dev/sdX status=progress conv=fsync
```

---

## 4. Generating the Qt 5.15 Cross-Compilation SDK

To generate the SDK installer script for Qt Creator on host PCs:

```bash
source poky/oe-init-build-env build-hmi
bitbake core-image-minimal -c populate_sdk
```

### Installing the SDK:
The generated installer will be located at:
`build-hmi/tmp/deploy/sdk/poky-glibc-x86_64-core-image-minimal-cortexa7t2hf-neon-imx6ullevk-toolchain-4.0.35.sh`

Run the installer:
```bash
./tmp/deploy/sdk/poky-glibc-x86_64-core-image-minimal-cortexa7t2hf-neon-imx6ullevk-toolchain-4.0.35.sh
```

### Qt Creator Kit Configuration:
- **Compiler (C++)**: `/opt/poky/4.0.35/sysroots/x86_64-pokysdk-linux/usr/bin/arm-poky-linux-gnueabi/arm-poky-linux-gnueabi-g++`
- **Qt Version (`qmake`)**: `/opt/poky/4.0.35/sysroots/x86_64-pokysdk-linux/usr/bin/qmake`
- **Sysroot**: `/opt/poky/4.0.35/sysroots/cortexa7t2hf-neon-poky-linux-gnueabi`

---

## 5. Generating SWUpdate OTA Packages (.swu)

To generate an OTA update bundle for remote firmware updates:

```bash
source poky/oe-init-build-env build-hmi
bitbake swupdate-image
```

The resulting `.swu` update file will be placed in `build-hmi/tmp/deploy/images/imx6ullevk/swupdate-image-imx6ullevk.swu`.

---

## 6. Automatic Factory eMMC Flasher (`emmc-installer`)

The system includes an automated first-boot factory installer that clones the complete system from an SD card onto the onboard eMMC chip with a dual-bank A/B partitioning layout.

### A/B Partition Scheme:
- **`p1` (128 MB, FAT)**: Shared boot partition containing `boot.scr`, `zImage`, device tree binaries (`okmx6ull-c-emmc.dtb`), and power-on splash logo (`logo.bmp`).
- **`p2` (Bank A RootFS, ext4)**: Primary active production root filesystem.
- **`p3` (Bank B RootFS, ext4)**: Alternate standby partition for SWUpdate OTA upgrades.
- **`p4` (Data Partition, ext4)**: Persistent user storage for application databases, configuration files, and system logs.

### Detection & Bootloader Initialization:
- **Hardware-Level Detection**: The installer inspects `/sys/block/<dev>/device/type` (`MMC` vs `SD`) and hardware boot partitions (`/dev/<dev>boot0`) to reliably distinguish the MicroSD card from the onboard eMMC regardless of kernel enumeration order.
- **U-Boot Persistent Configuration**: During factory flashing, the installer initializes U-Boot environment variables for seamless eMMC boot:
  - `fl_menu1=0`: Disables vendor interactive menu prompts for silent autoboot.
  - `bootdev=emmc` & `mmcdev=1`: Targets the onboard eMMC (USDHC2).
  - `panel=TFT70AB-1024x600`: Configures the 1024x600 RGB LCD display.
  - `active_rootfs=rootfs_a`: Sets Bank A as the default active bank.

---

## 7. Interactive HMI Application Features (`hmi-app`)

The Qt 5.15 HMI application provides an integrated testing and operations suite:

### A. Dedicated SPI Test Tab
Designed for testing arbitrary SPI peripherals, bus communication, and signal integrity without requiring specific peripheral hardware:
- **Interfaces**: Supports **ECSPI1** (`/dev/spidev0.0`) and **ECSPI2** (`/dev/spidev1.0`).
- **Configurable Bus Speeds**: 100 kHz, 500 kHz, 1 MHz (default), 5 MHz, 10 MHz, 20 MHz.
- **Test Modes**:
  1. **Loopback Test (MOSI → MISO)**: Transmits a 16-byte reference pattern and verifies byte-for-byte matching with visual PASS/FAIL status.
  2. **JEDEC ID Probe (0x9F)**: Queries Manufacturer ID, Memory Type, and Capacity from SPI Flash / EEPROM chips.
  3. **Walking Bit Sweep**: Transmits walking 1s (`0x01` to `0x80`) to verify signal integrity across data pins.
  4. **Custom Hex / ASCII Transfers**: Send arbitrary byte sequences and inspect full-duplex responses in Hex and ASCII formats.

### B. Hardware UART Console Tab
- **Ports**: Direct access to `/dev/ttymxc1` (UART2), `/dev/ttymxc2` (UART3), `/dev/ttymxc3` (UART4), and `/dev/ttymxc4` (UART5).
- **Features**: Real-time asynchronous RX/TX log monitor with timestamps, baud rate selector (9600 to 921600), loopback ping test (`PING_OKMX6ULL`), and dynamic P17 pinout reference.

### C. Display Sleep & Touch-Wake Control
- **Inactivity Timeout**: Configurable sleep timer (Always On, 30s, 1m, 2m, 5m, or custom seconds) persisted in `/etc/hmi-sleep.conf`.
- **Instant Sleep Test**: Blank display immediately with screen tap to wake.
- **Daemon Architecture**: `hmi-sleep-daemon` polls non-exclusive evdev events on `/dev/input/touchscreen0` and controls `/sys/class/graphics/fb0/blank`.

### D. System Architecture Paths & P17 Pinout Reference
- **System Paths Table**: Comprehensive documentation of application binaries, systemd units, network configs, and OTA paths.
- **P17 Expansion Header Table**: Full 40-pin hardware reference showing pin numbers, SoC signals, peripheral functions, and voltage levels.

---

## 8. Dual-Bank Fail-Safe OTA Workflow

1. **Host Server**: Run an HTTP server serving `version.json` and `update.swu`:
   ```bash
   python3 -m http.server 8000 --directory ota_server/
   ```
2. **Device Trigger**: Navigate to the **Network & OTA** tab on the HMI screen:
   - Click **"Check for Update"** to query the manifest.
   - Click **"Install Update"** to download and stream into the inactive bank (`rootfs_b` or `rootfs_a`) with live percentage tracking.
3. **Rollback Safety**: U-Boot watchdog (`bootlimit 3`, `bootcount`) automatically rolls back to the previous bank if the newly flashed image fails to confirm boot via `confirm-boot.service`.

---

## Licensing & Author

- **Target Hardware**: NXP i.MX6ULL Custom HMI Board (OKMX6ULL-C Compatible)
- **Maintainer**: Custom HMI Solutions Engineering Team
- **License**: MIT
