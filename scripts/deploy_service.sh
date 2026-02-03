#!/bin/bash
set -e

# --- CONFIGURATION ---
PROJECT_DIR=$(pwd)
EXEC_PATH="$PROJECT_DIR/build/horus_app"
ROUTINE_SCRIPT="$PROJECT_DIR/daily_routine.sh"
BOOT_SCRIPT="$PROJECT_DIR/boot_sleepmode.sh"
USER_NAME=$(whoami)

echo "[Horus] Starting Survival Deployment..."
echo "  Project Dir: $PROJECT_DIR"
echo "  Executable:  $EXEC_PATH"
echo "  Routine:     $ROUTINE_SCRIPT"
echo "  Boot Script: $BOOT_SCRIPT"

# Validation
if [ ! -f "$EXEC_PATH" ]; then
    echo "ERROR: Could not find executable at $EXEC_PATH"
    exit 1
fi
if [ ! -f "$ROUTINE_SCRIPT" ]; then
    echo "ERROR: Could not find routine script at $ROUTINE_SCRIPT"
    echo "Please create daily_routine.sh first!"
    exit 1
fi

# --- 0. CLEANUP OLD SERVICES ---
# We disable the old names to prevent conflicts
echo "  -> Cleaning up old services..."
sudo systemctl disable --now horus-internal.timer horus-internal.service 2>/dev/null || true
sudo systemctl disable --now horus-external.timer horus-external.service 2>/dev/null || true
sudo systemctl disable --now horus-capture.timer horus-capture.service 2>/dev/null || true

# --- ADD BOOT SILENCER SERVICE ---
# This ensures the modem is silenced at boot before anything else can interfere
echo "  -> Configuring Boot Silencer Service..."

sudo bash -c "cat > /etc/systemd/system/horus-boot.service" <<EOF
[Unit]
Description=Horus Modem Silencer (Save Power at Boot)
After=network.target hardware.target
# We want this to run even if network fails, just needs USB
Wants=dev-ttyUSB2.device

[Service]
Type=oneshot
ExecStart=$ROUTINE_SCRIPT
User=$USER_NAME
WorkingDirectory=$PROJECT_DIR
StandardOutput=journal
StandardError=journal

# Give it time to finish, but kill it if it hangs
TimeoutStartSec=60

[Install]
WantedBy=multi-user.target
EOF

# --- 1. ENV MONITOR (Every 15 Minutes) ---
# Task: monitor_env (BME280 -> CSV)
# This runs locally 24/7. Does NOT touch the modem.
echo "  -> Configuring Environmental Monitor (15 min)..."

sudo bash -c "cat > /etc/systemd/system/horus-monitor.service" <<EOF
[Unit]
Description=Horus Environmental Logger (BME280)
After=multi-user.target

[Service]
Type=oneshot
ExecStart=$EXEC_PATH --task monitor_env
User=$USER_NAME
WorkingDirectory=$PROJECT_DIR
StandardOutput=journal
StandardError=journal
EOF

sudo bash -c "cat > /etc/systemd/system/horus-monitor.timer" <<EOF
[Unit]
Description=Trigger Horus Monitor every 15 minutes

[Timer]
# Run 4 minutes after boot, then every 15 minutes
OnBootSec=4min
OnUnitActiveSec=15min
Unit=horus-monitor.service

[Install]
WantedBy=timers.target
EOF

# --- 2. DAILY MASTER (12:00 PM) ---
# Task: daily_routine.sh (Modem -> GPS -> Picture -> Upload -> Sleep)
echo "  -> Configuring Daily Master Routine (12:00 PM)..."

sudo bash -c "cat > /etc/systemd/system/horus-daily.service" <<EOF
[Unit]
Description=Horus Daily Routine (Upload & Maintenance)
After=multi-user.target

[Service]
Type=oneshot
# IMPORTANT: Points to the BASH SCRIPT, not the C++ App directly
ExecStart=$ROUTINE_SCRIPT
User=root
WorkingDirectory=$PROJECT_DIR
StandardOutput=journal
StandardError=journal
EOF

sudo bash -c "cat > /etc/systemd/system/horus-daily.timer" <<EOF
[Unit]
Description=Trigger Horus Daily Routine at Noon

[Timer]
# Run exactly at 12:00:00 PM every day
OnCalendar=*-*-* 12:00:00
Persistent=true
Unit=horus-daily.service

[Install]
WantedBy=timers.target
EOF

# --- APPLY CHANGES ---
echo "[Horus] Reloading Systemd..."
sudo systemctl daemon-reload

echo "[Horus] Enabling Boot Service..."
sudo systemctl enable --now horus-boot.service

echo "[Horus] Enabling Timers..."
sudo systemctl enable --now horus-monitor.timer
sudo systemctl enable --now horus-daily.timer

echo "[Horus] Deployment Complete!"
echo "------------------------------------------------"
echo "Active Timers:"
systemctl list-timers --all | grep horus