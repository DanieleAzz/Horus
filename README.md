# Horus
Horus is the name of this embedded device used to capture agriculture pictures

# Hardware
RPI CM4 WiFi yes, RAM 2GB, eMMC Flash 16GB + External SD-Card 64GB

## Installation steps:
- OS: 2025-11-24-raspios-bookworm-arm64-lite.img.xz
- Commands:
  - sudo apt update
  - sudo apt upgrade
  - edited the /boot/config.txt to insert all the cameras dtoverlay
  - sudo raspi-config, enabling i2c and spi, then reboot
  - sudo apt-get install i2c-tools
  - 

# Horus Project Architecture & Software Engineering Summary

## Executive Overview
Horus is an autonomous, solar-powered edge computing device designed for agricultural IoT, specifically targeting tree monitoring applications. Operating on a Raspberry Pi CM4 (2GB RAM, 16GB eMMC) running Raspberry Pi OS Bookworm Lite, the system is engineered for remote deployments with strict power constraints. The system captures high-resolution imagery—utilizing ArUco markers for downstream scale and CO2 sequestration calculations—alongside environmental telemetry, syncing the data to AWS S3 via an intermittent 4G LTE connection.

## System Architecture & Core Workflows

The system architecture relies on a hybrid approach, using robust Bash scripting for system-level orchestration and power management, combined with a modular C++17 application for high-performance hardware interfacing.

### 1. Power Management & Hardware Orchestration
To conserve solar battery reserves, the 4G modem is aggressively managed.
* **Boot Silencing:** The `boot_sleepmode.sh` script runs immediately at startup as a systemd oneshot service (`horus-boot.service`) to force the modem into flight mode (`AT+CFUN=0`) and disable the power-hungry GPS module (`AT+CGPS=0`).
* **Hardware Wake:** The `daily_routine.sh` master script utilizes GPIO pin 6 to trigger a physical hardware reset of the modem to ensure reliable wake-up from deep sleep states.

### 2. Telemetry Collection (High-Frequency Polling)
* Systemd timers (`horus-monitor.timer` and `horus-cpu.timer`) trigger lightweight data collection every 15 minutes. 
* This records external temperature, humidity, and pressure from the BME280 sensor via the C++ binary (`horus_app --task monitor_env`), alongside CPU thermals and throttling states via `monitor_cpu.sh`.

### 3. The Master Daily Routine (Low-Frequency Sync)
Scheduled daily at 12:00 PM via `horus-daily.timer`, the system executes its heavy workload:
* Restores the system clock from the RTC battery.
* Captures a baseline high-resolution field image.
* Wakes the LTE modem, dynamically injects the `option` and `usb_wwan` drivers for the specific modem ID (1e0e:9018), and acquires a GPS fix via AT commands.
* Utilizes `rclone` to push the daily folders (images and CSVs) to the `italy-tree-deployment-dev` AWS S3 bucket.
* Restarts ZeroTier to bind the VPN to the newly established LTE interface (`usb0`/`wwan0`), opening a strict 30-minute SSH maintenance window for remote administration before safely powering down the modem.

## Project Structure & Codebase Analysis

The C++ application is structured with clear separation of concerns, managed by CMake.

* **`src/main.cpp`**: The command-line entry point that routes execution based on the `--task` argument (`capture` or `monitor_env`).
* **`src/sensors/Camera/`**: Interfaces directly with the Raspberry Pi CSI camera subsystem. Instead of relying on high-level abstractions, it uses `libcamera` to configure a `StillCapture` stream. The module allocates memory buffers, maps the kernel DMA memory to user space (`mmap`), performs a manual BGR-to-RGB byte swap in memory, and compresses the raw buffer to JPEG using `libjpeg`.
* **`src/sensors/BME280/`**: Implements raw I2C communication (`/dev/i2c-1`) to interact with the environmental sensor. It manually reads the factory calibration registers and applies Bosch's complex bit-shifting compensation formulas to calculate precise float values without relying on heavy external Python libraries.
* **`src/utils/FileSystem.cpp`**: Handles daily directory creation (`/home/horus/DataCapture/YYYY-MM-DD/`) and safe CSV appending operations.
* **`scripts/` & `config/`**: Contains the deployment setup (`deploy_service.sh`) which provisions the systemd network, and `horus.conf` which centralizes global variables like file paths and bucket names.

## Technologies Used

* **Languages:** C++17, Bash.
* **Build System:** CMake (v3.16+).
* **Core C++ Libraries:** * `libcamera` (ISP and hardware stream control).
  * `jpeglib` (Image compression).
  * `<linux/i2c-dev.h>` (Low-level bus communication).
* **OS & Orchestration:** Raspberry Pi OS Bookworm, Systemd (Timers & Services).
* **Networking & Cloud:** AT Command set (Modem control), `rclone` (S3 syncing), ZeroTier (SD-WAN for remote SSH).


## Deployment Log:
```
Last login: Thu Feb  5 10:27:53 CET 2026 from 172.30.120.55 on pts/1
horus@horus:~$ tail -f Horus/log/daily_log.txt
[0:01:08.954899813] [1027] ERROR DeviceEnumerator device_enumerator.cpp:172 Removing media device /dev/media3 while still in use
[0:01:08.955036160] [1027] ERROR DeviceEnumerator device_enumerator.cpp:172 Removing media device /dev/media4 while still in use
[0:01:08.955077444] [1027] ERROR DeviceEnumerator device_enumerator.cpp:172 Removing media device /dev/media1 while still in use
[0:01:08.955113464] [1027] ERROR DeviceEnumerator device_enumerator.cpp:172 Removing media device /dev/media2 while still in use
2026-02-05 13:21:20 - [Cloud] Syncing to S3...
2026-02-05 13:21:20 - [Cloud] Syncing incomplete data from 2026-02-04...
2026-02-05 13:21:23 - [Cloud] Syncing data from 2026-02-05...
2026-02-05 13:21:36 - [Cloud] Upload complete.
2026-02-05 13:21:36 - [Maintenance] Window OPEN (30 min). SSH is possible.
2026-02-05 13:21:36 - [ZeroTier] Restarting service to bind to LTE...
^C
```
```
horus@horus:~$ date
Thu  5 Feb 13:21:45 CET 2026
horus@horus:~$ tail -f Horus/log/boot_log.txt
2026-02-05 10:11:20 - [Boot] Modem silenced. System ready.
2026-02-05 10:30:46 - [Boot] Silencer started. Waiting for modem...
2026-02-05 10:30:46 - [Modem] Re-applying USB Drivers...
2026-02-05 10:51:01 - [Boot] Modem found at /dev/ttyUSB2.
2026-02-05 10:51:02 - [Boot] Sending Sleep Command (AT+CFUN=0)...
2026-02-05 10:30:46 - [Boot] Silencer started. Waiting for modem...
2026-02-05 10:30:46 - [Modem] Re-applying USB Drivers...
2026-02-05 13:20:23 - [Boot] Modem found at /dev/ttyUSB2.
2026-02-05 13:20:23 - [Boot] Sending Sleep Command (AT+CFUN=0)...
2026-02-05 13:20:27 - [Boot] Modem silenced. System ready.
^C
```
```
horus@horus:~$ tail -f Horus/log/daily_log.txt
[0:01:08.954899813] [1027] ERROR DeviceEnumerator device_enumerator.cpp:172 Removing media device /dev/media3 while still in use
[0:01:08.955036160] [1027] ERROR DeviceEnumerator device_enumerator.cpp:172 Removing media device /dev/media4 while still in use
[0:01:08.955077444] [1027] ERROR DeviceEnumerator device_enumerator.cpp:172 Removing media device /dev/media1 while still in use
[0:01:08.955113464] [1027] ERROR DeviceEnumerator device_enumerator.cpp:172 Removing media device /dev/media2 while still in use
2026-02-05 13:21:20 - [Cloud] Syncing to S3...
2026-02-05 13:21:20 - [Cloud] Syncing incomplete data from 2026-02-04...
2026-02-05 13:21:23 - [Cloud] Syncing data from 2026-02-05...
2026-02-05 13:21:36 - [Cloud] Upload complete.
2026-02-05 13:21:36 - [Maintenance] Window OPEN (30 min). SSH is possible.
2026-02-05 13:21:36 - [ZeroTier] Restarting service to bind to LTE...
2026-02-05 13:23:36 - [ZeroTier] SUCCESS: Connected (ONLINE). Virtual IP: 172.30.120.112
^C
```
