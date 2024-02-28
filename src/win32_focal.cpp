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

#include <string.h>
struct string {
    u8 *data;
    u64 count;
    string() : data(NULL), count(0) {}
    string(const char *cstr) : data((u8 *)cstr), count(strlen(cstr)) {}
    string(char *str, u64 len) : data((u8 *)str), count(len) {}
};
#define STRZ(Str) string(Str, strlen(Str))

#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4244)
#include <stb_image.h>
#pragma warning(pop)

#include "focal_math.h"
#include "focal.h"

#define WIDTH 1280
#define HEIGHT 720

global IDXGISwapChain *swapchain;
global ID3D11Device *d3d_device;
global ID3D11DeviceContext *d3d_context;
global ID3D11RenderTargetView *render_target;

global Input_State input;

global f32 zoom = 1.0f;


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
        input.scroll_y += delta;
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

int main(int argc, char **argv) {
    argc--;
    argv++;

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
        printf("RegisterClassA failed, err:%d\n", GetLastError());
    }

    HWND window = 0;
    {
        RECT rc = {0, 0, WIDTH, HEIGHT};
        if (AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE)) {
            window = CreateWindowA(CLASSNAME, "Focal", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hinstance, NULL);
        } else {
            printf("AdjustWindowRect failed, err:%d\n", GetLastError());
        }
        if (!window) {
            printf("CreateWindowA failed, err:%d\n", GetLastError());
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
        desc.DepthEnable = false;
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

    // COMPILE IMAGE SHADER
    string image_shader_src = read_file("src/image.hlsl");
    ID3DBlob *vs_blob, *ps_blob, *error_blob;
    if (D3DCompile(image_shader_src.data, image_shader_src.count, "image.hlsl", NULL, NULL, "VS", "vs_5_0", D3DCOMPILE_DEBUG, 0, &vs_blob, &error_blob) != S_OK) {
        printf("Failed to compile vertex shader 'image.hlsl'\n");
        printf("%s\n", (char *)error_blob->GetBufferPointer());
    }
    if (D3DCompile(image_shader_src.data, image_shader_src.count, "image.hlsl", NULL, NULL, "PS", "ps_5_0", D3DCOMPILE_DEBUG, 0, &ps_blob, &error_blob) != S_OK) {
        printf("Failed to compile pixel shader 'image.hlsl'\n");
        printf("%s\n", (char *)error_blob->GetBufferPointer());
    }

    ID3D11VertexShader *image_vertex_shader = nullptr;
    ID3D11PixelShader  *image_pixel_shader = nullptr;
    {
        if (d3d_device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), NULL, &image_vertex_shader) != S_OK) {
            printf("Failed to create vertex shader 'image.hlsl'");
        }
        if (d3d_device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), NULL, &image_pixel_shader) != S_OK) {
            printf("Failed to create pixel shader 'image.hlsl'");
        }
    }

    ID3D11InputLayout *image_input_layout = nullptr;
    D3D11_INPUT_ELEMENT_DESC image_layout_items[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(Image_Vertex, p), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(Image_Vertex, uv),       D3D11_INPUT_PER_VERTEX_DATA, 0 },
        // { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Image_Vertex, color),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = d3d_device->CreateInputLayout(image_layout_items, ARRAYSIZE(image_layout_items), vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &image_input_layout);
    if (hr != S_OK) {
        printf("Failed to create input layout\n");
    }


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
    v2 size = sample_texture.size;

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
            printf("Failed to create sprite vertex buffer\n");
        }
    }

    ID3D11Buffer *image_constant_buffer = nullptr;
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(Image_Constants);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (d3d_device->CreateBuffer(&desc, NULL, &image_constant_buffer) != S_OK) {
            printf("Failed to create constant buffer\n");
        }
    }

    Image_Constants image_constants{};

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
                input.mouse = m;
            }
        }

        if (input.key_down(VK_ESCAPE)) {
            window_should_close = true;
        }

        f32 speed = 10.0f;
        if (input.key_down(VK_LEFT)) {
            position.x -= speed;
        }
        if (input.key_down(VK_RIGHT)) {
            position.x += speed;
        }
        if (input.key_down(VK_UP)) {
            position.y += speed;
        }
        if (input.key_down(VK_DOWN)) {
            position.y -= speed;
        }

        if (input.key_down('R')) {
            zoom = 1.0f;
        }

        if (input.key_press('L')) {
            sample_near = !sample_near;
        }

        float last_zoom = zoom;
#define ZOOM_FACTOR 0.1f
        zoom *= 1.0f + input.scroll_y_dt * ZOOM_FACTOR;

        v2 mouse = v2_subtract(input.mouse, window_center);
        position = v2_subtract(position, mouse);
        position = v2_mul_s(position, zoom / last_zoom);
        position = v2_add(position, mouse);

        input.scroll_y_dt = 0.0f;

        m4 projection{};
        projection.e[0][0] = 2.0f / width;
        projection.e[1][1] = 2.0f / height;
        projection.e[2][2] = 0.0f;
        projection.e[3][3] = 1.0f;
        projection.e[3][0] = -1.0f;
        projection.e[3][1] = -1.0f;
        projection.e[3][2] = 0.5f;
          
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
        
        float bg_color[4] = {0, 0, 0, 0};
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

        d3d_context->OMSetBlendState(blend_state, NULL, 0xffffffff);
        d3d_context->OMSetDepthStencilState(depth_stencil_state, 0);

        if (sample_near) {
            d3d_context->PSSetSamplers(0, 1, &nearest_sampler);
        } else {
            d3d_context->PSSetSamplers(0, 1, &linear_sampler);
        }

        d3d_context->VSSetConstantBuffers(0, 1, &image_constant_buffer);
        d3d_context->VSSetShader(image_vertex_shader, NULL, 0);
        d3d_context->PSSetShader(image_pixel_shader, NULL, 0);

        d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3d_context->IASetInputLayout(image_input_layout);

        UINT stride = sizeof(Image_Vertex);
        UINT offset = 0;
        d3d_context->IASetVertexBuffers(0, 1, &image_vertex_buffer, &stride, &offset);

        d3d_context->PSSetShaderResources(0, 1, &sample_texture.srv);

        // Upload image constants
        {
            D3D11_MAPPED_SUBRESOURCE res{};
            if (d3d_context->Map(image_constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res) != S_OK) {
                printf("Failed to map constant buffer\n");
            }
            memcpy(res.pData, &image_constants, sizeof(image_constants));
            d3d_context->Unmap(image_constant_buffer, 0);
        }
        d3d_context->Draw(6, 0);

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
