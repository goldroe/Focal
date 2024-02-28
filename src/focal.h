#ifndef FOCAL_H
#define FOCAL_H

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
    f32 scroll_y = 1.0f;
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


#endif // FOCAL_H
