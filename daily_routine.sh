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

# --- HELPER FUNCTIONS ---
log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1" >> "$LOG_FILE"
}

send_at() {
    echo -e "$1\r" > "$USB_AT"
}

# --- START ROUTINE ---
log "========================================"
log "STARTING ROUTINE ($PROJECT_NAME)"

# 2. WAKE UP MODEM
log "[Modem] Waking up..."
send_at "AT+CFUN=1"
sleep 45 

# 3. ENABLE GPS
log "[GPS] Enabling module..."
send_at "AT+CGPS=1"
sleep 5

# 4. GET GPS FIX
log "[GPS] Attempting fix..."
GPS_FIX="No Fix"
for i in {1..5}; do
    RAW=$(echo -e "AT+CGPSINFO\r" > "$USB_AT"; head -n 5 "$USB_AT" | grep "+CGPSINFO:")
    if [[ "$RAW" == *"+CGPSINFO:"* && "$RAW" != *",,,,,,"* ]]; then
        GPS_FIX=${RAW#"+CGPSINFO: "}
        break
    fi
    sleep 2
done

# 4.1 Turn off GPS if no fix
if [ "$GPS_FIX" == "No Fix" ]; then
    log "[GPS] No fix obtained."
else
    log "[GPS] Fix obtained: $GPS_FIX"
fi
log "[GPS] Location: $GPS_FIX"
log "[GPS] Turning off GPS"
send_at "AT+CGPS=0"
sleep 2

# 5. SAVE GPS DATA
echo "$(date '+%Y-%m-%d %H:%M:%S'),$GPS_FIX" >> "$DATA_DIR/gps_history.csv"

# 6. RUN SENSORS (BME280)
log "[Sensors] Reading BME280..."
$APP_PATH --task monitor_env >> "$LOG_FILE" 2>&1

# 7. CAPTURE PHOTO
log "[Camera] Taking picture..."
$APP_PATH --task capture >> "$LOG_FILE" 2>&1

# 8. UPLOAD TO CLOUD
if [ "$CLOUD_ENABLED" == "true" ]; then
    log "[Cloud] Syncing to S3 ($S3_BUCKET)..."
    TODAY=$(date +%F)
    # Using the variables directly from the config file
    rclone copy "$DATA_DIR" "$RCLONE_REMOTE:$S3_BUCKET/$TODAY" --log-file="$LOG_FILE"
else
    log "[Cloud] Disabled in config."
fi

# 9. MAINTENANCE WINDOW
log "[Maintenance] Window OPEN (30 min)."
sleep 1800 
log "[Maintenance] Window CLOSED."

# 10. SHUTDOWN
log "[Modem] GPS/Radio OFF..."
send_at "AT+CGPS=0"
send_at "AT+CFUN=0"

log "ROUTINE COMPLETE."
log "========================================"