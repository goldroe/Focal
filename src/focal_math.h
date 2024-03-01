#ifndef FOCAL_MATH_H
#define FOCAL_MATH_H

union v2 {
    struct {
        float x, y;
    };
    struct {
        float u, v;
    };
    float e[2];

    v2() : x(0), y(0) {}
    v2(float x, float y) : x(x), y(y) {}
    v2(int x, int y) : x((float)x), y((float)y) {}
    float &operator[](int index) { assert(index < 2); return e[index]; }
};

union v3 {
    struct {
        float x, y, z;
    };
    struct {
        float u, v, w;
    };
    float e[3];

    v3() : x(0), y(0), z(0) {}
    v3(float x, float y, float z) : x(x), y(y), z(z) {}
    v3(int x, int y, int z) : x((float)x), y((float)y), z((float)z) {}
    float &operator[](int index) { assert(index < 3); return e[index]; }
};

union v4 {
    struct {
        float x, y, z, w;
    };
    struct {
        float r, g, b, a;
    };
    float e[4];

    v4() : x(0), y(0), z(0), w(0) {}
    v4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    v4(int x, int y, int z, int w) : x((float)x), y((float)y), z((float)z), w((float)w) {}
    float &operator[](int index) { assert(index < 4); return e[index]; }
};

union m4 {
    float e[4][4];
};


internal m4 m4_id() {
    m4 result{};
    result.e[0][0] = 1.0f;
    result.e[1][1] = 1.0f;
    result.e[2][2] = 1.0f;
    result.e[3][3] = 1.0f;
    return result;
}

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

internal m4 m4_translate(v3 t) {
    m4 result = m4_id();
    result.e[3][0] = t.x;
    result.e[3][1] = t.y;
    result.e[3][2] = t.z;
    return result;
}

internal m4 m4_scale(v3 s) {
    m4 result{};
    result.e[0][0] = s.x;
    result.e[1][1] = s.y;
    result.e[2][2] = s.z;
    result.e[3][3] = 1.0f;
    return result;
}

internal v2 v2_mul_s(v2 v, float s) {
    v2 result;
    result.x = v.x * s;
    result.y = v.y * s;
    return result;
}

internal v2 v2_div_s(v2 v, float s) {
    v2 result;
    result.x = v.x / s;
    result.y = v.y / s;
    return result;
}

internal v2 v2_add(v2 a, v2 b) {
    v2 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

internal v2 v2_subtract(v2 a, v2 b) {
    v2 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

#endif // FOCAL_MATH_H
