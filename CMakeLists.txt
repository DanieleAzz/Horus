cmake_minimum_required(VERSION 3.16)
project(Horus VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
find_package(PkgConfig REQUIRED)
find_package(JPEG REQUIRED)

# 1. Use PkgConfig to find libcamera
pkg_check_modules(LIBCAMERA REQUIRED libcamera)

# 2. Find other packages
find_package(nlohmann_json 3.2.0 REQUIRED)

# 3. Paho MQTT (Try PkgConfig first, fallback to manual if needed)
pkg_check_modules(PAHO_MQTT_CPP paho-mqttpp3)
pkg_check_modules(PAHO_MQTT_C paho-mqtt3as) 

# --- Include Directories ---
# We must explicitly add the include dirs found by PkgConfig
include_directories(
    include 
    src
    ${LIBCAMERA_INCLUDE_DIRS}
    ${PAHO_MQTT_CPP_INCLUDE_DIRS}
    ${JPEG_INCLUDE_DIRS}
)

# --- Source Files ---
# Note: Ensure these files actually exist or CMake will complain
add_executable(horus_app
    src/main.cpp
    src/sensors/Camera/Camera.cpp
    src/sensors/BME280/bme280.cpp
    # src/sensors/DS18B20/DS18B20.cpp <-- Commented out until you create them
    src/utils/FileSystem.cpp
)

# --- Linking ---
target_link_libraries(horus_app PRIVATE
    ${LIBCAMERA_LIBRARIES}
    nlohmann_json::nlohmann_json
    ${PAHO_MQTT_CPP_LIBRARIES}
    ${PAHO_MQTT_C_LIBRARIES}
    i2c
    ${JPEG_LIBRARIES}
)

# --- Compile Options ---
# Add necessary flags found by PkgConfig (sometimes defines are needed)
target_compile_options(horus_app PRIVATE ${LIBCAMERA_CFLAGS_OTHER})