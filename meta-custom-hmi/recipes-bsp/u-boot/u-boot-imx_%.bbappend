FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Fix: mx6ullevk.h missing MXS_LCDIF_BASE definition (needed by mxsfb.c when CONFIG_VIDEO_MXS=y)
SRC_URI += " \
    file://bootdelay.cfg \
    file://0001-mx6ullevk-define-MXS_LCDIF_BASE-for-CONFIG_VIDEO_MXS.patch \
"
