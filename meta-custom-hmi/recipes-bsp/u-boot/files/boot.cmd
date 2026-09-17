echo "=== Loading MarknStamp HMI Linux System ==="

# Determine boot device (SD card mmc 0 vs eMMC mmc 1)
if test -z "${mmcdev}"; then
    setenv mmcdev 0
fi

if test "${mmcdev}" = "1"; then
    setenv mmcroot '/dev/mmcblk1p2 rootwait rw'
else
    setenv mmcroot '/dev/mmcblk0p2 rootwait rw'
fi

# Suppress console flicker, disable kernel PPM logo (logo.nologo), hide VT cursor
setenv bootargs console=ttymxc0,115200 root=${mmcroot} quiet loglevel=0 vt.global_cursor_default=0 logo.nologo systemd.mask=getty@tty1.service
setenv splashimage 0x83800000
setenv splashpos m,m

# Only load splash.bmp if logo was not already rendered by U-Boot video init
if test -z "${videodone}"; then
    if load mmc ${mmcdev}:1 ${splashimage} splash.bmp; then
        bmp display ${splashimage}
    fi
fi

load mmc ${mmcdev}:1 ${loadaddr} zImage

if load mmc ${mmcdev}:1 ${fdt_addr} okmx6ull-c-emmc.dtb; then
    bootz ${loadaddr} - ${fdt_addr}
elif load mmc ${mmcdev}:1 ${fdt_addr} imx6ull-14x14-evk.dtb; then
    bootz ${loadaddr} - ${fdt_addr}
elif load mmc ${mmcdev}:1 ${fdt_addr} imx6ull-custom-hmi.dtb; then
    bootz ${loadaddr} - ${fdt_addr}
fi
