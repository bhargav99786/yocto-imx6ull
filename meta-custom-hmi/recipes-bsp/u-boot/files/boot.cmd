echo "=== Loading MarknStamp HMI Linux System ==="
setenv mmcdev 0
setenv mmcroot '/dev/mmcblk0p2 rootwait rw'
setenv bootargs console=ttymxc0,115200 root=${mmcroot} quiet
load mmc 0:1 ${loadaddr} zImage
if load mmc 0:1 ${fdt_addr} okmx6ull-c-emmc.dtb; then
    bootz ${loadaddr} - ${fdt_addr}
elif load mmc 0:1 ${fdt_addr} imx6ull-14x14-evk.dtb; then
    bootz ${loadaddr} - ${fdt_addr}
elif load mmc 0:1 ${fdt_addr} imx6ull-custom-hmi.dtb; then
    bootz ${loadaddr} - ${fdt_addr}
fi
