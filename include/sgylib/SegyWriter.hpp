#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include "SegyReader.hpp"

class SegyWriter {
public:
    // Конструктор, создающий Writer на основе существующего Reader.
    explicit SegyWriter(const std::string& filename, const SegyReader& reader);
    
    // Конструктор, создающий Writer с явно заданными параметрами.
    SegyWriter(const std::string& filename,
               const std::vector<char>& text_header,
               const std::vector<uint8_t>& bin_header,
               int num_samples,
               float sample_interval);
    
    ~SegyWriter();

    // Запрещаем копирование, т.к. класс управляет файлом.
    SegyWriter(const SegyWriter&) = delete;
    SegyWriter& operator=(const SegyWriter&) = delete;

    // Записывает одну трассу (заголовок + отсчеты).
    void write_trace(const std::vector<uint8_t>& header, const std::vector<float>& samples);

    // Записывает целый сейсмосбор (несколько заголовков и трасс).
    void write_gather(const std::vector<std::vector<uint8_t>>& headers, const std::vector<std::vector<float>>& traces);
    
    // Псевдоним для обратной совместимости, если нужен.
    void write_gather_block(const std::vector<std::vector<uint8_t>>& headers, const std::vector<std::vector<float>>& traces);

    // Текущее количество записанных трасс.
    int num_traces() const { return num_traces_; }

private:
    std::string filename_;
    std::ofstream file_;
    std::vector<char> text_header_;
    std::vector<uint8_t> bin_header_;
    int num_traces_ = 0;
    int num_samples_ = 0;
    float sample_interval_ = 0.0f;
    int trace_bsize_ = 0;

    // --- ИСПРАВЛЕНИЕ: ДОБАВЛЕНЫ ОБЪЯВЛЕНИЯ ПРИВАТНЫХ МЕТОДОВ ---
    
    /**
     * @brief Инициализирует и открывает файл, записывает начальные заголовки.
     */
    void init();
    
    /**
     * @brief Обновляет бинарный заголовок и корректно закрывает файл.
     */
    void finalize_file();
};