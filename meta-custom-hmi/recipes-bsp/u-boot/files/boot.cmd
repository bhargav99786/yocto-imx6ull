echo "=== Loading MarknStamp HMI Linux System ==="
setenv mmcdev 0
setenv mmcroot '/dev/mmcblk0p2 rootwait rw'
setenv bootargs console=tty1 console=ttymxc0,115200 root=${mmcroot} quiet vt.global_cursor_default=0 systemd.mask=getty@tty1.service
setenv splashimage 0x89000000
setenv splashpos m,m

# Load and render U-Boot Splash Logo (1024x600 BMP) within 1 second of power-on
if load mmc 0:1 ${splashimage} logo.bmp; then
    bmp display ${splashimage}
fi

load mmc 0:1 ${loadaddr} zImage
if load mmc 0:1 ${fdt_addr} okmx6ull-c-emmc.dtb; then
    bootz ${loadaddr} - ${fdt_addr}
elif load mmc 0:1 ${fdt_addr} imx6ull-14x14-evk.dtb; then
    bootz ${loadaddr} - ${fdt_addr}
elif load mmc 0:1 ${fdt_addr} imx6ull-custom-hmi.dtb; then
    bootz ${loadaddr} - ${fdt_addr}
fi
