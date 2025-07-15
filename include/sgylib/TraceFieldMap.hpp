// TraceFieldMap.hpp
#pragma once

#include <unordered_map>
#include <string>
#include <stdexcept>
#include "SegyUtil.hpp"

inline const std::unordered_map<std::string, FieldInfo> TraceFieldOffsets = {
    {"TRACE_SEQUENCE_LINE", {1, 4}},
    {"TRACE_SEQUENCE_FILE", {5, 4}},
    {"FieldRecord", {9, 4}},
    {"TraceNumber", {13, 4}},
    {"EnergySourcePoint", {17, 4}},
    {"CDP", {21, 4}},
    {"CDP_TRACE", {25, 4}},
    {"TraceIdentificationCode", {29, 2}},
    {"NSummedTraces", {31, 2}},
    {"NStackedTraces", {33, 2}},
    {"DataUse", {35, 2}},
    {"offset", {37, 4}},
    {"ReceiverGroupElevation", {41, 4}},
    {"SourceSurfaceElevation", {45, 4}},
    {"SourceDepth", {49, 4}},
    {"ReceiverDatumElevation", {53, 4}},
    {"SourceDatumElevation", {57, 4}},
    {"SourceWaterDepth", {61, 4}},
    {"GroupWaterDepth", {65, 4}},
    {"ElevationScalar", {69, 2}},
    {"SourceGroupScalar", {71, 2}},
    {"SourceX", {73, 4}},
    {"SourceY", {77, 4}},
    {"GroupX", {81, 4}},
    {"GroupY", {85, 4}},
    {"CoordinateUnits", {89, 2}},
    {"WeatheringVelocity", {91, 2}},
    {"SubWeatheringVelocity", {93, 2}},
    {"SourceUpholeTime", {95, 2}},
    {"GroupUpholeTime", {97, 2}},
    {"SourceStaticCorrection", {99, 2}},
    {"GroupStaticCorrection", {101, 2}},
    {"TotalStaticApplied", {103, 2}},
    {"LagTimeA", {105, 2}},
    {"LagTimeB", {107, 2}},
    {"DelayRecordingTime", {109, 2}},
    {"MuteTimeStart", {111, 2}},
    {"MuteTimeEND", {113, 2}},
    {"TRACE_SAMPLE_COUNT", {115, 2}},
    {"TRACE_SAMPLE_INTERVAL", {117, 2}},
    {"GainType", {119, 2}},
    {"InstrumentGainConstant", {121, 2}},
    {"InstrumentInitialGain", {123, 2}},
    {"Correlated", {125, 2}},
    {"SweepFrequencyStart", {127, 2}},
    {"SweepFrequencyEnd", {129, 2}},
    {"SweepLength", {131, 2}},
    {"SweepType", {133, 2}},
    {"SweepTraceTaperLengthStart", {135, 2}},
    {"SweepTraceTaperLengthEnd", {137, 2}},
    {"TaperType", {139, 2}},
    {"AliasFilterFrequency", {141, 2}},
    {"AliasFilterSlope", {143, 2}},
    {"NotchFilterFrequency", {145, 2}},
    {"NotchFilterSlope", {147, 2}},
    {"LowCutFrequency", {149, 2}},
    {"HighCutFrequency", {151, 2}},
    {"LowCutSlope", {153, 2}},
    {"HighCutSlope", {155, 2}},
    {"YearDataRecorded", {157, 2}},
    {"DayOfYear", {159, 2}},
    {"HourOfDay", {161, 2}},
    {"MinuteOfHour", {163, 2}},
    {"SecondOfMinute", {165, 2}},
    {"TimeBaseCode", {167, 2}},
    {"TraceWeightingFactor", {169, 2}},
    {"GeophoneGroupNumberRoll1", {171, 2}},
    {"GeophoneGroupNumberFirstTraceOrigField", {173, 2}},
    {"GeophoneGroupNumberLastTraceOrigField", {175, 2}},
    {"GapSize", {177, 2}},
    {"OverTravel", {179, 2}},
    {"CDP_X", {181, 4}},
    {"CDP_Y", {185, 4}},
    {"INLINE_3D", {189, 4}},
    {"CROSSLINE_3D", {193, 4}},
    {"ShotPoint", {197, 4}},
    {"ShotPointScalar", {201, 4}},
    {"TraceValueMeasurementUnit", {203, 2}},
    {"TransductionConstantMantissa", {205, 2}},
    {"TransductionConstantPower", {209, 2}},
    {"TransductionUnit", {211, 2}},
    {"TraceIdentifier", {213, 2}},
    {"ScalarTraceHeader", {215, 2}},
    {"SourceType", {217, 2}},
    {"SourceEnergyDirectionVert", {219, 2}},
    {"SourceEnergyDirectionXline", {221, 2}},
    {"SourceEnergyDirectionIline", {223, 2}},
    {"SourceMeasurementMantissa", {225, 2}},
    {"SourceMeasurementExponent", {229, 2}},
    {"SourceMeasurementUnit", {231, 2}},
    {"UnassignedInt1", {233, 2}},
    {"UnassignedInt2", {237, 2}}
};

// Универсальная функция для чтения любого поля из trace header по имени
inline int32_t get_trace_field_value(const uint8_t* buf, const std::string& field_name) {
    auto it = TraceFieldOffsets.find(field_name);
    if (it == TraceFieldOffsets.end()) {
        throw std::invalid_argument("Unknown trace header field: " + field_name);
    }
    const FieldInfo& info = it->second;
    if (info.size == 2) {
        return static_cast<int32_t>(get_i16_be(buf, info.offset)); // sign-extend
    } else if (info.size == 4) {
        return get_i32_be(buf, info.offset);
    } else {
        throw std::runtime_error("Unsupported field size for trace header: " + std::to_string(info.size));
    }
}

