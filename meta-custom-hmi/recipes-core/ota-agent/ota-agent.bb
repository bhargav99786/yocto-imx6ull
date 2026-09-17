SUMMARY = "Remote OTA update client agent service"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://ota-update-agent \
    file://ota-server.conf \
    file://ota-agent.service \
"

inherit systemd

SYSTEMD_SERVICE:${PN} = "ota-agent.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

RDEPENDS:${PN} += "curl swupdate libubootenv-bin bash"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/ota-update-agent ${D}${bindir}/ota-update-agent

    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/ota-server.conf ${D}${sysconfdir}/ota-server.conf

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/ota-agent.service ${D}${systemd_system_unitdir}/ota-agent.service
}

FILES:${PN} += " \
    ${bindir}/ota-update-agent \
    ${sysconfdir}/ota-server.conf \
    ${systemd_system_unitdir}/ota-agent.service \
"
