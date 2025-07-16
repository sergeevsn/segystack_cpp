#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <stdexcept>

Config load_config(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Could not open config file: " + filename);
    }
    std::string line;
    std::unordered_map<std::string, std::string> params;

    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        std::istringstream is_line(line);
        std::string key;
        if (std::getline(is_line, key, '=')) {
            std::string value;
            if (std::getline(is_line, value)) {
                params[key] = value;
            }
        }
    }

    Config cfg;
    cfg.input_file = params["input_file"];
    cfg.output_file = params["output_file"];
    cfg.velocity_file = params["velocity_file"];
    cfg.nmo_stretch_muting_percent = std::stod(params["nmo_stretch_muting_percent"]);
    cfg.num_threads = std::stoi(params["num_threads"]);
    return cfg;
} 