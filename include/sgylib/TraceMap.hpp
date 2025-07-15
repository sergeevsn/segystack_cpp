#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <optional>

class SegyReader;

struct VectorHash {
    std::size_t operator()(const std::vector<int>& v) const {
        std::size_t seed = v.size();
        for (int i : v) {
            seed ^= std::hash<int>{}(i) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

struct OptionalVectorHash {
    std::size_t operator()(const std::vector<std::optional<int>>& v) const {
        std::size_t seed = v.size();
        for (const auto& opt : v) {
            seed ^= std::hash<int>{}(opt.value_or(-99999999)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

class TraceMap {
public:
    TraceMap() = default;
    TraceMap(const std::string& name, const std::vector<std::string>& keys);

    void build_map(const SegyReader& reader);
    std::vector<int> find_trace_indices(const SegyReader& reader, const std::vector<std::optional<int>>& key_values) const;
    std::vector<int> get_unique_values(const std::string& key) const;
    const std::string& name() const { return name_; }

private:
    std::string name_;
    std::vector<std::string> keys_;
    std::unordered_map<std::vector<int>, std::vector<int>, VectorHash> trace_index_map_;
    mutable std::unordered_map<std::vector<std::optional<int>>, std::vector<int>, OptionalVectorHash> query_cache_;
};
