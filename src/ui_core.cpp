#include "ui_core.h"

global UI_State ui_state;

internal UI_Hash ui_hash_djb2(char *str) {
    UI_Hash hash = 5381;
    int c;
    while (*str) {
        c = *str++;
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash;
}

internal void hash_tests() {
    UI_Hash hash;
    char *str;

    str = "foo";
    hash = ui_hash_djb2(str);
    printf("%s hash %d\n", str, hash);

    str = "buzz";
    hash = ui_hash_djb2(str);
    printf("%s hash %d\n", str, hash);

    str = "header";
    hash = ui_hash_djb2(str);
    printf("%s hash %d\n", str, hash);

    str = "main";
    hash = ui_hash_djb2(str);
    printf("%s hash %d\n", str, hash);

    str = "main_foo_buzz_bizz";
    hash = ui_hash_djb2(str);
    printf("%s hash %d\n", str, hash);
}

internal UI_Box *ui_find_box(UI_Hash hash, int build_index) {
    UI_Box *result = nullptr;
    for (int i = 0; i < ui_state.builds[build_index].count; i++) {
        UI_Box *box = &ui_state.builds[build_index][i];
        if (box->hash == hash) {
            result = box;
            break;
        }
    }
    return result;
}

internal UI_Box *ui_find_box(string string) {
    UI_Hash hash = ui_hash_djb2((char *)string.data);
    return ui_find_box(hash, !ui_state.build_index);
}

internal UI_Box *ui_get_current_parent() {
    if (ui_state.parents.is_empty()) return nullptr;
    return ui_state.parents.back();
}

internal void ui_push_parent(UI_Box *box) {
    ui_state.parents.push(box);
}

internal UI_Box *ui_build_box_from_hash(UI_Box_Flags flags, UI_Hash hash) {
    UI_Box *last_box = ui_find_box(hash, !ui_state.build_index);
    UI_Box box{};
    box.flags = flags;
    box.hash = hash;
    UI_Box *result = ui_state.builds[ui_state.build_index].push(box);
    return result;
}

internal UI_Box *ui_build_box_from_string(char *str, UI_Box_Flags flags) {
    UI_Box *box = ui_build_box_from_hash(flags, ui_hash_djb2(str));
    return box;
}

internal void ui_push_box(UI_Box *box, UI_Box *parent) {
    assert(parent && box);
    box->parent = parent;
    box->next = nullptr;
    if (parent->first) {
        parent->last->next = box;
        box->prev = parent->last;
        parent->last = box;
    } else {
        parent->first = parent->last = box;
    }
}

internal f32 measure_text_width(string text, Font *font) {
    f32 width = 0.0f;
    for (int i = 0; i < text.count; i++) {
        u8 c = text.data[i];
        width += font->glyphs[c].advance_x;
    }
    return width;
}

internal void ui_layout_calc_fixed_sizes(UI_Box *box, UI_Axis axis) {
    f32 size = 0;
    switch (box->sem_size[axis].type) {
    default: break;
    case UI_Size_TextContents:
        size = (axis == UI_AxisX) ? measure_text_width(box->text, ui_state.font) + 4.0f : ui_state.font->glyph_height;
        break;
    case UI_Size_Pixels:
        size = box->sem_size[axis].value;
        break;
    }
    box->size[axis] = size;

    for (UI_Box *child = box->first; child; child = child->next) {
        ui_layout_calc_fixed_sizes(child, axis);
    }
}

internal void ui_layout_calc_upward_dependent(UI_Box *box, UI_Axis axis) {
    switch (box->sem_size[axis].type) {
    default: break;
    case UI_Size_ParentPct: {
        UI_Box *parent = box->parent;
        while (parent->sem_size[axis].type == UI_Size_ParentPct) {
            parent = parent->parent;
            assert(parent);
        }
        f32 size = box->sem_size[axis].value * parent->size[axis];
        box->size[axis] = size;
        break;
    }
    }

    for (UI_Box *child = box->first; child; child = child->next) {
        ui_layout_calc_upward_dependent(child, axis);
    }
}

internal void ui_layout_calc_downward_dependent(UI_Box *box, UI_Axis axis) {
    for (UI_Box *child = box->first; child; child = child->next) {
        ui_layout_calc_downward_dependent(child, axis);
    }

    f32 size = 0;
    switch (box->sem_size[axis].type) {
    case UI_Size_ChildrenSum: {
        assert(box->first && "Leaves can't depend on children");
        f32 sum = 0;
        for (UI_Box *child = box->first; child; child = child->next) {
            if (box->child_layout_axis == axis) {
                sum += child->size[axis];
            } else {
                sum = ui_max(sum, child->size[axis]);
            }
        }
        box->size[axis] = sum;
        break;
    }
    }
}

internal void ui_layout_try_auto(UI_Box *box, UI_Axis axis) {
    // NOTE: Try to calculate the cursors(relative pos from parent) of the children according the axis and the size of the siblings
    if (box->child_layout_axis == axis) {
        f32 cursor = 0;
        for (UI_Box *child = box->first; child; child = child->next) {
            child->cursor[axis] = cursor;
            if (child->sem_position[axis].type == UI_Position_Automatic) {
                cursor += child->size[axis];
            }
        }
    }
    for (UI_Box *child = box->first; child; child = child->next) {
        ui_layout_try_auto(child, axis);
    }
}

internal void ui_layout_finalize_positions(UI_Box *box, UI_Axis axis) {
    switch (box->sem_position[axis].type) {
    case UI_Position_Absolute: {
        box->cursor[axis] = box->sem_position[axis].value;
        box->position[axis] = box->sem_position[axis].value;
        break;
    }
    case UI_Position_Automatic: {
        f32 position = 0;
        for (UI_Box *p = box; p; p = p->parent) {
            position += p->cursor[axis];
        }
        box->position[axis] = position;
        break; 
    }
    }
    for (UI_Box *child = box->first; child; child = child->next) {
        ui_layout_finalize_positions(child, axis);
    }
}

internal void ui_layout_calc(UI_Box *root, UI_Axis axis) {
    ui_layout_calc_fixed_sizes(root, axis);
    ui_layout_calc_upward_dependent(root, axis);
    ui_layout_calc_downward_dependent(root, axis);
    ui_layout_try_auto(root, axis);
    ui_layout_finalize_positions(root, axis);
}

internal void ui_start_build() {
    ui_state.parents.reset_count();
    ui_state.builds[ui_state.build_index].reset_count();
}

internal void ui_end_build() {
    for (int i = 0; i < ui_state.parents.count; i++) {
        UI_Box *parent = ui_state.parents[i];
        for (UI_Axis axis = UI_AxisX; axis < UI_AxisCount; axis = (UI_Axis)(axis + 1)) {
            ui_layout_calc(parent, axis);
        }
    }

    ui_state.build_index = !ui_state.build_index;
}

inline bool ui_in_rect(v2 p, UI_Rect rect) {
    return p.x >= rect.x && p.x <= (rect.x + rect.width) && p.y >= rect.y && p.y <= (rect.y + rect.height);
}

internal UI_Box *ui_button(string label) {
    UI_Box *last_button = ui_find_box(label);
    UI_Box *button = ui_build_box_from_string((char *)label.data, UI_BoxFlag_DrawText|UI_BoxFlag_DrawBackground);
    button->sem_size[UI_AxisX] = { UI_Size_TextContents, 1.0f };
    button->sem_size[UI_AxisY] = { UI_Size_TextContents, 1.0f };
    button->sem_position[UI_AxisX] = { UI_Position_Automatic, 0 };
    button->sem_position[UI_AxisY] = { UI_Position_Automatic, 0 };
    button->bg_color = v4(0.24f, 0.25f, 0.25f, 1.0f);
    button->text_color = v4(0.98f, 0.98f, 0.98f, 1.0f);
    button->text = label;

    bool hot = false;
    if (last_button) {
        hot = ui_in_rect(ui_state.mouse, UI_Rect(last_button->position, last_button->size));
    }
    if (hot) {
        button->bg_color = v4(0.6f, 0.64f, 0.64f, 1.0f);
        button->text_color = v4(0.4f, 0.4f, 0.4f, 1.0f);
        if (ui_state.mouse_pressed) {
        }
    }
    return button;
}
