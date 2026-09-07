SUMMARY = "Instant Power-On Fullscreen Boot Splash for MarknStamp HMI"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI = " \
    file://boot_logo.raw \
    file://boot-splash.service \
"

S = "${WORKDIR}"

inherit systemd

SYSTEMD_SERVICE:${PN} = "boot-splash.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install() {
    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/boot_logo.raw ${D}${sysconfdir}/boot_logo.raw

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/boot-splash.service ${D}${systemd_system_unitdir}/boot-splash.service
}

FILES:${PN} += " \
    ${sysconfdir}/boot_logo.raw \
    ${systemd_system_unitdir}/boot-splash.service \
"
