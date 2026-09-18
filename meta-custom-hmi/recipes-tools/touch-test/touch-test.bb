SUMMARY = "Standalone Touchscreen Diagnostic and Test Utility"
DESCRIPTION = "Lightweight C utility to test, analyze, and diagnose touchscreen digitizer, axis bounds, quadrants, and framebuffer feedback in minimal environments"
SECTION = "tools"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://touch_test.c"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} touch_test.c -o touch_test -lm
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 touch_test ${D}${bindir}/touch_test
    ln -sf touch_test ${D}${bindir}/touch-test
}

FILES:${PN} = "${bindir}/touch_test ${bindir}/touch-test"
