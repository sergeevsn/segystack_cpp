#include "sgylib/SegyReader.hpp"
#include "sgylib/SegyUtil.hpp"
#include "sgylib/BinFieldMap.hpp"
#include "sgylib/TraceFieldMap.hpp"
#include <algorithm>
#include <stdexcept>

SegyReader::SegyReader(const std::string& filename, const TraceMap& map,
                       const std::vector<std::string>& stat_keys, const std::string& mode)
    : filename_(filename), mode_(mode), stat_keys_(stat_keys) {
    std::ios_base::openmode openmode = std::ios::binary;
    if (mode == "r") openmode |= std::ios::in;
    else if (mode == "r+") openmode |= std::ios::in | std::ios::out;
    else throw std::invalid_argument("Unknown mode");

    file_.open(filename, openmode);
    if (!file_) throw std::runtime_error("Cannot open SEG-Y file");

    text_header_.resize(3200);
    file_.read(text_header_.data(), 3200);

    bin_header_.resize(400);
    file_.read(reinterpret_cast<char*>(bin_header_.data()), 400);

    num_samples_ = get_bin_field_value(bin_header_.data(), "SamplesPerTrace");
    sample_interval_ = get_bin_field_value(bin_header_.data(), "SampleInterval");

    file_.seekg(0, std::ios::end);
    std::streamoff file_size = file_.tellg();
    trace_bsize_ = 240 + num_samples_ * 4;
    num_traces_ = (file_size - data_offset()) / trace_bsize_;

    file_.seekg(data_offset(), std::ios::beg);

    tracemaps_[map.name()] = map;
    tracemaps_[map.name()].build_map(*this);
}

SegyReader::~SegyReader() {
    if (file_.is_open()) file_.close();
}

std::vector<float> SegyReader::get_trace(int index) const {
    std::vector<float> trace(num_samples_);
    std::streamoff offset = trace_data_offset(index);
    file_.seekg(offset, std::ios::beg);
    std::vector<uint8_t> buf(num_samples_ * 4);
    file_.read(reinterpret_cast<char*>(buf.data()), buf.size());
    for (int i = 0; i < num_samples_; ++i) {
        uint32_t ibm = get_u32_be(&buf[i * 4]);
        trace[i] = ibm_to_float(ibm);
    }
    return trace;
}

std::vector<uint8_t> SegyReader::get_trace_header(int index) const {
    std::vector<uint8_t> header(240);
    std::streamoff offset = trace_offset(index);
    file_.seekg(offset, std::ios::beg);
    file_.read(reinterpret_cast<char*>(header.data()), 240);
    return header;
}

int32_t SegyReader::get_header_value_i32(int trace_index, const std::string& key) const {
    return get_header_value_i32(get_trace_header(trace_index), key);
}

int32_t SegyReader::get_header_value_i32(const std::vector<uint8_t>& trace_header, const std::string& key) const {
    return get_trace_field_value(trace_header.data(), key);
}

int16_t SegyReader::get_header_value_i16(const std::vector<uint8_t>& trace_header, const std::string& key) const {
    auto it = TraceFieldOffsets.find(key);
    if (it == TraceFieldOffsets.end()) throw std::invalid_argument("Invalid key");
    return get_i16_be(trace_header.data(), it->second.offset);
}

int32_t SegyReader::get_bin_header_value_i32(const std::string& key) const {
    return get_bin_field_value(bin_header_.data(), key);
}

int16_t SegyReader::get_bin_header_value_i16(const std::string& key) const {
    return get_i16_be(bin_header_.data(), BinFieldOffsets.at(key).offset);
}

std::vector<std::vector<float>> SegyReader::get_gather(const std::string& tracemap_name,
                                                       const std::vector<std::optional<int>>& keys) const {
    std::vector<std::vector<uint8_t>> headers;
    std::vector<std::vector<float>> traces;
    get_gather_and_headers(tracemap_name, keys, headers, traces);
    return traces;
}

std::vector<std::vector<uint8_t>> SegyReader::get_gather_headers(const std::string& tracemap_name,
                                                                 const std::vector<std::optional<int>>& keys) const {
    std::vector<std::vector<uint8_t>> headers;
    std::vector<std::vector<float>> traces;
    get_gather_and_headers(tracemap_name, keys, headers, traces);
    return headers;
}

void SegyReader::get_gather_and_headers(const std::string& tracemap_name,
                                        const std::vector<std::optional<int>>& keys,
                                        std::vector<std::vector<uint8_t>>& headers,
                                        std::vector<std::vector<float>>& traces) const {
    auto it = tracemaps_.find(tracemap_name);
    if (it == tracemaps_.end()) throw std::invalid_argument("Invalid map name");
    auto indices = it->second.find_trace_indices(*this, keys);
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
        headers[i] = std::vector<uint8_t>(buf.begin(), buf.begin() + 240);

        std::vector<float> trace(num_samples_);
        for (int j = 0; j < num_samples_; ++j) {
            uint32_t ibm = get_u32_be(&buf[240 + j * 4]);
            trace[j] = ibm_to_float(ibm);
        }
        traces[i] = std::move(trace);
    }
}

int SegyReader::num_traces() const { return num_traces_; }
int SegyReader::num_samples() const { return num_samples_; }
float SegyReader::sample_interval() const { return sample_interval_; }
