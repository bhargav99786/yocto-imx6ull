SUMMARY = "systemd networkd AutoIP and fast network configuration"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://10-eth.network"

do_install:append() {
    install -d ${D}${sysconfdir}/systemd/network
    install -m 0644 ${WORKDIR}/10-eth.network ${D}${sysconfdir}/systemd/network/10-eth.network
    
    # Permanently disable getty login prompt on tty1 (LCD screen)
    install -d ${D}${sysconfdir}/systemd/system
    ln -sf /dev/null ${D}${sysconfdir}/systemd/system/getty@tty1.service
}

FILES:${PN} += "${sysconfdir}/systemd/network/10-eth.network ${sysconfdir}/systemd/system/getty@tty1.service"
