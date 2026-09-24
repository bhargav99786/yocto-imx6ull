IMAGE_FSTYPES += "ext4.gz"

IMAGE_INSTALL:append = " \
    boot-splash \
    hmi-app \
    ttf-dejavu-sans \
    ttf-dejavu-common \
    fontconfig \
    evtest \
    emmc-installer \
    swupdate \
    swupdate-www \
    libubootenv-bin \
    ota-agent \
    confirm-boot \
    touch-test \
    hmi-sleep \
"

# Qt5 SDK Configuration (Provides cross-compilation native qmake, qt.conf, and headers)
inherit populate_sdk_qt5_base

TOOLCHAIN_HOST_TASK:append = " nativesdk-packagegroup-qt5-toolchain-host"
TOOLCHAIN_TARGET_TASK:append = " qtbase-dev qtbase-mkspecs qtdeclarative-dev qtdeclarative-mkspecs"
