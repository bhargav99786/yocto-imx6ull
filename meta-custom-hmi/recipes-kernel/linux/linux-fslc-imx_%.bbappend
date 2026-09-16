FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://okmx6ull-c-emmc.dts \
    file://imx6ull-custom-hmi.dts \
    file://0001-add-gt9xx-touchscreen-driver.patch \
    file://logo_linux_clut224.ppm \
    file://motorcomm.c \
"

do_configure:append() {
    cp ${WORKDIR}/okmx6ull-c-emmc.dts ${S}/arch/arm/boot/dts/
    cp ${WORKDIR}/imx6ull-custom-hmi.dts ${S}/arch/arm/boot/dts/
    cp ${WORKDIR}/logo_linux_clut224.ppm ${S}/drivers/video/logo/logo_linux_clut224.ppm
    cp ${WORKDIR}/motorcomm.c ${S}/drivers/net/phy/motorcomm.c
    echo "CONFIG_MOTORCOMM_PHY=y" >> ${B}/.config
    echo "CONFIG_TOUCHSCREEN_GT9xx=y" >> ${B}/.config
    echo "CONFIG_LOGO=y" >> ${B}/.config
    echo "CONFIG_LOGO_LINUX_CLUT224=y" >> ${B}/.config
    echo "CONFIG_CGROUPS=y" >> ${B}/.config
    echo "CONFIG_CGROUP_FREEZER=y" >> ${B}/.config
    echo "CONFIG_CGROUP_PIDS=y" >> ${B}/.config
    echo "CONFIG_CGROUP_DEVICE=y" >> ${B}/.config
    echo "CONFIG_CGROUP_CPUACCT=y" >> ${B}/.config
    echo "CONFIG_CGROUP_SCHED=y" >> ${B}/.config
    echo "CONFIG_NAMESPACES=y" >> ${B}/.config
    echo "CONFIG_UTS_NS=y" >> ${B}/.config
    echo "CONFIG_IPC_NS=y" >> ${B}/.config
    echo "CONFIG_USER_NS=y" >> ${B}/.config
    echo "CONFIG_PID_NS=y" >> ${B}/.config
    echo "CONFIG_NET_NS=y" >> ${B}/.config
    echo "CONFIG_DEVTMPFS=y" >> ${B}/.config
    echo "CONFIG_DEVTMPFS_MOUNT=y" >> ${B}/.config
    echo "CONFIG_SECCOMP=y" >> ${B}/.config
    echo "CONFIG_TMPFS_POSIX_ACL=y" >> ${B}/.config
}


KERNEL_DEVICETREE += "okmx6ull-c-emmc.dtb imx6ull-custom-hmi.dtb"
