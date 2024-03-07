#include "ui_core.h"

global UI_State ui_state;

internal UI_Hash ui_hash_djb2(string str) {
    UI_Hash hash = 5381;
    int c;
    for (u64 i = 0; i < str.count; i++) {
        c = str.data[i];
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

internal UI_Box *ui_top_parent() {
    if (ui_state.parent_stack.is_empty()) return nullptr;
    return ui_state.parent_stack.back();
}

internal void ui_push_parent(UI_Box *box) {
    // NOTE: Top level parent
    if (ui_state.parent_stack.is_empty()) {
        ui_state.parents.push(box);
    }

    ui_state.parent_stack.push(box);
}

internal void ui_pop_parent() {
    ui_state.parent_stack.pop();
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

internal UI_Box *ui_build_box_from_hash(UI_Box_Flags flags, UI_Hash hash) {
    UI_Box *last_box = ui_find_box(hash, !ui_state.build_index);
    UI_Box box{};
    box.flags = flags;
    box.hash = hash;
    UI_Box *result = ui_state.builds[ui_state.build_index].push(box);
    UI_Box *parent = ui_top_parent();
    if (parent != nullptr) {
        ui_push_box(result, parent);
    }
    return result;
}

internal UI_Box *ui_build_box_from_string(char *str, UI_Box_Flags flags) {
    UI_Box *box = ui_build_box_from_hash(flags, ui_hash_djb2(str));
    return box;
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
        size += 2 * box->padding[axis];
        break;
    case UI_Size_Pixels:
        size = box->sem_size[axis].value;
        size += 2 * box->padding[axis];
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
        sum += 2 * box->padding[axis];
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
    box->cursor[axis] += box->padding[axis];
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

internal bool ui_button(string label, UI_Button_Style style) {
    UI_Box *last_button = ui_find_box(label);

    UI_Box *text_box = nullptr;
    UI_Box *box = nullptr;

    box = ui_build_box_from_string((char *)label.data, UI_BoxFlag_DrawBackground|UI_BoxFlag_Clickable);
    box->sem_size[UI_AxisX] = { UI_Size_ChildrenSum, 1.0f };
    box->sem_size[UI_AxisY] = { UI_Size_Pixels, ui_state.font->glyph_height * 2};
    box->sem_position[UI_AxisX] = { UI_Position_Automatic, 0 };
    box->sem_position[UI_AxisY] = { UI_Position_Automatic, 0 };
    box->bg_color = style.bg_color;

    string text_box_string = string_concat(label, "__TEXT");
    UI_Hash text_hash = ui_hash_djb2(text_box_string); 
    string_free(&text_box_string);

    ui_push_parent(box);

    text_box = ui_build_box_from_hash(UI_BoxFlag_DrawText, text_hash);
    text_box->sem_size[UI_AxisX] = { UI_Size_TextContents, 1.0f };
    text_box->sem_size[UI_AxisY] = { UI_Size_TextContents, 1.0f };
    text_box->sem_position[UI_AxisX] = { UI_Position_Automatic, 1.0f};
    text_box->sem_position[UI_AxisY] = { UI_Position_Automatic, 1.0f};
    text_box->text = label;
    text_box->text_color = style.text_color;

    ui_pop_parent();

    bool result = false;
    bool hot = false;
    if (last_button) {
        hot = ui_in_rect(ui_state.mouse, UI_Rect(last_button->position, last_button->size));
    }

    if (hot) {
        box->bg_color = style.hot_bg_color;
        text_box->text_color = style.hot_text_color;
        if (ui_state.mouse_pressed) {
            result = true;
        }
    }

    return result;
}

internal UI_Box *ui_bar(UI_Axis layout_axis, v4 bg_color) {
    UI_Box *box = ui_build_box_from_hash(UI_BoxFlag_DrawBackground, 0);
    box->child_layout_axis = layout_axis;
    box->sem_size[UI_AxisX] = { UI_Size_ChildrenSum, 1.0f };
    box->sem_size[UI_AxisY] = { UI_Size_ChildrenSum, 1.0f };
    box->sem_position[UI_AxisX] = { UI_Position_Automatic, 1.0f };
    box->sem_position[UI_AxisY] = { UI_Position_Automatic, 1.0f };
    box->bg_color = bg_color;
    return box;
}
