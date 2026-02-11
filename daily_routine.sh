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
    # Check if port is busy before trying
    if fuser "$USB_AT" >/dev/null 2>&1; then
        sleep 1
    fi
    echo -e "$1\r" > "$USB_AT"
    sleep 0.2
}

# --- START ROUTINE ---
log "========================================"
log "STARTING ROUTINE ($PROJECT_NAME)"

# 0. RESTORE TIME FROM BATTERY (RTC)
sudo hwclock -s
log "[Time] System clock synced from RTC Battery: $(date)"

# 1. TAKE PICTURE FIRST (Captures the state before we mess with the modem)
log "[rpicam-jpeg Camera] Taking picture..."
IMG_NAME="img_$(date '+%Y-%m-%dT%H_%M_%S').jpg"
TODAY_DIR="$DATA_DIR/$(date +%F)"
mkdir -p "$TODAY_DIR"
# Capture Command
# -t 2000: Warm up for 2s (Auto Exposure/White Balance)
# --width 4608 --height 2592: Full Res (IMX708)
if rpicam-jpeg -o "$TODAY_DIR/$IMG_NAME" --nopreview -t 3000; then
    log "[Camera] Saved: $TODAY_DIR/$IMG_NAME"
else
    log "[Camera] ERROR: Capture failed."
fi


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

# Prepare the command but don't execute yet
echo -e "AT+CGPSINFO\r" > "$USB_AT"

# Read the response safely using a temporary file descriptor capture
# We read for 2 seconds then kill the read process
RAW=$(timeout 2s cat "$USB_AT" | grep "+CGPSINFO:")

if [[ "$RAW" == *"+CGPSINFO:"* && "$RAW" != *",,,,,,"* ]]; then
    GPS_FIX=${RAW#"+CGPSINFO: "}
    log "[GPS] Fix Obtained: $GPS_FIX"
else
    log "[GPS] No Fix obtained."
fi

# Append to CSV (Permissions should be fixed now)
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
    
    # Define Dates
    TODAY=$(date +%F)
    YESTERDAY=$(date -d "yesterday" +%F)
    
    # 1. Upload YESTERDAY'S Folder (Catches the afternoon data missed by previous upload)
    # Rclone will skip files that are already there (Morning data) and just add the new ones.
    if [ -d "$DATA_DIR/$YESTERDAY" ]; then
        log "[Cloud] Syncing incomplete data from $YESTERDAY..."
        rclone copy "$DATA_DIR/$YESTERDAY" "$RCLONE_REMOTE:$S3_BUCKET/$YESTERDAY" \
            --config "$RCLONE_CONF" --transfers 4 --log-file="$LOG_FILE"
    fi

    # 2. Upload TODAY'S Folder (Catches the morning data so far)
    if [ -d "$DATA_DIR/$TODAY" ]; then
        log "[Cloud] Syncing data from $TODAY..."
        rclone copy "$DATA_DIR/$TODAY" "$RCLONE_REMOTE:$S3_BUCKET/$TODAY" \
            --config "$RCLONE_CONF" --transfers 4 --log-file="$LOG_FILE"
    fi

    # 3. Upload Cumulative GPS Log
    if [ -f "$DATA_DIR/gps_history.csv" ]; then
        rclone copy "$DATA_DIR/gps_history.csv" "$RCLONE_REMOTE:$S3_BUCKET/" \
            --config "$RCLONE_CONF" --log-file="$LOG_FILE"
    fi

    # 4. Upload Cumulative CPU Log
    if [ -f "$DATA_DIR/cpu_info.csv" ]; then
        rclone copy "$DATA_DIR/cpu_info.csv" "$RCLONE_REMOTE:$S3_BUCKET/" \
            --config "$RCLONE_CONF" --log-file="$LOG_FILE"
    fi
        
    log "[Cloud] Upload complete."
fi

# 9. MAINTENANCE WINDOW (30 Min)
log "[Maintenance] Window OPEN (30 min). SSH is possible."

# --- ZEROTIER AUTO-FIX (Matches Debug Script) ---
log "[ZeroTier] Restarting service to bind to LTE..."

# 1. Restart to force it to see the new usb0 interface
sudo systemctl restart zerotier-one

# 2. WAIT 30 SECONDS (Critical! Matches your working debug script)
# LTE takes time to negotiate the UDP tunnel.
sleep 30

# 3. Check Status (Column 5 is the real status: ONLINE/OFFLINE)
ZT_STATUS=$(sudo zerotier-cli status | awk '{print $5}')

if [ "$ZT_STATUS" == "ONLINE" ]; then
    # Grab the ZeroTier IP address
    ZT_IP=$(ip -o -4 addr show | grep 'zt' | awk '{print $4}' | cut -d/ -f1 | head -n 1)
    log "[ZeroTier] SUCCESS: Connected ($ZT_STATUS). Virtual IP: ${ZT_IP:-No IP assigned}"
else
    log "[ZeroTier] WARNING: Service is $ZT_STATUS. Retrying network map..."
    # Force a refresh if it's stuck
    sudo zerotier-cli listnetworks > /dev/null 2>&1
fi
# -------------------------

# --- ABSOLUTE TIME MAINTENANCE WINDOW ---
# Wait for maintenance (SSH access)
START_TIME=$(date +%s)
END_TIME=$((START_TIME + SSH_MAINTENANCE_TIME * 60))

log "[Maintenance] Window starts. Deadline: $(date -d @$END_TIME '+%H:%M:%S')"

while [ "$(date +%s)" -lt "$END_TIME" ]; do
    sleep 60
    CURRENT_TIME=$(date +%s)
    ELAPSED=$(( (CURRENT_TIME - START_TIME) / 60 ))

    if (( ELAPSED % 5 == 0)); then
        log "[Maintenance] Window active... ($ELAPSED / $SSH_MAINTENANCE_TIME min)"
    fi
done
log "[Maintenance] Window CLOSED."


# 10. SHUTDOWN & SLEEP
log "[Modem] Entering Flight Mode (Power Save)..."

# Explicitly close GPS first
send_at "AT+CGPS=0"
sleep 1

# Send Flight Mode command
echo -e "AT+CFUN=0\r" > "$USB_AT"
sleep 2

# FIX PERMISSIONS (Crucial for 15-min logs)
log "[Maintenance] Fixing file permissions..."
chown -R horus:horus "$DATA_DIR"
sleep 1

log "ROUTINE COMPLETE."
log "========================================"