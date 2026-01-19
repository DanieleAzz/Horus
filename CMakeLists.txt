cmake_minimum_required(VERSION 3.16)
project(Horus VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Packages
find_package(libcamera REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
# Paho MQTT (Manual find might be needed if not in standard path, but usually works)
find_library(PAHO_MQTT_CPP paho-mqttpp3)
find_library(PAHO_MQTT_C paho-mqtt3as) 

include_directories(include src)

add_executable(horus_app
    src/main.cpp
    src/sensors/Camera/Camera.cpp
    src/sensors/BME280.cpp
    src/sensors/DS18B20.cpp
    src/connectivity/MqttClient.cpp
    src/utils/FileSystem.cpp
)

target_link_libraries(horus_app PRIVATE
    libcamera::libcamera
    nlohmann_json::nlohmann_json
    ${PAHO_MQTT_CPP}
    ${PAHO_MQTT_C}
    i2c # For the BME280
)