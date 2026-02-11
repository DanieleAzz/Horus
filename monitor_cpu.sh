#!/bin/bash

# --- CONFIGURATION ---
# Base Directory (Root of DataCapture)
BASE_DIR="/home/horus/DataCapture"

# FILE LOCATION: Directly inside DataCapture (like gps_history.csv)
CSV_FILE="$BASE_DIR/cpu_info.csv"

# Ensure the base directory exists
mkdir -p "$BASE_DIR"

# --- COLLECT DATA ---
# Timestamp format (Matches your other logs)
TIMESTAMP=$(date '+%Y-%m-%dT%H:%M:%S%Z')

# 1. Get Temperature (Format: temp=52.1'C -> 52.1)
TEMP_RAW=$(vcgencmd measure_temp)
TEMP_CLEAN=$(echo $TEMP_RAW | sed "s/temp=//;s/'C//")

# 2. Get Throttled State (Format: throttled=0x0 -> 0x0)
THROT_RAW=$(vcgencmd get_throttled)
THROT_CLEAN=$(echo $THROT_RAW | sed "s/throttled=//")

# --- SAVE TO CSV ---

# Create Header if file doesn't exist
if [ ! -f "$CSV_FILE" ]; then
    echo "Timestamp,CPU_Temp_C,Throttled_Hex" > "$CSV_FILE"
fi

# Append Data
echo "$TIMESTAMP,$TEMP_CLEAN,$THROT_CLEAN" >> "$CSV_FILE"

# Log to journal for debugging
echo "[CPU Monitor] Saved: $TEMP_CLEAN °C | $THROT_CLEAN to $CSV_FILE"