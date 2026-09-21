#!/bin/bash
# ==============================================================================
# Automated Local OTA HTTP Server Setup for i.MX6ULL SWUpdate
# ==============================================================================

set -e

PORT="${1:-8000}"
VERSION="${2:-1.0.1}"

echo "========================================================"
echo "  Setting up Local OTA Update Server                    "
echo "========================================================"

# 1. Auto-detect primary IP address (exclude loopback and docker interfaces)
HOST_IP=$(ip -4 addr show scope global | grep -oP '(?<=inet\s)\d+(\.\d+){3}' | head -n 1)

if [ -z "$HOST_IP" ]; then
    HOST_IP=$(hostname -I | awk '{print $1}')
fi

if [ -z "$HOST_IP" ]; then
    echo "[-] Error: Could not determine host IP address automatically."
    echo "    Please run: ./start_ota_server.sh [PORT] [VERSION]"
    exit 1
fi

echo "[+] Detected Host IP : $HOST_IP"
echo "[+] OTA Server Port  : $PORT"
echo "[+] Target Version   : $VERSION"

# 2. Locate generated .swu package
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SWU_DEFAULT="$SCRIPT_DIR/build-hmi/tmp/deploy/images/imx6ullevk/swupdate-image-imx6ullevk.swu"

SERVER_DIR="$SCRIPT_DIR/ota_server"
mkdir -p "$SERVER_DIR"

if [ -f "$SWU_DEFAULT" ]; then
    echo "[+] Found SWUpdate package: $SWU_DEFAULT"
    cp -v "$SWU_DEFAULT" "$SERVER_DIR/update.swu"
elif [ -f "$SERVER_DIR/update.swu" ]; then
    echo "[+] Using existing package at $SERVER_DIR/update.swu"
else
    echo "[!] Warning: No .swu file found at $SWU_DEFAULT"
    echo "    Please make sure 'bitbake swupdate-image' has finished,"
    echo "    or place your .swu file at '$SERVER_DIR/update.swu'."
fi

# 3. Generate version.json
cat <<EOF > "$SERVER_DIR/version.json"
{
  "version": "$VERSION",
  "url": "http://$HOST_IP:$PORT/update.swu"
}
EOF

echo "[+] Created $SERVER_DIR/version.json"
echo "--------------------------------------------------------"
cat "$SERVER_DIR/version.json"
echo "--------------------------------------------------------"

# 4. Print instructions for target board
echo ""
echo "========================================================"
echo " RUN THIS ON THE TARGET BOARD TERMINAL:                "
echo "========================================================"
echo ""
echo "cat << 'EOF' > /etc/ota-server.conf"
echo "OTA_SERVER_URL=\"http://$HOST_IP:$PORT\""
echo "CHECK_INTERVAL=10"
echo "EOF"
echo "systemctl restart ota-agent"
echo "journalctl -u ota-agent -f"
echo ""
echo "========================================================"
echo " Starting HTTP Server at http://$HOST_IP:$PORT ...     "
echo " (Press Ctrl+C to stop the server)                     "
echo "========================================================"

cd "$SERVER_DIR"
exec python3 -m http.server "$PORT"
