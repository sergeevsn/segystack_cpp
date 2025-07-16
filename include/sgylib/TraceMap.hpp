#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>

// Прямое объявление, чтобы не включать заголовок sqlite3 в hpp-файл
struct sqlite3;
class SegyReader;

/**
 * @class TraceMap
 * @brief Создает и управляет картой трасс из SEG-Y файла, используя SQLite для хранения на диске.
 * 
 * Этот класс решает две основные проблемы при работе с большими SEG-Y файлами:
 * 1. Медленное создание карты: используется OpenMP для параллельного сканирования заголовков.
 * 2. Высокое потребление ОЗУ: карта хранится в базе данных SQLite на диске, а не в памяти.
 */
class TraceMap {
public:
    /**
     * @brief Конструктор.
     * @param db_path Путь к файлу базы данных SQLite. Если файл не существует, он будет создан.
     * @param keys Список имен полей заголовка трассы (например, "cdp", "offset"), которые будут использоваться в качестве ключей для индексации.
     */
    TraceMap(const std::string& db_path, const std::vector<std::string>& keys);
    ~TraceMap();

    // Запрещаем копирование, так как управляем ресурсом (БД)
    TraceMap(const TraceMap&) = delete;
    TraceMap& operator=(const TraceMap&) = delete;

    /**
     * @brief Сканирует SEG-Y файл и строит карту трасс в базе данных SQLite.
     * Если карта в БД уже существует, она будет полностью перезаписана.
     * @param reader Экземпляр SegyReader для доступа к файлу.
     */
    void build_map(const SegyReader& reader);

    /**
     * @brief Находит индексы трасс, соответствующих заданным значениям ключей.
     * @param key_values Вектор значений для поиска. Порядок должен соответствовать ключам, заданным в конструкторе.
     *                   Используйте std::nullopt для ключей, которые не нужно проверять.
     * @return Вектор индексов трасс, удовлетворяющих условиям.
     */
    std::vector<int> find_trace_indices(const std::vector<std::optional<int>>& key_values) const;

    /**
     * @brief Получает все уникальные значения для указанного ключа из карты.
     * @param key Имя ключа (должно быть одним из ключей, переданных в конструктор).
     * @return Отсортированный вектор уникальных значений.
     */
    std::vector<int> get_unique_values(const std::string& key) const;

    const std::string& db_path() const { return db_path_; }
    const std::vector<std::string>& keys() const { return keys_; }

private:
    void open_db();
    void create_table();
    void check_db_error(int error_code, const char* context) const;
    int find_key_index(const std::string& key) const;

    // Хелперы для сериализации/десериализации вектора индексов в/из BLOB
    static std::vector<char> serialize_indices(const std::vector<int>& indices);
    static std::vector<int> deserialize_indices(const char* data, int size);

    std::string db_path_;
    std::vector<std::string> keys_;
    sqlite3* db_ = nullptr;
    bool has_seq_number_ = false; // Флаг для специальной обработки 'sequence_number'
};