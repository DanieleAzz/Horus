#!/bin/bash
# ------------------------------------------------------------------
# HORUS MASTER ROUTINE
# Features: RTC Time, GPIO Hardware Wake, Driver Injection, Safe Sleep
# ------------------------------------------------------------------

# --- CONFIGURATION ---
CONFIG_FILE="/home/horus/Horus/config/horus.conf"
PIN_RST=6        # GPIO 6 for Hardware Reset
USB_AT="/dev/ttyUSB2"

# Load Config
if [ -f "$CONFIG_FILE" ]; then
    source "$CONFIG_FILE"
else
    echo "CRITICAL: Config not found at $CONFIG_FILE"
    exit 1
fi

# --- HELPER FUNCTIONS ---
log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1" >> "$LOG_FILE"
}

send_at() {
    echo -e "$1\r" > "$USB_AT"
    sleep 0.2
}

# --- START ROUTINE ---
log "========================================"
log "STARTING ROUTINE ($PROJECT_NAME)"

# 1. RESTORE TIME FROM BATTERY (RTC)
sudo hwclock -s
log "[Time] System clock synced from RTC Battery: $(date)"

# 2. HARDWARE WAKE UP
log "[Modem] Triggering Hardware Reset (GPIO $PIN_RST)..."
pinctrl set $PIN_RST op
pinctrl set $PIN_RST dh
sleep 1
pinctrl set $PIN_RST dl
sleep 5 # Wait for boot

# 3. DRIVER INJECTION (Critical for ID 9018)
log "[Modem] Re-applying USB Drivers..."
sudo modprobe option
sudo modprobe usb_wwan
echo 1e0e 9018 | sudo tee /sys/bus/usb-serial/drivers/option1/new_id > /dev/null 2>&1

# Wait for Port
log "[Modem] Waiting for $USB_AT..."
for i in {1..20}; do
    if [ -e "$USB_AT" ]; then
        break
    fi
    sleep 1
done

if [ ! -e "$USB_AT" ]; then
    log "[ERROR] Modem port not found. Aborting."
    exit 1
fi

# 4. ENABLE RADIO & GPS
log "[Modem] Enabling Radio (AT+CFUN=1)..."
stty -F $USB_AT 115200 raw -echo -echoe -echok -crtscts > /dev/null 2>&1
send_at "AT+CFUN=1"
sleep 20 # Allow network negotiation

log "[GPS] Enabling module..."
send_at "AT+CGPS=1"
sleep 2

# 5. GET GPS FIX (Safe Mode)
log "[GPS] Attempting fix..."
GPS_FIX="No Fix"

# Try 5 times with Timeout to prevent hanging
for i in {1..5}; do
    RAW=$(echo -e "AT+CGPSINFO\r" > "$USB_AT"; timeout 3 head -n 5 "$USB_AT" | grep "+CGPSINFO:")
    
    if [[ "$RAW" == *"+CGPSINFO:"* && "$RAW" != *",,,,,,"* ]]; then
        GPS_FIX=${RAW#"+CGPSINFO: "}
        break
    fi
    sleep 1
done
log "[GPS] Result: $GPS_FIX"
echo "$(date '+%Y-%m-%d %H:%M:%S'),$GPS_FIX" >> "$DATA_DIR/gps_history.csv"

# 5.1 TURN OFF GPS TO SAVE POWER:
log "[GPS] Disabling module..."
send_at "AT+CGPS=0"
sleep 2

# 6. CONNECT INTERFACE (DHCP)
LTE_IFACE=$(ip -o link show | awk -F': ' '{print $2}' | grep -E 'usb|wwan|ppp' | head -n 1)
if [ ! -z "$LTE_IFACE" ]; then
    sudo ip link set "$LTE_IFACE" up
    sudo dhclient "$LTE_IFACE" -v > /dev/null 2>&1
    log "[Network] Interface $LTE_IFACE configured."
fi

# 7. RUN SENSORS & CAMERA
log "[Sensors] Reading BME280..."
$APP_PATH --task monitor_env >> "$LOG_FILE" 2>&1

log "[Camera] Taking picture..."
$APP_PATH --task capture >> "$LOG_FILE" 2>&1

# 8. UPLOAD TO CLOUD
if [ "$CLOUD_ENABLED" == "true" ]; then
    log "[Cloud] Syncing to S3..."
    TODAY=$(date +%F)
    # Use --transfers 4 for better speed on LTE
    rclone copy "$DATA_DIR" "$RCLONE_REMOTE:$S3_BUCKET/$TODAY" --transfers 4 --log-file="$LOG_FILE"
fi

# 9. MAINTENANCE WINDOW (30 Min)
log "[Maintenance] Window OPEN (30 min). SSH is possible."
sleep 1800 
log "[Maintenance] Window CLOSED."

# 10. SHUTDOWN & SLEEP
log "[Modem] Entering Flight Mode (Power Save)..."
send_at "AT+CGPS=0"
send_at "AT+CFUN=0"
sleep 2

log "ROUTINE COMPLETE."
log "========================================"