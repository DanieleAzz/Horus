#pragma once
#include <string>

namespace horus {
namespace utils {

    // Returns a path like: "/home/horus/DataCapture/2026-01-20/"
    // Creates the directory if it doesn't exist.
    std::string getTodaysFolder();

    // Appends a line to a CSV file in today's folder.
    // If the file doesn't exist, it creates it and adds a header.
    void appendToCSV(const std::string& filename, const std::string& timestamp, const std::string& env_data);

}
}