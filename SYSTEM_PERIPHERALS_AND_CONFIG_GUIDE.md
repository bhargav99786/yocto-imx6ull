# i.MX6ULL Complete System, Peripherals & Services Configuration Guide

This guide is the single authoritative reference for hardware interfaces (GPIO, UART, SPI, Ethernet) and system services (OTA Agent, Display Sleep, USB Auto-Update) on the **NXP i.MX6ULL / OKMX6ULL-C** custom board.

---

## 📑 Quick Reference: "Which File Do I Edit?"

| Feature | Live on Device (Runtime) | Yocto Recipe Source (Permanent) | Service to Restart |
| :--- | :--- | :--- | :--- |
| **Ethernet eth0 (Port 1)** | `/etc/systemd/network/10-eth0.network` | [`meta-custom-hmi/recipes-core/systemd/files/10-eth0.network`](meta-custom-hmi/recipes-core/systemd/files/10-eth0.network) | `systemctl restart systemd-networkd` |
| **Ethernet eth1 (Port 2)** | `/etc/systemd/network/11-eth1.network` | [`meta-custom-hmi/recipes-core/systemd/files/11-eth1.network`](meta-custom-hmi/recipes-core/systemd/files/11-eth1.network) | `systemctl restart systemd-networkd` |
| **OTA Server & Polling** | `/etc/ota-server.conf` | [`meta-custom-hmi/recipes-core/ota-agent/files/ota-server.conf`](meta-custom-hmi/recipes-core/ota-agent/files/ota-server.conf) | `systemctl restart ota-agent` |
| **Display Sleep Timeout** | `/etc/hmi-sleep.conf` | [`meta-custom-hmi/recipes-hmi/hmi-sleep/files/hmi-sleep.conf`](meta-custom-hmi/recipes-hmi/hmi-sleep/files/hmi-sleep.conf) | `systemctl restart hmi-sleep` |
| **USB Auto-Update Script** | `/usr/bin/qt-auto-launcher.sh` | [`meta-custom-hmi/recipes-hmi/qt-auto-launcher/files/qt-auto-launcher.sh`](meta-custom-hmi/recipes-hmi/qt-auto-launcher/files/qt-auto-launcher.sh) | `systemctl restart qt-auto-launcher` |
| **Default Bundled Binary** | `/opt/hmi/bin/app` | [`meta-custom-hmi/recipes-hmi/qt-auto-launcher/files/test`](meta-custom-hmi/recipes-hmi/qt-auto-launcher/files/test) | `systemctl restart qt-auto-launcher` |
| **Device Tree (Pads/Buses)**| `/boot/okmx6ull-c-emmc.dtb` | [`meta-custom-hmi/recipes-kernel/linux/files/imx6ull-custom-hmi.dts`](meta-custom-hmi/recipes-kernel/linux/files/imx6ull-custom-hmi.dts) | Re-compile kernel / reboot |

---

## 1. GPIO Control & P17 Header Reference

### Linux GPIO Number Calculation
On i.MX6ULL, Linux sysfs GPIO numbers are calculated as:
$$\text{GPIO Number} = (\text{Bank} - 1) \times 32 + \text{Pin}$$

* **GPIO1** = `gpiochip0` (Pins 0–31)
* **GPIO2** = `gpiochip1` (Pins 32–63)
* **GPIO3** = `gpiochip2` (Pins 64–95)
* **GPIO4** = `gpiochip3` (Pins 96–127)
* **GPIO5** = `gpiochip4` (Pins 128–159)

### Key User Pin Mapping (P17 Expansion Header)

| P17 Pin | Board Pad | Linux GPIO | gpiod Line | Default Function | Usage / Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Pin 8** | `CSI_HSYNC` | **GPIO 116** (`GPIO4_IO20`) | `gpiochip3` Line 20 | User GPIO / I2C2 SCL | Safe for User Input/Output |
| **Pin 9** | `CSI_MCLK` | **GPIO 113** (`GPIO4_IO17`) | `gpiochip3` Line 17 | User GPIO / I2C1 SDA | *Shared with Touch/RTC on I2C1* |
| **Backlight** | `GPIO1_IO08` | **GPIO 8** (`GPIO1_IO08`) | `gpiochip0` Line 8 | PWM / Backlight | Display brightness control |
| **UART3 TX** | `UART3_TX` | **GPIO 24** (`GPIO1_IO24`) | `gpiochip0` Line 24 | UART3 Transmit | Can be repurposed if UART3 unused |
| **UART3 RX** | `UART3_RX` | **GPIO 25** (`GPIO1_IO25`) | `gpiochip0` Line 25 | UART3 Receive | Can be repurposed if UART3 unused |

### How to Control GPIO via Command Line

#### Method A: Modern `libgpiod` Tools (Recommended)
```bash
# List all GPIO controllers and lines
gpiodetect
gpioinfo gpiochip3

# Set Pin 8 (chip 3, line 20) as Output HIGH:
gpioset gpiochip3 20=1

# Set Pin 8 as Output LOW:
gpioset gpiochip3 20=0

# Read Pin 8 as Input:
gpioget gpiochip3 20
```

#### Method B: Legacy Sysfs (`/sys/class/gpio`)
```bash
# 1. Export GPIO 116 (Pin 8)
echo 116 > /sys/class/gpio/export

# 2. Set Direction (out or in)
echo out > /sys/class/gpio/gpio116/direction

# 3. Write Value (1 = HIGH, 0 = LOW)
echo 1 > /sys/class/gpio/gpio116/value
echo 0 > /sys/class/gpio/gpio116/value

# 4. Read Value
cat /sys/class/gpio/gpio116/value

# 5. Unexport when finished
echo 116 > /sys/class/gpio/unexport
```

---

## 2. UART Serial Port Paths & Usage

### Serial Device Node Mapping

| Linux Device Node | Physical Peripheral | Default Baud | Description / Header Pins |
| :--- | :--- | :--- | :--- |
| **`/dev/ttymxc0`** | UART1 | `115200 8N1` | **Debug Serial Console** (Host PC USB adapter) |
| **`/dev/ttymxc1`** | UART2 | Configurable | General Purpose RS232 / TTL |
| **`/dev/ttymxc2`** | UART3 | Configurable | General Purpose (P17 pins) |
| **`/dev/ttymxc3`** | UART4 | Configurable | General Purpose RS485 / TTL |
| **`/dev/ttymxc4`** | UART5 | Configurable | General Purpose TTL |

### How to Test / Communicate via UART

```bash
# 1. Configure baud rate and raw mode
stty -F /dev/ttymxc1 115200 raw -echo

# 2. Transmit data to UART2
echo "HELLO_UART2" > /dev/ttymxc1

# 3. Read data from UART2 (in background or separate terminal)
cat /dev/ttymxc1

# 4. Interactive serial session (using microcom or picocom)
microcom -s 115200 /dev/ttymxc1
```

---

## 3. SPI Bus Paths & Loopback Testing

### SPI Device Nodes

| Linux Device Node | Controller | Maximum Speed | Typical Usage |
| :--- | :--- | :--- | :--- |
| **`/dev/spidev0.0`** | ECSPI1, Chip Select 0 | Up to 20 MHz | Primary SPI bus (Flash / EEPROM / Sensors) |
| **`/dev/spidev1.0`** | ECSPI2, Chip Select 0 | Up to 20 MHz | Secondary SPI bus |

### How to Perform Hardware Loopback Test (MOSI ↔ MISO)

1. **Short MOSI and MISO pins** together on the board header.
2. Run SPI test tool using `spidev_test` (built into Yocto tools):
```bash
# Transmit test sequence and verify received data matches
spidev_test -D /dev/spidev0.0 -s 1000000 -v
```

3. Python SPI Transfer Test:
```python
import spidev
spi = spidev.SpiDev()
spi.open(0, 0)
spi.max_speed_hz = 1000000
spi.mode = 0
response = spi.xfer2([0xAA, 0x55, 0x12, 0x34])
print("Received:", [hex(x) for x in response])
spi.close()
```

---

## 4. Dual Ethernet `.network` Configuration (`systemd-networkd`)

The system uses `systemd-networkd` with dedicated configuration files for each Ethernet port.

### Configuration Files:
* **Port 1 (eth0 / fec1):** `/etc/systemd/network/10-eth0.network`
* **Port 2 (eth1 / fec2):** `/etc/systemd/network/11-eth1.network`
* **Yocto Recipe:** [`meta-custom-hmi/recipes-core/systemd/files/`](meta-custom-hmi/recipes-core/systemd/files/)

---

### Network Mode Examples

#### Option A: Auto-IP (Link-Local 169.254.x.x) + DHCP (Current Default)
Plug-and-play direct connection to a laptop or network switch without requiring a router.
```ini
[Match]
Name=eth0

[Network]
DHCP=yes
LinkLocalAddressing=ipv4
MulticastDNS=yes
LLMNR=yes
IPv6AcceptRA=no

[DHCPv4]
RouteMetric=10
```

#### Option B: Static IP Configuration
Edit `/etc/systemd/network/10-eth0.network`:
```ini
[Match]
Name=eth0

[Network]
Address=192.168.1.150/24
Gateway=192.168.1.1
DNS=8.8.8.8
DNS=1.1.1.1
LinkLocalAddressing=no
IPv6AcceptRA=no
```

#### Option C: Pure DHCP Only
```ini
[Match]
Name=eth0

[Network]
DHCP=ipv4
LinkLocalAddressing=no
```

### Apply Network Changes:
```bash
systemctl restart systemd-networkd
networkctl status eth0
ip addr show eth0
```

---

## 5. OTA Agent Service Configuration (`ota-agent`)

The `ota-agent` service polls an HTTP OTA server in the background, checks for new firmware bundles (`update.swu`), and installs them into the alternate rootfs partition (Dual A/B Bank fail-safe update).

### Files & Paths:
* **Runtime Config File:** `/etc/ota-server.conf`
* **Systemd Service:** `ota-agent.service` (`/lib/systemd/system/ota-agent.service`)
* **Agent Executable:** `/usr/bin/ota-update-agent`
* **Yocto Source:** [`meta-custom-hmi/recipes-core/ota-agent/files/ota-server.conf`](meta-custom-hmi/recipes-core/ota-agent/files/ota-server.conf)

### How to Change the OTA Server URL & Interval

Edit `/etc/ota-server.conf`:
```sh
# The HTTP server hosting version.json and update.swu
OTA_SERVER_URL="http://192.168.1.100:8000"

# Polling frequency in seconds (default: 30 seconds)
CHECK_INTERVAL=60
```

### Management Commands:
```bash
# Restart the agent to apply changes
systemctl restart ota-agent

# View live OTA checking logs
journalctl -u ota-agent -f
```

---

## 6. Display Sleep & Touch-to-Wake (`hmi-sleep`)

The `hmi-sleep` service monitors touch screen activity. If no touch input occurs within the timeout period, it blanks the LCD display (`/sys/class/graphics/fb0/blank`) and turns off backlight power to conserve energy. Touching anywhere on the screen immediately wakes the display up.

### Files & Paths:
* **Runtime Config File:** `/etc/hmi-sleep.conf`
* **Systemd Service:** `hmi-sleep.service` (`/lib/systemd/system/hmi-sleep.service`)
* **Daemon Binary:** `/usr/bin/hmi-sleep-daemon`
* **Yocto Source:** [`meta-custom-hmi/recipes-hmi/hmi-sleep/files/hmi-sleep.conf`](meta-custom-hmi/recipes-hmi/hmi-sleep/files/hmi-sleep.conf)

### How to Adjust Display Sleep Timeout

Edit `/etc/hmi-sleep.conf`:
```sh
# Inactivity duration in seconds before screen blanks
# Common settings:
#   SLEEP_TIMEOUT_SEC=30   (30 seconds)
#   SLEEP_TIMEOUT_SEC=120  (2 minutes — default)
#   SLEEP_TIMEOUT_SEC=300  (5 minutes)
#   SLEEP_TIMEOUT_SEC=0    (Always ON — sleep disabled)

SLEEP_TIMEOUT_SEC=120
```

### Management Commands:
```bash
# Restart the sleep daemon to apply new timeout
systemctl restart hmi-sleep

# Check current status and touch event monitoring
journalctl -u hmi-sleep -n 20 --no-pager
```

---

## 7. Power-On USB Auto-Update (`qt-auto-launcher`)

The system features an automated USB auto-update mechanism that replaces the running Qt application without requiring network access, SSH, or compiler tools on the target board.

### Files & Paths:
* **Target Application Binary:** `/opt/hmi/bin/app`
* **Launcher Script:** `/usr/bin/qt-auto-launcher.sh`
* **Systemd Service:** `qt-auto-launcher.service`
* **Yocto Source Recipe:** [`meta-custom-hmi/recipes-hmi/qt-auto-launcher/`](meta-custom-hmi/recipes-hmi/qt-auto-launcher/)

---

### Step-by-Step USB Update Workflow

#### 1. Prepare USB Flash Drive (FAT32)
Place these **2 files** directly in the root of the USB flash drive:
1. **`UpdateCont`**: Plain text file containing the single character `Y`:
   ```text
   Y
   ```
2. **`test`** (or **`app`**): The newly compiled ARM 32-bit executable binary.

#### 2. Automatic Boot Update
1. Insert the USB flash drive into the board's USB host port.
2. Power cycle or reboot the board.
3. [`qt-auto-launcher.sh`](qt-auto-launcher.sh) automatically:
   * Auto-mounts `/dev/sda1` or `/dev/sda` to `/media/sda1` or `/media/sda`.
   * Searches mount points `/media/sda1`, `/media/sda`, `/run/media/*`.
   * Validates `UpdateCont` equals `Y`.
   * Overwrites `/opt/hmi/bin/app` with the binary from the USB drive.
   * Runs `chmod +x /opt/hmi/bin/app` and `sync`.
   * Launches the updated application.

#### 3. Live Update (Without Board Reboot)
If connected via serial console or SSH, plug in the USB drive and run:
```bash
systemctl restart qt-auto-launcher
```

#### 4. Verify Update Success
```bash
# Check service logs for the update confirmation
journalctl -u qt-auto-launcher -n 25 --no-pager

# Check file size & MD5 checksum
ls -l /opt/hmi/bin/app
md5sum /opt/hmi/bin/app
```
