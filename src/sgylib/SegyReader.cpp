#include "sgylib/SegyReader.hpp"
#include "sgylib/SegyUtil.hpp"
#include "sgylib/BinFieldMap.hpp"
#include "sgylib/TraceFieldMap.hpp"
#include <algorithm>
#include <stdexcept>

// Основной конструктор, инициализирует чтение SEG-Y файла
SegyReader::SegyReader(const std::string& filename, const std::string& mode)
    : filename_(filename), mode_(mode) {
    std::ios_base::openmode openmode = std::ios::binary;
    if (mode == "r") {
        openmode |= std::ios::in;
    } else if (mode == "r+") {
        openmode |= std::ios::in | std::ios::out;
    } else {
        throw std::invalid_argument("Unknown mode for SegyReader: " + mode);
    }

    file_.open(filename, openmode);
    if (!file_) {
        throw std::runtime_error("Cannot open SEG-Y file: " + filename);
    }

    text_header_.resize(3200);
    file_.read(text_header_.data(), 3200);

    bin_header_.resize(400);
    file_.read(reinterpret_cast<char*>(bin_header_.data()), 400);

    num_samples_ = get_bin_field_value(bin_header_.data(), "SamplesPerTrace");
    sample_interval_ = get_bin_field_value(bin_header_.data(), "SampleInterval");
    
    // Проверка на корректность прочитанных данных
    if (num_samples_ <= 0) {
        throw std::runtime_error("Invalid number of samples per trace in binary header: " + std::to_string(num_samples_));
    }

    file_.seekg(0, std::ios::end);
    std::streamoff file_size = file_.tellg();
    trace_bsize_ = 240 + num_samples_ * 4;
    num_traces_ = (file_size - data_offset()) / trace_bsize_;

    file_.seekg(data_offset(), std::ios::beg);
}

SegyReader::~SegyReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

// --- Реализация новых методов управления TraceMap ---

void SegyReader::build_tracemap(const std::string& map_name, const std::string& db_path, const std::vector<std::string>& keys) {
    // Создаем объект TraceMap в динамической памяти и оборачиваем в shared_ptr
    auto map_ptr = std::make_shared<TraceMap>(db_path, keys);
    
    // Вызываем его метод для построения (это долгая, параллельная операция)
    map_ptr->build_map(*this);
    
    // Сохраняем умный указатель во внутреннем хранилище
    tracemaps_[map_name] = map_ptr;
}

void SegyReader::load_tracemap(const std::string& map_name, const std::string& db_path, const std::vector<std::string>& keys) {
    // Создаем объект, который откроет существующий файл БД, но не будет его перестраивать
    auto map_ptr = std::make_shared<TraceMap>(db_path, keys);
    
    // Сохраняем указатель
    tracemaps_[map_name] = map_ptr;
}

std::shared_ptr<TraceMap> SegyReader::get_tracemap(const std::string& name) const {
    auto it = tracemaps_.find(name);
    if (it == tracemaps_.end()) {
        throw std::runtime_error("TraceMap with name '" + name + "' not found.");
    }
    return it->second;
}

// --- Реализация методов доступа к данным ---

std::vector<float> SegyReader::get_trace(int index) const {
    std::vector<float> trace_data(num_samples_);
    std::streamoff offset = trace_data_offset(index);
    
    // В буфер читаем только данные трассы
    std::vector<uint8_t> buf(num_samples_ * 4);
    file_.seekg(offset, std::ios::beg);
    file_.read(reinterpret_cast<char*>(buf.data()), buf.size());

    for (int i = 0; i < num_samples_; ++i) {
        uint32_t ibm = get_u32_be(&buf[i * 4]);
        trace_data[i] = ibm_to_float(ibm);
    }
    return trace_data;
}

std::vector<uint8_t> SegyReader::get_trace_header(int index) const {
    std::vector<uint8_t> header(240);
    std::streamoff offset = trace_offset(index);
    file_.seekg(offset, std::ios::beg);
    file_.read(reinterpret_cast<char*>(header.data()), 240);
    return header;
}

std::vector<std::vector<float>> SegyReader::get_gather(const std::string& tracemap_name,
                                                       const std::vector<std::optional<int>>& keys) const {
    std::vector<std::vector<uint8_t>> headers; // Временный пустой вектор
    std::vector<std::vector<float>> traces;
    get_gather_and_headers(tracemap_name, keys, headers, traces);
    return traces;
}

std::vector<std::vector<uint8_t>> SegyReader::get_gather_headers(const std::string& tracemap_name,
                                                                 const std::vector<std::optional<int>>& keys) const {
    std::vector<std::vector<uint8_t>> headers;
    std::vector<std::vector<float>> traces; // Временный пустой вектор
    get_gather_and_headers(tracemap_name, keys, headers, traces);
    return headers;
}

void SegyReader::get_gather_and_headers(const std::string& tracemap_name,
                                        const std::vector<std::optional<int>>& keys,
                                        std::vector<std::vector<uint8_t>>& headers,
                                        std::vector<std::vector<float>>& traces) const {
    // Получаем указатель на нужную карту
    auto map_ptr = get_tracemap(tracemap_name);
    
    // Находим индексы через TraceMap. Этот вызов теперь обращается к SQLite.
    auto indices = map_ptr->find_trace_indices(keys);
    
    // Сортировка важна для более эффективного последовательного чтения с диска.
    std::sort(indices.begin(), indices.end());
    
    read_gather_block(indices, headers, traces);
}

void SegyReader::read_gather_block(const std::vector<int>& indices,
                                   std::vector<std::vector<uint8_t>>& headers,
                                   std::vector<std::vector<float>>& traces) const {
    headers.resize(indices.size());
    traces.resize(indices.size());

    for (size_t i = 0; i < indices.size(); ++i) {
        int idx = indices[i];
        std::streamoff offset = trace_offset(idx);
        file_.seekg(offset, std::ios::beg);

        std::vector<uint8_t> buf(trace_bsize_);
        file_.read(reinterpret_cast<char*>(buf.data()), trace_bsize_);
        
        // Копируем заголовок
        headers[i].assign(buf.begin(), buf.begin() + 240);

        // Конвертируем данные трассы
        traces[i].resize(num_samples_);
        for (int j = 0; j < num_samples_; ++j) {
            uint32_t ibm = get_u32_be(&buf[240 + j * 4]);
            traces[i][j] = ibm_to_float(ibm);
        }
    }
}

// --- Реализация геттеров и вспомогательных методов ---

int SegyReader::num_traces() const { return num_traces_; }
int SegyReader::num_samples() const { return num_samples_; }
float SegyReader::sample_interval() const { return sample_interval_; }

int32_t SegyReader::get_header_value_i32(int trace_index, const std::string& key) const {
    auto header = get_trace_header(trace_index);
    return get_trace_field_value(header.data(), key);
}

int32_t SegyReader::get_header_value_i32(const std::vector<uint8_t>& trace_header, const std::string& key) const {
    return get_trace_field_value(trace_header.data(), key);
}

int16_t SegyReader::get_header_value_i16(const std::vector<uint8_t>& trace_header, const std::string& key) const {
    auto it = TraceFieldOffsets.find(key);
    if (it == TraceFieldOffsets.end()) {
        throw std::invalid_argument("Invalid trace header key: " + key);
    }
    return get_i16_be(trace_header.data(), it->second.offset);
}

int32_t SegyReader::get_bin_header_value_i32(const std::string& key) const {
    return get_bin_field_value(bin_header_.data(), key);
}

int16_t SegyReader::get_bin_header_value_i16(const std::string& key) const {
    auto it = BinFieldOffsets.find(key);
    if (it == BinFieldOffsets.end()) {
        throw std::invalid_argument("Invalid binary header key: " + key);
    }
    return get_i16_be(bin_header_.data(), it->second.offset);
}

void SegyReader::read_raw_block(int start_trace_idx, size_t bytes_to_read, char* buffer) const {
    std::streamoff offset = trace_offset(start_trace_idx);
    file_.seekg(offset, std::ios::beg);
    file_.read(buffer, bytes_to_read);
}