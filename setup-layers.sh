#!/bin/bash
# Automate cloning all required Yocto Kirkstone layers

set -e

echo "========================================================"
echo " Cloning Yocto Kirkstone Layer Dependencies..."
echo "========================================================"

# Poky core
if [ ! -d "poky" ]; then
    echo "Cloning poky (kirkstone)..."
    git clone -b kirkstone git://git.yoctoproject.org/poky
fi

# meta-openembedded
if [ ! -d "meta-openembedded" ]; then
    echo "Cloning meta-openembedded (kirkstone)..."
    git clone -b kirkstone git://git.openembedded.org/meta-openembedded
fi

# meta-freescale
if [ ! -d "meta-freescale" ]; then
    echo "Cloning meta-freescale (kirkstone)..."
    git clone -b kirkstone https://github.com/Freescale/meta-freescale.git
fi

# meta-qt5
if [ ! -d "meta-qt5" ]; then
    echo "Cloning meta-qt5 (kirkstone)..."
    git clone -b kirkstone https://github.com/meta-qt5/meta-qt5.git
fi

# meta-swupdate
if [ ! -d "meta-swupdate" ]; then
    echo "Cloning meta-swupdate (kirkstone)..."
    git clone -b kirkstone https://github.com/sbabic/meta-swupdate.git
fi

echo "========================================================"
echo " All required Yocto layers successfully cloned!"
echo " Next step: source poky/oe-init-build-env build-hmi"
echo "========================================================"
