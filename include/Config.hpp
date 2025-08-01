#pragma once
#include <string>

struct Config {
    std::string input_file;
    std::string output_file;
    std::string velocity_file;
    double nmo_stretch_muting_percent;
    int num_threads = 0; 
};

Config load_config(const std::string& filename); 