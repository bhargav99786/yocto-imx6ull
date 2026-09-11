FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://okmx6ull-c-emmc.dts \
    file://imx6ull-custom-hmi.dts \
    file://0001-add-gt9xx-touchscreen-driver.patch \
    file://logo_linux_clut224.ppm \
"

do_configure:append() {
    cp ${WORKDIR}/okmx6ull-c-emmc.dts ${S}/arch/arm/boot/dts/
    cp ${WORKDIR}/imx6ull-custom-hmi.dts ${S}/arch/arm/boot/dts/
    cp ${WORKDIR}/logo_linux_clut224.ppm ${S}/drivers/video/logo/logo_linux_clut224.ppm
    echo "CONFIG_TOUCHSCREEN_GT9xx=y" >> ${B}/.config
}

KERNEL_DEVICETREE += "okmx6ull-c-emmc.dtb imx6ull-custom-hmi.dtb"
