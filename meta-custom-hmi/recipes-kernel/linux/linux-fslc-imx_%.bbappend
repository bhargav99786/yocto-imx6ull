FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://imx6ull-custom-hmi.dts"

do_configure:append() {
    cp ${WORKDIR}/imx6ull-custom-hmi.dts ${S}/arch/arm/boot/dts/
}

KERNEL_DEVICETREE += "imx6ull-custom-hmi.dtb"
