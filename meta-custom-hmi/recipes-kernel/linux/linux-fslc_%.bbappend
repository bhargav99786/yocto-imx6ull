FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://okmx6ull-c-emmc.dts \
    file://imx6ull-custom-hmi.dts \
    file://0001-goodix-split-i2c-transfer-to-prevent-repeated-start-.patch \
    file://logo_linux_clut224.ppm \
"

do_configure:append() {
    cp ${WORKDIR}/okmx6ull-c-emmc.dts ${S}/arch/arm/boot/dts/
    cp ${WORKDIR}/imx6ull-custom-hmi.dts ${S}/arch/arm/boot/dts/
    cp ${WORKDIR}/logo_linux_clut224.ppm ${S}/drivers/video/logo/logo_linux_clut224.ppm
}

KERNEL_DEVICETREE += "okmx6ull-c-emmc.dtb imx6ull-custom-hmi.dtb"
