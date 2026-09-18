SUMMARY = "Custom Qt 5.15 HMI Desktop Application"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS += "qtbase"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI = " \
    file://hmi-app.pro \
    file://main.cpp \
    file://loginwindow.h \
    file://loginwindow.cpp \
    file://mainwindow.h \
    file://mainwindow.cpp \
    file://touchcanvas.h \
    file://touchcanvas.cpp \
    file://numpaddialog.h \
    file://numpaddialog.cpp \
    file://hmi-app.service \
    file://hmi-app.init \
    file://99-goodix.rules \
    file://hmi-session.conf \
    file://hmi-session-launcher \
"

S = "${WORKDIR}"

inherit qmake5 systemd update-rc.d

SYSTEMD_SERVICE:${PN} = "hmi-app.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

INITSCRIPT_NAME = "hmi-app"
INITSCRIPT_PARAMS = "defaults 99"

do_install:append() {
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/hmi-app.init ${D}${sysconfdir}/init.d/hmi-app

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/hmi-app.service ${D}${systemd_system_unitdir}/hmi-app.service

    install -d ${D}${sysconfdir}/udev/rules.d
    install -m 0644 ${WORKDIR}/99-goodix.rules ${D}${sysconfdir}/udev/rules.d/99-goodix.rules

    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/hmi-session.conf ${D}${sysconfdir}/hmi-session.conf

    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/hmi-session-launcher ${D}${bindir}/hmi-session-launcher

    install -d ${D}/opt/hmi/bin
    ln -sf /usr/bin/hmi-app ${D}/opt/hmi/bin/app

    install -d ${D}/usr/lib
    ln -sf /usr/share/fonts/truetype ${D}/usr/lib/fonts
}

FILES:${PN} += " \
    ${bindir}/hmi-app \
    ${bindir}/hmi-session-launcher \
    ${sysconfdir}/hmi-session.conf \
    ${sysconfdir}/init.d/hmi-app \
    ${systemd_system_unitdir}/hmi-app.service \
    ${sysconfdir}/udev/rules.d/99-goodix.rules \
    /usr/lib/fonts \
    /opt/hmi \
    /opt/hmi/bin \
    /opt/hmi/bin/app \
"
