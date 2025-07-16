#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <stdexcept>
#include <string>


namespace {
    std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) {
            return ""; // Строка пуста или состоит только из пробелов
        }
        auto end = s.find_last_not_of(" \t\n\r");
        return s.substr(start, end - start + 1);
    }
    }
    
    Config load_config(const std::string& filename) {
        std::ifstream file(filename);
        if (!file) {
            throw std::runtime_error("Could not open config file: " + filename);
        }
        
        std::unordered_map<std::string, std::string> params;
        std::string line;
        int line_num = 0;
    
        while (std::getline(file, line)) {
            line_num++;
            // Пропускаем пустые строки и комментарии
            if (line.empty() || trim(line)[0] == '#') {
                continue;
            }
    
            std::istringstream is_line(line);
            std::string key, value;
            if (std::getline(is_line, key, '=') && std::getline(is_line, value)) {
                // ИСПОЛЬЗУЕМ trim для очистки ключа и значения
                std::string cleaned_key = trim(key);
                std::string cleaned_value = trim(value);
                if (!cleaned_key.empty()) {
                    params[cleaned_key] = cleaned_value;
                }
            } else if (!trim(line).empty()) {
                // Если в строке нет '=', но она не пустая - это ошибка формата
                throw std::runtime_error("Invalid format in config file at line " + std::to_string(line_num) + ": " + line);
            }
        }
    
        Config cfg; // Создаем конфиг со значениями по умолчанию
    
        try {
            // --- ОБЯЗАТЕЛЬНЫЕ ПАРАМЕТРЫ ---
            // Используем .at() который выбросит исключение, если ключ не найден
            cfg.input_file = params.at("input_file");
            cfg.output_file = params.at("output_file");
            cfg.velocity_file = params.at("velocity_file");
    
        } catch (const std::out_of_range& oor) {
            // Перехватываем исключение и выдаем понятное сообщение
            throw std::runtime_error(std::string("Missing mandatory parameter in config file: ") + oor.what());
        }
    
        // --- ОПЦИОНАЛЬНЫЕ ПАРАМЕТРЫ ---
        // Проверяем наличие ключа перед тем, как его использовать.
        // Если ключа нет, используется значение по умолчанию из Config.hpp.
        if (params.count("nmo_stretch_muting_percent")) {
            cfg.nmo_stretch_muting_percent = std::stof(params.at("nmo_stretch_muting_percent"));
        }
        if (params.count("num_threads")) {
            cfg.num_threads = std::stoi(params.at("num_threads"));
        }
    
        return cfg;
    }