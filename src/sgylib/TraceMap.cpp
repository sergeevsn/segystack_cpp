#include "sgylib/TraceMap.hpp"
#include "sgylib/SegyReader.hpp"
#include "sgylib/TraceFieldMap.hpp"
#include "sgylib/SegyUtil.hpp"
#include <stdexcept>
#include <algorithm>
#include <iostream>

TraceMap::TraceMap(const std::string& name, const std::vector<std::string>& keys)
    : name_(name), keys_(keys) {}

void TraceMap::build_map(const SegyReader& reader) {
    int n = reader.num_traces();
    std::cout << "Building trace map for " << name_ << " with " << n << " traces..." << std::endl;
    for (int i = 0; i < n; ++i) {
        auto header = reader.get_trace_header(i);
        std::vector<int> key_vals;
        for (const auto& key : keys_) {
            key_vals.push_back(get_trace_field_value(header.data(), key));
        }
        trace_index_map_[key_vals].push_back(i);
        if ((i + 1) % 100 == 0 || i + 1 == n) {
            print_progress_bar(i + 1, n);
        }
    }
}

std::vector<int> TraceMap::find_trace_indices(const SegyReader& /*reader*/,
                                              const std::vector<std::optional<int>>& key_values) const {
    auto it = query_cache_.find(key_values);
    if (it != query_cache_.end()) return it->second;

    std::vector<int> result;
    for (const auto& [key, indices] : trace_index_map_) {
        bool match = true;
        for (size_t i = 0; i < key_values.size(); ++i) {
            if (key_values[i] && *key_values[i] != key[i]) {
                match = false;
                break;
            }
        }
        if (match) result.insert(result.end(), indices.begin(), indices.end());
    }
    query_cache_[key_values] = result;
    return result;
}

std::vector<int> TraceMap::get_unique_values(const std::string& key) const {
    // Найти индекс ключа в keys_
    auto it = std::find(keys_.begin(), keys_.end(), key);
    if (it == keys_.end()) throw std::invalid_argument("Key not found in TraceMap: " + key);
    size_t key_idx = std::distance(keys_.begin(), it);
    std::set<int> unique;
    for (const auto& [key_vec, indices] : trace_index_map_) {
        if (key_idx < key_vec.size()) {
            unique.insert(key_vec[key_idx]);
        }
    }
    return std::vector<int>(unique.begin(), unique.end());
}
