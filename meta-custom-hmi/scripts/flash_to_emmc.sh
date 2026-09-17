#!/bin/sh
# ============================================================
# MarknStamp HMI - Flash System to Onboard eMMC (/dev/mmcblk1)
# Run this script directly on the OKMX6ULL-C board while booted from SD card!
# ============================================================

set -e

EMMC_DEV="/dev/mmcblk1"
EMMC_BOOT_PART="/dev/mmcblk1p1"
EMMC_ROOT_PART="/dev/mmcblk1p2"

echo "======================================================="
echo " Flashing MarknStamp HMI to eMMC ($EMMC_DEV)"
echo "======================================================="

# Safety check: ensure we are running from SD card (mmcblk0)
CURRENT_ROOT=$(findmnt -n -o SOURCE /)
if [ "$CURRENT_ROOT" = "$EMMC_ROOT_PART" ]; then
    echo "ERROR: You are currently booted from eMMC! Cannot flash self."
    exit 1
fi

echo "Current root is $CURRENT_ROOT. Target is $EMMC_DEV."

# 1. Update boot partition on eMMC
echo "--> [1/4] Copying Boot partition files to $EMMC_BOOT_PART..."
mkdir -p /mnt/boot
mount /dev/mmcblk0p1 /mnt/boot 2>/dev/null || true

mkdir -p /mnt/emmc_boot
mount "$EMMC_BOOT_PART" /mnt/emmc_boot

cp -v /mnt/boot/boot.scr /mnt/emmc_boot/boot.scr
cp -v /mnt/boot/splash.bmp /mnt/emmc_boot/splash.bmp
cp -v /mnt/boot/splash.bmp /mnt/emmc_boot/logo.bmp
cp -v /mnt/boot/zImage /mnt/emmc_boot/zImage
cp -v /mnt/boot/*.dtb /mnt/emmc_boot/ 2>/dev/null || true

sync
umount /mnt/emmc_boot
echo "Boot partition updated successfully."

# 2. Format eMMC rootfs partition as ext4
echo "--> [2/4] Formatting $EMMC_ROOT_PART as ext4..."
mkfs.ext4 -F -L "rootfs" "$EMMC_ROOT_PART"
sync

# 3. Clone Root Filesystem to eMMC
echo "--> [3/4] Cloning root filesystem to $EMMC_ROOT_PART..."
mkdir -p /mnt/emmc_root
mount "$EMMC_ROOT_PART" /mnt/emmc_root

cd /
tar --exclude='./dev' \
    --exclude='./proc' \
    --exclude='./sys' \
    --exclude='./tmp' \
    --exclude='./run' \
    --exclude='./mnt' \
    --exclude='./media' \
    --exclude='./lost+found' \
    -cf - . | (cd /mnt/emmc_root && tar xpf -)

# Create missing mountpoint directories
mkdir -p /mnt/emmc_root/dev /mnt/emmc_root/proc /mnt/emmc_root/sys /mnt/emmc_root/tmp /mnt/emmc_root/run /mnt/emmc_root/mnt /mnt/emmc_root/media
chmod 1777 /mnt/emmc_root/tmp

sync
umount /mnt/emmc_root
echo "Root filesystem cloned successfully."

# 4. Flash U-Boot to eMMC boot partition
echo "--> [4/4] Flashing U-Boot to eMMC boot partition..."
if [ -f /mnt/boot/u-boot.imx ]; then
    echo 0 > /sys/block/mmcblk1boot0/force_ro 2>/dev/null || true
    dd if=/mnt/boot/u-boot.imx of=/dev/mmcblk1boot0 bs=1K seek=1 conv=fsync status=progress
    echo 1 > /sys/block/mmcblk1boot0/force_ro 2>/dev/null || true
    dd if=/mnt/boot/u-boot.imx of=/dev/mmcblk1 bs=1K seek=1 conv=fsync status=progress
    echo "U-Boot flashed to eMMC."
else
    echo "Notice: /mnt/boot/u-boot.imx not found, keeping existing eMMC U-Boot."
fi

sync

echo ""
echo "======================================================="
echo " eMMC FLASH COMPLETED SUCCESSFULLY!"
echo "======================================================="
echo "To boot from eMMC:"
echo " 1. Power off board"
echo " 2. Remove the SD card (or flip boot switches to eMMC boot)"
echo " 3. Power on"
echo "======================================================="
