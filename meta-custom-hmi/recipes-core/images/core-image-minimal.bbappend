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

# Qt5 SDK Configuration (Provides cross-compilation native qmake, qt.conf, and target headers)
TOOLCHAIN_HOST_TASK:append = " nativesdk-packagegroup-qt5-toolchain-host"
TOOLCHAIN_TARGET_TASK:append = " qtbase-dev qtbase-mkspecs qtdeclarative-dev qtdeclarative-mkspecs"

create_sdk_files:append () {
    # Dynamically locate host qmake in SDK output and generate relative qt.conf
    for qmake_bin in $(find ${SDK_OUTPUT} -name "qmake" -type f 2>/dev/null); do
        qdir=$(dirname "$qmake_bin")
        cat << 'EOF' > "$qdir/qt.conf"
[Paths]
Prefix = /usr
Headers = /usr/include
Libraries = /usr/lib
ArchData = /usr/lib
Data = /usr/share
Binaries = /usr/bin
HostData = ../../cortexa7t2hf-neon-poky-linux-gnueabi/usr/lib
HostBinaries = /usr/bin
Sysroot = ../../cortexa7t2hf-neon-poky-linux-gnueabi
EOF
    done
}
