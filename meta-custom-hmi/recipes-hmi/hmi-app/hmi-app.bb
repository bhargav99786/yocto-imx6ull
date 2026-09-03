SUMMARY = "Custom Qt 5.15 HMI Application Launcher Service"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI = "file://hmi-app.service"

inherit systemd

SYSTEMD_SERVICE:${PN} = "hmi-app.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install() {
    install -d ${D}/opt/hmi
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/hmi-app.service ${D}${systemd_system_unitdir}/
}

FILES:${PN} += "/opt/hmi ${systemd_system_unitdir}/hmi-app.service"
