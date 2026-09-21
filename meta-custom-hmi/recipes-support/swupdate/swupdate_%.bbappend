FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://defconfig \
"

do_install:append() {
    install -d ${D}${sysconfdir}
    echo "imx6ullevk 1.0" > ${D}${sysconfdir}/hwrevision
}

