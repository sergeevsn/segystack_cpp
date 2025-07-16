#pragma once

#include <iostream>
#include <string>

inline void print_progress_bar(const std::string& label, int current, int total, int width = 50) {
    if (total == 0) return;

    // --- ИСПРАВЛЕНИЕ 1: Правильный расчет прогресса и ширины ---
    // Используем double для большей точности и добавляем 0.5 для правильного математического округления
    double progress = static_cast<double>(current) / total;
    int bar_width = static_cast<int>(progress * width + 0.5);

    // --- ИСПРАВЛЕНИЕ 2: Правильное форматирование вывода ---
    std::cout << "\r" // Возврат каретки
              << std::left << std::setw(30) << label // Устанавливаем фиксированную ширину метки и выравниваем по левому краю
              << ": [";

    for (int i = 0; i < width; ++i) {
        std::cout << (i < bar_width ? '#' : '.');
    }
    std::cout << "] " 
              << std::right << std::setw(3) << static_cast<int>(progress * 100.0) << "%" // Фиксированная ширина для процентов
              << " (" << current << "/" << total << ")"
              << std::flush; // Принудительный сброс буфера в консоль

    // --- ИСПРАВЛЕНИЕ 3: Перевод строки только ПОСЛЕ финального вывода ---
    if (current >= total) {
        std::cout << std::endl;
    }
}