#include <filesystem>
#include <ctime>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
namespace horus {
namespace utils {

std::string getTodaysFolder(){
    // Get current local time:
    std::time_t t = std::time(nullptr);
    char local_time[100];
    std::strftime(local_time, sizeof(local_time), "%Y-%m-%d", std::localtime(&t));

    // Create Path:
    std::string path = "/home/horus/DataCapture/"+ std::string(local_time);
    // Create the folder it doesnt exist
    if(!fs::exists(path)){
        fs::create_directories(path);
    }
    return path;
}

void appendToCSV(const std::string& filename, const std::string& timestamp, const std::string& env_data) {

    std::string path = getTodaysFolder() + "/" + filename;
    
    
    // Check if file exists to write header
    bool fileExists = fs::exists(path);
    
    std::ofstream file(path, std::ios::app); // Append mode

    if (file.is_open()) {
        if (!fileExists) {
            file << "Timestamp,External_Temperature_C,Pressure_hPa\n"; //First row of the .csv file
        }
        file << timestamp << "," << env_data << "\n";
        std::cout << "[FileSystem] Data appended to " << path << " successfully." << std::endl;
        file.close();
    }
}

}
}