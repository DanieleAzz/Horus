#include <iostream>
#include <string>
#include <cstring> // for strcmp
#include <chrono>
#include <iomanip>
#include <sstream>

// Include our modules
#include "sensors/Camera/Camera.hpp"
#include "utils/FileSystem.hpp"
#include "sensors/BME280/bme280.hpp"


// Helper to get a timestamped filename
// Returns: "2026-01-21T12:34:03CET" ISO 8601 format
std::string getTimestamped(const std::string& extension) {
    //ISO 8601 format
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    if(extension == ".csv"){
        //
        ss << std::put_time(std::localtime(&in_time_t), "%FT%H:%M:%S%Z");
    }else{
        ss << std::put_time(std::localtime(&in_time_t), "img_%FT%H:%M:%S%Z");
        ss << extension;
    }
    return ss.str();
}

void printUsage() {
    std::cout << "Horus Edge System v0.2" << std::endl;
    std::cout << "Usage: ./horus_app --task <task_name>" << std::endl;
    std::cout << "Tasks:" << std::endl;
    std::cout << "  capture          : Capture image from CSI camera" << std::endl;
    std::cout << "  monitor_internal : Read BME280 (Internal Temp)" << std::endl;
    std::cout << "  log_env          : Read DS18B20 (External Temp) & Save CSV" << std::endl;
}

bool getBME280Data(horus::BME280Data& bme280_data) {
    horus::BME280 sensor(0x76, 1);
    if (sensor.init()){
        bme280_data = sensor.readAll();
        return true;
    }
    return false;
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

    if(task== "capture"){
        // --- CAMERA TASK ---
        
        // A. Prepare the File System
        // We use the full path so we don't depend on where we run the script from
        std::string folderPath = horus::utils::getTodaysFolder();
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

    } else if(task == "monitor_internal"){
        std::cout << "[Main] Checking Internal Environment..." << std::endl;
        horus::BME280Data env_internal_data;

        if(getBME280Data(env_internal_data)){
            std::cout << "Internal Temp: " << env_internal_data.temperature << " C" << std::endl;
            std::cout << "Internal Hum:  " << env_internal_data.humidity << " %" << std::endl;
            if(env_internal_data.temperature > 60.0){
                std::cerr << "WARNING: Internal temperature overheating!" << std::endl;
                // execute --> task = "overheating";
        }else{
            std::cerr << "[Main] Failed to read BME280." << std::endl;
            //todo Write to log the error
            return 1;
            }
        }
    }else if(task == "monitor_external"){

        // TODO: DS18B20 logic
        // horus::DS18B20 env_external_data;
        // float external_temp = env_external_data.getData();
        float external_temp = 20.0; //dummy data for now
        
        // Read internal pressure from BME280:
        horus::BME280Data internal_data;
        float pressure = 0.0f;
        if(getBME280Data(internal_data)){
            pressure = internal_data.pressure;
        }

        std::cout << "External Temp: " << external_temp << " C" << std::endl;
        std::cout << "Pressure: " << pressure << " hPa" << std::endl;
        
        // Save to CSV the temperature data coming from DS18B20:
        std::string timestamp = getTimestampedFilename(".csv");

        // Formatting: Timestamp, External Data, Pressure:
        std::stringstream csvData;
        // csvData << env_external_data << "," << pressure;
        csvData << external_temp << "," << pressure;

        horus::utils::appendToCSV("enviromental_data.csv", timestamp, csvData.str());
    
    }

    else{
        std::cerr << "[Main] Unknown task: " << task << std::endl;
        printUsage();
        return 1;
    }

    return 0;
}