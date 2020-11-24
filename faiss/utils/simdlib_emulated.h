/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <cstdint>
#include <cstring>
#include <functional>


namespace faiss {


struct simd256bit {

    union {
        uint8_t u8[32];
        uint16_t u16[16];
        uint32_t u32[8];
        float f32[8];
    };

    simd256bit() {}

    simd256bit(const void *x)
    {
        memcpy(u8, x, 32);
    }

    void clear() {
        memset(u8, 0, 32);
    }

    void storeu(void *ptr) const {
        memcpy(ptr, u8, 32);
    }

    void loadu(const void *ptr) {
        memcpy(u8, ptr, 32);
    }

    void store(void *ptr) const {
        storeu(ptr);
    }

    void bin(char bits[257]) const {
        const char *bytes = (char*)this->u8;
        for (int i = 0; i < 256; i++) {
            bits[i] = '0' + ((bytes[i / 8] >> (i % 8)) & 1);
        }
        bits[256] = 0;
    }

    std::string bin() const {
        char bits[257];
        bin(bits);
        return std::string(bits);
    }

};




/// vector of 16 elements in uint16
struct simd16uint16: simd256bit {
    simd16uint16() {}

    simd16uint16(int x) {
        set1(x);
    }

    simd16uint16(uint16_t x) {
        set1(x);
    }

    simd16uint16(simd256bit x): simd256bit(x) {}

    simd16uint16(const uint16_t *x): simd256bit((const void*)x) {}

    std::string elements_to_string(const char * fmt) const {
        char res[1000], *ptr = res;
        for(int i = 0; i < 16; i++) {
            ptr += sprintf(ptr, fmt, u16[i]);
        }
        // strip last ,
        ptr[-1] = 0;
        return std::string(res);
    }

    std::string hex() const {
        return elements_to_string("%02x,");
    }

    std::string dec() const {
        return elements_to_string("%3d,");
    }

    static simd16uint16 unary_func(
        simd16uint16 a, std::function<uint16_t (uint16_t)> f)
    {
        simd16uint16 c;
        for(int j = 0; j < 16; j++) {
            c.u16[j] = f(a.u16[j]);
        }
        return c;
    }


    static simd16uint16 binary_func(
        simd16uint16 a, simd16uint16 b,
        std::function<uint16_t (uint16_t, uint16_t)> f)
    {
        simd16uint16 c;
        for(int j = 0; j < 16; j++) {
            c.u16[j] = f(a.u16[j], b.u16[j]);
        }
        return c;
    }

    void set1(uint16_t x) {
        for(int i = 0; i < 16; i++) {
            u16[i] = x;
        }
    }

    // shift must be known at compile time
    simd16uint16 operator >> (const int shift) const {
        return unary_func(*this, [shift](uint16_t a) {return a >> shift; });
    }


    // shift must be known at compile time
    simd16uint16 operator << (const int shift) const {
        return unary_func(*this, [shift](uint16_t a) {return a << shift; });
    }

    simd16uint16 operator += (simd16uint16 other) {
        *this = *this + other;
        return *this;
    }

    simd16uint16 operator -= (simd16uint16 other) {
        *this = *this - other;
        return *this;
    }

    simd16uint16 operator + (simd16uint16 other) const {
        return binary_func(*this, other,
            [](uint16_t a, uint16_t b) {return a + b; }
        );
    }

    simd16uint16 operator - (simd16uint16 other) const {
        return binary_func(*this, other,
            [](uint16_t a, uint16_t b) {return a - b; }
        );
    }

    simd16uint16 operator & (simd256bit other) const {
        return binary_func(*this, other,
            [](uint16_t a, uint16_t b) {return a & b; }
        );
    }

    simd16uint16 operator | (simd256bit other) const {
        return binary_func(*this, other,
            [](uint16_t a, uint16_t b) {return a | b; }
        );
    }

    // returns binary masks
    simd16uint16 operator == (simd256bit other) const {
        return binary_func(*this, other,
            [](uint16_t a, uint16_t b) {return a == b ? 0xffff : 0; }
        );
    }

    simd16uint16 operator ~() const {
        return unary_func(*this, [](uint16_t a) {return ~a; });
    }

    // get scalar at index 0
    uint16_t get_scalar_0() const {
        return u16[0];
    }

    // mask of elements where this >= thresh
    // 2 bit per component: 16 * 2 = 32 bit
    uint32_t ge_mask(simd16uint16 thresh) const {
        uint32_t gem = 0;
        for(int j = 0; j < 16; j++) {
            if (u16[j] >= thresh.u16[j]) {
                gem |= 3 << (j * 2);
            }
        }
        return gem;
    }

    uint32_t le_mask(simd16uint16 thresh) const {
        return thresh.ge_mask(*this);
    }

    uint32_t gt_mask(simd16uint16 thresh) const {
        return ~le_mask(thresh);
    }

    bool all_gt(simd16uint16 thresh) const {
        return le_mask(thresh) == 0;
    }

    // for debugging only
    uint16_t operator [] (int i) const {
        return u16[i];
    }

    void accu_min(simd16uint16 incoming) {
        for(int j = 0; j < 16; j++) {
            if (incoming.u16[j] < u16[j]) {
                u16[j] = incoming.u16[j];
            }
        }
    }

    void accu_max(simd16uint16 incoming) {
        for(int j = 0; j < 16; j++) {
            if (incoming.u16[j] > u16[j]) {
                u16[j] = incoming.u16[j];
            }
        }
    }

};


// not really a std::min because it returns an elementwise min
inline simd16uint16 min(simd16uint16 av, simd16uint16 bv) {
    return simd16uint16::binary_func(av, bv,
        [](uint16_t a, uint16_t b) {return std::min(a, b); }
    );
}

inline simd16uint16 max(simd16uint16 av, simd16uint16 bv) {
    return simd16uint16::binary_func(av, bv,
        [](uint16_t a, uint16_t b) {return std::max(a, b); }
    );
}


// decompose in 128-lanes: a = (a0, a1), b = (b0, b1)
// return (a0 + a1, b0 + b1)
// TODO find a better name
inline simd16uint16 combine2x2(simd16uint16 a, simd16uint16 b) {
    simd16uint16 c;
    for(int j = 0; j < 8; j++) {
        c.u16[j] = a.u16[j] + a.u16[j + 8];
        c.u16[j + 8] = b.u16[j] + b.u16[j + 8];
    }
    return c;
}

// compare d0 and d1 to thr, return 32 bits corresponding to the concatenation
// of d0 and d1 with thr
inline uint32_t cmp_ge32(simd16uint16 d0, simd16uint16 d1, simd16uint16 thr) {
    uint32_t gem = 0;
    for(int j = 0; j < 16; j++) {
        if (d0.u16[j] >= thr.u16[j]) {
            gem |= 1 << j;
        }
        if (d1.u16[j] >= thr.u16[j]) {
            gem |= 1 << (j + 16);
        }
    }
    return gem;
}


inline uint32_t cmp_le32(simd16uint16 d0, simd16uint16 d1, simd16uint16 thr) {
    uint32_t gem = 0;
    for(int j = 0; j < 16; j++) {
        if (d0.u16[j] <= thr.u16[j]) {
            gem |= 1 << j;
        }
        if (d1.u16[j] <= thr.u16[j]) {
            gem |= 1 << (j + 16);
        }
    }
    return gem;
}



// vector of 32 unsigned 8-bit integers
struct simd32uint8: simd256bit {

    simd32uint8() {}

    simd32uint8(int x) {set1(x); }

    simd32uint8(uint8_t x) {set1(x); }

    simd32uint8(simd256bit x): simd256bit(x) {}

    simd32uint8(const uint8_t *x): simd256bit((const void*)x) {}

    std::string elements_to_string(const char * fmt) const {
        char res[1000], *ptr = res;
        for(int i = 0; i < 32; i++) {
            ptr += sprintf(ptr, fmt, u8[i]);
        }
        // strip last ,
        ptr[-1] = 0;
        return std::string(res);
    }

    std::string hex() const {
        return elements_to_string("%02x,");
    }

    std::string dec() const {
        return elements_to_string("%3d,");
    }

    void set1(uint8_t x) {
        for(int j = 0; j < 32; j++) {
            u8[j] = x;
        }
    }

    static simd32uint8 binary_func(
        simd32uint8 a, simd32uint8 b,
        std::function<uint8_t (uint8_t, uint8_t)> f)
    {
        simd32uint8 c;
        for(int j = 0; j < 32; j++) {
            c.u8[j] = f(a.u8[j], b.u8[j]);
        }
        return c;
    }


    simd32uint8 operator & (simd256bit other) const {
        return binary_func(*this, other,
            [](uint8_t a, uint8_t b) {return a & b; }
        );
    }

    simd32uint8 operator + (simd32uint8 other) const {
        return binary_func(*this, other,
            [](uint8_t a, uint8_t b) {return a + b; }
        );
    }

    // The very important operation that everything relies on
    simd32uint8 lookup_2_lanes(simd32uint8 idx) const {
        simd32uint8 c;
        for(int j = 0; j < 32; j++) {
            if (idx.u8[j] & 0x80) {
                c.u8[j] = 0;
            } else {
                uint8_t i = idx.u8[j] & 15;
                if (j < 16) {
                    c.u8[j] = u8[i];
                } else {
                    c.u8[j] = u8[16 + i];
                }
            }
        }
        return c;
    }

    // extract + 0-extend lane
    // this operation is slow (3 cycles)
    /*
    simd16uint16 lane0_as_uint16() const {
        __m128i x = _mm256_extracti128_si256(i, 0);
        return simd16uint16(_mm256_cvtepu8_epi16(x));
    }

    simd16uint16 lane1_as_uint16() const {
        __m128i x = _mm256_extracti128_si256(i, 1);
        return simd16uint16(_mm256_cvtepu8_epi16(x));
    }
*/
    simd32uint8 operator += (simd32uint8 other) {
        *this = *this + other;
        return *this;
    }

    simd16uint16 operator + (simd16uint16 other) const {
        return binary_func(*this, other,
            [](uint8_t a, uint8_t b) {return a + b; }
        );
    }

    // for debugging only
    uint8_t operator [] (int i) const {
        return u8[i];
    }

};

/// vector of 8 unsigned 32-bit integers
struct simd8uint32: simd256bit {
    simd8uint32() {}


    simd8uint32(uint32_t x) {set1(x); }

    simd8uint32(simd256bit x): simd256bit(x) {}

    simd8uint32(const uint8_t *x): simd256bit((const void*)x) {}

    std::string elements_to_string(const char * fmt) const {
        char res[1000], *ptr = res;
        for(int i = 0; i < 8; i++) {
            ptr += sprintf(ptr, fmt, u32[i]);
        }
        // strip last ,
        ptr[-1] = 0;
        return std::string(res);
    }

    std::string hex() const {
        return elements_to_string("%08x,");
    }

    std::string dec() const {
        return elements_to_string("%10d,");
    }

    void set1(uint32_t x) {
        for (int i = 0; i < 8; i++) {
            u32[i] = x;
        }
    }

};

struct simd8float32: simd256bit {

    simd8float32() {}

    simd8float32(simd256bit x): simd256bit(x) {}

    simd8float32(float x) {set1(x); }

    simd8float32(const float *x) {loadu((void*)x); }

    void set1(float x) {
        for(int i = 0; i < 8; i++) {
            f32[i] = x;
        }
    }

    static simd8float32 binary_func(
        simd8float32 a, simd8float32 b,
        std::function<float (float, float)> f)
    {
        simd8float32 c;
        for(int j = 0; j < 8; j++) {
            c.f32[j] = f(a.f32[j], b.f32[j]);
        }
        return c;
    }

    simd8float32 operator * (simd8float32 other) const {
        return binary_func(*this, other,
            [](float a, float b) {return a * b; }
        );
    }

    simd8float32 operator + (simd8float32 other) const {
        return binary_func(*this, other,
            [](float a, float b) {return a + b; }
        );
    }

    simd8float32 operator - (simd8float32 other) const {
        return binary_func(*this, other,
            [](float a, float b) {return a - b; }
        );
    }

    std::string tostring() const {
        char res[1000], *ptr = res;
        for(int i = 0; i < 8; i++) {
            ptr += sprintf(ptr, "%g,", f32[i]);
        }
        // strip last ,
        ptr[-1] = 0;
        return std::string(res);
    }

};


// hadd does not cross lanes
inline simd8float32 hadd(simd8float32 a, simd8float32 b) {
    simd8float32 c;
    c.f32[0] = a.f32[0] + a.f32[1];
    c.f32[1] = a.f32[2] + a.f32[3];
    c.f32[2] = b.f32[0] + b.f32[1];
    c.f32[3] = b.f32[2] + b.f32[3];

    c.f32[4] = a.f32[4] + a.f32[5];
    c.f32[5] = a.f32[6] + a.f32[7];
    c.f32[6] = b.f32[4] + b.f32[5];
    c.f32[7] = b.f32[6] + b.f32[7];

    return c;
}

inline simd8float32 unpacklo(simd8float32 a, simd8float32 b) {
    simd8float32 c;
    c.f32[0] = a.f32[0];
    c.f32[1] = b.f32[0];
    c.f32[2] = a.f32[1];
    c.f32[3] = b.f32[1];

    c.f32[4] = a.f32[4];
    c.f32[5] = b.f32[4];
    c.f32[6] = a.f32[5];
    c.f32[7] = b.f32[5];

    return c;
}

inline simd8float32 unpackhi(simd8float32 a, simd8float32 b) {
    simd8float32 c;
    c.f32[0] = a.f32[2];
    c.f32[1] = b.f32[2];
    c.f32[2] = a.f32[3];
    c.f32[3] = b.f32[3];

    c.f32[4] = a.f32[6];
    c.f32[5] = b.f32[6];
    c.f32[6] = a.f32[7];
    c.f32[7] = b.f32[7];

    return c;
}

// compute a * b + c
inline simd8float32 fmadd(simd8float32 a, simd8float32 b, simd8float32 c) {
    simd8float32 res;
    for(int i = 0; i < 8; i++) {
        res.f32[i] = a.f32[i] * b.f32[i] + c.f32[i];
    }
    return res;
}



} // namespace faiss
