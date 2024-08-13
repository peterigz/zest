#include "zest.h"
#define ZLOC_IMPLEMENTATION
//#define ZLOC_OUTPUT_ERROR_MESSAGES
//#define ZLOC_EXTRA_DEBUGGING
#define STB_RECT_PACK_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "lib_bundle.h"

zest_device_t* ZestDevice = 0;
zest_app_t* ZestApp = 0;
zest_renderer_t* ZestRenderer = 0;
zest_hasher* ZestHasher = 0;

#ifdef _WIN32
zest_millisecs zest_Millisecs(void) {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    zest_ull ms = (zest_ull)(counter.QuadPart * 1000LL / frequency.QuadPart);
    return (zest_millisecs)ms;
}

zest_microsecs zest_Microsecs(void) {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    zest_ull us = (zest_ull)(counter.QuadPart * 1000000LL / frequency.QuadPart);
    return (zest_microsecs)us;
}
#elif defined(__APPLE__)
#include <mach/mach_time.h>
zest_millisecs zest_Millisecs(void) {
    static mach_timebase_info_data_t timebase_info;
    if (timebase_info.denom == 0) {
        mach_timebase_info(&timebase_info);
    }

    uint64_t time_ns = mach_absolute_time() * timebase_info.numer / timebase_info.denom;
    zest_millisecs ms = (zest_millisecs)(time_ns / 1000000);
    return (zest_millisecs)ms;
}

zest_microsecs zest_Microsecs(void) {
    static mach_timebase_info_data_t timebase_info;
    if (timebase_info.denom == 0) {
        mach_timebase_info(&timebase_info);
    }

    uint64_t time_ns = mach_absolute_time() * timebase_info.numer / timebase_info.denom;
    zest_microsecs us = (zest_microsecs)(time_ns / 1000);
    return us;
}
#else
zest_millisecs zest_Millisecs(void) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    long m = now.tv_sec * 1000 + now.tv_nsec / 1000000;
    return (zest_millisecs)m;
}

zest_microsecs zest_Microsecs(void) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    zest_ull us = now.tv_sec * 1000000ULL + now.tv_nsec / 1000;
    return (zest_microsecs)us;
}
#endif

#ifdef _WIN32
FILE* zest__open_file(const char* file_name, const char* mode) {
    FILE* file = NULL;
    errno_t err = fopen_s(&file, file_name, mode);
    if (err != 0 || file == NULL) {
        return NULL;
    }
    return file;
}

void zest__destroy_window_callback(void* user_data) {
    DestroyWindow(ZestApp->window->window_handle);
    PostQuitMessage(0);
}

LRESULT CALLBACK zest__window_proc(HWND window_handle, UINT message, WPARAM wParam, LPARAM lParam) {
    LRESULT result = 0;
    switch (message) {
    case WM_CLOSE:
    case WM_QUIT: {
        ZestApp->flags |= zest_app_flag_quit_application;
    } break;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC DeviceContext = BeginPaint(window_handle, &paint);
        EndPaint(window_handle, &paint);
    } break;
    case WM_LBUTTONDOWN: {
        ZestApp->mouse_button = 1;
    } break;
    case WM_RBUTTONDOWN: {
        ZestApp->mouse_button = 2;
    } break;
    case WM_SIZE: {
    } break;
    case WM_DESTROY: {
    } break;
    default: {
        result = DefWindowProc(window_handle, message, wParam, lParam);
        break;
    }
    }

    return result;
}

void zest__os_poll_events() {
    MSG message = { 0 };

    POINT cursor_position;
    GetCursorPos(&cursor_position);
    ScreenToClient(ZestApp->window->window_handle, &cursor_position);
    double last_mouse_x = ZestApp->mouse_x;
    double last_mouse_y = ZestApp->mouse_y;
    ZestApp->mouse_x = (double)cursor_position.x;
    ZestApp->mouse_y = (double)cursor_position.y;
    ZestApp->mouse_delta_x = last_mouse_x - ZestApp->mouse_x;
    ZestApp->mouse_delta_y = last_mouse_y - ZestApp->mouse_y;
    ZestApp->mouse_button = 0;

    for (;;) {
        BOOL result = 0;
        DWORD SkipMessages[] = { 0x738, 0xFFFFFFFF };
        DWORD LastMessage = 0;
        for (zest_uint SkipIndex = 0; SkipIndex < 2; ++SkipIndex) {

            DWORD Skip = SkipMessages[SkipIndex];
            result = PeekMessage(&message, 0, LastMessage, Skip - 1, PM_REMOVE);
            if (result)
            {
                TranslateMessage(&message);
                DispatchMessage(&message);
                break;
            }

            LastMessage = Skip + 1;
        }

        if (!result) {
            break;
        }
    }

}

zest_window zest__os_create_window(int x, int y, int width, int height, zest_bool maximised, const char* title) {
    ZEST_ASSERT(ZestDevice);        //Must initialise the ZestDevice first

    zest_window_instance = GetModuleHandle(NULL);

    zest_window window = ZEST__NEW(zest_window);
    WNDCLASS window_class = { 0 };
    memset(window, 0, sizeof(zest_window_t));

    window->window_width = width;
    window->window_height = height;

    window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = zest__window_proc;
    window_class.hInstance = zest_window_instance;
    window_class.hIcon = LoadIcon(zest_window_instance, MAKEINTRESOURCE(IDI_APPLICATION));
    window_class.lpszClassName = "zest_app_class_name";

    if (!RegisterClass(&window_class)) {
        ZEST_ASSERT(0);        //Failed to register window
    }

    DWORD style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    style |= WS_SYSMENU | WS_MINIMIZEBOX;
    style |= WS_CAPTION;
    style |= WS_MAXIMIZEBOX | WS_THICKFRAME;

    RECT rect = { 0, 0, width, height };

    AdjustWindowRectEx(&rect, style, FALSE, 0);

    int frame_width = rect.right - rect.left;
    int frame_height = rect.bottom - rect.top;

    window->window_handle = CreateWindowEx(0, window_class.lpszClassName, title, style | WS_VISIBLE, x, y, frame_width, frame_height, 0, 0, zest_window_instance, 0);
    ZEST_ASSERT(window->window_handle);        //Unable to open a window!

    SetForegroundWindow(window->window_handle);
    SetFocus(window->window_handle);

    return window;
}

void zest__os_create_window_surface(zest_window window) {
    VkWin32SurfaceCreateInfoKHR surface_create_info;
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.pNext = NULL;
    surface_create_info.flags = 0;
    surface_create_info.hinstance = zest_window_instance;
    surface_create_info.hwnd = window->window_handle;
    ZEST_VK_CHECK_RESULT(vkCreateWin32SurfaceKHR(ZestDevice->instance, &surface_create_info, &ZestDevice->allocation_callbacks, &window->surface));
}

void zest__os_add_platform_extensions() {
    zest_AddInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME);
    zest_AddInstanceExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
}

void zest__get_window_size_callback(void* user_data, int* fb_width, int* fb_height, int* window_width, int* window_height) {
    RECT window_rect;
    GetClientRect(ZestApp->window->window_handle, &window_rect);
    *fb_width = window_rect.right - window_rect.left;
    *fb_height = window_rect.bottom - window_rect.top;
    *window_width = *fb_width;
    *window_height = *fb_height;
}

void zest__os_set_window_title(const char* title) {
    SetWindowText(ZestApp->window->window_handle, title);
}

#else

FILE* zest__open_file(const char* file_name, const char* mode) {
    return fopen(file_name, mode);
}

zest_window zest__os_create_window(int x, int y, int width, int height, zest_bool maximised, const char* title) {
    ZEST_ASSERT(ZestDevice);        //Must initialise the ZestDevice first

    zest_window window = ZEST__NEW(zest_window);
    memset(window, 0, sizeof(zest_window_t));
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    if (maximised)
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    window->window_width = width;
    window->window_height = height;

    window->window_handle = glfwCreateWindow(width, height, title, 0, 0);
    if (!maximised) {
        glfwSetWindowPos(window->window_handle, x, y);
    }
    glfwSetWindowUserPointer(window->window_handle, ZestApp);

    if (maximised) {
        int width, height;
        glfwGetFramebufferSize(window->window_handle, &width, &height);
        window->window_width = width;
        window->window_height = height;
    }

    return window;
}

void zest__os_create_window_surface(zest_window window) {
    ZEST_VK_CHECK_RESULT(glfwCreateWindowSurface(ZestDevice->instance, window->window_handle, &ZestDevice->allocation_callbacks, &window->surface));
}

void zest__os_poll_events(void) {
    glfwPollEvents();
    double mouse_x, mouse_y;
    glfwGetCursorPos(ZestApp->window->window_handle, &mouse_x, &mouse_y);
    double last_mouse_x = ZestApp->mouse_x;
    double last_mouse_y = ZestApp->mouse_y;
    ZestApp->mouse_x = mouse_x;
    ZestApp->mouse_y = mouse_y;
    ZestApp->mouse_delta_x = last_mouse_x - ZestApp->mouse_x;
    ZestApp->mouse_delta_y = last_mouse_y - ZestApp->mouse_y;
    ZestApp->flags |= glfwWindowShouldClose(ZestApp->window->window_handle) ? zest_app_flag_quit_application : 0;
}

void zest__os_add_platform_extensions(void) {
    zest_uint count;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
    for (int i = 0; i != count; ++i) {
        zest_AddInstanceExtension((char*)glfw_extensions[i]);
    }
}

void zest__get_window_size_callback(void* user_data, int* fb_width, int* fb_height, int* window_width, int* window_height) {
    glfwGetFramebufferSize(ZestApp->window->window_handle, fb_width, fb_height);
    glfwGetWindowSize(ZestApp->window->window_handle, window_width, window_height);
}

void zest__destroy_window_callback(void* user_data) {
    glfwDestroyWindow((GLFWwindow*)ZestApp->window->window_handle);
}
#endif

const char* zest__vulkan_error(VkResult errorCode)
{
    switch (errorCode)
    {
#define ZEST_ERROR_STR(r) case VK_ ##r: return #r
        ZEST_ERROR_STR(NOT_READY);
        ZEST_ERROR_STR(TIMEOUT);
        ZEST_ERROR_STR(EVENT_SET);
        ZEST_ERROR_STR(EVENT_RESET);
        ZEST_ERROR_STR(INCOMPLETE);
        ZEST_ERROR_STR(ERROR_OUT_OF_HOST_MEMORY);
        ZEST_ERROR_STR(ERROR_OUT_OF_DEVICE_MEMORY);
        ZEST_ERROR_STR(ERROR_INITIALIZATION_FAILED);
        ZEST_ERROR_STR(ERROR_DEVICE_LOST);
        ZEST_ERROR_STR(ERROR_MEMORY_MAP_FAILED);
        ZEST_ERROR_STR(ERROR_LAYER_NOT_PRESENT);
        ZEST_ERROR_STR(ERROR_EXTENSION_NOT_PRESENT);
        ZEST_ERROR_STR(ERROR_FEATURE_NOT_PRESENT);
        ZEST_ERROR_STR(ERROR_INCOMPATIBLE_DRIVER);
        ZEST_ERROR_STR(ERROR_TOO_MANY_OBJECTS);
        ZEST_ERROR_STR(ERROR_FORMAT_NOT_SUPPORTED);
        ZEST_ERROR_STR(ERROR_SURFACE_LOST_KHR);
        ZEST_ERROR_STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
        ZEST_ERROR_STR(SUBOPTIMAL_KHR);
        ZEST_ERROR_STR(ERROR_OUT_OF_DATE_KHR);
        ZEST_ERROR_STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
        ZEST_ERROR_STR(ERROR_VALIDATION_FAILED_EXT);
        ZEST_ERROR_STR(ERROR_INVALID_SHADER_NV);
#undef STR
    default:
        return "UNKNOWN_ERROR";
    }
}

// --Math
zest_matrix4 zest_M4(float v) {
    zest_matrix4 matrix = { 0 };
    matrix.v[0].x = v;
    matrix.v[1].y = v;
    matrix.v[2].z = v;
    matrix.v[3].w = v;
    return matrix;
}

zest_matrix4 zest_M4SetWithVecs(zest_vec4 a, zest_vec4 b, zest_vec4 c, zest_vec4 d) {
    zest_matrix4 r;
    r.v[0].x = a.x; r.v[0].y = a.y; r.v[0].z = a.z; r.v[0].w = a.w;
    r.v[1].x = b.x; r.v[1].y = b.y; r.v[1].z = b.z; r.v[1].w = b.w;
    r.v[2].x = c.x; r.v[2].y = c.y; r.v[2].z = c.z; r.v[2].w = c.w;
    r.v[3].x = d.x; r.v[3].y = d.y; r.v[3].z = d.z; r.v[3].w = d.w;
    return r;
}

zest_matrix4 zest_ScaleMatrix4x4(zest_matrix4* m, zest_vec4* v) {
    zest_matrix4 result = zest_M4(1);
    result.v[0] = zest_ScaleVec4(m->v[0], v->x);
    result.v[1] = zest_ScaleVec4(m->v[1], v->y);
    result.v[2] = zest_ScaleVec4(m->v[2], v->z);
    result.v[3] = zest_ScaleVec4(m->v[3], v->w);
    return result;
}

zest_matrix4 zest_ScaleMatrix4(zest_matrix4* m, float scalar) {
    zest_matrix4 result = zest_M4(1);
    result.v[0] = zest_ScaleVec4(m->v[0], scalar);
    result.v[1] = zest_ScaleVec4(m->v[1], scalar);
    result.v[2] = zest_ScaleVec4(m->v[2], scalar);
    result.v[3] = zest_ScaleVec4(m->v[3], scalar);
    return result;
}

zest_vec2 zest_Vec2Set1(float v) {
    zest_vec2 vec; vec.x = v; vec.y = v; return vec;
}

zest_vec3 zest_Vec3Set1(float v) {
    zest_vec3 vec; vec.x = v; vec.y = v; vec.z = v; return vec;
}

zest_vec4 zest_Vec4Set1(float v) {
    zest_vec4 vec; vec.x = v; vec.y = v; vec.z = v; vec.w = v; return vec;
}

zest_vec2 zest_Vec2Set(float x, float y) {
    zest_vec2 vec; vec.x = x; vec.y = y; return vec;
}

zest_vec3 zest_Vec3Set(float x, float y, float z) {
    zest_vec3 vec; vec.x = x; vec.y = y; vec.z = z; return vec;
}

zest_vec4 zest_Vec4Set(float x, float y, float z, float w) {
    zest_vec4 vec; vec.x = x; vec.y = y; vec.z = z; vec.w = w; return vec;
}

zest_vec4 zest_Vec4SetVec(zest_vec4 in) {
    zest_vec4 vec; vec.x = in.x; vec.y = in.y; vec.z = in.z; vec.w = in.w; return vec;
}

zest_color zest_ColorSet(zest_byte r, zest_byte g, zest_byte b, zest_byte a) {
    zest_color color = {
        .r = r,.g = g,.b = b,.a = a
    };
    return color;
}

zest_color zest_ColorSet1(zest_byte c) {
    zest_color color = {
        .r = c,.g = c,.b = c,.a = c
    };
    return color;
}

zest_vec2 zest_AddVec2(zest_vec2 left, zest_vec2 right) {
    zest_vec2 result = {
        .x = left.x + right.x,
        .y = left.y + right.y,
    };
    return result;
}

zest_vec3 zest_AddVec3(zest_vec3 left, zest_vec3 right) {
    zest_vec3 result = {
        .x = left.x + right.x,
        .y = left.y + right.y,
        .z = left.z + right.z,
    };
    return result;
}

zest_vec4 zest_AddVec4(zest_vec4 left, zest_vec4 right) {
    zest_vec4 result = {
        .x = left.x + right.x,
        .y = left.y + right.y,
        .z = left.z + right.z,
        .w = left.w + right.w,
    };
    return result;
}

zest_ivec2 zest_AddiVec2(zest_ivec2 left, zest_ivec2 right) {
    zest_ivec2 result = {
        .x = left.x + right.x,
        .y = left.y + right.y,
    };
    return result;
}

zest_ivec3 zest_AddiVec3(zest_ivec3 left, zest_ivec3 right) {
    zest_ivec3 result = {
        .x = left.x + right.x,
        .y = left.y + right.y,
        .z = left.z + right.z,
    };
    return result;
}

zest_vec2 zest_SubVec2(zest_vec2 left, zest_vec2 right) {
    zest_vec2 result = {
        .x = left.x - right.x,
        .y = left.y - right.y,
    };
    return result;
}

zest_vec3 zest_SubVec3(zest_vec3 left, zest_vec3 right) {
    zest_vec3 result = {
        .x = left.x - right.x,
        .y = left.y - right.y,
        .z = left.z - right.z,
    };
    return result;
}

zest_vec4 zest_SubVec4(zest_vec4 left, zest_vec4 right) {
    zest_vec4 result = {
        .x = left.x - right.x,
        .y = left.y - right.y,
        .z = left.z - right.z,
        .w = left.w - right.w,
    };
    return result;
}

zest_vec3 zest_FlipVec3(zest_vec3 vec3) {
    zest_vec3 flipped;
    flipped.x = -vec3.x;
    flipped.y = -vec3.y;
    flipped.z = -vec3.z;
    return flipped;
}

zest_vec2 zest_ScaleVec2(zest_vec2 vec, float scalar) {
    zest_vec2 result;
    result.x = vec.x * scalar;
    result.y = vec.y * scalar;
    return result;
}

zest_vec3 zest_ScaleVec3(zest_vec3 vec, float scalar) {
    zest_vec3 result;
    result.x = vec.x * scalar;
    result.y = vec.y * scalar;
    result.z = vec.z * scalar;
    return result;
}

zest_vec4 zest_ScaleVec4(zest_vec4 vec, float scalar) {
    zest_vec4 result;
    result.x = vec.x * scalar;
    result.y = vec.y * scalar;
    result.z = vec.z * scalar;
    result.w = vec.w * scalar;
    return result;
}

zest_vec3 zest_MulVec3(zest_vec3 left, zest_vec3 right) {
    zest_vec3 result = {
        .x = left.x * right.x,
        .y = left.y * right.y,
        .z = left.z * right.z,
    };
    return result;
}

zest_vec4 zest_MulVec4(zest_vec4 left, zest_vec4 right) {
    zest_vec4 result = {
        .x = left.x * right.x,
        .y = left.y * right.y,
        .z = left.z * right.z,
        .w = left.w * right.w
    };
    return result;
}

float zest_Vec2Length(zest_vec2 const v) {
    return sqrtf(zest_Vec2Length2(v));
}

float zest_Vec2Length2(zest_vec2 const v) {
    return v.x * v.x + v.y * v.y;
}

float zest_LengthVec3(zest_vec3 const v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

float zest_LengthVec4(zest_vec4 const v) {
    return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
}

float zest_LengthVec(zest_vec3 const v) {
    return sqrtf(zest_LengthVec3(v));
}

zest_vec2 zest_NormalizeVec2(zest_vec2 const v) {
    float length = zest_Vec2Length(v);
    zest_vec2 result = {
        .x = v.x / length,
        .y = v.y / length,
    };
    return result;
}

zest_vec3 zest_NormalizeVec3(zest_vec3 const v) {
    float length = zest_LengthVec(v);
    zest_vec3 result = {
        .x = v.x / length,
        .y = v.y / length,
        .z = v.z / length
    };
    return result;
}

zest_vec4 zest_NormalizeVec4(zest_vec4 const v) {
    float length = sqrtf(zest_LengthVec4(v));
    zest_vec4 result = {
        .x = v.x / length,
        .y = v.y / length,
        .z = v.z / length,
        .w = v.w / length
    };
    return result;
}

zest_matrix4 zest_Inverse(zest_matrix4* m) {
    float Coef00 = m->v[2].z * m->v[3].w - m->v[3].z * m->v[2].w;
    float Coef02 = m->v[1].z * m->v[3].w - m->v[3].z * m->v[1].w;
    float Coef03 = m->v[1].z * m->v[2].w - m->v[2].z * m->v[1].w;

    float Coef04 = m->v[2].y * m->v[3].w - m->v[3].y * m->v[2].w;
    float Coef06 = m->v[1].y * m->v[3].w - m->v[3].y * m->v[1].w;
    float Coef07 = m->v[1].y * m->v[2].w - m->v[2].y * m->v[1].w;

    float Coef08 = m->v[2].y * m->v[3].z - m->v[3].y * m->v[2].z;
    float Coef10 = m->v[1].y * m->v[3].z - m->v[3].y * m->v[1].z;
    float Coef11 = m->v[1].y * m->v[2].z - m->v[2].y * m->v[1].z;

    float Coef12 = m->v[2].x * m->v[3].w - m->v[3].x * m->v[2].w;
    float Coef14 = m->v[1].x * m->v[3].w - m->v[3].x * m->v[1].w;
    float Coef15 = m->v[1].x * m->v[2].w - m->v[2].x * m->v[1].w;

    float Coef16 = m->v[2].x * m->v[3].z - m->v[3].x * m->v[2].z;
    float Coef18 = m->v[1].x * m->v[3].z - m->v[3].x * m->v[1].z;
    float Coef19 = m->v[1].x * m->v[2].z - m->v[2].x * m->v[1].z;

    float Coef20 = m->v[2].x * m->v[3].y - m->v[3].x * m->v[2].y;
    float Coef22 = m->v[1].x * m->v[3].y - m->v[3].x * m->v[1].y;
    float Coef23 = m->v[1].x * m->v[2].y - m->v[2].x * m->v[1].y;

    zest_vec4 Fac0 = zest_Vec4Set(Coef00, Coef00, Coef02, Coef03);
    zest_vec4 Fac1 = zest_Vec4Set(Coef04, Coef04, Coef06, Coef07);
    zest_vec4 Fac2 = zest_Vec4Set(Coef08, Coef08, Coef10, Coef11);
    zest_vec4 Fac3 = zest_Vec4Set(Coef12, Coef12, Coef14, Coef15);
    zest_vec4 Fac4 = zest_Vec4Set(Coef16, Coef16, Coef18, Coef19);
    zest_vec4 Fac5 = zest_Vec4Set(Coef20, Coef20, Coef22, Coef23);

    zest_vec4 Vec0 = zest_Vec4Set(m->v[1].x, m->v[0].x, m->v[0].x, m->v[0].x);
    zest_vec4 Vec1 = zest_Vec4Set(m->v[1].y, m->v[0].y, m->v[0].y, m->v[0].y);
    zest_vec4 Vec2 = zest_Vec4Set(m->v[1].z, m->v[0].z, m->v[0].z, m->v[0].z);
    zest_vec4 Vec3 = zest_Vec4Set(m->v[1].w, m->v[0].w, m->v[0].w, m->v[0].w);

    zest_vec4 Inv0 = zest_Vec4SetVec(zest_AddVec4(zest_SubVec4(zest_MulVec4(Vec1, Fac0), zest_MulVec4(Vec2, Fac1)), zest_MulVec4(Vec3, Fac2)));
    zest_vec4 Inv1 = zest_Vec4SetVec(zest_AddVec4(zest_SubVec4(zest_MulVec4(Vec0, Fac0), zest_MulVec4(Vec2, Fac3)), zest_MulVec4(Vec3, Fac4)));
    zest_vec4 Inv2 = zest_Vec4SetVec(zest_AddVec4(zest_SubVec4(zest_MulVec4(Vec0, Fac1), zest_MulVec4(Vec1, Fac3)), zest_MulVec4(Vec3, Fac5)));
    zest_vec4 Inv3 = zest_Vec4SetVec(zest_AddVec4(zest_SubVec4(zest_MulVec4(Vec0, Fac2), zest_MulVec4(Vec1, Fac4)), zest_MulVec4(Vec2, Fac5)));

    zest_vec4 SignA = zest_Vec4Set(+1, -1, +1, -1);
    zest_vec4 SignB = zest_Vec4Set(-1, +1, -1, +1);
    zest_matrix4 inverse = zest_M4SetWithVecs(zest_MulVec4(Inv0, SignA), zest_MulVec4(Inv1, SignB), zest_MulVec4(Inv2, SignA), zest_MulVec4(Inv3, SignB));

    zest_vec4 Row0 = zest_Vec4Set(inverse.v[0].x, inverse.v[1].x, inverse.v[2].x, inverse.v[3].x);

    zest_vec4 Dot0 = zest_Vec4SetVec(zest_MulVec4(m->v[0], Row0));
    float Dot1 = (Dot0.x + Dot0.y) + (Dot0.z + Dot0.w);

    float OneOverDeterminant = 1.f / Dot1;

    return zest_ScaleMatrix4(&inverse, OneOverDeterminant);
}

zest_matrix4 zest_Matrix4RotateX(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    zest_matrix4 r =
    { {
        {1, 0, 0, 0},
        {0, c,-s, 0},
        {0, s, c, 0},
        {0, 0, 0, 1}},
    };
    return r;
}

zest_matrix4 zest_Matrix4RotateY(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    zest_matrix4 r =
    { {
        { c, 0, s, 0},
        { 0, 1, 0, 0},
        {-s, 0, c, 0},
        { 0, 0, 0, 1}},
    };
    return r;
}

zest_matrix4 zest_Matrix4RotateZ(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    zest_matrix4 r =
    { {
        {c, -s, 0, 0},
        {s,  c, 0, 0},
        {0,  0, 1, 0},
        {0,  0, 0, 1}},
    };
    return r;
}

zest_matrix4 zest_CreateMatrix4(float pitch, float yaw, float roll, float x, float y, float z, float sx, float sy, float sz) {
    zest_matrix4 roll_mat = zest_Matrix4RotateZ(roll);
    zest_matrix4 pitch_mat = zest_Matrix4RotateX(pitch);
    zest_matrix4 yaw_mat = zest_Matrix4RotateY(yaw);
    zest_matrix4 matrix = zest_MatrixTransform(&yaw_mat, &pitch_mat);
    matrix = zest_MatrixTransform(&matrix, &roll_mat);
    matrix.v[0].w = x;
    matrix.v[1].w = y;
    matrix.v[2].w = z;
    matrix.v[3].x = sx;
    matrix.v[3].y = sy;
    matrix.v[3].z = sz;
    return matrix;
}

#ifdef ZEST_INTEL

zest_vec4 zest_MatrixTransformVector(zest_matrix4* mat, zest_vec4 vec) {
    zest_vec4 v;

    __m128 v4 = _mm_set_ps(vec.w, vec.z, vec.y, vec.x);

    __m128 mrow1 = _mm_load_ps(&mat->v[0].x);
    __m128 mrow2 = _mm_load_ps(&mat->v[1].x);
    __m128 mrow3 = _mm_load_ps(&mat->v[2].x);
    __m128 mrow4 = _mm_load_ps(&mat->v[3].x);

    __m128 row1result = _mm_mul_ps(v4, mrow1);
    __m128 row2result = _mm_mul_ps(v4, mrow2);
    __m128 row3result = _mm_mul_ps(v4, mrow3);
    __m128 row4result = _mm_mul_ps(v4, mrow4);

    float tmp[4];
    _mm_store_ps(tmp, row1result);
    v.x = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    _mm_store_ps(tmp, row2result);
    v.y = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    _mm_store_ps(tmp, row3result);
    v.z = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    _mm_store_ps(tmp, row4result);
    v.w = tmp[0] + tmp[1] + tmp[2] + tmp[3];

    return v;
}

zest_matrix4 zest_MatrixTransform(zest_matrix4* in, zest_matrix4* m) {
    zest_matrix4 res = { 0 };

    __m128 in_row[4];
    in_row[0] = _mm_load_ps(&in->v[0].x);
    in_row[1] = _mm_load_ps(&in->v[1].x);
    in_row[2] = _mm_load_ps(&in->v[2].x);
    in_row[3] = _mm_load_ps(&in->v[3].x);

    __m128 m_row1 = _mm_set_ps(m->v[3].x, m->v[2].x, m->v[1].x, m->v[0].x);
    __m128 m_row2 = _mm_set_ps(m->v[3].y, m->v[2].y, m->v[1].y, m->v[0].y);
    __m128 m_row3 = _mm_set_ps(m->v[3].z, m->v[2].z, m->v[1].z, m->v[0].z);
    __m128 m_row4 = _mm_set_ps(m->v[3].w, m->v[2].w, m->v[1].w, m->v[0].w);

    for (int r = 0; r <= 3; ++r)
    {

        __m128 row1result = _mm_mul_ps(in_row[r], m_row1);
        __m128 row2result = _mm_mul_ps(in_row[r], m_row2);
        __m128 row3result = _mm_mul_ps(in_row[r], m_row3);
        __m128 row4result = _mm_mul_ps(in_row[r], m_row4);

        float tmp[4];
        _mm_store_ps(tmp, row1result);
        res.v[r].x = tmp[0] + tmp[1] + tmp[2] + tmp[3];
        _mm_store_ps(tmp, row2result);
        res.v[r].y = tmp[0] + tmp[1] + tmp[2] + tmp[3];
        _mm_store_ps(tmp, row3result);
        res.v[r].z = tmp[0] + tmp[1] + tmp[2] + tmp[3];
        _mm_store_ps(tmp, row4result);
        res.v[r].w = tmp[0] + tmp[1] + tmp[2] + tmp[3];

    }
    return res;
}

#elif defined(ZEST_ARM)
zest_vec4 zest_MatrixTransformVector(zest_matrix4* mat, zest_vec4 vec) {
    zest_vec4 v;

    float32x4_t v4 = vld1q_f32(&vec.x);

    float32x4_t mrow1 = vld1q_f32(&mat->v[0].x);
    float32x4_t mrow2 = vld1q_f32(&mat->v[1].x);
    float32x4_t mrow3 = vld1q_f32(&mat->v[2].x);
    float32x4_t mrow4 = vld1q_f32(&mat->v[3].x);

    float32x4_t row1result = vmulq_f32(v4, mrow1);
    float32x4_t row2result = vmulq_f32(v4, mrow2);
    float32x4_t row3result = vmulq_f32(v4, mrow3);
    float32x4_t row4result = vmulq_f32(v4, mrow4);

    float tmp[4];
    vst1q_f32(tmp, row1result);
    v.x = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    vst1q_f32(tmp, row2result);
    v.y = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    vst1q_f32(tmp, row3result);
    v.z = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    vst1q_f32(tmp, row4result);
    v.w = tmp[0] + tmp[1] + tmp[2] + tmp[3];

    return v;
}

zest_matrix4 zest_MatrixTransform(zest_matrix4* in, zest_matrix4* m) {
    zest_matrix4 res = { 0 };

    float32x4_t in_row[4];
    in_row[0] = vld1q_f32(&in->v[0].x);
    in_row[1] = vld1q_f32(&in->v[1].x);
    in_row[2] = vld1q_f32(&in->v[2].x);
    in_row[3] = vld1q_f32(&in->v[3].x);

    float32x4_t m_row1 = vsetq_lane_f32(m->v[0].x, vdupq_n_f32(m->v[1].x), 0);
    m_row1 = vsetq_lane_f32(m->v[2].x, m_row1, 1);
    m_row1 = vsetq_lane_f32(m->v[3].x, m_row1, 2);

    float32x4_t m_row2 = vsetq_lane_f32(m->v[0].y, vdupq_n_f32(m->v[1].y), 0);
    m_row2 = vsetq_lane_f32(m->v[2].y, m_row2, 1);
    m_row2 = vsetq_lane_f32(m->v[3].y, m_row2, 2);

    float32x4_t m_row3 = vsetq_lane_f32(m->v[0].z, vdupq_n_f32(m->v[1].z), 0);
    m_row3 = vsetq_lane_f32(m->v[2].z, m_row3, 1);
    m_row3 = vsetq_lane_f32(m->v[3].z, m_row3, 2);

    float32x4_t m_row4 = vsetq_lane_f32(m->v[0].w, vdupq_n_f32(m->v[1].w), 0);
    m_row4 = vsetq_lane_f32(m->v[2].w, m_row4, 1);
    m_row4 = vsetq_lane_f32(m->v[3].w, m_row4, 2);

    for (int r = 0; r <= 3; ++r)
    {
        float32x4_t row1result = vmulq_f32(in_row[r], m_row1);
        float32x4_t row2result = vmulq_f32(in_row[r], m_row2);
        float32x4_t row3result = vmulq_f32(in_row[r], m_row3);
        float32x4_t row4result = vmulq_f32(in_row[r], m_row4);

        float32x4_t tmp;
        tmp = vaddq_f32(row1result, row2result);
        tmp = vaddq_f32(tmp, row3result);
        tmp = vaddq_f32(tmp, row4result);

        res.v[r].x = vgetq_lane_f32(tmp, 0);
        res.v[r].y = vgetq_lane_f32(tmp, 1);
        res.v[r].z = vgetq_lane_f32(tmp, 2);
        res.v[r].w = vgetq_lane_f32(tmp, 3);
    }
    return res;
}
#endif

zest_vec3 zest_CrossProduct(const zest_vec3 x, const zest_vec3 y)
{
    zest_vec3 result = {
        .x = x.y * y.z - y.y * x.z,
        .y = x.z * y.x - y.z * x.x,
        .z = x.x * y.y - y.x * x.y,
    };
    return result;
}

float zest_DotProduct3(const zest_vec3 a, const zest_vec3 b)
{
    return (a.x * b.x + a.y * b.y + a.z * b.z);
}

float zest_DotProduct4(const zest_vec4 a, const zest_vec4 b)
{
    return (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
}

zest_matrix4 zest_LookAt(const zest_vec3 eye, const zest_vec3 center, const zest_vec3 up)
{
    zest_vec3 f = zest_NormalizeVec3(zest_SubVec3(center, eye));
    zest_vec3 s = zest_NormalizeVec3(zest_CrossProduct(f, up));
    zest_vec3 u = zest_CrossProduct(s, f);

    zest_matrix4 result = { 0 };
    result.v[0].x = s.x;
    result.v[1].x = s.y;
    result.v[2].x = s.z;
    result.v[0].y = u.x;
    result.v[1].y = u.y;
    result.v[2].y = u.z;
    result.v[0].z = -f.x;
    result.v[1].z = -f.y;
    result.v[2].z = -f.z;
    result.v[3].x = -zest_DotProduct3(s, eye);
    result.v[3].y = -zest_DotProduct3(u, eye);
    result.v[3].z = zest_DotProduct3(f, eye);
    result.v[3].w = 1.f;
    return result;
}

zest_matrix4 zest_Ortho(float left, float right, float bottom, float top, float z_near, float z_far)
{
    zest_matrix4 result = { 0 };
    result.v[0].x = 2.f / (right - left);
    result.v[1].y = 2.f / (top - bottom);
    result.v[2].z = -1.f / (z_far - z_near);
    result.v[3].x = -(right + left) / (right - left);
    result.v[3].y = -(top + bottom) / (top - bottom);
    result.v[3].z = -z_near / (z_far - z_near);
    result.v[3].w = 1.f;
    return result;
}

//-- Events and States
zest_bool zest_SwapchainWasRecreated() {
    return (ZestRenderer->flags & zest_renderer_flag_swapchain_was_recreated);
}
//-- End Events and States

//-- Camera and other helpers
zest_camera_t zest_CreateCamera() {
    zest_camera_t camera = { 0 };
    camera.yaw = 0.f;
    camera.pitch = 0.f;
    camera.fov = 1.5708f;
    camera.ortho_scale = 5.f;
    camera.up = zest_Vec3Set(0.f, 1.f, 0.f);
    camera.right = zest_Vec3Set(1.f, 0.f, 0.f);
    zest_CameraUpdateFront(&camera);
    return camera;
}

void zest_TurnCamera(zest_camera_t* camera, float turn_x, float turn_y, float sensitivity) {
    camera->yaw -= zest_Radians(turn_x * sensitivity);
    camera->pitch += zest_Radians(turn_y * sensitivity);
    camera->pitch = ZEST__CLAMP(camera->pitch, -1.55334f, 1.55334f);
    zest_CameraUpdateFront(camera);
}

void zest_CameraUpdateFront(zest_camera_t* camera) {
    zest_vec3 direction;
    direction.x = cosf(camera->yaw) * cosf(camera->pitch);
    direction.y = sinf(camera->pitch);
    direction.z = sinf(camera->yaw) * cosf(camera->pitch);
    camera->front = zest_NormalizeVec3(direction);
}

void zest_CameraMoveForward(zest_camera_t* camera, float speed) {
    camera->position = zest_AddVec3(camera->position, zest_ScaleVec3(camera->front, speed));
}

void zest_CameraMoveBackward(zest_camera_t* camera, float speed) {
    camera->position = zest_SubVec3(camera->position, zest_ScaleVec3(camera->front, speed));
}

void zest_CameraMoveUp(zest_camera_t* camera, float speed) {
    zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(camera->front, camera->right));
    camera->position = zest_AddVec3(camera->position, zest_ScaleVec3(cross, speed));
}

void zest_CameraMoveDown(zest_camera_t* camera, float speed) {
    zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(camera->front, camera->right));
    camera->position = zest_SubVec3(camera->position, zest_ScaleVec3(cross, speed));
}

void zest_CameraStrafLeft(zest_camera_t* camera, float speed) {
    zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(camera->front, camera->up));
    camera->position = zest_SubVec3(camera->position, zest_ScaleVec3(cross, speed));
}

void zest_CameraStrafRight(zest_camera_t* camera, float speed) {
    zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(camera->front, camera->up));
    camera->position = zest_AddVec3(camera->position, zest_ScaleVec3(cross, speed));
}

void zest_CameraPosition(zest_camera_t* camera, zest_vec3 position) {
    camera->position = position;
}

void zest_CameraSetFoV(zest_camera_t* camera, float degrees) {
    camera->fov = zest_Radians(degrees);
}

void zest_CameraSetPitch(zest_camera_t* camera, float degrees) {
    camera->pitch = zest_Radians(degrees);
}

void zest_CameraSetYaw(zest_camera_t* camera, float degrees) {
    camera->yaw = zest_Radians(degrees);
}

zest_bool zest_RayIntersectPlane(zest_vec3 ray_origin, zest_vec3 ray_direction, zest_vec3 plane, zest_vec3 plane_normal, float* distance, zest_vec3* intersection) {
    float ray_to_plane_normal_dp = zest_DotProduct3(plane_normal, ray_direction);
    if (ray_to_plane_normal_dp >= 0)
        return ZEST_FALSE;
    float d = zest_DotProduct3(plane, plane_normal);
    *distance = (d - zest_DotProduct3(ray_origin, plane_normal)) / ray_to_plane_normal_dp;
    *intersection = zest_ScaleVec3(ray_direction, *distance);
    *intersection = zest_AddVec3(*intersection, ray_origin);
    return ZEST_TRUE;
}

zest_vec3 zest_ScreenRay(float xpos, float ypos, float view_width, float view_height, zest_matrix4* projection, zest_matrix4* view) {
    // converts a position from the 2d xpos, ypos to a normalized 3d direction
    float x = (2.0f * xpos) / view_width - 1.f;
    float y = (2.0f * ypos) / view_height - 1.f;
    zest_vec4 ray_clip = zest_Vec4Set(x, y, -1.f, 1.0f);
    // eye space to clip we would multiply by projection so
    // clip space to eye space is the inverse projection
    zest_matrix4 inverse_projection = zest_Inverse(projection);
    zest_vec4 ray_eye = zest_MatrixTransformVector(&inverse_projection, ray_clip);
    // convert point to forwards
    ray_eye = zest_Vec4Set(ray_eye.x, ray_eye.y, -1.f, 1.0f);
    // world space to eye space is usually multiply by view so
    // eye space to world space is inverse view
    zest_vec4 inv_ray_wor = zest_MatrixTransformVector(view, ray_eye);
    zest_vec3 ray_wor = zest_Vec3Set(inv_ray_wor.x, inv_ray_wor.y, inv_ray_wor.z);
    return ray_wor;
}

zest_vec2 zest_WorldToScreen(const float point[3], float view_width, float view_height, zest_matrix4* projection, zest_matrix4* view) {
    float w = view->v[0].z * point[0] + view->v[1].z * point[1] + view->v[2].z * point[2] + view->v[3].z;
    // If you try to convert the camera's "from" position to screen space, you will
    // end up dividing by zero (please don't do that)
    //if (w <= 0) return [-1, -1];
    if (w == 0) return zest_Vec2Set(-1.f, -1.f);
    float cx = projection->v[2].x + projection->v[0].x * (view->v[0].x * point[0] + view->v[1].x * point[1] + view->v[2].x * point[2] + view->v[3].x) / w;
    float cy = projection->v[2].y + projection->v[1].y * (view->v[0].y * point[0] + view->v[1].y * point[1] + view->v[2].y * point[2] + view->v[3].y) / w;

    return zest_Vec2Set((0.5f + 0.5f * -cx) * view_width, (0.5f + 0.5f * -cy) * view_height);
}

zest_vec2 zest_WorldToScreenOrtho(const float point[3], float view_width, float view_height, zest_matrix4* projection, zest_matrix4* view) {
    float cx = projection->v[3].x + projection->v[0].x * (view->v[0].x * point[0] + view->v[1].x * point[1] + view->v[2].x * point[2] + view->v[3].x);
    float cy = projection->v[3].y + projection->v[1].y * (view->v[0].y * point[0] + view->v[1].y * point[1] + view->v[2].y * point[2] + view->v[3].y);

    return zest_Vec2Set((0.5f + 0.5f * -cx) * view_width, (0.5f + 0.5f * -cy) * view_height);
}

zest_matrix4 zest_Perspective(float fovy, float aspect, float zNear, float zFar) {
    float const tanHalfFovy = tanf(fovy / 2.f);
    zest_matrix4 result = zest_M4(0.f);
    result.v[0].x = 1.f / (aspect * tanHalfFovy);
    result.v[1].y = 1.f / (tanHalfFovy);
    result.v[2].z = zFar / (zNear - zFar);
    result.v[2].w = -1.f;
    result.v[3].z = -(zFar * zNear) / (zFar - zNear);
    return result;
}

zest_matrix4 zest_TransposeMatrix4(zest_matrix4* mat) {
    zest_matrix4 r;
    r.v[0].x = mat->v[0].x; r.v[0].y = mat->v[1].x; r.v[0].z = mat->v[2].x; r.v[0].w = mat->v[3].x;
    r.v[1].x = mat->v[0].y; r.v[1].y = mat->v[1].y; r.v[1].z = mat->v[2].y; r.v[1].w = mat->v[3].y;
    r.v[2].x = mat->v[0].z; r.v[2].y = mat->v[1].z; r.v[2].z = mat->v[2].z; r.v[2].w = mat->v[3].z;
    r.v[3].x = mat->v[0].w; r.v[3].y = mat->v[1].w; r.v[3].z = mat->v[2].w; r.v[3].w = mat->v[3].w;
    return r;
}

void zest_CalculateFrustumPlanes(zest_matrix4* view_matrix, zest_matrix4* proj_matrix, zest_vec4 planes[6]) {
    zest_matrix4 matrix = zest_MatrixTransform(view_matrix, proj_matrix);
    // Extracting frustum planes from view-projection matrix

    planes[zest_LEFT].x = matrix.v[0].w + matrix.v[0].x;
    planes[zest_LEFT].y = matrix.v[1].w + matrix.v[1].x;
    planes[zest_LEFT].z = matrix.v[2].w + matrix.v[2].x;
    planes[zest_LEFT].w = matrix.v[3].w + matrix.v[3].x;

    planes[zest_RIGHT].x = matrix.v[0].w - matrix.v[0].x;
    planes[zest_RIGHT].y = matrix.v[1].w - matrix.v[1].x;
    planes[zest_RIGHT].z = matrix.v[2].w - matrix.v[2].x;
    planes[zest_RIGHT].w = matrix.v[3].w - matrix.v[3].x;

    planes[zest_TOP].x = matrix.v[0].w - matrix.v[0].y;
    planes[zest_TOP].y = matrix.v[1].w - matrix.v[1].y;
    planes[zest_TOP].z = matrix.v[2].w - matrix.v[2].y;
    planes[zest_TOP].w = matrix.v[3].w - matrix.v[3].y;

    planes[zest_BOTTOM].x = matrix.v[0].w + matrix.v[0].y;
    planes[zest_BOTTOM].y = matrix.v[1].w + matrix.v[1].y;
    planes[zest_BOTTOM].z = matrix.v[2].w + matrix.v[2].y;
    planes[zest_BOTTOM].w = matrix.v[3].w + matrix.v[3].y;

    planes[zest_BACK].x = matrix.v[0].w + matrix.v[0].z;
    planes[zest_BACK].y = matrix.v[1].w + matrix.v[1].z;
    planes[zest_BACK].z = matrix.v[2].w + matrix.v[2].z;
    planes[zest_BACK].w = matrix.v[3].w + matrix.v[3].z;

    planes[zest_FRONT].x = matrix.v[0].w - matrix.v[0].z;
    planes[zest_FRONT].y = matrix.v[1].w - matrix.v[1].z;
    planes[zest_FRONT].z = matrix.v[2].w - matrix.v[2].z;
    planes[zest_FRONT].w = matrix.v[3].w - matrix.v[3].z;

    for (int i = 0; i < 6; ++i) {
        float length = sqrtf(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
        planes[i].x /= length;
        planes[i].y /= length;
        planes[i].z /= length;
        planes[i].w /= length;
    }
}

zest_bool zest_IsPointInFrustum(const zest_vec4 planes[6], const float point[3]) {
    for (int i = 0; i < 6; i++) {
        if ((planes[i].x * point[0]) + (planes[i].y * point[1]) + (planes[i].z * point[2]) + planes[i].w < 0)
        {
            return ZEST_FALSE;
        }
    }
    // Point is inside the frustum
    return ZEST_TRUE;
}

zest_bool zest_IsSphereInFrustum(const zest_vec4 planes[6], const float point[3], float radius) {
    for (int i = 0; i < 6; i++) {
        if ((planes[i].x * point[0]) + (planes[i].y * point[1]) + (planes[i].z * point[2]) + planes[i].w <= -radius)
        {
            return ZEST_FALSE;
        }
    }
    // Point is inside the frustum
    return ZEST_TRUE;
}
//-- End camera and other helpers

float zest_Distance(float fromx, float fromy, float tox, float toy) {
    float w = tox - fromx;
    float h = toy - fromy;
    return sqrtf(w * w + h * h);
}

zest_uint zest_Pack16bit(float x, float y) {
    union
    {
        signed short in[2];
        zest_uint out;
    } u;

    x = x * 32767.f;
    y = y * 32767.f;

    u.in[0] = (signed short)x;
    u.in[1] = (signed short)y;

    return u.out;
}

zest_uint zest_Pack8bit(float x, float y, float z) {
    union
    {
        signed char in[4];
        zest_uint out;
    } u;

    x = x * 127.f;
    y = y * 127.f;
    z = z * 127.f;

    u.in[0] = (signed char)x;
    u.in[1] = (signed char)y;
    u.in[2] = (signed char)z;
    u.in[3] = 0;

    return u.out;
}

zest_uint zest_Pack16bitStretch(float x, float y) {
    union
    {
        signed short in[2];
        zest_uint out;
    } u;

    x = x * 655.34f;
    y = y * 32767.f;

    u.in[0] = (signed short)x;
    u.in[1] = (signed short)y;

    return u.out;
}

zest_uint zest_Pack10bit(zest_vec3* v, zest_uint extra) {
    zest_vec3 converted = zest_ScaleVec3(*v, 511.f);
    zest_packed10bit result = { 0 };
    result.pack = 0;
    result.data.x = (zest_uint)converted.z;
    result.data.y = (zest_uint)converted.y;
    result.data.z = (zest_uint)converted.x;
    result.data.w = extra;
    return result.pack;
}

zest_uint zest_Pack8bitx3(zest_vec3* v) {
    zest_vec3 converted = zest_ScaleVec3(*v, 255.f);
    zest_packed8bit result = { 0 };
    result.pack = 0;
    result.data.x = (zest_uint)converted.z;
    result.data.y = (zest_uint)converted.y;
    result.data.z = (zest_uint)converted.x;
    result.data.w = 0;
    return result.pack;
}

zest_size zest_GetNextPower(zest_size n) {
    return 1ULL << (zloc__scan_reverse(n) + 1);
}

float zest_Radians(float degrees) { return degrees * 0.01745329251994329576923690768489f; }
float zest_Degrees(float radians) { return radians * 57.295779513082320876798154814105f; }

zest_vec3 zest_LerpVec3(zest_vec3* captured, zest_vec3* present, float lerp) {
    return zest_AddVec3(zest_ScaleVec3(*present, lerp), zest_ScaleVec3(*captured, (1.f - lerp)));
}

zest_vec2 zest_LerpVec2(zest_vec2* captured, zest_vec2* present, float lerp) {
    return zest_AddVec2(zest_ScaleVec2(*present, lerp), zest_ScaleVec2(*captured, (1.f - lerp)));
}

float zest_Lerp(float captured, float present, float lerp) {
    return present * lerp + captured * (1.f - lerp);
}

//  --End Math

// Initialisation and destruction
void zest_Initialise(zest_create_info_t* info) {
    void* memory_pool = ZEST__ALLOCATE_POOL(info->memory_pool_size);

    ZEST_ASSERT(memory_pool);    //unable to allocate initial memory pool

    zloc_allocator* allocator = zloc_InitialiseAllocatorWithPool(memory_pool, info->memory_pool_size);
    ZestDevice = zloc_Allocate(allocator, sizeof(zest_device_t));
    ZestDevice->allocator = allocator;

    ZestApp = zloc_Allocate(allocator, sizeof(zest_app_t));
    ZestRenderer = zloc_Allocate(allocator, sizeof(zest_renderer_t));
    ZestHasher = zloc_Allocate(allocator, sizeof(zest_hasher));
    memset(ZestDevice, 0, sizeof(zest_device_t));
    memset(ZestApp, 0, sizeof(zest_app_t));
    memset(ZestRenderer, 0, sizeof(zest_renderer_t));
    ZestDevice->allocator = allocator;
    ZestDevice->allocator_start = allocator;
    ZestDevice->allocator_end = (char*)allocator + info->memory_pool_size;
    ZestDevice->memory_pools[0] = memory_pool;
    ZestDevice->memory_pool_sizes[0] = info->memory_pool_size;
    ZestDevice->memory_pool_count = 1;
    ZestDevice->color_format = info->color_format;
    ZestDevice->allocation_callbacks.pfnAllocation = zest_vk_allocate_callback;
    ZestDevice->allocation_callbacks.pfnReallocation = zest_vk_reallocate_callback;
    ZestDevice->allocation_callbacks.pfnFree = zest_vk_free_callback;
    if (info->log_path) {
        zest_SetErrorLogPath(info->log_path);
    }
    ZestRenderer->destroy_window_callback = info->destroy_window_callback;
    ZestRenderer->get_window_size_callback = info->get_window_size_callback;
    ZestRenderer->poll_events_callback = info->poll_events_callback;
    ZestRenderer->add_platform_extensions_callback = info->add_platform_extensions_callback;
    ZestRenderer->create_window_callback = info->create_window_callback;
    ZestRenderer->create_window_surface_callback = info->create_window_surface_callback;
    ZestRenderer->shader_compiler = shaderc_compiler_initialize();
    ZEST_ASSERT(ZestRenderer->shader_compiler); //Unable to create the shader compiler
    zest__initialise_app(info);
    zest__initialise_device();
    zest__initialise_renderer(info);
    zest__create_empty_command_queue(&ZestRenderer->empty_queue);
    if (info->flags & zest_init_flag_initialise_with_command_queue) {
        ZestApp->default_command_queue = zest_NewCommandQueue("Default command queue");
        {
            ZestApp->default_draw_commands = zest_NewDrawCommandSetupSwap("Default Draw Commands");
            {
                ZestApp->default_layer = zest_NewBuiltinLayerSetup("Sprite 2d Layer", zest_builtin_layer_sprites);
            }
            zest_ConnectQueueToPresent();
            zest_FinishQueueSetup();
        }
    }
}

void zest_Start() {
    zest__main_loop();
    zest__destroy();
}

void zest__initialise_device() {
    zest__create_instance();
    zest__setup_validation();
    zest__pick_physical_device();
    zest__create_logical_device();
    zest__set_limit_data();
    zest__set_default_pool_sizes();
}

void zest_SetUserData(void* data) {
    ZestApp->user_data = data;
}

void zest_SetUserUpdateCallback(void(*callback)(zest_microsecs, void*)) {
    ZestApp->update_callback = callback;
}

void zest_SetActiveCommandQueue(zest_command_queue command_queue) {
    ZEST_ASSERT(!ZestRenderer->active_command_queue);                                            //You already have an active queue for this frame.
    ZEST_ASSERT(ZEST__FLAGGED(command_queue->flags, zest_command_queue_flag_validated));        //Make sure that the command queue creation ended with the command: zest_FinishQueueSetup
    ZestRenderer->active_command_queue = command_queue;
    ZestRenderer->semaphores[ZEST_FIF].incoming = zest_GetCommandQueuePresentSemaphore(command_queue);
}

void zest_SetDestroyWindowCallback(void(*destroy_window_callback)(void* user_data)) {
    ZestRenderer->destroy_window_callback = destroy_window_callback;
}

void zest_SetGetWindowSizeCallback(void(*get_window_size_callback)(void* user_data, int* fb_width, int* fb_height, int* window_width, int* window_height)) {
    ZestRenderer->get_window_size_callback = get_window_size_callback;
}

void zest_SetPollEventsCallback(void(*poll_events_callback)(void)) {
    ZestRenderer->poll_events_callback = poll_events_callback;
}

void zest_SetPlatformExtensionsCallback(void(*add_platform_extensions_callback)(void)) {
    ZestRenderer->add_platform_extensions_callback = add_platform_extensions_callback;
}
//-- End Initialisation and destruction

/*
Functions that create a vulkan device
*/
void zest__create_instance(void) {
    if (ZEST_ENABLE_VALIDATION_LAYER) {
        zest_bool validation_support = zest__check_validation_layer_support();
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Checking for validation support" ZEST_NL);
        ZEST_ASSERT(validation_support);
    }

    VkApplicationInfo app_info = { 0 };
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Zest";
    app_info.applicationVersion = VK_API_VERSION_1_0;
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_API_VERSION_1_0;
    app_info.apiVersion = VK_API_VERSION_1_0;
    ZestDevice->api_version = app_info.apiVersion;

    VkInstanceCreateInfo create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
#ifdef ZEST_PORTABILITY_ENUMERATION
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Flagging for enumerate portability on MACOS\n");
    create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    zest__get_required_extensions();
    zest_uint extension_count = zest_vec_size(ZestDevice->extensions);
    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = (const char**)ZestDevice->extensions;

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = { 0 };
    if (ZEST_ENABLE_VALIDATION_LAYER) {
        create_info.enabledLayerCount = (zest_uint)zest__validation_layer_count;
        create_info.ppEnabledLayerNames = zest_validation_layers;
        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_create_info.pfnUserCallback = zest_debug_callback;
        create_info.pNext = &debug_create_info;
    }
    else {
        create_info.enabledLayerCount = 0;
    }

    zest_uint extension_property_count = 0;
    vkEnumerateInstanceExtensionProperties(ZEST_NULL, &extension_property_count, ZEST_NULL);

    ZEST__ARRAY(available_extensions, VkExtensionProperties, extension_property_count);

    vkEnumerateInstanceExtensionProperties(ZEST_NULL, &extension_property_count, available_extensions);

    for (zest_foreach_i(ZestDevice->extensions)) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Extension: %s\n", ZestDevice->extensions[i]);
    }

    VkResult result = vkCreateInstance(&create_info, &ZestDevice->allocation_callbacks, &ZestDevice->instance);

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Validating Vulkan Instance" ZEST_NL);
    ZEST_VK_CHECK_RESULT(result);

    ZEST__FREE(available_extensions);
    ZestRenderer->create_window_surface_callback(ZestApp->window);

}

static VKAPI_ATTR VkBool32 VKAPI_CALL zest_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    if (ZestDevice->log_path.str) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Validation Layer: %s\n", pCallbackData->pMessage);
    }
    else {
        ZEST_PRINT_WARNING("Validation Layer: %s", pCallbackData->pMessage);
    }

    return VK_FALSE;
}

VkResult zest_create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != ZEST_NULL) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void zest_destroy_debug_messenger(void) {
    if (ZestDevice->debug_messenger != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessenger =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ZestDevice->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroyDebugUtilsMessenger) {
            destroyDebugUtilsMessenger(ZestDevice->instance, ZestDevice->debug_messenger, &ZestDevice->allocation_callbacks);
        }
        ZestDevice->debug_messenger = VK_NULL_HANDLE;
    }
}

void zest__setup_validation(void) {
    if (!ZEST_ENABLE_VALIDATION_LAYER) return;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Enabling validation layers\n");

    VkDebugUtilsMessengerCreateInfoEXT create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = zest_debug_callback;

    ZEST_VK_CHECK_RESULT(zest_create_debug_messenger(ZestDevice->instance, &create_info, &ZestDevice->allocation_callbacks, &ZestDevice->debug_messenger));

    ZestDevice->pfnSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(ZestDevice->instance, "vkSetDebugUtilsObjectNameEXT");
}

void zest_AddInstanceExtension(char* extension) {
    zest_vec_push(ZestDevice->extensions, extension);
}

//Get the extensions that we need for the app.
void zest__get_required_extensions() {
    ZestRenderer->add_platform_extensions_callback();

    //If you're compiling on Mac and hitting this assert then it could be because you need to allow 3rd party libraries when signing the app.
    //Check "Disable Library Validation" under Signing and Capabilities
    ZEST_ASSERT(ZestDevice->extensions); //Vulkan not available

    if (ZEST_ENABLE_VALIDATION_LAYER) {
        zest_AddInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    //zest_AddInstanceExtension(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME);
#ifdef ZEST_PORTABILITY_ENUMERATION
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Adding enumerate portability extension\n");
    zest_AddInstanceExtension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    zest_AddInstanceExtension("VK_KHR_get_physical_device_properties2");
#endif
}

zest_uint zest_find_memory_type(zest_uint typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(ZestDevice->physical_device, &memory_properties);

    for (zest_uint i = 0; i < memory_properties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return ZEST_INVALID;
}

zest_bool zest__check_validation_layer_support(void) {
    zest_uint layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, ZEST_NULL);

    ZEST__ARRAY(available_layers, VkLayerProperties, layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

    for (int i = 0; i != zest__validation_layer_count; ++i) {
        zest_bool layer_found = 0;
        const char* layer_name = zest_validation_layers[i];

        for (int layer = 0; layer != layer_count; ++layer) {
            if (strcmp(layer_name, available_layers[layer].layerName) == 0) {
                layer_found = 1;
                break;
            }
        }

        if (!layer_found) {
            return 0;
        }
    }

    ZEST__FREE(available_layers);

    return 1;
}

void zest__pick_physical_device(void) {
    zest_uint device_count = 0;
    vkEnumeratePhysicalDevices(ZestDevice->instance, &device_count, ZEST_NULL);

    ZEST_ASSERT(device_count);        //Failed to find GPUs with Vulkan support!

    ZEST__ARRAY(devices, VkPhysicalDevice, device_count);
    vkEnumeratePhysicalDevices(ZestDevice->instance, &device_count, devices);

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Found %i devices" ZEST_NL, device_count);
    for (int i = 0; i != device_count; ++i) {
        zest__log_device_name(devices[i]);
    }
    ZestDevice->physical_device = VK_NULL_HANDLE;

    //Prioritise discrete GPUs when picking physical device
    if (device_count == 1 && zest__is_device_suitable(devices[0])) {
        if (zest__device_is_discrete_gpu(devices[0])) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "The one device found is suitable and is a discrete GPU" ZEST_NL);
        }
        else {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "The one device found is suitable" ZEST_NL);
        }
        ZestDevice->physical_device = devices[0];
    }
    else {
        VkPhysicalDevice discrete_device = VK_NULL_HANDLE;
        for (int i = 0; i != device_count; ++i) {
            if (zest__is_device_suitable(devices[i]) && zest__device_is_discrete_gpu(devices[i])) {
                discrete_device = devices[i];
                break;
            }
        }
        if (discrete_device != VK_NULL_HANDLE) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Found suitable device that is a discrete GPU: ");
            zest__log_device_name(discrete_device);
            ZestDevice->physical_device = discrete_device;
        }
        else {
            for (int i = 0; i != device_count; ++i) {
                if (zest__is_device_suitable(devices[i])) {
                    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Found suitable device:" ZEST_NL);
                    zest__log_device_name(devices[i]);
                    ZestDevice->physical_device = devices[i];
                    break;
                }
            }
        }
    }

    if (ZestDevice->physical_device == VK_NULL_HANDLE) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Could not find a suitable device!" ZEST_NL);
    }
    ZEST_ASSERT(ZestDevice->physical_device != VK_NULL_HANDLE);    //Unable to find suitable GPU
    ZestDevice->msaa_samples = zest__get_max_useable_sample_count();

    // Store Properties features, limits and properties of the physical ZestDevice for later use
    // ZestDevice properties also contain limits and sparse properties
    vkGetPhysicalDeviceProperties(ZestDevice->physical_device, &ZestDevice->properties);
    // Features should be checked by the examples before using them
    vkGetPhysicalDeviceFeatures(ZestDevice->physical_device, &ZestDevice->features);
    // Memory properties are used regularly for creating all kinds of buffers
    vkGetPhysicalDeviceMemoryProperties(ZestDevice->physical_device, &ZestDevice->memory_properties);

    //Print out the memory available
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Max Memory Allocation Count: %i" ZEST_NL, ZestDevice->properties.limits.maxMemoryAllocationCount);
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Memory available in GPU:" ZEST_NL);
    for (int i = 0; i != ZestDevice->memory_properties.memoryHeapCount; ++i) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "    Heap flags: %i, Size: %zi" ZEST_NL, ZestDevice->memory_properties.memoryHeaps[i].flags, ZestDevice->memory_properties.memoryHeaps[i].size);
    }

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Memory types mapping in GPU:" ZEST_NL);
    for (int i = 0; i != ZestDevice->memory_properties.memoryTypeCount; ++i) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "    %i) Heap Index: %i, Property Flags: %i" ZEST_NL, i, ZestDevice->memory_properties.memoryTypes[i].heapIndex, ZestDevice->memory_properties.memoryTypes[i].propertyFlags);
    }

    ZEST__FREE(devices);
}

zest_bool zest__is_device_suitable(VkPhysicalDevice physical_device) {
    zest_bool extensions_supported = zest__check_device_extension_support(physical_device);

    zest_bool swapchain_adequate = 0;
    if (extensions_supported) {
        ZestDevice->swapchain_support_details = zest__query_swapchain_support(physical_device);
        swapchain_adequate = ZestDevice->swapchain_support_details.formats_count && ZestDevice->swapchain_support_details.present_modes_count;
    }

    VkPhysicalDeviceFeatures supported_features;
    vkGetPhysicalDeviceFeatures(physical_device, &supported_features);

    return extensions_supported && swapchain_adequate && supported_features.samplerAnisotropy;
}

void zest__log_device_name(VkPhysicalDevice physical_device) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "\t%s" ZEST_NL, properties.deviceName);
}

zest_bool zest__device_is_discrete_gpu(VkPhysicalDevice physical_device) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    return properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

zest_index zest__get_queue_family_index(VkQueueFlags queue_flags, VkQueueFamilyProperties* queue_families) {
    // Dedicated queue for compute
    // Try to find a queue family index that supports compute but not graphics
    if ((queue_flags & VK_QUEUE_COMPUTE_BIT) == queue_flags)
    {
        for (zest_foreach_i(queue_families)) {
            if ((queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
            {
                return i;
            }
        }
    }

    // Dedicated queue for transfer
    // Try to find a queue family index that supports transfer but not graphics and compute
    if ((queue_flags & VK_QUEUE_TRANSFER_BIT) == queue_flags)
    {
        for (zest_foreach_i(queue_families)) {
            if ((queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
            {
                return i;
            }
        }
    }

    // For other queue types or if no separate compute queue is present, return the first one to support the requested flags
    for (zest_foreach_i(queue_families))
    {
        if ((queue_families[i].queueFlags & queue_flags) == queue_flags)
        {
            return i;
        }
    }

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "ERROR: Failed to find any queue index for Flag: %i" ZEST_NL, queue_flags);
    return 0;
}

zest_queue_family_indices zest__find_queue_families(VkPhysicalDevice physical_device, VkDeviceQueueCreateInfo* queue_create_infos) {
}

zest_bool zest__check_device_extension_support(VkPhysicalDevice physical_device) {
    zest_uint extension_count;
    vkEnumerateDeviceExtensionProperties(physical_device, ZEST_NULL, &extension_count, ZEST_NULL);

    ZEST__ARRAY(available_extensions, VkExtensionProperties, extension_count);
    vkEnumerateDeviceExtensionProperties(physical_device, ZEST_NULL, &extension_count, available_extensions);

    zest_uint required_extensions_found = 0;
    for (int i = 0; i != extension_count; ++i) {
        for (int e = 0; e != zest__required_extension_names_count; ++e) {
            if (strcmp(available_extensions[i].extensionName, zest_required_extensions[e]) == 0) {
                required_extensions_found++;
            }
        }
    }

    return required_extensions_found >= zest__required_extension_names_count;
}

zest_swapchain_support_details_t zest__query_swapchain_support(VkPhysicalDevice physical_device) {
    zest_swapchain_support_details_t details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, ZestApp->window->surface, &details.capabilities);

    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, ZestApp->window->surface, &details.formats_count, ZEST_NULL);

    if (details.formats_count != 0) {
        details.formats = ZEST__ALLOCATE(sizeof(VkSurfaceFormatKHR) * details.formats_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, ZestApp->window->surface, &details.formats_count, details.formats);
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, ZestApp->window->surface, &details.present_modes_count, ZEST_NULL);

    if (details.present_modes_count != 0) {
        details.present_modes = ZEST__ALLOCATE(sizeof(VkPresentModeKHR) * details.present_modes_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, ZestApp->window->surface, &details.present_modes_count, details.present_modes);
    }

    return details;
}

VkSampleCountFlagBits zest__get_max_useable_sample_count(void) {
    VkPhysicalDeviceProperties physical_device_properties;
    vkGetPhysicalDeviceProperties(ZestDevice->physical_device, &physical_device_properties);

    VkSampleCountFlags counts = ZEST__MIN(physical_device_properties.limits.framebufferColorSampleCounts, physical_device_properties.limits.framebufferDepthSampleCounts);
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

void zest__create_logical_device(void) {
    zest_queue_family_indices indices = { 0 };

    zest_uint queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ZestDevice->physical_device, &queue_family_count, ZEST_NULL);

    VkQueueFamilyProperties* queue_families = 0;
    zest_vec_resize(queue_families, queue_family_count);
    //VkQueueFamilyProperties queue_families[10];
    vkGetPhysicalDeviceQueueFamilyProperties(ZestDevice->physical_device, &queue_family_count, queue_families);

    float queue_priority = 0.0f;
    VkDeviceQueueCreateInfo queue_create_infos[3];

    int queue_create_count = 0;
    // Graphics queue
    {
        indices.graphics_family = zest__get_queue_family_index(VK_QUEUE_GRAPHICS_BIT, queue_families);
        VkDeviceQueueCreateInfo queue_info = { 0 };
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = indices.graphics_family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &queue_priority;
        queue_create_infos[0] = queue_info;
        queue_create_count++;
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Graphics queue index set to: %i" ZEST_NL, indices.graphics_family);
    }

    // Dedicated compute queue
    {
        indices.compute_family = zest__get_queue_family_index(VK_QUEUE_COMPUTE_BIT, queue_families);
        if (indices.compute_family != indices.graphics_family)
        {
            // If compute family index differs, we need an additional queue create info for the compute queue
            VkDeviceQueueCreateInfo queue_info = { 0 };
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info.queueFamilyIndex = indices.compute_family;
            queue_info.queueCount = 1;
            queue_info.pQueuePriorities = &queue_priority;
            queue_create_infos[1] = queue_info;
            queue_create_count++;
        }
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Compute queue index set to: %i" ZEST_NL, indices.compute_family);
    }

    // Dedicated transfer queue - Note that currently we're not actually doing anything with dedicated transfer queues yet.
    {
        indices.transfer_index = zest__get_queue_family_index(VK_QUEUE_TRANSFER_BIT, queue_families);
        if ((indices.transfer_index != indices.graphics_family) && (indices.transfer_index != indices.compute_family))
        {
            // If transfer_index family index differs, we need an additional queue create info for the transfer queue
            VkDeviceQueueCreateInfo queue_info = { 0 };
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info.queueFamilyIndex = indices.transfer_index;
            queue_info.queueCount = 1;
            queue_info.pQueuePriorities = &queue_priority;
            queue_create_infos[2] = queue_info;
            queue_create_count++;
        }
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Transfer queue index set to: %i" ZEST_NL, indices.transfer_index);
    }

    zest_vec_free(queue_families);

    VkPhysicalDeviceFeatures device_features = { 0 };
    device_features.samplerAnisotropy = VK_TRUE;
    //device_features.wideLines = VK_TRUE;
    //device_features.dualSrcBlend = VK_TRUE;
    //device_features.vertexPipelineStoresAndAtomics = VK_TRUE;
    VkPhysicalDeviceVulkan12Features device_features_12 = { 0 };
    device_features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    device_features_12.bufferDeviceAddress = VK_TRUE;

    //I put this in when trying to get RenderDoc to work but it turned out to be unrelated
    //Leaving here for now though.
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = { 0 };
    bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;  // Enable bufferDeviceAddress feature
    bufferDeviceAddressFeatures.bufferDeviceAddressCaptureReplay = VK_TRUE;
    // Create VkDeviceCreateInfo structure and include VkPhysicalDeviceBufferDeviceAddressFeatures

    VkDeviceCreateInfo create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pEnabledFeatures = &device_features;
    create_info.queueCreateInfoCount = queue_create_count;
    create_info.pQueueCreateInfos = queue_create_infos;
    create_info.enabledExtensionCount = zest__required_extension_names_count;
    create_info.ppEnabledExtensionNames = zest_required_extensions;
    //create_info.pNext = &bufferDeviceAddressFeatures;
    if (ZestDevice->api_version == VK_API_VERSION_1_2) {
        create_info.pNext = &device_features_12;
    }

    if (ZEST_ENABLE_VALIDATION_LAYER) {
        create_info.enabledLayerCount = zest__validation_layer_count;
        create_info.ppEnabledLayerNames = zest_validation_layers;
    }
    else {
        create_info.enabledLayerCount = 0;
    }

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating logical device" ZEST_NL);
    ZEST_VK_CHECK_RESULT(vkCreateDevice(ZestDevice->physical_device, &create_info, &ZestDevice->allocation_callbacks, &ZestDevice->logical_device));

    vkGetDeviceQueue(ZestDevice->logical_device, indices.graphics_family, 0, &ZestDevice->graphics_queue);
    //There's no real point of a one time graphics queue on the same index. It seems like AMD cards have a queue count of one on the graphics family so you can't
    //do one off queues while other rendering is going on.
    vkGetDeviceQueue(ZestDevice->logical_device, indices.graphics_family, 0, &ZestDevice->one_time_graphics_queue);
    vkGetDeviceQueue(ZestDevice->logical_device, indices.compute_family, 0, &ZestDevice->compute_queue);
    vkGetDeviceQueue(ZestDevice->logical_device, indices.transfer_index, 0, &ZestDevice->transfer_queue);

    ZestDevice->graphics_queue_family_index = indices.graphics_family;
    ZestDevice->transfer_queue_family_index = indices.transfer_index;
    ZestDevice->compute_queue_family_index = indices.compute_family;

    VkCommandPoolCreateInfo cmd_info_pool = { 0 };
    cmd_info_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_info_pool.queueFamilyIndex = ZestDevice->graphics_queue_family_index;
    cmd_info_pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating command queues" ZEST_NL);
    ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &cmd_info_pool, &ZestDevice->allocation_callbacks, &ZestDevice->command_pool));
    ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &cmd_info_pool, &ZestDevice->allocation_callbacks, &ZestDevice->one_time_command_pool));
}

void zest__set_limit_data() {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(ZestDevice->physical_device, &physicalDeviceProperties);

    ZestDevice->max_image_size = physicalDeviceProperties.limits.maxImageDimension2D;
}

zest_buffer_type_t zest__get_buffer_memory_type(VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, zest_size size) {
    VkBufferCreateInfo buffer_info = { 0 };
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage_flags;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_info.flags = 0;
    VkBuffer temp_buffer;
    ZEST_VK_CHECK_RESULT(vkCreateBuffer(ZestDevice->logical_device, &buffer_info, &ZestDevice->allocation_callbacks, &temp_buffer));

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(ZestDevice->logical_device, temp_buffer, &memory_requirements);

    zest_buffer_type_t buffer_type;
    buffer_type.alignment = memory_requirements.alignment;
    buffer_type.memory_type_index = zest_find_memory_type(memory_requirements.memoryTypeBits, property_flags);
    vkDestroyBuffer(ZestDevice->logical_device, temp_buffer, &ZestDevice->allocation_callbacks);
    return buffer_type;
}

void zest__set_default_pool_sizes() {
    zest_buffer_usage_t usage = { 0 };
    //Images stored on device use share a single pool as far as I know
    //But not true was having issues with this. An image buffer can share the same usage properties but have different alignment requirements
    //So they will get separate pools
    //Depth buffers
    usage.image_flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    zest_SetDeviceImagePoolSize("Depth Buffer", usage.image_flags, usage.property_flags, 1024, zloc__MEGABYTE(64));

    //General Textures
    usage.image_flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    zest_SetDeviceImagePoolSize("Texture Buffer", usage.image_flags, usage.property_flags, 1024, zloc__MEGABYTE(64));

    //Storage Textures For Writing/Reading from
    usage.image_flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    zest_SetDeviceImagePoolSize("Texture Read/Write", usage.image_flags, usage.property_flags, 1024, zloc__MEGABYTE(64));

    //Render targets
    usage.image_flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    zest_SetDeviceImagePoolSize("Render Target", usage.image_flags, usage.property_flags, 1024, zloc__MEGABYTE(64));

    //Uniform buffers
    usage.image_flags = 0;
    usage.usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    zest_SetDeviceBufferPoolSize("Uniform Buffers", usage.usage_flags, usage.property_flags, 64, zloc__MEGABYTE(1));

    //Staging buffer
    usage.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    zest_SetDeviceBufferPoolSize("Host Staging Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(2), zloc__MEGABYTE(32));

    //Vertex buffer
    usage.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    zest_SetDeviceBufferPoolSize("Vertex Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));

    //CPU Visible Vertex buffer
    usage.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    zest_SetDeviceBufferPoolSize("CPU Visible Vertex Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(32));

    //Storage buffer
    usage.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    zest_SetDeviceBufferPoolSize("Storage Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));

    //CPU Visible Storage buffer
    usage.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    zest_SetDeviceBufferPoolSize("CPU Visible Storage Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(32));

    //Index buffer
    usage.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    zest_SetDeviceBufferPoolSize("Index Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));

    //CPU Visible Index buffer
    usage.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    zest_SetDeviceBufferPoolSize("Index Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(32));

    //Vertex buffer with storage flag
    usage.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    zest_SetDeviceBufferPoolSize("Vertex Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));

    //Index buffer with storage flag
    usage.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    zest_SetDeviceBufferPoolSize("Index Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Set device pool sizes" ZEST_NL);
}


zest_bool zest__has_blit_support(VkFormat src_format) {
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(ZestDevice->physical_device, src_format, &format_properties);
    if (!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
        return 0;
    }

    // Check if the device supports blitting to linear images
    vkGetPhysicalDeviceFormatProperties(ZestDevice->physical_device, VK_FORMAT_R8G8B8A8_UNORM, &format_properties);
    if (!(format_properties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
        return 0;
    }
    return ZEST_TRUE;
}

void* zest_vk_allocate_callback(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) {
    return ZEST__ALLOCATE(size);
}

void* zest_vk_reallocate_callback(void* pUserData, void* memory, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) {
    return ZEST__REALLOCATE(memory, size);
}

void zest_vk_free_callback(void* pUserData, void* memory) {
    ZEST__FREE(memory);
}

/*
End of Device creation functions
*/

void zest__initialise_app(zest_create_info_t* create_info) {
    memset(ZestApp, 0, sizeof(zest_app_t));
    ZestApp->create_info = *create_info;
    ZestApp->update_callback = 0;
    ZestApp->current_elapsed = 0;
    ZestApp->update_time = 0;
    ZestApp->render_time = 0;
    ZestApp->frame_timer = 0;

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create window with dimensions: %i, %i" ZEST_NL, create_info->screen_width, create_info->screen_height);
    ZestApp->window = ZestRenderer->create_window_callback(create_info->screen_x, create_info->screen_y, create_info->screen_width, create_info->screen_height, ZEST__FLAGGED(create_info->flags, zest_init_flag_maximised), "Zest");
}

void zest__destroy(void) {
    zest_WaitForIdleDevice();
    zest__cleanup_renderer();
    vkDestroySurfaceKHR(ZestDevice->instance, ZestApp->window->surface, &ZestDevice->allocation_callbacks);
    zest_destroy_debug_messenger();
    vkDestroyPipelineCache(ZestDevice->logical_device, ZestRenderer->pipeline_cache, &ZestDevice->allocation_callbacks);
    vkDestroyCommandPool(ZestDevice->logical_device, ZestDevice->command_pool, &ZestDevice->allocation_callbacks);
    vkDestroyCommandPool(ZestDevice->logical_device, ZestDevice->one_time_command_pool, &ZestDevice->allocation_callbacks);
    vkDestroyDevice(ZestDevice->logical_device, &ZestDevice->allocation_callbacks);
    vkDestroyInstance(ZestDevice->instance, &ZestDevice->allocation_callbacks);
    ZestRenderer->destroy_window_callback(ZestApp->user_data);
    free(ZestDevice->memory_pools[0]);
}

zest_microsecs zest__set_elapsed_time(void) {
    ZestApp->current_elapsed = zest_Microsecs() - ZestApp->current_elapsed_time;
    ZestApp->current_elapsed_time = zest_Microsecs();

    return ZestApp->current_elapsed;
}

void zest__do_scheduled_tasks(void) {
    if (zest_vec_size(ZestRenderer->texture_delete_queue)) {
        for (zest_foreach_i(ZestRenderer->texture_delete_queue)) {
            zest_texture texture = ZestRenderer->texture_delete_queue[i];
            zest__delete_texture(texture);
        }
        zest_vec_clear(ZestRenderer->texture_delete_queue);
    }

    if (zest_vec_size(ZestRenderer->texture_reprocess_queue)) {
#ifdef __APPLE__
        //Temporary measure to get Mac working
        //Without this metal complains about the texture being in use
        zest_WaitForIdleDevice();
#endif
        for (zest_foreach_i(ZestRenderer->texture_reprocess_queue)) {
            zest_ProcessTextureImages(ZestRenderer->texture_reprocess_queue[i]);
            zest_RefreshTextureDescriptors(ZestRenderer->texture_reprocess_queue[i]);
        }
#if defined(ZEST_ATOMICS)
        atomic_store(&ZestRenderer->lock_texture_reprocess_queue, 0);
#endif
        zest_vec_clear(ZestRenderer->texture_reprocess_queue);
    }

    if (zest_vec_size(ZestRenderer->render_target_recreate_queue[ZEST_FIF])) {
        for (zest_foreach_i(ZestRenderer->render_target_recreate_queue[ZEST_FIF])) {
            zest_render_target render_target = ZestRenderer->render_target_recreate_queue[ZEST_FIF][i];
            zest_RecreateRenderTargetResources(render_target, ZEST_FIF);
        }
        zest__update_command_queue_viewports();
        zest_vec_clear(ZestRenderer->render_target_recreate_queue[ZEST_FIF]);
    }

    if (zest_vec_size(ZestRenderer->rt_sampler_refresh_queue[ZEST_FIF])) {
        for (zest_foreach_i(ZestRenderer->rt_sampler_refresh_queue[ZEST_FIF])) {
            zest_render_target render_target = ZestRenderer->rt_sampler_refresh_queue[ZEST_FIF][i];
            zest__refresh_render_target_sampler(render_target, ZEST_FIF);
        }
        zest_vec_clear(ZestRenderer->rt_sampler_refresh_queue[ZEST_FIF]);
    }

}

void zest__main_loop(void) {
    ZestApp->current_elapsed_time = zest_Microsecs();
    ZEST_ASSERT(ZestApp->update_callback);    //Must define an update callback function
    while (!(ZestApp->flags & zest_app_flag_quit_application)) {

        ZEST_VK_CHECK_RESULT(vkWaitForFences(ZestDevice->logical_device, 1, &ZestRenderer->fif_fence[ZEST_FIF], VK_TRUE, UINT64_MAX));

        zest__do_scheduled_tasks();

        ZestRenderer->poll_events_callback();

        zest__set_elapsed_time();

        ZestApp->update_callback(ZestApp->current_elapsed, ZestApp->user_data);

        zest__draw_renderer_frame();

        ZestApp->frame_timer += ZestApp->current_elapsed;
        ZestApp->frame_count++;
        if (ZestApp->frame_timer >= ZEST_MICROSECS_SECOND) {
            ZestApp->last_fps = ZestApp->frame_count;
            ZestApp->frame_count = 0;
            ZestApp->frame_timer = ZestApp->frame_timer - ZEST_MICROSECS_SECOND;
            if (ZEST__FLAGGED(ZestApp->flags, zest_app_flag_output_fps)) {
                printf("FPS: %u\n", ZestApp->last_fps);
            }
        }

        ZestRenderer->active_command_queue = ZEST_NULL;
    }
}


void zest__update_window_size(zest_window window, zest_uint width, zest_uint height) {
    window->window_width = width;
    window->window_height = height;
}

// --Buffer & Memory Management
void zest__add_host_memory_pool(zest_size size) {
    ZEST_ASSERT(ZestDevice->memory_pool_count < 32);    //Reached the max number of memory pools
    zest_size pool_size = ZestApp->create_info.memory_pool_size;
    if (pool_size <= size) {
        pool_size = zest_GetNextPower(size);
    }
    ZestDevice->memory_pools[ZestDevice->memory_pool_count] = ZEST__ALLOCATE_POOL(pool_size);
    ZEST_ASSERT(ZestDevice->memory_pools[ZestDevice->memory_pool_count]);    //Unable to allocate more memory. Out of memory?
    zloc_AddPool(ZestDevice->allocator, ZestDevice->memory_pools[ZestDevice->memory_pool_count], pool_size);
    ZestDevice->memory_pool_sizes[ZestDevice->memory_pool_count] = pool_size;
    ZestDevice->memory_pool_count++;
    ZEST_PRINT_NOTICE(ZEST_NOTICE_COLOR"Note: Ran out of space in the host memory pool so adding a new one of size %zu. ", pool_size);
}

void* zest__allocate(zest_size size) {
    void* allocation = zloc_Allocate(ZestDevice->allocator, size);
    if (!allocation) {
        zest__add_host_memory_pool(size);
        allocation = zloc_Allocate(ZestDevice->allocator, size);
        ZEST_ASSERT(allocation);    //Unable to allocate even after adding a pool
    }
    return allocation;
}

void* zest__reallocate(void* memory, zest_size size) {
    void* allocation = zloc_Reallocate(ZestDevice->allocator, memory, size);
    if (!allocation) {
        zest__add_host_memory_pool(size);
        allocation = zloc_Reallocate(ZestDevice->allocator, memory, size);
        ZEST_ASSERT(allocation);    //Unable to allocate even after adding a pool
    }
    return allocation;
}

ZEST_PRIVATE void* zest__allocate_aligned(zest_size size, zest_size alignment) {
    void* allocation = zloc_AllocateAligned(ZestDevice->allocator, size, alignment);
    if (!allocation) {
        zest__add_host_memory_pool(size);
        allocation = zloc_AllocateAligned(ZestDevice->allocator, size, alignment);
        ZEST_ASSERT(allocation);    //Unable to allocate even after adding a pool
    }
    return allocation;
}

VkResult zest__bind_memory(zest_device_memory_pool memory_allocation, VkDeviceSize offset) {
    return vkBindBufferMemory(ZestDevice->logical_device, memory_allocation->buffer, memory_allocation->memory, offset);
}

VkResult zest__map_memory(zest_device_memory_pool memory_allocation, VkDeviceSize size, VkDeviceSize offset) {
    return vkMapMemory(ZestDevice->logical_device, memory_allocation->memory, offset, size, 0, &memory_allocation->mapped);
}

void zest__unmap_memory(zest_device_memory_pool memory_allocation) {
    if (memory_allocation->mapped) {
        vkUnmapMemory(ZestDevice->logical_device, memory_allocation->memory);
        memory_allocation->mapped = ZEST_NULL;
    }
}

void zest__destroy_memory(zest_device_memory_pool memory_allocation) {
    if (memory_allocation->buffer) {
        vkDestroyBuffer(ZestDevice->logical_device, memory_allocation->buffer, &ZestDevice->allocation_callbacks);
    }
    if (memory_allocation->memory) {
        vkFreeMemory(ZestDevice->logical_device, memory_allocation->memory, &ZestDevice->allocation_callbacks);
    }
    memory_allocation->mapped = ZEST_NULL;
}

VkResult zest__flush_memory(zest_device_memory_pool memory_allocation, VkDeviceSize size, VkDeviceSize offset) {
    VkMappedMemoryRange mappedRange = { 0 };
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = memory_allocation->memory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkFlushMappedMemoryRanges(ZestDevice->logical_device, 1, &mappedRange);
}

void zest__buffer_write_barrier(VkCommandBuffer command_buffer, zest_buffer buffer) {
    VkBufferMemoryBarrier buffer_barrier = { 0 };
    buffer_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffer_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    buffer_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    buffer_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buffer_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buffer_barrier.buffer = buffer->memory_pool->buffer; // Destination buffer used in vkCmdCopyBuffer
    buffer_barrier.offset = buffer->memory_offset;
    buffer_barrier.size = buffer->size;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &buffer_barrier, 0, NULL);
}

void zest__create_device_memory_pool(VkDeviceSize size, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, zest_device_memory_pool buffer, const char* name) {
    VkBufferCreateInfo buffer_info = { 0 };
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage_flags;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_info.flags = 0;

    ZEST_VK_CHECK_RESULT(vkCreateBuffer(ZestDevice->logical_device, &buffer_info, &ZestDevice->allocation_callbacks, &buffer->buffer));

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(ZestDevice->logical_device, buffer->buffer, &memory_requirements);

    VkMemoryAllocateFlagsInfo flags;
    flags.deviceMask = 0;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    flags.pNext = NULL;
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;

    VkMemoryAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = zest_find_memory_type(memory_requirements.memoryTypeBits, property_flags);
    ZEST_ASSERT(alloc_info.memoryTypeIndex != ZEST_INVALID);
    if (ZEST_ENABLE_VALIDATION_LAYER && ZestDevice->api_version == VK_API_VERSION_1_2) {
        alloc_info.pNext = &flags;
    }
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Allocating buffer memory pool, size: %zi type: %i, alignment: %zi, type bits: %i" ZEST_NL, alloc_info.allocationSize, alloc_info.memoryTypeIndex, memory_requirements.alignment, memory_requirements.memoryTypeBits);
    ZEST_VK_CHECK_RESULT(vkAllocateMemory(ZestDevice->logical_device, &alloc_info, &ZestDevice->allocation_callbacks, &buffer->memory));

    if (ZEST_ENABLE_VALIDATION_LAYER && ZestDevice->api_version == VK_API_VERSION_1_2) {
        ZestDevice->use_labels_address_bit = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        const VkDebugUtilsObjectNameInfoEXT buffer_name_info =
        {
            VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, // sType
            NULL,                                               // pNext
            VK_OBJECT_TYPE_BUFFER,                              // objectType
            (uint64_t)buffer->buffer,                           // objectHandle
            name,                                               // pObjectName
        };

        ZestDevice->pfnSetDebugUtilsObjectNameEXT(ZestDevice->logical_device, &buffer_name_info);
    }

    buffer->size = memory_requirements.size;
    buffer->alignment = memory_requirements.alignment;
    buffer->minimum_allocation_size = ZEST__MAX(buffer->alignment, buffer->minimum_allocation_size);
    buffer->memory_type_index = alloc_info.memoryTypeIndex;
    buffer->property_flags = property_flags;
    buffer->usage_flags = usage_flags;
    buffer->descriptor.buffer = buffer->buffer;
    buffer->descriptor.offset = 0;
    buffer->descriptor.range = buffer->size;

    ZEST_VK_CHECK_RESULT(zest__bind_memory(buffer, 0));
}

void zest__create_image_memory_pool(VkDeviceSize size_in_bytes, VkImage image, VkMemoryPropertyFlags property_flags, zest_device_memory_pool buffer) {
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(ZestDevice->logical_device, image, &memory_requirements);

    VkMemoryAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = size_in_bytes;
    alloc_info.memoryTypeIndex = zest_find_memory_type(memory_requirements.memoryTypeBits, property_flags);
    ZEST_ASSERT(alloc_info.memoryTypeIndex != ZEST_INVALID);

    buffer->size = size_in_bytes;
    buffer->alignment = memory_requirements.alignment;
    buffer->minimum_allocation_size = ZEST__MAX(buffer->alignment, buffer->minimum_allocation_size);
    buffer->memory_type_index = alloc_info.memoryTypeIndex;
    buffer->property_flags = property_flags;
    buffer->usage_flags = 0;

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Allocating image memory pool, size: %zi type: %i, alignment: %zi, type bits: %i" ZEST_NL, alloc_info.allocationSize, alloc_info.memoryTypeIndex, memory_requirements.alignment, memory_requirements.memoryTypeBits);
    ZEST_VK_CHECK_RESULT(vkAllocateMemory(ZestDevice->logical_device, &alloc_info, &ZestDevice->allocation_callbacks, &buffer->memory));
}

void zest__on_add_pool(void* user_data, void* block) {
    zest_buffer_allocator_t* pools = (zest_buffer_allocator_t*)user_data;
    zest_buffer_t* buffer = (zest_buffer_t*)block;
    buffer->size = pools->memory_pools[zest_vec_last_index(pools->memory_pools)]->size;
    buffer->memory_pool = pools->memory_pools[zest_vec_last_index(pools->memory_pools)];
    buffer->memory_offset = 0;
}

void zest__on_split_block(void* user_data, zloc_header* block, zloc_header* trimmed_block, zest_size remote_size) {
    zest_buffer_allocator_t* pools = (zest_buffer_allocator_t*)user_data;
    zest_buffer_t* buffer = zloc_BlockUserExtensionPtr(block);
    zest_buffer_t* trimmed_buffer = zloc_BlockUserExtensionPtr(trimmed_block);
    trimmed_buffer->size = buffer->size - remote_size;
    buffer->size = remote_size;
    trimmed_buffer->memory_offset = buffer->memory_offset + buffer->size;
    //--
    trimmed_buffer->memory_pool = buffer->memory_pool;
    trimmed_buffer->buffer_allocator = buffer->buffer_allocator;
    trimmed_buffer->memory_in_use = 0;
    if (pools->buffer_info.property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        buffer->data = (void*)((char*)buffer->memory_pool->mapped + buffer->memory_offset);
        buffer->end = (void*)((char*)buffer->data + buffer->size);
    }
}

void zest__on_reallocation_copy(void* user_data, zloc_header* block, zloc_header* new_block) {
    zest_buffer_allocator_t* pools = (zest_buffer_allocator_t*)user_data;
    zest_buffer_t* buffer = zloc_BlockUserExtensionPtr(block);
    zest_buffer_t* new_buffer = zloc_BlockUserExtensionPtr(new_block);
    if (pools->buffer_info.property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        new_buffer->data = (void*)((char*)new_buffer->memory_pool->mapped + new_buffer->memory_offset);
        new_buffer->end = (void*)((char*)new_buffer->data + new_buffer->size);
        memcpy(new_buffer->data, buffer->data, buffer->size);
    }
}

zloc_size zest__get_minimum_block_size(zest_size pool_size) {
    return pool_size > zloc__MEGABYTE(1) ? pool_size / 128 : 256;
}

zest_device_memory_pool zest__create_vk_memory_pool(zest_buffer_info_t* buffer_info, VkImage image, zest_key key, zest_size minimum_size) {
    zest_device_memory_pool_t blank_buffer_pool = { 0 };
    zest_device_memory_pool buffer_pool = ZEST__NEW(zest_device_memory_pool);
    *buffer_pool = blank_buffer_pool;
    if (image == VK_NULL_HANDLE) {
        zest_buffer_pool_size_t pre_defined_pool_size = zest_GetDeviceBufferPoolSize(buffer_info->usage_flags, buffer_info->property_flags, buffer_info->image_usage_flags);
        if (pre_defined_pool_size.pool_size > 0) {
            buffer_pool->name = pre_defined_pool_size.name;
            buffer_pool->size = pre_defined_pool_size.pool_size > minimum_size ? pre_defined_pool_size.pool_size : zest_GetNextPower(minimum_size + minimum_size / 2);
            buffer_pool->minimum_allocation_size = pre_defined_pool_size.minimum_allocation_size;
        }
        else {
            ZEST_PRINT_WARNING(ZEST_WARNING_COLOR"Allocating memory where no default pool size was found for usage flags: %i and property flags: %i. Defaulting to next power from size + size / 2",
                buffer_info->usage_flags, buffer_info->property_flags);
            buffer_pool->size = zest_GetNextPower(minimum_size + minimum_size / 2);
            buffer_pool->name = "Unknown";
            buffer_pool->minimum_allocation_size = zest__get_minimum_block_size(buffer_pool->size);
        }
        zest__create_device_memory_pool(buffer_pool->size, buffer_info->usage_flags, buffer_info->property_flags, buffer_pool, "");
    }
    else {
        //zest_buffer_pool_size_t pre_defined_pool_size = zest_GetDevicePoolSize(key);
        zest_buffer_pool_size_t pre_defined_pool_size = zest_GetDeviceBufferPoolSize(buffer_info->usage_flags, buffer_info->property_flags, buffer_info->image_usage_flags);
        if (pre_defined_pool_size.pool_size > 0) {
            buffer_pool->name = pre_defined_pool_size.name;
            buffer_pool->size = pre_defined_pool_size.pool_size > minimum_size ? pre_defined_pool_size.pool_size : zest_GetNextPower(minimum_size + minimum_size / 2);
            buffer_pool->minimum_allocation_size = pre_defined_pool_size.minimum_allocation_size;
        }
        else {
            ZEST_PRINT_WARNING(ZEST_WARNING_COLOR"Allocating image memory where no default pool size was found for image usage flags: %i, and property flags: %i. Defaulting to next power from size + size / 2",
                buffer_info->image_usage_flags, buffer_info->property_flags);
            buffer_pool->size = zest_GetNextPower(minimum_size + minimum_size / 2);
            buffer_pool->name = "Unknown";
            buffer_pool->minimum_allocation_size = zest__get_minimum_block_size(buffer_pool->size);
        }
        zest__create_image_memory_pool(buffer_pool->size, image, buffer_info->property_flags, buffer_pool);
    }
    if (buffer_info->property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        zest__map_memory(buffer_pool, VK_WHOLE_SIZE, 0);
    }
    return buffer_pool;
}

void zest__add_remote_range_pool(zest_buffer_allocator buffer_allocator, zest_device_memory_pool buffer_pool) {
    zest_vec_push(buffer_allocator->memory_pools, buffer_pool);
    zloc_size range_pool_size = zloc_CalculateRemoteBlockPoolSize(buffer_allocator->allocator, buffer_pool->size);
    zest_pool_range* range_pool = ZEST__ALLOCATE(range_pool_size);
    zest_vec_push(buffer_allocator->range_pools, range_pool);
    zloc_AddRemotePool(buffer_allocator->allocator, range_pool, range_pool_size, buffer_pool->size);
    ZEST_PRINT_NOTICE(ZEST_NOTICE_COLOR"Note: Ran out of space in the Device memory pool (%s) so adding a new one of size %zu. ", buffer_pool->name, (size_t)buffer_pool->size);
}

void zest__set_buffer_details(zest_buffer_allocator buffer_allocator, zest_buffer_t* buffer, zest_bool is_host_visible) {
    buffer->buffer_allocator = buffer_allocator;
    buffer->memory_in_use = 0;
    if (is_host_visible) {
        buffer->data = (void*)((char*)buffer->memory_pool->mapped + buffer->memory_offset);
        buffer->end = (void*)((char*)buffer->data + buffer->size);
    }
    else {
        buffer->data = ZEST_NULL;
        buffer->end = ZEST_NULL;
    }
}

void zloc__output_buffer_info(void* ptr, size_t size, int free, void* user, int count)
{
    zest_buffer_t* buffer = (zest_buffer_t*)user;
    printf("%i) \t%s range size: \t%zi \tbuffer size: %llu \toffset: %llu \n", count, free ? "free" : "used", size, buffer->size, buffer->memory_offset);
}

zloc__error_codes zloc_VerifyRemoteBlocks(zloc_header* first_block, zloc__block_output output_function, void* user_data) {
    zloc_header* current_block = first_block;
    int count = 0;
    while (!zloc__is_last_block_in_pool(current_block)) {
        void* remote_block = zloc_BlockUserExtensionPtr(current_block);
        if (output_function) {
            output_function(current_block, zloc__block_size(current_block), zloc__is_free_block(current_block), remote_block, ++count);
        }
        zloc_header* last_block = current_block;
        current_block = zloc__next_physical_block(current_block);
        if (last_block != current_block->prev_physical_block) {
            ZEST_ASSERT(0);
            return zloc__PHYSICAL_BLOCK_MISALIGNMENT;
        }
    }
    return zloc__OK;
}

zloc__error_codes zloc_VerifyBlocks(zloc_header* first_block, zloc__block_output output_function, void* user_data) {
    zloc_header* current_block = first_block;
    while (!zloc__is_last_block_in_pool(current_block)) {
        if (output_function) {
            output_function(current_block, zloc__block_size(current_block), zloc__is_free_block(current_block), user_data, 0);
        }
        zloc_header* last_block = current_block;
        current_block = zloc__next_physical_block(current_block);
        if (last_block != current_block->prev_physical_block) {
            ZEST_ASSERT(0);
            return zloc__PHYSICAL_BLOCK_MISALIGNMENT;
        }
    }
    return zloc__OK;
}

zest_uint zloc_CountBlocks(zloc_header* first_block) {
    zloc_header* current_block = first_block;
    int count = 0;
    while (!zloc__is_last_block_in_pool(current_block)) {
        void* remote_block = zloc_BlockUserExtensionPtr(current_block);
        zloc_header* last_block = current_block;
        current_block = zloc__next_physical_block(current_block);
        ZEST_ASSERT(last_block == current_block->prev_physical_block);
        count++;
    }
    return count;
}

zest_buffer zest_CreateBuffer(VkDeviceSize size, zest_buffer_info_t* buffer_info, VkImage image) {
    if (image != VK_NULL_HANDLE) {
        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(ZestDevice->logical_device, image, &memory_requirements);
        buffer_info->memory_type_bits = memory_requirements.memoryTypeBits;
        buffer_info->alignment = memory_requirements.alignment;
    }

    zest_key key = zest_map_hash_ptr(ZestRenderer->buffer_allocators, buffer_info, sizeof(zest_buffer_info_t));
    if (!zest_map_valid_key(ZestRenderer->buffer_allocators, key)) {
        //If an allocator doesn't exist yet for this combination of usage and buffer properties then create one.
        zest_buffer_allocator_t blank_buffer_allocator = { 0 };
        zest_buffer_allocator buffer_allocator = ZEST__NEW(zest_buffer_allocator);
        *buffer_allocator = blank_buffer_allocator;
        buffer_allocator->buffer_info = *buffer_info;
        zest_device_memory_pool buffer_pool = zest__create_vk_memory_pool(buffer_info, image, key, size);

        buffer_allocator->alignment = buffer_pool->alignment;
        zest_vec_push(buffer_allocator->memory_pools, buffer_pool);
        buffer_allocator->allocator = ZEST__ALLOCATE(zloc_AllocatorSize());
        buffer_allocator->allocator = zloc_InitialiseAllocatorForRemote(buffer_allocator->allocator);
        zloc_SetBlockExtensionSize(buffer_allocator->allocator, sizeof(zest_buffer_t));
        zloc_SetMinimumAllocationSize(buffer_allocator->allocator, buffer_pool->minimum_allocation_size);
        zloc_size range_pool_size = zloc_CalculateRemoteBlockPoolSize(buffer_allocator->allocator, buffer_pool->size);
        zest_pool_range* range_pool = ZEST__ALLOCATE(range_pool_size);
        zest_vec_push(buffer_allocator->range_pools, range_pool);
        zest_map_insert_key(ZestRenderer->buffer_allocators, key, buffer_allocator);
        buffer_allocator->allocator->user_data = *zest_map_at_key(ZestRenderer->buffer_allocators, key);
        buffer_allocator->allocator->add_pool_callback = zest__on_add_pool;
        buffer_allocator->allocator->split_block_callback = zest__on_split_block;
        buffer_allocator->allocator->unable_to_reallocate_callback = zest__on_reallocation_copy;
        zloc_AddRemotePool(buffer_allocator->allocator, range_pool, range_pool_size, buffer_pool->size);
    }

    zest_buffer_allocator buffer_allocator = *zest_map_at_key(ZestRenderer->buffer_allocators, key);
    zloc_size adjusted_size = zloc__align_size_up(size, buffer_allocator->alignment);
    zest_buffer_t* buffer = zloc_AllocateRemote(buffer_allocator->allocator, adjusted_size);
    if (buffer) {
        zest__set_buffer_details(buffer_allocator, buffer, buffer_info->property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }
    else {
        zest_device_memory_pool buffer_pool = zest__create_vk_memory_pool(buffer_info, image, key, size);
        ZEST_ASSERT(buffer_pool->alignment == buffer_allocator->alignment);    //The new pool should have the same alignment as the alignment set in the allocator, this
        //would have been set when the first pool was created

        zest__add_remote_range_pool(buffer_allocator, buffer_pool);
        buffer = zloc_AllocateRemote(buffer_allocator->allocator, adjusted_size);
        ZEST_ASSERT(buffer);    //Unable to allocate memory. Out of memory?
        zest__set_buffer_details(buffer_allocator, buffer, buffer_info->property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }

    return buffer;
}

zest_buffer zest_CreateStagingBuffer(VkDeviceSize size, void* data) {
    zest_buffer_info_t buffer_info = zest_CreateStagingBufferInfo();
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    if (data) {
        memcpy(buffer->data, data, size);
    }
    return buffer;
}

zest_buffer zest_CreateIndexBuffer(VkDeviceSize size, zest_buffer staging_buffer) {
    zest_buffer_info_t buffer_info = zest_CreateIndexBufferInfo(0);
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    if (staging_buffer) {
        ZEST_ASSERT(buffer->size >= staging_buffer->size);
        zest_CopyBuffer(staging_buffer, buffer, size);
    }
    return buffer;
}

zest_buffer zest_CreateVertexBuffer(VkDeviceSize size, zest_buffer staging_buffer) {
    zest_buffer_info_t buffer_info = zest_CreateVertexBufferInfo(0);
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    if (staging_buffer) {
        ZEST_ASSERT(buffer->size >= staging_buffer->size);
        zest_CopyBuffer(staging_buffer, buffer, size);
    }
    return buffer;
}

zest_buffer zest_CreateStorageBuffer(VkDeviceSize size, zest_buffer staging_buffer) {
    zest_buffer_info_t buffer_info = zest_CreateStorageBufferInfo();
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    if (staging_buffer) {
        ZEST_ASSERT(buffer->size >= staging_buffer->size);
        zest_CopyBuffer(staging_buffer, buffer, size);
    }
    return buffer;
}

zest_buffer zest_CreateComputeVertexBuffer(VkDeviceSize size, zest_buffer staging_buffer) {
    zest_buffer_info_t buffer_info = zest_CreateComputeVertexBufferInfo();
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    if (staging_buffer) {
        ZEST_ASSERT(buffer->size >= staging_buffer->size);
        zest_CopyBuffer(staging_buffer, buffer, size);
    }
    return buffer;
}

zest_buffer zest_CreateComputeIndexBuffer(VkDeviceSize size, zest_buffer staging_buffer) {
    zest_buffer_info_t buffer_info = zest_CreateComputeVertexBufferInfo();
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    if (staging_buffer) {
        ZEST_ASSERT(buffer->size >= staging_buffer->size);
        zest_CopyBuffer(staging_buffer, buffer, size);
    }
    return buffer;
}

void zest_CopyBuffer(zest_buffer src_buffer, zest_buffer dst_buffer, VkDeviceSize size) {
    ZEST_ASSERT(size <= src_buffer->size);        //size must be less than or equal to the staging buffer size and the device buffer size
    ZEST_ASSERT(size <= dst_buffer->size);
    VkBufferCopy copyInfo = { 0 };
    copyInfo.srcOffset = src_buffer->memory_offset;
    copyInfo.dstOffset = dst_buffer->memory_offset;
    copyInfo.size = size;
    VkCommandBuffer command_buffer = zest__begin_single_time_commands();
    vkCmdCopyBuffer(command_buffer, src_buffer->memory_pool->buffer, dst_buffer->memory_pool->buffer, 1, &copyInfo);
    zest__end_single_time_commands(command_buffer);
}

void zest_CopyBufferCB(VkCommandBuffer command_buffer, zest_buffer src_buffer, zest_buffer dst_buffer, VkDeviceSize size, zest_bool use_barrier) {
    ZEST_ASSERT(size <= src_buffer->size);        //size must be less than or equal to the staging buffer size and the device buffer size
    ZEST_ASSERT(size <= dst_buffer->size);
    VkBufferCopy copyInfo = { 0 };
    copyInfo.srcOffset = src_buffer->memory_offset;
    copyInfo.dstOffset = dst_buffer->memory_offset;
    copyInfo.size = size;
    vkCmdCopyBuffer(command_buffer, src_buffer->memory_pool->buffer, dst_buffer->memory_pool->buffer, 1, &copyInfo);
    if (use_barrier) {
        zest__buffer_write_barrier(command_buffer, dst_buffer);
    }
}

VkResult zest_FlushBuffer(zest_buffer buffer) {
    return zest__flush_memory(buffer->memory_pool, buffer->size, buffer->memory_offset);
}

zest_bool zest_GrowBuffer(zest_buffer* buffer, zest_size unit_size, zest_size minimum_bytes) {
    ZEST_ASSERT(unit_size);
    if (minimum_bytes && (*buffer)->size > minimum_bytes) {
        return ZEST_FALSE;
    }
    zest_size units = (*buffer)->size / unit_size;
    zest_size new_size = (units ? units + units / 2 : 8) * unit_size;
    new_size = ZEST__MAX(new_size, minimum_bytes);
    if (new_size <= (*buffer)->size) {
        return ZEST_FALSE;
    }
    zest_buffer_allocator_t* buffer_allocator = (*buffer)->buffer_allocator;
    zest_size memory_in_use = (*buffer)->memory_in_use;
    zest_buffer new_buffer = zloc_ReallocateRemote(buffer_allocator->allocator, *buffer, new_size);
    if (new_buffer) {
        new_buffer->buffer_allocator = (*buffer)->buffer_allocator;
        *buffer = new_buffer;
        zest__set_buffer_details(buffer_allocator, *buffer, buffer_allocator->buffer_info.property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        new_buffer->memory_in_use = memory_in_use;
    }
    else {
        //Create a new memory pool and try again
        zest_device_memory_pool buffer_pool = zest__create_vk_memory_pool(&buffer_allocator->buffer_info, ZEST_NULL, 0, new_size);
        ZEST_ASSERT(buffer_pool->alignment == buffer_allocator->alignment);    //The new pool should have the same alignment as the alignment set in the allocator, this
        //would have been set when the first pool was created
        zest__add_remote_range_pool(buffer_allocator, buffer_pool);
        new_buffer = zloc_ReallocateRemote(buffer_allocator->allocator, *buffer, new_size);
        ZEST_ASSERT(new_buffer);    //Unable to allocate memory. Out of memory?
        *buffer = new_buffer;
        zest__set_buffer_details(buffer_allocator, *buffer, buffer_allocator->buffer_info.property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        new_buffer->memory_in_use = memory_in_use;
    }
    return new_buffer ? ZEST_TRUE : ZEST_FALSE;
}

zest_bool zest_GrowDescriptorBuffer(zest_descriptor_buffer buffer, zest_size unit_size, zest_size minimum_bytes) {
    zest_bool grow_result = zest_GrowBuffer(&buffer->buffer[buffer->all_frames_in_flight ? ZEST_FIF : 0], unit_size, minimum_bytes);
    if (grow_result) {
        for (ZEST_EACH_FIF_i) {
            if (buffer->all_frames_in_flight) {
                buffer->descriptor_info[i].buffer = buffer->buffer[i]->memory_pool->buffer;
                buffer->descriptor_info[i].offset = buffer->buffer[i]->memory_offset;
                buffer->descriptor_info[i].range = buffer->buffer[i]->size;
            }
            else {
                buffer->descriptor_info[i].buffer = buffer->buffer[0]->memory_pool->buffer;
                buffer->descriptor_info[i].offset = buffer->buffer[0]->memory_offset;
                buffer->descriptor_info[i].range = buffer->buffer[0]->size;
            }
        }
    }
    return grow_result;
}

zest_buffer_info_t zest_CreateIndexBufferInfo(zest_bool cpu_visible) {
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (cpu_visible) {
        buffer_info.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
    else {
        buffer_info.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
    return buffer_info;
}

zest_buffer_info_t zest_CreateVertexBufferInfo(zest_bool cpu_visible) {
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (cpu_visible) {
        buffer_info.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
    else {
        buffer_info.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
    return buffer_info;
}

zest_buffer_info_t zest_CreateCPUVisibleStorageBufferInfo() {
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_info.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return buffer_info;
}

zest_buffer_info_t zest_CreateStorageBufferInfo() {
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_info.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return buffer_info;
}

zest_buffer_info_t zest_CreateComputeVertexBufferInfo() {
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_info.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return buffer_info;
}

zest_buffer_info_t zest_CreateComputeIndexBufferInfo() {
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_info.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return buffer_info;
}

zest_buffer_info_t zest_CreateStagingBufferInfo() {
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return buffer_info;
}

void zest_FreeBuffer(zest_buffer buffer) {
    if (!buffer) return;    //Nothing to free
    zloc_FreeRemote(buffer->buffer_allocator->allocator, buffer);
}

void zest_FreeDescriptorBuffer(zest_descriptor_buffer buffer) {
    if (buffer->all_frames_in_flight) {
        for (ZEST_EACH_FIF_i) {
            if (buffer->buffer[i]) {
                zloc_FreeRemote(buffer->buffer[i]->buffer_allocator->allocator, buffer->buffer[i]);
            }
        }
    }
    else {
        if (buffer->buffer[0]) {
            zloc_FreeRemote(buffer->buffer[0]->buffer_allocator->allocator, buffer->buffer[0]);
        }
    }
}

VkDeviceMemory zest_GetBufferDeviceMemory(zest_buffer buffer) {
    return buffer->memory_pool->memory;
}

VkBuffer* zest_GetBufferDeviceBuffer(zest_buffer buffer) {
    return &buffer->memory_pool->buffer;
}

void zest_AddCopyCommand(zest_buffer_uploader_t* uploader, zest_buffer_t* source_buffer, zest_buffer_t* target_buffer, VkDeviceSize target_offset) {
    if (uploader->flags & zest_buffer_upload_flag_initialised)
        ZEST_ASSERT(uploader->source_buffer == source_buffer && uploader->target_buffer == target_buffer);    //Buffer uploads must be to the same source and target ids with each copy. Use a separate BufferUpload for each combination of source and target buffers

    uploader->source_buffer = source_buffer;
    uploader->target_buffer = target_buffer;
    uploader->flags |= zest_buffer_upload_flag_initialised;

    VkBufferCopy buffer_info = { 0 };
    buffer_info.srcOffset = source_buffer->memory_offset;
    buffer_info.dstOffset = target_buffer->memory_offset + target_offset;
    buffer_info.size = source_buffer->memory_in_use;
    zest_vec_push(uploader->buffer_copies, buffer_info);
    target_buffer->memory_in_use = source_buffer->memory_in_use;
}

zest_bool zest_UploadBuffer(zest_buffer_uploader_t* uploader, VkCommandBuffer command_buffer) {
    if (!zest_vec_size(uploader->buffer_copies)) {
        return ZEST_FALSE;
    }

    vkCmdCopyBuffer(command_buffer, *zest_GetBufferDeviceBuffer(uploader->source_buffer), *zest_GetBufferDeviceBuffer(uploader->target_buffer), zest_vec_size(uploader->buffer_copies), uploader->buffer_copies);
    zest__buffer_write_barrier(command_buffer, uploader->target_buffer);

    zest_vec_free(uploader->buffer_copies);
    uploader->flags = 0;
    zest_buffer_uploader_t fresh_uploader = { 0 };
    *uploader = fresh_uploader;

    return ZEST_TRUE;
}

zest_buffer_pool_size_t zest_GetDevicePoolSize(zest_key hash) {
    if (zest_map_valid_key(ZestDevice->pool_sizes, hash)) {
        return *zest_map_at_key(ZestDevice->pool_sizes, hash);
    }
    zest_buffer_pool_size_t pool_size = { 0 };
    return pool_size;
}

zest_buffer_pool_size_t zest_GetDeviceBufferPoolSize(VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, VkImageUsageFlags image_flags) {
    zest_buffer_usage_t usage;
    usage.usage_flags = usage_flags;
    usage.property_flags = property_flags;
    usage.image_flags = image_flags;
    zest_key usage_hash = zest_Hash(ZestHasher, &usage, sizeof(zest_buffer_usage_t), ZEST_HASH_SEED);
    if (zest_map_valid_key(ZestDevice->pool_sizes, usage_hash)) {
        return *zest_map_at_key(ZestDevice->pool_sizes, usage_hash);
    }
    zest_buffer_pool_size_t pool_size = { 0 };
    return pool_size;
}

zest_buffer_pool_size_t zest_GetDeviceImagePoolSize(const char* name) {
    zest_key usage_hash = zest_Hash(ZestHasher, name, strlen(name), ZEST_HASH_SEED);
    if (zest_map_valid_key(ZestDevice->pool_sizes, usage_hash)) {
        return *zest_map_at_key(ZestDevice->pool_sizes, usage_hash);
    }
    zest_buffer_pool_size_t pool_size = { 0 };
    return pool_size;
}

void zest_SetDeviceBufferPoolSize(const char* name, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, zest_size minimum_allocation, zest_size pool_size) {
    ZEST_ASSERT(pool_size);                    //Must set a pool size
    ZEST_ASSERT(ZEST__POW2(pool_size));        //Pool size must be a power of 2
    zest_index size_index = ZEST__MAX(zloc__scan_forward(pool_size) - 20, 0);
    minimum_allocation = ZEST__MAX(minimum_allocation, 64 << size_index);
    zest_buffer_usage_t usage = { 0 };
    usage.usage_flags = usage_flags;
    usage.property_flags = property_flags;
    zest_key usage_hash = zest_Hash(ZestHasher, &usage, sizeof(zest_buffer_usage_t), ZEST_HASH_SEED);
    zest_buffer_pool_size_t pool_sizes;
    pool_sizes.name = name;
    pool_sizes.minimum_allocation_size = minimum_allocation;
    pool_sizes.pool_size = pool_size;
    zest_map_insert_key(ZestDevice->pool_sizes, usage_hash, pool_sizes);
}

void zest_SetDeviceImagePoolSize(const char* name, VkImageUsageFlags image_flags, VkMemoryPropertyFlags property_flags, zest_size minimum_allocation, zest_size pool_size) {
    ZEST_ASSERT(pool_size);                    //Must set a pool size
    ZEST_ASSERT(ZEST__POW2(pool_size));        //Pool size must be a power of 2
    zest_index size_index = ZEST__MAX(zloc__scan_forward(pool_size) - 20, 0);
    minimum_allocation = ZEST__MAX(minimum_allocation, 64 << size_index);
    zest_buffer_usage_t usage = { 0 };
    usage.image_flags = image_flags;
    usage.property_flags = property_flags;
    zest_key usage_hash = zest_Hash(ZestHasher, &usage, sizeof(zest_buffer_usage_t), ZEST_HASH_SEED);
    zest_buffer_pool_size_t pool_sizes;
    pool_sizes.name = name;
    pool_sizes.minimum_allocation_size = minimum_allocation;
    pool_sizes.pool_size = pool_size;
    zest_map_insert_key(ZestDevice->pool_sizes, usage_hash, pool_sizes);
}
// --End Vulkan Buffer Management

// --Renderer and related functions
void zest__initialise_renderer(zest_create_info_t* create_info) {
    ZestRenderer->flags |= create_info->flags & zest_init_flag_enable_vsync;
#if defined(ZEST_ATOMICS)
    ZestRenderer->lock_texture_reprocess_queue = ATOMIC_VAR_INIT(0);
#else
    ZestRenderer->lock_texture_reprocess_queue = 0;
#endif
    zest_SetText(&ZestRenderer->shader_path_prefix, create_info->shader_path_prefix);
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create swap chain" ZEST_NL);
    zest__create_swapchain();
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create swap chain image views" ZEST_NL);
    zest__create_swapchain_image_views();

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create standard render passes" ZEST_NL);
    zest__make_standard_render_passes();

    if (ZEST__FLAGGED(create_info->flags, zest_init_flag_use_depth_buffer)) {
        ZestRenderer->final_render_pass = *zest_map_at(ZestRenderer->render_passes, "Render pass present");
        ZestRenderer->flags |= zest_renderer_flag_has_depth_buffer;
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create depth passes" ZEST_NL);
        ZestRenderer->depth_resource_buffer = zest__create_depth_resources();
    }
    else {
        ZestRenderer->final_render_pass = *zest_map_at(ZestRenderer->render_passes, "Render pass present no depth");
    }
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create swap chain frame buffers" ZEST_NL);
    zest__create_swap_chain_frame_buffers(ZEST__FLAGGED(ZestRenderer->flags, zest_renderer_flag_has_depth_buffer));
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create sync objects" ZEST_NL);
    zest__create_sync_objects();
    ZestRenderer->push_constants.screen_resolution.x = 1.f / ZestRenderer->swapchain_extent.width;
    ZestRenderer->push_constants.screen_resolution.y = 1.f / ZestRenderer->swapchain_extent.height;

    ZestRenderer->standard_uniform_buffer = zest_CreateUniformBuffer("Standard 2d Uniform Buffer", sizeof(zest_uniform_buffer_data_t));

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create descriptor pools" ZEST_NL);
    zest__create_descriptor_pools(create_info->pool_counts, create_info->max_descriptor_pool_sets);
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create descriptor layouts" ZEST_NL);
    zest__make_standard_descriptor_layouts();
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create pipeline cache" ZEST_NL);
    zest__create_pipeline_cache();
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Compile shaders" ZEST_NL);
    zest__compile_builtin_shaders();
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create standard pipelines" ZEST_NL);
    zest__prepare_standard_pipelines();

    VkFenceCreateInfo fence_info = { 0 };
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (ZEST_EACH_FIF_i) {
        ZEST_VK_CHECK_RESULT(vkCreateFence(ZestDevice->logical_device, &fence_info, &ZestDevice->allocation_callbacks, &ZestRenderer->fif_fence[i]));
        zest_Update2dUniformBufferFIF(i);
    }
}

void zest__create_swapchain() {
    zest_swapchain_support_details_t swapchain_support = zest__query_swapchain_support(ZestDevice->physical_device);

    VkSurfaceFormatKHR surfaceFormat = zest__choose_swapchain_format(swapchain_support.formats);
    VkPresentModeKHR presentMode = zest_choose_present_mode(swapchain_support.present_modes, ZestRenderer->flags & zest_renderer_flag_vsync_enabled);
    VkExtent2D extent = zest_choose_swap_extent(&swapchain_support.capabilities);
    ZestRenderer->dpi_scale = (float)extent.width / (float)ZestApp->window->window_width;

    ZestRenderer->swapchain_image_format = surfaceFormat.format;
    ZestRenderer->swapchain_extent = extent;
    ZestRenderer->window_extent.width = ZestApp->window->window_width;
    ZestRenderer->window_extent.height = ZestApp->window->window_height;

    zest_uint image_count = swapchain_support.capabilities.minImageCount + 1;

    if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = ZestApp->window->surface;

    createInfo.minImageCount = image_count;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.preTransform = swapchain_support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    ZEST_VK_CHECK_RESULT(vkCreateSwapchainKHR(ZestDevice->logical_device, &createInfo, &ZestDevice->allocation_callbacks, &ZestRenderer->swapchain));

    vkGetSwapchainImagesKHR(ZestDevice->logical_device, ZestRenderer->swapchain, &image_count, ZEST_NULL);
    zest_vec_resize(ZestRenderer->swapchain_images, image_count);
    vkGetSwapchainImagesKHR(ZestDevice->logical_device, ZestRenderer->swapchain, &image_count, ZestRenderer->swapchain_images);
}

VkPresentModeKHR zest_choose_present_mode(VkPresentModeKHR* available_present_modes, zest_bool use_vsync) {
    VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

    if (use_vsync) {
        return best_mode;
    }

    for (zest_foreach_i(available_present_modes)) {
        if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return available_present_modes[i];
        }
        else if (available_present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            return available_present_modes[i];
        }
    }

    return best_mode;
}

VkExtent2D zest_choose_swap_extent(VkSurfaceCapabilitiesKHR* capabilities) {
    /*
    Currently forcing getting the current window size each time because if your app starts up on a different
     monitor with a different dpi then it can crash because the DPI doesn't match the initial surface capabilities
     that were found.
    if (capabilities->currentExtent.width != ZEST_U32_MAX_VALUE) {
        return capabilities->currentExtent;
    }
    else {
     */
    int fb_width = 0, fb_height = 0;
    int window_width = 0, window_height = 0;
    ZestRenderer->get_window_size_callback(ZestApp->user_data, &fb_width, &fb_height, &window_width, &window_height);

    VkExtent2D actual_extent = {
        .width = (zest_uint)(fb_width),
        .height = (zest_uint)(fb_height)
    };

    actual_extent.width = ZEST__MAX(capabilities->minImageExtent.width, ZEST__MIN(capabilities->maxImageExtent.width, actual_extent.width));
    actual_extent.height = ZEST__MAX(capabilities->minImageExtent.height, ZEST__MIN(capabilities->maxImageExtent.height, actual_extent.height));

    return actual_extent;
    //}
}

VkSurfaceFormatKHR zest__choose_swapchain_format(VkSurfaceFormatKHR* available_formats) {
    if (zest_vec_size(available_formats) == 1 && available_formats[0].format == VK_FORMAT_UNDEFINED) {
        VkSurfaceFormatKHR format = {
            .format = VK_FORMAT_B8G8R8A8_UNORM,
            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        };
        return format;
    }

    VkFormat format = ZestApp->create_info.color_format == VK_FORMAT_R8G8B8A8_UNORM ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_B8G8R8A8_SRGB;

    for (zest_foreach_i(available_formats)) {
        if (available_formats[i].format == format && available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_formats[i];
        }
    }

    return available_formats[0];
}

void zest__create_pipeline_cache() {
    VkPipelineCacheCreateInfo pipeline_cache_create_info = { 0 };
    pipeline_cache_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    ZEST_VK_CHECK_RESULT(vkCreatePipelineCache(ZestDevice->logical_device, &pipeline_cache_create_info, &ZestDevice->allocation_callbacks, &ZestRenderer->pipeline_cache));
}

void zest__cleanup_swapchain() {
    vkDestroyImageView(ZestDevice->logical_device, ZestRenderer->final_render_pass_depth_attachment.view, &ZestDevice->allocation_callbacks);
    vkDestroyImage(ZestDevice->logical_device, ZestRenderer->final_render_pass_depth_attachment.image, &ZestDevice->allocation_callbacks);

    for (zest_foreach_i(ZestRenderer->swapchain_frame_buffers)) {
        vkDestroyFramebuffer(ZestDevice->logical_device, ZestRenderer->swapchain_frame_buffers[i], &ZestDevice->allocation_callbacks);
    }

    for (zest_foreach_i(ZestRenderer->swapchain_image_views)) {
        vkDestroyImageView(ZestDevice->logical_device, ZestRenderer->swapchain_image_views[i], &ZestDevice->allocation_callbacks);
    }

    vkDestroySwapchainKHR(ZestDevice->logical_device, ZestRenderer->swapchain, &ZestDevice->allocation_callbacks);
}

void zest__destroy_pipeline_set(zest_pipeline_t* p) {
    vkDestroyPipeline(ZestDevice->logical_device, p->pipeline, &ZestDevice->allocation_callbacks);
    vkDestroyPipelineLayout(ZestDevice->logical_device, p->pipeline_layout, &ZestDevice->allocation_callbacks);
}

void zest__cleanup_pipelines() {
    for (zest_map_foreach_i(ZestRenderer->pipelines)) {
        zest_pipeline pipeline = *zest_map_at_index(ZestRenderer->pipelines, i);
        zest__destroy_pipeline_set(pipeline);
    }
}

void zest__cleanup_textures() {
    for (zest_map_foreach_i(ZestRenderer->textures)) {
        zest_texture texture = *zest_map_at_index(ZestRenderer->textures, i);
        if (texture->flags & zest_texture_flag_textures_ready) {
            zest__cleanup_texture(texture);
        }
    }
}

void zest__cleanup_framebuffer(zest_frame_buffer_t* frame_buffer) {
    if (frame_buffer->resolve_buffer.view != VK_NULL_HANDLE) vkDestroyImageView(ZestDevice->logical_device, frame_buffer->resolve_buffer.view, &ZestDevice->allocation_callbacks);
    if (frame_buffer->resolve_buffer.image != VK_NULL_HANDLE) vkDestroyImage(ZestDevice->logical_device, frame_buffer->resolve_buffer.image, &ZestDevice->allocation_callbacks);
    if (frame_buffer->color_buffer.view != VK_NULL_HANDLE) vkDestroyImageView(ZestDevice->logical_device, frame_buffer->color_buffer.view, &ZestDevice->allocation_callbacks);
    if (frame_buffer->depth_buffer.view != VK_NULL_HANDLE) vkDestroyImageView(ZestDevice->logical_device, frame_buffer->depth_buffer.view, &ZestDevice->allocation_callbacks);
    if (frame_buffer->color_buffer.image != VK_NULL_HANDLE) vkDestroyImage(ZestDevice->logical_device, frame_buffer->color_buffer.image, &ZestDevice->allocation_callbacks);
    if (frame_buffer->depth_buffer.image != VK_NULL_HANDLE) vkDestroyImage(ZestDevice->logical_device, frame_buffer->depth_buffer.image, &ZestDevice->allocation_callbacks);
    if (frame_buffer->sampler != VK_NULL_HANDLE) vkDestroySampler(ZestDevice->logical_device, frame_buffer->sampler, &ZestDevice->allocation_callbacks);
    if (frame_buffer->device_frame_buffer != VK_NULL_HANDLE) vkDestroyFramebuffer(ZestDevice->logical_device, frame_buffer->device_frame_buffer, &ZestDevice->allocation_callbacks);
}

void zest__cleanup_renderer() {
    zest__cleanup_swapchain();
    vkDestroyDescriptorPool(ZestDevice->logical_device, ZestRenderer->descriptor_pool, &ZestDevice->allocation_callbacks);

    zest__cleanup_pipelines();
    zest_map_clear(ZestRenderer->pipelines);

    zest__cleanup_textures();

    for (zest_map_foreach_i(ZestRenderer->render_targets)) {
        zest_render_target render_target = *zest_map_at_index(ZestRenderer->render_targets, i);
        for (zest_index fif = 0; fif != render_target->frames_in_flight; ++fif) {
            zest_CleanUpRenderTarget(render_target, fif);
        }
    }
    zest_map_clear(ZestRenderer->render_targets);

    for (zest_map_foreach_i(ZestRenderer->render_passes)) {
        zest_render_pass render_pass = *zest_map_at_index(ZestRenderer->render_passes, i);
        vkDestroyRenderPass(ZestDevice->logical_device, render_pass->render_pass, &ZestDevice->allocation_callbacks);
    }
    zest_map_clear(ZestRenderer->render_passes);

    for (zest_map_foreach_i(ZestRenderer->draw_routines)) {
        zest_draw_routine routine = *zest_map_at_index(ZestRenderer->draw_routines, i);
        if (routine->clean_up_callback) {
            routine->clean_up_callback(routine);
        }
    }

    zest_map_clear(ZestRenderer->draw_routines);

    for (zest_map_foreach_i(ZestRenderer->descriptor_layouts)) {
        zest_descriptor_set_layout layout = *zest_map_at_index(ZestRenderer->descriptor_layouts, i);
        vkDestroyDescriptorSetLayout(ZestDevice->logical_device, layout->descriptor_layout, &ZestDevice->allocation_callbacks);
    }
    zest_map_clear(ZestRenderer->descriptor_layouts);


    for (ZEST_EACH_FIF_i) {
        vkDestroySemaphore(ZestDevice->logical_device, ZestRenderer->semaphores[i].outgoing, &ZestDevice->allocation_callbacks);
        vkDestroyFence(ZestDevice->logical_device, ZestRenderer->fif_fence[i], &ZestDevice->allocation_callbacks);
    }

    for (zest_map_foreach_i(ZestRenderer->buffer_allocators)) {
        zest_buffer_allocator buffer_allocator = *zest_map_at_index(ZestRenderer->buffer_allocators, i);
        for (zest_foreach_j(buffer_allocator->memory_pools)) {
            zest__destroy_memory(buffer_allocator->memory_pools[j]);
        }
    }

    for (zest_map_foreach_i(ZestRenderer->command_queues)) {
        zest_command_queue command_queue = *zest_map_at_index(ZestRenderer->command_queues, i);
        zest__cleanup_command_queue(command_queue);
    }
    zest__cleanup_command_queue(&ZestRenderer->empty_queue);

    for (zest_map_foreach_i(ZestRenderer->computes)) {
        zest_compute compute = *zest_map_at_index(ZestRenderer->computes, i);
        zest__clean_up_compute(compute);
    }

}

void zest__recreate_swapchain() {
    int fb_width = 0, fb_height = 0;
    int window_width = 0, window_height = 0;
    ZestRenderer->flags |= zest_renderer_flag_swapchain_was_recreated;
    while (fb_width == 0 || fb_height == 0) {
        ZestRenderer->get_window_size_callback(ZestApp->user_data, &fb_width, &fb_height, &window_width, &window_height);
        if (fb_width == 0 || fb_height == 0) {
            zest__os_poll_events();
        }
    }

    zest__update_window_size(ZestApp->window, window_width, window_height);
    ZestRenderer->push_constants.screen_resolution.x = 1.f / fb_width;
    ZestRenderer->push_constants.screen_resolution.y = 1.f / fb_height;

    zest_WaitForIdleDevice();

    zest__cleanup_swapchain();
    zest__cleanup_pipelines();

    for (ZEST_EACH_FIF_i) {
        vkDestroySemaphore(ZestDevice->logical_device, ZestRenderer->semaphores[i].outgoing, &ZestDevice->allocation_callbacks);
    }

    zest__create_swapchain();
    zest__create_swapchain_image_views();
    zest__create_sync_objects();

    if (ZEST__FLAGGED(ZestRenderer->flags, zest_renderer_flag_has_depth_buffer)) {
        zest_FreeBuffer(ZestRenderer->depth_resource_buffer);
        ZestRenderer->depth_resource_buffer = zest__create_depth_resources();
    }

    zest__create_swap_chain_frame_buffers(ZEST__FLAGGED(ZestRenderer->flags, zest_renderer_flag_has_depth_buffer));
    for (zest_map_foreach_i(ZestRenderer->render_targets)) {
        zest_render_target render_target = *zest_map_at_index(ZestRenderer->render_targets, i);
        for (int fif = 0; fif != render_target->frames_in_flight; ++fif) {
            zest_RecreateRenderTargetResources(render_target, fif);
        }
    }
    VkExtent2D extent;
    extent.width = fb_width;
    extent.height = fb_height;
    for (zest_map_foreach_i(ZestRenderer->pipelines)) {
        zest_pipeline pipeline = *zest_map_at_index(ZestRenderer->pipelines, i);
        if (!pipeline->rebuild_pipeline_function) {
            zest__rebuild_pipeline(pipeline);
        }
        else {
            pipeline->rebuild_pipeline_function(pipeline);
        }
    }

    for (zest_map_foreach_i(ZestRenderer->draw_routines)) {
        zest_draw_routine draw_routine = *zest_map_at_index(ZestRenderer->draw_routines, i);
        if (!draw_routine->update_resolution_callback && draw_routine->draw_data != ZEST_NULL) {
            zest_layer layer = (zest_layer)draw_routine->draw_data;
            zest__update_instance_layer_resolution(layer);
        }
        else if (draw_routine->update_resolution_callback)
            draw_routine->update_resolution_callback(draw_routine);
    }

    for (zest_map_foreach_j(ZestRenderer->command_queues)) {
        zest_command_queue command_queue = *zest_map_at_index(ZestRenderer->command_queues, j);
        for (ZEST_EACH_FIF_i) {
            zest_vec_clear(command_queue->semaphores[i].fif_incoming_semaphores);
            zest_vec_clear(command_queue->fif_wait_stage_flags[i]);
        }
        if (ZEST__FLAGGED(command_queue->flags, zest_command_queue_flag_present_dependency)) {
            zest_ConnectPresentToCommandQueue(command_queue, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        }
        for (zest_foreach_k(command_queue->draw_commands)) {
            zest_command_queue_draw_commands draw_commands = command_queue->draw_commands[k];
            if (draw_commands->render_target) {
                draw_commands->viewport = draw_commands->render_target->viewport;
            }
            else {
                if (draw_commands->viewport_type == zest_render_viewport_type_scale_with_window) {
                    draw_commands->viewport.extent.width = (zest_uint)((float)fb_width * draw_commands->viewport_scale.x);
                    draw_commands->viewport.extent.height = (zest_uint)((float)fb_height * draw_commands->viewport_scale.y);
                }
            }
        }
    }

    for (ZEST_EACH_FIF_i) {
        zest_vec_clear(ZestRenderer->empty_queue.semaphores[i].fif_incoming_semaphores);
        zest_vec_clear(ZestRenderer->empty_queue.fif_wait_stage_flags[i]);
    }

    zest_ConnectPresentToCommandQueue(&ZestRenderer->empty_queue, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    for (zest_foreach_i(ZestRenderer->empty_queue.draw_commands)) {
        zest_command_queue_draw_commands draw_commands = ZestRenderer->empty_queue.draw_commands[i];
        draw_commands->viewport.extent.width = fb_width;
        draw_commands->viewport.extent.height = fb_height;
    }

}

void zest__create_swapchain_image_views() {
    zest_vec_resize(ZestRenderer->swapchain_image_views, zest_vec_size(ZestRenderer->swapchain_images));

    for (zest_foreach_i(ZestRenderer->swapchain_images)) {
        zest_vec_set(ZestRenderer->swapchain_image_views, i, zest__create_image_view(ZestRenderer->swapchain_images[i], ZestRenderer->swapchain_image_format, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 1));
    }
}

void zest__make_standard_render_passes() {
    zest__add_render_pass("Render pass present", zest__create_render_pass(ZestRenderer->swapchain_image_format, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ATTACHMENT_LOAD_OP_CLEAR, ZEST_TRUE));
    zest__add_render_pass("Render pass standard", zest__create_render_pass(ZestRenderer->swapchain_image_format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, ZEST_TRUE));
    zest__add_render_pass("Render pass standard no clear", zest__create_render_pass(ZestRenderer->swapchain_image_format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_DONT_CARE, ZEST_TRUE));
    zest__add_render_pass("Render pass present no depth", zest__create_render_pass(ZestRenderer->swapchain_image_format, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ATTACHMENT_LOAD_OP_CLEAR, ZEST_FALSE));
    zest__add_render_pass("Render pass standard no depth", zest__create_render_pass(ZestRenderer->swapchain_image_format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, ZEST_FALSE));
    zest__add_render_pass("Render pass standard no clear no depth", zest__create_render_pass(ZestRenderer->swapchain_image_format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_DONT_CARE, ZEST_FALSE));
}

zest_render_pass zest__add_render_pass(const char* name, VkRenderPass render_pass) {
    zest_render_pass_t blank_r = { 0 };
    zest_render_pass r = ZEST__NEW(zest_render_pass);
    *r = blank_r;
    r->render_pass = render_pass;
    r->name = name;
    zest_map_insert(ZestRenderer->render_passes, name, r);
    return r;
}

zest_buffer_t* zest__create_depth_resources() {
    VkFormat depth_format = zest__find_depth_format();

    zest_buffer_t* buffer = zest__create_image(ZestRenderer->swapchain_extent.width, ZestRenderer->swapchain_extent.height, 1, VK_SAMPLE_COUNT_1_BIT, depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &ZestRenderer->final_render_pass_depth_attachment.image);
    ZestRenderer->final_render_pass_depth_attachment.view = zest__create_image_view(ZestRenderer->final_render_pass_depth_attachment.image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, 1, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 1);

    zest__transition_image_layout(ZestRenderer->final_render_pass_depth_attachment.image, depth_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, 1);

    return buffer;
}

void zest__create_swap_chain_frame_buffers(zest_bool depth_buffer) {
    zest_vec_resize(ZestRenderer->swapchain_frame_buffers, zest_vec_size(ZestRenderer->swapchain_image_views));

    for (zest_foreach_i(ZestRenderer->swapchain_image_views)) {
        VkFramebufferCreateInfo frame_buffer_info = { 0 };

        VkImageView* attachments = 0;
        zest_vec_push(attachments, ZestRenderer->swapchain_image_views[i]);
        if (depth_buffer) {
            zest_vec_push(attachments, ZestRenderer->final_render_pass_depth_attachment.view);
        }
        frame_buffer_info.attachmentCount = zest_vec_size(attachments);
        frame_buffer_info.pAttachments = attachments;
        frame_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frame_buffer_info.renderPass = ZestRenderer->final_render_pass->render_pass;
        frame_buffer_info.width = ZestRenderer->swapchain_extent.width;
        frame_buffer_info.height = ZestRenderer->swapchain_extent.height;
        frame_buffer_info.layers = 1;

        ZEST_VK_CHECK_RESULT(vkCreateFramebuffer(ZestDevice->logical_device, &frame_buffer_info, &ZestDevice->allocation_callbacks, &ZestRenderer->swapchain_frame_buffers[i]));

        zest_vec_free(attachments);
    }
}

void zest__create_sync_objects() {
    VkSemaphoreCreateInfo semaphore_info = { 0 };
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (ZEST_EACH_FIF_i) {
        ZEST_VK_CHECK_RESULT(vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, &ZestDevice->allocation_callbacks, &ZestRenderer->semaphores[i].outgoing));
    }
}

zest_uniform_buffer zest_CreateUniformBuffer(const char* name, zest_size uniform_struct_size) {
    zest_uniform_buffer uniform_buffer = ZEST__NEW(zest_descriptor_buffer);
    uniform_buffer->all_frames_in_flight = ZEST_TRUE;
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_info.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (ZEST_EACH_FIF_i) {
        uniform_buffer->buffer[i] = zest_CreateBuffer(uniform_struct_size, &buffer_info, ZEST_NULL);
        uniform_buffer->descriptor_info[i].buffer = *zest_GetBufferDeviceBuffer(uniform_buffer->buffer[i]);
        uniform_buffer->descriptor_info[i].offset = uniform_buffer->buffer[i]->memory_offset;
        uniform_buffer->descriptor_info[i].range = uniform_struct_size;
    }
    return zest__add_uniform_buffer(name, uniform_buffer);
}

void zest_Update2dUniformBuffer() {
    zest_uniform_buffer_data_t* ubo_ptr = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(ZestRenderer->standard_uniform_buffer);
    zest_vec3 eye = { .x = 0.f, .y = 0.f, .z = -1 };
    zest_vec3 center = { 0 };
    zest_vec3 up = { .x = 0.f, .y = -1.f, .z = 0.f };
    ubo_ptr->view = zest_LookAt(eye, center, up);
    ubo_ptr->proj = zest_Ortho(0.f, zest_SwapChainWidthf() / ZestRenderer->dpi_scale, 0.f, -zest_SwapChainHeightf() / ZestRenderer->dpi_scale, -1000.f, 1000.f);
    ubo_ptr->screen_size.x = zest_ScreenWidthf();
    ubo_ptr->screen_size.y = zest_ScreenHeightf();
    ubo_ptr->millisecs = zest_Millisecs() % 1000;
}

void zest_Update2dUniformBufferFIF(zest_index fif) {
    zest_uniform_buffer_data_t* ubo_ptr = (zest_uniform_buffer_data_t*)zest_GetUniformBufferDataFIF(ZestRenderer->standard_uniform_buffer, fif);
    zest_vec3 eye = { .x = 0.f, .y = 0.f, .z = -1 };
    zest_vec3 center = { 0 };
    zest_vec3 up = { .x = 0.f, .y = -1.f, .z = 0.f };
    ubo_ptr->view = zest_LookAt(eye, center, up);
    ubo_ptr->proj = zest_Ortho(0.f, (float)ZestRenderer->swapchain_extent.width, 0.f, -(float)ZestRenderer->swapchain_extent.height, -1000.f, 1000.f);
    ubo_ptr->screen_size.x = zest_ScreenWidthf();
    ubo_ptr->screen_size.y = zest_ScreenHeightf();
    ubo_ptr->millisecs = zest_Millisecs() % 1000;
}

zest_descriptor_buffer zest_CreateDescriptorBuffer(zest_buffer_info_t* buffer_info, zest_size size, zest_bool all_frames_in_flight) {
    zest_descriptor_buffer_t blank_buffer = { 0 };
    zest_descriptor_buffer descriptor_buffer = ZEST__NEW(zest_descriptor_buffer);
    *descriptor_buffer = blank_buffer;
    for (int i = 0; i != (all_frames_in_flight ? ZEST_MAX_FIF : 1); ++i) {
        descriptor_buffer->buffer[i] = zest_CreateBuffer(size, buffer_info, ZEST_NULL);
        descriptor_buffer->descriptor_info[i].buffer = *zest_GetBufferDeviceBuffer(descriptor_buffer->buffer[i]);
        descriptor_buffer->descriptor_info[i].offset = descriptor_buffer->buffer[i]->memory_offset;
        descriptor_buffer->descriptor_info[i].range = size;
    }
    descriptor_buffer->all_frames_in_flight = all_frames_in_flight;
    return descriptor_buffer;
}

zest_descriptor_buffer zest_CreateStorageDescriptorBuffer(zest_size size, zest_bool all_frames_in_flight) {
    zest_buffer_info_t buffer_info = zest_CreateStorageBufferInfo();
    zest_descriptor_buffer_t blank_buffer = { 0 };
    zest_descriptor_buffer descriptor_buffer = ZEST__NEW(zest_descriptor_buffer);
    *descriptor_buffer = blank_buffer;
    for (int i = 0; i != (all_frames_in_flight ? ZEST_MAX_FIF : 1); ++i) {
        descriptor_buffer->buffer[i] = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
        descriptor_buffer->descriptor_info[i].buffer = *zest_GetBufferDeviceBuffer(descriptor_buffer->buffer[i]);
        descriptor_buffer->descriptor_info[i].offset = descriptor_buffer->buffer[i]->memory_offset;
        descriptor_buffer->descriptor_info[i].range = size;
    }
    descriptor_buffer->all_frames_in_flight = all_frames_in_flight;
    return descriptor_buffer;
}

zest_descriptor_buffer zest_CreateCPUVisibleStorageDescriptorBuffer(zest_size size, zest_bool all_frames_in_flight) {
    zest_buffer_info_t buffer_info = zest_CreateCPUVisibleStorageBufferInfo();
    zest_descriptor_buffer_t blank_buffer = { 0 };
    zest_descriptor_buffer descriptor_buffer = ZEST__NEW(zest_descriptor_buffer);
    *descriptor_buffer = blank_buffer;
    for (int i = 0; i != (all_frames_in_flight ? ZEST_MAX_FIF : 1); ++i) {
        descriptor_buffer->buffer[i] = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
        descriptor_buffer->descriptor_info[i].buffer = *zest_GetBufferDeviceBuffer(descriptor_buffer->buffer[i]);
        descriptor_buffer->descriptor_info[i].offset = descriptor_buffer->buffer[i]->memory_offset;
        descriptor_buffer->descriptor_info[i].range = size;
    }
    descriptor_buffer->all_frames_in_flight = all_frames_in_flight;
    return descriptor_buffer;
}

zest_descriptor_buffer zest_CreateVertexDescriptorBuffer(zest_size size, zest_bool all_frames_in_flight, zest_bool cpu_visible) {
    zest_buffer_info_t buffer_info = zest_CreateVertexBufferInfo(cpu_visible);
    zest_descriptor_buffer_t blank_buffer = { 0 };
    zest_descriptor_buffer descriptor_buffer = ZEST__NEW(zest_descriptor_buffer);
    *descriptor_buffer = blank_buffer;
    for (int i = 0; i != (all_frames_in_flight ? ZEST_MAX_FIF : 1); ++i) {
        descriptor_buffer->buffer[i] = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
        descriptor_buffer->descriptor_info[i].buffer = *zest_GetBufferDeviceBuffer(descriptor_buffer->buffer[i]);
        descriptor_buffer->descriptor_info[i].offset = descriptor_buffer->buffer[i]->memory_offset;
        descriptor_buffer->descriptor_info[i].range = size;
    }
    descriptor_buffer->all_frames_in_flight = all_frames_in_flight;
    return descriptor_buffer;
}

zest_descriptor_buffer zest_CreateIndexDescriptorBuffer(zest_size size, zest_bool all_frames_in_flight, zest_bool cpu_visible) {
    zest_buffer_info_t buffer_info = zest_CreateIndexBufferInfo(cpu_visible);
    zest_descriptor_buffer_t blank_buffer = { 0 };
    zest_descriptor_buffer descriptor_buffer = ZEST__NEW(zest_descriptor_buffer);
    *descriptor_buffer = blank_buffer;
    for (int i = 0; i != (all_frames_in_flight ? ZEST_MAX_FIF : 1); ++i) {
        descriptor_buffer->buffer[i] = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
        descriptor_buffer->descriptor_info[i].buffer = *zest_GetBufferDeviceBuffer(descriptor_buffer->buffer[i]);
        descriptor_buffer->descriptor_info[i].offset = descriptor_buffer->buffer[i]->memory_offset;
        descriptor_buffer->descriptor_info[i].range = size;
    }
    descriptor_buffer->all_frames_in_flight = all_frames_in_flight;
    return descriptor_buffer;
}

zest_descriptor_buffer zest_CreateComputeVertexDescriptorBuffer(zest_size size, zest_bool all_frames_in_flight) {
    zest_buffer_info_t buffer_info = zest_CreateComputeVertexBufferInfo();
    zest_descriptor_buffer_t blank_buffer = { 0 };
    zest_descriptor_buffer descriptor_buffer = ZEST__NEW(zest_descriptor_buffer);
    *descriptor_buffer = blank_buffer;
    for (int i = 0; i != (all_frames_in_flight ? ZEST_MAX_FIF : 1); ++i) {
        descriptor_buffer->buffer[i] = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
        descriptor_buffer->descriptor_info[i].buffer = *zest_GetBufferDeviceBuffer(descriptor_buffer->buffer[i]);
        descriptor_buffer->descriptor_info[i].offset = descriptor_buffer->buffer[i]->memory_offset;
        descriptor_buffer->descriptor_info[i].range = size;
    }
    descriptor_buffer->all_frames_in_flight = all_frames_in_flight;
    return descriptor_buffer;
}

zest_descriptor_buffer zest_CreateComputeIndexDescriptorBuffer(zest_size size, zest_bool all_frames_in_flight) {
    zest_buffer_info_t buffer_info = zest_CreateComputeIndexBufferInfo();
    zest_descriptor_buffer_t blank_buffer = { 0 };
    zest_descriptor_buffer descriptor_buffer = ZEST__NEW(zest_descriptor_buffer);
    *descriptor_buffer = blank_buffer;
    for (int i = 0; i != (all_frames_in_flight ? ZEST_MAX_FIF : 1); ++i) {
        descriptor_buffer->buffer[i] = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
        descriptor_buffer->descriptor_info[i].buffer = *zest_GetBufferDeviceBuffer(descriptor_buffer->buffer[i]);
        descriptor_buffer->descriptor_info[i].offset = descriptor_buffer->buffer[i]->memory_offset;
        descriptor_buffer->descriptor_info[i].range = size;
    }
    descriptor_buffer->all_frames_in_flight = all_frames_in_flight;
    return descriptor_buffer;
}

zest_buffer zest_GetBufferFromDescriptorBuffer(zest_descriptor_buffer descriptor_buffer) {
    return descriptor_buffer->buffer[descriptor_buffer->all_frames_in_flight ? ZEST_FIF : 0];
}

zest_uniform_buffer zest__add_uniform_buffer(const char* name, zest_uniform_buffer buffer) {
    zest_map_insert(ZestRenderer->uniform_buffers, name, buffer);
    return buffer;
}

void zest__create_descriptor_pools(zest_map_descriptor_pool_sizes pool_sizes, zest_uint max_sets) {
    VkDescriptorPoolSize pool_size;

    if (!zest_map_valid_key(pool_sizes, VK_DESCRIPTOR_TYPE_SAMPLER)) {
        pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLER;
        pool_size.descriptorCount = 100;
        zest_map_insert_key(pool_sizes, pool_size.type, pool_size);
    }

    if (!zest_map_valid_key(pool_sizes, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) {
        pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_size.descriptorCount = 100;
        zest_map_insert_key(pool_sizes, pool_size.type, pool_size);
    }

    if (!zest_map_valid_key(pool_sizes, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)) {
        pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        pool_size.descriptorCount = 100;
        zest_map_insert_key(pool_sizes, pool_size.type, pool_size);
    }

    if (!zest_map_valid_key(pool_sizes, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)) {
        pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_size.descriptorCount = 100;
        zest_map_insert_key(pool_sizes, pool_size.type, pool_size);
    }

    if (!zest_map_valid_key(pool_sizes, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)) {
        pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        pool_size.descriptorCount = 10;
        zest_map_insert_key(pool_sizes, pool_size.type, pool_size);
    }

    if (!zest_map_valid_key(pool_sizes, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)) {
        pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        pool_size.descriptorCount = 10;
        zest_map_insert_key(pool_sizes, pool_size.type, pool_size);
    }

    if (!zest_map_valid_key(pool_sizes, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)) {
        pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_size.descriptorCount = 10;
        zest_map_insert_key(pool_sizes, pool_size.type, pool_size);
    }

    if (!zest_map_valid_key(pool_sizes, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)) {
        pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        pool_size.descriptorCount = 10;
        zest_map_insert_key(pool_sizes, pool_size.type, pool_size);
    }

    if (!zest_map_valid_key(pool_sizes, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)) {
        pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        pool_size.descriptorCount = 10;
        zest_map_insert_key(pool_sizes, pool_size.type, pool_size);
    }

    if (!zest_map_valid_key(pool_sizes, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)) {
        pool_size.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        pool_size.descriptorCount = 10;
        zest_map_insert_key(pool_sizes, pool_size.type, pool_size);
    }

    VkDescriptorPoolCreateInfo pool_info = { 0 };
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = (zest_uint)(zest_map_size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes.data;
    pool_info.maxSets = max_sets;

    ZEST_VK_CHECK_RESULT(vkCreateDescriptorPool(ZestDevice->logical_device, &pool_info, &ZestDevice->allocation_callbacks, &ZestRenderer->descriptor_pool));
}

zest_render_pass zest_GetRenderPass(const char* name) { ZEST_ASSERT(zest_map_valid_name(ZestRenderer->render_passes, name)); return *zest_map_at(ZestRenderer->render_passes, name); }

void zest__make_standard_descriptor_layouts() {
    zest_AddDescriptorLayout("Standard 1 uniform 1 sampler", zest_CreateDescriptorSetLayout(1, 1, 0));
    zest_AddDescriptorLayout("Polygon layout (no sampler)", zest_CreateDescriptorSetLayout(1, 0, 0));
    zest_AddDescriptorLayout("Render target layout", zest_CreateDescriptorSetLayout(0, 1, 0));
    zest_AddDescriptorLayout("Ribbon 2d layout", zest_CreateDescriptorSetLayout(1, 0, 1));
}

zest_descriptor_set_layout zest_AddDescriptorLayout(const char* name, VkDescriptorSetLayout layout) {
    zest_descriptor_set_layout l = ZEST__NEW(zest_descriptor_set_layout);
    l->name = name;
    l->descriptor_layout = layout;
    zest_map_insert(ZestRenderer->descriptor_layouts, name, l);
    return l;
}

VkDescriptorSetLayout zest_CreateDescriptorSetLayout(zest_uint uniforms, zest_uint samplers, zest_uint storage_buffers) {
    VkDescriptorSetLayoutBinding* bindings = 0;

    for (int c = 0; c != uniforms; ++c) {
        zest_vec_push(bindings, zest_CreateUniformLayoutBinding(c));
    }

    for (int c = 0; c != samplers; ++c) {
        zest_vec_push(bindings, zest_CreateSamplerLayoutBinding(c + uniforms));
    }

    for (int c = 0; c != storage_buffers; ++c) {
        zest_vec_push(bindings, zest_CreateStorageLayoutBinding(c + samplers + uniforms));
    }

    VkDescriptorSetLayout layout = zest_CreateDescriptorSetLayoutWithBindings(zest_vec_size(bindings), bindings);
    zest_vec_free(bindings);
    return layout;
}

VkDescriptorSetLayout zest_CreateDescriptorSetLayoutWithBindings(zest_uint count, VkDescriptorSetLayoutBinding* bindings) {
    ZEST_ASSERT(bindings);    //must have bindings to create the layout

    VkDescriptorSetLayoutCreateInfo layoutInfo = { 0 };
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = (zest_uint)count;
    layoutInfo.pBindings = bindings;

    VkDescriptorSetLayout layout;

    ZEST_VK_CHECK_RESULT(vkCreateDescriptorSetLayout(ZestDevice->logical_device, &layoutInfo, &ZestDevice->allocation_callbacks, &layout) != VK_SUCCESS);

    return layout;
}

VkDescriptorSetLayoutBinding zest_CreateDescriptorLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, zest_uint binding, zest_uint descriptorCount) {
    VkDescriptorSetLayoutBinding set_layout_binding = { 0 };
    set_layout_binding.descriptorType = type;
    set_layout_binding.stageFlags = stageFlags;
    set_layout_binding.binding = binding;
    set_layout_binding.descriptorCount = descriptorCount;
    return set_layout_binding;;
}

VkDescriptorSetLayoutBinding zest_CreateUniformLayoutBinding(zest_uint binding) {

    VkDescriptorSetLayoutBinding view_layout_binding = { 0 };
    view_layout_binding.binding = binding;
    view_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    view_layout_binding.descriptorCount = 1;
    view_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    view_layout_binding.pImmutableSamplers = ZEST_NULL;

    return view_layout_binding;
}

VkDescriptorSetLayoutBinding zest_CreateSamplerLayoutBinding(zest_uint binding) {

    VkDescriptorSetLayoutBinding sampler_layout_binding = { 0 };
    sampler_layout_binding.binding = binding;
    sampler_layout_binding.descriptorCount = 1;
    sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_layout_binding.pImmutableSamplers = ZEST_NULL;
    sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    return sampler_layout_binding;
}

VkDescriptorSetLayoutBinding zest_CreateStorageLayoutBinding(zest_uint binding) {

    VkDescriptorSetLayoutBinding storage_layout_binding = { 0 };
    storage_layout_binding.binding = binding;
    storage_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    storage_layout_binding.descriptorCount = 1;
    storage_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    storage_layout_binding.pImmutableSamplers = ZEST_NULL;

    return storage_layout_binding;

}

VkWriteDescriptorSet zest_CreateBufferDescriptorWriteWithType(VkDescriptorSet descriptor_set, VkDescriptorBufferInfo* view_buffer_info, zest_uint dst_binding, VkDescriptorType type) {
    VkWriteDescriptorSet write = { 0 };
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_set;
    write.dstBinding = dst_binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pBufferInfo = view_buffer_info;
    return write;
}

VkWriteDescriptorSet zest_CreateImageDescriptorWriteWithType(VkDescriptorSet descriptor_set, VkDescriptorImageInfo* view_image_info, zest_uint dst_binding, VkDescriptorType type) {
    VkWriteDescriptorSet write = { 0 };
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_set;
    write.dstBinding = dst_binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pImageInfo = view_image_info;
    return write;
}

zest_descriptor_set_builder_t zest_NewDescriptorSetBuilder() {
    zest_descriptor_set_builder_t builder = { 0 };
    return builder;
}

void zest_AddBuilderDescriptorWriteImage(zest_descriptor_set_builder_t* builder, VkDescriptorImageInfo* view_image_info, zest_uint dst_binding, VkDescriptorType type) {
    VkWriteDescriptorSet write = { 0 };
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = 0;
    write.dstBinding = dst_binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pImageInfo = view_image_info;
    zest_vec_push(builder->writes, write);
}

void zest_AddBuilderDescriptorWriteBuffer(zest_descriptor_set_builder_t* builder, VkDescriptorBufferInfo* view_buffer_info, zest_uint dst_binding, VkDescriptorType type) {
    VkWriteDescriptorSet write = { 0 };
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = 0;
    write.dstBinding = dst_binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pBufferInfo = view_buffer_info;
    zest_vec_push(builder->writes, write);
}

void zest_AddBuilderDescriptorWriteImages(zest_descriptor_set_builder_t* builder, zest_uint image_count, VkDescriptorImageInfo* view_image_infos, zest_uint dst_binding, VkDescriptorType type) {
    VkWriteDescriptorSet write = { 0 };
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = 0;
    write.dstBinding = dst_binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = image_count;
    write.pImageInfo = view_image_infos;
    zest_vec_push(builder->writes, write);
}

void zest_AddBuilderDescriptorWriteBuffers(zest_descriptor_set_builder_t* builder, zest_uint buffer_count, VkDescriptorBufferInfo* view_buffer_infos, zest_uint dst_binding, VkDescriptorType type) {
    VkWriteDescriptorSet write = { 0 };
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = 0;
    write.dstBinding = dst_binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = buffer_count;
    write.pBufferInfo = view_buffer_infos;
    zest_vec_push(builder->writes, write);
}

zest_descriptor_set_t zest_BuildDescriptorSet(zest_descriptor_set_builder_t* builder, zest_descriptor_set_layout layout) {
    ZEST_ASSERT(zest_vec_size(builder));        //Nothing to build.  Call AddBuilder functions to add descriptor writes first.
    //Note that calling this function will free the builder descriptor so you will need to add to the builder again
    zest_descriptor_set_t set = { 0 };
    for (ZEST_EACH_FIF_i) {
        zest_AllocateDescriptorSet(ZestRenderer->descriptor_pool, layout->descriptor_layout, &set.descriptor_set[i]);
        for (zest_foreach_j(builder->writes)) {
            VkWriteDescriptorSet write = builder->writes[j];
            write.dstSet = set.descriptor_set[i];
            zest_vec_push(set.descriptor_writes[i], write);
        }
    }
    for (ZEST_EACH_FIF_i) {
        zest_UpdateDescriptorSet(set.descriptor_writes[i]);
    }
    zest_vec_free(builder->writes);
    return set;
}

void zest_AllocateDescriptorSet(VkDescriptorPool descriptor_pool, VkDescriptorSetLayout descriptor_layout, VkDescriptorSet* descriptor_set) {
    VkDescriptorSetAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &descriptor_layout;

    ZEST_VK_CHECK_RESULT(vkAllocateDescriptorSets(ZestDevice->logical_device, &alloc_info, descriptor_set));
}

void zest_UpdateDescriptorSet(VkWriteDescriptorSet* descriptor_writes) {
    vkUpdateDescriptorSets(ZestDevice->logical_device, zest_vec_size(descriptor_writes), descriptor_writes, 0, ZEST_NULL);
}

void zest__add_pipeline_descriptor_write(zest_pipeline pipeline, VkWriteDescriptorSet set, zest_index fif) {
    zest_vec_push(pipeline->descriptor_writes[fif], set);
}

void zest_MakePipelineDescriptorWrites(zest_pipeline pipeline) {
    zest_uint binding = 0;
    for (ZEST_EACH_FIF_i) {
        zest_AllocateDescriptorSet(ZestRenderer->descriptor_pool, pipeline->descriptor_layout->descriptor_layout, &pipeline->descriptor_set[i]);
        zest_vec_clear(pipeline->descriptor_writes[i]);
        if (pipeline->uniforms) {
            if (!pipeline->uniform_buffer) {
                zest__add_pipeline_descriptor_write(pipeline, zest_CreateBufferDescriptorWriteWithType(pipeline->descriptor_set[i], zest_GetUniformBufferInfo("Standard 2d Uniform Buffer", i), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER), i);
            }
            else {
                zest__add_pipeline_descriptor_write(pipeline, zest_CreateBufferDescriptorWriteWithType(pipeline->descriptor_set[i], &pipeline->uniform_buffer->descriptor_info[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER), i);
            }
            binding++;
        }
    }
    for (ZEST_EACH_FIF_i) {
        zest_UpdateDescriptorSet(pipeline->descriptor_writes[i]);
    }
}

zest_pipeline zest__create_pipeline() {
    zest_pipeline_t blank_pipeline = {
        .create_info = {0},
        .pipeline_template = {0},
        .descriptor_layout = 0,
        .pipeline = VK_NULL_HANDLE,
        .pipeline_layout = VK_NULL_HANDLE,
        .uniform_buffer = 0,
        .uniforms = 1,
        .push_constant_size = 0,
        .textures = 0,
        .rebuild_pipeline_function = ZEST_NULL,
        .flags = zest_pipeline_set_flag_none,
    };
    zest_pipeline pipeline = ZEST__NEW(zest_pipeline);
    *pipeline = blank_pipeline;
    return pipeline;
}

zest_pipeline_template_create_info_t zest_CreatePipelineTemplateCreateInfo(void) {
    zest_pipeline_template_create_info_t create_info = {
        .descriptorSetLayout = zest_GetDescriptorSetLayout("Standard 1 uniform 1 sampler"),
        .no_vertex_input = ZEST_FALSE,
        .attributeDescriptions = 0,
        .bindingDescriptions = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };
    zest_SetText(&create_info.fragShaderFunctionName, "main");
    zest_SetText(&create_info.vertShaderFunctionName, "main");
    zest_SetText(&create_info.shader_path_prefix, ZestRenderer->shader_path_prefix.str);
    zest_vec_push(create_info.dynamicStates, VK_DYNAMIC_STATE_VIEWPORT);
    zest_vec_push(create_info.dynamicStates, VK_DYNAMIC_STATE_SCISSOR);
    create_info.viewport.offset.x = 0;
    create_info.viewport.offset.y = 0;
    create_info.viewport.extent = zest_GetSwapChainExtent();
    return create_info;
}

void zest_SetPipelineTemplateVertShader(zest_pipeline_template_create_info_t* create_info, const char* file, const char* prefix) {
    zest_SetText(&create_info->vertShaderFile, file);
    if (prefix) {
        zest_SetText(&create_info->shader_path_prefix, prefix);
    }
}

void zest_SetPipelineTemplateFragShader(zest_pipeline_template_create_info_t* create_info, const char* file, const char* prefix) {
    zest_SetText(&create_info->fragShaderFile, file);
    if (prefix) {
        zest_SetText(&create_info->shader_path_prefix, prefix);
    }
}

void zest_SetPipelineTemplateShader(zest_pipeline_template_create_info_t* create_info, const char* file, const char* prefix) {
    zest_SetText(&create_info->fragShaderFile, file);
    zest_SetText(&create_info->vertShaderFile, file);
    if (prefix) {
        zest_SetText(&create_info->shader_path_prefix, prefix);
    }
}

void zest_AddPipelineTemplatePushConstantRange(zest_pipeline_template_create_info_t* create_info, VkPushConstantRange range) {
    zest_vec_push(create_info->pushConstantRange, range);
}

void zest_SetPipelineUniformBuffer(zest_pipeline pipeline, zest_uniform_buffer uniform_buffer) {
    pipeline->uniform_buffer = uniform_buffer;
}

VkShaderStageFlags zest_PipelinePushConstantStageFlags(zest_pipeline pipeline, zest_uint index) {
    ZEST_ASSERT(index < pipeline->pipeline_template.pipelineLayoutInfo.pushConstantRangeCount);    //Index must not be outside the range of the number of push constants
    return pipeline->pipeline_template.pipelineLayoutInfo.pPushConstantRanges[index].stageFlags;
}

zest_uint zest_PipelinePushConstantSize(zest_pipeline pipeline, zest_uint index) {
    ZEST_ASSERT(index < pipeline->pipeline_template.pipelineLayoutInfo.pushConstantRangeCount);    //Index must not be outside the range of the number of push constants
    return pipeline->pipeline_template.pipelineLayoutInfo.pPushConstantRanges[index].size;
}

zest_uint zest_PipelinePushConstantOffset(zest_pipeline pipeline, zest_uint index) {
    ZEST_ASSERT(index < pipeline->pipeline_template.pipelineLayoutInfo.pushConstantRangeCount); //Index must not be outside the range of the number of push constants
    return pipeline->pipeline_template.pipelineLayoutInfo.pPushConstantRanges[index].offset;
}

VkVertexInputBindingDescription zest_AddVertexInputBindingDescription(zest_pipeline_template_create_info_t* create_info, zest_uint binding, zest_uint stride, VkVertexInputRate input_rate) {
    for (zest_foreach_i(create_info->bindingDescriptions)) {
        //You already have a binding with that index in the bindindDescriptions array
        //Maybe you copied a template with zest_CopyTemplateFromPipeline but didn't call zest_ClearVertexInputBindingDescriptions on the copy before
        //adding your own
        ZEST_ASSERT(binding != create_info->bindingDescriptions[i].binding);
    }
    VkVertexInputBindingDescription input_binding_description = { 0 };
    input_binding_description.binding = binding;
    input_binding_description.stride = stride;
    input_binding_description.inputRate = input_rate;
    zest_vec_push(create_info->bindingDescriptions, input_binding_description);
    zest_size size = zest_vec_size(create_info->bindingDescriptions);
    return input_binding_description;
}

void zest_ClearVertexInputBindingDescriptions(zest_pipeline_template_create_info_t* create_info) {
    zest_vec_clear(create_info->bindingDescriptions);
}

VkVertexInputAttributeDescription zest_CreateVertexInputDescription(zest_uint binding, zest_uint location, VkFormat format, zest_uint offset) {
    VkVertexInputAttributeDescription input_attribute_description = { 0 };
    input_attribute_description.location = location;
    input_attribute_description.binding = binding;
    input_attribute_description.format = format;
    input_attribute_description.offset = offset;
    return input_attribute_description;
}

VkPipelineColorBlendAttachmentState zest_AdditiveBlendState() {
    VkPipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    return color_blend_attachment;
}

VkPipelineColorBlendAttachmentState zest_AdditiveBlendState2() {
    VkPipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    return color_blend_attachment;
}

VkPipelineColorBlendAttachmentState zest_AlphaOnlyBlendState() {
    VkPipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    return color_blend_attachment;
}

VkPipelineColorBlendAttachmentState zest_AlphaBlendState() {
    VkPipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    return color_blend_attachment;
}

VkPipelineColorBlendAttachmentState zest_PreMultiplyBlendState() {
    VkPipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    return color_blend_attachment;
}

VkPipelineColorBlendAttachmentState zest_PreMultiplyBlendStateForSwap() {
    VkPipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    return color_blend_attachment;
}

VkPipelineColorBlendAttachmentState zest_MaxAlphaBlendState() {
    VkPipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    return color_blend_attachment;
}

VkPipelineColorBlendAttachmentState zest_ImGuiBlendState() {
    VkPipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    return color_blend_attachment;
}

void zest_BindPipeline(zest_pipeline pipeline, VkDescriptorSet descriptor_set) {
    vkCmdBindPipeline(ZestRenderer->current_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    vkCmdBindDescriptorSets(ZestRenderer->current_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0, 1, &descriptor_set, 0, 0);
}

void zest_BindComputePipeline(zest_compute compute, zest_index shader_index) {
    //If you get an error here, then check if you're manually running the compute shader in a compute_command_buffer_callback, in which case you should
    //call zest_BingComputePipelineCB where you can specify the command buffer to use (ie, the one passed into the callback).
    vkCmdBindPipeline(ZestRenderer->current_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipelines[shader_index]);
    vkCmdBindDescriptorSets(ZestRenderer->current_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipeline_layout, 0, 1, &compute->descriptor_set[ZEST_FIF], 0, 0);
}

void zest_BindComputePipelineCB(VkCommandBuffer command_buffer, zest_compute compute, zest_index shader_index) {
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipelines[shader_index]);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipeline_layout, 0, 1, &compute->descriptor_set[ZEST_FIF], 0, 0);
}

void zest_BindPipelineCB(VkCommandBuffer command_buffer, zest_pipeline_t* pipeline, VkDescriptorSet descriptor_set) {
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0, 1, &descriptor_set, 0, 0);
}

void zest__set_pipeline_template(zest_pipeline_template_t* pipeline_template, zest_pipeline_template_create_info_t* create_info) {
    pipeline_template->vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    pipeline_template->vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_template->vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    pipeline_template->vertShaderStageInfo.pName = create_info->vertShaderFunctionName.str;

    pipeline_template->fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_template->fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipeline_template->fragShaderStageInfo.pName = create_info->fragShaderFunctionName.str;

    if (zest_TextLength(&create_info->shader_path_prefix)) {
        zest_SetTextf(&pipeline_template->vertShaderFile, "%s%s", create_info->shader_path_prefix, create_info->vertShaderFile.str);
        zest_SetTextf(&pipeline_template->fragShaderFile, "%s%s", create_info->shader_path_prefix, create_info->fragShaderFile.str);
    }
    else {
        zest_SetTextf(&pipeline_template->vertShaderFile, "%s", create_info->vertShaderFile.str);
        zest_SetTextf(&pipeline_template->fragShaderFile, "%s", create_info->fragShaderFile.str);
    }

    pipeline_template->inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipeline_template->inputAssembly.topology = create_info->topology;
    pipeline_template->inputAssembly.primitiveRestartEnable = VK_FALSE;

    if (!create_info->no_vertex_input) {
        ZEST_ASSERT(zest_vec_size(create_info->bindingDescriptions));    //If the pipeline is set to have vertex input, then you must add bindingDescriptions. You can use zest_AddVertexInputBindingDescription for this
        pipeline_template->vertexInputInfo.vertexBindingDescriptionCount = (zest_uint)zest_vec_size(create_info->bindingDescriptions);
        pipeline_template->vertexInputInfo.pVertexBindingDescriptions = create_info->bindingDescriptions;
        pipeline_template->vertexInputInfo.vertexAttributeDescriptionCount = (zest_uint)zest_vec_size(create_info->attributeDescriptions);
        pipeline_template->vertexInputInfo.pVertexAttributeDescriptions = create_info->attributeDescriptions;
    }

    pipeline_template->viewport.x = 0.0f;
    pipeline_template->viewport.y = 0.0f;
    pipeline_template->viewport.width = (float)create_info->viewport.extent.width;
    pipeline_template->viewport.height = (float)create_info->viewport.extent.height;
    pipeline_template->viewport.minDepth = 0.0f;
    pipeline_template->viewport.maxDepth = 1.0f;

    pipeline_template->scissor = create_info->viewport;

    pipeline_template->viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    pipeline_template->viewportState.viewportCount = 1;
    pipeline_template->viewportState.pViewports = &pipeline_template->viewport;
    pipeline_template->viewportState.scissorCount = 1;
    pipeline_template->viewportState.pScissors = &pipeline_template->scissor;

    pipeline_template->rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipeline_template->rasterizer.depthClampEnable = VK_FALSE;
    pipeline_template->rasterizer.rasterizerDiscardEnable = VK_FALSE;
    pipeline_template->rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    pipeline_template->rasterizer.lineWidth = 1.0f;
    pipeline_template->rasterizer.cullMode = VK_CULL_MODE_NONE;
    pipeline_template->rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    pipeline_template->rasterizer.depthBiasEnable = VK_FALSE;

    pipeline_template->multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipeline_template->multisampling.sampleShadingEnable = VK_FALSE;
    pipeline_template->multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    pipeline_template->depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pipeline_template->depthStencil.depthTestEnable = VK_TRUE;
    pipeline_template->depthStencil.depthWriteEnable = VK_TRUE;
    pipeline_template->depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    pipeline_template->depthStencil.depthBoundsTestEnable = VK_FALSE;
    pipeline_template->depthStencil.stencilTestEnable = VK_FALSE;

    pipeline_template->colorBlendAttachment = zest_AlphaBlendState();

    pipeline_template->colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pipeline_template->colorBlending.logicOpEnable = VK_FALSE;
    pipeline_template->colorBlending.logicOp = VK_LOGIC_OP_COPY;
    pipeline_template->colorBlending.attachmentCount = 1;
    pipeline_template->colorBlending.pAttachments = &pipeline_template->colorBlendAttachment;
    pipeline_template->colorBlending.blendConstants[0] = 0.0f;
    pipeline_template->colorBlending.blendConstants[1] = 0.0f;
    pipeline_template->colorBlending.blendConstants[2] = 0.0f;
    pipeline_template->colorBlending.blendConstants[3] = 0.0f;

    pipeline_template->pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_template->pipelineLayoutInfo.setLayoutCount = 1;
    pipeline_template->pipelineLayoutInfo.pSetLayouts = &create_info->descriptorSetLayout->descriptor_layout;

    if (!zest_vec_empty(create_info->pushConstantRange)) {
        pipeline_template->pipelineLayoutInfo.pPushConstantRanges = create_info->pushConstantRange;
        pipeline_template->pipelineLayoutInfo.pushConstantRangeCount = zest_vec_size(create_info->pushConstantRange);
    }

    pipeline_template->renderPass = create_info->renderPass;

    pipeline_template->dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pipeline_template->dynamicState.dynamicStateCount = (zest_uint)(zest_vec_size(create_info->dynamicStates));
    pipeline_template->dynamicState.pDynamicStates = create_info->dynamicStates;
    pipeline_template->dynamicState.flags = 0;
}

void zest_BuildPipeline(zest_pipeline pipeline) {
    ZEST_VK_CHECK_RESULT(vkCreatePipelineLayout(ZestDevice->logical_device, &pipeline->pipeline_template.pipelineLayoutInfo, &ZestDevice->allocation_callbacks, &pipeline->pipeline_layout));

    if (!pipeline->pipeline_template.vertShaderFile.str || !pipeline->pipeline_template.fragShaderFile.str) {
        ZEST_ASSERT(0);        //You must specify a vertex and frag shader file to load
    }

    VkShaderModule vert_shader_module = { 0 };
    VkShaderModule frag_shader_module = { 0 };
    if (zest_map_valid_name(ZestRenderer->shaders, pipeline->pipeline_template.vertShaderFile.str)) {
        zest_shader vert_shader = *zest_map_at(ZestRenderer->shaders, pipeline->pipeline_template.vertShaderFile.str);
		vert_shader_module = zest__create_shader_module(vert_shader->spv);
		pipeline->pipeline_template.vertShaderStageInfo.module = vert_shader_module;
    }
    else {
		zest_shader vert_shader = zest_AddShaderFromSPVFile(pipeline->pipeline_template.vertShaderFile.str, shaderc_vertex_shader);
		ZEST_ASSERT(vert_shader);        //Failed to load the shader file, make sure it exists at the location
		vert_shader_module = zest__create_shader_module(vert_shader->spv);
		pipeline->pipeline_template.vertShaderStageInfo.module = vert_shader_module;
    }

    if (zest_map_valid_name(ZestRenderer->shaders, pipeline->pipeline_template.fragShaderFile.str)) {
        zest_shader frag_shader = *zest_map_at(ZestRenderer->shaders, pipeline->pipeline_template.fragShaderFile.str);
		frag_shader_module = zest__create_shader_module(frag_shader->spv);
		pipeline->pipeline_template.fragShaderStageInfo.module = frag_shader_module;
    }
    else {
		zest_shader frag_shader = zest_AddShaderFromSPVFile(pipeline->pipeline_template.fragShaderFile.str, shaderc_fragment_shader);
		ZEST_ASSERT(frag_shader);        //Failed to load the shader file, make sure it exists at the location
		frag_shader_module = zest__create_shader_module(frag_shader->spv);
		zest_vec_free(frag_shader);
		pipeline->pipeline_template.fragShaderStageInfo.module = frag_shader_module;
    }

    pipeline->pipeline_template.vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline->pipeline_template.vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    pipeline->pipeline_template.vertShaderStageInfo.pName = pipeline->create_info.vertShaderFunctionName.str;

    pipeline->pipeline_template.fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline->pipeline_template.fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipeline->pipeline_template.fragShaderStageInfo.pName = pipeline->create_info.fragShaderFunctionName.str;

    VkPipelineShaderStageCreateInfo shaderStages[2] = { pipeline->pipeline_template.vertShaderStageInfo, pipeline->pipeline_template.fragShaderStageInfo };

    VkGraphicsPipelineCreateInfo pipeline_info = { 0 };
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shaderStages;
    pipeline_info.pVertexInputState = &pipeline->pipeline_template.vertexInputInfo;
    pipeline_info.pInputAssemblyState = &pipeline->pipeline_template.inputAssembly;
    pipeline_info.pViewportState = &pipeline->pipeline_template.viewportState;
    pipeline_info.pRasterizationState = &pipeline->pipeline_template.rasterizer;
    pipeline_info.pMultisampleState = &pipeline->pipeline_template.multisampling;
    pipeline_info.pColorBlendState = &pipeline->pipeline_template.colorBlending;
    pipeline_info.pDepthStencilState = &pipeline->pipeline_template.depthStencil;
    pipeline_info.layout = pipeline->pipeline_layout;
    pipeline_info.renderPass = pipeline->pipeline_template.renderPass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    if (pipeline->pipeline_template.dynamicState.dynamicStateCount) {
        pipeline_info.pDynamicState = &pipeline->pipeline_template.dynamicState;
    }

    ZEST_VK_CHECK_RESULT(vkCreateGraphicsPipelines(ZestDevice->logical_device, ZestRenderer->pipeline_cache, 1, &pipeline_info, &ZestDevice->allocation_callbacks, &pipeline->pipeline));

    vkDestroyShaderModule(ZestDevice->logical_device, frag_shader_module, &ZestDevice->allocation_callbacks);
    vkDestroyShaderModule(ZestDevice->logical_device, vert_shader_module, &ZestDevice->allocation_callbacks);
}

void zest_SetPipelineTemplatePushConstant(zest_pipeline_template_create_info_t* create_info, zest_uint size, zest_uint offset, VkShaderStageFlags stage_flags) {
    zest_vec_clear(create_info->pushConstantRange);
    VkPushConstantRange range;
    range.size = size;
    range.offset = offset;
    range.stageFlags = stage_flags;
    zest_vec_push(create_info->pushConstantRange, range);
}

zest_pipeline_template_t* zest_PipelineTemplate(zest_pipeline pipeline) {
    return &pipeline->pipeline_template;
}

void zest_MakePipelineTemplate(zest_pipeline pipeline, VkRenderPass render_pass, zest_pipeline_template_create_info_t* create_info) {

    if (create_info != &pipeline->create_info) {
        pipeline->create_info = zest__copy_pipeline_create_info(create_info);
        zest__free_pipeline_create_info(create_info);
    }
    pipeline->create_info.viewport.extent = zest_GetSwapChainExtent();
    if (!(pipeline->flags & zest_pipeline_set_flag_is_render_target_pipeline)) {
        pipeline->create_info.renderPass = render_pass;
    }

    zest__set_pipeline_template(&pipeline->pipeline_template, &pipeline->create_info);
    pipeline->pipeline_template.multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
}

VkShaderModule zest__create_shader_module(char* code) {
    VkShaderModuleCreateInfo create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = zest_vec_size(code);
    create_info.pCode = (zest_uint*)(code);

    VkShaderModule shader_module;
    ZEST_VK_CHECK_RESULT(vkCreateShaderModule(ZestDevice->logical_device, &create_info, &ZestDevice->allocation_callbacks, &shader_module));

    return shader_module;
}

zest_pipeline_template_create_info_t zest__copy_pipeline_create_info(zest_pipeline_template_create_info_t* create_info) {
    zest_pipeline_template_create_info_t copy = { 0 };
    copy.no_vertex_input = create_info->no_vertex_input;
    copy.renderPass = create_info->renderPass;
    copy.topology = create_info->topology;
    copy.viewport = create_info->viewport;
    copy.descriptorSetLayout = create_info->descriptorSetLayout;
    if (create_info->pushConstantRange) {
        zest_vec_resize(copy.pushConstantRange, zest_vec_size(create_info->pushConstantRange));
        memcpy(copy.pushConstantRange, create_info->pushConstantRange, zest_vec_size_in_bytes(create_info->pushConstantRange));
    }
    if (create_info->bindingDescriptions) {
        zest_vec_resize(copy.bindingDescriptions, zest_vec_size(create_info->bindingDescriptions));
        memcpy(copy.bindingDescriptions, create_info->bindingDescriptions, zest_vec_size_in_bytes(create_info->bindingDescriptions));
    }
    if (create_info->dynamicStates) {
        zest_vec_resize(copy.dynamicStates, zest_vec_size(create_info->dynamicStates));
        memcpy(copy.dynamicStates, create_info->dynamicStates, zest_vec_size_in_bytes(create_info->dynamicStates));
    }
    if (create_info->attributeDescriptions) {
        zest_vec_resize(copy.attributeDescriptions, zest_vec_size(create_info->attributeDescriptions));
        memcpy(copy.attributeDescriptions, create_info->attributeDescriptions, zest_vec_size_in_bytes(create_info->attributeDescriptions));
    }
    copy.fragShaderFile.str = 0;
    copy.fragShaderFunctionName.str = 0;
    copy.vertShaderFile.str = 0;
    copy.vertShaderFunctionName.str = 0;
    zest_SetText(&copy.fragShaderFile, create_info->fragShaderFile.str);
    zest_SetText(&copy.vertShaderFile, create_info->vertShaderFile.str);
    zest_SetText(&copy.fragShaderFunctionName, create_info->fragShaderFunctionName.str);
    zest_SetText(&copy.vertShaderFunctionName, create_info->vertShaderFunctionName.str);
    zest_SetText(&copy.shader_path_prefix, create_info->shader_path_prefix.str);
    return copy;
}

void zest__free_pipeline_create_info(zest_pipeline_template_create_info_t* create_info) {
    zest_vec_free(create_info->pushConstantRange);
    zest_vec_free(create_info->bindingDescriptions);
    zest_vec_free(create_info->dynamicStates);
    zest_vec_free(create_info->attributeDescriptions);
    zest_FreeText(&create_info->fragShaderFile);
    zest_FreeText(&create_info->vertShaderFile);
    zest_FreeText(&create_info->fragShaderFunctionName);
    zest_FreeText(&create_info->vertShaderFunctionName);
    zest_FreeText(&create_info->shader_path_prefix);
}

void zest__rebuild_pipeline(zest_pipeline_t* pipeline) {
    //Note: not setting this for pipelines messes with scaling, but not sure if some pipelines need this to be fixed
    pipeline->create_info.viewport.extent = zest_GetSwapChainExtent();
    pipeline->pipeline_template.renderPass = zest_GetStandardRenderPass();
    zest__update_pipeline_template(&pipeline->pipeline_template, &pipeline->create_info);
    zest_BuildPipeline(pipeline);
    for (ZEST_EACH_FIF_i) {
        zest_vec_clear(pipeline->descriptor_writes);
    }
}

zest_shader zest_NewShader() {
    zest_shader_t blank_shader = { 0 };
    zest_shader shader = ZEST__NEW(zest_shader);
    *shader = blank_shader;
    return shader;
}

void zest_CompileShader(const char *shader_code, shaderc_shader_kind shader_type, const char *name) {
    ZEST_ASSERT(name);     //You must give the shader a name
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->shaders, name));     //Shader already exitst, use zest_UpdateShader to update an existing shader
    zest_shader shader = zest_NewShader();
    if (zest_TextLength(&ZestRenderer->shader_path_prefix)) {
        zest_SetTextf(&shader->name, "%s%s", ZestRenderer->shader_path_prefix, name);
    }
    else {
        zest_SetTextf(&shader->name, "%s", name);
    }
    if (ZestApp->create_info.flags & zest_init_flag_cache_shaders) {
        shader->spv = zest_ReadEntireFile(shader->name.str, ZEST_FALSE);
        if (shader->spv) {
			zest_SetText(&shader->shader_code, shader_code);
			zest_map_insert(ZestRenderer->shaders, shader->name.str, shader);
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Loaded shader %s from cache and added to renderer shaders." ZEST_NL, name);
            return;
        }
    }

    shaderc_compilation_result_t result = shaderc_compile_into_spv( ZestRenderer->shader_compiler, shader_code, strlen(shader_code), shader_type, name, "main", NULL );

    if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {
		ZEST_APPEND_LOG(ZestDevice->log_path.str, "Shader compilation failed: %s, %s" ZEST_NL, name, shaderc_result_get_error_message(result));
        shaderc_result_release(result);
        zest_FreeText(&shader->name);
        ZEST__FREE(shader);
        ZEST_ASSERT(0); //There's a bug in this shader that needs fixing. You can check the log file for the error message
    }

    zest_uint spv_size = (zest_uint)shaderc_result_get_length(result);
    const char *spv_binary = shaderc_result_get_bytes(result);
    zest_vec_resize(shader->spv, spv_size);
    memcpy(shader->spv, spv_binary, spv_size);
    zest_SetText(&shader->shader_code, shader_code);
    zest_map_insert(ZestRenderer->shaders, shader->name.str, shader);
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Compiled shader %s and added to renderer shaders." ZEST_NL, name);
    shaderc_result_release(result);
    if (ZestApp->create_info.flags & zest_init_flag_cache_shaders) {
        FILE *shader_file = zest__open_file(shader->name.str, "wb");
        if (shader_file == NULL) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Failed to open file for writing: %s" ZEST_NL, shader->name.str);
        }
        size_t written = fwrite(shader->spv, 1, spv_size, shader_file);
        if (written != spv_size) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Failed to write entire shader to file: %s" ZEST_NL, shader->name.str);
            fclose(shader_file);
        }
        fclose(shader_file);
    }
}

zest_shader zest_AddShaderFromSPVFile(const char *filename) {
    ZEST_ASSERT(filename);     //You must give the shader a name
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->shaders, filename));     //Shader already exitst, use zest_UpdateShader to update an existing shader
    zest_shader shader = zest_NewShader();
    shader->spv = zest_ReadEntireFile(filename, ZEST_FALSE);
    if (shader->spv) {
        zest_SetText(&shader->name, filename);
        zest_map_insert(ZestRenderer->shaders, shader->name.str, shader);
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Loaded shader %s and added to renderer shaders." ZEST_NL, filename);
        return shader;
    }
    zest_FreeShader(shader);
    return 0;
}

zest_shader zest_AddShaderFromSPVMemory(const char *name, const void *buffer, zest_uint size) {
    ZEST_ASSERT(name);     //You must give the shader a name
    if (buffer && size) {
		zest_shader shader = zest_NewShader();
		if (zest_TextLength(&ZestRenderer->shader_path_prefix)) {
			zest_SetTextf(&shader->name, "%s%s", ZestRenderer->shader_path_prefix, name);
		}
		else {
			zest_SetTextf(&shader->name, "%s", name);
		}
		ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->shaders, shader->name.str));     //Shader already exitst, use zest_UpdateShader to update an existing shader
		zest_vec_resize(shader->spv, size);
		memcpy(shader->spv, buffer, size);
        zest_map_insert(ZestRenderer->shaders, shader->name.str, shader);
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Read shader %s from memory and added to renderer shaders." ZEST_NL, name);
        return shader;
    }
    return 0;
}

void zest_FreeShader(zest_shader shader) {
    if (zest_map_valid_name(ZestRenderer->shaders, shader->name.str)) {
        zest_map_remove(ZestRenderer->shaders, shader->name.str);
	}
    ZEST__FREE(shader);
}

zest_pipeline zest_AddPipeline(const char* name) {
    zest_pipeline pipeline = zest__create_pipeline();
    zest_map_insert(ZestRenderer->pipelines, name, pipeline);
    return pipeline;
}

zest_vertex_input_descriptions zest_NewVertexInputDescriptions() {
    zest_vertex_input_descriptions descriptions = 0;
    return descriptions;
}

void zest_AddVertexInputDescription(zest_vertex_input_descriptions* descriptions, VkVertexInputAttributeDescription description) {
    zest_vec_push(*descriptions, description);
}

VkRenderPass zest_GetStandardRenderPass() {
    if (ZEST__FLAGGED(ZestRenderer->flags, zest_renderer_flag_has_depth_buffer)) {
        return *zest_map_at(ZestRenderer->render_passes, "Render pass standard")->render_pass;
    }
    else {
        return *zest_map_at(ZestRenderer->render_passes, "Render pass standard no depth")->render_pass;
    }
}
zest_key zest_Hash(zest_hasher* hasher, const void* input, zest_ull length, zest_ull seed) { zest__hash_initialise(hasher, seed); zest__hasher_add(hasher, input, length); return (zest_key)zest__get_hash(hasher); }
VkFramebuffer zest_GetRendererFrameBuffer(zest_command_queue_draw_commands item) { return ZestRenderer->swapchain_frame_buffers[ZestRenderer->current_frame]; }
zest_descriptor_set_layout zest_GetDescriptorSetLayout(const char* name) { return *zest_map_at(ZestRenderer->descriptor_layouts, name); }
zest_pipeline zest_Pipeline(const char* name) { ZEST_ASSERT(zest_map_valid_name(ZestRenderer->pipelines, name)); return *zest_map_at(ZestRenderer->pipelines, name); }
zest_pipeline_template_create_info_t zest_CopyTemplateFromPipeline(const char* pipeline_name) {
    return zest__copy_pipeline_create_info(&zest_Pipeline(pipeline_name)->create_info);
}
zest_pipeline_template_create_info_t* zest_PipelineCreateInfo(const char* name) { ZEST_ASSERT(zest_map_valid_name(ZestRenderer->pipelines, name)); zest_pipeline pipeline = *zest_map_at(ZestRenderer->pipelines, name); return &pipeline->create_info; }
VkExtent2D zest_GetSwapChainExtent() { return ZestRenderer->swapchain_extent; }
VkExtent2D zest_GetWindowExtent(void) { return ZestRenderer->window_extent; }
zest_uint zest_SwapChainWidth(void) { return ZestRenderer->swapchain_extent.width; }
zest_uint zest_SwapChainHeight(void) { return ZestRenderer->swapchain_extent.height; }
float zest_SwapChainWidthf(void) { return (float)ZestRenderer->swapchain_extent.width; }
float zest_SwapChainHeightf(void) { return (float)ZestRenderer->swapchain_extent.height; }
zest_uint zest_ScreenWidth() { return ZestApp->window->window_width; }
zest_uint zest_ScreenHeight() { return ZestApp->window->window_height; }
float zest_ScreenWidthf() { return (float)ZestApp->window->window_width; }
float zest_ScreenHeightf() { return (float)ZestApp->window->window_height; }
float zest_MouseXf() { return (float)ZestApp->mouse_x; }
float zest_MouseYf() { return (float)ZestApp->mouse_y; }
float zest_DPIScale(void) { return ZestRenderer->dpi_scale; }
void zest_SetDPIScale(float scale) { ZestRenderer->dpi_scale = scale; }
zest_uint zest_FPS() { return ZestApp->last_fps; }
float zest_FPSf() { return (float)ZestApp->last_fps; }
zest_window zest_AllocateWindow() { zest_window window; window = ZEST__NEW(zest_window); memset(window, 0, sizeof(zest_window_t)); return window; }
zest_descriptor_buffer zest_GetUniformBuffer(const char* name) { ZEST_ASSERT(zest_map_valid_name(ZestRenderer->uniform_buffers, name)); return *zest_map_at(ZestRenderer->uniform_buffers, name); }
zest_bool zest_UniformBufferExists(const char* name) { return zest_map_valid_name(ZestRenderer->uniform_buffers, name); }
void zest_WaitForIdleDevice() { vkDeviceWaitIdle(ZestDevice->logical_device); }
void zest_MaybeQuit(zest_bool condition) { ZestApp->flags |= condition != 0 ? zest_app_flag_quit_application : 0; }
void zest__hash_initialise(zest_hasher* hasher, zest_ull seed) { hasher->state[0] = seed + zest__PRIME1 + zest__PRIME2; hasher->state[1] = seed + zest__PRIME2; hasher->state[2] = seed; hasher->state[3] = seed - zest__PRIME1; hasher->buffer_size = 0; hasher->total_length = 0; }
void zest_GetMouseSpeed(double* x, double* y) {
    double ellapsed_in_seconds = (double)ZestApp->current_elapsed / ZEST_MICROSECS_SECOND;
    *x = ZestApp->mouse_delta_x * ellapsed_in_seconds;
    *y = ZestApp->mouse_delta_y * ellapsed_in_seconds;
}

VkDescriptorBufferInfo* zest_GetUniformBufferInfo(const char* name, zest_index fif) {
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->uniform_buffers, name));
    return zest_map_at(ZestRenderer->uniform_buffers, name)->descriptor_info[fif];
}

void* zest_GetUniformBufferData(zest_uniform_buffer uniform_buffer) {
    return uniform_buffer->buffer[ZEST_FIF]->data;
}

void* zest_GetUniformBufferDataFIF(zest_uniform_buffer uniform_buffer, zest_index fif) {
    return uniform_buffer->buffer[fif]->data;
}

void zest_BindVertexBuffer(zest_buffer buffer) {
    VkDeviceSize offsets[] = { buffer->memory_offset };
    vkCmdBindVertexBuffers(ZestRenderer->current_command_buffer, 0, 1, zest_GetBufferDeviceBuffer(buffer), offsets);
}

void zest_BindIndexBuffer(zest_buffer buffer) {
    vkCmdBindIndexBuffer(ZestRenderer->current_command_buffer, *zest_GetBufferDeviceBuffer(buffer), buffer->memory_offset, VK_INDEX_TYPE_UINT32);
}

void zest_SendPushConstants(zest_pipeline pipeline, VkShaderStageFlags shader_flags, zest_uint size, void* data) {
    vkCmdPushConstants(ZestRenderer->current_command_buffer, pipeline->pipeline_layout, shader_flags, 0, size, data);
}

void zest_SendComputePushConstants(zest_compute compute) {
    vkCmdPushConstants(ZestRenderer->current_command_buffer, compute->pipeline_layout, compute->pushConstantRange.stageFlags, 0, compute->pushConstantRange.size, &compute->push_constants);
}

void zest_SendComputePushConstantsCB(VkCommandBuffer command_buffer, zest_compute compute) {
    vkCmdPushConstants(command_buffer, compute->pipeline_layout, compute->pushConstantRange.stageFlags, 0, compute->pushConstantRange.size, &compute->push_constants);
}

void zest_SendStandardPushConstants(zest_pipeline_t* pipeline, void* data) {
    vkCmdPushConstants(ZestRenderer->current_command_buffer, pipeline->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(zest_push_constants_t), data);
}

void zest_Draw(zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance) {
    vkCmdDraw(ZestRenderer->current_command_buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void zest_DrawIndexed(zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance) {
    vkCmdDrawIndexed(ZestRenderer->current_command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void zest_ComputeToVertexBarrier() {
    VkMemoryBarrier memory_barrier = { 0 };
    memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memory_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    memory_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    vkCmdPipelineBarrier(ZestRenderer->current_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &memory_barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
}

void zest_DispatchCompute(zest_compute compute, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z) {
    vkCmdDispatch(ZestRenderer->current_command_buffer, group_count_x, group_count_y, group_count_z);
}

void zest_DispatchComputeCB(VkCommandBuffer command_buffer, zest_compute compute, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z) {
    vkCmdDispatch(command_buffer, group_count_x, group_count_y, group_count_z);
}

void zest_EnableVSync() {
    ZEST__FLAG(ZestRenderer->flags, zest_renderer_flag_vsync_enabled);
    ZEST__FLAG(ZestRenderer->flags, zest_renderer_flag_schedule_change_vsync);
}

void zest_DisableVSync() {
    ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_vsync_enabled);
    ZEST__FLAG(ZestRenderer->flags, zest_renderer_flag_schedule_change_vsync);
}

void zest_LogFPSToConsole(zest_bool yesno) {
    if (yesno) {
        ZEST__FLAG(ZestApp->flags, zest_app_flag_output_fps);
    }
    else {
        ZEST__UNFLAG(ZestApp->flags, zest_app_flag_output_fps);
    }
}

void* zest_Window() {
    return ZestApp->window->window_handle;
}

void zest_SetFrameInFlight(zest_uint fif) {
    ZEST_ASSERT(fif < ZEST_MAX_FIF);        //Frame in flight must be less than the maximum amount of frames in flight
    ZestDevice->saved_fif = ZEST_FIF;
    ZEST_FIF = fif;
}

void zest_RestoreFrameInFlight(void) {
    ZEST_FIF = ZestDevice->saved_fif;
}

VkFence zest_CreateFence() {
    VkFenceCreateInfo fence_info = { 0 };
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = 0;
    VkFence fence;
    vkCreateFence(ZestDevice->logical_device, &fence_info, &ZestDevice->allocation_callbacks, &fence);
    return fence;
}

zest_bool zest_CheckFence(VkFence fence) {
    if (vkGetFenceStatus(ZestDevice->logical_device, fence) == VK_SUCCESS) {
        vkDestroyFence(ZestDevice->logical_device, fence, &ZestDevice->allocation_callbacks);
        return ZEST_TRUE;
    }
    return ZEST_FALSE;
}

void zest_WaitForFence(VkFence fence) {
    vkWaitForFences(ZestDevice->logical_device, 1, &fence, VK_TRUE, UINT64_MAX);
}

void zest_DestroyFence(VkFence fence) {
    zest_WaitForFence(fence);
    vkDestroyFence(ZestDevice->logical_device, fence, &ZestDevice->allocation_callbacks);
}

void zest_ResetEvent(VkEvent e) {
    vkResetEvent(ZestDevice->logical_device, e);
}

zest_bool zest_IsMemoryPropertyAvailable(VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(ZestDevice->physical_device, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((flags & memoryProperties.memoryTypes[i].propertyFlags) == flags) {
            return ZEST_TRUE;
        }
    }

    return ZEST_FALSE;
}

zest_bool zest_GPUHasDeviceLocalHostVisible(VkDeviceSize minimum_size) {
    if (ZEST_DISABLE_GPU_DIRECT_WRITE) {
        return ZEST_FALSE;
    }
    if (zest_IsMemoryPropertyAvailable(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        //Even though the memory property is available, we should check that the heap type that it maps to has enough memory to work with.
        zest_buffer_type_t buffer_type_large = zest__get_buffer_memory_type(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, minimum_size);
        int heap_index_large = ZestDevice->memory_properties.memoryTypes[buffer_type_large.memory_type_index].heapIndex;
        if (ZestDevice->memory_properties.memoryHeaps[heap_index_large].size > minimum_size) {
            return ZEST_TRUE;
        }
    }
    return ZEST_FALSE;
}

zest_uint zest__grow_capacity(void* T, zest_uint size) {
    zest_uint new_capacity = T ? (size + size / 2) : 8;
    return new_capacity > size ? new_capacity : size;
}
void* zest__vec_reserve(void* T, zest_uint unit_size, zest_uint new_capacity) {
    if (T && new_capacity <= zest__vec_header(T)->capacity)
        return T;
    void* new_data = ZEST__REALLOCATE((T ? zest__vec_header(T) : T), new_capacity * unit_size + zest__VEC_HEADER_OVERHEAD);
    if (!T) memset(new_data, 0, zest__VEC_HEADER_OVERHEAD);
    T = ((char*)new_data + zest__VEC_HEADER_OVERHEAD);
    ((zest_vec*)(new_data))->capacity = new_capacity;
    return T;
}

void zest_SetText(zest_text* buffer, const char* text) {
    if (!text) {
        zest_FreeText(buffer);
        return;
    }
    zest_uint length = (zest_uint)strlen(text) + 1;
    zest_vec_resize(buffer->str, length);
    zest_strcpy(buffer->str, length, text);
}

void zest_SetTextfv(zest_text* buffer, const char* text, va_list args)
{
    va_list args2;
    va_copy(args2, args);

#ifdef _MSC_VER
    int len = vsnprintf(NULL, 0, text, args);
    ZEST_ASSERT(len >= 0);

    if ((int)zest_TextLength(buffer) < len + 1)
    {
        zest_vec_resize(buffer->str, (zest_uint)(len + 1));
    }
    len = vsnprintf(buffer->str, len + 1, text, args2);
#else
    int len = vsnprintf(buffer->str ? buffer->str : NULL, (size_t)zest_TextLength(buffer), text, args);
    ZEST_ASSERT(len >= 0);

    if (zest_TextLength(buffer) < len + 1)
    {
        zest_vec_resize(buffer->str, len + 1);
        len = vsnprintf(buffer->str, (size_t)len + 1, text, args2);
    }
#endif

    ZEST_ASSERT(buffer->str);
}

void zest_SetTextf(zest_text* buffer, const char* text, ...) {
    va_list args;
    va_start(args, text);
    zest_SetTextfv(buffer, text, args);
    va_end(args);
}

void zest_FreeText(zest_text* buffer) {
    zest_vec_free(buffer->str);
    buffer->str = ZEST_NULL;
}

zest_uint zest_TextLength(zest_text* buffer) {
    return zest_vec_size(buffer->str);
}

//End api functions

void zest__compile_builtin_shaders(void) {
    zest_CompileShader(zest_shader_imgui_vert, shaderc_vertex_shader, "imgui_vert.spv");
    zest_CompileShader(zest_shader_imgui_frag, shaderc_fragment_shader, "imgui_frag.spv");
    zest_CompileShader(zest_shader_billboard_vert, shaderc_vertex_shader, "billboard_vert.spv");
    zest_CompileShader(zest_shader_sprite_frag, shaderc_fragment_shader, "image_frag.spv");
    zest_CompileShader(zest_shader_sprite_alpha_frag, shaderc_fragment_shader, "sprite_alpha_frag.spv");
    zest_CompileShader(zest_shader_sprite_vert, shaderc_vertex_shader, "sprite_vert.spv");
    zest_CompileShader(zest_shader_shape_vert, shaderc_vertex_shader, "shape_vert.spv");
    zest_CompileShader(zest_shader_shape_frag, shaderc_fragment_shader, "shape_frag.spv");
    zest_CompileShader(zest_shader_3d_lines_vert, shaderc_vertex_shader, "3d_lines_vert.spv");
    zest_CompileShader(zest_shader_3d_lines_frag, shaderc_fragment_shader, "3d_lines_frag.spv");
    zest_CompileShader(zest_shader_font_frag, shaderc_fragment_shader, "font_frag.spv");
    zest_CompileShader(zest_shader_mesh_vert, shaderc_vertex_shader, "mesh_vert.spv");
    zest_CompileShader(zest_shader_mesh_instance_vert, shaderc_vertex_shader, "mesh_instance_vert.spv");
    zest_CompileShader(zest_shader_mesh_instance_frag, shaderc_fragment_shader, "mesh_instance_frag.spv");
    zest_CompileShader(zest_shader_swap_vert, shaderc_vertex_shader, "swap_vert.spv");
    zest_CompileShader(zest_shader_swap_frag, shaderc_fragment_shader, "swap_frag.spv");
}

void zest__prepare_standard_pipelines() {
    VkRenderPass render_pass = zest_GetStandardRenderPass();

    zest_pipeline_template_create_info_t instance_create_info = zest_CreatePipelineTemplateCreateInfo();
    instance_create_info.viewport.extent = zest_GetSwapChainExtent();
    zest_vec* test = zest__vec_header(instance_create_info.dynamicStates);

    VkPushConstantRange image_pushconstant_range;
    image_pushconstant_range.size = sizeof(zest_push_constants_t);
    image_pushconstant_range.offset = 0;
    image_pushconstant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    zest_vec_push(instance_create_info.pushConstantRange, image_pushconstant_range);

    //2d sprite rendering
    zest_pipeline sprite_instance_pipeline = zest_AddPipeline("pipeline_2d_sprites");
    zest_AddVertexInputBindingDescription(&instance_create_info, 0, sizeof(zest_sprite_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);

    VkVertexInputAttributeDescription* instance_vertex_input_attributes = 0;

    zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(zest_sprite_instance_t, size)));                        // Location 0: Size of the sprite in pixels
    zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(zest_sprite_instance_t, handle)));                    // Location 1: Handle of the sprite
    zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_sprite_instance_t, uv)));                    // Location 2: UV coords
    zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 3, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_sprite_instance_t, position_rotation)));    // Location 3: Instance Position and rotation
    zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 4, VK_FORMAT_R32_SFLOAT, offsetof(zest_sprite_instance_t, intensity)));                    // Location 4: Intensity
    zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 5, VK_FORMAT_R16G16_SNORM, offsetof(zest_sprite_instance_t, alignment)));                    // Location 5: Alignment
    zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 6, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_sprite_instance_t, color)));                    // Location 6: Instance Color
    zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 7, VK_FORMAT_R32_UINT, offsetof(zest_sprite_instance_t, image_layer_index)));                // Location 7: Instance Parameters

    instance_create_info.attributeDescriptions = instance_vertex_input_attributes;
    zest_SetText(&instance_create_info.vertShaderFile, "sprite_vert.spv");
    zest_SetText(&instance_create_info.fragShaderFile, "image_frag.spv");
    zest_MakePipelineTemplate(sprite_instance_pipeline, render_pass, &instance_create_info);
    sprite_instance_pipeline->pipeline_template.colorBlendAttachment = zest_PreMultiplyBlendState();
    sprite_instance_pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
    sprite_instance_pipeline->pipeline_template.depthStencil.depthTestEnable = VK_TRUE;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "2d sprites pipeline" ZEST_NL);
    zest_BuildPipeline(sprite_instance_pipeline);

    instance_create_info = zest_CopyTemplateFromPipeline("pipeline_2d_sprites");
    //Sprites with 1 channel textures
    zest_pipeline sprite_instance_pipeline_alpha = zest_AddPipeline("pipeline_2d_sprites_alpha");
    zest_SetText(&instance_create_info.vertShaderFile, "sprite_vert.spv");
    zest_SetText(&instance_create_info.fragShaderFile, "sprite_alpha_frag.spv");
    zest_MakePipelineTemplate(sprite_instance_pipeline_alpha, render_pass, &instance_create_info);
    sprite_instance_pipeline_alpha->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
    sprite_instance_pipeline_alpha->pipeline_template.depthStencil.depthTestEnable = VK_TRUE;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "2d sprites alpha pipeline" ZEST_NL);
    zest_BuildPipeline(sprite_instance_pipeline_alpha);

    //SDF lines 2d
    instance_create_info = zest_CopyTemplateFromPipeline("pipeline_2d_sprites");
    zest_ClearVertexInputBindingDescriptions(&instance_create_info);
    zest_AddVertexInputBindingDescription(&instance_create_info, 0, sizeof(zest_shape_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);

    VkVertexInputAttributeDescription* line_instance_vertex_input_attributes = 0;

    zest_vec_push(line_instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_shape_instance_t, rect)));        // Location 0: Start Position
    zest_vec_push(line_instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_shape_instance_t, parameters)));    // Location 1: End Position
    zest_vec_push(line_instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_shape_instance_t, start_color)));        // Location 2: Start Color

    instance_create_info.attributeDescriptions = line_instance_vertex_input_attributes;
    zest_pipeline line_instance_pipeline = zest_AddPipeline("pipeline_line_instance");
    zest_SetText(&instance_create_info.vertShaderFile, "shape_vert.spv");
    zest_SetText(&instance_create_info.fragShaderFile, "shape_frag.spv");
    line_instance_pipeline->descriptor_layout = zest_GetDescriptorSetLayout("Polygon layout (no sampler)");
    instance_create_info.descriptorSetLayout = line_instance_pipeline->descriptor_layout;
    zest_MakePipelineTemplate(line_instance_pipeline, render_pass, &instance_create_info);
    line_instance_pipeline->pipeline_template.colorBlendAttachment = zest_PreMultiplyBlendState();
    line_instance_pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "SDF Lines pipeline" ZEST_NL);
    zest_BuildPipeline(line_instance_pipeline);
    zest_MakePipelineDescriptorWrites(line_instance_pipeline);

    //SDF lines 3d
    instance_create_info = zest_CopyTemplateFromPipeline("pipeline_line_instance");
    zest_ClearVertexInputBindingDescriptions(&instance_create_info);
    zest_AddVertexInputBindingDescription(&instance_create_info, 0, sizeof(zest_line_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);

    VkVertexInputAttributeDescription* line3d_instance_vertex_input_attributes = 0;

    zest_vec_push(line3d_instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_line_instance_t, start))); 		// Location 0: Start Position
    zest_vec_push(line3d_instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_line_instance_t, end)));		    // Location 1: End Position
    zest_vec_push(line3d_instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_line_instance_t, start_color)));		// Location 2: Start Color

    instance_create_info.attributeDescriptions = line3d_instance_vertex_input_attributes;
    zest_pipeline line3d_instance_pipeline = zest_AddPipeline("pipeline_line3d_instance");
    zest_SetText(&instance_create_info.vertShaderFile, "3d_lines_vert.spv");
    zest_SetText(&instance_create_info.fragShaderFile, "3d_lines_frag.spv");
    line3d_instance_pipeline->descriptor_layout = zest_GetDescriptorSetLayout("Polygon layout (no sampler)");
    instance_create_info.descriptorSetLayout = line3d_instance_pipeline->descriptor_layout;
    zest_MakePipelineTemplate(line3d_instance_pipeline, render_pass, &instance_create_info);
    line3d_instance_pipeline->pipeline_template.colorBlendAttachment = zest_PreMultiplyBlendState();
    line3d_instance_pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "SDF 3D Lines pipeline" ZEST_NL);
    zest_BuildPipeline(line3d_instance_pipeline);
    zest_MakePipelineDescriptorWrites(line3d_instance_pipeline);

    //Font Texture
    instance_create_info = zest_CopyTemplateFromPipeline("pipeline_2d_sprites_alpha");
    zest_pipeline font_pipeline = zest_AddPipeline("pipeline_fonts");
    zest_SetText(&instance_create_info.vertShaderFile, "sprite_vert.spv");
    zest_SetText(&instance_create_info.fragShaderFile, "font_frag.spv");
    zest_MakePipelineTemplate(font_pipeline, render_pass, &instance_create_info);
    font_pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
    font_pipeline->pipeline_template.depthStencil.depthTestEnable = VK_FALSE;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Font pipeline" ZEST_NL);
    zest_BuildPipeline(font_pipeline);

    //3d billboards
    instance_create_info = zest_CopyTemplateFromPipeline("pipeline_fonts");
    zest_ClearVertexInputBindingDescriptions(&instance_create_info);
    zest_AddVertexInputBindingDescription(&instance_create_info, 0, sizeof(zest_billboard_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);
    VkVertexInputAttributeDescription* billboard_vertex_input_attributes = 0;

    zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(zest_billboard_instance_t, position)));                    // Location 0: Position
    zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R16G16_SNORM, offsetof(zest_billboard_instance_t, uv_xy)));                            // Location 1: uv_xy
    zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(zest_billboard_instance_t, rotations)));                    // Location 2: scale pitch yaw
    zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 3, VK_FORMAT_R16G16_SNORM, offsetof(zest_billboard_instance_t, uv_zw)));                            // Location 3: uv_zw
    zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 4, VK_FORMAT_R32G32_SFLOAT, offsetof(zest_billboard_instance_t, scale)));                            // Location 4: Handle
    zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 5, VK_FORMAT_R32G32_SFLOAT, offsetof(zest_billboard_instance_t, handle)));                        // Location 5: Handle
    zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 6, VK_FORMAT_R32_UINT, offsetof(zest_billboard_instance_t, blend_texture_array)));                // Location 6: texture array index
    zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 7, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_billboard_instance_t, color)));                        // Location 7: Instance Color
    zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 8, VK_FORMAT_R32_SFLOAT, offsetof(zest_billboard_instance_t, stretch)));                            // Location 8: Stretch
    zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 9, VK_FORMAT_R8G8B8_SNORM, offsetof(zest_billboard_instance_t, alignment)));                        // Location 9: Alignment X, Y and Z

    zest_pipeline billboard_instance_pipeline = zest_AddPipeline("pipeline_billboard");
    instance_create_info.attributeDescriptions = billboard_vertex_input_attributes;
    zest_SetText(&instance_create_info.vertShaderFile, "billboard_vert.spv");
    zest_SetText(&instance_create_info.fragShaderFile, "image_frag.spv");
    zest_MakePipelineTemplate(billboard_instance_pipeline, render_pass, &instance_create_info);
    billboard_instance_pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
    billboard_instance_pipeline->pipeline_template.depthStencil.depthTestEnable = VK_TRUE;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Billboard pipeline" ZEST_NL);
    zest_BuildPipeline(billboard_instance_pipeline);

    instance_create_info = zest_CopyTemplateFromPipeline("pipeline_billboard");
    zest_pipeline billboard_pipeline_alpha = zest_AddPipeline("pipeline_billboard_alpha");
    zest_SetText(&instance_create_info.vertShaderFile, "billboard_vert.spv");
    zest_SetText(&instance_create_info.fragShaderFile, "sprite_alpha_frag.spv");
    zest_MakePipelineTemplate(billboard_pipeline_alpha, render_pass, &instance_create_info);
    billboard_pipeline_alpha->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
    billboard_pipeline_alpha->pipeline_template.depthStencil.depthTestEnable = VK_TRUE;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Billboard alpha pipeline" ZEST_NL);
    zest_BuildPipeline(billboard_pipeline_alpha);

    //ImGuiPipeline
    zest_pipeline_template_create_info_t imgui_pipeline_template = zest_CreatePipelineTemplateCreateInfo();
    imgui_pipeline_template.viewport.offset.x = 0;
    imgui_pipeline_template.viewport.offset.y = 0;
    VkPushConstantRange imgui_pushconstant_range;
    imgui_pushconstant_range.size = sizeof(zest_push_constants_t);
    imgui_pushconstant_range.offset = 0;
    imgui_pushconstant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    zest_vec_push(imgui_pipeline_template.pushConstantRange, imgui_pushconstant_range);
    zest_AddVertexInputBindingDescription(&imgui_pipeline_template, 0, sizeof(zest_ImDrawVert_t), VK_VERTEX_INPUT_RATE_VERTEX);
    VkVertexInputAttributeDescription* imgui_vertex_input_attributes = 0;
    zest_vec_push(imgui_vertex_input_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(zest_ImDrawVert_t, pos)));    // Location 0: Position
    zest_vec_push(imgui_vertex_input_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(zest_ImDrawVert_t, uv)));    // Location 1: UV
    zest_vec_push(imgui_vertex_input_attributes, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_ImDrawVert_t, col)));    // Location 2: Color

    imgui_pipeline_template.attributeDescriptions = imgui_vertex_input_attributes;
    zest_SetText(&imgui_pipeline_template.vertShaderFile, "imgui_vert.spv");
    zest_SetText(&imgui_pipeline_template.fragShaderFile, "imgui_frag.spv");

    zest_pipeline imgui_pipeline = zest_AddPipeline("pipeline_imgui");

    imgui_pipeline_template.viewport.extent = zest_GetSwapChainExtent();
    imgui_pipeline->flags |= zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild;
    imgui_pipeline->descriptor_layout = zest_GetDescriptorSetLayout("Standard 1 uniform 1 sampler");
    imgui_pipeline_template.descriptorSetLayout = zest_GetDescriptorSetLayout("Standard 1 uniform 1 sampler");
    zest_MakePipelineTemplate(imgui_pipeline, render_pass, &imgui_pipeline_template);

    imgui_pipeline->pipeline_template.rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    imgui_pipeline->pipeline_template.rasterizer.cullMode = VK_CULL_MODE_NONE;
    imgui_pipeline->pipeline_template.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    imgui_pipeline->pipeline_template.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    imgui_pipeline->pipeline_template.colorBlendAttachment = zest_ImGuiBlendState();
    imgui_pipeline->pipeline_template.depthStencil.depthTestEnable = VK_FALSE;
    imgui_pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "ImGui pipeline" ZEST_NL);
    zest_BuildPipeline(imgui_pipeline);

    //General mesh drawing
    zest_pipeline_template_create_info_t mesh_pipeline_template = zest_CreatePipelineTemplateCreateInfo();
    mesh_pipeline_template.viewport.offset.x = 0;
    mesh_pipeline_template.viewport.offset.y = 0;
    VkPushConstantRange mesh_pushconstant_range;
    mesh_pushconstant_range.size = sizeof(zest_push_constants_t);
    mesh_pushconstant_range.offset = 0;
    mesh_pushconstant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    zest_vec_push(mesh_pipeline_template.pushConstantRange, mesh_pushconstant_range);
    zest_AddVertexInputBindingDescription(&mesh_pipeline_template, 0, sizeof(zest_textured_vertex_t), VK_VERTEX_INPUT_RATE_VERTEX);
    VkVertexInputAttributeDescription* zest_vertex_input_attributes = 0;
    zest_vec_push(zest_vertex_input_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(zest_textured_vertex_t, pos)));        // Location 0: Position
    zest_vec_push(zest_vertex_input_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32_SFLOAT, offsetof(zest_textured_vertex_t, intensity)));        // Location 1: Alpha/Intensity
    zest_vec_push(zest_vertex_input_attributes, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(zest_textured_vertex_t, uv)));            // Location 2: Position
    zest_vec_push(zest_vertex_input_attributes, zest_CreateVertexInputDescription(0, 3, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_textured_vertex_t, color)));        // Location 3: Color
    zest_vec_push(zest_vertex_input_attributes, zest_CreateVertexInputDescription(0, 4, VK_FORMAT_R32_UINT, offsetof(zest_textured_vertex_t, parameters)));        // Location 4: Parameters

    mesh_pipeline_template.attributeDescriptions = zest_vertex_input_attributes;
    zest_SetText(&mesh_pipeline_template.vertShaderFile, "mesh_vert.spv");
    zest_SetText(&mesh_pipeline_template.fragShaderFile, "image_frag.spv");

    zest_pipeline mesh_pipeline = zest_AddPipeline("pipeline_mesh");

    mesh_pipeline_template.viewport.extent = zest_GetSwapChainExtent();
    mesh_pipeline->flags |= zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild;
    mesh_pipeline->descriptor_layout = zest_GetDescriptorSetLayout("Standard 1 uniform 1 sampler");
    mesh_pipeline_template.descriptorSetLayout = zest_GetDescriptorSetLayout("Standard 1 uniform 1 sampler");
    zest_MakePipelineTemplate(mesh_pipeline, render_pass, &mesh_pipeline_template);

    mesh_pipeline->pipeline_template.colorBlendAttachment = zest_PreMultiplyBlendState();
    mesh_pipeline->pipeline_template.depthStencil.depthTestEnable = VK_TRUE;
    mesh_pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_TRUE;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Mesh pipeline" ZEST_NL);
    zest_BuildPipeline(mesh_pipeline);

    //Instanced mesh drawing for drawing primatives
    zest_pipeline_template_create_info_t imesh_pipeline_template = zest_CreatePipelineTemplateCreateInfo();
    imesh_pipeline_template.viewport.offset.x = 0;
    imesh_pipeline_template.viewport.offset.y = 0;
    VkPushConstantRange imesh_pushconstant_range;
    imesh_pushconstant_range.size = sizeof(zest_push_constants_t);
    imesh_pushconstant_range.offset = 0;
    imesh_pushconstant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    zest_vec_push(imesh_pipeline_template.pushConstantRange, imesh_pushconstant_range);
    zest_AddVertexInputBindingDescription(&imesh_pipeline_template, 0, sizeof(zest_vertex_t), VK_VERTEX_INPUT_RATE_VERTEX);
    zest_AddVertexInputBindingDescription(&imesh_pipeline_template, 1, sizeof(zest_mesh_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);
    VkVertexInputAttributeDescription* imesh_input_attributes = 0;
    zest_vec_push(imesh_input_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0));                                          // Location 0: Vertex Position
    zest_vec_push(imesh_input_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_vertex_t, color)));               // Location 1: Vertex Color
    zest_vec_push(imesh_input_attributes, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R32_UINT, offsetof(zest_vertex_t, group)));                     // Location 2: Group id
    zest_vec_push(imesh_input_attributes, zest_CreateVertexInputDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(zest_vertex_t, normal)));            // Location 3: Vertex Position
    zest_vec_push(imesh_input_attributes, zest_CreateVertexInputDescription(1, 4, VK_FORMAT_R32G32B32_SFLOAT, 0));                                          // Location 4: Instance Position
    zest_vec_push(imesh_input_attributes, zest_CreateVertexInputDescription(1, 5, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_mesh_instance_t, color)));        // Location 5: Instance Color
    zest_vec_push(imesh_input_attributes, zest_CreateVertexInputDescription(1, 6, VK_FORMAT_R32G32B32_SFLOAT, offsetof(zest_mesh_instance_t, rotation)));   // Location 6: Instance Rotation
    zest_vec_push(imesh_input_attributes, zest_CreateVertexInputDescription(1, 7, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_mesh_instance_t, parameters)));   // Location 7: Instance Parameters
    zest_vec_push(imesh_input_attributes, zest_CreateVertexInputDescription(1, 8, VK_FORMAT_R32G32B32_SFLOAT, offsetof(zest_mesh_instance_t, scale)));      // Location 8: Instance Scale

    imesh_pipeline_template.attributeDescriptions = imesh_input_attributes;
    zest_SetText(&imesh_pipeline_template.vertShaderFile, "mesh_instance_vert.spv");
    zest_SetText(&imesh_pipeline_template.fragShaderFile, "mesh_instance_frag.spv");

    zest_pipeline imesh_pipeline = zest_AddPipeline("pipeline_mesh_instance");

    imesh_pipeline_template.viewport.extent = zest_GetSwapChainExtent();
    imesh_pipeline->flags |= zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild;
    imesh_pipeline->descriptor_layout = zest_GetDescriptorSetLayout("Polygon layout (no sampler)");
    imesh_pipeline_template.descriptorSetLayout = zest_GetDescriptorSetLayout("Polygon layout (no sampler)");
    zest_MakePipelineTemplate(imesh_pipeline, render_pass, &imesh_pipeline_template);

    imesh_pipeline->pipeline_template.rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    imesh_pipeline->pipeline_template.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    imesh_pipeline->pipeline_template.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    imesh_pipeline->pipeline_template.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    imesh_pipeline->pipeline_template.colorBlendAttachment = zest_AlphaBlendState();
    imesh_pipeline->pipeline_template.depthStencil.depthTestEnable = VK_TRUE;
    imesh_pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_TRUE;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Instance Mesh pipeline" ZEST_NL);
    zest_BuildPipeline(imesh_pipeline);
    zest_MakePipelineDescriptorWrites(imesh_pipeline);

    //Final Render Pipelines
    zest_pipeline final_render = zest_AddPipeline("pipeline_final_render");

    final_render->create_info = zest_CreatePipelineTemplateCreateInfo();
    final_render->create_info.viewport.extent = zest_GetSwapChainExtent();
    VkPushConstantRange final_render_pushconstant_range;
    final_render_pushconstant_range.offset = 0;
    final_render_pushconstant_range.size = sizeof(zest_final_render_push_constants_t);
    final_render_pushconstant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    zest_vec_push(final_render->create_info.pushConstantRange, final_render_pushconstant_range);
    final_render->create_info.renderPass = ZestRenderer->final_render_pass->render_pass;
    final_render->create_info.no_vertex_input = ZEST_TRUE;
    zest_SetText(&final_render->create_info.vertShaderFile, "swap_vert.spv");
    zest_SetText(&final_render->create_info.fragShaderFile, "swap_frag.spv");
    final_render->uniforms = 0;
    final_render->flags = zest_pipeline_set_flag_is_render_target_pipeline;
    final_render->descriptor_layout = zest_GetDescriptorSetLayout("Standard 1 uniform 1 sampler");
    final_render->create_info.descriptorSetLayout = final_render->descriptor_layout;

    zest_MakePipelineTemplate(final_render, render_pass, &final_render->create_info);
    final_render->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
    final_render->pipeline_template.depthStencil.depthTestEnable = VK_FALSE;

    final_render->pipeline_template.colorBlendAttachment = zest_PreMultiplyBlendStateForSwap();

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Final render pipeline" ZEST_NL);
    zest_BuildPipeline(final_render);
}

void zest__update_pipeline_template(zest_pipeline_template_t* pipeline_template, zest_pipeline_template_create_info_t* create_info) {
    pipeline_template->viewport.width = (float)create_info->viewport.extent.width;
    pipeline_template->viewport.height = (float)create_info->viewport.extent.height;
    pipeline_template->scissor.extent.width = create_info->viewport.extent.width;
    pipeline_template->scissor.extent.height = create_info->viewport.extent.height;

    pipeline_template->vertShaderStageInfo.pName = create_info->vertShaderFunctionName.str;
    pipeline_template->fragShaderStageInfo.pName = create_info->fragShaderFunctionName.str;

    if (!create_info->no_vertex_input) {
        ZEST_ASSERT(zest_vec_size(create_info->bindingDescriptions));    //If the pipeline is set to have vertex input, then you must add bindingDescriptions. You can use zest_AddVertexInputBindingDescription for this
        pipeline_template->vertexInputInfo.vertexBindingDescriptionCount = zest_vec_size(create_info->bindingDescriptions);
        pipeline_template->vertexInputInfo.pVertexBindingDescriptions = create_info->bindingDescriptions;
        pipeline_template->vertexInputInfo.pVertexAttributeDescriptions = create_info->attributeDescriptions;
    }
    pipeline_template->viewportState.pViewports = &pipeline_template->viewport;
    pipeline_template->viewportState.pScissors = &pipeline_template->scissor;
    pipeline_template->colorBlending.pAttachments = &pipeline_template->colorBlendAttachment;
    pipeline_template->pipelineLayoutInfo.pSetLayouts = &create_info->descriptorSetLayout->descriptor_layout;
    pipeline_template->dynamicState.pDynamicStates = create_info->dynamicStates;

    if (!zest_vec_empty(create_info->pushConstantRange)) {
        pipeline_template->pipelineLayoutInfo.pPushConstantRanges = create_info->pushConstantRange;
        pipeline_template->pipelineLayoutInfo.pushConstantRangeCount = zest_vec_size(create_info->pushConstantRange);
    }
}

void zest__create_empty_command_queue(zest_command_queue command_queue) {
    //VkCommandPoolCreateInfo cmd_pool_info = { 0 };
    //cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    //cmd_pool_info.queueFamilyIndex = ZestDevice->graphics_queue_family_index;
    //cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    //ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &cmd_pool_info, &ZestDevice->allocation_callbacks, &command_queue->command_pool));

    VkCommandBufferAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = ZEST_MAX_FIF;
    alloc_info.commandPool = ZestDevice->command_pool;
    ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, command_queue->command_buffer));

    for (ZEST_EACH_FIF_i) {
        zest_vec_push(command_queue->semaphores[i].fif_incoming_semaphores, ZestRenderer->semaphores[i].outgoing);
        zest_vec_push(command_queue->fif_wait_stage_flags[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }
    zest_command_queue_draw_commands_t blank_draw_commands = { 0 };
    zest_command_queue_draw_commands draw_commands = ZEST__NEW_ALIGNED(zest_command_queue_draw_commands, 16);
    memcpy(draw_commands, &blank_draw_commands, sizeof(zest_command_queue_draw_commands_t));
    //*draw_commands = blank_draw_commands;
    zest_bool is_aligned = zest__is_aligned(draw_commands, 16);
    draw_commands->render_pass = ZestRenderer->final_render_pass;
    draw_commands->get_frame_buffer = zest_GetRendererFrameBuffer;
    draw_commands->render_pass_function = zest__render_blank;
    draw_commands->viewport.extent = zest_GetSwapChainExtent();
    draw_commands->viewport.offset.x = draw_commands->viewport.offset.y = 0;
    draw_commands->viewport_scale.x = 1.f;
    draw_commands->viewport_scale.y = 1.f;
    zest_ConnectCommandQueueToPresent(command_queue);
    zest_map_insert(ZestRenderer->command_queue_draw_commands, "Blank Screen", draw_commands);
    zest_vec_push(command_queue->draw_commands, draw_commands);
}

void zest__render_blank(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, zest_render_pass render_pass, VkFramebuffer framebuffer) {
    VkRenderPassBeginInfo render_pass_info = { 0 };
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass->render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea = item->viewport;

    VkClearValue clear_values[2] = {
        [0] .color = {.float32[0] = 0.1f, .float32[1] = 0.0f, .float32[2] = 0.1f, .float32[3] = 1.f },
        [1].depthStencil = {.depth = 1.0f, .stencil = 0 }
    };

    render_pass_info.clearValueCount = 2;
    render_pass_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(command_buffer);
}

void zest__add_push_constant(zest_pipeline_template_create_info_t* create_info, VkPushConstantRange push_constant) {
    zest_vec_push(create_info->pushConstantRange, push_constant);
}

void zest__add_draw_routine(zest_command_queue_draw_commands draw_commands, zest_draw_routine draw_routine) {
    zest_vec_push(draw_commands->draw_routines, draw_routine);
    draw_routine->draw_commands = draw_commands;
}

zest_bool zest__acquire_next_swapchain_image() {
    VkResult result = vkAcquireNextImageKHR(ZestDevice->logical_device, ZestRenderer->swapchain, UINT64_MAX, ZestRenderer->semaphores[ZEST_FIF].outgoing, ZEST_NULL, &ZestRenderer->current_frame);

    //Has the window been resized? if so rebuild the swap chain.
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        zest__recreate_swapchain();
        return ZEST_FALSE;
    }
    ZEST_VK_CHECK_RESULT(result);
    return ZEST_TRUE;
}

void zest__draw_renderer_frame() {

    ZestRenderer->flags |= zest_renderer_flag_drawing_loop_running;
    ZestRenderer->flags &= ~zest_renderer_flag_swapchain_was_recreated;

    if (!zest__acquire_next_swapchain_image()) {
        return;
    }

    if (!ZestRenderer->active_command_queue) {
        //if there's no render queues at all, then we can draw this blank one to prevent errors when presenting the frame
        ZestRenderer->semaphores[ZEST_FIF].incoming = zest_GetCommandQueuePresentSemaphore(&ZestRenderer->empty_queue);
        zest_RecordCommandQueue(&ZestRenderer->empty_queue, ZEST_FIF);
        zest_SubmitCommandQueue(&ZestRenderer->empty_queue, ZestRenderer->fif_fence[ZEST_FIF]);
    }
    else {
        zest_RecordCommandQueue(ZestRenderer->active_command_queue, ZEST_FIF);
        zest_SubmitCommandQueue(ZestRenderer->active_command_queue, ZestRenderer->fif_fence[ZEST_FIF]);
    }

    zest__present_frame();
}

void zest__present_frame() {
    VkPresentInfoKHR presentInfo = { 0 };
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &ZestRenderer->semaphores[ZEST_FIF].incoming;
    VkSwapchainKHR swapChains[] = { ZestRenderer->swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &ZestRenderer->current_frame;
    presentInfo.pResults = ZEST_NULL;

    VkResult result = vkQueuePresentKHR(ZestDevice->graphics_queue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || ZestApp->window->framebuffer_resized || (ZestRenderer->flags & zest_renderer_flag_schedule_change_vsync)) {
        ZestApp->window->framebuffer_resized = ZEST_FALSE;
        ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_schedule_change_vsync);

        zest__recreate_swapchain();
        ZestRenderer->current_frame = 0;
    }
    else {
        ZEST_VK_CHECK_RESULT(result);
    }
    ZestDevice->current_fif = (ZestDevice->current_fif + 1) % ZEST_MAX_FIF;

}

void zest_RenderDrawRoutinesCallback(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, zest_render_pass render_pass, VkFramebuffer framebuffer) {
    VkRenderPassBeginInfo render_pass_info = { 0 };
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass->render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea = item->viewport;
    VkClearValue clear_values[2] = {
        [0] .color = {.float32[0] = item->cls_color.x,.float32[1] = item->cls_color.y,.float32[2] = item->cls_color.z,.float32[3] = item->cls_color.w},
        [1].depthStencil = {.depth = 1.0f,.stencil = 0 }
    };

    render_pass_info.clearValueCount = 2;
    render_pass_info.pClearValues = clear_values;

    //Copy the buffers

    for (zest_foreach_i(item->draw_routines)) {
        zest_draw_routine draw_routine = item->draw_routines[i];
        if (draw_routine->update_buffers_callback) {
            draw_routine->update_buffers_callback(draw_routine, command_buffer);
        }
    }

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport view = zest_CreateViewport(0.f, 0.f, (float)item->viewport.extent.width, (float)item->viewport.extent.height, 0.f, 1.f);
    VkRect2D scissor = zest_CreateRect2D(item->viewport.extent.width, item->viewport.extent.height, 0, 0);
    vkCmdSetViewport(command_buffer, 0, 1, &view);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    for (zest_foreach_i(item->draw_routines)) {
        zest_draw_routine draw_routine = item->draw_routines[i];
        if (draw_routine->draw_callback) {
            draw_routine->draw_callback(draw_routine, command_buffer);
        }
    }

    vkCmdEndRenderPass(command_buffer);

}

zest_command_queue_draw_commands zest_GetDrawCommands(const char* name) {
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->command_queue_draw_commands, name));    //That name could not be found in the storage
    return *zest_map_at(ZestRenderer->command_queue_draw_commands, name);
}
// --End Renderer functions

// --Command Queue functions
void zest__cleanup_command_queue(zest_command_queue command_queue) {
    for (ZEST_EACH_FIF_i) {
        for (zest_foreach_j(command_queue->semaphores[i].fif_outgoing_semaphores)) {
            VkSemaphore semaphore = command_queue->semaphores[i].fif_outgoing_semaphores[j];
            vkDestroySemaphore(ZestDevice->logical_device, semaphore, &ZestDevice->allocation_callbacks);
        }
    }
    vkFreeCommandBuffers(ZestDevice->logical_device, ZestDevice->command_pool, ZEST_MAX_FIF, command_queue->command_buffer);
    //vkDestroyCommandPool(ZestDevice->logical_device, command_queue->command_pool, &ZestDevice->allocation_callbacks);
}

void zest_ConnectCommandQueueToPresent(zest_command_queue sender) {
    VkSemaphoreCreateInfo semaphore_info = { 0 };
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (ZEST_EACH_FIF_i) {
        VkSemaphore semaphore = VK_NULL_HANDLE;
        ZEST_VK_CHECK_RESULT(vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, &ZestDevice->allocation_callbacks, &semaphore));
        zest_vec_push(sender->semaphores[i].fif_outgoing_semaphores, semaphore);
        sender->present_semaphore_index[i] = zest_vec_last_index(sender->semaphores[i].fif_outgoing_semaphores);
    }
}
// --Command Queue functions

// --General Helper Functions
VkViewport zest_CreateViewport(float x, float y, float width, float height, float minDepth, float maxDepth) {
    VkViewport viewport = { 0 };
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    return viewport;
}

VkRect2D zest_CreateRect2D(zest_uint width, zest_uint height, int offsetX, int offsetY) {
    VkRect2D rect2D = { 0 };
    rect2D.extent.width = width;
    rect2D.extent.height = height;
    rect2D.offset.x = offsetX;
    rect2D.offset.y = offsetY;
    return rect2D;
}

VkImageView zest__create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, zest_uint mip_levels, VkImageViewType view_type, zest_uint layer_count) {
    VkImageViewCreateInfo viewInfo = { 0 };
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = view_type;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspect_flags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mip_levels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = layer_count;

    VkImageView image_view;
    ZEST_VK_CHECK_RESULT(vkCreateImageView(ZestDevice->logical_device, &viewInfo, &ZestDevice->allocation_callbacks, &image_view) != VK_SUCCESS);

    return image_view;
}

void zest__create_temporary_image(zest_uint width, zest_uint height, zest_uint mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* memory) {
    VkImageCreateInfo image_info = { 0 };
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = mipLevels;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = numSamples;

    ZEST_VK_CHECK_RESULT(vkCreateImage(ZestDevice->logical_device, &image_info, &ZestDevice->allocation_callbacks, image));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(ZestDevice->logical_device, *image, &memRequirements);

    VkMemoryAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memRequirements.size;
    alloc_info.memoryTypeIndex = zest_find_memory_type(memRequirements.memoryTypeBits, properties);

    ZEST_VK_CHECK_RESULT(vkAllocateMemory(ZestDevice->logical_device, &alloc_info, &ZestDevice->allocation_callbacks, memory));

    vkBindImageMemory(ZestDevice->logical_device, *image, *memory, 0);
}

zest_buffer zest__create_image(zest_uint width, zest_uint height, zest_uint mip_levels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image) {
    VkImageCreateInfo image_info = { 0 };
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = mip_levels;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = numSamples;

    ZEST_VK_CHECK_RESULT(vkCreateImage(ZestDevice->logical_device, &image_info, &ZestDevice->allocation_callbacks, image));

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(ZestDevice->logical_device, *image, &memory_requirements);

    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.image_usage_flags = usage;
    buffer_info.property_flags = properties;
    zest_buffer_t* buffer = zest_CreateBuffer(memory_requirements.size, &buffer_info, *image);

    vkBindImageMemory(ZestDevice->logical_device, *image, zest_GetBufferDeviceMemory(buffer), buffer->memory_offset);

    return buffer;
}

zest_buffer zest__create_image_array(zest_uint width, zest_uint height, zest_uint mipLevels, zest_uint layers, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image) {
    VkImageCreateInfo image_info = { 0 };
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = mipLevels;
    image_info.arrayLayers = layers;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = numSamples;

    ZEST_VK_CHECK_RESULT(vkCreateImage(ZestDevice->logical_device, &image_info, &ZestDevice->allocation_callbacks, image));

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(ZestDevice->logical_device, *image, &memory_requirements);

    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.image_usage_flags = usage;
    buffer_info.property_flags = properties;
    zest_buffer_t* buffer = zest_CreateBuffer(memory_requirements.size, &buffer_info, *image);

    vkBindImageMemory(ZestDevice->logical_device, *image, zest_GetBufferDeviceMemory(buffer), buffer->memory_offset);
    return buffer;
}

void zest__copy_buffer_to_image(VkBuffer buffer, VkDeviceSize src_offset, VkImage image, zest_uint width, zest_uint height, VkImageLayout image_layout) {
    VkCommandBuffer command_buffer = zest__begin_single_time_commands();

    VkBufferImageCopy region = { 0 };
    region.bufferOffset = src_offset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset.x = 0;
    region.imageOffset.y = 0;
    region.imageOffset.z = 0;
    region.imageExtent.width = width;
    region.imageExtent.height = height;
    region.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(command_buffer, buffer, image, image_layout, 1, &region);

    zest__end_single_time_commands(command_buffer);
}

void zest__copy_buffer_regions_to_image(VkBufferImageCopy* regions, VkBuffer buffer, VkDeviceSize src_offset, VkImage image) {
    VkCommandBuffer command_buffer = zest__begin_single_time_commands();

    for (zest_foreach_i(regions)) {
        regions[i].bufferOffset += src_offset;
    }

    vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, zest_vec_size(regions), regions);

    zest__end_single_time_commands(command_buffer);
}

void zest__generate_mipmaps(VkImage image, VkFormat image_format, int32_t texture_width, int32_t texture_height, zest_uint mip_levels, zest_uint layer_count, VkImageLayout image_layout) {
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(ZestDevice->physical_device, image_format, &format_properties);

    ZEST_ASSERT(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT); //texture image format does not support linear blitting!

    VkCommandBuffer command_buffer = zest__begin_single_time_commands();

    VkImageMemoryBarrier barrier = { 0 };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layer_count;
    barrier.subresourceRange.levelCount = 1;

    int32_t mip_width = texture_width;
    int32_t mip_height = texture_height;

    for (zest_uint i = 1; i < mip_levels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, ZEST_NULL, 0, ZEST_NULL, 1, &barrier);

        VkImageBlit blit = { 0 };
        blit.srcOffsets[0].x = 0;
        blit.srcOffsets[0].y = 0;
        blit.srcOffsets[0].z = 0;
        blit.srcOffsets[1].x = mip_width;
        blit.srcOffsets[1].y = mip_height;
        blit.srcOffsets[1].z = 1;
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = layer_count;
        blit.dstOffsets[0].x = 0;
        blit.dstOffsets[0].y = 0;
        blit.dstOffsets[0].z = 0;
        blit.dstOffsets[1].x = mip_width > 1 ? mip_width / 2 : 1;
        blit.dstOffsets[1].y = mip_height > 1 ? mip_height / 2 : 1;
        blit.dstOffsets[1].z = 1;
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = layer_count;

        vkCmdBlitImage(command_buffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, ZEST_NULL, 0, ZEST_NULL, 1, &barrier);

        if (mip_width > 1) mip_width /= 2;
        if (mip_height > 1) mip_height /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = image_layout;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, ZEST_NULL, 0, ZEST_NULL, 1, &barrier);

    zest__end_single_time_commands(command_buffer);
}

void zest__transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, zest_uint mip_levels, zest_uint layerCount) {
    VkCommandBuffer command_buffer = zest__begin_single_time_commands();

    VkImageMemoryBarrier barrier = { 0 };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mip_levels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;
    barrier.srcAccessMask = 0;
    VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkPipelineStageFlags destination_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (zest__has_stencil_format(format)) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else {
        //throw std::invalid_argument("unsupported layout transition!");
        //Unsupported or General transition
    }

    vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0, 0, ZEST_NULL, 0, ZEST_NULL, 1, &barrier);

    zest__end_single_time_commands(command_buffer);

}

VkRenderPass zest__create_render_pass(VkFormat render_format, VkImageLayout final_layout, VkAttachmentLoadOp load_op, zest_bool depth_buffer) {
    VkAttachmentDescription color_attachment = { 0 };
    color_attachment.format = render_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = load_op;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = final_layout;

    VkAttachmentDescription depth_attachment = { 0 };
    depth_attachment.format = zest__find_depth_format();
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_attachment_ref = { 0 };
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = { 0 };
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = { 0 };
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = depth_buffer ? &depth_attachment_ref : VK_NULL_HANDLE;

    VkSubpassDependency dependency = { 0 };
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription* attachments = { 0 };
    zest_vec_push(attachments, color_attachment);
    if (depth_buffer) {
        zest_vec_push(attachments, depth_attachment);
    }
    VkRenderPassCreateInfo render_pass_info = { 0 };
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = zest_vec_size(attachments);
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    VkRenderPass render_pass;
    ZEST_VK_CHECK_RESULT(vkCreateRenderPass(ZestDevice->logical_device, &render_pass_info, &ZestDevice->allocation_callbacks, &render_pass));

    zest_vec_free(attachments);

    return render_pass;
}

VkFormat zest__find_depth_format() {
    VkFormat depth_formats[3] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
    return zest__find_supported_format(depth_formats, 3, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

zest_bool zest__has_stencil_format(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkFormat zest__find_supported_format(VkFormat* candidates, zest_uint candidates_count, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (int i = 0; i != candidates_count; ++i) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(ZestDevice->physical_device, candidates[i], &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return candidates[i];
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return candidates[i];
        }
    }

    return 0;
}

VkCommandBuffer zest__begin_single_time_commands() {
    VkCommandBufferAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = ZestDevice->one_time_command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info = { 0 };
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}

void zest__end_single_time_commands(VkCommandBuffer command_buffer) {
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info = { 0 };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(ZestDevice->one_time_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(ZestDevice->one_time_graphics_queue);

    vkFreeCommandBuffers(ZestDevice->logical_device, ZestDevice->one_time_command_pool, 1, &command_buffer);
}

void zest__insert_image_memory_barrier(VkCommandBuffer cmdbuffer, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange) {
    VkImageMemoryBarrier image_memory_barrier = { 0 };
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.srcAccessMask = srcAccessMask;
    image_memory_barrier.dstAccessMask = dstAccessMask;
    image_memory_barrier.oldLayout = oldImageLayout;
    image_memory_barrier.newLayout = newImageLayout;
    image_memory_barrier.image = image;
    image_memory_barrier.subresourceRange = subresourceRange;

    vkCmdPipelineBarrier(
        cmdbuffer,
        srcStageMask,
        dstStageMask,
        0,
        0, 0,
        0, 0,
        1, &image_memory_barrier);
}

zest_index zest__next_fif() {
    return (ZestDevice->current_fif + 1) % ZEST_MAX_FIF;
}

zest_create_info_t zest_CreateInfo() {
    zest_create_info_t create_info = {
        .memory_pool_size = zloc__MEGABYTE(64),
        .shader_path_prefix = "spv/",
        .log_path = NULL,
        .screen_width = 1280,
        .screen_height = 768,
        .screen_x = 0,
        .screen_y = 50,
        .virtual_width = 1280,
        .virtual_height = 768,
        .color_format = VK_FORMAT_R8G8B8A8_UNORM,
        .max_descriptor_pool_sets = 100,
        .flags = zest_init_flag_initialise_with_command_queue | zest_init_flag_enable_vsync | zest_init_flag_cache_shaders,
        .destroy_window_callback = zest__destroy_window_callback,
        .get_window_size_callback = zest__get_window_size_callback,
        .poll_events_callback = zest__os_poll_events,
        .add_platform_extensions_callback = zest__os_add_platform_extensions,
        .create_window_callback = zest__os_create_window,
        .create_window_surface_callback = zest__os_create_window_surface
    };
    return create_info;
}

void SetDescriptorPoolCount(zest_create_info_t* info, VkDescriptorType descriptor_type, zest_uint count) {
    VkDescriptorPoolSize pool_size;
    pool_size.type = descriptor_type;
    pool_size.descriptorCount = count;
    zest_map_insert_key(info->pool_counts, descriptor_type, pool_size);
}

char* zest_ReadEntireFile(const char* file_name, zest_bool terminate) {
    char* buffer = 0;
    FILE* file = NULL;
    file = zest__open_file(file_name, "rb");
    if (!file) {
        return buffer;
    }

    // file invalid? fseek() fail?
    if (file == NULL || fseek(file, 0, SEEK_END)) {
        fclose(file);
        return buffer;
    }

    long length = ftell(file);
    rewind(file);
    // Did ftell() fail?  Is the length too long?
    if (length == -1 || (unsigned long)length >= SIZE_MAX) {
        fclose(file);
        return buffer;
    }

    if (terminate) {
        zest_vec_resize(buffer, (zest_uint)length + 1);
    }
    else {
        zest_vec_resize(buffer, (zest_uint)length);
    }
    if (buffer == 0 || fread(buffer, 1, length, file) != length) {
        zest_vec_free(buffer);
        fclose(file);
        return buffer;
    }
    if (terminate) {
        buffer[length] = '\0';
    }

    fclose(file);
    return buffer;
}
// --End General Helper Functions

// --Command queue setup and modify functions
void zest__set_queue_context(zest_setup_context_type context) {
    if (ZestRenderer->setup_context.type == zest_setup_context_type_command_queue) {
        ZestRenderer->setup_context.draw_commands = ZEST_NULL;
        ZestRenderer->setup_context.layer = ZEST_NULL;
        ZestRenderer->setup_context.draw_routine = ZEST_NULL;
        ZestRenderer->setup_context.compute_index = -1;
    }
    else if (ZestRenderer->setup_context.type == zest_setup_context_type_render_pass) {
        ZestRenderer->setup_context.layer = ZEST_NULL;
        ZestRenderer->setup_context.compute_index = -1;
        ZestRenderer->setup_context.draw_routine = ZEST_NULL;
    }
}

zest_command_queue zest_NewCommandQueue(const char* name) {
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_none);    //Current setup context must be none. You can't setup a new command queue within another one
    zest__set_queue_context(zest_setup_context_type_command_queue);
    zest_command_queue command_queue = zest__create_command_queue(name);
    command_queue->flags = zest_command_queue_flag_present_dependency;
    ZestRenderer->setup_context.command_queue = command_queue;
    ZestRenderer->setup_context.type = zest_setup_context_type_command_queue;
    zest_ConnectPresentToCommandQueue(command_queue, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    return command_queue;
}

zest_command_queue zest_NewFloatingCommandQueue(const char* name) {
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_none);    //Current setup context must be none. You can't setup a new command queue within another one
    zest__set_queue_context(zest_setup_context_type_command_queue);
    zest_command_queue command_queue = zest__create_command_queue(name);
    command_queue->flags = 0;
    ZestRenderer->setup_context.command_queue = command_queue;
    ZestRenderer->setup_context.type = zest_setup_context_type_command_queue;
    return command_queue;
}

zest_command_queue zest__create_command_queue(const char* name) {
    zest_command_queue_t blank_command_queue = { 0 };
    zest_command_queue command_queue = ZEST__NEW(zest_command_queue);
    *command_queue = blank_command_queue;
    command_queue->name = name;

    for (ZEST_EACH_FIF_i) {
        command_queue->present_semaphore_index[i] = -1;
    }

    /*
        VkCommandPoolCreateInfo cmd_pool_info = { 0 };
        cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmd_pool_info.queueFamilyIndex = ZestDevice->graphics_queue_family_index;
        cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &cmd_pool_info, &ZestDevice->allocation_callbacks, &command_queue->command_pool));
    */

    VkCommandBufferAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = ZEST_MAX_FIF;
    alloc_info.commandPool = ZestDevice->command_pool;
    ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, command_queue->command_buffer));

    zest_map_insert(ZestRenderer->command_queues, name, command_queue);
    return command_queue;
}

zest_command_queue zest_GetCommandQueue(const char* name) {
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->command_queues, name)); //Not a valid command queue index
    return *zest_map_at(ZestRenderer->command_queues, name);
}

zest_command_queue_draw_commands zest_GetCommandQueueDrawCommands(const char* name) {
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->command_queue_draw_commands, name));    //name of command queue draw commands not found
    return *zest_map_at(ZestRenderer->command_queue_draw_commands, name);
}

void zest_SetDrawCommandsClsColor(zest_command_queue_draw_commands draw_commands, float r, float g, float b, float a) {
    draw_commands->cls_color = zest_Vec4Set(r, g, b, a);
}

void zest_ModifyCommandQueue(zest_command_queue command_queue) {
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_none);    //Current setup context must be none.
    zest__set_queue_context(zest_setup_context_type_command_queue);
    ZestRenderer->setup_context.command_queue = command_queue;
    ZestRenderer->setup_context.type = zest_setup_context_type_command_queue;
}

void zest_ModifyDrawCommands(zest_command_queue_draw_commands draw_commands) {
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_command_queue);    //Current setup context must be a command queue.
    int draw_commands_found = 0;
    for (zest_foreach_i(ZestRenderer->setup_context.command_queue->draw_commands)) {
        if (ZestRenderer->setup_context.command_queue->draw_commands[i] == draw_commands) {
            draw_commands_found = 1;
        }
    }
    ZEST_ASSERT(draw_commands_found);    //The draw commands that you're editing must exist within the command queue context. Call zest_ModifyCommandQueue with
    //the appropriate zest_command_queue
    zest__set_queue_context(zest_setup_context_type_render_pass);
    ZestRenderer->setup_context.draw_commands = draw_commands;
    ZestRenderer->setup_context.type = zest_setup_context_type_render_pass;
}

zest_draw_routine zest_ContextDrawRoutine() {
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_draw_routine || ZestRenderer->setup_context.type == zest_setup_context_type_layer);
    return ZestRenderer->setup_context.draw_routine;
}

void zest_ContextSetClsColor(float r, float g, float b, float a) {
    assert(ZestRenderer->setup_context.type == zest_setup_context_type_render_pass || ZestRenderer->setup_context.type == zest_setup_context_type_draw_routine || ZestRenderer->setup_context.type == zest_setup_context_type_layer);    //The current setup context must be a render pass using BeginRenderPassSetup or BeginRenderItemSC
    ZestRenderer->setup_context.draw_commands->cls_color = zest_Vec4Set(r, g, b, a);
}

void zest_FinishQueueSetup() {
    ZEST_ASSERT(ZestRenderer->setup_context.command_queue != ZEST_NULL);        //Trying to validate a queue that has no context. Finish queue setup must me run at the end of a queue setup
    zest_ValidateQueue(ZestRenderer->setup_context.command_queue);
    ZestRenderer->setup_context.command_queue = ZEST_NULL;
    ZestRenderer->setup_context.draw_commands = ZEST_NULL;
    ZestRenderer->setup_context.draw_routine = ZEST_NULL;
    ZestRenderer->setup_context.layer = ZEST_NULL;
    ZestRenderer->setup_context.type = zest_setup_context_type_none;
}

void zest_ValidateQueue(zest_command_queue command_queue) {
    if (command_queue->flags & zest_command_queue_flag_present_dependency) {
        zest_bool present_found = 0;
        zest_bool present_flags_found = 0;
        for (ZEST_EACH_FIF_i) {
            for (zest_foreach_j(command_queue->semaphores[i].fif_incoming_semaphores)) {
                present_found += (command_queue->semaphores[i].fif_incoming_semaphores[j] == ZestRenderer->semaphores[i].outgoing);
            }
            for (zest_foreach_j(command_queue->fif_wait_stage_flags[i])) {
                present_flags_found += (command_queue->fif_wait_stage_flags[i][j] == VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            }
        }
        ZEST_ASSERT(present_found == ZEST_MAX_FIF);                //The command queue was created with zest_command_queue_flag_present_dependency but no semaphore was found to connect the two
        ZEST_ASSERT(present_flags_found == ZEST_MAX_FIF);        //The command queue was created with zest_command_queue_flag_present_dependency but the stage flags should be set to VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    }
    command_queue->flags |= zest_command_queue_flag_validated;
}

VkCommandBuffer zest_CurrentCommandBuffer() {
    return ZestRenderer->current_command_buffer;
}

zest_command_queue zest_CurrentCommandQueue() {
    return ZestRenderer->active_command_queue;
}

zest_command_queue_draw_commands_t* zest_CurrentRenderPass() {
    return ZestRenderer->current_draw_commands;
}

void zest_ConnectPresentToCommandQueue(zest_command_queue receiver, VkPipelineStageFlags stage_flags) {
    for (ZEST_EACH_FIF_i) {
        zest_vec_push(receiver->semaphores[i].fif_incoming_semaphores, ZestRenderer->semaphores[i].outgoing);
        zest_vec_push(receiver->fif_wait_stage_flags[i], stage_flags);
    }
}

void zest_ConnectCommandQueues(zest_command_queue sender, zest_command_queue receiver, VkPipelineStageFlags stage_flags) {
    VkSemaphoreCreateInfo semaphore_info = { 0 };
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (ZEST_EACH_FIF_i) {
        VkSemaphore semaphore = VK_NULL_HANDLE;
        ZEST_VK_CHECK_RESULT(vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, &ZestDevice->allocation_callbacks, &semaphore));
        zest_vec_push(sender->semaphores[i].fif_outgoing_semaphores, semaphore);
        zest_vec_push(receiver->semaphores[i].fif_incoming_semaphores, semaphore);
        zest_vec_push(sender->fif_wait_stage_flags[i], stage_flags);
    }
}

void zest_ConnectQueueTo(zest_command_queue receiver, VkPipelineStageFlags stage_flags) {
    ZEST_ASSERT(ZestRenderer->setup_context.type != zest_setup_context_type_none);    //Must be within a command queue setup context
    zest_ConnectCommandQueues(ZestRenderer->setup_context.command_queue, receiver, stage_flags);
}

void zest_ConnectQueueToPresent() {
    ZEST_ASSERT(ZestRenderer->setup_context.type != zest_setup_context_type_none);    //Must be within a command queue setup context
    zest_command_queue command_queue = ZestRenderer->setup_context.command_queue;
    zest_ConnectCommandQueueToPresent(command_queue);
}

VkSemaphore zest_GetCommandQueuePresentSemaphore(zest_command_queue command_queue) {
    ZEST_ASSERT(command_queue->present_semaphore_index[ZEST_FIF] != -1);
    return command_queue->semaphores[ZEST_FIF].fif_outgoing_semaphores[command_queue->present_semaphore_index[ZEST_FIF]];
}

zest_command_queue_draw_commands zest_NewDrawCommandSetupSwap(const char* name) {
    zest__set_queue_context(zest_setup_context_type_render_pass);
    zest_command_queue command_queue = ZestRenderer->setup_context.command_queue;
    zest_command_queue_draw_commands draw_commands = zest__create_command_queue_draw_commands(name);
    zest_vec_push(command_queue->draw_commands, draw_commands);
    draw_commands->name = name;
    draw_commands->render_pass = ZestRenderer->final_render_pass;
    draw_commands->get_frame_buffer = zest_GetRendererFrameBuffer;
    draw_commands->render_pass_function = zest_RenderDrawRoutinesCallback;
    draw_commands->viewport.extent = zest_GetSwapChainExtent();
    draw_commands->viewport.offset.x = draw_commands->viewport.offset.y = 0;
    ZestRenderer->setup_context.draw_commands = draw_commands;
    ZestRenderer->setup_context.type = zest_setup_context_type_render_pass;
    return draw_commands;
}

zest_command_queue_draw_commands zest_NewDrawCommandSetupRenderTargetSwap(const char* name, zest_render_target render_target) {
    ZEST_ASSERT(render_target);    //render_target must be valid
    zest__set_queue_context(zest_setup_context_type_render_pass);
    zest_command_queue command_queue = ZestRenderer->setup_context.command_queue;
    zest_command_queue_draw_commands draw_commands = zest__create_command_queue_draw_commands(name);
    zest_vec_push(command_queue->draw_commands, draw_commands);
    draw_commands->name = name;
    draw_commands->render_pass = ZestRenderer->final_render_pass;
    draw_commands->get_frame_buffer = zest_GetRendererFrameBuffer;
    draw_commands->render_pass_function = zest_DrawRenderTargetsToSwapchain;
    draw_commands->render_target = render_target;
    zest_vec_push(draw_commands->render_targets, render_target);
    draw_commands->viewport.extent = zest_GetSwapChainExtent();
    draw_commands->viewport.offset.x = draw_commands->viewport.offset.y = 0;
    ZestRenderer->setup_context.draw_commands = draw_commands;
    ZestRenderer->setup_context.type = zest_setup_context_type_render_pass;
    return draw_commands;
}

void zest_AddRenderTarget(zest_render_target render_target) {
    ZEST_ASSERT(render_target);        //render_target must be a valid render target
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_render_pass);    //The current setup context must be a render pass using BeginRenderPassSetup or BeginRenderPassSetupSC
    zest_vec_push(ZestRenderer->setup_context.draw_commands->render_targets, render_target);
}

zest_command_queue_draw_commands zest_NewDrawCommandSetup(const char* name, zest_render_target render_target) {
    ZEST_ASSERT(render_target);        //render_target must be a valid index to a render target
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->command_queue_draw_commands, name));        //There are already draw commands with that name
    zest__set_queue_context(zest_setup_context_type_render_pass);
    zest_command_queue command_queue = ZestRenderer->setup_context.command_queue;
    zest_command_queue_draw_commands draw_commands = zest__create_command_queue_draw_commands(name);
    zest_vec_push(command_queue->draw_commands, draw_commands);
    draw_commands->name = name;
    draw_commands->render_pass = render_target->render_pass;
    draw_commands->get_frame_buffer = zest_GetRenderTargetFrameBufferCallback;
    draw_commands->render_pass_function = zest_RenderDrawRoutinesCallback;
    draw_commands->viewport = render_target->viewport;
    draw_commands->render_target = render_target;
    if (zest_Vec2Length2(render_target->create_info.ratio_of_screen_size)) {
        draw_commands->viewport_scale = render_target->create_info.ratio_of_screen_size;
        draw_commands->viewport_type = zest_render_viewport_type_scale_with_window;
    }
    else if (ZEST__FLAGGED(render_target->create_info.flags, zest_render_target_flag_fixed_size)) {
        draw_commands->viewport_type = zest_render_viewport_type_fixed;
    }
    else {
        draw_commands->viewport_type = zest_render_viewport_type_scale_with_window;
        draw_commands->viewport_scale = zest_Vec2Set(1.f, 1.f);
    }
    ZestRenderer->setup_context.draw_commands = draw_commands;
    ZestRenderer->setup_context.type = zest_setup_context_type_render_pass;
    return draw_commands;
}

void zest_SetDrawCommandsCallback(void(*render_pass_function)(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, zest_render_pass render_pass, VkFramebuffer framebuffer)) {
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_render_pass);    //The current setup context must be a render pass using BeginRenderPassSetup or BeginRenderPassSetupSC
    ZestRenderer->setup_context.draw_commands->render_pass_function = render_pass_function;
}

zest_draw_routine zest_GetDrawRoutine(const char* name) {
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->draw_routines, name));
    return *zest_map_at(ZestRenderer->draw_routines, name);
}

zest_command_queue_draw_commands zest__create_command_queue_draw_commands(const char* name) {
    zest_command_queue_draw_commands_t blank_draw_commands = { 0 };
    zest_command_queue_draw_commands draw_commands = ZEST__NEW_ALIGNED(zest_command_queue_draw_commands, 16);
    *draw_commands = blank_draw_commands;
    draw_commands->name = name;
    draw_commands->viewport_scale.x = 1.f;
    draw_commands->viewport_scale.y = 1.f;
    draw_commands->render_target = ZEST_NULL;
    draw_commands->viewport_type = zest_render_viewport_type_scale_with_window;

    //draw_commands.command_queue_index = command_queue.index_in_renderer;
    zest_map_insert(ZestRenderer->command_queue_draw_commands, name, draw_commands);
    return draw_commands;
}

zest_layer zest_NewMeshLayer(const char* name, zest_size vertex_struct_size) {
    zest__set_queue_context(zest_setup_context_type_layer);
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_render_pass || ZestRenderer->setup_context.type == zest_setup_context_type_layer);    //The current setup context must be a render pass, layer or compute
    ZEST_ASSERT(strlen(name) <= 50);    //Layer name must be less then 50 characters
    zest_command_queue_draw_commands draw_commands = ZestRenderer->setup_context.draw_commands;
    zest_draw_routine_t* draw_routine = { 0 };
    draw_routine = zest__create_draw_routine_with_builtin_layer(name, zest_builtin_layer_mesh);
    zest_layer layer = (zest_layer)draw_routine->draw_data;
    zest__initialise_mesh_layer(layer, vertex_struct_size, zloc__MEGABYTE(1));
    ZestRenderer->setup_context.layer = (zest_layer)draw_routine->draw_data;
    layer->draw_commands = draw_commands;
    layer->name = name;
    zest_SetLayerSize(layer, (float)draw_commands->viewport.extent.width, (float)draw_commands->viewport.extent.height);
    zest_AddDrawRoutineToDrawCommands(draw_commands, draw_routine);
    ZestRenderer->setup_context.type = zest_setup_context_type_layer;
    return layer;
}

zest_layer zest_CreateBuiltinSpriteLayer(const char* name) {
    zest_layer layer = zest_NewLayer();
    layer->name = name;
    layer->layer_type = zest_builtin_layer_sprites;
    zest__initialise_sprite_layer(layer, 1000);
    zest_map_insert(ZestRenderer->layers, name, layer);
    return layer;
}

zest_layer zest_CreateBuiltin2dLineLayer(const char* name) {
    zest_layer layer = zest_NewLayer();
    layer->name = name;
    layer->layer_type = zest_builtin_layer_lines;
    zest__initialise_shape_layer(layer, 1000);
    zest_map_insert(ZestRenderer->layers, name, layer);
    return layer;
}

zest_layer zest_CreateBuiltin3dLineLayer(const char* name) {
    zest_layer layer = zest_NewLayer();
    layer->name = name;
    layer->layer_type = zest_builtin_layer_3dlines;
    zest__initialise_3dline_layer(layer, 1000);
    zest_map_insert(ZestRenderer->layers, name, layer);
    return layer;
}

zest_layer zest_CreateBuiltinBillboardLayer(const char* name) {
    zest_layer layer = zest_NewLayer();
    layer->name = name;
    layer->layer_type = zest_builtin_layer_billboards;
    zest__initialise_billboard_layer(layer, 1000);
    zest_map_insert(ZestRenderer->layers, name, layer);
    return layer;
}

zest_layer zest_CreateBuiltinFontLayer(const char* name) {
    zest_layer layer = zest_NewLayer();
    layer->name = name;
    layer->layer_type = zest_builtin_layer_fonts;
    zest__initialise_font_layer(layer, 1000);
    zest_map_insert(ZestRenderer->layers, name, layer);
    return layer;
}

zest_layer zest_CreateBuiltinMeshLayer(const char* name) {
    zest_layer layer = zest_NewLayer();
    layer->name = name;
    layer->layer_type = zest_builtin_layer_mesh;
    zest__initialise_mesh_layer(layer, sizeof(zest_textured_vertex_t), 1000);
    zest_map_insert(ZestRenderer->layers, name, layer);
    return layer;
}

zest_layer zest_CreateBuiltinInstanceMeshLayer(const char* name) {
    zest_layer layer = zest_NewLayer();
    layer->name = name;
    layer->layer_type = zest_builtin_layer_mesh_instance;
    zest__initialise_instance_mesh_layer(layer, sizeof(zest_mesh_instance_t), 100);
    zest_map_insert(ZestRenderer->layers, name, layer);
    return layer;
}

void zest_InsertLayer(zest_layer layer) {
    zest_map_insert(ZestRenderer->layers, layer->name, layer);
}

zest_layer zest_NewBuiltinLayerSetup(const char* name, zest_builtin_layer_type builtin_layer) {
    zest__set_queue_context(zest_setup_context_type_layer);
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_render_pass || ZestRenderer->setup_context.type == zest_setup_context_type_layer);    //The current setup context must be a render pass, layer or compute
    ZEST_ASSERT(strlen(name) <= 50);    //Layer name must be less then 50 characters
    zest_command_queue_draw_commands draw_commands = ZestRenderer->setup_context.draw_commands;
    zest_draw_routine draw_routine = zest__create_draw_routine_with_builtin_layer(name, builtin_layer);
    zest_layer layer = (zest_layer)draw_routine->draw_data;
    ZestRenderer->setup_context.layer = layer;
    layer->draw_commands = ZestRenderer->setup_context.draw_commands;
    layer->name = name;
    layer->layer_type = builtin_layer;
    zest_SetLayerSize(layer, (float)draw_commands->viewport.extent.width, (float)draw_commands->viewport.extent.height);
    zest_AddDrawRoutineToDrawCommands(draw_commands, draw_routine);
    ZestRenderer->setup_context.type = zest_setup_context_type_layer;
    return layer;
}

void zest_AddLayer(zest_layer layer) {
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_render_pass || ZestRenderer->setup_context.type == zest_setup_context_type_layer);    //The current setup context must be a render pass, layer or compute
    zest__set_queue_context(zest_setup_context_type_layer);
    zest_command_queue_draw_commands draw_commands = ZestRenderer->setup_context.draw_commands;
    if (!layer->draw_routine) {
        ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->draw_routines, layer->name));    //A draw routine with that name already exists
        zest_draw_routine_t blank_draw_routine = { 0 };
        zest_draw_routine draw_routine = ZEST__NEW(zest_draw_routine);
        *draw_routine = blank_draw_routine;
        draw_routine->name = layer->name;
        draw_routine->command_queue = ZestRenderer->setup_context.command_queue;
        draw_routine->draw_commands = ZestRenderer->setup_context.draw_commands;
        if (layer->layer_type == zest_builtin_layer_sprites) {
            draw_routine->draw_callback = zest__draw_sprite_layer_callback;
            draw_routine->draw_data = layer;
            draw_routine->update_buffers_callback = zest__update_instance_layer_buffers_callback;
        }
        else if (layer->layer_type == zest_builtin_layer_billboards) {
            draw_routine->draw_callback = zest__draw_billboard_layer_callback;
            draw_routine->draw_data = layer;
            draw_routine->update_buffers_callback = zest__update_instance_layer_buffers_callback;
        }
        else if (layer->layer_type == zest_builtin_layer_mesh) {
            draw_routine->draw_data = layer;
            draw_routine->update_buffers_callback = zest_UploadMeshBuffersCallback;
            draw_routine->draw_callback = zest__draw_mesh_layer_callback;
        }
        else if (layer->layer_type == zest_builtin_layer_mesh) {
            draw_routine->draw_data = layer;
            draw_routine->update_buffers_callback = zest__update_instance_layer_buffers_callback;
            draw_routine->draw_callback = zest__draw_instance_mesh_layer_callback;
        }
        else if (layer->layer_type == zest_builtin_layer_fonts) {
            draw_routine->draw_callback = zest__draw_font_layer_callback;
            draw_routine->draw_data = layer;
            draw_routine->update_buffers_callback = zest__update_instance_layer_buffers_callback;
        }
        zest_map_insert(ZestRenderer->draw_routines, draw_routine->name, draw_routine);
        ZestRenderer->setup_context.draw_routine = draw_routine;
        layer->draw_routine = draw_routine;
    }
    ZestRenderer->setup_context.layer = layer;
    ZestRenderer->setup_context.type = zest_setup_context_type_layer;
    ZestRenderer->setup_context.draw_routine = layer->draw_routine;
    zest_SetLayerSize(layer, (float)draw_commands->viewport.extent.width, (float)draw_commands->viewport.extent.height);
    zest_AddDrawRoutineToDrawCommands(draw_commands, layer->draw_routine);
}

zest_command_queue_compute zest_CreateComputeItem(const char* name, zest_command_queue command_queue) {
    zest_command_queue_compute_t blank_compute_item = { 0 };
    zest_command_queue_compute compute_commands = ZEST__NEW(zest_command_queue_compute);
    *compute_commands = blank_compute_item;
    zest_map_insert(ZestRenderer->command_queue_computes, name, compute_commands);
    zest_vec_push(command_queue->compute_commands, compute_commands);
    return compute_commands;
}

zest_command_queue_compute zest_NewComputeSetup(const char* name, zest_compute compute_shader, void(*compute_function)(zest_command_queue_compute item)) {
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_command_queue);                //Current setup context must be command_queue. Add your compute shaders first before
    //adding any other draw routines. Also make sure that you call zest_FinishQueueSetup everytime
    //everytime you create or modify a queue
    zest__set_queue_context(zest_setup_context_type_compute);

    zest_command_queue command_queue = ZestRenderer->setup_context.command_queue;
    zest_command_queue_compute compute_commands = zest_CreateComputeItem(name, command_queue);
    zest_vec_push(compute_commands->compute_shaders, compute_shader);
    compute_commands->compute_function = compute_function;
    return compute_commands;
}

zest_draw_routine zest__create_draw_routine_with_builtin_layer(const char* name, zest_builtin_layer_type builtin_layer) {
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->draw_routines, name));    //A draw routine with that name already exists

    zest_draw_routine_t blank_draw_routine = { 0 };
    zest_draw_routine draw_routine = ZEST__NEW(zest_draw_routine);
    *draw_routine = blank_draw_routine;

    zest_layer layer = 0;
    if (builtin_layer == zest_builtin_layer_sprites) {
        layer = zest_CreateBuiltinSpriteLayer(name);
        draw_routine->draw_callback = zest__draw_sprite_layer_callback;
        draw_routine->draw_data = layer;
        draw_routine->update_buffers_callback = zest__update_instance_layer_buffers_callback;
    }
    else if (builtin_layer == zest_builtin_layer_lines) {
        layer = zest_CreateBuiltin2dLineLayer(name);
        draw_routine->draw_callback = zest__draw_shape_layer_callback;
        draw_routine->draw_data = layer;
        draw_routine->update_buffers_callback = zest__update_instance_layer_buffers_callback;
    }
    else if (builtin_layer == zest_builtin_layer_3dlines) {
        layer = zest_CreateBuiltin3dLineLayer(name);
        draw_routine->draw_callback = zest__draw_3dline_layer_callback;
        draw_routine->draw_data = layer;
        draw_routine->update_buffers_callback = zest__update_instance_layer_buffers_callback;
    }
    else if (builtin_layer == zest_builtin_layer_billboards) {
        layer = zest_CreateBuiltinBillboardLayer(name);
        draw_routine->draw_callback = zest__draw_billboard_layer_callback;
        draw_routine->draw_data = layer;
        draw_routine->update_buffers_callback = zest__update_instance_layer_buffers_callback;
    }
    else if (builtin_layer == zest_builtin_layer_mesh) {
        layer = zest_CreateBuiltinMeshLayer(name);
        draw_routine->draw_data = layer;
        draw_routine->update_buffers_callback = zest_UploadMeshBuffersCallback;
        draw_routine->draw_callback = zest__draw_mesh_layer_callback;
    }
    else if (builtin_layer == zest_builtin_layer_mesh_instance) {
        layer = zest_CreateBuiltinInstanceMeshLayer(name);
        draw_routine->draw_data = layer;
        draw_routine->update_buffers_callback = zest__update_instance_layer_buffers_callback;
        draw_routine->draw_callback = zest__draw_instance_mesh_layer_callback;
    }
    else if (builtin_layer == zest_builtin_layer_fonts) {
        layer = zest_CreateBuiltinFontLayer(name);
        draw_routine->draw_callback = zest__draw_font_layer_callback;
        draw_routine->draw_data = layer;
        draw_routine->update_buffers_callback = zest__update_instance_layer_buffers_callback;
    }
    draw_routine->name = name;
    draw_routine->command_queue = ZestRenderer->setup_context.command_queue;
    draw_routine->draw_commands = ZestRenderer->setup_context.draw_commands;

    zest_map_insert(ZestRenderer->draw_routines, name, draw_routine);
    ZestRenderer->setup_context.draw_routine = draw_routine;

    layer->draw_routine = draw_routine;
    return draw_routine;
}

void zest__update_command_queue_viewports(void) {
    for (zest_map_foreach_i(ZestRenderer->command_queues)) {
        zest_command_queue queue = *zest_map_at_index(ZestRenderer->command_queues, i);
        for (zest_foreach_j(queue->draw_commands)) {
            zest_command_queue_draw_commands draw_commands = queue->draw_commands[j];
            if (draw_commands->render_target != 0) {
                draw_commands->viewport = draw_commands->render_target->viewport;
            }
        }
    }
}

zest_draw_routine zest_CreateDrawRoutine(const char* name) {
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->draw_routines, name));    //A draw routine with that name already exists

    zest_draw_routine_t blank_draw_routine = { 0 };
    zest_draw_routine draw_routine = ZEST__NEW(zest_draw_routine);
    *draw_routine = blank_draw_routine;

    draw_routine->name = name;
    zest_map_insert(ZestRenderer->draw_routines, name, draw_routine);
    return draw_routine;
}

void zest_AddDrawRoutine(zest_draw_routine draw_routine) {
    zest__set_queue_context(zest_setup_context_type_layer);
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_render_pass || ZestRenderer->setup_context.type == zest_setup_context_type_layer);    //The current setup context must be a render pass, layer or compute
    zest_command_queue_draw_commands draw_commands = ZestRenderer->setup_context.draw_commands;
    ZestRenderer->setup_context.type = zest_setup_context_type_layer;
    ZestRenderer->setup_context.draw_routine = draw_routine;
    draw_routine->command_queue = ZestRenderer->setup_context.command_queue;
    draw_routine->draw_commands = ZestRenderer->setup_context.draw_commands;
    zest_AddDrawRoutineToDrawCommands(draw_commands, draw_routine);
}

void zest_AddDrawRoutineToDrawCommands(zest_command_queue_draw_commands draw_commands, zest_draw_routine draw_routine) {
    zest_vec_push(draw_commands->draw_routines, draw_routine);
    //draw_routine.command_queue = command_queue_index;
    draw_routine->draw_commands = draw_commands;
}

zest_layer zest_GetLayer(const char* name) {
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->layers, name));        //That index could not be found in the storage
    return *zest_map_at(ZestRenderer->layers, name);
}

void zest_ResetComputeRoutinesIndex(zest_command_queue_compute compute_queue) {
    compute_queue->shader_index = 0;
}

zest_compute zest_NextComputeRoutine(zest_command_queue_compute compute_queue) {
    if (compute_queue->shader_index < (zest_index)zest_vec_size(compute_queue->compute_shaders)) {
        return compute_queue->compute_shaders[compute_queue->shader_index++];
    }
    else {
        return 0;
    }
}

void zest_RecordCommandQueue(zest_command_queue command_queue, zest_index fif) {
    VkCommandBufferBeginInfo begin_info = { 0 };
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkResetCommandBuffer(command_queue->command_buffer[fif], 0);
    ZEST_VK_CHECK_RESULT(vkBeginCommandBuffer(command_queue->command_buffer[fif], &begin_info));

    ZestRenderer->current_command_buffer = command_queue->command_buffer[fif];
    for (zest_foreach_i(command_queue->compute_commands)) {
        zest_command_queue_compute compute_commands = command_queue->compute_commands[i];
        ZEST_ASSERT(compute_commands->compute_function);        //Compute item must have its compute function callback set
        compute_commands->shader_index = 0;
        ZestRenderer->current_compute_routine = compute_commands;
        compute_commands->compute_function(compute_commands);
    }

    for (zest_foreach_i(command_queue->draw_commands)) {
        zest_command_queue_draw_commands draw_commands = command_queue->draw_commands[i];
        ZestRenderer->current_draw_commands = draw_commands;
        draw_commands->render_pass_function(draw_commands, command_queue->command_buffer[fif], draw_commands->render_pass, draw_commands->get_frame_buffer(draw_commands));
    }

    ZEST_VK_CHECK_RESULT(vkEndCommandBuffer(command_queue->command_buffer[fif]));

    ZestRenderer->current_command_buffer = ZEST_NULL;
    ZestRenderer->current_compute_routine = ZEST_NULL;
    ZestRenderer->current_draw_commands = ZEST_NULL;
}

void zest_SubmitCommandQueue(zest_command_queue command_queue, VkFence fence) {
    VkSubmitInfo submit_info = { 0 };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    if (!zest_vec_empty(command_queue->semaphores[ZEST_FIF].fif_incoming_semaphores)) {
        submit_info.pWaitSemaphores = command_queue->semaphores[ZEST_FIF].fif_incoming_semaphores;
        submit_info.waitSemaphoreCount = zest_vec_size(command_queue->semaphores[ZEST_FIF].fif_incoming_semaphores);
        submit_info.pWaitDstStageMask = command_queue->fif_wait_stage_flags[ZEST_FIF];
    }
    else {
        submit_info.pWaitSemaphores = VK_NULL_HANDLE;
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitDstStageMask = VK_NULL_HANDLE;
    }
    submit_info.signalSemaphoreCount = zest_vec_size(command_queue->semaphores[ZEST_FIF].fif_outgoing_semaphores);
    submit_info.pSignalSemaphores = command_queue->semaphores[ZEST_FIF].fif_outgoing_semaphores;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_queue->command_buffer[ZEST_FIF];

    vkResetFences(ZestDevice->logical_device, 1, &fence);
    ZEST_VK_CHECK_RESULT(vkQueueSubmit(ZestDevice->graphics_queue, 1, &submit_info, fence));
}
// --End Command queue setup and modify functions

// --Texture and Image functions
zest_map_textures* zest_GetTextures() {
    return &ZestRenderer->textures;
}

zest_texture zest_NewTexture() {
    zest_texture_t blank = { 0 };
    zest_texture texture = ZEST__NEW(zest_texture);
    *texture = blank;
    texture->struct_type = zest_struct_type_texture;
    texture->flags = zest_texture_flag_use_filtering;
    texture->sampler = VK_NULL_HANDLE;
    texture->storage_type = zest_texture_storage_type_single;
    texture->image_index = 1;
    texture->image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    texture->image_format = VK_FORMAT_R8G8B8A8_UNORM;
    texture->color_channels = 4;
    texture->imgui_blend_type = zest_imgui_blendtype_none;
    texture->image_view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    texture->packed_border_size = 0;
    texture->sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    texture->sampler_info.magFilter = VK_FILTER_LINEAR;
    texture->sampler_info.minFilter = VK_FILTER_LINEAR;
    texture->sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texture->sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texture->sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texture->sampler_info.anisotropyEnable = VK_FALSE;
    texture->sampler_info.maxAnisotropy = 1.f;
    texture->sampler_info.unnormalizedCoordinates = VK_FALSE;
    texture->sampler_info.compareEnable = VK_FALSE;
    texture->sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    texture->sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    texture->sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    texture->sampler_info.mipLodBias = 0.f;
    texture->sampler_info.minLod = 0.0f;
    texture->sampler_info.maxLod = 1.0f;
    texture->sampler_info.pNext = VK_NULL_HANDLE;
    texture->sampler_info.flags = 0;
    texture->sampler = VK_NULL_HANDLE;
    texture->texture_layer_size = 1024;
    texture->stream_staging_buffer = ZEST_NULL;

    return texture;
}

zest_image_t zest_NewImage(void) {
    zest_image_t image = { 0 };
    image.struct_type = zest_struct_type_image;
    return image;
}

zest_image zest_CreateImage() {
    zest_image_t blank_image = zest_NewImage();
    zest_image image = ZEST__NEW_ALIGNED(zest_image, 16);
    *image = blank_image;
    image->scale = zest_Vec2Set1(1.f);
    image->handle = zest_Vec2Set1(0.5f);
    image->uv.x = 0.f;
    image->uv.y = 0.f;
    image->uv.z = 1.f;
    image->uv.w = 1.f;
    image->layer = 0;
    image->frames = 1;
    return image;
}

zest_image zest_CreateAnimation(zest_uint frames) {
    zest_image image = (zest_image)ZEST__ALLOCATE_ALIGNED(sizeof(zest_image_t) * frames, 16);
    image->frames = frames;
    return image;
}

void zest_AllocateBitmap(zest_bitmap_t* bitmap, int width, int height, int channels, zest_uint fill_color) {
    bitmap->size = width * height * channels;
    if (bitmap->size > 0) {
        bitmap->data = (zest_byte*)ZEST__ALLOCATE(bitmap->size);
        bitmap->width = width;
        bitmap->height = height;
        bitmap->channels = channels;
        bitmap->stride = width * channels;
    }
    memset(bitmap->data, fill_color, bitmap->size);
}

void zest_LoadBitmapImage(zest_bitmap_t* image, const char* file, int color_channels) {
    int width, height, original_no_channels;
    unsigned char* img = stbi_load(file, &width, &height, &original_no_channels, color_channels);
    if (img != NULL) {
        image->width = width;
        image->height = height;
        image->data = img;
        image->channels = color_channels ? color_channels : original_no_channels;
        image->stride = width * original_no_channels;
        image->size = width * height * original_no_channels;
        zest_SetText(&image->name, file);
    }
    else {
        image->data = ZEST_NULL;
    }
}

void zest_LoadBitmapImageMemory(zest_bitmap_t* image, const unsigned char* buffer, int size, int desired_no_channels) {
    int width, height, original_no_channels;
    unsigned char* img = stbi_load_from_memory(buffer, size, &width, &height, &original_no_channels, desired_no_channels);
    if (img != NULL) {
        image->width = width;
        image->height = height;
        image->data = img;
        image->channels = original_no_channels;
        image->stride = width * original_no_channels;
        image->size = width * height * original_no_channels;
    }
    else {
        image->data = ZEST_NULL;
    }
}

void zest_FreeBitmap(zest_bitmap_t* image) {
    if (image->data) {
        ZEST__FREE(image->data);
    }
    zest_FreeText(&image->name);
    image->data = ZEST_NULL;
}

zest_bitmap_t zest_NewBitmap() {
    zest_bitmap_t bitmap = { 0 };
    return bitmap;
}

zest_bitmap_t zest_CreateBitmapFromRawBuffer(const char* name, unsigned char* pixels, int size, int width, int height, int channels) {
    zest_bitmap_t bitmap = zest_NewBitmap();
    bitmap.data = pixels;
    bitmap.width = width;
    bitmap.height = height;
    bitmap.channels = channels;
    bitmap.size = size;
    bitmap.stride = width * channels;
    zest_SetText(&bitmap.name, name);
    return bitmap;
}

zest_bitmap_t* zest_GetBitmap(zest_texture texture, zest_index bitmap_index) {
    zest_vec_test(texture->image_bitmaps, (zest_uint)bitmap_index);
    return &texture->image_bitmaps[bitmap_index];
}

void zest_ConvertBitmapToTextureFormat(zest_bitmap_t* src, VkFormat format) {
    if (format == VK_FORMAT_R8G8B8A8_UNORM) {
        zest_ConvertBitmapToRGBA(src, 255);
    }
    else if (format == VK_FORMAT_B8G8R8A8_UNORM) {
        zest_ConvertBitmapToBGRA(src, 255);
    }
    else if (format == VK_FORMAT_R8_UNORM) {
        zest_ConvertBitmapTo1Channel(src);
    }
    else {
        ZEST_ASSERT(0);    //Unknown texture format
    }
}

void zest_ConvertBitmapTo1Channel(zest_bitmap_t* image) {
    if (image->channels == 1) {
        return;
    }

    zest_bitmap_t converted = { 0 };
    zest_AllocateBitmap(&converted, image->width, image->height, 1, 0);
    zest_ConvertBitmapToAlpha(image);

    zest_size pos = 0;
    zest_size converted_pos = 0;
    if (image->channels == 4) {
        while (pos < image->size) {
            converted.data[converted_pos++] = *(image->data + pos + 3);
            pos += image->channels;
        }
    }
    else if (image->channels == 3) {
        while (pos < image->size) {
            converted.data[converted_pos++] = *(image->data + pos);
            pos += image->channels;
        }
    }
    else if (image->channels == 2) {
        while (pos < image->size) {
            converted.data[converted_pos++] = *(image->data + pos + 1);
            pos += image->channels;
        }
    }
    zest_FreeBitmap(image);
    *image = converted;
}

void zest_ConvertBitmapToAlpha(zest_bitmap_t* image) {

    zest_size pos = 0;

    if (image->channels == 4) {
        while (pos < image->size) {
            zest_byte c = (zest_byte)ZEST__MIN(((float)*(image->data + pos) * 0.3f) + ((float)*(image->data + pos + 1) * .59f) + ((float)*(image->data + pos + 2) * .11f), (float)*(image->data + pos + 3));
            *(image->data + pos) = 255;
            *(image->data + pos + 1) = 255;
            *(image->data + pos + 2) = 255;
            *(image->data + pos + 3) = c;
            pos += image->channels;
        }
    }
    else if (image->channels == 3) {
        while (pos < image->size) {
            zest_byte c = (zest_byte)(((float)*(image->data + pos) * 0.3f) + ((float)*(image->data + pos + 1) * .59f) + ((float)*(image->data + pos + 2) * .11f));
            *(image->data + pos) = c;
            *(image->data + pos + 1) = c;
            *(image->data + pos + 2) = c;
            pos += image->channels;
        }
    }
    else if (image->channels == 2) {
        while (pos < image->size) {
            *(image->data + pos) = 255;
            pos += image->channels;
        }
    }
    else if (image->channels == 1) {
        return;
    }
}

void zest_ConvertBitmap(zest_bitmap_t* src, zest_texture_format format, zest_byte alpha_level) {
    //Todo: simd this
    if (src->channels == 4)
        return;

    ZEST_ASSERT(src->data);

    zest_byte* new_image;
    int channels = 4;
    zest_size new_size = src->width * src->height * channels;
    new_image = (zest_byte*)ZEST__ALLOCATE(new_size);

    zest_size pos = 0;
    zest_size new_pos = 0;

    zest_uint order[3] = { 1, 2, 3 };
    if (format == zest_texture_format_bgra) {
        order[0] = 3; order[2] = 1;
    }

    if (src->channels == 1) {
        while (pos < src->size) {
            *(new_image + new_pos) = *(src->data + pos);
            *(new_image + new_pos + order[0]) = *(src->data + pos);
            *(new_image + new_pos + order[1]) = *(src->data + pos);
            *(new_image + new_pos + order[2]) = alpha_level;
            pos += src->channels;
            new_pos += 4;
        }
    }
    else if (src->channels == 2) {
        while (pos < src->size) {
            *(new_image + new_pos) = *(src->data + pos);
            *(new_image + new_pos + order[0]) = *(src->data + pos);
            *(new_image + new_pos + order[1]) = *(src->data + pos);
            *(new_image + new_pos + order[2]) = *(src->data + pos + 1);
            pos += src->channels;
            new_pos += 4;
        }
    }
    else if (src->channels == 3) {
        while (pos < src->size) {
            *(new_image + new_pos) = *(src->data + pos);
            *(new_image + new_pos + order[0]) = *(src->data + pos + 1);
            *(new_image + new_pos + order[1]) = *(src->data + pos + 2);
            *(new_image + new_pos + order[2]) = alpha_level;
            pos += src->channels;
            new_pos += 4;
        }
    }

    ZEST__FREE(src->data);
    src->channels = channels;
    src->size = new_size;
    src->stride = src->width * channels;
    src->data = new_image;

}

void zest_ConvertBitmapToBGRA(zest_bitmap_t* src, zest_byte alpha_level) {
    zest_ConvertBitmap(src, zest_texture_format_bgra, alpha_level);
}

void zest_ConvertBitmapToRGBA(zest_bitmap_t* src, zest_byte alpha_level) {
    zest_ConvertBitmap(src, zest_texture_format_rgba, alpha_level);
}

void zest_ConvertBGRAToRGBA(zest_bitmap_t* src) {
    if (src->channels != 4)
        return;

    zest_size pos = 0;
    zest_size new_pos = 0;
    zest_byte* data = src->data;

    while (pos < src->size) {
        zest_byte b = *(data + pos);
        *(data + pos) = *(data + pos + 2);
        *(data + pos + 2) = b;
        pos += 4;
    }
}

void zest_CopyWholeBitmap(zest_bitmap_t* src, zest_bitmap_t* dst) {
    ZEST_ASSERT(src->data && src->size);

    zest_SetText(&dst->name, src->name.str);
    dst->channels = src->channels;
    dst->height = src->height;
    dst->width = src->width;
    dst->size = src->size;
    dst->stride = src->stride;
    dst->data = ZEST_NULL;
    dst->data = (zest_byte*)ZEST__ALLOCATE(src->size);
    ZEST_ASSERT(dst->data);    //out of memory;
    memcpy(dst->data, src->data, src->size);

}

void zest_CopyBitmap(zest_bitmap_t* src, int from_x, int from_y, int width, int height, zest_bitmap_t* dst, int to_x, int to_y) {
    ZEST_ASSERT(src->data);
    ZEST_ASSERT(dst->data);
    ZEST_ASSERT(src->channels == dst->channels);

    if (from_x + width > src->width)
        width = src->width - from_x;
    if (from_y + height > src->height)
        height = src->height - from_y;

    if (to_x + width > dst->width)
        to_x = dst->width - width;
    if (to_y + height > dst->height)
        to_y = dst->height - height;

    if (src->data && dst->data && width > 0 && height > 0) {
        int src_row = from_y * src->stride + (from_x * src->channels);
        int dst_row = to_y * dst->stride + (to_x * dst->channels);
        size_t row_size = width * src->channels;
        int rows_copied = 0;
        while (rows_copied < height) {
            memcpy(dst->data + dst_row, src->data + src_row, row_size);
            rows_copied++;
            src_row += src->stride;
            dst_row += dst->stride;
        }
    }
}

zest_color zest_SampleBitmap(zest_bitmap_t* image, int x, int y) {
    ZEST_ASSERT(image->data);

    size_t offset = y * image->stride + (x * image->channels);
    zest_color c = { 0 };
    if (offset < image->size) {
        c.r = *(image->data + offset);

        if (image->channels == 2) {
            c.a = *(image->data + offset + 1);
        }

        if (image->channels == 3) {
            c.g = *(image->data + offset + 1);
            c.b = *(image->data + offset + 2);
        }

        if (image->channels == 4) {
            c.g = *(image->data + offset + 1);
            c.b = *(image->data + offset + 2);
            c.a = *(image->data + offset + 3);
        }
    }

    return c;
}

float zest_FindBitmapRadius(zest_bitmap_t* image) {
    //Todo: optimise with SIMD
    ZEST_ASSERT(image->data);
    float max_radius = 0;
    for (int x = 0; x < image->width; ++x) {
        for (int y = 0; y < image->height; ++y) {
            zest_color c = zest_SampleBitmap(image, x, y);
            if (c.a) {
                max_radius = ceilf(ZEST__MAX(max_radius, zest_Distance((float)image->width / 2.f, (float)image->height / 2.f, (float)x, (float)y)));
            }
        }
    }
    return ceilf(max_radius);
}

zest_index zest__texture_image_index(zest_texture texture) {
    return texture->image_index;
}

void zest_DestroyBitmapArray(zest_bitmap_array_t* bitmap_array) {
    if (bitmap_array->data) {
        ZEST__FREE(bitmap_array->data);
    }
    bitmap_array->data = ZEST_NULL;
    bitmap_array->size_of_array = 0;
}

zest_bitmap_t zest_GetImageFromArray(zest_bitmap_array_t* bitmap_array, zest_index index) {
    zest_bitmap_t image = zest_NewBitmap();
    image.width = bitmap_array->width;
    image.height = bitmap_array->height;
    image.channels = bitmap_array->channels;
    image.stride = bitmap_array->width * bitmap_array->channels;
    image.data = bitmap_array->data + index * bitmap_array->size_of_each_image;
    return image;
}

zest_image zest_AddTextureImageFile(zest_texture texture, const char* filename) {
    zest_image image = zest_CreateImage();
    zest_vec_push(texture->images, image);
    texture->image_index = zest_vec_last_index(texture->images);
    image->index = texture->image_index;
    zest_vec_push(texture->image_bitmaps, zest_NewBitmap());
    zest_bitmap_t* bitmap = zest_GetBitmap(texture, zest__texture_image_index(texture));

    zest_LoadBitmapImage(bitmap, filename, 0);
    ZEST_ASSERT(bitmap->data != ZEST_NULL);            //No image data found, make sure the image is loading ok
    zest_ConvertBitmapToTextureFormat(bitmap, texture->image_format);

    image->width = bitmap->width;
    image->height = bitmap->height;
    image->texture = texture;
    if (texture->flags & zest_texture_flag_get_max_radius) {
        image->max_radius = zest_FindBitmapRadius(bitmap);
    }
    zest_SetText(&image->name, filename);
    zest__update_image_vertices(image);

    return image;
}

zest_image zest_AddTextureImageBitmap(zest_texture texture, zest_bitmap_t* bitmap_to_load) {
    zest_image image = zest_CreateImage();
    zest_vec_push(texture->images, image);
    texture->image_index = zest_vec_last_index(texture->images);
    image->index = texture->image_index;
    zest_bitmap_t image_copy = zest_NewBitmap();
    zest_CopyWholeBitmap(bitmap_to_load, &image_copy);
    zest_vec_push(texture->image_bitmaps, image_copy);
    zest_bitmap_t* bitmap = zest_GetBitmap(texture, zest__texture_image_index(texture));

    ZEST_ASSERT(bitmap->data != ZEST_NULL);
    zest_ConvertBitmapToTextureFormat(bitmap, texture->image_format);

    image->width = bitmap->width;
    image->height = bitmap->height;
    image->texture = texture;
    if (texture->flags & zest_texture_flag_get_max_radius) {
        image->max_radius = zest_FindBitmapRadius(bitmap);
    }
    zest_SetText(&image->name, bitmap->name.str);
    zest__update_image_vertices(image);

    return image;
}

zest_image zest_AddTextureImageMemory(zest_texture texture, const char* name, const unsigned char* buffer, int buffer_size) {
    zest_image image = zest_CreateImage();
    zest_vec_push(texture->images, image);
    texture->image_index = zest_vec_last_index(texture->images);
    image->index = texture->image_index;
    zest_vec_push(texture->image_bitmaps, zest_NewBitmap());
    zest_bitmap_t* bitmap = zest_GetBitmap(texture, zest__texture_image_index(texture));

    zest_LoadBitmapImageMemory(bitmap, buffer, buffer_size, 0);
    ZEST_ASSERT(bitmap->data != ZEST_NULL);
    zest_ConvertBitmapToTextureFormat(bitmap, texture->image_format);

    image->width = bitmap->width;
    image->height = bitmap->height;
    image->texture = texture;
    if (texture->flags & zest_texture_flag_get_max_radius) {
        image->max_radius = zest_FindBitmapRadius(bitmap);
    }
    zest_SetText(&image->name, name);
    zest__update_image_vertices(image);

    return image;
}

zest_image zest_AddTextureAnimationFile(zest_texture texture, const char* filename, int width, int height, zest_uint frames, float* _max_radius, zest_bool row_by_row) {
    zest_bitmap_t spritesheet = { 0 };
    float max_radius;

    zest_LoadBitmapImage(&spritesheet, filename, 0);
    ZEST_ASSERT(spritesheet.data != ZEST_NULL);
    zest_ConvertBitmapToTextureFormat(&spritesheet, texture->image_format);

    zest_uint animation_area = spritesheet.width * spritesheet.height;
    zest_uint frame_area = width * height;
    zest_index first_index = zest_vec_size(texture->images);

    ZEST_ASSERT(frames <= animation_area / frame_area);    // ERROR: The animation being loaded is the wrong size for the number of frames that you want to load for the specified width and height.
    ZEST_ASSERT(spritesheet.width >= width);            // ERROR: The animation being loaded is not wide enough for the width of each frame specified.
    ZEST_ASSERT(spritesheet.height >= height);            // ERROR: The animation being loaded is not heigh enough for the height of each frame specified.

    max_radius = zest__copy_animation_frames(texture, &spritesheet, width, height, frames, row_by_row);
    zest_FreeBitmap(&spritesheet);

    if (_max_radius) {
        *_max_radius = max_radius;
    }

    zest_SetText(&texture->images[first_index]->name, filename);
    return texture->images[first_index];
}

zest_image zest_AddTextureAnimationBitmap(zest_texture texture, zest_bitmap_t* spritesheet, int width, int height, zest_uint frames, float* _max_radius, zest_bool row_by_row) {
    float max_radius;

    ZEST_ASSERT(spritesheet->data != ZEST_NULL);
    zest_ConvertBitmapToTextureFormat(spritesheet, texture->image_format);

    zest_uint animation_area = spritesheet->width * spritesheet->height;
    zest_uint frame_area = width * height;

    zest_index first_index = zest_vec_size(texture->images);

    ZEST_ASSERT(frames <= animation_area / frame_area);    // ERROR: The animation being loaded is the wrong size for the number of frames that you want to load for the specified width and height.
    ZEST_ASSERT(spritesheet->width >= width);            // ERROR: The animation being loaded is not wide enough for the width of each frame specified.
    ZEST_ASSERT(spritesheet->height >= height);            // ERROR: The animation being loaded is not heigh enough for the height of each frame specified.

    max_radius = zest__copy_animation_frames(texture, spritesheet, width, height, frames, row_by_row);

    if (_max_radius) {
        *_max_radius = max_radius;
    }

    zest_SetText(&texture->images[first_index]->name, spritesheet->name.str);
    return texture->images[first_index];
}

zest_image zest_AddTextureAnimationMemory(zest_texture texture, const char* name, unsigned char* buffer, int buffer_size, int width, int height, zest_uint frames, float* _max_radius, zest_bool row_by_row) {
    float max_radius;
    zest_bitmap_t spritesheet = { 0 };

    zest_LoadBitmapImageMemory(&spritesheet, buffer, buffer_size, texture->color_channels);
    ZEST_ASSERT(spritesheet.data != ZEST_NULL);
    zest_ConvertBitmapToTextureFormat(&spritesheet, texture->image_format);

    zest_uint animation_area = spritesheet.width * spritesheet.height;
    zest_uint frame_area = width * height;

    zest_index first_index = zest_vec_size(texture->images);

    ZEST_ASSERT(frames <= animation_area / frame_area);    //ERROR: The animation being loaded is the wrong size for the number of frames that you want to load for the specified width and height.
    ZEST_ASSERT(spritesheet.width >= width);                // ERROR: The animation being loaded is not wide enough for the width of each frame specified.
    ZEST_ASSERT(spritesheet.height >= height);            // ERROR: The animation being loaded is not heigh enough for the height of each frame specified.

    max_radius = zest__copy_animation_frames(texture, &spritesheet, width, height, frames, row_by_row);
    zest_FreeBitmap(&spritesheet);

    if (_max_radius) {
        *_max_radius = max_radius;
    }

    zest_SetText(&texture->images[first_index]->name, name);
    return texture->images[first_index];
}

float zest__copy_animation_frames(zest_texture texture, zest_bitmap_t* spritesheet, int width, int height, zest_uint frames, zest_bool row_by_row) {
    zest_uint rows = spritesheet->height / height;
    zest_uint cols = spritesheet->width / width;

    zest_uint frame_count = 0;
    float max_radius = 0;
    zest_image_t blank_image = zest_NewImage();
    zest_image frame = zest_CreateAnimation(frames);

    for (zest_uint r = 0; r != (row_by_row ? rows : cols); ++r) {
        for (zest_uint c = 0; c != (row_by_row ? cols : rows); ++c) {
            if (frame_count >= frames) {
                break;
            }
            *frame = blank_image;
            zest_vec_push(texture->images, frame);
            texture->image_index = zest_vec_last_index(texture->images);
            frame->index = texture->image_index;
            zest_vec_push(texture->image_bitmaps, zest_NewBitmap());
            zest_bitmap_t* image_bitmap = zest_GetBitmap(texture, texture->image_index);
            zest_AllocateBitmap(image_bitmap, width, height, spritesheet->channels, 0);
            zest_CopyBitmap(spritesheet, c * width, r * height, width, height, image_bitmap, 0, 0);
            frame->width = image_bitmap->width;
            frame->height = image_bitmap->height;
            zest__update_image_vertices(frame);
            frame->texture = texture;
            if (texture->flags & zest_texture_flag_get_max_radius) {
                frame->max_radius = zest_FindBitmapRadius(image_bitmap);
                max_radius = ZEST__MAX(max_radius, frame->max_radius);
            }
            frame->frames = frames;
            frame = frame + 1;
            frame_count++;
        }
    }

    return max_radius;

}

// --Internal Texture functions
void zest__free_all_texture_images(zest_texture texture) {
    int count = 0;
    for (zest_foreach_i(texture->image_bitmaps)) {
        zest_FreeBitmap(&texture->image_bitmaps[i]);
        count++;
    }
    zest__delete_texture_layers(texture);
    zest_uint i = 0;
    while (i < zest_vec_size(texture->images)) {
        zest_image image = texture->images[i];
        i += image->frames;
        zest_FreeText(&image->name);
        ZEST__FREE(image);
    }
    zest_vec_clear(texture->images);
    zest_vec_clear(texture->image_bitmaps);
    zest_FreeBitmapArray(&texture->bitmap_array);
    texture->image_index = 0;
    zest_FreeBitmap(&texture->texture_bitmap);
}

void zest__delete_texture(zest_texture texture) {
    zest__cleanup_texture(texture);
    zest__free_all_texture_images(texture);
    zest_map_free(texture->descriptor_sets);
    if (texture->stream_staging_buffer) {
        zest_FreeBuffer(texture->stream_staging_buffer);
    }
    zest_vec_free(texture->buffer_copy_regions);
    ZEST__UNFLAG(texture->flags, zest_texture_flag_textures_ready);
    zest_map_remove(ZestRenderer->textures, texture->name.str);
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->textures, texture->name.str));
    zest_FreeText(&texture->name);
}

void zest__delete_font(zest_font_t* font) {
    zest_map_remove(ZestRenderer->fonts, font->name.str);
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->fonts, font->name.str));
    zest_vec_free(font->characters);
    zest_FreeText(&font->name);
}

void zest__cleanup_texture(zest_texture texture) {
    vkDestroySampler(ZestDevice->logical_device, texture->sampler, &ZestDevice->allocation_callbacks);
    vkDestroyImageView(ZestDevice->logical_device, texture->frame_buffer.view, &ZestDevice->allocation_callbacks);
    vkDestroyImage(ZestDevice->logical_device, texture->frame_buffer.image, &ZestDevice->allocation_callbacks);
    zest_FreeBuffer(texture->frame_buffer.buffer);
    texture->frame_buffer.buffer = 0;
    texture->sampler = VK_NULL_HANDLE;
    texture->frame_buffer.view = VK_NULL_HANDLE;
    texture->frame_buffer.image = VK_NULL_HANDLE;
    texture->flags &= ~zest_texture_flag_textures_ready;
}

void zest__reindex_texture_images(zest_texture texture) {
    zest_index index = 0;
    for (zest_foreach_i(texture->images)) {
        texture->images[i]->index = index++;
    }
    texture->image_index = index;
}
// --End Internal Texture functions

void zest_ProcessTextureImages(zest_texture texture) {
    if (texture->flags & zest_texture_flag_textures_ready) {
        zest__cleanup_texture(texture);
    }

    zest_uint size = zest_vec_size(texture->images);
    if (zest_vec_empty(texture->images) && texture->storage_type != zest_texture_storage_type_storage && texture->storage_type != zest_texture_storage_type_single)
        return;

    if (texture->storage_type == zest_texture_storage_type_packed) {
        zest__pack_images(texture, texture->texture_layer_size);
    }
    else if (texture->storage_type == zest_texture_storage_type_bank) {
        zest__make_image_bank(texture, texture->images[0]->width);
    }
    else if (texture->storage_type == zest_texture_storage_type_sprite_sheet) {
        zest__make_sprite_sheet(texture);
    }

    zest_uint mip_levels = 1;

    if (texture->storage_type < zest_texture_storage_type_sprite_sheet) {

        if (texture->flags & zest_texture_flag_use_filtering)
            mip_levels = (zest_uint)floor(log2(ZEST__MAX(texture->bitmap_array.width, texture->bitmap_array.height))) + 1;
        else
            mip_levels = 1;

        zest__create_texture_image_array(texture, mip_levels);
        zest__create_texture_image_view(texture, texture->image_view_type, mip_levels, texture->layer_count);
        zest__create_texture_sampler(texture, texture->sampler_info, mip_levels);

        texture->descriptor.imageView = texture->frame_buffer.view;
        texture->descriptor.sampler = texture->sampler;
        texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        texture->flags |= zest_texture_flag_textures_ready;
    }
    else if (texture->storage_type == zest_texture_storage_type_sprite_sheet) {

        if (texture->flags & zest_texture_flag_use_filtering)
            mip_levels = (zest_uint)floor(log2(ZEST__MAX(texture->images[0]->width, texture->images[0]->height))) + 1;
        else
            mip_levels = 1;

        texture->layer_count = 1;

        VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (texture->image_layout == VK_IMAGE_LAYOUT_GENERAL) {
            flags |= VK_IMAGE_USAGE_STORAGE_BIT;
            zest__create_texture_image(texture, mip_levels, flags, VK_IMAGE_LAYOUT_GENERAL, ZEST_TRUE);
            texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        else {
            zest__create_texture_image(texture, mip_levels, flags, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ZEST_TRUE);
            texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        zest__create_texture_image_view(texture, texture->image_view_type, mip_levels, texture->layer_count);
        zest__create_texture_sampler(texture, texture->sampler_info, mip_levels);

        texture->descriptor.imageView = texture->frame_buffer.view;
        texture->descriptor.sampler = texture->sampler;
        texture->flags |= zest_texture_flag_textures_ready;

    }
    else if (texture->storage_type == zest_texture_storage_type_single) {

        if (texture->flags & zest_texture_flag_use_filtering)
            mip_levels = (zest_uint)floor(log2(ZEST__MAX(texture->images[0]->width, texture->images[0]->height))) + 1;
        else
            mip_levels = 1;

        texture->layer_count = 1;

        VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (texture->image_layout == VK_IMAGE_LAYOUT_GENERAL) {
            flags |= VK_IMAGE_USAGE_STORAGE_BIT;
            zest__create_texture_image(texture, mip_levels, flags, VK_IMAGE_LAYOUT_GENERAL, ZEST_TRUE);
            texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        else {
            zest__create_texture_image(texture, mip_levels, flags, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ZEST_TRUE);
            texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        zest__create_texture_image_view(texture, texture->image_view_type, mip_levels, texture->layer_count);
        zest__create_texture_sampler(texture, texture->sampler_info, mip_levels);

        texture->descriptor.imageView = texture->frame_buffer.view;
        texture->descriptor.sampler = texture->sampler;
        texture->flags |= zest_texture_flag_textures_ready;

    }
    else if (texture->storage_type == zest_texture_storage_type_stream) {

        if (texture->flags & zest_texture_flag_use_filtering)
            mip_levels = (zest_uint)floor(log2(ZEST__MAX(texture->images[0]->width, texture->images[0]->height))) + 1;
        else
            mip_levels = 1;

        texture->layer_count = 1;

        VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (texture->image_layout == VK_IMAGE_LAYOUT_GENERAL) {
            flags |= VK_IMAGE_USAGE_STORAGE_BIT;
            zest__create_texture_stream(texture, mip_levels, flags, VK_IMAGE_LAYOUT_GENERAL, ZEST_TRUE);
            texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        else {
            zest__create_texture_stream(texture, mip_levels, flags, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ZEST_TRUE);
            texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        zest__create_texture_image_view(texture, texture->image_view_type, mip_levels, texture->layer_count);
        zest__create_texture_sampler(texture, texture->sampler_info, mip_levels);

        texture->descriptor.imageView = texture->frame_buffer.view;
        texture->descriptor.sampler = texture->sampler;
        texture->flags |= zest_texture_flag_textures_ready;

    }
    else if (texture->storage_type == zest_texture_storage_type_storage) {
        mip_levels = 1;
        texture->layer_count = 1;

        zest__create_texture_image(texture, mip_levels, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_LAYOUT_GENERAL, ZEST_FALSE);
        zest__create_texture_image_view(texture, texture->image_view_type, mip_levels, texture->layer_count);
        zest__create_texture_sampler(texture, texture->sampler_info, mip_levels);

        texture->descriptor.imageView = texture->frame_buffer.view;
        texture->descriptor.sampler = texture->sampler;
        texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        texture->flags |= zest_texture_flag_textures_ready;
    }

    if (!(texture->flags & zest_texture_flag_descriptor_sets_created)) {
        zest_CreateSimpleTextureDescriptorSet(texture, "Default", "Standard 2d Uniform Buffer");
        zest_SwitchTextureDescriptorSet(texture, "Default");
    }

    zest__delete_texture_layers(texture);

}

void zest__delete_texture_layers(zest_texture texture) {
    for (zest_foreach_i(texture->layers)) {
        zest_FreeBitmap(&texture->layers[i]);
    }
    zest_vec_free(texture->layers);
}

zest_descriptor_set zest_CreateSimpleTextureDescriptorSet(zest_texture texture, const char* name, const char* uniform_buffer_name) {
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->uniform_buffers, uniform_buffer_name));    //No uniform buffer found with that name
    zest_descriptor_set_t blank_set = { 0 };
    zest_descriptor_set set = ZEST__NEW(zest_descriptor_set);
    *set = blank_set;
    set->buffer = zest_GetUniformBuffer(uniform_buffer_name);
    for (ZEST_EACH_FIF_i) {
        zest_AllocateDescriptorSet(ZestRenderer->descriptor_pool, zest_GetDescriptorSetLayout("Standard 1 uniform 1 sampler")->descriptor_layout, &set->descriptor_set[i]);
        zest_vec_push(set->descriptor_writes[i], zest_CreateBufferDescriptorWriteWithType(set->descriptor_set[i], &set->buffer->descriptor_info[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER));
        zest_vec_push(set->descriptor_writes[i], zest_CreateImageDescriptorWriteWithType(set->descriptor_set[i], &texture->descriptor, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER));
    }
    zest_AddTextureDescriptorSet(texture, name, set);
    zest_UpdateTextureSingleDescriptorSet(texture, name);
    texture->flags |= zest_texture_flag_descriptor_sets_created;
    return set;
}

zest_descriptor_set zest_GetTextureDescriptorSet(zest_texture texture, const char* name) {
    ZEST_ASSERT(zest_map_valid_name(texture->descriptor_sets, name));
    return *zest_map_at(texture->descriptor_sets, name);
}

void zest_SwitchTextureDescriptorSet(zest_texture texture, const char* name) {
    ZEST_ASSERT(zest_map_valid_name(texture->descriptor_sets, name));
    for (ZEST_EACH_FIF_i) {
        texture->current_descriptor_set[i] = *zest_map_at(texture->descriptor_sets, name)->descriptor_set[i];
    }
}

VkDescriptorSet zest_CurrentTextureDescriptorSet(zest_texture texture) {
    return texture->current_descriptor_set[ZEST_FIF];
}

void zest_UpdateTextureSingleDescriptorSet(zest_texture texture, const char* name) {
    ZEST_ASSERT(zest_map_valid_name(texture->descriptor_sets, name));
    zest_descriptor_set set = *zest_map_at(texture->descriptor_sets, name);
    for (ZEST_EACH_FIF_i) {
        zest_UpdateDescriptorSet(set->descriptor_writes[i]);
    }
}

void zest__update_all_texture_descriptor_writes(zest_texture texture) {
    for (ZEST_EACH_FIF_i) {
        for (zest_map_foreach_j(texture->descriptor_sets)) {
            zest_index set_index = zest_map_index(texture->descriptor_sets, j);
            zest_descriptor_set set = *zest_map_at_index(texture->descriptor_sets, set_index);
            for (zest_foreach_k(set->descriptor_writes[i])) {
                VkWriteDescriptorSet* write = &set->descriptor_writes[i][k];
                write->dstSet = set->descriptor_set[i];
                if (write->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
                    write->pBufferInfo = &set->buffer->descriptor_info[i];
                }
                else if (write->descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                    write->pImageInfo = &texture->descriptor;
                }
            }
        }
    }
}

void zest__update_all_texture_descriptor_sets(zest_texture texture) {
    for (ZEST_EACH_FIF_i) {
        for (zest_map_foreach_j(texture->descriptor_sets)) {
            zest_index set_index = zest_map_index(texture->descriptor_sets, j);
            zest_descriptor_set set = *zest_map_at_index(texture->descriptor_sets, set_index);
            zest_UpdateDescriptorSet(set->descriptor_writes[i]);
        }
    }
}

void zest_RefreshTextureDescriptors(zest_texture texture) {
    zest__update_all_texture_descriptor_writes(texture);
    zest__update_all_texture_descriptor_sets(texture);
}

void zest_ScheduleTextureReprocess(zest_texture texture) {
    zest_vec_push(ZestRenderer->texture_reprocess_queue, texture);
#if defined(ZEST_ATOMICS)
    atomic_store(&ZestRenderer->lock_texture_reprocess_queue, 1);
#endif
}

void zest_WaitUntilTexturesReprocessed() {
#if defined(ZEST_ATOMICS)
    while (atomic_load(&ZestRenderer->lock_texture_reprocess_queue)) {
        // Spin until lock is acquired
    }
#else
    while (ZestRenderer->texture_reprocess_queue && zest_vec_size(ZestRenderer->texture_reprocess_queue)) {
        //Spin. This should be run from a separate thread so the scheduler can actually do it's thing
    }
#endif
}

void zest_AddTextureDescriptorSet(zest_texture texture, const char* name, zest_descriptor_set descriptor_set) {
    zest_map_insert(texture->descriptor_sets, name, descriptor_set);
}

zest_bitmap_t* zest_GetTextureSingleBitmap(zest_texture texture) {
    if (texture->texture_bitmap.data) {
        return &texture->texture_bitmap;
    }

    if (zest_vec_size(texture->images) > 0) {
        return &texture->image_bitmaps[0];
    }

    return &texture->texture_bitmap;
}

void zest__create_texture_image(zest_texture texture, zest_uint mip_levels, VkImageUsageFlags usage_flags, VkImageLayout image_layout, zest_bool copy_bitmap) {
    int channels = texture->color_channels;
    VkDeviceSize image_size = zest_GetTextureSingleBitmap(texture)->width * zest_GetTextureSingleBitmap(texture)->height * channels;

    zest_buffer_t* staging_buffer = 0;
    if (copy_bitmap) {
        zest_buffer_info_t buffer_info = zest_CreateStagingBufferInfo();
        staging_buffer = zest_CreateBuffer(image_size, &buffer_info, ZEST_NULL);
        memcpy(staging_buffer->data, zest_GetTextureSingleBitmap(texture)->data, (zest_size)image_size);
    }

    texture->frame_buffer.buffer = zest__create_image(zest_GetTextureSingleBitmap(texture)->width, zest_GetTextureSingleBitmap(texture)->height, mip_levels, VK_SAMPLE_COUNT_1_BIT, texture->image_format, VK_IMAGE_TILING_OPTIMAL, usage_flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texture->frame_buffer.image);

    texture->frame_buffer.format = texture->image_format;
    if (copy_bitmap) {
        zest__transition_image_layout(texture->frame_buffer.image, texture->image_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip_levels, 1);
        zest__copy_buffer_to_image(*zest_GetBufferDeviceBuffer(staging_buffer), staging_buffer->memory_offset, texture->frame_buffer.image, (zest_uint)zest_GetTextureSingleBitmap(texture)->width, (zest_uint)zest_GetTextureSingleBitmap(texture)->height, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        zloc_FreeRemote(staging_buffer->buffer_allocator->allocator, staging_buffer);
        zest__generate_mipmaps(texture->frame_buffer.image, texture->image_format, zest_GetTextureSingleBitmap(texture)->width, zest_GetTextureSingleBitmap(texture)->height, mip_levels, 1, image_layout);
    }
    else {
        zest__transition_image_layout(texture->frame_buffer.image, texture->image_format, VK_IMAGE_LAYOUT_UNDEFINED, image_layout, mip_levels, 1);
    }
}

void zest__create_texture_image_array(zest_texture texture, zest_uint mip_levels) {
    VkDeviceSize image_size = texture->bitmap_array.total_mem_size;

    zest_buffer_info_t buffer_info = zest_CreateStagingBufferInfo();
    zest_buffer_t* staging_buffer = zest_CreateBuffer(image_size, &buffer_info, ZEST_NULL);

    //memcpy(staging_buffer.mapped, texture.TextureArray().data(), texture.TextureArray().size());
    memcpy(staging_buffer->data, texture->bitmap_array.data, texture->bitmap_array.total_mem_size);

    //texture.FrameBuffer().allocation_id = CreateImageArray(texture.TextureArray().extent().x, texture.TextureArray().extent().y, mip_levels, texture.LayerCount(), VK_SAMPLE_COUNT_1_BIT, image_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture.FrameBuffer().image);
    texture->frame_buffer.buffer = zest__create_image_array(texture->bitmap_array.width, texture->bitmap_array.height, mip_levels, texture->layer_count, VK_SAMPLE_COUNT_1_BIT, texture->image_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texture->frame_buffer.image);
    texture->frame_buffer.format = texture->image_format;

    zest__transition_image_layout(texture->frame_buffer.image, texture->image_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip_levels, texture->layer_count);
    zest__copy_buffer_regions_to_image(texture->buffer_copy_regions, *zest_GetBufferDeviceBuffer(staging_buffer), staging_buffer->memory_offset, texture->frame_buffer.image);

    zloc_FreeRemote(staging_buffer->buffer_allocator->allocator, staging_buffer);

    zest__generate_mipmaps(texture->frame_buffer.image, texture->image_format, texture->bitmap_array.width, texture->bitmap_array.height, mip_levels, texture->layer_count, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void zest__create_texture_stream(zest_texture texture, zest_uint mip_levels, VkImageUsageFlags usage_flags, VkImageLayout image_layout, zest_bool copy_bitmap) {
    int channels = texture->color_channels;

    VkDeviceSize image_size = zest_GetTextureSingleBitmap(texture)->width * zest_GetTextureSingleBitmap(texture)->height * channels;

    if (copy_bitmap) {
        zest_buffer_info_t buffer_info = zest_CreateStagingBufferInfo();
        texture->stream_staging_buffer = zest_CreateBuffer(image_size, &buffer_info, ZEST_NULL);
        memcpy(texture->stream_staging_buffer->data, zest_GetTextureSingleBitmap(texture)->data, (zest_size)image_size);
    }

    texture->frame_buffer.buffer = zest__create_image(zest_GetTextureSingleBitmap(texture)->width, zest_GetTextureSingleBitmap(texture)->height, mip_levels, VK_SAMPLE_COUNT_1_BIT, texture->image_format, VK_IMAGE_TILING_OPTIMAL, usage_flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texture->frame_buffer.image);

    texture->frame_buffer.format = texture->image_format;
    if (copy_bitmap) {
        zest__transition_image_layout(texture->frame_buffer.image, texture->image_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip_levels, 1);
        zest__copy_buffer_to_image(*zest_GetBufferDeviceBuffer(texture->stream_staging_buffer), texture->stream_staging_buffer->memory_offset, texture->frame_buffer.image, (zest_uint)zest_GetTextureSingleBitmap(texture)->width, (zest_uint)zest_GetTextureSingleBitmap(texture)->height, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        zest__generate_mipmaps(texture->frame_buffer.image, texture->image_format, zest_GetTextureSingleBitmap(texture)->width, zest_GetTextureSingleBitmap(texture)->width, mip_levels, 1, image_layout);
    }
    else {
        zest__transition_image_layout(texture->frame_buffer.image, texture->image_format, VK_IMAGE_LAYOUT_UNDEFINED, image_layout, mip_levels, 1);
    }
}

void zest__create_texture_image_view(zest_texture texture, VkImageViewType view_type, zest_uint mip_levels, zest_uint layer_count) {
    texture->frame_buffer.view = zest__create_image_view(texture->frame_buffer.image, texture->image_format, VK_IMAGE_ASPECT_COLOR_BIT, mip_levels, view_type, layer_count);
}

void zest__create_texture_sampler(zest_texture texture, VkSamplerCreateInfo sampler_info, zest_uint mip_levels) {

    sampler_info.maxLod = (float)mip_levels;

    ZEST_VK_CHECK_RESULT(vkCreateSampler(ZestDevice->logical_device, &sampler_info, &ZestDevice->allocation_callbacks, &texture->sampler));
}

zest_frame_buffer_t zest_CreateFrameBuffer(VkRenderPass render_pass, uint32_t width, uint32_t height, VkFormat render_format, zest_bool use_depth_buffer, zest_bool is_src) {
    zest_frame_buffer_t frame_buffer = { 0 };
    frame_buffer.format = render_format;
    frame_buffer.width = width;
    frame_buffer.height = height;
    VkFormat color_format = frame_buffer.format;

    VkImageUsageFlags usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (is_src) {
        usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    frame_buffer.color_buffer.buffer = zest__create_image(frame_buffer.width, frame_buffer.height, 1, VK_SAMPLE_COUNT_1_BIT, color_format, VK_IMAGE_TILING_OPTIMAL, usage_flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &frame_buffer.color_buffer.image);
    frame_buffer.color_buffer.view = zest__create_image_view(frame_buffer.color_buffer.image, color_format, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 1);

    zest__transition_image_layout(frame_buffer.color_buffer.image, color_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1);

    VkFormat depth_format = zest__find_depth_format();

    if (use_depth_buffer) {
        frame_buffer.depth_buffer.buffer = zest__create_image(frame_buffer.width, frame_buffer.height, 1, VK_SAMPLE_COUNT_1_BIT, depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &frame_buffer.depth_buffer.image);
        frame_buffer.depth_buffer.view = zest__create_image_view(frame_buffer.depth_buffer.image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, 1, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 1);
        zest__transition_image_layout(frame_buffer.depth_buffer.image, depth_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, 1);
    }

    VkImageView* fb_attachments = 0;
    zest_vec_push(fb_attachments, frame_buffer.color_buffer.view);
    if (use_depth_buffer) {
        zest_vec_push(fb_attachments, frame_buffer.depth_buffer.view);
    }

    VkFramebufferCreateInfo frame_buffer_create_info = { 0 };
    frame_buffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frame_buffer_create_info.renderPass = render_pass;
    frame_buffer_create_info.attachmentCount = (zest_uint)(zest_vec_size(fb_attachments));
    frame_buffer_create_info.pAttachments = fb_attachments;
    frame_buffer_create_info.width = frame_buffer.width;
    frame_buffer_create_info.height = frame_buffer.height;
    frame_buffer_create_info.layers = 1;

    ZEST_VK_CHECK_RESULT(vkCreateFramebuffer(ZestDevice->logical_device, &frame_buffer_create_info, &ZestDevice->allocation_callbacks, &frame_buffer.device_frame_buffer));

    zest_vec_free(fb_attachments);
    return frame_buffer;
}

zest_byte zest__calculate_texture_layers(stbrp_rect* rects, zest_uint size, const zest_uint node_count) {
    stbrp_node* nodes = 0;
    zest_vec_resize(nodes, node_count);
    zest_byte layers = 0;
    stbrp_rect* current_rects = 0;
    stbrp_rect* rects_copy = 0;
    zest_vec_resize(rects_copy, zest_vec_size(rects));
    memcpy(rects_copy, rects, zest_vec_size_in_bytes(rects));
    while (!zest_vec_empty(rects_copy) && layers <= 255) {

        stbrp_context context;
        stbrp_init_target(&context, size, size, nodes, node_count);
        stbrp_pack_rects(&context, rects_copy, (int)zest_vec_size(rects_copy));

        for (zest_foreach_i(rects_copy)) {
            zest_vec_push(current_rects, rects_copy[i]);
        }

        zest_vec_clear(rects_copy);

        for (zest_foreach_i(current_rects)) {
            if (!rects_copy[i].was_packed) {
                zest_vec_push(rects_copy, rects_copy[i]);
            }
        }

        zest_vec_clear(current_rects);
        layers++;
    }

    zest_vec_free(nodes);
    zest_vec_free(current_rects);
    zest_vec_free(rects_copy);
    return layers;
}

void zest_CreateBitmapArray(zest_bitmap_array_t* images, int width, int height, int channels, zest_uint size_of_array) {
    ZEST_ASSERT(size_of_array);            //must create with atleast one image in the array
    if (images->data) {
        ZEST__FREE(images->data);
        images->data = ZEST_NULL;
    }
    images->width = width;
    images->height = height;
    images->channels = channels;
    images->stride = width * channels;
    images->size_of_array = size_of_array;
    images->size_of_each_image = width * height * channels;
    images->total_mem_size = images->size_of_array * images->size_of_each_image;
    images->data = (zest_byte*)ZEST__ALLOCATE(images->total_mem_size);
}

void zest_FreeBitmapArray(zest_bitmap_array_t* images) {
    if (images->data) {
        ZEST__FREE(images->data);
    }
    images->data = ZEST_NULL;
    images->size_of_array = 0;
}

void zest__make_image_bank(zest_texture texture, zest_uint size) {

    //array_texture = gli::texture2d_array(gli::FORMAT_RGBA8_UNORM_PACK8, gli::texture2d_array::extent_type(size, size), images.size());
    zest_CreateBitmapArray(&texture->bitmap_array, size, size, 4, zest_vec_size(texture->images));

    zest_uint id = 0;

    for (zest_foreach_i(texture->images)) {
        zest_image image = texture->images[i];

        image->uv.x = 0;
        image->uv.y = 0;
        image->uv.z = 1.f;
        image->uv.w = 1.f;
        image->uv_xy = zest_Pack16bit(image->uv.x, image->uv.y);
        image->uv_zw = zest_Pack16bit(image->uv.z, image->uv.w);
        image->left = 0;
        image->top = 0;
        image->layer = id;
        image->texture = texture;

        zest_bitmap_t tmp_image = zest_NewBitmap();
        zest_AllocateBitmap(&tmp_image, image->width, image->height, texture->color_channels, 0);

        if (image->width != size || image->height != size) {
            zest_bitmap_t* image_bitmap = &texture->image_bitmaps[image->index];
            zest_CopyBitmap(image_bitmap, 0, 0, image->width, image->height, &tmp_image, 0, 0);
            stbir_resize_uint8(tmp_image.data, tmp_image.width, tmp_image.height, tmp_image.stride, tmp_image.data, size, size, size * 4, 4);
        }
        else {
            zest_bitmap_t* image_bitmap = &texture->image_bitmaps[image->index];
            zest_CopyBitmap(image_bitmap, 0, 0, size, size, &tmp_image, 0, 0);
        }

        tmp_image.width = size;
        tmp_image.height = size;

        //auto format = gli::FORMAT_RGBA8_UNORM_PACK8;

        //array_texture[id] = gli::texture2d(format, gli::texture2d::extent_type(size, size), 1);

        //memcpy(array_texture[id][0].data(), tmp_image.data, array_texture[id][0].size());
        memcpy(zest_BitmapArrayLookUp(&texture->bitmap_array, id), tmp_image.data, texture->bitmap_array.size_of_each_image);
        zest_FreeBitmap(&tmp_image);

        id++;
    }

    texture->layer_count = id;

    zest_size offset = 0;

    zest_vec_clear(texture->buffer_copy_regions);

    for (zest_uint layer = 0; layer < texture->layer_count; layer++) {
        VkBufferImageCopy buffer_copy_region = { 0 };
        buffer_copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        buffer_copy_region.imageSubresource.mipLevel = 0;
        buffer_copy_region.imageSubresource.baseArrayLayer = layer;
        buffer_copy_region.imageSubresource.layerCount = 1;
        buffer_copy_region.imageExtent.width = size;
        buffer_copy_region.imageExtent.height = size;
        buffer_copy_region.imageExtent.depth = 1;
        buffer_copy_region.bufferOffset = offset;

        zest_vec_push(texture->buffer_copy_regions, buffer_copy_region);

        // Increase offset into staging buffer for next level / face
        //offset += array_texture[layer].size();
        offset += texture->bitmap_array.size_of_each_image;
    }

}

void zest__make_sprite_sheet(zest_texture texture) {
    stbrp_rect* rects = 0;
    stbrp_rect* rects_to_process = 0;

    zest_uint id = 0;
    zest_uint max_width = 0, max_height = 0;
    for (zest_foreach_i(texture->images)) {
        zest_image image = texture->images[i];
        stbrp_rect rect;
        rect.w = image->width + texture->packed_border_size * 2;
        rect.h = image->height + texture->packed_border_size * 2;
        rect.x = 0;
        rect.y = 0;
        rect.was_packed = 0;
        rect.id = image->index;

        zest_vec_push(rects, rect);
        zest_vec_push(rects_to_process, rect);

        max_width = (zest_uint)ZEST__MAX((float)max_width, (float)rect.w);
        max_height = (zest_uint)ZEST__MAX((float)max_height, (float)rect.h);

        id++;
    }

    zest_uint size = (zest_uint)ZEST__MAX((float)max_width, (float)max_height);
    size = (zest_uint)zest_GetNextPower(size);

    const zest_uint node_count = size * 2;
    stbrp_node* nodes = 0;
    zest_vec_resize(nodes, node_count);

    while (zest__calculate_texture_layers(rects, size, node_count) != 1 || size > ZestDevice->max_image_size) {
        size *= 2;
    }

    ZEST_ASSERT(size <= ZestDevice->max_image_size);    //Texture layer size is greater then GPU capability

    texture->layer_count = 1;

    zest_byte current_layer = 0;

    for (zest_foreach_i(texture->layers)) {
        zest_FreeBitmap(&texture->layers[i]);
    }
    zest_vec_clear(texture->layers);

    stbrp_context context;
    stbrp_init_target(&context, size, size, nodes, node_count);
    stbrp_pack_rects(&context, rects, (int)zest_vec_size(rects));

    stbrp_rect* current_rects = 0;
    for (zest_foreach_i(rects)) {
        zest_vec_push(current_rects, rects[i]);
    }

    zest_vec_clear(rects);

    zest_FreeBitmap(&texture->texture_bitmap);
    zest_AllocateBitmap(&texture->texture_bitmap, size, size, texture->color_channels, 0);
    texture->texture.width = size;
    texture->texture.height = size;

    for (zest_foreach_i(current_rects)) {
        stbrp_rect* rect = &current_rects[i];

        if (rect->was_packed) {
            zest_image image = texture->images[rect->id];

            float rect_x = (float)rect->x + texture->packed_border_size;
            float rect_y = (float)rect->y + texture->packed_border_size;

            image->uv.x = (rect_x + 0.5f) / (float)size;
            image->uv.y = (rect_y + 0.5f) / (float)size;
            image->uv.z = ((float)image->width + (rect_x - 0.5f)) / (float)size;
            image->uv.w = ((float)image->height + (rect_y - 0.5f)) / (float)size;
            image->uv_xy = zest_Pack16bit(image->uv.x, image->uv.y);
            image->uv_zw = zest_Pack16bit(image->uv.z, image->uv.w);
            image->left = (zest_uint)rect_x;
            image->top = (zest_uint)rect_y;
            image->layer = current_layer;
            image->texture = texture;

            zest_CopyBitmap(&texture->image_bitmaps[rect->id], 0, 0, image->width, image->height, &texture->texture_bitmap, image->left, image->top);
        }
        else {
            zest_vec_push(rects, *rect);
        }
    }

    zest_vec_free(rects);
    zest_vec_free(rects_to_process);
    zest_vec_free(nodes);
    zest_vec_free(current_rects);
}

void zest__pack_images(zest_texture texture, zest_uint size) {
    stbrp_rect* rects = 0;
    stbrp_rect* rects_to_process = 0;

    zest_uint max_width = 0;
    int max_size = texture->texture_layer_size;
    for (zest_foreach_i(texture->images)) {
        zest_image image = texture->images[i];
        stbrp_rect rect;
        rect.w = image->width + texture->packed_border_size * 2;
        rect.h = image->height + texture->packed_border_size * 2;
        rect.x = 0;
        rect.y = 0;
        rect.was_packed = 0;
        rect.id = image->index;

        max_size = ZEST__MAX(max_size, rect.w);
        max_size = ZEST__MAX(max_size, rect.h);

        zest_vec_push(rects, rect);
        zest_vec_push(rects_to_process, rect);
    }

    //Todo: we need to return an error code if there's an images that is too large
    zest_SetTextureLayerSize(texture, max_size);
    size = texture->texture_layer_size;

    const zest_uint node_count = size * 2;
    stbrp_node* nodes = 0;
    zest_vec_resize(nodes, node_count);

    if (texture->storage_type == zest_texture_storage_type_sprite_sheet) {
        while (zest__calculate_texture_layers(rects, max_width, node_count) != 1 || size > ZestDevice->max_image_size) {
            max_width *= 2;
        }

        if (max_width > ZestDevice->max_image_size) {
            return;        //todo: return somekind of error
        }
    }
    else {
        texture->layer_count = zest__calculate_texture_layers(rects, size, node_count);

        zest_FreeBitmapArray(&texture->bitmap_array);
        zest_CreateBitmapArray(&texture->bitmap_array, size, size, 4, texture->layer_count);
    }

    zest_byte current_layer = 0;

    for (zest_foreach_i(texture->layers)) {
        zest_FreeBitmap(&texture->layers[i]);
    }
    zest_vec_clear(texture->layers);

    zest_color fillcolor;
    fillcolor.r = 0;
    fillcolor.g = 0;
    fillcolor.b = 0;
    fillcolor.a = 0;

    stbrp_rect* current_rects = 0;
    while (!zest_vec_empty(rects) && current_layer < texture->layer_count) {
        stbrp_context context;
        stbrp_init_target(&context, size, size, nodes, node_count);
        stbrp_pack_rects(&context, rects, (int)zest_vec_size(rects));

        for (zest_foreach_i(rects)) {
            zest_vec_push(current_rects, rects[i]);
        }

        zest_vec_clear(rects);

        zest_bitmap_t tmp_image = zest_NewBitmap();
        zest_AllocateBitmap(&tmp_image, size, size, texture->color_channels, 0);
        int count = 0;

        for (zest_foreach_i(current_rects)) {
            stbrp_rect* rect = &current_rects[i];

            if (rect->was_packed) {
                zest_image image = texture->images[rect->id];

                float rect_x = (float)rect->x + texture->packed_border_size;
                float rect_y = (float)rect->y + texture->packed_border_size;

                image->uv.x = (rect_x + 0.5f) / (float)size;
                image->uv.y = (rect_y + 0.5f) / (float)size;
                image->uv.z = ((float)image->width + (rect_x - 0.5f)) / (float)size;
                image->uv.w = ((float)image->height + (rect_y - 0.5f)) / (float)size;
                image->uv_xy = zest_Pack16bit(image->uv.x, image->uv.y);
                image->uv_zw = zest_Pack16bit(image->uv.z, image->uv.w);
                image->left = (zest_uint)rect_x;
                image->top = (zest_uint)rect_y;
                image->layer = current_layer;
                image->texture = texture;

                zest_CopyBitmap(&texture->image_bitmaps[rect->id], 0, 0, image->width, image->height, &tmp_image, image->left, image->top);

                count++;
            }
            else {
                zest_vec_push(rects, *rect);
            }
        }

        memcpy(zest_BitmapArrayLookUp(&texture->bitmap_array, current_layer), tmp_image.data, texture->bitmap_array.size_of_each_image);

        zest_vec_push(texture->layers, tmp_image);
        current_layer++;
        zest_vec_clear(current_rects);
    }

    size_t offset = 0;

    zest_vec_clear(texture->buffer_copy_regions);

    for (zest_uint layer = 0; layer < texture->layer_count; layer++)
    {
        VkBufferImageCopy buffer_copy_region = { 0 };
        buffer_copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        buffer_copy_region.imageSubresource.mipLevel = 0;
        buffer_copy_region.imageSubresource.baseArrayLayer = layer;
        buffer_copy_region.imageSubresource.layerCount = 1;
        buffer_copy_region.imageExtent.width = size;
        buffer_copy_region.imageExtent.height = size;
        buffer_copy_region.imageExtent.depth = 1;
        buffer_copy_region.bufferOffset = offset;

        zest_vec_push(texture->buffer_copy_regions, buffer_copy_region);

        // Increase offset into staging buffer for next level / face
        offset += texture->bitmap_array.size_of_each_image;
    }

    zest_vec_free(rects);
    zest_vec_free(rects_to_process);
    zest_vec_free(nodes);
    zest_vec_free(current_rects);

}

void zest_SetImageHandle(zest_image image, float x, float y) {
    if (image->handle.x == x && image->handle.y == y)
        return;
    image->handle.x = x;
    image->handle.y = y;
    zest__update_image_vertices(image);
}

void zest__update_image_vertices(zest_image image) {
    image->min.x = image->width * (0.f - image->handle.x) * image->scale.x;
    image->min.y = image->height * (0.f - image->handle.y) * image->scale.y;
    image->max.x = image->width * (1.f - image->handle.x) * image->scale.x;
    image->max.y = image->height * (1.f - image->handle.y) * image->scale.y;
}

void zest__update_texture_single_image_meta(zest_texture texture, zest_uint width, zest_uint height) {
    zest_image_t image = zest_NewImage();
    image.width = width;
    image.height = height;
    zest__update_image_vertices(&image);
    image.uv.x = 0.f;
    image.uv.y = 0.f;
    image.uv.z = 1.f;
    image.uv.w = 1.f;
    image.uv_xy = zest_Pack16bit(image.uv.x, image.uv.y);
    image.uv_zw = zest_Pack16bit(image.uv.z, image.uv.w);
    image.texture = texture;
    image.layer = 0;
    if (zest_vec_empty(texture->images)) {
        zest_vec_push(texture->images, zest_CreateImage());
        texture->image_index = 0;
    }
    ZEST_ASSERT(zest_vec_size(texture->images) > 0);
    *texture->images[0] = image;
}

void zest_SetTextureUseFiltering(zest_texture texture, zest_bool value) {
    if (value) {
        texture->flags |= zest_texture_flag_use_filtering;
    }
    else {
        texture->flags &= ~zest_texture_flag_use_filtering;
    }
}

void zest_SetTexturePackedBorder(zest_texture texture, zest_uint value) {
    texture->packed_border_size = value;
}

void zest_SetTextureStorageType(zest_texture texture, zest_texture_storage_type value) {
    texture->storage_type = value;
}

zest_texture zest_CreateTexture(const char* name, zest_texture_storage_type storage_type, zest_bool use_filtering, zest_texture_format image_format, zest_uint reserve_images) {
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->textures, name));    //That name already exists
    zest_texture texture = zest_NewTexture();
    zest_SetText(&texture->name, name);
    zest_SetTextureImageFormat(texture, image_format);
    if (storage_type < zest_texture_storage_type_render_target) {
        zest_SetTextureStorageType(texture, storage_type);
        zest_SetTextureUseFiltering(texture, use_filtering);
    }
    if (storage_type == zest_texture_storage_type_single || storage_type == zest_texture_storage_type_bank) {
        zest_SetTextureWrappingRepeat(texture);
    }
    if (reserve_images) {
        zest_vec_reserve(texture->images, reserve_images);
    }
    zest_map_insert(ZestRenderer->textures, name, texture);
    return texture;
}

zest_texture zest_CreateTexturePacked(const char* name, zest_texture_format image_format) {
    return zest_CreateTexture(name, zest_texture_storage_type_packed, zest_texture_flag_use_filtering, image_format, 10);
}

zest_texture zest_CreateTextureSpritesheet(const char* name, zest_texture_format image_format) {
    return zest_CreateTexture(name, zest_texture_storage_type_sprite_sheet, zest_texture_flag_use_filtering, image_format, 10);
}

zest_texture zest_CreateTextureSingle(const char* name, zest_texture_format image_format) {
    return zest_CreateTexture(name, zest_texture_storage_type_single, zest_texture_flag_use_filtering, image_format, 1);
}

zest_texture zest_CreateTextureBank(const char* name, zest_texture_format image_format) {
    return zest_CreateTexture(name, zest_texture_storage_type_bank, zest_texture_flag_use_filtering, image_format, 10);
}

zest_texture zest_CreateTextureStorage(const char* name, int width, int height, zest_texture_format format, VkImageViewType view_type) {
    zest_texture texture = zest_CreateTexture(name, zest_texture_storage_type_storage, 0, format, 1);
    texture->texture.width = width;
    texture->texture.height = height;
    texture->image_layout = VK_IMAGE_LAYOUT_GENERAL;
    zest__update_image_vertices(&texture->texture);
    zest_GetTextureSingleBitmap(texture)->width = width;
    zest_GetTextureSingleBitmap(texture)->height = height;
    texture->image_view_type = view_type;
    zest_ProcessTextureImages(texture);
    return texture;
}

void zest_DeleteTexture(zest_texture texture) {
    zest_vec_push(ZestRenderer->texture_delete_queue, texture);
}

void zest_ResetTexture(zest_texture texture) {
    zest__free_all_texture_images(texture);
    zest__cleanup_texture(texture);
}

void zest_FreeTextureBitmaps(zest_texture texture) {
    zest__free_all_texture_images(texture);
}

void zest_RemoveTextureImage(zest_texture texture, zest_image image) {
    ZEST_ASSERT((zest_uint)image->index < zest_vec_size(texture->images));    //Image not within range of image indexes in the texture
    zest_FreeBitmap(&texture->image_bitmaps[image->index]);
    zest_vec_erase(texture->image_bitmaps, &texture->image_bitmaps[image->index]);
    zest_vec_erase(texture->images, &texture->images[image->index]);
    ZEST__FREE(image);
    zest__reindex_texture_images(texture);
}

void zest_RemoveTextureAnimation(zest_texture texture, zest_image first_image) {
    ZEST_ASSERT(first_image->frames > 1);    //Must be an animation image that you're deleting and also note that you must pass in the first frame of the image
    for (zest_uint i = 0; i < first_image->frames; ++i) {
        zest_FreeBitmap(&texture->image_bitmaps[first_image->index + i]);
    }
    zest_vec_erase_range(texture->image_bitmaps, &texture->image_bitmaps[first_image->index], &texture->image_bitmaps[first_image->index + first_image->frames]);
    zest_vec_erase_range(texture->images, &texture->images[first_image->index], &texture->images[first_image->index + first_image->frames]);
    ZEST__FREE(first_image);
    zest__reindex_texture_images(texture);
}

void zest_SetTextureImageFormat(zest_texture texture, zest_texture_format format) {
    ZEST_ASSERT(zest_vec_size(texture->images) == 0);    //You cannot change the image format of a texture that already has images
    texture->image_format = format;
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM: {
        texture->color_channels = 4;
    } break;
    case VK_FORMAT_R8_UNORM: {
        texture->color_channels = 1;
    } break;
    default: {
        ZEST_ASSERT(0);    //Unknown texture format
    } break;
    }
}

zest_byte* zest_BitmapArrayLookUp(zest_bitmap_array_t* bitmap_array, zest_index index) {
    ZEST_ASSERT((zest_uint)index < bitmap_array->size_of_array);
    return bitmap_array->data + index * bitmap_array->size_of_each_image;
}

zest_image zest_GetImageFromTexture(zest_texture texture, zest_index index) {
    return texture->images[index];
}

zest_texture zest_GetTexture(const char* name) {
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->textures, name));    //That name could not be found in the storage
    return *zest_map_at(ZestRenderer->textures, name);
}

zest_bool zest_TextureCanTile(zest_texture texture) {
    return texture->storage_type == zest_texture_storage_type_bank || texture->storage_type == zest_texture_storage_type_single;
}

void zest_SetTextureWrapping(zest_texture texture, VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w) {
    texture->sampler_info.addressModeU = u;
    texture->sampler_info.addressModeV = v;
    texture->sampler_info.addressModeW = w;
}

void zest_SetTextureWrappingClamp(zest_texture texture) {
    texture->sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texture->sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texture->sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}

void zest_SetTextureWrappingBorder(zest_texture texture) {
    texture->sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    texture->sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    texture->sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
}

void zest_SetTextureWrappingRepeat(zest_texture texture) {
    texture->sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    texture->sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    texture->sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

void zest_SetTextureLayerSize(zest_texture texture, zest_uint size) {
    zest_uint next_pow2 = (zest_uint)ZEST__NEXTPOW2(size);
    ZEST_ASSERT(ZEST__POW2(next_pow2));
    texture->texture_layer_size = next_pow2;
}

void zest_SetTextureMaxRadiusOnLoad(zest_texture texture, zest_bool yesno) {
    if (yesno) {
        ZEST__FLAG(texture->flags, zest_texture_flag_get_max_radius);
    }
    else {
        ZEST__UNFLAG(texture->flags, zest_texture_flag_get_max_radius);
    }
}

void zest_SetTextureImguiBlendType(zest_texture texture, zest_imgui_blendtype blend_type) {
    texture->imgui_blend_type = blend_type;
}

void zest_TextureResize(zest_texture texture, zest_uint width, zest_uint height) {
    ZEST_ASSERT(width > 0 && height > 0);
    ZEST_ASSERT(texture->storage_type == zest_texture_storage_type_single || texture->storage_type == zest_texture_storage_type_storage);
    zest_ResetTexture(texture);
    texture->texture.width = width;
    texture->texture.height = height;
    zest__update_image_vertices(&texture->texture);
    zest_GetTextureSingleBitmap(texture)->width = width;
    zest_GetTextureSingleBitmap(texture)->height = height;
    ZEST__UNFLAG(texture->flags, zest_texture_flag_textures_ready);
    zest_ProcessTextureImages(texture);
}

void zest_TextureClear(zest_texture texture) {
    VkCommandBuffer command_buffer = zest__begin_single_time_commands();

    VkImageSubresourceRange image_sub_resource_range;
    image_sub_resource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_sub_resource_range.baseMipLevel = 0;
    image_sub_resource_range.levelCount = 1;
    image_sub_resource_range.baseArrayLayer = 0;
    image_sub_resource_range.layerCount = 1;

    VkClearColorValue ClearColorValue = { 0.0, 0.0, 0.0, 0.0 };

    zest__insert_image_memory_barrier(
        command_buffer,
        texture->frame_buffer.image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        texture->image_layout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_sub_resource_range);

    vkCmdClearColorImage(command_buffer, texture->frame_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &ClearColorValue, 1, &image_sub_resource_range);

    zest__insert_image_memory_barrier(
        command_buffer,
        texture->frame_buffer.image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        texture->image_layout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_sub_resource_range);

    zest__end_single_time_commands(command_buffer);
}

void zest_CopyFramebufferToTexture(zest_frame_buffer_t* src_image, zest_texture texture, int src_x, int src_y, int dst_x, int dst_y, int width, int height) {
    VkCommandBuffer copy_command = zest__begin_single_time_commands();

    VkImageLayout target_layout = texture->descriptor.imageLayout;

    VkImageSubresourceRange image_range = { 0 };
    image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_range.baseMipLevel = 0;
    image_range.levelCount = 1;
    image_range.baseArrayLayer = 0;
    image_range.layerCount = 1;

    // Transition destination image to transfer destination layout
    zest__insert_image_memory_barrier(
        copy_command,
        texture->frame_buffer.image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        texture->descriptor.imageLayout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    // Transition swapchain image from present to transfer source layout
    zest__insert_image_memory_barrier(
        copy_command,
        src_image->color_buffer.image,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    VkOffset3D src_blit_offset;
    src_blit_offset.x = src_x;
    src_blit_offset.y = src_y;
    src_blit_offset.z = 0;

    VkOffset3D dst_blit_offset;
    dst_blit_offset.x = dst_x;
    dst_blit_offset.y = dst_y;
    dst_blit_offset.z = 0;

    VkOffset3D blit_size_src;
    blit_size_src.x = src_x + width;
    blit_size_src.y = src_y + height;
    blit_size_src.z = 1;

    VkOffset3D blit_size_dst;
    blit_size_dst.x = dst_x + width;
    blit_size_dst.y = dst_y + height;
    blit_size_dst.z = 1;

    VkImageBlit image_blit = { 0 };
    image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_blit.srcSubresource.layerCount = 1;
    image_blit.srcOffsets[0] = src_blit_offset;
    image_blit.srcOffsets[1] = blit_size_src;
    image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_blit.dstSubresource.layerCount = 1;
    image_blit.dstOffsets[0] = dst_blit_offset;
    image_blit.dstOffsets[1] = blit_size_dst;

    // Issue the blit command
    vkCmdBlitImage(
        copy_command,
        src_image->color_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        texture->frame_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &image_blit,
        VK_FILTER_LINEAR);

    // Transition destination image to general layout, which is the required layout for mapping the image memory later on
    zest__insert_image_memory_barrier(
        copy_command,
        texture->frame_buffer.image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        target_layout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    zest__insert_image_memory_barrier(
        copy_command,
        src_image->color_buffer.image,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    zest__end_single_time_commands(copy_command);

}

void zest_CopyTextureToTexture(zest_texture src_image, zest_texture target, int src_x, int src_y, int dst_x, int dst_y, int width, int height) {
    VkCommandBuffer copy_command = zest__begin_single_time_commands();

    VkImageLayout target_layout = target->descriptor.imageLayout;

    VkImageSubresourceRange image_range = { 0 };
    image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_range.baseMipLevel = 0;
    image_range.levelCount = 1;
    image_range.baseArrayLayer = 0;
    image_range.layerCount = 1;

    // Transition destination image to transfer destination layout
    zest__insert_image_memory_barrier(
        copy_command,
        target->frame_buffer.image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        target->descriptor.imageLayout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    // Transition swapchain image from present to transfer source layout
    zest__insert_image_memory_barrier(
        copy_command,
        src_image->frame_buffer.image,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        src_image->descriptor.imageLayout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    VkOffset3D src_blit_offset;
    src_blit_offset.x = src_x;
    src_blit_offset.y = src_y;
    src_blit_offset.z = 0;

    VkOffset3D dst_blit_offset;
    dst_blit_offset.x = dst_x;
    dst_blit_offset.y = dst_y;
    dst_blit_offset.z = 0;

    VkOffset3D blit_size_src;
    blit_size_src.x = src_x + width;
    blit_size_src.y = src_y + height;
    blit_size_src.z = 1;

    VkOffset3D blit_size_dst;
    blit_size_dst.x = dst_x + width;
    blit_size_dst.y = dst_y + height;
    blit_size_dst.z = 1;

    VkImageBlit image_blit_region = { 0 };
    image_blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_blit_region.srcSubresource.layerCount = 1;
    image_blit_region.srcOffsets[0] = src_blit_offset;
    image_blit_region.srcOffsets[1] = blit_size_src;
    image_blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_blit_region.dstSubresource.layerCount = 1;
    image_blit_region.dstOffsets[0] = dst_blit_offset;
    image_blit_region.dstOffsets[1] = blit_size_dst;

    // Issue the blit command
    vkCmdBlitImage(
        copy_command,
        src_image->frame_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        target->frame_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &image_blit_region,
        VK_FILTER_NEAREST);

    // Transition destination image to general layout, which is the required layout for mapping the image memory later on
    zest__insert_image_memory_barrier(
        copy_command,
        target->frame_buffer.image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        target_layout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    zest__insert_image_memory_barrier(
        copy_command,
        src_image->frame_buffer.image,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src_image->descriptor.imageLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    zest__end_single_time_commands(copy_command);

}

void zest_CopyTextureToBitmap(zest_texture src_image, zest_bitmap_t* image, int src_x, int src_y, int dst_x, int dst_y, int width, int height, zest_bool swap_channel) {
    VkCommandBuffer copy_command = zest__begin_single_time_commands();
    VkImage temp_image;
    VkDeviceMemory temp_memory;
    zest__create_temporary_image(width, height, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &temp_image, &temp_memory);

    zest_bool supports_blit = zest__has_blit_support(src_image->frame_buffer.format);

    VkImageSubresourceRange image_range = { 0 };
    image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_range.baseMipLevel = 0;
    image_range.levelCount = 1;
    image_range.baseArrayLayer = 0;
    image_range.layerCount = 1;

    // Transition temporary image to transfer destination layout
    zest__insert_image_memory_barrier(
        copy_command,
        temp_image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    // Transition source image from present to transfer source layout
    zest__insert_image_memory_barrier(
        copy_command,
        src_image->frame_buffer.image,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        src_image->image_layout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    if (supports_blit) {
        VkOffset3D src_blit_offset;
        src_blit_offset.x = src_x;
        src_blit_offset.y = src_y;
        src_blit_offset.z = 0;

        VkOffset3D dst_blit_offset;
        dst_blit_offset.x = dst_x;
        dst_blit_offset.y = dst_y;
        dst_blit_offset.z = 0;

        VkOffset3D blit_size_src;
        blit_size_src.x = src_x + width;
        blit_size_src.y = src_y + height;
        blit_size_src.z = 1;

        VkOffset3D blit_size_dst;
        blit_size_dst.x = dst_x + width;
        blit_size_dst.y = dst_y + height;
        blit_size_dst.z = 1;

        VkImageBlit image_blit_region = { 0 };
        image_blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_blit_region.srcSubresource.layerCount = 1;
        image_blit_region.srcOffsets[0] = src_blit_offset;
        image_blit_region.srcOffsets[1] = blit_size_src;
        image_blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_blit_region.dstSubresource.layerCount = 1;
        image_blit_region.dstOffsets[0] = dst_blit_offset;
        image_blit_region.dstOffsets[1] = blit_size_dst;

        // Issue the blit command
        vkCmdBlitImage(
            copy_command,
            src_image->frame_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            temp_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &image_blit_region,
            VK_FILTER_NEAREST);
    }
    else
    {
        // Otherwise use image copy (requires us to manually flip components)
        VkImageCopy image_copy_region = { 0 };
        image_copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_copy_region.srcSubresource.layerCount = 1;
        image_copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_copy_region.dstSubresource.layerCount = 1;
        image_copy_region.srcOffset.x = src_x;
        image_copy_region.srcOffset.y = src_y;
        image_copy_region.dstOffset.x = dst_x;
        image_copy_region.dstOffset.y = dst_y;
        image_copy_region.extent.width = width;
        image_copy_region.extent.height = height;
        image_copy_region.extent.depth = 1;

        // Issue the copy command
        vkCmdCopyImage(
            copy_command,
            src_image->frame_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            temp_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &image_copy_region);
    }

    // Transition destination image to general layout
    zest__insert_image_memory_barrier(
        copy_command,
        temp_image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    // Transition back the source image
    zest__insert_image_memory_barrier(
        copy_command,
        src_image->frame_buffer.image,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src_image->image_layout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    zest__end_single_time_commands(copy_command);

    // Get layout of the image (including row pitch)
    VkImageSubresource sub_resource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout sub_resource_layout;
    vkGetImageSubresourceLayout(ZestDevice->logical_device, temp_image, &sub_resource, &sub_resource_layout);

    // If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
    zest_bool color_swizzle = 0;
    // Check if source is BGR
    // Note: Not complete, only contains most common and basic BGR surface formats for demonstation purposes
    if (!supports_blit)
    {
        if (src_image->image_format == VK_FORMAT_B8G8R8A8_SRGB || src_image->image_format == VK_FORMAT_B8G8R8A8_UNORM || src_image->image_format == VK_FORMAT_B8G8R8A8_SNORM) {
            color_swizzle = 1;
        }
    }

    // Map image memory so we can start copying from it
    void* data = 0;
    vkMapMemory(ZestDevice->logical_device, temp_memory, 0, VK_WHOLE_SIZE, 0, &data);
    memcpy(image->data, data, image->size);

    if (color_swizzle) {
        zest_ConvertBGRAToRGBA(image);
    }

    vkUnmapMemory(ZestDevice->logical_device, temp_memory);
    vkFreeMemory(ZestDevice->logical_device, temp_memory, &ZestDevice->allocation_callbacks);
    vkDestroyImage(ZestDevice->logical_device, temp_image, &ZestDevice->allocation_callbacks);
}
//-- End Texture and Image Functions

//-- Fonts
zest_font zest_LoadMSDFFont(const char* filename) {
    zest_font_t blank_font = { 0 };
    zest_font font = ZEST__NEW(zest_font);
    *font = blank_font;

    char* font_data = zest_ReadEntireFile(filename, 0);
    ZEST_ASSERT(font_data);            //File not found
    zest_vec_resize(font->characters, 256);
    font->characters['\n'].flags = zest_character_flag_new_line;

    zest_font_character_t c;
    zest_uint character_count = 0;
    zest_uint file_version = 0;

    zest_uint position = 0;
    char magic_number[6];
    memcpy(magic_number, font_data + position, 6);
    position += 6;
    ZEST_ASSERT(strcmp((const char*)magic_number, "!TSEZ") == 0);        //Not a valid ztf file

    memcpy(&character_count, font_data + position, sizeof(zest_uint));
    position += sizeof(zest_uint);
    memcpy(&file_version, font_data + position, sizeof(zest_uint));
    position += sizeof(zest_uint);
    memcpy(&font->pixel_range, font_data + position, sizeof(float));
    position += sizeof(float);
    memcpy(&font->miter_limit, font_data + position, sizeof(float));
    position += sizeof(float);
    memcpy(&font->padding, font_data + position, sizeof(float));
    position += sizeof(float);

    for (zest_uint i = 0; i != character_count; ++i) {
        memcpy(&c, font_data + position, sizeof(zest_font_character_t));
        position += sizeof(zest_font_character_t);

        font->max_yoffset = ZEST__MAX(fabsf(c.yoffset), font->max_yoffset);

        const char key = c.character[0];
        font->characters[key] = c;
    }

    memcpy(&font->font_size, font_data + position, sizeof(float));
    position += sizeof(float);

    zest_uint image_size;

    memcpy(&image_size, font_data + position, sizeof(zest_uint));
    position += sizeof(zest_uint);
    zest_byte* image_data = 0;
    zest_vec_resize(image_data, image_size);
    size_t buffer_size;
    memcpy(&buffer_size, font_data + position, sizeof(size_t));
    position += sizeof(size_t);
    memcpy(image_data, font_data + position, buffer_size);

    font->texture = zest_CreateTextureSingle(filename, zest_texture_format_rgba);

    stbi_set_flip_vertically_on_load(1);
    zest_LoadBitmapImageMemory(zest_GetTextureSingleBitmap(font->texture), image_data, image_size, 0);

    zest_image_t* image = &font->texture->texture;
    image->width = zest_GetTextureSingleBitmap(font->texture)->width;
    image->height = zest_GetTextureSingleBitmap(font->texture)->height;

    zest_vec_free(image_data);
    zest_vec_free(font_data);

    zest__setup_font_texture(font);
    stbi_set_flip_vertically_on_load(0);

    zest_SetText(&font->name, filename);

    return zest__add_font(font);
}

void zest_UnloadFont(zest_font font) {
    zest_DeleteTexture(font->texture);
    zest__delete_font(font);
}

zest_font zest__add_font(zest_font font) {
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->fonts, font->name.str));    //A font already exists with that name
    zest_map_insert(ZestRenderer->fonts, font->name.str, font);
    return font;
}

zest_font zest_GetFont(const char* name) {
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->fonts, name));    //No font found with that index
    return *zest_map_at(ZestRenderer->fonts, name);
}

void zest__setup_font_texture(zest_font font) {
    if (ZEST__FLAGGED(font->texture->flags, zest_texture_flag_textures_ready)) {
        zest__cleanup_texture(font->texture);
    }

    zest_SetTextureUseFiltering(font->texture, ZEST_FALSE);
    zest_ProcessTextureImages(font->texture);

    font->pipeline = zest_Pipeline("pipeline_fonts");
    font->descriptor_set = zest_GetTextureDescriptorSet(font->texture, "Default");
}
//-- End Fonts

//Render Targets
void zest__initialise_render_target(zest_render_target render_target, zest_render_target_create_info_t* info) {
    if (ZEST__FLAGGED(render_target->flags, zest_render_target_flag_initialised)) {
        for (ZEST_EACH_FIF_i) {
            zest__cleanup_framebuffer(&render_target->framebuffers[i]);
        }
    }

    ZEST_ASSERT(info->viewport.extent.width != 0 && info->viewport.extent.height != 0); //You must define a viewport width and height before initialising the drawing operations
    render_target->viewport = info->viewport;
    render_target->render_width = info->viewport.extent.width;
    render_target->render_height = info->viewport.extent.height;
    render_target->cls_color = zest_Vec4Set1(0.f);

    if (ZEST__FLAGGED(info->flags, zest_render_target_flag_single_frame_buffer_only)) {
        render_target->frames_in_flight = 1;
    }

    render_target->create_info = *info;

    for (zest_index i = 0; i != render_target->frames_in_flight; ++i) {
        zest_frame_buffer_t frame_buffer = { 0 };
        render_target->framebuffers[i] = frame_buffer;
        render_target->sampler_textures[i] = 0;
    }

    render_target->render_format = info->render_format;
    if (ZEST__FLAGGED(render_target->flags, zest_render_target_flag_use_depth_buffer)) {
        render_target->render_pass = zest_GetRenderPass("Render pass standard");
    }
    else {
        render_target->render_pass = zest_GetRenderPass("Render pass standard no depth");
    }
    for (zest_index i = 0; i != render_target->frames_in_flight; ++i) {
        render_target->framebuffers[i] = zest_CreateFrameBuffer(render_target->render_pass->render_pass,
            render_target->render_width, render_target->render_height, info->render_format,
            ZEST__FLAGGED(render_target->flags, zest_render_target_flag_use_depth_buffer), ZEST__FLAGGED(info->flags, zest_render_target_flag_is_src));
    }

    zest__create_render_target_sampler_image(render_target);
    render_target->final_render = zest_Pipeline("pipeline_final_render");

    render_target->push_constants.screen_resolution.x = (float)(info->viewport.extent.width);
    render_target->push_constants.screen_resolution.y = (float)(info->viewport.extent.height);

    ZEST__FLAG(render_target->flags, zest_render_target_flag_initialised);
}

void zest__create_render_target_sampler_image(zest_render_target render_target) {
    if (ZEST__FLAGGED(render_target->create_info.flags, zest_render_target_flag_sampler_size_match_texture)) {
        render_target->sampler_image.width = render_target->create_info.viewport.extent.width;
        render_target->sampler_image.height = render_target->create_info.viewport.extent.height;
    }
    else {
        render_target->sampler_image.width = zest_GetSwapChainExtent().width;
        render_target->sampler_image.height = zest_GetSwapChainExtent().height;
    }
    for (zest_index i = 0; i != render_target->frames_in_flight; ++i) {
        char* texture_name = 0;
        zest_vec_resize(texture_name, (zest_uint)strlen(render_target->name) + 1);
        zest_size size = zest_vec_size(texture_name);
        zest_strcpy(texture_name, size, render_target->name);
        zest_vec_pop(texture_name);
        zest_vec_push(texture_name, '_');
        char temp = 48 + i;
        zest_vec_push(texture_name, temp);
        zest_vec_push(texture_name, '\0');
        render_target->sampler_textures[i] = zest_CreateTexture(texture_name, zest_texture_storage_type_render_target, 0, render_target->render_format, 0);

        zest_texture texture = render_target->sampler_textures[i];
        texture->imgui_blend_type = render_target->create_info.imgui_blend_type;
        zest__update_texture_single_image_meta(texture, render_target->sampler_image.width, render_target->sampler_image.height);

        texture->sampler_info.addressModeU = render_target->create_info.sampler_address_mode_u;
        texture->sampler_info.addressModeV = render_target->create_info.sampler_address_mode_v;
        texture->sampler_info.addressModeW = render_target->create_info.sampler_address_mode_w;
        zest__create_texture_sampler(texture, texture->sampler_info, 1);

        VkDescriptorImageInfo image_info = { 0 };
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = render_target->framebuffers[i].color_buffer.view;
        image_info.sampler = texture->sampler;
        texture->descriptor = image_info;
        zest_CreateSimpleTextureDescriptorSet(texture, "Default", "Standard 2d Uniform Buffer");
        zest_SwitchTextureDescriptorSet(texture, "Default");

        zest_vec_free(texture_name);
    }
}

zest_render_target zest_NewRenderTarget() {
    zest_render_target_t blank_render_target = { 0 };
    zest_render_target render_target = ZEST__NEW_ALIGNED(zest_render_target, 16);
    *render_target = blank_render_target;
    render_target->render_width = 0;
    render_target->render_height = 0;
    render_target->flags = zest_render_target_flag_render_to_swap_chain;
    render_target->post_process_callback = 0;
    render_target->post_process_user_data = 0;
    render_target->frames_in_flight = ZEST_MAX_FIF;
    return render_target;
}

zest_render_target_create_info_t zest_RenderTargetCreateInfo() {
    zest_render_target_create_info_t create_info = {
        .ratio_of_screen_size = zest_Vec2Set1(0.f),
        .imgui_blend_type = zest_imgui_blendtype_pass,
        .sampler_address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .sampler_address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .sampler_address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .flags = zest_render_target_flag_is_src | zest_render_target_flag_sampler_size_match_texture,
        .input_source = 0,
        .render_format = ZestRenderer->swapchain_image_format,
        .viewport = {0}
    };
    if (ZEST__FLAGGED(ZestRenderer->flags, zest_renderer_flag_has_depth_buffer)) {
        create_info.flags |= zest_render_target_flag_use_depth_buffer;
    }
    return create_info;
}

zest_render_target zest_CreateRenderTarget(const char* name) {
    return zest_CreateRenderTargetWithInfo(name, zest_RenderTargetCreateInfo());
}

zest_render_target zest_CreateRenderTargetWithInfo(const char* name, zest_render_target_create_info_t create_info) {
    if (zest_map_valid_name(ZestRenderer->render_targets, name)) {
        ZEST_PRINT_WARNING("%s - %s", name, "This render target name already exists, existing render target will be overwritten.");
    }
    zest_render_target render_target = zest_NewRenderTarget();
    render_target->name = name;
    if (create_info.viewport.extent.width == 0 || create_info.viewport.extent.height == 0) {
        create_info.viewport.extent = ZestRenderer->swapchain_extent;
        ZEST__UNFLAG(create_info.flags, zest_render_target_flag_fixed_size);
    }
    else if (!zest_Vec2Length2(create_info.ratio_of_screen_size)) {
        ZEST__FLAG(create_info.flags, zest_render_target_flag_fixed_size);
    }
    create_info.render_format = ZestRenderer->swapchain_image_format;

    ZEST__FLAG(ZestRenderer->flags, zest_renderer_flag_schedule_rerecord_final_render_buffer);

    render_target->create_info = create_info;
    render_target->input_source = create_info.input_source;
    ZEST__FLAG(render_target->flags, create_info.flags);

    zest_map_insert(ZestRenderer->render_targets, name, render_target);

    zest__initialise_render_target(render_target, &render_target->create_info);

    return render_target;
}

zest_render_target zest_AddPostProcessRenderTarget(const char* name, float width_ratio_of_input, float height_ration_of_input, zest_render_target input_source, void* user_data, void(*render_callback)(zest_render_target target, void* user_data)) {
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->render_targets, name));    //Couldn't find a render target by that name
    zest_render_target_create_info_t create_info = { 0 };
    create_info.ratio_of_screen_size = zest_Vec2Set(width_ratio_of_input, height_ration_of_input);
    create_info.viewport.extent.width = (zest_uint)((float)input_source->render_width * width_ratio_of_input);
    create_info.viewport.extent.height = (zest_uint)((float)input_source->render_height * height_ration_of_input);
    create_info.input_source = input_source;
    create_info.sampler_address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    create_info.sampler_address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    create_info.sampler_address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ZEST__UNFLAG(create_info.flags, zest_render_target_flag_sampler_size_match_texture);
    zest_render_target target = zest_CreateRenderTargetWithInfo(name, create_info);
    zest_SetRenderTargetPostProcessCallback(target, render_callback);
    zest_SetRenderTargetPostProcessUserData(target, user_data);
    return target;
}

void zest_SetRenderTargetPostProcessCallback(zest_render_target render_target, void(*render_callback)(zest_render_target target, void* user_data)) {
    render_target->post_process_callback = render_callback;
}

void zest_SetRenderTargetPostProcessUserData(zest_render_target render_target, void* user_data) {
    render_target->post_process_user_data = user_data;
}

VkDescriptorSet* zest_GetRenderTargetSamplerDescriptorSet(zest_render_target render_target) {
    return &render_target->sampler_textures[ZEST_FIF]->descriptor_sets.data[0]->descriptor_set[ZEST_FIF];
}

VkDescriptorSet* zest_GetRenderTargetSourceDescriptorSet(zest_render_target render_target) {
    return zest_GetRenderTargetSamplerDescriptorSet(render_target->input_source);
}

zest_texture zest_GetRenderTargetTexture(zest_render_target render_target) {
    return render_target->sampler_textures[ZEST_FIF];
}

zest_image zest_GetRenderTargetImage(zest_render_target render_target) {
    zest_texture texture = render_target->sampler_textures[ZEST_FIF];
    return zest_GetImageFromTexture(texture, 0);
}

zest_frame_buffer_t* zest_RenderTargetFramebuffer(zest_render_target render_target) {
    return &render_target->framebuffers[ZEST_FIF];
}

zest_frame_buffer_t* zest_RenderTargetFramebufferByFIF(zest_render_target render_target, zest_index fif) {
    return &render_target->framebuffers[fif];
}

zest_render_target zest_GetRenderTarget(const char* name) {
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->render_targets, name));    //No render target found with that index
    return *zest_map_at(ZestRenderer->render_targets, name);
}

VkFramebuffer zest_GetRendererFrameBufferCallback(zest_command_queue_draw_commands item) {
    return ZestRenderer->swapchain_frame_buffers[ZestRenderer->current_frame];
}

VkFramebuffer zest_GetRenderTargetFrameBufferCallback(zest_command_queue_draw_commands item) {
    //This is dubious if the render target does not need multiple frames in flight?
    zest_index fif = ZEST__FLAGGED(item->render_target->flags, zest_render_target_flag_single_frame_buffer_only) ? 0 : ZEST_FIF;
    return item->render_target->framebuffers[fif].device_frame_buffer;
}

void zest_RecreateRenderTargetResources(zest_render_target render_target, zest_index fif) {
    int width, height;
    if (zest_Vec2Length(render_target->create_info.ratio_of_screen_size)) {
        width = (zest_uint)((float)render_target->create_info.input_source->render_width * render_target->create_info.ratio_of_screen_size.x);
        height = (zest_uint)((float)render_target->create_info.input_source->render_height * render_target->create_info.ratio_of_screen_size.y);
    }
    else if (ZEST__NOT_FLAGGED(render_target->create_info.flags, zest_render_target_flag_fixed_size)) {
        width = zest_SwapChainWidth();
        height = zest_SwapChainHeight();
    }
    else {
        width = render_target->create_info.viewport.extent.width;
        height = render_target->create_info.viewport.extent.height;
    }

    render_target->viewport.extent.width = width;
    render_target->viewport.extent.height = height;
    render_target->final_render->create_info.viewport.extent = render_target->viewport.extent;
    render_target->final_render->create_info.viewport.offset = render_target->viewport.offset;
    render_target->render_width = width;
    render_target->render_height = height;
    render_target->create_info.viewport.extent.width = width;
    render_target->create_info.viewport.extent.height = height;

    zest_CleanUpRenderTarget(render_target, fif);

    zest_FreeBuffer(render_target->framebuffers[fif].color_buffer.buffer);
    if (ZEST__FLAGGED(render_target->flags, zest_render_target_flag_use_depth_buffer)) {
        zest_FreeBuffer(render_target->framebuffers[fif].depth_buffer.buffer);
    }
    render_target->framebuffers[fif] = zest_CreateFrameBuffer(render_target->render_pass->render_pass, render_target->render_width, render_target->render_height,
        render_target->render_format,
        ZEST__FLAGGED(render_target->flags, zest_render_target_flag_use_depth_buffer),
        ZEST__FLAGGED(render_target->flags, zest_render_target_flag_is_src));

    if (ZEST__FLAGGED(render_target->flags, zest_render_target_flag_sampler_size_match_texture)) {
        render_target->sampler_image.width = width;
        render_target->sampler_image.height = height;
    }
    else {
        render_target->sampler_image.width = zest_GetSwapChainExtent().width;
        render_target->sampler_image.height = zest_GetSwapChainExtent().height;
    }

    zest__refresh_render_target_sampler(render_target, fif);

}

void zest__refresh_render_target_sampler(zest_render_target render_target, zest_index fif) {
    if (render_target->sampler_textures[fif]->sampler != VK_NULL_HANDLE) {
        vkDestroySampler(ZestDevice->logical_device, render_target->sampler_textures[fif]->sampler, &ZestDevice->allocation_callbacks);
    }
    zest_texture texture = render_target->sampler_textures[fif];
    zest__update_texture_single_image_meta(texture, render_target->sampler_image.width, render_target->sampler_image.height);

    zest__create_texture_sampler(texture, texture->sampler_info, 1);

    VkDescriptorImageInfo image_info = { 0 };
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = render_target->framebuffers[fif].color_buffer.view;
    image_info.sampler = texture->sampler;
    texture->descriptor = image_info;
    zest_RefreshTextureDescriptors(texture);
}

void zest_DrawToRenderTargetCallback(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, zest_render_pass render_pass, VkFramebuffer framebuffer) {
    VkRenderPassBeginInfo render_pass_info = { 0 };
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass->render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea = item->viewport;

    VkClearValue clear_values[2] = {
        [0] .color = {.float32[0] = item->cls_color.x, .float32[1] = item->cls_color.y, .float32[2] = item->cls_color.y, .float32[3] = item->cls_color.w },
        [1].depthStencil = {.depth = 1.0f, .stencil = 0 }
    };

    render_pass_info.clearValueCount = 2;
    render_pass_info.pClearValues = clear_values;
    zest_render_target target = item->render_target;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport view = zest_CreateViewport(0.f, 0.f, (float)item->viewport.extent.width, (float)item->viewport.extent.height, 0.f, 1.f);
    VkRect2D scissor = zest_CreateRect2D(item->viewport.extent.width, item->viewport.extent.height, 0, 0);
    vkCmdSetViewport(command_buffer, 0, 1, &view);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    if (!target->post_process_callback) {

        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, target->final_render->pipeline_layout, 0, 1, zest_GetRenderTargetSamplerDescriptorSet(target), 0, NULL);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, target->final_render->pipeline);

        vkCmdPushConstants(
            command_buffer,
            target->final_render->pipeline_layout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(zest_final_render_push_constants_t),
            &target->push_constants);

        vkCmdDraw(command_buffer, 3, 1, 0, 0);
    }
    else {
        target->post_process_callback(target, target->post_process_user_data);
    }

    vkCmdEndRenderPass(command_buffer);
}

void zest_DrawRenderTargetsToSwapchain(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, zest_render_pass render_pass, VkFramebuffer framebuffer) {
    VkRenderPassBeginInfo render_pass_info = { 0 };
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass->render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea = item->viewport;

    VkClearValue clear_values[2] = {
        [0] .color = {.float32[0] = item->cls_color.x, .float32[1] = item->cls_color.y, .float32[2] = item->cls_color.z, .float32[3] = item->cls_color.w },
        [1].depthStencil = {.depth = 1.0f, .stencil = 0 }
    };

    render_pass_info.clearValueCount = 2;
    render_pass_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    for (zest_foreach_i(item->render_targets)) {
        zest_render_target target = item->render_targets[i];
        if (!target->post_process_callback) {
            VkViewport view = zest_CreateViewport(0.f, 0.f, (float)item->viewport.extent.width, (float)item->viewport.extent.height, 0.f, 1.f);
            VkRect2D scissor = zest_CreateRect2D(item->viewport.extent.width, item->viewport.extent.height, 0, 0);
            vkCmdSetViewport(command_buffer, 0, 1, &view);
            vkCmdSetScissor(command_buffer, 0, 1, &scissor);

            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, target->final_render->pipeline_layout, 0, 1, zest_GetRenderTargetSamplerDescriptorSet(target), 0, NULL);
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, target->final_render->pipeline);

            vkCmdPushConstants(
                command_buffer,
                target->final_render->pipeline_layout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(zest_final_render_push_constants_t),
                &target->push_constants);

            vkCmdDraw(command_buffer, 3, 1, 0, 0);
        }
        else {
            target->post_process_callback(target, target->post_process_user_data);
        }
    }

    vkCmdEndRenderPass(command_buffer);
}

void zest_CleanUpRenderTarget(zest_render_target render_target, zest_index fif) {
    zest__cleanup_framebuffer(&render_target->framebuffers[fif]);
    if (render_target->sampler_textures[fif]->sampler != VK_NULL_HANDLE) {
        vkDestroySampler(ZestDevice->logical_device, render_target->sampler_textures[fif]->sampler, &ZestDevice->allocation_callbacks);
        render_target->sampler_textures[fif]->sampler = VK_NULL_HANDLE;
    }
}

void zest_PreserveRenderTargetFrames(zest_render_target render_target, zest_bool yesno) {
    if (ZEST__FLAGGED(render_target->flags, zest_render_target_flag_use_depth_buffer)) {
        render_target->render_pass = (yesno != 0 ? zest_GetRenderPass("Render pass standard no clear") : zest_GetRenderPass("Render pass standard"));
    }
    else {
        render_target->render_pass = (yesno != 0 ? zest_GetRenderPass("Render pass standard no clear no depth") : zest_GetRenderPass("Render pass standard no depth"));
    }
}

void zest_ResizeRenderTarget(zest_render_target render_target, zest_uint width, zest_uint height) {
    render_target->create_info.viewport.extent.width = width;
    render_target->create_info.viewport.extent.height = height;
    ZEST__FLAG(render_target->create_info.flags, zest_render_target_flag_fixed_size);

    if (render_target->frames_in_flight == 1) {
        //Not sure if pushing a render with with only 1 set of resources to the current frame in flight is correct. Maybe there's a chance that it could be
        //recreated while in use, but render targets with a single set of resources should not be used for per frame rendering so maybe it's ok.
        zest_vec_push(ZestRenderer->render_target_recreate_queue[0], render_target);
    }
    else {
        for (ZEST_EACH_FIF_i) {
            zest_vec_push(ZestRenderer->render_target_recreate_queue[i], render_target);
        }
    }
}

void zest_SetRenderTargetSamplerToClamp(zest_render_target render_target) {
    for (ZEST_EACH_FIF_i) {
        zest_SetTextureWrappingClamp(render_target->sampler_textures[i]);
        zest_vec_push(ZestRenderer->rt_sampler_refresh_queue[i], render_target);
    }
}

void zest_SetRenderTargetSamplerToRepeat(zest_render_target render_target) {
    for (ZEST_EACH_FIF_i) {
        zest_SetTextureWrappingRepeat(render_target->sampler_textures[i]);
        zest_vec_push(ZestRenderer->rt_sampler_refresh_queue[i], render_target);
    }
}

void zest_RenderTargetClear(zest_render_target render_target, zest_uint fif) {
    VkCommandBuffer command_buffer = zest__begin_single_time_commands();

    VkImageSubresourceRange image_sub_resource_range;
    image_sub_resource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_sub_resource_range.baseMipLevel = 0;
    image_sub_resource_range.levelCount = 1;
    image_sub_resource_range.baseArrayLayer = 0;
    image_sub_resource_range.layerCount = 1;

    VkClearColorValue ClearColorValue = { 0.f, 0.f, 0.f, 0.f };

    zest__insert_image_memory_barrier(
        command_buffer,
        render_target->framebuffers[fif].color_buffer.image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_sub_resource_range);

    vkCmdClearColorImage(command_buffer, render_target->framebuffers[fif].color_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &ClearColorValue, 1, &image_sub_resource_range);

    zest__insert_image_memory_barrier(
        command_buffer,
        render_target->framebuffers[fif].color_buffer.image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_sub_resource_range);

    zest__end_single_time_commands(command_buffer);
}
//--End Render Targets

//-- Draw Layers
//-- internal
zest_layer_instruction_t zest__layer_instruction() {
    zest_layer_instruction_t instruction = { 0 };
    instruction.pipeline = ZEST_NULL;
    return instruction;
}

void zest_ResetInstanceLayerDrawing(zest_layer instance_layer) {
    zest_vec_clear(instance_layer->draw_instructions[ZEST_FIF]);
    instance_layer->memory_refs[ZEST_FIF].write_to_buffer->memory_in_use = 0;
    instance_layer->current_instruction = zest__layer_instruction();
    instance_layer->memory_refs[ZEST_FIF].instance_count = 0;
    instance_layer->memory_refs[ZEST_FIF].instance_ptr = instance_layer->memory_refs[ZEST_FIF].write_to_buffer->data;
}

void zest__reset_mesh_layer_drawing(zest_layer layer) {
    zest_vec_clear(layer->draw_instructions[ZEST_FIF]);
    layer->memory_refs[ZEST_FIF].write_to_buffer->memory_in_use = 0;
    layer->memory_refs[ZEST_FIF].write_to_index_buffer->memory_in_use = 0;
    layer->current_instruction = zest__layer_instruction();
    layer->memory_refs[ZEST_FIF].index_count = 0;
    layer->memory_refs[ZEST_FIF].vertex_count = 0;
    layer->memory_refs[ZEST_FIF].vertex_ptr = layer->memory_refs[ZEST_FIF].write_to_buffer->data;
    layer->memory_refs[ZEST_FIF].index_ptr = layer->memory_refs[ZEST_FIF].write_to_index_buffer->data;
}

void zest_StartInstanceInstructions(zest_layer instance_layer) {
    instance_layer->current_instruction.start_index = instance_layer->memory_refs[ZEST_FIF].instance_count ? instance_layer->memory_refs[ZEST_FIF].instance_count : 0;
    instance_layer->current_instruction.push_constants = instance_layer->push_constants;
}

void zest__start_mesh_instructions(zest_layer layer) {
    layer->current_instruction.start_index = layer->memory_refs[ZEST_FIF].index_count ? layer->memory_refs[ZEST_FIF].index_count : 0;
    layer->current_instruction.push_constants = layer->push_constants;
}

void zest_EndInstanceInstructions(zest_layer instance_layer) {
    if (instance_layer->current_instruction.total_instances) {
        instance_layer->last_draw_mode = zest_draw_mode_none;
        zest_vec_push(instance_layer->draw_instructions[ZEST_FIF], instance_layer->current_instruction);

        instance_layer->memory_refs[ZEST_FIF].write_to_buffer->memory_in_use += instance_layer->current_instruction.total_instances * instance_layer->instance_struct_size;
        instance_layer->current_instruction.total_instances = 0;
        instance_layer->current_instruction.start_index = 0;
    }
    else if (instance_layer->current_instruction.draw_mode == zest_draw_mode_viewport) {
        zest_vec_push(instance_layer->draw_instructions[ZEST_FIF], instance_layer->current_instruction);

        instance_layer->last_draw_mode = zest_draw_mode_none;
    }
}

void zest__end_mesh_instructions(zest_layer layer) {
    if (layer->current_instruction.total_indexes) {
        layer->last_draw_mode = zest_draw_mode_none;
        zest_vec_push(layer->draw_instructions[ZEST_FIF], layer->current_instruction);

        layer->memory_refs[ZEST_FIF].write_to_index_buffer->memory_in_use += layer->current_instruction.total_indexes * sizeof(zest_uint);
        layer->memory_refs[ZEST_FIF].write_to_buffer->memory_in_use += layer->current_instruction.total_indexes * layer->vertex_struct_size;
        layer->current_instruction.total_indexes = 0;
        layer->current_instruction.start_index = 0;
    }
    else if (layer->current_instruction.draw_mode == zest_draw_mode_viewport) {
        zest_vec_push(layer->draw_instructions[ZEST_FIF], layer->current_instruction);

        layer->last_draw_mode = zest_draw_mode_none;
    }
}

void zest__update_instance_layer_buffers_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer) {
    zest_layer instance_layer = (zest_layer)draw_routine->draw_data;
    zest_EndInstanceInstructions(instance_layer);

    if (ZEST__NOT_FLAGGED(instance_layer->flags, zest_layer_flag_device_local_direct)) {
        if (!zest_vec_empty(instance_layer->draw_instructions[ZEST_FIF])) {
            zest_AddCopyCommand(&instance_layer->vertex_upload, instance_layer->memory_refs[ZEST_FIF].staging_instance_data, instance_layer->memory_refs[ZEST_FIF].device_instance_data, 0);
        }

        zest_UploadBuffer(&instance_layer->vertex_upload, command_buffer);
    }

}

void zest__update_instance_layer_resolution(zest_layer layer) {
    layer->viewport.width = (float)zest_GetSwapChainExtent().width;
    layer->viewport.height = (float)zest_GetSwapChainExtent().height;
    layer->screen_scale.x = layer->viewport.width / layer->layer_size.x;
    layer->screen_scale.y = layer->viewport.height / layer->layer_size.y;
    layer->scissor.extent.width = (zest_uint)layer->viewport.width;
    layer->scissor.extent.height = (zest_uint)layer->viewport.height;
}

void zest_DrawInstanceLayer(zest_layer instance_layer, VkCommandBuffer command_buffer) {
    VkDeviceSize instance_data_offsets[] = { instance_layer->memory_refs[ZEST_FIF].device_instance_data->memory_offset };

    for (zest_foreach_i(instance_layer->draw_instructions[ZEST_FIF])) {
        zest_layer_instruction_t* current = &instance_layer->draw_instructions[ZEST_FIF][i];

        vkCmdSetViewport(command_buffer, 0, 1, &current->viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &current->scissor);

        vkCmdBindVertexBuffers(command_buffer, 0, 1, zest_GetBufferDeviceBuffer(instance_layer->memory_refs[ZEST_FIF].device_instance_data), instance_data_offsets);

        zest_BindPipelineCB(command_buffer, current->pipeline, current->descriptor_set);

        vkCmdPushConstants(
            command_buffer,
            current->pipeline->pipeline_layout,
            zest_PipelinePushConstantStageFlags(current->pipeline, 0),
            zest_PipelinePushConstantOffset(current->pipeline, 0),
            zest_PipelinePushConstantSize(current->pipeline, 0),
            &current->push_constants);

        vkCmdDraw(command_buffer, 6, current->total_instances, 0, current->start_index);
    }
}

void zest__draw_mesh_layer(zest_layer layer, VkCommandBuffer command_buffer) {
    zest_BindMeshIndexBuffer(layer);
    zest_BindMeshVertexBuffer(layer);

    for (zest_foreach_i(layer->draw_instructions[ZEST_FIF])) {
        zest_layer_instruction_t* current = &layer->draw_instructions[ZEST_FIF][i];

        vkCmdSetViewport(command_buffer, 0, 1, &current->viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &current->scissor);

        zest_BindPipelineCB(command_buffer, current->pipeline, current->descriptor_set);

        vkCmdPushConstants(
            command_buffer,
            current->pipeline->pipeline_layout,
            zest_PipelinePushConstantStageFlags(current->pipeline, 0),
            zest_PipelinePushConstantOffset(current->pipeline, 0),
            zest_PipelinePushConstantSize(current->pipeline, 0),
            &current->push_constants);

        vkCmdDrawIndexed(command_buffer, current->total_indexes, 1, current->start_index, 0, 0);
    }
}

void zest_DrawInstanceMeshLayer(zest_layer instance_layer, VkCommandBuffer command_buffer) {
    VkDeviceSize instance_data_offsets[] = { instance_layer->memory_refs[ZEST_FIF].device_instance_data->memory_offset };
	zest_BindMeshVertexBuffer(instance_layer);
	zest_BindMeshIndexBuffer(instance_layer);

    for (zest_foreach_i(instance_layer->draw_instructions[ZEST_FIF])) {
        zest_layer_instruction_t* current = &instance_layer->draw_instructions[ZEST_FIF][i];

        vkCmdSetViewport(command_buffer, 0, 1, &current->viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &current->scissor);

        vkCmdBindVertexBuffers(command_buffer, 1, 1, zest_GetBufferDeviceBuffer(instance_layer->memory_refs[ZEST_FIF].device_instance_data), instance_data_offsets);

        zest_BindPipelineCB(command_buffer, current->pipeline, current->descriptor_set);

        vkCmdPushConstants(
            command_buffer,
            current->pipeline->pipeline_layout,
            zest_PipelinePushConstantStageFlags(current->pipeline, 0),
            zest_PipelinePushConstantOffset(current->pipeline, 0),
            zest_PipelinePushConstantSize(current->pipeline, 0),
            &current->push_constants);

        vkCmdDrawIndexed(command_buffer, instance_layer->index_count, current->total_instances, 0, 0, current->start_index);
    }
}

//Start internal sprite layer functionality -----
void zest__draw_sprite_layer_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer) {
    zest_layer sprite_layer = (zest_layer)draw_routine->draw_data;
    zest_DrawInstanceLayer(sprite_layer, command_buffer);
    zest_ResetInstanceLayerDrawing(sprite_layer);
}

void zest__next_sprite_instance(zest_layer layer) {
    zest_sprite_instance_t* instance_ptr = (zest_sprite_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;
    instance_ptr = instance_ptr + 1;
    //Make sure we're not trying to write outside of the buffer range
    ZEST_ASSERT(instance_ptr >= (zest_sprite_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->data && instance_ptr <= (zest_sprite_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->end);
    if (instance_ptr == layer->memory_refs[ZEST_FIF].write_to_buffer->end) {
        zest_bool grown = 0;
        if (ZEST__FLAGGED(layer->flags, zest_layer_flag_device_local_direct)) {
            grown = zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_instance_data, sizeof(zest_sprite_instance_t), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].device_instance_data;
        }
        else {
            grown = zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].staging_instance_data, sizeof(zest_sprite_instance_t), 0);
            zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_instance_data, sizeof(zest_sprite_instance_t), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].staging_instance_data;
        }
        if (grown) {
            layer->memory_refs[ZEST_FIF].instance_count++;
            instance_ptr = (zest_sprite_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->data;
            instance_ptr += layer->memory_refs[ZEST_FIF].instance_count;
        }
        else {
            instance_ptr = instance_ptr - 1;
        }
    }
    else {
        layer->memory_refs[ZEST_FIF].instance_count++;
    }
    layer->memory_refs[ZEST_FIF].instance_ptr = instance_ptr;
}
// End internal sprite layer functionality -----

//Start internal sprite layer functionality -----
void zest__draw_font_layer_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer) {
    zest_layer font_layer = (zest_layer)draw_routine->draw_data;
    zest_DrawInstanceLayer(font_layer, command_buffer);
    zest_ResetInstanceLayerDrawing(font_layer);
}

void zest__next_font_instance(zest_layer layer) {
    zest_sprite_instance_t* instance_ptr = (zest_sprite_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;
    instance_ptr = instance_ptr + 1;
    //Make sure we're not trying to write outside of the buffer range
    ZEST_ASSERT(instance_ptr >= (zest_sprite_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->data && instance_ptr <= (zest_sprite_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->end);
    if (instance_ptr == layer->memory_refs[ZEST_FIF].write_to_buffer->end) {
        zest_bool grown = 0;
        if (ZEST__FLAGGED(layer->flags, zest_layer_flag_device_local_direct)) {
            grown = zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_instance_data, sizeof(zest_sprite_instance_t), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].device_instance_data;
        }
        else {
            grown = zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].staging_instance_data, sizeof(zest_sprite_instance_t), 0);
            zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_instance_data, sizeof(zest_sprite_instance_t), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].staging_instance_data;
        }
        if (grown) {
            layer->memory_refs[ZEST_FIF].instance_count++;
            instance_ptr = (zest_sprite_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->data;
            instance_ptr += layer->memory_refs[ZEST_FIF].instance_count;
        }
        else {
            instance_ptr = instance_ptr - 1;
        }
    }
    else {
        layer->memory_refs[ZEST_FIF].instance_count++;
    }
    layer->memory_refs[ZEST_FIF].instance_ptr = instance_ptr;
}
// End internal sprite layer functionality -----

//Start internal 3d billboard layer functionality -----
void zest__draw_billboard_layer_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer) {
    zest_layer billboard_layer = (zest_layer)draw_routine->draw_data;
    zest_DrawInstanceLayer(billboard_layer, command_buffer);
    zest_ResetInstanceLayerDrawing(billboard_layer);
}

void zest__next_billboard_instance(zest_layer layer) {
    zest_billboard_instance_t* instance_ptr = (zest_billboard_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;
    instance_ptr = instance_ptr + 1;
    //Make sure we're not trying to write outside of the buffer range
    ZEST_ASSERT(instance_ptr >= (zest_billboard_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->data && instance_ptr <= (zest_billboard_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->end);
    if (instance_ptr == layer->memory_refs[ZEST_FIF].write_to_buffer->end) {
        zest_bool grown = 0;
        if (ZEST__FLAGGED(layer->flags, zest_layer_flag_device_local_direct)) {
            grown = zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_instance_data, sizeof(zest_billboard_instance_t), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].device_instance_data;
        }
        else {
            grown = zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].staging_instance_data, sizeof(zest_billboard_instance_t), 0);
            zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_instance_data, sizeof(zest_billboard_instance_t), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].staging_instance_data;
        }
        if (grown) {
            layer->memory_refs[ZEST_FIF].instance_count++;
            instance_ptr = (zest_billboard_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->data;
            instance_ptr += layer->memory_refs[ZEST_FIF].instance_count;
        }
        else {
            instance_ptr = instance_ptr - 1;
        }
    }
    else {
        layer->memory_refs[ZEST_FIF].instance_count++;
    }
    layer->memory_refs[ZEST_FIF].instance_ptr = instance_ptr;
}
// End internal 3d billboard layer functionality -----

//Start internal shape instance layer functionality -----
void zest__draw_shape_layer_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer) {
    zest_layer line_layer = (zest_layer)draw_routine->draw_data;
    zest_DrawInstanceLayer(line_layer, command_buffer);
    zest_ResetInstanceLayerDrawing(line_layer);
}

void zest__next_shape_instance(zest_layer layer) {
    zest_shape_instance_t* instance_ptr = (zest_shape_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;
    instance_ptr = instance_ptr + 1;
    //Make sure we're not trying to write outside of the buffer range
    ZEST_ASSERT(instance_ptr >= (zest_shape_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->data && instance_ptr <= (zest_shape_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->end);
    if (instance_ptr == layer->memory_refs[ZEST_FIF].write_to_buffer->end) {
        zest_bool grown = 0;
        if (ZEST__FLAGGED(layer->flags, zest_layer_flag_device_local_direct)) {
            grown = zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_instance_data, sizeof(zest_shape_instance_t), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].device_instance_data;
        }
        else {
            grown = zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].staging_instance_data, sizeof(zest_shape_instance_t), 0);
            zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_instance_data, sizeof(zest_shape_instance_t), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].staging_instance_data;
        }
        if (grown) {
            layer->memory_refs[ZEST_FIF].instance_count++;
            instance_ptr = (zest_shape_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->data;
            instance_ptr += layer->memory_refs[ZEST_FIF].instance_count;
        }
        else {
            instance_ptr = instance_ptr - 1;
        }
    }
    else {
        layer->memory_refs[ZEST_FIF].instance_count++;
    }
    layer->memory_refs[ZEST_FIF].instance_ptr = instance_ptr;
}
// End internal shape layer functionality -----

//Start internal 3dline instance layer functionality -----
void zest__draw_3dline_layer_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer) {
    zest_layer line_layer = (zest_layer)draw_routine->draw_data;
    zest_DrawInstanceLayer(line_layer, command_buffer);
    zest_ResetInstanceLayerDrawing(line_layer);
}

void zest__next_3dline_instance(zest_layer layer) {
    zest_line_instance_t* instance_ptr = (zest_line_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;
    instance_ptr = instance_ptr + 1;
    //Make sure we're not trying to write outside of the buffer range
    ZEST_ASSERT(instance_ptr >= (zest_line_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->data && instance_ptr <= (zest_line_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->end);
    if (instance_ptr == layer->memory_refs[ZEST_FIF].write_to_buffer->end) {
        zest_bool grown = 0;
        if (ZEST__FLAGGED(layer->flags, zest_layer_flag_device_local_direct)) {
            grown = zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_instance_data, sizeof(zest_line_instance_t), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].device_instance_data;
        }
        else {
            grown = zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].staging_instance_data, sizeof(zest_line_instance_t), 0);
            zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_instance_data, sizeof(zest_line_instance_t), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].staging_instance_data;
        }
        if (grown) {
            layer->memory_refs[ZEST_FIF].instance_count++;
            instance_ptr = (zest_line_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->data;
            instance_ptr += layer->memory_refs[ZEST_FIF].instance_count;
        }
        else {
            instance_ptr = instance_ptr - 1;
        }
    }
    else {
        layer->memory_refs[ZEST_FIF].instance_count++;
    }
    layer->memory_refs[ZEST_FIF].instance_ptr = instance_ptr;
}
// End internal shape layer functionality -----

//Start internal mesh layer functionality -----
void zest__draw_mesh_layer_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer) {
    zest_layer layer = (zest_layer)draw_routine->draw_data;
    zest__draw_mesh_layer(layer, command_buffer);
    zest__reset_mesh_layer_drawing(layer);
}
// End internal mesh layer functionality -----

//Start internal instanced mesh layer functionality -----
void zest__draw_instance_mesh_layer_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer) {
    zest_layer layer = (zest_layer)draw_routine->draw_data;
    zest_DrawInstanceMeshLayer(layer, command_buffer);
    zest_ResetInstanceLayerDrawing(layer);
}

void zest__next_mesh_instance(zest_layer layer) {
    zest_mesh_instance_t* instance_ptr = (zest_mesh_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;
    instance_ptr = instance_ptr + 1;
    //Make sure we're not trying to write outside of the buffer range
    ZEST_ASSERT(instance_ptr >= (zest_mesh_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->data && instance_ptr <= (zest_mesh_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->end);
    if (instance_ptr == layer->memory_refs[ZEST_FIF].write_to_buffer->end) {
        zest_bool grown = 0;
        if (ZEST__FLAGGED(layer->flags, zest_layer_flag_device_local_direct)) {
            grown = zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_instance_data, sizeof(zest_mesh_instance_t), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].device_instance_data;
        }
        else {
            grown = zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].staging_instance_data, sizeof(zest_mesh_instance_t), 0);
            zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_instance_data, sizeof(zest_mesh_instance_t), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].staging_instance_data;
        }
        if (grown) {
            layer->memory_refs[ZEST_FIF].instance_count++;
            instance_ptr = (zest_mesh_instance_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->data;
            instance_ptr += layer->memory_refs[ZEST_FIF].instance_count;
        }
        else {
            instance_ptr = instance_ptr - 1;
        }
    }
    else {
        layer->memory_refs[ZEST_FIF].instance_count++;
    }
    layer->memory_refs[ZEST_FIF].instance_ptr = instance_ptr;
}
// End internal instanced mesh layer functionality -----

//-- Draw Layers API
zest_layer zest_NewLayer() {
    zest_layer_t blank_layer = { 0 };
    zest_layer layer = ZEST__NEW_ALIGNED(zest_layer, 16);
    *layer = blank_layer;
    ZEST_ASSERT(sizeof(*layer) == sizeof(blank_layer));
    return layer;
}

void zest_SetLayerViewPort(zest_layer instance_layer, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height) {
    instance_layer->scissor = zest_CreateRect2D(scissor_width, scissor_height, x, y);
    instance_layer->viewport = zest_CreateViewport((float)x, (float)y, viewport_width, viewport_height, 0.f, 1.f);
}

void zest_SetLayerScissor(zest_layer instance_layer, int offset_x, int offset_y, zest_uint scissor_width, zest_uint scissor_height) {
    instance_layer->scissor = zest_CreateRect2D(scissor_width, scissor_height, offset_x, offset_y);
}

void zest_SetLayerSize(zest_layer layer, float width, float height) {
    layer->layer_size.x = width;
    layer->layer_size.y = height;
}

void zest_SetLayerScale(zest_layer layer, float x, float y) {
    layer->screen_scale.x = x;
    layer->screen_scale.y = y;
    layer->push_constants.model = zest_M4(1.f);
    zest_vec4 scale = zest_Vec4Set(x, y, 1.f, 1.f);
    layer->push_constants.model = zest_ScaleMatrix4x4(&layer->push_constants.model, &scale);
}

void zest_SetLayerColor(zest_layer layer, zest_byte red, zest_byte green, zest_byte blue, zest_byte alpha) {
    layer->current_color = zest_ColorSet(red, green, blue, alpha);
}

void zest_SetLayerColorf(zest_layer layer, float red, float green, float blue, float alpha) {
    layer->current_color = zest_ColorSet((zest_byte)(red * 255.f), (zest_byte)(green * 255.f), (zest_byte)(blue * 255.f), (zest_byte)(alpha * 255.f));
}

void zest_SetLayerIntensity(zest_layer layer, float value) {
    layer->intensity = value;
}

void zest_SetLayerDirty(zest_layer layer) {
    for (ZEST_EACH_FIF_i) {
        layer->dirty[i] = 1;
    }
}
//-- End Draw Layers

//-- Start Sprite Drawing API
void zest__initialise_sprite_layer(zest_layer sprite_layer, zest_uint instance_pool_size) {
    sprite_layer->current_color.r = 255;
    sprite_layer->current_color.g = 255;
    sprite_layer->current_color.b = 255;
    sprite_layer->current_color.a = 255;
    sprite_layer->intensity = 1;
    sprite_layer->push_constants.model = zest_M4(1);
    sprite_layer->push_constants.parameters1 = zest_Vec4Set1(0.f);
    sprite_layer->push_constants.parameters2.x = 0.f;
    sprite_layer->push_constants.parameters2.y = 0.f;
    sprite_layer->push_constants.parameters2.z = 0.25f;
    sprite_layer->push_constants.parameters2.w = 0.5f;
    sprite_layer->layer_size = zest_Vec2Set1(1.f);
    sprite_layer->instance_struct_size = sizeof(zest_sprite_instance_t);

    zest_buffer_info_t device_buffer_info = zest_CreateVertexBufferInfo(0);
    if (zest_GPUHasDeviceLocalHostVisible(sizeof(zest_sprite_instance_t) * instance_pool_size)) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating Device local buffers for sprite layer" ZEST_NL);
        ZEST__FLAG(sprite_layer->flags, zest_layer_flag_device_local_direct);
        device_buffer_info = zest_CreateVertexBufferInfo(ZEST_TRUE);
    }

    zest_buffer_info_t staging_buffer_info = zest_CreateStagingBufferInfo();
    for (ZEST_EACH_FIF_i) {
        sprite_layer->memory_refs[i].device_instance_data = zest_CreateBuffer(sizeof(zest_sprite_instance_t) * instance_pool_size, &device_buffer_info, ZEST_NULL);
        if (ZEST__NOT_FLAGGED(sprite_layer->flags, zest_layer_flag_device_local_direct)) {
            sprite_layer->memory_refs[i].staging_instance_data = zest_CreateBuffer(sizeof(zest_sprite_instance_t) * instance_pool_size, &staging_buffer_info, ZEST_NULL);
            sprite_layer->memory_refs[i].instance_ptr = sprite_layer->memory_refs[i].staging_instance_data->data;
            sprite_layer->memory_refs[i].write_to_buffer = sprite_layer->memory_refs[i].staging_instance_data;
        }
        else {
            sprite_layer->memory_refs[i].instance_ptr = sprite_layer->memory_refs[i].device_instance_data->data;
            sprite_layer->memory_refs[i].write_to_buffer = sprite_layer->memory_refs[i].device_instance_data;
        }
        sprite_layer->memory_refs[i].instance_count = 0;
        sprite_layer->memory_refs[i].instance_count = 0;
    }

    zest_SetLayerViewPort(sprite_layer, 0, 0, zest_SwapChainWidth(), zest_SwapChainHeight(), zest_SwapChainWidthf(), zest_SwapChainHeightf());

    zest_ResetInstanceLayerDrawing(sprite_layer);
}

void zest_SetSpriteDrawing(zest_layer sprite_layer, zest_texture texture, zest_descriptor_set descriptor_set, zest_pipeline pipeline) {
    zest_EndInstanceInstructions(sprite_layer);
    zest_StartInstanceInstructions(sprite_layer);
    sprite_layer->current_instruction.pipeline = pipeline;
    if (descriptor_set) {
        sprite_layer->current_instruction.descriptor_set = descriptor_set->descriptor_set[ZEST_FIF];
    }
    else {
        sprite_layer->current_instruction.descriptor_set = texture->current_descriptor_set[ZEST_FIF];
    }
    sprite_layer->current_instruction.draw_mode = zest_draw_mode_instance;
    sprite_layer->current_instruction.scissor = sprite_layer->scissor;
    sprite_layer->current_instruction.viewport = sprite_layer->viewport;
    sprite_layer->last_draw_mode = zest_draw_mode_instance;
}

void zest_DrawSprite(zest_layer layer, zest_image image, float x, float y, float r, float sx, float sy, float hx, float hy, zest_uint alignment, float stretch) {
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_instance);    //Call zest_StartSpriteDrawing before calling this function

    zest_sprite_instance_t* sprite = (zest_sprite_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;

    sprite->size = zest_Vec2Set(sx, sy);
    sprite->handle = zest_Vec2Set(hx, hy);
    sprite->position_rotation = zest_Vec4Set(x, y, stretch, r);
    sprite->uv = image->uv;
    sprite->intensity = layer->intensity;
    sprite->alignment = alignment;
    sprite->color = layer->current_color;
    sprite->image_layer_index = image->layer;
    layer->current_instruction.total_instances++;

    zest__next_sprite_instance(layer);
}

void zest_DrawTexturedSprite(zest_layer layer, zest_image image, float x, float y, float width, float height, float scale_x, float scale_y, float offset_x, float offset_y) {
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_instance);    //Call zest_StartSpriteDrawing before calling this function

    zest_sprite_instance_t* sprite = (zest_sprite_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;

    sprite->uv = zest_Vec4Set(0.f, 0.f, 1.f, 1.f);
    if (offset_x || offset_y) {
        offset_x = fmodf(offset_x, (float)image->width) / image->width;
        offset_y = fmodf(offset_y, (float)image->height) / image->height;
        offset_x *= scale_x;
        offset_y *= scale_y;
    }
    scale_x *= width / (float)image->width;
    scale_y *= height / (float)image->height;
    sprite->uv.z = (sprite->uv.z * scale_x) - offset_x;
    sprite->uv.w = (sprite->uv.w * scale_y) - offset_y;
    sprite->uv.x -= offset_x;
    sprite->uv.y -= offset_y;

    sprite->size = zest_Vec2Set(width, height);
    sprite->handle = zest_Vec2Set(0.f, 0.f);
    sprite->position_rotation = zest_Vec4Set(x, y, 0.f, 0.f);
    sprite->intensity = layer->intensity;
    sprite->alignment = 0;
    sprite->color = layer->current_color;
    sprite->image_layer_index = (image->layer << 24);
    layer->current_instruction.total_instances++;

    zest__next_sprite_instance(layer);
}
//-- End Sprite Drawing API

//-- Start Font Drawing API
void zest__initialise_font_layer(zest_layer font_layer, zest_uint instance_pool_size) {
    font_layer->current_color.r = 255;
    font_layer->current_color.g = 255;
    font_layer->current_color.b = 255;
    font_layer->current_color.a = 255;
    font_layer->intensity = 1;
    font_layer->push_constants.model = zest_M4(1);
    font_layer->push_constants.parameters1 = zest_Vec4Set1(0.f);
    font_layer->push_constants.parameters2.x = 1.f;        //Shadow Vector
    font_layer->push_constants.parameters2.y = 1.f;
    font_layer->push_constants.parameters2.z = 0.2f;    //Shadow Smoothing
    font_layer->push_constants.parameters2.w = 0.f;        //Shadow Clipping

    //Font defaults.
    font_layer->push_constants.parameters1.x = 25.f;    //Radius
    font_layer->push_constants.parameters1.y = 0.25f;    //Bleed
    font_layer->push_constants.parameters1.z = 5.f;        //AA factor
    font_layer->push_constants.parameters1.w = 5.5f;    //Thickness

    font_layer->layer_size = zest_Vec2Set1(1.f);
    font_layer->instance_struct_size = sizeof(zest_sprite_instance_t);

    zest_buffer_info_t device_buffer_info = zest_CreateVertexBufferInfo(0);
    if (zest_GPUHasDeviceLocalHostVisible(sizeof(zest_sprite_instance_t) * instance_pool_size)) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating Device local buffers for font layer" ZEST_NL);
        ZEST__FLAG(font_layer->flags, zest_layer_flag_device_local_direct);
        device_buffer_info = zest_CreateVertexBufferInfo(ZEST_TRUE);
    }

    zest_buffer_info_t staging_buffer_info = zest_CreateStagingBufferInfo();
    for (ZEST_EACH_FIF_i) {
        font_layer->memory_refs[i].device_instance_data = zest_CreateBuffer(sizeof(zest_sprite_instance_t) * instance_pool_size, &device_buffer_info, ZEST_NULL);
        if (ZEST__NOT_FLAGGED(font_layer->flags, zest_layer_flag_device_local_direct)) {
            font_layer->memory_refs[i].staging_instance_data = zest_CreateBuffer(sizeof(zest_sprite_instance_t) * instance_pool_size, &staging_buffer_info, ZEST_NULL);
            font_layer->memory_refs[i].instance_ptr = font_layer->memory_refs[i].staging_instance_data->data;
            font_layer->memory_refs[i].write_to_buffer = font_layer->memory_refs[i].staging_instance_data;
        }
        else {
            font_layer->memory_refs[i].instance_ptr = font_layer->memory_refs[i].device_instance_data->data;
            font_layer->memory_refs[i].write_to_buffer = font_layer->memory_refs[i].device_instance_data;
        }
        font_layer->memory_refs[i].instance_count = 0;
        font_layer->memory_refs[i].instance_count = 0;
    }

    zest_SetLayerViewPort(font_layer, 0, 0, zest_SwapChainWidth(), zest_SwapChainHeight(), zest_SwapChainWidthf(), zest_SwapChainHeightf());

    zest_ResetInstanceLayerDrawing(font_layer);
}

void zest_SetMSDFFontDrawing(zest_layer font_layer, zest_font font) {
    zest_EndInstanceInstructions(font_layer);
    zest_StartInstanceInstructions(font_layer);
    ZEST_ASSERT(ZEST__FLAGGED(font->texture->flags, zest_texture_flag_textures_ready));        //Make sure the font is properly loaded or wasn't recently deleted
    font_layer->current_instruction.pipeline = font->pipeline;
    if (font->descriptor_set) {
        font_layer->current_instruction.descriptor_set = font->descriptor_set->descriptor_set[ZEST_FIF];
    }
    else {
        font_layer->current_instruction.descriptor_set = font->texture->current_descriptor_set[ZEST_FIF];
    }
    font_layer->current_instruction.draw_mode = zest_draw_mode_text;
    font_layer->current_instruction.asset = font;
    font_layer->current_instruction.scissor = font_layer->scissor;
    font_layer->current_instruction.viewport = font_layer->viewport;
    font_layer->current_instruction.push_constants = font_layer->push_constants;
    font_layer->last_draw_mode = zest_draw_mode_text;
}

void zest_SetMSDFFontShadow(zest_layer font_layer, float shadow_length, float shadow_smoothing, float shadow_clipping) {
    zest_vec2 shadow = zest_NormalizeVec2(zest_Vec2Set(1.f, 1.f));
    font_layer->push_constants.parameters2.x = shadow.x * shadow_length;
    font_layer->push_constants.parameters2.y = shadow.y * shadow_length;
    font_layer->push_constants.parameters2.z = shadow_smoothing;
    font_layer->push_constants.parameters2.w = shadow_clipping;
    font_layer->current_instruction.push_constants.parameters2 = font_layer->push_constants.parameters2;
}

void zest_SetMSDFFontShadowColor(zest_layer font_layer, float red, float green, float blue, float alpha) {
    font_layer->push_constants.parameters3 = zest_Vec4Set(red, green, blue, alpha);
    font_layer->current_instruction.push_constants.parameters3 = font_layer->push_constants.parameters3;
}

void zest_TweakMSDFFont(zest_layer font_layer, float bleed, float thickness, float aa_factor, float radius) {
    font_layer->push_constants.parameters1.x = radius;
    font_layer->push_constants.parameters1.y = bleed;
    font_layer->push_constants.parameters1.z = aa_factor;
    font_layer->push_constants.parameters1.w = thickness;
    font_layer->current_instruction.push_constants.parameters1 = font_layer->push_constants.parameters1;
    font_layer->current_instruction.push_constants.camera.w = bleed;
}

void zest_SetMSDFFontBleed(zest_layer font_layer, float bleed) {
    font_layer->push_constants.parameters1.y = bleed;
    font_layer->current_instruction.push_constants.camera.w = bleed;
}

void zest_SetMSDFFontThickness(zest_layer font_layer, float thickness) {
    font_layer->push_constants.parameters1.w = thickness;
    font_layer->current_instruction.push_constants.parameters1.w = thickness;
}

void zest_SetMSDFFontAAFactor(zest_layer font_layer, float aa_factor) {
    font_layer->push_constants.parameters1.z = aa_factor;
    font_layer->current_instruction.push_constants.parameters1.z = aa_factor;
}

void zest_SetMSDFFontRadius(zest_layer font_layer, float radius) {
    font_layer->push_constants.parameters1.x = radius;
    font_layer->current_instruction.push_constants.parameters1.x = radius;
}

void zest_SetMSDFFontDetail(zest_layer font_layer, float detail) {
    font_layer->push_constants.parameters1.y = detail;
    font_layer->current_instruction.push_constants.parameters1.y = detail;
}

float zest_DrawMSDFText(zest_layer font_layer, const char* text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing) {
    ZEST_ASSERT(font_layer->current_instruction.draw_mode == zest_draw_mode_text);        //Call zest_StartFontDrawing before calling this function

    zest_font font = (zest_font)(font_layer->current_instruction.asset);

    size_t length = strlen(text);
    if (length <= 0) {
        return 0;
    }

    float scaled_line_height = font->font_size * size;
    float scaled_offset = font->max_yoffset * size;
    x -= zest_TextWidth(font, text, size, letter_spacing) * handle_x;
    y -= (scaled_line_height * handle_y) - scaled_offset;

    float xpos = x;

    for (int i = 0; i != length; ++i) {
        zest_sprite_instance_t* font_instance = (zest_sprite_instance_t*)font_layer->memory_refs[ZEST_FIF].instance_ptr;
        zest_font_character_t* character = &font->characters[text[i]];

        if (character->flags > 0) {
            xpos += character->xadvance * size + letter_spacing;
            continue;
        }

        float width = character->width * size;
        float height = character->height * size;
        float xoffset = character->xoffset * size;
        float yoffset = character->yoffset * size;

        float uv_width = font->texture->texture.width * (character->uv.z - character->uv.x);
        float uv_height = font->texture->texture.height * (character->uv.y - character->uv.w);
        float scale = width / uv_width;

        font_instance->size = zest_Vec2Set(width, height);
        font_instance->handle = zest_Vec2Set1(0.f);
        font_instance->position_rotation = zest_Vec4Set(xpos + xoffset, y + yoffset, 0.f, 0.f);
        font_instance->uv = character->uv;
        font_instance->intensity = font_layer->intensity;
        font_instance->alignment = 1;
        font_instance->color = font_layer->current_color;
        font_instance->image_layer_index = 0;
        font_layer->current_instruction.total_instances++;

        zest__next_font_instance(font_layer);

        xpos += character->xadvance * size + letter_spacing;
    }

    return xpos;
}

float zest_DrawMSDFParagraph(zest_layer font_layer, const char* text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing, float line_height) {
    ZEST_ASSERT(font_layer->current_instruction.draw_mode == zest_draw_mode_text);        //Call zest_StartFontDrawing before calling this function

    zest_font font = (zest_font)(font_layer->current_instruction.asset);

    size_t length = strlen(text);
    if (length <= 0) {
        return 0;
    }

    float scaled_line_height = line_height * font->font_size * size;
    float scaled_offset = font->max_yoffset * size;
    float paragraph_height = scaled_line_height * (handle_y);
    for (int i = 0; i != length; ++i) {
        zest_font_character_t* character = &font->characters[text[i]];
        if (character->flags == zest_character_flag_new_line) {
            paragraph_height += scaled_line_height;
        }
    }

    x -= zest_TextWidth(font, text, size, letter_spacing) * handle_x;
    y -= (paragraph_height * handle_y) - scaled_offset;

    float xpos = x;

    for (int i = 0; i != length; ++i) {
        zest_sprite_instance_t* font_instance = (zest_sprite_instance_t*)font_layer->memory_refs[ZEST_FIF].instance_ptr;
        zest_font_character_t* character = &font->characters[text[i]];

        if (character->flags == zest_character_flag_skip) {
            xpos += character->xadvance * size + letter_spacing;
            continue;
        }
        else if (character->flags == zest_character_flag_new_line) {
            y += scaled_line_height;
            xpos = x;
            continue;
        }

        float width = character->width * size;
        float height = character->height * size;
        float xoffset = character->xoffset * size;
        float yoffset = character->yoffset * size;

        float uv_width = font->texture->texture.width * (character->uv.z - character->uv.x);
        float uv_height = font->texture->texture.height * (character->uv.y - character->uv.w);
        float scale = width / uv_width;

        font_instance->size = zest_Vec2Set(width, height);
        font_instance->handle = zest_Vec2Set1(0.f);
        font_instance->position_rotation = zest_Vec4Set(xpos + xoffset, y + yoffset, 0.f, 0.f);
        font_instance->uv = character->uv;
        font_instance->intensity = font_layer->intensity;
        font_instance->alignment = 1;
        font_instance->color = font_layer->current_color;
        font_instance->image_layer_index = 0;
        font_layer->current_instruction.total_instances++;

        zest__next_font_instance(font_layer);

        xpos += character->xadvance * size + letter_spacing;
    }

    return xpos;
}

float zest_TextWidth(zest_font font, const char* text, float font_size, float letter_spacing) {

    float width = 0;
    float max_width = 0;

    size_t length = strlen(text);

    for (int i = 0; i != length; ++i) {
        zest_font_character_t* character = &font->characters[text[i]];

        if (character->flags == zest_character_flag_new_line) {
            width = 0;
        }

        width += character->xadvance * font_size + letter_spacing;
        max_width = ZEST__MAX(width, max_width);
    }

    return max_width;
}
//-- End Font Drawing API

//-- Start Billboard Drawing API
void zest__initialise_billboard_layer(zest_layer billboard_layer, zest_uint instance_pool_size) {
    billboard_layer->current_color.r = 255;
    billboard_layer->current_color.g = 255;
    billboard_layer->current_color.b = 255;
    billboard_layer->current_color.a = 255;
    billboard_layer->intensity = 1;
    billboard_layer->push_constants.model = zest_M4(1);
    billboard_layer->push_constants.parameters1 = zest_Vec4Set1(0.f);
    billboard_layer->push_constants.parameters2.x = 0.f;
    billboard_layer->push_constants.parameters2.y = 0.f;
    billboard_layer->push_constants.parameters2.z = 0.25f;
    billboard_layer->push_constants.parameters2.w = 0.5f;
    billboard_layer->layer_size = zest_Vec2Set1(1.f);
    billboard_layer->instance_struct_size = sizeof(zest_billboard_instance_t);

    zest_buffer_info_t device_buffer_info = zest_CreateVertexBufferInfo(0);
    if (zest_GPUHasDeviceLocalHostVisible(sizeof(zest_billboard_instance_t) * instance_pool_size)) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating Device local buffers for billboard layer" ZEST_NL);
        ZEST__FLAG(billboard_layer->flags, zest_layer_flag_device_local_direct);
        device_buffer_info = zest_CreateVertexBufferInfo(ZEST_TRUE);
    }

    zest_buffer_info_t staging_buffer_info = zest_CreateStagingBufferInfo();
    for (ZEST_EACH_FIF_i) {
        billboard_layer->memory_refs[i].device_instance_data = zest_CreateBuffer(sizeof(zest_billboard_instance_t) * instance_pool_size, &device_buffer_info, ZEST_NULL);
        billboard_layer->memory_refs[i].instance_count = 0;
        billboard_layer->memory_refs[i].instance_count = 0;
        if (ZEST__NOT_FLAGGED(billboard_layer->flags, zest_layer_flag_device_local_direct)) {
            billboard_layer->memory_refs[i].staging_instance_data = zest_CreateBuffer(sizeof(zest_billboard_instance_t) * instance_pool_size, &staging_buffer_info, ZEST_NULL);
            billboard_layer->memory_refs[i].instance_ptr = billboard_layer->memory_refs[i].staging_instance_data->data;
            billboard_layer->memory_refs[i].write_to_buffer = billboard_layer->memory_refs[i].staging_instance_data;
        }
        else {
            billboard_layer->memory_refs[i].instance_ptr = billboard_layer->memory_refs[i].device_instance_data->data;
            billboard_layer->memory_refs[i].write_to_buffer = billboard_layer->memory_refs[i].device_instance_data;
        }
    }

    zest_SetLayerViewPort(billboard_layer, 0, 0, zest_SwapChainWidth(), zest_SwapChainHeight(), zest_SwapChainWidthf(), zest_SwapChainHeightf());

    zest_ResetInstanceLayerDrawing(billboard_layer);
}

void zest_SetBillboardDrawing(zest_layer billboard_layer, zest_texture texture, zest_descriptor_set descriptor_set, zest_pipeline pipeline) {
    zest_EndInstanceInstructions(billboard_layer);
    zest_StartInstanceInstructions(billboard_layer);
    billboard_layer->current_instruction.pipeline = pipeline;
    if (descriptor_set) {
        billboard_layer->current_instruction.descriptor_set = descriptor_set->descriptor_set[ZEST_FIF];
    }
    else {
        billboard_layer->current_instruction.descriptor_set = texture->current_descriptor_set[ZEST_FIF];
    }
    billboard_layer->current_instruction.draw_mode = zest_draw_mode_instance;
    billboard_layer->current_instruction.scissor = billboard_layer->scissor;
    billboard_layer->current_instruction.viewport = billboard_layer->viewport;
    billboard_layer->last_draw_mode = zest_draw_mode_instance;
}

void zest_DrawBillboard(zest_layer layer, zest_image image, float position[3], zest_uint alignment, float angles[3], float handle[2], float stretch, zest_uint alignment_type, float sx, float sy) {
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_instance);    //Call zest_StartSpriteDrawing before calling this function

    zest_billboard_instance_t* billboard = (zest_billboard_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;

    billboard->scale = zest_Vec2Set(sx, sy);
    billboard->position = zest_Vec3Set(position[0], position[1], position[2]);
    billboard->rotations = zest_Vec3Set(angles[0], angles[1], angles[2]);
    billboard->uv_xy = image->uv_xy;
    billboard->uv_zw = image->uv_zw;
    billboard->handle = zest_Vec2Set(handle[0], handle[1]);
    billboard->stretch = stretch;
    billboard->alignment = alignment;
    billboard->color = layer->current_color;
    billboard->blend_texture_array = (image->layer << 24) + (alignment_type << 22) + (zest_uint)(layer->intensity * 0.125f * 4194303.f);
    layer->current_instruction.total_instances++;

    zest__next_billboard_instance(layer);
}

void zest_DrawBillboardSimple(zest_layer layer, zest_image image, float position[3], float angle, float sx, float sy) {
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_instance);        //Call zest_StartSpriteDrawing before calling this function

    zest_billboard_instance_t* billboard = (zest_billboard_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;

    billboard->scale = zest_Vec2Set(sx, sy);
    billboard->position = zest_Vec3Set(position[0], position[1], position[2]);
    billboard->rotations = zest_Vec3Set(0.f, 0.f, angle);
    billboard->uv_xy = image->uv_xy;
    billboard->uv_zw = image->uv_zw;
    billboard->handle = zest_Vec2Set(0.5f, 0.5f);
    billboard->stretch = 0;
    billboard->alignment = 16711680;        //x = 0.f, y = 1.f, z = 0.f;
    billboard->color = layer->current_color;
    billboard->blend_texture_array = (image->layer << 24) + (zest_uint)(layer->intensity * 0.125f * 4194303.f);
    layer->current_instruction.total_instances++;

    zest__next_billboard_instance(layer);
}
//-- End Billboard Drawing API

//-- Start Line Drawing API
void zest__initialise_shape_layer(zest_layer line_layer, zest_uint instance_pool_size) {
    line_layer->current_color.r = 255;
    line_layer->current_color.g = 255;
    line_layer->current_color.b = 255;
    line_layer->current_color.a = 255;
    line_layer->intensity = 1;
    line_layer->push_constants.model = zest_M4(1);
    line_layer->push_constants.parameters1 = zest_Vec4Set1(0.f);
    line_layer->push_constants.parameters2.x = 0.f;
    line_layer->push_constants.parameters2.y = 0.f;
    line_layer->push_constants.parameters2.z = 0.25f;
    line_layer->push_constants.parameters2.w = 0.5f;
    line_layer->layer_size = zest_Vec2Set1(1.f);
    line_layer->instance_struct_size = sizeof(zest_shape_instance_t);

    zest_buffer_info_t device_buffer_info = zest_CreateVertexBufferInfo(0);
    if (zest_GPUHasDeviceLocalHostVisible(sizeof(zest_shape_instance_t) * instance_pool_size)) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating Device local buffers for shape layer" ZEST_NL);
        ZEST__FLAG(line_layer->flags, zest_layer_flag_device_local_direct);
        device_buffer_info = zest_CreateVertexBufferInfo(ZEST_TRUE);
    }

    zest_buffer_info_t staging_buffer_info = zest_CreateStagingBufferInfo();
    for (ZEST_EACH_FIF_i) {
        line_layer->memory_refs[i].device_instance_data = zest_CreateBuffer(sizeof(zest_shape_instance_t) * instance_pool_size, &device_buffer_info, ZEST_NULL);
        line_layer->memory_refs[i].instance_count = 0;
        line_layer->memory_refs[i].instance_count = 0;
        if (ZEST__NOT_FLAGGED(line_layer->flags, zest_layer_flag_device_local_direct)) {
            line_layer->memory_refs[i].staging_instance_data = zest_CreateBuffer(sizeof(zest_shape_instance_t) * instance_pool_size, &staging_buffer_info, ZEST_NULL);
            line_layer->memory_refs[i].instance_ptr = line_layer->memory_refs[i].staging_instance_data->data;
            line_layer->memory_refs[i].write_to_buffer = line_layer->memory_refs[i].staging_instance_data;
        }
        else {
            line_layer->memory_refs[i].instance_ptr = line_layer->memory_refs[i].device_instance_data->data;
            line_layer->memory_refs[i].write_to_buffer = line_layer->memory_refs[i].device_instance_data;
        }
    }

    zest_SetLayerViewPort(line_layer, 0, 0, zest_SwapChainWidth(), zest_SwapChainHeight(), zest_SwapChainWidthf(), zest_SwapChainHeightf());

    zest_ResetInstanceLayerDrawing(line_layer);
}

void zest_SetShapeDrawing(zest_layer line_layer, zest_shape_type shape_type, zest_descriptor_set descriptor_set, zest_pipeline pipeline) {
    zest_EndInstanceInstructions(line_layer);
    zest_StartInstanceInstructions(line_layer);
    line_layer->current_instruction.pipeline = pipeline;
    if (descriptor_set) {
        line_layer->current_instruction.descriptor_set = descriptor_set->descriptor_set[ZEST_FIF];
    }
    else {
        line_layer->current_instruction.descriptor_set = pipeline->descriptor_set[ZEST_FIF];
    }
    line_layer->current_instruction.scissor = line_layer->scissor;
    line_layer->current_instruction.viewport = line_layer->viewport;
    line_layer->current_instruction.push_constants.parameters1.x = shape_type;
    line_layer->current_instruction.draw_mode = shape_type;
    line_layer->last_draw_mode = shape_type;
}

void zest_DrawLine(zest_layer layer, float start_point[2], float end_point[2], float width) {
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_line_instance || layer->current_instruction.draw_mode == zest_draw_mode_dashed_line);    //Call zest_StartSpriteDrawing before calling this function

    zest_shape_instance_t* line = (zest_shape_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;

    line->rect.x = start_point[0];
    line->rect.y = start_point[1];
    line->rect.z = end_point[0];
    line->rect.w = end_point[1];
    line->parameters.x = width;
    line->parameters.x = width;
    line->start_color = layer->current_color;
    line->end_color = layer->current_color;
    layer->current_instruction.total_instances++;

    zest__next_shape_instance(layer);
}

void zest_DrawRect(zest_layer layer, float top_left[2], float width, float height) {
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_rect_instance);    //Call zest_StartSpriteDrawing before calling this function

    zest_shape_instance_t* line = (zest_shape_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;

    line->rect.x = top_left[0];
    line->rect.y = top_left[1];
    line->rect.z = top_left[0] + width;
    line->rect.w = top_left[1] + height;
    line->start_color = layer->current_color;
    line->end_color = layer->current_color;
    layer->current_instruction.total_instances++;

    zest__next_shape_instance(layer);
}
//-- End Line Drawing API

//-- Start 3D Line Drawing API
void zest__initialise_3dline_layer(zest_layer line_layer, zest_uint instance_pool_size) {
    line_layer->current_color.r = 255;
    line_layer->current_color.g = 255;
    line_layer->current_color.b = 255;
    line_layer->current_color.a = 255;
    line_layer->intensity = 1;
    line_layer->push_constants.model = zest_M4(1);
    line_layer->push_constants.parameters1 = zest_Vec4Set1(0.f);
    line_layer->push_constants.parameters2.x = 0.f;
    line_layer->push_constants.parameters2.y = 0.f;
    line_layer->push_constants.parameters2.z = 0.25f;
    line_layer->push_constants.parameters2.w = 0.5f;
    line_layer->layer_size = zest_Vec2Set1(1.f);
    line_layer->instance_struct_size = sizeof(zest_line_instance_t);

    zest_buffer_info_t device_buffer_info = zest_CreateVertexBufferInfo(0);
    if (zest_GPUHasDeviceLocalHostVisible(sizeof(zest_line_instance_t) * instance_pool_size)) {
        ZEST__FLAG(line_layer->flags, zest_layer_flag_device_local_direct);
        device_buffer_info = zest_CreateVertexBufferInfo(ZEST_TRUE);
    }

    zest_buffer_info_t staging_buffer_info = zest_CreateStagingBufferInfo();
    for (ZEST_EACH_FIF_i) {
        line_layer->memory_refs[i].device_instance_data = zest_CreateBuffer(sizeof(zest_line_instance_t) * instance_pool_size, &device_buffer_info, ZEST_NULL);
        line_layer->memory_refs[i].instance_count = 0;
        line_layer->memory_refs[i].instance_count = 0;
        if (ZEST__NOT_FLAGGED(line_layer->flags, zest_layer_flag_device_local_direct)) {
            line_layer->memory_refs[i].staging_instance_data = zest_CreateBuffer(sizeof(zest_line_instance_t) * instance_pool_size, &staging_buffer_info, ZEST_NULL);
            line_layer->memory_refs[i].instance_ptr = line_layer->memory_refs[i].staging_instance_data->data;
            line_layer->memory_refs[i].write_to_buffer = line_layer->memory_refs[i].staging_instance_data;
        }
        else {
            line_layer->memory_refs[i].instance_ptr = line_layer->memory_refs[i].device_instance_data->data;
            line_layer->memory_refs[i].write_to_buffer = line_layer->memory_refs[i].device_instance_data;
        }
    }

    zest_SetLayerViewPort(line_layer, 0, 0, zest_SwapChainWidth(), zest_SwapChainHeight(), zest_SwapChainWidthf(), zest_SwapChainHeightf());

    zest_ResetInstanceLayerDrawing(line_layer);
}

void zest_Set3DLineDrawing(zest_layer line_layer, zest_descriptor_set descriptor_set, zest_pipeline pipeline) {
    zest_EndInstanceInstructions(line_layer);
    zest_StartInstanceInstructions(line_layer);
    line_layer->current_instruction.pipeline = pipeline;
    if (descriptor_set) {
        line_layer->current_instruction.descriptor_set = descriptor_set->descriptor_set[ZEST_FIF];
    }
    else {
        line_layer->current_instruction.descriptor_set = pipeline->descriptor_set[ZEST_FIF];
    }
    line_layer->current_instruction.scissor = line_layer->scissor;
    line_layer->current_instruction.viewport = line_layer->viewport;
    line_layer->current_instruction.draw_mode = zest_draw_mode_line_instance;
    line_layer->last_draw_mode = zest_draw_mode_line_instance;
}

void zest_Draw3DLine(zest_layer layer, float start_point[3], float end_point[3], float width) {
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_line_instance || layer->current_instruction.draw_mode == zest_draw_mode_dashed_line);	//Call zest_StartSpriteDrawing before calling this function

    zest_line_instance_t* line = (zest_line_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;

    line->start.x = start_point[0];
    line->start.y = start_point[1];
    line->start.z = start_point[2];
    line->end.x = end_point[0];
    line->end.y = end_point[1];
    line->end.z = end_point[2];
    line->start.w = width;
    line->end.w = width;
    line->start_color = layer->current_color;
    line->end_color = layer->current_color;
    layer->current_instruction.total_instances++;

    zest__next_3dline_instance(layer);
}
//-- End Line Drawing API

//-- Start Mesh Drawing API
void zest__initialise_mesh_layer(zest_layer mesh_layer, zest_size vertex_struct_size, zest_size initial_vertex_capacity) {
    mesh_layer->current_color.r = 255;
    mesh_layer->current_color.g = 255;
    mesh_layer->current_color.b = 255;
    mesh_layer->current_color.a = 255;
    mesh_layer->intensity = 1;
    mesh_layer->push_constants.model = zest_M4(1);
    mesh_layer->push_constants.parameters1 = zest_Vec4Set1(0.f);
    mesh_layer->push_constants.parameters2.x = 0.f;
    mesh_layer->push_constants.parameters2.y = 0.f;
    mesh_layer->push_constants.parameters2.z = 0.25f;
    mesh_layer->push_constants.parameters2.w = 0.5f;
    mesh_layer->layer_size = zest_Vec2Set1(1.f);
    mesh_layer->vertex_struct_size = vertex_struct_size;

    zest_buffer_info_t device_vertex_buffer_info = zest_CreateVertexBufferInfo(0);
    zest_buffer_info_t device_index_buffer_info = zest_CreateIndexBufferInfo(0);
    if (zest_GPUHasDeviceLocalHostVisible(initial_vertex_capacity)) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating Device local buffers for mesh layer" ZEST_NL);
        ZEST__FLAG(mesh_layer->flags, zest_layer_flag_device_local_direct);
        device_vertex_buffer_info = zest_CreateVertexBufferInfo(ZEST_TRUE);
        device_index_buffer_info = zest_CreateIndexBufferInfo(ZEST_TRUE);
    }

    zest_buffer_info_t staging_buffer_info = zest_CreateStagingBufferInfo();

    for (ZEST_EACH_FIF_i) {
        mesh_layer->memory_refs[i].device_vertex_data = zest_CreateBuffer(initial_vertex_capacity, &device_vertex_buffer_info, ZEST_NULL);
        mesh_layer->memory_refs[i].device_index_data = zest_CreateBuffer(initial_vertex_capacity, &device_index_buffer_info, ZEST_NULL);
        mesh_layer->memory_refs[i].index_count = 0;
        mesh_layer->memory_refs[i].index_position = 0;
        mesh_layer->memory_refs[i].last_index = 0;
        mesh_layer->memory_refs[i].vertex_count = 0;
        if (ZEST__NOT_FLAGGED(mesh_layer->flags, zest_layer_flag_device_local_direct)) {
            mesh_layer->memory_refs[i].staging_vertex_data = zest_CreateBuffer(initial_vertex_capacity, &staging_buffer_info, ZEST_NULL);
            mesh_layer->memory_refs[i].staging_index_data = zest_CreateBuffer(initial_vertex_capacity, &staging_buffer_info, ZEST_NULL);
            mesh_layer->memory_refs[i].vertex_ptr = mesh_layer->memory_refs[i].staging_vertex_data->data;
            mesh_layer->memory_refs[i].index_ptr = mesh_layer->memory_refs[i].staging_index_data->data;
            mesh_layer->memory_refs[i].write_to_buffer = mesh_layer->memory_refs[i].staging_vertex_data;
            mesh_layer->memory_refs[i].write_to_index_buffer = mesh_layer->memory_refs[i].staging_index_data;
        }
        else {
            mesh_layer->memory_refs[i].vertex_ptr = mesh_layer->memory_refs[i].device_vertex_data->data;
            mesh_layer->memory_refs[i].index_ptr = mesh_layer->memory_refs[i].device_index_data->data;
            mesh_layer->memory_refs[i].write_to_buffer = mesh_layer->memory_refs[i].device_vertex_data;
            mesh_layer->memory_refs[i].write_to_index_buffer = mesh_layer->memory_refs[i].device_index_data;
        }
    }

    zest_SetLayerViewPort(mesh_layer, 0, 0, zest_SwapChainWidth(), zest_SwapChainHeight(), zest_SwapChainWidthf(), zest_SwapChainHeightf());
    zest__reset_mesh_layer_drawing(mesh_layer);

}

void zest_BindMeshVertexBuffer(zest_layer layer) {
    zest_buffer_t* buffer = layer->memory_refs[ZEST_FIF].device_vertex_data;
    VkDeviceSize offsets[] = { buffer->memory_offset };
    vkCmdBindVertexBuffers(ZestRenderer->current_command_buffer, 0, 1, zest_GetBufferDeviceBuffer(buffer), offsets);
}

void zest_BindMeshIndexBuffer(zest_layer layer) {
    zest_buffer_t* buffer = layer->memory_refs[ZEST_FIF].device_index_data;
    vkCmdBindIndexBuffer(ZestRenderer->current_command_buffer, *zest_GetBufferDeviceBuffer(buffer), buffer->memory_offset, VK_INDEX_TYPE_UINT32);
}

zest_buffer zest_GetVertexWriteBuffer(zest_layer layer) {
    return layer->memory_refs[ZEST_FIF].write_to_buffer;
}

zest_buffer zest_GetIndexWriteBuffer(zest_layer layer) {
    return layer->memory_refs[ZEST_FIF].write_to_index_buffer;
}

zest_buffer zest_GetVertexDeviceBuffer(zest_layer layer) {
    return layer->memory_refs[ZEST_FIF].device_vertex_data;
}

zest_buffer zest_GetIndexDeviceBuffer(zest_layer layer) {
    return layer->memory_refs[ZEST_FIF].device_index_data;
}

void zest_GrowMeshVertexBuffers(zest_layer layer) {
    zest_size memory_in_use = zest_GetVertexWriteBuffer(layer)->memory_in_use;
    zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].write_to_buffer, layer->vertex_struct_size, memory_in_use);
    zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_vertex_data, layer->vertex_struct_size, memory_in_use);
}

void zest_GrowMeshIndexBuffers(zest_layer layer) {
    zest_size memory_in_use = zest_GetVertexWriteBuffer(layer)->memory_in_use;
    zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].write_to_index_buffer, sizeof(zest_uint), memory_in_use);
    zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_index_data, sizeof(zest_uint), memory_in_use);
}

void zest_UploadMeshBuffersCallback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer) {
    zest_layer mesh_layer = (zest_layer)draw_routine->draw_data;
    zest__end_mesh_instructions(mesh_layer);

    if (ZEST__NOT_FLAGGED(mesh_layer->flags, zest_layer_flag_device_local_direct)) {
        zest_buffer staging_vertex = mesh_layer->memory_refs[ZEST_FIF].write_to_buffer;
        zest_buffer staging_index = mesh_layer->memory_refs[ZEST_FIF].write_to_index_buffer;
        zest_buffer device_vertex = mesh_layer->memory_refs[ZEST_FIF].device_vertex_data;
        zest_buffer device_index = mesh_layer->memory_refs[ZEST_FIF].device_index_data;

        if (staging_vertex->memory_in_use && staging_index->memory_in_use) {
            zest_AddCopyCommand(&mesh_layer->vertex_upload, staging_vertex, device_vertex, 0);
            zest_AddCopyCommand(&mesh_layer->index_upload, staging_index, device_index, 0);
        }

        zest_UploadBuffer(&mesh_layer->vertex_upload, command_buffer);
        zest_UploadBuffer(&mesh_layer->index_upload, command_buffer);
    }
}

void zest_PushVertex(zest_layer layer, float pos_x, float pos_y, float pos_z, float intensity, float uv_x, float uv_y, zest_color color, zest_uint parameters) {
    zest_textured_vertex_t vertex = { 0 };
    vertex.pos = zest_Vec3Set(pos_x, pos_y, pos_z);
    vertex.intensity = intensity;
    vertex.uv = zest_Vec2Set(uv_x, uv_y);
    vertex.color = color;
    vertex.parameters = parameters;
    zest_textured_vertex_t* vertex_ptr = (zest_textured_vertex_t*)layer->memory_refs[ZEST_FIF].vertex_ptr;
    *vertex_ptr = vertex;
    vertex_ptr = vertex_ptr + 1;
    ZEST_ASSERT(vertex_ptr >= (zest_textured_vertex_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->data && vertex_ptr <= (zest_textured_vertex_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->end);
    if (vertex_ptr == layer->memory_refs[ZEST_FIF].write_to_buffer->end) {
        zest_bool grown = 0;
        if (ZEST__FLAGGED(layer->flags, zest_layer_flag_device_local_direct)) {
            grown = zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_vertex_data, sizeof(zest_textured_vertex_t), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].device_vertex_data;
        }
        else {
            grown = zest_GrowBuffer(&layer->memory_refs[ZestDevice->current_fif].write_to_buffer, sizeof(zest_textured_vertex_t), 0);
            zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_vertex_data, sizeof(zest_textured_vertex_t), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].staging_vertex_data;
        }
        if (grown) {
            layer->memory_refs[ZEST_FIF].vertex_count++;
            vertex_ptr = (zest_textured_vertex_t*)layer->memory_refs[ZEST_FIF].write_to_buffer->data;
            vertex_ptr += layer->memory_refs[ZEST_FIF].vertex_count;
        }
        else {
            vertex_ptr = vertex_ptr - 1;
        }
    }
    else {
        layer->memory_refs[ZEST_FIF].vertex_count++;
    }
    layer->memory_refs[ZEST_FIF].vertex_ptr = vertex_ptr;
}

void zest_PushIndex(zest_layer layer, zest_uint offset) {
    zest_uint index = layer->memory_refs[ZEST_FIF].vertex_count + offset;
    zest_uint* index_ptr = (zest_uint*)layer->memory_refs[ZEST_FIF].index_ptr;
    *index_ptr = index;
    index_ptr = index_ptr + 1;
    ZEST_ASSERT(index_ptr >= (zest_uint*)layer->memory_refs[ZEST_FIF].write_to_index_buffer->data && index_ptr <= (zest_uint*)layer->memory_refs[ZEST_FIF].write_to_index_buffer->end);
    if (index_ptr == layer->memory_refs[ZEST_FIF].write_to_index_buffer->end) {
        zest_bool grown = 0;
        if (ZEST__FLAGGED(layer->flags, zest_layer_flag_device_local_direct)) {
            grown = zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_index_data, sizeof(zest_uint), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].device_index_data;
        }
        else {
            grown = zest_GrowBuffer(&layer->memory_refs[ZestDevice->current_fif].write_to_buffer, sizeof(zest_uint), 0);
            zest_GrowBuffer(&layer->memory_refs[ZEST_FIF].device_index_data, sizeof(zest_uint), 0);
            layer->memory_refs[ZEST_FIF].write_to_buffer = layer->memory_refs[ZEST_FIF].staging_index_data;
        }
        if (grown) {
            layer->memory_refs[ZEST_FIF].index_count++;
            layer->current_instruction.total_indexes++;
            index_ptr = (zest_uint*)layer->memory_refs[ZEST_FIF].write_to_index_buffer->data;
            index_ptr += layer->memory_refs[ZEST_FIF].index_count;
        }
        else {
            index_ptr = index_ptr - 1;
        }
    }
    else {
        layer->memory_refs[ZEST_FIF].index_count++;
        layer->current_instruction.total_indexes++;
    }
    layer->memory_refs[ZEST_FIF].index_ptr = index_ptr;
}

void zest_SetMeshDrawing(zest_layer layer, zest_texture texture, zest_descriptor_set descriptor_set, zest_pipeline pipeline) {
    zest__end_mesh_instructions(layer);
    zest__start_mesh_instructions(layer);
    layer->current_instruction.pipeline = pipeline;
    if (descriptor_set) {
        layer->current_instruction.descriptor_set = descriptor_set->descriptor_set[ZEST_FIF];
    }
    else {
        layer->current_instruction.descriptor_set = texture->current_descriptor_set[ZEST_FIF];
    }
    layer->current_instruction.draw_mode = zest_draw_mode_mesh;
    layer->current_instruction.scissor = layer->scissor;
    layer->current_instruction.viewport = layer->viewport;
    layer->last_draw_mode = zest_draw_mode_mesh;
}

void zest_DrawTexturedPlane(zest_layer layer, zest_image image, float x, float y, float z, float width, float height, float scale_x, float scale_y, float offset_x, float offset_y) {
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_mesh);    //You must call zest_SetMeshDrawing before calling this function and also make sure you're passing in the correct mesh layer
    zest_vec4 uv = zest_Vec4Set(0.f, 0.f, 1.f, 1.f);
    if (offset_x || offset_y) {
        offset_x = fmodf(offset_x, (float)image->width) / image->width;
        offset_y = fmodf(offset_y, (float)image->height) / image->height;
        offset_x *= scale_x;
        offset_y *= scale_y;
    }

    scale_x *= width / (float)image->width;
    scale_y *= height / (float)image->height;
    uv.z = (uv.z * scale_x) - offset_x;
    uv.w = (uv.w * scale_y) - offset_y;
    uv.x -= offset_x;
    uv.y -= offset_y;

    zest_PushIndex(layer, 0);
    zest_PushVertex(layer, x, y, z, layer->intensity, uv.x, uv.y, layer->current_color, image->layer);
    zest_PushIndex(layer, 0);
    zest_PushVertex(layer, x + width, y, z, layer->intensity, uv.z, uv.y, layer->current_color, image->layer);
    zest_PushIndex(layer, 0);
    zest_PushIndex(layer, 0);
    zest_PushIndex(layer, -1);
    zest_PushVertex(layer, x, y, z + height, layer->intensity, uv.x, uv.w, layer->current_color, image->layer);
    zest_PushIndex(layer, 0);
    zest_PushVertex(layer, x + width, y, z + height, layer->intensity, uv.z, uv.w, layer->current_color, image->layer);
}
//-- End Mesh Drawing API

//-- Instanced_mesh_drawing
void zest__initialise_instance_mesh_layer(zest_layer mesh_layer, zest_size vertex_struct_size, zest_size instance_pool_size) {
    mesh_layer->current_color.r = 255;
    mesh_layer->current_color.g = 255;
    mesh_layer->current_color.b = 255;
    mesh_layer->current_color.a = 255;
    mesh_layer->intensity = 1;
    mesh_layer->push_constants.model = zest_M4(1);
    mesh_layer->push_constants.parameters1 = zest_Vec4Set1(0.f);
    mesh_layer->push_constants.parameters2.x = 0.f;
    mesh_layer->push_constants.parameters2.y = 0.f;
    mesh_layer->push_constants.parameters2.z = 0.25f;
    mesh_layer->push_constants.parameters2.w = 0.5f;
    mesh_layer->layer_size = zest_Vec2Set1(1.f);
    mesh_layer->instance_struct_size = sizeof(zest_mesh_instance_t);

    zest_buffer_info_t device_buffer_info = zest_CreateVertexBufferInfo(0);
    if (zest_GPUHasDeviceLocalHostVisible(sizeof(zest_mesh_instance_t) * instance_pool_size)) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating Device local buffers for billboard layer" ZEST_NL);
        ZEST__FLAG(mesh_layer->flags, zest_layer_flag_device_local_direct);
        device_buffer_info = zest_CreateVertexBufferInfo(ZEST_TRUE);
    }

    zest_buffer_info_t staging_buffer_info = zest_CreateStagingBufferInfo();
    for (ZEST_EACH_FIF_i) {
        mesh_layer->memory_refs[i].device_instance_data = zest_CreateBuffer(sizeof(zest_mesh_instance_t) * instance_pool_size, &device_buffer_info, ZEST_NULL);
        mesh_layer->memory_refs[i].instance_count = 0;
        mesh_layer->memory_refs[i].instance_count = 0;
        if (ZEST__NOT_FLAGGED(mesh_layer->flags, zest_layer_flag_device_local_direct)) {
            mesh_layer->memory_refs[i].staging_instance_data = zest_CreateBuffer(sizeof(zest_mesh_instance_t) * instance_pool_size, &staging_buffer_info, ZEST_NULL);
            mesh_layer->memory_refs[i].instance_ptr = mesh_layer->memory_refs[i].staging_instance_data->data;
            mesh_layer->memory_refs[i].write_to_buffer = mesh_layer->memory_refs[i].staging_instance_data;
        }
        else {
            mesh_layer->memory_refs[i].instance_ptr = mesh_layer->memory_refs[i].device_instance_data->data;
            mesh_layer->memory_refs[i].write_to_buffer = mesh_layer->memory_refs[i].device_instance_data;
        }
    }

    zest_SetLayerViewPort(mesh_layer, 0, 0, zest_SwapChainWidth(), zest_SwapChainHeight(), zest_SwapChainWidthf(), zest_SwapChainHeightf());

    zest_ResetInstanceLayerDrawing(mesh_layer);
}

void zest_BindInstanceMeshVertexBuffer(zest_layer layer) {
    zest_buffer_t* buffer = layer->memory_refs[ZEST_FIF].device_vertex_data;
    VkDeviceSize offsets[] = { buffer->memory_offset };
    vkCmdBindVertexBuffers(ZestRenderer->current_command_buffer, 0, 1, zest_GetBufferDeviceBuffer(buffer), offsets);
}

void zest_BindInstanceMeshIndexBuffer(zest_layer layer) {
    zest_buffer_t* buffer = layer->memory_refs[ZEST_FIF].device_index_data;
    vkCmdBindIndexBuffer(ZestRenderer->current_command_buffer, *zest_GetBufferDeviceBuffer(buffer), buffer->memory_offset, VK_INDEX_TYPE_UINT32);
}

void zest_SetInstanceMeshDrawing(zest_layer layer, zest_descriptor_set descriptor_set, zest_pipeline pipeline) {
    zest_EndInstanceInstructions(layer);
    zest_StartInstanceInstructions(layer);
    layer->current_instruction.pipeline = pipeline;
    if (descriptor_set) {
        layer->current_instruction.descriptor_set = descriptor_set->descriptor_set[ZEST_FIF];
    }
    else {
        layer->current_instruction.descriptor_set = pipeline->descriptor_set[ZEST_FIF];
    }
    layer->current_instruction.draw_mode = zest_draw_mode_mesh_instance;
    layer->current_instruction.scissor = layer->scissor;
    layer->current_instruction.viewport = layer->viewport;
    layer->last_draw_mode = zest_draw_mode_mesh_instance;
}

void zest_PushMeshVertex(zest_mesh_t* mesh, float pos_x, float pos_y, float pos_z, zest_color color) {
    zest_vertex_t vertex = { pos_x, pos_y, pos_z, color };
    zest_vec_push(mesh->vertices, vertex);
}

void zest_PushMeshIndex(zest_mesh_t* mesh, zest_uint index) {
    ZEST_ASSERT(index < zest_vec_size(mesh->vertices)); //Add vertices first before triangles to make sure you're indexing vertices that exist
    zest_vec_push(mesh->indexes, index);
}

void zest_PushMeshTriangle(zest_mesh_t *mesh, zest_uint i1, zest_uint i2, zest_uint i3) {
    ZEST_ASSERT(i1 < zest_vec_size(mesh->vertices));	//Add vertices first before triangles to make sure you're indexing vertices that exist
    ZEST_ASSERT(i2 < zest_vec_size(mesh->vertices));	//Add vertices first before triangles to make sure you're indexing vertices that exist
    ZEST_ASSERT(i3 < zest_vec_size(mesh->vertices));	//Add vertices first before triangles to make sure you're indexing vertices that exist
    zest_vec_push(mesh->indexes, i1);
    zest_vec_push(mesh->indexes, i2);
    zest_vec_push(mesh->indexes, i3);
}

void zest_FreeMesh(zest_mesh_t* mesh) {
    zest_vec_free(mesh->vertices);
    zest_vec_free(mesh->indexes);
}

void zest_PositionMesh(zest_mesh_t* mesh, zest_vec3 position) {
    for (zest_foreach_i(mesh->vertices)) {
        mesh->vertices[i].pos.x += position.x;
        mesh->vertices[i].pos.y += position.y;
        mesh->vertices[i].pos.z += position.z;
    }
}

zest_matrix4 zest_RotateMesh(zest_mesh_t* mesh, float pitch, float yaw, float roll) {
    zest_matrix4 roll_mat = zest_Matrix4RotateZ(roll);
    zest_matrix4 pitch_mat = zest_Matrix4RotateX(pitch);
    zest_matrix4 yaw_mat = zest_Matrix4RotateY(yaw);
    zest_matrix4 rotate_mat = zest_MatrixTransform(&yaw_mat, &pitch_mat);
    rotate_mat = zest_MatrixTransform(&rotate_mat, &roll_mat);
    for (zest_foreach_i(mesh->vertices)) {
        zest_vec4 pos = { mesh->vertices[i].pos.x, mesh->vertices[i].pos.y, mesh->vertices[i].pos.z, 1.f };
        pos = zest_MatrixTransformVector(&rotate_mat, pos);
        mesh->vertices[i].pos = zest_Vec3Set(pos.x, pos.y, pos.z);
    }
    return rotate_mat;
}

zest_matrix4 zest_TransformMesh(zest_mesh_t* mesh, float pitch, float yaw, float roll, float x, float y, float z, float sx, float sy, float sz) {
    zest_matrix4 roll_mat = zest_Matrix4RotateZ(roll);
    zest_matrix4 pitch_mat = zest_Matrix4RotateX(pitch);
    zest_matrix4 yaw_mat = zest_Matrix4RotateY(yaw);
    zest_matrix4 rotate_mat = zest_MatrixTransform(&yaw_mat, &pitch_mat);
    rotate_mat = zest_MatrixTransform(&rotate_mat, &roll_mat);
    rotate_mat.v[0].w = x;
    rotate_mat.v[1].w = y;
    rotate_mat.v[2].w = z;
    rotate_mat.v[3].x = sx;
    rotate_mat.v[3].y = sy;
    rotate_mat.v[3].z = sz;
    for (zest_foreach_i(mesh->vertices)) {
        zest_vec4 pos = { mesh->vertices[i].pos.x, mesh->vertices[i].pos.y, mesh->vertices[i].pos.z, 1.f };
        pos = zest_MatrixTransformVector(&rotate_mat, pos);
        mesh->vertices[i].pos = zest_Vec3Set(pos.x, pos.y, pos.z);
    }
    return rotate_mat;
}

zest_bounding_box_t zest_NewBoundingBox() {
    zest_bounding_box_t bb = { 0 };
    bb.max_bounds = zest_Vec3Set( -9999999.f, -9999999.f, -9999999.f );
    bb.min_bounds = zest_Vec3Set( FLT_MAX, FLT_MAX, FLT_MAX );
    return bb;
}

zest_bounding_box_t zest_GetMeshBoundingBox(zest_mesh_t* mesh) {
    zest_bounding_box_t bb = zest_NewBoundingBox();
    for (zest_foreach_i(mesh->vertices)) {
        bb.max_bounds.x = ZEST__MAX(mesh->vertices[i].pos.x, bb.max_bounds.x);
        bb.max_bounds.y = ZEST__MAX(mesh->vertices[i].pos.y, bb.max_bounds.y);
        bb.max_bounds.z = ZEST__MAX(mesh->vertices[i].pos.z, bb.max_bounds.z);
        bb.min_bounds.x = ZEST__MIN(mesh->vertices[i].pos.x, bb.min_bounds.x);
        bb.min_bounds.y = ZEST__MIN(mesh->vertices[i].pos.y, bb.min_bounds.y);
        bb.min_bounds.z = ZEST__MIN(mesh->vertices[i].pos.z, bb.min_bounds.z);
    }
    return bb;
}

void zest_SetMeshGroupID(zest_mesh_t* mesh, zest_uint group_id) {
    for (zest_foreach_i(mesh->vertices)) {
        mesh->vertices[i].group = group_id;
    }
}

zest_size zest_MeshVertexDataSize(zest_mesh_t* mesh) {
    return zest_vec_size(mesh->vertices) * sizeof(zest_vertex_t);
}

zest_size zest_MeshIndexDataSize(zest_mesh_t* mesh) {
    return zest_vec_size(mesh->indexes) * sizeof(zest_uint);
}

void zest_AddMeshToMesh(zest_mesh_t* dst_mesh, zest_mesh_t* src_mesh) {
    zest_uint dst_vertex_size = zest_vec_size(dst_mesh->vertices);
    zest_uint src_vertex_size = zest_vec_size(src_mesh->vertices);
    zest_uint dst_index_size = zest_vec_size(dst_mesh->indexes);
    zest_uint src_index_size = zest_vec_size(src_mesh->indexes);
    zest_vec_resize(dst_mesh->vertices, dst_vertex_size + src_vertex_size);
    memcpy(dst_mesh->vertices + dst_vertex_size, src_mesh->vertices, src_vertex_size * sizeof(zest_vertex_t));
    zest_vec_resize(dst_mesh->indexes, dst_index_size + src_index_size);
    memcpy(dst_mesh->indexes + dst_index_size, src_mesh->indexes, src_index_size * sizeof(zest_uint));
    for (int i = dst_index_size; i != src_index_size + dst_index_size; ++i) {
        dst_mesh->indexes[i] += dst_vertex_size;
    }
}

void zest_AddMeshToLayer(zest_layer layer, zest_mesh_t* src_mesh) {
    zest_buffer vertex_staging_buffer = zest_CreateStagingBuffer(zest_MeshVertexDataSize(src_mesh), src_mesh->vertices);
    zest_buffer index_staging_buffer = zest_CreateStagingBuffer(zest_MeshIndexDataSize(src_mesh), src_mesh->indexes);
    layer->index_count = zest_vec_size(src_mesh->indexes);
    for (ZEST_EACH_FIF_i) {
		zest_FreeBuffer(layer->memory_refs[i].device_vertex_data);
		zest_FreeBuffer(layer->memory_refs[i].device_index_data);
        layer->memory_refs[i].device_vertex_data = zest_CreateVertexBuffer(vertex_staging_buffer->size, vertex_staging_buffer);
        layer->memory_refs[i].device_index_data = zest_CreateVertexBuffer(index_staging_buffer->size, index_staging_buffer);
    }
}

void zest_DrawInstancedMesh(zest_layer layer, float pos[3], float rot[3], float scale[3]) {
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_mesh_instance);        //Call zest_StartSpriteDrawing before calling this function

    zest_mesh_instance_t* instance = (zest_mesh_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;

    instance->pos = zest_Vec3Set(pos[0], pos[1], pos[2]);
    instance->rotation = zest_Vec3Set(rot[0], rot[1], rot[2]);
    instance->scale = zest_Vec3Set(scale[0], scale[1], scale[2]);
    instance->color = layer->current_color;
    instance->parameters = 0;
    layer->current_instruction.total_instances++;

    zest__next_mesh_instance(layer);
}

zest_mesh_t zest_CreateCylinderMesh(int sides, float radius, float height, zest_color color, zest_bool cap) {
    float angle_increment = 2.0f * ZEST_PI / sides;

    int vertex_count = sides * 2 + (cap ? sides * 2 : 0);
    int index_count = sides * 2 + (cap ? sides : 0);

    zest_mesh_t mesh = { 0 };
    zest_vec_resize(mesh.vertices, (zest_uint)vertex_count);

    for (int i = 0; i < sides; ++i) {
        float angle = i * angle_increment;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);

        mesh.vertices[i].pos = zest_Vec3Set( x, height / 2.0f, z );
        mesh.vertices[i + sides].pos = zest_Vec3Set( x, -height / 2.0f, z );
        mesh.vertices[i].color = color;
        mesh.vertices[i + sides].color = color;
    }

    int base_index = sides * 2;

    if (cap) {
        // Generate vertices for the caps of the cylinder
        for (int i = 0; i < sides; ++i) {
            float angle = i * angle_increment;
            float x = radius * cosf(angle);
            float z = radius * sinf(angle);

            mesh.vertices[base_index + i].pos = zest_Vec3Set( x, height / 2.0f, z);
            mesh.vertices[base_index + sides + i].pos = zest_Vec3Set( x, -height / 2.0f, z);
			mesh.vertices[base_index + i].color = color;
			mesh.vertices[base_index + i + sides].color = color;
        }

        // Generate indices for the caps of the cylinder
        for (int i = 0; i < sides - 2; ++i) {
			zest_PushMeshIndex(&mesh, base_index );
			zest_PushMeshIndex(&mesh, base_index + i + 1);
			zest_PushMeshIndex(&mesh, base_index + i + 2);

			zest_PushMeshIndex(&mesh, base_index + sides);
			zest_PushMeshIndex(&mesh, base_index + sides + i + 2);
			zest_PushMeshIndex(&mesh, base_index + sides + i + 1);
        }
    }

    // Generate indices for the sides of the cylinder
    for (int i = 0; i < sides; ++i) {
        int next = (i + 1) % sides;

        zest_PushMeshIndex(&mesh, i);
        zest_PushMeshIndex(&mesh, (i + 1) % sides);
        zest_PushMeshIndex(&mesh, i + sides);

        zest_PushMeshIndex(&mesh, i + sides);
        zest_PushMeshIndex(&mesh, (i + 1) % sides);
        zest_PushMeshIndex(&mesh, (i + 1) % sides + sides );
    }
    
    return mesh;
}

zest_mesh_t zest_CreateCone(int sides, float radius, float height, zest_color color) {
    // Calculate the angle between each side
    float angle_increment = 2.0f * ZEST_PI / sides;

    zest_mesh_t mesh = { 0 };
    zest_vec_resize(mesh.vertices, (zest_uint)sides + 1);

    // Generate vertices for the base of the cone
    for (int i = 0; i < sides; ++i) {
        float angle = i * angle_increment;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);

        mesh.vertices[i].pos = zest_Vec3Set( x, 0.0f, z);
        mesh.vertices[i].color = color;
    }

    // Generate the vertex for the tip of the cone
    mesh.vertices[sides].pos = zest_Vec3Set( 0.0f, height, 0.0f);
    mesh.vertices[sides].color = color;

    // Generate indices for the sides of the cone
    for (int i = 0; i < sides; ++i) {
        zest_PushMeshTriangle(&mesh, i, sides, (i + 1) % sides);
    }

    // Generate indices for the base of the cone
	for (int i = 0; i < sides; ++i) {
		zest_PushMeshTriangle(&mesh, i, (i + 1) % sides, sides);
	}

    return mesh;
}

zest_mesh_t zest_CreateSphere(int rings, int sectors, float radius, zest_color color) {
    // Calculate the angles between rings and sectors
    float ring_angle_increment = ZEST_PI / rings;
    float sector_angle_increment = 2.0f * ZEST_PI / sectors;
    float ring_angle;
    float sector_angle;

    zest_mesh_t mesh = {0};

    // Generate vertices for the sphere
    for (int i = 0; i <= rings; ++i)
    {
        ring_angle = ZEST_PI / 2.f - i * ring_angle_increment; /* Starting -pi/2 to pi/2 */
        float xy = radius * cosf(ring_angle);    /* r * cos(phi) */
        float z = radius * sinf(ring_angle);     /* r * sin(phi )*/

        /*
         * We add (rings + 1) vertices per longitude because of equator,
         * the North pole and South pole are not counted here, as they overlap.
         * The first and last vertices have same position and normal, but
         * different tex coords.
         */
        for (int j = 0; j <= sectors; ++j)
        {
            sector_angle = j * sector_angle_increment;
            zest_vec3 vertex;
            vertex.x = xy * cosf(sector_angle);       /* x = r * cos(phi) * cos(theta)  */
            vertex.y = xy * sinf(sector_angle);       /* y = r * cos(phi) * sin(theta) */
            vertex.z = z;                               /* z = r * sin(phi) */
            zest_PushMeshVertex(&mesh, vertex.x, vertex.y, vertex.z, color);
        }
    }

    // Generate indices for the sphere    
    unsigned int k1, k2;
    for (int i = 0; i < rings; ++i)
    {
        k1 = i * (sectors + 1);
        k2 = k1 + sectors + 1;
        // 2 Triangles per latitude block excluding the first and last sectors blocks
        for (int j = 0; j < sectors; ++j, ++k1, ++k2)
        {
            if (i != 0)
            {
                zest_PushMeshIndex(&mesh, k1);
                zest_PushMeshIndex(&mesh, k2);
                zest_PushMeshIndex(&mesh, k1 + 1);
            }

            if (i != (rings - 1))
            {
                zest_PushMeshIndex(&mesh, k1 + 1);
                zest_PushMeshIndex(&mesh, k2);
                zest_PushMeshIndex(&mesh, k2 + 1);
            }
        }
    }
    
    return mesh;
}

zest_mesh_t zest_CreateCube(float size, zest_color color) {
    zest_mesh_t mesh = { 0 };

    // Generate vertices for the corners of the cube
    float halfSize = size / 2.0f;
    zest_PushMeshVertex(&mesh, -halfSize, -halfSize, -halfSize, color);
    zest_PushMeshVertex(&mesh, -halfSize, -halfSize, halfSize, color);
    zest_PushMeshVertex(&mesh, halfSize, -halfSize, halfSize, color);
    zest_PushMeshVertex(&mesh, halfSize, -halfSize, -halfSize, color);
    zest_PushMeshVertex(&mesh, -halfSize, halfSize, -halfSize, color);
    zest_PushMeshVertex(&mesh, -halfSize, halfSize, halfSize, color);
    zest_PushMeshVertex(&mesh, halfSize, halfSize, halfSize, color);
    zest_PushMeshVertex(&mesh, halfSize, halfSize, -halfSize, color);

    // Generate indices for the cube
    zest_PushMeshTriangle(&mesh, 0, 2, 1);
    zest_PushMeshTriangle(&mesh, 0, 3, 2);
    zest_PushMeshTriangle(&mesh, 4, 5, 6);
    zest_PushMeshTriangle(&mesh, 4, 6, 7);
    zest_PushMeshTriangle(&mesh, 1, 6, 5);
    zest_PushMeshTriangle(&mesh, 1, 2, 6);
    zest_PushMeshTriangle(&mesh, 0, 4, 7);
    zest_PushMeshTriangle(&mesh, 0, 7, 3);
    zest_PushMeshTriangle(&mesh, 0, 1, 5);
    zest_PushMeshTriangle(&mesh, 0, 5, 4);
    zest_PushMeshTriangle(&mesh, 2, 7, 6);
    zest_PushMeshTriangle(&mesh, 2, 3, 7);

    return mesh;
}

zest_mesh_t zest_CreateRoundedRectangle(float width, float height, float radius, int segments, zest_bool backface, zest_color color) {
    // Calculate the number of vertices and indices needed
    int num_vertices = segments * 4 + 8;

    zest_mesh_t mesh = { 0 };
    //zest_vec_resize(mesh.vertices, (zest_uint)num_vertices);

    // Calculate the step angle
    float angle_increment = ZEST_PI / 2.0f / segments;

    width = ZEST__MAX(radius, width - radius * 2.f);
    height = ZEST__MAX(radius, height - radius * 2.f);

    //centre vertex;
	zest_PushMeshVertex(&mesh, 0.f, 0.f, 0.0f, color);

    // Bottom left corner
    for (int i = 0; i <= segments; ++i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushMeshVertex(&mesh, -width / 2.0f - x, -height / 2.0f - y, 0.0f, color);
    }

    // Bottom right corner
    for (int i = segments; i >= 0; --i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushMeshVertex(&mesh, width / 2.0f + x, -height / 2.0f - y, 0.0f, color);
    }

    // Top right corner
    for (int i = 0; i <= segments; ++i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushMeshVertex(&mesh, width / 2.0f + x, height / 2.0f + y, 0.0f, color);
    }

    // Top left corner
    for (int i = segments; i >= 0; --i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushMeshVertex(&mesh, - width / 2.0f - x, height / 2.0f + y, 0.0f, color);
    }

    zest_uint vertex_count = zest_vec_size(mesh.vertices);

    for (zest_uint i = 1; i < vertex_count - 1; ++i) {
        zest_PushMeshTriangle(&mesh, 0, i, i + 1);
    }
	zest_PushMeshTriangle(&mesh, 0, vertex_count - 1, 1);

    if (backface) {
		for (zest_uint i = 1; i < vertex_count - 1; ++i) {
			zest_PushMeshTriangle(&mesh, 0, i + 1, i);
		}
		zest_PushMeshTriangle(&mesh, 0, 1, vertex_count - 1);
    }

    return mesh;
}
//--End Instance Draw mesh layers

//Compute shaders
zest_compute zest_CreateCompute(const char* name) {
    zest_compute_t blank_compute = { 0 };
    zest_compute compute = ZEST__NEW_ALIGNED(zest_compute, 16);
    *compute = blank_compute;
    compute->descriptor_pool = VK_NULL_HANDLE;
    compute->group_count_x = 1;
    compute->group_count_y = 1;
    compute->group_count_z = 1;
    compute->render_target = 0;
    compute->extra_cleanup_callback = ZEST_NULL;
    compute->push_constants.parameters.x = 0.f;
    compute->push_constants.parameters.y = 0.f;
    compute->push_constants.parameters.z = 0.f;
    compute->push_constants.parameters.w = 0.f;
    for (ZEST_EACH_FIF_i) {
        compute->frame_has_run[i] = ZEST_FALSE;
        compute->fence[i] = VK_NULL_HANDLE;
        compute->fif_incoming_semaphore[i] = VK_NULL_HANDLE;
        compute->fif_outgoing_semaphore[i] = VK_NULL_HANDLE;
    }
    zest_map_insert(ZestRenderer->computes, name, compute);
    return compute;
}

zest_compute_builder_t zest_NewComputeBuilder() {
    zest_compute_builder_t builder = { 0 };
    builder.descriptor_update_callback = zest_StandardComputeDescriptorUpdate;
    return builder;
}

zest_compute zest_RegisterCompute() {
    zest_compute_t blank_compute = { 0 };
    zest_compute compute = ZEST__NEW(zest_compute);
    *compute = blank_compute;
    return compute;
}

void zest_AddComputeLayoutBinding(zest_compute_builder_t* builder, VkDescriptorType descriptor_type, int binding) {
    zest_vec_push(builder->layout_bindings, zest_CreateDescriptorLayoutBinding(descriptor_type, VK_SHADER_STAGE_COMPUTE_BIT, binding, 1));
    if (zest_map_valid_key(builder->descriptor_pool_sizes, descriptor_type)) {
        VkDescriptorPoolSize* pool_size = zest_map_at_key(builder->descriptor_pool_sizes, descriptor_type);
        pool_size->descriptorCount += ZEST_MAX_FIF;
    }
    else {
        VkDescriptorPoolSize pool_size;
        pool_size.type = descriptor_type;
        pool_size.descriptorCount = ZEST_MAX_FIF;
        zest_map_insert_key(builder->descriptor_pool_sizes, descriptor_type, pool_size);
    }
}

void zest_AddComputeBufferForBinding(zest_compute_builder_t* builder, zest_descriptor_buffer buffer) {
    zest_descriptor_infos_for_binding_t info = { 0 };
    for (ZEST_EACH_FIF_i) {
        info.descriptor_buffer_info[i] = buffer->descriptor_info[i];
    }
    info.buffer = buffer;
    info.all_frames_in_flight = buffer->all_frames_in_flight;
    zest_vec_push(builder->descriptor_infos, info);
}

void zest_AddComputeImageForBinding(zest_compute_builder_t* builder, zest_texture texture) {
    zest_descriptor_infos_for_binding_t info = { 0 };
    for (ZEST_EACH_FIF_i) {
        info.descriptor_image_info[i] = texture->descriptor;
    }
    info.texture = texture;
    info.all_frames_in_flight = ZEST_FALSE;
    zest_vec_push(builder->descriptor_infos, info);
}

zest_index zest_AddComputeShader(zest_compute_builder_t* builder, const char* path, const char* prefix) {
    zest_text path_text = { 0 };
    if (!prefix) {
        zest_SetText(&path_text, path);
    }
    else {
        zest_SetTextf(&path_text, "%s%s", prefix, path);
    }
    zest_vec_push(builder->shader_names, path_text);
    return zest_vec_last_index(builder->shader_names);
}

void zest_SetComputePushConstantSize(zest_compute_builder_t* builder, zest_uint size) {
    assert(size && size <= 256);        //push constants must be within 0 and 256 bytes in size
    builder->push_constant_size = size;
}

void zest_SetComputeDescriptorUpdateCallback(zest_compute_builder_t* builder, void(*descriptor_update_callback)(zest_compute compute)) {
    builder->descriptor_update_callback = descriptor_update_callback;
}

void zest_SetComputeCommandBufferUpdateCallback(zest_compute_builder_t* builder, void(*command_buffer_update_callback)(zest_compute compute, VkCommandBuffer command_buffer)) {
    builder->command_buffer_update_callback = command_buffer_update_callback;
}

void zest_SetComputeExtraCleanUpCallback(zest_compute_builder_t* builder, void(*extra_cleanup_callback)(zest_compute compute)) {
    builder->extra_cleanup_callback = extra_cleanup_callback;
}

void zest_SetComputeUserData(zest_compute_builder_t* builder, void* data) {
    builder->user_data = data;
}

void zest_MakeCompute(zest_compute_builder_t* builder, zest_compute compute) {
    compute->user_data = builder->user_data;

    VkDescriptorPoolCreateInfo pool_info = { 0 };
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = zest_map_size(builder->descriptor_pool_sizes);
    pool_info.pPoolSizes = builder->descriptor_pool_sizes.data;
    pool_info.maxSets = zest_vec_size(builder->descriptor_infos);

    ZEST_VK_CHECK_RESULT(vkCreateDescriptorPool(ZestDevice->logical_device, &pool_info, &ZestDevice->allocation_callbacks, &compute->descriptor_pool));

    compute->queue = ZestDevice->compute_queue;

    // Create compute pipeline
    // Compute pipelines are created separate from graphics pipelines even if they use the same queue

    for (zest_foreach_i(builder->layout_bindings)) {
        VkDescriptorSetLayoutBinding descriptor_set = builder->layout_bindings[i];
        zest_vec_push(compute->set_layout_bindings, descriptor_set);
    }

    compute->descriptor_set_layout = zest_CreateDescriptorSetLayoutWithBindings(zest_vec_size(compute->set_layout_bindings), compute->set_layout_bindings);

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = { 0 };
    if (builder->push_constant_size) {
        compute->pushConstantRange.offset = 0;
        compute->pushConstantRange.size = builder->push_constant_size;
        compute->pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline_layout_create_info.pPushConstantRanges = &compute->pushConstantRange;
        pipeline_layout_create_info.pushConstantRangeCount = 1;
    }
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = 1;
    pipeline_layout_create_info.pSetLayouts = &compute->descriptor_set_layout;

    ZEST_VK_CHECK_RESULT(vkCreatePipelineLayout(ZestDevice->logical_device, &pipeline_layout_create_info, &ZestDevice->allocation_callbacks, &compute->pipeline_layout));

    VkWriteDescriptorSet* compute_write_descriptor_sets = 0;
    for (ZEST_EACH_FIF_i) {
        zest_AllocateDescriptorSet(compute->descriptor_pool, compute->descriptor_set_layout, &compute->descriptor_set[i]);
        int binding_index = 0;
        for (zest_foreach_j(builder->descriptor_infos)) {
            zest_descriptor_infos_for_binding_t* descriptor_info = &builder->descriptor_infos[j];
            if (descriptor_info->all_frames_in_flight) {
                if (descriptor_info->descriptor_buffer_info[i].buffer) {
                    zest_vec_push(compute_write_descriptor_sets, zest_CreateBufferDescriptorWriteWithType(compute->descriptor_set[i], &descriptor_info->descriptor_buffer_info[i], binding_index++, compute->set_layout_bindings[j].descriptorType));
                }
                else if (descriptor_info->descriptor_image_info[i].sampler) {
                    zest_vec_push(compute_write_descriptor_sets, zest_CreateImageDescriptorWriteWithType(compute->descriptor_set[i], &descriptor_info->descriptor_image_info[i], binding_index++, compute->set_layout_bindings[j].descriptorType));
                }
            }
            else {
                if (descriptor_info->descriptor_buffer_info[0].buffer) {
                    zest_vec_push(compute_write_descriptor_sets, zest_CreateBufferDescriptorWriteWithType(compute->descriptor_set[i], &descriptor_info->descriptor_buffer_info[0], binding_index++, compute->set_layout_bindings[j].descriptorType));
                }
                else if (descriptor_info->descriptor_image_info[0].sampler) {
                    zest_vec_push(compute_write_descriptor_sets, zest_CreateImageDescriptorWriteWithType(compute->descriptor_set[i], &descriptor_info->descriptor_image_info[0], binding_index++, compute->set_layout_bindings[j].descriptorType));
                }
            }
        }
        zest_UpdateDescriptorSet(compute_write_descriptor_sets);
        zest_vec_clear(compute_write_descriptor_sets);
    }
    zest_vec_free(compute_write_descriptor_sets);
    compute->descriptor_infos = builder->descriptor_infos;

    // Create compute shader pipelines
    VkComputePipelineCreateInfo compute_pipeline_create_info = { 0 };
    compute_pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_create_info.layout = compute->pipeline_layout;
    compute_pipeline_create_info.flags = 0;

    // One pipeline for each effect
    compute->shader_names = builder->shader_names;
    VkShaderModule shader_module = { 0 };
    for (zest_foreach_i(compute->shader_names)) {
        zest_text filename = compute->shader_names[i];
        char* code = zest_ReadEntireFile(filename.str, ZEST_FALSE);

        if (zest_map_valid_name(ZestRenderer->shaders, filename.str)) {
            zest_shader shader = *zest_map_at(ZestRenderer->shaders, filename.str);
            shader_module = zest__create_shader_module(shader->spv);
        }
        else {
            zest_shader vert_shader = zest_AddShaderFromSPVFile(filename.str, shaderc_compute_shader);
            ZEST_ASSERT(vert_shader);        //Failed to load the shader file, make sure it exists at the location
            shader_module = zest__create_shader_module(vert_shader->spv);
        }

        VkPipelineShaderStageCreateInfo compute_shader_stage_info = { 0 };
        compute_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compute_shader_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compute_shader_stage_info.module = shader_module;
        compute_shader_stage_info.pName = "main";
        compute_pipeline_create_info.stage = compute_shader_stage_info;

        VkPipeline pipeline;
        ZEST_VK_CHECK_RESULT(vkCreateComputePipelines(ZestDevice->logical_device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, &ZestDevice->allocation_callbacks, &pipeline));
        zest_vec_push(compute->pipelines, pipeline);
        vkDestroyShaderModule(ZestDevice->logical_device, shader_module, &ZestDevice->allocation_callbacks);
        zest_vec_free(code);
    }

    // Separate command pool as queue family for compute may be different than graphics
    compute->queue_family_index = ZestDevice->compute_queue_family_index;
    VkCommandPoolCreateInfo command_pool_info = { 0 };
    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.queueFamilyIndex = compute->queue_family_index;
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &command_pool_info, &ZestDevice->allocation_callbacks, &compute->command_pool));

    // Create a command buffer for compute operations
    VkCommandBufferAllocateInfo command_buffer_allocate_info = { 0 };
    command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.commandPool = compute->command_pool;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = 1;

    for (ZEST_EACH_FIF_i) {
        ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &command_buffer_allocate_info, &compute->command_buffer[i]));
    }

    // Fence for compute CB sync
    VkFenceCreateInfo fence_create_info = { 0 };
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (ZEST_EACH_FIF_i) {
        ZEST_VK_CHECK_RESULT(vkCreateFence(ZestDevice->logical_device, &fence_create_info, &ZestDevice->allocation_callbacks, &compute->fence[i]));
    }

    compute->group_count_x = 64;
    compute->group_count_y = 1;
    ZEST__FLAG(compute->flags, zest_compute_flag_sync_required);
    ZEST__FLAG(compute->flags, zest_compute_flag_rewrite_command_buffer);

    VkSemaphoreCreateInfo semaphore_info = { 0 };
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (ZEST_EACH_FIF_i) {
        compute->fif_outgoing_semaphore[i] = VK_NULL_HANDLE;
        ZEST_VK_CHECK_RESULT(vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, &ZestDevice->allocation_callbacks, &compute->fif_outgoing_semaphore[i]))
    }

    if (builder->descriptor_update_callback) {
        compute->descriptor_update_callback = builder->descriptor_update_callback;
    }
    if (builder->command_buffer_update_callback) {
        compute->command_buffer_update_callback = builder->command_buffer_update_callback;
    }

    ZEST__FLAG(compute->flags, zest_compute_flag_is_active);
}

void zest_RunCompute(zest_compute compute) {
    vkWaitForFences(ZestDevice->logical_device, 1, &compute->fence[ZEST_FIF], VK_TRUE, UINT64_MAX);
    vkResetFences(ZestDevice->logical_device, 1, &compute->fence[ZEST_FIF]);

    if (ZEST__FLAGGED(compute->flags, zest_compute_flag_rewrite_command_buffer)) {
        VkCommandBufferBeginInfo begin_info = { 0 };
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        ZEST_VK_CHECK_RESULT(vkBeginCommandBuffer(compute->command_buffer[ZEST_FIF], &begin_info));
        ZEST_ASSERT(compute->command_buffer_update_callback);        //You must set a call back for command_buffer_update_callback so that you can build the command buffer for the compute shader
        //if you're going to run the compute shader via this function. See zest_SetComputeCommandBufferUpdateCallback when building your
        //compute shader
        compute->command_buffer_update_callback(compute, compute->command_buffer[ZEST_FIF]);
        vkEndCommandBuffer(compute->command_buffer[ZEST_FIF]);
    }

    VkSubmitInfo compute_submit_info = { 0 };
    compute_submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    compute_submit_info.commandBufferCount = 1;
    compute_submit_info.pCommandBuffers = &compute->command_buffer[ZEST_FIF];

    //Make sure you're ending the command buffer at the end of your command_buffer_update_callback before calling this. Use vkEndCommandBuffer
    ZEST_VK_CHECK_RESULT(vkQueueSubmit(compute->queue, 1, &compute_submit_info, compute->fence[ZEST_FIF]));
}

void zest_WaitForCompute(zest_compute compute) {
    vkWaitForFences(ZestDevice->logical_device, 1, &compute->fence[ZEST_FIF], VK_TRUE, UINT64_MAX);
}

void zest_ComputeAttachRenderTarget(zest_compute compute, zest_render_target render_target) {
    compute->render_target = render_target;
    ZEST__FLAG(compute->flags, zest_compute_flag_has_render_target);
}

void zest_UpdateComputeDescriptors(zest_compute compute) {
    compute->descriptor_update_callback(compute);
}

void zest_UpdateComputeDescriptorInfos(zest_compute compute) {
    for (zest_foreach_j(compute->descriptor_infos)) {
        zest_descriptor_infos_for_binding_t* descriptor_info = &compute->descriptor_infos[j];
        for (ZEST_EACH_FIF_i) {
            zest_index descriptor_fif_index = descriptor_info->all_frames_in_flight ? i : 0;
            if (descriptor_info->descriptor_buffer_info[i].buffer) {
                descriptor_info->descriptor_buffer_info[i] = descriptor_info->buffer->descriptor_info[descriptor_fif_index];
            }
            else if (descriptor_info->descriptor_image_info[i].sampler) {
                descriptor_info->descriptor_image_info[i] = descriptor_info->texture->descriptor;
            }
        }
    }
}

void zest_StandardComputeDescriptorUpdate(zest_compute compute) {
    VkWriteDescriptorSet* compute_write_descriptor_sets = 0;

    zest_UpdateComputeDescriptorInfos(compute);

    for (ZEST_EACH_FIF_i) {
        int binding_index = 0;
        for (zest_foreach_j(compute->descriptor_infos)) {
            zest_descriptor_infos_for_binding_t* descriptor_info = &compute->descriptor_infos[j];
            if (descriptor_info->all_frames_in_flight) {
                if (descriptor_info->descriptor_buffer_info[i].buffer) {
                    zest_vec_push(compute_write_descriptor_sets, zest_CreateBufferDescriptorWriteWithType(compute->descriptor_set[i], &descriptor_info->descriptor_buffer_info[i], binding_index++, compute->set_layout_bindings[j].descriptorType));
                }
                else if (descriptor_info->descriptor_image_info[i].sampler) {
                    zest_vec_push(compute_write_descriptor_sets, zest_CreateImageDescriptorWriteWithType(compute->descriptor_set[i], &descriptor_info->descriptor_image_info[i], binding_index++, compute->set_layout_bindings[j].descriptorType));
                }
            }
            else {
                if (descriptor_info->descriptor_buffer_info[0].buffer) {
                    zest_vec_push(compute_write_descriptor_sets, zest_CreateBufferDescriptorWriteWithType(compute->descriptor_set[i], &descriptor_info->descriptor_buffer_info[0], binding_index++, compute->set_layout_bindings[j].descriptorType));
                }
                else if (descriptor_info->descriptor_image_info[0].sampler) {
                    zest_vec_push(compute_write_descriptor_sets, zest_CreateImageDescriptorWriteWithType(compute->descriptor_set[i], &descriptor_info->descriptor_image_info[0], binding_index++, compute->set_layout_bindings[j].descriptorType));
                }
            }
        }
        zest_UpdateDescriptorSet(compute_write_descriptor_sets);
        zest_vec_clear(compute_write_descriptor_sets);
    }
    zest_vec_free(compute_write_descriptor_sets);
}

void zest__clean_up_compute(zest_compute compute) {
    if (ZEST__NOT_FLAGGED(compute->flags, zest_compute_flag_is_active)) {
        return;
    }
    for (ZEST_EACH_FIF_i) {
        if (compute->fence[i]) {
            vkDestroyFence(ZestDevice->logical_device, compute->fence[i], &ZestDevice->allocation_callbacks);
        }
    }
    for (zest_foreach_i(compute->pipelines)) {
        vkDestroyPipeline(ZestDevice->logical_device, compute->pipelines[i], &ZestDevice->allocation_callbacks);
    }
    if (compute->descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ZestDevice->logical_device, compute->descriptor_set_layout, &ZestDevice->allocation_callbacks);
    }
    vkDestroyPipelineLayout(ZestDevice->logical_device, compute->pipeline_layout, &ZestDevice->allocation_callbacks);
    vkDestroyDescriptorPool(ZestDevice->logical_device, compute->descriptor_pool, &ZestDevice->allocation_callbacks);
    vkDestroyCommandPool(ZestDevice->logical_device, compute->command_pool, &ZestDevice->allocation_callbacks);
    if (ZEST__FLAGGED(compute->flags, zest_compute_flag_sync_required)) {
        for (ZEST_EACH_FIF_i) {
            if (compute->fif_outgoing_semaphore[i])
                vkDestroySemaphore(ZestDevice->logical_device, compute->fif_outgoing_semaphore[i], &ZestDevice->allocation_callbacks);
        }
    }
    if (compute->extra_cleanup_callback) {
        compute->extra_cleanup_callback(compute);
    }

}
//--End Compute shaders

//-- Start debug helpers
const char* zest_DrawFunctionToString(void* function) {
    if (function == zest_RenderDrawRoutinesCallback) {
        return "zest_RenderDrawRoutinesCallback";
    }
    else if (function == zest__render_blank) {
        return "zest__render_blank";
    }
    else if (function == zest_DrawToRenderTargetCallback) {
        return "zest_DrawToRenderTargetCallback";
    }
    else if (function == zest_DrawRenderTargetsToSwapchain) {
        return "zest_DrawRenderTargetsToSwapchain";
    }
    else {
        return "Custom draw function";
    }
}

void zest_OutputQueues() {
    for (zest_map_foreach_i(ZestRenderer->command_queues)) {
        zest_index draw_order = 1;
        zest_command_queue command_queue = *zest_map_at_index(ZestRenderer->command_queues, i);
        printf("Command Queue: %s\n", command_queue->name);
        if (!zest_vec_empty(command_queue->draw_commands)) {
            printf("\tDraw Commands\n");
            for (zest_foreach_j(command_queue->draw_commands)) {
                zest_command_queue_draw_commands draw_commands = command_queue->draw_commands[j];
                printf("\t%i) %s\n", draw_order++, draw_commands->name);
                if (!zest_vec_empty(draw_commands->draw_routines)) {
                    for (zest_foreach_k(draw_commands->draw_routines)) {
                        zest_draw_routine draw_routine = draw_commands->draw_routines[k];
                        printf("\t\t-Draw Routine: %s\n", draw_routine->name);
                    }
                }
                else {
                    printf("\t\t(No draw routines)\n");
                }
                if (draw_commands->render_target != ZEST_NULL && zest_vec_empty(draw_commands->render_targets)) {
                    zest_render_target render_target = draw_commands->render_target;
                    printf("\t\t-Render target: %s\n", render_target->name);
                    if (render_target->post_process_callback) {
                        printf("\t\t\t-Has custom draw commands to render target\n");
                    }
                }
                if (draw_commands->render_pass_function) {
                    printf("\t\t-Draw function: %s\n", zest_DrawFunctionToString(draw_commands->render_pass_function));
                }
                if (!zest_vec_empty(draw_commands->render_targets)) {
                    for (zest_foreach_k(draw_commands->render_targets)) {
                        zest_render_target render_target = draw_commands->render_targets[k];
                        printf("\t\t-Render Target: %s\n", render_target->name);
                    }
                }
            }
        }
        else {
            printf("\t(No draw commands in queue)\n");
        }
        if (!zest_vec_empty(command_queue->compute_commands)) {
            for (zest_foreach_j(command_queue->compute_commands)) {
            }
        }
        else {
            printf("\t(No compute commands in queue)\n");
        }
        printf("\n");
        printf("\t\t\t\t-- * --\n");
        printf("\n");
    }
}

void zest_OutputMemoryUsage() {
    printf("Host Memory Pools\n");
    zest_size total_host_memory = 0;
    zest_size total_device_memory = 0;
    for (int i = 0; i != ZestDevice->memory_pool_count; ++i) {
        printf("\tMemory Pool Size: %zu\n", ZestDevice->memory_pool_sizes[i]);
        total_host_memory += ZestDevice->memory_pool_sizes[i];
    }
    printf("Device Memory Pools\n");
    for (zest_map_foreach_i(ZestRenderer->buffer_allocators)) {
        zest_buffer_allocator buffer_allocator = *zest_map_at_index(ZestRenderer->buffer_allocators, i);
        zest_buffer_usage_t usage = { buffer_allocator->buffer_info.usage_flags, buffer_allocator->buffer_info.property_flags, buffer_allocator->buffer_info.image_usage_flags };
        zest_key usage_key = zest_map_hash_ptr(ZestDevice->pool_sizes, &usage, sizeof(zest_buffer_usage_t));
        zest_buffer_pool_size_t pool_size = *zest_map_at_key(ZestDevice->pool_sizes, usage_key);
        if (buffer_allocator->buffer_info.image_usage_flags) {
            printf("\t%s (%s), Usage: %u, Image Usage: %u\n", "Image", pool_size.name, buffer_allocator->buffer_info.usage_flags, buffer_allocator->buffer_info.image_usage_flags);
        }
        else {
            printf("\t%s (%s), Usage: %u, Properties: %u\n", "Buffer", pool_size.name, buffer_allocator->buffer_info.usage_flags, buffer_allocator->buffer_info.property_flags);
        }
        for (zest_foreach_j(buffer_allocator->memory_pools)) {
            zest_device_memory_pool memory_pool = buffer_allocator->memory_pools[j];
            printf("\t\tMemory Pool Size: %llu\n", memory_pool->size);
            total_device_memory += memory_pool->size;
        }
    }
    printf("Total Host Memory: %zu (%llu MB)\n", total_host_memory, total_host_memory / zloc__MEGABYTE(1));
    printf("Total Device Memory: %zu (%llu MB)\n", total_device_memory, total_device_memory / zloc__MEGABYTE(1));
    printf("\n");
    printf("\t\t\t\t-- * --\n");
    printf("\n");
}

zest_bool zest_SetErrorLogPath(const char* path) {
    ZEST_ASSERT(ZestDevice);    //Have you initialised Zest yet?
    zest_SetTextf(&ZestDevice->log_path, "%s/%s", path, "zest_log.txt");
    FILE* log_file = zest__open_file(ZestDevice->log_path.str, "wb");
    if (!log_file) {
        printf("Could Not Create Log file!");
        return ZEST_FALSE;
    }
    ZEST_LOG(log_file, "Start of Log\n");
    fclose(log_file);
    return ZEST_TRUE;
}
//-- End Debug helpers

//-- Timer functions
zest_timer zest_CreateTimer(double update_frequency) {
    zest_timer_t blank_timer = { 0 };
    zest_timer timer = ZEST__NEW(zest_timer);
    *timer = blank_timer;
    zest_TimerSetUpdateFrequency(timer, update_frequency);
    zest_TimerReset(timer);
    return timer;
}

void zest_FreeTimer(zest_timer timer) {
    ZEST__FREE(timer);
}

void zest_TimerReset(zest_timer timer) {
    timer->start_time = (double)zest_Microsecs();
    timer->current_time = (double)zest_Microsecs();
}

double zest_TimerDeltaTime(zest_timer timer) {
    return timer->delta_time;
}

void zest_TimerTick(zest_timer timer) {
    timer->delta_time = (double)zest_Microsecs() - timer->start_time;
}

double zest_TimerUpdateTime(zest_timer timer) {
    return timer->update_time;
}

double zest_TimerFrameLength(zest_timer timer) {
    return timer->update_tick_length;
}

double zest_TimerAccumulate(zest_timer timer) {
    double new_time = (double)zest_Microsecs();
    double frame_time = fabs(new_time - timer->current_time);
    frame_time = ZEST__MIN(frame_time, ZEST_MICROSECS_SECOND);
    timer->current_time = new_time;

    timer->accumulator_delta = timer->accumulator;
    timer->accumulator += frame_time;
    return frame_time;
}

void zest_TimerUnAccumulate(zest_timer timer) {
    timer->accumulator -= timer->update_tick_length;
    timer->accumulator_delta = timer->accumulator_delta - timer->accumulator;
}

zest_bool zest_TimerDoUpdate(zest_timer timer) {
    return timer->accumulator >= timer->update_tick_length;
}

double zest_TimerLerp(zest_timer timer) {
    return timer->lerp;
}

void zest_TimerSet(zest_timer timer) {
    timer->lerp = timer->accumulator / timer->update_tick_length;
}

void zest_TimerSetUpdateFrequency(zest_timer timer, double frequency) {
    timer->update_frequency = frequency;
    timer->update_tick_length = ZEST_MICROSECS_SECOND / timer->update_frequency;
    timer->update_time = 1.f / timer->update_frequency;
    timer->ticker = zest_Microsecs() - timer->update_tick_length;
}

double zest_TimerUpdateFrequency(zest_timer timer) {
    return timer->update_frequency;
}
//-- End Timer Functions

