echo "=== Loading MarknStamp HMI Linux System ==="
setenv mmcdev 0
setenv mmcroot '/dev/mmcblk0p2 rootwait rw'
setenv bootargs console=ttymxc0,115200 root=${mmcroot} quiet loglevel=0 vt.global_cursor_default=0 systemd.mask=getty@tty1.service
setenv splashimage 0x89000000
setenv splashpos m,m

# Load and render MarknStamp Logo (1024x600 BMP) in U-Boot within 1 second
if fatload mmc 0:1 0x89000000 logo.bmp; then
    bmp display 0x89000000
fi

load mmc 0:1 ${loadaddr} zImage
if load mmc 0:1 ${fdt_addr} okmx6ull-c-emmc.dtb; then
    bootz ${loadaddr} - ${fdt_addr}
elif load mmc 0:1 ${fdt_addr} imx6ull-14x14-evk.dtb; then
    bootz ${loadaddr} - ${fdt_addr}
elif load mmc 0:1 ${fdt_addr} imx6ull-custom-hmi.dtb; then
    bootz ${loadaddr} - ${fdt_addr}
fi
