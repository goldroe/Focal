#ifndef FOCAL_H
#define FOCAL_H

#include <stdint.h>
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef s8  b8;
typedef s32 b32;
typedef float f32;
typedef double f64;
#define internal static
#define global static
#define local_persist static

#include <string.h>
struct string {
    u8 *data;
    u64 count;
    string() : data(NULL), count(0) {}
    string(const char *cstr) : data((u8 *)cstr), count(strlen(cstr)) {}
    string(char *str, u64 len) : data((u8 *)str), count(len) {}
};
#define STRZ(Str) string(Str, strlen(Str))

inline string string_copy(const string str) {
    string result{};
    result.data = (u8 *)malloc(str.count);
    memcpy(result.data, str.data, str.count);
    result.count = str.count;
    return result;
}

union v2 {
    struct {
        f32 x, y;
    };
    struct {
        f32 u, v;
    };

    v2() : x(0), y(0) {}
    v2(f32 x, f32 y) : x(x), y(y) {}
    v2(int x, int y) : x((f32)x), y((f32)y) {}
};

union m4 {
    f32 e[4][4];
};

struct Image_Vertex {
    v2 p;
    v2 uv;
};

struct Image_Constants {
    m4 mvp;
};

struct Texture {
    string name;
    v2 size;
    ID3D11ShaderResourceView *srv;
};

struct Input_State {
    v2 mouse;
    b8 keys_down[256];
    b8 keys_pressed[256];

    bool key_down(int key) {
        return keys_down[key];
    }
    bool key_pressed(int key) {
        return keys_pressed[key];
    }
};


internal m4 m4_mult(m4 a, m4 b) {
    m4 result{};
    result.e[0][0] = a.e[0][0] * b.e[0][0] + a.e[0][1] * b.e[1][0] + a.e[0][2] * b.e[2][0] + a.e[0][3] * b.e[3][0]; 
    result.e[0][1] = a.e[0][0] * b.e[0][1] + a.e[0][1] * b.e[1][1] + a.e[0][2] * b.e[2][1] + a.e[0][3] * b.e[3][1]; 
    result.e[0][2] = a.e[0][0] * b.e[0][2] + a.e[0][1] * b.e[1][2] + a.e[0][2] * b.e[2][2] + a.e[0][3] * b.e[3][2]; 
    result.e[0][3] = a.e[0][0] * b.e[0][3] + a.e[0][1] * b.e[1][3] + a.e[0][2] * b.e[2][3] + a.e[0][3] * b.e[3][3]; 
    result.e[1][0] = a.e[1][0] * b.e[0][0] + a.e[1][1] * b.e[1][0] + a.e[1][2] * b.e[2][0] + a.e[1][3] * b.e[3][0]; 
    result.e[1][1] = a.e[1][0] * b.e[0][1] + a.e[1][1] * b.e[1][1] + a.e[1][2] * b.e[2][1] + a.e[1][3] * b.e[3][1]; 
    result.e[1][2] = a.e[1][0] * b.e[0][2] + a.e[1][1] * b.e[1][2] + a.e[1][2] * b.e[2][2] + a.e[1][3] * b.e[3][2]; 
    result.e[1][3] = a.e[1][0] * b.e[0][3] + a.e[1][1] * b.e[1][3] + a.e[1][2] * b.e[2][3] + a.e[1][3] * b.e[3][3]; 
    result.e[2][0] = a.e[2][0] * b.e[0][0] + a.e[2][1] * b.e[1][0] + a.e[2][2] * b.e[2][0] + a.e[2][3] * b.e[3][0]; 
    result.e[2][1] = a.e[2][0] * b.e[0][1] + a.e[2][1] * b.e[1][1] + a.e[2][2] * b.e[2][1] + a.e[2][3] * b.e[3][1]; 
    result.e[2][2] = a.e[2][0] * b.e[0][2] + a.e[2][1] * b.e[1][2] + a.e[2][2] * b.e[2][2] + a.e[2][3] * b.e[3][2]; 
    result.e[2][3] = a.e[2][0] * b.e[0][3] + a.e[2][1] * b.e[1][3] + a.e[2][2] * b.e[2][3] + a.e[2][3] * b.e[3][3]; 
    result.e[3][0] = a.e[3][0] * b.e[0][0] + a.e[3][1] * b.e[1][0] + a.e[3][2] * b.e[2][0] + a.e[3][3] * b.e[3][0]; 
    result.e[3][1] = a.e[3][0] * b.e[0][1] + a.e[3][1] * b.e[1][1] + a.e[3][2] * b.e[2][1] + a.e[3][3] * b.e[3][1]; 
    result.e[3][2] = a.e[3][0] * b.e[0][2] + a.e[3][1] * b.e[1][2] + a.e[3][2] * b.e[2][2] + a.e[3][3] * b.e[3][2]; 
    result.e[3][3] = a.e[3][0] * b.e[0][3] + a.e[3][1] * b.e[1][3] + a.e[3][2] * b.e[2][3] + a.e[3][3] * b.e[3][3]; 
    return result;
}

#endif // FOCAL_H
