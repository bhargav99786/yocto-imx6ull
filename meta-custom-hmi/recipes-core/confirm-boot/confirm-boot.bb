SUMMARY = "Confirm clean system boot to disarm SWUpdate rollback watchdog"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://confirm-boot \
    file://confirm-boot.service \
"

inherit systemd

SYSTEMD_SERVICE:${PN} = "confirm-boot.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

RDEPENDS:${PN} += "libubootenv-bin"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/confirm-boot ${D}${bindir}/confirm-boot

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/confirm-boot.service ${D}${systemd_system_unitdir}/confirm-boot.service
}

FILES:${PN} += " \
    ${bindir}/confirm-boot \
    ${systemd_system_unitdir}/confirm-boot.service \
"
