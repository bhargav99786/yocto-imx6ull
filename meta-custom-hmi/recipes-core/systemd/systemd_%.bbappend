SUMMARY = "systemd networkd AutoIP and fast network configuration"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://10-eth.network"

do_install:append() {
    install -d ${D}${sysconfdir}/systemd/network
    install -m 0644 ${WORKDIR}/10-eth.network ${D}${sysconfdir}/systemd/network/10-eth.network
}

FILES:${PN} += "${sysconfdir}/systemd/network/10-eth.network"
