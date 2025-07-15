#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include "SegyReader.hpp"

class SegyWriter {
public:
    // Construct from SegyReader (copies headers and metadata)
    SegyWriter(const std::string& filename, const SegyReader& reader);
    // Construct from explicit metadata
    SegyWriter(const std::string& filename,
               const std::vector<char>& text_header,
               const std::vector<uint8_t>& bin_header,
               int num_samples,
               float sample_interval);
    ~SegyWriter();

    // Write a single trace (header + samples)
    void write_trace(const std::vector<uint8_t>& header, const std::vector<float>& samples);
    // Write a gather (multiple headers + traces)
    void write_gather(const std::vector<std::vector<uint8_t>>& headers, const std::vector<std::vector<float>>& traces);

    // Get current number of traces written
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
    void update_bin_header_num_traces();
    void write_headers();
    void open_file();
    void close_file();
    void write_trace_internal(const std::vector<uint8_t>& header, const std::vector<float>& samples);
}; 