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

## Licensing & Author

- **Target Hardware**: NXP i.MX6ULL Custom HMI Board
- **Maintainer**: Custom HMI Solutions Engineering Team
- **License**: MIT
