SUMMARY = "Goodix GT911 firmware configuration file to prevent sysfs boot wait"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit allarch

do_install() {
    install -d ${D}/lib/firmware
    touch ${D}/lib/firmware/goodix_911_cfg.bin
}

FILES:${PN} = "/lib/firmware/goodix_911_cfg.bin"
