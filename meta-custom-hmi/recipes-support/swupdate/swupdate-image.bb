SUMMARY = "SWUpdate dual-bank OTA update package recipe"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://sw-description \
    file://postupdate.sh \
"

inherit swupdate

SWUPDATE_IMAGES = "core-image-minimal"
SWUPDATE_IMAGES_FSTYPES[core-image-minimal] = ".ext4.gz"

IMAGE_DEPENDS = "core-image-minimal"
