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
