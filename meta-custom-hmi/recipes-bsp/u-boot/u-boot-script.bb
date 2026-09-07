SUMMARY = "U-Boot boot script for OKMX6ULL-C HMI"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS = "u-boot-mkimage-native"

SRC_URI = "file://boot.cmd"

inherit deploy

do_compile() {
    mkimage -A arm -T script -C none -n "OKMX6ULL-C Boot Script" -d ${WORKDIR}/boot.cmd ${B}/boot.scr
}

do_deploy() {
    install -d ${DEPLOYDIR}
    install -m 0644 ${B}/boot.scr ${DEPLOYDIR}/boot.scr
}

addtask do_deploy after do_compile before do_build
