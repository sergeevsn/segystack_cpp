#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <fstream>
#include <unordered_map>
#include <set>
#include <optional>
#include "TraceMap.hpp"

class SegyReader {
public:
    SegyReader(const std::string& filename, const TraceMap& map, const std::vector<std::string>& stat_keys = {}, const std::string& mode = "r");
    ~SegyReader();

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

    void add_tracemap(const TraceMap& map);

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

    const TraceMap& tracemap(const std::string& name) const {
        auto it = tracemaps_.find(name);
        if (it == tracemaps_.end()) throw std::invalid_argument("No such TraceMap: " + name);
        return it->second;
    }

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
    std::unordered_map<std::string, TraceMap> tracemaps_;
    std::vector<std::string> stat_keys_;
    std::unordered_map<std::string, std::set<int>> unique_stats_;

    inline std::streamoff data_offset() const { return 3200 + 400; }
    inline std::streamoff trace_offset(int index) const { return data_offset() + static_cast<std::streamoff>(index) * trace_bsize_; }
    inline std::streamoff trace_data_offset(int index) const { return trace_offset(index) + 240; }

    void read_gather_block(const std::vector<int>& indices,
                           std::vector<std::vector<uint8_t>>& headers,
                           std::vector<std::vector<float>>& traces) const;
};
