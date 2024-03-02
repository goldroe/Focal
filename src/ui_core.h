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
};

enum UI_Axis {
    UI_AxisNil = -1,
    UI_AxisX,
    UI_AxisY,
    UI_AxisCount
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
    UI_BoxFlag_DrawBackground = (1 << 0),
    UI_BoxFlag_DrawText = (1 << 1),
    UI_BoxFlag_DrawBorder = (1 << 2),
};

typedef u32 UI_Hash;

struct UI_Box {
    UI_Hash hash;
    UI_Box_Flags flags;

    UI_Size req_size[UI_AxisCount];
    UI_Position position[UI_AxisCount];
    v2 calc_size;
    UI_Rect rect;
    v2 rel_p;

    UI_Axis child_layout_axis;

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

    Font *font;
};


#endif // UI_CORE_H
