#include "zest.h"
#define ZLOC_IMPLEMENTATION
#define ZEST_SPIRV_REFLECT_IMPLENTATION
//#define ZLOC_OUTPUT_ERROR_MESSAGES
//#define ZLOC_EXTRA_DEBUGGING
#define STB_RECT_PACK_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "lib_bundle.h"

zest_device_t* ZestDevice = 0;
zest_app_t* ZestApp = 0;
zest_renderer_t* ZestRenderer = 0;
zest_storage_t* zest__globals = 0;

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

    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {  // Left button
        ZestApp->mouse_button |= 1;
    }
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {  // Right button
        ZestApp->mouse_button |= 2;
    }
    if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) {  // Middle button
        ZestApp->mouse_button |= 4;
    }

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

    window->magic = zest_INIT_MAGIC;
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

    window->magic = zest_INIT_MAGIC;
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
    ZestApp->mouse_button = 0;
    int left = glfwGetMouseButton(ZestApp->window->window_handle, GLFW_MOUSE_BUTTON_LEFT);
    if (left == GLFW_PRESS) {
        ZestApp->mouse_button |= 1;
    }

    int right = glfwGetMouseButton(ZestApp->window->window_handle, GLFW_MOUSE_BUTTON_RIGHT);
    if (right == GLFW_PRESS) {
        ZestApp->mouse_button |= 2;
    }

    int middle = glfwGetMouseButton(ZestApp->window->window_handle, GLFW_MOUSE_BUTTON_MIDDLE);
    if (middle == GLFW_PRESS) {
        ZestApp->mouse_button |= 4;
    }
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

bool zest__create_folder(const char *path) {
    int result = ZEST_CREATE_DIR(path);
    if (result == 0) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Folder created successfully: %s", path);
        return true;
    } else {
        if (result == -1) {
            return true;
        } else {
            char buffer[100];
            if (zest_strerror(buffer, sizeof(buffer), result) != 0) {
				ZEST_APPEND_LOG(ZestDevice->log_path.str, "Error creating folder: %s (Error: Unknown)", path);
            } else {
				ZEST_APPEND_LOG(ZestDevice->log_path.str, "Error creating folder: %s (Error: %s)", path, buffer);
            }
            return false;
        }
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

zest_uint zest_Pack16bit2SNorm(float x, float y) {
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

zest_u64 zest_Pack16bit4SNorm(float x, float y, float z, float w) {
    union
    {
        signed short in[4];
        zest_u64 out;
    } u;

    x = x * 32767.f;
    y = y * 32767.f;
    z = z * 32767.f;
    w = w * 32767.f;

    u.in[0] = (signed short)x;
    u.in[1] = (signed short)y;
    u.in[2] = (signed short)z;
    u.in[3] = (signed short)w;

    return u.out;
}

zest_uint zest_FloatToHalf(float f) {
    union {
        float f;
        zest_uint u;
    } u = { f };

    zest_uint sign = (u.u & 0x80000000) >> 16;
    zest_uint rest = u.u & 0x7FFFFFFF;

    if (rest >= 0x47800000) {  // Infinity or NaN
        return (uint16_t)(sign | 0x7C00 | (rest > 0x7F800000 ? 0x0200 : 0));
    }

    if (rest < 0x38800000) {  // Subnormal or zero
        rest = (rest & 0x007FFFFF) | 0x00800000;  // Add leading 1
        int shift = 113 - (rest >> 23);
        rest = (rest << 14) >> shift;
        return (uint16_t)(sign | rest);
    }

    zest_uint exponent = rest & 0x7F800000;
    zest_uint mantissa = rest & 0x007FFFFF;
    return (uint16_t)(sign | ((exponent - 0x38000000) >> 13) | (mantissa >> 13));
}

zest_u64 zest_Pack16bit4SFloat(float x, float y, float z, float w) {
    uint16_t hx = zest_FloatToHalf(x);
    uint16_t hy = zest_FloatToHalf(y);
    uint16_t hz = zest_FloatToHalf(z);
    uint16_t hw = zest_FloatToHalf(w);
    return ((zest_u64)hx) |
        ((zest_u64)hy << 16) |
        ((zest_u64)hz << 32) |
        ((zest_u64)hw << 48);
}

zest_uint zest_Pack16bit2SScaled(float x, float y, float max_value) {
    int16_t x_scaled = (int16_t)(x * 32767.0f / max_value);
    int16_t y_scaled = (int16_t)(y * 32767.0f / max_value);
    return ((zest_uint)x_scaled) | ((zest_uint)y_scaled << 16);
}

zest_u64 zest_Pack16bit4SScaled(float x, float y, float z, float w, float max_value_xy, float max_value_zw) {
    int16_t x_scaled = (int16_t)(x * 32767.0f / max_value_xy);
    int16_t y_scaled = (int16_t)(y * 32767.0f / max_value_xy);
    int16_t z_scaled = (int16_t)(z * 32767.0f / max_value_zw);
    int16_t w_scaled = (int16_t)(w * 32767.0f / max_value_zw);
    return ((zest_u64)x_scaled) | ((zest_u64)y_scaled << 16) | ((zest_u64)z_scaled << 32) | ((zest_u64)w_scaled << 48);
}

zest_u64 zest_Pack16bit4SScaledZWPacked(float x, float y, zest_uint zw, float max_value_xy) {
    int16_t x_scaled = (int16_t)(x * 32767.0f / max_value_xy);
    int16_t y_scaled = (int16_t)(y * 32767.0f / max_value_xy);
    return ((zest_u64)x_scaled) | ((zest_u64)y_scaled << 16) | ((zest_u64)zw << 32);
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

zest_vec4 zest_LerpVec4(zest_vec4* from, zest_vec4* to, float lerp) {
    return zest_AddVec4(zest_ScaleVec4(*to, lerp), zest_ScaleVec4(*from, (1.f - lerp)));
}

zest_vec3 zest_LerpVec3(zest_vec3* from, zest_vec3* to, float lerp) {
    return zest_AddVec3(zest_ScaleVec3(*to, lerp), zest_ScaleVec3(*from, (1.f - lerp)));
}

zest_vec2 zest_LerpVec2(zest_vec2* from, zest_vec2* to, float lerp) {
    return zest_AddVec2(zest_ScaleVec2(*to, lerp), zest_ScaleVec2(*from, (1.f - lerp)));
}

float zest_Lerp(float from, float to, float lerp) {
    return to * lerp + from * (1.f - lerp);
}

//  --End Math

// Initialisation and destruction
zest_bool zest_Initialise(zest_create_info_t* info) {
    void* memory_pool = ZEST__ALLOCATE_POOL(info->memory_pool_size);

    ZEST_ASSERT(memory_pool);    //unable to allocate initial memory pool

    zloc_allocator* allocator = zloc_InitialiseAllocatorWithPool(memory_pool, info->memory_pool_size);
    ZestDevice = zloc_Allocate(allocator, sizeof(zest_device_t));
    ZestDevice->allocator = allocator;

    ZestApp = zloc_Allocate(allocator, sizeof(zest_app_t));
    ZestRenderer = zloc_Allocate(allocator, sizeof(zest_renderer_t));
    zest__globals = zloc_Allocate(allocator, sizeof(zest_storage_t));
    memset(zest__globals, 0, sizeof(zest_storage_t));
    zest__globals->thread_count = info->thread_count;
    if (info->thread_count > 0) {
        zest__initialise_thread_queues(&zest__globals->thread_queues);
        for (int i = 0; i < info->thread_count; i++) {
            if (!zest__create_worker_thread(zest__globals, i)) {
                ZEST_ASSERT(0);     //Unable to create thread!
            }
        }
    }
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
    ZestDevice->setup_info = *info;
    if (info->log_path) {
        zest_SetErrorLogPath(info->log_path);
    }
    ZestRenderer->destroy_window_callback = info->destroy_window_callback;
    ZestRenderer->get_window_size_callback = info->get_window_size_callback;
    ZestRenderer->poll_events_callback = info->poll_events_callback;
    ZestRenderer->add_platform_extensions_callback = info->add_platform_extensions_callback;
    ZestRenderer->create_window_callback = info->create_window_callback;
    ZestRenderer->create_window_surface_callback = info->create_window_surface_callback;
	zest__initialise_app(info);
    if (zest__initialise_device(info)) {
        zest__initialise_renderer(info);
        return ZEST_TRUE;
    }
    return ZEST_FALSE;
}

void zest_Start() {
    zest__main_loop();
    zest__destroy();
}

zest_bool zest__initialise_device() {
    if (zest__create_instance()) {
        if (zest__validation_layers_are_enabled()) {
            zest__setup_validation();
        }
        zest__pick_physical_device();
        if (zest__create_logical_device()) {
            zest__set_limit_data();
            zest__set_default_pool_sizes();
            return ZEST_TRUE;
        } else {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Unable to create logical device, program will now end.");
            return ZEST_FALSE;
        }
    }
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Unable to create Vulkan instance, program will now end.");
    return ZEST_FALSE;
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
zest_bool zest__create_instance() {
    if (zest__validation_layers_are_enabled()) {
        zest_bool validation_support = zest__check_validation_layer_support();
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Checking for validation support");
        ZEST_ASSERT(validation_support);
    }

    zest_uint instance_api_version_supported;
    if (vkEnumerateInstanceVersion(&instance_api_version_supported) == VK_SUCCESS) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "System supports Vulkan API Version: %d.%d.%d",
             VK_API_VERSION_MAJOR(instance_api_version_supported),
             VK_API_VERSION_MINOR(instance_api_version_supported),
             VK_API_VERSION_PATCH(instance_api_version_supported));
        if (instance_api_version_supported < VK_API_VERSION_1_2) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Zest requires minumum Vulkan version: 1.2+. Version found: %u", instance_api_version_supported);
            return ZEST_FALSE;
        }
    } else {
        ZEST_PRINT_WARNING("Vulkan 1.0 detected (vkEnumerateInstanceVersion not found). Zest requires Vulkan 1.2+.");
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Vulkan 1.0 detected (vkEnumerateInstanceVersion not found). Zest requiresVulkan 1.2+.")
        return ZEST_FALSE;
    }

    VkApplicationInfo app_info = { 0 };
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Zest";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.apiVersion = VK_API_VERSION_1_2;
    ZestDevice->api_version = app_info.apiVersion;

    VkInstanceCreateInfo create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
#ifdef ZEST_PORTABILITY_ENUMERATION
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Flagging for enumerate portability on MACOS");
    create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    zest__get_required_extensions();
    zest_uint extension_count = zest_vec_size(ZestDevice->extensions);
    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = (const char**)ZestDevice->extensions;

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = { 0 };
	VkValidationFeatureEnableEXT *enabled_validation_features = 0;
    if (zest__validation_layers_are_enabled()) {
        create_info.enabledLayerCount = (zest_uint)zest__validation_layer_count;
        create_info.ppEnabledLayerNames = zest_validation_layers;
        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_create_info.pfnUserCallback = zest_debug_callback;

        if (zest__validation_layers_with_sync_are_enabled()) {
            zest_vec_push(enabled_validation_features, VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
            // Potentially add others like VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT for more advice
            VkValidationFeaturesEXT validation_features_ext = { 0 };
            validation_features_ext.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
            validation_features_ext.enabledValidationFeatureCount = zest_vec_size(enabled_validation_features);
            validation_features_ext.pEnabledValidationFeatures = enabled_validation_features;
			debug_create_info.pNext = &validation_features_ext;
        }

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

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Validating Vulkan Instance");
    ZEST_VK_CHECK_RESULT(result);

    ZEST__FREE(available_extensions);
    ZestRenderer->create_window_surface_callback(ZestApp->window);

    zest_vec_free(enabled_validation_features);

    return ZEST_TRUE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL zest_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    if (ZestDevice->log_path.str) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Validation Layer: %s", pCallbackData->pMessage);
    }
    if(ZEST__FLAGGED(ZestApp->create_info.flags, zest_init_flag_log_validation_errors_to_console)) {
        ZEST_PRINT("Validation Layer: %s", pCallbackData->pMessage);
        ZEST_PRINT("-------------------------------------------------------");
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

    if (zest__validation_layers_are_enabled()) {
        zest_AddInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    //zest_AddInstanceExtension(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME);
#ifdef ZEST_PORTABILITY_ENUMERATION
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Adding enumerate portability extension");
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

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Found %i devices", device_count);
    for (int i = 0; i != device_count; ++i) {
        zest__log_device_name(devices[i]);
    }
    ZestDevice->physical_device = VK_NULL_HANDLE;

    //Prioritise discrete GPUs when picking physical device
    if (device_count == 1 && zest__is_device_suitable(devices[0])) {
        if (zest__device_is_discrete_gpu(devices[0])) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "The one device found is suitable and is a discrete GPU");
        }
        else {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "The one device found is suitable");
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
                    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Found suitable device:");
                    zest__log_device_name(devices[i]);
                    ZestDevice->physical_device = devices[i];
                    break;
                }
            }
        }
    }

    if (ZestDevice->physical_device == VK_NULL_HANDLE) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Could not find a suitable device!");
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
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Max Memory Allocation Count: %i", ZestDevice->properties.limits.maxMemoryAllocationCount);
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Memory available in GPU:");
    for (int i = 0; i != ZestDevice->memory_properties.memoryHeapCount; ++i) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "    Heap flags: %i, Size: %llu", ZestDevice->memory_properties.memoryHeaps[i].flags, ZestDevice->memory_properties.memoryHeaps[i].size);
    }

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Memory types mapping in GPU:");
    for (int i = 0; i != ZestDevice->memory_properties.memoryTypeCount; ++i) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "    %i) Heap Index: %i, Property Flags: %i", i, ZestDevice->memory_properties.memoryTypes[i].heapIndex, ZestDevice->memory_properties.memoryTypes[i].propertyFlags);
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
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "\t%s", properties.deviceName);
}

zest_bool zest__device_is_discrete_gpu(VkPhysicalDevice physical_device) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    return properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
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

zest_bool zest__create_logical_device() {
    zest_queue_family_indices indices = { 0 };

    zest_uint queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ZestDevice->physical_device, &queue_family_count, ZEST_NULL);

    zest_vec_resize(ZestDevice->queue_families, queue_family_count);
    //VkQueueFamilyProperties queue_families[10];
    vkGetPhysicalDeviceQueueFamilyProperties(ZestDevice->physical_device, &queue_family_count, ZestDevice->queue_families);

    zest_uint graphics_candidate = ZEST_INVALID;
    zest_uint compute_candidate = ZEST_INVALID;
    zest_uint transfer_candidate = ZEST_INVALID;

	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Iterate available queues:");
    zest_vec_foreach(i, ZestDevice->queue_families) {
        VkQueueFamilyProperties properties = ZestDevice->queue_families[i];

        zest_text_t queue_flags = zest__vulkan_queue_flags_to_string(properties.queueFlags);
		ZEST_APPEND_LOG(ZestDevice->log_path.str, "Index: %i) %s, Queue count: %i", i, queue_flags.str, properties.queueCount);
        zest_FreeText(&queue_flags);

        // Is it a dedicated transfer queue?
        if ((properties.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(properties.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Found a dedicated transfer queue on index %i", i);
            transfer_candidate = i;
        }

        // Is it a dedicated compute queue?
        if ((properties.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Found a dedicated compute queue on index %i", i);
            compute_candidate = i;
        }
    }

    zest_vec_foreach(i, ZestDevice->queue_families) {
        VkQueueFamilyProperties properties = ZestDevice->queue_families[i];
        // Find the primary graphics queue (must support presentation!)
        VkBool32 present_support = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(ZestDevice->physical_device, i, ZestApp->window->surface, &present_support);

        if ((properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_support) {
            if (graphics_candidate == ZEST_INVALID) {
                graphics_candidate = i;
            }
        }

        if (compute_candidate == ZEST_INVALID) {
            if (properties.queueFlags & VK_QUEUE_COMPUTE_BIT) {
				ZEST_APPEND_LOG(ZestDevice->log_path.str, "Set compute queue to index %i", i);
                compute_candidate = i;
            }
        }

        if (transfer_candidate == ZEST_INVALID) {
            if (properties.queueFlags & VK_QUEUE_TRANSFER_BIT) {
				ZEST_APPEND_LOG(ZestDevice->log_path.str, "Set transfer queue to index %i", i);
                transfer_candidate = i;
            }
        }
    }

	if (graphics_candidate == ZEST_INVALID) {
		ZEST_APPEND_LOG(ZestDevice->log_path.str, "Fatal Error: Unable to find graphics queue/present support!");
		return 0;
	}

    float queue_priority = 0.0f;
    VkDeviceQueueCreateInfo queue_create_infos[3];

    int queue_create_count = 0;
    // Graphics queue
    {
        indices.graphics_family_index = graphics_candidate;
        VkDeviceQueueCreateInfo queue_info = { 0 };
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = indices.graphics_family_index;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &queue_priority;
        queue_create_infos[0] = queue_info;
        queue_create_count++;
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Graphics queue index set to: %i", indices.graphics_family_index);
		zest_map_insert_key(ZestDevice->queue_names, indices.graphics_family_index, "Graphics Queue");
    }

    // Dedicated compute queue
    {
        indices.compute_family_index = compute_candidate;
        if (indices.compute_family_index != indices.graphics_family_index)
        {
            // If compute family index differs, we need an additional queue create info for the compute queue
            VkDeviceQueueCreateInfo queue_info = { 0 };
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info.queueFamilyIndex = indices.compute_family_index;
            queue_info.queueCount = 1;
            queue_info.pQueuePriorities = &queue_priority;
            queue_create_infos[1] = queue_info;
            queue_create_count++;
			zest_map_insert_key(ZestDevice->queue_names, indices.compute_family_index, "Compute Queue");
        }
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Compute queue index set to: %i", indices.compute_family_index);
    }

    //Dedicated transfer queue
    {
        indices.transfer_family_index = transfer_candidate;
        if (indices.transfer_family_index != indices.graphics_family_index && indices.transfer_family_index != indices.compute_family_index)
        {
            // If transfer_index family index differs, we need an additional queue create info for the transfer queue
            VkDeviceQueueCreateInfo queue_info = { 0 };
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info.queueFamilyIndex = indices.transfer_family_index;
            queue_info.queueCount = 1;
            queue_info.pQueuePriorities = &queue_priority;
            queue_create_infos[2] = queue_info;
            queue_create_count++;
			zest_map_insert_key(ZestDevice->queue_names, indices.transfer_family_index, "Transfer Queue");
        }
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Transfer queue index set to: %i", indices.transfer_family_index);
    }
    
	zest_map_insert_key(ZestDevice->queue_names, VK_QUEUE_FAMILY_IGNORED, "Ignored");

	// Check for bindless support
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptor_indexing_features = { 0 };
    descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    // No pNext needed for querying this specific struct usually, unless querying other chained features too
    VkPhysicalDeviceFeatures2 physical_device_features2 = { 0 };
    physical_device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physical_device_features2.pNext = &descriptor_indexing_features; // Chain it
    vkGetPhysicalDeviceFeatures2(ZestDevice->physical_device, &physical_device_features2);
    if (!descriptor_indexing_features.shaderSampledImageArrayNonUniformIndexing ||
        !descriptor_indexing_features.descriptorBindingPartiallyBound ||
        !descriptor_indexing_features.runtimeDescriptorArray) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Fatal Error: Required descriptor indexing features not supported by this GPU!");
        return 0;
    }

    VkPhysicalDeviceFeatures device_features = { 0 };
    device_features.samplerAnisotropy = VK_TRUE;
    device_features.multiDrawIndirect = VK_TRUE;
    device_features.shaderInt64 = VK_TRUE;
    //device_features.wideLines = VK_TRUE;
    //device_features.dualSrcBlend = VK_TRUE;
    //device_features.vertexPipelineStoresAndAtomics = VK_TRUE;
    if (ZEST__FLAGGED(ZestDevice->setup_info.flags, zest_init_flag_enable_fragment_stores_and_atomics)) device_features.fragmentStoresAndAtomics = VK_TRUE;
    VkPhysicalDeviceVulkan12Features device_features_12 = { 0 };
    device_features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    //For bindless descriptor sets:
	device_features_12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    device_features_12.shaderStorageImageArrayNonUniformIndexing = VK_TRUE; 
    device_features_12.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE; 
    device_features_12.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE; 
    device_features_12.descriptorBindingPartiallyBound = VK_TRUE; 
    device_features_12.descriptorBindingVariableDescriptorCount = VK_TRUE; 
    device_features_12.runtimeDescriptorArray = VK_TRUE; 
    device_features_12.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
    device_features_12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    device_features_12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    device_features_12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    device_features_12.descriptorBindingUniformTexelBufferUpdateAfterBind = VK_TRUE;
    device_features_12.descriptorBindingStorageTexelBufferUpdateAfterBind = VK_TRUE;
    device_features_12.bufferDeviceAddress = VK_TRUE;
    device_features_12.timelineSemaphore = VK_TRUE;

    VkDeviceCreateInfo create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pEnabledFeatures = &device_features;
    create_info.queueCreateInfoCount = queue_create_count;
    create_info.pQueueCreateInfos = queue_create_infos;
    create_info.enabledExtensionCount = zest__required_extension_names_count;
    create_info.ppEnabledExtensionNames = zest_required_extensions;
	create_info.pNext = &device_features_12;

    if (ZEST__FLAGGED(ZestDevice->setup_info.flags, zest_init_flag_enable_validation_layers)) {
        create_info.enabledLayerCount = zest__validation_layer_count;
        create_info.ppEnabledLayerNames = zest_validation_layers;
    }
    else {
        create_info.enabledLayerCount = 0;
    }

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating logical device");
    ZEST_VK_CHECK_RESULT(vkCreateDevice(ZestDevice->physical_device, &create_info, &ZestDevice->allocation_callbacks, &ZestDevice->logical_device));

    vkGetDeviceQueue(ZestDevice->logical_device, indices.graphics_family_index, 0, &ZestDevice->graphics_queue.vk_queue);
    vkGetDeviceQueue(ZestDevice->logical_device, indices.compute_family_index, 0, &ZestDevice->compute_queue.vk_queue);
    vkGetDeviceQueue(ZestDevice->logical_device, indices.transfer_family_index, 0, &ZestDevice->transfer_queue.vk_queue);

    ZestDevice->graphics_queue_family_index = indices.graphics_family_index;
    ZestDevice->transfer_queue_family_index = indices.transfer_family_index;
    ZestDevice->compute_queue_family_index = indices.compute_family_index;

    zest_ForEachFrameInFlight(fif) {
		//Create the timeline semaphore pool
		VkSemaphoreTypeCreateInfo timeline_create_info = { 0 };
		timeline_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
		timeline_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		timeline_create_info.initialValue = 0;
        VkSemaphoreCreateInfo semaphore_info = { 0 };
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphore_info.pNext = &timeline_create_info;

		vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, &ZestDevice->allocation_callbacks, &ZestDevice->graphics_queue.semaphore[fif]);
		vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, &ZestDevice->allocation_callbacks, &ZestDevice->compute_queue.semaphore[fif]);
		vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, &ZestDevice->allocation_callbacks, &ZestDevice->transfer_queue.semaphore[fif]);

        ZestDevice->graphics_queue.current_count[fif] = 0;
        ZestDevice->compute_queue.current_count[fif] = 0;
        ZestDevice->transfer_queue.current_count[fif] = 0;
    }

    VkCommandPoolCreateInfo cmd_info_pool = { 0 };
    cmd_info_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_info_pool.queueFamilyIndex = ZestDevice->graphics_queue_family_index;
    cmd_info_pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating command pools");
    ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &cmd_info_pool, &ZestDevice->allocation_callbacks, &ZestDevice->command_pool));
    ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &cmd_info_pool, &ZestDevice->allocation_callbacks, &ZestDevice->one_time_command_pool));

    return 1;
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

    //Indirect draw buffers that are host visible
    usage.image_flags = 0;
    usage.usage_flags = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    zest_SetDeviceBufferPoolSize("Host Indirect Draw Buffers", usage.usage_flags, usage.property_flags, 64, zloc__MEGABYTE(1));

    //Indirect draw buffers
    usage.image_flags = 0;
    usage.usage_flags = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    usage.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    zest_SetDeviceBufferPoolSize("Host Indirect Draw Buffers", usage.usage_flags, usage.property_flags, 64, zloc__MEGABYTE(1));

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
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Set device pool sizes");
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

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create window with dimensions: %i, %i", create_info->screen_width, create_info->screen_height);
    ZestApp->window = ZestRenderer->create_window_callback(create_info->screen_x, create_info->screen_y, create_info->screen_width, create_info->screen_height, ZEST__FLAGGED(create_info->flags, zest_init_flag_maximised), "Zest");
}

void zest__end_thread(zest_work_queue_t *queue, void *data) {
    return;
}

void zest__destroy(void) {
    zest_WaitForIdleDevice();
    zest__globals->thread_queues.end_all_threads = true;
    zest_work_queue_t end_queue = { 0 };
    zest_uint thread_count = zest__globals->thread_count;
    while (thread_count > 0) {
        zest__add_work_queue_entry(&end_queue, 0, zest__end_thread);
        zest__complete_all_work(&end_queue);
        thread_count--;
    }
    for (int i = 0; i != zest__globals->thread_count; ++i) {
        zest__cleanup_thread(zest__globals, i);
    }
    zest__cleanup_renderer();
    zest__cleanup_device();
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

zest_bool zest__validation_layers_are_enabled(void) {
    return ZEST__FLAGGED(ZestDevice->setup_info.flags, zest_init_flag_enable_validation_layers);
}

zest_bool zest__validation_layers_with_sync_are_enabled(void) {
    return ZEST__FLAGGED(ZestDevice->setup_info.flags, zest_init_flag_enable_validation_layers_with_sync);
}

void zest__do_scheduled_tasks(void) {
    zloc_ResetLinearAllocator(ZestRenderer->render_graph_allocator);

    if (zest_vec_size(ZestRenderer->deferred_resource_freeing_list.buffers[ZEST_FIF])) {
        zest_vec_foreach(i, ZestRenderer->deferred_resource_freeing_list.buffers[ZEST_FIF]) {
            zest_buffer buffer = ZestRenderer->deferred_resource_freeing_list.buffers[ZEST_FIF][i];
            zest_FreeBuffer(buffer);
        }
		zest_vec_clear(ZestRenderer->deferred_resource_freeing_list.buffers[ZEST_FIF]);
    }

    if (zest_vec_size(ZestRenderer->deferred_resource_freeing_list.binding_indexes[ZEST_FIF])) {
        zest_vec_foreach(i, ZestRenderer->deferred_resource_freeing_list.binding_indexes[ZEST_FIF]) {
            zest_binding_index_for_release_t index = ZestRenderer->deferred_resource_freeing_list.binding_indexes[ZEST_FIF][i];
            zest__release_bindless_index(index.layout, index.binding_number, index.binding_index);
        }
		zest_vec_clear(ZestRenderer->deferred_resource_freeing_list.binding_indexes[ZEST_FIF]);
    }

    if (zest_vec_size(ZestRenderer->deferred_resource_freeing_list.images[ZEST_FIF])) {
        zest_vec_foreach(i, ZestRenderer->deferred_resource_freeing_list.images[ZEST_FIF]) {
            zest_image_buffer_t *image_buffer = &ZestRenderer->deferred_resource_freeing_list.images[ZEST_FIF][i];
            zest_FreeBuffer(image_buffer->buffer);
            vkDestroyImage(ZestDevice->logical_device, image_buffer->image, &ZestDevice->allocation_callbacks);
            vkDestroyImageView(ZestDevice->logical_device, image_buffer->base_view, &ZestDevice->allocation_callbacks);
            if (zest_vec_size(image_buffer->mip_views)) {
                zest_vec_foreach(j, image_buffer->mip_views) {
					vkDestroyImageView(ZestDevice->logical_device, image_buffer->mip_views[j], &ZestDevice->allocation_callbacks);
                }
                zest_vec_free(image_buffer->mip_views);
            }
        }
		zest_vec_clear(ZestRenderer->deferred_resource_freeing_list.images[ZEST_FIF]);
    }

    if (zest_vec_size(ZestRenderer->deferred_resource_freeing_list.textures[ZEST_FIF])) {
        zest_vec_foreach(i, ZestRenderer->deferred_resource_freeing_list.textures[ZEST_FIF]) {
            zest_texture texture = ZestRenderer->deferred_resource_freeing_list.textures[ZEST_FIF][i];
			zest__cleanup_texture(texture);
			zest__free_all_texture_images(texture);
			zest_vec_free(texture->buffer_copy_regions);
			zest_FreeText(&texture->name);
            ZEST__FREE(texture);
        }
		zest_vec_clear(ZestRenderer->deferred_resource_freeing_list.textures[ZEST_FIF]);
    }

    zest_vec_foreach(i, ZestRenderer->old_frame_buffers[ZEST_FIF]) {
        VkFramebuffer frame_buffer = ZestRenderer->old_frame_buffers[ZEST_FIF][i];
		vkDestroyFramebuffer(ZestDevice->logical_device, frame_buffer, &ZestDevice->allocation_callbacks);
    }
    zest_vec_clear(ZestRenderer->old_frame_buffers[ZEST_FIF]);

    zest_vec_foreach(i, ZestRenderer->used_semaphores[ZEST_FIF]) {
        VkSemaphore semaphore = ZestRenderer->used_semaphores[ZEST_FIF][i];
        zest_vec_push(ZestRenderer->free_semaphores, semaphore);
    }
    zest_vec_clear(ZestRenderer->used_semaphores[ZEST_FIF]);

    zest_vec_foreach(i, ZestRenderer->used_graphics_command_buffers[ZEST_FIF]) {
        VkCommandBuffer command_buffer = ZestRenderer->used_graphics_command_buffers[ZEST_FIF][i];
        vkResetCommandBuffer(command_buffer, 0);
        zest_vec_push(ZestRenderer->command_buffers.free_graphics, command_buffer);
    }
    zest_vec_clear(ZestRenderer->used_graphics_command_buffers[ZEST_FIF]);

    zest_vec_foreach(i, ZestRenderer->used_compute_command_buffers[ZEST_FIF]) {
        VkCommandBuffer command_buffer = ZestRenderer->used_compute_command_buffers[ZEST_FIF][i];
        vkResetCommandBuffer(command_buffer, 0);
        zest_vec_push(ZestRenderer->command_buffers.free_compute, command_buffer);
    }
    zest_vec_clear(ZestRenderer->used_compute_command_buffers[ZEST_FIF]);

    zest_vec_foreach(i, ZestRenderer->used_transfer_command_buffers[ZEST_FIF]) {
        VkCommandBuffer command_buffer = ZestRenderer->used_transfer_command_buffers[ZEST_FIF][i];
        vkResetCommandBuffer(command_buffer, 0);
        zest_vec_push(ZestRenderer->command_buffers.free_transfer, command_buffer);
    }
    zest_vec_clear(ZestRenderer->used_transfer_command_buffers[ZEST_FIF]);

    if (zest_vec_size(ZestRenderer->pipeline_destroy_queue)) {
        for (zest_foreach_i(ZestRenderer->pipeline_destroy_queue)) {
            zest_pipeline_handles_t handles = ZestRenderer->pipeline_destroy_queue[i];
			vkDestroyPipeline(ZestDevice->logical_device, handles.pipeline, &ZestDevice->allocation_callbacks);
			vkDestroyPipelineLayout(ZestDevice->logical_device, handles.pipeline_layout, &ZestDevice->allocation_callbacks);
        }
        zest_vec_clear(ZestRenderer->pipeline_destroy_queue);
    }

    if (zest_vec_size(ZestRenderer->pipeline_recreate_queue)) {
        for (zest_foreach_i(ZestRenderer->pipeline_recreate_queue)) {
            zest_pipeline pipeline = ZestRenderer->pipeline_recreate_queue[i];
            zest_pipeline_handles_t handles = {0};
            handles.pipeline = pipeline->pipeline;
            handles.pipeline_layout = pipeline->pipeline_layout;
            zest_vec_push(ZestRenderer->pipeline_destroy_queue, handles);
            zest__rebuild_pipeline(pipeline);
        }
        zest_vec_clear(ZestRenderer->pipeline_recreate_queue);
    }

    if (zest_map_size(ZestRenderer->render_target_recreate_queue)) {
        bool complete = true;
        for (zest_map_foreach_i(ZestRenderer->render_target_recreate_queue)) {
            zest_render_target render_target = ZestRenderer->render_target_recreate_queue.data[i];
            if (ZEST__FLAGGED(render_target->flags, zest_render_target_flag_single_frame_buffer_only)) {
                if (render_target->frame_buffer_dirty[0]) {
                    zest_RecreateRenderTargetResources(render_target, 0);
                    zest__refresh_render_target_sampler(render_target);
					if (ZEST__FLAGGED(render_target->flags, zest_render_target_flag_multi_mip)) {
						zest__refresh_render_target_mip_samplers(render_target);
					}
                    render_target->recorder->outdated[0] = 1;
                    render_target->frame_buffer_dirty[0] = 0;
                    complete = false;
                }
            } else {
                if (render_target->frame_buffer_dirty[ZEST_FIF]) {
                    zest_RecreateRenderTargetResources(render_target, ZEST_FIF);
                    zest__refresh_render_target_sampler(render_target);
					if (ZEST__FLAGGED(render_target->flags, zest_render_target_flag_multi_mip)) {
						zest__refresh_render_target_mip_samplers(render_target);
					}
                    render_target->recorder->outdated[ZEST_FIF] = 1;
                    render_target->frame_buffer_dirty[ZEST_FIF] = 0;
                    complete = false;
                }
            }
        }
		zest__update_command_queue_viewports();
        if (complete) {
            zest_map_clear(ZestRenderer->render_target_recreate_queue);
        }
    }

    if (zest_map_size(ZestRenderer->rt_sampler_refresh_queue)) {
        for (zest_map_foreach_i(ZestRenderer->rt_sampler_refresh_queue)) {
            zest_render_target render_target = ZestRenderer->rt_sampler_refresh_queue.data[i];
            zest_ForEachFrameInFlight(fif) {
				render_target->recorder->outdated[fif] = 1;
            }
			zest__refresh_render_target_sampler(render_target);
        }
        zest_map_clear(ZestRenderer->rt_sampler_refresh_queue);
    }

    zest_vec_foreach(i, ZestRenderer->staging_buffers) {
        zest_buffer staging_buffer = ZestRenderer->staging_buffers[i];
		zloc_FreeRemote(staging_buffer->buffer_allocator->allocator, staging_buffer);
    }
    zest_vec_clear(ZestRenderer->staging_buffers);
}

void zest__main_loop(void) {
    ZestApp->current_elapsed_time = zest_Microsecs();
    ZEST_ASSERT(ZestApp->update_callback);    //Must define an update callback function
    while (!(ZestApp->flags & zest_app_flag_quit_application)) {

        ZEST_VK_CHECK_RESULT(vkWaitForFences(ZestDevice->logical_device, 1, &ZestRenderer->fif_fence[ZEST_FIF], VK_TRUE, UINT64_MAX));
        vkResetFences(ZestDevice->logical_device, 1, &ZestRenderer->fif_fence[ZEST_FIF]);
		ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_acquired);
		ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_used);

        zest__do_scheduled_tasks();

        ZestApp->mouse_hit = 0;
        zest_mouse_button button_state = ZestApp->mouse_button;

        ZestRenderer->poll_events_callback();

        ZestApp->mouse_hit = button_state & (~ZestApp->mouse_button);

        zest__set_elapsed_time();

        ZestApp->update_callback(ZestApp->current_elapsed, ZestApp->user_data);

        zest__render_target_maintennance();

        //zest__draw_renderer_frame();

        //Cover some cases where a render graph wasn't created or it was but there was nothing render etc., to make sure
        //that the fence is always signalled and another frame can happen
        if (ZEST__FLAGGED(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_acquired) && ZEST__FLAGGED(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_used)) {
            if (ZEST__NOT_FLAGGED(ZestRenderer->flags, zest_renderer_flag_work_was_submitted)) {
                zest__dummy_submit_for_present_only();
            }
			zest__present_frame();
        } else if (ZEST__NOT_FLAGGED(ZestRenderer->flags, zest_renderer_flag_work_was_submitted) && ZEST__FLAGGED(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_acquired)) {
			zest__dummy_submit_for_present_only();
			zest__present_frame();
        } else if(ZEST__NOT_FLAGGED(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_acquired)) {
            zest__dummy_submit_fence_only();
        }

        ZestApp->frame_timer += ZestApp->current_elapsed;
        ZestApp->frame_count++;
        ZestRenderer->total_frame_count++;
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

// --Threading related funcitons
unsigned int zest_HardwareConcurrencySafe(void) {
    unsigned int count = zest_HardwareConcurrency();
    return count > 0 ? count : 1;
}

unsigned int zest_GetDefaultThreadCount(void) {
    unsigned int count = zest_HardwareConcurrency();
    return count > 1 ? count - 1 : 1;
}

#ifdef _WIN32
ZEST_PRIVATE unsigned WINAPI zest__thread_worker(void *arg) {
    zest_queue_processor_t *queue_processor = (zest_queue_processor_t *)arg;
    while (!zest__do_next_work_queue(queue_processor)) {
        // Continue processing
    }
    return 0;
}
#else
void *zest__thread_worker(void *arg) {
    zest_queue_processor_t *queue_processor = (zest_queue_processor_t *)arg;
    while (!zest__do_next_work_queue(queue_processor)) {
        // Continue processing
    }
    return 0;
}
#endif

bool zest__create_worker_thread(zest_storage_t * storage, int thread_index) {
#ifdef _WIN32
    storage->threads[thread_index] = (HANDLE)_beginthreadex(
        NULL,
        0,
        zest__thread_worker,
        &storage->thread_queues,
        0,
        NULL
    );
    return storage->threads[thread_index] != NULL;
#else
    return pthread_create(
        &storage->threads[thread_index],
        NULL,
        zest__thread_worker,
        &storage->thread_queues
    ) == 0;
#endif
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

void *zest_AllocateMemory(zest_size size) {
    return zest__allocate(size);
}

void zest_FreeMemory(void *allocation) {
    ZEST__FREE(allocation);
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
    if (zest__validation_layers_are_enabled() && ZestDevice->api_version == VK_API_VERSION_1_2) {
        alloc_info.pNext = &flags;
    }
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Allocating buffer memory pool, size: %llu type: %i, alignment: %llu, type bits: %i", alloc_info.allocationSize, alloc_info.memoryTypeIndex, memory_requirements.alignment, memory_requirements.memoryTypeBits);
    ZEST_VK_CHECK_RESULT(vkAllocateMemory(ZestDevice->logical_device, &alloc_info, &ZestDevice->allocation_callbacks, &buffer->memory));

    if (zest__validation_layers_are_enabled() && ZestDevice->api_version == VK_API_VERSION_1_2) {
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

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Allocating image memory pool, size: %llu type: %i, alignment: %llu, type bits: %i", alloc_info.allocationSize, alloc_info.memoryTypeIndex, memory_requirements.alignment, memory_requirements.memoryTypeBits);
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
    buffer_pool->magic = zest_INIT_MAGIC;
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
    buffer->array_index = ZEST_INVALID;
    buffer->owner_queue_family = VK_QUEUE_FAMILY_IGNORED;
    buffer->last_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    buffer->last_access_mask = VK_ACCESS_NONE;
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
        buffer_allocator->magic = zest_INIT_MAGIC;
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
    zest_buffer buffer = zloc_AllocateRemote(buffer_allocator->allocator, adjusted_size);
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

zest_frame_staging_buffer zest_CreateFrameStagingBuffer(VkDeviceSize size) {
    zest_frame_staging_buffer_t blank = { 0 };

    zest_frame_staging_buffer staging_buffer = ZEST__NEW(zest_frame_staging_buffer);
    *staging_buffer = blank;
    staging_buffer->magic = zest_INIT_MAGIC;
    zest_buffer_info_t buffer_info = zest_CreateStagingBufferInfo();
    zest_ForEachFrameInFlight(fif) {
        buffer_info.frame_in_flight = fif;
        staging_buffer->buffer[fif] = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    }
    return staging_buffer;
}

zest_buffer zest_GetStagingBuffer(zest_frame_staging_buffer frame_staging_buffer) {
    ZEST_CHECK_HANDLE(frame_staging_buffer);    //Not a valid buffer handle
    return frame_staging_buffer->buffer[ZEST_FIF];
}

void zest_ClearBufferToZero(zest_buffer buffer) {
    ZEST_ASSERT(buffer->data);  //Must be a host visible buffer and the data is mapped
    memset(buffer->data, 0, buffer->size);
}

void zest_ClearFrameStagingBufferToZero(zest_frame_staging_buffer staging_buffer) {
    ZEST_CHECK_HANDLE(staging_buffer);  //Not a valid staging_buffer handle
    zest_ForEachFrameInFlight(fif) {
        memset(staging_buffer->buffer[fif]->data, 0, staging_buffer->buffer[fif]->size);
    }
}

zest_buffer zest_CreateIndexBuffer(VkDeviceSize size, zest_uint fif) {
    zest_buffer_info_t buffer_info = zest_CreateIndexBufferInfo(0);
    buffer_info.frame_in_flight = fif;
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    return buffer;
}

zest_buffer zest_CreateVertexBuffer(VkDeviceSize size, zest_uint fif) {
    zest_buffer_info_t buffer_info = zest_CreateVertexBufferInfo(0);
    buffer_info.frame_in_flight = fif;
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    return buffer;
}

zest_buffer zest_CreateVertexStorageBuffer(VkDeviceSize size, zest_uint fif) {
    zest_buffer_info_t buffer_info = zest_CreateComputeVertexBufferInfo();
    buffer_info.frame_in_flight = fif;
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    return buffer;
}

zest_buffer zest_CreateStorageBuffer(VkDeviceSize size, zest_uint fif) {
    zest_buffer_info_t buffer_info = zest_CreateStorageBufferInfo();
    buffer_info.frame_in_flight = fif;
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    return buffer;
}

zest_buffer zest_CreateCPUStorageBuffer(VkDeviceSize size, zest_uint fif) {
    zest_buffer_info_t buffer_info = zest_CreateCPUVisibleStorageBufferInfo();
    buffer_info.frame_in_flight = fif;
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    return buffer;
}

zest_buffer zest_CreateComputeVertexBuffer(VkDeviceSize size, zest_uint fif) {
    zest_buffer_info_t buffer_info = zest_CreateComputeVertexBufferInfo();
    buffer_info.frame_in_flight = fif;
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    return buffer;
}

zest_buffer zest_CreateComputeIndexBuffer(VkDeviceSize size, zest_uint fif) {
    zest_buffer_info_t buffer_info = zest_CreateComputeVertexBufferInfo();
    buffer_info.frame_in_flight = fif;
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    return buffer;
}

void zest_CopyBufferOneTime(zest_buffer src_buffer, zest_buffer dst_buffer, VkDeviceSize size) {
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

void zest_CopyBuffer(VkCommandBuffer command_buffer, zest_buffer src_buffer, zest_buffer dst_buffer, VkDeviceSize size) {
    ZEST_ASSERT(size <= src_buffer->size);        //size must be less than or equal to the staging buffer size and the device buffer size
    ZEST_ASSERT(size <= dst_buffer->size);
    VkBufferCopy copyInfo = { 0 };
    copyInfo.srcOffset = src_buffer->memory_offset;
    copyInfo.dstOffset = dst_buffer->memory_offset;
    copyInfo.size = size;
    vkCmdCopyBuffer(command_buffer, src_buffer->memory_pool->buffer, dst_buffer->memory_pool->buffer, 1, &copyInfo);
}

void zest_CopyFrameStagingBuffer(VkCommandBuffer command_buffer, zest_frame_staging_buffer src_buffer, zest_buffer dst_buffer) {
    ZEST_CHECK_HANDLE(src_buffer);  //Not a valid frame staging buffer
    ZEST_ASSERT(dst_buffer);  //Not a valid destination buffer
    ZEST_ASSERT(src_buffer->buffer[ZEST_FIF]->memory_in_use <= dst_buffer->size);   //Not enough space in the destination buffer
    VkBufferCopy copyInfo = { 0 };
    copyInfo.srcOffset = src_buffer->buffer[ZEST_FIF]->memory_offset;
    copyInfo.dstOffset = dst_buffer->memory_offset;
    copyInfo.size = src_buffer->buffer[ZEST_FIF]->memory_in_use;
    vkCmdCopyBuffer(command_buffer, src_buffer->buffer[ZEST_FIF]->memory_pool->buffer, dst_buffer->memory_pool->buffer, 1, &copyInfo);
}

void zest_StageFrameData(void *src_data, zest_frame_staging_buffer dst_staging_buffer, zest_size size) {
    ZEST_CHECK_HANDLE(dst_staging_buffer);  //Not a valid frame staging buffer
    ZEST_ASSERT(src_data);                  //No source data to copy!
    ZEST_ASSERT(size <= dst_staging_buffer->buffer[ZEST_FIF]->size);
	memcpy(dst_staging_buffer->buffer[ZEST_FIF]->data, src_data, size);
    dst_staging_buffer->buffer[ZEST_FIF]->memory_in_use = size;
}

zest_size zest_GetFrameStageBufferMemoryInUse(zest_frame_staging_buffer staging_buffer) {
    ZEST_CHECK_HANDLE(staging_buffer);  //Not a valid frame staging buffer
    return staging_buffer->buffer[ZEST_FIF]->memory_in_use;
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
	//Preserve the bindless array index
	zest_uint array_index = (*buffer)->array_index;
    if (new_buffer) {
        new_buffer->buffer_allocator = (*buffer)->buffer_allocator;
        *buffer = new_buffer;
        zest__set_buffer_details(buffer_allocator, *buffer, buffer_allocator->buffer_info.property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        new_buffer->array_index = array_index;
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
        new_buffer->array_index = array_index;
        new_buffer->memory_in_use = memory_in_use;
    }
    return new_buffer ? ZEST_TRUE : ZEST_FALSE;
}

zest_bool zest_ResizeBuffer(zest_buffer *buffer, zest_size new_size) {
    ZEST_ASSERT(new_size);
    if ((*buffer)->size > new_size) {
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

zest_buffer_info_t zest_CreateIndexBufferInfoWithStorage(zest_bool cpu_visible) {
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
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

zest_buffer_info_t zest_CreateVertexBufferInfoWithStorage(zest_bool cpu_visible) {
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
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
    buffer_info.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return buffer_info;
}

zest_buffer_info_t zest_CreateStorageBufferInfo() {
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_info.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return buffer_info;
}

zest_buffer_info_t zest_CreateStorageBufferInfoWithSrcFlag() {
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
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
    buffer_info.frame_in_flight = ZEST_FIF;
    return buffer_info;
}

zest_buffer_info_t zest_CreateDrawCommandsBufferInfo(zest_bool host_visible) {
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (host_visible) {
        buffer_info.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    } else {
		buffer_info.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
    return buffer_info;
}

void zest_FreeBuffer(zest_buffer buffer) {
    if (!buffer) return;    //Nothing to free
    zloc_FreeRemote(buffer->buffer_allocator->allocator, buffer);
}

VkDeviceMemory zest_GetBufferDeviceMemory(zest_buffer buffer) {
    return buffer->memory_pool->memory;
}

VkBuffer* zest_GetBufferDeviceBuffer(zest_buffer buffer) {
    return &buffer->memory_pool->buffer;
}

VkDescriptorBufferInfo zest_vk_GetBufferInfo(zest_buffer buffer) {
    VkDescriptorBufferInfo buffer_info = { 0 };
    buffer_info.buffer = buffer->memory_pool->buffer;
    buffer_info.offset = buffer->memory_offset;
    buffer_info.range = buffer->size;
    return buffer_info;
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
    ZEST_ASSERT(source_buffer->memory_in_use <= target_buffer->size + target_offset);
    buffer_info.size = source_buffer->memory_in_use;
    zest_vec_linear_push(ZestRenderer->render_graph_allocator, uploader->buffer_copies, buffer_info);
    target_buffer->memory_in_use = source_buffer->memory_in_use;
}

zest_bool zest_UploadBuffer(zest_buffer_uploader_t* uploader, VkCommandBuffer command_buffer) {
    if (!zest_vec_size(uploader->buffer_copies)) {
        return ZEST_FALSE;
    }

    vkCmdCopyBuffer(command_buffer, *zest_GetBufferDeviceBuffer(uploader->source_buffer), *zest_GetBufferDeviceBuffer(uploader->target_buffer), zest_vec_size(uploader->buffer_copies), uploader->buffer_copies);

    zest_vec_clear(uploader->buffer_copies);
    uploader->flags = 0;
    uploader->source_buffer = 0;
    uploader->target_buffer = 0;

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
    zest_key usage_hash = zest_Hash(&usage, sizeof(zest_buffer_usage_t), ZEST_HASH_SEED);
    if (zest_map_valid_key(ZestDevice->pool_sizes, usage_hash)) {
        return *zest_map_at_key(ZestDevice->pool_sizes, usage_hash);
    }
    zest_buffer_pool_size_t pool_size = { 0 };
    return pool_size;
}

zest_buffer_pool_size_t zest_GetDeviceImagePoolSize(const char* name) {
    zest_key usage_hash = zest_Hash(name, strlen(name), ZEST_HASH_SEED);
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
    zest_key usage_hash = zest_Hash(&usage, sizeof(zest_buffer_usage_t), ZEST_HASH_SEED);
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
    zest_key usage_hash = zest_Hash(&usage, sizeof(zest_buffer_usage_t), ZEST_HASH_SEED);
    zest_buffer_pool_size_t pool_sizes;
    pool_sizes.name = name;
    pool_sizes.minimum_allocation_size = minimum_allocation;
    pool_sizes.pool_size = pool_size;
    zest_map_insert_key(ZestDevice->pool_sizes, usage_hash, pool_sizes);
}
// --End Vulkan Buffer Management

// --Renderer and related functions
void zest__initialise_renderer(zest_create_info_t* create_info) {
    ZestRenderer->flags |= (create_info->flags & zest_init_flag_enable_vsync) ? zest_renderer_flag_vsync_enabled : 0;
    zest_SetText(&ZestRenderer->shader_path_prefix, create_info->shader_path_prefix);
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create swap chain");
    zest__create_swapchain();
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create swap chain image views");
    zest__create_swapchain_image_views();

    if (ZEST__FLAGGED(create_info->flags, zest_init_flag_use_depth_buffer)) {
        ZestRenderer->final_render_pass = zest__get_render_pass(ZestRenderer->swapchain_image_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ATTACHMENT_LOAD_OP_CLEAR, ZEST_TRUE);
        ZestRenderer->flags |= zest_renderer_flag_has_depth_buffer;
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create depth passes");
        ZestRenderer->depth_resource_buffer = zest__create_depth_resources();
    }
    else {
        ZestRenderer->final_render_pass = zest__get_render_pass(ZestRenderer->swapchain_image_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ATTACHMENT_LOAD_OP_CLEAR, ZEST_FALSE);
    }
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create swap chain frame buffers");
    zest__create_swap_chain_frame_buffers(ZEST__FLAGGED(ZestRenderer->flags, zest_renderer_flag_has_depth_buffer));
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create sync objects");
    zest__create_sync_objects();
    zest__create_command_buffer_pools();
    ZestRenderer->push_constants.screen_resolution.x = 1.f / ZestRenderer->swapchain_extent.width;
    ZestRenderer->push_constants.screen_resolution.y = 1.f / ZestRenderer->swapchain_extent.height;

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create descriptor layouts");

	ZestRenderer->uniform_buffer = zest_CreateUniformBuffer("Zest Uniform Buffer", sizeof(zest_uniform_buffer_data_t));

    //Create a global bindless descriptor set for storage buffers and texture samplers
    zest_set_layout_builder_t layout_builder = zest_BeginSetLayoutBuilder();
    zest_AddLayoutBuilderCombinedImageSamplerBindless(&layout_builder, zest_combined_image_sampler_binding, create_info->bindless_combined_sampler_count);
	zest_AddLayoutBuilderStorageBufferBindless(&layout_builder, zest_storage_buffer_binding, create_info->bindless_storage_buffer_count, zest_shader_all_stages);
    zest_AddLayoutBuilderSamplerBindless(&layout_builder, zest_sampler_binding, create_info->bindless_sampler_count);
    zest_AddLayoutBuilderSampledImageBindless(&layout_builder, zest_sampled_image_binding, create_info->bindless_sampled_image_count);
    ZestRenderer->global_bindless_set_layout = zest_FinishDescriptorSetLayoutForBindless(&layout_builder, 1, 0, "Zest Descriptor Layout");
    ZestRenderer->global_set = zest_CreateBindlessSet(ZestRenderer->global_bindless_set_layout);

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create pipeline cache");
    zest__create_pipeline_cache();
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Compile shaders");
    zest__compile_builtin_shaders(ZEST__FLAGGED(create_info->flags, zest_init_flag_disable_shaderc));
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create standard pipelines");
    zest__prepare_standard_pipelines();

    VkFenceCreateInfo fence_info = { 0 };
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (ZEST_EACH_FIF_i) {
        ZEST_VK_CHECK_RESULT(vkCreateFence(ZestDevice->logical_device, &fence_info, &ZestDevice->allocation_callbacks, &ZestRenderer->fif_fence[i]));
    }

    zest__create_debug_layout_and_pool(create_info->maximum_textures);

    VkCommandBufferAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = ZEST_MAX_FIF;
    alloc_info.commandPool = ZestDevice->command_pool;
    ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, ZestRenderer->utility_command_buffer));

    void *linear_memory = ZEST__ALLOCATE(zloc__MEGABYTE(1));

    ZestRenderer->render_graph_allocator = zloc_InitialiseLinearAllocator(linear_memory, zloc__MEGABYTE(1));

    ZEST_ASSERT(ZestRenderer->render_graph_allocator);    //Unabable to allocate the render graph allocator, 

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Finished zest initialisation");

    ZEST__FLAG(ZestRenderer->flags, zest_renderer_flag_initialised);
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

    ZestRenderer->swapchain_image_count = image_count;

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

VkSurfaceFormatKHR zest__choose_swapchain_format(VkSurfaceFormatKHR *available_formats) {
    size_t num_available_formats = zest_vec_size(available_formats);

    // --- 1. Handle the rare case where the surface provides no preferred formats ---
    if (num_available_formats == 1 && available_formats[0].format == VK_FORMAT_UNDEFINED) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: Surface format is UNDEFINED. Choosing VK_FORMAT_B8G8R8A8_SRGB by default.");
        VkSurfaceFormatKHR chosen_format = {
            .format = VK_FORMAT_B8G8R8A8_SRGB, // Prefer SRGB for automatic gamma correction
            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        };
        // Note: A truly robust implementation might double-check if this default
        // is *actually* supported by querying all possible formats, but this case is rare.
        return chosen_format;
    }

    // --- 2. Determine the user's desired format ---
    VkFormat desired_format = ZestApp->create_info.color_format;

    if (desired_format == VK_FORMAT_UNDEFINED) {
        desired_format = VK_FORMAT_B8G8R8A8_SRGB; // Default to SRGB if user doesn't care
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: User preference is UNDEFINED. Defaulting preference to VK_FORMAT_B8G8R8A8_SRGB.");
    } else {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: User preference is Format %d.", desired_format);
    }

    // --- 3. Search for the User's Preferred Format with SRGB Color Space ---
    for (size_t i = 0; i < num_available_formats; ++i) {
        if (available_formats[i].format == desired_format &&
            available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: Found exact user preference: Format %d, Colorspace %d", available_formats[i].format, available_formats[i].colorSpace);
            return available_formats[i];
        }
    }
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: User preferred format (%d) with SRGB colorspace not available.", desired_format);

    // --- 4. Fallback: Search for *any* SRGB Format with SRGB Color Space ---
    for (size_t i = 0; i < num_available_formats; ++i) {
        if (available_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: Falling back to available VK_FORMAT_B8G8R8A8_SRGB.");
            return available_formats[i];
        }
    }
    for (size_t i = 0; i < num_available_formats; ++i) {
        if (available_formats[i].format == VK_FORMAT_R8G8B8A8_SRGB &&
            available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: Falling back to available VK_FORMAT_R8G8B8A8_SRGB.");
            return available_formats[i];
        }
    }
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: No SRGB formats with SRGB colorspace available.");

    // --- 5. Fallback: Search for *any* UNORM Format with SRGB Color Space ---
    // If no SRGB format works, take any UNORM format as long as the colorspace is right.
    // This means we'll have to handle gamma correction manually in the shader.
    for (size_t i = 0; i < num_available_formats; ++i) {
        if (available_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
            available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: Falling back to VK_FORMAT_B8G8R8A8_UNORM with SRGB colorspace (Manual gamma needed).");
            return available_formats[i];
        }
    }
    for (size_t i = 0; i < num_available_formats; ++i) {
        if (available_formats[i].format == VK_FORMAT_R8G8B8A8_UNORM &&
            available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: Falling back to VK_FORMAT_R8G8B8A8_UNORM with SRGB colorspace (Manual gamma needed).");
            return available_formats[i];
        }
    }
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: No UNORM formats with SRGB colorspace available.");

    // --- 6. Last Resort Fallback ---
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: Critical Fallback! Using first available format: Format %d, Colorspace %d. Check results!", available_formats[0].format, available_formats[0].colorSpace);
    return available_formats[0];
}

void zest__create_pipeline_cache() {
    VkPipelineCacheCreateInfo pipeline_cache_create_info = { 0 };
    pipeline_cache_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    ZEST_VK_CHECK_RESULT(vkCreatePipelineCache(ZestDevice->logical_device, &pipeline_cache_create_info, &ZestDevice->allocation_callbacks, &ZestRenderer->pipeline_cache));
}

void zest__cleanup_swapchain() {
    vkDestroyImageView(ZestDevice->logical_device, ZestRenderer->final_render_pass_depth_attachment.base_view, &ZestDevice->allocation_callbacks);
    vkDestroyImage(ZestDevice->logical_device, ZestRenderer->final_render_pass_depth_attachment.image, &ZestDevice->allocation_callbacks);

    for (zest_foreach_i(ZestRenderer->swapchain_frame_buffers)) {
        vkDestroyFramebuffer(ZestDevice->logical_device, ZestRenderer->swapchain_frame_buffers[i], &ZestDevice->allocation_callbacks);
    }

    for (zest_foreach_i(ZestRenderer->swapchain_image_views)) {
        vkDestroyImageView(ZestDevice->logical_device, ZestRenderer->swapchain_image_views[i], &ZestDevice->allocation_callbacks);
    }

    vkDestroySwapchainKHR(ZestDevice->logical_device, ZestRenderer->swapchain, &ZestDevice->allocation_callbacks);
}

void zest__destroy_pipeline_set(zest_pipeline p) {
    vkDestroyPipeline(ZestDevice->logical_device, p->pipeline, &ZestDevice->allocation_callbacks);
    vkDestroyPipelineLayout(ZestDevice->logical_device, p->pipeline_layout, &ZestDevice->allocation_callbacks);
}

void zest__cleanup_pipelines() {
    for (zest_map_foreach_i(ZestRenderer->cached_pipelines)) {
        zest_pipeline pipeline = *zest_map_at_index(ZestRenderer->cached_pipelines, i);
        zest__destroy_pipeline_set(pipeline);
    }
}

void zest__cleanup_textures() {
    for (zest_map_foreach_i(ZestRenderer->textures)) {
        zest_texture texture = *zest_map_at_index(ZestRenderer->textures, i);
        if (texture->flags & zest_texture_flag_ready) {
            zest__cleanup_texture(texture);
        }
    }
}

void zest__cleanup_framebuffer(zest_frame_buffer_t* frame_buffer) {
    if (frame_buffer->color_buffer.base_view != VK_NULL_HANDLE) vkDestroyImageView(ZestDevice->logical_device, frame_buffer->color_buffer.base_view, &ZestDevice->allocation_callbacks);
    if (frame_buffer->depth_buffer.base_view != VK_NULL_HANDLE) vkDestroyImageView(ZestDevice->logical_device, frame_buffer->depth_buffer.base_view, &ZestDevice->allocation_callbacks);
    if (frame_buffer->color_buffer.image != VK_NULL_HANDLE) vkDestroyImage(ZestDevice->logical_device, frame_buffer->color_buffer.image, &ZestDevice->allocation_callbacks);
    if (frame_buffer->depth_buffer.image != VK_NULL_HANDLE) vkDestroyImage(ZestDevice->logical_device, frame_buffer->depth_buffer.image, &ZestDevice->allocation_callbacks);
    if (frame_buffer->sampler != VK_NULL_HANDLE) vkDestroySampler(ZestDevice->logical_device, frame_buffer->sampler, &ZestDevice->allocation_callbacks);
    zest_vec_foreach(i, frame_buffer->framebuffers) {
        if (frame_buffer->framebuffers[i] != VK_NULL_HANDLE) vkDestroyFramebuffer(ZestDevice->logical_device, frame_buffer->framebuffers[i], &ZestDevice->allocation_callbacks);
    }
    zest_vec_clear(frame_buffer->framebuffers);
    zest_vec_foreach(i, frame_buffer->color_buffer.mip_views) {
        if (frame_buffer->color_buffer.mip_views[i] != VK_NULL_HANDLE) vkDestroyImageView(ZestDevice->logical_device, frame_buffer->color_buffer.mip_views[i], &ZestDevice->allocation_callbacks);
    }
    zest_vec_clear(frame_buffer->color_buffer.mip_views);
}

void zest__cleanup_draw_routines(void) {
    zest_map_foreach(i, ZestRenderer->draw_routines) {
        zest_draw_routine draw_routine = ZestRenderer->draw_routines.data[i];
        if (draw_routine->recorder) {
            zest_FreeRecorder(draw_routine->recorder);
        }
    }
}

void zest__cleanup_device(void) {
    zest_ForEachFrameInFlight(fif) {
        vkDestroySemaphore(ZestDevice->logical_device, ZestDevice->graphics_queue.semaphore[fif], &ZestDevice->allocation_callbacks);
        vkDestroySemaphore(ZestDevice->logical_device, ZestDevice->compute_queue.semaphore[fif], &ZestDevice->allocation_callbacks);
        vkDestroySemaphore(ZestDevice->logical_device, ZestDevice->transfer_queue.semaphore[fif], &ZestDevice->allocation_callbacks);
    }
}

void zest__cleanup_renderer() {
    zest__cleanup_swapchain();

    zest__cleanup_pipelines();
    zest_map_clear(ZestRenderer->pipelines);

    zest__cleanup_textures();

    for (zest_map_foreach_i(ZestRenderer->samplers)) {
        zest_sampler sampler = *zest_map_at_index(ZestRenderer->samplers, i);
        if (sampler->vk_sampler) {
            vkDestroySampler(ZestDevice->logical_device, sampler->vk_sampler, &ZestDevice->allocation_callbacks);
        }
        ZEST__FREE(sampler);
    }
    zest_map_clear(ZestRenderer->samplers);

    for (zest_map_foreach_i(ZestRenderer->render_targets)) {
        zest_render_target render_target = *zest_map_at_index(ZestRenderer->render_targets, i);
		zest_CleanUpRenderTarget(render_target);
        zest_FreeRecorder(render_target->recorder);
    }
    zest_map_clear(ZestRenderer->render_targets);

    for (zest_map_foreach_i(ZestRenderer->render_passes)) {
        VkRenderPass render_pass = *zest_map_at_index(ZestRenderer->render_passes, i);
        vkDestroyRenderPass(ZestDevice->logical_device, render_pass, &ZestDevice->allocation_callbacks);
    }
    zest_map_clear(ZestRenderer->render_passes);

    zest__cleanup_draw_routines();
    for (zest_map_foreach_i(ZestRenderer->draw_routines)) {
        zest_draw_routine routine = *zest_map_at_index(ZestRenderer->draw_routines, i);
        if (routine->clean_up_callback) {
            routine->clean_up_callback(routine);
        }
    }

    zest_map_clear(ZestRenderer->draw_routines);

    for (zest_map_foreach_i(ZestRenderer->descriptor_layouts)) {
        zest_set_layout layout = *zest_map_at_index(ZestRenderer->descriptor_layouts, i);
		vkDestroyDescriptorSetLayout(ZestDevice->logical_device, layout->vk_layout, &ZestDevice->allocation_callbacks);
        if (layout->pool) {
            vkDestroyDescriptorPool(ZestDevice->logical_device, layout->pool->vk_descriptor_pool, &ZestDevice->allocation_callbacks);
        }
    }
    zest_map_clear(ZestRenderer->descriptor_layouts);

    for (ZEST_EACH_FIF_i) {
        vkDestroySemaphore(ZestDevice->logical_device, ZestRenderer->image_available_semaphore[i], &ZestDevice->allocation_callbacks);
        vkDestroyFence(ZestDevice->logical_device, ZestRenderer->fif_fence[i], &ZestDevice->allocation_callbacks);
    }

    zest_vec_foreach(i, ZestRenderer->render_finished_semaphore) {
        vkDestroySemaphore(ZestDevice->logical_device, ZestRenderer->render_finished_semaphore[i], &ZestDevice->allocation_callbacks);
    }

    zest_vec_foreach(i, ZestRenderer->semaphore_pool) {
        vkDestroySemaphore(ZestDevice->logical_device, ZestRenderer->semaphore_pool[i], &ZestDevice->allocation_callbacks);
    }

    zest_vec_foreach(i, ZestRenderer->timeline_semaphores) {
        vkDestroySemaphore(ZestDevice->logical_device, ZestRenderer->timeline_semaphores[i]->semaphore, &ZestDevice->allocation_callbacks);
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

    for (zest_map_foreach_i(ZestRenderer->computes)) {
        zest_compute compute = *zest_map_at_index(ZestRenderer->computes, i);
        zest__clean_up_compute(compute);
    }

	zest_ForEachFrameInFlight(fif) {
		zest_vec_foreach(i, ZestRenderer->old_frame_buffers[fif]) {
			VkFramebuffer frame_buffer = ZestRenderer->old_frame_buffers[fif][i];
			vkDestroyFramebuffer(ZestDevice->logical_device, frame_buffer, &ZestDevice->allocation_callbacks);
		}
	}

	vkDestroyCommandPool(ZestDevice->logical_device, ZestRenderer->command_buffers.graphics_command_pool, &ZestDevice->allocation_callbacks);
	vkDestroyCommandPool(ZestDevice->logical_device, ZestRenderer->command_buffers.compute_command_pool, &ZestDevice->allocation_callbacks);
	vkDestroyCommandPool(ZestDevice->logical_device, ZestRenderer->command_buffers.transfer_command_pool, &ZestDevice->allocation_callbacks);
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
        vkDestroySemaphore(ZestDevice->logical_device, ZestRenderer->image_available_semaphore[i], &ZestDevice->allocation_callbacks);
    }

    zest_vec_foreach(i, ZestRenderer->render_finished_semaphore) {
        vkDestroySemaphore(ZestDevice->logical_device, ZestRenderer->render_finished_semaphore[i], &ZestDevice->allocation_callbacks);
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
		zest_CleanUpRenderTarget(render_target);
        for (int fif = 0; fif != render_target->frames_in_flight; ++fif) {
            zest_RecreateRenderTargetResources(render_target, fif);
        }
		zest__refresh_render_target_sampler(render_target);
        if (ZEST__FLAGGED(render_target->flags, zest_render_target_flag_multi_mip)) {
			zest__refresh_render_target_mip_samplers(render_target);
        }
    }

    VkExtent2D extent;
    extent.width = fb_width;
    extent.height = fb_height;
    for (zest_map_foreach_i(ZestRenderer->cached_pipelines)) {
        zest_pipeline_template pipeline_template = *zest_map_at_index(ZestRenderer->pipelines, i);
        zest__refresh_pipeline_template(pipeline_template);
    }
    for (zest_map_foreach_i(ZestRenderer->cached_pipelines)) {
        zest_pipeline pipeline = *zest_map_at_index(ZestRenderer->cached_pipelines, i);
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
        else if (draw_routine->update_resolution_callback) {
            draw_routine->update_resolution_callback(draw_routine);
        }
        for (ZEST_EACH_FIF_i) {
            draw_routine->recorder->outdated[i] = 1;
        }
    }

    for (zest_map_foreach_j(ZestRenderer->command_queues)) {
        zest_command_queue command_queue = *zest_map_at_index(ZestRenderer->command_queues, j);
        for (ZEST_EACH_FIF_i) {
            zest_vec_clear(command_queue->fif_wait_stage_flags[i]);
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
            if (draw_commands->composite_pipeline) {
                zest_uint layer_count = zest_vec_size(draw_commands->render_targets);
				zest_ResetDescriptorPool(draw_commands->descriptor_pool);
				zest_ForEachFrameInFlight(fif) {
					zest_descriptor_set_builder_t set_builder = zest_BeginDescriptorSetBuilder(draw_commands->composite_descriptor_layout);
					zest_vec_foreach(c, draw_commands->render_targets) {
						zest_render_target layer = draw_commands->render_targets[c];
                        zest_AddSetBuilderDirectImageWrite(&set_builder, c, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &layer->image_info[fif]);
                    }
					zest_FinishDescriptorSet(draw_commands->descriptor_pool, &set_builder, &draw_commands->composite_descriptor_set[fif]);
                }
                zest_ForEachFrameInFlight(fif) {
                    if (!ZEST_VALID_HANDLE(draw_commands->composite_shader_resources)) {
                        draw_commands->composite_shader_resources = zest_CreateShaderResources();
                        zest_AddDescriptorSetToResources(draw_commands->composite_shader_resources, &draw_commands->composite_descriptor_set[fif], fif);
                    } else {
                        zest_UpdateShaderResources(draw_commands->composite_shader_resources, &draw_commands->composite_descriptor_set[fif], 0, fif);
                    }
                }
            }

        }
    }

    for (ZEST_EACH_FIF_i) {
        zest_vec_clear(ZestRenderer->empty_queue.fif_wait_stage_flags[i]);
    }

    for (zest_foreach_i(ZestRenderer->empty_queue.draw_commands)) {
        zest_command_queue_draw_commands draw_commands = ZestRenderer->empty_queue.draw_commands[i];
        draw_commands->viewport.extent.width = fb_width;
        draw_commands->viewport.extent.height = fb_height;
    }

}

void zest__create_swapchain_image_views() {
    zest_vec_resize(ZestRenderer->swapchain_image_views, zest_vec_size(ZestRenderer->swapchain_images));

    for (zest_foreach_i(ZestRenderer->swapchain_images)) {
        zest_vec_set(ZestRenderer->swapchain_image_views, i, zest__create_image_view(ZestRenderer->swapchain_images[i], ZestRenderer->swapchain_image_format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 1));
    }
}

VkRenderPass zest__get_render_pass_with_info(zest_render_pass_info_t render_pass_info) {
    zest_key hash = zest_Hash(&render_pass_info, sizeof(zest_render_pass_info_t), ZEST_HASH_SEED);
    if (zest_map_valid_key(ZestRenderer->render_passes, hash)) {
		return *zest_map_at_key(ZestRenderer->render_passes, hash); 
    }
    VkRenderPass render_pass = zest__create_render_pass(render_pass_info.render_format, render_pass_info.initial_layout, render_pass_info.final_layout, render_pass_info.load_op, render_pass_info.depth_buffer);
    zest_map_insert_key(ZestRenderer->render_passes, hash, render_pass);
    return render_pass;
}

VkRenderPass zest__get_render_pass(VkFormat render_format, VkImageLayout initial_layout, VkImageLayout final_layout, VkAttachmentLoadOp load_op, zest_bool depth_buffer) {
    zest_render_pass_info_t render_pass_info;
    render_pass_info.render_format = render_format;
    render_pass_info.initial_layout = initial_layout;
    render_pass_info.final_layout = final_layout;
    render_pass_info.load_op = load_op;
    render_pass_info.depth_buffer = depth_buffer;
    zest_key hash = zest_Hash(&render_pass_info, sizeof(zest_render_pass_info_t), ZEST_HASH_SEED);
    if (zest_map_valid_key(ZestRenderer->render_passes, hash)) {
		return *zest_map_at_key(ZestRenderer->render_passes, hash); 
    }
    VkRenderPass render_pass = zest__create_render_pass(render_format, initial_layout, final_layout, load_op, depth_buffer);
    zest_map_insert_key(ZestRenderer->render_passes, hash, render_pass);
    return render_pass;
}

zest_buffer_t* zest__create_depth_resources() {
    VkFormat depth_format = zest__find_depth_format();

    zest_buffer_t* buffer = zest__create_image(ZestRenderer->swapchain_extent.width, ZestRenderer->swapchain_extent.height, 1, VK_SAMPLE_COUNT_1_BIT, depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &ZestRenderer->final_render_pass_depth_attachment.image);
    ZestRenderer->final_render_pass_depth_attachment.base_view = zest__create_image_view(ZestRenderer->final_render_pass_depth_attachment.image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 0, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 1);

    zest__transition_image_layout(ZestRenderer->final_render_pass_depth_attachment.image, depth_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, 1, 0);

    return buffer;
}

void zest__create_swap_chain_frame_buffers(zest_bool depth_buffer) {
    zest_vec_resize(ZestRenderer->swapchain_frame_buffers, zest_vec_size(ZestRenderer->swapchain_image_views));

    for (zest_foreach_i(ZestRenderer->swapchain_image_views)) {
        VkFramebufferCreateInfo frame_buffer_info = { 0 };

        VkImageView* attachments = 0;
        zest_vec_push(attachments, ZestRenderer->swapchain_image_views[i]);
        if (depth_buffer) {
            zest_vec_push(attachments, ZestRenderer->final_render_pass_depth_attachment.base_view);
        }
        frame_buffer_info.attachmentCount = zest_vec_size(attachments);
        frame_buffer_info.pAttachments = attachments;
        frame_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frame_buffer_info.renderPass = ZestRenderer->final_render_pass;
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
        ZEST_VK_CHECK_RESULT(vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, &ZestDevice->allocation_callbacks, &ZestRenderer->image_available_semaphore[i]));
    }

    zest_vec_resize(ZestRenderer->render_finished_semaphore, ZestRenderer->swapchain_image_count);

    zest_vec_foreach(i, ZestRenderer->render_finished_semaphore) {
        ZEST_VK_CHECK_RESULT(vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, &ZestDevice->allocation_callbacks, &ZestRenderer->render_finished_semaphore[i]));
    }

    //Create the semaphore pool for render graphs
    for (int i = 0; i != 10; ++i) {
        VkSemaphore semaphore;
        vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, &ZestDevice->allocation_callbacks, &semaphore);
        zest_vec_push(ZestRenderer->semaphore_pool, semaphore);
        zest_vec_push(ZestRenderer->free_semaphores, semaphore);
    }

}

void zest__create_command_buffer_pools() {
    ZestRenderer->command_buffers.graphics_command_pool = zest__create_queue_command_pool(ZestDevice->graphics_queue_family_index);
    ZestRenderer->command_buffers.compute_command_pool = zest__create_queue_command_pool(ZestDevice->compute_queue_family_index);
    ZestRenderer->command_buffers.transfer_command_pool = zest__create_queue_command_pool(ZestDevice->transfer_queue_family_index);
    zest_vec_resize(ZestRenderer->command_buffers.graphics, 10);
    zest_vec_resize(ZestRenderer->command_buffers.compute, 10);
    zest_vec_resize(ZestRenderer->command_buffers.transfer, 10);
    VkCommandBufferAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 10;
    alloc_info.commandPool = ZestRenderer->command_buffers.graphics_command_pool;
    ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, ZestRenderer->command_buffers.graphics));
    alloc_info.commandPool = ZestRenderer->command_buffers.compute_command_pool;
    ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, ZestRenderer->command_buffers.compute));
    alloc_info.commandPool = ZestRenderer->command_buffers.transfer_command_pool;
    ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, ZestRenderer->command_buffers.transfer));
    zest_vec_foreach(i, ZestRenderer->command_buffers.graphics) {
        zest_vec_push(ZestRenderer->command_buffers.free_graphics, ZestRenderer->command_buffers.graphics[i]);
        zest_vec_push(ZestRenderer->command_buffers.free_compute, ZestRenderer->command_buffers.compute[i]);
        zest_vec_push(ZestRenderer->command_buffers.free_transfer, ZestRenderer->command_buffers.transfer[i]);
    }
}

VkCommandBuffer zest__acquire_graphics_command_buffer(VkCommandBufferLevel level) {
    zest_command_buffer_pools_t *command_buffers = &ZestRenderer->command_buffers;
    if (zest_vec_size(command_buffers->free_graphics) > 0) {
        VkCommandBuffer command_buffer = zest_vec_back(command_buffers->free_graphics);
        zest_vec_pop(command_buffers->free_graphics);
        return command_buffer;
    } else {
        // Pool is empty, create a new one
		VkCommandBufferAllocateInfo alloc_info = { 0 };
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.level = level;
		alloc_info.commandBufferCount = 1;
		alloc_info.commandPool = command_buffers->graphics_command_pool;
        VkCommandBuffer new_command_buffer;
		ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, &new_command_buffer));
        zest_vec_push(command_buffers->graphics, new_command_buffer); 
        return new_command_buffer;
    }
}

VkCommandBuffer zest__acquire_compute_command_buffer(VkCommandBufferLevel level) {
    zest_command_buffer_pools_t *command_buffers = &ZestRenderer->command_buffers;
    if (zest_vec_size(command_buffers->free_compute) > 0) {
        VkCommandBuffer command_buffer = zest_vec_back(command_buffers->free_compute);
        zest_vec_pop(command_buffers->free_compute);
        return command_buffer;
    } else {
        // Pool is empty, create a new one
		VkCommandBufferAllocateInfo alloc_info = { 0 };
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.level = level;
		alloc_info.commandBufferCount = 1;
        VkCommandBuffer new_command_buffer;
		alloc_info.commandPool = command_buffers->compute_command_pool;
		ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, &new_command_buffer));
        zest_vec_push(command_buffers->compute, new_command_buffer); 
        return new_command_buffer;
    }
}

VkCommandBuffer zest__acquire_transfer_command_buffer(VkCommandBufferLevel level) {
    zest_command_buffer_pools_t *command_buffers = &ZestRenderer->command_buffers;
    if (zest_vec_size(command_buffers->free_transfer) > 0) {
        VkCommandBuffer command_buffer = zest_vec_back(command_buffers->free_transfer);
        zest_vec_pop(command_buffers->free_transfer);
        return command_buffer;
    } else {
        // Pool is empty, create a new one
		VkCommandBufferAllocateInfo alloc_info = { 0 };
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.level = level;
		alloc_info.commandBufferCount = 1;
        VkCommandBuffer new_command_buffer;
		alloc_info.commandPool = command_buffers->transfer_command_pool;
		ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, &new_command_buffer));
        zest_vec_push(command_buffers->transfer, new_command_buffer); 
        return new_command_buffer;
    }
}

VkSemaphore zest__acquire_semaphore() {
    if (zest_vec_size(ZestRenderer->free_semaphores) > 0) {
        VkSemaphore semaphore = zest_vec_back(ZestRenderer->free_semaphores);
        zest_vec_pop(ZestRenderer->free_semaphores);
        return semaphore;
    } else {
        // Pool is empty, create a new one
        VkSemaphoreCreateInfo semaphore_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkSemaphore new_semaphore;
        ZEST_VK_CHECK_RESULT(vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, &ZestDevice->allocation_callbacks, &new_semaphore));
        zest_vec_push(ZestRenderer->semaphore_pool, new_semaphore); 
        return new_semaphore;
    }
}

zest_uniform_buffer zest_CreateUniformBuffer(const char *name, zest_size uniform_struct_size) {
    zest_uniform_buffer_t blank = { 0 };
    zest_uniform_buffer uniform_buffer = ZEST__NEW(zest_uniform_buffer);
    *uniform_buffer = blank;
    uniform_buffer->magic = zest_INIT_MAGIC;
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_info.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	zest_set_layout_builder_t uniform_layout_builder = zest_BeginSetLayoutBuilder();
	zest_AddLayoutBuilderUniformBuffer(&uniform_layout_builder, 0, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	uniform_buffer->set_layout = zest_FinishDescriptorSetLayout(&uniform_layout_builder, "Layout for: %s", name);
	zest_CreateDescriptorPoolForLayout(uniform_buffer->set_layout, ZEST_MAX_FIF, 0);

    zest_ForEachFrameInFlight(fif) {
        uniform_buffer->buffer[fif] = zest_CreateBuffer(uniform_struct_size, &buffer_info, ZEST_NULL);
        uniform_buffer->descriptor_info[fif].buffer = *zest_GetBufferDeviceBuffer(uniform_buffer->buffer[fif]);
        uniform_buffer->descriptor_info[fif].offset = uniform_buffer->buffer[fif]->memory_offset;
        uniform_buffer->descriptor_info[fif].range = uniform_struct_size;

		zest_descriptor_set_builder_t uniform_set_builder = zest_BeginDescriptorSetBuilder(uniform_buffer->set_layout);
		zest_AddSetBuilderUniformBuffer(&uniform_set_builder, 0, 0, uniform_buffer, fif);
		uniform_buffer->descriptor_set[fif] = zest_FinishDescriptorSet(uniform_buffer->set_layout->pool, &uniform_set_builder, uniform_buffer->descriptor_set[fif]);

    }
    return zest__add_uniform_buffer(uniform_buffer);
}

void zest_Update2dUniformBuffer() {
    zest_uniform_buffer_data_t* ubo_ptr = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(ZestRenderer->uniform_buffer);
    zest_vec3 eye = { .x = 0.f, .y = 0.f, .z = -1 };
    zest_vec3 center = { 0 };
    zest_vec3 up = { .x = 0.f, .y = -1.f, .z = 0.f };
    ubo_ptr->view = zest_LookAt(eye, center, up);
    ubo_ptr->proj = zest_Ortho(0.f, (float)ZestRenderer->swapchain_extent.width / ZestRenderer->dpi_scale, 0.f, -(float)ZestRenderer->swapchain_extent.height / ZestRenderer->dpi_scale, -1000.f, 1000.f);
    ubo_ptr->screen_size.x = zest_ScreenWidthf();
    ubo_ptr->screen_size.y = zest_ScreenHeightf();
    ubo_ptr->millisecs = zest_Millisecs() % 1000;
}

zest_uniform_buffer zest__add_uniform_buffer(zest_uniform_buffer buffer) {
	zest_vec_push(ZestRenderer->uniform_buffers, buffer);
    return buffer;
}

zest_set_layout_builder_t zest_BeginSetLayoutBuilder() {
    zest_set_layout_builder_t builder = { 0 };
    return builder;
}

void zest_SetLayoutBuilderCreateFlags(zest_set_layout_builder_t *builder, VkDescriptorSetLayoutCreateFlags flags) {
    builder->create_flags = flags;
}

bool zest__binding_exists_in_layout_builder(zest_set_layout_builder_t *builder, zest_uint binding) {
    return (builder->binding_indexes & (1ull << binding)) != 0;
}

void zest_AddLayoutBuilderBinding(zest_set_layout_builder_t *builder, zest_uint binding_number, VkDescriptorType descriptor_type, zest_uint descriptor_count, zest_supported_shader_stages stage_flags, const VkSampler *p_immutable_samplers) {
    bool binding_exists = zest__binding_exists_in_layout_builder(builder, binding_number);
    ZEST_ASSERT(binding_number < 64);   //Invalid binding number, values of 0 - 63 only
    ZEST_ASSERT(!binding_exists);       //That binding number already exists in the layout builder
    VkDescriptorSetLayoutBinding binding = { 0 };
    binding.binding = binding_number;
    binding.descriptorCount = descriptor_count;
    binding.descriptorType = descriptor_type;
    binding.stageFlags = stage_flags;
    binding.pImmutableSamplers = p_immutable_samplers;
    builder->binding_indexes |= (1ull << binding_number);
    zest_vec_push(builder->bindings, binding);
}

void zest_AddLayoutBuilderSampler(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint descriptor_count) {
    zest_AddLayoutBuilderBinding(builder, binding_number, VK_DESCRIPTOR_TYPE_SAMPLER, descriptor_count, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
}

void zest_AddLayoutBuilderSampledImage(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint descriptor_count) {
    zest_AddLayoutBuilderBinding(builder, binding_number, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, descriptor_count, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
}

void zest_AddLayoutBuilderCombinedImageSampler(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint descriptor_count) {
    zest_AddLayoutBuilderBinding(builder, binding_number, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptor_count, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
}

void zest_AddLayoutBuilderUniformBuffer(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint descriptor_count, zest_supported_shader_stages shader_stages) {
    zest_AddLayoutBuilderBinding(builder, binding_number, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptor_count, shader_stages, 0);
}

void zest_AddLayoutBuilderStorageBuffer(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint descriptor_count, zest_supported_shader_stages shader_stages) {
    zest_AddLayoutBuilderBinding(builder, binding_number, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptor_count, shader_stages, 0);
}

void zest_AddLayoutBuilderBindingBindless(zest_set_layout_builder_t *builder, zest_uint binding_number, VkDescriptorType descriptor_type, zest_uint descriptor_count, zest_supported_shader_stages stage_flags, VkDescriptorBindingFlags binding_flags, const VkSampler *p_immutable_samplers) {
    bool binding_exists = zest__binding_exists_in_layout_builder(builder, binding_number);
    ZEST_ASSERT(binding_number < 64);   //Invalid binding number, values of 0 - 63 only
    ZEST_ASSERT(!binding_exists);       //That binding number already exists in the layout builder
    VkDescriptorSetLayoutBinding binding = { 0 };
    binding.binding = binding_number;
    binding.descriptorCount = descriptor_count;
    binding.descriptorType = descriptor_type;
    binding.stageFlags = stage_flags;
    binding.pImmutableSamplers = p_immutable_samplers;
	builder->binding_indexes |= (1ull << binding_number);
    zest_vec_push(builder->bindings, binding);
    zest_vec_push(builder->binding_flags, binding_flags);
}

void zest_AddLayoutBuilderSamplerBindless(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint max_texture_count) {
    VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    zest_AddLayoutBuilderBindingBindless(builder, binding_number, VK_DESCRIPTOR_TYPE_SAMPLER, max_texture_count, VK_SHADER_STAGE_FRAGMENT_BIT, binding_flags, 0);
}

void zest_AddLayoutBuilderSampledImageBindless(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint max_texture_count) {
    VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    zest_AddLayoutBuilderBindingBindless(builder, binding_number, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, max_texture_count, VK_SHADER_STAGE_FRAGMENT_BIT, binding_flags, 0);
}

void zest_AddLayoutBuilderCombinedImageSamplerBindless(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint max_texture_count) {
    VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    zest_AddLayoutBuilderBindingBindless(builder, binding_number, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_texture_count, VK_SHADER_STAGE_FRAGMENT_BIT, binding_flags, 0);
}

void zest_AddLayoutBuilderUniformBufferBindless(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint max_buffer_count, zest_supported_shader_stages shader_stages) {
    VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    zest_AddLayoutBuilderBindingBindless(builder, binding_number, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, max_buffer_count, shader_stages, binding_flags, 0);
}

void zest_AddLayoutBuilderStorageBufferBindless(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint max_buffer_count, zest_supported_shader_stages shader_stages) {
    VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    zest_AddLayoutBuilderBindingBindless(builder, binding_number, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, max_buffer_count, shader_stages, binding_flags, 0);
}

zest_set_layout zest_FinishDescriptorSetLayout(zest_set_layout_builder_t *builder, const char *name, ...) {
    ZEST_ASSERT(name);  //Give the descriptor set a unique name
    zest_text_t layer_name = { 0 };
    va_list args;
    va_start(args, name);
    zest_SetTextfv(&layer_name, name, args);
    va_end(args);
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->descriptor_layouts, layer_name.str));
    ZEST_ASSERT(builder->bindings);     //must have bindings to create the layout
    zest_uint binding_count = (zest_uint)zest_vec_size(builder->bindings);
    ZEST_ASSERT(binding_count > 0);     //Must add bindings before finishing the descriptor layout builder
    VkDescriptorSetLayoutCreateInfo layoutInfo = { 0 };
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = binding_count;
    layoutInfo.pBindings = builder->bindings;
    layoutInfo.flags = builder->create_flags;
    layoutInfo.pNext = 0;   

    VkDescriptorSetLayout layout;
    ZEST_VK_CHECK_RESULT(vkCreateDescriptorSetLayout(ZestDevice->logical_device, &layoutInfo, &ZestDevice->allocation_callbacks, &layout) != VK_SUCCESS);

    zest_set_layout set_layout = zest__add_descriptor_set_layout(layer_name.str, layout);
    set_layout->binding_indexes = builder->binding_indexes;
    zest_vec_resize(set_layout->layout_bindings, binding_count);
    zest_uint count = zest_vec_size(set_layout->layout_bindings);
    zest_uint size_of_binding = sizeof(VkDescriptorSetLayoutBinding);
    zest_uint size_in_bytes = zest_vec_size_in_bytes(builder->bindings);
    zest_vec_foreach(i, builder->bindings) {
        set_layout->layout_bindings[i] = builder->bindings[i];
    }

    zest_vec_free(builder->bindings);
    zest_FreeText(&layer_name);
    return set_layout;
}

zest_set_layout zest_FinishDescriptorSetLayoutForBindless(zest_set_layout_builder_t *builder, zest_uint num_global_sets_this_pool_should_support, VkDescriptorPoolCreateFlags additional_pool_flags, const char *name, ...) {
    ZEST_ASSERT(name);  //Give the descriptor set a unique name
    zest_text_t layer_name = { 0 };
    va_list args;
    va_start(args, name);
    zest_SetTextfv(&layer_name, name, args);
    va_end(args);
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->descriptor_layouts, layer_name.str));
    ZEST_ASSERT(builder->bindings);     //must have bindings to create the layout
    zest_uint binding_count = (zest_uint)zest_vec_size(builder->bindings);
	ZEST_ASSERT(binding_count > 0);     //Must add bindings before finishing the descriptor layout builder
    ZEST_ASSERT(binding_count == zest_vec_size(builder->binding_flags));

    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info = { 0 };
    binding_flags_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    binding_flags_create_info.bindingCount = binding_count; 
    binding_flags_create_info.pBindingFlags = builder->binding_flags; 

    VkDescriptorSetLayoutCreateInfo layoutInfo = { 0 };
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = binding_count;
    layoutInfo.pBindings = builder->bindings;
    layoutInfo.flags = builder->create_flags | VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.pNext = &binding_flags_create_info;   

    VkDescriptorSetLayout layout;
    ZEST_VK_CHECK_RESULT(vkCreateDescriptorSetLayout(ZestDevice->logical_device, &layoutInfo, &ZestDevice->allocation_callbacks, &layout) != VK_SUCCESS);

    zest_set_layout set_layout = zest__add_descriptor_set_layout(layer_name.str, layout);
    set_layout->binding_indexes = builder->binding_indexes;
    zest_vec_resize(set_layout->descriptor_indexes, zest_vec_size(builder->bindings));
    zest_vec_resize(set_layout->layout_bindings, binding_count);
    zest_uint count = zest_vec_size(set_layout->layout_bindings);
    zest_uint size_of_binding = sizeof(VkDescriptorSetLayoutBinding);
    zest_uint size_in_bytes = zest_vec_size_in_bytes(builder->bindings);
    zest_vec_foreach(i, builder->bindings) {
        set_layout->layout_bindings[i] = builder->bindings[i];
        set_layout->descriptor_indexes[i] = (zest_descriptor_indices_t){0};
        set_layout->descriptor_indexes[i].capacity = builder->bindings[i].descriptorCount;
        set_layout->descriptor_indexes[i].descriptor_type = builder->bindings[i].descriptorType;
    }
    zest_vec_free(builder->bindings);
    zest_vec_free(builder->binding_flags);

    zest_CreateDescriptorPoolForLayoutBindless(set_layout, 1, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

    zest_FreeText(&layer_name);
    return set_layout;
}

ZEST_API zest_descriptor_set zest_CreateBindlessSet(zest_set_layout layout) {
    ZEST_CHECK_HANDLE(layout);  //Not a valid layout
    ZEST_ASSERT(layout->vk_layout != VK_NULL_HANDLE && "VkDescriptorSetLayout in wrapper is null.");
    ZEST_ASSERT(layout->pool->vk_descriptor_pool != VK_NULL_HANDLE && "VkDescriptorPool in wrapper is null.");
    // max_sets_in_pool was set during pool creation, usually 1 for a global bindless set.
    ZEST_ASSERT(layout->pool->max_sets >= 1 && "Pool was not created to allow allocation of at least one set.");

    VkDescriptorSetAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = layout->pool->vk_descriptor_pool;
    alloc_info.descriptorSetCount = 1; // Maybe add this as a parameter at some point if it's needed
    alloc_info.pSetLayouts = &layout->vk_layout;

    zest_descriptor_set_t blank = { 0 };
    zest_descriptor_set set = ZEST__NEW(zest_descriptor_set);
    *set = blank;
    set->magic = zest_INIT_MAGIC;
    ZEST_VK_CHECK_RESULT(vkAllocateDescriptorSets(ZestDevice->logical_device, &alloc_info, &set->vk_descriptor_set));

    return set;
}

zest_set_layout zest__add_descriptor_set_layout(const char *name, VkDescriptorSetLayout layout) {
    zest_set_layout_t blank = { 0 };
    zest_set_layout descriptor_layout = ZEST__NEW(zest_set_layout);
    *descriptor_layout = blank;
    descriptor_layout->name.str = 0;
    descriptor_layout->magic = zest_INIT_MAGIC;
    zest_SetText(&descriptor_layout->name, name);
    descriptor_layout->vk_layout = layout;
    zest_map_insert(ZestRenderer->descriptor_layouts, name, descriptor_layout);
    return descriptor_layout;
}


zest_uint zest__acquire_bindless_index(zest_set_layout layout_handle, zest_uint binding_number) {
    ZEST_CHECK_HANDLE(layout_handle);   //Not a valid layout handle!
    if (binding_number >= zest_vec_size(layout_handle->descriptor_indexes)) {
        ZEST_PRINT("Attempted to acquire index for out-of-bounds binding_number %u for layout '%s'. Are you sure this is a layout that's configured for bindless descriptors?", binding_number, layout_handle->name.str);
        return ZEST_INVALID; 
    }

    zest_descriptor_indices_t *manager = &layout_handle->descriptor_indexes[binding_number];

    if (zest_vec_size(manager->free_indices) > 0) {
        zest_uint index = zest_vec_back(manager->free_indices); // Reuse from free list
        zest_vec_pop(manager->free_indices);
        return index;
    } else {
        if (manager->next_new_index < manager->capacity) {
            return manager->next_new_index++; // Allocate a new one sequentially
        } else {
            ZEST_PRINT("Ran out of bindless indices for binding %u (type %d, capacity %u) in layout '%s'!",
                binding_number, manager->descriptor_type, manager->capacity, layout_handle->name.str);
            return ZEST_INVALID;
        }
    }
}

void zest__release_bindless_index(zest_set_layout layout_handle, zest_uint binding_number, zest_uint index_to_release) {
    ZEST_CHECK_HANDLE(layout_handle);   //Not a valid layout handle!
    if (index_to_release == ZEST_INVALID) return;

    if (binding_number >= zest_vec_size(layout_handle->descriptor_indexes)) {
        ZEST_PRINT("Attempted to release index for out-of-bounds binding_number %u for layout '%s'. Check that layout is actually intended for bindless.", binding_number, layout_handle->name.str);
        return;
    }

    zest_descriptor_indices_t *manager = &layout_handle->descriptor_indexes[binding_number];

    zest_vec_foreach(i, manager->free_indices) {
        if (index_to_release == manager->free_indices[i]) {
			ZEST_PRINT("Attempted to release index for binding_number %u for layout '%s' that is already free. Make sure the binding number is correct.", binding_number, layout_handle->name.str);
            return;
        }
    }

    ZEST_ASSERT(index_to_release < manager->capacity);
    zest_vec_push(manager->free_indices, index_to_release);
}

VkDescriptorSetLayout zest_CreateDescriptorSetLayoutWithBindings(zest_uint count, VkDescriptorSetLayoutBinding* bindings) {
    ZEST_ASSERT(bindings);    //must have bindings to create the layout

    VkDescriptorSetLayoutCreateInfo layoutInfo = { 0 };
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = count;
    layoutInfo.pBindings = bindings;

    VkDescriptorSetLayout layout;

    ZEST_VK_CHECK_RESULT(vkCreateDescriptorSetLayout(ZestDevice->logical_device, &layoutInfo, &ZestDevice->allocation_callbacks, &layout) != VK_SUCCESS);

    return layout;
}

VkDescriptorSetLayoutBinding zest_CreateDescriptorLayoutBinding(VkDescriptorType type, zest_supported_shader_stages stageFlags, zest_uint binding, zest_uint descriptorCount) {
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

VkDescriptorSetLayoutBinding zest_CreateCombinedSamplerLayoutBinding(zest_uint binding) {

    VkDescriptorSetLayoutBinding sampler_layout_binding = { 0 };
    sampler_layout_binding.binding = binding;
    sampler_layout_binding.descriptorCount = 1;
    sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_layout_binding.pImmutableSamplers = ZEST_NULL;
    sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    return sampler_layout_binding;
}

VkDescriptorSetLayoutBinding zest_CreateSamplerLayoutBinding(zest_uint binding) {

    VkDescriptorSetLayoutBinding sampler_layout_binding = { 0 };
    sampler_layout_binding.binding = binding;
    sampler_layout_binding.descriptorCount = 1;
    sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
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

VkWriteDescriptorSet zest_CreateBufferDescriptorWriteWithType(VkDescriptorSet descriptor_set, VkDescriptorBufferInfo* buffer_info, zest_uint dst_binding, VkDescriptorType type) {
    VkWriteDescriptorSet write = { 0 };
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_set;
    write.dstBinding = dst_binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pBufferInfo = buffer_info;
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

zest_descriptor_set_builder_t zest_BeginDescriptorSetBuilder(zest_set_layout layout) {
    zest_descriptor_set_builder_t builder = { 0 };
    builder.associated_layout = layout;
    return builder;
}

VkDescriptorSetLayoutBinding *zest__get_layout_binding_info(zest_set_layout layout, zest_uint binding_index) {
    zest_vec_foreach(i, layout->layout_bindings) {
        if (layout->layout_bindings[i].binding == binding_index) {
            return &layout->layout_bindings[i];
        }
    }
    return NULL;
}

// The new general-purpose write function
void zest_AddSetBuilderWrite( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_uint descriptor_count, VkDescriptorType descriptor_type, const VkDescriptorImageInfo *p_image_infos, const VkDescriptorBufferInfo *p_buffer_infos, const VkBufferView *p_texel_buffer_views) {
    // Verify that the descriptor write complies with the associated descriptor set layout
    ZEST_ASSERT(builder != NULL && builder->associated_layout != NULL);
    const VkDescriptorSetLayoutBinding *layout_binding = zest__get_layout_binding_info(builder->associated_layout, dst_binding); 

    ZEST_ASSERT(layout_binding != NULL); // Binding must exist in the layout

    // 1. Type Check
    ZEST_ASSERT(layout_binding->descriptorType == descriptor_type); //The descriptor type must equal the same that's in the layout

    ZEST_ASSERT(dst_array_element + descriptor_count <= layout_binding->descriptorCount); //Ensure write fits within the layout's declared count for this binding

    // Immutable Sampler Check
    if (layout_binding->pImmutableSamplers != NULL) {
        if (descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLER) {
            // This write is redundant/conflicting if the layout has an immutable sampler.
            // Consider warning or making this an error. For now, let's assume it's an error to try.
            ZEST_ASSERT(false); //Attempting to write to a binding with an immutable sampler
            return;
        }
        if (descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && p_image_infos != NULL) {
            for (zest_uint i = 0; i < descriptor_count; ++i) {
                // The sampler part of the provided image_info will be ignored by Vulkan,
                // the immutable one from the layout is used.
                // It's good practice for p_image_infos[i].sampler to be VK_NULL_HANDLE here.
                ZEST_ASSERT(p_image_infos[i].sampler == VK_NULL_HANDLE); //Provided sampler in VkDescriptorImageInfo will be ignored due to immutable sampler in layout.
            }
        }
    } else {
        // If layout binding is NOT immutable sampler, but type is SAMPLER or COMBINED_IMAGE_SAMPLER,
        // ensure a sampler is actually provided.
        if ((descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLER || descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) && p_image_infos != NULL) {
            for (zest_uint i = 0; i < descriptor_count; ++i) {
                ZEST_ASSERT(p_image_infos[i].sampler != VK_NULL_HANDLE);
            }
        }
    }
    // --- END VERIFICATION ---

    VkWriteDescriptorSet write = { 0 };
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    // Note: write.dstSet will be set by zest_FinishDescriptorSet
    write.dstBinding = dst_binding;
    write.dstArrayElement = dst_array_element;
    write.descriptorType = descriptor_type;
    write.descriptorCount = descriptor_count;

    // Copy the descriptor info into the builder's internal storage and point the write to it
    if (p_image_infos) {
        zest_uint start_index = zest_vec_size(builder->image_infos_storage);
        for (zest_uint i = 0; i < descriptor_count; ++i) {
            zest_vec_push(builder->image_infos_storage, p_image_infos[i]);
        }
        // Point to the data now stored within the builder
        write.pImageInfo = &builder->image_infos_storage[start_index];
    } else if (p_buffer_infos) {
        zest_uint start_index = zest_vec_size(builder->buffer_infos_storage);
        for (zest_uint i = 0; i < descriptor_count; ++i) {
            zest_vec_push(builder->buffer_infos_storage, p_buffer_infos[i]);
        }
        write.pBufferInfo = &builder->buffer_infos_storage[start_index];
    } else if (p_texel_buffer_views) {
        zest_uint start_index = zest_vec_size(builder->texel_buffer_view_storage);
        for (zest_uint i = 0; i < descriptor_count; ++i) {
            zest_vec_push(builder->texel_buffer_view_storage, p_texel_buffer_views[i]);
        }
        write.pTexelBufferView = &builder->texel_buffer_view_storage[start_index];
    } else {
        ZEST_ASSERT(false);  //No descriptor info provided for write operation
        return;
    }

    zest_vec_push(builder->writes, write);
}

void zest_AddSetBuilderSampler( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, VkSampler sampler_handle) {
    ZEST_ASSERT(sampler_handle != VK_NULL_HANDLE);  //Must be a valid sampler handle
    VkDescriptorImageInfo image_info = { 0 };
    image_info.sampler = sampler_handle;
    image_info.imageView = VK_NULL_HANDLE;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; 

    zest_AddSetBuilderWrite(builder, dst_binding, dst_array_element, 1, VK_DESCRIPTOR_TYPE_SAMPLER, &image_info, NULL, NULL);
}

void zest_AddSetBuilderCombinedImageSampler( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, VkSampler sampler_handle, VkImageView image_view_handle, VkImageLayout layout) {
    // If the layout has an immutable sampler for this binding, sampler_handle here should ideally be NULL
    // and the verification in zest_AddSetBuilderWrite will check/warn.
    // If not immutable, sampler_handle must be valid.
    ZEST_ASSERT(image_view_handle != VK_NULL_HANDLE);

    VkDescriptorImageInfo image_info = { 0 };
    image_info.sampler = sampler_handle;
    image_info.imageView = image_view_handle;
    image_info.imageLayout = layout;

    zest_AddSetBuilderWrite(builder, dst_binding, dst_array_element, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &image_info, NULL, NULL);
}

void zest_AddSetBuilderTexture(zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_texture texture) {
    VkDescriptorImageInfo image_info = { 0 };
    image_info.sampler = texture->sampler->vk_sampler;
    image_info.imageView = texture->image_buffer.base_view;
    image_info.imageLayout = texture->image_layout;
    zest_AddSetBuilderWrite(builder, dst_binding, dst_array_element, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &image_info, NULL, NULL);
}

void zest_AddSetBuilderCombinedImageSamplers( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_uint count, const VkDescriptorImageInfo *p_image_infos ) {
    zest_AddSetBuilderWrite(builder, dst_binding, dst_array_element, count, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, p_image_infos, NULL, NULL);
}

void zest_AddSetBuilderDirectImageWrite( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, VkDescriptorType type, const VkDescriptorImageInfo *image_info) {
    zest_AddSetBuilderWrite(builder, dst_binding, dst_array_element, 1, type, image_info, NULL, NULL);
}

void zest_AddSetBuilderUniformBuffer( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_uniform_buffer uniform_buffer, zest_uint fif) {
    ZEST_CHECK_HANDLE(uniform_buffer);  //Not a valid uniform buffer
    VkDescriptorBufferInfo buffer_info = uniform_buffer->descriptor_info[fif];
    zest_AddSetBuilderWrite(builder, dst_binding, dst_array_element, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &buffer_info, NULL);
}

void zest_AddSetBuilderStorageBuffer( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_buffer storage_buffer) {
    ZEST_CHECK_HANDLE(storage_buffer);  //Not a valid storage buffer
    VkDescriptorBufferInfo buffer_info = zest_vk_GetBufferInfo(storage_buffer);
    zest_AddSetBuilderWrite(builder, dst_binding, dst_array_element, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &buffer_info, NULL);
}

zest_descriptor_set zest_FinishDescriptorSet(zest_descriptor_pool pool, zest_descriptor_set_builder_t *builder, zest_descriptor_set new_set_to_populate_or_update ) {
    ZEST_CHECK_HANDLE(pool);    //Must be a valid desriptor pool
    ZEST_ASSERT(builder != NULL && builder->associated_layout != NULL);

    if (!ZEST_VALID_HANDLE(new_set_to_populate_or_update)) {
        zest_descriptor_set_t blank = { 0 };
        new_set_to_populate_or_update = ZEST__NEW(zest_descriptor_set);
        *new_set_to_populate_or_update = blank;
        new_set_to_populate_or_update->magic = zest_INIT_MAGIC;
    }

    VkDescriptorSet target_set = new_set_to_populate_or_update->vk_descriptor_set;

    if (target_set == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo allocation_info = { 0 };
        allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocation_info.descriptorPool = pool->vk_descriptor_pool;
        allocation_info.descriptorSetCount = 1;
        allocation_info.pSetLayouts = &(builder->associated_layout->vk_layout); // Assuming your layout struct holds the Vk handle

        ZEST_VK_CHECK_RESULT(vkAllocateDescriptorSets(ZestDevice->logical_device, &allocation_info, &target_set));
    }

    // Update dstSet for all stored write operations
    zest_vec_foreach(i, builder->writes) {
        builder->writes[i].dstSet = target_set;
    }

    if (zest_vec_size(builder->writes) > 0) {
        vkUpdateDescriptorSets( ZestDevice->logical_device, zest_vec_size(builder->writes), builder->writes, 0, NULL);
    }

    // Clear the builder for potential reuse (or if it's single-use, this is part of cleanup)
    zest_vec_clear(builder->writes);
    zest_vec_clear(builder->image_infos_storage);
    zest_vec_clear(builder->buffer_infos_storage);
    zest_vec_clear(builder->texel_buffer_view_storage);

    new_set_to_populate_or_update->vk_descriptor_set = target_set;
    return new_set_to_populate_or_update;
}

zest_shader_resources zest_CreateShaderResources() {
    zest_shader_resources_t blank_bundle = { 0 };
    zest_shader_resources bundle = ZEST__NEW(zest_shader_resources);
    *bundle = blank_bundle;
    bundle->magic = zest_INIT_MAGIC;
    return bundle;
}

void zest_AddDescriptorSetToResources(zest_shader_resources resources, zest_descriptor_set descriptor_set, zest_uint fif) {
	zest_vec_push(resources->sets[fif], descriptor_set);
}

void zest_AddUniformBufferToResources(zest_shader_resources shader_resources, zest_uniform_buffer buffer) {
    zest_ForEachFrameInFlight(fif) {
        zest_AddDescriptorSetToResources(shader_resources, buffer->descriptor_set[fif], fif);
    }
}

void zest_AddGlobalBindlessSetToResources(zest_shader_resources shader_resources) {
    zest_ForEachFrameInFlight(fif) {
        zest_AddDescriptorSetToResources(shader_resources, ZestRenderer->global_set, fif);
    }
}

void zest_AddBindlessSetToResources(zest_shader_resources shader_resources, zest_descriptor_set set) {
    zest_ForEachFrameInFlight(fif) {
        zest_AddDescriptorSetToResources(shader_resources, set, fif);
    }
}

void zest_UpdateShaderResources(zest_shader_resources resources, zest_descriptor_set descriptor_set, zest_uint index, zest_uint fif) {
	ZEST_ASSERT(resources);    //Make sure the resources have been created, use zest_CreateShaderResources()
	ZEST_ASSERT(index < zest_vec_size(resources->sets));    //Must be a valid index for the descriptor set in the resources that you want to update.
	resources->sets[fif][index] = descriptor_set;
}

void zest_ClearShaderResources(zest_shader_resources shader_resources) {
    if (!shader_resources) return;
    zest_vec_clear(shader_resources->sets);
}

void zest_FreeShaderResources(zest_shader_resources shader_resources) {
    if (!shader_resources) return;
    zest_ForEachFrameInFlight(fif) {
        zest_vec_free(shader_resources->sets[fif]);
    }
}

bool zest_ValidateShaderResource(zest_shader_resources shader_resources) {
    ZEST_CHECK_HANDLE(shader_resources);
    zest_ForEachFrameInFlight(fif) {
        zest_uint set_size = zest_vec_size(shader_resources->sets[fif]);
        zest_vec_foreach(i, shader_resources->sets[fif]) {
            if (!ZEST_VALID_HANDLE(shader_resources->sets[fif][i])) {
                ZEST_PRINT("Descriptor set at frame in flight %i index %i was not valid. You need to make sure that you add a descriptor set for all frames in flight in the shader resource.", fif, i);
                return false;
            }
        }
    }
    return true;
}

zest_descriptor_pool zest__create_descriptor_pool(zest_uint max_sets) {
    zest_descriptor_pool_t blank = { 0 };
    zest_descriptor_pool pool = ZEST__NEW(zest_descriptor_pool);
    *pool = blank;
    pool->max_sets = max_sets;
    pool->magic = zest_INIT_MAGIC;
    return pool;
}

void zest_CreateDescriptorPoolForLayout(zest_set_layout layout, zest_uint max_set_count, VkDescriptorPoolCreateFlags pool_flags) {
    ZEST_ASSERT(max_set_count > 0); //Must set a set count for the pool

    zest_hash_map(zest_uint) descriptor_types;
    descriptor_types type_counts = { 0 };
    // Assuming ZestLayout->bindings is a vector of VkDescriptorSetLayoutBinding
    zest_vec_foreach(i, layout->layout_bindings) {
        // Check if this binding is purely an immutable sampler and should be skipped for pool sizing
        VkDescriptorSetLayoutBinding *binding = &layout->layout_bindings[i];
        bool is_purely_immutable_sampler_binding =
            (binding->descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER && binding->pImmutableSamplers != NULL);

        if (!is_purely_immutable_sampler_binding) {
            if (!zest_map_valid_key(type_counts, (zest_key)binding->descriptorType)) {
                zest_map_insert_key(type_counts, (zest_key)binding->descriptorType, binding->descriptorCount);
            } else {
                zest_uint *count = zest_map_at_key(type_counts, (zest_key)binding->descriptorType);
                *count += binding->descriptorCount;
            }
        }
    }

    VkDescriptorPoolSize *vk_pool_sizes = 0;
    zest_map_foreach(i, type_counts) {
        VkDescriptorType type = (VkDescriptorType)type_counts.map[i].key;
        zest_uint count = type_counts.data[i];
        if (count > 0) { // Only add if this type is actually used
            VkDescriptorPoolSize vk_pool_size = { 0 };
            vk_pool_size.type = type;
            vk_pool_size.descriptorCount = count * max_set_count;
            zest_vec_push(vk_pool_sizes, vk_pool_size);
        }
    }

    if (zest_vec_empty(vk_pool_sizes) && max_set_count > 0) {
        ZEST_ASSERT(0); //There were no pool sizes. Make sure the layout has valid descriptor types
    }

    VkDescriptorPoolCreateInfo pool_create_info = { 0 };
    pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.flags = pool_flags;

    if (layout->create_flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT) {
        pool_create_info.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    }

    pool_create_info.maxSets = max_set_count;
    pool_create_info.poolSizeCount = zest_vec_size(vk_pool_sizes);
    pool_create_info.pPoolSizes = vk_pool_sizes;

    zest_descriptor_pool pool = zest__create_descriptor_pool(max_set_count);
    ZEST_VK_CHECK_RESULT(vkCreateDescriptorPool(ZestDevice->logical_device, &pool_create_info, &ZestDevice->allocation_callbacks, &pool->vk_descriptor_pool));

    layout->pool = pool;
}

void zest_CreateDescriptorPoolForLayoutBindless(zest_set_layout layout, zest_uint max_set_count, VkDescriptorPoolCreateFlags pool_flags) {
    ZEST_ASSERT(max_set_count > 0); //Must set a set count for the pool

	zest_hash_map(zest_uint) descriptor_types;
    descriptor_types type_counts = { 0 };
    // Assuming ZestLayout->bindings is a vector of VkDescriptorSetLayoutBinding
    zest_vec_foreach(i, layout->layout_bindings) {
        // Check if this binding is purely an immutable sampler and should be skipped for pool sizing
        VkDescriptorSetLayoutBinding *binding = &layout->layout_bindings[i];
        bool is_purely_immutable_sampler_binding =
            (binding->descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER && binding->pImmutableSamplers != NULL);

        if (!is_purely_immutable_sampler_binding) {
            if (!zest_map_valid_key(type_counts, (zest_key)binding->descriptorType)) {
                zest_map_insert_key(type_counts, (zest_key)binding->descriptorType, binding->descriptorCount);
            } else {
                zest_uint *count = zest_map_at_key(type_counts, (zest_key)binding->descriptorType);
                *count += binding->descriptorCount;
            }
        }
    }

    VkDescriptorPoolSize *pool_sizes = NULL; 

    // Iterate through the bindings defined in the builder to sum up descriptor counts by type
    zest_vec_foreach(i, layout->layout_bindings) {
        VkDescriptorSetLayoutBinding *binding = &layout->layout_bindings[i];
        zest_bool found_type = ZEST_FALSE;
        // Aggregate counts for the same descriptor type
        zest_vec_foreach(j, pool_sizes) {
            if (pool_sizes[j].type == binding->descriptorType) {
                pool_sizes[j].descriptorCount += binding->descriptorCount;
                found_type = ZEST_TRUE;
                break;
            }
        }
        if (!found_type) {
            VkDescriptorPoolSize new_pool_size = { binding->descriptorType, binding->descriptorCount };
            zest_vec_push(pool_sizes, new_pool_size);
        }
    }

    if (zest_vec_empty(pool_sizes) && max_set_count > 0) {
        ZEST_ASSERT(0); //There were no pool sizes. Make sure the layout has valid descriptor types
    }

    VkDescriptorPoolCreateInfo pool_create_info = { 0 };
    pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.flags = pool_flags;

    if (layout->create_flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT) {
        pool_create_info.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    }

    pool_create_info.maxSets = max_set_count;
    pool_create_info.poolSizeCount = zest_vec_size(pool_sizes);
    pool_create_info.pPoolSizes = pool_sizes;

    zest_descriptor_pool pool = zest__create_descriptor_pool(max_set_count);
    ZEST_VK_CHECK_RESULT(vkCreateDescriptorPool(ZestDevice->logical_device, &pool_create_info, &ZestDevice->allocation_callbacks, &pool->vk_descriptor_pool));

    layout->pool = pool;
}

void zest_ResetDescriptorPool(zest_descriptor_pool pool) {
    ZEST_CHECK_HANDLE(pool);        //Not a valid descriptor pool
    vkResetDescriptorPool(ZestDevice->logical_device, pool->vk_descriptor_pool, 0);
    pool->allocations = 0;
}

void zest_FreeDescriptorPool(zest_descriptor_pool pool) {
    ZEST_CHECK_HANDLE(pool);        //Not a valid descriptor pool
    vkResetDescriptorPool(ZestDevice->logical_device, pool->vk_descriptor_pool, 0);
    vkDestroyDescriptorPool(ZestDevice->logical_device, pool->vk_descriptor_pool, &ZestDevice->allocation_callbacks);
    ZEST__FREE(pool);
}

void zest_UpdateDescriptorSet(VkWriteDescriptorSet* descriptor_writes) {
    vkUpdateDescriptorSets(ZestDevice->logical_device, zest_vec_size(descriptor_writes), descriptor_writes, 0, ZEST_NULL);
}

zest_pipeline zest__create_pipeline() {
    zest_pipeline_t blank_pipeline = {
        .pipeline_template = {0},
        .pipeline = VK_NULL_HANDLE,
        .pipeline_layout = VK_NULL_HANDLE,
        .rebuild_pipeline_function = ZEST_NULL,
        .flags = zest_pipeline_set_flag_none,
    };
    zest_pipeline pipeline = ZEST__NEW(zest_pipeline);
    *pipeline = blank_pipeline;
    pipeline->magic = zest_INIT_MAGIC;
    return pipeline;
}

void zest_SetPipelineVertShader(zest_pipeline_template pipeline_template, const char* file, const char* prefix) {
    zest_SetText(&pipeline_template->vertShaderFile, file);
    if (prefix) {
        zest_SetText(&pipeline_template->shader_path_prefix, prefix);
    }
}

void zest_SetPipelineFragShader(zest_pipeline_template pipeline_template, const char* file, const char* prefix) {
    zest_SetText(&pipeline_template->fragShaderFile, file);
    if (prefix) {
        zest_SetText(&pipeline_template->shader_path_prefix, prefix);
    }
}

void zest_SetPipelineShaders(zest_pipeline_template pipeline_template, const char *vertex_shader, const char *fragment_shader, const char *prefix) {
    ZEST_ASSERT(vertex_shader);     //Must set the name of the shaders
    ZEST_ASSERT(fragment_shader);     //Must set the name of the shaders
    zest_SetText(&pipeline_template->fragShaderFile, fragment_shader);
    if (prefix) {
        zest_SetText(&pipeline_template->shader_path_prefix, prefix);
    }
    zest_SetText(&pipeline_template->vertShaderFile, vertex_shader);
    if (prefix) {
        zest_SetText(&pipeline_template->shader_path_prefix, prefix);
    }
}

void zest_SetPipelineShader(zest_pipeline_template pipeline_template, const char* file, const char* prefix) {
    zest_SetText(&pipeline_template->fragShaderFile, file);
    zest_SetText(&pipeline_template->vertShaderFile, file);
    if (prefix) {
        zest_SetText(&pipeline_template->shader_path_prefix, prefix);
    }
}

void zest_SetPipelinePushConstantRange(zest_pipeline_template pipeline_template, zest_uint size, zest_supported_shader_stages stage_flags) {
    VkPushConstantRange range;
    range.size = size;
    range.offset = 0;
    range.stageFlags = stage_flags;
    pipeline_template->pushConstantRange = range;
}

void zest_SetPipelinePushConstants(zest_pipeline_template pipeline_template, void *push_constants) {
    pipeline_template->push_constants = push_constants;
}

void zest_SetPipelineBlend(zest_pipeline_template pipeline_template, VkPipelineColorBlendAttachmentState blend_attachment) {
    pipeline_template->colorBlendAttachment = blend_attachment;
}

void zest_SetPipelineDepthTest(zest_pipeline_template pipeline_template, bool enable_test, bool write_enable) {
	pipeline_template->depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipeline_template->depthStencil.depthTestEnable = enable_test;
	pipeline_template->depthStencil.depthWriteEnable = write_enable;
	pipeline_template->depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	pipeline_template->depthStencil.depthBoundsTestEnable = VK_FALSE;
	pipeline_template->depthStencil.stencilTestEnable = VK_FALSE;
}

void zest_AddPipelineDescriptorLayout(zest_pipeline_template pipeline_template, VkDescriptorSetLayout layout) {
    zest_vec_push(pipeline_template->descriptorSetLayouts, layout);
}

void zest_ClearPipelineDescriptorLayouts(zest_pipeline_template pipeline_template) {
    zest_vec_clear(pipeline_template->descriptorSetLayouts);
}

VkShaderStageFlags zest_PipelinePushConstantStageFlags(zest_pipeline pipeline, zest_uint index) {
    ZEST_ASSERT(index < pipeline->pipeline_template->pipelineLayoutInfo.pushConstantRangeCount);    //Index must not be outside the range of the number of push constants
    return pipeline->pipeline_template->pipelineLayoutInfo.pPushConstantRanges[index].stageFlags;
}

zest_uint zest_PipelinePushConstantSize(zest_pipeline pipeline, zest_uint index) {
    ZEST_ASSERT(index < pipeline->pipeline_template->pipelineLayoutInfo.pushConstantRangeCount);    //Index must not be outside the range of the number of push constants
    return pipeline->pipeline_template->pipelineLayoutInfo.pPushConstantRanges[index].size;
}

zest_uint zest_PipelinePushConstantOffset(zest_pipeline pipeline, zest_uint index) {
    ZEST_ASSERT(index < pipeline->pipeline_template->pipelineLayoutInfo.pushConstantRangeCount); //Index must not be outside the range of the number of push constants
    return pipeline->pipeline_template->pipelineLayoutInfo.pPushConstantRanges[index].offset;
}

VkVertexInputBindingDescription zest_AddVertexInputBindingDescription(zest_pipeline_template pipeline_template, zest_uint binding, zest_uint stride, VkVertexInputRate input_rate) {
    for (zest_foreach_i(pipeline_template->bindingDescriptions)) {
        //You already have a binding with that index in the bindindDescriptions array
        //Maybe you copied a template with zest_CopyPipelineTemplate but didn't call zest_ClearVertexInputBindingDescriptions on the copy before
        //adding your own
        ZEST_ASSERT(binding != pipeline_template->bindingDescriptions[i].binding);
    }
    VkVertexInputBindingDescription input_binding_description = { 0 };
    input_binding_description.binding = binding;
    input_binding_description.stride = stride;
    input_binding_description.inputRate = input_rate;
    zest_vec_push(pipeline_template->bindingDescriptions, input_binding_description);
    zest_size size = zest_vec_size(pipeline_template->bindingDescriptions);
    return input_binding_description;
}

void zest_ClearVertexInputBindingDescriptions(zest_pipeline_template pipeline_template) {
    zest_vec_clear(pipeline_template->bindingDescriptions);
}

void zest_ClearVertexAttributeDescriptions(zest_pipeline_template pipeline_template) {
    zest_vec_clear(pipeline_template->attributeDescriptions);
}

void zest_ClearPipelinePushConstantRanges(zest_pipeline_template pipeline_template) {
    pipeline_template->pushConstantRange.offset = 0;
    pipeline_template->pushConstantRange.size = 0;
    pipeline_template->pushConstantRange.stageFlags = 0;
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

void zest_BindPipelineShaderResource(VkCommandBuffer command_buffer, zest_pipeline pipeline, zest_shader_resources shader_resources) {
	ZEST_ASSERT(shader_resources); //Not a valid shader resource
    zest_vec_foreach(set_index, shader_resources->sets[ZEST_FIF]) {
        zest_descriptor_set set = shader_resources->sets[ZEST_FIF][set_index];
        ZEST_CHECK_HANDLE(set);     //Not a valid desriptor set in the shaer resource. Did you set all frames in flight?
		zest_vec_push(shader_resources->binding_sets, set->vk_descriptor_set);
	}
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0, zest_vec_size(shader_resources->binding_sets), shader_resources->binding_sets, 0, 0);
    zest_vec_clear(shader_resources->binding_sets);
}

void zest_BindPipeline(VkCommandBuffer command_buffer, zest_pipeline_t* pipeline, VkDescriptorSet *descriptor_set, zest_uint set_count) {
    ZEST_ASSERT(set_count && descriptor_set);    //No descriptor sets. Must bind the pipeline with a valid desriptor set
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0, set_count, descriptor_set, 0, 0);
}

void zest_BindComputePipeline(VkCommandBuffer command_buffer, zest_compute compute, VkDescriptorSet *descriptor_set, zest_uint set_count) {
    ZEST_CHECK_HANDLE(compute);                 //Not a valid compute handle
    ZEST_ASSERT(set_count && descriptor_set);   //No descriptor sets. Must bind the pipeline with a valid desriptor set
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipeline_layout, 0, set_count, descriptor_set, 0, 0);
}

void zest__set_pipeline_template(zest_pipeline_template pipeline_template) {
    pipeline_template->vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    pipeline_template->vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_template->vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

    pipeline_template->fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_template->fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

    pipeline_template->inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipeline_template->inputAssembly.primitiveRestartEnable = VK_FALSE;
    pipeline_template->inputAssembly.topology = pipeline_template->topology;
    pipeline_template->inputAssembly.flags = 0;

    if (!pipeline_template->no_vertex_input) {
        ZEST_ASSERT(zest_vec_size(pipeline_template->bindingDescriptions));    //If the pipeline is set to have vertex input, then you must add bindingDescriptions. You can use zest_AddVertexInputBindingDescription for this
        pipeline_template->vertexInputInfo.vertexBindingDescriptionCount = (zest_uint)zest_vec_size(pipeline_template->bindingDescriptions);
        pipeline_template->vertexInputInfo.pVertexBindingDescriptions = pipeline_template->bindingDescriptions;
        pipeline_template->vertexInputInfo.vertexAttributeDescriptionCount = (zest_uint)zest_vec_size(pipeline_template->attributeDescriptions);
        pipeline_template->vertexInputInfo.pVertexAttributeDescriptions = pipeline_template->attributeDescriptions;
    }

    pipeline_template->viewport.x = 0.0f;
    pipeline_template->viewport.y = 0.0f;
    pipeline_template->viewport.width = (float)pipeline_template->scissor.extent.width;
    pipeline_template->viewport.height = (float)pipeline_template->scissor.extent.height;
    pipeline_template->viewport.minDepth = 0.0f;
    pipeline_template->viewport.maxDepth = 1.0f;

    pipeline_template->scissor = pipeline_template->scissor;

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

    if (pipeline_template->depthStencil.sType != VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO) {
        pipeline_template->depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        pipeline_template->depthStencil.depthTestEnable = VK_TRUE;
        pipeline_template->depthStencil.depthWriteEnable = VK_TRUE;
        pipeline_template->depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        pipeline_template->depthStencil.depthBoundsTestEnable = VK_FALSE;
        pipeline_template->depthStencil.stencilTestEnable = VK_FALSE;
    }

    pipeline_template->colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pipeline_template->colorBlending.logicOpEnable = VK_FALSE;
    pipeline_template->colorBlending.logicOp = VK_LOGIC_OP_COPY;
    pipeline_template->colorBlending.attachmentCount = 1;
    pipeline_template->colorBlending.pAttachments = &pipeline_template->colorBlendAttachment;
    pipeline_template->colorBlending.blendConstants[0] = 0.0f;
    pipeline_template->colorBlending.blendConstants[1] = 0.0f;
    pipeline_template->colorBlending.blendConstants[2] = 0.0f;
    pipeline_template->colorBlending.blendConstants[3] = 0.0f;

    if (zest_vec_size(pipeline_template->descriptorSetLayouts) > 0) {
        pipeline_template->pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_template->pipelineLayoutInfo.setLayoutCount = zest_vec_size(pipeline_template->descriptorSetLayouts);
        pipeline_template->pipelineLayoutInfo.pSetLayouts = pipeline_template->descriptorSetLayouts;
    }

    if (pipeline_template->pushConstantRange.size) {
        pipeline_template->pipelineLayoutInfo.pPushConstantRanges = &pipeline_template->pushConstantRange;
        pipeline_template->pipelineLayoutInfo.pushConstantRangeCount = 1;
    }

    pipeline_template->dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pipeline_template->dynamicState.dynamicStateCount = (zest_uint)(zest_vec_size(pipeline_template->dynamicStates));
    pipeline_template->dynamicState.pDynamicStates = pipeline_template->dynamicStates;
    pipeline_template->dynamicState.flags = 0;
}

zest_pipeline zest__cache_pipeline(zest_pipeline_template pipeline_template, VkRenderPass render_pass) {
	zest_pipeline pipeline = zest__create_pipeline();
    pipeline->pipeline_template = pipeline_template;
    pipeline->render_pass = render_pass;
    zest_BuildPipeline(pipeline);
    zest_key pipeline_key = zest_Hash(pipeline_template->name.str, zest_TextLength(&pipeline_template->name), ZEST_HASH_SEED);
    struct {
        zest_key pipeline_key;
        VkRenderPass render_pass;
    } cached_pipeline;
    cached_pipeline.pipeline_key = pipeline_key;
    cached_pipeline.render_pass = render_pass;
    zest_key cached_pipeline_key = zest_Hash(&cached_pipeline, sizeof(cached_pipeline), ZEST_HASH_SEED);
    zest_map_insert_key(ZestRenderer->cached_pipelines, cached_pipeline_key, pipeline);
    return pipeline;
}

void zest_BuildPipeline(zest_pipeline pipeline) {
    ZEST_VK_CHECK_RESULT(vkCreatePipelineLayout(ZestDevice->logical_device, &pipeline->pipeline_template->pipelineLayoutInfo, &ZestDevice->allocation_callbacks, &pipeline->pipeline_layout));

    if (!pipeline->pipeline_template->vertShaderFile.str || !pipeline->pipeline_template->fragShaderFile.str) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "No vertex or fragment shader specified when building pipeline %s", pipeline->pipeline_template->name.str);
        ZEST_ASSERT(0);        //You must specify a vertex and frag shader file to load
    }

    VkShaderModule vert_shader_module = { 0 };
    VkShaderModule frag_shader_module = { 0 };
	zest_text_t vert_path = { 0 };
	zest_text_t frag_path = { 0 };
	zest_SetTextf(&vert_path, "%s%s", pipeline->pipeline_template->shader_path_prefix, pipeline->pipeline_template->vertShaderFile.str);
	zest_SetTextf(&frag_path, "%s%s", pipeline->pipeline_template->shader_path_prefix, pipeline->pipeline_template->fragShaderFile.str);
    if (zest_map_valid_name(ZestRenderer->shaders, vert_path.str)) {
        zest_shader vert_shader = *zest_map_at(ZestRenderer->shaders, vert_path.str);
        if (!vert_shader->spv) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Failed to find any spv data at %s", vert_path.str);
            ZEST_ASSERT(0);    //Shader must have been compiled first before building the pipeline
        }
        vert_shader_module = zest__create_shader_module(vert_shader->spv);
        pipeline->pipeline_template->vertShaderStageInfo.module = vert_shader_module;
    }
    else {
        zest_shader vert_shader = zest_AddShaderFromSPVFile(vert_path.str, shaderc_vertex_shader);
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Failed to find any shader at %s. Trying to load spv instead.", vert_path.str);
        ZEST_ASSERT(vert_shader);        //Failed to load the shader file, make sure it exists at the location
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "No spv file was found at %s", vert_path.str);
        vert_shader_module = zest__create_shader_module(vert_shader->spv);
        pipeline->pipeline_template->vertShaderStageInfo.module = vert_shader_module;
    }

    if (zest_map_valid_name(ZestRenderer->shaders, frag_path.str)) {
        zest_shader frag_shader = *zest_map_at(ZestRenderer->shaders, frag_path.str);
        if (!frag_shader->spv) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Failed to find any spv data at %s.", frag_path.str);
            ZEST_ASSERT(0);    //Shader must have been compiled first before building the pipeline
        }
        frag_shader_module = zest__create_shader_module(frag_shader->spv);
        pipeline->pipeline_template->fragShaderStageInfo.module = frag_shader_module;
    }
    else {
        zest_shader frag_shader = zest_AddShaderFromSPVFile(frag_path.str, shaderc_fragment_shader);
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Failed to find any shader at %s. Trying to load spv instead.", frag_path.str);
        ZEST_ASSERT(frag_shader);        //Failed to load the shader file, make sure it exists at the location
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "No spv file was found at %s", frag_path.str);
        frag_shader_module = zest__create_shader_module(frag_shader->spv);
        pipeline->pipeline_template->fragShaderStageInfo.module = frag_shader_module;
    }
	zest_FreeText(&frag_path);
	zest_FreeText(&vert_path);

    pipeline->pipeline_template->vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline->pipeline_template->vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    pipeline->pipeline_template->vertShaderStageInfo.pName = pipeline->pipeline_template->vertShaderFunctionName.str;

    pipeline->pipeline_template->fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline->pipeline_template->fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipeline->pipeline_template->fragShaderStageInfo.pName = pipeline->pipeline_template->fragShaderFunctionName.str;

    VkPipelineShaderStageCreateInfo shaderStages[2] = { pipeline->pipeline_template->vertShaderStageInfo, pipeline->pipeline_template->fragShaderStageInfo };

    VkGraphicsPipelineCreateInfo pipeline_info = { 0 };
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shaderStages;
    pipeline_info.pVertexInputState = &pipeline->pipeline_template->vertexInputInfo;
    pipeline_info.pInputAssemblyState = &pipeline->pipeline_template->inputAssembly;
    pipeline_info.pViewportState = &pipeline->pipeline_template->viewportState;
    pipeline_info.pRasterizationState = &pipeline->pipeline_template->rasterizer;
    pipeline_info.pMultisampleState = &pipeline->pipeline_template->multisampling;
    pipeline_info.pColorBlendState = &pipeline->pipeline_template->colorBlending;
    pipeline_info.pDepthStencilState = &pipeline->pipeline_template->depthStencil;
    pipeline_info.layout = pipeline->pipeline_layout;
    pipeline_info.renderPass = pipeline->render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    if (pipeline->pipeline_template->dynamicState.dynamicStateCount) {
        pipeline_info.pDynamicState = &pipeline->pipeline_template->dynamicState;
    }

    ZEST_VK_CHECK_RESULT(vkCreateGraphicsPipelines(ZestDevice->logical_device, ZestRenderer->pipeline_cache, 1, &pipeline_info, &ZestDevice->allocation_callbacks, &pipeline->pipeline));
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Built pipeline %s", pipeline->pipeline_template->name.str);

    vkDestroyShaderModule(ZestDevice->logical_device, frag_shader_module, &ZestDevice->allocation_callbacks);
    vkDestroyShaderModule(ZestDevice->logical_device, vert_shader_module, &ZestDevice->allocation_callbacks);
}

void zest_EndPipelineTemplate(zest_pipeline_template pipeline_template) {
    zest__set_pipeline_template(pipeline_template);
    pipeline_template->multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
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

zest_pipeline_template zest_CopyPipelineTemplate(const char *name, zest_pipeline_template pipeline_to_copy) {
    zest_pipeline_template copy = zest_BeginPipelineTemplate(name);
    copy->no_vertex_input = pipeline_to_copy->no_vertex_input;
    copy->topology = pipeline_to_copy->topology;
    copy->viewport = pipeline_to_copy->viewport;
    copy->pushConstantRange = pipeline_to_copy->pushConstantRange;
    zest_vec_clear(copy->descriptorSetLayouts);
    zest_vec_foreach(i, pipeline_to_copy->descriptorSetLayouts) {
        zest_vec_push(copy->descriptorSetLayouts, pipeline_to_copy->descriptorSetLayouts[i]);
    }
    if (pipeline_to_copy->bindingDescriptions) {
        zest_vec_resize(copy->bindingDescriptions, zest_vec_size(pipeline_to_copy->bindingDescriptions));
        memcpy(copy->bindingDescriptions, pipeline_to_copy->bindingDescriptions, zest_vec_size_in_bytes(pipeline_to_copy->bindingDescriptions));
    }
    if (pipeline_to_copy->dynamicStates) {
        zest_vec_resize(copy->dynamicStates, zest_vec_size(pipeline_to_copy->dynamicStates));
        memcpy(copy->dynamicStates, pipeline_to_copy->dynamicStates, zest_vec_size_in_bytes(pipeline_to_copy->dynamicStates));
    }
    if (pipeline_to_copy->attributeDescriptions) {
        zest_vec_resize(copy->attributeDescriptions, zest_vec_size(pipeline_to_copy->attributeDescriptions));
        memcpy(copy->attributeDescriptions, pipeline_to_copy->attributeDescriptions, zest_vec_size_in_bytes(pipeline_to_copy->attributeDescriptions));
    }
    copy->fragShaderFile.str = 0;
    copy->fragShaderFunctionName.str = 0;
    copy->vertShaderFile.str = 0;
    copy->vertShaderFunctionName.str = 0;
    zest_SetText(&copy->fragShaderFile, pipeline_to_copy->fragShaderFile.str);
    zest_SetText(&copy->vertShaderFile, pipeline_to_copy->vertShaderFile.str);
    zest_SetText(&copy->fragShaderFunctionName, pipeline_to_copy->fragShaderFunctionName.str);
    zest_SetText(&copy->vertShaderFunctionName, pipeline_to_copy->vertShaderFunctionName.str);
    zest_SetText(&copy->shader_path_prefix, pipeline_to_copy->shader_path_prefix.str);
    return copy;
}

void zest__refresh_pipeline_template(zest_pipeline_template pipeline_template) {
    //Note: not setting this for pipelines messes with scaling, but not sure if some pipelines need this to be fixed
    pipeline_template->scissor.extent = zest_GetSwapChainExtent();
    zest__update_pipeline_template(pipeline_template);
}

void zest__rebuild_pipeline(zest_pipeline pipeline) {
    zest_BuildPipeline(pipeline);
}

zest_shader zest_NewShader(shaderc_shader_kind type) {
    zest_shader_t blank_shader = { 0 };
    zest_shader shader = ZEST__NEW(zest_shader);
    *shader = blank_shader;
    shader->magic = zest_INIT_MAGIC;
    shader->type = type;
    return shader;
}

shaderc_compilation_result_t zest_ValidateShader(const char *shader_code, shaderc_shader_kind type, const char *name, shaderc_compiler_t compiler) {
    shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, shader_code, strlen(shader_code), type, name, "main", NULL);
    return result;
}

zest_bool zest_CompileShader(zest_shader shader, shaderc_compiler_t compiler) {
    shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, shader->shader_code.str, zest_TextLength(&shader->shader_code), shader->type, shader->name.str, "main", NULL);
    if (shaderc_result_get_compilation_status(result) == shaderc_compilation_status_success) {
		ZEST_APPEND_LOG(ZestDevice->log_path.str, "Successfully compiled shader: %s.", shader->name.str);
        zest_UpdateShaderSPV(shader, result);
        return 1;
    }
	const char *error_message = shaderc_result_get_error_message(result);
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Error compiling shader: %s.\n%s", shader->name.str, error_message);
    return 0;
}

zest_shader zest_CreateShaderFromFile(const char *file, const char *name, shaderc_shader_kind type, zest_bool disable_caching, shaderc_compiler_t compiler, shaderc_compile_options_t options) {
    char *shader_code = zest_ReadEntireFile(file, ZEST_TRUE);
    ZEST_ASSERT(shader_code);   //Unable to load the shader code, check the path is valid
    zest_shader shader = zest_CreateShader(shader_code, type, name, ZEST_FALSE, disable_caching, compiler, options);
    zest_SetText(&shader->file_path, file);
    zest_vec_free(shader_code);
    return shader;
}

zest_bool zest_ReloadShader(zest_shader shader) {
    ZEST_ASSERT(zest_TextLength(&shader->file_path));    //The shader must have a file path set.
    char *shader_code = zest_ReadEntireFile(shader->file_path.str, ZEST_TRUE);
    if (!shader_code) {
        return 0;
    }
    zest_SetText(&shader->shader_code, shader_code);
    return 1;
}

zest_shader zest_CreateShader(const char *shader_code, shaderc_shader_kind type, const char *name, zest_bool format_code, zest_bool disable_caching, shaderc_compiler_t compiler, shaderc_compile_options_t options) {
    ZEST_ASSERT(name);     //You must give the shader a name
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->shaders, name));     //Shader already exitst, use zest_UpdateShader to update an existing shader
    zest_shader shader = zest_NewShader(type);
    if (zest_TextSize(&ZestRenderer->shader_path_prefix)) {
        zest_SetTextf(&shader->name, "%s%s", ZestRenderer->shader_path_prefix, name);
    }
    else {
        zest_SetTextf(&shader->name, "%s", name);
    }
    shader->type = type;
    if (!disable_caching && ZestApp->create_info.flags & zest_init_flag_cache_shaders) {
        shader->spv = zest_ReadEntireFile(shader->name.str, ZEST_FALSE);
        if (shader->spv) {
            shader->spv_size = zest_vec_size(shader->spv);
			zest_SetText(&shader->shader_code, shader_code);
            if (format_code != 0) {
                zest__format_shader_code(&shader->shader_code);
            }
			zest_map_insert(ZestRenderer->shaders, shader->name.str, shader);
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Loaded shader %s from cache and added to renderer shaders.", name);
            return shader;
        }
    }
    
    if(!compiler) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Was unable to load the shader from the cached shaders location and compiler is disabled, so cannot go any further with shader %s", name);
        return shader;
    }

	zest_SetText(&shader->shader_code, shader_code);
	if (format_code != 0) {
		zest__format_shader_code(&shader->shader_code);
	}
    shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, shader->shader_code.str, zest_TextLength(&shader->shader_code), type, name, "main", options );

    if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {
		ZEST_APPEND_LOG(ZestDevice->log_path.str, "Shader compilation failed: %s, %s", name, shaderc_result_get_error_message(result));
        shaderc_result_release(result);
        zest_FreeShader(shader);
        ZEST_ASSERT(0); //There's a bug in this shader that needs fixing. You can check the log file for the error message
    }

    zest_uint spv_size = (zest_uint)shaderc_result_get_length(result);
    const char *spv_binary = shaderc_result_get_bytes(result);
    zest_vec_resize(shader->spv, spv_size);
    memcpy(shader->spv, spv_binary, spv_size);
    shader->spv_size = spv_size;
    zest_map_insert(ZestRenderer->shaders, shader->name.str, shader);
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Compiled shader %s and added to renderer shaders.", name);
    shaderc_result_release(result);
    if (!disable_caching && ZestApp->create_info.flags & zest_init_flag_cache_shaders) {
        zest_CacheShader(shader);
    }
    return shader;
}

void zest_CacheShader(zest_shader shader) {
    zest__create_folder(ZestRenderer->shader_path_prefix.str);
    FILE *shader_file = zest__open_file(shader->name.str, "wb");
    if (shader_file == NULL) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Failed to open file for writing: %s", shader->name.str);
    }
    size_t written = fwrite(shader->spv, 1, shader->spv_size, shader_file);
    if (written != shader->spv_size) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Failed to write entire shader to file: %s", shader->name.str);
        fclose(shader_file);
    }
    fclose(shader_file);
}

void zest_UpdateShaderSPV(zest_shader shader, shaderc_compilation_result_t result) {
    zest_uint spv_size = (zest_uint)shaderc_result_get_length(result);
    const char *spv_binary = shaderc_result_get_bytes(result);
    shader->spv_size = spv_size;
    zest_vec_resize(shader->spv, spv_size);
    memcpy(shader->spv, spv_binary, spv_size);
}

zest_shader zest_AddShaderFromSPVFile(const char *filename, shaderc_shader_kind type) {
    ZEST_ASSERT(filename);     //You must give the shader a name
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->shaders, filename));     //Shader already exists, use zest_UpdateShader to update an existing shader
    zest_shader shader = zest_NewShader(type);
    shader->spv = zest_ReadEntireFile(filename, ZEST_FALSE);
    ZEST_ASSERT(shader->spv);   //File not found, could not load this shader!
    shader->spv_size = zest_vec_size(shader->spv);
	zest_SetText(&shader->name, filename);
	zest_map_insert(ZestRenderer->shaders, shader->name.str, shader);
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Loaded shader %s and added to renderer shaders.", filename);
	return shader;
    zest_FreeShader(shader);
    return 0;
}

zest_shader zest_AddShaderFromSPVMemory(const char *name, const void *buffer, zest_uint size, shaderc_shader_kind type) {
    ZEST_ASSERT(name);     //You must give the shader a name
    ZEST_ASSERT(!strstr(name, "/"));    //name must not contain /, the shader will be prefixed with the cache folder automatically
    if (buffer && size) {
		zest_shader shader = zest_NewShader(type);
		if (zest_TextSize(&ZestRenderer->shader_path_prefix)) {
			zest_SetTextf(&shader->name, "%s%s", ZestRenderer->shader_path_prefix, name);
		}
		else {
			zest_SetTextf(&shader->name, "%s", name);
		}
		ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->shaders, shader->name.str));     //Shader already exitst, use zest_UpdateShader to update an existing shader
		zest_vec_resize(shader->spv, size);
		memcpy(shader->spv, buffer, size);
        zest_map_insert(ZestRenderer->shaders, shader->name.str, shader);
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Read shader %s from memory and added to renderer shaders.", name);
        shader->spv_size = size;
        return shader;
    }
    return 0;
}

void zest_AddShader(zest_shader shader, const char *name) {
    if (name) {
		ZEST_ASSERT(!strstr(name, "/"));    //name must not contain /, the shader will be prefixed with the cache folder automatically
        if (zest_TextSize(&ZestRenderer->shader_path_prefix)) {
            zest_SetTextf(&shader->name, "%s%s", ZestRenderer->shader_path_prefix, name);
        }
        else {
            zest_SetTextf(&shader->name, "%s", name);
        }
    }
    else {
        ZEST_ASSERT(shader->name.str);
    }
	ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->shaders, shader->name.str));     //Shader already exists, use zest_UpdateShader to update an existing shader
	zest_map_insert(ZestRenderer->shaders, shader->name.str, shader);
}

zest_shader zest_GetShader(const char *name) {
    if (zest_map_valid_name(ZestRenderer->shaders, name)) {
        return *zest_map_at(ZestRenderer->shaders, name);
    }
    return NULL;
}

zest_shader zest_CopyShader(const char *name, const char *new_name) {
    if (zest_map_valid_name(ZestRenderer->shaders, name)) {
        zest_shader shader = *zest_map_at(ZestRenderer->shaders, name);
        zest_shader shader_copy = zest_NewShader(shader->type);
        zest_SetText(&shader_copy->shader_code, shader->shader_code.str);
        if (zest_TextSize(&ZestRenderer->shader_path_prefix)) {
            zest_SetTextf(&shader_copy->name, "%s%s", ZestRenderer->shader_path_prefix, new_name);
        }
        else {
            zest_SetTextf(&shader_copy->name, "%s", new_name);
        }
        if (zest_vec_size(shader->spv)) {
            zest_vec_resize(shader_copy->spv, zest_vec_size(shader->spv));
            memcpy(shader_copy->spv, shader->spv, zest_vec_size(shader->spv));
        }
        return shader_copy;
	}
    return 0;
}

void zest_FreeShader(zest_shader shader) {
    if (zest_map_valid_name(ZestRenderer->shaders, shader->name.str)) {
        zest_map_remove(ZestRenderer->shaders, shader->name.str);
	}
    zest_FreeText(&shader->name);
    zest_FreeText(&shader->shader_code);
    if (shader->spv) {
        ZEST__FREE(shader->spv);
    }
    ZEST__FREE(shader);
}

zest_uint zest_GetDescriptorSetsForBinding(zest_shader_resources shader_resources, VkDescriptorSet **draw_sets) {
    zest_vec_foreach(set_index, shader_resources->sets[ZEST_FIF]) {
        zest_descriptor_set set = shader_resources->sets[ZEST_FIF][set_index];
        ZEST_CHECK_HANDLE(set);     //Not a valid descriptor set in the shader resource. Make sure all frame in flights are set
        zest_vec_push(*draw_sets, set->vk_descriptor_set);
    }
    return zest_vec_size(*draw_sets);
}

zest_uint zest_GetLayerVertexDescriptorIndex(zest_layer layer, bool last_frame) {
    ZEST_CHECK_HANDLE(layer);       //Not a valid layer handle
    return layer->memory_refs[last_frame ? layer->prev_fif : layer->fif].device_vertex_data->array_index;
}

void zest_ClearShaderResourceDescriptorSets(VkDescriptorSet *draw_sets) {
    if (draw_sets) {
        zest_vec_clear(draw_sets);
    }
}

zest_uint zest_ShaderResourceSetCount(VkDescriptorSet *draw_sets) {
    return draw_sets ? zest_vec_size(draw_sets) : 0;
}

VkCommandPool zest__create_queue_command_pool(int queue_family_index) {
	VkCommandPoolCreateInfo cmd_info_pool = { 0 };
	cmd_info_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmd_info_pool.queueFamilyIndex = queue_family_index;
	cmd_info_pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    VkCommandPool command_pool;
	ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &cmd_info_pool, &ZestDevice->allocation_callbacks, &command_pool));
    return command_pool;
}

zest_recorder zest_CreatePrimaryRecorderWithPool(int queue_family_index) {
    zest_recorder recorder = ZEST__NEW(zest_recorder);
    recorder->magic = zest_INIT_MAGIC;
    recorder->flags = zest_command_buffer_flag_primary;
    for (ZEST_EACH_FIF_i) {
        recorder->outdated[i] = 1;
    }

    recorder->command_pool = zest__create_queue_command_pool(queue_family_index);

    VkCommandBufferAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = ZEST_MAX_FIF;
    alloc_info.commandPool = recorder->command_pool;
    ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, recorder->command_buffer));
    return recorder;
}

zest_recorder zest_CreatePrimaryRecorder() {
    zest_recorder recorder = ZEST__NEW(zest_recorder);
    recorder->magic = zest_INIT_MAGIC;
    recorder->flags = zest_command_buffer_flag_primary;
    recorder->command_pool = 0;
    for (ZEST_EACH_FIF_i) {
        recorder->outdated[i] = 1;
    }
    VkCommandBufferAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = ZEST_MAX_FIF;
    alloc_info.commandPool = ZestDevice->command_pool;
    ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, recorder->command_buffer));
    return recorder;
}

zest_recorder zest_CreateSecondaryRecorder() {
    zest_recorder recorder = ZEST__NEW(zest_recorder);
    recorder->command_pool = 0;
    recorder->magic = zest_INIT_MAGIC;
    recorder->flags = zest_command_buffer_flag_secondary;
    for (ZEST_EACH_FIF_i) {
        recorder->outdated[i] = 1;
    }

    //Each recorder gets it's own command pool for multithreading purposes
    VkCommandPoolCreateInfo cmd_info_pool = { 0 };
    cmd_info_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_info_pool.queueFamilyIndex = ZestDevice->graphics_queue_family_index;
    cmd_info_pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating command pools");
    ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &cmd_info_pool, &ZestDevice->allocation_callbacks, &recorder->command_pool));

    VkCommandBufferAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    alloc_info.commandBufferCount = ZEST_MAX_FIF;
    alloc_info.commandPool = recorder->command_pool;
    ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, recorder->command_buffer));
    return recorder;
}

void zest_FreeCommandBuffers(zest_recorder recorder) {
    if (ZEST__FLAGGED(recorder->flags, zest_command_buffer_flag_primary)) {
        vkFreeCommandBuffers(ZestDevice->logical_device, ZestDevice->command_pool, ZEST_MAX_FIF, recorder->command_buffer);
    } else {
        vkFreeCommandBuffers(ZestDevice->logical_device, recorder->command_pool, ZEST_MAX_FIF, recorder->command_buffer);
    }
    for (ZEST_EACH_FIF_i) {
        recorder->command_buffer[i] = VK_NULL_HANDLE;
    }
}

void zest_FreeRecorder(zest_recorder recorder) {
    if (ZEST__FLAGGED(recorder->flags, zest_command_buffer_flag_primary)) {
        if (recorder->command_buffer) {
            vkFreeCommandBuffers(ZestDevice->logical_device, ZestDevice->command_pool, 2, recorder->command_buffer);
            vkDestroyCommandPool(ZestDevice->logical_device, ZestDevice->command_pool, &ZestDevice->allocation_callbacks);
        } else if (recorder->command_pool) {
            vkDestroyCommandPool(ZestDevice->logical_device, ZestDevice->command_pool, &ZestDevice->allocation_callbacks);
        }
    } else {
        if (recorder->command_buffer) {
            vkFreeCommandBuffers(ZestDevice->logical_device, recorder->command_pool, 2, recorder->command_buffer);
            vkDestroyCommandPool(ZestDevice->logical_device, recorder->command_pool, &ZestDevice->allocation_callbacks);
        } else if (recorder->command_pool) {
            vkDestroyCommandPool(ZestDevice->logical_device, recorder->command_pool, &ZestDevice->allocation_callbacks);
        }
    }
    ZEST__FREE(recorder);
}

zest_pipeline_template zest_BeginPipelineTemplate(const char* name) {
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->pipelines, name));    //That pipeline name already exists!
    zest_pipeline_template_t blank = { 0 };
    zest_pipeline_template pipeline_template = ZEST__NEW(zest_pipeline_template);
    *pipeline_template = blank;
    pipeline_template->magic = zest_INIT_MAGIC;
    zest_SetText(&pipeline_template->name, name);
    pipeline_template->no_vertex_input = ZEST_FALSE;
    pipeline_template->attributeDescriptions = 0;
    pipeline_template->bindingDescriptions = 0;
    pipeline_template->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    zest_SetText(&pipeline_template->fragShaderFunctionName, "main");
    zest_SetText(&pipeline_template->vertShaderFunctionName, "main");
    zest_SetText(&pipeline_template->shader_path_prefix, ZestRenderer->shader_path_prefix.str);
    zest_vec_push(pipeline_template->dynamicStates, VK_DYNAMIC_STATE_VIEWPORT);
    zest_vec_push(pipeline_template->dynamicStates, VK_DYNAMIC_STATE_SCISSOR);
    pipeline_template->scissor.offset.x = 0;
    pipeline_template->scissor.offset.y = 0;
    pipeline_template->scissor.extent = zest_GetSwapChainExtent();
    pipeline_template->colorBlendAttachment = zest_AlphaBlendState();
    zest_map_insert(ZestRenderer->pipelines, name, pipeline_template);
    return pipeline_template;
}

zest_uint zest__get_vk_format_size(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R4G4_UNORM_PACK8:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_A8_UNORM:
        return 1;
        break;
    case VK_FORMAT_A1B5G5R5_UNORM_PACK16:
    case VK_FORMAT_R10X6_UNORM_PACK16:
    case VK_FORMAT_R12X4_UNORM_PACK16:
    case VK_FORMAT_A4R4G4B4_UNORM_PACK16:
    case VK_FORMAT_A4B4G4R4_UNORM_PACK16:
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_SFLOAT:
        return 2;
        break;
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8_SRGB:
        return 3;
        break;
    case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
    case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
    case VK_FORMAT_R16G16_SFIXED5_NV:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
        return 4;
        break;
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_USCALED:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16_SFLOAT:
        return 6;
        break;
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_USCALED:
    case VK_FORMAT_R16G16B16A16_SSCALED:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64_SFLOAT:
        return 8;
        break;
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_SFLOAT:
        return 12;
        break;
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R64G64_UINT:
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R64G64_SFLOAT:
        return 16;
        break;
    case VK_FORMAT_R64G64B64_UINT:
    case VK_FORMAT_R64G64B64_SINT:
    case VK_FORMAT_R64G64B64_SFLOAT:
        return 24;
        break;
    case VK_FORMAT_R64G64B64A64_UINT:
    case VK_FORMAT_R64G64B64A64_SINT:
    case VK_FORMAT_R64G64B64A64_SFLOAT:
        return 32;
        break;
    default:
        ZEST_ASSERT(0); //Unknown format found in shader. You might have to manually add the attributes using zest_AddVertexAttribute
    }
    return 0;
}

int zest__compare_interface_variables(const void *a, const void *b) {
    const SpvReflectInterfaceVariable *var_a = (const SpvReflectInterfaceVariable *)a;
    const SpvReflectInterfaceVariable *var_b = (const SpvReflectInterfaceVariable *)b;
    if (var_a->location < var_b->location) return -1;
    if (var_a->location > var_b->location) return 1;
    return 0;
}

zest_pipeline_template zest_CreateGraphicsPipeline(const char *name, const char *vertex_shader, const char *fragment_shader, bool instanced) {
    zest_pipeline_template pipeline = zest_BeginPipelineTemplate(name);

    zest_text_t vert_path = { 0 };
    zest_text_t frag_path = { 0 };
    if (zest_TextSize(&ZestRenderer->shader_path_prefix)) {
        zest_SetTextf(&vert_path, "%s%s", ZestRenderer->shader_path_prefix, vertex_shader);
        zest_SetTextf(&frag_path, "%s%s", ZestRenderer->shader_path_prefix, fragment_shader);
    } else {
        zest_SetTextf(&vert_path, "%s", name);
        zest_SetTextf(&frag_path, "%s", name);
    }

    zest_shader vert_shader = 0;
    if (zest_map_valid_name(ZestRenderer->shaders, vert_path.str)) {
        vert_shader = *zest_map_at(ZestRenderer->shaders, vert_path.str);
        if (!vert_shader->spv) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Failed to find any spv data at %s", vert_path.str);
            ZEST_ASSERT(0);    //Shader must have been compiled first before building the pipeline
        }
    } else {
        vert_shader = zest_AddShaderFromSPVFile(vert_path.str, shaderc_vertex_shader);
        if (!vert_shader) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "No spv file was found at %s", vert_path.str);
        }
        ZEST_ASSERT(vert_shader);        //Failed to load the shader file, make sure it exists at the location
    }

    zest_shader frag_shader = 0;
    if (zest_map_valid_name(ZestRenderer->shaders, frag_path.str)) {
        frag_shader = *zest_map_at(ZestRenderer->shaders, frag_path.str);
        if (!frag_shader->spv) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Failed to find any spv data at %s", frag_path.str);
            ZEST_ASSERT(0);    //Shader must have been compiled first before building the pipeline
        }
    } else {
        frag_shader = zest_AddShaderFromSPVFile(frag_path.str, shaderc_vertex_shader);
        if (!frag_shader) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "No spv file was found at %s", frag_path.str);
        }
        ZEST_ASSERT(frag_shader);        //Failed to load the shader file, make sure it exists at the location
    }

    zest_SetText(&pipeline->vertShaderFile, "sprite_vert.spv");
    zest_SetText(&pipeline->fragShaderFile, "image_frag.spv");

    //Read the vertex shader and setup any vertex attributes and push constant ranges we find
    SpvReflectShaderModule vert_module;
    spvReflectCreateShaderModule(vert_shader->spv_size, vert_shader->spv, &vert_module);
    SpvReflectShaderModule frag_module;
    spvReflectCreateShaderModule(frag_shader->spv_size, frag_shader->spv, &frag_module);

    // Vertex Inputs
    uint32_t count = 0;
    spvReflectEnumerateInputVariables(&vert_module, &count, NULL);
    if (count) {

        SpvReflectInterfaceVariable **all_inputs = 0;
        zest_vec_resize(all_inputs, count);
        zest_vec *header = zest__vec_header(all_inputs);
        spvReflectEnumerateInputVariables(&vert_module, &count, all_inputs);
        SpvReflectInterfaceVariable *inputs = 0;
        zest_vec_foreach(i, all_inputs) {
            SpvReflectInterfaceVariable *p_var = all_inputs[i];
            if (!(p_var->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)) {
                zest_vec_push(inputs, *p_var);
            }
        }

        zest_vec_free(all_inputs);
        qsort(inputs, zest_vec_size(inputs), sizeof(SpvReflectInterfaceVariable), zest__compare_interface_variables);

        zest_uint current_offset = 0;
        zest_vec_foreach(i, inputs) {
            zest_uint attribute_size = zest__get_vk_format_size(inputs[i].format);
            zest_AddVertexAttribute(pipeline, i, inputs[i].format, current_offset);
            ZEST_PRINT("  - Location %u: %s, Format: %d, Size: %u, Offset: %u", inputs[i].location, inputs[i].name, inputs[i].format, attribute_size, current_offset); 
            current_offset += attribute_size;
        }

        zest_AddVertexInputBindingDescription(pipeline, 0, current_offset, instanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX);
        zest_vec_free(inputs);
    }

	// Push Constants
	count = 0;
    zest_uint push_size = 0;
    zest_supported_shader_stages stage_flags = 0;
	spvReflectEnumeratePushConstantBlocks(&vert_module, &count, NULL);
	if(count) {
        ZEST_ASSERT(count == 1); //Currently only one push constant block is supported. (you can have the block in both the vert and frag shader)
        SpvReflectBlockVariable **vert_push_constants = 0;
        zest_vec_resize(vert_push_constants, count);
        spvReflectEnumeratePushConstantBlocks(&vert_module, &count, vert_push_constants);

		push_size = vert_push_constants[0]->size;
        stage_flags = zest_shader_vertex_stage;
        zest_vec_free(vert_push_constants);
        count = 0;
    }

	spvReflectEnumeratePushConstantBlocks(&frag_module, &count, NULL);
	if(count) {
        ZEST_ASSERT(count == 1); //Currently only one push constant block is supported. (you can have the block in both the vert and frag shader)
        SpvReflectBlockVariable **frag_push_constants = 0;
        zest_vec_resize(frag_push_constants, count);
        spvReflectEnumeratePushConstantBlocks(&frag_module, &count, frag_push_constants);

		ZEST_ASSERT(push_size == frag_push_constants[0]->size);  //Fragment stage must have the same size push constant as currently only one push block is supported
        stage_flags |= zest_shader_fragment_stage;
        zest_vec_free(frag_push_constants);
        count = 0;
    }

    if (push_size) {
        zest_SetPipelinePushConstantRange(pipeline, push_size, stage_flags);
    }
	spvReflectDestroyShaderModule(&vert_module);
	spvReflectDestroyShaderModule(&frag_module);
    zest_FreeText(&vert_path);
    zest_FreeText(&frag_path);
    return pipeline;
}

zest_vertex_input_descriptions zest_NewVertexInputDescriptions() {
    zest_vertex_input_descriptions descriptions = 0;
    return descriptions;
}

void zest_AddVertexAttribute(zest_pipeline_template pipeline_template, zest_uint location, VkFormat format, zest_uint offset) {
    zest_vec_push(pipeline_template->attributeDescriptions, zest_CreateVertexInputDescription(0, location, format, offset));
}

zest_key zest_Hash(const void* input, zest_ull length, zest_ull seed) { 
    zest_hasher_t hasher;
    zest__hash_initialise(&hasher, seed); 
    zest__hasher_add(&hasher, input, length); 
    return (zest_key)zest__get_hash(&hasher); 
}
VkFramebuffer zest_GetRendererFrameBuffer(zest_command_queue_draw_commands item) { 
    return ZestRenderer->swapchain_frame_buffers[ZestRenderer->current_image_frame]; 
}
VkDescriptorSetLayout *zest_GetDescriptorSetLayoutVK(const char* name) { 
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->descriptor_layouts, name));   //Must be a valid descriptor set layout, check the name is correct
    return zest_map_at(ZestRenderer->descriptor_layouts, name)->vk_layout; 
}

zest_set_layout zest_GetDescriptorSetLayout(const char* name) { 
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->descriptor_layouts, name));   //Must be a valid descriptor set layout, check the name is correct
    return *zest_map_at(ZestRenderer->descriptor_layouts, name); 
}

zest_pipeline_template zest_PipelineTemplate(const char *name) {
    zest_key pipeline_key = zest_Hash(name, strlen(name), ZEST_HASH_SEED);
    ZEST_ASSERT(zest_map_valid_key(ZestRenderer->pipelines, pipeline_key)); 
    return *zest_map_at_key(ZestRenderer->pipelines, pipeline_key);
}

zest_pipeline zest_PipelineWithTemplate(zest_pipeline_template template, VkRenderPass render_pass) {
    ZEST_CHECK_HANDLE(template);        //Not a valid pipeline template!
    if (zest_vec_size(template->descriptorSetLayouts) == 0) {
        ZEST_PRINT("ERROR: You're trying to build a pipeline (%s) that has no descriptor set layouts configured. You can add descriptor layouts when building the pipeline with zest_AddPipelineTemplateDescriptorLayout.", template->name.str);
        return NULL;
    }
    zest_key pipeline_key = zest_Hash(template->name.str, zest_TextLength(&template->name), ZEST_HASH_SEED);
    struct {
        zest_key pipeline_key;
        VkRenderPass render_pass;
    } cached_pipeline;
    cached_pipeline.pipeline_key = pipeline_key;
    cached_pipeline.render_pass = render_pass;
    zest_key cached_pipeline_key = zest_Hash(&cached_pipeline, sizeof(cached_pipeline), ZEST_HASH_SEED);
    if (zest_map_valid_key(ZestRenderer->cached_pipelines, cached_pipeline_key)) {
		return *zest_map_at_key(ZestRenderer->cached_pipelines, cached_pipeline_key); 
    }
    return zest__cache_pipeline(*zest_map_at_key(ZestRenderer->pipelines, pipeline_key), render_pass);
}

zest_pipeline zest_Pipeline(const char* name, VkRenderPass render_pass) { 
    zest_key pipeline_key = zest_Hash(name, strlen(name), ZEST_HASH_SEED);
    ZEST_ASSERT(zest_map_valid_key(ZestRenderer->pipelines, pipeline_key)); 
    struct {
        zest_key pipeline_key;
        VkRenderPass render_pass;
    } cached_pipeline;
    cached_pipeline.pipeline_key = pipeline_key;
    cached_pipeline.render_pass = render_pass;
    zest_key cached_pipeline_key = zest_Hash(&cached_pipeline, sizeof(cached_pipeline), ZEST_HASH_SEED);
    if (zest_map_valid_key(ZestRenderer->cached_pipelines, cached_pipeline_key)) {
		return *zest_map_at_key(ZestRenderer->cached_pipelines, cached_pipeline_key); 
    }
    return zest__cache_pipeline(*zest_map_at_key(ZestRenderer->pipelines, pipeline_key), render_pass);
}
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
bool zest_MouseDown(zest_mouse_button button) { return (button & ZestApp->mouse_button) > 0; }
bool zest_MouseHit(zest_mouse_button button) { return (button & ZestApp->mouse_hit) > 0; }
float zest_DPIScale(void) { return ZestRenderer->dpi_scale; }
void zest_SetDPIScale(float scale) { ZestRenderer->dpi_scale = scale; }
zest_uint zest_FPS() { return ZestApp->last_fps; }
float zest_FPSf() { return (float)ZestApp->last_fps; }
zest_window zest_AllocateWindow() { zest_window window; window = ZEST__NEW(zest_window); memset(window, 0, sizeof(zest_window_t)); window->magic = zest_INIT_MAGIC; return window; }
void zest_WaitForIdleDevice() { vkDeviceWaitIdle(ZestDevice->logical_device); }
void zest_MaybeQuit(zest_bool condition) { ZestApp->flags |= condition != 0 ? zest_app_flag_quit_application : 0; }
void zest__hash_initialise(zest_hasher_t* hasher, zest_ull seed) { hasher->state[0] = seed + zest__PRIME1 + zest__PRIME2; hasher->state[1] = seed + zest__PRIME2; hasher->state[2] = seed; hasher->state[3] = seed - zest__PRIME1; hasher->buffer_size = 0; hasher->total_length = 0; }
void zest_GetMouseSpeed(double* x, double* y) {
    double ellapsed_in_seconds = (double)ZestApp->current_elapsed / ZEST_MICROSECS_SECOND;
    *x = ZestApp->mouse_delta_x * ellapsed_in_seconds;
    *y = ZestApp->mouse_delta_y * ellapsed_in_seconds;
}

VkDescriptorBufferInfo* zest_GetUniformBufferInfo(zest_uniform_buffer buffer) {
    ZEST_CHECK_HANDLE(buffer);  //Not a valid uniform buffer
    return &buffer->descriptor_info[ZEST_FIF];
}

VkDescriptorSetLayout zest_vk_GetUniformBufferLayout(zest_uniform_buffer buffer) {
    ZEST_CHECK_HANDLE(buffer);  //Not a valid uniform buffer
    ZEST_CHECK_HANDLE(buffer->set_layout); //The buffer is not properly initialised, no descriptor set found
    return buffer->set_layout->vk_layout;
}

zest_set_layout zest_GetUniformBufferLayout(zest_uniform_buffer buffer) {
    ZEST_CHECK_HANDLE(buffer);  //Not a valid uniform buffer
    return buffer->set_layout;
}

VkDescriptorSet zest_vk_GetUniformBufferSet(zest_uniform_buffer buffer) {
    ZEST_CHECK_HANDLE(buffer);  //Not a valid uniform buffer
    ZEST_CHECK_HANDLE(buffer->descriptor_set[ZEST_FIF]);  //The buffer is not properly initialised, no descriptor set found
    return buffer->descriptor_set[ZEST_FIF]->vk_descriptor_set;
}

zest_descriptor_set zest_GetUniformBufferSet(zest_uniform_buffer buffer) {
    ZEST_CHECK_HANDLE(buffer);  //Not a valid uniform buffer
    ZEST_CHECK_HANDLE(buffer->descriptor_set[ZEST_FIF]);  //The buffer is not properly initialised, no descriptor set found
    return buffer->descriptor_set[ZEST_FIF];
}

zest_descriptor_set zest_GetFIFUniformBufferSet(zest_uniform_buffer buffer, zest_uint fif) {
    ZEST_ASSERT(fif < ZEST_MAX_FIF);
    ZEST_CHECK_HANDLE(buffer);  //Not a valid uniform buffer
    ZEST_CHECK_HANDLE(buffer->descriptor_set[fif]);  //The buffer is not properly initialised, no descriptor set found
    return buffer->descriptor_set[fif];
}

VkDescriptorSet zest_vk_GetFIFUniformBufferSet(zest_uniform_buffer buffer, zest_uint fif) {
    ZEST_ASSERT(fif < ZEST_MAX_FIF);
    ZEST_CHECK_HANDLE(buffer);  //Not a valid uniform buffer
    ZEST_CHECK_HANDLE(buffer->descriptor_set[fif]);  //The buffer is not properly initialised, no descriptor set found
    return buffer->descriptor_set[fif]->vk_descriptor_set;
}

void* zest_GetUniformBufferData(zest_uniform_buffer uniform_buffer) {
    return uniform_buffer->buffer[ZEST_FIF]->data;
}

zest_descriptor_set zest_GetDefaultUniformBufferSet() {
    return ZestRenderer->uniform_buffer->descriptor_set[ZEST_FIF];
}

VkDescriptorSet zest_vk_GetDefaultUniformBufferSet() {
    return ZestRenderer->uniform_buffer->descriptor_set[ZEST_FIF]->vk_descriptor_set;
}

zest_set_layout zest_GetDefaultUniformBufferLayout() {
    return ZestRenderer->uniform_buffer->set_layout;
}

VkDescriptorSetLayout zest_vk_GetDefaultUniformBufferLayout() {
    return ZestRenderer->uniform_buffer->set_layout->vk_layout;
}

zest_uniform_buffer zest_GetDefaultUniformBuffer() {
    return ZestRenderer->uniform_buffer;
}

void zest_BindVertexBuffer(VkCommandBuffer command_buffer, zest_buffer buffer) {
    VkDeviceSize offsets[] = { buffer->memory_offset };
    vkCmdBindVertexBuffers(command_buffer, 0, 1, zest_GetBufferDeviceBuffer(buffer), offsets);
}

void zest_BindIndexBuffer(VkCommandBuffer command_buffer, zest_buffer buffer) {
    vkCmdBindIndexBuffer(command_buffer, *zest_GetBufferDeviceBuffer(buffer), buffer->memory_offset, VK_INDEX_TYPE_UINT32);
}

zest_uint zest_GetBufferDescriptorIndex(zest_buffer buffer) {
    ZEST_ASSERT(buffer);  //Not a valid buffer handle
    return buffer->array_index;
}

void zest_SendPushConstants(VkCommandBuffer command_buffer, zest_pipeline pipeline, void *data) {
    vkCmdPushConstants(command_buffer, pipeline->pipeline_layout, pipeline->pipeline_template->pushConstantRange.stageFlags, pipeline->pipeline_template->pushConstantRange.offset, pipeline->pipeline_template->pushConstantRange.size, data);
}

void zest_SendComputePushConstants(VkCommandBuffer command_buffer, zest_compute compute) {
    vkCmdPushConstants(command_buffer, compute->pipeline_layout, compute->pushConstantRange.stageFlags, 0, compute->pushConstantRange.size, &compute->push_constants);
}

void zest_SendCustomComputePushConstants(VkCommandBuffer command_buffer, zest_compute compute, const void *push_constant) {
    vkCmdPushConstants(command_buffer, compute->pipeline_layout, compute->pushConstantRange.stageFlags, 0, compute->pushConstantRange.size, push_constant);
}

void zest_SendStandardPushConstants(VkCommandBuffer command_buffer, zest_pipeline_t* pipeline, void* data) {
    vkCmdPushConstants(command_buffer, pipeline->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(zest_push_constants_t), data);
}

void zest_Draw(VkCommandBuffer command_buffer, zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance) {
    vkCmdDraw(command_buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void zest_DrawLayerInstruction(VkCommandBuffer command_buffer, zest_uint vertex_count, zest_layer_instruction_t *instruction) {
    vkCmdDraw(command_buffer, vertex_count, instruction->total_indexes, 0, instruction->start_index);
}

void zest_DrawIndexed(VkCommandBuffer command_buffer, zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance) {
    vkCmdDrawIndexed(command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void zest_DispatchCompute(VkCommandBuffer command_buffer, zest_compute compute, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z) {
    vkCmdDispatch(command_buffer, group_count_x, group_count_y, group_count_z);
}

void zest_ResetCompute(zest_compute compute) {
    ZEST_CHECK_HANDLE(compute);	//Not a valid handle!
    compute->compute_commands->last_fif = compute->fif;
    compute->fif = (compute->fif + 1) % ZEST_MAX_FIF;
    for (ZEST_EACH_FIF_i) {
        compute->recorder->outdated[i] = 1;
    }
}

int zest_ComputeConditionAlwaysTrue(zest_compute layer) {
    return 1;
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

    for (zest_uint i = 0; i < memoryProperties.memoryTypeCount; ++i) {
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

float zest_LinearToSRGB(float value) {
    return powf(value, 2.2f);
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

void *zest__vec_linear_reserve(zloc_linear_allocator_t *allocator, void *T, zest_uint unit_size, zest_uint new_capacity) {
    if (T && new_capacity <= zest__vec_header(T)->capacity)
        return T;
    void* new_data = zloc_LinearAllocation(allocator, new_capacity * unit_size + zest__VEC_HEADER_OVERHEAD);
    if (!T) memset(new_data, 0, zest__VEC_HEADER_OVERHEAD);
    T = ((char*)new_data + zest__VEC_HEADER_OVERHEAD);
    ((zest_vec*)(new_data))->capacity = new_capacity;
    return T;
}

void zest_SetText(zest_text_t* buffer, const char* text) {
    if (!text) {
        zest_FreeText(buffer);
        return;
    }
    zest_uint length = (zest_uint)strlen(text) + 1;
    zest_vec_resize(buffer->str, length);
    zest_strcpy(buffer->str, length, text);
}

void zest_ResizeText(zest_text_t *buffer, zest_uint size) {
    zest_vec_resize(buffer->str, size);
}

void zest_SetTextfv(zest_text_t* buffer, const char* text, va_list args)
{
    va_list args2;
    va_copy(args2, args);

#ifdef _MSC_VER
    int len = vsnprintf(NULL, 0, text, args);
    ZEST_ASSERT(len >= 0);

    if ((int)zest_TextSize(buffer) < len + 1)
    {
        zest_vec_resize(buffer->str, (zest_uint)(len + 1));
    }
    vsnprintf(buffer->str, len + 1, text, args2);
#else
    int len = vsnprintf(buffer->str ? buffer->str : NULL, (size_t)zest_TextSize(buffer), text, args);
    ZEST_ASSERT(len >= 0);

    if (zest_TextSize(buffer) < len + 1)
    {
        zest_vec_resize(buffer->str, len + 1);
        vsnprintf(buffer->str, (size_t)len + 1, text, args2);
    }
#endif

    ZEST_ASSERT(buffer->str);
}

void zest_AppendTextf(zest_text_t *buffer, const char *format, ...) {
    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);

    int len = vsnprintf(NULL, 0, format, args);
    ZEST_ASSERT(len >= 0);
    if (len <= 0)
    {
        va_end(args_copy);
        return;
    }

    if (zest_TextSize(buffer)) {
        if (zest_vec_back(buffer->str) == '\0') {
            zest_vec_pop(buffer->str);
        }
    }

    const zest_uint write_off = zest_TextSize(buffer);
    const zest_uint needed_sz = write_off + len + 1;
	zest_vec_resize(buffer->str, needed_sz);
    vsnprintf(buffer->str + write_off, len + 1, format, args_copy);
    va_end(args_copy);

    va_end(args);
}

void zest_SetTextf(zest_text_t* buffer, const char* text, ...) {
    va_list args;
    va_start(args, text);
    zest_SetTextfv(buffer, text, args);
    va_end(args);
}

void zest_FreeText(zest_text_t* buffer) {
    zest_vec_free(buffer->str);
    buffer->str = ZEST_NULL;
}

zest_uint zest_TextLength(zest_text_t* buffer) {
    return (zest_uint)strlen(buffer->str);
}

zest_uint zest_TextSize(zest_text_t* buffer) {
    return zest_vec_size(buffer->str);
}
//End api functions

void zest__log_entry_v(char *str, const char *text, va_list args)
{
    va_list args2;
    va_copy(args2, args);

#ifdef _MSC_VER
    int len = vsnprintf(NULL, 0, text, args);
    ZEST_ASSERT(len >= 0 && len + 1 < ZEST_LOG_ENTRY_SIZE); //log entry must be within buffer limit

    vsnprintf(str, len + 1, text, args2);
#else
    int len = vsnprintf(str ? str : NULL, (size_t)ZEST_LOG_ENTRY_SIZE, text, args);
    ZEST_ASSERT(len >= 0 && len + 1 < ZEST_LOG_ENTRY_SIZE); //log entry must be within buffer limit

	vsnprintf(buffer->str, (size_t)len + 1, text, args2);
#endif

    ZEST_ASSERT(str);
}

void zest__log_entry(const char *text, ...) {
    va_list args;
    va_start(args, text);
    zest_log_entry_t entry = { 0 };
    entry.time = zest_Microsecs();
    zest__log_entry_v(entry.str, text, args);
    zest_vec_push(ZestRenderer->debug.frame_log, entry);
    va_end(args);
}

void zest__reset_frame_log(char *str, const char *entry, ...) {
    zest_vec_clear(ZestRenderer->debug.frame_log);
    ZestRenderer->debug.function_depth = 0;
}

void zest__add_line(zest_text_t *text, char current_char, zest_uint *position, zest_uint tabs) {
    ZEST_ASSERT(*position < zest_TextSize(text));
    text->str[(*position)++] = current_char;
    ZEST_ASSERT(*position < zest_TextSize(text));
    text->str[(*position)++] = '\n';
    for (int t = 0; t != tabs; ++t) {
        text->str[(*position)++] = '\t';
    }
}

void zest__format_shader_code(zest_text_t *code) {
    zest_uint length = zest_TextSize(code);
    zest_uint pos = 0;
    zest_text_t formatted_code = { 0 };
    //First pass to calculate the new formatted buffer size
    zest_uint extra_size = 0;
    int tabs = 0;
    for (zest_uint i = 0; i != length; ++i) {
        if (code->str[i] == ';') {
            extra_size++;
            extra_size += tabs;
        }
        else if (code->str[i] == '{') {
            extra_size++;
            extra_size += tabs;
            tabs++;
        }
        else if (code->str[i] == '}') {
            extra_size++;
            extra_size += tabs;
            tabs--;
        }
    }
    pos = 0;
    zest_ResizeText(&formatted_code, length + extra_size);
    zest_uint new_length = zest_TextSize(&formatted_code);
    zest_uint f_pos = 0;
    bool new_line_added = false;
    for (zest_uint i = 0; i != length; ++i) {
        if (new_line_added) {
            if (code->str[i] == ' ') {
                continue;
            }
            else {
                new_line_added = false;
            }
        }
        if (code->str[i] == ';') {
            zest__add_line(&formatted_code, code->str[i], &f_pos, tabs);
            new_line_added = true;
        }
        else if (code->str[i] == '{') {
            tabs++;
            zest__add_line(&formatted_code, code->str[i], &f_pos, tabs);
            new_line_added = true;
        }
        else if (code->str[i] == '}') {
            if (tabs > 0) {
                f_pos--;
                tabs--;
            }
            zest__add_line(&formatted_code, code->str[i], &f_pos, tabs);
            new_line_added = true;
        }
        else {
            ZEST_ASSERT(f_pos < new_length);
            formatted_code.str[f_pos++] = code->str[i];
        }
    }
    zest_FreeText(code);
    *code = formatted_code;
}

void zest__compile_builtin_shaders(zest_bool compile_shaders) {
    shaderc_compiler_t compiler = { 0 }; 
    if (!compile_shaders) {
        compiler = shaderc_compiler_initialize();
		ZEST_ASSERT(compiler); //unable to create compiler
    }
    zest_CreateShader(zest_shader_imgui_vert, shaderc_vertex_shader, "imgui_vert.spv", 1, 0, compiler, 0);
    zest_CreateShader(zest_shader_imgui_frag, shaderc_fragment_shader, "imgui_frag.spv", 1, 0, compiler, 0);
    zest_CreateShader(zest_shader_sprite_frag, shaderc_fragment_shader, "image_frag.spv", 1, 0, compiler, 0);
    zest_CreateShader(zest_shader_sprite_alpha_frag, shaderc_fragment_shader, "sprite_alpha_frag.spv", 1, 0, compiler, 0);
    zest_CreateShader(zest_shader_sprite_vert, shaderc_vertex_shader, "sprite_vert.spv", 1, 0, compiler, 0);
    zest_CreateShader(zest_shader_shape_vert, shaderc_vertex_shader, "shape_vert.spv", 1, 0, compiler, 0);
    zest_CreateShader(zest_shader_shape_frag, shaderc_fragment_shader, "shape_frag.spv", 1, 0, compiler, 0);
    zest_CreateShader(zest_shader_3d_lines_vert, shaderc_vertex_shader, "3d_lines_vert.spv", 1, 0, compiler, 0);
    zest_CreateShader(zest_shader_3d_lines_frag, shaderc_fragment_shader, "3d_lines_frag.spv", 1, 0, compiler, 0);
    zest_CreateShader(zest_shader_font_frag, shaderc_fragment_shader, "font_frag.spv", 1, 0, compiler, 0);
    zest_CreateShader(zest_shader_mesh_vert, shaderc_vertex_shader, "mesh_vert.spv", 1, 0, compiler, 0);
    zest_CreateShader(zest_shader_mesh_instance_vert, shaderc_vertex_shader, "mesh_instance_vert.spv", 1, 0, compiler, 0);
    zest_CreateShader(zest_shader_mesh_instance_frag, shaderc_fragment_shader, "mesh_instance_frag.spv", 1, 0, compiler, 0);
    zest_CreateShader(zest_shader_swap_vert, shaderc_vertex_shader, "swap_vert.spv", 1, 0, compiler, 0);
    zest_CreateShader(zest_shader_swap_frag, shaderc_fragment_shader, "swap_frag.spv", 1, 0, compiler, 0);
    if (compiler) {
        shaderc_compiler_release(compiler);
    }
}

zest_sampler zest_GetSampler(VkSamplerCreateInfo *info) {
    zest_key key = zest_Hash(info, sizeof(VkSamplerCreateInfo), ZEST_HASH_SEED);
    if (zest_map_valid_key(ZestRenderer->samplers, key)) {
        return *zest_map_at_key(ZestRenderer->samplers, key);
    }

    zest_sampler sampler = ZEST__NEW(zest_sampler);
    sampler->magic = zest_INIT_MAGIC;
    sampler->create_info = *info;
    ZEST_VK_CHECK_RESULT(vkCreateSampler(ZestDevice->logical_device, info, &ZestDevice->allocation_callbacks, &sampler->vk_sampler));

    zest_map_insert_key(ZestRenderer->samplers, key, sampler);

    return sampler;
}

void zest__create_debug_layout_and_pool(zest_uint max_texture_count) {
	zest_set_layout_builder_t builder = zest_BeginSetLayoutBuilder();
	zest_AddLayoutBuilderCombinedImageSampler(&builder, 0, 1);
	ZestRenderer->texture_debug_layout = zest_FinishDescriptorSetLayout(&builder, "Texture Debug Layout");
	zest_CreateDescriptorPoolForLayout(ZestRenderer->texture_debug_layout, max_texture_count, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
}

void zest__prepare_standard_pipelines() {

    //2d sprite rendering
    zest_pipeline_template sprite_instance_pipeline = zest_BeginPipelineTemplate("pipeline_2d_sprites");
    zest_AddVertexInputBindingDescription(sprite_instance_pipeline, 0, sizeof(zest_sprite_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);

    zest_AddVertexAttribute(sprite_instance_pipeline, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_sprite_instance_t, uv));                  // Location 0: UV coords
    zest_AddVertexAttribute(sprite_instance_pipeline, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_sprite_instance_t, position_rotation));   // Location 1: Instance Position and rotation
    zest_AddVertexAttribute(sprite_instance_pipeline, 2, VK_FORMAT_R16G16B16A16_SSCALED, offsetof(zest_sprite_instance_t, size_handle));        // Location 2: Size of the sprite in pixels
    zest_AddVertexAttribute(sprite_instance_pipeline, 3, VK_FORMAT_R16G16_SNORM, offsetof(zest_sprite_instance_t, alignment));                  // Location 3: Alignment
    zest_AddVertexAttribute(sprite_instance_pipeline, 4, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_sprite_instance_t, color));                    // Location 4: Instance Color
    zest_AddVertexAttribute(sprite_instance_pipeline, 5, VK_FORMAT_R32_UINT, offsetof(zest_sprite_instance_t, intensity_texture_array));        // Location 5: Instance Parameters

    zest_SetPipelineShaders(sprite_instance_pipeline, "sprite_vert.spv", "image_frag.spv", ZestRenderer->shader_path_prefix.str);
    zest_SetPipelinePushConstantRange(sprite_instance_pipeline, sizeof(zest_push_constants_t), VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
	zest_AddPipelineDescriptorLayout(sprite_instance_pipeline, zest_vk_GetUniformBufferLayout(ZestRenderer->uniform_buffer));
	zest_AddPipelineDescriptorLayout(sprite_instance_pipeline, zest_vk_GetGlobalBindlessLayout());
    zest_SetPipelineBlend(sprite_instance_pipeline, zest_PreMultiplyBlendState());
    zest_SetPipelineDepthTest(sprite_instance_pipeline, false, true);
    zest_EndPipelineTemplate(sprite_instance_pipeline);
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "2d sprites pipeline");

    //Sprites with 1 channel textures
    zest_pipeline_template sprite_instance_pipeline_alpha = zest_CopyPipelineTemplate("pipeline_2d_sprites_alpha", sprite_instance_pipeline);
    zest_SetPipelineShaders(sprite_instance_pipeline_alpha, "sprite_vert.spv", "sprite_alpha_frag.spv", ZestRenderer->shader_path_prefix.str);
    zest_SetText(&sprite_instance_pipeline_alpha->vertShaderFile, "sprite_vert.spv");
    zest_SetText(&sprite_instance_pipeline_alpha->fragShaderFile, "sprite_alpha_frag.spv");
    zest_SetPipelineDepthTest(sprite_instance_pipeline_alpha, false, true);
    zest_EndPipelineTemplate(sprite_instance_pipeline_alpha);
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "2d sprites alpha pipeline");

    //SDF lines 3d
    /*
    zest_pipeline_template line3d_instance_pipeline = zest_CopyPipelineTemplate("pipeline_line3d_instance", line_instance_pipeline);
    zest_ClearVertexInputBindingDescriptions(line3d_instance_pipeline);
    zest_AddVertexInputBindingDescription(line3d_instance_pipeline, 0, sizeof(zest_line_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);

    VkVertexInputAttributeDescription* line3d_instance_vertex_input_attributes = 0;

    zest_vec_push(line3d_instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_line_instance_t, start))); 		// Location 0: Start Position
    zest_vec_push(line3d_instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_line_instance_t, end)));		    // Location 1: End Position
    zest_vec_push(line3d_instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_line_instance_t, start_color)));		// Location 2: Start Color

    line3d_instance_pipeline->attributeDescriptions = line3d_instance_vertex_input_attributes;
    zest_SetText(&line3d_instance_pipeline->vertShaderFile, "3d_lines_vert.spv");
    zest_SetText(&line3d_instance_pipeline->fragShaderFile, "3d_lines_frag.spv");
    zest_EndPipelineTemplate(line3d_instance_pipeline);
    line3d_instance_pipeline->colorBlendAttachment = zest_PreMultiplyBlendState();
    line3d_instance_pipeline->depthStencil.depthWriteEnable = VK_FALSE;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "SDF 3D Lines pipeline");
    */

    //Font Texture
    zest_pipeline_template font_pipeline = zest_CopyPipelineTemplate("pipeline_fonts", sprite_instance_pipeline);
    zest_SetPipelinePushConstantRange(font_pipeline, sizeof(zest_push_constants_t), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    zest_SetText(&font_pipeline->vertShaderFile, "sprite_vert.spv");
    zest_SetText(&font_pipeline->fragShaderFile, "font_frag.spv");
    zest_AddPipelineDescriptorLayout(font_pipeline, ZestRenderer->uniform_buffer->set_layout->vk_layout);
    zest_AddPipelineDescriptorLayout(font_pipeline, ZestRenderer->global_bindless_set_layout->vk_layout);
    zest_EndPipelineTemplate(font_pipeline);
    font_pipeline->depthStencil.depthWriteEnable = VK_FALSE;
    font_pipeline->depthStencil.depthTestEnable = VK_FALSE;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Font pipeline");

    //General mesh drawing
    zest_pipeline_template mesh_pipeline = zest_BeginPipelineTemplate("pipeline_mesh");
    mesh_pipeline->scissor.offset.x = 0;
    mesh_pipeline->scissor.offset.y = 0;
    zest_SetPipelinePushConstantRange(mesh_pipeline, sizeof(zest_push_constants_t), VK_SHADER_STAGE_VERTEX_BIT);
    zest_AddVertexInputBindingDescription(mesh_pipeline, 0, sizeof(zest_textured_vertex_t), VK_VERTEX_INPUT_RATE_VERTEX);
    VkVertexInputAttributeDescription* zest_vertex_input_attributes = 0;
    zest_vec_push(zest_vertex_input_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(zest_textured_vertex_t, pos)));        // Location 0: Position
    zest_vec_push(zest_vertex_input_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32_SFLOAT, offsetof(zest_textured_vertex_t, intensity)));        // Location 1: Alpha/Intensity
    zest_vec_push(zest_vertex_input_attributes, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(zest_textured_vertex_t, uv)));            // Location 2: Position
    zest_vec_push(zest_vertex_input_attributes, zest_CreateVertexInputDescription(0, 3, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_textured_vertex_t, color)));        // Location 3: Color
    zest_vec_push(zest_vertex_input_attributes, zest_CreateVertexInputDescription(0, 4, VK_FORMAT_R32_UINT, offsetof(zest_textured_vertex_t, parameters)));        // Location 4: Parameters

    mesh_pipeline->attributeDescriptions = zest_vertex_input_attributes;
    zest_SetText(&mesh_pipeline->vertShaderFile, "mesh_vert.spv");
    zest_SetText(&mesh_pipeline->fragShaderFile, "image_frag.spv");

    mesh_pipeline->scissor.extent = zest_GetSwapChainExtent();
    mesh_pipeline->flags |= zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild;
    zest_EndPipelineTemplate(mesh_pipeline);

    mesh_pipeline->colorBlendAttachment = zest_PreMultiplyBlendState();
    mesh_pipeline->depthStencil.depthTestEnable = VK_TRUE;
    mesh_pipeline->depthStencil.depthWriteEnable = VK_TRUE;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Mesh pipeline");

    //Instanced mesh drawing for drawing primatives
    zest_pipeline_template imesh_pipeline = zest_BeginPipelineTemplate("pipeline_mesh_instance");
    imesh_pipeline->scissor.offset.x = 0;
    imesh_pipeline->scissor.offset.y = 0;
    zest_SetPipelinePushConstantRange(imesh_pipeline, sizeof(zest_push_constants_t), VK_SHADER_STAGE_VERTEX_BIT);
    zest_AddVertexInputBindingDescription(imesh_pipeline, 0, sizeof(zest_vertex_t), VK_VERTEX_INPUT_RATE_VERTEX);
    zest_AddVertexInputBindingDescription(imesh_pipeline, 1, sizeof(zest_mesh_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);
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

    imesh_pipeline->attributeDescriptions = imesh_input_attributes;
    zest_SetText(&imesh_pipeline->vertShaderFile, "mesh_instance_vert.spv");
    zest_SetText(&imesh_pipeline->fragShaderFile, "mesh_instance_frag.spv");

    imesh_pipeline->scissor.extent = zest_GetSwapChainExtent();
    imesh_pipeline->flags |= zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild;
    zest_ClearPipelineDescriptorLayouts(imesh_pipeline);
    zest_EndPipelineTemplate(imesh_pipeline);

    imesh_pipeline->rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    imesh_pipeline->rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    imesh_pipeline->rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    imesh_pipeline->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    imesh_pipeline->colorBlendAttachment = zest_AlphaBlendState();
    imesh_pipeline->depthStencil.depthTestEnable = VK_TRUE;
    imesh_pipeline->depthStencil.depthWriteEnable = VK_TRUE;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Instance Mesh pipeline");

    //Final Render Pipelines
    zest_pipeline_template final_render = zest_BeginPipelineTemplate("pipeline_swap_chain");
    final_render->scissor.extent = zest_GetSwapChainExtent();
    zest_SetPipelinePushConstantRange(final_render, sizeof(zest_render_target_push_constants_t), VK_SHADER_STAGE_VERTEX_BIT);
    final_render->no_vertex_input = ZEST_TRUE;
    zest_SetText(&final_render->vertShaderFile, "swap_vert.spv");
    zest_SetText(&final_render->fragShaderFile, "swap_frag.spv");
    final_render->uniforms = 0;
    final_render->flags = zest_pipeline_set_flag_is_render_target_pipeline;

    zest_ClearPipelineDescriptorLayouts(final_render);
    zest_EndPipelineTemplate(final_render);
    final_render->depthStencil.depthWriteEnable = VK_FALSE;
    final_render->depthStencil.depthTestEnable = VK_FALSE;

    final_render->colorBlendAttachment = zest_PreMultiplyBlendStateForSwap();

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Final render pipeline");
}

void zest__update_pipeline_template(zest_pipeline_template pipeline_template) {
    pipeline_template->viewport.width = (float)pipeline_template->scissor.extent.width;
    pipeline_template->viewport.height = (float)pipeline_template->scissor.extent.height;

    if (!pipeline_template->no_vertex_input) {
        ZEST_ASSERT(zest_vec_size(pipeline_template->bindingDescriptions));    //If the pipeline is set to have vertex input, then you must add bindingDescriptions. You can use zest_AddVertexInputBindingDescription for this
        pipeline_template->vertexInputInfo.vertexBindingDescriptionCount = zest_vec_size(pipeline_template->bindingDescriptions);
        pipeline_template->vertexInputInfo.pVertexBindingDescriptions = pipeline_template->bindingDescriptions;
        pipeline_template->vertexInputInfo.pVertexAttributeDescriptions = pipeline_template->attributeDescriptions;
    }
    pipeline_template->viewportState.pViewports = &pipeline_template->viewport;
    pipeline_template->viewportState.pScissors = &pipeline_template->scissor;
    pipeline_template->colorBlending.pAttachments = &pipeline_template->colorBlendAttachment;
    pipeline_template->pipelineLayoutInfo.setLayoutCount = zest_vec_size(pipeline_template->descriptorSetLayouts);
    pipeline_template->pipelineLayoutInfo.pSetLayouts = pipeline_template->descriptorSetLayouts;
    pipeline_template->dynamicState.pDynamicStates = pipeline_template->dynamicStates;

    if (pipeline_template->pushConstantRange.size) {
        pipeline_template->pipelineLayoutInfo.pPushConstantRanges = &pipeline_template->pushConstantRange;
        pipeline_template->pipelineLayoutInfo.pushConstantRangeCount = 1;
    }
}

void zest__create_empty_command_queue(zest_command_queue command_queue) {
    command_queue->recorder = zest_CreatePrimaryRecorder();

    for (ZEST_EACH_FIF_i) {
        zest_vec_push(command_queue->fif_wait_stage_flags[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }
    zest_command_queue_draw_commands_t blank_draw_commands = { 0 };
    zest_command_queue_draw_commands draw_commands = ZEST__NEW_ALIGNED(zest_command_queue_draw_commands, 16);
    memcpy(draw_commands, &blank_draw_commands, sizeof(zest_command_queue_draw_commands_t));
    //*draw_commands = blank_draw_commands;
    zest_bool is_aligned = zest__is_aligned(draw_commands, 16);
    draw_commands->magic = zest_INIT_MAGIC;
    draw_commands->render_pass = ZestRenderer->final_render_pass;
    draw_commands->get_frame_buffer = zest_GetRendererFrameBuffer;
    draw_commands->render_pass_function = zest__render_blank;
    draw_commands->viewport.extent = zest_GetSwapChainExtent();
    draw_commands->viewport.offset.x = draw_commands->viewport.offset.y = 0;
    draw_commands->viewport_scale.x = 1.f;
    draw_commands->viewport_scale.y = 1.f;
    zest_map_insert(ZestRenderer->command_queue_draw_commands, "Blank Screen", draw_commands);
    zest_vec_push(command_queue->draw_commands, draw_commands);
}

void zest__render_blank(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer) {
    VkRenderPassBeginInfo render_pass_info = { 0 };
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass;
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

void zest__add_draw_routine(zest_command_queue_draw_commands draw_commands, zest_draw_routine draw_routine) {
    zest_vec_push(draw_commands->draw_routines, draw_routine);
}

void zest__draw_renderer_frame() {

    ZestRenderer->flags |= zest_renderer_flag_drawing_loop_running;
    ZestRenderer->flags &= ~zest_renderer_flag_swapchain_was_recreated;

    if (!zest_AcquireSwapChainImage()) {
        return;
    }

    if (!ZestRenderer->active_command_queue) {
        //if there's no render queues at all, then we can draw this blank one to prevent errors when presenting the frame
        zest_StartCommandQueue(&ZestRenderer->empty_queue, ZEST_FIF);
        zest_RecordCommandQueue(&ZestRenderer->empty_queue, ZEST_FIF);
        zest_EndCommandQueue(&ZestRenderer->empty_queue, ZEST_FIF);
        zest_SubmitCommandQueue(&ZestRenderer->empty_queue, ZestRenderer->fif_fence[ZEST_FIF]);
    }
    else {
        zest_StartCommandQueue(ZestRenderer->active_command_queue, ZEST_FIF);
        zest_RecordCommandQueue(ZestRenderer->active_command_queue, ZEST_FIF);
        zest_EndCommandQueue(ZestRenderer->active_command_queue, ZEST_FIF);
        zest_SubmitCommandQueue(ZestRenderer->active_command_queue, ZestRenderer->fif_fence[ZEST_FIF]);
    }

    zest__present_frame();
}

void zest__present_frame() {
    VkPresentInfoKHR presentInfo = { 0 };
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &ZestRenderer->render_finished_semaphore[ZestRenderer->current_image_frame];
    VkSwapchainKHR swapChains[] = { ZestRenderer->swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &ZestRenderer->current_image_frame;
    presentInfo.pResults = ZEST_NULL;

    VkResult result = vkQueuePresentKHR(ZestDevice->graphics_queue.vk_queue, &presentInfo);

    if ((ZestRenderer->flags & zest_renderer_flag_schedule_change_vsync) || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || ZestApp->window->framebuffer_resized) {
        ZestApp->window->framebuffer_resized = ZEST_FALSE;
        ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_schedule_change_vsync);

        zest__recreate_swapchain();
    }
    else {
        ZEST_VK_CHECK_RESULT(result);
    }
    ZestDevice->previous_fif = ZestDevice->current_fif;
    ZestDevice->current_fif = (ZestDevice->current_fif + 1) % ZEST_MAX_FIF;

}

void zest__dummy_submit_fence_only(void) {
    VkPipelineStageFlags dst_stage_mask_array[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; 

    VkSubmitInfo submit_info = { 0 };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pWaitDstStageMask = dst_stage_mask_array;      
    submit_info.commandBufferCount = 0;
    VkFence fence = ZestRenderer->fif_fence[ZEST_FIF];
    vkResetFences(ZestDevice->logical_device, 1, &fence);
    ZEST_VK_CHECK_RESULT(vkQueueSubmit(ZestDevice->graphics_queue.vk_queue, 1, &submit_info, fence));
    ZestDevice->previous_fif = ZestDevice->current_fif;
    ZestDevice->current_fif = (ZestDevice->current_fif + 1) % ZEST_MAX_FIF;
}

void zest__dummy_submit_for_present_only(void) {
    vkResetCommandBuffer(ZestRenderer->utility_command_buffer[ZEST_FIF], 0); // Or use appropriate reset flags

    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(ZestRenderer->utility_command_buffer[ZEST_FIF], &beginInfo);

    VkImageMemoryBarrier image_barrier = { 0 };
    image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barrier.srcAccessMask = 0; // Coming from UNDEFINED after acquire
    image_barrier.dstAccessMask = 0; // Presentation engine doesn't have a GPU access mask
    image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier.image = ZestRenderer->swapchain_images[ZestRenderer->current_image_frame];
    image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_barrier.subresourceRange.baseMipLevel = 0;
    image_barrier.subresourceRange.levelCount = 1;
    image_barrier.subresourceRange.baseArrayLayer = 0;
    image_barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        ZestRenderer->utility_command_buffer[ZEST_FIF],
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,      // Stage for source (acquire happens before pipeline)
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,   // Ensure transition completes before present can occur
        // (or VK_PIPELINE_STAGE_ALL_COMMANDS_BIT if simpler)
        0,                                      // Dependency flags
        0, NULL,                                // Global Memory Barriers
        0, NULL,                                // Buffer Memory Barriers
        1, &image_barrier                       // Image Memory Barriers
    );

    vkEndCommandBuffer(ZestRenderer->utility_command_buffer[ZEST_FIF]);

    VkPipelineStageFlags wait_stage_array[] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT }; 
    VkSubmitInfo submit_info = { 0 };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &ZestRenderer->image_available_semaphore[ZEST_FIF];
    submit_info.pWaitDstStageMask = wait_stage_array;

    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &ZestRenderer->render_finished_semaphore[ZestRenderer->current_image_frame];

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &ZestRenderer->utility_command_buffer[ZEST_FIF];

    VkFence fence = ZestRenderer->fif_fence[ZEST_FIF];
    ZEST_VK_CHECK_RESULT(vkQueueSubmit(ZestDevice->graphics_queue.vk_queue, 1, &submit_info, fence));
}

void zest_RenderDrawRoutinesCallback(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer) {
    VkRenderPassBeginInfo render_pass_info = { 0 };
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass;
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

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    
    for (zest_foreach_i(item->draw_routines)) {
        zest_draw_routine draw_routine = item->draw_routines[i];
		ZEST_ASSERT(draw_routine->condition_callback);  //You must set the condition callback for the draw routine
														//to determine if there's anything to record

		if(draw_routine->condition_callback(draw_routine)) {
			if (draw_routine->record_callback) {
				zest__add_work_queue_entry(&ZestRenderer->work_queue, draw_routine, draw_routine->record_callback);
				zest_vec_push(item->secondary_command_buffers, draw_routine->recorder->command_buffer[ZEST_FIF]);
			}
		}
    }

    zest__complete_all_work(&ZestRenderer->work_queue);

    zest_uint command_buffer_count = zest_vec_size(item->secondary_command_buffers);
    if (command_buffer_count > 0) {
        //Note: If you get a validation error here about an empty command queue being executed then make sure that
        //the condition callback for the draw routine is returning the correct value when there's nothing to record.
        vkCmdExecuteCommands(command_buffer, command_buffer_count, item->secondary_command_buffers);
		zest_vec_clear(item->secondary_command_buffers);
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
    zest_FreeCommandBuffers(command_queue->recorder);
    vkDestroyQueryPool(ZestDevice->logical_device, command_queue->query_pool, &ZestDevice->allocation_callbacks);
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

void zest_SetScreenSizedViewport(VkCommandBuffer command_buffer, float min_depth, float max_depth) {
    ZEST_ASSERT(command_buffer);    //Must be a valid command buffer
    VkViewport viewport = {
        .width = (float)ZestApp->window->window_width,
        .height = (float)ZestApp->window->window_height,
        .minDepth = min_depth,
        .maxDepth = max_depth,
    };
    VkRect2D scissor = {
		.extent.width = ZestApp->window->window_width,
		.extent.height = ZestApp->window->window_height,
		.offset.x = 0,
		.offset.y = 0,
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

VkRect2D zest_CreateRect2D(zest_uint width, zest_uint height, int offsetX, int offsetY) {
    VkRect2D rect2D = { 0 };
    rect2D.extent.width = width;
    rect2D.extent.height = height;
    rect2D.offset.x = offsetX;
    rect2D.offset.y = offsetY;
    return rect2D;
}

void zest_Clip(VkCommandBuffer command_buffer, float x, float y, float width, float height, float minDepth, float maxDepth) {
	VkViewport view = zest_CreateViewport(x, y, width, height, minDepth, maxDepth);
	VkRect2D scissor = zest_CreateRect2D((zest_uint)width, (zest_uint)height, (zest_uint)x, (zest_uint)y);
	vkCmdSetViewport(command_buffer, 0, 1, &view);
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

VkImageView zest__create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, zest_uint mip_levels_this_view, zest_uint base_mip, VkImageViewType view_type, zest_uint layer_count) {
    VkImageViewCreateInfo viewInfo = { 0 };
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = view_type;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspect_flags;
    viewInfo.subresourceRange.baseMipLevel = base_mip;
    viewInfo.subresourceRange.levelCount = mip_levels_this_view;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = layer_count;

    VkImageView image_view;
    ZEST_VK_CHECK_RESULT(vkCreateImageView(ZestDevice->logical_device, &viewInfo, &ZestDevice->allocation_callbacks, &image_view) != VK_SUCCESS);

    return image_view;
}

void zest__create_transient_buffer(zest_resource_node node) {
    node->storage_buffer = zest_CreateBuffer(node->buffer_desc.size, &node->buffer_desc.buffer_info, 0);
}

void zest__create_transient_image(zest_resource_node node) {
    VkImageCreateInfo image_info = { 0 };
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = node->image_desc.width;
    image_info.extent.height = node->image_desc.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = node->image_desc.mip_levels;
    image_info.arrayLayers = 1;
    image_info.format = node->image_desc.format;
    image_info.tiling = node->image_desc.tiling;
    image_info.initialLayout = node->current_layout;
    image_info.usage = node->image_desc.usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = node->image_desc.numSamples;

    ZEST_VK_CHECK_RESULT(vkCreateImage(ZestDevice->logical_device, &image_info, &ZestDevice->allocation_callbacks, &node->image_buffer.image));

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(ZestDevice->logical_device, node->image_buffer.image, &memory_requirements);

    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.image_usage_flags = node->image_desc.usage;
    buffer_info.property_flags = node->image_desc.properties;
    node->image_buffer.buffer = zest_CreateBuffer(memory_requirements.size, &buffer_info, node->image_buffer.image);

    vkBindImageMemory(ZestDevice->logical_device, node->image_buffer.image, zest_GetBufferDeviceMemory(node->image_buffer.buffer), node->image_buffer.buffer->memory_offset);
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

void zest__copy_buffer_to_image(VkBuffer buffer, VkDeviceSize src_offset, VkImage image, zest_uint width, zest_uint height, VkImageLayout image_layout, VkCommandBuffer cb) {
    VkCommandBuffer command_buffer = cb ? cb : zest__begin_single_time_commands();

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

void zest__copy_buffer_regions_to_image(VkBufferImageCopy* regions, VkBuffer buffer, VkDeviceSize src_offset, VkImage image, VkCommandBuffer cb) {
    VkCommandBuffer command_buffer = cb ? cb : zest__begin_single_time_commands();

    for (zest_foreach_i(regions)) {
        regions[i].bufferOffset += src_offset;
    }

    vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, zest_vec_size(regions), regions);

    if (!cb) {
        zest__end_single_time_commands(command_buffer);
    }
}

void zest__generate_mipmaps(VkImage image, VkFormat image_format, int32_t texture_width, int32_t texture_height, zest_uint mip_levels, zest_uint layer_count, VkImageLayout image_layout, VkCommandBuffer cb) {
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(ZestDevice->physical_device, image_format, &format_properties);

    ZEST_ASSERT(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT); //texture image format does not support linear blitting!

    VkCommandBuffer command_buffer = cb ? cb : zest__begin_single_time_commands();

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

    if (!cb) {
        zest__end_single_time_commands(command_buffer);
    }
}

VkImageMemoryBarrier zest__create_base_image_memory_barrier(VkImage image) {
    VkImageMemoryBarrier barrier = { 0 };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    return barrier;
}

VkImageMemoryBarrier zest__create_image_memory_barrier(VkImage image, VkAccessFlags from_access, VkAccessFlags to_access, VkImageLayout from_layout, VkImageLayout to_layout, zest_uint target_mip_level, zest_uint mip_count) {
    VkImageMemoryBarrier barrier = { 0 };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = from_layout;
    barrier.newLayout = to_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = target_mip_level;
    barrier.subresourceRange.levelCount = mip_count;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = from_access;
    barrier.dstAccessMask = to_access;
    return barrier;
}

VkBufferMemoryBarrier zest__create_buffer_memory_barrier( VkBuffer buffer, VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask, VkDeviceSize offset, VkDeviceSize size) {
    VkBufferMemoryBarrier barrier = { 0 }; 
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; 
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = offset;
    barrier.size = size;
    return barrier;
}

void zest__place_fragment_barrier(VkCommandBuffer command_buffer, VkImageMemoryBarrier *barrier) {
	vkCmdPipelineBarrier(
		command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, NULL,
		0, NULL,
		1, barrier
	);
}

void zest__place_image_barrier(VkCommandBuffer command_buffer, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage, VkImageMemoryBarrier *barrier) {
	vkCmdPipelineBarrier(
		command_buffer,
		src_stage,
		dst_stage,
		0,
		0, NULL,
		0, NULL,
		1, barrier
	);
}

void zest__transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, zest_uint mip_levels, zest_uint layerCount, VkCommandBuffer cb) {
    VkCommandBuffer command_buffer = cb ? cb : zest__begin_single_time_commands();

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

    if (!cb) {
        zest__end_single_time_commands(command_buffer);
    }

}

VkRenderPass zest__create_render_pass(VkFormat render_format, VkImageLayout initial_layout, VkImageLayout final_layout, VkAttachmentLoadOp load_op, zest_bool depth_buffer) {
    VkAttachmentDescription color_attachment = { 0 };
    color_attachment.format = render_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = load_op;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = initial_layout;
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

    vkQueueSubmit(ZestDevice->graphics_queue.vk_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(ZestDevice->graphics_queue.vk_queue);

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
        .render_graph_allocator_size = zloc__MEGABYTE(1),
        .shader_path_prefix = "spv/",
        .log_path = NULL,
        .screen_width = 1280,
        .screen_height = 768,
        .screen_x = 0,
        .screen_y = 50,
        .virtual_width = 1280,
        .virtual_height = 768,
        .thread_count = zest_GetDefaultThreadCount(),
        .color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .flags = zest_init_flag_enable_vsync | zest_init_flag_cache_shaders,
        .destroy_window_callback = zest__destroy_window_callback,
        .get_window_size_callback = zest__get_window_size_callback,
        .poll_events_callback = zest__os_poll_events,
        .add_platform_extensions_callback = zest__os_add_platform_extensions,
        .create_window_callback = zest__os_create_window,
        .create_window_surface_callback = zest__os_create_window_surface,
        .maximum_textures = 1024,
        .bindless_combined_sampler_count = 256,
        .bindless_sampler_count = 256,
        .bindless_sampled_image_count = 256,
        .bindless_storage_buffer_count = 256,
    };
    return create_info;
}

zest_create_info_t zest_CreateInfoWithValidationLayers(zest_validation_flags flags) {
    zest_create_info_t create_info = zest_CreateInfo();
    ZEST__FLAG(create_info.flags, zest_init_flag_enable_validation_layers);
    if (flags & zest_validation_flag_enable_sync) {
        create_info.flags |= zest_init_flag_enable_validation_layers_with_sync;
    }
    return create_info;
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

// --Render Graph functions
zest_render_graph zest__new_render_graph(const char *name) {
    zest_render_graph_t blank = { 0 };
    zest_render_graph render_graph = zloc_LinearAllocation(ZestRenderer->render_graph_allocator, sizeof(zest_render_graph_t));
    *render_graph = blank;
    render_graph->magic = zest_INIT_MAGIC;
    render_graph->name = name;
    render_graph->bindless_layout = ZestRenderer->global_bindless_set_layout;
    render_graph->bindless_set = ZestRenderer->global_set;
    VkSemaphoreCreateInfo semaphore_info = { 0 };
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    zest_vec_linear_push(ZestRenderer->render_graph_allocator, ZestRenderer->render_graphs, render_graph);
    /*
    render_graph->timestamp_count = zest_vec_size(render_graph->draw_commands) * 2;
    if (!render_graph->query_pool) {
        render_graph->query_pool = zest__create_query_pool(render_graph->timestamp_count);
    }
    zest_vec_resize(render_graph->timestamps[0], render_graph->timestamp_count / 2);
    zest_vec_resize(render_graph->timestamps[1], render_graph->timestamp_count / 2);
    */
    return render_graph;
}

bool zest_BeginRenderGraph(const char *name) {
	ZEST_ASSERT(ZEST__NOT_FLAGGED(ZestRenderer->flags, zest_renderer_flag_building_render_graph));  //Render graph already being built. You cannot build a render graph within another begin render graph process.

    zest_render_graph render_graph = zest__new_render_graph(name);

	ZEST__UNFLAG(render_graph->flags, zest_render_graph_expecting_swap_chain_usage);
	ZEST__FLAG(ZestRenderer->flags, zest_renderer_flag_building_render_graph);
    ZestRenderer->current_render_graph = render_graph;
    return true;
}

bool zest_BeginRenderToScreen(const char *name) {
    if (zest_BeginRenderGraph(name)) {
		ZEST__FLAG(ZestRenderer->current_render_graph->flags, zest_render_graph_expecting_swap_chain_usage);
    } else {
        return false;
    }
    ZEST_ASSERT(ZEST__NOT_FLAGGED(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_acquired));    //Swap chain was already acquired. Only one render graph can output to the swap chain per frame.
    if (!zest_AcquireSwapChainImage()) {
        ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_building_render_graph);
        ZEST_PRINT("Unable to acquire the swap chain!");
        return false;
    }
    return true;
}

void zest_ForceRenderGraphOnGraphicsQueue() {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);      //This function should only be called immediately after BeginRenderGraph/BeginRenderToScreen
    ZEST__FLAG(ZestRenderer->current_render_graph->flags, zest_render_graph_force_on_graphics_queue);
}

zest_bool zest__is_stage_compatible_with_qfi( VkPipelineStageFlags stages_to_check, VkQueueFlags queue_family_capabilities) {
    if (stages_to_check == VK_PIPELINE_STAGE_NONE_KHR) { // Or just == 0
        return ZEST_TRUE; // No stages specified is trivially compatible
    }

    VkPipelineStageFlags current_stages_to_evaluate = stages_to_check;

    // Iterate through each individual bit set in stages_to_check
    while (current_stages_to_evaluate != 0) {
        // Get the lowest set bit
        VkPipelineStageFlags single_stage_bit = current_stages_to_evaluate & (~current_stages_to_evaluate + 1);
        // Remove this bit for the next iteration
        current_stages_to_evaluate &= ~single_stage_bit;

        bool stage_is_compatible = ZEST_FALSE;
        switch (single_stage_bit) {
            // Stages allowed on ANY queue type that supports any command submission
        case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
        case VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT:
        case VK_PIPELINE_STAGE_HOST_BIT: // Host access can always be synchronized against
        case VK_PIPELINE_STAGE_ALL_COMMANDS_BIT: // Valid, but its scope is limited by queue caps
            stage_is_compatible = ZEST_TRUE;
            break;

            // Stages requiring GRAPHICS_BIT capability
        case VK_PIPELINE_STAGE_VERTEX_INPUT_BIT:
        case VK_PIPELINE_STAGE_VERTEX_SHADER_BIT:
        case VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT:
        case VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT:
        case VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT:
        case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:
        case VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT:
        case VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:
        case VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:
        case VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT:
        //Can add more extensions here if needed
            if (queue_family_capabilities & VK_QUEUE_GRAPHICS_BIT) {
                stage_is_compatible = ZEST_TRUE;
            }
            break;

		// Stage requiring COMPUTE_BIT capability
        case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
            if (queue_family_capabilities & VK_QUEUE_COMPUTE_BIT) {
                stage_is_compatible = ZEST_TRUE;
            }
            break;

		// Stage DRAW_INDIRECT can be used by Graphics or Compute
        case VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT:
            if (queue_family_capabilities & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
                stage_is_compatible = ZEST_TRUE;
            }
            break;

		// Stage requiring TRANSFER_BIT capability (also often implied by Graphics/Compute)
        case VK_PIPELINE_STAGE_TRANSFER_BIT:
            if (queue_family_capabilities & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)) {
                stage_is_compatible = ZEST_TRUE;
            }
            break;

        default:
            stage_is_compatible = ZEST_TRUE; 
            break;
        }

        if (!stage_is_compatible) {
            return ZEST_FALSE; 
        }
    }

    return ZEST_TRUE; 
}

/*
Render graph compiler index:
[Set_producers_and_consumers]
[Set_adjacency_list]
[Process_dependency_queue]
[Resource_journeys]
[Create_command_batches]
[Calculate_lifetime_of_resources]
[Create_resource_barriers]
[Create_semaphores]
[Cull_unused_resources_and_passes]
[Alocate_transient_buffers]
[Process_compiled_execution_order]
[Create_memory_barriers_for_inputs]
[Create_memory_barriers_for_outputs]
[Create_render_passes]
[Create_frame_buffers]
*/
zest_render_graph zest_EndRenderGraph() {
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    ZEST_CHECK_HANDLE(render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen

    zloc_linear_allocator_t *allocator = ZestRenderer->render_graph_allocator;

    zest_pass_adjacency_list_t *adjacency_list = { 0 };
    zest_uint *dependency_count = 0;
    zest_vec_linear_resize(allocator, adjacency_list, zest_vec_size(render_graph->passes));
    zest_vec_linear_resize(allocator, dependency_count, zest_vec_size(render_graph->passes));

    bool has_execution_callback = false;

    // Set_producers_and_consumers:
    // For each output in a pass, we set it's producer_pass_idx - the pass that writes the output
    // and for each input in a pass we add all of the comuser_pass_idx's that read the inputs
    zest_vec_foreach(i, render_graph->passes) { 
        zest_pass_node pass_node = &render_graph->passes[i];
        dependency_count[i] = 0; // Initialize in-degree
        zest_pass_adjacency_list_t adj_list = { 0 };
        adjacency_list[i] = adj_list;

        switch (pass_node->queue_type) {
        case zest_queue_graphics: 
            pass_node->queue = &ZestDevice->graphics_queue; 
            pass_node->queue_family_index = ZestDevice->graphics_queue_family_index; 
            if (pass_node->execution_callbacks) {
                has_execution_callback = true;
            }
            break;
        case zest_queue_compute: 
            pass_node->queue = &ZestDevice->compute_queue; 
            pass_node->queue_family_index = ZestDevice->compute_queue_family_index; 
            break;
        case zest_queue_transfer: 
            pass_node->queue = &ZestDevice->transfer_queue; 
            pass_node->queue_family_index = ZestDevice->transfer_queue_family_index; 
            break;
        }

        zest_map_foreach(j, pass_node->outputs) {
            zest_resource_usage_t *output_usage = &pass_node->outputs.data[j];
            //A resource should only have one producer in a valid graph.
            ZEST_ASSERT(output_usage->resource_node->producer_pass_idx == -1 || output_usage->resource_node->producer_pass_idx == i);
            output_usage->resource_node->producer_pass_idx = i;
        }

        zest_map_foreach(j, pass_node->inputs) {
            zest_resource_usage_t *input_usage = &pass_node->inputs.data[j];
            zest_resource_node resource = input_usage->resource_node;
            zest_vec_linear_push(allocator, resource->consumer_pass_indices, i); // pass 'i' consumes this
        }
    }

    if (!has_execution_callback) {
        ZEST_PRINT("WARNING: No execution callbacks found in render graph %s.", render_graph->name);
        ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_building_render_graph);
        return render_graph;
    }

    // Set_adjacency_list
    zest_vec_foreach(consumer_idx, render_graph->passes) {
        zest_pass_node consumer_pass = &render_graph->passes[consumer_idx];
        zest_map_foreach(j, consumer_pass->inputs) {
            zest_resource_usage_t *input_usage = &consumer_pass->inputs.data[j];
            zest_resource_node resource = input_usage->resource_node;

            int producer_idx = resource->producer_pass_idx;
            if (producer_idx != -1 && producer_idx != consumer_idx) {
                // Dependency: producer_idx ---> consumer_idx
                // Add consumer_idx to the list of passes that producer_idx must precede
                // Ensure no duplicates if a pass reads the same produced resource multiple times in its input list
                zest_bool already_linked = ZEST_FALSE;
                zest_vec_foreach(k_adj, adjacency_list[producer_idx].pass_indices) {
                    if (adjacency_list[producer_idx].pass_indices[k_adj] == consumer_idx) {
                        already_linked = ZEST_TRUE;
                        break;
                    }
                }
                if (!already_linked) {
                    zest_vec_linear_push(allocator, adjacency_list[producer_idx].pass_indices, consumer_idx);
                    dependency_count[consumer_idx]++; // consumer_idx gains an incoming edge
                }
            }
        }
    }

    int *dependency_queue = 0;
    zest_vec_foreach(i, dependency_count) {
        if (dependency_count[i] == 0) {
            zest_vec_linear_push(allocator, dependency_queue, i);
        }
    }

    //Process_dependency_queue
    //Now we loop through the dependency_queue, all of which will have the dependency count (in degrees) set to 0.
    int index = 0;
    while (index != zest_vec_size(dependency_queue)) {
        int pass_index = dependency_queue[index++];
        //Make the pass aware of the execution order index, this is used later when we link up the barriers that need
        //to be executed for resource transitioning and acquiring.
        //Add the pass index to the compiled_execution_order list;
        zest_vec_linear_push(allocator, render_graph->compiled_execution_order, pass_index);
        //Check it's adjacency list and for those adjacent passes, reduce their dependency count by one.
        zest_vec_foreach(i, adjacency_list[pass_index].pass_indices) {
            int adj_index = adjacency_list[pass_index].pass_indices[i];
            dependency_count[adj_index]--;
            //If the dependency count becomes 0 then we can add the pass to the dependency queue
            if (dependency_count[adj_index] == 0) {
                zest_vec_linear_push(allocator, dependency_queue, adj_index);
            }
        }
    }

    if (zest_vec_size(render_graph->compiled_execution_order) == 0) {
        return render_graph;
    }

    ZEST_ASSERT(zest_vec_size(render_graph->compiled_execution_order) == zest_vec_size(render_graph->passes));

    //[Resource_journeys]
    zest_vec_foreach(execution_index, render_graph->compiled_execution_order) {
        int current_pass_index = render_graph->compiled_execution_order[execution_index];
        zest_pass_node current_pass = &render_graph->passes[current_pass_index];
        current_pass->execution_order_index = execution_index;

        zest_batch_key batch_key = { 0 };
        batch_key.current_family_index = current_pass->queue_family_index;

        //Calculate_lifetime_of_resources and also create a state for each resource and plot
        //it's journey through the render graph so that the appropriate barriers and semaphores
        //can be set up
        zest_map_foreach(input_idx, current_pass->inputs) {
            zest_resource_node resource_node = current_pass->inputs.data[input_idx].resource_node;
            if (resource_node) {
                resource_node->first_usage_pass_idx = ZEST__MIN(resource_node->first_usage_pass_idx, (zest_uint)execution_index);
                resource_node->last_usage_pass_idx = ZEST__MAX(resource_node->last_usage_pass_idx, (zest_uint)execution_index);
            }
            zest_resource_state_t state = { 0 };
            state.pass_index = current_pass_index;
            state.queue_family_index = current_pass->queue_family_index;
            state.usage = current_pass->inputs.data[input_idx];
            zest_vec_linear_push(allocator, resource_node->journey, state);
        }

        // Check OUTPUTS of the current pass
        bool requires_new_batch = false;
        zest_map_foreach(output_idx, current_pass->outputs) {
            zest_resource_node resource_node = current_pass->outputs.data[output_idx].resource_node;
            if (resource_node) {
                resource_node->first_usage_pass_idx = ZEST__MIN(resource_node->first_usage_pass_idx, (zest_uint)execution_index);
                resource_node->last_usage_pass_idx = ZEST__MAX(resource_node->last_usage_pass_idx, (zest_uint)execution_index);
            }
            zest_resource_state_t state = { 0 };
            state.pass_index = current_pass_index;
            state.queue_family_index = current_pass->queue_family_index;
            state.usage = current_pass->outputs.data[output_idx];
            zest_vec_linear_push(allocator, resource_node->journey, state);
            zest_vec_foreach(adjacent_index, adjacency_list[current_pass_index].pass_indices) {
                zest_uint consumer_pass_index = adjacency_list[current_pass_index].pass_indices[adjacent_index];
                batch_key.next_pass_indexes |= (1ull << consumer_pass_index);
                batch_key.next_family_indexes |= (1ull << render_graph->passes[consumer_pass_index].queue_family_index);
            }
        }
    }

    //Create_command_batches
    //This is the most complicated section IMO. Because we can have 3 separate queues, graphics, compute and transfer, it means
    //that we need to figure out how to group batches based on which passes are producing for other passes on other queues. To
    //do this I use a batch key which consists of the current queue index for the curren pass, then 2 bit fields:
    //next_pass_indexes:    This is a bit field of all the pass indexes that consume the output from the current pass. (yes that
    //                      means that we have a maximum number of passes per render graph of 64.
    //next_family_indexes:  A bit field of all the queues that consume the output from this pass.
    //Once the key has been made it can be used to see if a batch needs to be split into separate batches, and then semaphores
    //can be set up so that each submission by each batch signals and waits correctly.
    //If a batch has a pass that produce for multiple queues then that batch will need multiple signal semaphores. In this case
    //the next_family_indexes will have multiple bits set.

    // --- Bootstrap the first batch from the very first pass ---
    int first_pass_index = render_graph->compiled_execution_order[0];
    zest_pass_node first_pass = &render_graph->passes[first_pass_index];

    // Create the first submission batch.
    zest_submission_batch_t current_batch = { 0 };
    current_batch.queue = first_pass->queue;
    current_batch.queue_family_index = first_pass->queue_family_index;
    current_batch.timeline_wait_stage = first_pass->timeline_wait_stage;
    current_batch.queue_type = first_pass->queue_type;

    zest_vec_linear_push(allocator, current_batch.pass_indices, first_pass_index);
    first_pass->batch_index = 0; // The first pass is in the first batch (index 0)

    // Generate the key for the first pass, this key now defines the "signature" of our current_batch.
    zest_batch_key current_batch_key = { 0 };
    current_batch_key.current_family_index = first_pass->queue_family_index;

    zest_map_foreach(output_idx, first_pass->outputs) {
        zest_vec_foreach(adjacent_index, adjacency_list[first_pass_index].pass_indices) {
            zest_uint consumer_pass_index = adjacency_list[first_pass_index].pass_indices[adjacent_index];
            zest_pass_node consumer_pass = &render_graph->passes[consumer_pass_index];
            current_batch_key.next_pass_indexes |= (1ull << consumer_pass_index);
            current_batch_key.next_family_indexes |= (1ull << consumer_pass->queue_family_index);
        }
    }

    for (zest_uint execution_index = 1; execution_index < zest_vec_size(render_graph->compiled_execution_order); ++execution_index) {
        int current_pass_index = render_graph->compiled_execution_order[execution_index];
        zest_pass_node current_pass = &render_graph->passes[current_pass_index];

        // Generate the key for the current pass.
        zest_batch_key batch_key = { 0 };

        batch_key.current_family_index = current_pass->queue_family_index;
        zest_map_foreach(output_idx, current_pass->outputs) {
            zest_vec_foreach(adjacent_index, adjacency_list[current_pass_index].pass_indices) {
                zest_uint consumer_pass_index = adjacency_list[current_pass_index].pass_indices[adjacent_index];
				zest_pass_node consumer_pass = &render_graph->passes[consumer_pass_index];
                batch_key.next_pass_indexes |= (1ull << consumer_pass_index);
				batch_key.next_family_indexes |= (1ull << consumer_pass->queue_family_index);
            }
        }

        // Compare the pass's key to the current batch's key.
        if (ZEST__FLAGGED(render_graph->flags, zest_render_graph_force_on_graphics_queue) || memcmp(&batch_key, &current_batch_key, sizeof(zest_batch_key)) == 0) {
        //if (memcmp(&batch_key, &current_batch_key, sizeof(zest_batch_key)) == 0) {
            // --- KEYS MATCH: Add this pass to the current batch ---
			current_batch.timeline_wait_stage |= current_pass->timeline_wait_stage;
            zest_vec_linear_push(allocator, current_batch.pass_indices, current_pass_index);
        } else {
            // --- KEYS DIFFER: The current batch must end here. ---

            zest_vec_linear_push(allocator, render_graph->submissions, current_batch);

            current_batch = (zest_submission_batch_t){ 0 }; // Clear it
			current_batch.timeline_wait_stage = current_pass->timeline_wait_stage;
            current_batch.queue = current_pass->queue;
            current_batch.queue_family_index = current_pass->queue_family_index;
            current_batch.queue_type = current_pass->queue_type;
            zest_vec_linear_push(allocator, current_batch.pass_indices, current_pass_index);

            current_batch_key = batch_key;
        }
		current_pass->batch_index = zest_vec_size(render_graph->submissions);
    }

    zest_vec_linear_push(allocator, render_graph->submissions, current_batch);

    zest_vec_foreach(execution_index, render_graph->compiled_execution_order) {
        int current_pass_index = render_graph->compiled_execution_order[execution_index];
        zest_pass_node current_pass = &render_graph->passes[current_pass_index];
        current_pass->execution_order_index = execution_index;
    }

    //Create_semaphores
    //Now that the batches have been created we need to figure out the dependencies and set up the semaphore signalling
    //in each batch (if there's more then one render batch)
    if (zest_vec_size(render_graph->submissions) > 1) {

        // A temporary map to track which semaphore we've created for each
        // dependency between two batches.
        zest_hash_map(VkSemaphore) dependency_semaphores_map;
        dependency_semaphores_map dependency_semaphores = { 0 };

        zest_vec_foreach(resource_index, render_graph->resources) {
            zest_resource_node resource = &render_graph->resources[resource_index];
            zest_pass_node producer_pass = resource->producer_pass_idx != -1 ? &render_graph->passes[resource->producer_pass_idx] : NULL;

            if (producer_pass) {
                zest_vec_foreach(resource_consumer_index, resource->consumer_pass_indices) {
                    zest_uint consumer_pass_index = resource->consumer_pass_indices[resource_consumer_index];
                    zest_pass_node consumer_pass = &render_graph->passes[consumer_pass_index];

                    if (consumer_pass->batch_index != producer_pass->batch_index) {
                        zest_uint producer_idx = producer_pass->batch_index;
                        zest_uint consumer_idx = consumer_pass->batch_index;
                        zest_submission_batch_t *producer_batch = &render_graph->submissions[producer_idx];
                        zest_submission_batch_t *consumer_batch = &render_graph->submissions[consumer_idx];

                        // Create a unique key for the dependency arc from producer batch to consumer batch.
                        uint64_t dependency_key = ((uint64_t)producer_idx << 32) | consumer_idx;

                        VkSemaphore semaphore_for_this_dependency = 0;

                        // Have we already created a semaphore for this specific dependency (A->B)?
                        if (!zest_map_valid_key(dependency_semaphores, dependency_key)) {
                            // --- First time we see this dependency arc ---

                            // 1. Acquire a new semaphore.
                            VkSemaphore new_semaphore = zest__acquire_semaphore();
                            zest_vec_push(ZestRenderer->used_semaphores[ZEST_FIF], new_semaphore);
                            semaphore_for_this_dependency = new_semaphore;

                            // 2. Log it in our map.
                            zest_map_insert_linear_key(allocator, dependency_semaphores, dependency_key, new_semaphore);

                            // 3. Add it to the producer's list of semaphores to SIGNAL.
                            zest_vec_linear_push(allocator, producer_batch->signal_semaphores, new_semaphore);

                            // 4. Add it to the consumer's list of semaphores to WAIT on.
                            zest_vec_linear_push(allocator, consumer_batch->wait_semaphores, new_semaphore);

                            // 5. Add its corresponding stage mask.
                            zest_resource_usage_t *usage = zest_map_at(consumer_pass->inputs, resource->name);
                            zest_vec_linear_push(allocator, consumer_batch->wait_dst_stage_masks, usage->stage_mask);

                        } else {
                            // --- We already have a semaphore for this dependency arc ---
                            // This happens if another resource also travels from this same producer batch
                            // to this same consumer batch. We don't need a new semaphore.

                            semaphore_for_this_dependency = *zest_map_at_key(dependency_semaphores, dependency_key);

                            // We just need to find the existing wait entry and merge the pipeline stage flags.
                            zest_vec_foreach(wait_index, consumer_batch->wait_semaphores) {
                                if (consumer_batch->wait_semaphores[wait_index] == semaphore_for_this_dependency) {
                                    zest_resource_usage_t *usage = zest_map_at(consumer_pass->inputs, resource->name);
                                    consumer_batch->wait_dst_stage_masks[wait_index] |= usage->stage_mask;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    //Potentially connect the first batch that uses the swap chain image
    if (zest_vec_size(render_graph->submissions) > 0) {
        // --- Handle imageAvailableSemaphore ---
        if (ZEST__FLAGGED(render_graph->flags, zest_render_graph_expecting_swap_chain_usage)) {

            zest_submission_batch_t *first_batch_to_wait = NULL;
            VkPipelineStageFlags wait_stage_for_acquire_semaphore = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // Safe default

            zest_resource_node swapchain_node = render_graph->swapchain_resource_handle; 
            if (swapchain_node->first_usage_pass_idx != UINT_MAX) {
                zest_uint pass_index = render_graph->compiled_execution_order[swapchain_node->first_usage_pass_idx];
				zest_pass_node pass = &render_graph->passes[pass_index];
				zest_submission_batch_t *batch_pass_is_in = &render_graph->submissions[pass->batch_index];
				zest_bool uses_swapchain = ZEST_FALSE;
				VkPipelineStageFlags first_swapchain_usage_stage_in_this_batch = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                if (zest_map_valid_name(pass->inputs, swapchain_node->name)) {
					zest_resource_usage_t *usage = zest_map_at(pass->inputs, swapchain_node->name);
					uses_swapchain = ZEST_TRUE;
					first_swapchain_usage_stage_in_this_batch = usage->stage_mask;
                } else if (zest_map_valid_name(pass->outputs, swapchain_node->name)) {
					zest_resource_usage_t *usage = zest_map_at(pass->outputs, swapchain_node->name);
					uses_swapchain = ZEST_TRUE;
					first_swapchain_usage_stage_in_this_batch = usage->stage_mask;
                }
                if (uses_swapchain) {
                    first_batch_to_wait = batch_pass_is_in;
                    wait_stage_for_acquire_semaphore = first_swapchain_usage_stage_in_this_batch;
                    // Ensure this stage is compatible with the batch's queue
                    if (!zest__is_stage_compatible_with_qfi(wait_stage_for_acquire_semaphore, ZestDevice->queue_families[first_batch_to_wait->queue_family_index].queueFlags)) {
                        zest_text_t pipeline_stage = zest__vulkan_pipeline_stage_flags_to_string(wait_stage_for_acquire_semaphore);
                        ZEST_PRINT("Swapchain usage stage %s is not compatible with queue family %u for batch %i",
                            pipeline_stage.str,
                            first_batch_to_wait->queue_family_index, pass->batch_index);
                        // Fallback or error. Forcing TOP_OF_PIPE might be safer if this happens,
                        // though it indicates a graph definition error.
                        wait_stage_for_acquire_semaphore = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                        zest_FreeText(&pipeline_stage);
                    }
                }
            }

            // 2. Assign the wait:
            if (first_batch_to_wait) {
                // The first batch that actually uses the swapchain will wait for it.
                zest_vec_linear_push(allocator, first_batch_to_wait->wait_semaphores, ZestRenderer->image_available_semaphore[ZEST_FIF]);
                zest_vec_linear_push(allocator, first_batch_to_wait->wait_dst_stage_masks, wait_stage_for_acquire_semaphore);
            } else {
                // Image was acquired, but no pass in the graph uses it.
                // The *very first submission batch of the graph* must wait on the semaphore to consume it.
                // The wait stage must be compatible with this first batch's queue.
                zest_submission_batch_t *actual_first_batch = &render_graph->submissions[0];
                VkPipelineStageFlags compatible_dummy_wait_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

                // Make the dummy wait stage more specific if possible, but ensure compatibility
                if (ZestDevice->queue_families[actual_first_batch->queue_family_index].queueFlags & VK_QUEUE_TRANSFER_BIT) {
                    compatible_dummy_wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                } else if (ZestDevice->queue_families[actual_first_batch->queue_family_index].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                    compatible_dummy_wait_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                }
                // Graphics queue can also do TOP_OF_PIPE or even COLOR_ATTACHMENT_OUTPUT_BIT if it's just for semaphore flow.

                zest_vec_linear_push(allocator, actual_first_batch->wait_semaphores, ZestRenderer->image_available_semaphore[ZEST_FIF]);
                zest_vec_linear_push(allocator, actual_first_batch->wait_dst_stage_masks, compatible_dummy_wait_stage);
				zest_text_t pipeline_stage = zest__vulkan_pipeline_stage_flags_to_string(compatible_dummy_wait_stage);
                ZEST_PRINT("RenderGraph: Swapchain image acquired but not used by any pass. First batch (on QFI %u) will wait on imageAvailableSemaphore at stage %s.",
                    actual_first_batch->queue_family_index,
                    pipeline_stage.str);
                zest_FreeText(&pipeline_stage);
            }
        }

        // --- Handle renderFinishedSemaphore for the last batch ---
        zest_submission_batch_t *last_batch = &zest_vec_back(render_graph->submissions);
        // This assumes the last batch's *primary* signal is renderFinished.
        // If it also needs to signal internal semaphores, `signal_semaphore` needs to become a list.
        if (!last_batch->signal_semaphores) {
            zest_vec_linear_push(allocator, last_batch->signal_semaphores, ZestRenderer->render_finished_semaphore[ZestRenderer->current_image_frame]);
        } else {
            // This case needs `p_signal_semaphores` to be a list in your batch struct.
            // You would then add ZestRenderer->frame_sync[ZEST_FIF].render_finished_semaphore to that list.
            ZEST_PRINT("Last batch already has an internal signal_semaphore. Logic to add external renderFinishedSemaphore needs p_signal_semaphores to be a list.");
            // For now, you might just overwrite if single signal is assumed for external:
            // last_batch->internal_signal_semaphore = ZestRenderer->frame_sync[ZEST_FIF].render_finished_semaphore;
        }
    }

    //Cull_unused_resources_and_passes
    //--------------------------------------------------------
    //Potentially cull unused resources and render passes here. TBA.
    //--------------------------------------------------------

    zest_uint size_of_exe_order = zest_vec_size(render_graph->compiled_execution_order);
    VkWriteDescriptorSet *desriptor_writes = 0;

    //Alocate_transient_buffers
    zest_vec_foreach(r, render_graph->resources) {
        zest_resource_node resource = &render_graph->resources[r];

        // Check if this resource is transient AND actually used in the compiled graph
        if (ZEST__FLAGGED(resource->flags, zest_resource_node_flag_transient) &&
            resource->first_usage_pass_idx <= resource->last_usage_pass_idx && // Ensures it's used
            resource->first_usage_pass_idx < zest_vec_size(render_graph->compiled_execution_order)) { // Valid index

            if (resource->type == zest_resource_type_image && resource->image_buffer.image == VK_NULL_HANDLE) {
                zest__create_transient_image(resource); 
                resource->image_buffer.base_view = zest__create_image_view(
                    resource->image_buffer.image,
                    resource->image_desc.format,
                    zest__determine_aspect_flag(resource->image_desc.format), 
                    resource->image_desc.mip_levels,
                    0, 
                    VK_IMAGE_VIEW_TYPE_2D,
                    1  
                );
                resource->current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
                resource->current_access_mask = 0;
                resource->last_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                if (ZEST__FLAGGED(resource->flags, zest_resource_node_flag_is_bindless) && ZEST_VALID_HANDLE(render_graph->bindless_layout)) {
                    resource->bindless_index = zest__acquire_bindless_index(render_graph->bindless_layout, resource->binding_number);
                    zest_binding_index_for_release_t binding_index = { render_graph->bindless_layout, resource->bindless_index, resource->binding_number };
                    zest_vec_push(ZestRenderer->deferred_resource_freeing_list.binding_indexes[ZEST_FIF], binding_index);
                }

            } else if (resource->type == zest_resource_type_buffer && resource->storage_buffer == NULL) { 
                zest__create_transient_buffer(resource); 
                resource->current_access_mask = 0;
                resource->last_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                if (ZEST__FLAGGED(resource->flags, zest_resource_node_flag_is_bindless) && ZEST_VALID_HANDLE(render_graph->bindless_layout)) {
                    resource->bindless_index = zest__acquire_bindless_index(render_graph->bindless_layout, resource->binding_number);
                    VkDescriptorBufferInfo buffer_info;
                    buffer_info.buffer = resource->storage_buffer->memory_pool->buffer;
                    buffer_info.offset = resource->storage_buffer->memory_offset;
                    buffer_info.range = resource->storage_buffer->size;
                     
                    VkWriteDescriptorSet write = zest_CreateBufferDescriptorWriteWithType(render_graph->bindless_set->vk_descriptor_set, &buffer_info, resource->binding_number, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
                    write.dstArrayElement = resource->bindless_index;
                    vkUpdateDescriptorSets(ZestDevice->logical_device, 1, &write, 0, 0);
                    zest_binding_index_for_release_t binding_index = { render_graph->bindless_layout, resource->bindless_index, resource->binding_number };
                    zest_vec_push(ZestRenderer->deferred_resource_freeing_list.binding_indexes[ZEST_FIF], binding_index);
                }
            }
        }
    }

    zest_vec_linear_resize(allocator, render_graph->pass_exec_details_list, zest_vec_size(render_graph->compiled_execution_order));
    memset(render_graph->pass_exec_details_list, 0, zest_vec_size_in_bytes(render_graph->pass_exec_details_list));

    //Create_resource_barriers
    zest_vec_foreach(resource_index, render_graph->resources) {
        zest_resource_node resource = &render_graph->resources[resource_index];
        zest_resource_state_t *prev_state = NULL;
        int starting_state_index = 0;
        zest_vec_foreach(state_index, resource->journey) {
            zest_resource_state_t *current_state = &resource->journey[state_index];
            zest_pass_node pass = &render_graph->passes[current_state->pass_index];
            zest_resource_usage_t *current_usage = &current_state->usage;
            zest_resource_state_t *next_state = NULL;
            zest_execution_details_t *exe_details = &render_graph->pass_exec_details_list[pass->execution_order_index];
            zest_execution_barriers_t *barriers = &render_graph->pass_exec_details_list[pass->execution_order_index].barriers;
            if (state_index + 1 < (int)zest_vec_size(resource->journey)) {
                next_state = &resource->journey[state_index + 1];
            }
            VkPipelineStageFlags required_stage = current_usage->stage_mask;
            if (resource->type == zest_resource_type_image || resource->type == zest_resource_type_swap_chain_image) {
                VkImage image = resource->image_buffer.image;
                if (!prev_state) {
                    //This is the first state of the resource
                    //If there's no previous state then we need to see if a barrier is needed to transition from the resource
                    //start state. We put this in the acquire barrier as it needs to be put in place before the pass is executed.
                    zest_uint src_queue_family_index = resource->current_queue_family_index;
                    zest_uint dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
                    if (src_queue_family_index == VK_QUEUE_FAMILY_IGNORED) {
                        if (resource->current_layout != current_usage->image_layout ||
                            (resource->current_access_mask & zest_access_write_bits_general) &&
                            (current_usage->access_mask & zest_access_read_bits_general)) {
                            zest__add_image_barrier(resource, barriers, true,
								resource->current_access_mask, current_usage->access_mask,
								resource->current_layout, current_usage->image_layout, 
                                src_queue_family_index, dst_queue_family_index,
                                resource->last_stage_mask, current_state->usage.stage_mask);
                        }
                    } else if (ZEST__NOT_FLAGGED(render_graph->flags, zest_render_graph_force_on_graphics_queue)) {
                        //This resource already belongs to a queue which means that it's an imported resource
                        //If the render graph is on the graphics queue only then there's no need to acquire from a prior release.
                        ZEST_ASSERT(ZEST__FLAGGED(resource->flags, zest_resource_node_flag_imported));
                        dst_queue_family_index = current_state->queue_family_index;
                        if ( src_queue_family_index != VK_QUEUE_FAMILY_IGNORED) {
                            zest__add_image_barrier(resource, barriers, true,
								resource->current_access_mask, current_usage->access_mask,
								resource->current_layout, current_usage->image_layout, 
                                src_queue_family_index, dst_queue_family_index,
                                resource->last_stage_mask, current_state->usage.stage_mask);
                        }
                    }
                    bool needs_releasing = false;
                } else {
                    //There is a previous state, do we need to acquire the resource from a different queue?
                    zest_uint src_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
                    zest_uint dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
                    bool needs_acquiring = false;
                    if (prev_state && (prev_state->queue_family_index != current_state->queue_family_index || prev_state->was_released)) {
                        //The next state is in a different queue so the resource needs to be released to that queue
                        src_queue_family_index = prev_state->queue_family_index;
                        dst_queue_family_index = current_state->queue_family_index;
                        needs_acquiring = true;
                    }
                    if (needs_acquiring) {
                        zest_resource_usage_t *prev_usage = &prev_state->usage;
                        //Acquire the resource. No transitioning is done here, acquire only if needed
						zest__add_image_barrier(resource, barriers, true,
							prev_usage->access_mask, current_usage->access_mask,
							current_usage->image_layout, current_usage->image_layout, 
							src_queue_family_index, dst_queue_family_index,
							prev_state->usage.stage_mask, current_state->usage.stage_mask);
                    }
                }
                if (next_state) {
                    //If the resource has a state after this one then we can check if a barrier is needed to transition and/or
                    //release to another queue family
                    zest_uint src_queue_family_index = current_state->queue_family_index;
                    zest_uint dst_queue_family_index = next_state->queue_family_index;
                    zest_resource_usage_t *next_usage = &next_state->usage;
                    bool needs_releasing = false;
                    if (next_state && next_state->queue_family_index != current_state->queue_family_index) {
                        //The next state is in a different queue so the resource needs to be released to that queue
                        src_queue_family_index = current_state->queue_family_index;
                        dst_queue_family_index = next_state->queue_family_index;
                        needs_releasing = true;
                    }
                    if (needs_releasing || current_usage->image_layout != next_usage->image_layout ||
                        (current_usage->access_mask & zest_access_write_bits_general) &&
                        (next_usage->access_mask & zest_access_read_bits_general)) {
                        VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                        if (needs_releasing) {
                            current_state->was_released = true;
                        } else {
                            dst_stage = next_state->usage.stage_mask;
                            current_state->was_released = false;
                        }
						zest__add_image_barrier(resource, barriers, false,
							current_usage->access_mask, VK_ACCESS_NONE,
							current_usage->image_layout, next_usage->image_layout, 
							src_queue_family_index, dst_queue_family_index,
							current_state->usage.stage_mask, dst_stage);
                    }
                } else if (resource->flags & zest_resource_node_flag_imported
                    && current_state->queue_family_index != ZestDevice->transfer_queue_family_index
                    && ZEST__NOT_FLAGGED(render_graph->flags, zest_render_graph_force_on_graphics_queue)) {
                    //Maybe we add something for images here if it's needed.
                }
                prev_state = current_state;
                if (current_state->usage.access_mask & zest_access_render_pass_bits) {
                    exe_details->requires_dynamic_render_pass = true;
                }
            } else if(resource->type == zest_resource_type_buffer) {
                if (!prev_state) {
                    //This is the first state of the resource
                    //If there's no previous state then we need to see if a barrier is needed to transition from the resource
                    //start state. We put this in the acquire barrier as it needs to be put in place before the pass is executed.
                    zest_uint src_queue_family_index = resource->current_queue_family_index;
                    zest_uint dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
                    if (src_queue_family_index == VK_QUEUE_FAMILY_IGNORED) {
                        if ((resource->current_access_mask & zest_access_write_bits_general) &&
                            (current_usage->access_mask & zest_access_read_bits_general)) {
                            zest__add_memory_buffer_barrier(resource, barriers, true,
                                resource->current_access_mask, current_usage->access_mask,
                                src_queue_family_index, dst_queue_family_index,
                                resource->last_stage_mask, current_state->usage.stage_mask);
                        }
                    } else if (ZEST__NOT_FLAGGED(render_graph->flags, zest_render_graph_force_on_graphics_queue)) {
                        //This resource already belongs to a queue which means that it's an imported resource
                        //If the render graph is on the graphics queue only then there's no need to acquire from a prior release.
                        ZEST_ASSERT(ZEST__FLAGGED(resource->flags, zest_resource_node_flag_imported));
                        dst_queue_family_index = current_state->queue_family_index;
                        if (src_queue_family_index != VK_QUEUE_FAMILY_IGNORED) {
                            zest__add_memory_buffer_barrier(resource, barriers, true,
                                resource->current_access_mask, current_usage->access_mask,
                                src_queue_family_index, dst_queue_family_index,
                                resource->last_stage_mask, current_state->usage.stage_mask);
                        }
                    }
                    bool needs_releasing = false;
                } else {
                    //There is a previous state, do we need to acquire the resource from a different queue?
                    zest_uint src_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
                    zest_uint dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
                    bool needs_acquiring = false;
                    if (prev_state && (prev_state->queue_family_index != current_state->queue_family_index || prev_state->was_released)) {
                        //The next state is in a different queue so the resource needs to be released to that queue
                        src_queue_family_index = prev_state->queue_family_index;
                        dst_queue_family_index = current_state->queue_family_index;
                        needs_acquiring = true;
                    }
                    if (needs_acquiring) {
                        zest_resource_usage_t *prev_usage = &prev_state->usage;
                        //Acquire the resource. No transitioning is done here, acquire only if needed
                        zest__add_memory_buffer_barrier(resource, barriers, true,
                            prev_usage->access_mask, current_usage->access_mask,
                            src_queue_family_index, dst_queue_family_index,
                            prev_state->usage.stage_mask, current_state->usage.stage_mask);
                    }
                }
                if (next_state) {
                    //If the resource has a state after this one then we can check if a barrier is needed to transition and/or
                    //release to another queue family
                    zest_uint src_queue_family_index = current_state->queue_family_index;
                    zest_uint dst_queue_family_index = next_state->queue_family_index;
                    zest_resource_usage_t *next_usage = &next_state->usage;
                    bool needs_releasing = false;
                    if (next_state && next_state->queue_family_index != current_state->queue_family_index) {
                        //The next state is in a different queue so the resource needs to be released to that queue
                        src_queue_family_index = current_state->queue_family_index;
                        dst_queue_family_index = next_state->queue_family_index;
                        needs_releasing = true;
                    }
                    if (needs_releasing || 
                        (current_usage->access_mask & zest_access_write_bits_general) &&
                        (next_usage->access_mask & zest_access_read_bits_general)) {
                        VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                        if (needs_releasing) {
                            current_state->was_released = true;
                        } else {
                            dst_stage = next_state->usage.stage_mask;
                            current_state->was_released = false;
                        }
                        zest__add_memory_buffer_barrier(resource, barriers, false,
                            current_usage->access_mask, next_state->usage.access_mask,
                            src_queue_family_index, dst_queue_family_index,
                            current_state->usage.stage_mask, dst_stage);
                    }
                } else if (resource->flags & zest_resource_node_flag_release_after_use
                    && current_state->queue_family_index != ZestDevice->transfer_queue_family_index
                    && ZEST__NOT_FLAGGED(render_graph->flags, zest_render_graph_force_on_graphics_queue)) {
                    //Release the buffer so that it's ready to be acquired by any other queue in the next frame
                    //Release to the transfer queue by default (if it's not already on the transfer queue).
					zest__add_memory_buffer_barrier(resource, barriers, false,
						current_usage->access_mask, VK_ACCESS_NONE,
						current_state->queue_family_index, ZestDevice->transfer_queue_family_index,
						current_state->usage.stage_mask, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
                }
                prev_state = current_state;
            }
 
        }
    }

    zest_hash_map(zest_uint) attachment_idx;

    zest_uint k_global = 0;
    zest_uint current_batch_qfi;

    //Process_compiled_execution_order
    zest_vec_foreach(batch_index, render_graph->submissions) {
        zest_submission_batch_t *batch = &render_graph->submissions[batch_index];
        zest_uint batch_target_qfi = batch->queue_family_index;
        current_batch_qfi = batch_target_qfi;
        zest_vec_foreach(k, batch->pass_indices) {
            int current_pass_index = batch->pass_indices[k];
            zest_pass_node pass = &render_graph->passes[current_pass_index];
            zest_execution_details_t *exe_details = &render_graph->pass_exec_details_list[pass->execution_order_index];

            // Handle TOP_OF_PIPE if no actual prior stages generated by resource usage
            if (exe_details->barriers.overall_src_stage_mask_for_acquire_barriers == 0) {
                exe_details->barriers.overall_src_stage_mask_for_acquire_barriers = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            }
            // Handle BOTTOM_OF_PIPE if no actual subsequent stages (though less common for pre-pass barriers)
            if (exe_details->barriers.overall_dst_stage_mask_for_acquire_barriers == 0) {
                exe_details->barriers.overall_dst_stage_mask_for_acquire_barriers = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            }

            // Handle TOP_OF_PIPE if no actual prior stages generated by resource usage
            if (exe_details->barriers.overall_src_stage_mask_for_release_barriers == 0) {
                exe_details->barriers.overall_src_stage_mask_for_release_barriers = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            }
            // Handle BOTTOM_OF_PIPE if no actual subsequent stages (though less common for pre-pass barriers)
            if (exe_details->barriers.overall_dst_stage_mask_for_release_barriers == 0) {
                exe_details->barriers.overall_dst_stage_mask_for_release_barriers = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            }

            //If this pass is a render pass with an execution callback
            //Create_render_passes
            if (zest_vec_size(pass->execution_callbacks) && exe_details->requires_dynamic_render_pass) {
                zest_temp_attachment_info_t *color_attachment_info = 0;
                zest_temp_attachment_info_t depth_attachment_info = { 0 };
                zest_uint color_attachment_index = 0;
                //Determine attachments for color and depth (resolve can come later), first for outputs
                zest_map_foreach(o, pass->outputs) {
                    zest_resource_usage_t *output_usage = &pass->outputs.data[o];
                    zest_resource_node output_resource = pass->outputs.data[o].resource_node;
                    if (output_resource->type == zest_resource_type_image || output_resource->type == zest_resource_type_swap_chain_image) {
                        if (ZEST__FLAGGED(output_usage->access_mask, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) {
                            zest_temp_attachment_info_t color = { 0 };
                            color.resource_node = output_resource;
                            color.usage_info = output_usage;
                            color.attachment_slot = color_attachment_index++;
                            zest_vec_linear_push(allocator, color_attachment_info, color);
                        } else if (ZEST__FLAGGED(output_usage->access_mask, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) {
                            depth_attachment_info.resource_node = output_resource;
                            depth_attachment_info.usage_info = output_usage;
                            depth_attachment_info.attachment_slot = 0;
                            ZEST__FLAG(output_resource->flags, zest_resource_node_flag_used_in_output);
                        }
                    }
                }
                zest_uint input_attachment_index = 0;
                //Do the same for inputs
                zest_map_foreach(i, pass->inputs) {
                    zest_resource_usage_t *input_usage = &pass->inputs.data[i];
                    zest_resource_node input_resource = pass->inputs.data[i].resource_node;
                    if (input_resource->type == zest_resource_type_image) {
                        if (ZEST__FLAGGED(input_usage->access_mask, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)) {
                            zest_temp_attachment_info_t color = { 0 };
                            color.resource_node = input_resource;
                            color.usage_info = input_usage;
                            color.attachment_slot = input_attachment_index++;
                            zest_vec_linear_push(allocator, color_attachment_info, color);
                        } else if (ZEST__FLAGGED(input_usage->access_mask, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT) && ZEST__NOT_FLAGGED(input_resource->flags, zest_resource_node_flag_used_in_output)) {
                            zest_temp_attachment_info_t depth = { 0 };
                            depth.resource_node = input_resource;
                            depth.usage_info = input_usage;
                            depth.attachment_slot = 0;
                        }
                    }
                }
                VkAttachmentDescription *attachments = 0;
                attachment_idx attachment_indexes = { 0 };
                zest_resource_node *attachment_resource_nodes = 0;
                //Now that we've parsed the inputs and outputs, make the attachment descriptions
                zest_key render_pass_hash = 0;
                zest_vec_foreach(c, color_attachment_info) {
                    zest_resource_node node = color_attachment_info[c].resource_node;
                    zest_resource_usage_t *usage_info = color_attachment_info[c].usage_info;
                    if (!zest_map_valid_key(attachment_indexes, (zest_key)node)) {
                        VkAttachmentDescription attachment = { 0 };
                        attachment.format = node->image_buffer.format;
                        attachment.samples = node->image_desc.numSamples;
                        attachment.loadOp = usage_info->load_op;
                        attachment.storeOp = usage_info->store_op;
                        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                        attachment.initialLayout = node->current_layout;
                        attachment.finalLayout = zest__determine_final_layout(k_global + 1, node, usage_info);
                        if (attachment.finalLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                            ZEST__FLAG(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_used);
                        }
                        zest_vec_linear_push(allocator, attachments, attachment);
                        zest_map_insert_linear_key(allocator, attachment_indexes, (zest_key)node, (zest_vec_size(attachments) - 1));
                        zest_vec_linear_push(allocator, attachment_resource_nodes, node);
                        zest_vec_linear_push(allocator, exe_details->clear_values, usage_info->clear_value);
                        render_pass_hash += zest_Hash(&attachment, sizeof(VkAttachmentDescription), ZEST_HASH_SEED);
                    }
                }
                if (depth_attachment_info.resource_node) {
                    zest_resource_node node = depth_attachment_info.resource_node;
                    zest_resource_usage_t *usage_info = depth_attachment_info.usage_info;
                    if (!zest_map_valid_key(attachment_indexes, (zest_key)node)) {
                        VkAttachmentDescription attachment = { 0 };
                        attachment.format = node->image_buffer.format;
                        attachment.samples = node->image_desc.numSamples;
                        attachment.loadOp = usage_info->stencil_load_op;
                        attachment.storeOp = usage_info->stencil_store_op;
                        attachment.stencilLoadOp = usage_info->stencil_load_op;
                        attachment.stencilStoreOp = usage_info->stencil_store_op;
                        attachment.initialLayout = node->current_layout;
                        attachment.finalLayout = zest__determine_final_layout(k_global + 1, node, usage_info);
                        zest_vec_linear_push(allocator, attachments, attachment);
                        zest_map_insert_linear_key(allocator, attachment_indexes, (zest_key)node, (zest_vec_size(attachments) - 1));
                        zest_vec_linear_push(allocator, attachment_resource_nodes, node);
                        zest_vec_linear_push(allocator, exe_details->clear_values, usage_info->clear_value);
                        render_pass_hash += zest_Hash(&attachment, sizeof(VkAttachmentDescription), ZEST_HASH_SEED);
                    }
                }
                //Resolve will go here, not worried about it just yet.

                VkAttachmentReference *vk_color_refs = 0;
                VkAttachmentReference vk_depth_ref = { VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED };
                VkAttachmentReference *vk_input_refs = 0;       //Not used yet
                VkAttachmentReference *vk_resolve_refs = 0;		//Not used yet
                zest_vec_foreach(c, color_attachment_info) {
                    zest_resource_node node = color_attachment_info[c].resource_node;
                    zest_resource_usage_t *usage_info = color_attachment_info[c].usage_info;
                    VkAttachmentReference reference = { 0 };
                    reference.attachment = *zest_map_at_key(attachment_indexes, (zest_key)node);
                    reference.layout = usage_info->image_layout;
                    zest_vec_linear_push(allocator, vk_color_refs, reference);
                }
                if (depth_attachment_info.resource_node) {
                    zest_resource_node node = depth_attachment_info.resource_node;
                    zest_resource_usage_t *usage_info = depth_attachment_info.usage_info;
                    vk_depth_ref.attachment = *zest_map_at_key(attachment_indexes, (zest_key)node);
                    vk_depth_ref.layout = usage_info->image_layout;
                }
                //Not worried about input and resolve attachments just yet, will leave that for later

                //Construct the subpass:
                VkSubpassDescription subpass_desc = { 0 };
                subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                subpass_desc.colorAttachmentCount = zest_vec_size(vk_color_refs);
                subpass_desc.inputAttachmentCount = zest_vec_size(vk_input_refs);

                render_pass_hash += zest_Hash(&subpass_desc, sizeof(VkSubpassDescription), ZEST_HASH_SEED);

                subpass_desc.pColorAttachments = vk_color_refs;
                subpass_desc.pDepthStencilAttachment = (vk_depth_ref.attachment != VK_ATTACHMENT_UNUSED) ? &vk_depth_ref : VK_NULL_HANDLE;
                subpass_desc.pInputAttachments = vk_input_refs;
                subpass_desc.pResolveAttachments = VK_NULL_HANDLE;

                //Handle sub pass dependencies
                VkSubpassDependency initial_dependency = { 0 };
                initial_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
                initial_dependency.dstSubpass = 0;
                initial_dependency.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // A safe bet if barriers handle specifics
                initial_dependency.dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;   // Stages where attachments are first used
                initial_dependency.srcAccessMask = 0;
                initial_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT; // A broad mask - can be refined later?
                initial_dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
                render_pass_hash += zest_Hash(&initial_dependency, sizeof(VkSubpassDependency), ZEST_HASH_SEED);

                VkSubpassDependency final_dependency = { 0 };
                final_dependency.srcSubpass = 0;
                final_dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
                final_dependency.srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;		// Stages where attachments were last used
                final_dependency.dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;      // Stages of subsequent operations
                final_dependency.srcAccessMask = initial_dependency.dstAccessMask;      // Accesses this subpass performed
                final_dependency.dstAccessMask = 0; // Subsequent access will be handled by next pass's barriers/RP
                final_dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
                render_pass_hash += zest_Hash(&final_dependency, sizeof(VkSubpassDependency), ZEST_HASH_SEED);

                VkRenderPassCreateInfo render_pass_info = { 0 };
                render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
                render_pass_info.attachmentCount = zest_vec_size(attachments);
                render_pass_info.subpassCount = 1;
                render_pass_info.dependencyCount = 2;
                render_pass_hash += zest_Hash(&render_pass_info, sizeof(VkRenderPassCreateInfo), ZEST_HASH_SEED);

                render_pass_info.pAttachments = attachments;
                render_pass_info.pSubpasses = &subpass_desc;
                render_pass_info.pDependencies = (VkSubpassDependency[]){ initial_dependency, final_dependency };
                if (!zest_map_valid_key(ZestRenderer->render_passes, render_pass_hash)) {
                    ZEST_VK_CHECK_RESULT(vkCreateRenderPass(ZestDevice->logical_device, &render_pass_info, &ZestDevice->allocation_callbacks, &exe_details->render_pass));
                    zest_map_insert_key(ZestRenderer->render_passes, render_pass_hash, exe_details->render_pass);
                } else {
                    exe_details->render_pass = *zest_map_at_key(ZestRenderer->render_passes, render_pass_hash);
                }

                //Create_frame_buffers
                VkImageView *image_views = 0;
                zest_map_foreach(i, attachment_indexes) {
                    zest_resource_node node = attachment_resource_nodes[i];
                    zest_vec_linear_push(allocator, image_views, node->image_buffer.base_view);
                }
                VkFramebufferCreateInfo fb_create_info = { 0 };
                fb_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                fb_create_info.renderPass = exe_details->render_pass;
                fb_create_info.attachmentCount = zest_vec_size(image_views);
                fb_create_info.pAttachments = image_views;
                if (zest_vec_size(color_attachment_info)) {
                    fb_create_info.width = color_attachment_info[0].resource_node->image_desc.width;
                    fb_create_info.height = color_attachment_info[0].resource_node->image_desc.height;
                } else {
                    fb_create_info.width = depth_attachment_info.resource_node->image_desc.width;
                    fb_create_info.height = depth_attachment_info.resource_node->image_desc.height;
                }
                exe_details->render_area.extent.width = fb_create_info.width;
                exe_details->render_area.extent.height = fb_create_info.height;
                exe_details->render_area.offset.x = 0;
                exe_details->render_area.offset.y = 0;
                fb_create_info.layers = 1;
                ZEST_VK_CHECK_RESULT(vkCreateFramebuffer(ZestDevice->logical_device, &fb_create_info, &ZestDevice->allocation_callbacks, &exe_details->frame_buffer));
                zest_vec_push(ZestRenderer->old_frame_buffers[ZEST_FIF], exe_details->frame_buffer);
            }
			k_global++;
        }   //Passes within batch loop
        
    }   //Batch loop

    if (ZEST__FLAGGED(render_graph->flags, zest_render_graph_expecting_swap_chain_usage)) {
        ZEST_ASSERT(ZEST__FLAGGED(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_used));    
        //Error: the render graph is trying to render to the screen but no swap chain image was used!
        //Make sure that you call zest_ImportSwapChainResource and zest_ConnectSwapChainOutput in your render graph setup.
    }
	ZEST__FLAG(render_graph->flags, zest_render_graph_is_compiled);  

    zest__execute_render_graph();
    return render_graph;
}

VkImageAspectFlags zest__determine_aspect_flag(VkFormat format) {
    switch (format) {
        // Depth-Only Formats
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32: // D24_UNORM component, X8 is undefined
    case VK_FORMAT_D32_SFLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
        // Stencil-Only Formats
    case VK_FORMAT_S8_UINT:
        return VK_IMAGE_ASPECT_STENCIL_BIT;
        // Combined Depth/Stencil Formats
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        // Default for all other formats (assumed to be color formats)
        // This includes various VK_FORMAT_R*, VK_FORMAT_G*, VK_FORMAT_B*,
        // VK_FORMAT_RG*, VK_FORMAT_RGB*, VK_FORMAT_BGR*,
        // VK_FORMAT_RGBA*, VK_FORMAT_BGRA*,
        // _UNORM, _SNORM, _SRGB, _SFLOAT, _UINT, _SINT, etc.
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

VkImageLayout zest__determine_final_layout(int start_from_idx, zest_resource_node node, zest_resource_usage_t *current_usage) {
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    ZEST_CHECK_HANDLE(render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    for (zest_uint idx = start_from_idx; idx < zest_vec_size(render_graph->compiled_execution_order); ++idx) {
        int pass_index = render_graph->compiled_execution_order[idx];
        zest_pass_node pass = &render_graph->passes[pass_index];
        zest_map_foreach(i, pass->inputs) {
            if (node == pass->inputs.data[i].resource_node) {
                return pass->inputs.data[i].image_layout;
            }
        }
        zest_map_foreach(o, pass->outputs) {
            if (node == pass->outputs.data[o].resource_node) {
                return pass->outputs.data[o].image_layout;
            }
        }
    }
    if (node->type == zest_resource_type_swap_chain_image) {
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    } else if (ZEST__FLAGGED(node->flags, zest_resource_node_flag_imported)) {
        node->final_layout; 
    } 
    if (current_usage->store_op == VK_ATTACHMENT_STORE_OP_DONT_CARE) {
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void zest__deferr_buffer_destruction(zest_buffer storage_buffer) {
    zest_vec_push(ZestRenderer->deferred_resource_freeing_list.buffers[ZEST_FIF], storage_buffer);
}

void zest__deferr_image_destruction(zest_image_buffer_t *image_buffer) {
    zest_vec_push(ZestRenderer->deferred_resource_freeing_list.images[ZEST_FIF], *image_buffer);
}

void zest__execute_render_graph() {
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    ZEST_CHECK_HANDLE(render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zloc_linear_allocator_t *allocator = ZestRenderer->render_graph_allocator;
    zest_map_queue_value queues = { 0 };
    zest_vec_foreach(batch_index, render_graph->submissions) {
        zest_submission_batch_t *batch = &render_graph->submissions[batch_index];
        VkCommandBuffer command_buffer;

        ZEST_ASSERT(zest_vec_size(batch->pass_indices));    //A batch was created without any pass indices. Bug in the Compile stage!

        VkPipelineStageFlags timeline_wait_stage;

        zest_pass_node first_pass_in_batch = &render_graph->passes[batch->pass_indices[0]];
        // 1. acquire an appropriate command buffer
        switch (render_graph->passes[batch->pass_indices[0]].queue_type) { 
        case zest_queue_graphics:
            command_buffer = zest__acquire_graphics_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
			vkResetCommandBuffer(command_buffer, 0);
			zest_vec_push(ZestRenderer->used_graphics_command_buffers[ZEST_FIF], command_buffer);
            timeline_wait_stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
        case zest_queue_compute:
            command_buffer = zest__acquire_compute_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
			vkResetCommandBuffer(command_buffer, 0);
			zest_vec_push(ZestRenderer->used_compute_command_buffers[ZEST_FIF], command_buffer);
            timeline_wait_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
        case zest_queue_transfer:
            command_buffer = zest__acquire_transfer_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
			vkResetCommandBuffer(command_buffer, 0);
			zest_vec_push(ZestRenderer->used_transfer_command_buffers[ZEST_FIF], command_buffer);
            timeline_wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        default:
            ZEST_ASSERT(0); //Unknown queue type for batch. Corrupt memory perhaps?!
        }

        VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(command_buffer, &begin_info);
		render_graph->context.command_buffer = command_buffer;

        zest_vec_foreach(i, batch->pass_indices) {
            zest_uint pass_index = batch->pass_indices[i];
            zest_pass_node pass = &render_graph->passes[pass_index];
            zest_execution_details_t *exe_details = &render_graph->pass_exec_details_list[pass->execution_order_index];

            //Batch execute acquire barriers for images and buffers
            if (zest_vec_size(exe_details->barriers.acquire_buffer_barriers) > 0 ||
                zest_vec_size(exe_details->barriers.acquire_image_barriers) > 0) {
                zest_uint buffer_count = zest_vec_size(exe_details->barriers.acquire_buffer_barriers);
                zest_uint image_count = zest_vec_size(exe_details->barriers.acquire_image_barriers);
                vkCmdPipelineBarrier(
                    command_buffer,
                    exe_details->barriers.overall_src_stage_mask_for_acquire_barriers, // Single mask for all barriers in this batch
                    exe_details->barriers.overall_dst_stage_mask_for_acquire_barriers, // Single mask for all barriers in this batch
                    0,
                    0, NULL,
                    buffer_count,
                    exe_details->barriers.acquire_buffer_barriers,
                    image_count,
                    exe_details->barriers.acquire_image_barriers
                );
            }

            render_graph->context.render_pass = exe_details->render_pass;

            //Begin the render pass if the pass has one
            if (exe_details->render_pass != VK_NULL_HANDLE) {
                VkRenderPassBeginInfo render_pass_info = { 0 };
                render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                render_pass_info.renderPass = exe_details->render_pass;
                render_pass_info.framebuffer = exe_details->frame_buffer;
                render_pass_info.renderArea = exe_details->render_area;

                zest_uint clear_size = zest_vec_size(exe_details->clear_values);
                render_pass_info.clearValueCount = zest_vec_size(exe_details->clear_values);
                render_pass_info.pClearValues = exe_details->clear_values;
                vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
            }

            render_graph->context.pass_node = pass;

            //Execute the callbacks in the pass
            zest_vec_foreach(callback_index, pass->execution_callbacks) {
                zest_pass_execution_callback_t *callback_data = &pass->execution_callbacks[callback_index];
                callback_data->execution_callback(command_buffer, &render_graph->context, callback_data->user_data);
            }

            if (exe_details->render_pass != VK_NULL_HANDLE) {
                vkCmdEndRenderPass(command_buffer);
            }

            //Batch execute release barriers for images and buffers
            if (zest_vec_size(exe_details->barriers.release_buffer_barriers) > 0 ||
                zest_vec_size(exe_details->barriers.release_image_barriers) > 0) {
                vkCmdPipelineBarrier(
                    command_buffer,
                    exe_details->barriers.overall_src_stage_mask_for_release_barriers, // Single mask for all barriers in this batch
                    exe_details->barriers.overall_dst_stage_mask_for_release_barriers, // Single mask for all barriers in this batch
                    0,
                    0, NULL,
                    zest_vec_size(exe_details->barriers.release_buffer_barriers),
                    exe_details->barriers.release_buffer_barriers,
                    zest_vec_size(exe_details->barriers.release_image_barriers),
                    exe_details->barriers.release_image_barriers
                );
            }
            //End pass
        }
        vkEndCommandBuffer(command_buffer);

        VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer; // Use the CB we just recorded

        // Set signal semaphores for this batch
        zest_queue queue = batch->queue;

        zest_uint queue_fif = queue->fif;
        VkSemaphore timeline_semaphore_for_this_fif = queue->semaphore[queue_fif];

        //Increment the queue count for the timeline semaphores if the queue hasn't been used yet this render graph
        zest_u64 wait_value = 0;
        zest_uint wait_index = queue->fif;
        if (zest_map_valid_key(queues, (zest_key)queue)) {
            //Intraframe timeline semaphore required. This will happen if there are more than one batch for a queue family
            wait_value = *zest_map_at_key(queues, (zest_key)queue)->signal_value;
        } else {
            //Interframe timeline semaphore required. Has to connect to the semaphore value in the previous frame that this
            //queue was ran.
            wait_index = (queue_fif + 1) % ZEST_MAX_FIF;
            wait_value = queue->current_count[wait_index];
        }

        zest_u64 signal_value = wait_value + 1;
        queue->signal_value = signal_value;
        queue->current_count[queue_fif] = signal_value;

		zest_map_insert_linear_key(allocator, queues, (zest_key)queue, queue);

        //We need to mix the binary semaphores in the batches with the timeline semaphores in the queue,
        //so use these arrays for that. Because they're mixed we also need wait values for the binary values
        //even if they're not used (they're set to 0)
        VkSemaphore *wait_semaphores = 0;
        VkPipelineStageFlags *wait_stages = 0;
        VkSemaphore *signal_semaphores = 0;
        zest_u64 *wait_values = 0;
        zest_u64 *signal_values = 0;

        zest_vec_linear_push(allocator, signal_semaphores, timeline_semaphore_for_this_fif);
        zest_vec_linear_push(allocator, signal_values, signal_value);

        VkTimelineSemaphoreSubmitInfo timeline_info = { 0 };
        timeline_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;

        //If there's been enough frames in flight processed then add a timeline wait semphore that waits on the
        //previous frame in flight from max frames in flight ago.
        if (wait_value > 0) {
            zest_vec_linear_push(allocator, wait_semaphores, queue->semaphore[wait_index]);
            zest_vec_linear_push(allocator, wait_values, wait_value);
            zest_vec_linear_push(allocator, wait_stages, batch->timeline_wait_stage);
        }

        zest_vec_foreach(i, batch->wait_semaphores) {
            zest_vec_linear_push(allocator, wait_semaphores, batch->wait_semaphores[i]);
            zest_vec_linear_push(allocator, wait_stages, batch->wait_dst_stage_masks[i]);
			zest_vec_linear_push(allocator, wait_values, 0);
        }

        if (batch_index == 0 && zest_vec_size(render_graph->wait_on_timelines)) {
            zest_vec_foreach(timeline_index, render_graph->wait_on_timelines) {
                zest_execution_timeline timeline = render_graph->wait_on_timelines[timeline_index];
                if (timeline->current_value > 0) {
					zest_vec_linear_push(allocator, wait_semaphores, timeline->semaphore);
					zest_vec_linear_push(allocator, wait_stages, timeline_wait_stage);
					zest_vec_linear_push(allocator, wait_values, timeline->current_value);
                }
            }
        }

		timeline_info.waitSemaphoreValueCount = zest_vec_size(wait_values);
		timeline_info.pWaitSemaphoreValues = wait_values;

        // Set wait semaphores for this batch
        if (wait_semaphores) {
			submit_info.waitSemaphoreCount = zest_vec_size(wait_semaphores);
			submit_info.pWaitSemaphores = wait_semaphores;
			submit_info.pWaitDstStageMask = wait_stages; // Needs to be correctly set
        }

        VkSemaphore *batch_signal_semaphores = 0;
        zest_vec_foreach(signal_index, batch->signal_semaphores) {
            zest_vec_linear_push(allocator, batch_signal_semaphores, batch->signal_semaphores[signal_index]);
        }

        //If this is the last batch then add the fence that tells the cpu to wait each frame
        VkFence submit_fence = VK_NULL_HANDLE;
        if (batch_index == zest_vec_size(render_graph->submissions) - 1) { 
            submit_fence = ZestRenderer->fif_fence[ZEST_FIF];

			if (zest_vec_size(render_graph->signal_timelines)) {
				zest_vec_foreach(timeline_index, render_graph->signal_timelines) {
					zest_execution_timeline timeline = render_graph->signal_timelines[timeline_index];
					timeline->current_value += 1;
					zest_vec_linear_push(allocator, signal_semaphores, timeline->semaphore);
					zest_vec_linear_push(allocator, signal_values, timeline->current_value);
				}
			}
        }

        //If the batch is signalling then add that to the signal semaphores along with the queue timeline semaphore
        zest_vec_foreach(signal_index, batch_signal_semaphores) {
			zest_vec_linear_push(allocator, signal_semaphores, batch_signal_semaphores[signal_index]);
			zest_vec_linear_push(allocator, signal_values, 0);
        }

        //Finish the rest of the queue submit info and submit the queue
		timeline_info.signalSemaphoreValueCount = zest_vec_size(signal_values);
		timeline_info.pSignalSemaphoreValues = signal_values;
		submit_info.signalSemaphoreCount = zest_vec_size(signal_semaphores);
		submit_info.pSignalSemaphores = signal_semaphores;
        submit_info.pNext = &timeline_info;

        ZEST_VK_CHECK_RESULT(vkQueueSubmit(batch->queue->vk_queue, 1, &submit_info, submit_fence));
		ZEST__FLAG(ZestRenderer->flags, zest_renderer_flag_work_was_submitted);

        batch->final_wait_semaphores = wait_semaphores;
        batch->final_signal_semaphores = signal_semaphores;
        batch->wait_stages = wait_stages;
        batch->wait_values = wait_values;
        batch->signal_values = signal_values;
    }

    zest_map_foreach(i, queues) {
        zest_queue queue = queues.data[i];
        queue->fif = (queue->fif + 1) % ZEST_MAX_FIF;
    }

	zest_vec_foreach(index, render_graph->resources) {
		zest_resource_node resource = &render_graph->resources[index];
		if (ZEST__FLAGGED(resource->flags, zest_resource_node_flag_transient)) {
			if (resource->storage_buffer) {
				zest__deferr_buffer_destruction(resource->storage_buffer);
			} else if(resource->image_buffer.image) {
				zest__deferr_image_destruction(&resource->image_buffer);
			}
		}
	}
    ZEST__FLAG(render_graph->flags, zest_render_graph_is_executed);
    ZestRenderer->current_render_graph = 0;
	ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_building_render_graph);  
}

bool zest_RenderGraphWasExecuted(zest_render_graph render_graph) {
    return ZEST__FLAGGED(render_graph->flags, zest_render_graph_is_executed);
}

const char *zest__vulkan_image_layout_to_string(VkImageLayout layout) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED: return "UNDEFINED"; break;
	case VK_IMAGE_LAYOUT_GENERAL : return "GENERAL"; break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : return "COLOR_ATTACHMENT_OPTIMAL"; break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : return "DEPTH_STENCIL_ATTACHMENT_OPTIMAL"; break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : return "DEPTH_STENCIL_READ_ONLY_OPTIMAL"; break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : return "SHADER_READ_ONLY_OPTIMAL"; break;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : return "TRANSFER_SRC_OPTIMAL"; break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : return "TRANSFER_DST_OPTIMAL"; break;
	case VK_IMAGE_LAYOUT_PREINITIALIZED : return "PREINITIALIZED"; break;
	case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL : return "DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL"; break;
	case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL : return "DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL"; break;
	case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : return "DEPTH_ATTACHMENT_OPTIMAL"; break;
	case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL : return "DEPTH_READ_ONLY_OPTIMAL"; break;
	case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL : return "STENCIL_ATTACHMENT_OPTIMAL"; break;
	case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL : return "STENCIL_READ_ONLY_OPTIMAL"; break;
	case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL : return "READ_ONLY_OPTIMAL"; break;
	case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : return "ATTACHMENT_OPTIMAL"; break;
	default: return "Unknown Layout";
    }
}

zest_text_t zest__vulkan_access_flags_to_string(VkAccessFlags flags) {
    zest_text_t string = { 0 };
    if (!flags) {
		zest_AppendTextf(&string, "%s", "NONE");
        return string;
    }
    zloc_size flags_field = flags;
    while (flags_field) {
        if (zest_TextSize(&string)) {
            zest_AppendTextf(&string, ", ");
        }
        zest_uint index = zloc__scan_forward(flags_field);
        switch (1ull << index) {
        case VK_ACCESS_INDIRECT_COMMAND_READ_BIT: zest_AppendTextf(&string, "%s", "INDIRECT_COMMAND_READ_BIT"); break;
		case VK_ACCESS_INDEX_READ_BIT : zest_AppendTextf(&string, "%s", "INDEX_READ_BIT"); break;
		case VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT : zest_AppendTextf(&string, "%s", "VERTEX_ATTRIBUTE_READ_BIT"); break;
		case VK_ACCESS_UNIFORM_READ_BIT : zest_AppendTextf(&string, "%s", "UNIFORM_READ_BIT"); break;
		case VK_ACCESS_INPUT_ATTACHMENT_READ_BIT : zest_AppendTextf(&string, "%s", "INPUT_ATTACHMENT_READ_BIT"); break;
		case VK_ACCESS_SHADER_READ_BIT : zest_AppendTextf(&string, "%s", "SHADER_READ_BIT"); break;
		case VK_ACCESS_SHADER_WRITE_BIT : zest_AppendTextf(&string, "%s", "SHADER_WRITE_BIT"); break;
		case VK_ACCESS_COLOR_ATTACHMENT_READ_BIT : zest_AppendTextf(&string, "%s", "COLOR_ATTACHMENT_READ_BIT"); break;
		case VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : zest_AppendTextf(&string, "%s", "COLOR_ATTACHMENT_WRITE_BIT"); break;
		case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT : zest_AppendTextf(&string, "%s", "DEPTH_STENCIL_ATTACHMENT_READ_BIT"); break;
		case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : zest_AppendTextf(&string, "%s", "DEPTH_STENCIL_ATTACHMENT_WRITE_BIT"); break;
		case VK_ACCESS_TRANSFER_READ_BIT : zest_AppendTextf(&string, "%s", "TRANSFER_READ_BIT"); break;
		case VK_ACCESS_TRANSFER_WRITE_BIT : zest_AppendTextf(&string, "%s", "TRANSFER_WRITE_BIT"); break;
		case VK_ACCESS_HOST_READ_BIT : zest_AppendTextf(&string, "%s", "HOST_READ_BIT"); break;
		case VK_ACCESS_HOST_WRITE_BIT : zest_AppendTextf(&string, "%s", "HOST_WRITE_BIT"); break;
		case VK_ACCESS_MEMORY_READ_BIT : zest_AppendTextf(&string, "%s", "MEMORY_READ_BIT"); break;
		case VK_ACCESS_MEMORY_WRITE_BIT : zest_AppendTextf(&string, "%s", "MEMORY_WRITE_BIT"); break;
		case VK_ACCESS_NONE : zest_AppendTextf(&string, "%s", "NONE"); break;
		default: zest_AppendTextf(&string, "%s", "Unknown Access Flags"); break;
        }
        flags_field &= ~(1ull << index);
    }
    return string;
}

zest_text_t zest__vulkan_pipeline_stage_flags_to_string(VkPipelineStageFlags flags) {
    zest_text_t string = { 0 };
    if (!flags) {
		zest_AppendTextf(&string, "%s", "NONE");
        return string;
    }
    zloc_size flags_field = flags;
    while (flags_field) {
        if (zest_TextSize(&string)) {
            zest_AppendTextf(&string, ", ");
        }
        zest_uint index = zloc__scan_forward(flags_field);
        switch (1ull << index) {
        case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT: zest_AppendTextf(&string, "%s", "TOP_OF_PIPE_BIT"); break;
        case VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT: zest_AppendTextf(&string, "%s", "DRAW_INDIRECT_BIT"); break;
        case VK_PIPELINE_STAGE_VERTEX_INPUT_BIT: zest_AppendTextf(&string, "%s", "VERTEX_INPUT_BIT"); break;
        case VK_PIPELINE_STAGE_VERTEX_SHADER_BIT: zest_AppendTextf(&string, "%s", "VERTEX_SHADER_BIT"); break;
        case VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT: zest_AppendTextf(&string, "%s", "TESSELLATION_CONTROL_SHADER_BIT"); break;
        case VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT: zest_AppendTextf(&string, "%s", "TESSELLATION_EVALUATION_SHADER_BIT"); break;
        case VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT: zest_AppendTextf(&string, "%s", "GEOMETRY_SHADER_BIT"); break;
        case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT: zest_AppendTextf(&string, "%s", "FRAGMENT_SHADER_BIT"); break;
        case VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT: zest_AppendTextf(&string, "%s", "EARLY_FRAGMENT_TESTS_BIT"); break;
        case VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT: zest_AppendTextf(&string, "%s", "LATE_FRAGMENT_TESTS_BIT"); break;
        case VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT: zest_AppendTextf(&string, "%s", "COLOR_ATTACHMENT_OUTPUT_BIT"); break;
        case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT: zest_AppendTextf(&string, "%s", "COMPUTE_SHADER_BIT"); break;
        case VK_PIPELINE_STAGE_TRANSFER_BIT: zest_AppendTextf(&string, "%s", "TRANSFER_BIT"); break;
        case VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT: zest_AppendTextf(&string, "%s", "BOTTOM_OF_PIPE_BIT"); break;
        case VK_PIPELINE_STAGE_HOST_BIT: zest_AppendTextf(&string, "%s", "HOST_BIT"); break;
        case VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT: zest_AppendTextf(&string, "%s", "ALL_GRAPHICS_BIT"); break;
        case VK_PIPELINE_STAGE_ALL_COMMANDS_BIT: zest_AppendTextf(&string, "%s", "ALL_COMMANDS_BIT"); break;
        case VK_PIPELINE_STAGE_NONE: zest_AppendTextf(&string, "%s", "NONE"); break;
        default: zest_AppendTextf(&string, "%s", "Unknown Pipeline Stage"); break;
        }
        flags_field &= ~(1ull << index);
    }
    return string;
}

zest_text_t zest__vulkan_queue_flags_to_string(VkQueueFlags flags) {
    zest_text_t string = { 0 };
    if (!flags) {
        zest_AppendTextf(&string, "%s", "NONE");
        return string;
    }
    zloc_size flags_field = flags;
    while (flags_field) {
        if (zest_TextSize(&string)) {
            zest_AppendTextf(&string, ", ");
        }
        zest_uint index = zloc__scan_forward(flags_field);
        switch (1ull << index) {
        case VK_QUEUE_GRAPHICS_BIT: zest_AppendTextf(&string, "%s", "GRAPHICS"); break;
        case VK_QUEUE_COMPUTE_BIT: zest_AppendTextf(&string, "%s", "COMPUTE"); break;
        case VK_QUEUE_TRANSFER_BIT: zest_AppendTextf(&string, "%s", "TRANSFER"); break;
        case VK_QUEUE_SPARSE_BINDING_BIT: zest_AppendTextf(&string, "%s", "SPARSE BINDING"); break;
        case VK_QUEUE_PROTECTED_BIT: zest_AppendTextf(&string, "%s", "PROTECTED"); break;
        case VK_QUEUE_VIDEO_DECODE_BIT_KHR: zest_AppendTextf(&string, "%s", "VIDEO_DECODE"); break;
        case VK_QUEUE_VIDEO_ENCODE_BIT_KHR: zest_AppendTextf(&string, "%s", "VIDEO_ENCODE"); break;
        case VK_QUEUE_OPTICAL_FLOW_BIT_NV: zest_AppendTextf(&string, "%s", "OPTICAL_FLOW"); break;
        case VK_QUEUE_FLAG_BITS_MAX_ENUM: zest_AppendTextf(&string, "%s", "MAX"); break;
        default: zest_AppendTextf(&string, "%s", "Unknown Queue Flag"); break;
        }
        flags_field &= ~(1ull << index);
    }
    return string;
}

void zest_PrintCompiledRenderGraph(zest_render_graph render_graph) {
    ZEST_CHECK_HANDLE(render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    ZEST_PRINT("--- Render Graph Execution Plan ---");
    if (!ZEST_VALID_HANDLE(render_graph)) {
        ZEST_PRINT("Render graph handle is NULL.");
        return;
    }

    if (!ZEST__FLAGGED(render_graph->flags, zest_render_graph_is_compiled)) {
        ZEST_PRINT("Render graph is not in a compiled state");
        return;
    }

    ZEST_PRINT("Resource List: Total Resources: %u\n", zest_vec_size(render_graph->resources));

    zest_vec_foreach(resource_index, render_graph->resources) {
        zest_resource_node resource = &render_graph->resources[resource_index];
        if (resource->type == zest_resource_type_buffer) {
            ZEST_PRINT("Buffer: %s - VkBuffer: %p, Offset: %zu, Size: %zu", resource->name, resource->storage_buffer->memory_pool->buffer, resource->storage_buffer->memory_offset, resource->storage_buffer->size);
            zest_buffer buffer = resource->storage_buffer;
			zest_text_t access_mask = zest__vulkan_access_flags_to_string(buffer->last_access_mask);
			zest_text_t pipeline_stage = zest__vulkan_pipeline_stage_flags_to_string(buffer->last_stage_mask);
            ZEST_PRINT("  -Initial State: Owned by [%s], Last Access: [%s], Last Stage: [%s]",
                *zest_map_at_key(ZestDevice->queue_names, buffer->owner_queue_family),
                access_mask.str,
                pipeline_stage.str);
			zest_FreeText(&access_mask);
			zest_FreeText(&pipeline_stage);
            zest_vec_foreach(i, resource->journey) {
                zest_resource_state_t *state = &resource->journey[i];
				zest_text_t access_mask = zest__vulkan_access_flags_to_string(state->usage.access_mask);
				zest_text_t pipeline_stage = zest__vulkan_pipeline_stage_flags_to_string(state->usage.stage_mask);
                ZEST_PRINT("  -Used in pass [%s on %s]: Access: [%s], Stage: [%s]", 
                    render_graph->passes[state->pass_index].name, 
                    *zest_map_at_key(ZestDevice->queue_names, state->queue_family_index),
                    access_mask.str,
                    pipeline_stage.str);
                zest_FreeText(&access_mask);
                zest_FreeText(&pipeline_stage);
            }
        } else if (resource->type == zest_resource_type_image) {
            ZEST_PRINT("Image: %s - VkImage: %p, Offset: %zu, Size: %zu", 
                resource->name, resource->image_buffer.image, resource->image_buffer.buffer->memory_offset, 
                resource->image_buffer.buffer->size);
            zest_vec_foreach(i, resource->journey) {
                zest_resource_state_t *state = &resource->journey[i];
                zest_text_t access_mask = zest__vulkan_access_flags_to_string(state->usage.access_mask);
                zest_text_t pipeline_stage = zest__vulkan_pipeline_stage_flags_to_string(state->usage.stage_mask);
                ZEST_PRINT("  -Used in pass [%s on %s]: Access: [%s], Stage: [%s], Layout: [%s]",
                    render_graph->passes[state->pass_index].name,
                    *zest_map_at_key(ZestDevice->queue_names, state->queue_family_index),
                    access_mask.str,
                    pipeline_stage.str,
                    zest__vulkan_image_layout_to_string(state->usage.image_layout));
                zest_FreeText(&access_mask);
                zest_FreeText(&pipeline_stage);
            }
        } else if (resource->type == zest_resource_type_swap_chain_image) {
            ZEST_PRINT("Image: %s - VkImage: %p", 
                resource->name, resource->image_buffer.image);
            zest_vec_foreach(i, resource->journey) {
                zest_resource_state_t *state = &resource->journey[i];
                zest_text_t access_mask = zest__vulkan_access_flags_to_string(state->usage.access_mask);
                zest_text_t pipeline_stage = zest__vulkan_pipeline_stage_flags_to_string(state->usage.stage_mask);
                ZEST_PRINT("  -Used in pass [%s on %s]: Access: [%s], Stage: [%s], Layout: [%s]",
                    render_graph->passes[state->pass_index].name,
                    *zest_map_at_key(ZestDevice->queue_names, state->queue_family_index),
                    access_mask.str,
                    pipeline_stage.str,
                    zest__vulkan_image_layout_to_string(state->usage.image_layout));
                zest_FreeText(&access_mask);
                zest_FreeText(&pipeline_stage);
            }
        }
    }

    ZEST_PRINT("");
    ZEST_PRINT("Number of Submission Batches: %u\n", zest_vec_size(render_graph->submissions));

    zest_vec_foreach(batch_index, render_graph->submissions) {
        zest_submission_batch_t *batch = &render_graph->submissions[batch_index];
        ZEST_PRINT("Batch %i:", batch_index);
        if (zest_map_valid_key(ZestDevice->queue_names, batch->queue_family_index)) {
            ZEST_PRINT("  Target Queue Family: %s - index: %u (VkQueue: %p)", *zest_map_at_key(ZestDevice->queue_names, batch->queue_family_index), batch->queue_family_index, (void *)batch->queue);
        } else {
            ZEST_PRINT("  Target Queue Family: %s - index: %u (VkQueue: %p)", "Ignored", batch->queue_family_index, (void *)batch->queue);
        }

        // --- Print Wait Semaphores for the Batch ---
        // (Your batch struct needs to store enough info for this, e.g., an array of wait semaphores and stages)
        // For simplicity, assuming single wait_on_batch_semaphore for now, and you'd identify if it's external
        if (batch->final_wait_semaphores) {
            // This stage should ideally be stored with the batch submission info by EndRenderGraph
            ZEST_PRINT("  Waits on the following Semaphores:");
            zest_vec_foreach(semaphore_index, batch->final_wait_semaphores) {
                zest_text_t pipeline_stages = zest__vulkan_pipeline_stage_flags_to_string(batch->wait_stages[semaphore_index]);
                if (zest_vec_size(batch->wait_values) && batch->wait_values[semaphore_index] > 0) {
                    ZEST_PRINT("     Timeline Semaphore: %p, Value: %zu at Stage: %s", (void *)batch->final_wait_semaphores[semaphore_index], batch->wait_values[semaphore_index], pipeline_stages.str);
                } else {
                    ZEST_PRINT("     Binary Semaphore:   %p at Stage: %s", (void *)batch->final_wait_semaphores[semaphore_index], pipeline_stages.str);
                }
                zest_FreeText(&pipeline_stages);
            }
        } else {
            ZEST_PRINT("  Does not wait on any semaphores.");
        }

        ZEST_PRINT("  Passes in this batch:");
        zest_vec_foreach(batch_pass_index, batch->pass_indices) {
            int pass_index = batch->pass_indices[batch_pass_index];
            zest_pass_node pass_node = &render_graph->passes[pass_index];
            zest_execution_details_t *exe_details = &render_graph->pass_exec_details_list[pass_node->execution_order_index];

            // Find k_global (execution order index) for this pass_index
            zest_uint compiled_order_index = 0; // You'd need to search compiled_execution_order for pass_index
            for (compiled_order_index = 0; compiled_order_index < zest_vec_size(render_graph->compiled_execution_order); ++compiled_order_index) {
                if (render_graph->compiled_execution_order[compiled_order_index] == (zest_uint)pass_index) break;
            }

            ZEST_PRINT("    Pass [%d] (Execution Index %u): \"%s\" (QueueType: %d)",
                pass_index, compiled_order_index, pass_node->name, pass_node->queue_type);

            if (zest_vec_size(exe_details->barriers.acquire_buffer_barriers) > 0 ||
                zest_vec_size(exe_details->barriers.acquire_image_barriers) > 0) {
				zest_text_t overal_src_pipeline_stages = zest__vulkan_pipeline_stage_flags_to_string(exe_details->barriers.overall_src_stage_mask_for_acquire_barriers);
				zest_text_t overal_dst_pipeline_stages = zest__vulkan_pipeline_stage_flags_to_string(exe_details->barriers.overall_dst_stage_mask_for_acquire_barriers);
                ZEST_PRINT("      Acquire Barriers (Overall Pipeline Src Stages: %s, Dst Stages: %s):",
                    overal_src_pipeline_stages.str,
                    overal_dst_pipeline_stages.str);
                zest_FreeText(&overal_src_pipeline_stages);
                zest_FreeText(&overal_dst_pipeline_stages);

                ZEST_PRINT("        Images:");
                zest_vec_foreach(barrier_index, exe_details->barriers.acquire_image_barriers) {
                    VkImageMemoryBarrier *imb = &exe_details->barriers.acquire_image_barriers[barrier_index];
                    zest_resource_node image_resource = exe_details->barriers.acquire_image_barrier_nodes[barrier_index];
                    zest_text_t src_access_mask = zest__vulkan_access_flags_to_string(imb->srcAccessMask);
                    zest_text_t dst_access_mask = zest__vulkan_access_flags_to_string(imb->dstAccessMask);
                    ZEST_PRINT("            %s, Layout: %s -> %s, Access: %s -> %s, QFI: %u -> %u",
                        image_resource->name, 
                        zest__vulkan_image_layout_to_string(imb->oldLayout), zest__vulkan_image_layout_to_string(imb->newLayout),
                        src_access_mask.str, dst_access_mask.str,
                        imb->srcQueueFamilyIndex, imb->dstQueueFamilyIndex);
                    zest_FreeText(&src_access_mask);
                    zest_FreeText(&dst_access_mask);
                }

                ZEST_PRINT("        Buffers:");
                zest_vec_foreach(barrier_index, exe_details->barriers.acquire_buffer_barriers) {
                    VkBufferMemoryBarrier *bmb = &exe_details->barriers.acquire_buffer_barriers[barrier_index];
                    zest_resource_node buffer_resource = exe_details->barriers.acquire_buffer_barrier_nodes[barrier_index];
                    // You need a robust way to get resource_name from bmb->image
                    zest_text_t src_access_mask = zest__vulkan_access_flags_to_string(bmb->srcAccessMask);
                    zest_text_t dst_access_mask = zest__vulkan_access_flags_to_string(bmb->dstAccessMask);
                    ZEST_PRINT("            %s | %p, Access: %s -> %s, QFI: %u -> %u, Offset: %zu, Size: %zu",
                        buffer_resource->name, buffer_resource->storage_buffer->memory_pool->buffer,
                        src_access_mask.str, dst_access_mask.str,
                        bmb->srcQueueFamilyIndex, bmb->dstQueueFamilyIndex,
                        buffer_resource->storage_buffer->memory_offset, buffer_resource->storage_buffer->size);
                    zest_FreeText(&src_access_mask);
                    zest_FreeText(&dst_access_mask);
                }
            }

            // Print Inputs and Outputs (simplified)
            // ...

            if (exe_details->render_pass != VK_NULL_HANDLE) {
                ZEST_PRINT("      VkRenderPass: %p, VkFramebuffer: %p, RenderArea: (%d,%d)-(%ux%u)",
                    (void *)exe_details->render_pass, (void *)exe_details->frame_buffer,
                    exe_details->render_area.offset.x, exe_details->render_area.offset.y,
                    exe_details->render_area.extent.width, exe_details->render_area.extent.height);
                // Further detail: iterate VkRenderPassCreateInfo's attachments (if stored or re-derived)
                // and print each VkAttachmentDescription's load/store/layouts and clear values.
            }

            if (zest_vec_size(exe_details->barriers.release_buffer_barriers) > 0 ||
                zest_vec_size(exe_details->barriers.release_image_barriers) > 0) {
                zest_text_t overal_src_pipeline_stages = zest__vulkan_pipeline_stage_flags_to_string(exe_details->barriers.overall_src_stage_mask_for_release_barriers);
                zest_text_t overal_dst_pipeline_stages = zest__vulkan_pipeline_stage_flags_to_string(exe_details->barriers.overall_dst_stage_mask_for_release_barriers);
                ZEST_PRINT("      Release Barriers (Overall Pipeline Src Stages: %s, Dst Stages: %s):",
                    overal_src_pipeline_stages.str,
                    overal_dst_pipeline_stages.str);
                zest_FreeText(&overal_src_pipeline_stages);
                zest_FreeText(&overal_dst_pipeline_stages);

                ZEST_PRINT("        Images:");
                zest_vec_foreach(barrier_index, exe_details->barriers.release_image_barriers) {
                    VkImageMemoryBarrier *imb = &exe_details->barriers.release_image_barriers[barrier_index];
                    zest_resource_node image_resource = exe_details->barriers.release_image_barrier_nodes[barrier_index];
                    zest_text_t src_access_mask = zest__vulkan_access_flags_to_string(imb->srcAccessMask);
                    zest_text_t dst_access_mask = zest__vulkan_access_flags_to_string(imb->dstAccessMask);
                    ZEST_PRINT("            %s, Layout: %s -> %s, Access: %s -> %s, QFI: %u -> %u",
                        image_resource->name,
                        zest__vulkan_image_layout_to_string(imb->oldLayout), zest__vulkan_image_layout_to_string(imb->newLayout),
                        src_access_mask.str, dst_access_mask.str,
                        imb->srcQueueFamilyIndex, imb->dstQueueFamilyIndex);
                    zest_FreeText(&src_access_mask);
                    zest_FreeText(&dst_access_mask);
                }

                ZEST_PRINT("        Buffers:");
                zest_vec_foreach(barrier_index, exe_details->barriers.release_buffer_barriers) {
                    VkBufferMemoryBarrier *bmb = &exe_details->barriers.release_buffer_barriers[barrier_index];
                    zest_resource_node buffer_resource = exe_details->barriers.release_buffer_barrier_nodes[barrier_index];
                    // You need a robust way to get resource_name from bmb->image
                    zest_text_t src_access_mask = zest__vulkan_access_flags_to_string(bmb->srcAccessMask);
                    zest_text_t dst_access_mask = zest__vulkan_access_flags_to_string(bmb->dstAccessMask);
                    ZEST_PRINT("            %s, Access: %s -> %s, QFI: %u -> %u, Offset: %zu, Size: %zu",
                        buffer_resource->name,
                        src_access_mask.str, dst_access_mask.str,
                        bmb->srcQueueFamilyIndex, bmb->dstQueueFamilyIndex,
                        buffer_resource->storage_buffer->memory_offset, buffer_resource->storage_buffer->size);
                    zest_FreeText(&src_access_mask);
                    zest_FreeText(&dst_access_mask);
                }
            }
        }

        // --- Print Signal Semaphores for the Batch ---
        if (batch->final_signal_semaphores != 0) {
            ZEST_PRINT("  Signal Semaphores:");
            zest_vec_foreach(signal_index, batch->final_signal_semaphores) {
                if (batch->signal_values[signal_index] > 0) {
                    ZEST_PRINT("  Timeline Semaphore:   %p", (void *)batch->final_signal_semaphores[signal_index]);
                } else {
                    ZEST_PRINT("  Binary Semaphore: %p, Value: %zu", (void *)batch->final_signal_semaphores[signal_index], batch->signal_values[signal_index]);
                }
            }
        }
        ZEST_PRINT(""); // End of batch
    }
    ZEST_PRINT("--- End of Report ---");
}

void zest_EmptyRenderPass(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
    //Nothing here to render, it's just for render graphs that have nothing to render
}

// --Command Queue functions
zest_uint zest__get_image_binding_number(zest_resource_node resource, bool image_view_only) {
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    ZEST_CHECK_HANDLE(render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (!ZEST_VALID_HANDLE(render_graph->bindless_layout)) {
        ZEST_PRINT("ERROR: render graph %s doesn't have a bindless layout assigned, but you're tring to add a resource that uses a binding number.", render_graph->name);
        return ZEST_INVALID;
    }

    ZEST_ASSERT(resource->type == zest_resource_type_image);   //resource must be a image
    zest_uint binding_number = ZEST_INVALID;
	zest_vec_foreach(i, render_graph->bindless_layout->layout_bindings) {
		VkDescriptorSetLayoutBinding *binding = &render_graph->bindless_layout->layout_bindings[i];
		if (!image_view_only && resource->image_desc.usage & VK_IMAGE_USAGE_SAMPLED_BIT && binding->descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
			binding_number = binding->binding;
			break;
		} else if (resource->image_desc.usage & VK_IMAGE_USAGE_SAMPLED_BIT && binding->descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
			binding_number = binding->binding;
			break;
		} else if (resource->image_desc.usage & VK_IMAGE_USAGE_STORAGE_BIT && binding->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
			binding_number = binding->binding;
			break;
		}
	}

    return binding_number;
}

zest_uint zest__get_buffer_binding_number(zest_resource_node resource) {
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    ZEST_CHECK_HANDLE(render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (!ZEST_VALID_HANDLE(render_graph->bindless_layout)) {
        ZEST_PRINT("ERROR: render graph %s doesn't have a bindless layout assigned, but you're tring to add a resource that uses a binding number.", render_graph->name);
        return ZEST_INVALID;
    }
    ZEST_ASSERT(resource->type == zest_resource_type_buffer);   //resource must be a buffer

    zest_uint binding_number = ZEST_INVALID;
	zest_vec_foreach(i, render_graph->bindless_layout->layout_bindings) {
		VkDescriptorSetLayoutBinding *binding = &render_graph->bindless_layout->layout_bindings[i];
		if (resource->buffer_desc.buffer_info.usage_flags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT && 
			binding->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
			binding->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
			) {
			binding_number = binding->binding;
			break;
		} else if (resource->buffer_desc.buffer_info.usage_flags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT && 
			(binding->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || 
			 binding->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)) {
			binding_number = binding->binding;
			break;
		}
	}

    return binding_number;
}

zest_uint zest_AcquireBindlessTextureIndex(zest_texture texture, zest_set_layout layout, zest_descriptor_set set, zest_uint target_binding_number) {
    ZEST_CHECK_HANDLE(layout);  //Must be a valid handle to a descriptor set layout

    zest_uint binding_number = ZEST_INVALID;
    VkDescriptorType descriptor_type;
	zest_vec_foreach(i, layout->layout_bindings) {
		VkDescriptorSetLayoutBinding *binding = &layout->layout_bindings[i];
		if (binding->binding == target_binding_number && 
            (binding->descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
             binding->descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)) {
            descriptor_type = binding->descriptorType;
			binding_number = binding->binding;
			break;
        }
	}

    ZEST_ASSERT(binding_number != ZEST_INVALID);    //Could not find an appropriate descriptor type in the layout with that target binding number!
    zest_uint array_index = zest__acquire_bindless_index(layout, binding_number);
    ZEST_ASSERT(array_index != ZEST_INVALID);   //Ran out of space in the descriptor pool
    texture->binding_number = binding_number;
    texture->bindless_set = set;

    VkWriteDescriptorSet write = { 0 };
	VkDescriptorImageInfo image_info = texture->descriptor_image_info;
    if (descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        image_info.sampler = texture->sampler->vk_sampler;
		write = zest_CreateImageDescriptorWriteWithType(texture->bindless_set->vk_descriptor_set, &image_info, binding_number, descriptor_type);
    } else {
		write = zest_CreateImageDescriptorWriteWithType(texture->bindless_set->vk_descriptor_set, &image_info, binding_number, descriptor_type);
    }
    write.dstArrayElement = array_index;
	vkUpdateDescriptorSets(ZestDevice->logical_device, 1, &write, 0, 0);

    return array_index;
}

zest_uint zest_AcquireBindlessStorageBufferIndex(zest_buffer buffer, zest_set_layout layout, zest_descriptor_set set, zest_uint target_binding_number) {
    ZEST_CHECK_HANDLE(layout);  //Must be a valid handle to a descriptor set layout

    zest_uint binding_number = ZEST_INVALID;
    zest_vec_foreach(i, layout->layout_bindings) {
        VkDescriptorSetLayoutBinding *layout_binding = &layout->layout_bindings[i];
        if (target_binding_number == layout_binding->binding && layout_binding->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
            binding_number = layout_binding->binding;
            break;
        }
    }

    ZEST_ASSERT(binding_number != ZEST_INVALID);    //Could not find an appropriate descriptor type in the layout with that target binding number!
    zest_uint array_index = zest__acquire_bindless_index(layout, binding_number);

    VkDescriptorBufferInfo buffer_info = zest_vk_GetBufferInfo(buffer);

    VkWriteDescriptorSet write = zest_CreateBufferDescriptorWriteWithType(set->vk_descriptor_set, &buffer_info, binding_number, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    write.dstArrayElement = array_index;
    vkUpdateDescriptorSets(ZestDevice->logical_device, 1, &write, 0, 0);
    buffer->array_index = array_index;

    return array_index;
}

zest_uint zest_AcquireGlobalCombinedImageSampler(zest_texture texture) {
    texture->descriptor_array_index = zest_AcquireBindlessTextureIndex(texture, ZestRenderer->global_bindless_set_layout, ZestRenderer->global_set, 0);
    return texture->descriptor_array_index;
}

zest_uint zest_AcquireGlobalSampledImage(zest_texture texture) {
    return zest_AcquireBindlessTextureIndex(texture, ZestRenderer->global_bindless_set_layout, ZestRenderer->global_set, 3);
}

zest_uint zest_AcquireGlobalSampler(zest_texture texture) {
    return zest_AcquireBindlessTextureIndex(texture, ZestRenderer->global_bindless_set_layout, ZestRenderer->global_set, 2);
}

zest_uint zest_AcquireGlobalStorageBufferIndex(zest_buffer buffer) {
    zest_uint array_index = zest__acquire_bindless_index(ZestRenderer->global_bindless_set_layout, zest_storage_buffer_binding);

    VkDescriptorBufferInfo buffer_info = zest_vk_GetBufferInfo(buffer);

    VkWriteDescriptorSet write = zest_CreateBufferDescriptorWriteWithType(ZestRenderer->global_set->vk_descriptor_set, &buffer_info, zest_storage_buffer_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    write.dstArrayElement = array_index;
    vkUpdateDescriptorSets(ZestDevice->logical_device, 1, &write, 0, 0);
    buffer->array_index = array_index;

    return array_index;
}

void zest_AcquireGlobalInstanceLayerBufferIndex(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);   //Must be a valid layer handle
    ZEST_ASSERT(ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif));   //Layer must have been created with a persistant vertex buffer
    zest_ForEachFrameInFlight(fif) {
        zest_AcquireGlobalStorageBufferIndex(layer->memory_refs[fif].device_vertex_data);
    }
    layer->bindless_set = ZestRenderer->global_set;
    ZEST__FLAG(layer->flags, zest_layer_flag_using_global_bindless_layout);
}

void zest_ReleaseGlobalStorageBufferIndex(zest_buffer buffer) {
    ZEST_ASSERT(buffer->array_index != ZEST_INVALID);
    zest__release_bindless_index(ZestRenderer->global_bindless_set_layout, zest_storage_buffer_binding, buffer->array_index);
}

void zest_ReleaseGlobalTextureIndex(zest_texture texture) {
    ZEST_ASSERT(texture->descriptor_array_index != ZEST_INVALID);
    zest__release_bindless_index(ZestRenderer->global_bindless_set_layout, 0, texture->descriptor_array_index);
}

void zest_ReleaseGlobalSamplerIndex(zest_uint index) {
    ZEST_ASSERT(index != ZEST_INVALID);
    zest__release_bindless_index(ZestRenderer->global_bindless_set_layout, 3, index);
}

void zest_ReleaseGlobalSampledImageIndex(zest_uint index) {
    ZEST_ASSERT(index != ZEST_INVALID);
    zest__release_bindless_index(ZestRenderer->global_bindless_set_layout, 2, index);
}

void zest_ReleaseBindlessIndex(zest_uint index, zest_uint binding_number) {
    ZEST_ASSERT(index != ZEST_INVALID);
    zest__release_bindless_index(ZestRenderer->global_bindless_set_layout, binding_number, index);
}

VkDescriptorSet zest_vk_GetGlobalBindlessSet() {
    return ZestRenderer->global_set->vk_descriptor_set;
}

VkDescriptorSetLayout zest_vk_GetGlobalBindlessLayout() {
    return ZestRenderer->global_bindless_set_layout->vk_layout;
}

zest_set_layout zest_GetGlobalBindlessLayout() {
    return ZestRenderer->global_bindless_set_layout;
}

zest_descriptor_set zest_GetGlobalBindlessSet() {
    return ZestRenderer->global_set;
}

VkDescriptorSet zest_vk_GetGlobalUniformBufferDescriptorSet() {
    return ZestRenderer->uniform_buffer->descriptor_set[ZEST_FIF]->vk_descriptor_set;
}

zest_resource_node zest_AddTransientImageResource(const char *name, const zest_image_description_t *description, zest_bool assign_bindless, zest_bool image_view_binding_only) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    zest_resource_node_t node = { 0 };
    node.name = name;
	node.id = render_graph->id_counter++;
    node.first_usage_pass_idx = UINT_MAX;
    node.image_desc = *description;
    node.type = zest_resource_type_image;
    node.render_graph = render_graph;
    node.current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    node.current_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
    node.magic = zest_INIT_MAGIC;
    node.producer_pass_idx = -1;
    if (assign_bindless) {
        ZEST_CHECK_HANDLE(render_graph->bindless_layout);   //Trying to assing bindless index number but the render graph bindless layout is null.
                                                            //Make sure you assign it when creating the render graph.
        node.bindless_index = zest__get_image_binding_number(&node, image_view_binding_only);
		ZEST_ASSERT(node.binding_number != ZEST_INVALID);   //Could not find a valid binding number for the image/texture. Make sure that
                                                            //the bindless descriptor set layout you assigned to the render graph has the correct
															//desriptor types set.
        ZEST__FLAG(node.flags, zest_resource_node_flag_is_bindless);
    }
	ZEST__FLAG(node.flags, zest_resource_node_flag_transient);
    zest_vec_linear_push(ZestRenderer->render_graph_allocator, render_graph->resources, node);
    return &zest_vec_back(render_graph->resources);
}

zest_resource_node zest_AddTransientBufferResource(const char *name, const zest_buffer_description_t *description, zest_bool assign_bindless) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    if (description->size == 0) {
        return NULL;
    }
    zest_resource_node_t node = { 0 };
    node.name = name;
    node.id = render_graph->id_counter++;
    node.first_usage_pass_idx = UINT_MAX;
    node.buffer_desc = *description;
    node.type = zest_resource_type_buffer;
    node.render_graph = render_graph;
    node.current_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
    node.magic = zest_INIT_MAGIC;
    node.producer_pass_idx = -1;
    if (assign_bindless) {
        ZEST_CHECK_HANDLE(render_graph->bindless_layout);   //Trying to assign bindless index number but the render graph bindless layout is null.
        //Make sure you assign it when creating the render graph.
        node.binding_number = zest__get_buffer_binding_number(&node);
        ZEST_ASSERT(node.binding_number != ZEST_INVALID);   //Could not find a valid binding number for the image/texture. Make sure that
        //the bindless descriptor set layout you assigned to the render graph has the correct
        //desriptor types set.
        ZEST__FLAG(node.flags, zest_resource_node_flag_is_bindless);
    }
    ZEST__FLAG(node.flags, zest_resource_node_flag_transient);
    zest_vec_linear_push(ZestRenderer->render_graph_allocator, render_graph->resources, node);
    return &zest_vec_back(render_graph->resources);
}

zest_resource_node zest_AddInstanceLayerBufferResource(const char *name, const zest_layer layer, zest_bool prev_fif) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    ZEST_CHECK_HANDLE(layer);   //Not a valid layer handle
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
        layer->fif = ZEST_FIF;
        layer->dirty[ZEST_FIF] = 1;
        zest_buffer_description_t buffer_desc = { 0 };
        buffer_desc.size = layer->memory_refs[layer->fif].staging_instance_data->size;
        buffer_desc.buffer_info = zest_CreateVertexBufferInfo(0);
        layer->vertex_buffer_node = zest_AddTransientBufferResource(name, &buffer_desc, ZEST_FALSE);
    } else {
        zest_uint fif = prev_fif ? layer->prev_fif : layer->fif;
		zest_resource_node_t node = zest__create_import_buffer_resource_node(name, layer->memory_refs[fif].device_vertex_data);
		zest_vec_linear_push(ZestRenderer->render_graph_allocator, render_graph->resources, node);
        layer->vertex_buffer_node = &zest_vec_back(render_graph->resources);
        layer->vertex_buffer_node->bindless_index = layer->memory_refs[fif].device_vertex_data->array_index;
    }
    return layer->vertex_buffer_node;
}

zest_resource_node zest_AddFontLayerTextureResource(const zest_font font) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    return zest_ImportImageResourceReadOnly(font->texture->name.str, font->texture);
}

zest_resource_node zest_AddTransientVertexBufferResource(const char *name, zest_size size, zest_bool include_storage_flags, zest_bool assign_bindless) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    zest_buffer_description_t buffer_desc = { 0 };
    buffer_desc.size = size;
    buffer_desc.buffer_info = include_storage_flags ? zest_CreateVertexBufferInfoWithStorage(0) : zest_CreateVertexBufferInfo(0);
    return zest_AddTransientBufferResource(name, &buffer_desc, assign_bindless);
}

zest_resource_node zest_AddTransientIndexBufferResource(const char *name, zest_size size, zest_bool include_storage_flags, zest_bool assign_bindless) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    zest_buffer_description_t buffer_desc = { 0 };
    buffer_desc.size = size;
    buffer_desc.buffer_info = include_storage_flags ? zest_CreateIndexBufferInfoWithStorage(0) : zest_CreateIndexBufferInfo(0);
    return zest_AddTransientBufferResource(name, &buffer_desc, assign_bindless);
}

zest_resource_node zest_AddTransientStorageBufferResource(const char *name, zest_size size, zest_bool assign_bindless) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    zest_buffer_description_t buffer_desc = { 0 };
    buffer_desc.size = size;
    buffer_desc.buffer_info = zest_CreateStorageBufferInfo();
    return zest_AddTransientBufferResource(name, &buffer_desc, assign_bindless);
}

zest_resource_node_t zest__create_import_descriptor_buffer_resource_node(const char *name, zest_buffer buffer) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    zest_resource_node_t node = { 0 };
    node.name = name;
	node.id = render_graph->id_counter++;
	node.resource = (zest_resource_handle)buffer;
    node.first_usage_pass_idx = UINT_MAX;
	node.type = zest_resource_type_buffer;
    node.render_graph = render_graph;
    node.magic = zest_INIT_MAGIC;
    node.storage_buffer = buffer;
    node.current_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
    node.producer_pass_idx = -1;
	ZEST__FLAG(node.flags, zest_resource_node_flag_imported);
    return node;
}

zest_resource_node_t zest__create_import_buffer_resource_node(const char *name, zest_buffer buffer) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    zest_resource_node_t node = { 0 };
    node.name = name;
	node.id = render_graph->id_counter++;
	node.resource = (zest_resource_handle)buffer;
    node.first_usage_pass_idx = UINT_MAX;
	node.type = zest_resource_type_buffer;
    node.render_graph = render_graph;
    node.magic = zest_INIT_MAGIC;
    node.storage_buffer = buffer;
    node.current_queue_family_index = buffer->owner_queue_family;
    node.current_access_mask = buffer->last_access_mask;
    node.producer_pass_idx = -1;
	ZEST__FLAG(node.flags, zest_resource_node_flag_imported);
    return node;
}

zest_resource_node_t zest__create_import_image_resource_node(const char *name, zest_texture texture) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    zest_resource_node_t node = { 0 };
    node.name = name;
	node.id = render_graph->id_counter++;
	node.resource = (zest_resource_handle)texture;
    node.first_usage_pass_idx = UINT_MAX;
	node.type = zest_resource_type_image;
    node.render_graph = render_graph;
    node.magic = zest_INIT_MAGIC;
    node.image_buffer = texture->image_buffer;
    node.image_desc.width = texture->texture_layer_size;
    node.image_desc.numSamples = VK_SAMPLE_COUNT_1_BIT;
    node.image_desc.mip_levels = texture->mip_levels;
    node.current_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
    node.producer_pass_idx = -1;
	ZEST__FLAG(node.flags, zest_resource_node_flag_imported);
    return node;
}

zest_resource_node zest_ImportImageResource(const char *name, zest_texture texture, VkImageLayout initial_layout_at_graph_start, VkImageLayout desired_layout_after_graph_use) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    zest_resource_node_t node = zest__create_import_image_resource_node(name, texture);
    node.final_layout = desired_layout_after_graph_use;
    node.current_layout = initial_layout_at_graph_start;
    zest_vec_linear_push(ZestRenderer->render_graph_allocator, render_graph->resources, node);
    return &zest_vec_back(render_graph->resources);
}

zest_resource_node zest_ImportImageResourceReadOnly(const char *name, zest_texture texture) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    zest_resource_node_t node = zest__create_import_image_resource_node(name, texture);
    node.final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    node.current_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	ZEST__FLAG(node.flags, zest_resource_node_flag_imported);
    zest_vec_linear_push(ZestRenderer->render_graph_allocator, render_graph->resources, node);
    return &zest_vec_back(render_graph->resources);
}

zest_resource_node zest_ImportStorageBufferResource(const char *name, zest_buffer buffer) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    zest_resource_node_t node = zest__create_import_descriptor_buffer_resource_node(name, buffer);
    zest_vec_linear_push(ZestRenderer->render_graph_allocator, render_graph->resources, node);
    return &zest_vec_back(render_graph->resources);
}

zest_resource_node zest_ImportBufferResource(const char *name, zest_buffer buffer) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    zest_resource_node_t node = zest__create_import_buffer_resource_node(name, buffer);
    zest_vec_linear_push(ZestRenderer->render_graph_allocator, render_graph->resources, node);
    return &zest_vec_back(render_graph->resources);
}

zest_bool zest_AcquireSwapChainImage() {
    VkResult result = vkAcquireNextImageKHR(ZestDevice->logical_device, ZestRenderer->swapchain, UINT64_MAX, ZestRenderer->image_available_semaphore[ZEST_FIF], ZEST_NULL, &ZestRenderer->current_image_frame);

    //Has the window been resized? if so rebuild the swap chain.
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        zest__recreate_swapchain();
        return ZEST_FALSE;
    }
    ZEST_VK_CHECK_RESULT(result);
    ZEST__FLAG(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_acquired);
    return ZEST_TRUE;
}

zest_resource_node zest_ImportSwapChainResource(const char *name) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    if (ZEST__NOT_FLAGGED(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_acquired)) {
        ZEST_PRINT("WARNING: Swap chain is being imported but no swap chain image has been acquired. You can use zest_BeginRenderToScreen() to properly acquire a swap chain image.");
    }
    zest_resource_node_t node = { 0 };
    node.name = name;
	node.id = render_graph->id_counter++;
    node.first_usage_pass_idx = UINT_MAX;
	node.type = zest_resource_type_swap_chain_image;
    node.render_graph = render_graph;
    node.magic = zest_INIT_MAGIC;
    node.image_buffer.image = ZestRenderer->swapchain_images[ZestRenderer->current_image_frame];
    node.image_buffer.base_view = ZestRenderer->swapchain_image_views[ZestRenderer->current_image_frame];
    node.image_buffer.format = ZestRenderer->swapchain_image_format;
    node.image_desc.width = ZestRenderer->swapchain_extent.width;
    node.image_desc.height = ZestRenderer->swapchain_extent.height;
    node.current_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
    node.image_desc.mip_levels = 1;
    node.image_desc.numSamples = VK_SAMPLE_COUNT_1_BIT;
    node.current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    node.last_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    node.producer_pass_idx = -1;
    node.final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	ZEST__FLAG(node.flags, zest_resource_node_flag_imported);
    zest_vec_linear_push(ZestRenderer->render_graph_allocator, render_graph->resources, node);
    render_graph->swapchain_resource_handle = &zest_vec_back(render_graph->resources);
    return render_graph->swapchain_resource_handle;
}

void zest_AddPassTask(zest_pass_node pass, zest_rg_execution_callback callback, void *user_data) {
    ZEST_CHECK_HANDLE(pass);        //Not a valid pass node!
    zest_pass_execution_callback_t callback_data;
    callback_data.execution_callback = callback;
    callback_data.user_data = user_data;
    zest_vec_linear_push(ZestRenderer->render_graph_allocator, pass->execution_callbacks, callback_data);
}

void zest_AddPassInstanceLayerUpload(zest_pass_node pass, zest_layer layer) {
    ZEST_CHECK_HANDLE(pass);        //Not a valid pass node!
    if (ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif) && layer->dirty[layer->fif] == 0) {
        return;
    }
    zest_pass_execution_callback_t callback_data;
    callback_data.execution_callback = zest_UploadInstanceLayerData;
    callback_data.user_data = layer;
    zest_vec_linear_push(ZestRenderer->render_graph_allocator, pass->execution_callbacks, callback_data);
}

void zest_AddPassInstanceLayer(zest_pass_node pass, zest_layer layer) {
    ZEST_CHECK_HANDLE(pass);        //Not a valid pass node!
    zest_pass_execution_callback_t callback_data;
    callback_data.execution_callback = zest_DrawInstanceLayer;
    callback_data.user_data = layer;
    zest_vec_linear_push(ZestRenderer->render_graph_allocator, pass->execution_callbacks, callback_data);
}

void zest_ClearPassTasks(zest_pass_node pass) {
    zest_vec_clear(pass->execution_callbacks);
}

zest_pass_node zest__add_pass_node(const char *name, zest_device_queue_type queue_type) {
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    ZEST_CHECK_HANDLE(render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_pass_node_t node = { 0 };
    node.id = render_graph->id_counter++;
    node.queue_type = queue_type;
    node.name = name;
    node.render_graph = render_graph;
    node.magic = zest_INIT_MAGIC;
    zest_vec_linear_push(ZestRenderer->render_graph_allocator, render_graph->passes, node);
    return &zest_vec_back(render_graph->passes);
}

zest_pass_node zest_AddGraphicBlankScreen(const char *name) {
    zest_pass_node pass = zest__add_pass_node(name, zest_queue_graphics);
    zest_AddPassTask(pass, zest_EmptyRenderPass, 0);
    return pass;
}

zest_pass_node zest_AddRenderPassNode(const char *name) {
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);        //Not a valid render graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_pass_node node = zest__add_pass_node(name, zest_queue_graphics);
    node->timeline_wait_stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    return node;
}

zest_pass_node zest_AddComputePassNode(zest_compute compute, const char *name) {
    bool force_graphics_queue = (ZestRenderer->current_render_graph->flags & zest_render_graph_force_on_graphics_queue) > 0;
    zest_pass_node node = zest__add_pass_node(name, force_graphics_queue ? zest_queue_graphics : zest_queue_compute);
    node->compute = compute;
    node->timeline_wait_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    return node;
}

zest_pass_node zest_AddTransferPassNode(const char *name) {
    bool force_graphics_queue = (ZestRenderer->current_render_graph->flags & zest_render_graph_force_on_graphics_queue) > 0;
    zest_pass_node node = zest__add_pass_node(name, force_graphics_queue ? zest_queue_graphics : zest_queue_transfer);
    node->timeline_wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    return node;
}

zest_resource_node zest_GetPassInputResource(zest_pass_node pass, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(pass->inputs, name));  //Not a valid input resource name. Check the name and also maybe you meant to get from outputs?
    zest_resource_usage_t *usage = zest_map_at(pass->inputs, name);
    return usage->resource_node;
}

zest_buffer zest_GetPassInputBuffer(zest_pass_node pass, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(pass->inputs, name));  //Not a valid input resource name. Check the name and also maybe you meant to get from outputs?
    zest_resource_usage_t *usage = zest_map_at(pass->inputs, name);
    return usage->resource_node->storage_buffer;
}

zest_buffer zest_GetPassOutputBuffer(zest_pass_node pass, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(pass->outputs, name));  //Not a valid input resource name. Check the name and also maybe you meant to get from inputs?
    zest_resource_usage_t *usage = zest_map_at(pass->outputs, name);
    return usage->resource_node->storage_buffer;
}

zest_resource_node zest_GetPassOutputResource(zest_pass_node pass, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(pass->outputs, name));  //Not a valid output resource name. Check the name and also maybe you meant to get from inputs?
    zest_resource_usage_t *usage = zest_map_at(pass->outputs, name);
    return usage->resource_node;
}

void zest__add_pass_buffer_usage(zest_pass_node pass_node, zest_resource_node buffer_resource, zest_resource_purpose purpose, VkPipelineStageFlags relevant_pipeline_stages, zest_bool is_output) {
    zest_resource_usage_t usage = { 0 }; // Your internal struct to store resolved flags
    usage.resource_node = buffer_resource;

    switch (purpose) {
    case zest_purpose_vertex_buffer:
        usage.access_mask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        usage.stage_mask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        break;
    case zest_purpose_index_buffer:
        usage.access_mask = VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        usage.stage_mask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        break;
    case zest_purpose_uniform_buffer:
        usage.access_mask = VK_ACCESS_UNIFORM_READ_BIT;
        usage.stage_mask = relevant_pipeline_stages; 
        break;
    case zest_purpose_storage_buffer_read:
        usage.access_mask = VK_ACCESS_SHADER_READ_BIT;
        usage.stage_mask = relevant_pipeline_stages;
        break;
    case zest_purpose_storage_buffer_write:
        usage.access_mask = VK_ACCESS_SHADER_WRITE_BIT;
        usage.stage_mask = relevant_pipeline_stages;
        break;
    case zest_purpose_storage_buffer_read_write:
        usage.access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        usage.stage_mask = relevant_pipeline_stages;
        break;
    case zest_purpose_transfer_src_buffer:
        usage.access_mask = VK_ACCESS_TRANSFER_READ_BIT;
        usage.stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;
    case zest_purpose_transfer_dst_buffer:
        usage.access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
        usage.stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;
    default:
        ZEST_ASSERT(0);     //Unhandled buffer access purpose!
        return;
    }
    usage.image_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (is_output) { // Or derive is_output from purpose (e.g. WRITE implies output)
        zest_map_linear_insert(ZestRenderer->render_graph_allocator, pass_node->outputs, buffer_resource->name, usage);
    } else {
        zest_map_linear_insert(ZestRenderer->render_graph_allocator, pass_node->inputs, buffer_resource->name, usage);
    }
}

void zest__add_image_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, VkAccessFlags src_access, 
    VkAccessFlags dst_access, VkImageLayout old_layout, VkImageLayout new_layout, zest_uint src_family, zest_uint dst_family, 
    VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage) {
    VkImageMemoryBarrier image_barrier = zest__create_image_memory_barrier(
        resource->image_buffer.image,
        src_access,
        dst_access,
        old_layout,
        new_layout,
        0, resource->image_desc.mip_levels);
    image_barrier.srcQueueFamilyIndex = src_family;
    image_barrier.dstQueueFamilyIndex = dst_family;
    if (acquire) {
        zest_vec_linear_push(ZestRenderer->render_graph_allocator, barriers->acquire_image_barriers, image_barrier);
        zest_vec_linear_push(ZestRenderer->render_graph_allocator, barriers->acquire_image_barrier_nodes, resource);
		barriers->overall_src_stage_mask_for_acquire_barriers |= src_stage;
		barriers->overall_dst_stage_mask_for_acquire_barriers |= dst_stage;
    } else {
        zest_vec_linear_push(ZestRenderer->render_graph_allocator, barriers->release_image_barriers, image_barrier);
        zest_vec_linear_push(ZestRenderer->render_graph_allocator, barriers->release_image_barrier_nodes, resource);
		barriers->overall_src_stage_mask_for_release_barriers |= src_stage;
		barriers->overall_dst_stage_mask_for_release_barriers |= dst_stage;
    }
}

ZEST_PRIVATE void zest__add_memory_buffer_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, VkAccessFlags src_access, VkAccessFlags dst_access,
    zest_uint src_family, zest_uint dst_family, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage) {
    VkBufferMemoryBarrier buffer_barrier = zest__create_buffer_memory_barrier(
        resource->storage_buffer->memory_pool->buffer,
        src_access,
        dst_access,
        resource->storage_buffer->memory_offset, resource->storage_buffer->size);
    buffer_barrier.srcQueueFamilyIndex = src_family;
    buffer_barrier.dstQueueFamilyIndex = dst_family;
    if (acquire) {
        zest_vec_linear_push(ZestRenderer->render_graph_allocator, barriers->acquire_buffer_barriers, buffer_barrier);
        zest_vec_linear_push(ZestRenderer->render_graph_allocator, barriers->acquire_buffer_barrier_nodes, resource);
		barriers->overall_src_stage_mask_for_acquire_barriers |= src_stage;
		barriers->overall_dst_stage_mask_for_acquire_barriers |= dst_stage;
    } else {
        zest_vec_linear_push(ZestRenderer->render_graph_allocator, barriers->release_buffer_barriers, buffer_barrier);
        zest_vec_linear_push(ZestRenderer->render_graph_allocator, barriers->release_buffer_barrier_nodes, resource);
		barriers->overall_src_stage_mask_for_release_barriers |= src_stage;
		barriers->overall_dst_stage_mask_for_release_barriers |= dst_stage;
    }
}

void zest__add_pass_image_usage( zest_pass_node pass_node,zest_resource_node image_resource, zest_resource_purpose purpose, VkPipelineStageFlags relevant_pipeline_stages, zest_bool is_output, VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op, VkAttachmentLoadOp stencil_load_op, VkAttachmentStoreOp stencil_store_op, VkClearValue clear_value) {
    zest_resource_usage_t usage = { 0 }; // Your internal struct
    usage.resource_node = image_resource;

    // Aspect flags should be determined based on format and purpose
    // Let's assume you have image_resource->image_desc.format
    usage.aspect_flags = zest__determine_aspect_flag(image_resource->image_desc.format); // Default based on format

    // Attachment ops are only relevant for attachment purposes
    // For other usages, they'll default to 0/DONT_CARE which is fine.
    usage.load_op = load_op;
    usage.store_op = store_op;
    usage.stencil_load_op = stencil_load_op;
    usage.stencil_store_op = stencil_store_op;
    usage.clear_value = clear_value;

    // Determine if this usage implies an output (modifies the resource)
    // This could also replace the 'is_output' parameter for many cases
    zest_bool inferred_is_output = ZEST_FALSE;

    switch (purpose) {
    case zest_purpose_color_attachment_write:
        usage.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        usage.access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        // If load_op is VK_ATTACHMENT_LOAD_OP_LOAD, an implicit read occurs.
        // If blending with destination, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT is also involved.
        // For simplicity, if this is purely about the *write* aspect:
        if (load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
            usage.access_mask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT; // For the load itself
        }
        usage.stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        usage.aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
        inferred_is_output = ZEST_TRUE;
        break;

    case zest_purpose_color_attachment_read: 
        usage.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        usage.access_mask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        usage.stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; 
        usage.aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
        inferred_is_output = ZEST_FALSE; 
        break;

    case zest_purpose_sampled_image:
        usage.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        usage.access_mask = VK_ACCESS_SHADER_READ_BIT;
        usage.stage_mask = relevant_pipeline_stages; 
        inferred_is_output = ZEST_FALSE;
        break;

    case zest_purpose_storage_image_read:
        usage.image_layout = VK_IMAGE_LAYOUT_GENERAL; 
        usage.access_mask = VK_ACCESS_SHADER_READ_BIT;
        usage.stage_mask = relevant_pipeline_stages; 
        inferred_is_output = ZEST_FALSE;
        break;

    case zest_purpose_storage_image_write:
        usage.image_layout = VK_IMAGE_LAYOUT_GENERAL;
        usage.access_mask = VK_ACCESS_SHADER_WRITE_BIT;
        usage.stage_mask = relevant_pipeline_stages;
        inferred_is_output = ZEST_TRUE;
        break;

    case zest_purpose_storage_image_read_write:
        usage.image_layout = VK_IMAGE_LAYOUT_GENERAL;
        usage.access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        usage.stage_mask = relevant_pipeline_stages; 
        inferred_is_output = ZEST_TRUE;
        break;

    case zest_purpose_depth_stencil_attachment_read: 
        usage.image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        usage.access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        usage.stage_mask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        inferred_is_output = ZEST_FALSE;
        break;

    case zest_purpose_depth_stencil_attachment_write:
        usage.image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        usage.access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        if (load_op == VK_ATTACHMENT_LOAD_OP_LOAD || stencil_load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
            usage.access_mask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }
        usage.stage_mask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        inferred_is_output = ZEST_TRUE;
        break;

    case zest_purpose_depth_stencil_attachment_read_write: // Typical depth testing and writing
        usage.image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        usage.access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        usage.stage_mask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        inferred_is_output = ZEST_TRUE; // Modifies resource
        break;

    case zest_purpose_input_attachment: // For subpass reads
        usage.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; 
        usage.access_mask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        usage.stage_mask = relevant_pipeline_stages; 
        inferred_is_output = ZEST_FALSE;
        break;

    case zest_purpose_transfer_src_image:
        usage.image_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        usage.access_mask = VK_ACCESS_TRANSFER_READ_BIT;
        usage.stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        inferred_is_output = ZEST_FALSE;
        break;

    case zest_purpose_transfer_dst_image:
        usage.image_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        usage.access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
        usage.stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        inferred_is_output = ZEST_TRUE;
        break;

    case zest_purpose_present_src: 
        usage.image_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        usage.access_mask = 0; // No specific GPU access by the pass itself for this state.
        usage.stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // Ensure all prior work is done.
        inferred_is_output = is_output; 
        break;

    default:
        ZEST_ASSERT(0); //Unhandled image access purpose!"
        return;
    }

    if (inferred_is_output) {
        zest_map_linear_insert(ZestRenderer->render_graph_allocator, pass_node->outputs, image_resource->name, usage);
    } else {
        zest_map_linear_insert(ZestRenderer->render_graph_allocator, pass_node->inputs, image_resource->name, usage);
    }
}

void zest_ConnectVertexBufferInput(zest_pass_node pass, zest_resource_node vertex_buffer) {
    ZEST_CHECK_HANDLE(vertex_buffer);
    zest__add_pass_buffer_usage(pass, vertex_buffer, zest_purpose_vertex_buffer, 0, ZEST_FALSE);
}

void zest_ConnectIndexBufferInput(zest_pass_node pass, zest_resource_node index_buffer) {
    ZEST_CHECK_HANDLE(index_buffer);
    zest__add_pass_buffer_usage(pass, index_buffer, zest_purpose_index_buffer, 0, ZEST_FALSE);
}

void zest_ConnectUniformBufferInput(zest_pass_node pass, zest_resource_node uniform_buffer) {
    ZEST_CHECK_HANDLE(uniform_buffer);
    zest__add_pass_buffer_usage(pass, uniform_buffer, zest_purpose_uniform_buffer, pass->timeline_wait_stage, ZEST_FALSE);
}

void zest_ConnectStorageBufferInput(zest_pass_node pass, zest_resource_node storage_buffer) {
    ZEST_CHECK_HANDLE(storage_buffer);
    zest__add_pass_buffer_usage(pass, storage_buffer, zest_purpose_storage_buffer_read, pass->timeline_wait_stage, ZEST_FALSE);
}

void zest_ConnectStorageBufferOutput(zest_pass_node pass, zest_resource_node storage_buffer) {
    ZEST_CHECK_HANDLE(storage_buffer);
    zest__add_pass_buffer_usage(pass, storage_buffer, zest_purpose_storage_buffer_write, pass->timeline_wait_stage, ZEST_TRUE);
}

void zest_ConnectTransferBufferOutput(zest_pass_node pass, zest_resource_node dst_buffer) {
    ZEST_CHECK_HANDLE(dst_buffer);
    zest__add_pass_buffer_usage(pass, dst_buffer, zest_purpose_transfer_dst_buffer, pass->timeline_wait_stage, ZEST_TRUE);
}

void zest_ConnectTransferBufferInput(zest_pass_node pass, zest_resource_node src_buffer) {
    zest__add_pass_buffer_usage(pass, src_buffer, zest_purpose_transfer_src_buffer, pass->timeline_wait_stage, ZEST_TRUE);
}

void zest_ReleaseBufferAfterUse(zest_resource_node node) {
    ZEST_ASSERT(ZEST__FLAGGED(node->flags, zest_resource_node_flag_imported));  //The resource must be imported, transient buffers are simply discarded after use.
    ZEST__FLAG(node->flags, zest_resource_node_flag_release_after_use);
}

// --- Image Helpers ---
void zest_ConnectSampledImageInput(zest_pass_node pass, zest_resource_node texture, zest_supported_pipeline_stages stages) {
    zest__add_pass_image_usage(pass, texture, zest_purpose_sampled_image, stages, ZEST_FALSE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, (VkClearValue){0});
}

void zest_ConnectSwapChainOutput( zest_pass_node pass, zest_resource_node swapchain_resource, VkClearColorValue clear_color_on_load) {
    VkClearValue cv; cv.color = clear_color_on_load;
    // Assuming clear for swapchain if not explicitly loaded
    zest__add_pass_image_usage(pass, swapchain_resource, zest_purpose_color_attachment_write, 
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, ZEST_TRUE,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, cv);
}

// More flexible version:
void zest_ConnectColorAttachmentOutput(zest_pass_node pass_node, zest_resource_node color_target, VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op, VkClearColorValue clear_color_if_clearing) {
    VkClearValue cv = { 0 }; 
    if (load_op == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        cv.color = clear_color_if_clearing;
    }
    zest__add_pass_image_usage(pass_node, color_target, zest_purpose_color_attachment_write,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, ZEST_TRUE,
        load_op, store_op,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
        cv);
}

void zest_ConnectDepthStencilOutput(zest_pass_node pass_node, zest_resource_node depth_target, VkAttachmentLoadOp depth_load_op, VkAttachmentStoreOp depth_store_op, VkAttachmentLoadOp stencil_load_op, VkAttachmentStoreOp stencil_store_op, VkClearDepthStencilValue clear_value_if_clearing) {
    VkClearValue cv = { 0 };
    if (depth_load_op == VK_ATTACHMENT_LOAD_OP_CLEAR || stencil_load_op == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        cv.depthStencil = clear_value_if_clearing;
    }
    zest__add_pass_image_usage(pass_node, depth_target, zest_purpose_depth_stencil_attachment_write,
        0, ZEST_TRUE, 
        depth_load_op, depth_store_op,
        stencil_load_op, stencil_store_op,
        cv);
}

void zest_ConnectDepthStencilInputReadOnly(zest_pass_node pass_node, zest_resource_node depth_target) {
    zest__add_pass_image_usage(pass_node, depth_target, zest_purpose_depth_stencil_attachment_read,
        0, ZEST_FALSE, // Internal function sets correct pipeline stages
        VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_DONT_CARE, 
        VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_DONT_CARE,
        (VkClearValue) { 0 }); 
}

void zest_WaitOnTimeline(zest_execution_timeline timeline) {
    ZEST_CHECK_HANDLE(timeline);    //Not a valid execution timeline. Use zest_CreateExecutionTimeline to create one
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);  //This function must be called withing a Being/EndRenderGraph block
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    zest_vec_linear_push(ZestRenderer->render_graph_allocator, render_graph->wait_on_timelines, timeline);
}

void zest_SignalTimeline(zest_execution_timeline timeline) {
    ZEST_CHECK_HANDLE(timeline);    //Not a valid execution timeline. Use zest_CreateExecutionTimeline to create one
    ZEST_CHECK_HANDLE(ZestRenderer->current_render_graph);  //This function must be called withing a Being/EndRenderGraph block
    zest_render_graph render_graph = ZestRenderer->current_render_graph;
    zest_vec_linear_push(ZestRenderer->render_graph_allocator, render_graph->signal_timelines, timeline);
}

zest_execution_timeline zest_CreateExecutionTimeline() {
    zest_execution_timeline timeline = ZEST__NEW(zest_execution_timeline);
    timeline->magic = zest_INIT_MAGIC;
    VkSemaphoreTypeCreateInfo timeline_create_info = { 0 };
    timeline_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timeline_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timeline_create_info.initialValue = 0;
    VkSemaphoreCreateInfo semaphore_info = { 0 };
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.pNext = &timeline_create_info;
    vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, &ZestDevice->allocation_callbacks, &timeline->semaphore);
    timeline->current_value = 0;
    zest_vec_push(ZestRenderer->timeline_semaphores, timeline);
    return timeline;
}

// --End Render Graph functions

// --Command queue setup and modify functions
void zest__set_queue_context(zest_setup_context_type context) {
    ZestRenderer->setup_context.type = context;
    if (ZestRenderer->setup_context.type == zest_setup_context_type_command_queue) {
        ZestRenderer->setup_context.draw_commands = ZEST_NULL;
        ZestRenderer->setup_context.layer = ZEST_NULL;
        ZestRenderer->setup_context.draw_routine = ZEST_NULL;
        ZestRenderer->setup_context.compute_index = -1;
    } else if (ZestRenderer->setup_context.type == zest_setup_context_type_render_pass) {
        ZestRenderer->setup_context.layer = ZEST_NULL;
        ZestRenderer->setup_context.compute_index = -1;
        ZestRenderer->setup_context.draw_routine = ZEST_NULL;
    } else if (ZestRenderer->setup_context.type == zest_setup_context_type_composite_render_pass) {
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
    command_queue->magic = zest_INIT_MAGIC;
    command_queue->name = name;

    for (ZEST_EACH_FIF_i) {
        command_queue->present_semaphore_index[i] = -1;
    }

    command_queue->recorder = zest_CreatePrimaryRecorder();

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
    zest__connect_command_queue_to_present();
    ZEST_ASSERT(ZestRenderer->setup_context.command_queue != ZEST_NULL);        //Trying to validate a queue that has no context. Finish queue setup must me run at the end of a queue setup
    zest_ValidateQueue(ZestRenderer->setup_context.command_queue);
    //Finish setting up any composite render_targets
    zest_command_queue command_queue = ZestRenderer->setup_context.command_queue;
    /*
    zest_vec_foreach(i, command_queue->draw_commands) {
        zest_command_queue_draw_commands draw_command = ZestRenderer->setup_context.command_queue->draw_commands[i];
        if (ZEST_VALID_HANDLE(draw_command->composite_pipeline) && !zest_vec_empty(draw_command->render_targets)) {
            zest_uint layer_count = zest_vec_size(draw_command->render_targets);
            zest_text_t layout_name = {0};
            zest_SetTextf(&layout_name, "%i samplers", layer_count);
            if (draw_command->composite_descriptor_layout) {
                vkDestroyDescriptorSetLayout(ZestDevice->logical_device, draw_command->composite_descriptor_layout->vk_layout, &ZestDevice->allocation_callbacks);
            }
            zest_ResetDescriptorPool(draw_command->descriptor_pool);
            zest_set_layout_builder_t layout_builder = zest_BeginSetLayoutBuilder();
            for (int c = 0; c != layer_count; ++c) {
                zest_AddLayoutBuilderCombinedImageSampler(&layout_builder, c, 1);
            }
            draw_command->composite_descriptor_layout = zest_FinishDescriptorSetLayout(&layout_builder, "set_layout_for_%s", draw_command->name);
			draw_command->composite_shader_resources = zest_CreateShaderResources();
			zest_ForEachFrameInFlight(fif) {
				zest_descriptor_set_builder_t set_builder = zest_BeginDescriptorSetBuilder(draw_command->composite_descriptor_layout);
				zest_vec_foreach(c, draw_command->render_targets) {
					zest_render_target layer = draw_command->render_targets[c];
                    zest_AddSetBuilderDirectImageWrite(&set_builder, c, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &layer->image_info[fif]);
                }
				draw_command->composite_descriptor_set[fif].vk_descriptor_set = zest_FinishDescriptorSet(draw_command->descriptor_pool, &set_builder, draw_command->composite_descriptor_set[fif].vk_descriptor_set);
				zest_AddDescriptorSetToResources(draw_command->composite_shader_resources, &draw_command->composite_descriptor_set[fif], fif);
            }
            zest_FreeText(&layout_name);
        }
    }
    */
    command_queue->timestamp_count = zest_vec_size(command_queue->draw_commands) * 2;
    if (!command_queue->query_pool) {
        command_queue->query_pool = zest__create_query_pool(command_queue->timestamp_count);
    }
    zest_vec_resize(command_queue->timestamps[0], command_queue->timestamp_count / 2);
    zest_vec_resize(command_queue->timestamps[1], command_queue->timestamp_count / 2);
    ZestRenderer->setup_context.command_queue = ZEST_NULL;
    ZestRenderer->setup_context.draw_commands = ZEST_NULL;
    ZestRenderer->setup_context.draw_routine = ZEST_NULL;
    ZestRenderer->setup_context.layer = ZEST_NULL;
    ZestRenderer->setup_context.composite_render_target = ZEST_NULL;
    ZestRenderer->setup_context.type = zest_setup_context_type_none;
}

void zest_ValidateQueue(zest_command_queue command_queue) {
    if (command_queue->flags & zest_command_queue_flag_present_dependency) {
        zest_bool present_found = 0;
        zest_bool present_flags_found = 0;
        for (ZEST_EACH_FIF_i) {
            for (zest_foreach_j(command_queue->fif_wait_stage_flags[i])) {
                present_flags_found += (command_queue->fif_wait_stage_flags[i][j] == VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            }
        }
        ZEST_ASSERT(present_found == ZEST_MAX_FIF);                //The command queue was created with zest_command_queue_flag_present_dependency but no semaphore was found to connect the two
        ZEST_ASSERT(present_flags_found == ZEST_MAX_FIF);        //The command queue was created with zest_command_queue_flag_present_dependency but the stage flags should be set to VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    }
    command_queue->flags |= zest_command_queue_flag_validated;
}

zest_command_queue zest_CurrentCommandQueue() {
    return ZestRenderer->active_command_queue;
}

zest_command_queue_draw_commands_t* zest_CurrentRenderPass() {
    return ZestRenderer->current_draw_commands;
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

zest_command_queue_draw_commands zest_NewDrawCommandSetupCompositeToSwap(const char* name, zest_pipeline_template composite_pipeline) {
    zest__set_queue_context(zest_setup_context_type_render_pass);
    zest_command_queue command_queue = ZestRenderer->setup_context.command_queue;
    zest_command_queue_draw_commands draw_commands = zest__create_command_queue_draw_commands(name);
    zest_vec_push(command_queue->draw_commands, draw_commands);
    draw_commands->name = name;
    draw_commands->render_pass = ZestRenderer->final_render_pass;
    draw_commands->get_frame_buffer = zest_GetRendererFrameBuffer;
    draw_commands->render_pass_function = zest_CompositeRenderTargets;
    draw_commands->composite_pipeline = composite_pipeline;
    draw_commands->viewport.extent = zest_GetSwapChainExtent();
    draw_commands->viewport.offset.x = draw_commands->viewport.offset.y = 0;
    ZestRenderer->setup_context.draw_commands = draw_commands;
    ZestRenderer->setup_context.type = zest_setup_context_type_render_pass;
    return draw_commands;
}

zest_command_queue_draw_commands zest_NewDrawCommandSetupCompositor(const char* name, zest_render_target render_target, zest_pipeline_template pipeline_template) {
    ZEST_ASSERT(render_target);        //render_target must be a valid index to a render target
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->command_queue_draw_commands, name));        //There are already draw commands with that name
    zest__set_queue_context(zest_setup_context_type_composite_render_pass);
    zest_command_queue command_queue = ZestRenderer->setup_context.command_queue;
    zest_command_queue_draw_commands draw_commands = zest__create_command_queue_draw_commands(name);
    zest_vec_push(command_queue->draw_commands, draw_commands);
    render_target->filter_pipeline_template = pipeline_template;
    draw_commands->name = name;
    draw_commands->render_pass = render_target->render_pass;
    draw_commands->get_frame_buffer = zest_GetRenderTargetFrameBufferCallback;
    draw_commands->render_pass_function = zest_CompositeRenderTargets;
    draw_commands->viewport = render_target->viewport;
    draw_commands->render_target = render_target;
    if (zest_Vec2Length2(render_target->create_info.ratio_of_screen_size)) {
        draw_commands->viewport_scale = render_target->create_info.ratio_of_screen_size;
        draw_commands->viewport_type = zest_render_viewport_type_scale_with_window;
    } else if (ZEST__FLAGGED(render_target->create_info.flags, zest_render_target_flag_fixed_size)) {
        draw_commands->viewport_type = zest_render_viewport_type_fixed;
    } else {
        draw_commands->viewport_type = zest_render_viewport_type_scale_with_window;
        draw_commands->viewport_scale = zest_Vec2Set(1.f, 1.f);
    }
    ZestRenderer->setup_context.draw_commands = draw_commands;
    ZestRenderer->setup_context.composite_render_target = render_target;
    return draw_commands;
}

void zest_AddRenderTarget(zest_render_target render_target) {
    ZEST_ASSERT(render_target);        //render_target must be a valid render target
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_render_pass);    //The current setup context must be a render pass using BeginRenderPassSetup or BeginRenderPassSetupSC
    zest_vec_push(ZestRenderer->setup_context.draw_commands->render_targets, render_target);
}

void zest_AddCompositeLayer(zest_render_target render_target) {
    ZEST_ASSERT(render_target);        //render_target must be a valid render target
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_composite_render_pass);    //The current setup context must be a composite render pass 
    zest_vec_push(ZestRenderer->setup_context.composite_render_target->composite_layers, render_target);
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

zest_command_queue_draw_commands zest_NewDrawCommandSetupDownSampler(const char *name, zest_render_target render_target, zest_render_target input_source, zest_pipeline_template filter_pipeline) {
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
    render_target->input_source = input_source;
    render_target->filter_pipeline_template = filter_pipeline;
    if (zest_Vec2Length2(render_target->create_info.ratio_of_screen_size)) {
        draw_commands->viewport_scale = render_target->create_info.ratio_of_screen_size;
        draw_commands->viewport_type = zest_render_viewport_type_scale_with_window;
    } else if (ZEST__FLAGGED(render_target->create_info.flags, zest_render_target_flag_fixed_size)) {
        draw_commands->viewport_type = zest_render_viewport_type_fixed;
    } else {
        draw_commands->viewport_type = zest_render_viewport_type_scale_with_window;
        draw_commands->viewport_scale = zest_Vec2Set(1.f, 1.f);
    }
    ZestRenderer->setup_context.draw_commands = draw_commands;
    ZestRenderer->setup_context.type = zest_setup_context_type_render_pass;
    return draw_commands;
}

zest_command_queue_draw_commands zest_NewDrawCommandSetupUpSampler(const char *name, zest_render_target render_target, zest_render_target downsampler, zest_pipeline_template pipeline) { ZEST_CHECK_HANDLE(render_target);  //render_target must be a valid handle to a render target
	ZEST_CHECK_HANDLE(downsampler);    //Must be a valid downsampler render target if you're specifying one
	ZEST_ASSERT(ZEST__FLAGGED(downsampler->flags, zest_render_target_flag_initialised));   //Down sampler source must be an initialised render target
	ZEST_ASSERT(downsampler->mip_levels == render_target->mip_levels);    //Down sampler mip levels must be the same as the upsampler you're creating
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
    render_target->input_source = downsampler;
    render_target->pipeline_template = pipeline;
    ZEST__FLAG(render_target->flags, zest_render_target_flag_upsampler);
    zest_CreateDescriptorPoolForLayout(zest_GetDescriptorSetLayout("2 sampler"), render_target->mip_levels * 2 * ZEST_MAX_FIF + 4, 0);
    zest_ForEachFrameInFlight(fif) {
		zest_vec_resize(render_target->mip_level_descriptor_sets[render_target->current_index][fif], (zest_uint)render_target->mip_levels);
    }
    for (int mip_level = 0; mip_level != render_target->mip_levels; ++mip_level) {
        zest_ForEachFrameInFlight(fif) {
			zest_descriptor_set_builder_t builder = zest_BeginDescriptorSetBuilder(zest_GetDescriptorSetLayout("2 sampler"));
            zest_AddSetBuilderDirectImageWrite(&builder, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &render_target->mip_level_image_infos[fif][mip_level]);
            zest_AddSetBuilderDirectImageWrite(&builder, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &downsampler->image_info[fif]);
            zest_FinishDescriptorSet(draw_commands->descriptor_pool,&builder, &render_target->mip_level_descriptor_sets[render_target->current_index][fif][mip_level]);
        }
    }
    if (zest_Vec2Length2(render_target->create_info.ratio_of_screen_size)) {
        draw_commands->viewport_scale = render_target->create_info.ratio_of_screen_size;
        draw_commands->viewport_type = zest_render_viewport_type_scale_with_window;
    } else if (ZEST__FLAGGED(render_target->create_info.flags, zest_render_target_flag_fixed_size)) {
        draw_commands->viewport_type = zest_render_viewport_type_fixed;
    } else {
        draw_commands->viewport_type = zest_render_viewport_type_scale_with_window;
        draw_commands->viewport_scale = zest_Vec2Set(1.f, 1.f);
    }
    ZestRenderer->setup_context.draw_commands = draw_commands;
    ZestRenderer->setup_context.type = zest_setup_context_type_render_pass;
    return draw_commands;
}

void zest_SetDrawCommandsCallback(void(*render_pass_function)(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer)) {
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
    draw_commands->magic = zest_INIT_MAGIC;
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
    ZEST_ASSERT(strlen(name) <= 50);    //Layer name must be less then 50 characters
    zest_draw_routine_t* draw_routine = { 0 };
    draw_routine = zest__create_draw_routine_with_mesh_layer(name);
    zest_layer layer = (zest_layer)draw_routine->draw_data;
    zest__initialise_mesh_layer(layer, vertex_struct_size, zloc__MEGABYTE(1));
    ZestRenderer->setup_context.layer = (zest_layer)draw_routine->draw_data;
    layer->name = name;
    zest_SetLayerSize(layer, (float)ZestRenderer->swapchain_extent.width, (float)ZestRenderer->swapchain_extent.height);
    return layer;
}

zest_layer zest__create_instance_layer(const char *name, zest_size instance_type_size, zest_uint initial_instance_count) {
    zest_layer layer = zest_NewLayer();
    layer->name = name;
    zest_InitialiseInstanceLayer(layer, instance_type_size, initial_instance_count);
    zest_map_insert(ZestRenderer->layers, name, layer);
    return layer;
}

zest_layer zest_CreateBuiltinSpriteLayer(const char* name) {
    zest_layer layer = zest_NewLayer();
    layer->name = name;
    zest_InitialiseInstanceLayer(layer, sizeof(zest_sprite_instance_t), 1000);
    zest_map_insert(ZestRenderer->layers, name, layer);
    return layer;
}

zest_layer zest_CreateBuiltin3dLineLayer(const char* name) {
    zest_layer layer = zest_NewLayer();
    layer->name = name;
    zest_InitialiseInstanceLayer(layer, sizeof(zest_line_instance_t), 1000);
    zest_map_insert(ZestRenderer->layers, name, layer);
    return layer;
}

zest_layer zest_CreateBuiltinBillboardLayer(const char* name) {
    zest_layer layer = zest_NewLayer();
    layer->name = name;
    zest_InitialiseInstanceLayer(layer, sizeof(zest_billboard_instance_t), 1000);
    zest_map_insert(ZestRenderer->layers, name, layer);
    return layer;
}

zest_layer zest_CreateMeshLayer(const char* name, zest_size vertex_type_size) {
    ZEST_ASSERT(ZestRenderer);  //Initialise Zest first!
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->draw_routines, name));   //A draw routine with that name already exists
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->layers, name));          //A layer with that name already exists
    zest_layer layer = zest_NewLayer();
    layer->name = name;
    zest__initialise_mesh_layer(layer, sizeof(zest_textured_vertex_t), 1000);
    if(ZestRenderer && ZestRenderer->layers.map) zest_map_insert(ZestRenderer->layers, name, layer);
    zest_draw_routine_t blank_draw_routine = { 0 };
    zest_draw_routine draw_routine = ZEST__NEW(zest_draw_routine);
    *draw_routine = blank_draw_routine;
    draw_routine->magic = zest_INIT_MAGIC;
    draw_routine->last_fif = -1;
    draw_routine->fif = ZEST_FIF;
    draw_routine->name = layer->name;
    draw_routine->command_queue = ZestRenderer->setup_context.command_queue;
    draw_routine->recorder = zest_CreateSecondaryRecorder();
    draw_routine->draw_data = layer;
    draw_routine->update_buffers_callback = zest_UploadMeshBuffersCallback;
    draw_routine->record_callback = zest__draw_mesh_layer_callback;
    draw_routine->condition_callback = zest_LayerHasInstructionsCallback;
    zest_map_insert(ZestRenderer->draw_routines, name, draw_routine);
    return layer;
}

zest_layer zest_CreateBuiltinInstanceMeshLayer(const char* name) {
    zest_layer layer = zest_NewLayer();
    layer->name = name;
    zest_InitialiseInstanceLayer(layer, sizeof(zest_mesh_instance_t), 1000);
    zest_map_insert(ZestRenderer->layers, name, layer);
    return layer;
}

void zest_InsertLayer(zest_layer layer) {
    zest_map_insert(ZestRenderer->layers, layer->name, layer);
}

void zest_AddLayer(zest_layer layer) {
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_render_pass || ZestRenderer->setup_context.type == zest_setup_context_type_layer);    //The current setup context must be a render pass, layer or compute
    zest__set_queue_context(zest_setup_context_type_layer);
    zest_command_queue_draw_commands draw_commands = ZestRenderer->setup_context.draw_commands;
    ZestRenderer->setup_context.layer = layer;
    ZestRenderer->setup_context.type = zest_setup_context_type_layer;
    zest_SetLayerSize(layer, (float)draw_commands->viewport.extent.width, (float)draw_commands->viewport.extent.height);
}

zest_command_queue_compute zest_CreateComputeItem(const char* name, zest_command_queue command_queue) {
    zest_command_queue_compute_t blank_compute_item = { 0 };
    zest_command_queue_compute compute_commands = ZEST__NEW(zest_command_queue_compute);
    *compute_commands = blank_compute_item;
    compute_commands->magic = zest_INIT_MAGIC;
    zest_map_insert(ZestRenderer->command_queue_computes, name, compute_commands);
    zest_vec_push(command_queue->compute_commands, compute_commands);
    return compute_commands;
}

zest_command_queue_compute zest_NewComputeSetup(const char* name, zest_compute compute_shader, void(*compute_function)(zest_compute compute), int(*condition_function)(zest_compute compute)) {
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_command_queue);                //Current setup context must be command_queue. Add your compute shaders first before
    //adding any other draw routines. Also make sure that you call zest_FinishQueueSetup everytime
    //everytime you create or modify a queue

    zest_command_queue command_queue = ZestRenderer->setup_context.command_queue;
    zest_command_queue_compute compute_commands = zest_CreateComputeItem(name, command_queue);
    compute_commands->compute_function = compute_function;
    compute_commands->condition_function = condition_function ? condition_function : zest_ComputeConditionAlwaysTrue;
    compute_shader->compute_commands = compute_commands;
    compute_commands->compute = compute_shader;
    return compute_commands;
}

zest_draw_routine zest__create_draw_routine_with_mesh_layer(const char* name) {
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->draw_routines, name));    //A draw routine with that name already exists

    zest_draw_routine_t blank_draw_routine = { 0 };
    zest_draw_routine draw_routine = ZEST__NEW(zest_draw_routine);
    *draw_routine = blank_draw_routine;
    draw_routine->magic = zest_INIT_MAGIC;
    draw_routine->recorder = zest_CreateSecondaryRecorder();
	draw_routine->last_fif = -1;
	draw_routine->fif = ZEST_FIF;

    zest_layer layer = 0;
	layer = zest_CreateMeshLayer(name, sizeof(zest_textured_vertex_t));
	draw_routine->draw_data = layer;
	draw_routine->update_buffers_callback = zest_UploadMeshBuffersCallback;
	draw_routine->record_callback = zest__draw_mesh_layer_callback;
	draw_routine->condition_callback = zest_LayerHasInstructionsCallback;
    draw_routine->name = name;
    draw_routine->command_queue = ZestRenderer->setup_context.command_queue;

    zest_map_insert(ZestRenderer->draw_routines, name, draw_routine);
    ZestRenderer->setup_context.draw_routine = draw_routine;

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

void zest__connect_command_queue_to_present() {
    ZEST_ASSERT(ZestRenderer->setup_context.type != zest_setup_context_type_none);    //Must be within a command queue setup context
    zest_command_queue command_queue = ZestRenderer->setup_context.command_queue;
}

void zest__reset_query_pool(VkCommandBuffer command_buffer, VkQueryPool query_pool, zest_uint count) {
    zest_uint first_query = ZEST_FIF * count;
    vkCmdResetQueryPool(command_buffer, query_pool, first_query, count);
}

VkQueryPool zest__create_query_pool(zest_uint timestamp_count) {
    ZEST_ASSERT(timestamp_count);   //Must specify the number of timestamps required
    VkQueryPoolCreateInfo query_pool_info = { 0 };
    query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    query_pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    query_pool_info.queryCount = timestamp_count * ZEST_MAX_FIF; 
    VkQueryPool query_pool;
    vkCreateQueryPool(ZestDevice->logical_device, &query_pool_info, &ZestDevice->allocation_callbacks, &query_pool);
    return query_pool;
}

zest_timestamp_duration_t zest_CommandQueueRenderTimes(zest_command_queue command_queue) {
    zest_uint index = ZestDevice->previous_fif;
    zest_gpu_timestamp_t *ts = command_queue->timestamps[index];
    zest_timestamp_duration_t duration;
    duration.nanoseconds = (double)(ts->start - ts->end) * ZestDevice->properties.limits.timestampPeriod;
    duration.microseconds = duration.nanoseconds / 1000.0;
    duration.milliseconds = duration.microseconds / 1000.0;
    return duration;
}

zest_draw_routine zest_CreateDrawRoutine(const char* name) {
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->draw_routines, name));    //A draw routine with that name already exists

    zest_draw_routine_t blank_draw_routine = { 0 };
    zest_draw_routine draw_routine = ZEST__NEW(zest_draw_routine);
    *draw_routine = blank_draw_routine;
    draw_routine->magic = zest_INIT_MAGIC;
    draw_routine->last_fif = -1;
	draw_routine->fif = ZEST_FIF;
    draw_routine->recorder = zest_CreateSecondaryRecorder();

    draw_routine->name = name;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Created %s draw routine", name);
    zest_map_insert(ZestRenderer->draw_routines, name, draw_routine);
    return draw_routine;
}

zest_draw_routine zest_CreateInstanceDrawRoutine(const char *name, zest_size instance_size, zest_uint reserve_amount) {
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->draw_routines, name));    //A draw routine with that name already exists
    ZEST_ASSERT(instance_size > 0); //Must be an instace size greater than 0. You should just use sizeof(your_instance_struct)

    zest_draw_routine_t blank_draw_routine = { 0 };
    zest_draw_routine draw_routine = ZEST__NEW(zest_draw_routine);
    *draw_routine = blank_draw_routine;
    draw_routine->magic = zest_INIT_MAGIC;
    draw_routine->last_fif = -1;
	draw_routine->fif = ZEST_FIF;
    zest_layer layer = zest_NewLayer();
    layer->name = name;
    zest_InitialiseInstanceLayer(layer, instance_size, reserve_amount);
    zest_map_insert(ZestRenderer->layers, name, layer);
    draw_routine->record_callback = zest_RecordInstanceLayerCallback;
    draw_routine->condition_callback = zest_LayerHasInstructionsCallback;
    draw_routine->draw_data = layer;
    draw_routine->update_buffers_callback = zest__update_instance_layer_buffers_callback;
    draw_routine->recorder = zest_CreateSecondaryRecorder();

    draw_routine->name = name;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Created %s instanced draw routine", name);
    zest_map_insert(ZestRenderer->draw_routines, name, draw_routine);
    return draw_routine;
}

void zest_AddDrawRoutine(zest_draw_routine draw_routine) {
    ZEST_CHECK_HANDLE(draw_routine);  //Must be valid draw_rouine, use zest_CreateDrawRoutine or zest_CreateInstanceDrawRoutine
    zest__set_queue_context(zest_setup_context_type_layer);
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_render_pass || ZestRenderer->setup_context.type == zest_setup_context_type_layer);    //The current setup context must be a render pass, layer or compute
    zest_command_queue_draw_commands draw_commands = ZestRenderer->setup_context.draw_commands;
    ZestRenderer->setup_context.type = zest_setup_context_type_layer;
    ZestRenderer->setup_context.draw_routine = draw_routine;
    draw_routine->command_queue = ZestRenderer->setup_context.command_queue;
    draw_routine->draw_commands = draw_commands;
    zest_AddDrawRoutineToDrawCommands(draw_commands, draw_routine);
}

zest_layer zest_AddInstanceDrawRoutine(zest_draw_routine draw_routine) {
    zest__set_queue_context(zest_setup_context_type_layer);
    ZEST_CHECK_HANDLE(draw_routine);  //Must be valid draw_rouine, use zest_CreateDrawRoutine or zest_CreateInstanceDrawRoutine
    ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_render_pass || ZestRenderer->setup_context.type == zest_setup_context_type_layer);    //The current setup context must be a render pass, layer or compute
    ZEST_ASSERT(draw_routine->draw_data);   //Draw data must have been set and should be a zest_layer handle
    zest_command_queue_draw_commands draw_commands = ZestRenderer->setup_context.draw_commands;
    ZestRenderer->setup_context.type = zest_setup_context_type_layer;
    ZestRenderer->setup_context.draw_routine = draw_routine;
    draw_routine->command_queue = ZestRenderer->setup_context.command_queue;
    zest_AddDrawRoutineToDrawCommands(draw_commands, draw_routine);
    zest_layer layer = (zest_layer)draw_routine->draw_data;
    draw_routine->draw_commands = draw_commands;
    return layer;
}

void zest_AddDrawRoutineToDrawCommands(zest_command_queue_draw_commands draw_commands, zest_draw_routine draw_routine) {
    ZEST_CHECK_HANDLE(draw_routine);  //Must be valid draw_rouine, use zest_CreateDrawRoutine or zest_CreateInstanceDrawRoutine
    zest_vec_push(draw_commands->draw_routines, draw_routine);
}

void zest_ExecuteRecorderCommands(VkCommandBuffer primary_command_buffer, zest_recorder recorder, zest_uint fif) {
	vkCmdExecuteCommands(primary_command_buffer, 1, &recorder->command_buffer[fif]);
}

zest_layer zest_GetLayer(const char* name) {
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->layers, name));        //That index could not be found in the storage
    return *zest_map_at(ZestRenderer->layers, name);
}

void zest_ResetComputeRoutinesIndex(zest_command_queue_compute compute_queue) {
    compute_queue->shader_index = 0;
}

void zest_RecordCommandQueue(zest_command_queue command_queue, zest_index fif) {
    for (zest_foreach_i(command_queue->compute_commands)) {
        zest_command_queue_compute compute_commands = command_queue->compute_commands[i];
        ZEST_ASSERT(compute_commands->compute_function);        //Compute item must have its compute function callback set
        ZEST_ASSERT(compute_commands->condition_function);      //Compute item must have its condition function callback set. 
        compute_commands->shader_index = 0;
        ZestRenderer->current_compute_routine = compute_commands;
        zest_compute compute = compute_commands->compute;
        ZEST_CHECK_HANDLE(compute); //Not a valid compute handle, was the compute shader for this item setup?
        ZEST_ASSERT(ZEST__NOT_FLAGGED(compute->flags, zest_compute_flag_primary_recorder));
        if (compute_commands->condition_function(compute)) {
            if (compute->recorder->outdated[ZEST_FIF] != 0) {
                compute_commands->compute_function(compute);
            }
            if (ZEST__NOT_FLAGGED(compute->flags, zest_compute_flag_manual_fif)) {
                zest_MarkOutdated(compute->recorder);
                compute_commands->last_fif = compute->fif;
                compute->fif = (compute->fif + 1) % ZEST_MAX_FIF;
            }
            zest_vec_push(command_queue->secondary_compute_command_buffers, compute->recorder->command_buffer[ZEST_FIF]);
        }
    }

    zest_uint command_buffer_count = zest_vec_size(command_queue->secondary_compute_command_buffers);
    if (command_buffer_count > 0) {
        //Note: If you get a validation error here about an empty command queue being executed then make sure that
        //the condition callback for the compute routine is returning the correct value when there's nothing to record.
        //vkCmdExecuteCommands(ZestRenderer->current_command_buffer, command_buffer_count, command_queue->secondary_compute_command_buffers);
		zest_vec_clear(command_queue->secondary_compute_command_buffers);
    }

    //Insert any texture reprocessing in to the command queue
    if (zest_map_size(ZestRenderer->texture_reprocess_queue)) {
        zest_map_foreach(i, ZestRenderer->texture_reprocess_queue) {
            zest_texture texture = *zest_map_at_index(ZestRenderer->texture_reprocess_queue, i);
            zest__process_texture_images(texture, command_queue->recorder->command_buffer[fif]);
            if (texture->reprocess_callback) {
                texture->reprocess_callback(texture, texture->user_data);
                texture->reprocess_callback = 0;
            }
        }
        zest_map_clear(ZestRenderer->texture_reprocess_queue);
    }

    zest_uint current_timestamp = 0;
    for (zest_foreach_i(command_queue->draw_commands)) {
        zest_command_queue_draw_commands draw_commands = command_queue->draw_commands[i];
        ZestRenderer->current_draw_commands = draw_commands;

        if (command_queue->timestamp_count) {
            //vkCmdWriteTimestamp(ZestRenderer->current_command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, command_queue->query_pool, ZEST_FIF * command_queue->timestamp_count + current_timestamp++);
        }
        draw_commands->render_pass_function(draw_commands, command_queue->recorder->command_buffer[fif], draw_commands->render_pass, draw_commands->get_frame_buffer(draw_commands));
        if (command_queue->timestamp_count) {
            //vkCmdWriteTimestamp(ZestRenderer->current_command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, command_queue->query_pool, ZEST_FIF * command_queue->timestamp_count + current_timestamp++);
        }
    }
}

void zest_StartCommandQueue(zest_command_queue command_queue, zest_index fif) {
    if (command_queue->query_state[ZestDevice->previous_fif] == zest_query_state_ready) {
        VkResult result = vkGetQueryPoolResults(ZestDevice->logical_device,
            command_queue->query_pool,
            ZestDevice->previous_fif * command_queue->timestamp_count,
            command_queue->timestamp_count,
            sizeof(zest_u64) * command_queue->timestamp_count,
            command_queue->timestamps[ZEST_FIF],
            sizeof(zest_u64),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT); // Wait ensures results are ready
    }

    VkCommandBufferBeginInfo begin_info = { 0 };
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    ZEST_VK_CHECK_RESULT(vkBeginCommandBuffer(command_queue->recorder->command_buffer[fif], &begin_info));
    //ZestRenderer->current_command_buffer = command_queue->recorder->command_buffer[fif];
    if (command_queue->timestamp_count) {
        zest__reset_query_pool(command_queue->recorder->command_buffer[ZEST_FIF], command_queue->query_pool, command_queue->timestamp_count);
    }
}

void zest_EndCommandQueue(zest_command_queue command_queue, zest_index fif) {
    if (command_queue->timestamp_count) {
        command_queue->query_state[fif] = zest_query_state_ready;
    }
    ZEST_VK_CHECK_RESULT(vkEndCommandBuffer(command_queue->recorder->command_buffer[fif]));
    ZestRenderer->current_compute_routine = ZEST_NULL;
    ZestRenderer->current_draw_commands = ZEST_NULL;
}

void zest_SubmitCommandQueue(zest_command_queue command_queue, VkFence fence) {
    VkSubmitInfo submit_info = { 0 };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pWaitSemaphores = &ZestRenderer->image_available_semaphore[ZEST_FIF];
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitDstStageMask = command_queue->fif_wait_stage_flags[ZEST_FIF];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &ZestRenderer->render_finished_semaphore[ZestRenderer->current_image_frame];
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_queue->recorder->command_buffer[ZEST_FIF];

    vkResetFences(ZestDevice->logical_device, 1, &fence);
    ZEST_VK_CHECK_RESULT(vkQueueSubmit(ZestDevice->graphics_queue.vk_queue, 1, &submit_info, fence));
}
// --End Command queue setup and modify functions

// --Texture and Image functions
zest_map_textures* zest_GetTextures() {
    return &ZestRenderer->textures;
}

zest_texture zest_NewTexture() {
    zest_texture_t blank = { 0 };
    zest_texture texture = ZEST__NEW_ALIGNED(zest_texture, 16);
    *texture = blank;
    texture->magic = zest_INIT_MAGIC;
    texture->name.str = 0;
    texture->struct_type = zest_struct_type_texture;
    texture->flags = zest_texture_flag_use_filtering;
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
    texture->texture_layer_size = 1024;
    texture->descriptor_array_index = ZEST_INVALID;

    return texture;
}

zest_image_t zest_NewImage(void) {
    zest_image_t image = { 0 };
    image.magic = zest_INIT_MAGIC;
    image.struct_type = zest_struct_type_image;
    return image;
}

zest_imgui_image_t zest_NewImGuiImage(void) {
    zest_imgui_image_t imgui_image = { 0 };
    imgui_image.magic = zest_INIT_MAGIC;
    imgui_image.struct_type = zest_struct_type_imgui_image;
    return imgui_image;
}

zest_image zest_CreateImage() {
    zest_image_t blank_image = zest_NewImage();
    zest_image image = ZEST__NEW_ALIGNED(zest_image, 16);
    *image = blank_image;
    image->magic = zest_INIT_MAGIC;
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
    ZEST_ASSERT(image); //Couldn't allocate the image. Out of memory?
    image->frames = frames;
    return image;
}

void zest_AllocateBitmap(zest_bitmap_t* bitmap, int width, int height, int channels, zest_color fill_color) {
    bitmap->size = width * height * channels;
    if (bitmap->size > 0) {
        bitmap->data = (zest_byte*)ZEST__ALLOCATE(bitmap->size);
        bitmap->width = width;
        bitmap->height = height;
        bitmap->channels = channels;
        bitmap->stride = width * channels;
    }
    zest_FillBitmap(bitmap, fill_color);
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
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        zest_ConvertBitmapToRGBA(src, 255);
        break;
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
        zest_ConvertBitmapToBGRA(src, 255);
        break;
    case VK_FORMAT_R8_UNORM:
        zest_ConvertBitmapTo1Channel(src);
        break;
    default:
        ZEST_ASSERT(0);    //Unknown texture format
    }
}

void zest_ConvertBitmapTo1Channel(zest_bitmap_t* image) {
    if (image->channels == 1) {
        return;
    }

    zest_bitmap_t converted = { 0 };
    zest_AllocateBitmap(&converted, image->width, image->height, 1, zest_ColorSet1(0));
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

void zest_PlotBitmap(zest_bitmap_t *image, int x, int y, zest_color color) {

    size_t pos = y * image->stride + (x * image->channels);

    if (pos >= image->size) {
        return;
    }

    if (image->channels == 4) {
		*(image->data + pos) = color.r;
		*(image->data + pos + 1) = color.g;
		*(image->data + pos + 2) = color.b;
		*(image->data + pos + 3) = color.a;
    }
    else if (image->channels == 3) {
		*(image->data + pos) = color.r;
		*(image->data + pos + 1) = color.g;
		*(image->data + pos + 2) = color.b;
    }
    else if (image->channels == 2) {
		*(image->data + pos) = color.r;
		*(image->data + pos + 3) = color.a;
    }
    else if (image->channels == 1) {
		*(image->data + pos) = color.r;
    }

}

void zest_FillBitmap(zest_bitmap_t *image, zest_color color) {

    zest_size pos = 0;

    if (image->channels == 4) {
        while (pos < image->size) {
            *(image->data + pos) = color.r;
            *(image->data + pos + 1) = color.g;
            *(image->data + pos + 2) = color.b;
            *(image->data + pos + 3) = color.a;
            pos += image->channels;
        }
    }
    else if (image->channels == 3) {
        while (pos < image->size) {
            *(image->data + pos) = color.r;
            *(image->data + pos + 1) = color.g;
            *(image->data + pos + 2) = color.b;
            pos += image->channels;
        }
    }
    else if (image->channels == 2) {
        while (pos < image->size) {
            *(image->data + pos) = color.r;
            *(image->data + pos + 3) = color.a;
            pos += image->channels;
        }
    }
    else if (image->channels == 1) {
        while (pos < image->size) {
            *(image->data + pos) = color.r;
            pos += image->channels;
        }
    }
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
    if (format == zest_texture_format_bgra_unorm) {
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
    zest_ConvertBitmap(src, zest_texture_format_bgra_unorm, alpha_level);
}

void zest_ConvertBitmapToRGBA(zest_bitmap_t* src, zest_byte alpha_level) {
    zest_ConvertBitmap(src, zest_texture_format_rgba_unorm, alpha_level);
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
    if (texture->storage_type == zest_texture_storage_type_single) {
        if (zest_vec_size(texture->image_bitmaps)) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "INFO: Added image to single texture %s but this texture already has an image bitmap. This bitmap gets replaced with the new image (%s) you just added.", texture->name.str, filename);
			zest_FreeTextureBitmaps(texture);
        }
    }
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
    if (texture->storage_type == zest_texture_storage_type_single) {
        if (zest_vec_size(texture->image_bitmaps)) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "INFO: Added image to single texture %s but this texture already has an image bitmap. This bitmap gets replaced with the new image (%s) you just added.", texture->name.str, bitmap_to_load->name.str ? bitmap_to_load->name.str : "has no name");
			zest_FreeTextureBitmaps(texture);
        }
    }
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
    if (texture->storage_type == zest_texture_storage_type_single) {
        if (zest_vec_size(texture->image_bitmaps)) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "INFO: Added image to single texture %s but this texture already has an image bitmap. This bitmap gets replaced with the new image (%s) you just added.", texture->name.str, name ? name : "has no name");
			zest_FreeTextureBitmaps(texture);
        }
    }
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
    blank_image.magic = zest_INIT_MAGIC;
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
            zest_AllocateBitmap(image_bitmap, width, height, spritesheet->channels, zest_ColorSet1(0));
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
    zest_vec_free(texture->buffer_copy_regions);
    ZEST__UNFLAG(texture->flags, zest_texture_flag_ready);
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
	if(texture->image_buffer.base_view) vkDestroyImageView(ZestDevice->logical_device, texture->image_buffer.base_view, &ZestDevice->allocation_callbacks);
	if(texture->image_buffer.image) vkDestroyImage(ZestDevice->logical_device, texture->image_buffer.image, &ZestDevice->allocation_callbacks);
	if(texture->image_buffer.buffer) zest_FreeBuffer(texture->image_buffer.buffer);
    if (ZEST_VALID_HANDLE(texture->debug_set) && texture->debug_set->vk_descriptor_set) vkFreeDescriptorSets(ZestDevice->logical_device, ZestRenderer->texture_debug_layout->pool->vk_descriptor_pool, 1, &texture->debug_set->vk_descriptor_set);
	texture->image_buffer.buffer = 0;
	texture->image_buffer.base_view = VK_NULL_HANDLE;
	texture->image_buffer.image = VK_NULL_HANDLE;
    if(ZEST_VALID_HANDLE(texture->debug_set)) ZEST__FREE(texture->debug_set);
    texture->flags &= ~zest_texture_flag_ready;
}

void zest__reindex_texture_images(zest_texture texture) {
    zest_index index = 0;
    for (zest_foreach_i(texture->images)) {
        texture->images[i]->index = index++;
    }
    texture->image_index = index;
}

void zest__create_texture_debug_set(zest_texture texture) {
	zest_descriptor_set set = texture->debug_set;
    zest_descriptor_set_builder_t set_builder = zest_BeginDescriptorSetBuilder(ZestRenderer->texture_debug_layout);
	zest_AddSetBuilderCombinedImageSampler(&set_builder, 0, 0, texture->sampler->vk_sampler, texture->descriptor_image_info.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    texture->debug_set = zest_FinishDescriptorSet(ZestRenderer->texture_debug_layout->pool, &set_builder, set);
}

void zest__process_texture_images(zest_texture texture, VkCommandBuffer command_buffer) {
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

        zest__create_texture_image_array(texture, mip_levels, command_buffer);
        zest__create_texture_image_view(texture, texture->image_view_type, mip_levels, texture->layer_count);

        texture->descriptor_image_info.imageView = texture->image_buffer.base_view;
        texture->descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        texture->flags |= zest_texture_flag_ready;
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
            zest__create_texture_image(texture, mip_levels, flags, VK_IMAGE_LAYOUT_GENERAL, ZEST_TRUE, command_buffer);
            texture->descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        else {
            zest__create_texture_image(texture, mip_levels, flags, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ZEST_TRUE, command_buffer);
            texture->descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        zest__create_texture_image_view(texture, texture->image_view_type, mip_levels, texture->layer_count);

        texture->descriptor_image_info.imageView = texture->image_buffer.base_view;
        texture->flags |= zest_texture_flag_ready;

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
            zest__create_texture_image(texture, mip_levels, flags, VK_IMAGE_LAYOUT_GENERAL, ZEST_TRUE, command_buffer);
            texture->descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        else {
            zest__create_texture_image(texture, mip_levels, flags, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ZEST_TRUE, command_buffer);
            texture->descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        zest__create_texture_image_view(texture, texture->image_view_type, mip_levels, texture->layer_count);

        texture->descriptor_image_info.imageView = texture->image_buffer.base_view;
        texture->flags |= zest_texture_flag_ready;

    }
    else if (texture->storage_type == zest_texture_storage_type_storage) {
        mip_levels = 1;
        texture->layer_count = 1;

        zest__create_texture_image(texture, mip_levels, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_LAYOUT_GENERAL, ZEST_FALSE, command_buffer);
        zest__create_texture_image_view(texture, texture->image_view_type, mip_levels, texture->layer_count);

        texture->descriptor_image_info.imageView = texture->image_buffer.base_view;
        texture->descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        texture->flags |= zest_texture_flag_ready;
    }

    texture->mip_levels = mip_levels;
	texture->sampler_info.maxLod = (float)mip_levels - 1;
	texture->sampler = zest_GetSampler(&texture->sampler_info);
    zest__create_texture_debug_set(texture);

    zest__delete_texture_layers(texture);
}
// --End Internal Texture functions

void zest_ProcessTextureImages(zest_texture texture) {
    zest__process_texture_images(texture, 0);
}

zest_uint zest_GetTextureDescriptorIndex(zest_texture texture) {
    return texture->descriptor_array_index;
}

void zest__delete_texture_layers(zest_texture texture) {
    for (zest_foreach_i(texture->layers)) {
        zest_FreeBitmap(&texture->layers[i]);
    }
    zest_vec_free(texture->layers);
}

VkDescriptorImageInfo *zest_GetTextureDescriptorImageInfo(zest_texture texture) {
    return &texture->descriptor_image_info;
}

void zest_ScheduleTextureReprocess(zest_texture texture, void(*callback)(zest_texture texture, void *user_data)) {
    texture->reprocess_callback = callback;
    zest_map_insert(ZestRenderer->texture_reprocess_queue, texture->name.str, texture);
}

void zest_ScheduleTextureCleanOldBuffers(zest_texture texture) {
    zest_vec_push(ZestRenderer->texture_cleanup_queue, texture);
}

void zest_SchedulePipelineRecreate(zest_pipeline pipeline) {
    zest_vec_push(ZestRenderer->pipeline_recreate_queue, pipeline);
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

void zest__create_texture_image(zest_texture texture, zest_uint mip_levels, VkImageUsageFlags usage_flags, VkImageLayout image_layout, zest_bool copy_bitmap, VkCommandBuffer command_buffer) {
    int channels = texture->color_channels;
    VkDeviceSize image_size = zest_GetTextureSingleBitmap(texture)->width * zest_GetTextureSingleBitmap(texture)->height * channels;

    zest_buffer staging_buffer = 0;
    if (copy_bitmap) {
        zest_buffer_info_t buffer_info = zest_CreateStagingBufferInfo();
        staging_buffer = zest_CreateBuffer(image_size, &buffer_info, ZEST_NULL);
        memcpy(staging_buffer->data, zest_GetTextureSingleBitmap(texture)->data, (zest_size)image_size);
    }

    texture->image_buffer.buffer = zest__create_image(zest_GetTextureSingleBitmap(texture)->width, zest_GetTextureSingleBitmap(texture)->height, mip_levels, VK_SAMPLE_COUNT_1_BIT, texture->image_format, VK_IMAGE_TILING_OPTIMAL, usage_flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texture->image_buffer.image);

    texture->image_buffer.format = texture->image_format;
    if (copy_bitmap) {
        zest__transition_image_layout(texture->image_buffer.image, texture->image_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip_levels, 1, command_buffer);
        zest__copy_buffer_to_image(*zest_GetBufferDeviceBuffer(staging_buffer), staging_buffer->memory_offset, texture->image_buffer.image, (zest_uint)zest_GetTextureSingleBitmap(texture)->width, (zest_uint)zest_GetTextureSingleBitmap(texture)->height, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, command_buffer);
		if (!command_buffer) {
			zloc_FreeRemote(staging_buffer->buffer_allocator->allocator, staging_buffer);
		} else {
			zest_vec_push(ZestRenderer->staging_buffers, staging_buffer);
		}
        zest__generate_mipmaps(texture->image_buffer.image, texture->image_format, zest_GetTextureSingleBitmap(texture)->width, zest_GetTextureSingleBitmap(texture)->height, mip_levels, 1, image_layout, command_buffer);
    }
    else {
        zest__transition_image_layout(texture->image_buffer.image, texture->image_format, VK_IMAGE_LAYOUT_UNDEFINED, image_layout, mip_levels, 1, command_buffer);
    }
}

void zest__create_texture_image_array(zest_texture texture, zest_uint mip_levels, VkCommandBuffer command_buffer) {
    VkDeviceSize image_size = texture->bitmap_array.total_mem_size;
    
    zest_buffer_info_t buffer_info = zest_CreateStagingBufferInfo();
    zest_buffer staging_buffer = zest_CreateBuffer(image_size, &buffer_info, ZEST_NULL);

    //memcpy(staging_buffer.mapped, texture.TextureArray().data(), texture.TextureArray().size());
    memcpy(staging_buffer->data, texture->bitmap_array.data, texture->bitmap_array.total_mem_size);

    //texture.FrameBuffer().allocation_id = CreateImageArray(texture.TextureArray().extent().x, texture.TextureArray().extent().y, mip_levels, texture.LayerCount(), VK_SAMPLE_COUNT_1_BIT, image_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture.FrameBuffer().image);
    texture->image_buffer.buffer = zest__create_image_array(texture->bitmap_array.width, texture->bitmap_array.height, mip_levels, texture->layer_count, VK_SAMPLE_COUNT_1_BIT, texture->image_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texture->image_buffer.image);
    texture->image_buffer.format = texture->image_format;

    zest__transition_image_layout(texture->image_buffer.image, texture->image_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip_levels, texture->layer_count, command_buffer);
    zest__copy_buffer_regions_to_image(texture->buffer_copy_regions, *zest_GetBufferDeviceBuffer(staging_buffer), staging_buffer->memory_offset, texture->image_buffer.image, command_buffer);

    if (!command_buffer) {
        zloc_FreeRemote(staging_buffer->buffer_allocator->allocator, staging_buffer);
    } else {
        zest_vec_push(ZestRenderer->staging_buffers, staging_buffer);
    }

    zest__generate_mipmaps(texture->image_buffer.image, texture->image_format, texture->bitmap_array.width, texture->bitmap_array.height, mip_levels, texture->layer_count, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, command_buffer);
}

void zest__create_texture_image_view(zest_texture texture, VkImageViewType view_type, zest_uint mip_levels, zest_uint layer_count) {
    texture->image_buffer.base_view = zest__create_image_view(texture->image_buffer.image, texture->image_format, VK_IMAGE_ASPECT_COLOR_BIT, mip_levels, 0, view_type, layer_count);
}

VkSampler zest__create_sampler(VkSamplerCreateInfo sampler_info) {
    VkSampler sampler;
    ZEST_VK_CHECK_RESULT(vkCreateSampler(ZestDevice->logical_device, &sampler_info, &ZestDevice->allocation_callbacks, &sampler));
    return sampler;
}

zest_frame_buffer_t zest_CreateFrameBuffer(VkRenderPass render_pass, int mip_levels, zest_uint width, zest_uint height, VkFormat render_format, zest_bool use_depth_buffer, zest_bool is_src) {
    ZEST_ASSERT(mip_levels);    //Mip levels must be at least 1 or more
    zest_frame_buffer_t frame_buffer = { 0 };
    frame_buffer.format = render_format;
    frame_buffer.width = width;
    frame_buffer.height = height;
    VkFormat color_format = frame_buffer.format;

    VkImageUsageFlags usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (is_src) {
        usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    frame_buffer.color_buffer.buffer = zest__create_image(frame_buffer.width, frame_buffer.height, mip_levels, VK_SAMPLE_COUNT_1_BIT, color_format, VK_IMAGE_TILING_OPTIMAL, usage_flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &frame_buffer.color_buffer.image);
    frame_buffer.color_buffer.base_view = zest__create_image_view(frame_buffer.color_buffer.image, color_format, VK_IMAGE_ASPECT_COLOR_BIT, mip_levels, 0, VK_IMAGE_VIEW_TYPE_2D, 1);

    zest__transition_image_layout(frame_buffer.color_buffer.image, color_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mip_levels, 1, 0);

    VkFormat depth_format = zest__find_depth_format();

    if (use_depth_buffer) {
        frame_buffer.depth_buffer.buffer = zest__create_image(frame_buffer.width, frame_buffer.height, 1, VK_SAMPLE_COUNT_1_BIT, depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &frame_buffer.depth_buffer.image);
        frame_buffer.depth_buffer.base_view = zest__create_image_view(frame_buffer.depth_buffer.image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 0, VK_IMAGE_VIEW_TYPE_2D, 1);
        zest__transition_image_layout(frame_buffer.depth_buffer.image, depth_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, 1, 0);
    }

	VkImageView *fb_attachments = 0;
    if (mip_levels == 1) {
        zest_vec_push(fb_attachments, frame_buffer.color_buffer.base_view);
        if (use_depth_buffer) {
            zest_vec_push(fb_attachments, frame_buffer.depth_buffer.base_view);
        }

        VkFramebufferCreateInfo frame_buffer_create_info = { 0 };
        frame_buffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frame_buffer_create_info.renderPass = render_pass;
        frame_buffer_create_info.attachmentCount = (zest_uint)(zest_vec_size(fb_attachments));
        frame_buffer_create_info.pAttachments = fb_attachments;
        frame_buffer_create_info.width = frame_buffer.width;
        frame_buffer_create_info.height = frame_buffer.height;
        frame_buffer_create_info.layers = 1;

        zest_vec_push(frame_buffer.framebuffers, 0);

        ZEST_VK_CHECK_RESULT(vkCreateFramebuffer(ZestDevice->logical_device, &frame_buffer_create_info, &ZestDevice->allocation_callbacks, &zest_vec_back(frame_buffer.framebuffers)));
    } else {
        zest_uint current_mip_width = width; 
        zest_uint current_mip_height = height; 
        for (int mip_level = 0; mip_level != mip_levels; ++mip_level) {

			VkImageView mip_view = zest__create_image_view(frame_buffer.color_buffer.image, color_format, VK_IMAGE_ASPECT_COLOR_BIT, 1, mip_level, VK_IMAGE_VIEW_TYPE_2D, 1);
			zest_vec_push(frame_buffer.color_buffer.mip_views, mip_view);

            if (mip_level > 0) { 
                current_mip_width = ZEST__MAX(1u, current_mip_width / 2);
                current_mip_height = ZEST__MAX(1u, current_mip_height / 2);
            }

            zest_vec_push(fb_attachments, mip_view);
            //Note no depth view if there's more than one mip level

            VkFramebufferCreateInfo frame_buffer_create_info = { 0 };
            frame_buffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            frame_buffer_create_info.renderPass = render_pass;
            frame_buffer_create_info.attachmentCount = 1;
            frame_buffer_create_info.pAttachments = fb_attachments;
            frame_buffer_create_info.width = current_mip_width;
            frame_buffer_create_info.height = current_mip_height;
            frame_buffer_create_info.layers = 1;

            zest_vec_push(frame_buffer.framebuffers, 0);

            ZEST_VK_CHECK_RESULT(vkCreateFramebuffer(ZestDevice->logical_device, &frame_buffer_create_info, &ZestDevice->allocation_callbacks, &zest_vec_back(frame_buffer.framebuffers)));

            zest_vec_clear(fb_attachments);
        }
    }

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
        image->uv_packed = zest_Pack16bit4SNorm(image->uv.x, image->uv.y, image->uv.z, image->uv.w);
        image->left = 0;
        image->top = 0;
        image->layer = id;
        image->texture = texture;

        zest_bitmap_t tmp_image = zest_NewBitmap();
        zest_AllocateBitmap(&tmp_image, size, size, texture->color_channels, zest_ColorSet1(0));

        if (image->width != size || image->height != size) {
            zest_bitmap_t *image_bitmap = &texture->image_bitmaps[image->index];
            stbir_resize_uint8(image_bitmap->data, image_bitmap->width, image_bitmap->height, image_bitmap->stride, tmp_image.data, size, size, tmp_image.stride, 4);
        } else {
            zest_bitmap_t *image_bitmap = &texture->image_bitmaps[image->index];
            zest_CopyBitmap(image_bitmap, 0, 0, size, size, &tmp_image, 0, 0);
        }

        tmp_image.width = size;
        tmp_image.height = size;

        zloc_SafeCopyBlock(texture->bitmap_array.data, zest_BitmapArrayLookUp(&texture->bitmap_array, id), tmp_image.data, texture->bitmap_array.size_of_each_image);
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
		ZEST_ASSERT(image->width <= texture->texture_layer_size && image->height <= texture->texture_layer_size);   //This image will not pack into the texture because it's bigger then the texture layer size.
																													//Use zest_SetTextureLayerSize to set a bigger layer size after creating the texture.
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
    zest_AllocateBitmap(&texture->texture_bitmap, size, size, texture->color_channels, zest_ColorSet1(0));
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
            image->uv_packed = zest_Pack16bit4SNorm(image->uv.x, image->uv.y, image->uv.z, image->uv.w);
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
		ZEST_ASSERT(image->width <= texture->texture_layer_size && image->height <= texture->texture_layer_size);   //This image will not pack into the texture because it's bigger then the texture layer size.
																													//Use zest_SetTextureLayerSize to set a bigger layer size after creating the texture.
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
        zest_AllocateBitmap(&tmp_image, size, size, texture->color_channels, zest_ColorSet1(0));
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
                image->uv_packed = zest_Pack16bit4SNorm(image->uv.x, image->uv.y, image->uv.z, image->uv.w);
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

        zloc_SafeCopyBlock(texture->bitmap_array.data, zest_BitmapArrayLookUp(&texture->bitmap_array, current_layer), tmp_image.data, texture->bitmap_array.size_of_each_image);

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
	image.uv_packed = zest_Pack16bit4SNorm(image.uv.x, image.uv.y, image.uv.z, image.uv.w);
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

zest_texture zest_ReplaceTexture(zest_texture texture, zest_texture_storage_type storage_type, zest_bool use_filtering, zest_texture_format image_format, zest_uint reserve_images) {
    ZEST_CHECK_HANDLE(texture); //Not a valid texture handle
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->textures, texture->name.str));    //Texture must exist in the texture storage
    zest_texture new_texture = zest_NewTexture();
    zest_SetText(&new_texture->name, texture->name.str);
    zest_SetTextureImageFormat(new_texture, image_format);
    if (storage_type < zest_texture_storage_type_render_target) {
        zest_SetTextureStorageType(new_texture, storage_type);
        zest_SetTextureUseFiltering(new_texture, use_filtering);
    }
    if (storage_type == zest_texture_storage_type_single || storage_type == zest_texture_storage_type_bank) {
        zest_SetTextureWrappingRepeat(new_texture);
    }
    if (reserve_images) {
        zest_vec_reserve(new_texture->images, reserve_images);
    }        
    zest_map_insert(ZestRenderer->textures, texture->name.str, new_texture);
    zest_vec_push(ZestRenderer->deferred_resource_freeing_list.textures[ZEST_FIF], texture);
    return new_texture;
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
    zest__process_texture_images(texture, 0);
    return texture;
}

void zest_DeleteTexture(zest_texture texture) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    zest_vec_push(ZestRenderer->deferred_resource_freeing_list.textures[ZEST_FIF], texture);
}

void zest_ResetTexture(zest_texture texture) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    zest__free_all_texture_images(texture);
    zest__cleanup_texture(texture);
}

void zest_FreeTextureBitmaps(zest_texture texture) {
    zest__free_all_texture_images(texture);
}

void zest_RemoveTextureImage(zest_texture texture, zest_image image) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    ZEST_ASSERT((zest_uint)image->index < zest_vec_size(texture->images));    //Image not within range of image indexes in the texture
    zest_FreeBitmap(&texture->image_bitmaps[image->index]);
    zest_vec_erase(texture->image_bitmaps, &texture->image_bitmaps[image->index]);
    zest_vec_erase(texture->images, &texture->images[image->index]);
    ZEST__FREE(image);
    zest__reindex_texture_images(texture);
}

void zest_RemoveTextureAnimation(zest_texture texture, zest_image first_image) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
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
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    ZEST_ASSERT(zest_vec_size(texture->images) == 0);    //You cannot change the image format of a texture that already has images
    texture->image_format = format;
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB: 
    case VK_FORMAT_R16G16B16A16_SFLOAT: {
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

void zest_SetTextureUserData(zest_texture texture, void *user_data) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    texture->user_data = user_data;
}

void zest_SetTextureReprocessCallback(zest_texture texture, void(*callback)(zest_texture texture, void *user_data)) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    texture->reprocess_callback = callback;
}

void zest_SetTextureCleanupCallback(zest_texture texture, void(*callback)(zest_texture texture, void *user_data)) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    texture->reprocess_callback = callback;
}

zest_byte* zest_BitmapArrayLookUp(zest_bitmap_array_t* bitmap_array, zest_index index) {
    ZEST_ASSERT((zest_uint)index < bitmap_array->size_of_array);
    return bitmap_array->data + index * bitmap_array->size_of_each_image;
}

zest_image zest_GetImageFromTexture(zest_texture texture, zest_index index) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    return texture->images[index];
}

zest_texture zest_GetTexture(const char* name) {
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->textures, name));    //That name could not be found in the storage
    return *zest_map_at(ZestRenderer->textures, name);
}

zest_bool zest_TextureCanTile(zest_texture texture) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    return texture->storage_type == zest_texture_storage_type_bank || texture->storage_type == zest_texture_storage_type_single;
}

void zest_SetTextureWrapping(zest_texture texture, VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    texture->sampler_info.addressModeU = u;
    texture->sampler_info.addressModeV = v;
    texture->sampler_info.addressModeW = w;
}

void zest_SetTextureWrappingClamp(zest_texture texture) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    texture->sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texture->sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texture->sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}

void zest_SetTextureWrappingBorder(zest_texture texture) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    texture->sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    texture->sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    texture->sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
}

void zest_SetTextureWrappingRepeat(zest_texture texture) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    texture->sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    texture->sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    texture->sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

void zest_SetTextureLayerSize(zest_texture texture, zest_uint size) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    zest_uint next_pow2 = (zest_uint)ZEST__NEXTPOW2(size);
    ZEST_ASSERT(ZEST__POW2(next_pow2));
    texture->texture_layer_size = next_pow2;
}

void zest_SetTextureMaxRadiusOnLoad(zest_texture texture, zest_bool yesno) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    if (yesno) {
        ZEST__FLAG(texture->flags, zest_texture_flag_get_max_radius);
    }
    else {
        ZEST__UNFLAG(texture->flags, zest_texture_flag_get_max_radius);
    }
}

void zest_SetTextureImguiBlendType(zest_texture texture, zest_imgui_blendtype blend_type) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    texture->imgui_blend_type = blend_type;
}

void zest_TextureResize(zest_texture texture, zest_uint width, zest_uint height) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    ZEST_ASSERT(width > 0 && height > 0);
    ZEST_ASSERT(texture->storage_type == zest_texture_storage_type_single || texture->storage_type == zest_texture_storage_type_storage);
    zest_ResetTexture(texture);
    texture->texture.width = width;
    texture->texture.height = height;
    zest__update_image_vertices(&texture->texture);
    zest_GetTextureSingleBitmap(texture)->width = width;
    zest_GetTextureSingleBitmap(texture)->height = height;
    ZEST__UNFLAG(texture->flags, zest_texture_flag_ready);
    zest__process_texture_images(texture, 0);
}

void zest_TextureClear(zest_texture texture) {
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
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
        texture->image_buffer.image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        texture->image_layout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_sub_resource_range);

    vkCmdClearColorImage(command_buffer, texture->image_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &ClearColorValue, 1, &image_sub_resource_range);

    zest__insert_image_memory_barrier(
        command_buffer,
        texture->image_buffer.image,
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
    ZEST_CHECK_HANDLE(texture);	//Not a valid handle!
    VkCommandBuffer copy_command = zest__begin_single_time_commands();

    VkImageLayout target_layout = texture->descriptor_image_info.imageLayout;

    VkImageSubresourceRange image_range = { 0 };
    image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_range.baseMipLevel = 0;
    image_range.levelCount = 1;
    image_range.baseArrayLayer = 0;
    image_range.layerCount = 1;

    // Transition destination image to transfer destination layout
    zest__insert_image_memory_barrier(
        copy_command,
        texture->image_buffer.image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        texture->descriptor_image_info.imageLayout,
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
        texture->image_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &image_blit,
        VK_FILTER_LINEAR);

    // Transition destination image to general layout, which is the required layout for mapping the image memory later on
    zest__insert_image_memory_barrier(
        copy_command,
        texture->image_buffer.image,
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
    ZEST_CHECK_HANDLE(src_image);	//Not a valid handle!
    ZEST_CHECK_HANDLE(target);	    //Not a valid handle!
    VkCommandBuffer copy_command = zest__begin_single_time_commands();

    VkImageLayout target_layout = target->descriptor_image_info.imageLayout;

    VkImageSubresourceRange image_range = { 0 };
    image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_range.baseMipLevel = 0;
    image_range.levelCount = 1;
    image_range.baseArrayLayer = 0;
    image_range.layerCount = 1;

    // Transition destination image to transfer destination layout
    zest__insert_image_memory_barrier(
        copy_command,
        target->image_buffer.image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        target->descriptor_image_info.imageLayout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    // Transition swapchain image from present to transfer source layout
    zest__insert_image_memory_barrier(
        copy_command,
        src_image->image_buffer.image,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        src_image->descriptor_image_info.imageLayout,
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
        src_image->image_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        target->image_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &image_blit_region,
        VK_FILTER_NEAREST);

    // Transition destination image to general layout, which is the required layout for mapping the image memory later on
    zest__insert_image_memory_barrier(
        copy_command,
        target->image_buffer.image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        target_layout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    zest__insert_image_memory_barrier(
        copy_command,
        src_image->image_buffer.image,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src_image->descriptor_image_info.imageLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    zest__end_single_time_commands(copy_command);

}

void zest_CopyTextureToBitmap(zest_texture src_image, zest_bitmap_t* image, int src_x, int src_y, int dst_x, int dst_y, int width, int height, zest_bool swap_channel) {
    ZEST_CHECK_HANDLE(src_image);	//Not a valid handle!
    VkCommandBuffer copy_command = zest__begin_single_time_commands();
    VkImage temp_image;
    VkDeviceMemory temp_memory;
    zest__create_temporary_image(width, height, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &temp_image, &temp_memory);

    zest_bool supports_blit = zest__has_blit_support(src_image->image_buffer.format);

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
        src_image->image_buffer.image,
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
            src_image->image_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
            src_image->image_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
        src_image->image_buffer.image,
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
    zloc_SafeCopy(image->data, data, image->size);

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
    font->magic = zest_INIT_MAGIC;

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
        c.uv_packed = zest_Pack16bit4SNorm(c.uv.x, c.uv.y, c.uv.z, c.uv.w);
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

    font->texture = zest_CreateTextureSingle(filename, zest_texture_format_rgba_unorm);

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
    ZEST_CHECK_HANDLE(font);	//Not a valid handle!
    zest_DeleteTexture(font->texture);
    zest__delete_font(font);
}

zest_font zest__add_font(zest_font font) {
    ZEST_CHECK_HANDLE(font);	//Not a valid handle!
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->fonts, font->name.str));    //A font already exists with that name
    zest_map_insert(ZestRenderer->fonts, font->name.str, font);
    return font;
}

zest_font zest_GetFont(const char* name) {
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->fonts, name));    //No font found with that index
    return *zest_map_at(ZestRenderer->fonts, name);
}

void zest_UploadInstanceLayerData(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	zest_layer layer = (zest_layer)user_data;
    ZEST_CHECK_HANDLE(layer);   //You must pass in the zest_layer in the user data
    zest_EndInstanceInstructions(layer);

    if (!layer->dirty[layer->fif]) {
        return;
    }

	zest_buffer staging_buffer = layer->memory_refs[layer->fif].staging_instance_data;
	zest_buffer device_buffer = ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif) ? layer->memory_refs[layer->fif].device_vertex_data : layer->vertex_buffer_node->storage_buffer;

	zest_buffer_uploader_t instance_upload = { 0, staging_buffer, device_buffer, 0 };

    layer->dirty[layer->fif] = 0;

	if (staging_buffer->memory_in_use) {
		zest_AddCopyCommand(&instance_upload, staging_buffer, device_buffer, 0);
    } else {
        return;
    }

	zest_uint vertex_size = zest_vec_size(instance_upload.buffer_copies);

	zest_UploadBuffer(&instance_upload, command_buffer);
}


void zest_DrawFonts(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	//Grab the app object from the user_data that we set in the render graph when adding this function callback 
	zest_layer layer = (zest_layer)user_data;
    ZEST_CHECK_HANDLE(layer);       //Not a valid layer. Make sure that you pass in the font layer to the zest_AddPassTask function
	zest_buffer device_buffer = layer->vertex_buffer_node->storage_buffer;
	VkDeviceSize instance_data_offsets[] = { device_buffer->memory_offset };
	vkCmdBindVertexBuffers(command_buffer, 0, 1, &device_buffer->memory_pool->buffer, instance_data_offsets);
    bool has_instruction_view_port = false;

	VkDescriptorSet sets[] = {
		zest_vk_GetGlobalUniformBufferDescriptorSet(),
		zest_vk_GetGlobalBindlessSet()
	};

    for (zest_foreach_i(layer->draw_instructions[layer->fif])) {
        zest_layer_instruction_t* current = &layer->draw_instructions[layer->fif][i];

        if (current->draw_mode == zest_draw_mode_viewport) {
            vkCmdSetViewport(command_buffer, 0, 1, &current->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &current->scissor);
            has_instruction_view_port = true;
            continue;
        } else if(!has_instruction_view_port) {
            vkCmdSetViewport(command_buffer, 0, 1, &layer->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &layer->scissor);
        }

        zest_pipeline pipeline = zest_PipelineWithTemplate(current->pipeline_template, context->render_pass);
        if (pipeline) {
            zest_BindPipeline(command_buffer, pipeline, sets, 2);
        } else {
            continue;
        }

		vkCmdPushConstants(
			command_buffer,
			pipeline->pipeline_layout,
			zest_PipelinePushConstantStageFlags(pipeline, 0),
			zest_PipelinePushConstantOffset(pipeline, 0),
			zest_PipelinePushConstantSize(pipeline, 0),
			&current->push_constant);

        vkCmdDraw(command_buffer, 6, current->total_instances, 0, current->start_index);
    }
	zest_ResetInstanceLayerDrawing(layer);
}

void zest__setup_font_texture(zest_font font) {
    ZEST_CHECK_HANDLE(font);	//Not a valid handle!
    if (ZEST__FLAGGED(font->texture->flags, zest_texture_flag_ready)) {
        zest__cleanup_texture(font->texture);
    }

    zest_SetTextureUseFiltering(font->texture, ZEST_FALSE);
    zest__process_texture_images(font->texture, 0);

    zest_AcquireGlobalCombinedImageSampler(font->texture);

    font->pipeline_template = zest_PipelineTemplate("pipeline_fonts");
	font->shader_resources = zest_CreateShaderResources();
    zest_ForEachFrameInFlight(fif) {
        zest_AddDescriptorSetToResources(font->shader_resources, ZestRenderer->uniform_buffer->descriptor_set[fif], fif);
        zest_AddDescriptorSetToResources(font->shader_resources, ZestRenderer->global_set, fif);
    }
    zest_ValidateShaderResource(font->shader_resources);
}
//-- End Fonts

//Render Targets
void zest__initialise_render_target(zest_render_target render_target, zest_render_target_create_info_t* info) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
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
    render_target->mip_levels = info->mip_levels;

    if (ZEST__FLAGGED(info->flags, zest_render_target_flag_single_frame_buffer_only)) {
        render_target->frames_in_flight = 1;
    }

    render_target->create_info = *info;

    for (zest_index i = 0; i != render_target->frames_in_flight; ++i) {
        zest_frame_buffer_t frame_buffer = { 0 };
        render_target->framebuffers[i] = frame_buffer;
    }

    render_target->render_format = info->render_format;
	render_target->render_pass_info;
	render_target->render_pass_info.render_format = render_target->render_format;
	render_target->render_pass_info.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (render_target->mip_levels > 1) {
        ZEST__FLAG(render_target->flags, zest_render_target_flag_multi_mip);
        render_target->render_pass_info.final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    } else {
        render_target->render_pass_info.final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
	render_target->render_pass_info.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    if (ZEST__FLAGGED(render_target->flags, zest_render_target_flag_use_depth_buffer)) {
		render_target->render_pass_info.depth_buffer = 1;
    }
    else {
		render_target->render_pass_info.depth_buffer = 0;
    }
	render_target->render_pass = zest__get_render_pass_with_info(render_target->render_pass_info);
    for (zest_index i = 0; i != render_target->frames_in_flight; ++i) {
        render_target->framebuffers[i] = zest_CreateFrameBuffer(render_target->render_pass, render_target->mip_levels,
            render_target->render_width, render_target->render_height, info->render_format,
            ZEST__FLAGGED(render_target->flags, zest_render_target_flag_use_depth_buffer), ZEST__FLAGGED(info->flags, zest_render_target_flag_is_src));
    }

    zest_image_t image = zest_NewImage();
    image.width = render_target->sampler_image.width;
    image.height = render_target->sampler_image.width;
    zest__update_image_vertices(&image);
    image.uv.x = 0.f;
    image.uv.y = 0.f;
    image.uv.z = 1.f;
    image.uv.w = 1.f;
	image.uv_packed = zest_Pack16bit4SNorm(image.uv.x, image.uv.y, image.uv.z, image.uv.w);
    image.texture = 0;
    image.layer = 0;
	render_target->sampler_info.addressModeU = render_target->create_info.sampler_address_mode_u;
	render_target->sampler_info.addressModeV = render_target->create_info.sampler_address_mode_v;
	render_target->sampler_info.addressModeW = render_target->create_info.sampler_address_mode_w;
    render_target->sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    render_target->sampler_info.magFilter = VK_FILTER_LINEAR;
    render_target->sampler_info.minFilter = VK_FILTER_LINEAR;
    render_target->sampler_info.anisotropyEnable = VK_FALSE;
    render_target->sampler_info.maxAnisotropy = 1.f;
    render_target->sampler_info.unnormalizedCoordinates = VK_FALSE;
    render_target->sampler_info.compareEnable = VK_FALSE;
    render_target->sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    render_target->sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    render_target->sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    render_target->sampler_info.mipLodBias = 0.f;
    render_target->sampler_info.minLod = 0.0f;
    render_target->sampler_info.maxLod = (float)render_target->mip_levels - 1.f;
    render_target->sampler_info.pNext = VK_NULL_HANDLE;
    render_target->sampler_info.flags = 0;
	render_target->sampler[render_target->current_index] = zest__create_sampler(render_target->sampler_info);

    zest_CreateDescriptorPoolForLayout(zest_GetDescriptorSetLayout("2 sampler"), render_target->mip_levels * 2 * ZEST_MAX_FIF + 4, 0);
	zest__create_render_target_sampler(render_target);

    if (ZEST__FLAGGED(render_target->flags, zest_render_target_flag_multi_mip)) {
        zest__create_mip_level_render_target_samplers(render_target);
    }

    if (info->pipeline) {
		render_target->pipeline_template = info->pipeline;
    } else {
        render_target->pipeline_template = zest_PipelineTemplate("pipeline_swap_chain");
    }

    render_target->recorder = zest_CreateSecondaryRecorder();

    ZEST__FLAG(render_target->flags, zest_render_target_flag_initialised);
}

void zest__create_render_target_sampler(zest_render_target render_target) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    int index = render_target->current_index;
    zest_ForEachFrameInFlight(fif) {
		zest_descriptor_set set = &render_target->sampler_descriptor_set[index][fif];
        if (ZEST__FLAGGED(render_target->create_info.flags, zest_render_target_flag_sampler_size_match_texture)) {
            render_target->sampler_image.width = render_target->create_info.viewport.extent.width;
            render_target->sampler_image.height = render_target->create_info.viewport.extent.height;
        } else {
            render_target->sampler_image.width = zest_GetSwapChainExtent().width;
            render_target->sampler_image.height = zest_GetSwapChainExtent().height;
        }

        render_target->image_info[fif].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        render_target->image_info[fif].imageView = render_target->framebuffers[fif].color_buffer.base_view;
        render_target->image_info[fif].sampler = render_target->sampler[index];

        zest_descriptor_set_builder_t builder = zest_BeginDescriptorSetBuilder(zest_GetDescriptorSetLayout("1 sampler"));
        zest_AddSetBuilderDirectImageWrite(&builder, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &render_target->image_info[fif]);
        set = zest_FinishDescriptorSet(render_target->descriptor_pool[index], &builder, set);
    }
}

void zest__create_mip_level_render_target_samplers(zest_render_target render_target) {
    //When downsampling and writing from one mip to the next, we need a descriptor set for each
    //mip level image view otherwise the validation layer will complain that image layout is incorrect
    //because the layer that's being written to is in the wrong layout even though we're not actually 
    //reading from it.
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    int index = render_target->current_index;

    zest_ForEachFrameInFlight(fif) {
		zest_vec_resize(render_target->mip_level_samplers[index][fif], (zest_uint)render_target->mip_levels);
        zest_vec_resize(render_target->mip_level_image_infos[fif], (zest_uint)render_target->mip_levels);
    }

	zest_uint current_mip_width = render_target->render_width; 
	zest_uint current_mip_height = render_target->render_height; 

    for (zest_index mip_level = 0; mip_level != render_target->mip_levels; ++mip_level) {
		if (mip_level > 0) { 
			current_mip_width = ZEST__MAX(1u, current_mip_width / 2);
			current_mip_height = ZEST__MAX(1u, current_mip_height / 2);
		}

		VkRect2D mip_size = { 0 };
		mip_size.extent.width = current_mip_width;
		mip_size.extent.height = current_mip_height;
		zest_vec_push(render_target->mip_level_sizes, mip_size);

		zest_ForEachFrameInFlight(fif) {
			zest_descriptor_set_t blank = { 0 };
			zest_descriptor_set set = &render_target->mip_level_samplers[index][fif][mip_level];
			*set = blank;
			set->magic = zest_INIT_MAGIC;
            VkDescriptorImageInfo *image_info = &render_target->mip_level_image_infos[fif][mip_level];

            image_info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info->imageView = render_target->framebuffers[fif].color_buffer.mip_views[mip_level];
            image_info->sampler = render_target->sampler[index];

            zest_descriptor_set_builder_t builder = zest_BeginDescriptorSetBuilder(zest_GetDescriptorSetLayout("1 sampler"));
            zest_AddSetBuilderDirectImageWrite(&builder, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &render_target->image_info[fif]);
            set = zest_FinishDescriptorSet(render_target->descriptor_pool[index], &builder, set);
        }
    }
}

zest_render_target zest_NewRenderTarget() {
    zest_render_target_t blank_render_target = { 0 };
    zest_render_target render_target = ZEST__NEW_ALIGNED(zest_render_target, 16);
    *render_target = blank_render_target;
    render_target->magic = zest_INIT_MAGIC;
    render_target->render_width = 0;
    render_target->render_height = 0;
    render_target->flags = zest_render_target_flag_render_to_swap_chain;
    render_target->post_process_record_callback = 0;
    render_target->post_process_user_data = 0;
    render_target->frames_in_flight = ZEST_MAX_FIF;
    render_target->mip_levels = 1;
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
        .render_format = VK_FORMAT_B8G8R8A8_UNORM,
        .viewport = {0},
        .pipeline = 0,
        .mip_levels = 1
    };
    if (ZEST__FLAGGED(ZestRenderer->flags, zest_renderer_flag_has_depth_buffer)) {
        create_info.flags |= zest_render_target_flag_use_depth_buffer;
    }
    return create_info;
}

zest_render_target zest_CreateSimpleRenderTarget(const char* name) {
    zest_render_target_create_info_t info = zest_RenderTargetCreateInfo();
    return zest_CreateRenderTarget(name, info);
}

zest_render_target zest_CreateHDRRenderTarget(const char* name) {
    zest_render_target_create_info_t info = zest_RenderTargetCreateInfo();
    info.render_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    return zest_CreateRenderTarget(name, info);
}

zest_render_target zest_CreateScaledHDRRenderTarget(const char* name, float scale) {
    zest_render_target_create_info_t info = zest_RenderTargetCreateInfo();
    info.render_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    info.ratio_of_screen_size.x = scale;
    info.ratio_of_screen_size.y = scale;
    return zest_CreateRenderTarget(name, info);
}

zest_render_target zest_CreateMippedHDRRenderTarget(const char *name, int mip_levels) {
    ZEST_ASSERT(mip_levels > 1);        //If you're creating a mipped render target then 2 is the mimimum number of mip levels
    zest_render_target_create_info_t info = zest_RenderTargetCreateInfo();
    info.render_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    info.mip_levels = mip_levels;
    return zest_CreateRenderTarget(name, info);
}

zest_render_target zest_CreateRenderTarget(const char* name, zest_render_target_create_info_t create_info) {
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

    ZEST__FLAG(ZestRenderer->flags, zest_renderer_flag_schedule_rerecord_final_render_buffer);

    render_target->create_info = create_info;
    render_target->input_source = create_info.input_source;
    ZEST__FLAG(render_target->flags, create_info.flags);

    zest_map_insert(ZestRenderer->render_targets, name, render_target);

    zest__initialise_render_target(render_target, &render_target->create_info);

    return render_target;
}

zest_render_target zest_AddPostProcessRenderTarget(const char* name, float width_ratio_of_input, float height_ration_of_input, zest_render_target input_source, void* user_data, void(*render_callback)(zest_render_target target, void* user_data, zest_uint fif)) {
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->render_targets, name));    //That render target name already exists!
    zest_render_target_create_info_t create_info = { 0 };
    create_info.ratio_of_screen_size = zest_Vec2Set(width_ratio_of_input, height_ration_of_input);
    create_info.viewport.extent.width = (zest_uint)((float)input_source->render_width * width_ratio_of_input);
    create_info.viewport.extent.height = (zest_uint)((float)input_source->render_height * height_ration_of_input);
    create_info.input_source = input_source;
    create_info.render_format = input_source->render_format;
    create_info.sampler_address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    create_info.sampler_address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    create_info.sampler_address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    create_info.mip_levels = 1;
    ZEST__UNFLAG(create_info.flags, zest_render_target_flag_sampler_size_match_texture);
    zest_render_target target = zest_CreateRenderTarget(name, create_info);
    zest_SetRenderTargetPostProcessCallback(target, render_callback);
    zest_SetRenderTargetPostProcessUserData(target, user_data);
    if (!render_callback) {
        for (ZEST_EACH_FIF_i) {
            zest__record_render_target_commands(target, i);
        }
    } else {
        for (ZEST_EACH_FIF_i) {
            target->recorder->outdated[i] = 1;
        }
    }
    return target;
}

void zest_SetRenderTargetPostProcessCallback(zest_render_target render_target, void(*render_callback)(zest_render_target target, void* user_data, zest_uint fif)) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    render_target->post_process_record_callback = render_callback;
}

void zest_SetRenderTargetPostProcessUserData(zest_render_target render_target, void* user_data) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    render_target->post_process_user_data = user_data;
}

VkDescriptorSet* zest_GetRenderTargetSamplerDescriptorSetVK(zest_render_target render_target) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    return &render_target->sampler_descriptor_set[render_target->current_index][ZEST_FIF].vk_descriptor_set;
}

zest_descriptor_set zest_GetRenderTargetSamplerDescriptorSet(zest_render_target render_target) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    return render_target->sampler_descriptor_set[render_target->current_index];
}

VkDescriptorSet* zest_GetRenderTargetSourceDescriptorSet(zest_render_target render_target) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    return zest_GetRenderTargetSamplerDescriptorSetVK(render_target->input_source);
}

zest_image zest_GetRenderTargetImage(zest_render_target render_target) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    return &render_target->sampler_image;
}

zest_frame_buffer_t* zest_RenderTargetFramebuffer(zest_render_target render_target) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    return &render_target->framebuffers[ZEST_FIF];
}

zest_frame_buffer_t* zest_RenderTargetFramebufferByFIF(zest_render_target render_target, zest_index fif) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    return &render_target->framebuffers[fif];
}

zest_render_target zest_GetRenderTarget(const char* name) {
    ZEST_ASSERT(zest_map_valid_name(ZestRenderer->render_targets, name));    //No render target found with that index
    return *zest_map_at(ZestRenderer->render_targets, name);
}

VkFramebuffer zest_GetRendererFrameBufferCallback(zest_command_queue_draw_commands item) {
    return ZestRenderer->swapchain_frame_buffers[ZestRenderer->current_image_frame];
}

VkFramebuffer zest_GetRenderTargetFrameBufferCallback(zest_command_queue_draw_commands item) {
    zest_index fif = ZEST__FLAGGED(item->render_target->flags, zest_render_target_flag_single_frame_buffer_only) ? 0 : ZEST_FIF;
    ZEST_ASSERT(zest_vec_size(item->render_target->framebuffers[fif].framebuffers));
    return item->render_target->framebuffers[fif].framebuffers[0];
}

void zest_RecreateRenderTargetResources(zest_render_target render_target, zest_index fif) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    int width, height;
    if (render_target->create_info.input_source && zest_Vec2Length(render_target->create_info.ratio_of_screen_size)) {
        width = (zest_uint)((float)render_target->create_info.input_source->render_width * render_target->create_info.ratio_of_screen_size.x);
        height = (zest_uint)((float)render_target->create_info.input_source->render_height * render_target->create_info.ratio_of_screen_size.y);
    } else if (zest_Vec2Length(render_target->create_info.ratio_of_screen_size)) {
        width = (zest_uint)((float)ZestApp->window->window_width * render_target->create_info.ratio_of_screen_size.x);
        height = (zest_uint)((float)ZestApp->window->window_height * render_target->create_info.ratio_of_screen_size.y);
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
    render_target->pipeline_template->scissor.extent = render_target->viewport.extent;
    render_target->pipeline_template->scissor.offset = render_target->viewport.offset;
    render_target->render_width = width;
    render_target->render_height = height;
    render_target->create_info.viewport.extent.width = width;
    render_target->create_info.viewport.extent.height = height;
    render_target->recorder->outdated[fif] = 1;

    if (ZEST_VALID_HANDLE(render_target->filter_pipeline_template)) {
		render_target->filter_pipeline_template->scissor.extent = render_target->viewport.extent;
		render_target->filter_pipeline_template->scissor.offset = render_target->viewport.offset;
    }

    zest_FreeBuffer(render_target->framebuffers[fif].color_buffer.buffer);
    if (ZEST__FLAGGED(render_target->flags, zest_render_target_flag_use_depth_buffer)) {
        zest_FreeBuffer(render_target->framebuffers[fif].depth_buffer.buffer);
    }
    render_target->framebuffers[fif] = zest_CreateFrameBuffer(
        render_target->render_pass, 
        render_target->mip_levels,
        render_target->render_width, 
        render_target->render_height,
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
	ZEST__FLAG(render_target->flags, zest_render_target_flag_was_changed);
}

void zest__refresh_render_target_sampler(zest_render_target render_target) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    render_target->current_index ^= 1;
    int index = render_target->current_index;
    if (render_target->sampler[index]) vkDestroySampler(ZestDevice->logical_device, render_target->sampler[index], &ZestDevice->allocation_callbacks);
    zest__update_image_vertices(&render_target->sampler_image);
	render_target->sampler[render_target->current_index] = zest__create_sampler(render_target->sampler_info);

    zest_ResetDescriptorPool(render_target->descriptor_pool[index]);
	zest_ForEachFrameInFlight(fif) {
		zest_descriptor_set set = &render_target->sampler_descriptor_set[index][fif];
        render_target->image_info[fif].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        render_target->image_info[fif].imageView = render_target->framebuffers[fif].color_buffer.base_view;
        render_target->image_info[fif].sampler = render_target->sampler[index];

        zest_descriptor_set_builder_t builder = zest_BeginDescriptorSetBuilder(zest_GetDescriptorSetLayout("1 sampler"));
        zest_AddSetBuilderDirectImageWrite(&builder, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &render_target->image_info[fif]);
        set = zest_FinishDescriptorSet(render_target->descriptor_pool[index], &builder, set);
    }
}

void zest__refresh_render_target_mip_samplers(zest_render_target render_target) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    int index = render_target->current_index;

    zest_ForEachFrameInFlight(fif) {
        zest_vec_resize(render_target->mip_level_samplers[index][fif], (zest_uint)render_target->mip_levels);
		zest_vec_resize(render_target->mip_level_descriptor_sets[index][fif], (zest_uint)render_target->mip_levels);
    }
    zest_uint mip_descriptor_size = zest_vec_size(render_target->mip_level_image_infos);

	zest_uint current_mip_width = render_target->render_width; 
	zest_uint current_mip_height = render_target->render_height; 

    zest_vec_clear(render_target->mip_level_sizes);

    zest_ResetDescriptorPool(render_target->descriptor_pool[index]);

    for (zest_index mip_level = 0; mip_level != render_target->mip_levels; ++mip_level) {
		zest_ForEachFrameInFlight(fif) {
			zest_descriptor_set set = &render_target->mip_level_samplers[index][fif][mip_level];
			zest_descriptor_set_t blank = { 0 };
			*set = blank;
			set->magic = zest_INIT_MAGIC;
			VkDescriptorImageInfo *image_info = &render_target->mip_level_image_infos[fif][mip_level];
			image_info->imageView = render_target->framebuffers[fif].color_buffer.mip_views[mip_level];
			image_info->sampler = render_target->sampler[index];

			zest_descriptor_set_builder_t builder = zest_BeginDescriptorSetBuilder(zest_GetDescriptorSetLayout("1 sampler"));
			zest_AddSetBuilderDirectImageWrite(&builder, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &render_target->image_info[fif]);
			set = zest_FinishDescriptorSet(render_target->descriptor_pool[index], &builder, set);
        }

		VkRect2D mip_size = { 0 };
		mip_size.extent.width = current_mip_width;
		mip_size.extent.height = current_mip_height;
		zest_vec_push(render_target->mip_level_sizes, mip_size);

		current_mip_width = ZEST__MAX(1u, current_mip_width / 2);
		current_mip_height = ZEST__MAX(1u, current_mip_height / 2);

        if (ZEST__FLAGGED(render_target->flags, zest_render_target_flag_upsampler)) {
            zest_ForEachFrameInFlight(fif) {
				zest_descriptor_set_builder_t builder = zest_BeginDescriptorSetBuilder(zest_GetDescriptorSetLayout("2 sampler"));
				VkDescriptorImageInfo *image_info = &render_target->mip_level_image_infos[fif][mip_level];
                zest_AddSetBuilderDirectImageWrite(&builder, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info);
                zest_AddSetBuilderDirectImageWrite(&builder, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &render_target->input_source->image_info[fif]);
				zest_FinishDescriptorSet(render_target->descriptor_pool[index], &builder, &render_target->mip_level_descriptor_sets[index][fif][mip_level]);
            }
        }
    }
}

void zest__render_target_maintennance() {
    for (zest_map_foreach_i(ZestRenderer->render_targets)) {
        zest_render_target render_target = *zest_map_at_index(ZestRenderer->render_targets, i);
        ZEST__UNFLAG(render_target->flags, zest_render_target_flag_was_changed);
    }
}

void zest__record_render_target_commands(zest_render_target render_target, zest_index fif) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    VkCommandBuffer command_buffer = zest_BeginRecording(render_target->recorder, render_target->render_pass, fif);
    VkViewport view = zest_CreateViewport(0.f, 0.f, (float)render_target->viewport.extent.width, (float)render_target->viewport.extent.height, 0.f, 1.f);
    VkRect2D scissor = zest_CreateRect2D(render_target->viewport.extent.width, render_target->viewport.extent.height, 0, 0);
    vkCmdSetViewport(command_buffer, 0, 1, &view);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    zest_pipeline pipeline = zest_PipelineWithTemplate(render_target->pipeline_template, render_target->render_pass);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0, 1, zest_GetRenderTargetSamplerDescriptorSetVK(render_target), 0, NULL);
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

	vkCmdPushConstants(
		command_buffer,
		pipeline->pipeline_layout,
		VK_SHADER_STAGE_VERTEX_BIT,
		0,
		sizeof(zest_render_target_push_constants_t),
		&render_target->push_constants);

	vkCmdDraw(command_buffer, 3, 1, 0, 0);
    zest_EndRecording(render_target->recorder, fif);
}

void zest_DrawToRenderTargetCallback(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer) {
    VkRenderPassBeginInfo render_pass_info = { 0 };
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea = item->viewport;

    VkClearValue clear_values[2] = {
        [0] .color = {.float32[0] = item->cls_color.x, .float32[1] = item->cls_color.y, .float32[2] = item->cls_color.y, .float32[3] = item->cls_color.w },
        [1].depthStencil = {.depth = 1.0f, .stencil = 0 }
    };

    render_pass_info.clearValueCount = 2;
    render_pass_info.pClearValues = clear_values;
    zest_render_target target = item->render_target;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    if (target->post_process_record_callback && target->recorder->outdated[ZEST_FIF] != 0) {
        target->post_process_record_callback(target, target->post_process_user_data, ZEST_FIF);
    }
    
    zest_ExecuteRecorderCommands(command_buffer, target->recorder, ZEST_FIF);

    vkCmdEndRenderPass(command_buffer);
}

void zest_DrawRenderTargetsToSwapchain(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer) {
    VkRenderPassBeginInfo render_pass_info = { 0 };
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea = item->viewport;

    VkClearValue clear_values[2] = {
        [0] .color = {.float32[0] = item->cls_color.x, .float32[1] = item->cls_color.y, .float32[2] = item->cls_color.z, .float32[3] = item->cls_color.w },
        [1].depthStencil = {.depth = 1.0f, .stencil = 0 }
    };

    render_pass_info.clearValueCount = 2;
    render_pass_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport view = zest_CreateViewport(0.f, 0.f, (float)item->render_target->viewport.extent.width, (float)item->render_target->viewport.extent.height, 0.f, 1.f);
    VkRect2D scissor = zest_CreateRect2D(item->render_target->viewport.extent.width, item->render_target->viewport.extent.height, 0, 0);
    vkCmdSetViewport(command_buffer, 0, 1, &view);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    for (zest_foreach_i(item->render_targets)) {
        zest_render_target layer = item->render_targets[i];

        zest_pipeline pipeline = zest_PipelineWithTemplate(layer->pipeline_template, render_pass);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0, 1, zest_GetRenderTargetSamplerDescriptorSetVK(layer), 0, NULL);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

        vkCmdPushConstants(
            command_buffer,
            pipeline->pipeline_layout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(zest_render_target_push_constants_t),
            &layer->push_constants);

        vkCmdDraw(command_buffer, 3, 1, 0, 0);
    }

    vkCmdEndRenderPass(command_buffer);
}

void zest_CompositeRenderTargets(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer) {
    VkRenderPassBeginInfo render_pass_info = { 0 };
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea = item->viewport;

    VkClearValue clear_values[2] = {
        [0] .color = {.float32[0] = item->cls_color.x, .float32[1] = item->cls_color.y, .float32[2] = item->cls_color.z, .float32[3] = item->cls_color.w },
        [1].depthStencil = {.depth = 1.0f, .stencil = 0 }
    };

    render_pass_info.clearValueCount = 2;
    render_pass_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport view = zest_CreateViewport(0.f, 0.f, (float)item->viewport.extent.width, (float)item->viewport.extent.height, 0.f, 1.f);
	VkRect2D scissor = zest_CreateRect2D(item->viewport.extent.width, item->viewport.extent.height, 0, 0);
	vkCmdSetViewport(command_buffer, 0, 1, &view);
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    ZEST_ASSERT(item->composite_pipeline);  //The composite_pipeline must have been set in the draw commands. Use zest_NewDrawCommandSetupCompositeToSwap to set this up.
	zest_pipeline pipeline = zest_PipelineWithTemplate(item->composite_pipeline, render_pass);
    zest_BindPipelineShaderResource(command_buffer, pipeline, item->composite_shader_resources);

    if (pipeline->pipeline_template->pushConstantRange.size > 0) {
        ZEST_ASSERT(item->composite_pipeline->push_constants);   //You must specify the push constants for the composite shader when setting up the draw commands (zest_NewDrawCommandSetupCompositeToSwap)
        //You must also ensure that the push_constants pointer points to the correct place in memory or you'll just send
        //pushConstantRange.size worth of garbage to the shader.
        vkCmdPushConstants(
            command_buffer,
            pipeline->pipeline_layout,
            pipeline->pipeline_template->pushConstantRange.stageFlags,
            pipeline->pipeline_template->pushConstantRange.offset,
            pipeline->pipeline_template->pushConstantRange.size,
            item->composite_pipeline->push_constants);
    }

	vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);
}

void zest_DownSampleRenderTarget(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer unused) {
    ZEST_ASSERT(item->render_target->input_source); //You must assign an input source for the down sampler. This will be the image that is downsampled
	zest_frame_buffer_t *frame_buffer = &item->render_target->framebuffers[ZEST_FIF];

    VkImageMemoryBarrier input_source_barrier = zest__create_image_memory_barrier(
        item->render_target->input_source->framebuffers[ZEST_FIF].color_buffer.image,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        0, 1);
    zest__place_fragment_barrier(command_buffer, &input_source_barrier);

    //First render the input source in the render target to the render taget and apply the filter pipeline
    VkRenderPassBeginInfo render_pass_info = { 0 };
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass;
    render_pass_info.framebuffer = frame_buffer->framebuffers[0];
    render_pass_info.renderArea = item->viewport;

	VkClearValue clear_value = {
		.color = {.float32[0] = item->cls_color.x, .float32[1] = item->cls_color.y, .float32[2] = item->cls_color.z, .float32[3] = item->cls_color.w },
	};

    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport view = zest_CreateViewport(0.f, 0.f, (float)item->render_target->viewport.extent.width, (float)item->render_target->viewport.extent.height, 0.f, 1.f);
    VkRect2D scissor = zest_CreateRect2D(item->render_target->viewport.extent.width, item->render_target->viewport.extent.height, 0, 0);
    vkCmdSetViewport(command_buffer, 0, 1, &view);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

	zest_pipeline pipeline = zest_PipelineWithTemplate(item->render_target->filter_pipeline_template, render_pass);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0, 1, zest_GetRenderTargetSamplerDescriptorSetVK(item->render_target->input_source), 0, NULL);
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

    if (pipeline->pipeline_template->pushConstantRange.size > 0) {
        ZEST_ASSERT(item->render_target->push_constants);   //You must the point the push content data in the render target
        vkCmdPushConstants(
            command_buffer,
            pipeline->pipeline_layout,
            pipeline->pipeline_template->pushConstantRange.stageFlags,
            pipeline->pipeline_template->pushConstantRange.offset,
            pipeline->pipeline_template->pushConstantRange.size,
            item->render_target->push_constants);
    }

	vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);

	VkImageMemoryBarrier barrier = zest__create_image_memory_barrier(item->render_target->framebuffers[ZEST_FIF].color_buffer.image,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 1);
	zest__place_fragment_barrier(command_buffer, &barrier);

    VkRect2D current_viewport = { 0 };
    current_viewport.extent = item->viewport.extent;

    for (int mip_level = 1; mip_level != item->render_target->mip_levels; ++mip_level) {
        item->render_target->mip_push_constants.settings.w = (float)mip_level - 1.f;
        item->render_target->mip_push_constants.source_resolution.x = (float)current_viewport.extent.width;
        item->render_target->mip_push_constants.source_resolution.y = (float)current_viewport.extent.height;

		current_viewport.extent.width = ZEST__MAX(1u, current_viewport.extent.width / 2);
		current_viewport.extent.height = ZEST__MAX(1u, current_viewport.extent.height / 2);

        VkRenderPassBeginInfo render_pass_info = { 0 };
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = render_pass;
        render_pass_info.framebuffer = frame_buffer->framebuffers[mip_level];
        render_pass_info.renderArea = current_viewport;

        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues = &clear_value;

        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport view = zest_CreateViewport(0.f, 0.f, (float)current_viewport.extent.width, (float)current_viewport.extent.height, 0.f, 1.f);
        VkRect2D scissor = zest_CreateRect2D(current_viewport.extent.width, current_viewport.extent.height, 0, 0);
        vkCmdSetViewport(command_buffer, 0, 1, &view);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        zest_pipeline pipeline = zest_PipelineWithTemplate(item->render_target->pipeline_template, render_pass);
        VkDescriptorSet mip_level_set = item->render_target->mip_level_samplers[item->render_target->current_index][ZEST_FIF][mip_level - 1].vk_descriptor_set;
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0, 1, &mip_level_set, 0, NULL);
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

        if (pipeline->pipeline_template->pushConstantRange.size > 0) {
            ZEST_ASSERT(item->render_target->push_constants);   //You must the point the push content data in the render target
            vkCmdPushConstants(
                command_buffer,
                pipeline->pipeline_layout,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(zest_mip_push_constants_t),
                &item->render_target->mip_push_constants);
        }

        vkCmdDraw(command_buffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(command_buffer);

		VkImageMemoryBarrier barrier = zest__create_image_memory_barrier(item->render_target->framebuffers[ZEST_FIF].color_buffer.image,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			mip_level, 1);
		zest__place_fragment_barrier(command_buffer, &barrier);
    }

    item->render_target->recorder->outdated[ZEST_FIF] = 1;
}

void zest_UpSampleRenderTarget(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer unused) {
    ZEST_ASSERT(item->render_target->input_source); //You must assign an input source for the up sampler.
    ZEST_ASSERT(ZEST__FLAGGED(item->render_target->input_source->flags, zest_render_target_flag_multi_mip)); //Input source must be a multi mip, ie a downsampler that you now want to upscale
    ZEST_ASSERT(item->render_target->input_source->mip_levels == item->render_target->mip_levels);  //input source must have the same number of mip levels 
	zest_frame_buffer_t *frame_buffer = &item->render_target->framebuffers[ZEST_FIF];

    zest_uint mip_to_blit = item->render_target->mip_levels - 1;

    //Blit the smallest mip level from the downsampled render target first
    VkImageMemoryBarrier blit_src_barrier = zest__create_image_memory_barrier(item->render_target->input_source->framebuffers[ZEST_FIF].color_buffer.image,
        0,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        mip_to_blit, 1);
    zest__place_image_barrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &blit_src_barrier);

    VkImageMemoryBarrier blit_dst_barrier = zest__create_image_memory_barrier(item->render_target->framebuffers[ZEST_FIF].color_buffer.image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        mip_to_blit, 1);
    zest__place_image_barrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &blit_dst_barrier);

    VkOffset3D base_offset = { 0 };
    VkImageBlit blit = { 0 };
    blit.srcSubresource.mipLevel = mip_to_blit;
    blit.srcSubresource.layerCount = 1;
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = mip_to_blit;
    blit.dstSubresource.layerCount = 1;
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstOffsets[0] = base_offset;
    blit.srcOffsets[0] = base_offset;
    blit.dstOffsets[1].x = item->render_target->mip_level_sizes[mip_to_blit].extent.width;
    blit.dstOffsets[1].y = item->render_target->mip_level_sizes[mip_to_blit].extent.height;
    blit.dstOffsets[1].z = 1;
    blit.srcOffsets[1].x = item->render_target->input_source->mip_level_sizes[mip_to_blit].extent.width;
    blit.srcOffsets[1].y = item->render_target->input_source->mip_level_sizes[mip_to_blit].extent.height;
    blit.srcOffsets[1].z = 1;

    bool same_size = (blit.srcOffsets[1].x == blit.dstOffsets[1].x && blit.srcOffsets[1].y == blit.dstOffsets[1].y);

    vkCmdBlitImage(
        command_buffer,
        item->render_target->input_source->framebuffers[ZEST_FIF].color_buffer.image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        item->render_target->framebuffers[ZEST_FIF].color_buffer.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, same_size ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);

    blit_src_barrier = zest__create_image_memory_barrier(item->render_target->input_source->framebuffers[ZEST_FIF].color_buffer.image,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        mip_to_blit, 1);
    zest__place_image_barrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &blit_src_barrier);

    blit_dst_barrier = zest__create_image_memory_barrier(item->render_target->framebuffers[ZEST_FIF].color_buffer.image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        mip_to_blit, 1);
    zest__place_image_barrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &blit_dst_barrier);

    
    VkRect2D current_viewport = { 0 };
    current_viewport.extent = item->viewport.extent;

	VkClearValue clear_value = {
		.color = {.float32[0] = item->cls_color.x, .float32[1] = item->cls_color.y, .float32[2] = item->cls_color.z, .float32[3] = item->cls_color.w },
	};

    //We've already blitted the smallest mip, so start from the next level up in resolution
    for (int mip_level = mip_to_blit - 1; mip_level >= 0; --mip_level) {
		current_viewport.extent = item->render_target->mip_level_sizes[mip_level].extent;
        item->render_target->mip_push_constants.settings.w = (float)mip_level + 1.f;
        item->render_target->mip_push_constants.source_resolution.x = (float)current_viewport.extent.width;
        item->render_target->mip_push_constants.source_resolution.y = (float)current_viewport.extent.height;

        VkRenderPassBeginInfo render_pass_info = { 0 };
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = render_pass;
        render_pass_info.framebuffer = frame_buffer->framebuffers[mip_level];
        render_pass_info.renderArea = current_viewport;

        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues = &clear_value;

        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport view = zest_CreateViewport(0.f, 0.f, (float)current_viewport.extent.width, (float)current_viewport.extent.height, 0.f, 1.f);
        VkRect2D scissor = zest_CreateRect2D(current_viewport.extent.width, current_viewport.extent.height, 0, 0);
        vkCmdSetViewport(command_buffer, 0, 1, &view);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        zest_pipeline pipeline = zest_PipelineWithTemplate(item->render_target->pipeline_template, render_pass);
        zest_BindPipeline(command_buffer, pipeline, &item->render_target->mip_level_descriptor_sets[item->render_target->current_index][ZEST_FIF][mip_level + 1].vk_descriptor_set, 1);

        if (pipeline->pipeline_template->pushConstantRange.size > 0) {
            ZEST_ASSERT(sizeof(zest_mip_push_constants_t) == pipeline->pipeline_template->pushConstantRange.size);   //pipeline push constant range must mathc the mip push constants in the render target
            vkCmdPushConstants(
                command_buffer,
                pipeline->pipeline_layout,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(zest_mip_push_constants_t),
                &item->render_target->mip_push_constants);
        }

        vkCmdDraw(command_buffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(command_buffer);

		VkImageMemoryBarrier barrier = zest__create_image_memory_barrier(item->render_target->framebuffers[ZEST_FIF].color_buffer.image,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			mip_level, 1);
		zest__place_fragment_barrier(command_buffer, &barrier);
    }

    item->render_target->recorder->outdated[ZEST_FIF] = 1;
}

void zest_CleanUpRenderTarget(zest_render_target render_target) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    zest_ForEachFrameInFlight(fif) {
        zest__cleanup_framebuffer(&render_target->framebuffers[fif]);
    }
    for (int i = 0; i != 2; ++i) {
        if(render_target->sampler[i]) vkDestroySampler(ZestDevice->logical_device, render_target->sampler[i], &ZestDevice->allocation_callbacks);
        render_target->sampler[i] = 0;
    }
}

void zest_PreserveRenderTargetFrames(zest_render_target render_target, zest_bool yesno) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    if (ZEST__FLAGGED(render_target->flags, zest_render_target_flag_use_depth_buffer)) {
        render_target->render_pass = (yesno != 0 ? zest__get_render_pass(render_target->render_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, 1) 
            : zest__get_render_pass(render_target->render_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, 1));
    }
    else {
        render_target->render_pass = (yesno != 0 ? zest__get_render_pass(render_target->render_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, 0) 
            : zest__get_render_pass(render_target->render_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, 0));
    }
}

void zest_ResizeRenderTarget(zest_render_target render_target, zest_uint width, zest_uint height) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    render_target->create_info.viewport.extent.width = width;
    render_target->create_info.viewport.extent.height = height;
    ZEST__FLAG(render_target->create_info.flags, zest_render_target_flag_fixed_size);
    zest_ScheduleRenderTargetRefresh(render_target);
}

int zest_RenderTargetWasChanged(zest_render_target render_target) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    return ZEST__FLAGGED(render_target->flags, zest_render_target_flag_was_changed) > 0;
}

void zest_ScheduleRenderTargetRefresh(zest_render_target render_target) {
    zest_ForEachFrameInFlight(fif) {
        render_target->frame_buffer_dirty[fif] = 1;
    }
	zest_map_insert(ZestRenderer->render_target_recreate_queue, render_target->name, render_target);
}

void zest_SetRenderTargetSamplerToClamp(zest_render_target render_target) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    render_target->sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    render_target->sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    render_target->sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	zest_map_insert(ZestRenderer->rt_sampler_refresh_queue, render_target->name, render_target);
}

void zest_SetRenderTargetSamplerToRepeat(zest_render_target render_target) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    render_target->sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    render_target->sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    render_target->sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	zest_map_insert(ZestRenderer->rt_sampler_refresh_queue, render_target->name, render_target);
}

void zest_RenderTargetClear(zest_render_target render_target, zest_uint fif) {
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
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
    instruction.pipeline_template = ZEST_NULL;
    return instruction;
}

void zest_ResetInstanceLayerDrawing(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	            //Not a valid handle!
    zest_vec_clear(layer->draw_instructions[layer->fif]);
    layer->memory_refs[layer->fif].staging_instance_data->memory_in_use = 0;
    layer->current_instruction = zest__layer_instruction();
    layer->memory_refs[layer->fif].instance_count = 0;
    layer->memory_refs[layer->fif].instance_ptr = layer->memory_refs[layer->fif].staging_instance_data->data;
}

zest_uint zest_GetInstanceLayerCount(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    return layer->memory_refs[layer->fif].instance_count;
}

zest_bool zest__grow_instance_buffer(zest_layer layer, zest_size type_size, zest_size minimum_size) {
    zest_bool grown = 0;
    if (ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
		grown = zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_instance_data, type_size, minimum_size);
        zest_GrowBuffer(&layer->memory_refs[layer->fif].device_vertex_data, type_size, minimum_size);
		layer->memory_refs[layer->fif].staging_instance_data = layer->memory_refs[layer->fif].staging_instance_data;
        if (ZEST__FLAGGED(layer->flags, zest_layer_flag_using_global_bindless_layout) && layer->memory_refs[layer->fif].device_vertex_data->array_index != ZEST_INVALID) {
            zest_buffer instance_buffer = layer->memory_refs[layer->fif].device_vertex_data;
            VkDescriptorBufferInfo buffer_info = zest_vk_GetBufferInfo(instance_buffer);

            VkWriteDescriptorSet write = zest_CreateBufferDescriptorWriteWithType(layer->bindless_set->vk_descriptor_set, &buffer_info, zest_storage_buffer_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            write.dstArrayElement = layer->memory_refs[layer->fif].device_vertex_data->array_index;
            vkUpdateDescriptorSets(ZestDevice->logical_device, 1, &write, 0, 0);
        }
    } else {
		grown = zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_instance_data, type_size, minimum_size);
		layer->memory_refs[layer->fif].staging_instance_data = layer->memory_refs[layer->fif].staging_instance_data;
    }
    return grown;
}

void zest__reset_mesh_layer_drawing(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	            //Not a valid handle!
    zest_vec_clear(layer->draw_instructions[layer->fif]);
    layer->memory_refs[layer->fif].staging_vertex_data->memory_in_use = 0;
    layer->memory_refs[layer->fif].staging_index_data->memory_in_use = 0;
    layer->current_instruction = zest__layer_instruction();
    layer->memory_refs[layer->fif].index_count = 0;
    layer->memory_refs[layer->fif].vertex_count = 0;
    layer->memory_refs[layer->fif].vertex_ptr = layer->memory_refs[layer->fif].staging_vertex_data->data;
    layer->memory_refs[layer->fif].index_ptr = layer->memory_refs[layer->fif].staging_index_data->data;
}

int zest_LayerHasInstructionsCallback(zest_draw_routine draw_routine) {
    ZEST_CHECK_HANDLE(draw_routine);	//Not a valid handle!
    ZEST_ASSERT(draw_routine->draw_data);
    zest_layer layer = (zest_layer)draw_routine->draw_data;
    return zest_vec_size(layer->draw_instructions[layer->fif]) > 0;
}

int zest_AlwaysRecordCallback(zest_draw_routine draw_routine) {
    return 1;
}

void zest_StartInstanceInstructions(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    layer->current_instruction.start_index = layer->memory_refs[layer->fif].instance_count ? layer->memory_refs[layer->fif].instance_count : 0;
}

void zest_ResetLayer(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    layer->fif = (layer->fif + 1) % ZEST_MAX_FIF;
}

void zest_ResetInstanceLayer(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_ASSERT(ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif));   //You must have created the layer with zest_CreateFIFInstanceLayer
                                                                            //if you want to manually reset the layer
    layer->prev_fif = layer->fif;
    layer->fif = (layer->fif + 1) % ZEST_MAX_FIF;
    layer->dirty[layer->fif] = 1;
    zest_ResetInstanceLayerDrawing(layer);
}

void zest__start_mesh_instructions(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    layer->current_instruction.start_index = layer->memory_refs[layer->fif].index_count ? layer->memory_refs[layer->fif].index_count : 0;
}

void zest_EndInstanceInstructions(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
        layer->fif = ZEST_FIF;
    }
    if (layer->current_instruction.total_instances) {
        layer->last_draw_mode = zest_draw_mode_none;
        zest_vec_push(layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->memory_refs[layer->fif].staging_instance_data->memory_in_use += layer->current_instruction.total_instances * layer->instance_struct_size;
        layer->current_instruction.total_instances = 0;
        layer->current_instruction.start_index = 0;
    }
    else if (layer->current_instruction.draw_mode == zest_draw_mode_viewport) {
        zest_vec_push(layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->last_draw_mode = zest_draw_mode_none;
    }
}

zest_bool zest_MaybeEndInstanceInstructions(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    if (layer->current_instruction.total_instances) {
        layer->last_draw_mode = zest_draw_mode_none;
        zest_vec_push(layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->memory_refs[layer->fif].staging_instance_data->memory_in_use += layer->current_instruction.total_instances * layer->instance_struct_size;
        layer->current_instruction.total_instances = 0;
        layer->current_instruction.start_index = 0;
    }
    else if (layer->current_instruction.draw_mode == zest_draw_mode_viewport) {
        zest_vec_push(layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->last_draw_mode = zest_draw_mode_none;
    }
    return 1;
}

void zest__end_mesh_instructions(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    if (layer->current_instruction.total_indexes) {
        layer->last_draw_mode = zest_draw_mode_none;
        zest_vec_push(layer->draw_instructions[layer->fif], layer->current_instruction);

        layer->memory_refs[layer->fif].staging_index_data->memory_in_use += layer->current_instruction.total_indexes * sizeof(zest_uint);
        layer->memory_refs[layer->fif].staging_vertex_data->memory_in_use += layer->current_instruction.total_indexes * layer->vertex_struct_size;
        layer->current_instruction.total_indexes = 0;
        layer->current_instruction.start_index = 0;
    }
    else if (layer->current_instruction.draw_mode == zest_draw_mode_viewport) {
        zest_vec_push(layer->draw_instructions[layer->fif], layer->current_instruction);

        layer->last_draw_mode = zest_draw_mode_none;
    }
}

void zest__update_instance_layer_buffers_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer) {
    ZEST_CHECK_HANDLE(draw_routine);	//Not a valid handle!
    zest_layer layer = (zest_layer)draw_routine->draw_data;
    if(!zest_MaybeEndInstanceInstructions(layer)) return;

    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_device_local_direct)) {
        if (!zest_vec_empty(layer->draw_instructions[layer->fif])) {
            //zest_AddCopyCommand(&layer->vertex_upload, layer->memory_refs[layer->fif].staging_instance_data, layer->memory_refs[layer->fif].device_instance_data, 0);
        }

        zest_UploadBuffer(&layer->vertex_upload, command_buffer);
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

VkCommandBuffer zest_BeginRecording(zest_recorder recorder, VkRenderPass render_pass, zest_uint fif) {
    ZEST_CHECK_HANDLE(recorder);	//Not a valid handle!
    VkCommandBuffer command_buffer = recorder->command_buffer[fif];
    ZEST_ASSERT(ZEST__NOT_FLAGGED(recorder->flags, zest_command_buffer_flag_recording));    //Must not be in a recording state. Did you call end recording?
    ZEST__FLAG(recorder->flags, zest_command_buffer_flag_recording);

    VkCommandBufferInheritanceInfo inheritance_info = { 0 };
    inheritance_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritance_info.renderPass = render_pass;  
    inheritance_info.subpass = 0;                      
    inheritance_info.framebuffer = VK_NULL_HANDLE;     
    inheritance_info.occlusionQueryEnable = VK_FALSE;   
    inheritance_info.queryFlags = 0;                    
    inheritance_info.pipelineStatistics = 0;            

    VkCommandBufferBeginInfo begin_info = { 0 };
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;  
    begin_info.pInheritanceInfo = &inheritance_info;

	vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}

VkCommandBuffer zest_BeginComputeRecording(zest_recorder recorder, zest_uint fif) {
    ZEST_CHECK_HANDLE(recorder);	//Not a valid handle!
    VkCommandBuffer command_buffer = recorder->command_buffer[fif];
    ZEST_ASSERT(ZEST__NOT_FLAGGED(recorder->flags, zest_command_buffer_flag_recording));    //Must not be in a recording state. Did you call end recording?
    ZEST__FLAG(recorder->flags, zest_command_buffer_flag_recording);

    VkCommandBufferInheritanceInfo inheritance_info = { 0 };
    inheritance_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritance_info.renderPass = VK_NULL_HANDLE;  
    inheritance_info.subpass = 0;                      
    inheritance_info.framebuffer = VK_NULL_HANDLE;     
    inheritance_info.occlusionQueryEnable = VK_FALSE;   
    inheritance_info.queryFlags = 0;                    
    inheritance_info.pipelineStatistics = 0;            

    VkCommandBufferBeginInfo begin_info = { 0 };
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    begin_info.pInheritanceInfo = &inheritance_info;

	vkBeginCommandBuffer(command_buffer, &begin_info);
    return command_buffer;
}

void zest_EndRecording(zest_recorder recorder, zest_uint fif) {
    ZEST_CHECK_HANDLE(recorder);	//Not a valid handle!
    VkCommandBuffer command_buffer = recorder->command_buffer[fif];
    ZEST_ASSERT(ZEST__FLAGGED(recorder->flags, zest_command_buffer_flag_recording));    //Must be in a recording state! Did you call zest_BeingRecording?
    ZEST__UNFLAG(recorder->flags, zest_command_buffer_flag_recording);
    recorder->outdated[fif] = 0;
    vkEndCommandBuffer(command_buffer);
}

void zest_MarkOutdated(zest_recorder recorder) {
    ZEST_CHECK_HANDLE(recorder);	//Not a valid handle!
    for (ZEST_EACH_FIF_i) {
        recorder->outdated[i] = 1;
    }
}

void zest_ResetDrawRoutine(zest_draw_routine draw_routine) {
    ZEST_CHECK_HANDLE(draw_routine);	//Not a valid handle!
    for (ZEST_EACH_FIF_i) {
        draw_routine->recorder->outdated[i] = 1;
    }
}

void zest_SetViewport(VkCommandBuffer command_buffer, zest_draw_routine draw_routine) {
    ZEST_CHECK_HANDLE(draw_routine);	//Not a valid handle!
    VkViewport view = zest_CreateViewport(0.f, 0.f, (float)draw_routine->draw_commands->viewport.extent.width, (float)draw_routine->draw_commands->viewport.extent.height, 0.f, 1.f);
    VkRect2D scissor = zest_CreateRect2D(draw_routine->draw_commands->viewport.extent.width, draw_routine->draw_commands->viewport.extent.height, 0, 0);
    vkCmdSetViewport(command_buffer, 0, 1, &view);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

void zest_DrawInstanceLayer(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
    zest_layer layer = (zest_layer)user_data;
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle! Make sure you pass in the zest_layer in the user data

	zest_buffer device_buffer = layer->vertex_buffer_node->storage_buffer;
	VkDeviceSize instance_data_offsets[] = { device_buffer->memory_offset };
	vkCmdBindVertexBuffers(command_buffer, 0, 1, &device_buffer->memory_pool->buffer, instance_data_offsets);

    bool has_instruction_view_port = false;
    for (zest_foreach_i(layer->draw_instructions[layer->fif])) {
        zest_layer_instruction_t* current = &layer->draw_instructions[layer->fif][i];

        if (current->draw_mode == zest_draw_mode_viewport) {
            vkCmdSetViewport(command_buffer, 0, 1, &current->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &current->scissor);
            has_instruction_view_port = true;
            continue;
        } else if(!has_instruction_view_port) {
            vkCmdSetViewport(command_buffer, 0, 1, &layer->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &layer->scissor);
        }

        ZEST_CHECK_HANDLE(current->shader_resources); //Shader resources must be set in the instruction

        zest_pipeline pipeline = zest_PipelineWithTemplate(current->pipeline_template, context->render_pass);
        if (pipeline) {
			zest_BindPipelineShaderResource(command_buffer, pipeline, current->shader_resources);
        } else {
            continue;
        }

		vkCmdPushConstants(
			command_buffer,
			pipeline->pipeline_layout,
			zest_PipelinePushConstantStageFlags(pipeline, 0),
			zest_PipelinePushConstantOffset(pipeline, 0),
			zest_PipelinePushConstantSize(pipeline, 0),
			&current->push_constant);

        vkCmdDraw(command_buffer, 6, current->total_instances, 0, current->start_index);
    }
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
		zest_ResetInstanceLayerDrawing(layer);
    }
}

void zest__record_mesh_layer(zest_layer layer, zest_uint fif) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    VkCommandBuffer command_buffer = 0;
    //VkCommandBuffer command_buffer = zest_BeginRecording(layer->draw_routine->recorder, layer->draw_routine->draw_commands->render_pass, ZEST_FIF);

    //zest_BindMeshIndexBuffer(command_buffer, layer);
    //zest_BindMeshVertexBuffer(command_buffer, layer);

    bool has_instruction_view_port = false;
    for (zest_foreach_i(layer->draw_instructions[layer->fif])) {
        zest_layer_instruction_t* current = &layer->draw_instructions[layer->fif][i];

        if (current->draw_mode == zest_draw_mode_viewport) {
            vkCmdSetViewport(command_buffer, 0, 1, &current->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &current->scissor);
            has_instruction_view_port = true;
            continue;
        } else if(!has_instruction_view_port) {
            vkCmdSetViewport(command_buffer, 0, 1, &layer->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &layer->scissor);
        }

		ZEST_CHECK_HANDLE(current->shader_resources);     //Not a valid descriptor set in the shader resource. Make sure all frame in flights are set

        zest_pipeline pipeline = zest_PipelineWithTemplate(current->pipeline_template, 0);
        if (pipeline) {
			zest_BindPipelineShaderResource(command_buffer, pipeline, current->shader_resources);
        } else {
            continue;
        }

        vkCmdPushConstants(
            command_buffer,
            pipeline->pipeline_layout,
            zest_PipelinePushConstantStageFlags(pipeline, 0),
            zest_PipelinePushConstantOffset(pipeline, 0),
            zest_PipelinePushConstantSize(pipeline, 0),
            current->push_constant);

        vkCmdDrawIndexed(command_buffer, current->total_indexes, 1, current->start_index, 0, 0);
    }
    //zest_EndRecording(layer->draw_routine->recorder, ZEST_FIF);
}

void zest_DrawInstanceMeshLayer(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
    zest_layer layer = (zest_layer)user_data;
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle! Make sure you pass in the zest_layer in the user data
    if (layer->vertex_data && layer->index_data) {
        zest_BindMeshVertexBuffer(command_buffer, layer);
        zest_BindMeshIndexBuffer(command_buffer, layer);
    } else {
        ZEST_PRINT("No Vertex/Index data found in mesh layer!");
        return;
    }

	zest_buffer device_buffer = layer->vertex_buffer_node->storage_buffer;
	VkDeviceSize instance_data_offsets[] = { device_buffer->memory_offset };
	vkCmdBindVertexBuffers(command_buffer, 1, 1, &device_buffer->memory_pool->buffer, instance_data_offsets);

    bool has_instruction_view_port = false;
    for (zest_foreach_i(layer->draw_instructions[layer->fif])) {
        zest_layer_instruction_t *current = &layer->draw_instructions[layer->fif][i];

        if (current->draw_mode == zest_draw_mode_viewport) {
            vkCmdSetViewport(command_buffer, 0, 1, &current->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &current->scissor);
            has_instruction_view_port = true;
            continue;
        } else if(!has_instruction_view_port) {
            vkCmdSetViewport(command_buffer, 0, 1, &layer->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &layer->scissor);
        }

        ZEST_CHECK_HANDLE(current->shader_resources);

        zest_pipeline pipeline = zest_PipelineWithTemplate(current->pipeline_template, context->render_pass);
        if (pipeline) {
			zest_BindPipelineShaderResource(command_buffer, pipeline, current->shader_resources);
        } else {
            continue;
        }

        vkCmdPushConstants(
            command_buffer,
            pipeline->pipeline_layout,
            zest_PipelinePushConstantStageFlags(pipeline, 0),
            zest_PipelinePushConstantOffset(pipeline, 0),
            zest_PipelinePushConstantSize(pipeline, 0),
            current->push_constant);

        vkCmdDrawIndexed(command_buffer, layer->index_count, current->total_instances, 0, 0, current->start_index);
    }
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
		zest_ResetInstanceLayerDrawing(layer);
    }
}

//Start general instance layer functionality -----
void zest_RecordInstanceLayerCallback(struct zest_work_queue_t *queue, void *data) {
    zest_draw_routine draw_routine = (zest_draw_routine)data;
    zest_layer layer = (zest_layer)draw_routine->draw_data;
    if (draw_routine->recorder->outdated[ZEST_FIF] != 0) {
        //zest_DrawInstanceLayer(layer, ZEST_FIF);
    }
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
        zest_ResetInstanceLayerDrawing(layer);
        zest_MarkOutdated(draw_routine->recorder);
    }
}

void zest_NextInstance(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    zest_byte* instance_ptr = (zest_byte*)layer->memory_refs[layer->fif].instance_ptr + layer->instance_struct_size;
    //Make sure we're not trying to write outside of the buffer range
    ZEST_ASSERT(instance_ptr >= (zest_byte*)layer->memory_refs[layer->fif].staging_instance_data->data && instance_ptr <= (zest_byte*)layer->memory_refs[layer->fif].staging_instance_data->end);
    if (instance_ptr == layer->memory_refs[layer->fif].staging_instance_data->end) {
        if (zest__grow_instance_buffer(layer, layer->instance_struct_size, 0)) {
            layer->memory_refs[layer->fif].instance_count++;
            instance_ptr = (zest_byte*)layer->memory_refs[layer->fif].staging_instance_data->data;
            instance_ptr += layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
			layer->current_instruction.total_instances++;
        }
        else {
            instance_ptr = instance_ptr - layer->instance_struct_size;
        }
    }
    else {
		layer->current_instruction.total_instances++;
        layer->memory_refs[layer->fif].instance_count++;
    }
    layer->memory_refs[layer->fif].instance_ptr = instance_ptr;
}

zest_draw_buffer_result zest_DrawInstanceBuffer(zest_layer layer, void *src, zest_uint amount) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    if (!amount) return 0;
    zest_draw_buffer_result result = zest_draw_buffer_result_ok;
    zest_size size_in_bytes_to_copy = amount * layer->instance_struct_size;
    zest_byte *instance_ptr = (zest_byte *)layer->memory_refs[layer->fif].instance_ptr;
    int fif = ZEST_FIF;
    ptrdiff_t diff = (zest_byte *)layer->memory_refs[layer->fif].staging_instance_data->end - (instance_ptr + size_in_bytes_to_copy);
    if (diff <= 0) {
        if (zest__grow_instance_buffer(layer, layer->instance_struct_size, (layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size) + size_in_bytes_to_copy)) {
            instance_ptr = (zest_byte *)layer->memory_refs[layer->fif].staging_instance_data->data;
            instance_ptr += layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
            diff = (zest_byte *)layer->memory_refs[layer->fif].staging_instance_data->end - instance_ptr;
            result = zest_draw_buffer_result_buffer_grew;
        }
        else {
            return zest_draw_buffer_result_failed_to_grow;
        }
    }
    memcpy(instance_ptr, src, size_in_bytes_to_copy);
    layer->memory_refs[layer->fif].instance_count += amount;
    layer->current_instruction.total_instances += amount;
    instance_ptr += size_in_bytes_to_copy;
    layer->memory_refs[layer->fif].instance_ptr = instance_ptr;
    return result;
}

void zest_DrawInstanceInstruction(zest_layer layer, zest_uint amount) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    layer->memory_refs[layer->fif].instance_count += amount;
    layer->current_instruction.total_instances += amount;
    zest_size size_in_bytes_to_draw = amount * layer->instance_struct_size;
    zest_byte* instance_ptr = layer->memory_refs[layer->fif].instance_ptr;
    instance_ptr += size_in_bytes_to_draw;
    layer->memory_refs[layer->fif].instance_ptr = instance_ptr;
}

// End general instance layer functionality -----

//Start internal mesh layer functionality -----
void zest__draw_mesh_layer_callback(struct zest_work_queue_t *queue, void *data) {
    zest_draw_routine draw_routine = (zest_draw_routine)data;
    zest_layer layer = (zest_layer)draw_routine->draw_data;
    if (draw_routine->recorder->outdated[ZEST_FIF] != 0) {
        zest__record_mesh_layer(layer, ZEST_FIF);
    }
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
        zest__reset_mesh_layer_drawing(layer);
        zest_MarkOutdated(draw_routine->recorder);
    }
}

void zest__draw_instance_mesh_layer_callback(struct zest_work_queue_t *queue, void *data) {
    zest_draw_routine draw_routine = (zest_draw_routine)data;
    zest_layer layer = (zest_layer)draw_routine->draw_data;
    if (draw_routine->recorder->outdated[ZEST_FIF] != 0) {
        //zest_DrawInstanceMeshLayer(layer, ZEST_FIF);
    }
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
        zest_ResetInstanceLayerDrawing(layer);
        zest_MarkOutdated(draw_routine->recorder);
    }
}
// End internal mesh layer functionality -----

//-- Draw Layers API
zest_layer zest_NewLayer() {
    zest_layer_t blank_layer = { 0 };
    zest_layer layer = ZEST__NEW_ALIGNED(zest_layer, 16);
    *layer = blank_layer;
    layer->magic = zest_INIT_MAGIC;
    ZEST_ASSERT(sizeof(*layer) == sizeof(blank_layer));
    return layer;
}

void zest_SetLayerViewPort(zest_layer layer, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    layer->scissor = zest_CreateRect2D(scissor_width, scissor_height, x, y);
    layer->viewport = zest_CreateViewport((float)x, (float)y, viewport_width, viewport_height, 0.f, 1.f);
}

void zest_SetLayerScissor(zest_layer layer, int offset_x, int offset_y, zest_uint scissor_width, zest_uint scissor_height) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    layer->scissor = zest_CreateRect2D(scissor_width, scissor_height, offset_x, offset_y);
}

void zest_SetLayerSize(zest_layer layer, float width, float height) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    layer->layer_size.x = width;
    layer->layer_size.y = height;
}

void zest_SetLayerColor(zest_layer layer, zest_byte red, zest_byte green, zest_byte blue, zest_byte alpha) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    layer->current_color = zest_ColorSet(red, green, blue, alpha);
}

void zest_SetLayerColorf(zest_layer layer, float red, float green, float blue, float alpha) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    layer->current_color = zest_ColorSet((zest_byte)(red * 255.f), (zest_byte)(green * 255.f), (zest_byte)(blue * 255.f), (zest_byte)(alpha * 255.f));
}

void zest_SetLayerIntensity(zest_layer layer, float value) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    layer->intensity = value;
}

void zest_SetLayerDirty(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    for (ZEST_EACH_FIF_i) {
        layer->dirty[i] = 1;
    }
}

void zest_SetLayerUserData(zest_layer layer, void *data) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    layer->user_data = data;
}
//-- End Draw Layers

//-- Start Instance Drawing API
zest_layer zest_CreateInstanceLayer(const char* name, zest_size type_size) {
    zest_layer_builder_t builder = zest_NewInstanceLayerBuilder(type_size);
    return zest_BuildInstanceLayer(name, &builder);
}

zest_layer zest_CreateFIFInstanceLayer(const char* name, zest_size type_size) {
    zest_layer_builder_t builder = zest_NewInstanceLayerBuilder(type_size);
    builder.initial_count = 10000;
    zest_layer layer = zest_BuildInstanceLayer(name, &builder);
    zest_ForEachFrameInFlight(fif) {
        layer->memory_refs[fif].device_vertex_data = zest_CreateVertexStorageBuffer(layer->memory_refs[fif].staging_instance_data->size, fif);
    }
    ZEST__FLAG(layer->flags, zest_layer_flag_manual_fif);
    return layer;
}

zest_layer_builder_t zest_NewInstanceLayerBuilder(zest_size type_size) {
    zest_layer_builder_t builder = {
        .initial_count = 1000,
        .type_size = type_size,
		.record_callback = zest_RecordInstanceLayerCallback,
        .condition_callback = zest_LayerHasInstructionsCallback,
        .update_buffers_callback = zest__update_instance_layer_buffers_callback,
        .update_resolution_callback = 0,
        .clean_up_callback = 0,
    };
    return builder;
}

zest_layer zest_BuildInstanceLayer(const char *name, zest_layer_builder_t *builder) {
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->draw_routines, name));   //A draw routine with that name already exists
    ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->layers, name));          //A layer with that name already exists

    zest_draw_routine_t blank_draw_routine = { 0 };
    zest_draw_routine draw_routine = ZEST__NEW(zest_draw_routine);
    *draw_routine = blank_draw_routine;
    draw_routine->magic = zest_INIT_MAGIC;
    draw_routine->recorder = zest_CreateSecondaryRecorder();
	draw_routine->last_fif = -1;
	draw_routine->fif = ZEST_FIF;

    zest_layer layer = zest__create_instance_layer(name, builder->type_size, builder->initial_count);
	draw_routine->draw_data = layer;
	draw_routine->record_callback = zest_RecordInstanceLayerCallback;
	draw_routine->condition_callback = zest_LayerHasInstructionsCallback;
	draw_routine->update_buffers_callback = zest__update_instance_layer_buffers_callback;
    draw_routine->clean_up_callback = builder->clean_up_callback;
    draw_routine->update_resolution_callback = builder->update_resolution_callback;
    draw_routine->name = name;
    draw_routine->command_queue = ZestRenderer->setup_context.command_queue;

    zest_map_insert(ZestRenderer->draw_routines, name, draw_routine);
    ZestRenderer->setup_context.draw_routine = draw_routine;

    return layer;
}

void zest_InitialiseInstanceLayer(zest_layer layer, zest_size type_size, zest_uint instance_pool_size) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    layer->current_color.r = 255;
    layer->current_color.g = 255;
    layer->current_color.b = 255;
    layer->current_color.a = 255;
    layer->intensity = 1;
    layer->layer_size = zest_Vec2Set1(1.f);
    layer->instance_struct_size = type_size;

    zest_buffer_info_t device_buffer_info = zest_CreateVertexBufferInfoWithStorage(0);
    if (zest_GPUHasDeviceLocalHostVisible(type_size * instance_pool_size)) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating Device local buffers for sprite layer");
        ZEST__FLAG(layer->flags, zest_layer_flag_device_local_direct);
        device_buffer_info = zest_CreateVertexBufferInfo(ZEST_TRUE);
    }

    zest_buffer_info_t staging_buffer_info = zest_CreateStagingBufferInfo();
    zest_ForEachFrameInFlight(fif) {
        staging_buffer_info.frame_in_flight = fif;
		layer->memory_refs[fif].staging_instance_data = zest_CreateBuffer(type_size * instance_pool_size, &staging_buffer_info, ZEST_NULL);
		layer->memory_refs[fif].instance_ptr = layer->memory_refs[fif].staging_instance_data->data;
		layer->memory_refs[fif].staging_instance_data = layer->memory_refs[fif].staging_instance_data;
        layer->memory_refs[fif].instance_count = 0;
        zest_vec_clear(layer->draw_instructions[fif]);
        layer->memory_refs[fif].staging_instance_data->memory_in_use = 0;
        layer->current_instruction = zest__layer_instruction();
        layer->memory_refs[fif].instance_count = 0;
        layer->memory_refs[fif].instance_ptr = layer->memory_refs[fif].staging_instance_data->data;
    }

    zest_SetLayerViewPort(layer, 0, 0, zest_SwapChainWidth(), zest_SwapChainHeight(), zest_SwapChainWidthf(), zest_SwapChainHeightf());
}

zest_layer zest_CreateFontLayer(const char *name) {
    zest_layer_builder_t builder = zest_NewInstanceLayerBuilder(sizeof(zest_sprite_instance_t));
    zest_layer layer = zest_BuildInstanceLayer(name, &builder);
    return layer;
}

void zest_DrawSprite(zest_layer layer, zest_image image, float x, float y, float r, float sx, float sy, float hx, float hy, zest_uint alignment, float stretch) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_CHECK_HANDLE(image);	//Not a valid handle!
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_instance);    //Call zest_StartSpriteDrawing before calling this function

    zest_sprite_instance_t* sprite = (zest_sprite_instance_t*)layer->memory_refs[layer->fif].instance_ptr;

    sprite->size_handle = zest_Pack16bit4SScaled(sx, sy, hx, hy, 4096.f, 128.f);
    sprite->position_rotation = zest_Vec4Set(x, y, stretch, r);
    sprite->uv = image->uv;
    sprite->alignment = alignment;
    sprite->color = layer->current_color;
    sprite->intensity_texture_array = image->layer;
    sprite->intensity_texture_array = (image->layer << 24) + (zest_uint)(layer->intensity * 0.125f * 4194303.f);

    zest_NextInstance(layer);
}

void zest_DrawSpritePacked(zest_layer layer, zest_image image, float x, float y, float r, zest_u64 size_handle, zest_uint alignment, float stretch) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_CHECK_HANDLE(image);	//Not a valid handle!
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_instance);    //Call zest_StartSpriteDrawing before calling this function

    zest_sprite_instance_t* sprite = (zest_sprite_instance_t*)layer->memory_refs[layer->fif].instance_ptr;

    sprite->size_handle = size_handle;
    sprite->position_rotation = zest_Vec4Set(x, y, stretch, r);
    sprite->uv = image->uv;
    sprite->alignment = alignment;
    sprite->color = layer->current_color;
    sprite->intensity_texture_array = image->layer;
    sprite->intensity_texture_array = (image->layer << 24) + (zest_uint)(layer->intensity * 0.125f * 4194303.f);

    zest_NextInstance(layer);
}

void zest_DrawTexturedSprite(zest_layer layer, zest_image image, float x, float y, float width, float height, float scale_x, float scale_y, float offset_x, float offset_y) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_CHECK_HANDLE(image);	//Not a valid handle!
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_instance);    //Call zest_StartSpriteDrawing before calling this function

    zest_sprite_instance_t* sprite = (zest_sprite_instance_t*)layer->memory_refs[layer->fif].instance_ptr;

    if (offset_x || offset_y) {
        offset_x = fmodf(offset_x, (float)image->width) / image->width;
        offset_y = fmodf(offset_y, (float)image->height) / image->height;
        offset_x *= scale_x;
        offset_y *= scale_y;
    }
    scale_x *= width / (float)image->width;
    scale_y *= height / (float)image->height;
    sprite->uv = zest_Vec4Set(-offset_x, -offset_y, scale_x - offset_x, scale_y - offset_y);

    sprite->size_handle = zest_Pack16bit4SScaledZWPacked(width, height, 0, 4096.f);
    sprite->position_rotation = zest_Vec4Set(x, y, 0.f, 0.f);
    sprite->alignment = 0;
    sprite->color = layer->current_color;
    sprite->intensity_texture_array = (image->layer << 24);
    sprite->intensity_texture_array = (image->layer << 24) + (zest_uint)(layer->intensity * 0.125f * 4194303.f);

    zest_NextInstance(layer);
}
//-- End Sprite Drawing API

//-- Start Font Drawing API
void zest_SetMSDFFontDrawing(zest_layer layer, zest_font font) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_CHECK_HANDLE(font);	//Not a valid handle!
    zest_EndInstanceInstructions(layer);
    zest_StartInstanceInstructions(layer);
    ZEST_ASSERT(ZEST__FLAGGED(font->texture->flags, zest_texture_flag_ready));        //Make sure the font is properly loaded or wasn't recently deleted
    layer->current_instruction.pipeline_template = font->pipeline_template;
	layer->current_instruction.shader_resources = font->shader_resources;
    layer->current_instruction.draw_mode = zest_draw_mode_text;
    layer->current_instruction.asset = font;
    zest_font_push_constants_t push = { 0 };

    //Font defaults.
    push.shadow_vector.x = 1.f;
    push.shadow_vector.y = 1.f;
    push.shadow_smoothing = 0.2f;
    push.shadow_clipping = 0.f;
    push.radius = 25.f;
    push.bleed = 0.25f; 
    push.aa_factor = 5.f;
    push.thickness = 5.5f;
    push.font_texture_index = font->texture->descriptor_array_index;

    (*(zest_font_push_constants_t*)layer->current_instruction.push_constant) = push;
    layer->last_draw_mode = zest_draw_mode_text;
}

void zest_SetMSDFFontShadow(zest_layer layer, float shadow_length, float shadow_smoothing, float shadow_clipping) {
    zest_vec2 shadow = zest_NormalizeVec2(zest_Vec2Set(1.f, 1.f));
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->shadow_vector.x = shadow.x * shadow_length;
    push->shadow_vector.y = shadow.y * shadow_length;
    push->shadow_smoothing = shadow_smoothing;
    push->shadow_clipping = shadow_clipping;
}

void zest_SetMSDFFontShadowColor(zest_layer layer, float red, float green, float blue, float alpha) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->shadow_color = zest_Vec4Set(red, green, blue, alpha);
}

void zest_TweakMSDFFont(zest_layer layer, float bleed, float thickness, float aa_factor, float radius) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->radius = radius;
    push->bleed = bleed;
    push->aa_factor = aa_factor;
    push->thickness = thickness;
}

void zest_SetMSDFFontBleed(zest_layer layer, float bleed) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->bleed = bleed;
}

void zest_SetMSDFFontThickness(zest_layer layer, float thickness) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->thickness = thickness;
}

void zest_SetMSDFFontAAFactor(zest_layer layer, float aa_factor) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->aa_factor = aa_factor;
}

void zest_SetMSDFFontRadius(zest_layer layer, float radius) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->radius = radius;
}

void zest_SetMSDFFontDetail(zest_layer layer, float bleed) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->bleed = bleed;
}

float zest_DrawMSDFText(zest_layer layer, const char* text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_text);        //Call zest_StartFontDrawing before calling this function

    zest_font font = (zest_font)(layer->current_instruction.asset);

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
        zest_sprite_instance_t* font_instance = (zest_sprite_instance_t*)layer->memory_refs[layer->fif].instance_ptr;
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

		font_instance->size_handle = zest_Pack16bit4SScaledZWPacked(width, height, 0, 4096.f);
        font_instance->position_rotation = zest_Vec4Set(xpos + xoffset, y + yoffset, 0.f, 0.f);
        font_instance->uv = character->uv;
        font_instance->alignment = 1;
        font_instance->color = layer->current_color;
        font_instance->intensity_texture_array = 0;
		font_instance->intensity_texture_array = (zest_uint)(layer->intensity * 0.125f * 4194303.f);

        zest_NextInstance(layer);

        xpos += character->xadvance * size + letter_spacing;
    }

    return xpos;
}

float zest_DrawMSDFParagraph(zest_layer layer, const char* text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing, float line_height) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_text);        //Call zest_StartFontDrawing before calling this function

    zest_font font = (zest_font)(layer->current_instruction.asset);

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
        zest_sprite_instance_t* font_instance = (zest_sprite_instance_t*)layer->memory_refs[layer->fif].instance_ptr;
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

		font_instance->size_handle = zest_Pack16bit4SScaledZWPacked(width, height, 0, 4096.f);
        font_instance->position_rotation = zest_Vec4Set(xpos + xoffset, y + yoffset, 0.f, 0.f);
        font_instance->uv = character->uv;
        font_instance->alignment = 1;
        font_instance->color = layer->current_color;
		font_instance->intensity_texture_array = (zest_uint)(layer->intensity * 0.125f * 4194303.f);

        zest_NextInstance(layer);

        xpos += character->xadvance * size + letter_spacing;
    }

    return xpos;
}

float zest_TextWidth(zest_font font, const char* text, float font_size, float letter_spacing) {
    ZEST_CHECK_HANDLE(font);	//Not a valid handle!

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


//-- Start Instance Drawing API
void zest_SetInstanceDrawing(zest_layer layer, zest_shader_resources shader_resources, zest_pipeline_template pipeline) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    zest_EndInstanceInstructions(layer);
    zest_StartInstanceInstructions(layer);
    layer->current_instruction.pipeline_template = pipeline;
	ZEST_CHECK_HANDLE(shader_resources);   //You must specifiy the shader resources to draw with
	layer->current_instruction.shader_resources = shader_resources;
    layer->current_instruction.draw_mode = zest_draw_mode_instance;
    layer->last_draw_mode = zest_draw_mode_instance;
}

void zest_SetLayerDrawingViewport(zest_layer layer, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    zest_EndInstanceInstructions(layer);
    zest_StartInstanceInstructions(layer);
    layer->current_instruction.scissor = zest_CreateRect2D(scissor_width, scissor_height, x, y);
    layer->current_instruction.viewport = zest_CreateViewport((float)x, (float)y, viewport_width, viewport_height, 0.f, 1.f);
    layer->current_instruction.draw_mode = zest_draw_mode_viewport;
}

void zest_SetLayerPushConstants(zest_layer layer, void *push_constants, zest_size size) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_ASSERT(size <= 128);    //Push constant size must not exceed 128 bytes
    memcpy(layer->current_instruction.push_constant, push_constants, size);
}

//-- Start Billboard Drawing API
void zest_DrawBillboard(zest_layer layer, zest_image image, float position[3], zest_uint alignment, float angles[3], float handle[2], float stretch, zest_uint alignment_type, float sx, float sy) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_CHECK_HANDLE(image);	//Not a valid handle!
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_instance);    //Call zest_StartSpriteDrawing before calling this function

    zest_billboard_instance_t* billboard = (zest_billboard_instance_t*)layer->memory_refs[layer->fif].instance_ptr;

    billboard->position = zest_Vec3Set(position[0], position[1], position[2]);
    billboard->rotations_stretch = zest_Vec4Set(angles[0], angles[1], angles[2], stretch);
    billboard->uv = image->uv_packed;
    billboard->scale_handle = zest_Pack16bit4SScaled(sx, sy, handle[0], handle[1], 256.f, 128.f);
    billboard->alignment = alignment;
    billboard->color = layer->current_color;
    billboard->intensity_texture_array = (image->layer << 24) + (alignment_type << 22) + (zest_uint)(layer->intensity * 0.125f * 4194303.f);

    zest_NextInstance(layer);
}

void zest_DrawBillboardPacked(zest_layer layer, zest_image image, float position[3], zest_uint alignment, float angles[3], zest_u64 scale_handle, float stretch, zest_uint alignment_type) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_CHECK_HANDLE(image);	//Not a valid handle!
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_instance);    //Call zest_StartSpriteDrawing before calling this function

    zest_billboard_instance_t* billboard = (zest_billboard_instance_t*)layer->memory_refs[layer->fif].instance_ptr;

    billboard->position = zest_Vec3Set(position[0], position[1], position[2]);
    billboard->rotations_stretch = zest_Vec4Set(angles[0], angles[1], angles[2], stretch);
    billboard->uv = image->uv_packed;
    billboard->scale_handle = scale_handle;
    billboard->alignment = alignment;
    billboard->color = layer->current_color;
    billboard->intensity_texture_array = (image->layer << 24) + (alignment_type << 22) + (zest_uint)(layer->intensity * 0.125f * 4194303.f);

    zest_NextInstance(layer);
}

void zest_DrawBillboardSimple(zest_layer layer, zest_image image, float position[3], float angle, float sx, float sy) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_CHECK_HANDLE(image);	//Not a valid handle!
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_instance);        //Call zest_StartSpriteDrawing before calling this function

    zest_billboard_instance_t* billboard = (zest_billboard_instance_t*)layer->memory_refs[layer->fif].instance_ptr;

    billboard->scale_handle = zest_Pack16bit4SScaled(sx, sy, 0.5f, 0.5f, 256.f, 128.f);
    billboard->position = zest_Vec3Set(position[0], position[1], position[2]);
    billboard->rotations_stretch = zest_Vec4Set( 0.f, 0.f, angle, 0.f );
    billboard->uv = image->uv_packed;
    billboard->alignment = 16711680;        //x = 0.f, y = 1.f, z = 0.f;
    billboard->color = layer->current_color;
    billboard->intensity_texture_array = (image->layer << 24) + (zest_uint)(layer->intensity * 0.125f * 4194303.f);

    zest_NextInstance(layer);
}
//-- End Billboard Drawing API


//-- Start 3D Line Drawing API
void zest_Set3DLineDrawing(zest_layer layer, zest_shader_resources shader_resources, zest_pipeline_template pipeline) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_CHECK_HANDLE(shader_resources);	//Not a valid handle!
    zest_EndInstanceInstructions(layer);
    zest_StartInstanceInstructions(layer);
    layer->current_instruction.pipeline_template = pipeline;
	layer->current_instruction.shader_resources = shader_resources;
    layer->current_instruction.scissor = layer->scissor;
    layer->current_instruction.viewport = layer->viewport;
    layer->current_instruction.draw_mode = zest_draw_mode_line_instance;
    layer->last_draw_mode = zest_draw_mode_line_instance;
}

void zest_Draw3DLine(zest_layer layer, float start_point[3], float end_point[3], float width) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_line_instance || layer->current_instruction.draw_mode == zest_draw_mode_dashed_line);	//Call zest_StartSpriteDrawing before calling this function

    zest_line_instance_t* line = (zest_line_instance_t*)layer->memory_refs[layer->fif].instance_ptr;

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

    zest_NextInstance(layer);
}
//-- End Line Drawing API

//-- Start Mesh Drawing API

void zest__initialise_mesh_layer(zest_layer mesh_layer, zest_size vertex_struct_size, zest_size initial_vertex_capacity) {
    mesh_layer->current_color.r = 255;
    mesh_layer->current_color.g = 255;
    mesh_layer->current_color.b = 255;
    mesh_layer->current_color.a = 255;
    mesh_layer->intensity = 1;
    mesh_layer->layer_size = zest_Vec2Set1(1.f);
    mesh_layer->vertex_struct_size = vertex_struct_size;

    zest_buffer_info_t device_vertex_buffer_info = zest_CreateVertexBufferInfo(0);
    zest_buffer_info_t device_index_buffer_info = zest_CreateIndexBufferInfo(0);
    if (zest_GPUHasDeviceLocalHostVisible(initial_vertex_capacity)) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating Device local buffers for mesh layer");
        ZEST__FLAG(mesh_layer->flags, zest_layer_flag_device_local_direct);
        device_vertex_buffer_info = zest_CreateVertexBufferInfo(ZEST_TRUE);
        device_index_buffer_info = zest_CreateIndexBufferInfo(ZEST_TRUE);
    }

    zest_buffer_info_t staging_buffer_info = zest_CreateStagingBufferInfo();

    zest_ForEachFrameInFlight(fif) {
        mesh_layer->memory_refs[fif].index_count = 0;
        mesh_layer->memory_refs[fif].index_position = 0;
        mesh_layer->memory_refs[fif].last_index = 0;
        mesh_layer->memory_refs[fif].vertex_count = 0;
		mesh_layer->memory_refs[fif].staging_vertex_data = zest_CreateBuffer(initial_vertex_capacity, &staging_buffer_info, ZEST_NULL);
		mesh_layer->memory_refs[fif].staging_index_data = zest_CreateBuffer(initial_vertex_capacity, &staging_buffer_info, ZEST_NULL);
		mesh_layer->memory_refs[fif].vertex_ptr = mesh_layer->memory_refs[fif].staging_vertex_data->data;
		mesh_layer->memory_refs[fif].index_ptr = mesh_layer->memory_refs[fif].staging_index_data->data;
		mesh_layer->memory_refs[fif].staging_vertex_data = mesh_layer->memory_refs[fif].staging_vertex_data;
		mesh_layer->memory_refs[fif].staging_index_data = mesh_layer->memory_refs[fif].staging_index_data;
    }

    zest_SetLayerViewPort(mesh_layer, 0, 0, zest_SwapChainWidth(), zest_SwapChainHeight(), zest_SwapChainWidthf(), zest_SwapChainHeightf());
    zest_ForEachFrameInFlight(fif) {
        zest_vec_clear(mesh_layer->draw_instructions[fif]);
        mesh_layer->memory_refs[fif].staging_vertex_data->memory_in_use = 0;
        mesh_layer->memory_refs[fif].staging_index_data->memory_in_use = 0;
        mesh_layer->current_instruction = zest__layer_instruction();
        mesh_layer->memory_refs[fif].index_count = 0;
        mesh_layer->memory_refs[fif].vertex_count = 0;
        mesh_layer->memory_refs[fif].vertex_ptr = mesh_layer->memory_refs[fif].staging_vertex_data->data;
        mesh_layer->memory_refs[fif].index_ptr = mesh_layer->memory_refs[fif].staging_index_data->data;
    }
}

void zest_BindMeshVertexBuffer(VkCommandBuffer command_buffer, zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_ASSERT(layer->vertex_data);    //There's no vertex data in the buffer. Did you call zest_AddMeshToLayer?
    zest_buffer_t *buffer = layer->vertex_data;
    VkDeviceSize offsets[] = { buffer->memory_offset };
    vkCmdBindVertexBuffers(command_buffer, 0, 1, zest_GetBufferDeviceBuffer(buffer), offsets);
}

void zest_BindMeshIndexBuffer(VkCommandBuffer command_buffer, zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_ASSERT(layer->index_data);    //There's no index data in the buffer. Did you call zest_AddMeshToLayer?
    zest_buffer_t *buffer = layer->index_data;
    vkCmdBindIndexBuffer(command_buffer, *zest_GetBufferDeviceBuffer(buffer), buffer->memory_offset, VK_INDEX_TYPE_UINT32);
}

zest_buffer zest_GetVertexWriteBuffer(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    return layer->memory_refs[layer->fif].staging_vertex_data;
}

zest_buffer zest_GetIndexWriteBuffer(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    return layer->memory_refs[layer->fif].staging_index_data;
}

void zest_GrowMeshVertexBuffers(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    zest_size memory_in_use = zest_GetVertexWriteBuffer(layer)->memory_in_use;
    zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_vertex_data, layer->vertex_struct_size, memory_in_use);
}

void zest_GrowMeshIndexBuffers(zest_layer layer) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    zest_size memory_in_use = zest_GetVertexWriteBuffer(layer)->memory_in_use;
    zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_index_data, sizeof(zest_uint), memory_in_use);
}

void zest_UploadMeshBuffersCallback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer) {
    zest_layer layer = (zest_layer)draw_routine->draw_data;
    zest__end_mesh_instructions(layer);

    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_device_local_direct)) {
        zest_buffer staging_vertex = layer->memory_refs[layer->fif].staging_vertex_data;
        zest_buffer staging_index = layer->memory_refs[layer->fif].staging_index_data;

        if (staging_vertex->memory_in_use && staging_index->memory_in_use) {
            //zest_AddCopyCommand(&layer->vertex_upload, staging_vertex, device_vertex, 0);
            //zest_AddCopyCommand(&layer->index_upload, staging_index, device_index, 0);
        }

        zest_UploadBuffer(&layer->vertex_upload, command_buffer);
        zest_UploadBuffer(&layer->index_upload, command_buffer);
    }
}

void zest_PushVertex(zest_layer layer, float pos_x, float pos_y, float pos_z, float intensity, float uv_x, float uv_y, zest_color color, zest_uint parameters) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    zest_textured_vertex_t vertex = { 0 };
    vertex.pos = zest_Vec3Set(pos_x, pos_y, pos_z);
    vertex.intensity = intensity;
    vertex.uv = zest_Vec2Set(uv_x, uv_y);
    vertex.color = color;
    vertex.parameters = parameters;
    zest_textured_vertex_t* vertex_ptr = (zest_textured_vertex_t*)layer->memory_refs[layer->fif].vertex_ptr;
    *vertex_ptr = vertex;
    vertex_ptr = vertex_ptr + 1;
    ZEST_ASSERT(vertex_ptr >= (zest_textured_vertex_t*)layer->memory_refs[layer->fif].staging_vertex_data->data && vertex_ptr <= (zest_textured_vertex_t*)layer->memory_refs[layer->fif].staging_vertex_data->end);
    if (vertex_ptr == layer->memory_refs[layer->fif].staging_vertex_data->end) {
        zest_bool grown = 0;
		grown = zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_vertex_data, sizeof(zest_textured_vertex_t), 0);
		layer->memory_refs[layer->fif].staging_vertex_data = layer->memory_refs[layer->fif].staging_vertex_data;
        if (grown) {
            layer->memory_refs[layer->fif].vertex_count++;
            vertex_ptr = (zest_textured_vertex_t*)layer->memory_refs[layer->fif].staging_vertex_data->data;
            vertex_ptr += layer->memory_refs[layer->fif].vertex_count;
        }
        else {
            vertex_ptr = vertex_ptr - 1;
        }
    }
    else {
        layer->memory_refs[layer->fif].vertex_count++;
    }
    layer->memory_refs[layer->fif].vertex_ptr = vertex_ptr;
}

void zest_PushIndex(zest_layer layer, zest_uint offset) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    zest_uint index = layer->memory_refs[layer->fif].vertex_count + offset;
    zest_uint* index_ptr = (zest_uint*)layer->memory_refs[layer->fif].index_ptr;
    *index_ptr = index;
    index_ptr = index_ptr + 1;
    ZEST_ASSERT(index_ptr >= (zest_uint*)layer->memory_refs[layer->fif].staging_index_data->data && index_ptr <= (zest_uint*)layer->memory_refs[layer->fif].staging_index_data->end);
    if (index_ptr == layer->memory_refs[layer->fif].staging_index_data->end) {
        zest_bool grown = 0;
		grown = zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_vertex_data, sizeof(zest_uint), 0);
		layer->memory_refs[layer->fif].staging_vertex_data = layer->memory_refs[layer->fif].staging_index_data;
        if (grown) {
            layer->memory_refs[layer->fif].index_count++;
            layer->current_instruction.total_indexes++;
            index_ptr = (zest_uint*)layer->memory_refs[layer->fif].staging_index_data->data;
            index_ptr += layer->memory_refs[layer->fif].index_count;
        }
        else {
            index_ptr = index_ptr - 1;
        }
    }
    else {
        layer->memory_refs[layer->fif].index_count++;
        layer->current_instruction.total_indexes++;
    }
    layer->memory_refs[layer->fif].index_ptr = index_ptr;
}

void zest_SetMeshDrawing(zest_layer layer, zest_shader_resources shader_resources, zest_pipeline_template pipeline) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_CHECK_HANDLE(shader_resources);	//Not a valid handle!
    ZEST_CHECK_HANDLE(pipeline);	//Not a valid handle!
    zest__end_mesh_instructions(layer);
    zest__start_mesh_instructions(layer);
    layer->current_instruction.pipeline_template = pipeline;
	layer->current_instruction.shader_resources = shader_resources;
    layer->current_instruction.draw_mode = zest_draw_mode_mesh;
    layer->last_draw_mode = zest_draw_mode_mesh;
}

void zest_DrawTexturedPlane(zest_layer layer, zest_image image, float x, float y, float z, float width, float height, float scale_x, float scale_y, float offset_x, float offset_y) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_CHECK_HANDLE(image);	//Not a valid handle!
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
void zest_SetInstanceMeshDrawing(zest_layer layer, zest_shader_resources shader_resources, zest_pipeline_template pipeline) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_CHECK_HANDLE(shader_resources);	//Not a valid handle!
    ZEST_CHECK_HANDLE(pipeline);	//Not a valid handle!
    zest_EndInstanceInstructions(layer);
    zest_StartInstanceInstructions(layer);
    layer->current_instruction.pipeline_template = pipeline;
	layer->current_instruction.shader_resources = shader_resources;
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
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    zest_buffer vertex_staging_buffer = zest_CreateStagingBuffer(zest_MeshVertexDataSize(src_mesh), src_mesh->vertices);
    zest_buffer index_staging_buffer = zest_CreateStagingBuffer(zest_MeshIndexDataSize(src_mesh), src_mesh->indexes);
    layer->index_count = zest_vec_size(src_mesh->indexes);
	zest_FreeBuffer(layer->vertex_data);
	zest_FreeBuffer(layer->index_data);
	layer->vertex_data = zest_CreateVertexBuffer(vertex_staging_buffer->size, 0);
	layer->index_data = zest_CreateIndexBuffer(index_staging_buffer->size, 0);
    zest_CopyBufferOneTime(vertex_staging_buffer, layer->vertex_data, vertex_staging_buffer->size);
    zest_CopyBufferOneTime(index_staging_buffer, layer->index_data, index_staging_buffer->size);
    zest_FreeBuffer(vertex_staging_buffer);
    zest_FreeBuffer(index_staging_buffer);
}

void zest_DrawInstancedMesh(zest_layer layer, float pos[3], float rot[3], float scale[3]) {
    ZEST_CHECK_HANDLE(layer);	//Not a valid handle!
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_instance);        //Call zest_StartSpriteDrawing before calling this function

    zest_mesh_instance_t* instance = (zest_mesh_instance_t*)layer->memory_refs[layer->fif].instance_ptr;

    instance->pos = zest_Vec3Set(pos[0], pos[1], pos[2]);
    instance->rotation = zest_Vec3Set(rot[0], rot[1], rot[2]);
    instance->scale = zest_Vec3Set(scale[0], scale[1], scale[2]);
    instance->color = layer->current_color;
    instance->parameters = 0;

    zest_NextInstance(layer);
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
zest_compute zest__create_compute(const char* name) {
    zest_compute_t blank_compute = { 0 };
    zest_compute compute = ZEST__NEW_ALIGNED(zest_compute, 16);
    *compute = blank_compute;
    compute->magic = zest_INIT_MAGIC;
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
        compute->fence[i] = VK_NULL_HANDLE;
        compute->fif_incoming_semaphore[i] = VK_NULL_HANDLE;
        compute->fif_outgoing_semaphore[i] = VK_NULL_HANDLE;
    }
    zest_map_insert(ZestRenderer->computes, name, compute);
    return compute;
}

zest_compute_builder_t zest_BeginComputeBuilder() {
    zest_compute_builder_t builder = { 0 };
    //builder.descriptor_update_callback = zest_StandardComputeDescriptorUpdate;
    return builder;
}

zest_compute zest_RegisterCompute() {
    zest_compute_t blank_compute = { 0 };
    zest_compute compute = ZEST__NEW(zest_compute);
    *compute = blank_compute;
    compute->magic = zest_INIT_MAGIC;
    return compute;
}

void zest_SetComputeBindlessLayout(zest_compute_builder_t *builder, zest_set_layout bindless_layout) {
    ZEST_CHECK_HANDLE(bindless_layout); //Not a valid descriptor set layout!
    builder->bindless_layout = bindless_layout;
}

void zest_AddComputeSetLayout(zest_compute_builder_t *builder, zest_set_layout layout) {
    ZEST_CHECK_HANDLE(layout); //Not a valid descriptor set layout!
    zest_vec_push(builder->non_bindless_layouts, layout);
}

zest_index zest_AddComputeShader(zest_compute_builder_t* builder, zest_shader shader) {
    zest_text_t path_text = { 0 };
    zest_vec_push(builder->shaders, shader);
    return zest_vec_last_index(builder->shaders);
}

void zest_SetComputePushConstantSize(zest_compute_builder_t* builder, zest_uint size) {
    assert(size && size <= 256);        //push constants must be within 0 and 256 bytes in size
    builder->push_constant_size = size;
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

void zest_SetComputeManualRecord(zest_compute_builder_t *builder) {
    ZEST__FLAG(builder->flags, zest_compute_flag_manual_fif);
}

void zest_SetComputePrimaryRecorder(zest_compute_builder_t *builder) {
    ZEST__FLAG(builder->flags, zest_compute_flag_primary_recorder);
}

zest_compute zest_FinishCompute(zest_compute_builder_t *builder, const char *name) {
	zest_compute compute = zest__create_compute(name);
    ZEST_CHECK_HANDLE(compute);	//Not a valid handle! Unable to create a compute object
    compute->user_data = builder->user_data;
    compute->flags = builder->flags;

    compute->queue = ZestDevice->compute_queue.vk_queue;
    compute->bindless_layout = builder->bindless_layout;

    // Create compute pipeline
    // Compute pipelines are created separate from graphics pipelines even if they use the same queue

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = { 0 };
    if (builder->push_constant_size) {
        compute->pushConstantRange.offset = 0;
        compute->pushConstantRange.size = builder->push_constant_size;
        compute->pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline_layout_create_info.pPushConstantRanges = &compute->pushConstantRange;
        pipeline_layout_create_info.pushConstantRangeCount = 1;
    }
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkDescriptorSetLayout *set_layouts = 0;
    if (compute->bindless_layout) {
        zest_vec_push(set_layouts, compute->bindless_layout->vk_layout);
    }
    zest_vec_foreach(i, builder->non_bindless_layouts) {
        zest_vec_push(set_layouts, builder->non_bindless_layouts[i]->vk_layout);
    }
    pipeline_layout_create_info.setLayoutCount = zest_vec_size(set_layouts);
    pipeline_layout_create_info.pSetLayouts = set_layouts;

    ZEST_VK_CHECK_RESULT(vkCreatePipelineLayout(ZestDevice->logical_device, &pipeline_layout_create_info, &ZestDevice->allocation_callbacks, &compute->pipeline_layout));
    zest_vec_free(set_layouts);

    // Create compute shader pipelines
    VkComputePipelineCreateInfo compute_pipeline_create_info = { 0 };
    compute_pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_create_info.layout = compute->pipeline_layout;
    compute_pipeline_create_info.flags = 0;

    // One pipeline for each effect
    ZEST_ASSERT(zest_vec_size(builder->shaders));
    compute->shaders = builder->shaders;
    VkShaderModule shader_module = { 0 };
    for (zest_foreach_i(compute->shaders)) {
        zest_shader shader = compute->shaders[i];

        ZEST_ASSERT(shader->spv);   //Compile the shader first before making the compute pipeline
		shader_module = zest__create_shader_module(shader->spv);

        VkPipelineShaderStageCreateInfo compute_shader_stage_info = { 0 };
        compute_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compute_shader_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compute_shader_stage_info.module = shader_module;
        compute_shader_stage_info.pName = "main";
        compute_pipeline_create_info.stage = compute_shader_stage_info;

        VkPipeline pipeline;
        //If you get validation errors here or even a crash then make sure that you're shader has the correct inputs that match the
        //compute builder set up. For example if the shader expects a push constant range then use zest_SetComputePushConstantSize before
        //calling this function.
        ZEST_VK_CHECK_RESULT(vkCreateComputePipelines(ZestDevice->logical_device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, &ZestDevice->allocation_callbacks, &pipeline));
        compute->pipeline =  pipeline;
        vkDestroyShaderModule(ZestDevice->logical_device, shader_module, &ZestDevice->allocation_callbacks);
    }

    if (ZEST__FLAGGED(compute->flags, zest_compute_flag_primary_recorder)) {
        compute->recorder = zest_CreatePrimaryRecorderWithPool(ZestDevice->compute_queue_family_index);
    } else {
        compute->recorder = zest_CreateSecondaryRecorder();
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
    ZEST__MAYBE_FLAG(compute->flags, zest_compute_flag_manual_fif, builder->flags & zest_compute_flag_manual_fif);

    VkSemaphoreCreateInfo semaphore_info = { 0 };
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (ZEST_EACH_FIF_i) {
        compute->fif_outgoing_semaphore[i] = VK_NULL_HANDLE;
        ZEST_VK_CHECK_RESULT(vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, &ZestDevice->allocation_callbacks, &compute->fif_outgoing_semaphore[i]))
    }

    if (builder->command_buffer_update_callback) {
        compute->command_buffer_update_callback = builder->command_buffer_update_callback;
    }

    ZEST__FLAG(compute->flags, zest_compute_flag_is_active);

    zest_vec_free(builder->non_bindless_layouts);
    zest_vec_free(builder->shaders);

    return compute;
}

void zest_RunCompute(zest_compute compute) {
    ZEST_CHECK_HANDLE(compute);	//Not a valid handle!
    ZEST_ASSERT(ZEST__FLAGGED(compute->flags, zest_compute_flag_primary_recorder)); //Compute must have been initialised with zest_SetComputePrimaryRecorder
    vkWaitForFences(ZestDevice->logical_device, 1, &compute->fence[ZEST_FIF], VK_TRUE, UINT64_MAX);
    vkResetFences(ZestDevice->logical_device, 1, &compute->fence[ZEST_FIF]);

    if (ZEST__FLAGGED(compute->flags, zest_compute_flag_rewrite_command_buffer)) {
        VkCommandBufferBeginInfo begin_info = { 0 };
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        ZEST_VK_CHECK_RESULT(vkBeginCommandBuffer(compute->recorder->command_buffer[ZEST_FIF], &begin_info));
        ZEST_ASSERT(compute->command_buffer_update_callback);        //You must set a call back for command_buffer_update_callback so that you can build the command buffer for the compute shader
        //if you're going to run the compute shader via this function. See zest_SetComputeCommandBufferUpdateCallback when building your
        //compute shader
        compute->command_buffer_update_callback(compute, compute->recorder->command_buffer[ZEST_FIF]);
        vkEndCommandBuffer(compute->recorder->command_buffer[ZEST_FIF]);
    }

    VkSubmitInfo compute_submit_info = { 0 };
    compute_submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    compute_submit_info.commandBufferCount = 1;
    compute_submit_info.pCommandBuffers = &compute->recorder->command_buffer[ZEST_FIF];

    //Make sure you're ending the command buffer at the end of your command_buffer_update_callback before calling this. Use vkEndCommandBuffer
    ZEST_VK_CHECK_RESULT(vkQueueSubmit(compute->queue, 1, &compute_submit_info, compute->fence[ZEST_FIF]));
}

void zest_WaitForCompute(zest_compute compute) {
    ZEST_CHECK_HANDLE(compute);	//Not a valid handle!
    vkWaitForFences(ZestDevice->logical_device, 1, &compute->fence[ZEST_FIF], VK_TRUE, UINT64_MAX);
}

void zest_ComputeAttachRenderTarget(zest_compute compute, zest_render_target render_target) {
    ZEST_CHECK_HANDLE(compute);	//Not a valid handle!
    ZEST_CHECK_HANDLE(render_target);	//Not a valid handle!
    compute->render_target = render_target;
    ZEST__FLAG(compute->flags, zest_compute_flag_has_render_target);
}

void zest_UpdateComputeDescriptors(zest_compute compute, zest_uint fif) {
    ZEST_CHECK_HANDLE(compute);	//Not a valid handle!
    compute->descriptor_update_callback(compute, fif);
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
	vkDestroyPipeline(ZestDevice->logical_device, compute->pipeline, &ZestDevice->allocation_callbacks);
    vkDestroyPipelineLayout(ZestDevice->logical_device, compute->pipeline_layout, &ZestDevice->allocation_callbacks);
    vkDestroyCommandPool(ZestDevice->logical_device, compute->recorder->command_pool, &ZestDevice->allocation_callbacks);
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
                    if (render_target->post_process_record_callback) {
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
    timer->magic = zest_INIT_MAGIC;
    zest_TimerSetUpdateFrequency(timer, update_frequency);
    zest_TimerSetMaxFrames(timer, 4.0);
    zest_TimerReset(timer);
    return timer;
}

void zest_FreeTimer(zest_timer timer) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    ZEST__FREE(timer);
}

void zest_TimerReset(zest_timer timer) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    timer->start_time = (double)zest_Microsecs();
    timer->current_time = (double)zest_Microsecs();
}

double zest_TimerDeltaTime(zest_timer timer) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    return timer->delta_time;
}

void zest_TimerTick(zest_timer timer) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    timer->delta_time = (double)zest_Microsecs() - timer->start_time;
}

double zest_TimerUpdateTime(zest_timer timer) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    return timer->update_time;
}

double zest_TimerFrameLength(zest_timer timer) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    return timer->update_tick_length;
}

double zest_TimerAccumulate(zest_timer timer) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    double new_time = (double)zest_Microsecs();
    double frame_time = fabs(new_time - timer->current_time);
    frame_time = ZEST__MIN(frame_time, ZEST_MICROSECS_SECOND);
    timer->current_time = new_time;

    timer->accumulator_delta = timer->accumulator;
    timer->accumulator += frame_time;
    timer->accumulator = ZEST__MIN(timer->accumulator, timer->max_elapsed_time);
    timer->update_count = 0;
    return frame_time;
}

int zest_TimerPendingTicks(zest_timer timer) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    return (int)(timer->accumulator / timer->update_tick_length);
}

void zest_TimerUnAccumulate(zest_timer timer) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    timer->accumulator -= timer->update_tick_length;
    timer->accumulator_delta = timer->accumulator_delta - timer->accumulator;
    timer->update_count++;
    timer->time_passed += timer->update_time;
    timer->seconds_passed += timer->update_time * 1000.f;
}

zest_bool zest_TimerDoUpdate(zest_timer timer) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    return timer->accumulator >= timer->update_tick_length;
}

double zest_TimerLerp(zest_timer timer) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    return timer->update_count > 1 ? 1.f : timer->lerp;
}

void zest_TimerSet(zest_timer timer) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    timer->lerp = timer->accumulator / timer->update_tick_length;
}

void zest_TimerSetMaxFrames(zest_timer timer, double frames) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    timer->max_elapsed_time = timer->update_tick_length * frames;
}

void zest_TimerSetUpdateFrequency(zest_timer timer, double frequency) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    timer->update_frequency = frequency;
    timer->update_tick_length = ZEST_MICROSECS_SECOND / timer->update_frequency;
    timer->update_time = 1.f / timer->update_frequency;
    timer->ticker = zest_Microsecs() - timer->update_tick_length;
}

double zest_TimerUpdateFrequency(zest_timer timer) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    return timer->update_frequency;
}

zest_bool zest_TimerUpdateWasRun(zest_timer timer) {
    ZEST_CHECK_HANDLE(timer);	//Not a valid handle!
    return timer->update_count > 0;
}
//-- End Timer Functions

