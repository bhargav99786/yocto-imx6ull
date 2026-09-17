#!/bin/sh
# SWUpdate Pre-Update Script
# Dynamically determines inactive partition and sets /dev/update_target symlink

CURRENT_ROOT=$(fw_printenv active_rootfs 2>/dev/null | cut -d'=' -f2)

if [ "$CURRENT_ROOT" = "rootfs_b" ]; then
    echo "[SWUpdate] Active rootfs is B. Target partition for update is /dev/mmcblk1p2 (rootfs_a)"
    ln -sf /dev/mmcblk1p2 /dev/update_target
else
    echo "[SWUpdate] Active rootfs is A. Target partition for update is /dev/mmcblk1p3 (rootfs_b)"
    ln -sf /dev/mmcblk1p3 /dev/update_target
fi

exit 0
