#include "sgylib/TraceMap.hpp"
#include "sgylib/SegyReader.hpp"
#include "sgylib/TraceFieldMap.hpp"
#include "sgylib/SegyUtil.hpp"
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <cstring>
#include <iostream>
#include "util.hpp"

// Заголовки для параллелизации и работы с БД
#include <omp.h>
#include <sqlite3.h>


// Хеш-функция для std::vector<int>, нужна для временной карты в памяти
struct VectorHash {
    std::size_t operator()(const std::vector<int>& v) const {
        std::size_t seed = v.size();
        for (int i : v) {
            seed ^= std::hash<int>{}(i) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

TraceMap::TraceMap(const std::string& db_path, const std::vector<std::string>& keys)
    : db_path_(db_path), keys_(keys) 
{
    if (keys.empty()) {
        throw std::invalid_argument("Keys vector cannot be empty.");
    }
    // Проверяем наличие специального ключа 'sequence_number'
    if (!keys_.empty() && keys_.back() == "sequence_number") {
        has_seq_number_ = true;
        // Удаляем его из основного списка ключей, так как он обрабатывается особо
        keys_.pop_back(); 
    }
    open_db();
    create_table();
}

TraceMap::~TraceMap() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void TraceMap::open_db() {
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db_)));
    }
    // Оптимизация для скорости записи
    sqlite3_exec(db_, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);
}

void TraceMap::create_table() {
    std::stringstream sql;
    sql << "CREATE TABLE IF NOT EXISTS trace_map (";
    for (const auto& key : keys_) {
        sql << "\"" << key << "\" INTEGER NOT NULL, ";
    }
    sql << "indices BLOB NOT NULL, ";
    sql << "PRIMARY KEY (";
    for (size_t i = 0; i < keys_.size(); ++i) {
        sql << "\"" << keys_[i] << "\"" << (i == keys_.size() - 1 ? "" : ", ");
    }
    sql << "));";

    check_db_error(sqlite3_exec(db_, sql.str().c_str(), nullptr, nullptr, nullptr), "Table creation");
}

void TraceMap::build_map(const SegyReader& reader) {
    std::cout << "Starting high-performance map build..." << std::endl;
    int n_traces = reader.num_traces();
    size_t n_keys = keys_.size();
    
    // Определяем размер одного полного блока трассы (заголовок + данные)
    const int trace_size = 240 + reader.num_samples() * 4;
    
    // Устанавливаем большой размер буфера для чтения (например, 256 МБ)
    const size_t CHUNK_SIZE_BYTES = 256 * 1024 * 1024;
    // Сколько полных трасс помещается в наш буфер
    const int traces_per_chunk = CHUNK_SIZE_BYTES / trace_size;
    
    // Временный буфер для чтения больших кусков файла
    std::vector<char> buffer(traces_per_chunk * trace_size);

    // --- Основная карта для агрегации результатов ---
    using InMemoryMap = std::unordered_map<std::vector<int>, std::vector<int>, VectorHash>;
    InMemoryMap final_map;

    int traces_processed = 0;
    while (traces_processed < n_traces) {
        // Определяем, сколько трасс читать на этой итерации
        int traces_to_read = std::min(traces_per_chunk, n_traces - traces_processed);
        size_t bytes_to_read = traces_to_read * trace_size;
        
        // 1. Читаем большой непрерывный блок с диска за один вызов
        reader.read_raw_block(traces_processed, bytes_to_read, buffer.data());

        // 2. Параллельно обрабатываем заголовки из этого блока УЖЕ В ПАМЯТИ
        std::vector<InMemoryMap> local_maps;
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            #pragma omp single
            {
                local_maps.resize(omp_get_num_threads());
            }

            #pragma omp for schedule(dynamic)
            for (int i = 0; i < traces_to_read; ++i) {
                // Получаем указатель на заголовок внутри нашего буфера в памяти
                const char* header_ptr = buffer.data() + i * trace_size;
                
                std::vector<int> key_vals(n_keys);
                for (size_t j = 0; j < n_keys; ++j) {
                    key_vals[j] = get_trace_field_value(reinterpret_cast<const uint8_t*>(header_ptr), keys_[j]);
                }
                
                int global_trace_index = traces_processed + i;
                local_maps[thread_id][key_vals].push_back(global_trace_index);
            }
        } // Конец параллельной секции

        // 3. Сливаем результаты из локальных карт в общую
        for (const auto& local_map : local_maps) {
            for (const auto& pair : local_map) {
                final_map[pair.first].insert(final_map[pair.first].end(), pair.second.begin(), pair.second.end());
            }
        }

        traces_processed += traces_to_read;
        print_progress_bar("1/2 Reading & Processing", traces_processed, n_traces);
    }
    
    // --- Шаг 4: Запись объединенной карты в SQLite (без изменений) ---
    sqlite3_stmt* stmt;
    std::stringstream sql;
    sql << "INSERT OR REPLACE INTO trace_map (";
    for(const auto& key : keys_) sql << "\"" << key << "\", ";
    sql << "indices) VALUES (";
    for(size_t i = 0; i < keys_.size(); ++i) sql << "?, ";
    sql << "?);";

    check_db_error(sqlite3_exec(db_, "DELETE FROM trace_map;", nullptr, nullptr, nullptr), "Clear table");
    
    check_db_error(sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr), "Begin transaction");
    check_db_error(sqlite3_prepare_v2(db_, sql.str().c_str(), -1, &stmt, nullptr), "Prepare insert");

    int written_keys = 0;
    int total_keys = final_map.size();
    for (const auto& [key_vec, indices_vec] : final_map) {
        for (size_t i = 0; i < key_vec.size(); ++i) {
            sqlite3_bind_int(stmt, i + 1, key_vec[i]);
        }
        auto blob = serialize_indices(indices_vec);
        sqlite3_bind_blob(stmt, keys_.size() + 1, blob.data(), blob.size(), SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {/* обработка ошибки */}
        sqlite3_reset(stmt);
        
        if (++written_keys % 1000 == 0 || written_keys == total_keys) {
            print_progress_bar("2/2 Writing to database", written_keys, total_keys);
        }
    }
    
    sqlite3_finalize(stmt);
    check_db_error(sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr), "Commit transaction");
    std::cout << "Trace map built successfully." << std::endl;
}

std::vector<int> TraceMap::find_trace_indices(const std::vector<std::optional<int>>& key_values) const {
    std::vector<int> result_indices;
    
    // Определяем, был ли запрошен конкретный номер в последовательности
    std::optional<int> seq_num;
    if (has_seq_number_ && key_values.size() == keys_.size() + 1) {
        seq_num = key_values.back();
    }
    
    std::stringstream sql;
    sql << "SELECT indices FROM trace_map WHERE ";
    std::vector<int> bind_values;
    bool first_clause = true;

    for (size_t i = 0; i < keys_.size(); ++i) {
        if (i < key_values.size() && key_values[i].has_value()) {
            if (!first_clause) sql << " AND ";
            sql << "\"" << keys_[i] << "\" = ?";
            bind_values.push_back(*key_values[i]);
            first_clause = false;
        }
    }
    
    if (first_clause) { // Если не задано ни одного фильтра, возвращаем всё (или ничего)
        return {}; // или можно выбрать другое поведение
    }

    sqlite3_stmt* stmt;
    check_db_error(sqlite3_prepare_v2(db_, sql.str().c_str(), -1, &stmt, nullptr), "Prepare select");

    for (size_t i = 0; i < bind_values.size(); ++i) {
        sqlite3_bind_int(stmt, i + 1, bind_values[i]);
    }

    std::vector<int> combined_indices;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* blob = static_cast<const char*>(sqlite3_column_blob(stmt, 0));
        int blob_size = sqlite3_column_bytes(stmt, 0);
        auto indices = deserialize_indices(blob, blob_size);
        combined_indices.insert(combined_indices.end(), indices.begin(), indices.end());
    }
    sqlite3_finalize(stmt);

    // Если был запрошен конкретный номер, выбираем его из общего результата
    if (seq_num.has_value() && !combined_indices.empty()) {
        int idx = *seq_num;
        if (idx >= 0 && idx < (int)combined_indices.size()) {
            return { combined_indices[idx] };
        } else {
            return {}; // Индекс за пределами диапазона
        }
    }

    return combined_indices;
}

std::vector<int> TraceMap::get_unique_values(const std::string& key) const {
    int key_idx = find_key_index(key);
    
    std::stringstream sql;
    sql << "SELECT DISTINCT \"" << keys_[key_idx] << "\" FROM trace_map ORDER BY \"" << keys_[key_idx] << "\";";
    
    sqlite3_stmt* stmt;
    check_db_error(sqlite3_prepare_v2(db_, sql.str().c_str(), -1, &stmt, nullptr), "Prepare unique select");
    
    std::vector<int> unique_values;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        unique_values.push_back(sqlite3_column_int(stmt, 0));
    }
    
    sqlite3_finalize(stmt);
    return unique_values;
}

// --- Приватные хелперы ---

void TraceMap::check_db_error(int error_code, const char* context) const {
    if (error_code != SQLITE_OK) {
        std::string msg = "SQLite error in ";
        msg += context;
        msg += ": ";
        msg += sqlite3_errmsg(db_);
        throw std::runtime_error(msg);
    }
}

int TraceMap::find_key_index(const std::string& key) const {
    auto it = std::find(keys_.begin(), keys_.end(), key);
    if (it == keys_.end()) {
        throw std::invalid_argument("Key not found in TraceMap: " + key);
    }
    return std::distance(keys_.begin(), it);
}

std::vector<char> TraceMap::serialize_indices(const std::vector<int>& indices) {
    std::vector<char> blob(indices.size() * sizeof(int));
    memcpy(blob.data(), indices.data(), blob.size());
    return blob;
}

std::vector<int> TraceMap::deserialize_indices(const char* data, int size) {
    std::vector<int> indices(size / sizeof(int));
    memcpy(indices.data(), data, size);
    return indices;
}