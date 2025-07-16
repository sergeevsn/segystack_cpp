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
#include "util.hpp"
#include <filesystem>

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
// ======================== ГЛАВНАЯ ЛОГИКА ============================
int main(int argc, char** argv) {
    auto start_time = std::chrono::high_resolution_clock::now();
    if (argc < 2) {
        std::cerr << "Error: Configuration file path not provided.\nUsage: " << argv[0] << " <path_to_config>\n";
        return 1;
    }
    Config cfg = load_config(argv[1]);
   
    std::cout << "Input:    " << cfg.input_file << std::endl;
    std::cout << "Output:   " << cfg.output_file << std::endl;
    std::cout << "Velocity: " << cfg.velocity_file << std::endl;
    
    // --- ИЗМЕНЕНИЕ: Упрощенная проверка файлов, т.к. конструкторы Segy* сами вызовут ошибку ---
    if (!std::filesystem::exists(cfg.input_file)) {
        std::cerr << "Error: Cannot find input SEG-Y file: " << cfg.input_file << std::endl;
        return 2;
    }
    if (!std::filesystem::exists(cfg.velocity_file)) {
        std::cerr << "Error: Cannot find velocity file: " << cfg.velocity_file << std::endl;
        return 3;
    }
    
    // --- ИЗМЕНЕНИЕ: Новый, явный подход к созданию/загрузке карты трасс ---
    const std::string main_db_path = cfg.input_file + ".cdp_offset.sqlite";
    const std::string main_map_name = "cdp_offset_map";
    const std::vector<std::string> main_map_keys = {"CDP", "offset"};

    if (!std::filesystem::exists(main_db_path)) {
        std::cout << "\nTrace map for input file not found. Building new one..." << std::endl;
        SegyReader temp_reader(cfg.input_file);
        temp_reader.build_tracemap(main_map_name, main_db_path, main_map_keys);
    } else {
        std::cout << "\nFound existing trace map for input file." << std::endl;
    }

    // --- Основная часть программы ---
    // Создаем ридер и ЗАГРУЖАЕМ в него уже готовую карту
    SegyReader input_reader(cfg.input_file);
    input_reader.load_tracemap(main_map_name, main_db_path, main_map_keys);
    
    int num_samples = input_reader.num_samples();
    float dt = input_reader.sample_interval() * 1e-6f;

    // Получаем уникальные CDP, используя новый API
    auto tmap = input_reader.get_tracemap(main_map_name);
    auto cdp_values = tmap->get_unique_values("CDP");
    int num_cdps = cdp_values.size();
    
    std::cout << "Found " << num_cdps << " unique CDPs to process." << std::endl;

    // --- Считывание и интерполяция скоростей ---
    std::map<int, std::vector<float>> cdp_velocities;

    if (cfg.velocity_file.ends_with(".sgy") || cfg.velocity_file.ends_with(".segy")) {
        std::cout << "Reading velocities from SEG-Y file..." << std::endl;
        
        // ИЗМЕНЕНИЕ: Тот же "умный" подход для файла скоростей
        const std::string vel_db_path = cfg.velocity_file + ".cdp.sqlite";
        const std::string vel_map_name = "cdp_map";
        const std::vector<std::string> vel_map_keys = {"CDP"};

        if (!std::filesystem::exists(vel_db_path)) {
            std::cout << "Trace map for velocity file not found. Building new one..." << std::endl;
            SegyReader temp_vel_reader(cfg.velocity_file);
            temp_vel_reader.build_tracemap(vel_map_name, vel_db_path, vel_map_keys);
        }
        
        // Создаем ридер для файла скоростей и загружаем его карту
        SegyReader vel_reader(cfg.velocity_file);
        vel_reader.load_tracemap(vel_map_name, vel_db_path, vel_map_keys);

        for (int cdp : cdp_values) {
            auto g = vel_reader.get_gather(vel_map_name, {cdp});
            if (!g.empty()) {
                cdp_velocities[cdp] = g[0];
            }
        }
        
        // Интерполяция/экстраполяция (без изменений)
        VelTable table;
        for (const auto& [cdp, vec] : cdp_velocities) {
            table[cdp].clear();
            for (size_t i = 0; i < vec.size(); ++i)
                table[cdp].emplace_back(i * dt, vec[i]);
        }
        cdp_velocities = interpolate_velocity_cube(table, cdp_values, num_samples, dt);

    } else {
        std::cout << "Reading velocities from table file..." <<std::endl;
        VelTable table = read_velocity_table(cfg.velocity_file);
        std::cout << "Interpolating velocity..." << std::endl;
        cdp_velocities = interpolate_velocity_cube(table, cdp_values, num_samples, dt);
    }

    // --- Основной цикл обработки и записи ---
    SegyWriter writer(cfg.output_file, input_reader);
    std::cout << "\nStarting NMO correction and stacking..." << std::endl;

    int processed = 0;
    for (int cdp : cdp_values) {
        print_progress_bar("Processing CDPs", ++processed, num_cdps);

        std::vector<std::vector<uint8_t>> headers;
        std::vector<std::vector<float>> traces;
        // ИЗМЕНЕНИЕ: get_gather_and_headers теперь не требует TraceMap в качестве аргумента
        input_reader.get_gather_and_headers(main_map_name, {cdp, std::nullopt}, headers, traces);

        if (traces.empty() || cdp_velocities.find(cdp) == cdp_velocities.end()) {
            continue;
        }

        std::vector<float> offsets;
        offsets.reserve(headers.size());
        for (const auto& h : headers) {
            offsets.push_back(static_cast<float>(input_reader.get_header_value_i32(h, "offset")));
        }

        auto corrected = nmo_correction(traces, offsets, cdp_velocities.at(cdp), dt, cfg.nmo_stretch_muting_percent);
        auto stacked = stack_traces(corrected);

        // Используем заголовок первой трассы сейсмосбора как шаблон для суммарной трассы
        writer.write_trace(headers.front(), stacked);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << "\nNMO+Stacking finished. Output written to: " << cfg.output_file << "\n";
    std::cout << "Total processing time: " << std::fixed << std::setprecision(2) << elapsed.count() << " seconds.\n";
    
    return 0;
}