SUMMARY = "Custom Qt Application Launcher with Power-On USB Auto-Update"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS += "qtbase"
RDEPENDS:${PN} += "qtbase qtbase-plugins"

# Skip file-rdeps QA check for prebuilt binary
INSANE_SKIP:${PN} += "file-rdeps"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI = " \
    file://qt-auto-launcher.sh \
    file://qt-auto-launcher.service \
    file://test \
"

S = "${WORKDIR}"

inherit systemd

SYSTEMD_SERVICE:${PN} = "qt-auto-launcher.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/qt-auto-launcher.sh ${D}${bindir}/qt-auto-launcher.sh

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/qt-auto-launcher.service ${D}${systemd_system_unitdir}/qt-auto-launcher.service

    # Install default ready test binary to /opt/hmi/bin/app
    install -d ${D}/opt/hmi/bin
    install -m 0755 ${WORKDIR}/test ${D}/opt/hmi/bin/app
}

FILES:${PN} += " \
    ${bindir}/qt-auto-launcher.sh \
    ${systemd_system_unitdir}/qt-auto-launcher.service \
    /opt/hmi \
    /opt/hmi/bin \
    /opt/hmi/bin/app \
"
