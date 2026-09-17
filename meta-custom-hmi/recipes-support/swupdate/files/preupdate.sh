#!/bin/sh
# SWUpdate Pre-Update Script
# Dynamically determines inactive partition and sets /dev/update_target symlink

if [ "$1" = "postinst" ]; then
    exit 0
fi

# Detect current root disk (/dev/mmcblk0 or /dev/mmcblk1)
CURRENT_PART=$(findmnt -n -o SOURCE / 2>/dev/null || mount | grep " / " | cut -d' ' -f1)
DISK=$(echo "$CURRENT_PART" | sed -E 's/p[0-9]+$//')

CURRENT_ROOT=$(fw_printenv active_rootfs 2>/dev/null | cut -d'=' -f2)

if [ "$CURRENT_ROOT" = "rootfs_b" ] || echo "$CURRENT_PART" | grep -q "p3"; then
    echo "[SWUpdate] Active rootfs is B. Target partition is ${DISK}p2 (rootfs_a)"
    ln -sf ${DISK}p2 /dev/update_target
else
    echo "[SWUpdate] Active rootfs is A. Target partition is ${DISK}p3 (rootfs_b)"
    ln -sf ${DISK}p3 /dev/update_target
fi

exit 0
