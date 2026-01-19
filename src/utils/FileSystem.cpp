#include <filesystem>
#include <ctime>

namespace fs = std::filesystem;

std::string generateTodaysFolder(){
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