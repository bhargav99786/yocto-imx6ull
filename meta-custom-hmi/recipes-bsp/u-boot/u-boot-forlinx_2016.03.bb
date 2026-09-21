SUMMARY = "U-Boot bootloader with OKMX6ULL-C LCDIF 1024x600 video support"
DESCRIPTION = "Factory-tested U-Boot for OKMX6ULL-C supporting early 1024x600 display and logo"
LICENSE = "GPL-2.0-or-later"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

PROVIDES += "u-boot virtual/bootloader"

SRC_URI = "file://factory_u-boot.imx"

S = "${WORKDIR}"

inherit deploy

do_compile[noexec] = "1"
do_install[noexec] = "1"

do_deploy() {
    install -d ${DEPLOYDIR}
    install -m 0644 ${WORKDIR}/factory_u-boot.imx ${DEPLOYDIR}/u-boot.imx
    install -m 0644 ${WORKDIR}/factory_u-boot.imx ${DEPLOYDIR}/u-boot-imx6ullevk.imx
    install -m 0644 ${WORKDIR}/factory_u-boot.imx ${DEPLOYDIR}/u-boot-imx6ullevk.imx-sd
    install -m 0644 ${WORKDIR}/factory_u-boot.imx ${DEPLOYDIR}/u-boot-sd.imx
    install -m 0644 ${WORKDIR}/factory_u-boot.imx ${DEPLOYDIR}/u-boot-${MACHINE}.imx
}

addtask do_deploy before do_build after do_compile

COMPATIBLE_MACHINE = "(imx6ullevk|imx6ull)"
