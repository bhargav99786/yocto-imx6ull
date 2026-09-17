SUMMARY = "Early Kernel Boot Loading Screen for MarknStamp HMI"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI = " \
    file://loading.bmp \
    file://loading_32bpp.raw \
    file://loading_16bpp.raw \
    file://fbshow.c \
    file://boot-splash.service \
"

S = "${WORKDIR}"

inherit systemd

SYSTEMD_SERVICE:${PN} = "boot-splash.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} ${WORKDIR}/fbshow.c -o ${B}/fbshow
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${B}/fbshow ${D}${bindir}/fbshow

    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/loading.bmp ${D}${sysconfdir}/loading.bmp
    install -m 0644 ${WORKDIR}/loading_32bpp.raw ${D}${sysconfdir}/loading_32bpp.raw
    install -m 0644 ${WORKDIR}/loading_16bpp.raw ${D}${sysconfdir}/loading_16bpp.raw

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/boot-splash.service ${D}${systemd_system_unitdir}/boot-splash.service
}

FILES:${PN} += " \
    ${bindir}/fbshow \
    ${sysconfdir}/loading.bmp \
    ${sysconfdir}/loading_32bpp.raw \
    ${sysconfdir}/loading_16bpp.raw \
    ${systemd_system_unitdir}/boot-splash.service \
"
