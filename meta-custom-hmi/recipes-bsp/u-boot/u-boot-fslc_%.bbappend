FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Fast boot optimization for U-Boot (2s delay boot)
SRC_URI += "file://bootdelay.cfg"

