# Determine boot device (SD card mmc 0 vs eMMC mmc 1)
if test -z "${mmcdev}"; then
    setenv mmcdev 0
fi

if test "${mmcdev}" = "0"; then
    echo "=== Booting MarknStamp HMI from SD Card (/dev/mmcblk0p2) ==="
    setenv rootfs_dev /dev/mmcblk0p2
else
    if test "${active_rootfs}" = "rootfs_b"; then
        echo "=== Booting MarknStamp HMI Bank B (/dev/mmcblk1p3) ==="
        setenv rootfs_dev /dev/mmcblk1p3
    else
        echo "=== Booting MarknStamp HMI Bank A (/dev/mmcblk1p2) ==="
        setenv rootfs_dev /dev/mmcblk1p2
    fi
fi

setenv bootargs "console=ttymxc0,115200 root=${rootfs_dev} rootwait rw quiet loglevel=0 vt.global_cursor_default=0 logo.nologo fbcon=map:9"

# Watchdog rollback handling (only for eMMC boot)
if test "${mmcdev}" = "1"; then
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
fi

setenv splashimage 0x83800000
setenv splashpos m,m

# Load user-replaceable BMP splash screen from current boot partition 1
if load mmc ${mmcdev}:1 ${splashimage} splash.bmp; then
    echo "Loaded splash.bmp from mmc ${mmcdev}:1"
elif load mmc ${mmcdev}:1 ${splashimage} logo.bmp; then
    echo "Loaded logo.bmp from mmc ${mmcdev}:1"
fi

# Load zImage and DTB from current boot partition 1
load mmc ${mmcdev}:1 ${loadaddr} zImage
if load mmc ${mmcdev}:1 ${fdt_addr} okmx6ull-c-emmc.dtb; then
    echo "Loaded okmx6ull-c-emmc.dtb"
elif load mmc ${mmcdev}:1 ${fdt_addr} imx6ull-custom-hmi.dtb; then
    echo "Loaded imx6ull-custom-hmi.dtb"
fi

bootz ${loadaddr} - ${fdt_addr}
