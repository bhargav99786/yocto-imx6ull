#!/bin/sh
# SWUpdate Post-Update Script
# Toggles U-Boot active partition variable upon successful OTA installation

if [ "$1" = "preinst" ]; then
    exit 0
fi

CURRENT_PART=$(findmnt -n -o SOURCE / 2>/dev/null || mount | grep " / " | cut -d' ' -f1)
CURRENT_ROOT=$(fw_printenv active_rootfs 2>/dev/null | cut -d'=' -f2)

if [ "$CURRENT_ROOT" = "rootfs_a" ] || echo "$CURRENT_PART" | grep -q "p2"; then
    echo "[SWUpdate] Switching active boot target to rootfs_b..."
    fw_setenv active_rootfs rootfs_b 2>/dev/null || true
    fw_setenv mmcpart 3 2>/dev/null || true
else
    echo "[SWUpdate] Switching active boot target to rootfs_a..."
    fw_setenv active_rootfs rootfs_a 2>/dev/null || true
    fw_setenv mmcpart 2 2>/dev/null || true
fi

fw_setenv upgrade_available 1 2>/dev/null || true
fw_setenv bootlimit 3 2>/dev/null || true
fw_setenv bootcount 0 2>/dev/null || true

echo "[SWUpdate] OTA update applied successfully. System will boot into new partition on restart."
exit 0
