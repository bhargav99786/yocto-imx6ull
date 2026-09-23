#!/bin/bash
set -e

SD_TARGET="/media/bhargav/rootfs_a"

if [ ! -d "$SD_TARGET" ]; then
    echo "ERROR: $SD_TARGET is not mounted or not found!"
    echo "Current mounts:"
    lsblk
    exit 1
fi

echo "=== Updating SD Card Partition: $SD_TARGET ==="

echo "[1/6] Copying hmi-app..."
cp -v /tmp/hmi-app-pkg/usr/bin/hmi-app "$SD_TARGET/usr/bin/hmi-app"
chmod 755 "$SD_TARGET/usr/bin/hmi-app"

echo "[2/6] Copying helper tools & launcher..."
cp -v /tmp/hmi-app-pkg/usr/bin/set-hmi-app "$SD_TARGET/usr/bin/set-hmi-app"
chmod 755 "$SD_TARGET/usr/bin/set-hmi-app"

cp -v /tmp/hmi-app-pkg/usr/bin/hmi-session-launcher "$SD_TARGET/usr/bin/hmi-session-launcher"
chmod 755 "$SD_TARGET/usr/bin/hmi-session-launcher"

echo "[3/6] Updating hmi-session.conf & touch parameters..."
cp -v /tmp/hmi-app-pkg/etc/hmi-session.conf "$SD_TARGET/etc/hmi-session.conf"
mkdir -p "$SD_TARGET/opt/hmi/bin"
ln -sf /usr/bin/hmi-app "$SD_TARGET/opt/hmi/bin/app"

echo "[4/6] Installing hmi-sleep daemon & configs..."
cp -v /tmp/hmi-sleep-pkg/usr/bin/hmi-sleep-daemon "$SD_TARGET/usr/bin/hmi-sleep-daemon"
chmod 755 "$SD_TARGET/usr/bin/hmi-sleep-daemon"

if [ ! -f "$SD_TARGET/etc/hmi-sleep.conf" ]; then
    cp -v /tmp/hmi-sleep-pkg/etc/hmi-sleep.conf "$SD_TARGET/etc/hmi-sleep.conf"
fi

cp -v /tmp/hmi-sleep-pkg/lib/systemd/system/hmi-sleep.service "$SD_TARGET/lib/systemd/system/hmi-sleep.service"
cp -v /tmp/hmi-app-pkg/lib/systemd/system/hmi-app.service "$SD_TARGET/lib/systemd/system/hmi-app.service"

echo "[5/6] Enabling auto-flash-emmc trigger & configuring OTA server..."
touch "$SD_TARGET/etc/auto-flash-emmc"
echo "Created trigger flag: $SD_TARGET/etc/auto-flash-emmc"

cat <<EOF > "$SD_TARGET/etc/ota-server.conf"
OTA_SERVER_URL="http://192.168.0.110:8000"
CHECK_INTERVAL=30
EOF
echo "Configured OTA server: http://192.168.0.110:8000 in $SD_TARGET/etc/ota-server.conf"

mkdir -p "$SD_TARGET/etc/systemd/system/multi-user.target.wants"
ln -sf /lib/systemd/system/hmi-app.service "$SD_TARGET/etc/systemd/system/multi-user.target.wants/hmi-app.service"
ln -sf /lib/systemd/system/hmi-sleep.service "$SD_TARGET/etc/systemd/system/multi-user.target.wants/hmi-sleep.service"
ln -sf /lib/systemd/system/emmc-autoinstall.service "$SD_TARGET/etc/systemd/system/multi-user.target.wants/emmc-autoinstall.service"

echo "[6/6] Syncing disk caches to SD card..."
sync

echo ""
echo "==========================================================="
echo " SUCCESS: SD Card rootfs_a has been updated with:"
echo "   ✓ New hmi-app (with UART, Sleep Timer, Paths, & SPI Test)"
echo "   ✓ set-hmi-app CLI helper"
echo "   ✓ hmi-sleep-daemon & sleep service"
echo "   ✓ Automatic eMMC Factory Flasher (/etc/auto-flash-emmc ENABLED)"
echo "   ✓ OTA Server configured to http://192.168.0.110:8000"
echo "==========================================================="
