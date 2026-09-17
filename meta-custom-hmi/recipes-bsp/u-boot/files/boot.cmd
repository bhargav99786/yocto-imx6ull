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

setenv bootargs console=ttymxc0,115200 root=${mmcroot} quiet loglevel=0 vt.global_cursor_default=0 systemd.mask=getty@tty1.service
setenv splashimage 0x83800000
setenv splashpos m,m

# Load user-replaceable BMP splash image from FAT boot partition
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
