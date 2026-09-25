#!/bin/sh
# ==============================================================================
# Custom Qt Application Launcher with Power-On USB Auto-Update
# Checks attached USB drive on boot for verification file and executable
# ==============================================================================

APP_DIR="/opt/hmi/bin"
APP_EXEC="$APP_DIR/app"

mkdir -p "$APP_DIR"

# 1. Search for mounted USB drive path
USB_PATH=""
for path in /media/sda1 /run/media/sda1 /media/usb /run/media/*/* /run/media/*; do
    if [ -d "$path" ]; then
        USB_PATH="$path"
        break
    fi
done

echo "[qt-launcher] Checking USB mount path: ${USB_PATH:-None}"

if [ -n "$USB_PATH" ] && [ -d "$USB_PATH" ]; then
    # File 1: Verification / Flag file
    VERIFY_FILE=""
    for vf in "$USB_PATH/UpdateCont" "$USB_PATH/update.txt" "$USB_PATH/UPDATE"; do
        if [ -f "$vf" ]; then
            VERIFY_FILE="$vf"
            break
        fi
    done

    # File 2: Execution file to update
    USB_EXEC=""
    for ef in "$USB_PATH/test" "$USB_PATH/app" "$USB_PATH/7-1-stamp" "$USB_PATH/AppUpdater"; do
        if [ -f "$ef" ]; then
            USB_EXEC="$ef"
            break
        fi
    done

    if [ -n "$VERIFY_FILE" ] && [ -n "$USB_EXEC" ]; then
        FLAG=$(cat "$VERIFY_FILE" | tr -d '\r\n ')
        echo "[qt-launcher] Found verification file: $VERIFY_FILE (Value: '$FLAG')"
        echo "[qt-launcher] Found USB binary: $USB_EXEC"

        if [ "$FLAG" = "Y" ] || [ "$FLAG" = "y" ] || [ "$FLAG" = "YES" ] || [ "$FLAG" = "1" ]; then
            echo "[qt-launcher] Verification PASSED. Updating $APP_EXEC from USB..."
            cp "$USB_EXEC" "$APP_EXEC"
            chmod +x "$APP_EXEC"
            sync
            echo "[qt-launcher] USB update completed successfully!"
        else
            echo "[qt-launcher] Verification flag is '$FLAG' (not 'Y'). Skipping USB update."
        fi
    fi
fi

# Ensure touchscreen event node exists
if [ ! -e /dev/input/touchscreen0 ]; then
    for ev in /dev/input/event*; do
        if [ -e "$ev" ]; then
            ln -sf "$ev" /dev/input/touchscreen0 2>/dev/null || true
            break
        fi
    done
fi

# Export Qt Platform & Generic Plugin configurations
export QT_QPA_PLATFORM="linuxfb:fb=/dev/fb0"
export QT_QPA_GENERIC_PLUGINS="evdevtouch:/dev/input/touchscreen0"
export QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS="/dev/input/touchscreen0"
export QT_QPA_FB_NO_LIBINPUT=1

if [ -d "/usr/share/fonts/truetype" ]; then
    export QT_QPA_FONTDIR="/usr/share/fonts/truetype"
fi

# Ensure framebuffer is unblanked and backlight is on
echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null || true
for b in /sys/class/backlight/*; do
    [ -d "$b" ] && echo 7 > "$b/brightness" 2>/dev/null && echo 0 > "$b/bl_power" 2>/dev/null || true
done

# Launch Qt Binary
if [ -x "$APP_EXEC" ]; then
    echo "[qt-launcher] Launching $APP_EXEC ..."
    exec "$APP_EXEC"
else
    echo "[qt-launcher] Error: $APP_EXEC not found or not executable!" >&2
    exit 1
fi
