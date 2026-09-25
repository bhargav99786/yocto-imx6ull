#!/bin/bash
# ==============================================================================
# Helper to launch Qt Creator configured with i.MX6ULL Yocto SDK Environment
# ==============================================================================

SDK_ENV="/opt/poky/4.0.35/environment-setup-cortexa7t2hf-neon-poky-linux-gnueabi"

if [ -f "$SDK_ENV" ]; then
    echo "Sourcing Yocto SDK environment from: $SDK_ENV"
    source "$SDK_ENV"
else
    echo "Warning: $SDK_ENV not found! Ensure toolchain is installed in /opt/poky/4.0.35"
fi

if ! which qtcreator >/dev/null 2>&1; then
    echo "Error: Qt Creator is not installed!"
    echo "Install it with: sudo apt update && sudo apt install -y qtcreator"
    exit 1
fi

echo "Launching Qt Creator..."
exec qtcreator "$@"
