#ifndef FOCAL_H
#define FOCAL_H

struct Image_Vertex {
    v2 p;
    v2 uv;
};

struct Texture {
    string name;
    v2 size;
    ID3D11ShaderResourceView *srv;
};

struct Input_State {
    v2 mouse;
    v2 last_mouse;
    f32 scroll_y_dt;
    b8 keys_down[256];
    b8 keys_pressed[256];

    bool key_down(int key) {
        return keys_down[key];
    }
    bool key_press(int key) {
        return keys_pressed[key];
    }
};

struct Image_Constants {
    m4 mvp;
};

struct Grid_Constants {
    v4 c1;
    v4 c2;
    f32 size;
    v3 padding;
};

struct Shader_Program {
    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;
    ID3D11InputLayout *input_layout;
    ID3D11Buffer *constant_buffer;
};

internal void os_popup(char *str) {
    MessageBoxA(NULL, str, NULL, MB_OK);
}

internal void os_popupf(char *fmt, ...) {
    char *buffer = nullptr;
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(nullptr, 0, fmt, args) + 1;
    buffer = (char *)malloc(len);
    vsnprintf(buffer, len, fmt, args);
    va_end(args);
    os_popup(buffer);
    free(buffer);
}

#endif // FOCAL_H
