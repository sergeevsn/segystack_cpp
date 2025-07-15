#include "sgylib/SegyReader.hpp"
#include "sgylib/SegyWriter.hpp"
#include "sgylib/TraceMap.hpp"
#include "nmo/nmo.hpp"
#include "Config.hpp"
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <cmath>
#include <iomanip>
#include <string>
#include <omp.h>
#include "sgylib/SegyUtil.hpp"
#include <chrono>

// ------------------------ Типы ----------------------------
using VelTable = std::map<int, std::vector<std::pair<float, float>>>;

// -------------------- Считывание таблицы ------------------
VelTable read_velocity_table(const std::string& path) {
    VelTable table;
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open velocity table: " + path);

    std::string line;
    bool header_skipped = false;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // Пропускаем строку-заголовок, если ещё не пропущена
        if (!header_skipped && (line.find("CDP") != std::string::npos || line.find("cdp") != std::string::npos)) {
            header_skipped = true;
            continue;
        }

        std::istringstream iss(line);
        int cdp;
        float time_ms, vel;
        if (iss >> cdp >> time_ms >> vel) {
            table[cdp].emplace_back(time_ms * 1e-3f, vel);  // Преобразуем в секунды
        } else {
            std::cerr << "Warning: failed to parse line: " << line << "\n";
        }
    }

    return table;
}


// --------- Интерполяция скоростей на временной оси -------
std::map<int, std::vector<float>> interpolate_velocity_cube(
    const VelTable& table,
    const std::vector<int>& cdps,
    int n, float dt)
{
    std::map<int, std::vector<float>> result;

    for (int cdp : cdps) {
        // Поиск ближайших CDP
        auto it = table.lower_bound(cdp);
        VelTable::const_iterator use;
        if (it == table.end()) {
            use = std::prev(it);
        } else if (it == table.begin()) {
            use = it;
        } else {
            auto prev_it = std::prev(it);
            use = (cdp - prev_it->first < it->first - cdp) ? prev_it : it;
        }

        const auto& pairs = use->second;
        std::vector<float> v(n);
        for (int i = 0; i < n; ++i) {
            float t = i * dt;

            auto upper = std::lower_bound(
                pairs.begin(), pairs.end(), std::make_pair(t, 0.0f),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            if (upper == pairs.end()) {
                v[i] = pairs.back().second;
            } else if (upper == pairs.begin()) {
                v[i] = pairs.front().second;
            } else {
                auto lower = std::prev(upper);
                float t1 = lower->first, v1 = lower->second;
                float t2 = upper->first, v2 = upper->second;
                float alpha = (t - t1) / (t2 - t1 + 1e-6f);
                v[i] = v1 + alpha * (v2 - v1);
            }
        }
        result[cdp] = std::move(v);
    }

    return result;
}

// ------------------- Суммирование -------------------------
std::vector<float> stack_traces(const std::vector<std::vector<float>>& traces) {
    if (traces.empty()) return {};
    int n = traces[0].size();
    int m = traces.size();
    std::vector<float> out(n, 0.0f);
    float inv_m = 1.0f / m;

    #pragma omp parallel for
    for (int i = 0; i < n; ++i) {
        float sum = 0.0f;
        for (int j = 0; j < m; ++j) {
            sum += traces[j][i];
        }
        out[i] = sum * inv_m;
    }

    return out;
}

// ------------------------ main ----------------------------
int main(int argc, char** argv) {
    auto start_time = std::chrono::high_resolution_clock::now();
    if (argc < 2) {
        std::cerr << "Error: Configuration file path not provided." << std::endl;
        std::cerr << "Usage: " << argv[0] << " <file_path>" << std::endl;
        return 1; // Return error code
    }
    Config cfg = load_config(argv[1]);
   
    std::cout << "Input: " << cfg.input_file << std::endl;
    std::cout << "Output: " << cfg.output_file << std::endl;
    std::cout << "Velocity: " << cfg.velocity_file << std::endl;
    std::cout << "NMO Stretch Muting Percent: " << cfg.nmo_stretch_muting_percent << std::endl;

    // Проверка наличия входного файла
    std::ifstream test_input(cfg.input_file);
    if (!test_input) {
        std::cerr << "Error: Cannot open input SEG-Y file: " << cfg.input_file << std::endl;
        return 2;
    }
    test_input.close();

    // Проверка наличия файла скоростей
    std::ifstream test_vel(cfg.velocity_file);
    if (!test_vel) {
        std::cerr << "Error: Cannot open velocity file: " << cfg.velocity_file << std::endl;
        return 3;
    }
    test_vel.close();

    // Проверка возможности создания выходного файла
    std::ofstream test_out(cfg.output_file, std::ios::binary | std::ios::trunc);
    if (!test_out) {
        std::cerr << "Error: Cannot create output SEG-Y file: " << cfg.output_file << std::endl;
        return 4;
    }
    test_out.close();


    TraceMap cdp_offset_map("cdp_offset", {"CDP", "offset"});   
    SegyReader input_reader(cfg.input_file, cdp_offset_map, {"offset", "CDP"});

    int num_samples = input_reader.num_samples();
    float dt = input_reader.sample_interval() * 1e-6f;

    auto cdp_values = input_reader.tracemap("cdp_offset").get_unique_values("CDP");
    int num_cdps = cdp_values.size();

    std::map<int, std::vector<float>> cdp_velocities;

    if (cfg.velocity_file.ends_with(".sgy") || cfg.velocity_file.ends_with(".segy")) {
        TraceMap cdp_map("cdp", {"CDP"});  
        
        std::cout << "Reading velocity SEG-Y file " << cfg.velocity_file << "..." << std::endl;
        SegyReader vel_reader(cfg.velocity_file, cdp_map);

        for (int cdp : cdp_values) {
            auto g = vel_reader.get_gather("cdp", {cdp});
            if (!g.empty()) {
                std::vector<float> v(g[0].begin(), g[0].end());
                cdp_velocities[cdp] = std::move(v);
            }
        }

        // Интерполяция/экстраполяция отсутствующих CDP
        VelTable table;
        for (const auto& [cdp, vec] : cdp_velocities) {
            table[cdp].clear();
            for (size_t i = 0; i < vec.size(); ++i)
                table[cdp].emplace_back(i * dt, vec[i]);
        }
        cdp_velocities = interpolate_velocity_cube(table, cdp_values, num_samples, dt);

    } else {
        std::cout << "Reading velocity table file " << cfg.velocity_file << "..." <<std::endl;
        VelTable table = read_velocity_table(cfg.velocity_file);
        std::cout << "Interpolating velocity..." << std::endl;
        cdp_velocities = interpolate_velocity_cube(table, cdp_values, num_samples, dt);
    }

    SegyWriter writer(cfg.output_file, input_reader);

    std::cout << "Processing (NMO + stacking) " << num_cdps << " CDPs..." << std::endl;

    int processed = 0;
    for (int cdp : cdp_values) {
        ++processed;
        if ((processed % 50 == 0) || (processed == num_cdps)) {
            print_progress_bar(processed, num_cdps);
        }

        std::vector<std::vector<uint8_t>> headers;
        std::vector<std::vector<float>> traces;
        input_reader.get_gather_and_headers("cdp_offset", {cdp, std::nullopt}, headers, traces);

        if (traces.empty() || !cdp_velocities.count(cdp)) continue;

        std::vector<float> offsets;
        for (const auto& h : headers)
            offsets.push_back(static_cast<float>(input_reader.get_header_value_i32(h, "offset")));

        auto corrected = nmo_correction(traces, offsets, cdp_velocities[cdp], dt, cfg.nmo_stretch_muting_percent);
        auto stacked = stack_traces(corrected);

        writer.write_trace(headers.front(), stacked);
    }

    std::cout << "\nStacked output written to: " << cfg.output_file << "\n";
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << "Total processing time: " << elapsed.count() << " seconds.\n";
    return 0;
}

