#pragma once

#include <string>
#include <vector>
#include <memory> // Для std::shared_ptr
#include <cstdint>
#include <fstream>
#include <unordered_map>
#include <optional>
#include "TraceMap.hpp" // Включаем новый заголовок TraceMap

class SegyReader {
public:
    /**
     * @brief Основной конструктор. Открывает SEG-Y файл для чтения.
     * @param filename Путь к SEG-Y файлу.
     * @param mode Режим открытия ("r" - чтение, "r+" - чтение/запись).
     */
    explicit SegyReader(const std::string& filename, const std::string& mode = "r");
    ~SegyReader();

    // Запрещаем копирование и присваивание, т.к. класс управляет файловым ресурсом.
    SegyReader(const SegyReader&) = delete;
    SegyReader& operator=(const SegyReader&) = delete;

    // --- НОВЫЕ МЕТОДЫ ДЛЯ УПРАВЛЕНИЯ TraceMap ---

    /**
     * @brief Создает и строит TraceMap для этого ридера.
     * Этот метод сканирует весь SEG-Y файл, используя OpenMP, и сохраняет результат
     * в файле базы данных SQLite. Если файл БД уже существует, он будет перезаписан.
     * @param map_name Внутреннее имя для этой карты (например, "cdp_gather").
     * @param db_path Путь к файлу SQLite, где будет храниться карта.
     * @param keys Ключи заголовка для построения карты (например, {"cdp", "offset"}).
     */
    void build_tracemap(const std::string& map_name, const std::string& db_path, const std::vector<std::string>& keys);

    /**
     * @brief Загружает ранее созданную TraceMap из файла БД.
     * Этот метод не сканирует SEG-Y файл, а лишь открывает существующую карту,
     * что делает его очень быстрым.
     * @param map_name Внутреннее имя для этой карты.
     * @param db_path Путь к файлу SQLite.
     * @param keys Ключи, с которыми была создана эта карта (должны совпадать).
     */
    void load_tracemap(const std::string& map_name, const std::string& db_path, const std::vector<std::string>& keys);

    // --- ОСНОВНЫЕ МЕТОДЫ ДОСТУПА К ДАННЫМ ---

    std::vector<float> get_trace(int index) const;
    std::vector<uint8_t> get_trace_header(int index) const;

    std::vector<std::vector<float>> get_gather(const std::string& tracemap_name,
                                               const std::vector<std::optional<int>>& keys) const;

    std::vector<std::vector<uint8_t>> get_gather_headers(const std::string& tracemap_name,
                                                         const std::vector<std::optional<int>>& keys) const;

    void get_gather_and_headers(const std::string& tracemap_name,
                                const std::vector<std::optional<int>>& keys,
                                std::vector<std::vector<uint8_t>>& headers,
                                std::vector<std::vector<float>>& traces) const;

    // --- ГЕТТЕРЫ И ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ---

    int num_traces() const;
    int num_samples() const;
    float sample_interval() const;

    int32_t get_header_value_i32(int trace_index, const std::string& key) const;
    int32_t get_header_value_i32(const std::vector<uint8_t>& trace_header, const std::string& key) const;
    int16_t get_header_value_i16(const std::vector<uint8_t>& trace_header, const std::string& key) const;

    int32_t get_bin_header_value_i32(const std::string& key) const;
    int16_t get_bin_header_value_i16(const std::string& key) const;

    const std::vector<char>& text_header() const { return text_header_; }
    const std::vector<uint8_t>& bin_header() const { return bin_header_; }

    /**
     * @brief Возвращает указатель на загруженную карту трасс.
     * @param name Имя карты, заданное в build_tracemap или load_tracemap.
     * @return Умный указатель на объект TraceMap.
     */
    std::shared_ptr<TraceMap> get_tracemap(const std::string& name) const;


private:
    std::string filename_;
    std::string mode_;
    mutable std::fstream file_;
    std::vector<char> text_header_;
    std::vector<uint8_t> bin_header_;
    int num_traces_ = 0;
    int num_samples_ = 0;
    float sample_interval_ = 0.0f;
    int trace_bsize_ = 0;

    // ИЗМЕНЕНО: Храним умные указатели на TraceMap, а не сами объекты.
    std::unordered_map<std::string, std::shared_ptr<TraceMap>> tracemaps_;

    // Вспомогательные методы для вычисления смещений в файле
    inline std::streamoff data_offset() const { return 3200 + 400; }
    inline std::streamoff trace_offset(int index) const { return data_offset() + static_cast<std::streamoff>(index) * trace_bsize_; }
    inline std::streamoff trace_data_offset(int index) const { return trace_offset(index) + 240; }

    void read_gather_block(const std::vector<int>& indices,
                           std::vector<std::vector<uint8_t>>& headers,
                           std::vector<std::vector<float>>& traces) const;
};