#include "sgylib/SegyWriter.hpp"
#include "sgylib/SegyUtil.hpp"
#include <stdexcept>
#include <cstring>
#include "sgylib/BinFieldMap.hpp"

SegyWriter::SegyWriter(const std::string& filename, const SegyReader& reader)
    : filename_(filename),
      text_header_(reader.text_header()),
      bin_header_(reader.bin_header()),
      num_samples_(reader.num_samples()),
      sample_interval_(reader.sample_interval())
{
    num_traces_ = 0;
    open_file();
    write_headers();
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
    num_traces_ = 0;
    open_file();
    write_headers();
}

SegyWriter::~SegyWriter() {
    update_bin_header_num_traces();
    close_file();
}

void SegyWriter::open_file() {
    file_.open(filename_, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!file_) throw std::runtime_error("Failed to open file for writing: " + filename_);
}

void SegyWriter::close_file() {
    if (file_.is_open()) file_.close();
}

void SegyWriter::write_headers() {
    // Write text header (3200 bytes)
    file_.write(reinterpret_cast<const char*>(text_header_.data()), text_header_.size());
    // Write binary header (400 bytes)
    file_.write(reinterpret_cast<const char*>(bin_header_.data()), bin_header_.size());
}

void SegyWriter::update_bin_header_num_traces() {
    // Update the number of traces in the binary header ("DataTracesPerEnsemble" or similar field)
    // For classic SEGY, this is not always strictly required, but we can update "DataTracesPerEnsemble" or another suitable field if present
    // Here, we update "DataTracesPerEnsemble" if it exists
    auto it = BinFieldOffsets.find("DataTracesPerEnsemble");
    if (it != BinFieldOffsets.end()) {
        const FieldInfo& info = it->second;
        if (info.size == 2) {
            set_i16_be(bin_header_.data(), info.offset, static_cast<int16_t>(num_traces_));
        } else if (info.size == 4) {
            set_i32_be(bin_header_.data(), info.offset, num_traces_);
        }
        // Rewrite the binary header at offset 3200
        if (file_.is_open()) {
            file_.seekp(3200, std::ios::beg);
            file_.write(reinterpret_cast<const char*>(bin_header_.data()), bin_header_.size());
        }
    }
}

void SegyWriter::write_trace(const std::vector<uint8_t>& header, const std::vector<float>& samples) {
    write_trace_internal(header, samples);
}

void SegyWriter::write_gather(const std::vector<std::vector<uint8_t>>& headers, const std::vector<std::vector<float>>& traces) {
    if (headers.size() != traces.size()) throw std::invalid_argument("Headers and traces size mismatch");
    for (size_t i = 0; i < headers.size(); ++i) {
        write_trace_internal(headers[i], traces[i]);
    }
}

void SegyWriter::write_trace_internal(const std::vector<uint8_t>& header, const std::vector<float>& samples) {
    if (header.size() != 240) throw std::invalid_argument("Trace header must be 240 bytes");
    if (samples.size() != static_cast<size_t>(num_samples_)) throw std::invalid_argument("Trace samples size mismatch");
    // Write header
    file_.write(reinterpret_cast<const char*>(header.data()), header.size());
    // Write samples as IBM floats
    std::vector<uint8_t> ibm_buf(4 * num_samples_);
    for (int i = 0; i < num_samples_; ++i) {
        uint32_t ibm = ieee_to_ibm(samples[i]);
        ibm_buf[4 * i + 0] = static_cast<uint8_t>((ibm >> 24) & 0xFF);
        ibm_buf[4 * i + 1] = static_cast<uint8_t>((ibm >> 16) & 0xFF);
        ibm_buf[4 * i + 2] = static_cast<uint8_t>((ibm >> 8) & 0xFF);
        ibm_buf[4 * i + 3] = static_cast<uint8_t>(ibm & 0xFF);
    }
    file_.write(reinterpret_cast<const char*>(ibm_buf.data()), ibm_buf.size());
    ++num_traces_;
} 
