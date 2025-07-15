#include "sgylib/SegyUtil.hpp"
#include <iostream>

void print_progress_bar(int current, int total, int width) {
    float progress = static_cast<float>(current) / total;
    int bar_width = static_cast<int>(progress * width);

    std::cout << "\rProgress [";
    for (int i = 0; i < width; ++i)
        std::cout << (i < bar_width ? '#' : '.');
    std::cout << "] (" << current << "/" << total << ")" << std::flush;

    if (current == total)
        std::cout << "\n";
} 