# i.MX6ULL Yocto System Configuration, Boot Logos & Yocto Integration Guide

---

## 1. Yocto Image Recipe Configuration (`core-image-minimal.bbappend`)

`hmi-app` has been removed from the Yocto build recipe so it is no longer installed into your image. **`qt-auto-launcher`** is now the sole active launcher recipe in the Yocto build and bundles the default ready **`test`** binary out-of-the-box!

### File: `meta-custom-hmi/recipes-core/images/core-image-minimal.bbappend`

```bitbake
IMAGE_INSTALL:append = " \
    boot-splash \
    ttf-dejavu-sans \
    ttf-dejavu-common \
    fontconfig \
    evtest \
    emmc-installer \
    swupdate \
    swupdate-www \
    libubootenv-bin \
    ota-agent \
    confirm-boot \
    touch-test \
    hmi-sleep \
    qt-auto-launcher \
"
```

---

## 2. Native Yocto Layer Integration for Qt Service (`qt-auto-launcher`)

We have updated `qt-auto-launcher` in **`meta-custom-hmi`** so it includes the ready **`test`** binary by default!

### A. Yocto Layer Directory Layout:
```text
meta-custom-hmi/recipes-hmi/qt-auto-launcher/
├── qt-auto-launcher.bb
└── files/
    ├── qt-auto-launcher.service
    ├── qt-auto-launcher.sh
    └── test                       <-- Bundled Ready Qt Binary
```

### B. Updated Yocto Recipe File (`qt-auto-launcher.bb`)
Path: [`meta-custom-hmi/recipes-hmi/qt-auto-launcher/qt-auto-launcher.bb`](file:///home/bhargav/yocto-imx6ull/meta-custom-hmi/recipes-hmi/qt-auto-launcher/qt-auto-launcher.bb)

```bitbake
SUMMARY = "Custom Qt Application Launcher with Power-On USB Auto-Update"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI = " \
    file://qt-auto-launcher.sh \
    file://qt-auto-launcher.service \
    file://test \
"

S = "${WORKDIR}"
inherit systemd

SYSTEMD_SERVICE:${PN} = "qt-auto-launcher.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/qt-auto-launcher.sh ${D}${bindir}/qt-auto-launcher.sh

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/qt-auto-launcher.service ${D}${systemd_system_unitdir}/qt-auto-launcher.service

    # Install default ready test binary to /opt/hmi/bin/app
    install -d ${D}/opt/hmi/bin
    install -m 0755 ${WORKDIR}/test ${D}/opt/hmi/bin/app
}

FILES:${PN} += " \
    ${bindir}/qt-auto-launcher.sh \
    ${systemd_system_unitdir}/qt-auto-launcher.service \
    /opt/hmi \
    /opt/hmi/bin \
    /opt/hmi/bin/app \
"
```

---

## 3. Power-On USB Auto-Update Workflow

### Default Installation (No USB Drive attached):
- Out-of-the-box, the Yocto build installs the bundled **`test`** binary to `/opt/hmi/bin/app`.
- On boot, `qt-auto-launcher.service` runs `/opt/hmi/bin/app`.

### USB Drive Auto-Update (When USB Drive is inserted):
Place these **2 files** at the root of your USB flash drive before powering on:
1. `UpdateCont` (Verification text file containing character **`Y`**)
2. `test` (New/Updated Qt compiled ARM 32-bit executable binary)

On power-on, `qt-auto-launcher.sh` detects the USB drive, verifies `UpdateCont`, overwrites `/opt/hmi/bin/app` with the new binary from USB, and launches it.
