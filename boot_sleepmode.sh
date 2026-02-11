#!/bin/bash
# ---------------------------------------------------------
# HORUS BOOT SILENCER
# Purpose: Force Modem into Flight Mode immediately at boot
# ---------------------------------------------------------

# --- CONFIGURATION ---
CONFIG_FILE="/home/horus/Horus/config/horus.conf"
USB_AT="/dev/ttyUSB2"

# Load Config
if [ -f "$CONFIG_FILE" ]; then
    source "$CONFIG_FILE"
else
    echo "CRITICAL: Config not found at $CONFIG_FILE"
    exit 1
fi

log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1" >> "$BOOT_LOG_FILE"
}

log "========================================"
log "STARTING BOOT"

log "[Boot] Silencer started. Waiting for modem..."

# 0. DRIVER INJECTION (Critical for ID 9018)
log "[Modem] Re-applying USB Drivers..."
sudo modprobe option
sudo modprobe usb_wwan
echo 1e0e 9018 | sudo tee /sys/bus/usb-serial/drivers/option1/new_id > /dev/null 2>&1

# 1. Wait for the USB device to appear (Driver loading)
for i in {1..30}; do
    if [ -e "$USB_AT" ]; then
        log "[Boot] Modem found at $USB_AT."
        break
    fi
    sleep 1
done

if [ ! -e "$USB_AT" ]; then
    log "[Error] Modem did not appear within 30s. Aborting."
    exit 1
fi

# 2. Kill interference (Just in case)
# We ignore errors in case they aren't running
fuser -k "$USB_AT" >/dev/null 2>&1

# 3. Send Sleep Commands
log "[Boot] Sending Sleep Command (AT+CFUN=0)..."

# Configure port
stty -F $USB_AT 115200 raw -echo -echoe -echok -crtscts > /dev/null 2>&1

# Disable GPS (Power Hog)
echo -e "AT+CGPS=0\r" > "$USB_AT"
sleep 2

# Disable Radio (Flight Mode)
echo -e "AT+CFUN=0\r" > "$USB_AT"
sleep 2

log "[Boot] Modem silenced. System ready."