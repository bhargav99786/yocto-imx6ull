#!/bin/sh
# SWUpdate Post-Update Script
# Toggles U-Boot active partition variable upon successful OTA installation

CURRENT_ROOT=$(fw_printenv active_rootfs | cut -d'=' -f2)

if [ "$CURRENT_ROOT" = "rootfs_a" ]; then
    echo "Switching active boot target to rootfs_b..."
    fw_setenv active_rootfs rootfs_b
    fw_setenv mmcdev 1
    fw_setenv mmcpart 3
else
    echo "Switching active boot target to rootfs_a..."
    fw_setenv active_rootfs rootfs_a
    fw_setenv mmcdev 1
    fw_setenv mmcpart 2
fi

fw_setenv upgrade_available 1
fw_setenv bootlimit 3
fw_setenv bootcount 0

echo "OTA update applied successfully. System will boot into new partition on restart."
exit 0
