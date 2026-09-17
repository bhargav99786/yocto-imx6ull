SUMMARY = "Automatic eMMC installer for factory flashing"
DESCRIPTION = "Automatically flashes rootfs and bootloader to onboard eMMC on first SD boot"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://emmc-autoinstall.service \
    file://auto-flash-to-emmc \
"

inherit systemd

SYSTEMD_SERVICE:${PN} = "emmc-autoinstall.service"
SYSTEMD_AUTO_ENABLE = "enable"

RDEPENDS:${PN} = " \
    e2fsprogs-mke2fs \
    util-linux-findmnt \
    tar \
    coreutils \
"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/auto-flash-to-emmc ${D}${bindir}/auto-flash-to-emmc
    ln -sf auto-flash-to-emmc ${D}${bindir}/flash_to_emmc.sh

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/emmc-autoinstall.service ${D}${systemd_system_unitdir}/emmc-autoinstall.service

    # Create the first-boot trigger flag in /etc
    install -d ${D}${sysconfdir}
    touch ${D}${sysconfdir}/auto-flash-emmc
}

FILES:${PN} += " \
    ${bindir}/auto-flash-to-emmc \
    ${bindir}/flash_to_emmc.sh \
    ${systemd_system_unitdir}/emmc-autoinstall.service \
    ${sysconfdir}/auto-flash-emmc \
"
