#pragma once

#include <iostream>
#include <string>

// Эта функция теперь будет доступна всем, кто подключит "util.hpp"
inline void print_progress_bar(const std::string& label, int current, int total, int width = 50) {
    if (total == 0) return;
    float progress = static_cast<float>(current) / total;
    int bar_width = static_cast<int>(progress * width);

    std::cout << "\r" << label;
    std::cout.width(30 - label.length()); // Выравнивание
    std::cout << std::left << ": [";
    for (int i = 0; i < width; ++i) {
        std::cout << (i < bar_width ? '#' : '.');
    }
    std::cout << "] " << static_cast<int>(progress * 100.0) << "%" << std::flush;

    if (current == total) {
        std::cout << std::endl;
    }
}