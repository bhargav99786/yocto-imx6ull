# Technical Specification, Proposal & Quotation

**Project Title:** Embedded Linux (Yocto) BSP & Qt 5.15.2 HMI Development for Custom i.MX6ULL Platform  
**Target Hardware:** NXP i.MX6ULL Custom SOM (Derived from Forlinx OKMX6ULL-C Architecture)  
**Client Name:** Mr. Kaushik Kale  
**Date:** August 21, 2026  
**Document Version:** 1.0  

---

## 1. Executive Summary

This document details the complete technical specifications, software architecture, timeline, individual developer effort, and commercial quotation for migrating from a manual VM-based RootFS workflow to an automated **Yocto Project Build System**. 

The system will feature **Qt 5.15.2 LTS**, **4-to-5 second Fast Boot**, **Plug-and-Play AutoIP networking**, and **SWUpdate A/B dual-partition Over-The-Air (OTA) updates** for an industrial HMI Touch Screen controller based on the NXP i.MX6ULL.

---

## 2. Master Feature & Requirement Checklist

### A. Hardware & Platform Integration
* [x] **SoC / Microcontroller:** NXP i.MX6ULL (ARM Cortex-A7 @ 528MHz).
* [x] **Base Module:** Forlinx OKMX6ULL-C derived custom carrier board.
* [x] **Display Interface:** 7-inch or 10-inch Parallel RGB LCD (eLCDIF 24-bit/18-bit interface).
* [x] **Touchscreen Controller:** Capacitive Touch Screen via I2C interface (Goodix GT911 / FocalTech FT5x06 family).
* [x] **Dual Ethernet (2x LAN):** Dual Ethernet PHY drivers (`eth0` & `eth1` via FEC1 and FEC2).
* [x] **USB Ports:** 2x USB 2.0 Host ports + 1x USB-C OTG (Dual-role / Flashing mode).
* [x] **Real-Time Clock (RTC):** External I2C RTC (PCF8563 / DS1307) with automatic system `hwclock` synchronization on boot.
* [x] **Serial Interfaces:** Multiple UARTs configured for RS232 and RS485 with RTS pin hardware direction control.
* [x] **General Peripherals:** Pin multiplexing & Device Tree configuration for active carrier board GPIOs.

### B. Yocto Build System, Kernel & Qt 5.15.2
* [x] **Kernel Upgrade:** **Upgrade Linux Kernel from legacy 4.1.15 to Linux Kernel 5.10 LTS / 5.15 LTS** (required for `libgpiod`, Qt 5.15.2, and SWUpdate).
* [x] **Build System:** Yocto Kirkstone (4.0 LTS) or Dunfell (3.1 LTS) setup.
* [x] **Qt Framework:** **Qt 5.15.2 LTS** integrated via official `meta-qt5` layer.
* [x] **GPIO Control:** **`libgpiod` & `libgpiod-dev`** character device library integrated into RootFS & SDK for high-speed Qt 5.15.2 C++ I/O programming (`/dev/gpiochipX`).
* [x] **Application Deployment:** Standardized RootFS target folder structure for executable deployment.
* [x] **Auto-Start Service:** Automatic execution of Qt 5.15.2 application on boot via `systemd`.
* [x] **Developer SDK:** Exported cross-compilation toolchain installer (`.sh`) for application compilation in Qt Creator.

### C. Performance & Network Requirements
* [x] **Fast Boot Time:** **4 to 5 seconds** target boot time from power-on to Qt 5.15.2 HMI screen.
* [x] **Plug-and-Play AutoIP (IPv4LL):** Automatic self-assigned `169.254.x.x` link-local IP on direct Windows PC connection.
* [x] **DHCP Support:** Automatic IP allocation when connected to a router/switch.

### D. Remote Updates & Maintenance
* [x] **SWUpdate OTA System:** Mobile-style firmware & application update system using **SWUpdate**.
* [x] **A/B Dual Partitioning:** Dual rootfs layout (`RootFS_A` and `RootFS_B`) for safe, zero-downtime updates.
* [x] **Automatic Rollback:** U-Boot hardware boot counter protection (`bootlimit`) to revert if update fails.
* [x] **Production Flashing Tools:** SD Card / USB / MFGTool scripts for factory programming.

---

## 3. Technical Architecture & Implementation Details

### A. Yocto Layer Structure (`meta-custom-hmi`)
```
meta-custom-hmi/
├── conf/
│   └── layer.conf
├── recipes-bsp/
│   └── u-boot/
│       └── u-boot-imx_%.bbappend (Fast boot & A/B fallback env)
├── recipes-core/
│   └── systemd/
│       └── systemd_%.bbappend (AutoIP & fast networkd setup)
├── recipes-graphics/
│   └── qt5/
│       └── meta-qt5 integration (Qt 5.15.2 recipes)
├── recipes-hmi/
│   └── hmi-app/
│       └── hmi-app.bb (Qt App auto-start launcher service)
└── recipes-kernel/
    └── linux/
        ├── files/
        │   └── imx6ull-custom-hmi.dts (Device Tree for custom board)
        └── linux-imx_%.bbappend
```

### B. Device Tree (.dts) Configuration Sample
```dts
/* 7" / 10" Parallel RGB LCD Panel */
&lcdif {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_lcdif_dat &pinctrl_lcdif_ctrl>;
    status = "okay";

    display-timings {
        native-mode = <&timing0>;
        timing0: timing0 {
            clock-frequency = <33000000>;
            hactive = <800>;
            vactive = <480>;
            hback-porch = <88>;
            hfront-porch = <40>;
            hsync-len = <48>;
            vback-porch = <32>;
            vfront-porch = <13>;
            vsync-len = <3>;
        };
    };
};

/* I2C Capacitive Touchscreen (Goodix GT911) */
&i2c1 {
    clock-frequency = <100000>;
    status = "okay";

    gt911: touchscreen@14 {
        compatible = "goodix,gt911";
        reg = <0x14>;
        interrupt-parent = <&gpio1>;
        interrupts = <9 IRQ_TYPE_EDGE_FALLING>;
        reset-gpios = <&gpio5 9 GPIO_ACTIVE_LOW>;
        status = "okay";
    };
};
```

### C. Direct Cable Plug-and-Play Networking (AutoIP)
Configuration for `/etc/systemd/network/10-eth.network`:
```ini
[Match]
Name=eth0 eth1

[Network]
DHCP=yes
LinkLocalAddressing=yes
IPv4LL=yes
MulticastDNS=yes
LLMNR=yes
```

### D. SWUpdate Dual-Bank (A/B) Partition Layout
```
+-------------------------------------------------------------------------+
| Partition | Name        | Size     | Function                           |
+-----------+-------------+----------+------------------------------------+
| p1        | Boot        | 64 MB    | U-Boot, Kernel zImage, DTB         |
| p2        | RootFS_A    | 1.5 GB   | Active System System Image         |
| p3        | RootFS_B    | 1.5 GB   | Backup / OTA Target System Image   |
| p4        | UserData    | Remaining| Persistent Database & Config Files |
+-------------------------------------------------------------------------+
```

### E. 4-to-5 Second Fast Boot Strategy
1. **U-Boot Optimization:** `CONFIG_BOOTDELAY=0`, `CONFIG_SILENT_CONSOLE=y`.
2. **Kernel Pruning:** Uncompressed `zImage`/`uImage` with LZ4 decompression, `quiet` boot, built-in drivers (no initramfs overhead).
3. **Direct Qt Launch:** Execute Qt 5.15.2 directly on Linux Framebuffer (`QT_QPA_PLATFORM=linuxfb`) prior to heavy background networking initialization.

---

## 4. Individual Developer Effort & Hours Breakdown

| Phase | Task Description | Dedicated Hours |
| :--- | :--- | :--- |
| **Phase 1** | **Yocto Base Environment & Qt 5.15.2 Setup**<br>• Setup Yocto Kirkstone environment.<br>• Integrate `meta-qt5` (Qt 5.15.2) & build base image.<br>• Export `.sh` cross-compilation SDK for Qt Creator. | **20 – 25 Hours** |
| **Phase 2** | **Device Tree (.dts) & Driver Customization**<br>• Audit finalized schematic PDF.<br>• Config 7"/10" RGB LCD & I2C Touchscreen.<br>• Config Dual LAN (FEC1+FEC2), 2x USB, RTC, UARTs, GPIOs. | **25 – 30 Hours** |
| **Phase 3** | **Fast Boot Optimization (4 to 5 Seconds)**<br>• U-Boot zero-delay & silent boot.<br>• Prune Linux kernel (LZ4 compression).<br>• Configure parallel init & launch Qt 5.15.2 on `linuxfb`. | **20 – 25 Hours** |
| **Phase 4** | **AutoIP / Plug-and-Play Networking**<br>• Configure `systemd-networkd` / `Avahi` on `eth0` & `eth1`.<br>• Test direct Windows PC connection over Ethernet cable. | **10 – 15 Hours** |
| **Phase 5** | **SWUpdate A/B System (Remote Updates)**<br>• Setup eMMC A/B dual-partitioning layout.<br>• U-Boot auto-rollback scripts (`bootlimit`).<br>• Generate `.swu` update packages. | **25 – 30 Hours** |
| **Phase 6** | **Flashing Utilities & Final Handover**<br>• Create production SD Card flasher scripts.<br>• Hardware testing on physical prototype board & documentation. | **15 – 20 Hours** |
| **TOTAL** | **Total Individual Engineering Effort** | **115 – 145 Hours** |

---

## 5. Timeline & Delivery Schedule

* **Full-Time Schedule (30–35 hrs/week):** 4 Weeks
* **Part-Time Schedule (15–20 hrs/week):** 6 to 7 Weeks
* **Committed Client Schedule:** **4 to 5 Weeks** (includes 1 week buffer for hardware shipping & testing).

---

## 6. Detailed Commercial Pricing & Quotation (INR ₹)

### A. Itemized Module-wise Cost Breakdown

| Module | Technical Deliverable | Effort (Hours) | Cost (INR ₹) |
| :--- | :--- | :--- | :--- |
| **Module 1** | **Yocto Kirkstone/Dunfell BSP & Qt 5.15.2 Integration**<br>Yocto layer setup, Qt 5.15.2 recipes, SDK installer export | 25 hrs | **₹80,000** |
| **Module 2** | **Device Tree (.dts) & Hardware Peripherals**<br>7"/10" RGB LCD panel, I2C Touch driver, Dual LAN, USB, RTC, UARTs, GPIOs | 30 hrs | **₹90,000** |
| **Module 3** | **4 to 5 Second Fast Boot Optimization**<br>U-Boot zero-delay, quiet console, LZ4 kernel, direct Qt `linuxfb` init | 25 hrs | **₹70,000** |
| **Module 4** | **Plug-and-Play AutoIP Networking**<br>Dual LAN AutoIP (IPv4LL `169.254.x.x`) + DHCP for direct Windows PC link | 15 hrs | **₹40,000** |
| **Module 5** | **SWUpdate A/B OTA Remote Update System**<br>Dual eMMC partitions, U-Boot auto-rollback scripts, `.swu` package creation | 25 hrs | **₹70,000** |
| **Module 6** | **Flashing Utilities & Documentation**<br>Production SD Card / MFGTool flashing scripts & handover docs | 15 hrs | **₹30,000** |
| **TOTAL** | **Module-wise Itemized Subtotal** | **135 hrs** | **₹3,80,000 INR** |
| **PROPOSAL**| **Special Package Price for Mr. Kaushik Kale** | **Fixed Price** | **₹3,50,000 INR** |

---

### B. Package Tier Options

| Tier | Package | Included Scope | Estimated Timeline | Fixed Price (INR ₹) |
| :--- | :--- | :--- | :--- | :--- |
| **Tier 1** | **Basic BSP + Qt 5.15.2** | Yocto setup, Qt 5.15.2 SDK, basic 7"/10" display & touch bootable | 2 – 3 Weeks | **₹1,80,000 INR** |
| **Tier 2** | **Standard (Recommended)** | Full BSP, Qt 5.15.2, Dual LAN AutoIP, 4-5s Fast Boot, SWUpdate OTA, Flashing tool | 4 – 5 Weeks | **₹3,50,000 INR** |
| **Tier 3** | **Turnkey + Hardware Supply** | Standard Package + 2 Units of 7" Industrial RGB Touch Panels supplied | 4 – 5 Weeks | **₹3,75,000 INR** |

---

### C. Optional Hardware Supply Costs (If Display Panel is Provided by Developer)

| Item | Hardware Description | Unit Cost (INR ₹) |
| :--- | :--- | :--- |
| **Display Panel** | 7-inch Industrial Parallel RGB LCD Panel (800x480 / 1024x600) | **₹6,500 – ₹8,500** per unit |
| **Touch Sensor** | I2C Capacitive Touch Sensor Panel (Goodix GT911 integrated) | Included with LCD |
| **Accessories** | FPC Flex Cable & Adapter Board | **₹1,500** per set |

---

### D. Hourly Rate Billing Option
If the client prefers Time & Material (T&M) billing instead of a Fixed Price:
* **Hourly Billing Rate:** **₹2,000 INR / Hour**
* **Estimated Project Effort:** 130 to 150 Hours

---

### E. Milestone Payment Schedule (Trust-Building Structure)

To build client confidence, we keep the initial advance low and link major payments to verified hardware demonstrations:

| Milestone | Deliverable / Demonstration | Payment % | Amount (INR) |
| :--- | :--- | :--- | :--- |
| **Commitment Advance** | Project Kickoff & Yocto Repository Setup | **15%** | **₹52,500** |
| **Milestone 1** | Bootable Yocto BSP + Qt 5.15.2 SDK + 7"/10" Touch LCD working live on hardware | **35%** | **₹1,22,500** |
| **Milestone 2** | Dual LAN AutoIP working + 4-5s Fast Boot demonstrated on board | **25%** | **₹87,500** |
| **Milestone 3** | SWUpdate A/B OTA working + Flashing tools + Final Repository Handover | **25%** | **₹87,500** |
| **TOTAL** | | **100%** | **₹3,50,000** |




---

## 7. Client Prerequisites

To maintain the project schedule, **Mr. Kaushik Kale / Client** will provide:
1. **Hardware:** Minimum **2 physical prototype boards** along with carrier board schematic PDF.
2. **Peripherals:** 7-inch or 10-inch RGB Touch screen panel for hardware-in-the-loop testing.
3. **Warranty:** 30 days of post-delivery bug-fixing support.
