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
