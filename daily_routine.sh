#!/bin/bash
# ------------------------------------------------------------------
# HORUS MASTER ROUTINE
# Reads config from: /home/horus/Horus/config/horus.conf
# ------------------------------------------------------------------

# --- 1. LOAD CONFIGURATION ---
CONFIG_FILE="/home/horus/Horus/config/horus.conf"

if [ -f "$CONFIG_FILE" ]; then
    source "$CONFIG_FILE"
else
    echo "CRITICAL: Config not found at $CONFIG_FILE"
    exit 1
fi

# --- 2. INITIALIZE SERIAL PORT (CRITICAL FIX) ---
# This sets the baud rate to 115200 and disables flow control.
# Without this, the script WILL HANG.
stty -F $USB_AT 115200 raw -echo -echoe -echok -crtscts

# --- HELPER FUNCTIONS ---
log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1" >> "$LOG_FILE"
}

send_at() {
    echo -e "$1\r" > "$USB_AT"
    # Small delay to ensure the modem processes the command
    sleep 0.2
}

# --- START ROUTINE ---
log "========================================"
log "STARTING ROUTINE ($PROJECT_NAME)"

# 3. WAKE UP MODEM
log "[Modem] Waking up..."
send_at "AT+CFUN=1"
sleep 45 

# 4. ENABLE GPS
log "[GPS] Enabling module..."
send_at "AT+CGPS=1"
sleep 5

# 5. GET GPS FIX
log "[GPS] Attempting fix..."
GPS_FIX="No Fix"
for i in {1..5}; do
    # Read raw output with a timeout protection
    RAW=$(echo -e "AT+CGPSINFO\r" > "$USB_AT"; timeout 2 head -n 5 "$USB_AT" | grep "+CGPSINFO:")
    
    if [[ "$RAW" == *"+CGPSINFO:"* && "$RAW" != *",,,,,,"* ]]; then
        GPS_FIX=${RAW#"+CGPSINFO: "}
        break
    fi
    sleep 2
done

# 5.1 Handle GPS Result
if [ "$GPS_FIX" == "No Fix" ]; then
    log "[GPS] No fix obtained."
else
    log "[GPS] Fix obtained: $GPS_FIX"
fi

# 6. SAVE GPS DATA
echo "$(date '+%Y-%m-%d %H:%M:%S'),$GPS_FIX" >> "$DATA_DIR/gps_history.csv"

# 7. RUN SENSORS (BME280)
log "[Sensors] Reading BME280..."
$APP_PATH --task monitor_env >> "$LOG_FILE" 2>&1

# 8. CAPTURE PHOTO
log "[Camera] Taking picture..."
$APP_PATH --task capture >> "$LOG_FILE" 2>&1

# 9. UPLOAD TO CLOUD
if [ "$CLOUD_ENABLED" == "true" ]; then
    log "[Cloud] Syncing to S3 ($S3_BUCKET)..."
    TODAY=$(date +%F)
    # Using the variables directly from the config file
    rclone copy "$DATA_DIR" "$RCLONE_REMOTE:$S3_BUCKET/$TODAY" --log-file="$LOG_FILE"
else
    log "[Cloud] Disabled in config."
fi

# 10. MAINTENANCE WINDOW
log "[Maintenance] Window OPEN (30 min)."
# Keeping GPS/Modem ON so you can SSH in
sleep 1800 
log "[Maintenance] Window CLOSED."

# 11. SHUTDOWN
log "[Modem] GPS/Radio OFF..."
send_at "AT+CGPS=0"
send_at "AT+CFUN=0"

log "ROUTINE COMPLETE."
log "========================================"