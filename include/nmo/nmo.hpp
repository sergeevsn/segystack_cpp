#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

std::vector<std::vector<float>> nmo_correction(
    const std::vector<std::vector<float>>& cdp_gather,
    const std::vector<float>& offsets,
    const std::vector<float>& velocities,
    float dt,
    float stretch_mute_percent);
    
