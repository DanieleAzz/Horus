#pragma once
#include <string>

namespace horus {
namespace utils {

    // Returns a path like: "/home/horus/DataCapture/2026-01-20/"
    // Creates the directory if it doesn't exist.
    std::string generateTodaysFolder();

}
}