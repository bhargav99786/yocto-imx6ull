echo "=== Loading MarknStamp HMI Linux System ==="

# Determine boot device (SD card mmc 0 vs eMMC mmc 1)
if test -z "${mmcdev}"; then
    setenv mmcdev 0
fi

# A/B Dual-Bank Active RootFS Selection
if test -z "${active_rootfs}"; then
    setenv active_rootfs "rootfs_a"
fi

# Watchdog / Bootcount check for Fail-Safe Rollback
if test "${upgrade_available}" = "1"; then
    setexpr bootcount ${bootcount} + 1
    saveenv
    if test ${bootcount} -gt ${bootlimit}; then
        echo "OTA boot failed! Rolling back to previous rootfs bank..."
        if test "${active_rootfs}" = "rootfs_b"; then
            setenv active_rootfs "rootfs_a"
        else
            setenv active_rootfs "rootfs_b"
        fi
        setenv upgrade_available 0
        saveenv
    fi
fi

if test "${mmcdev}" = "1"; then
    if test "${active_rootfs}" = "rootfs_b"; then
        echo "--> Booting RootFS Bank B (/dev/mmcblk1p3)..."
        setenv mmcroot '/dev/mmcblk1p3 rootwait rw'
    else
        echo "--> Booting RootFS Bank A (/dev/mmcblk1p2)..."
        setenv mmcroot '/dev/mmcblk1p2 rootwait rw'
    fi
else
    setenv mmcroot '/dev/mmcblk0p2 rootwait rw'
fi

# Suppress console flicker, disable kernel PPM logo (logo.nologo), hide VT cursor, isolate fbcon
setenv bootargs console=ttymxc0,115200 root=${mmcroot} quiet loglevel=0 vt.global_cursor_default=0 logo.nologo fbcon=map:9 systemd.mask=getty@tty1.service
setenv splashimage 0x83800000
setenv splashpos m,m

# Load user-replaceable BMP splash screen from FAT boot partition
if load mmc ${mmcdev}:1 ${splashimage} splash.bmp; then
    bmp display ${splashimage}
elif load mmc ${mmcdev}:1 ${splashimage} logo.bmp; then
    bmp display ${splashimage}
fi

load mmc ${mmcdev}:1 ${loadaddr} zImage

if load mmc ${mmcdev}:1 ${fdt_addr} okmx6ull-c-emmc.dtb; then
    bootz ${loadaddr} - ${fdt_addr}
elif load mmc ${mmcdev}:1 ${fdt_addr} imx6ull-14x14-evk.dtb; then
    bootz ${loadaddr} - ${fdt_addr}
elif load mmc ${mmcdev}:1 ${fdt_addr} imx6ull-custom-hmi.dtb; then
    bootz ${loadaddr} - ${fdt_addr}
fi
