# ============================================================
# MarknStamp HMI - Universal A/B Dual-Bank Boot Script
# Supports both SD Card (mmc 0) and Onboard eMMC (mmc 1)
# ============================================================

# 1. Determine boot device (SD card mmc 0 vs eMMC mmc 1)
if test -z "${mmcdev}"; then
    setenv mmcdev 0
fi

# 2. Display initial power-on boot logo immediately:
if load mmc ${mmcdev}:1 0x88000000 logo.bmp || load mmc ${mmcdev}:1 0x88000000 splash.bmp; then
    bmp display 0x88000000 || true
    gpio set 8 || true
fi

# 2. Determine active A/B rootfs bank
if test -z "${active_rootfs}"; then
    setenv active_rootfs "rootfs_a"
fi

if test "${active_rootfs}" = "rootfs_b"; then
    setenv active_part 3
else
    setenv active_part 2
fi

setenv rootfs_dev /dev/mmcblk${mmcdev}p${active_part}
echo "=== Booting MarknStamp HMI (mmc ${mmcdev}, ${active_rootfs} -> ${rootfs_dev}) ==="

# 3. Kernel bootargs
setenv bootargs "console=ttymxc0,115200 root=${rootfs_dev} rootwait rw quiet loglevel=0 vt.global_cursor_default=0 logo.nologo fbcon=map:9"

# 4. Watchdog rollback handling for OTA fail-safe
if test "${upgrade_available}" = "1"; then
    setexpr bootcount ${bootcount} + 1
    saveenv
    if test ${bootcount} -gt ${bootlimit}; then
        echo "=== OTA UPDATE FAILED! Rolling back to alternate bank ==="
        if test "${active_rootfs}" = "rootfs_b"; then
            setenv active_rootfs rootfs_a
        else
            setenv active_rootfs rootfs_b
        fi
        setenv upgrade_available 0
        setenv bootcount 0
        saveenv
        reset
    fi
fi

# 5. Memory addresses
setenv loadaddr 0x82000000
setenv fdt_addr 0x83000000

# 6. Load kernel zImage and Device Tree WHILE the logo is still on screen:
load mmc ${mmcdev}:1 ${loadaddr} zImage
if load mmc ${mmcdev}:1 ${fdt_addr} okmx6ull-c-emmc.dtb; then
    echo "Loaded okmx6ull-c-emmc.dtb"
elif load mmc ${mmcdev}:1 ${fdt_addr} imx6ull-custom-hmi.dtb; then
    echo "Loaded imx6ull-custom-hmi.dtb"
fi

# Hold the clean, centered power-on logo visible for total 2 seconds:
sleep 1.5

# 7. Clean handoff right before launching the kernel:
# Wipe full 4MB (0x100000 words) so all 600 lines are black (NO bottom lines remaining!):
mw.l 0x87b00000 0x0 0x100000
# Turn off backlight so kernel boot transition is 100% flicker-free:
gpio clear 8 || true

# 8. Boot kernel immediately:
bootz ${loadaddr} - ${fdt_addr}
