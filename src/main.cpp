#include <iostream>
#include <string>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <sstream>

// Include our modules
#include "sensors/Camera/Camera.hpp"
#include "utils/FileSystem.hpp"
#include "sensors/BME280/bme280.hpp"

// --- HELPERS ---

// Returns ISO 8601 string: "2026-02-03T12:00:00"
// or specific format for images
std::string getTimestamped(const std::string& extension) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    if(extension == ".csv"){
        ss << std::put_time(std::localtime(&in_time_t), "%FT%H:%M:%S%Z");
    } else {
        ss << std::put_time(std::localtime(&in_time_t), "img_%FT%H:%M:%S%Z");
        ss << extension;
    }
    return ss.str();
}

void printUsage() {
    std::cout << "Horus Edge System v1.0 (Torino Release)" << std::endl;
    std::cout << "Usage: ./horus_app --task <task_name>" << std::endl;
    std::cout << "Tasks:" << std::endl;
    std::cout << "  capture      : Capture image from CSI camera" << std::endl;
    std::cout << "  monitor_env  : Read BME280 & Save to CSV" << std::endl;
}

// Helper to init and read BME280
bool getBME280Data(horus::BME280Data& data) {
    horus::BME280 sensor(0x77, 1); // Address 0x76, Bus 1
    if (sensor.init()){
        data = sensor.readAll();
        return true;
    }
    return false;
}

// --- MAIN ---

int main(int argc, char* argv[]) {
    // 1. Parse Arguments
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string task = "";
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--task", 6) == 0) {
            if (i + 1 < argc) {
                task = argv[i+1];
            } else {
                char* eq = std::strchr(argv[i], '=');
                if (eq) task = eq + 1;
            }
        }
    }

    if (task.empty()) {
        std::cerr << "[Main] Error: No task specified." << std::endl;
        return 1;
    }

    std::cout << "[Main] Starting Task: " << task << std::endl;

    // 2. Task Router

    if(task == "capture"){
        // --- TASK: IMAGE CAPTURE ---
        std::string folderPath = horus::utils::getTodaysFolder();
        std::string fullPath = folderPath + "/" + getTimestamped(".jpg");
        std::cout << "[Main] Target File: " << fullPath << std::endl;

        horus::Camera cam;
        if (!cam.start()) {
            std::cerr << "[Main] Critical: Camera init failed." << std::endl;
            return 2;
        }

        if (cam.capture(fullPath)) {
            std::cout << "[Main] Capture Success." << std::endl;
        } else {
            std::cerr << "[Main] Capture Failed." << std::endl;
            return 3;
        }
        cam.stop();
    } 
    
    else if(task == "monitor_env"){
        // --- TASK: ENVIRONMENTAL LOGGING ---
        // Used to be monitor_external, now consolidated for BME280
        
        horus::BME280Data data;
        if(getBME280Data(data)){
            // 1. Print to Console (for debugging/journalctl)
            std::cout << "Temp: " << data.temperature << " C | ";
            std::cout << "Hum: "  << data.humidity << " % | ";
            std::cout << "Pres: " << data.pressure << " hPa" << std::endl;

            // 2. CSV Formatting
            // Format: Timestamp, Temp, Humidity, Pressure
            std::string timestamp = getTimestamped(".csv");
            std::stringstream csvRow;
            csvRow << data.temperature << "," << data.humidity << "," << data.pressure;

            // 3. Save to File
            // Note: csv header should be: Timestamp,Temperature,Humidity,Pressure
            horus::utils::appendToCSV("environmental_data.csv", timestamp, csvRow.str());
            std::cout << "[Main] Data appended to CSV." << std::endl;

        } else {
            std::cerr << "[Main] Failed to read BME280 sensor." << std::endl;
            return 1;
        }
    } 
    
    else {
        std::cerr << "[Main] Unknown task: " << task << std::endl;
        printUsage();
        return 1;
    }

    return 0;
}