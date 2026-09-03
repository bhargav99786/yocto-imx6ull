FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Fast boot optimization for U-Boot (Zero-delay boot)
SRC_URI += "file://bootdelay.cfg"

# Add fastboot & quiet kernel cmdline parameters
APPEND:append = " quiet loglevel=0 "

