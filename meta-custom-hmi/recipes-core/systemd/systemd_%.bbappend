SUMMARY = "systemd networkd AutoIP dual ethernet configuration"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Two separate .network files — one per interface (required by systemd-networkd)
# eth0 = fec1 / ENET1,  eth1 = fec2 / ENET2
# Both use Auto-IP (IPv4 Link-Local 169.254.x.x), no DHCP
SRC_URI += " \
    file://10-eth0.network \
    file://11-eth1.network \
    file://99-touchscreen.rules \
"

do_install:append() {
    install -d ${D}${sysconfdir}/systemd/network
    install -m 0644 ${WORKDIR}/10-eth0.network ${D}${sysconfdir}/systemd/network/10-eth0.network
    install -m 0644 ${WORKDIR}/11-eth1.network ${D}${sysconfdir}/systemd/network/11-eth1.network

    # Remove old combined file if it exists (replaced by per-interface files)
    rm -f ${D}${sysconfdir}/systemd/network/10-eth.network

    # Permanently disable getty login prompt on tty1 (LCD screen — Qt HMI handles display)
    install -d ${D}${sysconfdir}/systemd/system
    ln -sf /dev/null ${D}${sysconfdir}/systemd/system/getty@tty1.service

    # Install corrected touchscreen udev rule (identity matrix — no axis swap)
    install -d ${D}${sysconfdir}/udev/rules.d
    install -m 0644 ${WORKDIR}/99-touchscreen.rules ${D}${sysconfdir}/udev/rules.d/99-touchscreen.rules
}

FILES:${PN} += " \
    ${sysconfdir}/systemd/network/10-eth0.network \
    ${sysconfdir}/systemd/network/11-eth1.network \
    ${sysconfdir}/systemd/system/getty@tty1.service \
    ${sysconfdir}/udev/rules.d/99-touchscreen.rules \
"
