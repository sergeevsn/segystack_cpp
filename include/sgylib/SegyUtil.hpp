#pragma once
#include <cstdint>
#include <cmath>

struct FieldInfo {
    int offset; // 1-based offset
    int size;   // bytes: 2 or 4
};

inline int16_t get_i16_be(const uint8_t* buf, int offset1based) {
    int offset = offset1based - 1;
    return (static_cast<int16_t>(buf[offset]) << 8) | buf[offset + 1];
}

inline int32_t get_i32_be(const uint8_t* buf, int offset1based) {
    int offset = offset1based - 1;
    return (static_cast<int32_t>(buf[offset]) << 24) |
           (static_cast<int32_t>(buf[offset + 1]) << 16) |
           (static_cast<int32_t>(buf[offset + 2]) << 8) |
           buf[offset + 3];
}

inline float ibm_to_float(uint32_t ibm) {
    if (ibm == 0) return 0.0f;
    int sign = ((ibm >> 31) & 0x01);
    int exponent = ((ibm >> 24) & 0x7F) - 64;
    uint32_t fraction = ibm & 0x00FFFFFF;
    double mantissa = static_cast<double>(fraction) / static_cast<double>(0x01000000);
    double value = std::ldexp(mantissa, exponent * 4);
    return sign ? -static_cast<float>(value) : static_cast<float>(value);
}

inline uint32_t ieee_to_ibm(float val) {
    if (val == 0.0f) return 0;
    uint32_t sign = (val < 0) ? 0x80000000 : 0;
    if (val < 0) val = -val;
    int exponent = 64;
    while (val < 1.0f) { val *= 16.0f; --exponent; }
    while (val >= 1.0f) { val /= 16.0f; ++exponent; }
    uint32_t fraction = static_cast<uint32_t>(val * 0x01000000) & 0x00FFFFFF;
    return sign | (exponent << 24) | fraction;
} 

inline void set_i16_be(uint8_t* buf, int offset1based, int16_t value) {
    int offset = offset1based - 1;
    buf[offset] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>(value & 0xFF);
}

inline void set_i32_be(uint8_t* buf, int offset1based, int32_t value) {
    int offset = offset1based - 1;
    buf[offset]     = static_cast<uint8_t>((value >> 24) & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buf[offset + 2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buf[offset + 3] = static_cast<uint8_t>(value & 0xFF);
} 

inline uint32_t get_u32_be(const uint8_t* buf) {
    return (static_cast<uint32_t>(buf[0]) << 24) |
           (static_cast<uint32_t>(buf[1]) << 16) |
           (static_cast<uint32_t>(buf[2]) << 8)  |
           (static_cast<uint32_t>(buf[3]));
} 

inline void put_u32_be(uint8_t* buf, uint32_t value) {
    buf[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    buf[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buf[2] = static_cast<uint8_t>((value >> 8)  & 0xFF);
    buf[3] = static_cast<uint8_t>(value & 0xFF);
}