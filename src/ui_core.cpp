#include "ui_core.h"

global UI_State ui_state;

void ui_init() {

}

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

internal UI_Box *ui_get_current_parent() {
    if (ui_state.parents.is_empty()) return nullptr;
    return ui_state.parents.back();
}

internal void ui_push_parent(UI_Box *box) {
    ui_state.parents.push(box);
}

internal UI_Box *ui_push_box(UI_Box box) {
    int count = (int)ui_state.builds[ui_state.build_index].count;
    ui_state.builds[ui_state.build_index].push(box);
    return &ui_state.builds[ui_state.build_index][count];
}

internal UI_Box *ui_build_box_from_hash(UI_Box_Flags flags, UI_Hash hash) {
    UI_Box *last_box = ui_find_box(hash, !ui_state.build_index);
    UI_Box box{};
    box.flags = flags;
    box.hash = hash;
    box.parent = ui_get_current_parent();
    return ui_push_box(box);
}

internal UI_Box *ui_build_box_from_string(char *str, UI_Box_Flags flags) {
    UI_Box *box = ui_build_box_from_hash(flags, ui_hash_djb2(str));
    return box;
}

internal void ui_layout_calc_fixed_sizes(UI_Box *root, UI_Axis axis) {
    f32 size = 0;
    switch (root->req_size[axis].type) {
    default: break;
    case UI_Size_TextContents:
        // TODO: Get text size
        size = (axis == UI_AxisX) ? 100.0f : 20.0f;
        break;
    case UI_Size_Pixels:
        size = root->req_size[axis].value;
        break;
    }
    root->calc_size[axis] = size;

    for (UI_Box *child = root->first; child; child = child->next) {
        ui_layout_calc_fixed_sizes(child, axis);
    }
}

internal void ui_layout_calc_upward_dependent(UI_Box *root, UI_Axis axis) {
    UI_Box *parent = root->parent;
    f32 size = 0;
    switch (root->req_size[axis].type) {
    default: break;
    case UI_Size_ParentPct:
        if (parent == nullptr) break;
        size = root->req_size[axis].value * parent->calc_size[axis];
        break;
    }

    for (UI_Box *child = root->first; child; child = child->next) {
        ui_layout_calc_upward_dependent(child, axis);
    }
}

internal void ui_layout_calc_downward_dependent(UI_Box *root, UI_Axis axis) {
    switch (root->req_size[axis].type) {
    case UI_Size_ChildrenSum: {
        f32 sum = 0;
        for (UI_Box *child = root->first; child; child = child->next) {
            if (child->child_layout_axis == axis) {
                sum += child->calc_size[axis];
            }
        }
        sum = ui_clamp(sum, 0, root->calc_size[axis]);
        break;
    }
    }

    for (UI_Box *child = root->first; child; child = child->next) {
        ui_layout_calc_downward_dependent(child, axis);
    }
}

internal void ui_layout_calc_resolve_sizes(UI_Box *root, UI_Axis axis) {
    UI_Box *parent = root->parent;
    if (parent) {
        f32 end = root->rel_p[axis] + root->calc_size[axis];
        // TODO: cascade violation to the relative positions of siblings (axis-aligned?)
        if (end >= parent->calc_size[axis]) {
            f32 over = end - parent->calc_size[axis];
            root->calc_size[axis] -= over;
        }
    }

    for (UI_Box *child = root->first; child; child = child->next) {
        ui_layout_calc_resolve_sizes(child, axis);
    }
}

internal void ui_layout_place_boxes(UI_Box *root, UI_Axis axis) {
    f32 p = 0;
    // TODO: place boxes based on the layout axis, size and parent relative pos
    for (UI_Box *child = root->first; child; child = child->next) {
        child->rel_p[axis] = p;
        if (child->child_layout_axis == axis) {
            p += child->calc_size[axis];
        }
    }

    f32 abs_p = 0;
    for (UI_Box *box = root; box; box = box->parent) {
        abs_p += box->rel_p[axis];
    }

    root->rect.p[axis] = abs_p;
    root->rect.size[axis] = root->calc_size[axis];

    for (UI_Box *child = root->first; child; child = child->next) {
        ui_layout_place_boxes(root, axis);
    }
}

internal void ui_layout_apply_positions(UI_Box *root, UI_Axis axis) {
    switch (root->position[axis].type) {
    case UI_Position_Absolute: {
        root->rect.p[axis] = root->position[axis].value;
        break;
    }
    }

    for (UI_Box *child = root->first; child; child = child->next) {
        ui_layout_apply_positions(child, axis);
    }
}

internal void ui_layout_calc(UI_Box *root, UI_Axis axis) {
    ui_layout_calc_fixed_sizes(root, axis);
    ui_layout_calc_upward_dependent(root, axis);
    ui_layout_calc_downward_dependent(root, axis);
    ui_layout_calc_resolve_sizes(root, axis);
    ui_layout_place_boxes(root, axis);
    ui_layout_apply_positions(root, axis);
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
