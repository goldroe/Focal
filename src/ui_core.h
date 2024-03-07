#ifndef UI_CORE_H
#define UI_CORE_H

#define ui_clamp(V, Min, Max) ((V) < (Min) ? (Min) : (V) > (Max) ? (Max) : (V))
#define ui_min(A, B) ((A) < (B) ? (A) : (B))
#define ui_max(A, B) ((A) > (B) ? (A) : (B))

union UI_Rect {
    struct {
        f32 x, y;
        f32 width, height;
    };
    struct {
        v2 p;
        v2 size;
    };

    UI_Rect(f32 x, f32 y, f32 width, f32 height) : x(x), y(y), width(width), height(height) {}
    UI_Rect(v2 p, v2 size) : p(p), size(size) {}
};

enum UI_Axis {
    UI_AxisNil = -1,
    UI_AxisX,
    UI_AxisY,
    UI_AxisCount
};

enum UI_Alignment {
    UI_Alignment_Center,
    UI_Alignment_Start,
    UI_Alignment_End
};

enum UI_Size_Type {
    UI_Size_Pixels,
    UI_Size_TextContents,
    UI_Size_ChildrenSum,
    UI_Size_ParentPct,
};

enum UI_Position_Type {
    UI_Position_Automatic,
    UI_Position_Absolute,
};

struct UI_Position {
    UI_Position_Type type;
    f32 value;
};

struct UI_Size {
    UI_Size_Type type;
    f32 value;
    f32 strictness;
};

enum UI_Box_Flags {
    UI_BoxFlag_Nil = 0,
    UI_BoxFlag_DrawBackground = (1 << 0),
    UI_BoxFlag_DrawText = (1 << 1),
    UI_BoxFlag_DrawBorder = (1 << 2),
    UI_BoxFlag_Clickable = (1 << 3),
};

inline UI_Box_Flags operator|(UI_Box_Flags a, UI_Box_Flags b) {
    return static_cast<UI_Box_Flags>(static_cast<int>(a) | static_cast<int>(b));
}

inline UI_Box_Flags& operator|=(UI_Box_Flags &a, UI_Box_Flags b) {
    return a = a | b;
}

typedef u32 UI_Hash;

struct UI_Box {
    UI_Hash hash;
    UI_Box_Flags flags;

    UI_Size sem_size[UI_AxisCount];
    UI_Position sem_position[UI_AxisCount];
    v2 cursor;
    v2 position;
    v2 size;

    UI_Axis child_layout_axis;
    UI_Alignment alignment[UI_AxisCount];
    v2 padding;
    v2 spacing;

    UI_Box *first;
    UI_Box *last;
    UI_Box *next;
    UI_Box *prev;
    UI_Box *parent;

    string text;

    v4 bg_color;
    v4 text_color;
    v4 border_color;
};

struct UI_State {
    Array<UI_Box> builds[2];
    int build_index;

    Array<UI_Box*> parents;
    UI_Hash active_hash;

    Font *font;

    v2 mouse;
    bool mouse_down;
    bool mouse_pressed;
    bool want_mouse_capture;
    bool want_key_capture; 

    Array<UI_Box*> parent_stack;
};

struct UI_Button_Style {
    v4 bg_color;
    v4 text_color;
    v4 hot_bg_color;
    v4 hot_text_color;
};

#endif // UI_CORE_H
