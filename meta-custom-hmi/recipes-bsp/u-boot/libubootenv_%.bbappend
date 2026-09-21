FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " \
    file://fw_env.config \
    file://u-boot-initial-env \
"

do_install:append() {
    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/fw_env.config ${D}${sysconfdir}/fw_env.config
    install -m 0644 ${WORKDIR}/u-boot-initial-env ${D}${sysconfdir}/u-boot-initial-env
}

FILES:${PN}-bin += " \
    ${sysconfdir}/fw_env.config \
    ${sysconfdir}/u-boot-initial-env \
"
