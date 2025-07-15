#include <algorithm>
#include <vector>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <string>
#include <omp.h>
#include "nmo/nmo.hpp"

std::vector<std::vector<float>> 
nmo_correction(const std::vector<std::vector<float>>& cdp_gather,
               const std::vector<float>& offsets,
               const std::vector<float>& velocities,
               float dt,
               float stretch_mute_percent
               ) {

    int n_traces = static_cast<int>(cdp_gather.size());
    if (n_traces == 0) return {};

    int n_time_samples = static_cast<int>(cdp_gather[0].size());
    std::vector<std::vector<float>> nmo_corrected_gather(n_traces, std::vector<float>(n_time_samples, 0.0f));

    // Предрасчёт sinc-функции
    constexpr int SINC_HALF_WINDOW = 4;
    constexpr int SINC_WINDOW_SIZE = 2 * SINC_HALF_WINDOW + 1;
    std::vector<float> sinc_weights(SINC_WINDOW_SIZE);
    for (int k = 0; k < SINC_WINDOW_SIZE; ++k) {
        float x = static_cast<float>(k - SINC_HALF_WINDOW);
        sinc_weights[k] = (x == 0.0f) ? 1.0f : std::sin(M_PI * x) / (M_PI * x);
    }

    // Основной цикл — параллелим по трассам
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < n_traces; ++i) {
        const auto& trace = cdp_gather[i];
        auto& corrected_trace = nmo_corrected_gather[i];
        float offset = offsets[i];

        for (int j = 0; j < n_time_samples; ++j) {
            float time = j * dt;
            float velocity = velocities[j];
            if (velocity == 0.0f) velocity = 1e-12f;

            float tnmo = std::sqrt(time * time + (offset * offset) / (velocity * velocity));
            int tnmo_sample = static_cast<int>(std::round(tnmo / dt));

            if (tnmo_sample >= n_time_samples) {
                std::fill(corrected_trace.begin() + j, corrected_trace.end(), 0.0f);
                break;
            }

            float stretch_factor = (tnmo > 0.0f) ? (1.0f - time / tnmo) * 100.0f : 0.0f;
            if (stretch_factor > stretch_mute_percent) {
                corrected_trace[j] = 0.0f;
                continue;
            }

            int start_idx = tnmo_sample - SINC_HALF_WINDOW;
            int end_idx = tnmo_sample + SINC_HALF_WINDOW;

            if (start_idx < 0 || end_idx >= n_time_samples) {
                corrected_trace[j] = trace[std::clamp(tnmo_sample, 0, n_time_samples - 1)];
            } else {
                float interpolated_value = 0.0f;

                #pragma omp simd reduction(+:interpolated_value)
                for (size_t k = 0; k < sinc_weights.size(); ++k) {
                    interpolated_value += trace[start_idx + k] * sinc_weights[k];
                }

                corrected_trace[j] = interpolated_value;
            }
        }
    }

    return nmo_corrected_gather;
}

