#pragma once
#include <unordered_map>
#include <string>
#include <stdexcept>
#include "SegyUtil.hpp"

inline const std::unordered_map<std::string, FieldInfo> BinFieldOffsets = {
    {"JobID", {1, 4}},
    {"LineNumber", {5, 4}},
    {"ReelNumber", {9, 4}},
    {"DataTracesPerEnsemble", {13, 2}},
    {"AuxTracesPerEnsemble", {15, 2}},
    {"SampleInterval", {17, 2}},
    {"SampleIntervalOriginal", {19, 2}},
    {"SamplesPerTrace", {21, 2}},
    {"SamplesPerTraceOriginal", {23, 2}},
    {"DataSampleFormat", {25, 2}},
    {"EnsembleFold", {27, 2}},
    {"SortingCode", {29, 2}},
    {"VerticalSumCode", {31, 2}},
    {"SweepFrequencyStart", {33, 2}},
    {"SweepFrequencyEnd", {35, 2}},
    {"SweepLength", {37, 2}},
    {"SweepType", {39, 2}},
    {"SweepTraceTaperLengthStart", {41, 2}},
    {"SweepTraceTaperLengthEnd", {43, 2}},
    {"TaperType", {45, 2}},
    {"CorrelatedTraces", {47, 2}},
    {"BinaryGainRecovered", {49, 2}},
    {"AmplitudeRecoveryMethod", {51, 2}},
    {"MeasurementSystem", {53, 2}},
    {"ImpulseSignalPolarity", {55, 2}},
    {"VibratoryPolarityCode", {57, 2}},
    // ... можно добавить остальные поля по необходимости ...
};

// Универсальная функция для чтения любого поля из бинарного заголовка по имени
inline int32_t get_bin_field_value(const uint8_t* buf, const std::string& field_name) {
    auto it = BinFieldOffsets.find(field_name);
    if (it == BinFieldOffsets.end()) {
        throw std::invalid_argument("Unknown binary header field: " + field_name);
    }
    const FieldInfo& info = it->second;
    if (info.size == 2) {
        return static_cast<int32_t>(get_i16_be(buf, info.offset)); // sign-extend
    } else if (info.size == 4) {
        return get_i32_be(buf, info.offset);
    } else {
        throw std::runtime_error("Unsupported field size for binary header: " + std::to_string(info.size));
    }
} 