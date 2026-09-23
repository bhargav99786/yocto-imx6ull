SUMMARY = "MarknStamp HMI Display Sleep and Touch Wake Daemon"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://hmi-sleep-daemon.c \
    file://hmi-sleep.conf \
    file://hmi-sleep.service \
"

S = "${WORKDIR}"

inherit systemd

SYSTEMD_SERVICE:${PN} = "hmi-sleep.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} hmi-sleep-daemon.c -o hmi-sleep-daemon
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${S}/hmi-sleep-daemon ${D}${bindir}/hmi-sleep-daemon

    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/hmi-sleep.conf ${D}${sysconfdir}/hmi-sleep.conf

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/hmi-sleep.service ${D}${systemd_system_unitdir}/hmi-sleep.service
}

CONFFILES:${PN} = "${sysconfdir}/hmi-sleep.conf"

FILES:${PN} = " \
    ${bindir}/hmi-sleep-daemon \
    ${sysconfdir}/hmi-sleep.conf \
    ${systemd_system_unitdir}/hmi-sleep.service \
"
