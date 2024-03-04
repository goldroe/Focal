// TODO: Cropping and saving
// TODO: Display image info
// TODO: Directory loading

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif // _MSC_VER

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>
#include <windowsx.h>
#include <fileapi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#undef near
#undef far
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#endif // _WIN32

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4244)
#include <stb_image.h>
#pragma warning(pop)

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

struct string {
    u8 *data;
    u64 count;
    string() : data(NULL), count(0) {}
    string(const char *cstr) : data((u8 *)cstr), count(strlen(cstr)) {}
    string(char *str, u64 len) : data((u8 *)str), count(len) {}
};
#define STRZ(Str) string(Str, strlen(Str))

#include "array.cpp"

#include "focal_math.h"
#include "focal.h"
#include "ui_core.cpp"

#define WIDTH 840
#define HEIGHT 720

global IDXGISwapChain *swapchain;
global ID3D11Device *d3d_device;
global ID3D11DeviceContext *d3d_context;
global ID3D11RenderTargetView *render_target;

global Input_State input;

global f32 zoom = 1.0f;

global UI_Draw_Data ui_draw_data;

Font *load_font(char *font_name, int font_size) {
    Font *font = (Font *)malloc(sizeof(Font));
    FT_Library ft_lib;
    int err = FT_Init_FreeType(&ft_lib);
    if (err) {
        fprintf(stderr, "Error creating freetype lib: %d\n", err);
    }

    FT_Face face;
    err = FT_New_Face(ft_lib, font_name, 0, &face);
    if (err == FT_Err_Unknown_File_Format) {
        fprintf(stderr, "Format not supported\n"); 
    } else if (err) {
        fprintf(stderr, "Font file could not be read\n");
    }

    err = FT_Set_Pixel_Sizes(face, 0, font_size);
    if (err) {
        fprintf(stderr, "FT_Set_Pixel_Sizes failed\n");
    }

    int bbox_ymax = FT_MulFix(face->bbox.yMax, face->size->metrics.y_scale) >> 6;
    int bbox_ymin = FT_MulFix(face->bbox.yMin, face->size->metrics.y_scale) >> 6;
    int height = bbox_ymax - bbox_ymin;
    float ascend = face->size->metrics.ascender / 64.f;
    float descend = face->size->metrics.descender / 64.f;
    float bbox_height = (float)(bbox_ymax - bbox_ymin);
    float glyph_height = (float)face->size->metrics.height / 64.f;
    float glyph_width = (float)(face->bbox.xMax - face->bbox.xMin) / 64.f;

    int atlas_width = 0;
    int atlas_height = 0;
    int max_bmp_height = 0;
    for (int i = 32; i < 256; i++) {
        unsigned char c = (unsigned char)i;
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            printf("Error loading char %c\n", c);
            continue;
        }

        atlas_width += face->glyph->bitmap.width;
        if (atlas_height < (int)face->glyph->bitmap.rows) {
            atlas_height = face->glyph->bitmap.rows;
        }

        int bmp_height = face->glyph->bitmap.rows + face->glyph->bitmap_top;
        if (max_bmp_height < bmp_height) {
            max_bmp_height = bmp_height;
        }
    }

    // +1 for the white pixel
    atlas_width = atlas_width + 1;
    int atlas_x = 1;
        
    // Pack glyph bitmaps
    unsigned char *bitmap = (unsigned char *)calloc(atlas_width * atlas_height + 1, 1);
    bitmap[0] = 255;
    for (int i = 32; i < 256; i++) {
        unsigned char c = (unsigned char)i;
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            printf("Error loading char '%c'\n", c);
        }

        Font_Glyph *glyph = &font->glyphs[c];
        glyph->x0 = (f32)atlas_x / atlas_width;
        glyph->y0 = 0.0f;
        glyph->x1 = glyph->x0 + (f32)face->glyph->bitmap.width / atlas_width;
        glyph->y1 = glyph->y0 + (f32)face->glyph->bitmap.rows / atlas_height;
        glyph->size = v2((f32)face->glyph->bitmap.width, (f32)face->glyph->bitmap.rows);
        glyph->off_x = (f32)face->glyph->bitmap_left;
        glyph->off_y = (f32)face->glyph->bitmap_top;
        glyph->advance_x = (f32)(face->glyph->advance.x >> 6);

        // Write glyph bitmap to atlas
        for (int y = 0; y < (int)face->glyph->bitmap.rows; y++) {
            unsigned char *dest = bitmap + y * atlas_width + atlas_x;
            unsigned char *source = face->glyph->bitmap.buffer + y * face->glyph->bitmap.width;
            memcpy(dest, source, face->glyph->bitmap.width);
        }

        // printf("'%c'    advance_x:%f off_x:%f off_y:%f\n", c, glyph->advance_x, glyph->off_x, glyph->off_y);

        atlas_x += face->glyph->bitmap.width;
    }
    FT_Done_Face(face);
    FT_Done_FreeType(ft_lib);

    Texture font_texture{};
    font_texture.size = v2(atlas_width, atlas_height);

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = atlas_width;
    desc.Height = atlas_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    ID3D11Texture2D *font_tex2d = nullptr;
    D3D11_SUBRESOURCE_DATA sr_data{};
    sr_data.pSysMem = bitmap;
    sr_data.SysMemPitch = atlas_width;
    sr_data.SysMemSlicePitch = 0;
    HRESULT hr = d3d_device->CreateTexture2D(&desc, &sr_data, &font_tex2d);
    assert(SUCCEEDED(hr));
    assert(font_tex2d != nullptr);
    hr = d3d_device->CreateShaderResourceView(font_tex2d, nullptr, &font_texture.srv);
    assert(SUCCEEDED(hr));

    font->atlas_size = v2(atlas_width, atlas_height);
    font->max_bmp_height = max_bmp_height;
    font->ascend = ascend;
    font->descend = descend;
    font->bbox_height = height;
    font->glyph_width = glyph_width;
    font->glyph_height = glyph_height;
    font->texture = font_texture;
    return font;
}

inline string string_copy(const string str) {
    string result{};
    result.data = (u8 *)malloc(str.count);
    memcpy(result.data, str.data, str.count);
    result.count = str.count;
    return result;
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;
    switch (message) {
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
        input.scroll_y_dt = (float)delta;
        break;
    }
    case WM_SIZE: {
        // NOTE: Resize render target view
        if (swapchain) {
            d3d_context->OMSetRenderTargets(0, 0, 0);

            // Release all outstanding references to the swap chain's buffers.
            render_target->Release();

            // Preserve the existing buffer count and format.
            // Automatically choose the width and height to match the client rect for HWNDs.
            HRESULT hr = swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
                                            
            // Perform error handling here!

            // Get buffer and create a render-target-view.
            ID3D11Texture2D* buffer;
            hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**) &buffer);
            // Perform error handling here!

            hr = d3d_device->CreateRenderTargetView(buffer, NULL, &render_target);
            // Perform error handling here!
            buffer->Release();

            d3d_context->OMSetRenderTargets(1, &render_target, NULL);
        }
        break;
    }
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    default:
        result = DefWindowProcA(window, message, wparam, lparam);
    }
    return result;
}

internal string read_file(string file_name) {
    string result{};
    HANDLE file_handle = CreateFileA((LPCSTR)file_name.data, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle != INVALID_HANDLE_VALUE) {
        u64 bytes_to_read;
        if (GetFileSizeEx(file_handle, (PLARGE_INTEGER)&bytes_to_read)) {
            assert(bytes_to_read <= UINT32_MAX);
            result.data = (u8 *)malloc(bytes_to_read + 1);
            DWORD bytes_read;
            if (ReadFile(file_handle, result.data, (DWORD)bytes_to_read, &bytes_read, NULL) && (DWORD)bytes_to_read ==  bytes_read) {
                result.count = (u64)bytes_read;
                result.data[result.count] = '\0';
            } else {
                // TODO: error handling
                printf("ReadFile: error reading file, %s!\n", file_name.data);
            }
       } else {
            // TODO: error handling
            printf("GetFileSize: error getting size of file: %s!\n", file_name.data);
       }
       CloseHandle(file_handle);
    } else {
        // TODO: error handling
        printf("CreateFile: error opening file: %s!\n", file_name.data);
    }
    return result;
}

global LARGE_INTEGER performance_frequency;
internal float win32_get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
    float Result = (float)(end.QuadPart - start.QuadPart) / (float)performance_frequency.QuadPart;
    return Result;
}

internal LARGE_INTEGER win32_get_wall_clock() {
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

internal void win32_get_window_size(HWND window, int *w, int *h) {
    RECT rc;
    GetClientRect(window, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    if (w) *w = width;
    if (h) *h = height;
}

internal void upload_constants(ID3D11Buffer *constant_buffer, void *constants, int constants_size) {
    D3D11_MAPPED_SUBRESOURCE res{};
    if (d3d_context->Map(constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res) != S_OK) {
        printf("Failed to map constant buffer\n");
    }
    memcpy(res.pData, constants, constants_size);
    d3d_context->Unmap(constant_buffer, 0);
}

internal Shader_Program create_shader_program(string src, char *vs_entry, char *ps_entry, D3D11_INPUT_ELEMENT_DESC *items, int item_count, int constants_size) {
    Shader_Program program{};
    ID3DBlob *vs_blob, *ps_blob, *error_blob;
    if (D3DCompile(src.data, src.count, nullptr, NULL, NULL, vs_entry, "vs_5_0", D3DCOMPILE_DEBUG, 0, &vs_blob, &error_blob) != S_OK) {
        fprintf(stderr, "Failed to compile Vertex Shader\n%s", (char *)error_blob->GetBufferPointer());
    }
    if (D3DCompile(src.data, src.count, nullptr, NULL, NULL, "PS", "ps_5_0", D3DCOMPILE_DEBUG, 0, &ps_blob, &error_blob) != S_OK) {
        fprintf(stderr, "Failed to compile Pixel Shader\n%s", (char *)error_blob->GetBufferPointer());
    }

    if (d3d_device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), NULL, &program.vertex_shader) != S_OK) {
        fprintf(stderr, "Failed to compile Vertex Shader\n");
    }
    if (d3d_device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), NULL, &program.pixel_shader) != S_OK) {
        fprintf(stderr, "Failed to create Pixel Shader\n");
    }

    ID3D11InputLayout *input_layout = nullptr;
    if (d3d_device->CreateInputLayout(items, item_count, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &program.input_layout) != S_OK) {
        fprintf(stderr, "Failed to create Input Layout\n");
    }

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = constants_size;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (d3d_device->CreateBuffer(&desc, NULL, &program.constant_buffer) != S_OK) {
        fprintf(stderr, "Failed to create constant buffer\n");
    }

    return program;
}


internal void ui_push_vertex(UI_Vertex v) {
    ui_draw_data.vertices.push(v);
}

internal void ui_draw_rect(UI_Rect rect, v4 color) {
    f32 x0 = rect.x, y0 = rect.y;
    f32 x1 = rect.x + rect.width;
    f32 y1 = rect.y + rect.height;

    UI_Vertex v[4] = {};
    v[0].p = v2(x0, y0);
    v[0].col = color;
    v[1].p = v2(x0, y1);
    v[1].col = color;
    v[2].p = v2(x1, y1);
    v[2].col = color;
    v[3].p = v2(x1, y0);
    v[3].col = color;

    ui_push_vertex(v[0]);
    ui_push_vertex(v[1]);
    ui_push_vertex(v[2]);
    ui_push_vertex(v[0]);
    ui_push_vertex(v[2]);
    ui_push_vertex(v[3]);
}

internal void ui_draw_rect_outline(UI_Rect rect, f32 thickness, v4 color) {
    f32 x0 = rect.x, y0 = rect.y;
    f32 x1 = rect.x + rect.width;
    f32 y1 = rect.y + rect.height;

    ui_draw_rect(UI_Rect(x0, y0, rect.width, thickness), color);
    ui_draw_rect(UI_Rect(x0, y1 - thickness, rect.width, thickness), color);
    ui_draw_rect(UI_Rect(x0, y0, thickness, rect.height), color);
    ui_draw_rect(UI_Rect(x1 - thickness, y0, thickness, rect.height), color);
}

internal void ui_draw_glyph(v2 p, u8 c, v4 color, Font *font) {
    assert(c < 256);
    Font_Glyph g = font->glyphs[c];
    f32 x0 = p.x + g.off_x;
    f32 x1 = x0 + g.size.x;
    f32 y0 = p.y - g.off_y + font->ascend;
    f32 y1 = y0 + g.off_y;

    UI_Vertex v[4] = {};
    v[0].p = v2(x0, y1);
    v[0].col = color;
    v[0].uv = v2(g.x0, g.y1);
    v[1].p = v2(x0, y0);
    v[1].col = color;
    v[1].uv = v2(g.x0, g.y0);
    v[2].p = v2(x1, y0);
    v[2].col = color;
    v[2].uv = v2(g.x1, g.y0);
    v[3].p = v2(x1, y1);
    v[3].col = color;
    v[3].uv = v2(g.x1, g.y1);

    ui_push_vertex(v[0]);
    ui_push_vertex(v[1]);
    ui_push_vertex(v[2]);
    ui_push_vertex(v[0]);
    ui_push_vertex(v[2]);
    ui_push_vertex(v[3]);
}

internal void ui_draw_text(v2 p, string text, Font *font, v4 color) {
    v2 pos = p;
    for (u64 i = 0; i < text.count; i++) {
        u8 c = text.data[i];
        Font_Glyph g = font->glyphs[c];
        ui_draw_glyph(pos, c, color, font);
        pos.x += g.advance_x;
    }
}

internal void ui_draw_root(UI_Box *root) {
    if (root->flags & UI_BoxFlag_DrawBackground) {
        ui_draw_rect(UI_Rect(root->position, root->size), root->bg_color);
    }

    if (root->flags & UI_BoxFlag_DrawBorder) {
        ui_draw_rect_outline(UI_Rect(root->position, root->size), 1.0f, root->border_color);
    }

    if (root->flags & UI_BoxFlag_DrawText) {
        v2 p = root->position;
        p.x += 4.0f;
        ui_draw_text(p, root->text, ui_state.font, root->text_color);
    }

    for (UI_Box *child = root->first; child; child = child->next) {
        ui_draw_root(child);
    }
}

internal void ui_draw() {
    for (int i = 0; i < ui_state.parents.count; i++) {
        UI_Box *box = ui_state.parents[i];
        ui_draw_root(box);
    }

    // (Re)create vertex buffer
    if ((!ui_draw_data.vertex_buffer && ui_draw_data.vertices.count > 0) ||
        ui_draw_data.vertices.count > ui_draw_data.vertex_buffer_size) {
        if (ui_draw_data.vertex_buffer) {
            ui_draw_data.vertex_buffer->Release();
        }

        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = (UINT)ui_draw_data.vertices.capacity * sizeof(UI_Vertex);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (d3d_device->CreateBuffer(&desc, nullptr, &ui_draw_data.vertex_buffer) != S_OK) {
            fprintf(stderr, "Failed to create UI vertex buffer\n");
        }
        ui_draw_data.vertex_buffer_size = (int)ui_draw_data.vertices.capacity;
    }

    // Upload vertices
    if (ui_draw_data.vertex_buffer) {
        D3D11_MAPPED_SUBRESOURCE vertex_res{};
        if (d3d_context->Map(ui_draw_data.vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &vertex_res) != S_OK) {
            fprintf(stderr, "Failed to Map vertex buffer");
        }
        memcpy(vertex_res.pData, ui_draw_data.vertices.data, ui_draw_data.vertices.count * sizeof(UI_Vertex));
        d3d_context->Unmap(ui_draw_data.vertex_buffer, 0);
    }
}

int main(int argc, char **argv) {
    // hash_tests();
    argc--; argv++;

    string file_arg = "";

    if (argc > 0) {
        file_arg = argv[0];
    } else {
        printf("Focal\n");
        printf("Usage: focal.exe FILE\n");
        exit(0);
    }

    QueryPerformanceFrequency(&performance_frequency);
    timeBeginPeriod(1);
    UINT desired_scheduler_ms = 1;
    timeBeginPeriod(desired_scheduler_ms);
    DWORD target_frames_per_second = 60;
    DWORD target_ms_per_frame = (int)(1000.0f / target_frames_per_second);

    HRESULT hr = 0;
#define CLASSNAME "focal_hwnd_class"
    HINSTANCE hinstance = GetModuleHandle(NULL);
    WNDCLASSA window_class{};
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = window_proc;
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszClassName = CLASSNAME;
    window_class.hInstance = hinstance;
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    if (!RegisterClassA(&window_class)) {
        os_popupf("RegisterClassA failed, err:%d\n", GetLastError());
    }

    HWND window = 0;
    {
        RECT rc = {0, 0, WIDTH, HEIGHT};
        if (AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE)) {
            // NOTE: Center window
            RECT work_rc;
            SystemParametersInfo(SPI_GETWORKAREA, 0, &work_rc, 0);
            int x, y;
            x = ((work_rc.right - work_rc.left) - rc.right) / 2;
            y = ((work_rc.bottom - work_rc.top) - rc.bottom) / 2;
            window = CreateWindowA(CLASSNAME, "Focal", WS_OVERLAPPEDWINDOW | WS_VISIBLE, x, y, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hinstance, NULL);
        } else {
            fprintf(stderr, "AdjustWindowRect failed, err:%d\n", GetLastError());
        }
        if (!window) {
            os_popupf("CreateWindowA failed, err:%d\n", GetLastError());
        }
    }



    // INITIALIZE SWAP CHAIN
    DXGI_SWAP_CHAIN_DESC swapchain_desc{};
    {
        DXGI_MODE_DESC buffer_desc{};
        buffer_desc.Width = WIDTH;
        buffer_desc.Height = HEIGHT;
        buffer_desc.RefreshRate.Numerator = 60;
        buffer_desc.RefreshRate.Denominator = 1;
        buffer_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        buffer_desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        buffer_desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

        swapchain_desc.BufferDesc = buffer_desc;
        swapchain_desc.SampleDesc.Count = 1;
        swapchain_desc.SampleDesc.Quality = 0;
        swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchain_desc.BufferCount = 1;
        swapchain_desc.OutputWindow = window;
        swapchain_desc.Windowed = TRUE;
        swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    }

    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, NULL, NULL, D3D11_SDK_VERSION, &swapchain_desc, &swapchain, &d3d_device, NULL, &d3d_context);

    ID3D11Texture2D *backbuffer;
    hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backbuffer);
    
    hr = d3d_device->CreateRenderTargetView(backbuffer, NULL, &render_target);
    backbuffer->Release();

    ID3D11DepthStencilView *depth_stencil_view = nullptr;

    // DEPTH STENCIL BUFFER
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = WIDTH;
        desc.Height = HEIGHT;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        ID3D11Texture2D *depth_stencil_buffer = nullptr;
        hr = d3d_device->CreateTexture2D(&desc, NULL, &depth_stencil_buffer);
        hr = d3d_device->CreateDepthStencilView(depth_stencil_buffer, NULL, &depth_stencil_view);
    }

    // RASTERIZER STATE
    ID3D11RasterizerState *rasterizer_state = nullptr;
    {
        D3D11_RASTERIZER_DESC desc{};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.ScissorEnable = true;
        desc.DepthClipEnable = true;
        d3d_device->CreateRasterizerState(&desc, &rasterizer_state);
    }

    // DEPTH-STENCIL STATE
    ID3D11DepthStencilState *depth_stencil_state = nullptr;
    {
        D3D11_DEPTH_STENCIL_DESC desc{};
        desc.DepthEnable = true;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        desc.StencilEnable = false;
        desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        desc.BackFace = desc.FrontFace;
        d3d_device->CreateDepthStencilState(&desc, &depth_stencil_state);
    }

    // BLEND STATE
    ID3D11BlendState *blend_state = nullptr;
    {
        D3D11_BLEND_DESC desc{};
        desc.AlphaToCoverageEnable = false;
        desc.IndependentBlendEnable = false;
        desc.RenderTarget[0].BlendEnable = true;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (d3d_device->CreateBlendState(&desc, &blend_state) != S_OK) {
            printf("Error CreateBlendState\n");
        }
    }

    string grid_shader = read_file("src/grid.hlsl");
    D3D11_INPUT_ELEMENT_DESC grid_layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    Shader_Program grid_program = create_shader_program(grid_shader, "VS", "PS", grid_layout, ARRAYSIZE(grid_layout), sizeof(Grid_Constants));

    string image_shader = read_file("src/image.hlsl");
    D3D11_INPUT_ELEMENT_DESC image_layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(Image_Vertex, p), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(Image_Vertex, uv),       D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    Shader_Program image_program = create_shader_program(image_shader, "VS", "PS", image_layout, ARRAYSIZE(image_layout), sizeof(Image_Constants));

    string ui_shader = read_file("src/ui.hlsl");
    D3D11_INPUT_ELEMENT_DESC ui_layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(UI_Vertex, p), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(UI_Vertex, uv),       D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(UI_Vertex, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    Shader_Program ui_program = create_shader_program(ui_shader, "VS", "PS", ui_layout, ARRAYSIZE(ui_layout), sizeof(UI_Constants));

    ID3D11SamplerState *linear_sampler = nullptr;
    ID3D11SamplerState *nearest_sampler = nullptr;
    {
        D3D11_SAMPLER_DESC desc{};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD = 0;
        desc.MaxLOD = D3D11_FLOAT32_MAX;
        if (d3d_device->CreateSamplerState(&desc, &nearest_sampler) != S_OK) {
            printf("Failed to create sampler\n");
        }
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        if (d3d_device->CreateSamplerState(&desc, &linear_sampler) != S_OK) {
            printf("Failed to create sampler\n");
        }
    }

    Texture sample_texture{};
    {
        bool success = true;
        ID3D11ShaderResourceView *srv = nullptr;
        string file_name = file_arg;
        int width, height, nc;
        u8 *image_data = stbi_load((char *)file_name.data, &width, &height, &nc, 4);
        if (!image_data) {
            success = false;
        }
        ID3D11Texture2D *texture = nullptr;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = image_data;
        data.SysMemPitch = width * sizeof(u32);
        data.SysMemSlicePitch = 0;
        if (d3d_device->CreateTexture2D(&desc, &data, &texture) == S_OK) {
            if (d3d_device->CreateShaderResourceView(texture, NULL, &srv) == S_OK) {
            } else {
                success = false;
            }
        } else {
            success = false;
        }
        
        if (image_data) {
            stbi_image_free(image_data);
        }
        if (!success) {
            printf("Error creating texture '%s'\n", (char *)file_name.data);
        }
        sample_texture.size = v2(width, height);
        sample_texture.name = string_copy(file_name);
        sample_texture.srv = srv;
    }

    Image_Vertex vertices[] = {
        { v2(-0.5f, -0.5f), v2(0, 1) },
        { v2(-0.5f,  0.5f), v2(0, 0) },
        { v2( 0.5f,  0.5f), v2(1, 0) },
        { v2(-0.5f, -0.5f), v2(0, 1) },
        { v2( 0.5f,  0.5f), v2(1, 0) },
        { v2( 0.5f, -0.5f), v2(1, 1) },
    };

    ID3D11Buffer *image_vertex_buffer = nullptr;
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(Image_Vertex) * 6;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = vertices;
        if (d3d_device->CreateBuffer(&desc, &data, &image_vertex_buffer) != S_OK) {
            printf("Failed to create image vertex buffer\n");
        }
    }

    v2 grid_vertices[] = {
        v2(-1, -1),
        v2(-1, 1),
        v2(1, 1),
        v2(-1, -1),
        v2(1, 1),
        v2(1, -1)
    };

    ID3D11Buffer *grid_vertex_buffer = nullptr;
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(grid_vertices);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = grid_vertices;
        if (d3d_device->CreateBuffer(&desc, &data, &grid_vertex_buffer) != S_OK) {
            printf("Failed to create grid vertex buffer\n");
        }
    }

    ui_state.font = load_font("data/Roboto-Regular.ttf", 16);

    ui_state.builds[0].reserve(100);
    ui_state.builds[1].reserve(100);

    ID3D11SamplerState *font_sampler = nearest_sampler;

    Image_Constants image_constants{};
    Grid_Constants grid_constants{};
    UI_Constants ui_constants{};

    bool sample_near = false;

    LARGE_INTEGER start_counter = win32_get_wall_clock();
    LARGE_INTEGER last_counter = start_counter;

    v2 position{};

    bool window_should_close = false;
    while (!window_should_close) {
        MSG message{};
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                window_should_close = true;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        int width, height;
        win32_get_window_size(window, &width, &height);
        v2 render_dim = v2(width, height);

        v2 window_center = v2_div_s(render_dim, 2.0f);

        ui_draw_data.vertices.reset_count();
        ui_start_build();

        // ================
        // INPUT
        // ================
        BYTE keyboard_bytes[256];
        if (GetKeyboardState(keyboard_bytes)) {
            for (int key = 0; key < 256; key++) {
                BYTE byte = keyboard_bytes[key];
                b8 down = (byte >> 7);
                input.keys_pressed[key] = input.keys_down[key] && !down;
                input.keys_down[key] = down;
            }
        }
        POINT pt;
        if (GetCursorPos(&pt)) {
            if (ScreenToClient(window, &pt)) {
                v2 m = v2(pt.x, height - pt.y);
                input.last_mouse = input.mouse;
                input.mouse = m;
            }
        }

        ui_state.mouse = v2(input.mouse.x, height - input.mouse.y);
        ui_state.mouse_down = input.keys_down[VK_LBUTTON];
        ui_state.mouse_pressed = input.keys_pressed[VK_LBUTTON];

        UI_Box *menu = ui_build_box_from_string("menu", UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorder);
        menu->sem_position[UI_AxisX] = { UI_Position_Absolute, 100.0f};
        menu->sem_position[UI_AxisY] = { UI_Position_Absolute, render_dim.y - 60.0f};
        menu->sem_size[UI_AxisX] = { UI_Size_ChildrenSum, 1.0f};
        menu->sem_size[UI_AxisY] = { UI_Size_ChildrenSum, 1.0f};
        menu->bg_color = v4(0.17f, 0.17f, 0.17f, 1.0f);
        menu->border_color = v4(0.1f, 0.1f, 0.1f, 1.0f);
        menu->child_layout_axis = UI_AxisY;

        ui_push_parent(menu);

        UI_Box *zoom_button = ui_button("Zoom");
        ui_push_box(zoom_button, menu);

        UI_Box *prev_next_bar = ui_build_box_from_string("prev_next_bar", UI_BoxFlag_Nil);
        prev_next_bar->sem_size[UI_AxisX] = { UI_Size_ChildrenSum, 1.0f };
        prev_next_bar->sem_size[UI_AxisY] = { UI_Size_ChildrenSum, 1.0f };
        prev_next_bar->child_layout_axis = UI_AxisX;
        ui_push_box(prev_next_bar, menu);

        UI_Box *prev_button = ui_button("<< Prev");
        ui_push_box(prev_button, prev_next_bar);
        UI_Box *next_button = ui_button("Next >>");
        ui_push_box(next_button, prev_next_bar);

        UI_Box *file_index_button = ui_button("File");
        ui_push_box(file_index_button, menu);
        
        // ===============
        // UPDATE
        // ===============
        if (input.key_down(VK_ESCAPE)) {
            window_should_close = true;
        }

        f32 speed = 10.0f;
        if (input.key_down('A')) {
            position.x += speed;
        }
        if (input.key_down('D')) {
            position.x -= speed;
        }
        if (input.key_down('W')) {
            position.y -= speed;
        }
        if (input.key_down('S')) {
            position.y += speed;
        }

        if (input.key_down('R')) {
            zoom = 1.0f;
        }

        if (input.key_press('L')) {
            sample_near = !sample_near;
        }

        if (input.key_down(VK_LBUTTON)) {
            v2 mouse_dt = v2_subtract(input.mouse, input.last_mouse);
            position = v2_add(position, mouse_dt);
        }

        float last_zoom = zoom;
#define ZOOM_FACTOR 0.1f
        zoom *= 1.0f + input.scroll_y_dt * ZOOM_FACTOR;

        v2 mouse = v2_subtract(input.mouse, window_center);
        position = v2_subtract(position, mouse);
        position = v2_mul_s(position, zoom / last_zoom);
        position = v2_add(position, mouse);

        input.scroll_y_dt = 0.0f;

        
        // ==================
        // Upload GPU data
        // ==================

        m4 projection{};
        projection.e[0][0] = 2.0f / width;
        projection.e[1][1] = 2.0f / height;
        projection.e[2][2] = 0.0f;
        projection.e[3][3] = 1.0f;
        projection.e[3][0] = -1.0f;
        projection.e[3][1] = -1.0f;
        projection.e[3][2] = 0.0f;
          
        m4 camera_trans = m4_translate(v3(position.x, position.y, 0.0f));
        m4 camera_scale = m4_scale(v3(zoom, zoom, 1.0f));

        m4 trans = m4_translate(v3(window_center.x, window_center.y, 0.0f));
        m4 scale = m4_scale(v3(sample_texture.size.x, sample_texture.size.y, 1.0f));
        m4 mvp = m4_id();
        mvp = m4_mult(mvp, camera_scale);
        mvp = m4_mult(mvp, scale);
        mvp = m4_mult(mvp, camera_trans);
        mvp = m4_mult(mvp, trans);
        mvp = m4_mult(mvp, projection);
        
        image_constants.mvp = mvp;
        image_constants.channels = v4(1.0f, 1.0f, 1.0f, 1.0f);
        upload_constants(image_program.constant_buffer, (void *)&image_constants, sizeof(image_constants));

        grid_constants.c1 = v4(0.2f, 0.2f, 0.2f, 1.0f);
        grid_constants.c2 = v4(0.1f, 0.1f, 0.1f, 1.0f);
        grid_constants.size = 64.0f;
        grid_constants.cp = window_center;
        upload_constants(grid_program.constant_buffer, (void *)&grid_constants, sizeof(grid_constants));

        // NOTE: Create orthographic projection matrix and upload to constant buffer
        {
            float left = 0.0f;
            float right = left + render_dim.x;
            float top = 0.0f;
            float bottom = top + render_dim.y;
            float near = -1.0f;
            float far = 1.0f;
            m4 prj = m4_id();
            prj.e[0][0] = 2.0f / (right - left);
            prj.e[1][1] = 2.0f / (top - bottom);
            prj.e[2][2] = 1.0f / (near - far);
            prj.e[3][3] = 1.0f;
            prj.e[3][0] = (left + right) / (left - right);
            prj.e[3][1] = (bottom + top) / (bottom - top);
            prj.e[3][2] = 0.0f;
            ui_constants.projection = prj;
        }
        upload_constants(ui_program.constant_buffer, (void *)&ui_constants, sizeof(ui_constants));

        // render UI
        ui_end_build();
        ui_draw();

        // =================
        // RENDER
        // =================
        float bg_color[4] = {0.08f, 0.08f, 0.08f, 0};
        d3d_context->ClearRenderTargetView(render_target, bg_color);
        d3d_context->ClearDepthStencilView(depth_stencil_view, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);
        d3d_context->OMSetRenderTargets(1, &render_target, depth_stencil_view);

        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = (float)width;
        viewport.Height = (float)height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        d3d_context->RSSetViewports(1, &viewport);

        d3d_context->RSSetState(rasterizer_state);

        d3d_context->OMSetBlendState(blend_state, NULL, 0xffffffff);
        d3d_context->OMSetDepthStencilState(depth_stencil_state, 0);

        if (sample_near) {
            d3d_context->PSSetSamplers(0, 1, &nearest_sampler);
        } else {
            d3d_context->PSSetSamplers(0, 1, &linear_sampler);
        }

        // Render grid
        d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3d_context->IASetInputLayout(grid_program.input_layout);

        d3d_context->VSSetConstantBuffers(0, 1, &grid_program.constant_buffer);
        d3d_context->PSSetConstantBuffers(0, 1, &grid_program.constant_buffer);
        d3d_context->VSSetShader(grid_program.vertex_shader, NULL, 0);
        d3d_context->PSSetShader(grid_program.pixel_shader, NULL, 0);

        UINT stride = sizeof(v2);
        UINT offset = 0;
        d3d_context->IASetVertexBuffers(0, 1, &grid_vertex_buffer, &stride, &offset);

        d3d_context->Draw(6, 0);

        // Render image
        d3d_context->VSSetConstantBuffers(0, 1, &image_program.constant_buffer);
        d3d_context->PSSetConstantBuffers(0, 1, &image_program.constant_buffer);
        d3d_context->VSSetShader(image_program.vertex_shader, NULL, 0);
        d3d_context->PSSetShader(image_program.pixel_shader, NULL, 0);

        d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3d_context->IASetInputLayout(image_program.input_layout);

        stride = sizeof(Image_Vertex);
        offset = 0;
        d3d_context->IASetVertexBuffers(0, 1, &image_vertex_buffer, &stride, &offset);

        d3d_context->PSSetShaderResources(0, 1, &sample_texture.srv);
        
        d3d_context->Draw(6, 0);

        // Render UI
        d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3d_context->IASetInputLayout(ui_program.input_layout);

        d3d_context->VSSetConstantBuffers(0, 1, &ui_program.constant_buffer);
        d3d_context->VSSetShader(ui_program.vertex_shader, NULL, 0);

        d3d_context->PSSetConstantBuffers(0, 1, &ui_program.constant_buffer);
        d3d_context->PSSetShader(ui_program.pixel_shader, NULL, 0);
        d3d_context->PSSetSamplers(0, 1, &font_sampler);
        d3d_context->PSSetShaderResources(0, 1, &ui_state.font->texture.srv);

        stride = sizeof(UI_Vertex);
        offset = 0;
        d3d_context->IASetVertexBuffers(0, 1, &ui_draw_data.vertex_buffer, &stride, &offset);

        d3d_context->Draw((UINT)ui_draw_data.vertices.count, 0);

        swapchain->Present(0, 0);

        float work_seconds_elapsed = win32_get_seconds_elapsed(last_counter, win32_get_wall_clock());
        DWORD work_ms = (DWORD)(1000.0f * work_seconds_elapsed);
        if (work_ms < target_ms_per_frame) {
            DWORD sleep_ms = target_ms_per_frame - work_ms;
            Sleep(sleep_ms);
        }

        LARGE_INTEGER end_counter = win32_get_wall_clock();
#if 0
        float seconds_elapsed = 1000.0f * win32_get_seconds_elapsed(last_counter, end_counter);
        printf("seconds: %f\n", seconds_elapsed);
#endif
        last_counter = end_counter;
    }
    
    return 0;
}
