#!/bin/bash
set -e

# --- CONFIGURATION ---
# Get the absolute path to the project root (assuming script is run from project root)
PROJECT_DIR=$(pwd)
EXEC_PATH="$PROJECT_DIR/build/horus_app"
USER_NAME=$(whoami)

echo "[Horus] Starting Deployment..."
echo "  Project Dir: $PROJECT_DIR"
echo "  Executable:  $EXEC_PATH"
echo "  User:        $USER_NAME"

# Check if executable exists
if [ ! -f "$EXEC_PATH" ]; then
    echo "ERROR: Could not find executable at $EXEC_PATH"
    echo "Please compile the project first (cd build && make)"
    exit 1
fi

# --- 1. INTERNAL MONITOR ROBOT (Every 1 Minute) ---
# Task: monitor_internal (BME280)
echo "  -> Configuring Internal Monitor (1 min)..."

sudo bash -c "cat > /etc/systemd/system/horus-internal.service" <<EOF
[Unit]
Description=Horus Internal Monitor (BME280)
After=multi-user.target

[Service]
Type=oneshot
ExecStart=$EXEC_PATH --task monitor_internal
User=$USER_NAME
WorkingDirectory=$PROJECT_DIR
StandardOutput=journal
StandardError=journal
EOF

sudo bash -c "cat > /etc/systemd/system/horus-internal.timer" <<EOF
[Unit]
Description=Trigger Horus Internal Monitor every minute

[Timer]
OnBootSec=1min
OnUnitActiveSec=1min
Unit=horus-internal.service

[Install]
WantedBy=timers.target
EOF

# --- 2. EXTERNAL SENSOR ROBOT (Every 15 Minutes) ---
# Task: monitor_external (DS18B20 + CSV)
echo "  -> Configuring External Logger (15 min)..."

sudo bash -c "cat > /etc/systemd/system/horus-external.service" <<EOF
[Unit]
Description=Horus External Sensor (DS18B20)
After=multi-user.target

[Service]
Type=oneshot
ExecStart=$EXEC_PATH --task monitor_external
User=$USER_NAME
WorkingDirectory=$PROJECT_DIR
StandardOutput=journal
StandardError=journal
EOF

sudo bash -c "cat > /etc/systemd/system/horus-external.timer" <<EOF
[Unit]
Description=Trigger Horus External Logger every 15 minutes

[Timer]
OnBootSec=2min
OnUnitActiveSec=15min
Unit=horus-external.service

[Install]
WantedBy=timers.target
EOF

# --- 3. CAMERA ROBOT (12pm, 2pm, 4pm) ---
# Task: capture (Camera)
echo "  -> Configuring Camera Capture (12, 14, 16)..."

sudo bash -c "cat > /etc/systemd/system/horus-capture.service" <<EOF
[Unit]
Description=Horus Camera Capture
After=multi-user.target

[Service]
Type=oneshot
ExecStart=$EXEC_PATH --task capture
User=$USER_NAME
WorkingDirectory=$PROJECT_DIR
StandardOutput=journal
StandardError=journal
EOF

sudo bash -c "cat > /etc/systemd/system/horus-capture.timer" <<EOF
[Unit]
Description=Trigger Horus Camera at 12pm, 2pm, 4pm

[Timer]
# I am testing 8am 12pm 11pm first
# Run at 12:00, 14:00, and 16:00 every day
OnCalendar=*-*-* 08:00:00
OnCalendar=*-*-* 12:00:00
OnCalendar=*-*-* 23:00:00
Persistent=true
Unit=horus-capture.service

[Install]
WantedBy=timers.target
EOF

# --- APPLY CHANGES ---
echo "[Horus] Reloading Systemd..."
sudo systemctl daemon-reload

echo "[Horus] Enabling Timers..."
sudo systemctl enable --now horus-internal.timer
sudo systemctl enable --now horus-external.timer
sudo systemctl enable --now horus-capture.timer

echo "[Horus] Deployment Complete!"
echo "------------------------------------------------"
echo "Check status with: systemctl list-timers --all | grep horus"
echo "View logs with:    journalctl -t horus_app (or -u horus-capture.service)"