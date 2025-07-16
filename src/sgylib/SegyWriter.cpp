#include "sgylib/SegyWriter.hpp"
#include "sgylib/SegyUtil.hpp"
#include "sgylib/BinFieldMap.hpp" // Для доступа к смещениям в бинарном заголовке
#include <stdexcept>
#include <vector>

// --- Приватный метод для инициализации ---
void SegyWriter::init() {
    this->num_traces_ = 0;
    this->trace_bsize_ = 240 + this->num_samples_ * 4;
    
    file_.open(filename_, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!file_) {
        throw std::runtime_error("Failed to open file for writing: " + filename_);
    }
    
    // Записываем начальные заголовки
    file_.write(text_header_.data(), text_header_.size());
    file_.write(reinterpret_cast<const char*>(bin_header_.data()), bin_header_.size());
}

// --- Конструкторы ---

SegyWriter::SegyWriter(const std::string& filename, const SegyReader& reader)
    : filename_(filename),
      text_header_(reader.text_header()),
      bin_header_(reader.bin_header()),
      num_samples_(reader.num_samples()),
      sample_interval_(reader.sample_interval())
{
    init();
}

SegyWriter::SegyWriter(const std::string& filename,
                       const std::vector<char>& text_header,
                       const std::vector<uint8_t>& bin_header,
                       int num_samples,
                       float sample_interval)
    : filename_(filename),
      text_header_(text_header),
      bin_header_(bin_header),
      num_samples_(num_samples),
      sample_interval_(sample_interval)
{
    if (text_header.size() != 3200) throw std::invalid_argument("Text header must be 3200 bytes.");
    if (bin_header.size() != 400) throw std::invalid_argument("Binary header must be 400 bytes.");
    init();
}

// --- Деструктор и финализация ---

SegyWriter::~SegyWriter() {
    finalize_file();
}

void SegyWriter::finalize_file() {
    if (!file_.is_open()) {
        return;
    }

    // Обновляем количество трасс в бинарном заголовке.
    // Стандарт SEG-Y Rev 1 (поле "Number of data traces per ensemble")
    auto it = BinFieldOffsets.find("DataTracesPerEnsemble");
    if (it != BinFieldOffsets.end()) {
        set_i16_be(bin_header_.data(), it->second.offset, static_cast<int16_t>(num_traces_));
        
        // Перезаписываем обновленный бинарный заголовок в файле
        file_.seekp(3200, std::ios::beg);
        file_.write(reinterpret_cast<const char*>(bin_header_.data()), bin_header_.size());
    }
    
    file_.close();
}

// --- Методы для записи ---

void SegyWriter::write_trace(const std::vector<uint8_t>& header, const std::vector<float>& samples) {
    if (header.size() != 240) {
        throw std::invalid_argument("Trace header must be 240 bytes.");
    }
    if (samples.size() != static_cast<size_t>(num_samples_)) {
        throw std::invalid_argument("Trace samples size mismatch.");
    }

    // Записываем заголовок
    file_.write(reinterpret_cast<const char*>(header.data()), 240);
    
    // Конвертируем и записываем отсчеты, используя утилиты
    std::vector<uint8_t> sample_bytes(samples.size() * 4);
    for (size_t i = 0; i < samples.size(); ++i) {
        uint32_t ibm = ieee_to_ibm(samples[i]);
        // ИСПОЛЬЗУЕМ НОВУЮ УТИЛИТУ
        put_u32_be(&sample_bytes[i * 4], ibm);
    }
    file_.write(reinterpret_cast<const char*>(sample_bytes.data()), sample_bytes.size());

    num_traces_++;
}


void SegyWriter::write_gather(const std::vector<std::vector<uint8_t>>& headers, const std::vector<std::vector<float>>& traces) {
    if (headers.size() != traces.size()) {
        throw std::invalid_argument("Headers and traces count mismatch in gather.");
    }
    if (headers.empty()) {
        return; // Нечего записывать
    }

    // ОПТИМИЗАЦИЯ: Создаем один большой буфер для всего сейсмосбора.
    std::vector<uint8_t> gather_buffer;
    gather_buffer.reserve(headers.size() * trace_bsize_);

    for (size_t i = 0; i < headers.size(); ++i) {
        const auto& header = headers[i];
        const auto& trace_samples = traces[i];
        
        if (header.size() != 240 || trace_samples.size() != static_cast<size_t>(num_samples_)) {
            throw std::invalid_argument("Invalid header or samples size in gather at index " + std::to_string(i));
        }

        // Добавляем заголовок в буфер
        gather_buffer.insert(gather_buffer.end(), header.begin(), header.end());
        
        // Конвертируем и добавляем отсчеты в буфер
        size_t current_size = gather_buffer.size();
        gather_buffer.resize(current_size + trace_samples.size() * 4);
        for (size_t j = 0; j < trace_samples.size(); ++j) {
            uint32_t ibm = ieee_to_ibm(trace_samples[j]);
            // ИСПОЛЬЗУЕМ НОВУЮ УТИЛИТУ
            put_u32_be(&gather_buffer[current_size + j * 4], ibm);
        }
    }

    // Записываем весь буфер за один системный вызов
    file_.write(reinterpret_cast<const char*>(gather_buffer.data()), gather_buffer.size());
    
    num_traces_ += headers.size();
}

// УДАЛЕНО: write_gather_block и write_trace_internal.
// Их функциональность теперь чисто и эффективно реализована в write_gather и write_trace.
// Если вам все еще нужен write_gather_block, он должен просто вызывать write_gather.
void SegyWriter::write_gather_block(const std::vector<std::vector<uint8_t>>& headers, const std::vector<std::vector<float>>& traces) {
    // Этот метод теперь является просто псевдонимом для write_gather.
    write_gather(headers, traces);
}