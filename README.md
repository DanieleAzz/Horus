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

### Deployment Log:
Last login: Thu Feb  5 10:27:53 CET 2026 from 172.30.120.55 on pts/1
horus@horus:~$ tail -f Horus/log/daily_log.txt
[0:01:08.954899813] [1027] ERROR DeviceEnumerator device_enumerator.cpp:172 Remo                                                 ving media device /dev/media3 while still in use
[0:01:08.955036160] [1027] ERROR DeviceEnumerator device_enumerator.cpp:172 Remo                                                 ving media device /dev/media4 while still in use
[0:01:08.955077444] [1027] ERROR DeviceEnumerator device_enumerator.cpp:172 Remo                                                 ving media device /dev/media1 while still in use
[0:01:08.955113464] [1027] ERROR DeviceEnumerator device_enumerator.cpp:172 Remo                                                 ving media device /dev/media2 while still in use
2026-02-05 13:21:20 - [Cloud] Syncing to S3...
2026-02-05 13:21:20 - [Cloud] Syncing incomplete data from 2026-02-04...
2026-02-05 13:21:23 - [Cloud] Syncing data from 2026-02-05...
2026-02-05 13:21:36 - [Cloud] Upload complete.
2026-02-05 13:21:36 - [Maintenance] Window OPEN (30 min). SSH is possible.
2026-02-05 13:21:36 - [ZeroTier] Restarting service to bind to LTE...
^C
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
