#include <iostream>
#include <string>
#include <cstring> // for strcmp
#include <chrono>
#include <iomanip>
#include <sstream>

// Include our modules
#include "sensors/Camera/Camera.hpp"
#include "utils/FileSystem.hpp"
#include "BME280/bme280.hpp"


// Helper to get a timestamped filename
// Returns: "img_12-00-01.jpeg"
std::string getTimestampedFilename(const std::string& extension) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "img_%FT%H:%M:%S%Z");
    ss << extension;
    return ss.str();
}

void printUsage() {
    std::cout << "Horus Edge System v0.2" << std::endl;
    std::cout << "Usage: ./horus_app --task <task_name>" << std::endl;
    std::cout << "Tasks:" << std::endl;
    std::cout << "  capture          : Capture image from CSI camera" << std::endl;
    std::cout << "  monitor_internal : Read BME280 (Internal Temp)" << std::endl;
    std::cout << "  log_env          : Read DS18B20 (External Temp)" << std::endl;
}

int main(int argc, char* argv[]) {
    // 1. Argument Parsing
    // We expect at least one argument: the task name.
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string task = "";
    
    // Simple manual parsing (no extra libraries needed)
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--task", 6) == 0) {
            if (i + 1 < argc) {
                task = argv[i+1]; // Get the value after --task
            } else { // Handle --task=capture format
                char* eq = std::strchr(argv[i], '=');
                if (eq) task = eq + 1;
            }
        }
    }

    if (task.empty()) {
        std::cerr << "[Main] Error: No task specified." << std::endl;
        return 1;
    }

    // 2. Task Execution Router
    std::cout << "[Main] Starting Task: " << task << std::endl;

    if (task == "capture") {
        // --- CAMERA TASK ---
        
        // A. Prepare the File System
        // We use the full path so we don't depend on where we run the script from
        std::string folderPath = horus::utils::generateTodaysFolder();
        std::string fullPath = folderPath + "/" + getTimestampedFilename(".jpg");
        std::cout << "[Main] Target File: " << fullPath << std::endl;

        // B. Initialize Camera
        horus::Camera cam;
        if (!cam.start()) {
            std::cerr << "[Main] Critical: Could not start camera." << std::endl;
            return 2; // Return non-zero for systemd to know it failed
        }

        // C. Capture
        if (cam.capture(fullPath)) {
            std::cout << "[Main] Capture Success." << std::endl;
            // TODO: Trigger MQTT Upload here in future
        } else {
            std::cerr << "[Main] Capture Failed." << std::endl;
            return 3;
        }
        
        cam.stop();

    } else if (task == "monitor_internal") {
        std::cout << "[Main] Checking Internal Environment..." << std::endl;
        // Default address 0x76, Bus 1
        horus::BME280 sensor(0x76, 1);

        if (sensor.init())
        {
            auto data = sensor.readAll();
            std::cout << "--- Internal Status ---" << std::endl;
            std::cout << "Temp: " << data.temperature << " C" << std::endl;
            std::cout << "Hum:  " << data.humidity << " %" << std::endl;
            std::cout << "Pres: " << data.pressure << " hPa" << std::endl;

            // Safety check for internal box temperature:
            if(data.temperature > 60.0){
                std::cerr << "WARNING: Internal temperature overheating!" << std::endl;
            }
        } else{
            std::cerr << "[Main] Failed to initialize BME280." << std::endl;
        return 4;
        }
        

    } else if (task == "log_env") {
        // TODO: DS18B20 logic
        std::cout << "[Main] Environment logging not implemented yet." << std::endl;

    } else {
        std::cerr << "[Main] Unknown task: " << task << std::endl;
        printUsage();
        return 1;
    }

    return 0;
}