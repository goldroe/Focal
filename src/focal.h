#ifndef FOCAL_H
#define FOCAL_H

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

struct Image_Vertex {
    v2 p;
    v2 uv;
};

struct UI_Vertex {
    v2 p;
    v2 uv;
    v4 col;
};

struct Image_Constants {
    m4 mvp;
    v4 channels;
};

struct Grid_Constants {
    v4 c1;
    v4 c2;
    v2 cp;
    f32 size;
    f32 padding;
};

struct UI_Constants {
    m4 projection;
};

struct Shader_Program {
    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;
    ID3D11InputLayout *input_layout;
    ID3D11Buffer *constant_buffer;
};

struct UI_Draw_Data {
    int vertex_buffer_size;
    Array<UI_Vertex> vertices;
    ID3D11Buffer *vertex_buffer;
};

internal void os_popup(char *str) {
    MessageBoxA(NULL, str, NULL, MB_OK | MB_ICONERROR | MB_ICONEXCLAMATION);
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
