/*
    Zest Pocket Renderer - Vulkan implementation

    [Backend_structs]
    [Struct_definitions]
    [Enum_translators]
    [Struct_translators]
    [Backend_helpers]
*/

#define ZEST_VULKAN_IMPLEMENTATION
#include "zest.h"
#define ZLOC_IMPLEMENTATION
#define ZEST_SPIRV_REFLECT_IMPLENTATION
//#define ZLOC_OUTPUT_ERROR_MESSAGES
//#define ZLOC_EXTRA_DEBUGGING
#define STB_RECT_PACK_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYKTX_IMPLEMENTATION
#include "lib_bundle.h"


zest_device_t* ZestDevice = 0;
zest_app_t* ZestApp = 0;
zest_renderer_t* ZestRenderer = 0;
zest_platform_t* ZestPlatform = 0;
zest_storage_t* zest__globals = 0;

// --[Struct_definitions]
typedef struct zest_mesh_t {
    int magic;
    zest_vertex_t* vertices;
    zest_uint* indexes;
} zest_mesh_t;

typedef struct zest_output_group_t {
    int magic;
    zest_resource_node *resources;
} zest_output_group_t;

typedef struct zest_timer_t {
    int magic;
    zest_timer_handle handle;
    double start_time;
    double delta_time;
    double update_frequency;
    double update_tick_length;
    double update_time;
    double ticker;
    double accumulator;
    double accumulator_delta;
    double current_time;
    double lerp;
    double time_passed;
    double seconds_passed;
    double max_elapsed_time;
    int update_count;
} zest_timer_t;

//A simple buffer struct for creating and mapping GPU memory
typedef struct zest_device_memory_pool_t {
    int magic;
    zest_memory_pool_flags flags;
    zest_device_memory_pool_backend backend;
    zest_size size;
    zest_size minimum_allocation_size;
    zest_size alignment;
    zest_uint memory_type_index;
    void* mapped;
    const char *name;
} zest_device_memory_pool_t;

typedef struct zest_swapchain_platform_t{
    VkSwapchainKHR vk_swapchain;
    VkSemaphore *vk_render_finished_semaphore;
    VkSemaphore vk_image_available_semaphore[ZEST_MAX_FIF];
    zest_clear_value_t clear_color;
} zest_swapchain_platform_t;

typedef struct zest_buffer_allocator_t {
    int magic;
    zest_buffer_info_t buffer_info;
    zloc_allocator *allocator;
    zest_size alignment;
    zest_device_memory_pool *memory_pools;
    zest_pool_range *range_pools;
} zest_buffer_allocator_t;

typedef struct zest_vk_barrier_info_t {
    VkPipelineStageFlags stage_mask;    // Pipeline stages this usage pertains to
    VkAccessFlags access_mask;          // Vulkan access mask for barriers
    VkImageLayout image_layout;      // Required VkImageLayout if it's an image
} zest_vk_barrier_info_t;

// -- End Struct Definions

// -- Enum_translators
ZEST_PRIVATE inline VkImageUsageFlags zest__to_vk_image_usage(zest_image_usage_flags flags) {
    return (VkImageUsageFlags)flags;
}

ZEST_PRIVATE inline zest_texture_format zest__from_vk_format(VkFormat format) {
    return (zest_texture_format)format;
}

ZEST_PRIVATE inline VkBufferUsageFlags zest__to_vk_buffer_usage(zest_buffer_usage_flags flags) {
    return (VkBufferUsageFlags)flags;
}

ZEST_PRIVATE inline VkMemoryPropertyFlags zest__to_vk_memory_property(zest_memory_property_flags flags) {
    return (VkMemoryPropertyFlags)flags;
}

ZEST_PRIVATE inline VkFilter zest__to_vk_filter(zest_filter_type type) {
    return (VkFilter)type;
}

ZEST_PRIVATE inline VkSamplerMipmapMode zest__to_vk_mipmap_mode(VkSamplerMipmapMode mode) {
    return (VkSamplerMipmapMode)mode;
}

ZEST_PRIVATE inline VkSamplerAddressMode zest__to_vk_sampler_address_mode(VkSamplerAddressMode mode) {
    return (VkSamplerAddressMode)mode;
}

ZEST_PRIVATE inline VkCompareOp zest__to_vk_compare_op(VkCompareOp op) {
    return (VkCompareOp)op;
}

ZEST_PRIVATE inline VkImageTiling zest__to_vk_image_tiling(VkImageTiling tiling) {
    return (VkImageTiling)tiling;
}

ZEST_PRIVATE inline VkSampleCountFlags zest__to_vk_sample_count(VkSampleCountFlags flags) {
    return (VkSampleCountFlags)flags;
}

// -- End Enum converters

// -- Struct_translators

ZEST_PRIVATE inline VkClearValue zest__to_vk_clear_value(zest_clear_value_t zest_value, zest_image_aspect_flags aspect) {
    VkClearValue vk_value;
    if (aspect & zest_image_aspect_color_bit) {
        memcpy(vk_value.color.float32, &zest_value.color, sizeof(float) * 4);
    }
    else if (aspect & (zest_image_aspect_depth_bit | zest_image_aspect_stencil_bit)) {
        vk_value.depthStencil.depth = zest_value.depth_stencil.depth;
        vk_value.depthStencil.stencil = zest_value.depth_stencil.stencil;
    } else {
        // Default case or error: clear to black
        memset(&vk_value, 0, sizeof(VkClearValue));
    }
    return vk_value;
}

// -- End Struct translators

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

void zest__destroy_window_callback(zest_window window, void* user_data) {
	zest__cleanup_window_backend(window);
    DestroyWindow(window->window_handle);
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
    ScreenToClient(ZestRenderer->main_window->window_handle, &cursor_position);
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

    zest_window window = zest_AllocateWindow();
    WNDCLASS window_class = { 0 };

    window->window_width = width;
    window->window_height = height;

    window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = zest__window_proc;
    window_class.hInstance = zest_window_instance;
    window_class.hIcon = LoadIcon(zest_window_instance, MAKEINTRESOURCE(IDI_APPLICATION));
    window_class.lpszClassName = "zest_app_class_name";

    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);

    if (GetClassInfoEx(zest_window_instance, window_class.lpszClassName, &wcex) == 0) {
        if (!RegisterClass(&window_class)) {
            ZEST_ASSERT(0);        //Failed to register window
        }
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

void zest__os_set_window_size(zest_window window, int width, int height) {
	HWND handle = (HWND)window->window_handle;
	DWORD style = GetWindowLong(handle, GWL_STYLE);
	DWORD ex_style = GetWindowLong(handle, GWL_EXSTYLE);

	RECT rect = { 0, 0, width, height };
	AdjustWindowRectEx(&rect, style, FALSE, ex_style);

	SetWindowPos(handle, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
	window->window_width = width;
	window->window_height = height;
}

void zest__os_set_window_mode(zest_window window, zest_window_mode mode) {
	window->mode = mode;
	HWND handle = (HWND)window->window_handle;
	DWORD style = GetWindowLong(handle, GWL_STYLE);

	switch (mode) {
		case zest_window_mode_fullscreen: {
			MONITORINFO mi = { sizeof(mi) };
			GetMonitorInfo(MonitorFromWindow(handle, MONITOR_DEFAULTTOPRIMARY), &mi);
			SetWindowLong(handle, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(handle, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
				mi.rcMonitor.right - mi.rcMonitor.left,
				mi.rcMonitor.bottom - mi.rcMonitor.top,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			break;
		}
		case zest_window_mode_borderless: {
			SetWindowLong(handle, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(handle, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			break;
		}
		case zest_window_mode_bordered: {
			SetWindowLong(handle, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
			SetWindowPos(handle, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			break;
		}
	}
}

void zest__os_add_platform_extensions() {
    zest_AddInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME);
    zest_AddInstanceExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
}

void zest__get_window_size_callback(void* user_data, int* fb_width, int* fb_height, int* window_width, int* window_height) {
    RECT window_rect;
    GetClientRect(ZestRenderer->main_window->window_handle, &window_rect);
    *fb_width = window_rect.right - window_rect.left;
    *fb_height = window_rect.bottom - window_rect.top;
    *window_width = *fb_width;
    *window_height = *fb_height;
}

void zest__os_set_window_title(const char* title) {
    SetWindowText(ZestRenderer->main_window->window_handle, title);
}

#else

FILE* zest__open_file(const char* file_name, const char* mode) {
    return fopen(file_name, mode);
}

zest_window zest__os_create_window(int x, int y, int width, int height, zest_bool maximised, const char* title) {
    ZEST_ASSERT(ZestDevice);        //Must initialise the ZestDevice first

    zest_window current_window = ZEST__NEW(zest_window);
    memset(current_window, 0, sizeof(zest_window_t));
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    if (maximised)
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    current_window->magic = zest_INIT_MAGIC(zest_struct_type_window);
    current_window->window_width = width;
    current_window->window_height = height;

    current_window->window_handle = glfwCreateWindow(width, height, title, 0, 0);
    if (!maximised) {
        glfwSetWindowPos(current_window->window_handle, x, y);
    }
    glfwSetWindowUserPointer(current_window->window_handle, ZestApp);

    if (maximised) {
        int width, height;
        glfwGetFramebufferSize(current_window->window_handle, &width, &height);
        current_window->window_width = width;
        current_window->window_height = height;
    }

    return current_window;
}

void zest__os_create_window_surface(zest_window current_window) {
    ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_surface);
    ZEST_VK_ASSERT_RESULT(glfwCreateWindowSurface(ZestDevice->backend->instance, current_window->window_handle, &ZestDevice->backend->allocation_callbacks, &current_window->backend->surface));
}

void zest__os_poll_events(void) {
    glfwPollEvents();
    double mouse_x, mouse_y;
    glfwGetCursorPos(ZestRenderer->current_window->window_handle, &mouse_x, &mouse_y);
    double last_mouse_x = ZestApp->mouse_x;
    double last_mouse_y = ZestApp->mouse_y;
    ZestApp->mouse_x = mouse_x;
    ZestApp->mouse_y = mouse_y;
    ZestApp->mouse_delta_x = last_mouse_x - ZestApp->mouse_x;
    ZestApp->mouse_delta_y = last_mouse_y - ZestApp->mouse_y;
    ZestApp->flags |= glfwWindowShouldClose(ZestRenderer->current_window->window_handle) ? zest_app_flag_quit_application : 0;
    ZestApp->mouse_button = 0;
    int left = glfwGetMouseButton(ZestRenderer->current_window->window_handle, GLFW_MOUSE_BUTTON_LEFT);
    if (left == GLFW_PRESS) {
        ZestApp->mouse_button |= 1;
    }

    int right = glfwGetMouseButton(ZestRenderer->current_window->window_handle, GLFW_MOUSE_BUTTON_RIGHT);
    if (right == GLFW_PRESS) {
        ZestApp->mouse_button |= 2;
    }

    int middle = glfwGetMouseButton(ZestRenderer->current_window->window_handle, GLFW_MOUSE_BUTTON_MIDDLE);
    if (middle == GLFW_PRESS) {
        ZestApp->mouse_button |= 4;
    }
}

void zest__os_set_window_mode(zest_window window, zest_window_mode mode) {
    GLFWwindow *handle = (GLFWwindow *)window->window_handle;
    static int last_x, last_y, last_width, last_height;

    switch (mode) {
    case zest_window_mode_fullscreen:
    {
        if (!glfwGetWindowMonitor(handle)) {
            glfwGetWindowPos(handle, &last_x, &last_y);
            glfwGetWindowSize(handle, &last_width, &last_height);
        }

        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *vidmode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(handle, monitor, 0, 0, vidmode->width, vidmode->height, vidmode->refreshRate);
        break;
    }
    case zest_window_mode_bordered:
        if (glfwGetWindowMonitor(handle)) {
            glfwSetWindowMonitor(handle, NULL, last_x, last_y, last_width, last_height, 0);
        }
        glfwSetWindowAttrib(handle, GLFW_DECORATED, GLFW_TRUE);
        break;
    case zest_window_mode_borderless:
        if (glfwGetWindowMonitor(handle)) {
            glfwSetWindowMonitor(handle, NULL, last_x, last_y, last_width, last_height, 0);
        }
        glfwSetWindowAttrib(handle, GLFW_DECORATED, GLFW_FALSE);
        break;
    }
    window->mode = mode;
}


void zest__os_add_platform_extensions(void) {
    zest_uint count;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
    for (int i = 0; i != count; ++i) {
        zest_AddInstanceExtension((char*)glfw_extensions[i]);
    }
}

void zest__get_window_size_callback(void* user_data, int* fb_width, int* fb_height, int* window_width, int* window_height) {
    glfwGetFramebufferSize(ZestRenderer->current_window->window_handle, fb_width, fb_height);
    glfwGetWindowSize(ZestRenderer->current_window->window_handle, window_width, window_height);
}

void zest__destroy_window_callback(zest_window window, void* user_data) {
    glfwDestroyWindow((GLFWwindow*)window->window_handle);
}
#endif

void zest__log_vulkan_error(VkResult result, const char *file, int line) {
    // Only log actual errors, not warnings or success codes
    const char *error_str = zest__vulkan_error_string(result);
    if (result < 0) { // Vulkan errors are negative enum values
        ZEST_PRINT("Zest Vulkan Error: %s in %s at line %d", error_str, file, line);
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Zest Vulkan Error: %s in %s at line %d", error_str, file, line);
    } else {
        ZEST_PRINT("Zest Vulkan Warning: %s in %s at line %d", error_str, file, line);
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Zest Vulkan Warning: %s in %s at line %d", error_str, file, line);
    }
}

const char *zest__vulkan_error_string(VkResult errorCode) {
    switch (errorCode) {
#define ZEST_STR(r) case VK_ ##r: return #r
        ZEST_STR(SUCCESS);
        ZEST_STR(NOT_READY);
        ZEST_STR(TIMEOUT);
        ZEST_STR(EVENT_SET);
        ZEST_STR(EVENT_RESET);
        ZEST_STR(INCOMPLETE);
        ZEST_STR(ERROR_OUT_OF_HOST_MEMORY);
        ZEST_STR(ERROR_OUT_OF_DEVICE_MEMORY);
        ZEST_STR(ERROR_INITIALIZATION_FAILED);
        ZEST_STR(ERROR_DEVICE_LOST);
        ZEST_STR(ERROR_MEMORY_MAP_FAILED);
        ZEST_STR(ERROR_LAYER_NOT_PRESENT);
        ZEST_STR(ERROR_EXTENSION_NOT_PRESENT);
        ZEST_STR(ERROR_FEATURE_NOT_PRESENT);
        ZEST_STR(ERROR_INCOMPATIBLE_DRIVER);
        ZEST_STR(ERROR_TOO_MANY_OBJECTS);
        ZEST_STR(ERROR_FORMAT_NOT_SUPPORTED);
        ZEST_STR(ERROR_FRAGMENTED_POOL);
        ZEST_STR(ERROR_UNKNOWN);
        ZEST_STR(ERROR_OUT_OF_POOL_MEMORY);
        ZEST_STR(ERROR_INVALID_EXTERNAL_HANDLE);
        ZEST_STR(ERROR_FRAGMENTATION);
        ZEST_STR(ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
        ZEST_STR(PIPELINE_COMPILE_REQUIRED);
        ZEST_STR(ERROR_SURFACE_LOST_KHR);
        ZEST_STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
        ZEST_STR(SUBOPTIMAL_KHR);
        ZEST_STR(ERROR_OUT_OF_DATE_KHR);
        ZEST_STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
        ZEST_STR(ERROR_VALIDATION_FAILED_EXT);
        ZEST_STR(ERROR_INVALID_SHADER_NV);
        ZEST_STR(ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
        ZEST_STR(ERROR_NOT_PERMITTED_KHR);
        ZEST_STR(ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
        ZEST_STR(THREAD_IDLE_KHR);
        ZEST_STR(THREAD_DONE_KHR);
        ZEST_STR(OPERATION_DEFERRED_KHR);
        ZEST_STR(OPERATION_NOT_DEFERRED_KHR);
#undef ZEST_STR
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
zest_bool zest_SwapchainWasRecreated(zest_swapchain swapchain) {
    return ZEST__FLAGGED(swapchain->flags, zest_swapchain_flag_was_recreated);
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
    ZestPlatform = zloc_Allocate(allocator, sizeof(zest_platform_t));
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
    memset(ZestPlatform, 0, sizeof(zest_platform_t));
    ZestDevice->magic = zest_INIT_MAGIC(zest_struct_type_device);
    ZestDevice->allocator = allocator;
    ZestDevice->backend = zest__new_device_backend();
    ZestDevice->memory_pools[0] = memory_pool;
    ZestDevice->memory_pool_sizes[0] = info->memory_pool_size;
    ZestDevice->memory_pool_count = 1;
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
    ZestRenderer->set_window_mode_callback = info->set_window_mode_callback;
    ZestRenderer->set_window_size_callback = info->set_window_size_callback;
    ZestRenderer->backend = zest__new_renderer_backend();
    zest__initialise_platform(zest_platform_vulkan);
	zest__initialise_app(info);
	zest__initialise_window(info);
    if (zest__initialise_device()) {
        VkResult result = zest__initialise_renderer(info);
        if (result != VK_SUCCESS) {
			ZEST_PRINT("Unable to initialise the renderer. Check the log for details.");
			return ZEST_FALSE;
        }
    }
    return ZEST_TRUE;
}

void zest__initialise_platform(zest_platform_type type) {
    switch (type) {
    case zest_platform_vulkan: zest__initialise_platform_for_vulkan(); break;
    }
}

void zest__initialise_platform_for_vulkan() {
    ZestPlatform->begin_command_buffer                      = zest__vk_begin_command_buffer;
    ZestPlatform->end_command_buffer                        = zest__vk_end_command_buffer;
    ZestPlatform->set_next_command_buffer                   = zest__vk_set_next_command_buffer;
    ZestPlatform->set_execution_fence                       = zest__vk_set_execution_fence;
    ZestPlatform->acquire_barrier                           = zest__vk_acquire_barrier;
    ZestPlatform->release_barrier                           = zest__vk_release_barrier;
    ZestPlatform->get_frame_graph_semaphores                = zest__vk_get_frame_graph_semaphores;
    ZestPlatform->submit_frame_graph_batch                  = zest__vk_submit_frame_graph_batch;
    ZestPlatform->create_fg_render_pass                     = zest__vk_create_fg_render_pass;
    ZestPlatform->begin_render_pass                         = zest__vk_begin_render_pass;
    ZestPlatform->end_render_pass                           = zest__vk_end_render_pass;
    ZestPlatform->carry_over_semaphores                     = zest__vk_carry_over_semaphores;
    ZestPlatform->frame_graph_fence_wait                    = zest__vk_frame_graph_fence_wait;
    ZestPlatform->create_execution_timeline_backend         = zest__vk_create_execution_timeline_backend;
    ZestPlatform->new_execution_barriers_backend            = zest__vk_new_execution_barriers_backend;
    ZestPlatform->add_frame_graph_buffer_barrier            = zest__vk_add_memory_buffer_barrier;
    ZestPlatform->add_frame_graph_image_barrier             = zest__vk_add_image_barrier;
    ZestPlatform->validate_barrier_pipeline_stages          = zest__vk_validate_barrier_pipeline_stages;
    ZestPlatform->print_compiled_frame_graph                = zest__vk_print_compiled_frame_graph;
    ZestPlatform->deferr_framebuffer_destruction            = zest__vk_deferr_framebuffer_destruction;
    ZestPlatform->present_frame                             = zest__vk_present_frame;
    ZestPlatform->dummy_submit_for_present_only             = zest__vk_dummy_submit_for_present_only;
    ZestPlatform->acquire_swapchain_image                   = zest__vk_acquire_swapchain_image;

    ZestPlatform->create_set_layout                         = zest__vk_create_set_layout;
    ZestPlatform->create_set_pool                           = zest__vk_create_set_pool;
    ZestPlatform->create_bindless_set                       = zest__vk_create_bindless_set;

    ZestPlatform->new_execution_backend                     = zest__vk_new_execution_backend;
    ZestPlatform->new_frame_graph_semaphores_backend        = zest__vk_new_frame_graph_semaphores_backend;
    ZestPlatform->new_deferred_desctruction_backend         = zest__vk_new_deferred_destruction_backend;

    ZestPlatform->cleanup_frame_graph_semaphore             = zest__vk_cleanup_frame_graph_semaphore;
    ZestPlatform->cleanup_render_pass                       = zest__vk_cleanup_render_pass;
    ZestPlatform->cleanup_image_backend                     = zest__vk_cleanup_image_backend;
    ZestPlatform->cleanup_image_view_backend                = zest__vk_cleanup_image_view_backend;
    ZestPlatform->cleanup_deferred_framebuffers             = zest__vk_cleanup_deferred_framebuffers;
    ZestPlatform->cleanup_deferred_destruction_backend      = zest__vk_cleanup_deferred_destruction_backend;
}

void zest_Start() {
    zest__main_loop();
}

void zest_Shutdown(void) {
    zest__destroy();
}

void zest_SetCreateInfo(zest_create_info_t *info) {
    ZestApp->create_info = *info;
}

void zest_ResetRenderer() {

    zest_WaitForIdleDevice();

    zest_vulkan_command command_filter = zest_vk_pipelines;

    zest__cleanup_renderer();
    zest_create_info_t *info = &ZestApp->create_info;
    ZestRenderer->destroy_window_callback = info->destroy_window_callback;
    ZestRenderer->get_window_size_callback = info->get_window_size_callback;
    ZestRenderer->poll_events_callback = info->poll_events_callback;
    ZestRenderer->add_platform_extensions_callback = info->add_platform_extensions_callback;
    ZestRenderer->create_window_callback = info->create_window_callback;
    ZestRenderer->create_window_surface_callback = info->create_window_surface_callback;
    ZestRenderer->set_window_mode_callback = info->set_window_mode_callback;
    ZestRenderer->set_window_size_callback = info->set_window_size_callback;
    ZestRenderer->backend = ZEST__NEW(zest_renderer_backend);
    *ZestRenderer->backend = (zest_renderer_backend_t){ 0 };
	zest__initialise_window(&ZestApp->create_info);
    ZestRenderer->create_window_surface_callback(ZestRenderer->main_window);
    zest__initialise_renderer(&ZestApp->create_info);
}

void zest_SetUserData(void* data) {
    ZestApp->user_data = data;
}

void zest_SetUserUpdateCallback(void(*callback)(zest_microsecs, void*)) {
    ZestApp->update_callback = callback;
}

void zest_SetDestroyWindowCallback(void(*destroy_window_callback)(zest_window window, void* user_data)) {
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

void zest_SetPlatformWindowModeCallback(void(*set_window_mode_callback)(zest_window window, zest_window_mode mode)) {
    ZestRenderer->set_window_mode_callback = set_window_mode_callback;
}

void zest_SetPlatformWindowSizeCallback(void(*set_window_size_callback)(zest_window window, int width, int height)) {
    ZestRenderer->set_window_size_callback = set_window_size_callback;
}
//-- End Initialisation and destruction

/*
Functions that create a vulkan device
*/


void zest__destroy_debug_messenger(void) {
    ZEST_PRINT_FUNCTION;
    if (ZestDevice->backend->debug_messenger != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessenger =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ZestDevice->backend->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroyDebugUtilsMessenger) {
            destroyDebugUtilsMessenger(ZestDevice->backend->instance, ZestDevice->backend->debug_messenger, &ZestDevice->backend->allocation_callbacks);
        }
        ZestDevice->backend->debug_messenger = VK_NULL_HANDLE;
    }
}

void zest_AddInstanceExtension(char* extension) {
    zest_vec_push(ZestDevice->extensions, extension);
}

zest_buffer_type_t zest__get_buffer_memory_type(VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, zest_size size) {
    ZEST_PRINT_FUNCTION;
    VkBufferCreateInfo buffer_info = { 0 };
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage_flags;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_info.flags = 0;
    VkBuffer temp_buffer = VK_NULL_HANDLE;
	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_buffer);
    ZEST_VK_LOG(vkCreateBuffer(ZestDevice->backend->logical_device, &buffer_info, &ZestDevice->backend->allocation_callbacks, &temp_buffer));

    if (temp_buffer != VK_NULL_HANDLE) {
        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(ZestDevice->backend->logical_device, temp_buffer, &memory_requirements);

        zest_buffer_type_t buffer_type;
        buffer_type.alignment = memory_requirements.alignment;
        buffer_type.memory_type_index = zest_find_memory_type(memory_requirements.memoryTypeBits, property_flags);
        vkDestroyBuffer(ZestDevice->backend->logical_device, temp_buffer, &ZestDevice->backend->allocation_callbacks);
		return buffer_type;
    }

    return (zest_buffer_type_t) { 0 };
}

void zest__set_default_pool_sizes() {
    zest_buffer_usage_t usage = { 0 };
    //Images stored on device use share a single pool as far as I know
    //But not true was having issues with this. An image buffer can share the same usage properties but have different alignment requirements
    //So they will get separate pools
    //Depth buffers
    usage.image_flags = zest_image_usage_depth_stencil_attachment_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceImagePoolSize("Depth Buffer", usage.image_flags, usage.property_flags, 1024, zloc__MEGABYTE(64));

    //General Textures
    usage.image_flags = zest_image_usage_transfer_src_bit | zest_image_usage_transfer_dst_bit | zest_image_usage_sampled_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceImagePoolSize("Texture Buffer", usage.image_flags, usage.property_flags, 1024, zloc__MEGABYTE(64));

    //Storage Textures For Writing/Reading from
    usage.image_flags = zest_image_usage_transfer_src_bit | zest_image_usage_transfer_dst_bit | zest_image_usage_storage_bit | zest_image_usage_sampled_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceImagePoolSize("Texture Read/Write", usage.image_flags, usage.property_flags, 1024, zloc__MEGABYTE(64));

    //Render targets
    usage.image_flags = zest_image_usage_transfer_src_bit | zest_image_usage_transfer_dst_bit | zest_image_usage_sampled_bit | zest_image_usage_color_attachment_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceImagePoolSize("Render Target", usage.image_flags, usage.property_flags, 1024, zloc__MEGABYTE(64));

    //Uniform buffers
    usage.image_flags = 0;
    usage.usage_flags = zest_buffer_usage_uniform_buffer_bit;
    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit;
    zest_SetDeviceBufferPoolSize("Uniform Buffers", usage.usage_flags, usage.property_flags, 64, zloc__MEGABYTE(1));

    //Indirect draw buffers that are host visible
    usage.image_flags = 0;
    usage.usage_flags = zest_buffer_usage_indirect_buffer_bit | zest_buffer_usage_transfer_dst_bit;
    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit;
    zest_SetDeviceBufferPoolSize("Host Indirect Draw Buffers", usage.usage_flags, usage.property_flags, 64, zloc__MEGABYTE(1));

    //Indirect draw buffers
    usage.image_flags = 0;
    usage.usage_flags = zest_buffer_usage_indirect_buffer_bit | zest_buffer_usage_transfer_dst_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize("Host Indirect Draw Buffers", usage.usage_flags, usage.property_flags, 64, zloc__MEGABYTE(1));

    //Staging buffer
    usage.usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_transfer_src_bit;
    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit;
    zest_SetDeviceBufferPoolSize("Host Staging Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(2), zloc__MEGABYTE(32));

    //Vertex buffer
    usage.usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_vertex_buffer_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize("Vertex Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));

    //CPU Visible Vertex buffer
    usage.usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_vertex_buffer_bit;
    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize("CPU Visible Vertex Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(32));

    //Storage buffer
    usage.usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_storage_buffer_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize("Storage Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));

    //CPU Visible Storage buffer
    usage.usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_storage_buffer_bit;
    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize("CPU Visible Storage Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(32));

    //Index buffer
    usage.usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_index_buffer_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize("Index Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));

    //CPU Visible Index buffer
    usage.usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_index_buffer_bit;
    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize("Index Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(32));

    //Vertex buffer with storage flag
    usage.usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_vertex_buffer_bit | zest_buffer_usage_storage_buffer_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize("Vertex Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));

    //Index buffer with storage flag
    usage.usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_index_buffer_bit | zest_buffer_usage_storage_buffer_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize("Index Buffers", usage.usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Set device pool sizes");
}

void *zest_vk_allocate_callback(void *pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) {
    zest_vulkan_memory_info_t *pInfo = (zest_vulkan_memory_info_t *)pUserData;
    if (pInfo->timestamp == 116) {
        int d = 0;
    }
    void *pAllocation = ZEST__ALLOCATE(size + sizeof(zest_vulkan_memory_info_t));
    *(zest_vulkan_memory_info_t *)pAllocation = *pInfo;
    return (void *)((zest_vulkan_memory_info_t *)pAllocation + 1);
}

void *zest_vk_reallocate_callback(void *pUserData, void *pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) {
    zest_vulkan_memory_info_t *pHeader = ((zest_vulkan_memory_info_t *)pOriginal) - 1;
    void *pNewAllocation = ZEST__REALLOCATE(pHeader, size + sizeof(zest_vulkan_memory_info_t));
    zest_vulkan_memory_info_t *pInfo = (zest_vulkan_memory_info_t *)pUserData;
    *(zest_vulkan_memory_info_t *)pNewAllocation = *pInfo;
    return (void *)((zest_vulkan_memory_info_t *)pNewAllocation + 1);
}

void zest_vk_free_callback(void *pUserData, void *memory) {
    if (!memory) return;
    zest_vulkan_memory_info_t *original_allocation = (zest_vulkan_memory_info_t *)memory - 1;
    ZEST__FREE(original_allocation);
}

/*
End of Device creation functions
*/

void zest__initialise_app(zest_create_info_t* create_info) {
    ZestApp->magic = zest_INIT_MAGIC(zest_struct_type_app);
    ZestApp->create_info = *create_info;
    ZestApp->fence_wait_timeout_ns = create_info->fence_wait_timeout_ms * 1000 * 1000;
    ZestApp->update_callback = 0;
    ZestApp->current_elapsed = 0;
    ZestApp->update_time = 0;
    ZestApp->render_time = 0;
    ZestApp->frame_timer = 0;
}

void zest__initialise_window(zest_create_info_t *create_info) {
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create window with dimensions: %i, %i", create_info->screen_width, create_info->screen_height);
    ZestRenderer->main_window = ZestRenderer->create_window_callback(create_info->screen_x, create_info->screen_y, create_info->screen_width, create_info->screen_height, ZEST__FLAGGED(create_info->flags, zest_init_flag_maximised), "Zest");
}

void zest__end_thread(zest_work_queue_t *queue, void *data) {
    return;
}

void zest__destroy(void) {
    ZEST_PRINT_FUNCTION;
    zest_PrintReports();
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
    zloc_allocator *allocator = ZestDevice->allocator;
    zest__cleanup_renderer();
    zest__cleanup_device();
    zest__destroy_debug_messenger();
    zest_ForEachFrameInFlight(fif) {
        vkDestroyCommandPool(ZestDevice->backend->logical_device, ZestDevice->backend->one_time_command_pool[fif], &ZestDevice->backend->allocation_callbacks);
    }
    vkDestroyDevice(ZestDevice->backend->logical_device, &ZestDevice->backend->allocation_callbacks);
    vkDestroyInstance(ZestDevice->backend->instance, &ZestDevice->backend->allocation_callbacks);
    zest_ResetValidationErrors();
    ZEST__FREE(ZestDevice->backend);
    ZEST__FREE(ZestDevice);
    ZEST__FREE(ZestApp);
    ZEST__FREE(ZestRenderer);
    ZEST__FREE(ZestPlatform);
    ZEST__FREE(zest__globals);
	zloc_pool_stats_t stats = zloc_CreateMemorySnapshot(zloc__first_block_in_pool(zloc_GetPool(ZestDevice->allocator)));
    if (stats.used_blocks > 0) {
        ZEST_PRINT("There are still used memory blocks in Zest, this indicates a memory leak and a possible bug in the Zest Renderer. There should be no used blocks after Zest has shutdown. Check the type of allocation in the list below and check to make sure you're freeing those objects.");
        zest_PrintMemoryBlocks(zloc__first_block_in_pool(zloc_GetPool(ZestDevice->allocator)), 1, 0, 0);
    } else {
        ZEST_PRINT("Successful shutdown of Zest.");
    }
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

zest_bool zest__validation_layers_with_best_practices_are_enabled(void) {
    return ZEST__FLAGGED(ZestDevice->setup_info.flags, zest_init_flag_enable_validation_layers_with_best_practices);
}

void zest__do_scheduled_tasks(void) {
    ZEST_PRINT_FUNCTION;
    zloc_ResetLinearAllocator(ZestRenderer->frame_graph_allocator[ZEST_FIF]);
    ZestRenderer->frame_graphs = 0;

    zest_vec_foreach(i, ZestDevice->queues) {
        zest_queue queue = ZestDevice->queues[i];
        zest__reset_queue_command_pool(queue);
    }

    if (zest_vec_size(ZestRenderer->deferred_resource_freeing_list.resources[ZEST_FIF])) {
        zest_vec_foreach(i, ZestRenderer->deferred_resource_freeing_list.resources[ZEST_FIF]) {
            void *handle = ZestRenderer->deferred_resource_freeing_list.resources[ZEST_FIF][i];
            zest__free_handle(handle);
        }
		zest_vec_clear(ZestRenderer->deferred_resource_freeing_list.resources[ZEST_FIF]);
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
            zest_image image = &ZestRenderer->deferred_resource_freeing_list.images[ZEST_FIF][i];
            ZestPlatform->cleanup_image_backend(image);
            zest_FreeBuffer(image->buffer);
            if (image->default_view) {
				ZestPlatform->cleanup_image_view_backend(image->default_view);
            }
        }
		zest_vec_clear(ZestRenderer->deferred_resource_freeing_list.images[ZEST_FIF]);
    }

    if (zest_vec_size(ZestRenderer->deferred_resource_freeing_list.views[ZEST_FIF])) {
        zest_vec_foreach(i, ZestRenderer->deferred_resource_freeing_list.views[ZEST_FIF]) {
            zest_image_view view = &ZestRenderer->deferred_resource_freeing_list.views[ZEST_FIF][i];
            ZestPlatform->cleanup_image_view_backend(view);
        }
		zest_vec_clear(ZestRenderer->deferred_resource_freeing_list.views[ZEST_FIF]);
    }

    ZestPlatform->cleanup_deferred_framebuffers();

    zest_vec_foreach(i, ZestRenderer->staging_buffers) {
        zest_buffer staging_buffer = ZestRenderer->staging_buffers[i];
		zest_FreeBuffer(staging_buffer);
    }
    zest_vec_clear(ZestRenderer->staging_buffers);
}

void zest_Terminate(void) {
	ZEST__FLAG(ZestApp->flags, zest_app_flag_quit_application);
}

VkResult zest__main_loop_fence_wait() {
	if (ZestRenderer->fence_count[ZEST_FIF]) {
		zest_millisecs start_time = zest_Millisecs();
		zest_uint retry_count = 0;
		while(1) {
			VkResult result = vkWaitForFences(ZestDevice->backend->logical_device, ZestRenderer->fence_count[ZEST_FIF], ZestRenderer->backend->fif_fence[ZEST_FIF], VK_TRUE, ZestApp->fence_wait_timeout_ns);
            if (result == VK_SUCCESS) {
                break;
            } else if (result == VK_TIMEOUT) {
				zest_millisecs total_wait_time = zest_Millisecs() - start_time;
				if (ZestApp->fence_wait_timeout_callback) {
                    if (ZestApp->fence_wait_timeout_callback(total_wait_time, retry_count++, ZestApp->user_data)) {
                        continue;
                    } else {
                        return result;
                    }
				} else {
					if (total_wait_time > ZestApp->create_info.max_fence_timeout_ms) {
                        return result;
					}
				}
			} else {
                return result;
			}
		}
		vkResetFences(ZestDevice->backend->logical_device, ZestRenderer->fence_count[ZEST_FIF], ZestRenderer->backend->fif_fence[ZEST_FIF]);
		ZestRenderer->fence_count[ZEST_FIF] = 0;
	}
	return VK_SUCCESS;
}

void zest__main_loop(void) {
    ZestApp->current_elapsed_time = zest_Microsecs();
    ZEST_ASSERT(ZestApp->update_callback);    //Must define an update callback function
    while (!(ZestApp->flags & zest_app_flag_quit_application)) {
        VkResult fence_wait_result = zest__main_loop_fence_wait();
        if (fence_wait_result == VK_SUCCESS) {
        } else if (fence_wait_result == VK_TIMEOUT) {
            ZEST_PRINT("Fence wait timed out in the main loop, exiting renderer loop.");
            zest_Terminate();
            continue;
        } else {
            ZEST_VK_PRINT_RESULT(fence_wait_result);
            ZEST_PRINT("Critical error when waiting for the main loop fence, exiting renderer loop.");
            zest_Terminate();
            continue;
        }

		ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_acquired);

        zest__do_scheduled_tasks();

        ZestApp->mouse_hit = 0;
        zest_mouse_button button_state = ZestApp->mouse_button;

        ZestRenderer->poll_events_callback();

        ZestApp->mouse_hit = button_state & (~ZestApp->mouse_button);

        zest__set_elapsed_time();

        ZestApp->update_callback(ZestApp->current_elapsed, ZestApp->user_data);

        if (ZestApp->flags & zest_app_flag_quit_application) {
            continue;
        }

        //Cover some cases where a frame graph wasn't created or it was but there was nothing render etc., to make sure
        //that the fence is always signalled and another frame can happen
        if (ZEST__FLAGGED(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_acquired)) {
            if (ZEST__NOT_FLAGGED(ZestRenderer->flags, zest_renderer_flag_work_was_submitted)) {
                ZestPlatform->dummy_submit_for_present_only();
				ZestPlatform->present_frame(ZestRenderer->main_swapchain);
            }
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
unsigned WINAPI zest__thread_worker(void *arg) {
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

// --Uniform buffers
void zest__add_set_builder_uniform_buffer( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_uniform_buffer uniform_buffer, zest_uint fif) {
    VkDescriptorBufferInfo buffer_info = uniform_buffer->backend->descriptor_info[fif];
    zest_AddSetBuilderWrite(builder, dst_binding, dst_array_element, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &buffer_info, NULL);
}
// --End Uniform buffers

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
	ptrdiff_t offset_from_allocator = (ptrdiff_t)allocation - (ptrdiff_t)ZestDevice->allocator;
    if (offset_from_allocator == 31920128) {
        int d = 0;
    }
    // If there's something that isn't being freed on zest shutdown and it's of an unknown type then 
    // it should print out the offset from the allocator, you can use that offset to break here and 
    // find out what's being allocated.
    if (!allocation) {
        zest_size pool_size = (zest_size)ZEST__NEXTPOW2((double)size * 2);
        ZEST_PRINT("Added a new pool size of %zu", pool_size);
        zest__add_host_memory_pool(pool_size);
        allocation = zloc_Allocate(ZestDevice->allocator, size);
        ZEST_ASSERT(allocation);    //Out of memory? Unable to allocate even after adding a pool
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

void* zest__allocate_aligned(zest_size size, zest_size alignment) {
    void* allocation = zloc_AllocateAligned(ZestDevice->allocator, size, alignment);
    if (!allocation) {
        zest__add_host_memory_pool(size);
        allocation = zloc_AllocateAligned(ZestDevice->allocator, size, alignment);
        ZEST_ASSERT(allocation);    //Unable to allocate even after adding a pool
    }
    return allocation;
}

VkResult zest__bind_buffer_memory(zest_buffer buffer, VkDeviceSize offset) {
    ZEST_PRINT_FUNCTION;
    return vkBindBufferMemory(ZestDevice->backend->logical_device, buffer->backend->vk_buffer, buffer->memory_pool->backend->memory, offset);
}

VkResult zest__map_memory(zest_device_memory_pool memory_allocation, VkDeviceSize size, VkDeviceSize offset) {
    ZEST_PRINT_FUNCTION;
    return vkMapMemory(ZestDevice->backend->logical_device, memory_allocation->backend->memory, offset, size, 0, &memory_allocation->mapped);
}

void zest__unmap_memory(zest_device_memory_pool memory_allocation) {
    ZEST_PRINT_FUNCTION;
    if (memory_allocation->mapped) {
        vkUnmapMemory(ZestDevice->backend->logical_device, memory_allocation->backend->memory);
        memory_allocation->mapped = ZEST_NULL;
    }
}

void zest__destroy_memory(zest_device_memory_pool memory_allocation) {
    ZEST_PRINT_FUNCTION;
    if (memory_allocation->backend->vk_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(ZestDevice->backend->logical_device, memory_allocation->backend->vk_buffer, &ZestDevice->backend->allocation_callbacks);
    }
    if (memory_allocation->backend->memory) {
        vkFreeMemory(ZestDevice->backend->logical_device, memory_allocation->backend->memory, &ZestDevice->backend->allocation_callbacks);
    }
    ZEST__FREE(memory_allocation->backend);
    memory_allocation->mapped = ZEST_NULL;
}

VkResult zest__flush_memory(zest_device_memory_pool memory_allocation, VkDeviceSize size, VkDeviceSize offset) {
    ZEST_PRINT_FUNCTION;
    VkMappedMemoryRange mappedRange = { 0 };
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = memory_allocation->backend->memory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkFlushMappedMemoryRanges(ZestDevice->backend->logical_device, 1, &mappedRange);
}

VkResult zest__create_device_memory_pool(VkDeviceSize size, zest_buffer_info_t *buffer_info, zest_device_memory_pool memory_pool, const char* name) {
    ZEST_PRINT_FUNCTION;
    VkBufferCreateInfo create_buffer_info = { 0 };
    create_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_buffer_info.size = size;
    create_buffer_info.usage = buffer_info->usage_flags;
    create_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_buffer_info.flags = 0;

    VkBuffer temp_buffer;
	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_buffer);
    ZEST_VK_ASSERT_RESULT(vkCreateBuffer(ZestDevice->backend->logical_device, &create_buffer_info, &ZestDevice->backend->allocation_callbacks, &temp_buffer));

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(ZestDevice->backend->logical_device, temp_buffer, &memory_requirements);

    VkMemoryAllocateFlagsInfo flags;
    flags.deviceMask = 0;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    flags.pNext = NULL;
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;

    VkMemoryAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = zest_find_memory_type(memory_requirements.memoryTypeBits, buffer_info->property_flags);
    ZEST_ASSERT(alloc_info.memoryTypeIndex != ZEST_INVALID);
    if (zest__validation_layers_are_enabled() && ZestDevice->api_version == VK_API_VERSION_1_2) {
        alloc_info.pNext = &flags;
    }
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Allocating buffer memory pool, size: %llu type: %i, alignment: %llu, type bits: %i", alloc_info.allocationSize, alloc_info.memoryTypeIndex, memory_requirements.alignment, memory_requirements.memoryTypeBits);
	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_allocate_memory_pool);
    ZEST_VK_ASSERT_RESULT(vkAllocateMemory(ZestDevice->backend->logical_device, &alloc_info, &ZestDevice->backend->allocation_callbacks, &memory_pool->backend->memory));

    memory_pool->size = memory_requirements.size;
    memory_pool->alignment = memory_requirements.alignment;
    memory_pool->minimum_allocation_size = ZEST__MAX(memory_pool->alignment, memory_pool->minimum_allocation_size);
    memory_pool->memory_type_index = alloc_info.memoryTypeIndex;
    memory_pool->backend->property_flags = buffer_info->property_flags;
    memory_pool->backend->usage_flags = buffer_info->usage_flags;
    memory_pool->backend->buffer_info = create_buffer_info;

    if (ZEST__FLAGGED(create_buffer_info.flags, zest_memory_pool_flag_single_buffer)) {
        vkDestroyBuffer(ZestDevice->backend->logical_device, temp_buffer, &ZestDevice->backend->allocation_callbacks);
    } else {
        memory_pool->backend->vk_buffer = temp_buffer;
        vkBindBufferMemory(ZestDevice->backend->logical_device, memory_pool->backend->vk_buffer, memory_pool->backend->memory, 0);
    }
    return VK_SUCCESS;
}

VkResult zest__create_image_memory_pool(VkDeviceSize size_in_bytes, VkImage image, zest_buffer_info_t *buffer_info, zest_device_memory_pool buffer) {
    ZEST_PRINT_FUNCTION;
    VkMemoryAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = size_in_bytes;
    alloc_info.memoryTypeIndex = zest_find_memory_type(buffer_info->memory_type_bits, buffer_info->property_flags);
    ZEST_ASSERT(alloc_info.memoryTypeIndex != ZEST_INVALID);

    buffer->size = size_in_bytes;
    buffer->alignment = buffer_info->alignment;
    buffer->minimum_allocation_size = ZEST__MAX(buffer->alignment, buffer->minimum_allocation_size);
    buffer->memory_type_index = alloc_info.memoryTypeIndex;
    buffer->backend->property_flags = buffer_info->property_flags;
    buffer->backend->usage_flags = 0;

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Allocating image memory pool, size: %llu type: %i, alignment: %llu, type bits: %i", alloc_info.allocationSize, alloc_info.memoryTypeIndex, buffer_info->alignment, buffer_info->memory_type_bits);
	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_allocate_memory_pool);
    ZEST_VK_ASSERT_RESULT(vkAllocateMemory(ZestDevice->backend->logical_device, &alloc_info, &ZestDevice->backend->allocation_callbacks, &buffer->backend->memory));

    return VK_SUCCESS;
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
    zest_buffer buffer = zloc_BlockUserExtensionPtr(block);
    zest_buffer trimmed_buffer = zloc_BlockUserExtensionPtr(trimmed_block);
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
    zest_buffer buffer = zloc_BlockUserExtensionPtr(block);
    zest_buffer new_buffer = zloc_BlockUserExtensionPtr(new_block);
    if (pools->buffer_info.property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        new_buffer->data = (void*)((char*)new_buffer->memory_pool->mapped + new_buffer->memory_offset);
        new_buffer->end = (void*)((char*)new_buffer->data + new_buffer->size);
        memcpy(new_buffer->data, buffer->data, buffer->size);
    }
}

void zest__remote_merge_next_callback(void *user_data, zloc_header *block, zloc_header *next_block) {
    zest_buffer remote_block = (zest_buffer)zloc_BlockUserExtensionPtr(block);
    zest_buffer next_remote_block = (zest_buffer)zloc_BlockUserExtensionPtr(next_block);
    remote_block->size += next_remote_block->size;
    next_remote_block->magic = 0;
    next_remote_block->memory_offset = 0;
    next_remote_block->size = 0;
    if ((remote_block->magic & 0xFFFF) == ZEST_STRUCT_IDENTIFIER && ZEST__NOT_FLAGGED(remote_block->memory_pool->flags, zest_memory_pool_flag_single_buffer) && remote_block->backend->vk_buffer) {
        zest_vec_push(ZestRenderer->backend->used_buffers_ready_for_freeing, remote_block->backend->vk_buffer);
    }
    if ((next_remote_block->magic & 0xFFFF) == ZEST_STRUCT_IDENTIFIER && ZEST__NOT_FLAGGED(next_remote_block->memory_pool->flags, zest_memory_pool_flag_single_buffer) && next_remote_block->backend->vk_buffer) {
        zest_vec_push(ZestRenderer->backend->used_buffers_ready_for_freeing, next_remote_block->backend->vk_buffer);
    }
    remote_block->magic = 0;
    if (remote_block->backend) {
        remote_block->backend->vk_buffer = VK_NULL_HANDLE;
    }
    if (next_remote_block->backend = 0) {
        next_remote_block->backend->vk_buffer = VK_NULL_HANDLE;
    }
}

void zest__remote_merge_prev_callback(void *user_data, zloc_header *prev_block, zloc_header *block) {
    zest_buffer remote_block = (zest_buffer)zloc_BlockUserExtensionPtr(block);
    zest_buffer prev_remote_block = (zest_buffer)zloc_BlockUserExtensionPtr(prev_block);
    prev_remote_block->size += remote_block->size;
    prev_remote_block->magic = 0;
    remote_block->memory_offset = 0;
    remote_block->size = 0;
    if ((remote_block->magic & 0xFFFF) == ZEST_STRUCT_IDENTIFIER && ZEST__NOT_FLAGGED(remote_block->memory_pool->flags, zest_memory_pool_flag_single_buffer) && remote_block->backend->vk_buffer) {
        zest_vec_push(ZestRenderer->backend->used_buffers_ready_for_freeing, remote_block->backend->vk_buffer);
    }
    if ((prev_remote_block->magic & 0xFFFF) == ZEST_STRUCT_IDENTIFIER && ZEST__NOT_FLAGGED(prev_remote_block->memory_pool->flags, zest_memory_pool_flag_single_buffer) && prev_remote_block->backend->vk_buffer) {
        zest_vec_push(ZestRenderer->backend->used_buffers_ready_for_freeing, prev_remote_block->backend->vk_buffer);
    }
    remote_block->magic = 0;
    if (remote_block->backend) {
        remote_block->backend->vk_buffer = VK_NULL_HANDLE;
    }
    if (prev_remote_block->backend) {
        prev_remote_block->backend->vk_buffer = VK_NULL_HANDLE;
    }
}

zloc_size zest__get_minimum_block_size(zest_size pool_size) {
    return pool_size > zloc__MEGABYTE(1) ? pool_size / 128 : 256;
}

VkResult zest__create_vk_memory_pool(zest_buffer_info_t *buffer_info, VkImage image, zest_key key, zest_size minimum_size, zest_device_memory_pool *memory_pool) {
    zest_device_memory_pool buffer_pool = ZEST__NEW(zest_device_memory_pool);
    *buffer_pool = (zest_device_memory_pool_t){ 0 };
    buffer_pool->magic = zest_INIT_MAGIC(zest_struct_type_device_memory_pool);
    buffer_pool->backend = ZEST__NEW(zest_device_memory_pool_backend);
    *buffer_pool->backend = (zest_device_memory_pool_backend_t){ 0 };
    buffer_pool->flags = buffer_info->flags;
	zest_buffer_pool_size_t pre_defined_pool_size = zest_GetDeviceBufferPoolSize(buffer_info->usage_flags, buffer_info->property_flags, buffer_info->image_usage_flags);
    VkResult result = VK_SUCCESS;
    if (pre_defined_pool_size.pool_size > 0) {
        buffer_pool->name = pre_defined_pool_size.name;
        buffer_pool->size = pre_defined_pool_size.pool_size > minimum_size ? pre_defined_pool_size.pool_size : zest_GetNextPower(minimum_size + minimum_size / 2);
        buffer_pool->minimum_allocation_size = pre_defined_pool_size.minimum_allocation_size;
    } else {
        if (image) {
            ZEST_PRINT_WARNING(ZEST_WARNING_COLOR"Allocating image memory where no default pool size was found for image usage flags: %i, and property flags: %i. Defaulting to next power from size + size / 2",
                buffer_info->image_usage_flags, buffer_info->property_flags);
        } else {
            ZEST_PRINT_WARNING(ZEST_WARNING_COLOR"Allocating memory where no default pool size was found for usage flags: %i and property flags: %i. Defaulting to next power from size + size / 2",
                buffer_info->usage_flags, buffer_info->property_flags);
        }
        //Todo: we need a better solution then this
        buffer_pool->size = ZEST__MAX(zest_GetNextPower(minimum_size + minimum_size / 2), zloc__MEGABYTE(16));
        buffer_pool->name = "Unknown";
        buffer_pool->minimum_allocation_size = zest__get_minimum_block_size(buffer_pool->size);
    }
    if (image == VK_NULL_HANDLE) {
        result = zest__create_device_memory_pool(buffer_pool->size, buffer_info, buffer_pool, "");
        if (result != VK_SUCCESS) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Unable to allocate memory for buffer memory pool. Tried to allocate %zu.", buffer_pool->size);
            goto cleanup;
            return result;
        }
    }
    else {
        result = zest__create_image_memory_pool(buffer_pool->size, image, buffer_info, buffer_pool);
        if (result != VK_SUCCESS) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Unable to allocate memory for image memory pool. Tried to allocate %zu.", buffer_pool->size);
            goto cleanup;
            return result;
        }
    }
    if (buffer_info->property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        zest__map_memory(buffer_pool, VK_WHOLE_SIZE, 0);
    }
    *memory_pool = buffer_pool;
    return VK_SUCCESS;
    cleanup:
    zest__destroy_memory(buffer_pool);
    ZEST__FREE(buffer_pool);
    return result;
}

void zest__add_remote_range_pool(zest_buffer_allocator buffer_allocator, zest_device_memory_pool buffer_pool) {
    zest_vec_push(buffer_allocator->memory_pools, buffer_pool);
    zloc_size range_pool_size = zloc_CalculateRemoteBlockPoolSize(buffer_allocator->allocator, buffer_pool->size);
    zest_pool_range* range_pool = ZEST__ALLOCATE(range_pool_size);
    zest_vec_push(buffer_allocator->range_pools, range_pool);
    zloc_AddRemotePool(buffer_allocator->allocator, range_pool, range_pool_size, buffer_pool->size);
    ZEST_PRINT_NOTICE(ZEST_NOTICE_COLOR"Note: Ran out of space in the Device memory pool (%s) so adding a new one of size %zu. ", buffer_pool->name, (size_t)buffer_pool->size);
}

void zest__set_buffer_details(zest_buffer_allocator buffer_allocator, zest_buffer buffer, zest_bool is_host_visible) {
    ZEST_PRINT_FUNCTION;
    buffer->backend = zest__new_buffer_backend();
    if (ZEST__NOT_FLAGGED(buffer->memory_pool->flags, zest_memory_pool_flag_single_buffer) && buffer->memory_pool->backend->buffer_info.size > 0) {
        VkBufferCreateInfo create_buffer_info = buffer->memory_pool->backend->buffer_info;
        create_buffer_info.size = buffer->size;
		ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_buffer);
		vkCreateBuffer(ZestDevice->backend->logical_device, &create_buffer_info, &ZestDevice->backend->allocation_callbacks, &buffer->backend->vk_buffer);
		vkBindBufferMemory(ZestDevice->backend->logical_device, buffer->backend->vk_buffer, buffer->memory_pool->backend->memory, buffer->memory_offset);
        //As this buffer has it's own VkBuffer we set the buffer_offset to 0. Otherwise the offset needs to be the sub allocation
        //within the pool.
        buffer->buffer_offset = 0;
    } else {
		buffer->backend->vk_buffer = buffer->memory_pool->backend->vk_buffer;
        buffer->buffer_offset = buffer->memory_offset;
    }
    buffer->magic = zest_INIT_MAGIC(zest_struct_type_buffer);
    buffer->buffer_allocator = buffer_allocator;
    buffer->memory_in_use = 0;
    buffer->array_index = ZEST_INVALID;
    buffer->owner_queue_family = ZEST_QUEUE_FAMILY_IGNORED;
    buffer->backend->last_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    buffer->backend->last_access_mask = VK_ACCESS_NONE;
    if (is_host_visible) {
        buffer->data = (void*)((char*)buffer->memory_pool->mapped + buffer->memory_offset);
        buffer->end = (void*)((char*)buffer->data + buffer->size);
    }
    else {
        buffer->data = ZEST_NULL;
        buffer->end = ZEST_NULL;
    }
}

void zest_FlushUsedBuffers() {
    ZEST_PRINT_FUNCTION;
    zest_vec_foreach(i, ZestRenderer->backend->used_buffers_ready_for_freeing) {
        VkBuffer buffer = ZestRenderer->backend->used_buffers_ready_for_freeing[i];
        vkDestroyBuffer(ZestDevice->backend->logical_device, buffer, &ZestDevice->backend->allocation_callbacks);
    }
}

void zest__cleanup_buffers_in_allocators() {
    ZEST_PRINT_FUNCTION;
    zest_map_foreach(i, ZestRenderer->buffer_allocators) {
        zest_buffer_allocator allocator = ZestRenderer->buffer_allocators.data[i];
        zest_vec_foreach(j, allocator->range_pools) {
            zest_pool_range pool = allocator->range_pools[j];
            zloc_header *current_block = zloc__first_block_in_pool(pool);
            while (!zloc__is_last_block_in_pool(current_block)) {
                zest_buffer remote_block = (zest_buffer)zloc_BlockUserExtensionPtr(current_block);
                if (ZEST_IS_INTITIALISED(remote_block->magic)) {
                    if (remote_block->backend->vk_buffer != VK_NULL_HANDLE && ZEST__NOT_FLAGGED(remote_block->memory_pool->flags, zest_memory_pool_flag_single_buffer)) {
                        vkDestroyBuffer(ZestDevice->backend->logical_device, remote_block->backend->vk_buffer, &ZestDevice->backend->allocation_callbacks);
                    }
                    zest__cleanup_buffer_backend(remote_block);
                }
                zloc_header *last_block = current_block;
                current_block = zloc__next_physical_block(current_block);
            }
        }
    }
}

void zloc__output_buffer_info(void* ptr, size_t size, int free, void* user, int count)
{
    zest_buffer_t* buffer = (zest_buffer_t*)user;
    printf("%i) \t%s range size: \t%zi \tbuffer size: %llu \toffset: %llu \n", count, free ? "free" : "used", size, buffer->size, buffer->buffer_offset);
}

zloc__error_codes zloc_VerifyAllRemoteBlocks(zloc__block_output output_function, void* user_data) {
    zest_map_foreach(i, ZestRenderer->buffer_allocators) {
        zest_buffer_allocator allocator = ZestRenderer->buffer_allocators.data[i];
        zest_vec_foreach(j, allocator->range_pools) {
            zest_pool_range pool = allocator->range_pools[j];
            zloc_header *current_block = zloc__first_block_in_pool(pool);
            int count = 0;
            while (!zloc__is_last_block_in_pool(current_block)) {
                void *remote_block = zloc_BlockUserExtensionPtr(current_block);
                if (output_function) {
                    output_function(current_block, zloc__block_size(current_block), zloc__is_free_block(current_block), remote_block, ++count);
                }
                zloc_header *last_block = current_block;
                current_block = zloc__next_physical_block(current_block);
                if (last_block != current_block->prev_physical_block) {
                    ZEST_ASSERT(0);
                    return zloc__PHYSICAL_BLOCK_MISALIGNMENT;
                }
            }
        }
    }
    return zloc__OK;
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
    zest_key key = zest_map_hash_ptr(ZestRenderer->buffer_allocators, buffer_info, sizeof(zest_buffer_info_t));
    if (!zest_map_valid_key(ZestRenderer->buffer_allocators, key)) {
        //If an allocator doesn't exist yet for this combination of usage and buffer properties then create one.

        zest_buffer_allocator buffer_allocator = ZEST__NEW(zest_buffer_allocator);
        *buffer_allocator = (zest_buffer_allocator_t){ 0 };
        buffer_allocator->buffer_info = *buffer_info;
        buffer_allocator->magic = zest_INIT_MAGIC(zest_struct_type_buffer_allocator);
        zest_device_memory_pool buffer_pool = 0;
        if (zest__create_vk_memory_pool(buffer_info, image, key, size, &buffer_pool) != VK_SUCCESS) {
			return 0;
        }

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
        buffer_allocator->allocator->merge_next_callback = zest__remote_merge_next_callback;
        buffer_allocator->allocator->merge_prev_callback = zest__remote_merge_prev_callback;
        zloc_AddRemotePool(buffer_allocator->allocator, range_pool, range_pool_size, buffer_pool->size);
    }

    zest_buffer_allocator buffer_allocator = *zest_map_at_key(ZestRenderer->buffer_allocators, key);
    zloc_size adjusted_size = zloc__align_size_up(size, buffer_allocator->alignment);
    zest_buffer buffer = zloc_AllocateRemote(buffer_allocator->allocator, adjusted_size);
    if (buffer) {
        zest__set_buffer_details(buffer_allocator, buffer, buffer_info->property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }
    else {
        zest_device_memory_pool buffer_pool = 0;
        if(zest__create_vk_memory_pool(buffer_info, image, key, size, &buffer_pool) != VK_SUCCESS) {
            return 0;
        }
        ZEST_ASSERT(buffer_pool->alignment == buffer_allocator->alignment);    //The new pool should have the same alignment as the alignment set in the allocator, this
        //would have been set when the first pool was created

        zest__add_remote_range_pool(buffer_allocator, buffer_pool);
        buffer = zloc_AllocateRemote(buffer_allocator->allocator, adjusted_size);
        if (!buffer) {    //Unable to allocate memory. Out of memory?
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Unable to allocate %zu of memory. Exiting app.", size);
            zest_Terminate();
            return 0;
        }
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
    zest_buffer_info_t buffer_info = zest_CreateStorageBufferInfo(0);
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

zest_buffer zest_CreateUniqueIndexBuffer(VkDeviceSize size, zest_uint fif, zest_uint unique_id) {
    zest_buffer_info_t buffer_info = zest_CreateIndexBufferInfo(0);
    buffer_info.frame_in_flight = fif;
    buffer_info.unique_id = unique_id;
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    return buffer;
}

zest_buffer zest_CreateUniqueVertexBuffer(VkDeviceSize size, zest_uint fif, zest_uint unique_id) {
    zest_buffer_info_t buffer_info = zest_CreateVertexBufferInfo(0);
    buffer_info.frame_in_flight = fif;
    buffer_info.unique_id = unique_id;
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    return buffer;
}

zest_buffer zest_CreateUniqueVertexStorageBuffer(VkDeviceSize size, zest_uint fif, zest_uint unique_id) {
    zest_buffer_info_t buffer_info = zest_CreateComputeVertexBufferInfo();
    buffer_info.frame_in_flight = fif;
    buffer_info.unique_id = unique_id;
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    return buffer;
}

zest_buffer zest_CreateUniqueStorageBuffer(VkDeviceSize size, zest_uint fif, zest_uint unique_id) {
    zest_buffer_info_t buffer_info = zest_CreateStorageBufferInfo(0);
    buffer_info.frame_in_flight = fif;
    buffer_info.unique_id = unique_id;
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    return buffer;
}

zest_buffer zest_CreateUniqueCPUStorageBuffer(VkDeviceSize size, zest_uint fif, zest_uint unique_id) {
    zest_buffer_info_t buffer_info = zest_CreateCPUVisibleStorageBufferInfo();
    buffer_info.frame_in_flight = fif;
    buffer_info.unique_id = unique_id;
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    return buffer;
}

zest_buffer zest_CreateUniqueComputeVertexBuffer(VkDeviceSize size, zest_uint fif, zest_uint unique_id) {
    zest_buffer_info_t buffer_info = zest_CreateComputeVertexBufferInfo();
    buffer_info.frame_in_flight = fif;
    buffer_info.unique_id = unique_id;
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    return buffer;
}

zest_buffer zest_CreateUniqueComputeIndexBuffer(VkDeviceSize size, zest_uint fif, zest_uint unique_id) {
    zest_buffer_info_t buffer_info = zest_CreateComputeVertexBufferInfo();
    buffer_info.frame_in_flight = fif;
    buffer_info.unique_id = unique_id;
    zest_buffer buffer = zest_CreateBuffer(size, &buffer_info, ZEST_NULL);
    return buffer;
}


zest_bool zest_cmd_CopyBufferOneTime(zest_buffer src_buffer, zest_buffer dst_buffer, VkDeviceSize size) {
    ZEST_PRINT_FUNCTION;
    ZEST_ASSERT(size <= src_buffer->size);        //size must be less than or equal to the staging buffer size and the device buffer size
    ZEST_ASSERT(size <= dst_buffer->size);
    VkBufferCopy copyInfo = { 0 };
    copyInfo.srcOffset = src_buffer->buffer_offset;
    copyInfo.dstOffset = dst_buffer->buffer_offset;
    copyInfo.size = size;
    vkCmdCopyBuffer(ZestRenderer->backend->one_time_command_buffer, src_buffer->backend->vk_buffer, dst_buffer->backend->vk_buffer, 1, &copyInfo);
    return ZEST_TRUE;
}

void zest_StageData(void *src_data, zest_buffer dst_staging_buffer, zest_size size) {
    ZEST_ASSERT(dst_staging_buffer->data);  //No mapped data in staging buffer, is it an actual staging buffer?
    ZEST_ASSERT(src_data);                  //No source data to copy!
    ZEST_ASSERT(size <= dst_staging_buffer->size);  //Staging buffer not large enough
    memcpy(dst_staging_buffer->data, src_data, size);
    dst_staging_buffer->memory_in_use = size;
}

VkResult zest_FlushBuffer(zest_buffer buffer) {
    return zest__flush_memory(buffer->memory_pool, buffer->size, buffer->buffer_offset);
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
        zest_device_memory_pool buffer_pool = 0;
        if (zest__create_vk_memory_pool(&buffer_allocator->buffer_info, ZEST_NULL, 0, new_size, &buffer_pool) != VK_SUCCESS) {
            return ZEST_FALSE;
        } else {
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
        zest_device_memory_pool buffer_pool = 0;
        if (zest__create_vk_memory_pool(&buffer_allocator->buffer_info, ZEST_NULL, 0, new_size, &buffer_pool) != VK_SUCCESS) {
            return ZEST_FALSE;
        } else {
            ZEST_ASSERT(buffer_pool->alignment == buffer_allocator->alignment);    //The new pool should have the same alignment as the alignment set in the allocator, this
            //would have been set when the first pool was created
            zest__add_remote_range_pool(buffer_allocator, buffer_pool);
            new_buffer = zloc_ReallocateRemote(buffer_allocator->allocator, *buffer, new_size);
            ZEST_ASSERT(new_buffer);    //Unable to allocate memory. Out of memory?
            *buffer = new_buffer;
            zest__set_buffer_details(buffer_allocator, *buffer, buffer_allocator->buffer_info.property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            new_buffer->memory_in_use = memory_in_use;
        }
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

zest_buffer_info_t zest_CreateStorageBufferInfo(zest_bool cpu_visible) {
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (cpu_visible) {
        buffer_info.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
    else {
        buffer_info.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
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
    buffer_info.flags = zest_memory_pool_flag_single_buffer;
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
    zest__cleanup_buffer_backend(buffer);
}

VkDeviceMemory zest_GetBufferDeviceMemory(zest_buffer buffer) {
    return buffer->memory_pool->backend->memory;
}

VkBuffer* zest_GetBufferDeviceBuffer(zest_buffer buffer) {
    return &buffer->backend->vk_buffer;
}

VkDescriptorBufferInfo zest_vk_GetBufferInfo(zest_buffer buffer) {
    VkDescriptorBufferInfo buffer_info = { 0 };
    buffer_info.buffer = buffer->backend->vk_buffer;
    buffer_info.offset = buffer->buffer_offset;
    buffer_info.range = buffer->size;
    return buffer_info;
}

void zest_AddCopyCommand(zest_buffer_uploader_t* uploader, zest_buffer_t* source_buffer, zest_buffer_t* target_buffer, zest_size target_offset) {
    if (uploader->flags & zest_buffer_upload_flag_initialised) {
        ZEST_ASSERT(uploader->source_buffer == source_buffer && uploader->target_buffer == target_buffer);    //Buffer uploads must be to the same source and target ids with each copy. Use a separate BufferUpload for each combination of source and target buffers
    }

    uploader->source_buffer = source_buffer;
    uploader->target_buffer = target_buffer;
    uploader->flags |= zest_buffer_upload_flag_initialised;

    zest_buffer_copy_t buffer_info = { 0 };
    buffer_info.src_offset = source_buffer->buffer_offset;
    buffer_info.dst_offset = target_offset;
    ZEST_ASSERT(source_buffer->memory_in_use <= target_buffer->size + target_offset);
    buffer_info.size = source_buffer->memory_in_use;
    zest_vec_linear_push(ZestRenderer->frame_graph_allocator[ZEST_FIF], uploader->buffer_copies, buffer_info);
    target_buffer->memory_in_use = source_buffer->memory_in_use;
}

zest_buffer_pool_size_t zest_GetDevicePoolSize(zest_key hash) {
    if (zest_map_valid_key(ZestDevice->pool_sizes, hash)) {
        return *zest_map_at_key(ZestDevice->pool_sizes, hash);
    }
    zest_buffer_pool_size_t pool_size = { 0 };
    return pool_size;
}

zest_buffer_pool_size_t zest_GetDeviceBufferPoolSize(zest_buffer_usage_flags usage_flags, zest_memory_property_flags property_flags, zest_image_usage_flags image_flags) {
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
VkResult zest__initialise_renderer(zest_create_info_t* create_info) {
    ZEST_PRINT_FUNCTION;
    ZestRenderer->magic = zest_INIT_MAGIC(zest_struct_type_renderer);
    ZestRenderer->flags |= (create_info->flags & zest_init_flag_enable_vsync) ? zest_renderer_flag_vsync_enabled : 0;
    zest_SetText(&ZestRenderer->shader_path_prefix, create_info->shader_path_prefix);
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create swap chain");
    zest_swapchain swapchain = zest__create_swapchain(create_info->title);
    ZestRenderer->main_swapchain = swapchain;
    ZestRenderer->main_window->swapchain = swapchain;

    zest__initialise_store(&ZestRenderer->shader_resources, sizeof(zest_shader_resources_t));
    zest__initialise_store(&ZestRenderer->images, sizeof(zest_image_t));
    zest__initialise_store(&ZestRenderer->views, sizeof(zest_image_view));
    zest__initialise_store(&ZestRenderer->view_arrays, sizeof(zest_image_view_array));
    zest__initialise_store(&ZestRenderer->uniform_buffers, sizeof(zest_uniform_buffer_t));
    zest__initialise_store(&ZestRenderer->timers, sizeof(zest_timer_t));
    zest__initialise_store(&ZestRenderer->layers, sizeof(zest_layer_t));
    zest__initialise_store(&ZestRenderer->shaders, sizeof(zest_shader_t));
    zest__initialise_store(&ZestRenderer->compute_pipelines, sizeof(zest_compute_t));
    zest__initialise_store(&ZestRenderer->set_layouts, sizeof(zest_set_layout_t));
    zest__initialise_store(&ZestRenderer->samplers, sizeof(zest_sampler_t));

    ZestRenderer->deferred_resource_freeing_list.backend = ZestPlatform->new_deferred_desctruction_backend();

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create descriptor layouts");

    //Create a global bindless descriptor set for storage buffers and texture samplers
    zest_set_layout_builder_t layout_builder = zest_BeginSetLayoutBuilder();
    zest_AddLayoutBuilderBinding(&layout_builder, (zest_descriptor_binding_desc_t){ zest_sampler_2d_binding, zest_descriptor_type_sampler, create_info->bindless_combined_sampler_2d_count, zest_shader_compute_stage | zest_shader_fragment_stage } );
    zest_AddLayoutBuilderBinding(&layout_builder, (zest_descriptor_binding_desc_t){ zest_sampler_array_binding, zest_descriptor_type_sampler, create_info->bindless_combined_sampler_array_count, zest_shader_compute_stage | zest_shader_fragment_stage } );
    zest_AddLayoutBuilderBinding(&layout_builder, (zest_descriptor_binding_desc_t){ zest_sampler_cube_binding, zest_descriptor_type_sampler, create_info->bindless_combined_sampler_cube_count, zest_shader_compute_stage | zest_shader_fragment_stage } );
    zest_AddLayoutBuilderBinding(&layout_builder, (zest_descriptor_binding_desc_t){ zest_sampler_3d_binding, zest_descriptor_type_sampler, create_info->bindless_combined_sampler_3d_count, zest_shader_compute_stage | zest_shader_fragment_stage } );
    zest_AddLayoutBuilderBinding(&layout_builder, (zest_descriptor_binding_desc_t){ zest_storage_buffer_binding, zest_descriptor_type_storage_buffer, create_info->bindless_storage_buffer_count, zest_shader_all_stages } );
    zest_AddLayoutBuilderBinding(&layout_builder, (zest_descriptor_binding_desc_t){ zest_sampled_image_binding, zest_descriptor_type_sampled_image, create_info->bindless_sampled_image_count, zest_shader_compute_stage | zest_shader_fragment_stage  } );
    zest_AddLayoutBuilderBinding(&layout_builder, (zest_descriptor_binding_desc_t){ zest_storage_image_binding, zest_descriptor_type_storage_image, create_info->bindless_storage_image_count, zest_shader_compute_stage | zest_shader_fragment_stage  } );
    ZestRenderer->global_bindless_set_layout = zest_FinishDescriptorSetLayoutForBindless(&layout_builder, 1, 0, "Zest Descriptor Layout");
    ZestRenderer->global_set = zest_CreateBindlessSet(ZestRenderer->global_bindless_set_layout);

    ZestRenderer->depth_format = zest__from_vk_format(zest__find_depth_format());

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create pipeline cache");
    zest__create_pipeline_cache();
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Compile shaders");
    zest__compile_builtin_shaders(ZEST__FLAGGED(create_info->flags, zest_init_flag_disable_shaderc));
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create standard pipelines");
    zest__prepare_standard_pipelines();

    VkFenceCreateInfo fence_info = { 0 };
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = 0;
	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_fence);
	for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
		zest_ForEachFrameInFlight(i) {
            ZEST_VK_ASSERT_RESULT(vkCreateFence(ZestDevice->backend->logical_device, &fence_info, &ZestDevice->backend->allocation_callbacks, &ZestRenderer->backend->fif_fence[i][queue_index]));
			ZestRenderer->fence_count[i] = 0;
        }
		ZEST_VK_ASSERT_RESULT(vkCreateFence(ZestDevice->backend->logical_device, &fence_info, &ZestDevice->backend->allocation_callbacks, &ZestRenderer->backend->intraframe_fence[queue_index]));
    }

    zest__create_debug_layout_and_pool(create_info->maximum_textures);

    VkCommandBufferAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_command_buffer);
    zest_ForEachFrameInFlight(fif) {
		alloc_info.commandPool = ZestDevice->backend->one_time_command_pool[fif];
        ZEST_VK_ASSERT_RESULT(vkAllocateCommandBuffers(ZestDevice->backend->logical_device, &alloc_info, &ZestRenderer->backend->utility_command_buffer[fif]));
    }

    zest_ForEachFrameInFlight(fif) {
		void *frame_graph_linear_memory = ZEST__ALLOCATE(zloc__MEGABYTE(1));
        ZestRenderer->frame_graph_allocator[fif] = zloc_InitialiseLinearAllocator(frame_graph_linear_memory, zloc__MEGABYTE(1));
		ZEST_ASSERT(ZestRenderer->frame_graph_allocator[fif]);    //Unabable to allocate the frame graph allocator, 
    }

    void *utility_linear_memory = ZEST__ALLOCATE(zloc__MEGABYTE(1));
    ZestRenderer->utility_allocator = zloc_InitialiseLinearAllocator(utility_linear_memory, zloc__MEGABYTE(1));

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Finished zest initialisation");

    ZEST__FLAG(ZestRenderer->flags, zest_renderer_flag_initialised);

    return VK_SUCCESS;
}

zest_swapchain zest__create_swapchain(const char *name) {
    zest_swapchain swapchain = ZEST__NEW(zest_swapchain);
    *swapchain = (zest_swapchain_t){ 0 };
    swapchain->magic = zest_INIT_MAGIC(zest_struct_type_swapchain);
    swapchain->backend = zest__new_swapchain_backend();
    swapchain->name = name;
    if (!zest__initialise_swapchain(swapchain, ZestRenderer->main_window)) {
        zest__cleanup_swapchain(swapchain, ZEST_FALSE);
        return NULL;
    }

    swapchain->resolution.x = 1.f / swapchain->size.width;
    swapchain->resolution.y = 1.f / swapchain->size.height;
    return swapchain;
}

VkResult zest__create_pipeline_cache() {
    ZEST_PRINT_FUNCTION;
    VkPipelineCacheCreateInfo pipeline_cache_create_info = { 0 };
    pipeline_cache_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_pipeline_cache);
    ZEST_VK_ASSERT_RESULT(vkCreatePipelineCache(ZestDevice->backend->logical_device, &pipeline_cache_create_info, &ZestDevice->backend->allocation_callbacks, &ZestRenderer->backend->pipeline_cache));
    return VK_SUCCESS;
}

void zest__cleanup_swapchain(zest_swapchain swapchain, zest_bool for_recreation) {
    zest__cleanup_swapchain_backend(swapchain, for_recreation);
    zest_vec_free(swapchain->images);
    zest_vec_free(swapchain->views);
    if (!for_recreation) {
        ZEST__FREE(swapchain);
    }
}

void zest__cleanup_pipelines() {
    zest_map_foreach(i, ZestRenderer->cached_pipelines) {
        zest_pipeline pipeline = *zest_map_at_index(ZestRenderer->cached_pipelines, i);
        zest__cleanup_pipeline_backend(pipeline);
        ZEST__FREE(pipeline);
    }
}

void zest__cleanup_device(void) {
    ZEST_PRINT_FUNCTION;
	zest_vec_foreach(i, ZestDevice->queues) {
		zest_queue queue = ZestDevice->queues[i];
		zest__cleanup_queue_backend(queue);
	}
    zest_vec_free(ZestDevice->extensions);
    zest_vec_free(ZestDevice->backend->queue_families);
    zest_vec_free(ZestDevice->queues);
    zest_map_free(ZestDevice->queue_names);
    zest_map_free(ZestDevice->pool_sizes);
    zest_FreeText(&ZestDevice->log_path);
}

void zest__free_handle(void *handle) {
    zest_struct_type struct_type = (zest_struct_type)ZEST_STRUCT_TYPE(handle);
    switch (struct_type) {
    case zest_struct_type_pipeline_template:
        zest__cleanup_pipeline_template((zest_pipeline_template)handle);
        break;
    case zest_struct_type_swapchain:
        zest__cleanup_swapchain((zest_swapchain)handle, ZEST_FALSE);
        break;
    case zest_struct_type_image:
        zest__cleanup_image((zest_image)handle);
        break;
    }

}

void zest__scan_memory_and_free_resources() {
    zloc_pool_stats_t stats = zloc_CreateMemorySnapshot(zloc__first_block_in_pool(zloc_GetPool(ZestDevice->allocator)));
    if (stats.used_blocks == 0) {
        return;
    }
    void **memory_to_free = 0;
    zest_vec_reserve(memory_to_free, (zest_uint)stats.used_blocks);
    zloc_header *current_block = zloc__first_block_in_pool(zloc_GetPool(ZestDevice->allocator));
    while (!zloc__is_last_block_in_pool(current_block)) {
        if (!zloc__is_free_block(current_block)) {
            zest_size block_size = zloc__block_size(current_block);
            void *allocation = (void *)((char *)current_block + zloc__BLOCK_POINTER_OFFSET);
            if (ZEST_VALID_HANDLE(allocation)) {
                zest_struct_type struct_type = (zest_struct_type)ZEST_STRUCT_TYPE(allocation);
                switch (struct_type) {
                case zest_struct_type_pipeline_template:
                case zest_struct_type_swapchain:
                    zest_vec_push(memory_to_free, allocation);
                    break;
                }
            }
        }
        current_block = zloc__next_physical_block(current_block);
    }
    zest_vec_foreach(i, memory_to_free) {
        void *allocation = memory_to_free[i];
        zest__free_handle(allocation);
    }
    zest_vec_free(memory_to_free);
}

void zest__cleanup_image_store() {
    zest_image_t *images = ZestRenderer->images.data;
    for (int i = 0; i != ZestRenderer->images.current_size; ++i) {
        if (ZEST_VALID_HANDLE(&images[i])) {
            zest_image resource = &images[i];
            zest__cleanup_image(resource);
        }
    }
    zest__free_store(&ZestRenderer->images);
}

void zest__cleanup_sampler_store() {
    zest_sampler_t *samplers = ZestRenderer->samplers.data;
    for (int i = 0; i != ZestRenderer->samplers.current_size; ++i) {
        if (ZEST_VALID_HANDLE(&samplers[i])) {
            zest_sampler resource = &samplers[i];
            zest__cleanup_sampler(resource);
        }
    }
    zest__free_store(&ZestRenderer->samplers);
}

void zest__cleanup_uniform_buffer_store() {
    zest_uniform_buffer_t *uniform_buffers = ZestRenderer->uniform_buffers.data;
    for (int i = 0; i != ZestRenderer->uniform_buffers.current_size; ++i) {
        if (ZEST_VALID_HANDLE(&uniform_buffers[i])) {
            zest_uniform_buffer resource = &uniform_buffers[i];
            zest__cleanup_uniform_buffer(resource);
        }
    }
    zest__free_store(&ZestRenderer->uniform_buffers);
}

void zest__cleanup_timer_store() {
    zest_timer_t *timers = ZestRenderer->timers.data;
    for (int i = 0; i != ZestRenderer->timers.current_size; ++i) {
        if (ZEST_VALID_HANDLE(&timers[i])) {
            zest_timer resource = &timers[i];
            zest_FreeTimer(resource->handle);
        }
    }
    zest__free_store(&ZestRenderer->timers);
}

void zest__cleanup_layer_store() {
    zest_layer_t *layers = ZestRenderer->layers.data;
    for (int i = 0; i != ZestRenderer->layers.current_size; ++i) {
        if (ZEST_VALID_HANDLE(&layers[i])) {
            zest_layer resource = &layers[i];
            zest__cleanup_layer(resource);
        }
    }
    zest__free_store(&ZestRenderer->layers);
}

void zest__cleanup_shader_store() {
    zest_shader_t *shaders = ZestRenderer->shaders.data;
    for (int i = 0; i != ZestRenderer->shaders.current_size; ++i) {
        if (ZEST_VALID_HANDLE(&shaders[i])) {
            zest_shader resource = &shaders[i];
            zest_FreeShader(resource->handle);
        }
    }
    zest__free_store(&ZestRenderer->shaders);
}

void zest__cleanup_compute_store() {
    zest_compute_t *compute_pipelines = ZestRenderer->compute_pipelines.data;
    for (int i = 0; i != ZestRenderer->compute_pipelines.current_size; ++i) {
        if (ZEST_VALID_HANDLE(&compute_pipelines[i])) {
            zest_compute resource = &compute_pipelines[i];
            zest__cleanup_compute(resource);
        }
    }
    zest__free_store(&ZestRenderer->compute_pipelines);
}

void zest__cleanup_set_layout_store() {
    zest_set_layout_t *set_layouts = ZestRenderer->set_layouts.data;
    for (int i = 0; i != ZestRenderer->set_layouts.current_size; ++i) {
        if (ZEST_VALID_HANDLE(&set_layouts[i])) {
            zest_set_layout resource = &set_layouts[i];
            zest__cleanup_set_layout(resource);
        }
    }
    zest__free_store(&ZestRenderer->set_layouts);
}

void zest__cleanup_shader_resource_store() {
    zest_shader_resources_t *shader_resources = (zest_shader_resources)ZestRenderer->shader_resources.data;
    for (int i = 0; i != ZestRenderer->shader_resources.current_size; ++i) {
        if (ZEST_VALID_HANDLE(&shader_resources[i])) {
            zest_shader_resources resource = &shader_resources[i];
            zest__cleanup_shader_resources_backend(resource);
        }
    }
    zest__free_store(&ZestRenderer->shader_resources);
}

void zest__cleanup_view_store() {
    zest_image_view *views = ZestRenderer->views.data;
    for (int i = 0; i != ZestRenderer->views.current_size; ++i) {
        if (ZEST_VALID_HANDLE(views[i])) {
            zest_image_view resource = views[i];
            zest__cleanup_image_view(resource);
        }
    }
    zest__free_store(&ZestRenderer->views);
}

void zest__cleanup_view_array_store() {
    zest_image_view_array *view_arrays = ZestRenderer->view_arrays.data;
    for (int i = 0; i != ZestRenderer->view_arrays.current_size; ++i) {
        if (ZEST_VALID_HANDLE(view_arrays[i])) {
            zest_image_view_array resource = view_arrays[i];
            zest__cleanup_image_view_array(resource);
        }
    }
    zest__free_store(&ZestRenderer->view_arrays);
}

void zest__cleanup_renderer() {
    ZEST_PRINT_FUNCTION;

    zest_uint cached_pipelines_size = zest_map_size(ZestRenderer->cached_pipelines);

    zest__cleanup_shader_resource_store();
    zest__cleanup_image_store();
    zest__cleanup_uniform_buffer_store();
    zest__cleanup_timer_store();
    zest__cleanup_layer_store();
    zest__cleanup_shader_store();
    zest__cleanup_compute_store();
    zest__cleanup_set_layout_store();
    zest__cleanup_sampler_store();
    zest__cleanup_view_store();
    zest__cleanup_view_array_store();

    ZestRenderer->texture_debug_layout = (zest_set_layout_handle){ 0 };
    ZestRenderer->global_bindless_set_layout = (zest_set_layout_handle){ 0 };

    zest__scan_memory_and_free_resources();

    zest__cleanup_pipelines();

    vkDestroyPipelineCache(ZestDevice->backend->logical_device, ZestRenderer->backend->pipeline_cache, &ZestDevice->backend->allocation_callbacks);

    zest_map_foreach(i, ZestRenderer->cached_render_passes) {
        zest_render_pass render_pass = *zest_map_at_index(ZestRenderer->cached_render_passes, i);
        ZestPlatform->cleanup_render_pass(render_pass);
        ZEST__FREE(render_pass);
    }

	for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
		zest_ForEachFrameInFlight(i) {
            vkDestroyFence(ZestDevice->backend->logical_device, ZestRenderer->backend->fif_fence[i][queue_index], &ZestDevice->backend->allocation_callbacks);
        }
		vkDestroyFence(ZestDevice->backend->logical_device, ZestRenderer->backend->intraframe_fence[queue_index], &ZestDevice->backend->allocation_callbacks);
    }

    zest_vec_foreach(i, ZestRenderer->timeline_semaphores) {
        zest_execution_timeline timeline = ZestRenderer->timeline_semaphores[i];
        vkDestroySemaphore(ZestDevice->backend->logical_device, timeline->backend->semaphore, &ZestDevice->backend->allocation_callbacks);
        ZEST__FREE(timeline);
    }
    zest_vec_free(ZestRenderer->timeline_semaphores);

    zest_map_foreach(i, ZestRenderer->cached_frame_graph_semaphores) {
        zest_frame_graph_semaphores semaphores = ZestRenderer->cached_frame_graph_semaphores.data[i];
        ZestPlatform->cleanup_frame_graph_semaphore(semaphores);
        ZEST__FREE(semaphores);
    }

    zest_FlushUsedBuffers();
    zest__cleanup_buffers_in_allocators();

    zest_ForEachFrameInFlight(fif) {
        if (zest_vec_size(ZestRenderer->deferred_resource_freeing_list.images[fif])) {
            zest_vec_foreach(i, ZestRenderer->deferred_resource_freeing_list.images[fif]) {
                zest_image image = &ZestRenderer->deferred_resource_freeing_list.images[fif][i];
                ZestPlatform->cleanup_image_backend(image);
                zest_FreeBuffer(image->buffer);
                if (image->default_view) {
                    ZestPlatform->cleanup_image_view_backend(image->default_view);
                }
            }
            zest_vec_clear(ZestRenderer->deferred_resource_freeing_list.images[fif]);
        }

        if (zest_vec_size(ZestRenderer->deferred_resource_freeing_list.views[fif])) {
            zest_vec_foreach(i, ZestRenderer->deferred_resource_freeing_list.views[fif]) {
                zest_image_view view = &ZestRenderer->deferred_resource_freeing_list.views[fif][i];
                ZestPlatform->cleanup_image_view_backend(view);
            }
            zest_vec_clear(ZestRenderer->deferred_resource_freeing_list.views[fif]);
        }
    }

    ZestPlatform->cleanup_deferred_destruction_backend();

    zest_map_foreach(i, ZestRenderer->buffer_allocators) {
        zest_buffer_allocator buffer_allocator = *zest_map_at_index(ZestRenderer->buffer_allocators, i);
        zest_vec_foreach(j, buffer_allocator->memory_pools) {
            zest__destroy_memory(buffer_allocator->memory_pools[j]);
            ZEST__FREE(buffer_allocator->memory_pools[j]);
        }
        zest_vec_free(buffer_allocator->memory_pools);
        zest_vec_foreach(j, buffer_allocator->range_pools) {
            ZEST__FREE(buffer_allocator->range_pools[j]);
        }
        zest_vec_free(buffer_allocator->range_pools);
        ZEST__FREE(buffer_allocator->allocator);
        ZEST__FREE(buffer_allocator);
    }

	ZestRenderer->destroy_window_callback(ZestRenderer->main_window, ZestApp->user_data);
    ZEST__FREE(ZestRenderer->main_window->backend);
	ZEST__FREE(ZestRenderer->main_window);

    zest_map_foreach(i, ZestRenderer->reports) {
        zest_report_t *report = &ZestRenderer->reports.data[i];
        zest_FreeText(&report->message);
    }

    zest_map_foreach(i, ZestRenderer->cached_frame_graphs) {
        zest_cached_frame_graph_t *cached_graph = &ZestRenderer->cached_frame_graphs.data[i];
        ZEST__FREE(cached_graph->memory);
    }

    zest_ForEachFrameInFlight(fif) {
        ZEST__FREE(ZestRenderer->frame_graph_allocator[fif]);
        zest_vec_free(ZestRenderer->deferred_resource_freeing_list.resources[fif]);
        zest_vec_free(ZestRenderer->deferred_resource_freeing_list.images[fif]);
        zest_vec_free(ZestRenderer->deferred_resource_freeing_list.binding_indexes[fif]);
    }

    ZEST__FREE(ZestRenderer->utility_allocator);
    ZEST__FREE(ZestRenderer->global_set->backend);
    ZEST__FREE(ZestRenderer->global_set);

    zest_vec_free(ZestRenderer->staging_buffers);
    zest_vec_free(ZestRenderer->debug.frame_log);
    zest_map_free(ZestRenderer->reports);

    zest_map_free(ZestRenderer->buffer_allocators);
    zest_vec_free(ZestRenderer->backend->used_buffers_ready_for_freeing);
    zest_map_free(ZestRenderer->cached_render_passes);
    zest_map_free(ZestRenderer->cached_pipelines);
    zest_map_free(ZestRenderer->cached_frame_graph_semaphores);
    zest_map_free(ZestRenderer->cached_frame_graphs);

    zest_FreeText(&ZestRenderer->shader_path_prefix);

    ZEST__FREE(ZestRenderer->backend);
    *ZestRenderer = (zest_renderer_t){ 0 };
}

zest_bool zest__recreate_swapchain(zest_swapchain swapchain) {
    int fb_width = 0, fb_height = 0;
    int window_width = 0, window_height = 0;
    ZEST__FLAG(swapchain->flags, zest_swapchain_flag_was_recreated);
    while (fb_width == 0 || fb_height == 0) {
        ZestRenderer->get_window_size_callback(ZestApp->user_data, &fb_width, &fb_height, &window_width, &window_height);
        if (fb_width == 0 || fb_height == 0) {
            zest__os_poll_events();
        }
    }

    zest__update_window_size(ZestRenderer->main_window, window_width, window_height);
    swapchain->size = (zest_extent2d_t){ fb_width, fb_height };
    swapchain->resolution.x = 1.f / fb_width;
    swapchain->resolution.y = 1.f / fb_height;

    zest_WaitForIdleDevice();

    zest__cleanup_swapchain(swapchain, ZEST_TRUE);
    zest__cleanup_pipelines();
    zest_map_free(ZestRenderer->cached_pipelines);

    return zest__initialise_swapchain(swapchain, swapchain->window);
}

void zest__reset_queue_command_pool(zest_queue queue) {
    ZEST_PRINT_FUNCTION;
    vkResetCommandPool(ZestDevice->backend->logical_device, queue->backend->command_pool[ZEST_FIF], 0);
    queue->next_buffer = 0;
}

zest_uniform_buffer_handle zest_CreateUniformBuffer(const char *name, zest_size uniform_struct_size) {
    zest_uniform_buffer_handle handle = { zest__add_store_resource(&ZestRenderer->uniform_buffers) };
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(&ZestRenderer->uniform_buffers, handle.value);
    *uniform_buffer = (zest_uniform_buffer_t){ 0 };
    uniform_buffer->magic = zest_INIT_MAGIC(zest_struct_type_uniform_buffer);
    uniform_buffer->handle = handle;
    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_info.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	zest_set_layout_builder_t uniform_layout_builder = zest_BeginSetLayoutBuilder();
    zest_AddLayoutBuilderBinding(&uniform_layout_builder, (zest_descriptor_binding_desc_t){ 0, zest_descriptor_type_storage_image, 1, zest_shader_all_stages } );
	uniform_buffer->set_layout = zest_FinishDescriptorSetLayout(&uniform_layout_builder, "Layout for: %s", name);
	zest_CreateDescriptorPoolForLayout(uniform_buffer->set_layout, ZEST_MAX_FIF);

    zest_set_layout set_layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, uniform_buffer->set_layout.value);
    zest_ForEachFrameInFlight(fif) {
        uniform_buffer->buffer[fif] = zest_CreateBuffer(uniform_struct_size, &buffer_info, ZEST_NULL);
    }
    uniform_buffer->backend = zest__new_uniform_buffer_backend();
    zest__set_uniform_buffer_backend(uniform_buffer);
    zest_ForEachFrameInFlight(fif) {
		zest_descriptor_set_builder_t uniform_set_builder = zest_BeginDescriptorSetBuilder(uniform_buffer->set_layout);
		zest__add_set_builder_uniform_buffer(&uniform_set_builder, 0, 0, uniform_buffer, fif);
		uniform_buffer->descriptor_set[fif] = zest_FinishDescriptorSet(set_layout->pool, &uniform_set_builder, uniform_buffer->descriptor_set[fif]);
    }
    return handle;
}

void zest_FreeUniformBuffer(zest_uniform_buffer_handle handle) {
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(&ZestRenderer->uniform_buffers, handle.value);
    zest_vec_push(ZestRenderer->deferred_resource_freeing_list.resources[ZEST_FIF], uniform_buffer);
}

zest_set_layout_builder_t zest_BeginSetLayoutBuilder() {
    zest_set_layout_builder_t builder = { 0 };
    return builder;
}

bool zest__binding_exists_in_layout_builder(zest_set_layout_builder_t *builder, zest_uint binding) {
    zest_vec_foreach(i, builder->bindings) {
        if (builder->bindings[i].binding == binding) {
            return true;
        }
    }
    return false;
}

void zest_AddLayoutBuilderBinding( zest_set_layout_builder_t *builder, zest_descriptor_binding_desc_t description) {
    bool binding_exists = zest__binding_exists_in_layout_builder(builder, description.binding);
    ZEST_ASSERT(!binding_exists);       //That binding number already exists in the layout builder
    zest_vec_push(builder->bindings, description);
}

zest_set_layout_handle zest_FinishDescriptorSetLayout(zest_set_layout_builder_t *builder, const char *name, ...) {
    ZEST_ASSERT(name);  //Give the descriptor set a unique name
    zest_text_t layout_name = { 0 };
    va_list args;
    va_start(args, name);
    zest_SetTextfv(&layout_name, name, args);
    va_end(args);
    ZEST_ASSERT(builder->bindings);     //must have bindings to create the layout
    zest_uint binding_count = (zest_uint)zest_vec_size(builder->bindings);
    ZEST_ASSERT(binding_count > 0);     //Must add bindings before finishing the descriptor layout builder

    zest_set_layout_handle handle = { 0 };
    handle = zest__new_descriptor_set_layout(layout_name.str);
    zest_set_layout set_layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, handle.value);

    if (!ZestPlatform->create_set_layout(builder, set_layout, ZEST_TRUE)) {
		zest_vec_free(builder->bindings);
        zest__cleanup_set_layout(set_layout);
		zest_FreeText(&layout_name);
        return (zest_set_layout_handle){ 0 };
    }

    zest_CreateDescriptorPoolForLayout(handle, 1);

	zest_vec_free(builder->bindings);
    zest_FreeText(&layout_name);
    return handle;
}

zest_set_layout_handle zest_FinishDescriptorSetLayoutForBindless(zest_set_layout_builder_t *builder, zest_uint num_global_sets_this_pool_should_support, VkDescriptorPoolCreateFlags additional_pool_flags, const char *name, ...) {
    ZEST_ASSERT(name);  //Give the descriptor set a unique name
    zest_text_t layout_name = { 0 };
    va_list args;
    va_start(args, name);
    zest_SetTextfv(&layout_name, name, args);
    va_end(args);
    ZEST_ASSERT(builder->bindings);     //must have bindings to create the layout
    zest_uint binding_count = (zest_uint)zest_vec_size(builder->bindings);
	ZEST_ASSERT(binding_count > 0);     //Must add bindings before finishing the descriptor layout builder

    zest_set_layout_handle handle = zest__new_descriptor_set_layout(layout_name.str);
    zest_set_layout set_layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, handle.value);
    if (!ZestPlatform->create_set_layout(builder, set_layout, ZEST_TRUE)) {
        zest__cleanup_set_layout(set_layout);
		zest_FreeText(&layout_name);
        return (zest_set_layout_handle){ 0 };
    }

    zest_CreateDescriptorPoolForLayout(handle, 1);

    zest_FreeText(&layout_name);
    return handle;
}

ZEST_API zest_descriptor_set zest_CreateBindlessSet(zest_set_layout_handle layout_handle) {
    ZEST_PRINT_FUNCTION;
    zest_set_layout layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, layout_handle.value);
    ZEST_ASSERT(layout->backend->vk_layout != VK_NULL_HANDLE && "VkDescriptorSetLayout in wrapper is null.");
    ZEST_ASSERT(layout->pool->backend->vk_descriptor_pool != VK_NULL_HANDLE && "VkDescriptorPool in wrapper is null.");
    // max_sets_in_pool was set during pool creation, usually 1 for a global bindless set.
    ZEST_ASSERT(layout->pool->max_sets >= 1 && "Pool was not created to allow allocation of at least one set.");

    return ZestPlatform->create_bindless_set(layout);
}

zest_set_layout_handle zest__new_descriptor_set_layout(const char *name) {
    zest_set_layout_handle handle = (zest_set_layout_handle){ zest__add_store_resource(&ZestRenderer->set_layouts) };
    zest_set_layout descriptor_layout = (zest_set_layout)zest__get_store_resource(&ZestRenderer->set_layouts, handle.value);
    *descriptor_layout = (zest_set_layout_t){ 0 };
    descriptor_layout->backend = zest__new_set_layout_backend();
    descriptor_layout->name.str = 0;
    descriptor_layout->magic = zest_INIT_MAGIC(zest_struct_type_set_layout);
    descriptor_layout->handle = handle;
    zest_SetText(&descriptor_layout->name, name);
    return handle;
}


zest_uint zest__acquire_bindless_index(zest_set_layout layout, zest_uint binding_number) {
    if (binding_number >= zest_vec_size(layout->backend->descriptor_indexes)) {
        ZEST_PRINT("Attempted to acquire index for out-of-bounds binding_number %u for layout '%s'. Are you sure this is a layout that's configured for bindless descriptors?", binding_number, layout->name.str);
        return ZEST_INVALID; 
    }

    zest_descriptor_indices_t *manager = &layout->backend->descriptor_indexes[binding_number];

    if (zest_vec_size(manager->free_indices) > 0) {
        zest_uint index = zest_vec_back(manager->free_indices); // Reuse from free list
        zest_vec_pop(manager->free_indices);
        return index;
    } else {
        if (manager->next_new_index < manager->capacity) {
            return manager->next_new_index++; // Allocate a new one sequentially
        } else {
            ZEST_PRINT("Ran out of bindless indices for binding %u (type %d, capacity %u) in layout '%s'!",
                binding_number, manager->descriptor_type, manager->capacity, layout->name.str);
            return ZEST_INVALID;
        }
    }
}

void zest__release_bindless_index(zest_set_layout_handle layout_handle, zest_uint binding_number, zest_uint index_to_release) {
    zest_set_layout layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, layout_handle.value);
    ZEST_ASSERT_HANDLE(layout);   //Not a valid layout handle!
    if (index_to_release == ZEST_INVALID) return;

    if (binding_number >= zest_vec_size(layout->backend->descriptor_indexes)) {
        ZEST_PRINT("Attempted to release index for out-of-bounds binding_number %u for layout '%s'. Check that layout is actually intended for bindless.", binding_number, layout->name.str);
        return;
    }

    zest_descriptor_indices_t *manager = &layout->backend->descriptor_indexes[binding_number];

    zest_vec_foreach(i, manager->free_indices) {
        if (index_to_release == manager->free_indices[i]) {
			ZEST_PRINT("Attempted to release index for binding_number %u for layout '%s' that is already free. Make sure the binding number is correct.", binding_number, layout->name.str);
            return;
        }
    }

    ZEST_ASSERT(index_to_release < manager->capacity);
    zest_vec_push(manager->free_indices, index_to_release);
}

void zest__cleanup_set_layout(zest_set_layout layout) {
    ZEST_PRINT_FUNCTION;
    zest_FreeText(&layout->name);
    zest__cleanup_set_layout_backend(layout);
    ZEST__FREE(layout->pool);
    zest__remove_store_resource(&ZestRenderer->set_layouts, layout->handle.value);
}

void zest__cleanup_image_view(zest_image_view view) {
    ZEST_PRINT_FUNCTION;
    ZestPlatform->cleanup_image_view_backend(view);
    if (view->handle.value) {
        //Default views in images are not in resource storage
        zest__remove_store_resource(&ZestRenderer->views, view->handle.value);
    }
    ZEST__FREE(view);
}

void zest__cleanup_image_view_array(zest_image_view_array view_array) {
    ZEST_PRINT_FUNCTION;
    zest__cleanup_image_view_array_backend(view_array);
    ZEST__FREE(view_array->views);
    zest__remove_store_resource(&ZestRenderer->view_arrays, view_array->handle.value);
    ZEST__FREE(view_array);
}

void zest__add_descriptor_set_to_resources(zest_shader_resources resources, zest_descriptor_set descriptor_set, zest_uint fif) {
	zest_vec_push(resources->backend->sets[fif], descriptor_set);
}

VkDescriptorSetLayout zest_CreateDescriptorSetLayoutWithBindings(zest_uint count, VkDescriptorSetLayoutBinding* bindings) {
    ZEST_PRINT_FUNCTION;
    ZEST_ASSERT(bindings);    //must have bindings to create the layout

    VkDescriptorSetLayoutCreateInfo layoutInfo = { 0 };
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = count;
    layoutInfo.pBindings = bindings;

    VkDescriptorSetLayout layout;

	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_descriptor_layout);
    VkResult result = vkCreateDescriptorSetLayout(ZestDevice->backend->logical_device, &layoutInfo, &ZestDevice->backend->allocation_callbacks, &layout);

    if (result != VK_SUCCESS) {
        ZEST_VK_PRINT_RESULT(result);
        return VK_NULL_HANDLE;
    }

    return layout;
}

VkDescriptorSetLayoutBinding zest_CreateDescriptorLayoutBinding(VkDescriptorType type, zest_supported_shader_stages stageFlags, zest_uint binding, zest_uint descriptorCount) {
    VkDescriptorSetLayoutBinding set_layout_binding = { 0 };
    set_layout_binding.descriptorType = type;
    set_layout_binding.stageFlags = stageFlags;
    set_layout_binding.binding = binding;
    set_layout_binding.descriptorCount = descriptorCount;
    return set_layout_binding;
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

zest_descriptor_set_builder_t zest_BeginDescriptorSetBuilder(zest_set_layout_handle layout_handle) {
    zest_descriptor_set_builder_t builder = { 0 };
    builder.associated_layout = layout_handle;
    return builder;
}

VkDescriptorSetLayoutBinding *zest__get_layout_binding_info(zest_set_layout layout, zest_uint binding_index) {
    zest_vec_foreach(i, layout->backend->layout_bindings) {
        if (layout->backend->layout_bindings[i].binding == binding_index) {
            return &layout->backend->layout_bindings[i];
        }
    }
    return NULL;
}

void zest_AddSetBuilderWrite( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_uint descriptor_count, VkDescriptorType descriptor_type, const VkDescriptorImageInfo *p_image_infos, const VkDescriptorBufferInfo *p_buffer_infos, const VkBufferView *p_texel_buffer_views) {
    zest_set_layout associated_layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, builder->associated_layout.value);
    // Verify that the descriptor write complies with the associated descriptor set layout
    ZEST_ASSERT(builder != NULL && associated_layout != NULL);
    const VkDescriptorSetLayoutBinding *layout_binding = zest__get_layout_binding_info(associated_layout, dst_binding); 

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

void zest_AddSetBuilderCombinedImageSamplers( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_uint count, const VkDescriptorImageInfo *p_image_infos ) {
    zest_AddSetBuilderWrite(builder, dst_binding, dst_array_element, count, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, p_image_infos, NULL, NULL);
}

void zest_AddSetBuilderDirectImageWrite( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, VkDescriptorType type, const VkDescriptorImageInfo *image_info) {
    zest_AddSetBuilderWrite(builder, dst_binding, dst_array_element, 1, type, image_info, NULL, NULL);
}

void zest_AddSetBuilderUniformBuffer( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_uniform_buffer_handle handle, zest_uint fif) {
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(&ZestRenderer->uniform_buffers, handle.value);
    zest__add_set_builder_uniform_buffer(builder, dst_binding, dst_array_element, uniform_buffer, fif);
}

void zest_AddSetBuilderStorageBuffer( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_buffer storage_buffer) {
    ZEST_ASSERT_HANDLE(storage_buffer);  //Not a valid storage buffer
    VkDescriptorBufferInfo buffer_info = zest_vk_GetBufferInfo(storage_buffer);
    zest_AddSetBuilderWrite(builder, dst_binding, dst_array_element, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &buffer_info, NULL);
}

zest_descriptor_set zest_FinishDescriptorSet(zest_descriptor_pool pool, zest_descriptor_set_builder_t *builder, zest_descriptor_set new_set_to_populate_or_update ) {
    ZEST_PRINT_FUNCTION;
    zest_set_layout associated_layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, builder->associated_layout.value);
    ZEST_ASSERT_HANDLE(pool);    //Must be a valid desriptor pool
    ZEST_ASSERT(builder != NULL && associated_layout != NULL);

    if (!ZEST_VALID_HANDLE(new_set_to_populate_or_update)) {
        new_set_to_populate_or_update = ZEST__NEW(zest_descriptor_set);
        *new_set_to_populate_or_update = (zest_descriptor_set_t){ 0 };
		new_set_to_populate_or_update->backend = ZEST__NEW(zest_descriptor_set_backend);
		*new_set_to_populate_or_update->backend = (zest_descriptor_set_backend_t){ 0 };
        new_set_to_populate_or_update->magic = zest_INIT_MAGIC(zest_struct_type_descriptor_set);
    }

    VkDescriptorSet target_set = new_set_to_populate_or_update->backend->vk_descriptor_set;

    if (target_set == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo allocation_info = { 0 };
        allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocation_info.descriptorPool = pool->backend->vk_descriptor_pool;
        allocation_info.descriptorSetCount = 1;
        allocation_info.pSetLayouts = &(associated_layout->backend->vk_layout); // Assuming your layout struct holds the Vk handle

        VkResult result = vkAllocateDescriptorSets(ZestDevice->backend->logical_device, &allocation_info, &target_set);
        if (result != VK_SUCCESS) {
            ZEST_VK_PRINT_RESULT(result);
            target_set = VK_NULL_HANDLE;
            goto cleanup;
        }
    }

    // Update dstSet for all stored write operations
    zest_vec_foreach(i, builder->writes) {
        builder->writes[i].dstSet = target_set;
    }

    if (zest_vec_size(builder->writes) > 0) {
        vkUpdateDescriptorSets( ZestDevice->backend->logical_device, zest_vec_size(builder->writes), builder->writes, 0, NULL);
    }

cleanup:
    zest_vec_free(builder->writes);
    zest_vec_free(builder->image_infos_storage);
    zest_vec_free(builder->buffer_infos_storage);
    zest_vec_free(builder->texel_buffer_view_storage);

    new_set_to_populate_or_update->backend->vk_descriptor_set = target_set;
    return new_set_to_populate_or_update;
}

zest_shader_resources_handle zest_CreateShaderResources() {
    zest_shader_resources_handle handle = { zest__add_store_resource(&ZestRenderer->shader_resources) };
    zest_shader_resources bundle = (zest_shader_resources)zest__get_store_resource_checked(&ZestRenderer->shader_resources, handle.value);
    *bundle = (zest_shader_resources_t){ 0 };
    bundle->magic = zest_INIT_MAGIC(zest_struct_type_shader_resources);
    bundle->backend = ZEST__NEW(zest_shader_resources_backend);
    *bundle->backend = (zest_shader_resources_backend_t){ 0 };
    return handle;
}

void zest_FreeShaderResources(zest_shader_resources_handle handle) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(&ZestRenderer->shader_resources, handle.value);
    if (ZEST_VALID_HANDLE(shader_resources)) {
        zest_ForEachFrameInFlight(fif) {
            zest_vec_free(shader_resources->backend->sets[fif]);
        }
		zest_vec_free(shader_resources->backend->binding_sets);
        ZEST__FREE(shader_resources->backend);
        zest__remove_store_resource(&ZestRenderer->shader_resources, handle.value);
    }
}

void zest_AddDescriptorSetToResources(zest_shader_resources_handle handle, zest_descriptor_set descriptor_set, zest_uint fif) {
    zest_shader_resources resources = (zest_shader_resources)zest__get_store_resource_checked(&ZestRenderer->shader_resources, handle.value);
    ZEST_ASSERT_HANDLE(resources);   //Not a valid shader resource handle
	zest_vec_push(resources->backend->sets[fif], descriptor_set);
}

void zest_AddUniformBufferToResources(zest_shader_resources_handle shader_resources_handle, zest_uniform_buffer_handle buffer_handle) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(&ZestRenderer->shader_resources, shader_resources_handle.value);
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(&ZestRenderer->uniform_buffers, buffer_handle.value);
    zest_ForEachFrameInFlight(fif) {
        zest__add_descriptor_set_to_resources(shader_resources, uniform_buffer->descriptor_set[fif], fif);
    }
}

void zest_AddGlobalBindlessSetToResources(zest_shader_resources_handle handle) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(&ZestRenderer->shader_resources, handle.value);
    zest_ForEachFrameInFlight(fif) {
        zest__add_descriptor_set_to_resources(shader_resources, ZestRenderer->global_set, fif);
    }
}

void zest_AddBindlessSetToResources(zest_shader_resources_handle handle, zest_descriptor_set set) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(&ZestRenderer->shader_resources, handle.value);
    zest_ForEachFrameInFlight(fif) {
        zest__add_descriptor_set_to_resources(shader_resources, set, fif);
    }
}

void zest_UpdateShaderResources(zest_shader_resources_handle handle, zest_descriptor_set descriptor_set, zest_uint index, zest_uint fif) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(&ZestRenderer->shader_resources, handle.value);
	ZEST_ASSERT(index < zest_vec_size(shader_resources->backend->sets));    //Must be a valid index for the descriptor set in the resources that you want to update.
	shader_resources->backend->sets[fif][index] = descriptor_set;
}

void zest_ClearShaderResources(zest_shader_resources_handle handle) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(&ZestRenderer->shader_resources, handle.value);
    if (!shader_resources) return;
    zest_vec_clear(shader_resources->backend->sets);
}

bool zest_ValidateShaderResource(zest_shader_resources_handle handle) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(&ZestRenderer->shader_resources, handle.value);
    zest_ForEachFrameInFlight(fif) {
        zest_uint set_size = zest_vec_size(shader_resources->backend->sets[fif]);
        zest_vec_foreach(i, shader_resources->backend->sets[fif]) {
            if (!ZEST_VALID_HANDLE(shader_resources->backend->sets[fif][i])) {
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
    pool->magic = zest_INIT_MAGIC(zest_struct_type_descriptor_pool);
    pool->backend = zest__new_descriptor_pool_backend();
    return pool;
}

zest_bool zest_CreateDescriptorPoolForLayout(zest_set_layout_handle layout_handle, zest_uint max_set_count) {
    zest_set_layout layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, layout_handle.value);

    return ZestPlatform->create_set_pool(layout, max_set_count, ZEST__FLAGGED(layout->flags, zest_set_layout_flag_bindless));
}

void zest_ResetDescriptorPool(zest_descriptor_pool pool) {
    ZEST_PRINT_FUNCTION;
    ZEST_ASSERT_HANDLE(pool);        //Not a valid descriptor pool
    vkResetDescriptorPool(ZestDevice->backend->logical_device, pool->backend->vk_descriptor_pool, 0);
    pool->allocations = 0;
}

void zest_FreeDescriptorPool(zest_descriptor_pool pool) {
    ZEST_PRINT_FUNCTION;
    ZEST_ASSERT_HANDLE(pool);        //Not a valid descriptor pool
    vkResetDescriptorPool(ZestDevice->backend->logical_device, pool->backend->vk_descriptor_pool, 0);
    vkDestroyDescriptorPool(ZestDevice->backend->logical_device, pool->backend->vk_descriptor_pool, &ZestDevice->backend->allocation_callbacks);
    ZEST__FREE(pool);
}

void zest_UpdateDescriptorSet(VkWriteDescriptorSet* descriptor_writes) {
    ZEST_PRINT_FUNCTION;
    vkUpdateDescriptorSets(ZestDevice->backend->logical_device, zest_vec_size(descriptor_writes), descriptor_writes, 0, ZEST_NULL);
}

zest_pipeline zest__create_pipeline() {
    zest_pipeline pipeline = ZEST__NEW(zest_pipeline);
    *pipeline = (zest_pipeline_t){ 0 };
    pipeline->backend = ZEST__NEW(zest_pipeline);
    *pipeline->backend = (zest_pipeline_backend_t){ 0 };
    pipeline->magic = zest_INIT_MAGIC(zest_struct_type_pipeline);
    return pipeline;
}

void zest_SetPipelineVertShader(zest_pipeline_template pipeline_template, zest_shader_handle shader_handle) {
    ZEST_ASSERT_HANDLE(pipeline_template);
    zest_shader vertex_shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, shader_handle.value);
    ZEST_ASSERT_HANDLE(vertex_shader);  //Not a valid vertex shader handle
    pipeline_template->vertex_shader = shader_handle;
}

void zest_SetPipelineFragShader(zest_pipeline_template pipeline_template, zest_shader_handle shader_handle) {
    ZEST_ASSERT_HANDLE(pipeline_template);
    zest_shader fragment_shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, shader_handle.value);
    ZEST_ASSERT_HANDLE(fragment_shader);	//Not a valid fragment shader handle
    pipeline_template->fragment_shader = shader_handle;
}

void zest_SetPipelineShaders(zest_pipeline_template pipeline_template, zest_shader_handle vertex_shader_handle, zest_shader_handle fragment_shader_handle) {
    ZEST_ASSERT_HANDLE(pipeline_template);
    zest_shader vertex_shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, vertex_shader_handle.value);
    zest_shader fragment_shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, fragment_shader_handle.value);
    ZEST_ASSERT_HANDLE(vertex_shader);       //Not a valid vertex shader handle
    ZEST_ASSERT_HANDLE(fragment_shader);     //Not a valid fragment shader handle
    pipeline_template->vertex_shader = vertex_shader_handle;
    pipeline_template->fragment_shader = fragment_shader_handle;
}

void zest_SetPipelineShader(zest_pipeline_template pipeline_template, zest_shader_handle combined_vertex_and_fragment_shader_handle) {
    ZEST_ASSERT_HANDLE(pipeline_template);
    zest_shader combined_vertex_and_fragment_shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, combined_vertex_and_fragment_shader_handle.value);
    ZEST_ASSERT_HANDLE(combined_vertex_and_fragment_shader);       //Not a valid shader handle
    pipeline_template->vertex_shader = combined_vertex_and_fragment_shader_handle;
}

void zest_SetPipelineFrontFace(zest_pipeline_template pipeline_template, zest_front_face front_face) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //invalid pipeline template handle
    pipeline_template->rasterizer.frontFace = front_face == zest_front_face_clockwise ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

void zest_SetPipelineTopology(zest_pipeline_template pipeline_template, zest_topology topology) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //invalid pipeline template handle
    switch (topology) {
		case zest_topology_point_list                   : pipeline_template->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
		case zest_topology_line_list                    : pipeline_template->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
		case zest_topology_line_strip                   : pipeline_template->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
		case zest_topology_triangle_list                : pipeline_template->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
		case zest_topology_triangle_strip               : pipeline_template->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
		case zest_topology_triangle_fan                 : pipeline_template->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN; break;
		case zest_topology_line_list_with_adjacency     : pipeline_template->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY; break;
		case zest_topology_line_strip_with_adjacency    : pipeline_template->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY; break;
		case zest_topology_triangle_list_with_adjacency : pipeline_template->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY; break;
		case zest_topology_triangle_strip_with_adjacency: pipeline_template->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY; break;
		case zest_topology_patch_list                   : pipeline_template->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; break;
		default                                         : pipeline_template->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
    }
}

void zest_SetPipelineCullMode(zest_pipeline_template pipeline_template, zest_cull_mode cull_mode) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //invalid pipeline template handle
    pipeline_template->rasterizer.cullMode = (VkCullModeFlags)cull_mode;
}

void zest_SetPipelinePushConstantRange(zest_pipeline_template pipeline_template, zest_uint size, zest_supported_shader_stages stage_flags) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
    VkPushConstantRange range;
    range.size = size;
    range.offset = 0;
    range.stageFlags = stage_flags;
    pipeline_template->pushConstantRange = range;
}

void zest_SetPipelinePushConstants(zest_pipeline_template pipeline_template, void *push_constants) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
    pipeline_template->push_constants = push_constants;
}

void zest_SetPipelineBlend(zest_pipeline_template pipeline_template, VkPipelineColorBlendAttachmentState blend_attachment) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
    pipeline_template->colorBlendAttachment = blend_attachment;
}

void zest_SetPipelineDepthTest(zest_pipeline_template pipeline_template, bool enable_test, bool write_enable) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
	pipeline_template->depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipeline_template->depthStencil.depthTestEnable = enable_test;
	pipeline_template->depthStencil.depthWriteEnable = write_enable;
	pipeline_template->depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	pipeline_template->depthStencil.depthBoundsTestEnable = VK_FALSE;
	pipeline_template->depthStencil.stencilTestEnable = VK_FALSE;
}

void zest_AddPipelineDescriptorLayout(zest_pipeline_template pipeline_template, VkDescriptorSetLayout layout) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
    zest_vec_push(pipeline_template->descriptorSetLayouts, layout);
}

void zest_ClearPipelineDescriptorLayouts(zest_pipeline_template pipeline_template) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
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
    zest_vec_foreach(i, pipeline_template->bindingDescriptions) {
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

VkPipelineColorBlendAttachmentState zest_BlendStateNone() {
    VkPipelineColorBlendAttachmentState color_blend_attachment = { 0 };
    color_blend_attachment.colorWriteMask = 0;
    color_blend_attachment.blendEnable = VK_FALSE;
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

void zest__set_pipeline_template(zest_pipeline_template pipeline_template) {
    if (!pipeline_template->no_vertex_input) {
        ZEST_ASSERT(zest_vec_size(pipeline_template->bindingDescriptions));    //If the pipeline is set to have vertex input, then you must add bindingDescriptions. You can use zest_AddVertexInputBindingDescription for this
        pipeline_template->vertexInputInfo.vertexBindingDescriptionCount = (zest_uint)zest_vec_size(pipeline_template->bindingDescriptions);
        pipeline_template->vertexInputInfo.pVertexBindingDescriptions = pipeline_template->bindingDescriptions;
        pipeline_template->vertexInputInfo.vertexAttributeDescriptionCount = (zest_uint)zest_vec_size(pipeline_template->attributeDescriptions);
        pipeline_template->vertexInputInfo.pVertexAttributeDescriptions = pipeline_template->attributeDescriptions;
    }

    if (pipeline_template->depthStencil.sType != VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO) {
        pipeline_template->depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        pipeline_template->depthStencil.depthTestEnable = VK_TRUE;
        pipeline_template->depthStencil.depthWriteEnable = VK_TRUE;
        pipeline_template->depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        pipeline_template->depthStencil.depthBoundsTestEnable = VK_FALSE;
        pipeline_template->depthStencil.stencilTestEnable = VK_FALSE;
    }

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
}

VkResult zest__cache_pipeline(zest_pipeline_template pipeline_template, zest_render_pass render_pass, zest_key cached_pipeline_key, zest_pipeline *out_pipeline) {
	zest_pipeline pipeline = zest__create_pipeline();
    pipeline->pipeline_template = pipeline_template;
    pipeline->backend->render_pass = render_pass->backend->vk_render_pass;
    VkResult result = zest__build_pipeline(pipeline);
    zest_map_insert_key(ZestRenderer->cached_pipelines, cached_pipeline_key, pipeline);
    zest_vec_push(pipeline_template->cached_pipeline_keys, cached_pipeline_key);
    *out_pipeline = pipeline;
    return result;
}

VkResult zest__build_pipeline(zest_pipeline pipeline) {
    ZEST_PRINT_FUNCTION;
	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_pipeline_layout);
    VkResult result = vkCreatePipelineLayout(ZestDevice->backend->logical_device, &pipeline->pipeline_template->pipelineLayoutInfo, &ZestDevice->backend->allocation_callbacks, &pipeline->backend->pipeline_layout);
    if (result != VK_SUCCESS) {
        ZEST_VK_PRINT_RESULT(result);
        return result;
    }

    VkShaderModule vert_shader_module = { 0 };
    VkShaderModule frag_shader_module = { 0 };
    zest_shader vert_shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, pipeline->pipeline_template->vertex_shader.value);
    zest_shader frag_shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, pipeline->pipeline_template->fragment_shader.value);
    if (ZEST_VALID_HANDLE(vert_shader)) {
        if (!vert_shader->spv) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Vertex shader [%s] in pipeline [%s] did not have any spv data, make sure it's compiled.", vert_shader->name.str, pipeline->pipeline_template->name.str);
			result = VK_ERROR_UNKNOWN;
            goto cleanup;
        }
        result = zest__create_shader_module(vert_shader->spv, &vert_shader_module);
        pipeline->pipeline_template->vertShaderStageInfo.module = vert_shader_module;
    } 

    if (ZEST_VALID_HANDLE(frag_shader)) {
        if (!frag_shader->spv) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Vertex shader [%s] in pipeline [%s] did not have any spv data, make sure it's compiled.", frag_shader->name.str, pipeline->pipeline_template->name.str);
			result = VK_ERROR_UNKNOWN;
            goto cleanup;
        }
        result = zest__create_shader_module(frag_shader->spv, &frag_shader_module);
        pipeline->pipeline_template->fragShaderStageInfo.module = frag_shader_module;
    } 

    if (result != VK_SUCCESS) {
        ZEST_VK_PRINT_RESULT(result);
        goto cleanup;
    }

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
    pipeline_info.layout = pipeline->backend->pipeline_layout;
    pipeline_info.renderPass = pipeline->backend->render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    if (pipeline->pipeline_template->dynamicState.dynamicStateCount) {
        pipeline_info.pDynamicState = &pipeline->pipeline_template->dynamicState;
    }

	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_pipelines);
    result = vkCreateGraphicsPipelines(ZestDevice->backend->logical_device, ZestRenderer->backend->pipeline_cache, 1, &pipeline_info, &ZestDevice->backend->allocation_callbacks, &pipeline->backend->pipeline);
    if (result != VK_SUCCESS) {
        ZEST_VK_PRINT_RESULT(result);
    } else {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Built pipeline %s", pipeline->pipeline_template->name.str);
    }

    cleanup:
	vkDestroyShaderModule(ZestDevice->backend->logical_device, frag_shader_module, &ZestDevice->backend->allocation_callbacks);
	vkDestroyShaderModule(ZestDevice->backend->logical_device, vert_shader_module, &ZestDevice->backend->allocation_callbacks);
    return result;
}

void zest_EndPipelineTemplate(zest_pipeline_template pipeline_template) {
    zest__set_pipeline_template(pipeline_template);
    pipeline_template->multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
}

VkResult zest__create_shader_module(char *code, VkShaderModule *shader_module) {
    ZEST_PRINT_FUNCTION;
    VkShaderModuleCreateInfo create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = zest_vec_size(code);
    create_info.pCode = (zest_uint*)(code);

	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_shader_module);
    ZEST_VK_ASSERT_RESULT(vkCreateShaderModule(ZestDevice->backend->logical_device, &create_info, &ZestDevice->backend->allocation_callbacks, shader_module));

    return VK_SUCCESS;
}

zest_pipeline_template zest_CopyPipelineTemplate(const char *name, zest_pipeline_template pipeline_to_copy) {
    zest_pipeline_template copy = zest_BeginPipelineTemplate(name);
    copy->no_vertex_input = pipeline_to_copy->no_vertex_input;
    copy->inputAssembly = pipeline_to_copy->inputAssembly;
    copy->viewport = pipeline_to_copy->viewport;
    copy->rasterizer = pipeline_to_copy->rasterizer;
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
    copy->vertex_shader = pipeline_to_copy->vertex_shader;
    copy->fragment_shader = pipeline_to_copy->fragment_shader;
    return copy;
}

void zest_FreePipelineTemplate(zest_pipeline_template pipeline_template) {
    ZEST_ASSERT_HANDLE(pipeline_template);   //Not a valid pipeline template handle
	zest_vec_foreach(i, pipeline_template->cached_pipeline_keys) {
		zest_key key = pipeline_template->cached_pipeline_keys[i];
		if (zest_map_valid_key(ZestRenderer->cached_pipelines, key)) {
			zest_pipeline pipeline = *zest_map_at_key(ZestRenderer->cached_pipelines, key);
            zest_vec_push(ZestRenderer->deferred_resource_freeing_list.resources[ZEST_FIF], pipeline);
		}
	}
    zest__cleanup_pipeline_template(pipeline_template);
}

void zest__cleanup_pipeline_template(zest_pipeline_template pipeline_template) {
    zest_FreeText(&pipeline_template->name);
    zest_FreeText(&pipeline_template->vertShaderFunctionName);
    zest_FreeText(&pipeline_template->fragShaderFunctionName);
    zest_vec_free(pipeline_template->descriptorSetLayouts);
    zest_vec_free(pipeline_template->attributeDescriptions);
    zest_vec_free(pipeline_template->bindingDescriptions);
    zest_vec_free(pipeline_template->cached_pipeline_keys);
    zest_vec_free(pipeline_template->dynamicStates);
    ZEST__FREE(pipeline_template);
}

VkResult zest__rebuild_pipeline(zest_pipeline pipeline) {
    return zest__build_pipeline(pipeline);
}

zest_shader_handle zest__new_shader(shaderc_shader_kind type) {
    zest_shader_handle handle = (zest_shader_handle){ zest__add_store_resource(&ZestRenderer->shaders) };
    zest_shader shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, handle.value);
    *shader = (zest_shader_t){ 0 };
    shader->magic = zest_INIT_MAGIC(zest_struct_type_shader);
    shader->handle = handle;
    shader->type = type;
    return handle;
}

shaderc_compilation_result_t zest_ValidateShader(const char *shader_code, shaderc_shader_kind type, const char *name, shaderc_compiler_t compiler) {
    shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, shader_code, strlen(shader_code), type, name, "main", NULL);
    return result;
}

zest_bool zest_CompileShader(zest_shader_handle shader_handle, shaderc_compiler_t compiler) {
    zest_shader shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, shader_handle.value);
    shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, shader->shader_code.str, zest_TextLength(&shader->shader_code), shader->type, shader->name.str, "main", NULL);
    if (shaderc_result_get_compilation_status(result) == shaderc_compilation_status_success) {
		ZEST_APPEND_LOG(ZestDevice->log_path.str, "Successfully compiled shader: %s.", shader->name.str);
        zest__update_shader_spv(shader, result);
        return 1;
    }
	const char *error_message = shaderc_result_get_error_message(result);
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Error compiling shader: %s.\n%s", shader->name.str, error_message);
    return 0;
}

zest_shader_handle zest_CreateShaderFromFile(const char *file, const char *name, shaderc_shader_kind type, zest_bool disable_caching, shaderc_compiler_t compiler, shaderc_compile_options_t options) {
    char *shader_code = zest_ReadEntireFile(file, ZEST_TRUE);
    ZEST_ASSERT(shader_code);   //Unable to load the shader code, check the path is valid
    zest_shader_handle shader_handle = zest_CreateShader(shader_code, type, name, ZEST_FALSE, disable_caching, compiler, options);
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, shader_handle.value);
    zest_SetText(&shader->file_path, file);
    zest_vec_free(shader_code);
    return shader_handle;
}

zest_bool zest_ReloadShader(zest_shader_handle shader_handle) {
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, shader_handle.value);
    ZEST_ASSERT(zest_TextLength(&shader->file_path));    //The shader must have a file path set.
    char *shader_code = zest_ReadEntireFile(shader->file_path.str, ZEST_TRUE);
    if (!shader_code) {
        return 0;
    }
    zest_SetText(&shader->shader_code, shader_code);
    return 1;
}

zest_shader_handle zest_CreateShader(const char *shader_code, shaderc_shader_kind type, const char *name, zest_bool format_code, zest_bool disable_caching, shaderc_compiler_t compiler, shaderc_compile_options_t options) {
    ZEST_ASSERT(name);     //You must give the shader a name
    zest_text_t shader_name = { 0 };
    if (zest_TextSize(&ZestRenderer->shader_path_prefix)) {
        zest_SetTextf(&shader_name, "%s%s", ZestRenderer->shader_path_prefix, name);
    }
    else {
        zest_SetTextf(&shader_name, "%s", name);
    }
    zest_shader_handle shader_handle = zest__new_shader(type);
    zest_shader shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, shader_handle.value);
    shader->name = shader_name;
    if (!disable_caching && ZestApp->create_info.flags & zest_init_flag_cache_shaders) {
        shader->spv = zest_ReadEntireFile(shader->name.str, ZEST_FALSE);
        if (shader->spv) {
            shader->spv_size = zest_vec_size(shader->spv);
			zest_SetText(&shader->shader_code, shader_code);
            if (format_code != 0) {
                zest__format_shader_code(&shader->shader_code);
            }
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Loaded shader %s from cache.", name);
            return shader_handle;
        }
    }
    
    if(!compiler) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Was unable to load the shader from the cached shaders location and compiler is disabled, so cannot go any further with shader %s", name);
        return shader_handle;
    }

	zest_SetText(&shader->shader_code, shader_code);
	if (format_code != 0) {
		zest__format_shader_code(&shader->shader_code);
	}
    shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, shader->shader_code.str, zest_TextLength(&shader->shader_code), type, name, "main", options );

    if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {
		ZEST_APPEND_LOG(ZestDevice->log_path.str, "Shader compilation failed: %s, %s", name, shaderc_result_get_error_message(result));
		ZEST_PRINT("Shader compilation failed: %s, %s", name, shaderc_result_get_error_message(result));
        shaderc_result_release(result);
        zest_FreeShader(shader_handle);
        ZEST_ASSERT(0); //There's a bug in this shader that needs fixing. You can check the log file for the error message
    }

    zest_uint spv_size = (zest_uint)shaderc_result_get_length(result);
    const char *spv_binary = shaderc_result_get_bytes(result);
    zest_vec_resize(shader->spv, spv_size);
    memcpy(shader->spv, spv_binary, spv_size);
    shader->spv_size = spv_size;
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Compiled shader %s and added to renderer shaders.", name);
    shaderc_result_release(result);
    if (!disable_caching && ZestApp->create_info.flags & zest_init_flag_cache_shaders) {
        zest__cache_shader(shader);
    }
    return shader_handle;
}

void zest__cache_shader(zest_shader shader) {
    zest__create_folder(ZestRenderer->shader_path_prefix.str);
    FILE *shader_file = zest__open_file(shader->name.str, "wb");
    if (shader_file == NULL) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Failed to open file for writing: %s", shader->name.str);
        return;
    }
    size_t written = fwrite(shader->spv, 1, shader->spv_size, shader_file);
    if (written != shader->spv_size) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Failed to write entire shader to file: %s", shader->name.str);
        fclose(shader_file);
    }
    fclose(shader_file);
}

zest_shader_handle zest_CreateShaderSPVMemory(const unsigned char *shader_code, zest_uint spv_length, const char *name, shaderc_shader_kind type) {
    ZEST_ASSERT(shader_code);   //No shader code!
    ZEST_ASSERT(name);     //You must give the shader a name
    zest_text_t shader_name = { 0 };
    if (zest_TextSize(&ZestRenderer->shader_path_prefix)) {
        zest_SetTextf(&shader_name, "%s%s", ZestRenderer->shader_path_prefix, name);
    } else {
        zest_SetTextf(&shader_name, "%s", name);
    }
    zest_shader_handle shader_handle = zest__new_shader(type);
    zest_shader shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, shader_handle.value);
    zest_vec_resize(shader->spv, spv_length);
    memcpy(shader->spv, shader_code, spv_length);
    shader->spv_size = spv_length;
    zest_FreeText(&shader_name);
    return shader_handle;
}

void zest__update_shader_spv(zest_shader shader, shaderc_compilation_result_t result) {
    zest_uint spv_size = (zest_uint)shaderc_result_get_length(result);
    const char *spv_binary = shaderc_result_get_bytes(result);
    shader->spv_size = spv_size;
    zest_vec_resize(shader->spv, spv_size);
    memcpy(shader->spv, spv_binary, spv_size);
}

zest_shader_handle zest_AddShaderFromSPVFile(const char *filename, shaderc_shader_kind type) {
    ZEST_ASSERT(filename);     //You must give the shader a name
    zest_shader_handle shader_handle = zest__new_shader(type);
    zest_shader shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, shader_handle.value);
    shader->spv = zest_ReadEntireFile(filename, ZEST_FALSE);
    ZEST_ASSERT(shader->spv);   //File not found, could not load this shader!
    shader->spv_size = zest_vec_size(shader->spv);
	zest_SetText(&shader->name, filename);
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Loaded shader %s and added to renderer shaders.", filename);
	return shader_handle;
}

zest_shader_handle zest_AddShaderFromSPVMemory(const char *name, const void *buffer, zest_uint size, shaderc_shader_kind type) {
    ZEST_ASSERT(name);     //You must give the shader a name
    ZEST_ASSERT(!strstr(name, "/"));    //name must not contain /, the shader will be prefixed with the cache folder automatically
    if (buffer && size) {
		zest_shader_handle shader_handle = zest__new_shader(type);
		zest_shader shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, shader_handle.value);
		if (zest_TextSize(&ZestRenderer->shader_path_prefix)) {
			zest_SetTextf(&shader->name, "%s%s", ZestRenderer->shader_path_prefix, name);
		}
		else {
			zest_SetTextf(&shader->name, "%s", name);
		}
		zest_vec_resize(shader->spv, size);
		memcpy(shader->spv, buffer, size);
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Read shader %s from memory and added to renderer shaders.", name);
        shader->spv_size = size;
        return shader_handle;
    }
    return (zest_shader_handle){ 0 };
}

void zest_AddShader(zest_shader_handle shader_handle, const char *name) {
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, shader_handle.value);
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
}

void zest_FreeShader(zest_shader_handle shader_handle) {
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, shader_handle.value);
    zest_FreeText(&shader->name);
    zest_FreeText(&shader->shader_code);
    zest_FreeText(&shader->file_path);
    if (shader->spv) {
        zest_vec_free(shader->spv);
    }
    zest__remove_store_resource(&ZestRenderer->shaders, shader_handle.value);
}

zest_uint zest_GetDescriptorSetsForBinding(zest_shader_resources_handle handle, VkDescriptorSet **draw_sets) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(&ZestRenderer->shader_resources, handle.value);
    zest_vec_foreach(set_index, shader_resources->backend->sets[ZEST_FIF]) {
        zest_descriptor_set set = shader_resources->backend->sets[ZEST_FIF][set_index];
        ZEST_ASSERT_HANDLE(set);     //Not a valid descriptor set in the shader resource. Make sure all frame in flights are set
        zest_vec_push(*draw_sets, set->backend->vk_descriptor_set);
    }
    return zest_vec_size(*draw_sets);
}

zest_uint zest_GetLayerVertexDescriptorIndex(zest_layer_handle layer_handle, bool last_frame) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
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

zest_pipeline_template zest_BeginPipelineTemplate(const char* name) {
    zest_pipeline_template pipeline_template = ZEST__NEW(zest_pipeline_template);
    *pipeline_template = (zest_pipeline_template_t){ 0 };
    pipeline_template->magic = zest_INIT_MAGIC(zest_struct_type_pipeline_template);
    zest_SetText(&pipeline_template->name, name);
    pipeline_template->no_vertex_input = ZEST_FALSE;
    pipeline_template->attributeDescriptions = 0;
    pipeline_template->bindingDescriptions = 0;
    zest_SetText(&pipeline_template->fragShaderFunctionName, "main");
    zest_SetText(&pipeline_template->vertShaderFunctionName, "main");
    zest_vec_push(pipeline_template->dynamicStates, VK_DYNAMIC_STATE_VIEWPORT);
    zest_vec_push(pipeline_template->dynamicStates, VK_DYNAMIC_STATE_SCISSOR);
    pipeline_template->scissor.offset.x = 0;
    pipeline_template->scissor.offset.y = 0;
    pipeline_template->scissor.extent = (VkExtent2D){ (zest_uint)zest_GetSwapChainExtent().width, (zest_uint)zest_GetSwapChainExtent().height };
    pipeline_template->colorBlendAttachment = zest_AlphaBlendState();

    pipeline_template->vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    pipeline_template->vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_template->vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

    pipeline_template->fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_template->fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

    pipeline_template->inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipeline_template->inputAssembly.primitiveRestartEnable = VK_FALSE;
    pipeline_template->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipeline_template->inputAssembly.flags = 0;

    pipeline_template->viewport.x = 0.0f;
    pipeline_template->viewport.y = 0.0f;
    pipeline_template->viewport.width = (float)pipeline_template->scissor.extent.width;
    pipeline_template->viewport.height = (float)pipeline_template->scissor.extent.height;
    pipeline_template->viewport.minDepth = 0.0f;
    pipeline_template->viewport.maxDepth = 1.0f;

    pipeline_template->scissor = pipeline_template->scissor;

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

    pipeline_template->viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    pipeline_template->viewportState.viewportCount = 1;
    pipeline_template->viewportState.pViewports = &pipeline_template->viewport;
    pipeline_template->viewportState.scissorCount = 1;
    pipeline_template->viewportState.pScissors = &pipeline_template->scissor;

    pipeline_template->colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pipeline_template->colorBlending.logicOpEnable = VK_FALSE;
    pipeline_template->colorBlending.logicOp = VK_LOGIC_OP_COPY;
    pipeline_template->colorBlending.attachmentCount = 1;
    pipeline_template->colorBlending.pAttachments = &pipeline_template->colorBlendAttachment;
    pipeline_template->colorBlending.blendConstants[0] = 0.0f;
    pipeline_template->colorBlending.blendConstants[1] = 0.0f;
    pipeline_template->colorBlending.blendConstants[2] = 0.0f;
    pipeline_template->colorBlending.blendConstants[3] = 0.0f;

    pipeline_template->dynamicState.flags = 0;

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

void zest_AddVertexAttribute(zest_pipeline_template pipeline_template, zest_uint binding, zest_uint location, VkFormat format, zest_uint offset) {
    zest_vec_push(pipeline_template->attributeDescriptions, zest_CreateVertexInputDescription(binding, location, format, offset));
}

zest_key zest_Hash(const void* input, zest_ull length, zest_ull seed) { 
    zest_hasher_t hasher;
    zest__hash_initialise(&hasher, seed); 
    zest__hasher_add(&hasher, input, length); 
    return (zest_key)zest__get_hash(&hasher); 
}

zest_pipeline zest_PipelineWithTemplate(zest_pipeline_template pipeline_template, const zest_frame_graph_context context) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);  //Must be called within a frame graph
    if (zest_vec_size(pipeline_template->descriptorSetLayouts) == 0) {
        ZEST_PRINT("ERROR: You're trying to build a pipeline (%s) that has no descriptor set layouts configured. You can add descriptor layouts when building the pipeline with zest_AddPipelineTemplateDescriptorLayout.", pipeline_template->name.str);
        return NULL;
    }
    pipeline_template->multisampling.rasterizationSamples = (VkSampleCountFlagBits)context->render_pass->backend->sample_count;
    zest_key pipeline_key = (zest_key)pipeline_template;
    zest_cached_pipeline_key_t cached_pipeline = { pipeline_key, context->render_pass->backend->vk_render_pass };
    zest_key cached_pipeline_key = zest_Hash(&cached_pipeline, sizeof(cached_pipeline), ZEST_HASH_SEED);
    if (zest_map_valid_key(ZestRenderer->cached_pipelines, cached_pipeline_key)) {
		return *zest_map_at_key(ZestRenderer->cached_pipelines, cached_pipeline_key); 
    }
    zest_pipeline pipeline = 0;
    ZestRenderer->backend->last_result = zest__cache_pipeline(pipeline_template, context->render_pass, cached_pipeline_key, &pipeline);
	return pipeline;
}

zest_extent2d_t zest_GetSwapChainExtent() { return ZestRenderer->main_swapchain->size; }
zest_extent2d_t zest_GetWindowExtent(void) { return ZestRenderer->window_extent; }
zest_uint zest_SwapChainWidth(void) { return (zest_uint)ZestRenderer->main_swapchain->size.width; }
zest_uint zest_SwapChainHeight(void) { return (zest_uint)ZestRenderer->main_swapchain->size.height; }
float zest_SwapChainWidthf(void) { return (float)ZestRenderer->main_swapchain->size.width; }
float zest_SwapChainHeightf(void) { return (float)ZestRenderer->main_swapchain->size.height; }
zest_uint zest_ScreenWidth() { return ZestRenderer->main_window->window_width; }
zest_uint zest_ScreenHeight() { return ZestRenderer->main_window->window_height; }
float zest_ScreenWidthf() { return (float)ZestRenderer->main_window->window_width; }
float zest_ScreenHeightf() { return (float)ZestRenderer->main_window->window_height; }
float zest_MouseXf() { return (float)ZestApp->mouse_x; }
float zest_MouseYf() { return (float)ZestApp->mouse_y; }
bool zest_MouseDown(zest_mouse_button button) { return (button & ZestApp->mouse_button) > 0; }
bool zest_MouseHit(zest_mouse_button button) { return (button & ZestApp->mouse_hit) > 0; }
float zest_DPIScale(void) { return ZestRenderer->dpi_scale; }
void zest_SetDPIScale(float scale) { ZestRenderer->dpi_scale = scale; }
zest_uint zest_FPS() { return ZestApp->last_fps; }
float zest_FPSf() { return (float)ZestApp->last_fps; }
zest_window zest_AllocateWindow() { zest_window window; window = ZEST__NEW(zest_window); memset(window, 0, sizeof(zest_window_t)); window->magic = zest_INIT_MAGIC(zest_struct_type_window); window->backend = ZEST__NEW(zest_window_backend); *window->backend = (zest_window_backend_t){ 0 }; return window; }
void zest_WaitForIdleDevice() { vkDeviceWaitIdle(ZestDevice->backend->logical_device); }
void zest_MaybeQuit(zest_bool condition) { ZestApp->flags |= condition != 0 ? zest_app_flag_quit_application : 0; }
void zest__hash_initialise(zest_hasher_t* hasher, zest_ull seed) { hasher->state[0] = seed + zest__PRIME1 + zest__PRIME2; hasher->state[1] = seed + zest__PRIME2; hasher->state[2] = seed; hasher->state[3] = seed - zest__PRIME1; hasher->buffer_size = 0; hasher->total_length = 0; }
void zest_GetMouseSpeed(double* x, double* y) {
    double ellapsed_in_seconds = (double)ZestApp->current_elapsed / ZEST_MICROSECS_SECOND;
    *x = ZestApp->mouse_delta_x * ellapsed_in_seconds;
    *y = ZestApp->mouse_delta_y * ellapsed_in_seconds;
}

VkDescriptorBufferInfo* zest_GetUniformBufferInfo(zest_uniform_buffer_handle handle) {
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(&ZestRenderer->uniform_buffers, handle.value);
    return &uniform_buffer->backend->descriptor_info[ZEST_FIF];
}

VkDescriptorSetLayout zest_vk_GetUniformBufferLayout(zest_uniform_buffer_handle handle) {
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(&ZestRenderer->uniform_buffers, handle.value);
    zest_set_layout layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, uniform_buffer->set_layout.value);
    return layout->backend->vk_layout;
}

zest_set_layout_handle zest_GetUniformBufferLayout(zest_uniform_buffer_handle handle) {
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(&ZestRenderer->uniform_buffers, handle.value);
    return uniform_buffer->set_layout;
}

VkDescriptorSet zest_vk_GetUniformBufferSet(zest_uniform_buffer_handle handle) {
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(&ZestRenderer->uniform_buffers, handle.value);
    return uniform_buffer->descriptor_set[ZEST_FIF]->backend->vk_descriptor_set;
}

zest_descriptor_set zest_GetUniformBufferSet(zest_uniform_buffer_handle handle) {
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(&ZestRenderer->uniform_buffers, handle.value);
    return uniform_buffer->descriptor_set[ZEST_FIF];
}

zest_descriptor_set zest_GetFIFUniformBufferSet(zest_uniform_buffer_handle handle, zest_uint fif) {
    ZEST_ASSERT(fif < ZEST_MAX_FIF);
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(&ZestRenderer->uniform_buffers, handle.value);
    return uniform_buffer->descriptor_set[fif];
}

VkDescriptorSet zest_vk_GetFIFUniformBufferSet(zest_uniform_buffer_handle handle, zest_uint fif) {
    ZEST_ASSERT(fif < ZEST_MAX_FIF);
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(&ZestRenderer->uniform_buffers, handle.value);
    return uniform_buffer->descriptor_set[fif]->backend->vk_descriptor_set;
}

void* zest_GetUniformBufferData(zest_uniform_buffer_handle handle) {
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(&ZestRenderer->uniform_buffers, handle.value);
    return uniform_buffer->buffer[ZEST_FIF]->data;
}

zest_uint zest_GetBufferDescriptorIndex(zest_buffer buffer) {
    ZEST_ASSERT(buffer);  //Not a valid buffer handle
    return buffer->array_index;
}

void *zest_BufferData(zest_buffer buffer) {
    ZEST_ASSERT(buffer);  //Not a valid buffer handle
    return buffer->data;
}

zest_size zest_BufferSize(zest_buffer buffer) {
    ZEST_ASSERT(buffer);  //Not a valid buffer handle
    return buffer->size;
}

zest_size zest_BufferMemoryInUse(zest_buffer buffer) {
    ZEST_ASSERT(buffer);  //Not a valid buffer handle
    return buffer->memory_in_use;
}

void zest_SetBufferMemoryInUse(zest_buffer buffer, zest_size size) {
    ZEST_ASSERT(buffer);  //Not a valid buffer handle
    ZEST_ASSERT(size <= buffer->size);  //You're trying to set to the memory in use to a size larger than the buffer size
    buffer->memory_in_use = size;
}


void zest_ResetCompute(zest_compute_handle compute_handle) {
    zest_compute compute = (zest_compute)zest__get_store_resource_checked(&ZestRenderer->compute_pipelines, compute_handle.value);
    compute->fif = (compute->fif + 1) % ZEST_MAX_FIF;
}

void zest_FreeCompute(zest_compute_handle compute_handle) {
    zest_compute compute = (zest_compute)zest__get_store_resource_checked(&ZestRenderer->compute_pipelines, compute_handle.value);
    zest_vec_push(ZestRenderer->deferred_resource_freeing_list.resources[ZEST_FIF], compute);
}

int zest_ComputeConditionAlwaysTrue(zest_compute_handle layer) {
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
    return ZestRenderer->main_window->window_handle;
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
    ZEST_PRINT_FUNCTION;
    VkFenceCreateInfo fence_info = { 0 };
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = 0;
    VkFence fence;
	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_fence);
    vkCreateFence(ZestDevice->backend->logical_device, &fence_info, &ZestDevice->backend->allocation_callbacks, &fence);
    return fence;
}

zest_bool zest_CheckFence(VkFence fence) {
    ZEST_PRINT_FUNCTION;
    if (vkGetFenceStatus(ZestDevice->backend->logical_device, fence) == VK_SUCCESS) {
        vkDestroyFence(ZestDevice->backend->logical_device, fence, &ZestDevice->backend->allocation_callbacks);
        return ZEST_TRUE;
    }
    return ZEST_FALSE;
}

void zest_WaitForFence(VkFence fence) {
    ZEST_PRINT_FUNCTION;
    vkWaitForFences(ZestDevice->backend->logical_device, 1, &fence, VK_TRUE, UINT64_MAX);
}

void zest_DestroyFence(VkFence fence) {
    ZEST_PRINT_FUNCTION;
    zest_WaitForFence(fence);
    vkDestroyFence(ZestDevice->backend->logical_device, fence, &ZestDevice->backend->allocation_callbacks);
}

void zest_ResetEvent(VkEvent e) {
    ZEST_PRINT_FUNCTION;
    vkResetEvent(ZestDevice->backend->logical_device, e);
}

zest_bool zest_IsMemoryPropertyAvailable(VkMemoryPropertyFlags flags) {
    ZEST_PRINT_FUNCTION;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(ZestDevice->backend->physical_device, &memoryProperties);

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
        if (buffer_type_large.alignment == 0) {
            return ZEST_FALSE;
        }
        int heap_index_large = ZestDevice->backend->memory_properties.memoryTypes[buffer_type_large.memory_type_index].heapIndex;
        if (ZestDevice->backend->memory_properties.memoryHeaps[heap_index_large].size > minimum_size) {
            return ZEST_TRUE;
        }
    }
    return ZEST_FALSE;
}

float zest_LinearToSRGB(float value) {
    return powf(value, 2.2f);
}

zest_bool zest_IsValidComputeHandle(zest_compute_handle compute_handle) {
    zest_compute compute = (zest_compute)zest__get_store_resource(&ZestRenderer->compute_pipelines, compute_handle.value);
    return ZEST_VALID_HANDLE(compute);
}

zest_bool zest_IsValidImageHandle(zest_image_handle image_handle) {
    zest_image image = (zest_image)zest__get_store_resource(&ZestRenderer->images, image_handle.value);
    return ZEST_VALID_HANDLE(image);
}

zest_uint zest__grow_capacity(void* T, zest_uint size) {
    zest_uint new_capacity = T ? (size + size / 2) : 8;
    return new_capacity > size ? new_capacity : size;
}

void* zest__vec_reserve(void* T, zest_uint unit_size, zest_uint new_capacity) {
    if (T && new_capacity <= zest__vec_header(T)->capacity) {
        return T;
    }
    void* new_data = ZEST__REALLOCATE((T ? zest__vec_header(T) : T), new_capacity * unit_size + zest__VEC_HEADER_OVERHEAD);
    if (!T) memset(new_data, 0, zest__VEC_HEADER_OVERHEAD);
    T = ((char*)new_data + zest__VEC_HEADER_OVERHEAD);
    zest_vec *header = (zest_vec *)new_data;
    header->id = ZestDevice->vector_id++;
    if (header->id == 237) {
        int d = 0;
    }
    header->magic = zest_INIT_MAGIC(zest_struct_type_vector);
    header->capacity = new_capacity;
    return T;
}

void *zest__vec_linear_reserve(zloc_linear_allocator_t *allocator, void *T, zest_uint unit_size, zest_uint new_capacity) {
    if (T && new_capacity <= zest__vec_header(T)->capacity)
        return T;
    void* new_data = zloc_LinearAllocation(allocator, new_capacity * unit_size + zest__VEC_HEADER_OVERHEAD);
    if (!T) memset(new_data, 0, zest__VEC_HEADER_OVERHEAD);
    T = ((char*)new_data + zest__VEC_HEADER_OVERHEAD);
    zest_vec *header = (zest_vec *)new_data;
    header->magic = zest_INIT_MAGIC(zest_struct_type_vector);
    header->capacity = new_capacity;
    return T;
}

void zest__bucket_array_init(zest_bucket_array_t *array, zest_uint element_size, zest_uint bucket_capacity) {
    array->buckets = NULL; // zest_vec will handle initialization on first push
    array->bucket_capacity = bucket_capacity;
    array->current_size = 0;
    array->element_size = element_size;
}

void zest__bucket_array_free(zest_bucket_array_t *array) {
    if (!array || !array->buckets) return;
    // Free each individual bucket
    zest_vec_foreach(i, array->buckets) {
        ZEST__FREE(array->buckets[i]);
    }
    // Free the zest_vec that holds the bucket pointers
    zest_vec_free(array->buckets);
    array->buckets = NULL;
    array->current_size = 0;
}

void *zest__bucket_array_get(zest_bucket_array_t *array, zest_uint index) {
    if (!array || index >= array->current_size) {
        return NULL;
    }
    zest_uint bucket_index = index / array->bucket_capacity;
    zest_uint index_in_bucket = index % array->bucket_capacity;
    return (void *)((char *)array->buckets[bucket_index] + index_in_bucket * array->element_size);
}

void *zest__bucket_array_add(zest_bucket_array_t *array) {
    ZEST_ASSERT(array);
    // If the array is empty or the last bucket is full, allocate a new one.
    if (zest_vec_empty(array->buckets) || (array->current_size % array->bucket_capacity == 0)) {
        void *new_bucket = ZEST__ALLOCATE(array->element_size * array->bucket_capacity);
        zest_vec_push(array->buckets, new_bucket);
    }

    // Get the pointer to the new element's location
    zest_uint bucket_index = (array->current_size) / array->bucket_capacity;
    zest_uint index_in_bucket = (array->current_size) % array->bucket_capacity;
    void *new_element = (void *)((char *)array->buckets[bucket_index] + index_in_bucket * array->element_size);

    array->current_size++;
    return new_element;
}

inline void *zest__bucket_array_linear_add(zloc_linear_allocator_t *allocator, zest_bucket_array_t *array) {
    ZEST_ASSERT(array);
    // If the array is empty or the last bucket is full, allocate a new one.
    if (zest_vec_empty(array->buckets) || (array->current_size % array->bucket_capacity == 0)) {
		void *new_bucket = zloc_LinearAllocation(allocator, array->element_size * array->bucket_capacity);
        zest_vec_linear_push(allocator, array->buckets, new_bucket);
    }

    // Get the pointer to the new element's location
    zest_uint bucket_index = (array->current_size) / array->bucket_capacity;
    zest_uint index_in_bucket = (array->current_size) % array->bucket_capacity;
    void *new_element = (void *)((char *)array->buckets[bucket_index] + index_in_bucket * array->element_size);

    array->current_size++;
    return new_element;
}

void zest__free_store(zest_resource_store_t *store) { 
    if (store->data) { 
		ZEST__FREE(store->data); 
        zest_vec_free(store->generations);
        zest_vec_free(store->free_slots);
        *store = (zest_resource_store_t){ 0 };
    } 
}

void zest__reserve_store(zest_resource_store_t *store, zest_uint new_capacity) {
    ZEST_ASSERT(store->struct_size);	//Must assign a value to struct size
    if (new_capacity <= store->capacity) return;
    void *new_data;
    if (store->alignment != 0) {
        new_data = ZEST__ALLOCATE_ALIGNED((size_t)new_capacity * store->struct_size, store->alignment);
    } else {
        new_data = ZEST__REALLOCATE(store->data, (size_t)new_capacity * store->struct_size);
    }
    ZEST_ASSERT(new_data);    //Unable to allocate memory. todo: better handling
    if (store->data) {
        memcpy(new_data, store->data, (size_t)store->current_size * store->struct_size);
        ZEST__FREE(store->data);
    }
    store->data = new_data;
    store->capacity = new_capacity;
}

void zest__clear_store(zest_resource_store_t *store) {
    if (store->data) {
        store->current_size = 0;
    }
}

zest_uint zest__grow_store_capacity(zest_resource_store_t *store, zest_uint size) {
    zest_uint new_capacity = store->capacity ? (store->capacity + store->capacity / 2) : 8;
    return new_capacity > size ? new_capacity : size;
}

void zest__resize_store(zest_resource_store_t *store, zest_uint new_size) {
    if (new_size > store->capacity) zest__reserve_store(store, zest__grow_store_capacity(store, new_size));
    store->current_size = new_size;
}

void zest__resize_bytes_store(zest_resource_store_t *store, zest_uint new_size) {
    if (new_size > store->capacity) zest__reserve_store(store, zest__grow_store_capacity(store, new_size));
    store->current_size = new_size;
}

zest_uint zest__size_in_bytes_store(zest_resource_store_t *store) {
    return store->current_size * store->struct_size;
}

zest_handle zest__add_store_resource(zest_resource_store_t *store) {
        zest_uint index;                                                                                                           
        zest_uint generation;                                                                                                      
        if (zest_vec_size(store->free_slots) > 0) {
			index = zest_vec_back(store->free_slots);                                                                          
			zest_vec_pop(store->free_slots);                                                                                  
			generation = ++store->generations[index]; 
        } else {
			index = store->current_size;                                                                                             
			zest_vec_push(store->generations, 1);                                                                             
			generation = 1;                                                                       
			zest__resize_store(store, zest_vec_size(store->generations));
            char *position = (char *)store->data + index * store->struct_size;
        }                                                                                                                         
		return (zest_handle) { ZEST_CREATE_HANDLE(generation, index) };                                      
}

void zest__remove_store_resource(zest_resource_store_t *store, zest_handle handle) {
	zest_uint index = ZEST_HANDLE_INDEX(handle);                                                                     
	zest_vec_push(store->free_slots, index);                                                                               
}                                                                                                                             


void zest__initialise_store(zest_resource_store_t *store, zest_uint struct_size) {
    ZEST_ASSERT(!store->data);   //Must be an empty store
    store->struct_size = struct_size;
    store->alignment = 16;
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

void zest__add_report(zest_report_category category, int line_number, const char *file_name, const char *entry, ...) {
    zest_text_t message = { 0 };
    va_list args;
    va_start(args, entry);
    zest_SetTextfv(&message, entry, args);
    va_end(args);
    zest_key report_hash = zest_Hash(message.str, zest_TextSize(&message), 0);
    if (zest_map_valid_key(ZestRenderer->reports, report_hash)) {
        zest_report_t *report = zest_map_at_key(ZestRenderer->reports, report_hash);
        report->count++;
        zest_FreeText(&message);
    } else {
        zest_report_t report = { 0 };
        report.count = 1;
        report.line_number = line_number;
        report.file_name = file_name;
        report.message = message;
        report.category = category;
        zest_map_insert_key(ZestRenderer->reports, report_hash, report);
    }
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
    ZestRenderer->builtin_shaders.sprite_frag        = zest_CreateShader(zest_shader_sprite_frag, shaderc_fragment_shader, "image_frag.spv", 1, 0, compiler, 0);
    ZestRenderer->builtin_shaders.sprite_vert        = zest_CreateShader(zest_shader_sprite_vert, shaderc_vertex_shader, "sprite_vert.spv", 1, 0, compiler, 0);
    ZestRenderer->builtin_shaders.font_frag          = zest_CreateShader(zest_shader_font_frag, shaderc_fragment_shader, "font_frag.spv", 1, 0, compiler, 0);
    ZestRenderer->builtin_shaders.mesh_vert          = zest_CreateShader(zest_shader_mesh_vert, shaderc_vertex_shader, "mesh_vert.spv", 1, 0, compiler, 0);
    ZestRenderer->builtin_shaders.mesh_instance_vert = zest_CreateShader(zest_shader_mesh_instance_vert, shaderc_vertex_shader, "mesh_instance_vert.spv", 1, 0, compiler, 0);
    ZestRenderer->builtin_shaders.mesh_instance_frag = zest_CreateShader(zest_shader_mesh_instance_frag, shaderc_fragment_shader, "mesh_instance_frag.spv", 1, 0, compiler, 0);
    ZestRenderer->builtin_shaders.swap_vert          = zest_CreateShader(zest_shader_swap_vert, shaderc_vertex_shader, "swap_vert.spv", 1, 0, compiler, 0);
    ZestRenderer->builtin_shaders.swap_frag          = zest_CreateShader(zest_shader_swap_frag, shaderc_fragment_shader, "swap_frag.spv", 1, 0, compiler, 0);
    if (compiler) {
        shaderc_compiler_release(compiler);
    }
}

zest_sampler_handle zest_CreateSampler(zest_sampler_info_t *info) {
    zest_sampler_handle sampler_handle = (zest_sampler_handle){ zest__add_store_resource(&ZestRenderer->samplers) };
    zest_sampler sampler = (zest_sampler)zest__get_store_resource(&ZestRenderer->samplers, sampler_handle.value);
    sampler->magic = zest_INIT_MAGIC(zest_struct_type_sampler);
    sampler->handle = sampler_handle;
    sampler->create_info = *info;
    sampler->backend = zest__vk_new_sampler_backend();

    if (zest__create_sampler(sampler)) {
        return sampler_handle;
    }

    zest__cleanup_sampler(sampler);
    return (zest_sampler_handle){ 0 };
}

zest_sampler_info_t zest_CreateSamplerInfo() {
    zest_sampler_info_t sampler_info = { 0 };
    sampler_info.mag_filter = zest_filter_linear;
    sampler_info.min_filter = zest_filter_linear;
    sampler_info.address_mode_u = zest_sampler_address_mode_clamp_to_edge;
    sampler_info.address_mode_v = zest_sampler_address_mode_clamp_to_edge;
    sampler_info.address_mode_w = zest_sampler_address_mode_clamp_to_edge;
    sampler_info.anisotropy_enable = ZEST_FALSE;
    sampler_info.max_anisotropy = 1.f;
    sampler_info.compare_enable = ZEST_FALSE;
    sampler_info.compare_op = zest_compare_op_always;
    sampler_info.mipmap_mode = zest_mipmap_mode_linear;
    sampler_info.border_color = zest_border_color_float_transparent_black;
    sampler_info.mip_lod_bias = 0.f;
    sampler_info.min_lod = 0.0f;
    sampler_info.max_lod = 1.0f;
    return sampler_info;
}

zest_sampler_handle zest_CreateSamplerForImage(zest_image_handle image_handle) {
    zest_image image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, image_handle.value);
    zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
    sampler_info.max_lod = (float)image->info.mip_levels - 1.f;
    zest_sampler_handle sampler = zest_CreateSampler(&sampler_info);
    return sampler;
}

zest_sampler_info_t zest_CreateMippedSamplerInfo(zest_uint mip_levels) {
    zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
    sampler_info.mipmap_mode = zest_mipmap_mode_nearest;
    sampler_info.max_lod = (float)mip_levels - 1.f;
    return sampler_info;
}

void zest__create_debug_layout_and_pool(zest_uint max_texture_count) {
	zest_set_layout_builder_t builder = zest_BeginSetLayoutBuilder();
    zest_AddLayoutBuilderBinding(&builder, (zest_descriptor_binding_desc_t){ 0, zest_descriptor_type_sampled_image, 1, zest_shader_fragment_stage } );
	ZestRenderer->texture_debug_layout = zest_FinishDescriptorSetLayout(&builder, "Texture Debug Layout");
	zest_CreateDescriptorPoolForLayout(ZestRenderer->texture_debug_layout, max_texture_count);
}

void zest__prepare_standard_pipelines() {

    //2d sprite rendering
    /*
    ZestRenderer->pipeline_templates.sprites = zest_BeginPipelineTemplate("pipeline_2d_sprites");
    zest_pipeline_template sprites = ZestRenderer->pipeline_templates.sprites;
    zest_AddVertexInputBindingDescription(sprites, 0, sizeof(zest_sprite_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);

    zest_AddVertexAttribute(sprites, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_sprite_instance_t, uv));                  // Location 0: UV coords
    zest_AddVertexAttribute(sprites, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_sprite_instance_t, position_rotation));   // Location 1: Instance Position and rotation
    zest_AddVertexAttribute(sprites, 2, VK_FORMAT_R16G16B16A16_SSCALED, offsetof(zest_sprite_instance_t, size_handle));        // Location 2: Size of the sprite in pixels
    zest_AddVertexAttribute(sprites, 3, VK_FORMAT_R16G16_SNORM, offsetof(zest_sprite_instance_t, alignment));                  // Location 3: Alignment
    zest_AddVertexAttribute(sprites, 4, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_sprite_instance_t, color));                    // Location 4: Instance Color
    zest_AddVertexAttribute(sprites, 5, VK_FORMAT_R32_UINT, offsetof(zest_sprite_instance_t, intensity_texture_array));        // Location 5: Instance Parameters

    zest_SetPipelineShaders(sprites, ZestRenderer->builtin_shaders.sprite_frag, ZestRenderer->builtin_shaders.sprite_frag);
    zest_SetPipelinePushConstantRange(sprites, sizeof(zest_push_constants_t), VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
	zest_AddPipelineDescriptorLayout(sprites, zest_vk_GetUniformBufferLayout(ZestRenderer->uniform_buffer));
	zest_AddPipelineDescriptorLayout(sprites, zest_vk_GetGlobalBindlessLayout());
    zest_SetPipelineBlend(sprites, zest_PreMultiplyBlendState());
    zest_SetPipelineDepthTest(sprites, false, true);
    zest_EndPipelineTemplate(sprites);
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

    //Font Texture
    ZestRenderer->pipeline_templates.fonts = zest_CopyPipelineTemplate("pipeline_fonts", sprites);
    zest_pipeline_template fonts = ZestRenderer->pipeline_templates.fonts;
    zest_SetPipelinePushConstantRange(fonts, sizeof(zest_push_constants_t), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    zest_SetPipelineVertShader(fonts, ZestRenderer->builtin_shaders.sprite_vert);
    zest_SetPipelineFragShader(fonts, ZestRenderer->builtin_shaders.font_frag);
    zest_AddPipelineDescriptorLayout(fonts, ZestRenderer->uniform_buffer->set_layout->backend->vk_layout);
    zest_AddPipelineDescriptorLayout(fonts, ZestRenderer->global_bindless_set_layout->backend->vk_layout);
    zest_EndPipelineTemplate(fonts);
    fonts->depthStencil.depthWriteEnable = VK_FALSE;
    fonts->depthStencil.depthTestEnable = VK_FALSE;
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Font pipeline");

    //General mesh drawing
    ZestRenderer->pipeline_templates.mesh = zest_BeginPipelineTemplate("pipeline_mesh");
    zest_pipeline_template mesh = ZestRenderer->pipeline_templates.mesh;
    mesh->scissor.offset.x = 0;
    mesh->scissor.offset.y = 0;
    zest_SetPipelinePushConstantRange(mesh, sizeof(zest_push_constants_t), VK_SHADER_STAGE_VERTEX_BIT);
    zest_AddVertexInputBindingDescription(mesh, 0, sizeof(zest_textured_vertex_t), VK_VERTEX_INPUT_RATE_VERTEX);
    VkVertexInputAttributeDescription* zest_vertex_input_attributes = 0;
    zest_vec_push(zest_vertex_input_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(zest_textured_vertex_t, pos)));        // Location 0: Position
    zest_vec_push(zest_vertex_input_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32_SFLOAT, offsetof(zest_textured_vertex_t, intensity)));        // Location 1: Alpha/Intensity
    zest_vec_push(zest_vertex_input_attributes, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(zest_textured_vertex_t, uv)));            // Location 2: Position
    zest_vec_push(zest_vertex_input_attributes, zest_CreateVertexInputDescription(0, 3, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_textured_vertex_t, color)));        // Location 3: Color
    zest_vec_push(zest_vertex_input_attributes, zest_CreateVertexInputDescription(0, 4, VK_FORMAT_R32_UINT, offsetof(zest_textured_vertex_t, parameters)));        // Location 4: Parameters

    mesh->attributeDescriptions = zest_vertex_input_attributes;
    zest_SetPipelineVertShader(mesh, ZestRenderer->builtin_shaders.mesh_vert);
    zest_SetPipelineFragShader(mesh, ZestRenderer->builtin_shaders.sprite_frag);

    mesh->scissor.extent = zest_GetSwapChainExtent();
    mesh->flags |= zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild;
    zest_SetPipelineDepthTest(mesh, true, true);
    mesh->colorBlendAttachment = zest_PreMultiplyBlendState();
    zest_EndPipelineTemplate(mesh);

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Mesh pipeline");

    //Instanced mesh drawing for drawing primatives
    ZestRenderer->pipeline_templates.instanced_mesh = zest_BeginPipelineTemplate("pipeline_mesh_instance");
    zest_pipeline_template instanced_mesh = ZestRenderer->pipeline_templates.instanced_mesh;
    instanced_mesh->scissor.offset.x = 0;
    instanced_mesh->scissor.offset.y = 0;
    zest_SetPipelinePushConstantRange(instanced_mesh, sizeof(zest_push_constants_t), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    zest_AddVertexInputBindingDescription(instanced_mesh, 0, sizeof(zest_vertex_t), VK_VERTEX_INPUT_RATE_VERTEX);
    zest_AddVertexInputBindingDescription(instanced_mesh, 1, sizeof(zest_mesh_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);
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

    instanced_mesh->attributeDescriptions = imesh_input_attributes;
    zest_SetPipelineVertShader(instanced_mesh, ZestRenderer->builtin_shaders.mesh_instance_vert);
    zest_SetPipelineFragShader(instanced_mesh, ZestRenderer->builtin_shaders.mesh_instance_frag);

	zest_AddPipelineDescriptorLayout(instanced_mesh, zest_vk_GetUniformBufferLayout(ZestRenderer->uniform_buffer));
	//zest_AddPipelineDescriptorLayout(instanced_mesh, zest_vk_GetGlobalBindlessLayout());

    instanced_mesh->scissor.extent = zest_GetSwapChainExtent();
    instanced_mesh->flags |= zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild;
    instanced_mesh->colorBlendAttachment = zest_AlphaBlendState();
    zest_SetPipelineDepthTest(instanced_mesh, true, true);
    zest_EndPipelineTemplate(instanced_mesh);

    instanced_mesh->rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    instanced_mesh->rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    instanced_mesh->rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    instanced_mesh->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Instance Mesh pipeline");
    */

    //Final Render Pipelines
    ZestRenderer->pipeline_templates.swap = zest_BeginPipelineTemplate("pipeline_swap_chain");
    zest_pipeline_template swap = ZestRenderer->pipeline_templates.swap;
    zest_SetPipelinePushConstantRange(swap, sizeof(zest_vec2), VK_SHADER_STAGE_VERTEX_BIT);
    zest_SetPipelineBlend(swap, zest_PreMultiplyBlendStateForSwap());
    swap->no_vertex_input = ZEST_TRUE;
    zest_SetPipelineVertShader(swap, ZestRenderer->builtin_shaders.swap_vert);
    zest_SetPipelineFragShader(swap, ZestRenderer->builtin_shaders.swap_frag);
    swap->uniforms = 0;
    swap->flags = zest_pipeline_set_flag_is_render_target_pipeline;

    zest_ClearPipelineDescriptorLayouts(swap);
    zest_EndPipelineTemplate(swap);
    swap->depthStencil.depthWriteEnable = VK_FALSE;
    swap->depthStencil.depthTestEnable = VK_FALSE;

    swap->colorBlendAttachment = zest_PreMultiplyBlendStateForSwap();

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

// --End Renderer functions

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

void zest__create_transient_buffer(zest_resource_node node) {
    node->storage_buffer = zest_CreateBuffer(node->buffer_desc.size, &node->buffer_desc.buffer_info, 0);
}

zest_bool zest__create_transient_image(zest_resource_node resource) {
    zest_image image = &resource->image;
    image->magic = zest_INIT_MAGIC(zest_struct_type_image);
    image->backend = zest__new_frame_graph_image_backend(ZestRenderer->frame_graph_allocator[ZEST_FIF]);
    for (int i = 0; i != zest_max_global_binding_number; ++i) {
        image->bindless_index[i] = ZEST_INVALID;
    }
    image->info.flags |= zest_image_flag_transient;
    image->info.aspect_flags = zest__determine_aspect_flag(resource->image.info.format);
    image->info.mip_levels = resource->image.info.mip_levels > 0 ? resource->image.info.mip_levels : 1;
    if (ZEST__FLAGGED(image->info.flags, zest_image_flag_cubemap)) {
        ZEST_ASSERT(image->info.layer_count > 0 && image->info.layer_count % 6 == 0); // Cubemap must have layers in multiples of 6!
    }
    if (ZEST__FLAGGED(resource->image.info.flags, zest_image_flag_generate_mipmaps) && image->info.mip_levels == 1) {
        image->info.mip_levels = (zest_uint)floor(log2(ZEST__MAX(resource->image.info.extent.width, resource->image.info.extent.height))) + 1;
    }
    if (!zest__create_image(image, image->info.layer_count, zest_sample_count_1_bit, resource->image.info.flags)) {
		ZestPlatform->cleanup_image_backend(image);
        return FALSE;
    }
    image->info.layout = resource->image_layout = zest_image_layout_undefined;
    zest_image_view_type view_type = zest__get_image_view_type(image);
    image->default_view = zest__create_image_view_backend(image, view_type, image->info.mip_levels, 0, 0, image->info.layer_count, ZestRenderer->frame_graph_allocator[ZEST_FIF]);
    resource->view = image->default_view;

    return ZEST_TRUE;
}

VkResult zest__create_temporary_image(zest_uint width, zest_uint height, zest_uint mipLevels, VkSampleCountFlagBits sample_count, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* memory) {
    ZEST_PRINT_FUNCTION;
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
    image_info.samples = sample_count;

	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_image);
    ZEST_VK_ASSERT_RESULT(vkCreateImage(ZestDevice->backend->logical_device, &image_info, &ZestDevice->backend->allocation_callbacks, image));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(ZestDevice->backend->logical_device, *image, &memRequirements);

    VkMemoryAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memRequirements.size;
    alloc_info.memoryTypeIndex = zest_find_memory_type(memRequirements.memoryTypeBits, properties);

	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_allocate_memory_image);
    ZEST_VK_ASSERT_RESULT(vkAllocateMemory(ZestDevice->backend->logical_device, &alloc_info, &ZestDevice->backend->allocation_callbacks, memory));

    vkBindImageMemory(ZestDevice->backend->logical_device, *image, *memory, 0);

    return VK_SUCCESS;
}

VkImageMemoryBarrier zest__create_base_image_memory_barrier(VkImage image) {
    VkImageMemoryBarrier barrier = { 0 };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = ZEST_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = ZEST_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    return barrier;
}

void zest__place_image_barrier(VkCommandBuffer command_buffer, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage, VkImageMemoryBarrier *barrier) {
    ZEST_PRINT_FUNCTION;
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

VkFormat zest__find_depth_format() {
    VkFormat depth_formats[5] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM_S8_UINT };
    return zest__find_supported_format(depth_formats, 5, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat zest__find_supported_format(VkFormat* candidates, zest_uint candidates_count, VkImageTiling tiling, VkFormatFeatureFlags features) {
    ZEST_PRINT_FUNCTION;
    for (int i = 0; i != candidates_count; ++i) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(ZestDevice->backend->physical_device, candidates[i], &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return candidates[i];
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return candidates[i];
        }
    }

    return 0;
}

void zest__insert_image_memory_barrier(VkCommandBuffer cmdbuffer, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange) {
    ZEST_PRINT_FUNCTION;
    VkImageMemoryBarrier image_memory_barrier = { 0 };
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.srcQueueFamilyIndex = ZEST_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = ZEST_QUEUE_FAMILY_IGNORED;
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
        .title = "Zest Window",
        .memory_pool_size = zloc__MEGABYTE(64),
        .frame_graph_allocator_size = zloc__MEGABYTE(1),
        .shader_path_prefix = "spv/",
        .log_path = NULL,
        .screen_width = 1280,
        .screen_height = 768,
        .screen_x = 0,
        .screen_y = 50,
        .virtual_width = 1280,
        .virtual_height = 768,
        .fence_wait_timeout_ms = 250,
        .max_fence_timeout_ms = ZEST_SECONDS_IN_MILLISECONDS(10),
        .thread_count = zest_GetDefaultThreadCount(),
        .color_format = zest_format_b8g8r8a8_unorm,
        .flags = zest_init_flag_enable_vsync | zest_init_flag_cache_shaders,
        .destroy_window_callback = zest__destroy_window_callback,
        .get_window_size_callback = zest__get_window_size_callback,
        .poll_events_callback = zest__os_poll_events,
        .add_platform_extensions_callback = zest__os_add_platform_extensions,
        .create_window_callback = zest__os_create_window,
        .create_window_surface_callback = zest__os_create_window_surface,
        .set_window_mode_callback = zest__os_set_window_mode,
        .set_window_size_callback = zest__os_set_window_size,
        .maximum_textures = 1024,
        .bindless_combined_sampler_2d_count = 256,
        .bindless_combined_sampler_array_count = 64,
        .bindless_combined_sampler_cube_count = 64,
        .bindless_combined_sampler_3d_count = 64,
        .bindless_sampler_count = 256,
        .bindless_sampled_image_count = 256,
        .bindless_storage_buffer_count = 256,
        .bindless_storage_image_count = 256,
    };
    return create_info;
}

zest_create_info_t zest_CreateInfoWithValidationLayers(zest_validation_flags flags) {
    zest_create_info_t create_info = zest_CreateInfo();
    ZEST__FLAG(create_info.flags, zest_init_flag_enable_validation_layers);
    if (flags & zest_validation_flag_enable_sync) {
        create_info.flags |= zest_init_flag_enable_validation_layers_with_sync;
    }
    if (flags & zest_validation_flag_best_practices) {
        create_info.flags |= zest_init_flag_enable_validation_layers_with_best_practices;
    }
    return create_info;
}

 void zest_SetWindowMode(zest_window window, zest_window_mode mode) {
    ZEST_ASSERT_HANDLE(window);  //Not a valid window handle.
    ZestRenderer->set_window_mode_callback(window, mode);
}

 void zest_SetWindowSize(zest_window window, zest_uint width, zest_uint height) {
    ZEST_ASSERT_HANDLE(window);  //Not a valid window handle.
    ZestRenderer->set_window_size_callback(window, width, height);
}

void zest_UpdateWindowSize(zest_window window, zest_uint width, zest_uint height) {
    ZEST_ASSERT_HANDLE(window);  //Not a valid window handle.
    window->window_width = width;
    window->window_height = height;
}

 void zest_SetWindowHandle(zest_window window, void *handle) {
    ZEST_ASSERT_HANDLE(window);  //Not a valid window handle.
    window->window_handle = handle;
}

 zest_window zest_GetCurrentWindow() {
     return ZestRenderer->main_window;
}

 VkSurfaceKHR zest_WindowSurface() {
     return ZestRenderer->main_window->backend->surface;
}

void zest_SetWindowSurface(VkSurfaceKHR surface) {
     ZestRenderer->main_window->backend->surface = surface;
}

 void zest_CloseWindow(zest_window window) {
	 ZestRenderer->destroy_window_callback(window, ZestApp->user_data);
	 ZEST__FREE(window);
}

 void zest_CleanupWindow(zest_window window) {
     zest__cleanup_window_backend(window);
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

// --frame graph functions
zest_frame_graph zest__new_frame_graph(const char *name) {
    zest_frame_graph frame_graph = zloc_LinearAllocation(ZestRenderer->frame_graph_allocator[ZEST_FIF], sizeof(zest_frame_graph_t));
    *frame_graph = (zest_frame_graph_t){ 0 };
    frame_graph->magic = zest_INIT_MAGIC(zest_struct_type_frame_graph);
    frame_graph->context.magic = zest_INIT_MAGIC(zest_struct_type_frame_graph_context);
    frame_graph->name = name;
    frame_graph->bindless_layout = ZestRenderer->global_bindless_set_layout;
    frame_graph->bindless_set = ZestRenderer->global_set;
    zest_bucket_array_init(&frame_graph->resources, zest_resource_node_t, 8);
    zest_bucket_array_init(&frame_graph->potential_passes, zest_pass_node_t, 8);
    zest_vec_linear_push(ZestRenderer->frame_graph_allocator[ZEST_FIF], ZestRenderer->frame_graphs, frame_graph);
    return frame_graph;
}

zest_key zest__hash_frame_graph_cache_key(zest_frame_graph_cache_key_t *cache_key) {
    zest_key key = zest_Hash(&cache_key->auto_state, sizeof(zest_frame_graph_auto_state_t), ZEST_HASH_SEED);
    if (cache_key->user_state && cache_key->user_state_size) {
        key += zest_Hash(cache_key->user_state, cache_key->user_state_size, ZEST_HASH_SEED);
    }
    return key;
}

void zest__cache_frame_graph(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (!frame_graph->cache_key) return;    //Only cache if there's a key
    if (frame_graph->error_status) return; //Don't cache a frame graph that had errors
    zest_cached_frame_graph_t new_cached_graph = {
        zloc_PromoteLinearBlock(ZestDevice->allocator, ZestRenderer->frame_graph_allocator[ZEST_FIF], ZestRenderer->frame_graph_allocator[ZEST_FIF]->current_offset),
        frame_graph
    };
    ZEST__FLAG(frame_graph->flags, zest_frame_graph_is_cached);
	void *frame_graph_linear_memory = ZEST__ALLOCATE(zloc__MEGABYTE(1));
	ZestRenderer->frame_graph_allocator[ZEST_FIF] = zloc_InitialiseLinearAllocator(frame_graph_linear_memory, zloc__MEGABYTE(1));
    if (zest_map_valid_key(ZestRenderer->cached_frame_graphs, frame_graph->cache_key)) {
        zest_cached_frame_graph_t *cached_graph = zest_map_at_key(ZestRenderer->cached_frame_graphs, frame_graph->cache_key);
        ZEST__FREE(cached_graph->memory);
        *cached_graph = new_cached_graph;
    } else {
        zest_map_insert_key(ZestRenderer->cached_frame_graphs, frame_graph->cache_key, new_cached_graph);
    }
}

zest_image_view zest__swapchain_resource_provider(zest_resource_node resource) {
    return resource->swapchain->views[resource->swapchain->current_image_frame];
}

zest_buffer zest__instance_layer_resource_provider(zest_resource_node resource) {
    zest_layer layer = (zest_layer)resource->user_data;
    layer->vertex_buffer_node = resource;
    zest__end_instance_instructions(layer); //Make sure the staging buffer memory in use is up to date
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
		zest_size layer_size = layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
        if (layer_size == 0) return NULL;
        layer->fif = ZEST_FIF;
        layer->dirty[ZEST_FIF] = 1;
        zest_buffer_resource_info_t buffer_desc = { 0 };
        buffer_desc.size = layer_size;
        buffer_desc.usage_hints = zest_resource_usage_hint_vertex_buffer;
    } else {
        zest_uint fif = ZEST__FLAGGED(layer->flags, zest_layer_flag_use_prev_fif) ? layer->prev_fif : layer->fif;
        resource->bindless_index[0] = layer->memory_refs[fif].device_vertex_data->array_index;
        return layer->memory_refs[fif].device_vertex_data;
    }
    return NULL;
}

zest_frame_graph zest__get_cached_frame_graph(zest_key key) {
    if (zest_map_valid_key(ZestRenderer->cached_frame_graphs, key)) {
        zest_cached_frame_graph_t *cached_graph = zest_map_at_key(ZestRenderer->cached_frame_graphs, key);
        return cached_graph->frame_graph;
    }
    return NULL;
}

zest_frame_graph_cache_key_t zest_InitialiseCacheKey(zest_swapchain swapchain, const void *user_state, zest_size user_state_size) {
    zest_frame_graph_cache_key_t key = { 0 };
    key.auto_state.render_format = swapchain->format;
    key.auto_state.render_width = (zest_uint)swapchain->size.width;
    key.auto_state.render_height = (zest_uint)swapchain->size.height;
    key.user_state = user_state;
    key.user_state_size = user_state_size;
    return key;
}

bool zest_BeginFrameGraph(const char *name, zest_frame_graph_cache_key_t *cache_key) {
	ZEST_ASSERT(ZEST__NOT_FLAGGED(ZestRenderer->flags, zest_renderer_flag_building_frame_graph));  //frame graph already being built. You cannot build a frame graph within another begin frame graph process.

    zest_key key = 0;
    if (cache_key) {
        key = zest__hash_frame_graph_cache_key(cache_key);
        zest_frame_graph cached_graph = zest__get_cached_frame_graph(key);
        if (cached_graph) {
            ZestRenderer->current_frame_graph = cached_graph;
            return cached_graph;
        }
    }
    zest_frame_graph frame_graph = zest__new_frame_graph(name);
    frame_graph->cache_key = key;

    frame_graph->semaphores = ZestPlatform->get_frame_graph_semaphores(name);
    frame_graph->context.backend = zest__new_frame_graph_context_backend();

	ZEST__UNFLAG(frame_graph->flags, zest_frame_graph_expecting_swap_chain_usage);
	ZEST__FLAG(ZestRenderer->flags, zest_renderer_flag_building_frame_graph);
    ZestRenderer->current_frame_graph = frame_graph;
    return true;
}

bool zest_BeginFrameGraphSwapchain(zest_swapchain swapchain, const char *name, zest_frame_graph_cache_key_t *cache_key) {
    if (zest_BeginFrameGraph(name, cache_key)) {
        zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
        if (ZEST__FLAGGED(frame_graph->flags, zest_frame_graph_is_cached)) {
			if (!ZestPlatform->acquire_swapchain_image(swapchain)) {
				ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_building_frame_graph);
				ZEST_PRINT("Unable to acquire the swap chain!");
				return false;
			}
			ZEST__FLAG(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_acquired);
			ZEST__UNFLAG(swapchain->flags, zest_swapchain_flag_was_recreated);
			zest__execute_frame_graph(ZEST_FALSE);

			if (ZEST_VALID_HANDLE(frame_graph->swapchain) && ZEST__FLAGGED(frame_graph->flags, zest_frame_graph_present_after_execute)) {
				ZestPlatform->present_frame(frame_graph->swapchain);
			} 
            return false;
        }
		ZEST__FLAG(ZestRenderer->current_frame_graph->flags, zest_frame_graph_expecting_swap_chain_usage);
    } else {
        return false;
    }
    ZEST_ASSERT(ZEST__NOT_FLAGGED(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_acquired));    //Swap chain was already acquired. Only one frame graph can output to the swap chain per frame.
    if (!ZestPlatform->acquire_swapchain_image(swapchain)) {
        ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_building_frame_graph);
        ZEST_PRINT("Unable to acquire the swap chain!");
        return false;
    }
	ZEST__FLAG(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_acquired);
	ZEST__UNFLAG(swapchain->flags, zest_swapchain_flag_was_recreated);
	zest__import_swapchain_resource(swapchain);
    return true;
}

void zest_ForceFrameGraphOnGraphicsQueue() {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);      //This function should only be called immediately after BeginRenderGraph/BeginRenderToScreen
    ZEST__FLAG(ZestRenderer->current_frame_graph->flags, zest_frame_graph_force_on_graphics_queue);
}

zest_bool zest__is_stage_compatible_with_qfi(zest_pipeline_stage_flags stages_to_check, zest_device_queue_type queue_family_capabilities) {
    if (stages_to_check == zest_pipeline_stage_none) { // Or just == 0
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
        case zest_pipeline_stage_top_of_pipe_bit:
        case zest_pipeline_stage_bottom_of_pipe_bit:
        case zest_pipeline_stage_host_bit: // Host access can always be synchronized against
        case zest_pipeline_stage_all_commands_bit: // Valid, but its scope is limited by queue caps
            stage_is_compatible = ZEST_TRUE;
            break;

            // Stages requiring GRAPHICS_BIT capability
        case zest_pipeline_stage_vertex_input_bit:
        case zest_pipeline_stage_vertex_shader_bit:
        case zest_pipeline_stage_tessellation_control_shader_bit:
        case zest_pipeline_stage_tessellation_evaluation_shader_bit:
        case zest_pipeline_stage_geometry_shader_bit:
        case zest_pipeline_stage_fragment_shader_bit:
        case zest_pipeline_stage_early_fragment_tests_bit:
        case zest_pipeline_stage_late_fragment_tests_bit:
        case zest_pipeline_stage_color_attachment_output_bit:
        case zest_pipeline_stage_all_graphics_bit:
        //Can add more extensions here if needed
            if (queue_family_capabilities & zest_queue_graphics) {
                stage_is_compatible = ZEST_TRUE;
            }
            break;

		// Stage requiring COMPUTE_BIT capability
        case zest_pipeline_stage_compute_shader_bit:
            if (queue_family_capabilities & zest_queue_compute) {
                stage_is_compatible = ZEST_TRUE;
            }
            break;

		// Stage DRAW_INDIRECT can be used by Graphics or Compute
        case zest_pipeline_stage_draw_indirect_bit:
            if (queue_family_capabilities & (zest_queue_graphics | zest_queue_compute)) {
                stage_is_compatible = ZEST_TRUE;
            }
            break;

		// Stage requiring TRANSFER_BIT capability (also often implied by Graphics/Compute)
        case zest_pipeline_stage_transfer_bit:
            if (queue_family_capabilities & (zest_queue_graphics | zest_queue_compute | zest_queue_transfer)) {
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

void zest__free_transient_resource(zest_resource_node resource) {
    if (resource->type == zest_resource_type_buffer) {
        zest_FreeBuffer(resource->storage_buffer);
        resource->storage_buffer = 0;
    } else if (resource->type & zest_resource_type_is_image_or_depth) {
        zest_FreeBuffer(resource->image.buffer);
        resource->image.buffer = 0;
    }
}

zest_bool zest__create_transient_resource(zest_resource_node resource) {
    if (resource->type & zest_resource_type_is_image) {
        if (!zest__create_transient_image(resource)) {
            return ZEST_FALSE;
        }

        resource->image_layout = zest_image_layout_undefined;
        resource->access_mask = 0;
        resource->last_stage_mask = zest_pipeline_stage_top_of_pipe_bit;
    } else if (resource->type == zest_resource_type_buffer && resource->storage_buffer == NULL) {
        zest__create_transient_buffer(resource);
        if (!resource->storage_buffer) {
            return ZEST_FALSE;
        }
        resource->access_mask = 0;
        resource->last_stage_mask = zest_pipeline_stage_top_of_pipe_bit;
    }
    return ZEST_TRUE;
}

void zest__add_image_barriers(zest_frame_graph frame_graph, zloc_linear_allocator_t *allocator, zest_resource_node resource, zest_execution_barriers_t *barriers, 
                              zest_resource_state_t *current_state, zest_resource_state_t *prev_state, zest_resource_state_t *next_state) {
	zest_resource_usage_t *current_usage = &current_state->usage;
	zest_image_layout current_usage_layout = current_usage->image_layout;
    if (!prev_state) {
        //This is the first state of the resource
        //If there's no previous state then we need to see if a barrier is needed to transition from the resource
        //start state. We put this in the acquire barrier as it needs to be put in place before the pass is executed.
        zest_uint src_queue_family_index = resource->current_queue_family_index;
        zest_uint dst_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
        if (src_queue_family_index == ZEST_QUEUE_FAMILY_IGNORED) {
            if (resource->image_layout != current_usage_layout ||
                (resource->access_mask & zest_access_write_bits_general) &&
                (current_usage->access_mask & zest_access_read_bits_general)) {
                ZestPlatform->add_frame_graph_image_barrier(resource, barriers, true,
                    resource->access_mask, current_usage->access_mask,
                    resource->image_layout, current_usage_layout,
                    src_queue_family_index, dst_queue_family_index,
                    resource->last_stage_mask, current_usage->stage_mask);
            }
        } else if (ZEST__NOT_FLAGGED(frame_graph->flags, zest_frame_graph_force_on_graphics_queue)) {
            //This resource already belongs to a queue which means that it's an imported resource
            //If the frame graph is on the graphics queue only then there's no need to acquire from a prior release.
            ZEST_ASSERT(ZEST__FLAGGED(resource->flags, zest_resource_node_flag_imported));
            dst_queue_family_index = current_state->queue_family_index;
            if (src_queue_family_index != ZEST_QUEUE_FAMILY_IGNORED) {
                ZestPlatform->add_frame_graph_image_barrier(resource, barriers, true,
                    resource->access_mask, current_usage->access_mask,
                    resource->image_layout, current_usage_layout,
                    src_queue_family_index, dst_queue_family_index,
                    resource->last_stage_mask, current_usage->stage_mask);
            }
        }
        bool needs_releasing = false;
    } else {
        //There is a previous state, do we need to acquire the resource from a different queue?
        zest_uint src_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
        zest_uint dst_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
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
            ZestPlatform->add_frame_graph_image_barrier(resource, barriers, true,
                zest_access_none, current_usage->access_mask,
                prev_usage->image_layout, current_usage_layout,
                src_queue_family_index, dst_queue_family_index,
                zest_pipeline_stage_top_of_pipe_bit, current_usage->stage_mask);
            //Make sure that the batch that this resource is in (at this point) has the correct wait
            //stage to wait on.
            zest_submission_batch_t *batch = zest__get_submission_batch(current_state->submission_id);
            batch->queue_wait_stages |= zest_pipeline_stage_top_of_pipe_bit;
        }
    }
    if (next_state) {
        //If the resource has a state after this one then we can check if a barrier is needed to transition and/or
        //release to another queue family
        zest_uint src_queue_family_index = current_state->queue_family_index;
        zest_uint dst_queue_family_index = next_state->queue_family_index;
        zest_resource_usage_t *next_usage = &next_state->usage;
		zest_image_layout next_usage_layout = next_usage->image_layout;
        bool needs_releasing = false;
        if (next_state && next_state->queue_family_index != current_state->queue_family_index) {
            //The next state is in a different queue so the resource needs to be released to that queue
            src_queue_family_index = current_state->queue_family_index;
            dst_queue_family_index = next_state->queue_family_index;
            needs_releasing = true;
        }
        if ((needs_releasing || current_usage->image_layout != next_usage->image_layout ||
            (current_usage->access_mask & zest_access_write_bits_general) &&
            (next_usage->access_mask & zest_access_read_bits_general))) {
            zest_pipeline_stage_flags dst_stage = zest_pipeline_stage_bottom_of_pipe_bit;
            if (needs_releasing) {
                current_state->was_released = true;
            } else {
                dst_stage = next_state->usage.stage_mask;
                current_state->was_released = false;
            }
            ZestPlatform->add_frame_graph_image_barrier(resource, barriers, false,
                current_usage->access_mask, zest_access_none,
                current_usage->image_layout, next_usage_layout,
                src_queue_family_index, dst_queue_family_index,
                current_usage->stage_mask, dst_stage);
        }
    } else if (resource->flags & zest_resource_node_flag_imported
        && current_state->queue_family_index != ZestDevice->transfer_queue_family_index
        && ZEST__NOT_FLAGGED(frame_graph->flags, zest_frame_graph_force_on_graphics_queue)) {
        //Maybe we add something for images here if it's needed.
    }
}

zest_bool zest__detect_cyclic_recursion(zest_frame_graph frame_graph, zest_pass_node pass_node) {
    pass_node->visit_state = zest_pass_node_visiting;
    zest_map_foreach(i, pass_node->outputs) {
        zest_resource_usage_t *output_usage = &pass_node->outputs.data[i];
        zest_resource_node output_resource = output_usage->resource_node;
        zest_bucket_array_foreach(p_idx, frame_graph->potential_passes) {
            zest_pass_node dependent_pass = zest_bucket_array_get(&frame_graph->potential_passes, zest_pass_node_t, p_idx);
            if (zest_map_valid_name(dependent_pass->inputs, output_resource->name)) {
                if (dependent_pass == pass_node) {
                    continue;
                }
                if (dependent_pass->visit_state == zest_pass_node_visiting) {
                    return ZEST_TRUE; // Signal that a cycle was found
                }
                if (dependent_pass->visit_state == zest_pass_node_unvisited) {
                    if (zest__detect_cyclic_recursion(frame_graph, dependent_pass)) {
                        return ZEST_TRUE; // Propagate the cycle detection signal up
                    }
                }
            }
        }
    }
    pass_node->visit_state = zest_pass_node_visited;
    return ZEST_FALSE; // No cycle found from this pass
}

/*
frame graph compiler index:

Main compile phases:
    - Initial cull of passes with no outputs or execution callbacks
    - Put non culled passes into the ungrouped_passes list
    - Calculate dependencies on these ungrouped passes to build the graph
    - Build an ordered execution list from this
    - Group passes together that can be based on shared output, input dependencies and queue families
    - Build "resource journeys" for each resource for future barrier creation.
    - Calculate the lifetimes of resources for transient memory aliasing
    - Group these passes in to submission batches based on queue families
    - Create semaphores for the submission batches
    - Create lists ready for execution for barrier and transient buffer/image craeation during the execution stage
    - Process exexution list and create or fetch cached render passes.

[Check_unused_resources_and_passes]
[Set_producers_and_consumers]
[Set_adjacency_list]
[Process_dependency_queue]
[Resource_journeys]
[Create_command_batches]
[Calculate_lifetime_of_resources]
[Create_semaphores]
[Plan_transient_buffers]
[Plan_resource_barriers]
[Process_compiled_execution_order]
[Create_memory_barriers_for_inputs]
[Create_memory_barriers_for_outputs]
[Create_render_passes]
*/
zest_frame_graph zest__compile_frame_graph() {

    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen

    zloc_linear_allocator_t *allocator = ZestRenderer->frame_graph_allocator[ZEST_FIF];

    zest_bucket_array_foreach(i, frame_graph->potential_passes) {
        zest_pass_node pass_node = zest_bucket_array_get(&frame_graph->potential_passes, zest_pass_node_t, i);
        if (pass_node->visit_state == zest_pass_node_unvisited) {
            if (zest__detect_cyclic_recursion(frame_graph, pass_node)) {
                ZEST__REPORT(zest_report_cyclic_dependency, zest_message_cyclic_dependency, frame_graph->name, pass_node->name);
                ZEST__FLAG(frame_graph->error_status, zest_fgs_cyclic_dependency);
                return frame_graph;
            }
        }
    }

    //Check_unused_resources_and_passes and cull them if necessary
    bool a_pass_was_culled = 0;
    zest_bucket_array_foreach(i, frame_graph->potential_passes) {
        zest_pass_node pass_node = zest_bucket_array_get(&frame_graph->potential_passes, zest_pass_node_t, i);
        if (zest_map_size(pass_node->outputs) == 0 || pass_node->execution_callback.callback == 0) {
			//Cull passes that have no output and/or no execution callback and reduce any input resource reference counts 
            zest_map_foreach(j, pass_node->inputs) {
                pass_node->inputs.data[j].resource_node->reference_count--;
            }
            ZEST__FLAG(pass_node->flags, zest_pass_flag_culled);
            a_pass_was_culled = true;
            if (zest_map_size(pass_node->outputs) == 0) {
                ZEST__REPORT(zest_report_pass_culled, zest_message_pass_culled, pass_node->name);
            } else {
                ZEST__REPORT(zest_report_pass_culled, zest_message_pass_culled_no_work, pass_node->name);
            }
        } else {
            zest_uint reference_counts = 0;
            zest_map_foreach(output_index, pass_node->outputs) {
                zest_resource_node resource = pass_node->outputs.data[output_index].resource_node;
                if (ZEST__FLAGGED(resource->flags, zest_resource_node_flag_essential_output)) {
					reference_counts += 1;
                    continue;
                }
				reference_counts += resource->reference_count;
            }
            if (reference_counts == 0) {
                //Outputs are not consumed by any other pass so this pass can be culled
				zest_map_foreach(j, pass_node->inputs) {
					pass_node->inputs.data[j].resource_node->reference_count--;
				}
                ZEST__FLAG(pass_node->flags, zest_pass_flag_culled);
				a_pass_was_culled = true;
				ZEST__REPORT(zest_report_pass_culled, zest_message_pass_culled_not_consumed, pass_node->name);
            }
        }
    }

    //Keep iterating and culling passes whose output resources have 0 reference counts and keep reducing input resource reference counts
    while (a_pass_was_culled) {
        a_pass_was_culled = false;
		zest_bucket_array_foreach(i, frame_graph->potential_passes) {
			zest_pass_node pass_node = zest_bucket_array_get(&frame_graph->potential_passes, zest_pass_node_t, i);
            if (ZEST__NOT_FLAGGED(pass_node->flags, zest_pass_flag_culled) && ZEST__NOT_FLAGGED(pass_node->flags, zest_pass_flag_do_not_cull)) {
                bool all_outputs_non_referenced = true;
                zest_map_foreach(j, pass_node->outputs) {
                    if (pass_node->outputs.data[j].resource_node->reference_count > 0) {
                        all_outputs_non_referenced = false;
                        break;
                    }
                }
                if (all_outputs_non_referenced) {
                    ZEST__FLAG(pass_node->flags, zest_pass_flag_culled);
                    a_pass_was_culled = true;
                    zest_map_foreach(j, pass_node->inputs) {
						ZEST__MAYBE_REPORT(pass_node->inputs.data[j].resource_node->reference_count == 0, zest_report_invalid_reference_counts, zest_message_pass_culled_not_consumed, frame_graph->name);
                        frame_graph->error_status |= zest_fgs_critical_error;
                        return frame_graph;
                        pass_node->inputs.data[j].resource_node->reference_count--;
                    }
					ZEST__REPORT(zest_report_pass_culled, zest_message_pass_culled_not_consumed, pass_node->name);
                }
            }
        }
    }

    bool has_execution_callback = false;

    //Now group the potential passes in to final passes by ignoring any culled passes and grouping together passes that
    //have the same output. We need to be careful here though to make sure that even though 2 passes might share the same output
    //they might have conflicting dependencies. Output keys are generated by hashing the usage options for the resource.
	zest_bucket_array_foreach(i, frame_graph->potential_passes) {
		zest_pass_node pass_node = zest_bucket_array_get(&frame_graph->potential_passes, zest_pass_node_t, i);
        //Ignore culled passes
        if (ZEST__NOT_FLAGGED(pass_node->flags, zest_pass_flag_culled)) {
            //If no entry for the current pass output key exists, create one.
            if (!zest_map_valid_key(frame_graph->final_passes, pass_node->output_key)) {
                zest_pass_group_t pass_group = { 0 };
                pass_group.execution_details.barriers.backend = ZestPlatform->new_execution_barriers_backend(allocator);
                pass_group.queue_info = pass_node->queue_info;
                zest_map_foreach(output_index, pass_node->outputs) {
                    zest_hash_pair pair = pass_node->outputs.map[output_index];
                    zest_resource_usage_t *usage = &pass_node->outputs.data[pair.index];
                    if (!ZEST_VALID_HANDLE(usage->resource_node)) {
                        ZEST__REPORT(zest_fgs_critical_error, zest_message_usage_has_no_resource, frame_graph->name);
						frame_graph->error_status |= zest_fgs_critical_error;
						return frame_graph;
                    }
                    if (usage->resource_node->reference_count > 0 || ZEST__FLAGGED(usage->resource_node->flags, zest_resource_node_flag_essential_output)) {
                        zest_map_insert_linear_key(allocator, pass_group.outputs, pass_node->outputs.map[output_index].key, *usage);
                    }
                }
                zest_map_foreach(input_index, pass_node->inputs) {
                    zest_hash_pair pair = pass_node->inputs.map[input_index];
                    zest_resource_usage_t *usage = &pass_node->inputs.data[pair.index];
                    if (!ZEST_VALID_HANDLE(usage->resource_node)) {
                        ZEST__REPORT(zest_fgs_critical_error, zest_message_usage_has_no_resource, frame_graph->name);
						frame_graph->error_status |= zest_fgs_critical_error;
						return frame_graph;
                    }
                    zest_map_insert_linear_key(allocator, pass_group.inputs, pair.key, *usage);
                }
                if (pass_node->execution_callback.callback) {
                    has_execution_callback = true;
                }
                ZEST__FLAG(pass_group.flags, pass_node->flags);
                zest_vec_linear_push(allocator, pass_group.passes, pass_node);
                zest_map_insert_linear_key(allocator, frame_graph->final_passes, pass_node->output_key, pass_group);
            } else {
                zest_pass_group_t *pass_group = zest_map_at_key(frame_graph->final_passes, pass_node->output_key);
				if (pass_group->queue_info.queue_family_index != pass_node->queue_info.queue_family_index ||
                    pass_group->queue_info.timeline_wait_stage != pass_node->queue_info.timeline_wait_stage) {
					ZEST__REPORT(zest_fgs_critical_error, zest_message_multiple_swapchain_usage, frame_graph->name);
					frame_graph->error_status |= zest_fgs_critical_error;
					return frame_graph;
				}
                if (pass_node->execution_callback.callback) {
                    has_execution_callback = true;
                }
                ZEST__FLAG(pass_group->flags, pass_node->flags);
                zest_vec_linear_push(allocator, pass_group->passes, pass_node);
                zest_map_foreach(input_index, pass_node->inputs) {
                    zest_hash_pair pair = pass_node->inputs.map[input_index];
                    zest_resource_usage_t *usage = &pass_node->inputs.data[pair.index];
                    if (!ZEST_VALID_HANDLE(usage->resource_node)) {
						ZEST__REPORT(zest_fgs_critical_error, zest_message_usage_has_no_resource, frame_graph->name);
						frame_graph->error_status |= zest_fgs_critical_error;
						return frame_graph;
                    }
                    zest_map_insert_linear_key(allocator, pass_group->inputs, pair.key, *usage);
                }
            }
        } else {
            frame_graph->culled_passes_count++;
        }
    }

    zest_uint potential_passes = zest_bucket_array_size(&frame_graph->potential_passes);
    zest_uint final_passes = zest_map_size(frame_graph->final_passes);

    if (!has_execution_callback) {
        zest__set_rg_error_status(frame_graph, zest_fgs_no_work_to_do);
        return frame_graph;
    }

    zest_pass_adjacency_list_t *adjacency_list = { 0 };
    zest_uint *dependency_count = 0;
    zest_vec_linear_resize(allocator, adjacency_list, zest_map_size(frame_graph->final_passes));
    zest_vec_linear_resize(allocator, dependency_count, zest_map_size(frame_graph->final_passes));

    // Set_producers_and_consumers:
    // For each output in a pass, we set it's producer_pass_idx - the pass that writes the output
    // and for each input in a pass we add all of the comuser_pass_idx's that read the inputs
    zest_map_foreach(i, frame_graph->final_passes) {
        zest_pass_group_t *pass_node = &frame_graph->final_passes.data[i];
        dependency_count[i] = 0; // Initialize in-degree
        zest_pass_adjacency_list_t adj_list = { 0 };
        adjacency_list[i] = adj_list;

        switch (pass_node->queue_info.queue_type) {
        case zest_queue_graphics:
            pass_node->queue_info.queue = &ZestDevice->graphics_queue;
            pass_node->queue_info.queue_family_index = ZestDevice->graphics_queue_family_index;
            break;
        case zest_queue_compute:
            pass_node->queue_info.queue = &ZestDevice->compute_queue;
            pass_node->queue_info.queue_family_index = ZestDevice->compute_queue_family_index;
            break;
        case zest_queue_transfer:
            pass_node->queue_info.queue = &ZestDevice->transfer_queue;
            pass_node->queue_info.queue_family_index = ZestDevice->transfer_queue_family_index;
            break;
        }

        zest_map_foreach(j, pass_node->inputs) {
            zest_resource_usage_t *input_usage = &pass_node->inputs.data[j];
            zest_resource_node resource = input_usage->resource_node;
            zest_resource_versions_t *versions = zest_map_at_key(frame_graph->resource_versions, resource->original_id);
            ZEST_ASSERT(versions);  //Bug, this shouldn't be possible, all resources should have a version

            zest_resource_node latest_version = zest_vec_back(versions->resources);
            if (!zest_map_valid_name(pass_node->inputs, latest_version->name)) {
                latest_version->reference_count++;
            }
            input_usage->resource_node = latest_version;

            zest_map_linear_insert(ZestRenderer->frame_graph_allocator[ZEST_FIF], pass_node->inputs, latest_version->name, *input_usage);
            zest_vec_linear_push(allocator, resource->consumer_pass_indices, i); // pass 'i' consumes this
        }

        zest_map_foreach(j, pass_node->outputs) {
            zest_resource_usage_t *output_usage = &pass_node->outputs.data[j];
            zest_resource_node resource = output_usage->resource_node;
            zest_resource_versions_t *versions = zest__maybe_add_resource_version(resource);
            zest_resource_node latest_version = zest_vec_back(versions->resources);
            if (latest_version->id != resource->id) {
                if (resource->type == zest_resource_type_swap_chain_image) {
                    ZEST__REPORT(zest_fgs_critical_error, zest_message_multiple_swapchain_usage, frame_graph->name);
                    frame_graph->error_status |= zest_fgs_critical_error;
					return frame_graph;
                }
			    //Add the versioned alias to the outputs instead
                output_usage->resource_node = latest_version;
                zest_map_linear_insert(ZestRenderer->frame_graph_allocator[ZEST_FIF], pass_node->outputs, latest_version->name, *output_usage);
                //Check if the user already added this as input:
                if (!zest_map_valid_name(pass_node->inputs, resource->name)) {
                    //If not then add the resource as input for correct dependency chain with default usages
                    zest_resource_usage_t input_usage = { 0 };
                    switch (pass_node->queue_info.queue_type) {
                    case zest_queue_graphics:
                        input_usage = zest__configure_image_usage(resource, zest_purpose_color_attachment_read, resource->image.info.format, output_usage->load_op, output_usage->stencil_load_op, output_usage->stage_mask);
                        break;
                    case zest_queue_compute:
                    case zest_queue_transfer:
                        input_usage = zest__configure_image_usage(resource, zest_purpose_storage_image_read, resource->image.info.format, output_usage->load_op, output_usage->stencil_load_op, output_usage->stage_mask);
                        break;
                    }
                    input_usage.store_op = output_usage->store_op;
                    input_usage.stencil_store_op = output_usage->stencil_store_op;
                    input_usage.clear_value = output_usage->clear_value;
                    input_usage.purpose = output_usage->purpose;
                    resource->reference_count++;
                    zest_map_linear_insert(ZestRenderer->frame_graph_allocator[ZEST_FIF], pass_node->inputs, resource->name, input_usage);
                }
            }

            if (output_usage->resource_node->producer_pass_idx != -1 && output_usage->resource_node->producer_pass_idx != i) {
				ZEST__REPORT(zest_fgs_critical_error, zest_message_resource_added_as_ouput_more_than_once, frame_graph->name);
				frame_graph->error_status |= zest_fgs_critical_error;
				return frame_graph;
            }
            output_usage->resource_node->producer_pass_idx = i;
        }

    }

    // Set_adjacency_list
    zest_map_foreach(consumer_idx, frame_graph->final_passes) {
        zest_pass_group_t *consumer_pass = &frame_graph->final_passes.data[consumer_idx];
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
    
    zest_execution_wave_t first_wave = { 0 };
    zest_uint pass_count = 0;

    int *dependency_queue = 0;
    zest_vec_foreach(i, dependency_count) {
        if (dependency_count[i] == 0) {
            zest_pass_group_t *pass = &frame_graph->final_passes.data[i];
            first_wave.queue_bits |= pass->queue_info.queue_type;
            zest_vec_linear_push(allocator, dependency_queue, i);
            zest_vec_linear_push(allocator, first_wave.pass_indices, i);
            pass_count++;
        }
    }
    if (pass_count > 0) {
        zest_vec_linear_push(allocator, frame_graph->execution_waves, first_wave);
    }

    zest_uint current_wave_index = 0;
    while (current_wave_index < zest_vec_size(frame_graph->execution_waves)) {
        zest_execution_wave_t next_wave = { 0 };
        zest_execution_wave_t *current_wave = &frame_graph->execution_waves[current_wave_index];
        next_wave.level = current_wave->level + 1;
        zest_vec_foreach(i, current_wave->pass_indices) {
            int finished_pass_index = current_wave->pass_indices[i];
            zest_vec_foreach(j, adjacency_list[finished_pass_index].pass_indices) {
                int dependent_pass_index = adjacency_list[finished_pass_index].pass_indices[j];
                dependency_count[dependent_pass_index]--;
                if (dependency_count[dependent_pass_index] == 0) {
					zest_pass_group_t *pass = &frame_graph->final_passes.data[dependent_pass_index];
                    next_wave.queue_bits |= pass->queue_info.queue_type;
                    zest_vec_linear_push(allocator, next_wave.pass_indices, dependent_pass_index);
                    pass_count++;
                }
            }
        }
        if (zest_vec_size(next_wave.pass_indices) > 0) {
            zest_vec_push(frame_graph->execution_waves, next_wave);
        }
        current_wave_index++;
    }

    if (zest_vec_size(frame_graph->execution_waves) == 0) {
        //Nothing to send to the GPU!
        return frame_graph;
    }

    //Bug, pass count which is a count of all pass groups added to waves should equal the number of final passes.
    ZEST_ASSERT(pass_count == zest_map_size(frame_graph->final_passes));

    //Create_command_batches
    //We take the waves that we created that identified passes that can run in parallel on separate queues and 
    //organise them into wave submission batches:
    zest_wave_submission_t current_submission = { 0 };

    zest_bool interframe_has_waited[ZEST_QUEUE_COUNT] = { 0 };
    zest_bool interframe_has_signalled[ZEST_QUEUE_COUNT] = { 0 };

    zest_vec_foreach(wave_index, frame_graph->execution_waves) {
        zest_execution_wave_t *wave = &frame_graph->execution_waves[wave_index];
        //If the wave has more than one queue then the passes can run in parallel in separate submission batches
        zest_uint queue_type_count = zloc__count_bits(wave->queue_bits);
        //This should be impossible to hit, if no queue bit
        ZEST_ASSERT(queue_type_count);  //Must be using at lease 1 queue
        if (queue_type_count > 1) {
            if (current_submission.batches[0].magic || current_submission.batches[1].magic || current_submission.batches[2].magic) {
				zest_vec_linear_push(allocator, frame_graph->submissions, current_submission);
				current_submission = (zest_wave_submission_t){ 0 };
            }
            //Create parallel batches
            zest_vec_foreach(pass_index, wave->pass_indices) {
                zest_uint current_pass_index = wave->pass_indices[pass_index];
                zest_pass_group_t *pass = &frame_graph->final_passes.data[current_pass_index];
                zest_uint qi = zloc__scan_reverse(pass->queue_info.queue_type);
                if (!current_submission.batches[qi].magic) {
                    current_submission.batches[qi].magic = zest_INIT_MAGIC(zest_struct_type_wave_submission);
                    current_submission.batches[qi].backend = zest__new_submission_batch_backend();
                    current_submission.batches[qi].queue = pass->queue_info.queue;
                    current_submission.batches[qi].queue_family_index = pass->queue_info.queue_family_index;
                    current_submission.batches[qi].timeline_wait_stage = pass->queue_info.timeline_wait_stage;
                    current_submission.batches[qi].queue_type = pass->queue_info.queue_type;
                    current_submission.batches[qi].need_timeline_wait = interframe_has_waited[qi] ? ZEST_FALSE : ZEST_TRUE;
                    interframe_has_waited[qi] = ZEST_TRUE;
                }
				current_submission.queue_bits |= pass->queue_info.queue_type;
                zest_vec_linear_push(allocator, current_submission.batches[qi].pass_indices, current_pass_index);
            }
            zest_vec_linear_push(allocator, frame_graph->submissions, current_submission);
            current_submission = (zest_wave_submission_t){ 0 };
        } else {
            //Waves that have no parallel submissions 
			zest_uint qi = zloc__scan_reverse(wave->queue_bits);
            if (!current_submission.batches[qi].magic && (current_submission.batches[0].magic || current_submission.batches[1].magic || current_submission.batches[2].magic)) {
                //The queue is different from the last wave, finalise this submission
				zest_vec_linear_push(allocator, frame_graph->submissions, current_submission);
				current_submission = (zest_wave_submission_t){ 0 };
            }
            zest_vec_foreach(pass_index, wave->pass_indices) {
                zest_uint current_pass_index = wave->pass_indices[pass_index];
                zest_pass_group_t *pass = &frame_graph->final_passes.data[current_pass_index];
                if (!current_submission.batches[qi].magic) {
                    current_submission.batches[qi].magic = zest_INIT_MAGIC(zest_struct_type_wave_submission);
                    current_submission.batches[qi].backend = zest__new_submission_batch_backend();
                    current_submission.batches[qi].queue = pass->queue_info.queue;
                    current_submission.batches[qi].queue_family_index = pass->queue_info.queue_family_index;
                    current_submission.batches[qi].timeline_wait_stage = pass->queue_info.timeline_wait_stage;
                    current_submission.batches[qi].queue_type = pass->queue_info.queue_type;
                    current_submission.batches[qi].need_timeline_wait = interframe_has_waited[qi] ? ZEST_FALSE : ZEST_TRUE;
                    interframe_has_waited[qi] = ZEST_TRUE;
                }
				current_submission.queue_bits = pass->queue_info.queue_type;
                zest_vec_linear_push(allocator, current_submission.batches[qi].pass_indices, current_pass_index);
            }
        }
    }

    //Add the last batch that was being processed if it was a sequential one.
    if (current_submission.batches[0].magic || current_submission.batches[1].magic || current_submission.batches[2].magic) {
        zest_vec_linear_push(allocator, frame_graph->submissions, current_submission);
    }

    //[Resource_journeys]
    //Now that the passes have been grouped into wave submissions we can calculate the resource journey and set the
    //first and last usage index for each resource.
    zest_uint execution_order_index = 0;
    zest_vec_foreach(submission_index, frame_graph->submissions) {
        zest_wave_submission_t *current_wave = &frame_graph->submissions[submission_index];
        for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
            if (!current_wave->batches[queue_index].magic) continue;
            zest_submission_batch_t *batch = &current_wave->batches[queue_index];
            zest_vec_foreach(execution_index, batch->pass_indices) {
                zest_uint current_pass_index = batch->pass_indices[execution_index];
                zest_pass_group_t *current_pass = &frame_graph->final_passes.data[current_pass_index];
                current_pass->submission_id = ZEST__MAKE_SUBMISSION_ID(submission_index, execution_index, queue_index);

                zest_vec_linear_push(allocator, frame_graph->pass_execution_order, current_pass);

                zest_batch_key batch_key = { 0 };
                batch_key.current_family_index = current_pass->queue_info.queue_family_index;

                //Calculate_lifetime_of_resources and also create a state for each resource and plot
                //it's journey through the frame graph so that the appropriate barriers and semaphores
                //can be set up
                zest_map_foreach(input_idx, current_pass->inputs) {
                    zest_resource_node resource_node = current_pass->inputs.data[input_idx].resource_node;
                    if (resource_node->aliased_resource) resource_node = resource_node->aliased_resource;
                    if (resource_node) {
                        resource_node->first_usage_pass_idx = ZEST__MIN(resource_node->first_usage_pass_idx, execution_order_index);
                        resource_node->last_usage_pass_idx = ZEST__MAX(resource_node->last_usage_pass_idx, execution_order_index);
                    }
                    if (resource_node->producer_pass_idx != -1) {
                        zest_pass_group_t *producer_pass = &frame_graph->final_passes.data[resource_node->producer_pass_idx];
                        zest_uint producer_queue_index = ZEST__QUEUE_INDEX(producer_pass->submission_id);
                        if (queue_index != producer_queue_index) {
                            batch->queue_wait_stages |= current_pass->inputs.data[input_idx].stage_mask;
                        }
                    }
                    zest_resource_state_t state = { 0 };
                    state.pass_index = current_pass_index;
                    state.queue_family_index = current_pass->queue_info.queue_family_index;
                    state.usage = current_pass->inputs.data[input_idx];
                    state.submission_id = current_pass->submission_id;
                    zest_vec_linear_push(allocator, resource_node->journey, state);
                }

                // Check OUTPUTS of the current pass
                bool requires_new_batch = false;
                zest_map_foreach(output_idx, current_pass->outputs) {
                    zest_resource_node resource_node = current_pass->outputs.data[output_idx].resource_node;
                    if (resource_node->aliased_resource) resource_node = resource_node->aliased_resource;
                    if (resource_node) {
                        resource_node->first_usage_pass_idx = ZEST__MIN(resource_node->first_usage_pass_idx, execution_order_index);
                        resource_node->last_usage_pass_idx = ZEST__MAX(resource_node->last_usage_pass_idx, execution_order_index);
                    }
                    zest_resource_state_t state = { 0 };
                    state.pass_index = current_pass_index;
                    state.queue_family_index = current_pass->queue_info.queue_family_index;
                    state.usage = current_pass->outputs.data[output_idx];
                    state.submission_id = current_pass->submission_id;
                    zest_vec_linear_push(allocator, resource_node->journey, state);
                    zest_vec_foreach(adjacent_index, adjacency_list[current_pass_index].pass_indices) {
                        zest_uint consumer_pass_index = adjacency_list[current_pass_index].pass_indices[adjacent_index];
                        batch_key.next_pass_indexes |= (1ull << consumer_pass_index);
                        batch_key.next_family_indexes |= (1ull << frame_graph->final_passes.data[consumer_pass_index].queue_info.queue_family_index);
                    }
                }
                execution_order_index++;
            }
        }
    }

	//[Create_semaphores]
    //Potentially connect the first submission that uses the swap chain image
    if (zest_vec_size(frame_graph->submissions) > 0) {
        // --- Handle imageAvailableSemaphore ---
        if (ZEST__FLAGGED(frame_graph->flags, zest_frame_graph_expecting_swap_chain_usage)) {

            zest_submission_batch_t *first_batch_to_wait = NULL;
            zest_pipeline_stage_flags wait_stage_for_acquire_semaphore = zest_pipeline_stage_top_of_pipe_bit; // Safe default

            zest_resource_node swapchain_node = frame_graph->swapchain_resource; 
            if (swapchain_node->first_usage_pass_idx != ZEST_INVALID) {
                zest_pass_group_t *pass = frame_graph->pass_execution_order[swapchain_node->first_usage_pass_idx];
                zest_uint submission_index = ZEST__SUBMISSION_INDEX(pass->submission_id);
                zest_uint execution_index = ZEST__EXECUTION_INDEX(pass->submission_id);
                zest_uint queue_index = ZEST__QUEUE_INDEX(pass->submission_id);
                zest_wave_submission_t *wave_submission = &frame_graph->submissions[submission_index];
                zest_uint pass_index = wave_submission->batches[queue_index].pass_indices[execution_index];
				zest_bool uses_swapchain = ZEST_FALSE;
				zest_pipeline_stage_flags first_swapchain_usage_stage_in_this_batch = zest_pipeline_stage_top_of_pipe_bit;
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
                    first_batch_to_wait = &wave_submission->batches[queue_index];
                    wait_stage_for_acquire_semaphore = first_swapchain_usage_stage_in_this_batch;
                    // Ensure this stage is compatible with the batch's queue
                    if (!zest__is_stage_compatible_with_qfi(wait_stage_for_acquire_semaphore, ZestDevice->queues[queue_index]->type)) {
                        ZEST_PRINT("Swapchain usage stage %i is not compatible with queue family %u for wave submission %i",
                            wait_stage_for_acquire_semaphore,
                            first_batch_to_wait->queue_family_index, submission_index);
                        // Fallback or error. Forcing TOP_OF_PIPE might be safer if this happens,
                        // though it indicates a graph definition error.
                        wait_stage_for_acquire_semaphore = zest_pipeline_stage_top_of_pipe_bit;
                    }
                }
            }

            // 2. Assign the wait:
            if (first_batch_to_wait) {
                // The first batch that actually uses the swapchain will wait for it.
				zest_semaphore_reference_t semaphore_reference = { zest_dynamic_resource_image_available_semaphore, 0 };
                zest_vec_linear_push(allocator, first_batch_to_wait->wait_semaphores, semaphore_reference);
                zest_vec_linear_push(allocator, first_batch_to_wait->wait_dst_stage_masks, wait_stage_for_acquire_semaphore);
            } else {
                // Image was acquired, but no pass in the graph uses it.
                // The *very first submission batch of the graph* must wait on the semaphore to consume it.
                // The wait stage must be compatible with this first batch's queue.
                zest_wave_submission_t *actual_first_wave = &frame_graph->submissions[0];
                zest_pipeline_stage_flags compatible_dummy_wait_stage = zest_pipeline_stage_top_of_pipe_bit;
                zest_uint queue_family_index = ZEST_INVALID;
                zest_submission_batch_t *first_batch = NULL;

                // Make the dummy wait stage more specific if possible, but ensure compatibility
                for (int queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
                    if (actual_first_wave->batches[queue_index].magic) {
                        first_batch = &actual_first_wave->batches[queue_index];
                        if (ZestDevice->queues[queue_index]->type & zest_queue_transfer) {
                            compatible_dummy_wait_stage = zest_pipeline_stage_transfer_bit;
                            queue_family_index = actual_first_wave->batches[queue_index].queue_family_index;
                        } else if (ZestDevice->queues[queue_index]->type & zest_queue_compute) {
                            compatible_dummy_wait_stage = zest_pipeline_stage_compute_shader_bit;
                            queue_family_index = actual_first_wave->batches[queue_index].queue_family_index;
                        }
                        break;
                    }
                }
                // Graphics queue can also do TOP_OF_PIPE or even COLOR_ATTACHMENT_OUTPUT_BIT if it's just for semaphore flow.

                if (first_batch) {
                    zest_semaphore_reference_t semaphore_reference = { zest_dynamic_resource_image_available_semaphore, 0 };
                    zest_vec_linear_push(allocator, first_batch->wait_semaphores, semaphore_reference);
                    zest_vec_linear_push(allocator, first_batch->wait_dst_stage_masks, compatible_dummy_wait_stage);
                    ZEST_PRINT("RenderGraph: Swapchain image acquired but not used by any pass. First batch (on QFI %u) will wait on imageAvailableSemaphore at stage %i.",
                        queue_family_index,
                        compatible_dummy_wait_stage);
                }
            }
        }

        // --- Handle renderFinishedSemaphore for the last batch ---
        zest_wave_submission_t *last_wave = &zest_vec_back(frame_graph->submissions);
        if (last_wave->batches[ZEST_GRAPHICS_QUEUE_INDEX].magic) {  //Only if it's a graphics queue... We should probably check that it renders to a swap chain as well
            // This assumes the last batch's *primary* signal is renderFinished.
            // If it also needs to signal internal semaphores, `signal_semaphore` needs to become a list.
            if (!last_wave->batches[ZEST_GRAPHICS_QUEUE_INDEX].signal_semaphores) {
				zest_semaphore_reference_t semaphore_reference = { zest_dynamic_resource_render_finished_semaphore, 0 };
                zest_vec_linear_push(allocator, last_wave->batches[ZEST_GRAPHICS_QUEUE_INDEX].signal_semaphores, semaphore_reference);
            } else {
                // This case needs `p_signal_semaphores` to be a list in your batch struct.
                // You would then add ZestRenderer->frame_sync[ZEST_FIF].render_finished_semaphore to that list.
                ZEST_PRINT("Last batch already has an internal signal_semaphore. Logic to add external renderFinishedSemaphore needs p_signal_semaphores to be a list.");
                // For now, you might just overwrite if single signal is assumed for external:
                // last_batch->internal_signal_semaphore = ZestRenderer->frame_sync[ZEST_FIF].render_finished_semaphore;
            }
        }
    }

    //Plan_transient_buffers
    zest_bucket_array_foreach(resource_index, frame_graph->resources) {
        zest_resource_node resource = zest_bucket_array_get(&frame_graph->resources, zest_resource_node_t, resource_index);

        // Check if this resource is transient AND actually used in the compiled graph
        if (ZEST__FLAGGED(resource->flags, zest_resource_node_flag_transient)) {
            if (resource->reference_count > 0 || ZEST__FLAGGED(resource->flags, zest_resource_node_flag_essential_output) &&
                resource->first_usage_pass_idx <= resource->last_usage_pass_idx && // Ensures it's used
                resource->first_usage_pass_idx != ZEST_INVALID) {

                zest_pass_group_t *first_pass = frame_graph->pass_execution_order[resource->first_usage_pass_idx];
                zest_pass_group_t *last_pass = frame_graph->pass_execution_order[resource->last_usage_pass_idx];

                zest_vec_linear_push(allocator, first_pass->transient_resources_to_create, resource);
                zest_vec_linear_push(allocator, last_pass->transient_resources_to_free, resource);
            } else {
                frame_graph->culled_resources_count++;
                ZEST__REPORT(zest_report_resource_culled, "Transient resource [%s] was culled because it's was not consumed by any other passes. If you intended to use this output outside of the frame graph once it's executed then you can call zest_FlagResourceAsEssential.", resource->name);
            }
        } 
        if (resource->buffer_provider || resource->image_provider) {
            zest_vec_linear_push(allocator, frame_graph->resources_to_update, resource);
        }
    }

    //Plan_resource_barriers
    zest_bucket_array_foreach(resource_index, frame_graph->resources) {
        zest_resource_node resource = zest_bucket_array_get(&frame_graph->resources, zest_resource_node_t, resource_index);
        if (resource->aliased_resource) continue;
        zest_resource_state_t *prev_state = NULL;
        int starting_state_index = 0;
        zest_vec_foreach(state_index, resource->journey) {
            if (resource->reference_count == 0 && ZEST__NOT_FLAGGED(resource->flags, zest_resource_node_flag_essential_output)) {
                continue;
            }
            zest_resource_state_t *current_state = &resource->journey[state_index];
            zest_pass_group_t *pass = &frame_graph->final_passes.data[current_state->pass_index];
            zest_resource_usage_t *current_usage = &current_state->usage;
            zest_resource_state_t *next_state = NULL;
            zest_execution_details_t *exe_details = &pass->execution_details;
            zest_execution_barriers_t *barriers = &exe_details->barriers;
            if (state_index + 1 < (int)zest_vec_size(resource->journey)) {
                next_state = &resource->journey[state_index + 1];
            }
            zest_pipeline_stage_flags required_stage = current_usage->stage_mask;
            if (resource->type & zest_resource_type_is_image) {
                zest__add_image_barriers(frame_graph, allocator, resource, barriers, current_state, prev_state, next_state);
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
                    zest_uint dst_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
                    if (src_queue_family_index == ZEST_QUEUE_FAMILY_IGNORED) {
                        if ((resource->access_mask & zest_access_write_bits_general) &&
                            (current_usage->access_mask & zest_access_read_bits_general)) {
                            ZestPlatform->add_frame_graph_buffer_barrier(resource, barriers, true,
                                resource->access_mask, current_usage->access_mask,
                                src_queue_family_index, dst_queue_family_index,
                                resource->last_stage_mask, current_state->usage.stage_mask);
                        }
                    } else if (ZEST__NOT_FLAGGED(frame_graph->flags, zest_frame_graph_force_on_graphics_queue)) {
                        //This resource already belongs to a queue which means that it's an imported resource
                        //If the frame graph is on the graphics queue only then there's no need to acquire from a prior release.

                        //It shouldn't be possible for transient resources to be considered for a barrier at this point. 
                        ZEST_ASSERT(ZEST__FLAGGED(resource->flags, zest_resource_node_flag_imported));
                        dst_queue_family_index = current_state->queue_family_index;
                        if (src_queue_family_index != ZEST_QUEUE_FAMILY_IGNORED) {
                            ZestPlatform->add_frame_graph_buffer_barrier(resource, barriers, true,
                                resource->access_mask, current_usage->access_mask,
                                src_queue_family_index, dst_queue_family_index,
                                resource->last_stage_mask, current_state->usage.stage_mask);
                        }
                    }
                    bool needs_releasing = false;
                } else {
                    //There is a previous state, do we need to acquire the resource from a different queue?
                    zest_uint src_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
                    zest_uint dst_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
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
                        ZestPlatform->add_frame_graph_buffer_barrier(resource, barriers, true,
                            prev_usage->access_mask, current_usage->access_mask,
                            src_queue_family_index, dst_queue_family_index,
                            prev_state->usage.stage_mask, current_state->usage.stage_mask);
                        zest_submission_batch_t *batch = zest__get_submission_batch(current_state->submission_id);
                        batch->queue_wait_stages |= prev_state->usage.stage_mask;
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
                        zest_pipeline_stage_flags dst_stage = zest_pipeline_stage_bottom_of_pipe_bit;
                        if (needs_releasing) {
                            current_state->was_released = true;
                        } else {
                            dst_stage = next_state->usage.stage_mask;
                            current_state->was_released = false;
                        }
                        ZestPlatform->add_frame_graph_buffer_barrier(resource, barriers, false,
                            current_usage->access_mask, next_state->usage.access_mask,
                            src_queue_family_index, dst_queue_family_index,
                            current_state->usage.stage_mask, dst_stage);
                    }
                } else if (resource->flags & zest_resource_node_flag_release_after_use
                    && current_state->queue_family_index != ZestDevice->transfer_queue_family_index
                    && ZEST__NOT_FLAGGED(frame_graph->flags, zest_frame_graph_force_on_graphics_queue)) {
                    //Release the buffer so that it's ready to be acquired by any other queue in the next frame
                    //Release to the transfer queue by default (if it's not already on the transfer queue).
					ZestPlatform->add_frame_graph_buffer_barrier(resource, barriers, false,
						current_usage->access_mask, zest_access_none,
                        current_state->queue_family_index, ZestDevice->transfer_queue_family_index,
                        current_state->usage.stage_mask, zest_pipeline_stage_bottom_of_pipe_bit);
                }
                prev_state = current_state;
            }
 
        }
    }

    //Process_compiled_execution_order
    zest_vec_foreach(submission_index, frame_graph->submissions) {
        zest_wave_submission_t *submission = &frame_graph->submissions[submission_index];
        for (int queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
            zest_vec_foreach(batch_index, submission->batches[queue_index].pass_indices) {
                int current_pass_index = submission->batches[queue_index].pass_indices[batch_index];
                zest_pass_group_t *pass = &frame_graph->final_passes.data[current_pass_index];
                zest_execution_details_t *exe_details = &pass->execution_details;

                ZestPlatform->validate_barrier_pipeline_stages(&exe_details->barriers);

                //If this pass is a render pass with an execution callback
                //Create_render_passes
                frame_graph->error_status = ZestPlatform->create_fg_render_pass(pass, exe_details, current_pass_index);

            }   //Passes within batch loop
        }
        
    }   //Batch loop

    if (ZEST__FLAGGED(frame_graph->flags, zest_frame_graph_expecting_swap_chain_usage)) {
        if (ZEST__NOT_FLAGGED(frame_graph->flags, zest_frame_graph_present_after_execute)) {
            ZEST__REPORT(zest_report_expecting_swapchain_usage, "Swapchain usage was expected but the frame graph present flag was not set in frame graph [%s], indicating that a render pass could not be created. Check other reports.", frame_graph->name);
            ZEST__FLAG(frame_graph->flags, zest_frame_graph_present_after_execute);
        }
        //Error: the frame graph is trying to render to the screen but no swap chain image was used!
        //Make sure that you call zest_ConnectSwapChainOutput in your frame graph setup.
    }
	ZEST__FLAG(frame_graph->flags, zest_frame_graph_is_compiled);  

    return frame_graph;
}

zest_frame_graph zest_EndFrameGraph() {
    zest_frame_graph frame_graph = zest__compile_frame_graph();

    ZEST__MAYBE_REPORT(ZestRenderer->current_pass, zest_report_missing_end_pass, "Warning in frame graph [%s]: The current pass in the frame graph context is not null. This means that a call to zest_EndPass is missing in the frame graph setup.", frame_graph->name);
    ZestRenderer->current_pass = 0;

    if (frame_graph->error_status != zest_fgs_critical_error) {
        zest__cache_frame_graph(frame_graph);

        zest__execute_frame_graph(ZEST_FALSE);

        if (ZEST_VALID_HANDLE(frame_graph->swapchain) && ZEST__FLAGGED(frame_graph->flags, zest_frame_graph_present_after_execute)) {
			ZestPlatform->present_frame(frame_graph->swapchain);
        }
    } else {
        ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_work_was_submitted);
    }

    ZestRenderer->current_frame_graph = 0;
	ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_building_frame_graph);  

    return frame_graph;
}

zest_frame_graph zest_EndFrameGraphAndWait() {
    zest_frame_graph frame_graph = zest__compile_frame_graph();

    if (frame_graph->error_status != zest_fgs_critical_error) {
        zest__execute_frame_graph(ZEST_TRUE);
    } else {
        ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_work_was_submitted);
    }

    ZestRenderer->current_frame_graph = 0;
	ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_building_frame_graph);  

    return frame_graph;
}

zest_image_aspect_flags zest__determine_aspect_flag(zest_texture_format format) {
    switch (format) {
        // Depth-Only Formats
    case zest_format_d16_unorm:
    case zest_format_x8_d24_unorm_pack32: // D24_UNORM component, X8 is undefined
    case zest_format_d32_sfloat:
        return zest_image_aspect_depth_bit;
        // Stencil-Only Formats
    case zest_format_s8_uint:
        return zest_image_aspect_stencil_bit;
        // Combined Depth/Stencil Formats
    case zest_format_d16_unorm_s8_uint:
    case zest_format_d24_unorm_s8_uint:
    case zest_format_d32_sfloat_s8_uint:
        return zest_image_aspect_depth_bit | zest_image_aspect_stencil_bit;
    default:
        return zest_image_aspect_color_bit;
    }
}

void zest__interpret_hints(zest_resource_node resource, zest_resource_usage_hint usage_hints) {
    if (usage_hints & zest_resource_usage_hint_copy_dst) {
        resource->image.info.flags |= zest_image_flag_transfer_dst;
    }
    if (usage_hints & zest_resource_usage_hint_copy_src) {
        resource->image.info.flags |= zest_image_flag_transfer_src;
    }
    if (usage_hints & zest_resource_usage_hint_cpu_transfer) {
        resource->image.info.flags |= zest_image_flag_host_visible | zest_image_flag_host_coherent;
    }
}

zest_image_layout zest__determine_final_layout(zest_uint pass_index, zest_resource_node node, zest_resource_usage_t *current_usage) {
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_vec_foreach(state_index, node->journey) {
        zest_resource_state_t *state = &node->journey[state_index];
        if (state->pass_index == pass_index && (zest_uint)state_index < zest_vec_size(node->journey) - 1) {
            return node->journey[state_index + 1].usage.image_layout;
        }
    }
    if (node->type == zest_resource_type_swap_chain_image) {
        return zest_image_layout_present;
    } else if (ZEST__FLAGGED(node->flags, zest_resource_node_flag_imported)) {
        return node->image_layout; 
    } 
    if (current_usage->store_op == zest_store_op_dont_care) {
        return zest_image_layout_undefined;
    }
    return zest_image_layout_shader_read_only_optimal;
}

void zest__deferr_resource_destruction(void* handle) {
    zest_vec_push(ZestRenderer->deferred_resource_freeing_list.resources[ZEST_FIF], handle);
}

void zest__deferr_image_destruction(zest_image image) {
    zest_vec_push(ZestRenderer->deferred_resource_freeing_list.images[ZEST_FIF], *image);
}

zest_bool zest__execute_frame_graph(zest_bool is_intraframe) {
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zloc_linear_allocator_t *allocator = ZestRenderer->frame_graph_allocator[ZEST_FIF];
    zest_map_queue_value queues = { 0 };

    zest_execution_backend backend = ZestPlatform->new_execution_backend(allocator);

    ZestPlatform->set_execution_fence(backend, is_intraframe);

	zest_vec_foreach(resource_index, frame_graph->resources_to_update) {
		zest_resource_node resource = frame_graph->resources_to_update[resource_index];
		if (resource->buffer_provider) {
			resource->storage_buffer = resource->buffer_provider(resource);
		} else if (resource->image_provider) {
            resource->view = resource->image_provider(resource);
		}
	}

    zest_vec_foreach(submission_index, frame_graph->submissions) {
        zest_wave_submission_t *wave_submission = &frame_graph->submissions[submission_index];

        for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
            zest_submission_batch_t *batch = &wave_submission->batches[queue_index];
            if (!batch->magic) continue;
            ZEST_ASSERT(zest_vec_size(batch->pass_indices));    //A batch was created without any pass indices. Bug in the Compile stage!

            zest_pipeline_stage_flags timeline_wait_stage;

            // 1. acquire an appropriate command buffer
            switch (batch->queue_type) {
            case zest_queue_graphics:
                ZEST_CLEANUP_ON_FALSE(ZestPlatform->set_next_command_buffer(&frame_graph->context, batch->queue));
                timeline_wait_stage = zest_pipeline_stage_vertex_input_bit;
                break;
            case zest_queue_compute:
                ZEST_CLEANUP_ON_FALSE(ZestPlatform->set_next_command_buffer(&frame_graph->context, batch->queue));
                timeline_wait_stage = zest_pipeline_stage_compute_shader_bit;
                break;
            case zest_queue_transfer:
                ZEST_CLEANUP_ON_FALSE(ZestPlatform->set_next_command_buffer(&frame_graph->context, batch->queue));
                timeline_wait_stage = zest_pipeline_stage_transfer_bit;
                break;
            default:
                ZEST_ASSERT(0); //Unknown queue type for batch. Corrupt memory perhaps?!
            }

            ZEST_CLEANUP_ON_FALSE(ZestPlatform->begin_command_buffer(&frame_graph->context));

            zest_vec_foreach(i, batch->pass_indices) {
                zest_uint pass_index = batch->pass_indices[i];
                zest_pass_group_t *grouped_pass = &frame_graph->final_passes.data[pass_index];
                zest_execution_details_t *exe_details = &grouped_pass->execution_details;

                frame_graph->context.submission_index = submission_index;
                frame_graph->context.timeline_wait_stage = timeline_wait_stage;
                frame_graph->context.queue_index = queue_index;

                //Create any transient resources where they're first used in this grouped_pass
                zest_vec_foreach(r, grouped_pass->transient_resources_to_create) {
                    zest_resource_node resource = grouped_pass->transient_resources_to_create[r];
                    if (!zest__create_transient_resource(resource)) {
                        frame_graph->error_status |= zest_fgs_transient_resource_failure;
                        return ZEST_FALSE;
                    }
                }

                //Batch execute acquire barriers for images and buffers
				ZestPlatform->acquire_barrier(&frame_graph->context, exe_details);

                bool has_render_pass = ZEST_VALID_HANDLE(exe_details->render_pass);

                //Begin the render pass if the pass has one
                if (has_render_pass) {
					frame_graph->context.render_pass = exe_details->render_pass;
                    ZestPlatform->begin_render_pass(&frame_graph->context, exe_details);
                }

                //Execute the callbacks in the pass
                zest_vec_foreach(pass_index, grouped_pass->passes) {
                    zest_pass_node pass = grouped_pass->passes[pass_index];
                    if (pass->type == zest_pass_type_graphics && !frame_graph->context.render_pass) {
                        ZEST__REPORT(zest_report_render_pass_skipped, "Pass execution was skipped for pass [%s] becuase no render pass was found. Check other reports for why that is.", pass->name);
                        continue;
                    }
                    frame_graph->context.pass_node = pass;
                    frame_graph->context.frame_graph = frame_graph;
                    pass->execution_callback.callback(&frame_graph->context, pass->execution_callback.user_data);
                }

                zest_map_foreach(pass_input_index, grouped_pass->inputs) {
                    zest_resource_node resource = grouped_pass->inputs.data[pass_input_index].resource_node;
                    if (resource->aliased_resource) {
                        resource->aliased_resource->current_state_index++;
                    } else {
                        resource->current_state_index++;
                    }
                }

                zest_map_foreach(pass_output_index, grouped_pass->outputs) {
                    zest_resource_node resource = grouped_pass->outputs.data[pass_output_index].resource_node;
                    if (resource->aliased_resource) {
                        resource->aliased_resource->current_state_index++;
                    } else {
                        resource->current_state_index++;
                    }
                }

                if (has_render_pass) {
                    ZestPlatform->end_render_pass(&frame_graph->context);
                }

                //Batch execute release barriers for images and buffers

				ZestPlatform->release_barrier(&frame_graph->context, exe_details);

                zest_vec_foreach(r, grouped_pass->transient_resources_to_free) {
                    zest_resource_node resource = grouped_pass->transient_resources_to_free[r];
                    zest__free_transient_resource(resource);
                }
                //End pass
            }
			ZestPlatform->end_command_buffer(&frame_graph->context);

            ZestPlatform->submit_frame_graph_batch(frame_graph, backend, batch, &queues);

        }   //Batch

        //For each batch in the last wave add the queue semaphores that were used so that the next batch submissions can wait on them
        ZestPlatform->carry_over_semaphores(frame_graph, wave_submission, backend);
    }   //Wave 

    zest_map_foreach(i, queues) {
        zest_queue queue = queues.data[i];
        queue->fif = (queue->fif + 1) % ZEST_MAX_FIF;
    }

	zest_bucket_array_foreach(index, frame_graph->resources) {
		zest_resource_node resource = zest_bucket_array_get(&frame_graph->resources, zest_resource_node_t, index);
		if (ZEST__FLAGGED(resource->flags, zest_resource_node_flag_transient)) {
			if(resource->image.backend && resource->image.backend->vk_image) {
				zest__deferr_image_destruction(&resource->image);
                resource->image = (zest_image_t){ 0 };
                resource->mip_level_bindless_indexes = 0;
			}
		}
        //Reset the resource state indexes (the point in their graph journey). This is necessary for cached
        //frame graphs.
        if (resource->aliased_resource) {
            resource->aliased_resource->current_state_index = 0;
        } else {
            resource->current_state_index = 0;
        }

	}

    ZEST__FLAG(frame_graph->flags, zest_frame_graph_is_executed);

    if (is_intraframe && *backend->fence_count > 0) {
        //Todo: handle this better
        if (!ZestPlatform->frame_graph_fence_wait(backend)) {
            return ZEST_FALSE;
        }
    }

    return ZEST_TRUE;

cleanup:
    return ZEST_FALSE;
}

zest_key zest_GetPassOutputKey(zest_pass_node pass) {
    ZEST_ASSERT_HANDLE(pass);   //Not a valid pass node handle
    return pass->output_key;
}

bool zest_RenderGraphWasExecuted(zest_frame_graph frame_graph) {
    return ZEST__FLAGGED(frame_graph->flags, zest_frame_graph_is_executed);
}

void zest_PrintCachedRenderGraph(zest_frame_graph_cache_key_t *cache_key) {
    zest_key key = zest__hash_frame_graph_cache_key(cache_key);
    zest_frame_graph frame_graph = zest__get_cached_frame_graph(key);
    if (frame_graph) {
        zest_PrintCompiledRenderGraph(frame_graph);
    }
}

zest_frame_graph_result zest_GetFrameGraphResult(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    return frame_graph->error_status;
}

zest_uint zest_GetFrameGraphCulledResourceCount(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    return frame_graph->culled_resources_count;
}

zest_uint zest_GetFrameGraphFinalPassCount(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    return zest_map_size(frame_graph->final_passes);
}

zest_uint zest_GetFrameGraphPassTransientCreateCount(zest_frame_graph frame_graph, zest_key output_key) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (zest_map_valid_key(frame_graph->final_passes, output_key)) {
		zest_pass_group_t *final_pass = zest_map_at_key(frame_graph->final_passes, output_key);
        return zest_vec_size(final_pass->transient_resources_to_create);
    }
    return 0;
}

zest_uint zest_GetFrameGraphPassTransientFreeCount(zest_frame_graph frame_graph, zest_key output_key) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (zest_map_valid_key(frame_graph->final_passes, output_key)) {
		zest_pass_group_t *final_pass = zest_map_at_key(frame_graph->final_passes, output_key);
        return zest_vec_size(final_pass->transient_resources_to_free);
    }
    return 0;
}

zest_uint zest_GetFrameGraphCulledPassesCount(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    return frame_graph->culled_passes_count;
}

zest_uint zest_GetFrameGraphSubmissionCount(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    return zest_vec_size(frame_graph->submissions);
}

zest_uint zest_GetFrameGraphSubmissionBatchCount(zest_frame_graph frame_graph, zest_uint submission_index) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (submission_index < zest_vec_size(frame_graph->submissions)) {
        return zest_vec_size(frame_graph->submissions[submission_index].batches);
    }
    return 0;
}

zest_uint zest_GetSubmissionBatchPassCount(const zest_submission_batch_t *batch) {
    return zest_vec_size(batch->pass_indices);
}

const zest_submission_batch_t *zest_GetFrameGraphSubmissionBatch(zest_frame_graph frame_graph, zest_uint submission_index, zest_uint batch_index) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (submission_index < zest_vec_size(frame_graph->submissions)) {
		zest_wave_submission_t *submission = &frame_graph->submissions[submission_index];
        if (batch_index < zest_vec_size(submission->batches)) {
            return &submission->batches[batch_index];
        }
    }
    return NULL;
}

const zest_pass_group_t *zest_GetFrameGraphFinalPass(zest_frame_graph frame_graph, zest_uint pass_index) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (pass_index < zest_map_size(frame_graph->final_passes)) {
        return &frame_graph->final_passes.data[pass_index];
    }
    return NULL;
}

void zest_PrintCompiledRenderGraph(zest_frame_graph frame_graph) {
    ZestPlatform->print_compiled_frame_graph(frame_graph);
}

void zest_EmptyRenderPass(const zest_frame_graph_context context, void *user_data) {
    //Nothing here to render, it's just for frame graphs that have nothing to render
}

zest_uint zest_AcquireBindlessImageIndex(zest_image_handle handle, zest_image_view view, zest_sampler sampler, zest_set_layout_handle layout_handle, zest_descriptor_set set, zest_global_binding_number target_binding_number) {
    ZEST_PRINT_FUNCTION;
    zest_image image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, handle.value);
    zest_set_layout layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, layout_handle.value);

    zest_uint binding_number = ZEST_INVALID;
    VkDescriptorType descriptor_type;
	zest_vec_foreach(i, layout->backend->layout_bindings) {
		VkDescriptorSetLayoutBinding *binding = &layout->backend->layout_bindings[i];
		if (binding->binding == target_binding_number && 
            (binding->descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
             binding->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
             binding->descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)) {
            descriptor_type = binding->descriptorType;
			binding_number = binding->binding;
			break;
        }
	}

    ZEST_ASSERT(binding_number != ZEST_INVALID);    //Could not find an appropriate descriptor type in the layout with that target binding number!
    zest_uint array_index = zest__acquire_bindless_index(layout, binding_number);
    if (array_index == ZEST_INVALID) {
        //Ran out of space in the descriptor pool
        ZEST__REPORT(zest_report_bindless_indexes, "Ran out of space in the descriptor pool when trying to acquire an index for image, binding number %i.", binding_number);
        return ZEST_INVALID;
    }

    VkWriteDescriptorSet write = { 0 };
    VkDescriptorImageInfo image_info = { 0 };
    image_info.imageLayout = image->backend->vk_current_layout;
    image_info.imageView = view->backend->vk_view;
    ZEST_ASSERT(image_info.imageView);  //Must have an image view set in the texture
    if (descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        image_info.sampler = sampler->backend->vk_sampler;
		write = zest_CreateImageDescriptorWriteWithType(set->backend->vk_descriptor_set, &image_info, binding_number, descriptor_type);
    } else {
		write = zest_CreateImageDescriptorWriteWithType(set->backend->vk_descriptor_set, &image_info, binding_number, descriptor_type);
    }
    write.dstArrayElement = array_index;
	vkUpdateDescriptorSets(ZestDevice->backend->logical_device, 1, &write, 0, 0);

    image->bindless_index[binding_number] = array_index;
    return array_index;
}

zest_uint zest_AcquireBindlessStorageBufferIndex(zest_buffer buffer, zest_set_layout_handle layout_handle, zest_descriptor_set set, zest_uint target_binding_number) {
    ZEST_PRINT_FUNCTION;
    zest_set_layout layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, layout_handle.value);

    zest_uint binding_number = ZEST_INVALID;
    zest_vec_foreach(i, layout->backend->layout_bindings) {
        VkDescriptorSetLayoutBinding *layout_binding = &layout->backend->layout_bindings[i];
        if (target_binding_number == layout_binding->binding && layout_binding->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
            binding_number = layout_binding->binding;
            break;
        }
    }

    ZEST_ASSERT(binding_number != ZEST_INVALID);    //Could not find an appropriate descriptor type in the layout with that target binding number!
    zest_uint array_index = zest__acquire_bindless_index(layout, binding_number);

    VkDescriptorBufferInfo buffer_info = zest_vk_GetBufferInfo(buffer);

    VkWriteDescriptorSet write = zest_CreateBufferDescriptorWriteWithType(set->backend->vk_descriptor_set, &buffer_info, binding_number, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    write.dstArrayElement = array_index;
    vkUpdateDescriptorSets(ZestDevice->backend->logical_device, 1, &write, 0, 0);
    buffer->array_index = array_index;

    return array_index;
}

zest_uint zest_AcquireGlobalSampler(zest_image_handle image_handle, zest_sampler_handle sampler_handle, zest_global_binding_number binding_number) {
    zest_image image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, image_handle.value);
    zest_sampler sampler = (zest_sampler)zest__get_store_resource_checked(&ZestRenderer->samplers, sampler_handle.value);
    zest_uint index = zest_AcquireBindlessImageIndex(image_handle, image->default_view, sampler, ZestRenderer->global_bindless_set_layout, ZestRenderer->global_set, binding_number);
    return index;
}

zest_uint zest_AcquireGlobalSamplerWithView(zest_image_handle image_handle, zest_image_view_handle view_handle, zest_sampler sampler, zest_global_binding_number binding_number) {
    zest_image image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, image_handle.value);
    zest_image_view view = (zest_image_view)zest__get_store_resource_checked(&ZestRenderer->views, view_handle.value);
    zest_uint index = zest_AcquireBindlessImageIndex(image_handle, view, sampler, ZestRenderer->global_bindless_set_layout, ZestRenderer->global_set, binding_number);
    return index;
}

zest_uint *zest_AcquireGlobalImageMipIndexes(zest_image_handle image_handle, zest_sampler_handle sampler_handle, zest_image_view_array_handle view_array_handle, zest_global_binding_number binding_number) {
    ZEST_PRINT_FUNCTION;
    zest_image image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, image_handle.value);
    ZEST_ASSERT(image->info.mip_levels > 1);         //The resource does not have any mip levels. Make sure to set the number of mip levels when creating the resource in the frame graph

    if (zest_map_valid_key(image->mip_indexes, (zest_key)binding_number)) {
        zest_mip_index_collection *mip_collection = zest_map_at_key(image->mip_indexes, (zest_key)binding_number);
        return mip_collection->mip_indexes;
    }

    zest_sampler sampler = (zest_sampler)zest__get_store_resource_checked(&ZestRenderer->samplers, sampler_handle.value);
    zest_image_view_array view_array = *(zest_image_view_array*)zest__get_store_resource_checked(&ZestRenderer->view_arrays, view_array_handle.value);
    zest_mip_index_collection mip_collection = { 0 };
    mip_collection.binding_number = binding_number;
    zest_set_layout global_layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, ZestRenderer->global_bindless_set_layout.value);
    for (int mip_index = 0; mip_index != view_array->count; ++mip_index) {
        zest_uint bindless_index = zest__acquire_bindless_index(global_layout, binding_number);

        VkDescriptorImageInfo mip_buffer_info;
        mip_buffer_info.imageLayout = image->backend->vk_current_layout;
        mip_buffer_info.imageView = view_array->views[mip_index].backend->vk_view;
        mip_buffer_info.sampler = sampler->backend->vk_sampler;

        VkWriteDescriptorSet write = zest_CreateImageDescriptorWriteWithType(ZestRenderer->global_set->backend->vk_descriptor_set, &mip_buffer_info, binding_number, global_layout->backend->descriptor_indexes[binding_number].descriptor_type);
        write.dstArrayElement = bindless_index;
        vkUpdateDescriptorSets(ZestDevice->backend->logical_device, 1, &write, 0, 0);

        zest_vec_push(mip_collection.mip_indexes, bindless_index);
    }
    zest_map_insert_key( image->mip_indexes, (zest_key)binding_number, mip_collection);
    zest_uint size = zest_map_size(image->mip_indexes);
    return mip_collection.mip_indexes;
}

zest_uint zest_AcquireGlobalStorageBufferIndex(zest_buffer buffer) {
    ZEST_PRINT_FUNCTION;
    zest_set_layout global_layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, ZestRenderer->global_bindless_set_layout.value);
    zest_uint array_index = zest__acquire_bindless_index(global_layout, zest_storage_buffer_binding);

    VkDescriptorBufferInfo buffer_info = zest_vk_GetBufferInfo(buffer);

    VkWriteDescriptorSet write = zest_CreateBufferDescriptorWriteWithType(ZestRenderer->global_set->backend->vk_descriptor_set, &buffer_info, zest_storage_buffer_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    write.dstArrayElement = array_index;
    vkUpdateDescriptorSets(ZestDevice->backend->logical_device, 1, &write, 0, 0);
    buffer->array_index = array_index;

    return array_index;
}

void zest_AcquireGlobalInstanceLayerBufferIndex(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
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

void zest_ReleaseGlobalImageIndex(zest_image_handle handle, zest_global_binding_number binding_number) {
    zest_image image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, handle.value);
    ZEST_ASSERT(binding_number < zest_max_global_binding_number);
    if (image->bindless_index[binding_number] != ZEST_INVALID) {
        zest__release_bindless_index(ZestRenderer->global_bindless_set_layout, binding_number, image->bindless_index[binding_number]);
    }
}

void zest_ReleaseAllGlobalImageIndexes(zest_image_handle handle) {
    zest_image image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, handle.value);
    zest__release_all_global_texture_indexes(image);
}

void zest_ReleaseGlobalBindlessIndex(zest_uint index, zest_global_binding_number binding_number) {
    ZEST_ASSERT(index != ZEST_INVALID);
    zest__release_bindless_index(ZestRenderer->global_bindless_set_layout, binding_number, index);
}

VkDescriptorSet zest_vk_GetGlobalBindlessSet() {
    return ZestRenderer->global_set->backend->vk_descriptor_set;
}

VkDescriptorSetLayout zest_vk_GetDebugLayout() {
    zest_set_layout set_layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, ZestRenderer->texture_debug_layout.value);
    return set_layout->backend->vk_layout;
}

VkDescriptorSetLayout zest_vk_GetGlobalBindlessLayout() {
    zest_set_layout global_layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, ZestRenderer->global_bindless_set_layout.value);
    return global_layout->backend->vk_layout;
}

zest_set_layout_handle zest_GetGlobalBindlessLayout() {
    return ZestRenderer->global_bindless_set_layout;
}

zest_descriptor_set zest_GetGlobalBindlessSet() {
    return ZestRenderer->global_set;
}

zest_resource_node zest__add_transient_image_resource(const char *name, const zest_image_info_t *description, zest_bool assign_bindless, zest_bool image_view_binding_only) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    zest_resource_node_t resource = { 0 };
    resource.name = name;
	resource.id = frame_graph->id_counter++;
    resource.first_usage_pass_idx = ZEST_INVALID;
    resource.image.info = *description;
	resource.type = description->format == ZestRenderer->depth_format ? zest_resource_type_depth : zest_resource_type_image;
    resource.frame_graph = frame_graph;
    resource.current_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
    resource.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
    resource.producer_pass_idx = -1;
    resource.image.backend = zest__new_frame_graph_image_backend(ZestRenderer->frame_graph_allocator[ZEST_FIF]);
	ZEST__FLAG(resource.flags, zest_resource_node_flag_transient);
    zest_resource_node node = zest__add_frame_graph_resource(&resource);
    node->image_layout = zest_image_layout_undefined;
    return node;
}

void zest_FlagResourceAsEssential(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(resource);   //Not a valid resource handle!
    ZEST__FLAG(resource->flags, zest_resource_node_flag_essential_output);
}

void zest_AddSwapchainToRenderTargetGroup(zest_output_group group) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph->swapchain_resource);    //frame graph must have a swapchain, use zest_BeginFrameGraphSwapchain
    ZEST_ASSERT_HANDLE(group);                      //Not a valid render target group
    zest_vec_linear_push(ZestRenderer->frame_graph_allocator[ZEST_FIF], group->resources, frame_graph->swapchain_resource);
}

void zest_AddImageToRenderTargetGroup(zest_output_group group, zest_resource_node image) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    ZEST_ASSERT_HANDLE(group);                      //Not a valid render target group
    ZEST_ASSERT(image->type & zest_resource_type_is_image_or_depth);  //Must be a depth buffer resource type
	zest_FlagResourceAsEssential(image);
    zest_vec_linear_push(ZestRenderer->frame_graph_allocator[ZEST_FIF], group->resources, image);
}

zest_output_group zest_CreateOutputGroup() {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    zest_output_group group = zloc_LinearAllocation(ZestRenderer->frame_graph_allocator[ZEST_FIF], sizeof(zest_output_group_t));
    *group = (zest_output_group_t){ 0 };
    group->magic = zest_INIT_MAGIC(zest_struct_type_render_target_group);
    return group;
}

zest_resource_node zest_AddTransientImageResource(const char *name, zest_image_resource_info_t *info) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    zest_image_info_t description = { 0 };
    description.extent.width = info->width ? info->width : zest_ScreenWidth();
    description.extent.height = info->height ? info->height : zest_ScreenHeight();
    description.sample_count = (info->usage_hints & zest_resource_usage_hint_msaa) ? ZestDevice->backend->msaa_samples : VK_SAMPLE_COUNT_1_BIT;
    description.mip_levels = info->mip_levels ? info->mip_levels : 1;
    description.layer_count = info->layer_count ? info->layer_count : 1;
	description.format = info->format == zest_format_depth ? ZestRenderer->depth_format : info->format;
    description.aspect_flags = zest__determine_aspect_flag(description.format);
	zest_resource_node resource = zest__add_transient_image_resource(name, &description, 0, ZEST_TRUE);
    zest__interpret_hints(resource, info->usage_hints);
    return resource;
}

zest_resource_node zest_AddTransientBufferResource(const char *name, const zest_buffer_resource_info_t *info) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    zest_resource_node_t node = { 0 };
    node.name = name;
    node.id = frame_graph->id_counter++;
    node.first_usage_pass_idx = ZEST_INVALID;
    node.buffer_desc.size = info->size;
    if (info->usage_hints & zest_resource_usage_hint_vertex_buffer) {
        node.buffer_desc.buffer_info = zest_CreateVertexBufferInfo(info->usage_hints & zest_resource_usage_hint_cpu_transfer);;
    } else if (info->usage_hints & zest_resource_usage_hint_index_buffer) {
        node.buffer_desc.buffer_info = zest_CreateIndexBufferInfo(info->usage_hints & zest_resource_usage_hint_cpu_transfer);;
    } else {
        node.buffer_desc.buffer_info = zest_CreateStorageBufferInfo(info->usage_hints & zest_resource_usage_hint_cpu_transfer);;
    }
    if (info->usage_hints & zest_resource_usage_hint_copy_src) {
        node.buffer_desc.buffer_info.usage_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }
    node.type = zest_resource_type_buffer;
    node.frame_graph = frame_graph;
	node.buffer_desc.buffer_info.flags = zest_memory_pool_flag_single_buffer;
    node.current_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
    node.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
    node.producer_pass_idx = -1;
    ZEST__FLAG(node.flags, zest_resource_node_flag_transient);
    return zest__add_frame_graph_resource(&node);
}

zest_resource_node zest_AddTransientLayerResource(const char *name, const zest_layer_handle layer_handle, zest_bool prev_fif) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    zest_size layer_size = layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
    if (layer_size == 0) {
        return NULL;
    }
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    ZEST__MAYBE_FLAG(layer->flags, zest_layer_flag_use_prev_fif, prev_fif);
    zest_resource_node resource = 0;
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
        zest_buffer_resource_info_t buffer_desc = { 0 };
        buffer_desc.size = layer_size;
        buffer_desc.usage_hints = zest_resource_usage_hint_vertex_buffer;
        resource = zest_AddTransientBufferResource(name, &buffer_desc);
    } else {
        zest_uint fif = prev_fif ? layer->prev_fif : layer->fif;
        zest_buffer buffer = layer->memory_refs[fif].device_vertex_data;
		zest_resource_node_t node = zest__create_import_buffer_resource_node(name, buffer);
		resource = zest__add_frame_graph_resource(&node);
		resource->access_mask = buffer->backend->last_access_mask;
        resource->bindless_index[0] = layer->memory_refs[fif].device_vertex_data->array_index;
    }
    resource->user_data = layer;
    resource->buffer_provider = zest__instance_layer_resource_provider;
    layer->vertex_buffer_node = resource;
    return resource;
}

zest_resource_node_t zest__create_import_buffer_resource_node(const char *name, zest_buffer buffer) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    zest_resource_node_t node = { 0 };
    node.name = name;
	node.id = frame_graph->id_counter++;
    node.first_usage_pass_idx = ZEST_INVALID;
	node.type = zest_resource_type_buffer;
    node.frame_graph = frame_graph;
    node.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
    node.storage_buffer = buffer;
    node.current_queue_family_index = buffer->owner_queue_family;
    node.producer_pass_idx = -1;
	ZEST__FLAG(node.flags, zest_resource_node_flag_imported);
	ZEST__FLAG(node.flags, zest_resource_node_flag_essential_output);
    return node;
}

zest_resource_node_t zest__create_import_image_resource_node(const char *name, zest_image image) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    zest_resource_node_t node = { 0 };
    node.name = name;
	node.id = frame_graph->id_counter++;
    node.first_usage_pass_idx = ZEST_INVALID;
	node.type = zest_resource_type_image;
    node.frame_graph = frame_graph;
    node.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
    node.image = *image;
    node.view = image->default_view;
    node.current_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
    node.producer_pass_idx = -1;
	ZEST__FLAG(node.flags, zest_resource_node_flag_imported);
	ZEST__FLAG(node.flags, zest_resource_node_flag_essential_output);
    return node;
}

zest_resource_node zest_ImportImageResource(const char *name, zest_image_handle handle, zest_resource_image_provider provider) {
    zest_image image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, handle.value);
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    zest_resource_node_t resource = zest__create_import_image_resource_node(name, image);
	ZEST__FLAG(resource.flags, zest_resource_node_flag_imported);
    resource.image_provider = provider;
    zest_resource_node node = zest__add_frame_graph_resource(&resource);
    node->image_layout = image->info.layout;
    node->linked_layout = &image->info.layout;
    return node;
}

zest_resource_node zest_ImportBufferResource(const char *name, zest_buffer buffer, zest_resource_buffer_provider provider) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    zest_resource_node_t resource = zest__create_import_buffer_resource_node(name, buffer);
    resource.buffer_provider = provider;
    zest_resource_node node = zest__add_frame_graph_resource(&resource);
    node->access_mask = buffer->backend->last_access_mask;
    return node;
}

zest_resource_node zest__import_swapchain_resource(zest_swapchain swapchain) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    ZestRenderer->current_frame_graph->swapchain = swapchain;
    if (ZEST__NOT_FLAGGED(ZestRenderer->flags, zest_renderer_flag_swap_chain_was_acquired)) {
        ZEST_PRINT("WARNING: Swap chain is being imported but no swap chain image has been acquired. You can use zest_BeginRenderToScreen() to properly acquire a swap chain image.");
    }
    zest_resource_node_t node = { 0 };
    node.swapchain = swapchain;
    node.name = swapchain->name;
	node.id = frame_graph->id_counter++;
    node.first_usage_pass_idx = ZEST_INVALID;
	node.type = zest_resource_type_swap_chain_image;
    node.frame_graph = frame_graph;
    node.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
    node.image.backend = zest__new_frame_graph_image_backend(ZestRenderer->frame_graph_allocator[ZEST_FIF]);
    node.image.backend->vk_format = (VkFormat)swapchain->format;
    node.image.backend->vk_image = swapchain->images[swapchain->current_image_frame].backend->vk_image;
    node.image.info = swapchain->images[0].info;
    node.current_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
    node.producer_pass_idx = -1;
    node.image_provider = zest__swapchain_resource_provider;
	ZEST__FLAG(node.flags, zest_resource_node_flag_imported);
	ZEST__FLAG(node.flags, zest_resource_node_flag_essential_output);
    frame_graph->swapchain_resource = zest__add_frame_graph_resource(&node);
    frame_graph->swapchain_resource->image_layout = zest_image_layout_undefined;
    frame_graph->swapchain_resource->last_stage_mask = zest_pipeline_stage_color_attachment_output_bit;
    return frame_graph->swapchain_resource;
}

void zest_SetPassTask(zest_rg_execution_callback callback, void *user_data) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_pass);        //No current pass found, make sure you call zest_BeginPass
    zest_pass_node pass = ZestRenderer->current_pass;
    zest_pass_execution_callback_t callback_data;
    callback_data.callback = callback;
    callback_data.user_data = user_data;
    pass->execution_callback = callback_data;
}

void zest_SetPassInstanceLayerUpload(zest_layer_handle layer_handle) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_pass);        //No current pass found, make sure you call zest_BeginPass
    zest_pass_node pass = ZestRenderer->current_pass;
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    if (ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif) && layer->dirty[layer->fif] == 0) {
        return;
    }
    zest_pass_execution_callback_t callback_data;
    callback_data.callback = zest_UploadInstanceLayerData;
    callback_data.user_data = (void*)(uintptr_t)layer_handle.value;
    pass->execution_callback = callback_data;
}

void zest_SetPassInstanceLayer(zest_layer_handle layer_handle) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_pass);        //No current pass found, make sure you call zest_BeginPass
    zest_pass_node pass = ZestRenderer->current_pass;
    zest_pass_execution_callback_t callback_data;
    callback_data.callback = zest_cmd_DrawInstanceLayer;
    callback_data.user_data = (void*)(uintptr_t)layer_handle.value;
    pass->execution_callback = callback_data;
}

zest_pass_node zest__add_pass_node(const char *name, zest_device_queue_type queue_type) {
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_pass_node_t node = { 0 };
    node.id = frame_graph->id_counter++;
    node.queue_info.queue_type = queue_type;
    node.name = name;
    node.magic = zest_INIT_MAGIC(zest_struct_type_pass_node);
    node.output_key = queue_type;
    switch (queue_type) {
    case zest_queue_graphics:
        node.queue_info.timeline_wait_stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        node.type = zest_pass_type_graphics;
        break;
    case zest_queue_compute:
        node.queue_info.timeline_wait_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        node.type = zest_pass_type_compute;
        break;
    case zest_queue_transfer:
        node.queue_info.timeline_wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        node.type = zest_pass_type_transfer;
        break;
    }
    zest_pass_node pass_node = zest_bucket_array_linear_add(ZestRenderer->frame_graph_allocator[ZEST_FIF], &frame_graph->potential_passes, zest_pass_node_t);
    *pass_node = node;
    return pass_node;
}

zest_resource_node zest__add_frame_graph_resource(zest_resource_node resource) {
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zloc_linear_allocator_t *allocator = ZestRenderer->frame_graph_allocator[ZEST_FIF];
    zest_resource_node node = zest_bucket_array_linear_add(allocator, &frame_graph->resources, zest_resource_node_t);
    *node = *resource;
    for (int i = 0; i != zest_max_global_binding_number; ++i) {
        node->bindless_index[i] = ZEST_INVALID;
    }
    zest_resource_versions_t resource_version = { 0 };
    node->version = 0;
    node->original_id = node->id;
    zest_vec_linear_push(allocator, resource_version.resources, node);
    zest_map_insert_linear_key(allocator, frame_graph->resource_versions, resource->id, resource_version);
    return node;
}

zest_resource_versions_t *zest__maybe_add_resource_version(zest_resource_node resource) {
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_resource_versions_t *versions = zest_map_at_key(frame_graph->resource_versions, resource->original_id);
    zest_resource_node last_version = zest_vec_back(versions->resources);
    if (ZEST__FLAGGED(last_version->flags, zest_resource_node_flag_has_producer)) {
		zest_resource_node_t new_resource = { 0 };
		new_resource.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
		new_resource.name = last_version->name;
		new_resource.aliased_resource = versions->resources[0];
		new_resource.image.info = resource->image.info;
		new_resource.buffer_desc = resource->buffer_desc;
		new_resource.id = frame_graph->id_counter++;
        new_resource.version = last_version->version + 1;
        new_resource.original_id = last_version->original_id;
        new_resource.producer_pass_idx = -1;
		ZEST__FLAG(new_resource.flags, zest_resource_node_flag_aliased);
        ZEST__FLAG(new_resource.flags, zest_resource_node_flag_has_producer);
		zloc_linear_allocator_t *allocator = ZestRenderer->frame_graph_allocator[ZEST_FIF];
		zest_resource_node node = zest_bucket_array_linear_add(allocator, &frame_graph->resources, zest_resource_node_t);
        *node = new_resource;
        zest_vec_linear_push(allocator, versions->resources, node);
    }
	ZEST__FLAG(resource->flags, zest_resource_node_flag_has_producer);
    return versions;
}

zest_pass_node zest_BeginGraphicBlankScreen(const char *name) {
    zest_pass_node pass = zest__add_pass_node(name, zest_queue_graphics);
    ZestRenderer->current_pass = pass;
    zest_SetPassTask(zest_EmptyRenderPass, 0);
    return pass;
}

zest_pass_node zest_BeginRenderPass(const char *name) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    ZEST_ASSERT(!ZestRenderer->current_pass);   //Already begun a pass. Make sure that you call zest_EndPass before starting a new one
    zest_pass_node node = zest__add_pass_node(name, zest_queue_graphics);
    node->queue_info.timeline_wait_stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    ZestRenderer->current_pass = node;
    return node;
}

zest_pass_node zest_BeginComputePass(zest_compute_handle compute_handle, const char *name) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    ZEST_ASSERT(!ZestRenderer->current_pass);   //Already begun a pass. Make sure that you call zest_EndPass before starting a new one
    bool force_graphics_queue = (ZestRenderer->current_frame_graph->flags & zest_frame_graph_force_on_graphics_queue) > 0;
    zest_pass_node node = zest__add_pass_node(name, force_graphics_queue ? zest_queue_graphics : zest_queue_compute);
    zest_compute compute = (zest_compute)zest__get_store_resource_checked(&ZestRenderer->compute_pipelines, compute_handle.value);
    node->compute = compute;
    node->queue_info.timeline_wait_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    ZestRenderer->current_pass = node;
    return node;
}

zest_pass_node zest_BeginTransferPass(const char *name) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    ZEST_ASSERT(!ZestRenderer->current_pass);   //Already begun a pass. Make sure that you call zest_EndPass before starting a new one
    bool force_graphics_queue = (ZestRenderer->current_frame_graph->flags & zest_frame_graph_force_on_graphics_queue) > 0;
    zest_pass_node node = zest__add_pass_node(name, force_graphics_queue ? zest_queue_graphics : zest_queue_transfer);
    node->queue_info.timeline_wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    ZestRenderer->current_pass = node;
    return node;
}

void zest_EndPass() {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_pass); //No begin pass found, make sure you call BeginRender/Compute/TransferPass
    ZestRenderer->current_pass = 0;
}

zest_resource_node zest_GetPassInputResource(const zest_frame_graph_context context, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(context->pass_node->inputs, name));  //Not a valid input resource name. Check the name and also maybe you meant to get from outputs?
    zest_resource_usage_t *usage = zest_map_at(context->pass_node->inputs, name);
    return ZEST_VALID_HANDLE(usage->resource_node->aliased_resource) ? usage->resource_node->aliased_resource : usage->resource_node;
}

zest_buffer zest_GetPassInputBuffer(const zest_frame_graph_context context, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(context->pass_node->inputs, name));  //Not a valid input resource name. Check the name and also maybe you meant to get from outputs?
    zest_resource_usage_t *usage = zest_map_at(context->pass_node->inputs, name);
    zest_resource_node resource = ZEST_VALID_HANDLE(usage->resource_node->aliased_resource) ? usage->resource_node->aliased_resource : usage->resource_node;
    return resource->storage_buffer;
}

zest_buffer zest_GetPassOutputBuffer(const zest_frame_graph_context context, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(context->pass_node->outputs, name));  //Not a valid input resource name. Check the name and also maybe you meant to get from inputs?
    zest_resource_usage_t *usage = zest_map_at(context->pass_node->outputs, name);
    zest_resource_node resource = ZEST_VALID_HANDLE(usage->resource_node->aliased_resource) ? usage->resource_node->aliased_resource : usage->resource_node;
    return resource->storage_buffer;
}

zest_resource_node zest_GetPassOutputResource(const zest_frame_graph_context context, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(context->pass_node->outputs, name));  //Not a valid output resource name. Check the name and also maybe you meant to get from inputs?
    zest_resource_usage_t *usage = zest_map_at(context->pass_node->outputs, name);
    return ZEST_VALID_HANDLE(usage->resource_node->aliased_resource) ? usage->resource_node->aliased_resource : usage->resource_node;
}

zest_uint zest_GetTransientImageBindlessIndex(const zest_frame_graph_context context, zest_resource_node resource, zest_sampler_handle sampler_handle, zest_global_binding_number binding_number) {
    ZEST_PRINT_FUNCTION;
    ZEST_ASSERT_HANDLE(resource);            // Not a valid resource handle
    ZEST_ASSERT(resource->type & zest_resource_type_is_image);  //Must be an image resource type
    zest_frame_graph frame_graph = context->frame_graph;
    if (resource->bindless_index[binding_number] != ZEST_INVALID) return resource->bindless_index[binding_number];
    zest_set_layout bindless_layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, frame_graph->bindless_layout.value);
    zest_sampler sampler = (zest_sampler)zest__get_store_resource_checked(&ZestRenderer->samplers, sampler_handle.value);
    zest_uint bindless_index = zest__acquire_bindless_index(bindless_layout, binding_number);
    VkDescriptorImageInfo image_buffer_info = { 0 };
    VkDescriptorType descriptor_type = bindless_layout->backend->descriptor_indexes[binding_number].descriptor_type;
    ZEST_ASSERT(resource->current_state_index < zest_vec_size(resource->journey));
	image_buffer_info.imageLayout = zest__to_vk_image_layout(resource->journey[resource->current_state_index].usage.image_layout);
	image_buffer_info.imageView = resource->view->backend->vk_view;
	image_buffer_info.sampler = sampler->backend->vk_sampler;

    VkWriteDescriptorSet write = zest_CreateImageDescriptorWriteWithType(frame_graph->bindless_set->backend->vk_descriptor_set, &image_buffer_info, binding_number, descriptor_type);
    write.dstArrayElement = bindless_index;
    vkUpdateDescriptorSets(ZestDevice->backend->logical_device, 1, &write, 0, 0);

    zest_binding_index_for_release_t binding_index = { frame_graph->bindless_layout, bindless_index, binding_number };
    zest_vec_push(ZestRenderer->deferred_resource_freeing_list.binding_indexes[ZEST_FIF], binding_index);

    resource->bindless_index[binding_number];
    return bindless_index;
}

zest_uint *zest_GetTransientMipBindlessIndexes(const zest_frame_graph_context context, zest_resource_node resource, zest_sampler_handle sampler_handle, zest_global_binding_number binding_number) {
    ZEST_PRINT_FUNCTION;
    zest_set_layout bindless_layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, context->frame_graph->bindless_layout.value);
    ZEST_ASSERT(resource->type & zest_resource_type_is_image);  //Must be an image resource type
    ZEST_ASSERT(resource->image.info.mip_levels > 1);   //The resource does not have any mip levels. Make sure to set the number of mip levels when creating the resource in the frame graph
    ZEST_ASSERT(resource->current_state_index < zest_vec_size(resource->journey));
    zest_frame_graph frame_graph = context->frame_graph;

    if (zest_map_valid_key(resource->image.mip_indexes, (zest_key)binding_number)) {
        zest_mip_index_collection *mip_collection = zest_map_at_key(resource->image.mip_indexes, (zest_key)binding_number);
        return mip_collection->mip_indexes;
    }
    zloc_linear_allocator_t *allocator = ZestRenderer->frame_graph_allocator[ZEST_FIF];
    if (!resource->view_array) {
        resource->view_array = zest__create_image_views_per_mip_backend(&resource->image, zest_image_view_type_2d, 0, resource->image.info.layer_count, allocator);
    }
    zest_mip_index_collection mip_collection = { 0 };
    zest_sampler sampler = (zest_sampler)zest__get_store_resource_checked(&ZestRenderer->samplers, sampler_handle.value);
	for (int mip_index = 0; mip_index != resource->view_array->count; ++mip_index) {
		zest_uint bindless_index = zest__acquire_bindless_index(bindless_layout, binding_number);
		zest_vec_linear_push(allocator, resource->mip_level_bindless_indexes, bindless_index);

		VkDescriptorImageInfo mip_buffer_info;
		mip_buffer_info.imageLayout = zest__to_vk_image_layout(resource->journey[resource->current_state_index].usage.image_layout);
		mip_buffer_info.imageView = resource->view_array->views[mip_index].backend->vk_view;
		mip_buffer_info.sampler = sampler->backend->vk_sampler;

		VkWriteDescriptorSet write = zest_CreateImageDescriptorWriteWithType(frame_graph->bindless_set->backend->vk_descriptor_set, &mip_buffer_info, binding_number, bindless_layout->backend->descriptor_indexes[binding_number].descriptor_type);
		write.dstArrayElement = bindless_index;
		vkUpdateDescriptorSets(ZestDevice->backend->logical_device, 1, &write, 0, 0);

		zest_binding_index_for_release_t mip_binding_index = { frame_graph->bindless_layout, bindless_index, binding_number };
		zest_vec_push(ZestRenderer->deferred_resource_freeing_list.binding_indexes[ZEST_FIF], mip_binding_index);
		zest_vec_linear_push(allocator, mip_collection.mip_indexes, bindless_index );
	}
    zest_map_insert_linear_key(allocator, resource->image.mip_indexes, (zest_key)binding_number, mip_collection);
    return mip_collection.mip_indexes;
}

zest_uint zest_GetTransientBufferBindlessIndex(const zest_frame_graph_context context, zest_resource_node resource) {
    ZEST_PRINT_FUNCTION;
    zest_set_layout bindless_layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, context->frame_graph->bindless_layout.value);
    ZEST_ASSERT_HANDLE(resource);   //Not a valid resource handle
    ZEST_ASSERT(resource->type == zest_resource_type_buffer);   //Must be a buffer resource type for this bindlesss index acquisition
    if (resource->bindless_index[0] != ZEST_INVALID) return resource->bindless_index[0];
    zest_frame_graph frame_graph = context->frame_graph;
	zest_uint bindless_index = zest__acquire_bindless_index(bindless_layout, zest_storage_buffer_binding);
	VkDescriptorBufferInfo buffer_info;
	buffer_info.buffer = resource->storage_buffer->backend->vk_buffer;
	buffer_info.offset = resource->storage_buffer->buffer_offset;
	buffer_info.range = resource->storage_buffer->size;

	VkWriteDescriptorSet write = zest_CreateBufferDescriptorWriteWithType(frame_graph->bindless_set->backend->vk_descriptor_set, &buffer_info, zest_storage_buffer_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	write.dstArrayElement = bindless_index;
	vkUpdateDescriptorSets(ZestDevice->backend->logical_device, 1, &write, 0, 0);

	zest_binding_index_for_release_t binding_index = { frame_graph->bindless_layout, bindless_index, zest_storage_buffer_binding };
	zest_vec_push(ZestRenderer->deferred_resource_freeing_list.binding_indexes[ZEST_FIF], binding_index);
    resource->bindless_index[0] = bindless_index;
    return bindless_index;
}

zest_uint zest_GetResourceMipLevels(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(resource);	//Not a valid resource handle!
    return resource->image.info.mip_levels;
}

zest_uint zest_GetResourceWidth(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(resource);	//Not a valid resource handle!
    return resource->image.info.extent.width;
}

zest_uint zest_GetResourceHeight(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(resource);	//Not a valid resource handle!
    return resource->image.info.extent.height;
}

zest_image zest_GetResourceImage(zest_resource_node resource) {
	ZEST_ASSERT_HANDLE(resource);   //Not a valid resource handle!
	if (resource->type & zest_resource_type_is_image_or_depth) {
        return &resource->image;
	}
	return VK_NULL_HANDLE;
}

zest_resource_type zest_GetResourceType(zest_resource_node resource_node) {
	ZEST_ASSERT_HANDLE(resource_node);   //Not a valid resource handle!
	return resource_node->type;
}

zest_image_info_t zest_GetResourceImageDescription(zest_resource_node resource_node) {
	ZEST_ASSERT_HANDLE(resource_node);   //Not a valid resource handle!
	return resource_node->image.info;
}

void zest_SetResourceClearColor(zest_resource_node resource, float red, float green, float blue, float alpha) {
    ZEST_ASSERT_HANDLE(resource);   //Not a valid resource handle!
    resource->clear_color = (zest_vec4){red, green, blue, alpha};
}

void zest__add_pass_buffer_usage(zest_pass_node pass_node, zest_resource_node resource, zest_resource_purpose purpose, zest_pipeline_stage_flags relevant_pipeline_stages, zest_bool is_output) {
    zest_resource_usage_t usage = { 0 };    
    usage.resource_node = resource;
    usage.purpose = purpose;

    switch (purpose) {
    case zest_purpose_vertex_buffer:
        usage.access_mask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        usage.stage_mask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        resource->buffer_desc.buffer_info.usage_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        break;
    case zest_purpose_index_buffer:
        usage.access_mask = VK_ACCESS_INDEX_READ_BIT;
        usage.stage_mask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        resource->buffer_desc.buffer_info.usage_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        break;
    case zest_purpose_uniform_buffer:
        usage.access_mask = VK_ACCESS_UNIFORM_READ_BIT;
        usage.stage_mask = relevant_pipeline_stages; 
        resource->buffer_desc.buffer_info.usage_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        break;
    case zest_purpose_storage_buffer_read:
        usage.access_mask = VK_ACCESS_SHADER_READ_BIT;
        usage.stage_mask = relevant_pipeline_stages;
        resource->buffer_desc.buffer_info.usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        break;
    case zest_purpose_storage_buffer_write:
        usage.access_mask = VK_ACCESS_SHADER_WRITE_BIT;
        usage.stage_mask = relevant_pipeline_stages;
        resource->buffer_desc.buffer_info.usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        break;
    case zest_purpose_storage_buffer_read_write:
        usage.access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        usage.stage_mask = relevant_pipeline_stages;
        resource->buffer_desc.buffer_info.usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        break;
    case zest_purpose_transfer_buffer:
        usage.access_mask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        usage.stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        resource->buffer_desc.buffer_info.usage_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        break;
    default:
        ZEST_ASSERT(0);     //Unhandled buffer access purpose! Make sure you pass in a valid zest_resource_purpose
        return;
    }

    if (is_output) { // Or derive is_output from purpose (e.g. WRITE implies output)
        zest_map_linear_insert(ZestRenderer->frame_graph_allocator[ZEST_FIF], pass_node->outputs, resource->name, usage);
		pass_node->output_key += resource->id + zest_Hash(&usage, sizeof(zest_resource_usage_t), 0);
    } else {
		resource->reference_count++;
        zest_map_linear_insert(ZestRenderer->frame_graph_allocator[ZEST_FIF], pass_node->inputs, resource->name, usage);
    }
}

zest_render_pass zest__create_render_pass() {
    zest_render_pass render_pass = ZEST__NEW(zest_render_pass);
    *render_pass = (zest_render_pass_t){ 0 };
    render_pass->magic = zest_INIT_MAGIC(zest_struct_type_render_pass);
    return render_pass;
}

zest_submission_batch_t *zest__get_submission_batch(zest_uint submission_id) {
    zest_uint submission_index = ZEST__SUBMISSION_INDEX(submission_id);
    zest_uint queue_index = ZEST__QUEUE_INDEX(submission_id);
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);
    return &frame_graph->submissions[submission_index].batches[queue_index];
}

void zest__set_rg_error_status(zest_frame_graph frame_graph, zest_frame_graph_result result) {
    ZEST__FLAG(frame_graph->error_status, result);
	ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_building_frame_graph);
}

zest_image_usage_flags zest__get_image_usage_from_state(zest_resource_state state) {
    switch (state) {
    case zest_resource_state_render_target:
        return zest_image_usage_color_attachment_bit;
    case zest_resource_state_depth_stencil_write:
    case zest_resource_state_depth_stencil_read:
        return zest_image_usage_depth_stencil_attachment_bit;
    case zest_resource_state_shader_read:
        // This state could be fulfilled by a sampled image or an input attachment.
        // To be safe, you can require both, or have more specific states.
        // Let's assume for now it implies sampled.
        return zest_image_usage_sampled_bit;
    case zest_resource_state_unordered_access:
        return zest_image_usage_storage_bit;
    case zest_resource_state_copy_src:
        return zest_image_usage_transfer_src_bit;
    case zest_resource_state_copy_dst:
        return zest_image_usage_transfer_dst_bit;
    case zest_resource_state_present:
        // Presenting requires being a color attachment.
        return zest_image_usage_color_attachment_bit;
    default:
        return 0;
    }
}

zest_vk_barrier_info_t zest__purpose_to_barrier_info(zest_resource_purpose purpose, zest_supported_pipeline_stages relevant_pipeline_stages, zest_load_op load_op, zest_load_op stencil_load_op) {
    zest_vk_barrier_info_t barrier_info;
    switch (purpose) {
    case zest_purpose_color_attachment_write:
        barrier_info.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier_info.access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        if (load_op == zest_load_op_load) {
            barrier_info.access_mask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT; // For the load itself
        }
        barrier_info.stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        break;

    case zest_purpose_color_attachment_read:
        barrier_info.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier_info.access_mask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        barrier_info.stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        break;

    case zest_purpose_sampled_image:
        barrier_info.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier_info.access_mask = VK_ACCESS_SHADER_READ_BIT;
        barrier_info.stage_mask = relevant_pipeline_stages;
        break;

    case zest_purpose_storage_image_read:
        barrier_info.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier_info.access_mask = VK_ACCESS_SHADER_READ_BIT;
        barrier_info.stage_mask = relevant_pipeline_stages;
        break;

    case zest_purpose_storage_image_write:
        barrier_info.image_layout = VK_IMAGE_LAYOUT_GENERAL;
        barrier_info.access_mask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier_info.stage_mask = relevant_pipeline_stages;
        break;

    case zest_purpose_storage_image_read_write:
        barrier_info.image_layout = VK_IMAGE_LAYOUT_GENERAL;
        barrier_info.access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barrier_info.stage_mask = relevant_pipeline_stages;
        break;

    case zest_purpose_depth_stencil_attachment_read:
        barrier_info.image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        barrier_info.access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        barrier_info.stage_mask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        break;

    case zest_purpose_depth_stencil_attachment_write:
        barrier_info.image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier_info.access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        if (load_op == zest_load_op_load || stencil_load_op == zest_load_op_load) {
            barrier_info.access_mask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }
        barrier_info.stage_mask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        break;

    case zest_purpose_depth_stencil_attachment_read_write: // Typical depth testing and writing
        barrier_info.image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier_info.access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier_info.stage_mask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        break;

    case zest_purpose_input_attachment: // For subpass reads
        barrier_info.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier_info.access_mask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        barrier_info.stage_mask = relevant_pipeline_stages;
        break;

    case zest_purpose_transfer_image:
        barrier_info.image_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier_info.access_mask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier_info.stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;

    case zest_purpose_present_src:
        barrier_info.image_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier_info.access_mask = 0; // No specific GPU access by the pass itself for this state.
        barrier_info.stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // Ensure all prior work is done.
        break;

    default:
        ZEST_ASSERT(0); //Unhandled image access purpose!"
    }
    return barrier_info;
}

zest_resource_usage_t zest__configure_image_usage(zest_resource_node resource, zest_resource_purpose purpose, zest_texture_format format, zest_load_op load_op, zest_load_op stencil_load_op, zest_pipeline_stage_flags relevant_pipeline_stages) {

    zest_resource_usage_t usage = { 0 };

    usage.resource_node = resource;
    usage.aspect_flags = zest__determine_aspect_flag(format);   // Default based on format
    usage.load_op = load_op;
    usage.stencil_load_op = stencil_load_op;

    switch (purpose) {
    case zest_purpose_color_attachment_write:
        usage.image_layout = zest_image_layout_color_attachment_optimal;
        usage.access_mask = zest_access_color_attachment_write_bit;
        // If load_op is zest_attachment_load_op_load, AN IMPLICIT READ OCCURS.
        // If blending with destination, zest_access_color_attachment_read_bit IS ALSO INVOLVED.
        // For simplicity, if this is purely about the *write* aspect:
        if (load_op == zest_load_op_load) {
            usage.access_mask |= zest_access_color_attachment_read_bit; // fOR THE LOAD ITSElf
        }
        usage.stage_mask = zest_pipeline_stage_color_attachment_output_bit;
        usage.aspect_flags = zest_image_aspect_color_bit;
        usage.is_output = ZEST_TRUE;
        resource->image.info.flags |= zest_image_flag_color_attachment;
        break;

    case zest_purpose_color_attachment_read:
        usage.image_layout = zest_image_layout_color_attachment_optimal;
        usage.access_mask = zest_access_color_attachment_read_bit;
        usage.stage_mask = zest_pipeline_stage_color_attachment_output_bit;
        usage.aspect_flags = zest_image_aspect_color_bit;
        resource->image.info.flags |= zest_image_flag_color_attachment;
        usage.is_output = ZEST_FALSE;
        break;

    case zest_purpose_sampled_image:
        usage.image_layout = zest_image_layout_shader_read_only_optimal;
        usage.access_mask = zest_access_shader_read_bit;
        usage.stage_mask = relevant_pipeline_stages;
        resource->image.info.flags |= zest_image_flag_sampled;
        usage.is_output = ZEST_FALSE;
        break;

    case zest_purpose_storage_image_read:
        usage.image_layout = zest_image_layout_shader_read_only_optimal;
        usage.access_mask = zest_access_shader_read_bit;
        usage.stage_mask = relevant_pipeline_stages;
        resource->image.info.flags |= zest_image_flag_sampled | zest_image_flag_storage;
        usage.is_output = ZEST_FALSE;
        break;

    case zest_purpose_storage_image_write:
        usage.image_layout = zest_image_layout_general;
        usage.access_mask = zest_access_shader_write_bit;
        usage.stage_mask = relevant_pipeline_stages;
        resource->image.info.flags |= zest_image_flag_storage;
        usage.is_output = ZEST_TRUE;
        break;

    case zest_purpose_storage_image_read_write:
        usage.image_layout = zest_image_layout_general;
        usage.access_mask = zest_access_shader_read_bit | zest_access_shader_write_bit;
        usage.stage_mask = relevant_pipeline_stages;
        resource->image.info.flags |= zest_image_flag_sampled | zest_image_flag_storage;
        usage.is_output = ZEST_TRUE;
        break;

    case zest_purpose_depth_stencil_attachment_read:
        usage.image_layout = zest_image_layout_depth_stencil_read_only_optimal;
        usage.access_mask = zest_access_depth_stencil_attachment_read_bit;
        usage.stage_mask = zest_pipeline_stage_early_fragment_tests_bit | zest_pipeline_stage_late_fragment_tests_bit;
        usage.aspect_flags = zest_image_aspect_depth_bit;
        resource->image.info.flags |= zest_image_flag_depth_stencil_attachment;
        usage.is_output = ZEST_FALSE;
        break;

    case zest_purpose_depth_stencil_attachment_write:
        usage.image_layout = zest_image_layout_depth_stencil_attachment_optimal;
        usage.access_mask = zest_access_depth_stencil_attachment_write_bit;
        if (load_op == zest_load_op_load || stencil_load_op == zest_load_op_load) {
            usage.access_mask |= zest_access_depth_stencil_attachment_read_bit;
        }
        usage.aspect_flags = zest_image_aspect_depth_bit;
        usage.stage_mask = zest_pipeline_stage_early_fragment_tests_bit | zest_pipeline_stage_late_fragment_tests_bit;
        resource->image.info.flags |= zest_image_flag_depth_stencil_attachment;
        usage.is_output = ZEST_TRUE;
        break;

    case zest_purpose_depth_stencil_attachment_read_write: // Typical depth testing and writing
        usage.image_layout = zest_image_layout_depth_stencil_attachment_optimal;
        usage.access_mask = zest_access_depth_stencil_attachment_read_bit | zest_access_depth_stencil_attachment_write_bit;
        usage.stage_mask = zest_pipeline_stage_early_fragment_tests_bit | zest_pipeline_stage_late_fragment_tests_bit;
        usage.aspect_flags = zest_image_aspect_depth_bit;
        resource->image.info.flags |= zest_image_flag_depth_stencil_attachment;
        usage.is_output = ZEST_TRUE; // Modifies resource
        break;

    case zest_purpose_input_attachment: // For subpass reads
        usage.image_layout = zest_image_layout_shader_read_only_optimal;
        usage.access_mask = zest_access_input_attachment_read_bit;
        usage.stage_mask = relevant_pipeline_stages;
        resource->image.info.flags |= zest_image_flag_input_attachment;
        usage.is_output = ZEST_FALSE;
        break;

    case zest_purpose_transfer_image:
        usage.image_layout = zest_image_layout_transfer_src_optimal;
        usage.access_mask = zest_access_transfer_read_bit;
        usage.stage_mask = zest_pipeline_stage_transfer_bit;
        resource->image.info.flags |= zest_image_flag_transfer_src | zest_image_flag_transfer_dst;
        usage.is_output = ZEST_FALSE;
        break;

    case zest_purpose_present_src:
        usage.image_layout = zest_image_layout_present;
        usage.access_mask = 0; // No specific GPU access by the pass itself for this state.
        usage.stage_mask = zest_pipeline_stage_bottom_of_pipe_bit; // eNSURE ALL PRIOR WORK IS DONE.
        resource->image.info.flags |= zest_image_flag_color_attachment;
        usage.is_output = ZEST_TRUE;
        break;

    default:
        ZEST_ASSERT(0); //Unhandled image access purpose!"
    }
	return usage;
}

void zest__add_pass_image_usage(zest_pass_node pass_node, zest_resource_node image_resource, zest_resource_purpose purpose, zest_pipeline_stage_flags relevant_pipeline_stages, zest_bool is_output, zest_load_op load_op, zest_store_op store_op, zest_load_op stencil_load_op, zest_store_op stencil_store_op, zest_clear_value_t clear_value) {
    zest_resource_usage_t usage = zest__configure_image_usage(image_resource, purpose, image_resource->image.info.format, load_op, stencil_load_op, relevant_pipeline_stages);

    // Attachment ops are only relevant for attachment purposes
    // For other usages, they'll default to 0/DONT_CARE which is fine.
    usage.store_op = store_op;
    usage.stencil_store_op = stencil_store_op;
    usage.clear_value = clear_value;
    usage.purpose = purpose;
  
    if (usage.is_output) {
		//This is the first time this resource has been used as output
		usage.resource_node = image_resource;
		zest_map_linear_insert(ZestRenderer->frame_graph_allocator[ZEST_FIF], pass_node->outputs, image_resource->name, usage);
		pass_node->output_key += image_resource->id + zest_Hash(&usage, sizeof(zest_resource_usage_t), 0);
    } else {
        usage.resource_node = image_resource;
        ZEST_ASSERT(usage.resource_node);
		image_resource->reference_count++;
		zest_map_linear_insert(ZestRenderer->frame_graph_allocator[ZEST_FIF], pass_node->inputs, image_resource->name, usage);
    }
}

void zest_ReleaseBufferAfterUse(zest_resource_node node) {
    ZEST_ASSERT(ZEST__FLAGGED(node->flags, zest_resource_node_flag_imported));  //The resource must be imported, transient buffers are simply discarded after use.
    ZEST__FLAG(node->flags, zest_resource_node_flag_release_after_use);
}

// --- Image Helpers ---
void zest_ConnectInput(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);  //This function must be called withing a Being/EndRenderGraph block
    ZEST_ASSERT_HANDLE(ZestRenderer->current_pass);          //No current pass found. Make sure you call zest_BeginPass
    zest_pass_node pass = ZestRenderer->current_pass;
    if(!ZEST_VALID_HANDLE(resource)) return;  
    zest_supported_pipeline_stages stages = 0;
    zest_resource_purpose purpose = 0;
    if (resource->type & zest_resource_type_is_image) {
        switch (pass->type) {
        case zest_pass_type_graphics:
            stages = zest_pipeline_fragment_stage;
            purpose = zest_purpose_sampled_image;
            break;
        case zest_pass_type_compute:
            stages = zest_pipeline_compute_stage;
            purpose = zest_purpose_storage_image_read;
            break;
        case zest_pass_type_transfer:
            stages = zest_pipeline_transfer_stage;
            purpose = zest_purpose_transfer_image;
            break;
        }
        zest__add_pass_image_usage(pass, resource, purpose, stages, ZEST_FALSE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, (zest_clear_value_t) { 0 });
    } else {
        //Buffer input
        switch (pass->type) {
        case zest_pass_type_graphics: {
            stages = zest_pipeline_vertex_input_stage;
            purpose = zest_purpose_vertex_buffer;
            break;
        }
        case zest_pass_type_compute: {
            stages = zest_pipeline_compute_stage;
            purpose = zest_purpose_storage_buffer_read_write;
            break;
        }
        case zest_pass_type_transfer: {
            stages = zest_pipeline_transfer_stage;
            purpose = zest_purpose_transfer_buffer;
            break;
        }
        }
		zest__add_pass_buffer_usage(pass, resource, purpose, stages, ZEST_FALSE);
    }
}

void zest_ConnectSwapChainOutput() {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);  //This function must be called withing a Being/EndRenderGraph block
    ZEST_ASSERT_HANDLE(ZestRenderer->current_pass); //No current pass found. Make sure you call zest_BeginPass
    zest_pass_node pass = ZestRenderer->current_pass;
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph->swapchain_resource);  //Not a valid swapchain resource, did you call zest_BeginFrameGraphSwapchain?
    zest_clear_value_t cv = frame_graph->swapchain->clear_color;
    // Assuming clear for swapchain if not explicitly loaded
    ZEST__FLAG(pass->flags, zest_pass_flag_do_not_cull);
    zest__add_pass_image_usage(pass, frame_graph->swapchain_resource, zest_purpose_color_attachment_write, 
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, ZEST_TRUE,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, cv);
}

void zest_ConnectOutput(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);  //This function must be called withing a Being/EndRenderGraph block
    ZEST_ASSERT_HANDLE(ZestRenderer->current_pass); //No current pass found. Make sure you call zest_BeginPass
    zest_pass_node pass = ZestRenderer->current_pass;
    if (!ZEST_VALID_HANDLE(resource)) return;
    if (resource->image.info.sample_count > 1) {
        ZEST__FLAG(pass->flags, zest_pass_flag_output_resolve);
    }
    if (resource->type & zest_resource_type_is_image) {
        zest_clear_value_t cv = { 0 };
        switch (pass->type) {
        case zest_pass_type_graphics: {
            ZEST_ASSERT(resource->type & zest_resource_type_is_image); //Resource must be an image buffer when used as output in a graphics pass
            if (resource->image.info.format == zest__from_vk_format(ZestRenderer->depth_format)) {
                cv.depth_stencil.depth = 1.f;
                cv.depth_stencil.stencil = 0;
                zest__add_pass_image_usage(pass, resource, zest_purpose_depth_stencil_attachment_write,
                    0, ZEST_TRUE,
                    VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    cv);
            } else {
                cv.color = resource->clear_color;
                zest__add_pass_image_usage(pass, resource, zest_purpose_color_attachment_write,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, ZEST_TRUE,
                    VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    cv);
            }
            break;
        }
        case zest_pass_type_compute: {
            zest__add_pass_image_usage(pass, resource, zest_purpose_storage_image_write, zest_pipeline_compute_stage,
                ZEST_FALSE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, (zest_clear_value_t) { 0 });
            break;
        }
        case zest_pass_type_transfer: {
            zest__add_pass_image_usage(pass, resource, zest_purpose_transfer_image, zest_pipeline_compute_stage,
                ZEST_FALSE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, (zest_clear_value_t) { 0 });
            break;
        }
        }
    } else {
        //Buffer output
        switch (pass->type) {
        case zest_pass_type_graphics: {
            ZEST_ASSERT(resource->type & zest_resource_type_is_image); //Resource must be an image buffer when used as output in a graphics pass
            zest__add_pass_buffer_usage(pass, resource, zest_purpose_storage_buffer_write,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, ZEST_TRUE);
            break;
        }
        case zest_pass_type_compute: {
            zest__add_pass_buffer_usage(pass, resource, zest_purpose_storage_buffer_write,
                zest_pipeline_compute_stage, ZEST_TRUE);
            break;
        }
        case zest_pass_type_transfer: {
            zest__add_pass_buffer_usage(pass, resource, zest_purpose_transfer_buffer, 
                zest_pipeline_compute_stage, ZEST_TRUE);
            break;
        }
        }

    }
}

void zest_ConnectGroupedOutput(zest_output_group group) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);  //This function must be called withing a Being/EndRenderGraph block
    ZEST_ASSERT_HANDLE(ZestRenderer->current_pass);          //No current pass found. Make sure you call zest_BeginPass
    zest_pass_node pass = ZestRenderer->current_pass;
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    ZEST_ASSERT_HANDLE(group);  //Not a valid render target group
    ZEST_ASSERT_HANDLE(pass);  //Not a valid pass node
    ZEST_ASSERT(zest_vec_size(group->resources));   //There are no resources in the group!
    zest_vec_foreach(i, group->resources) {
        zest_resource_node resource = group->resources[i];
        switch (resource->type) {
			case zest_resource_type_swap_chain_image: zest_ConnectSwapChainOutput(); break;
			default: zest_ConnectOutput(resource); break;
        }
    }
}

void zest_WaitOnTimeline(zest_execution_timeline timeline) {
    ZEST_ASSERT_HANDLE(timeline);    //Not a valid execution timeline. Use zest_CreateExecutionTimeline to create one
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);  //This function must be called withing a Being/EndRenderGraph block
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    zest_vec_linear_push(ZestRenderer->frame_graph_allocator[ZEST_FIF], frame_graph->wait_on_timelines, timeline);
}

void zest_SignalTimeline(zest_execution_timeline timeline) {
    ZEST_ASSERT_HANDLE(timeline);    //Not a valid execution timeline. Use zest_CreateExecutionTimeline to create one
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);  //This function must be called withing a Being/EndRenderGraph block
    zest_frame_graph frame_graph = ZestRenderer->current_frame_graph;
    zest_vec_linear_push(ZestRenderer->frame_graph_allocator[ZEST_FIF], frame_graph->signal_timelines, timeline);
}

zest_execution_timeline zest_CreateExecutionTimeline() {
    ZEST_PRINT_FUNCTION;
    zest_execution_timeline timeline = ZEST__NEW(zest_execution_timeline);
    *timeline = (zest_execution_timeline_t){ 0 };
    timeline->magic = zest_INIT_MAGIC(zest_struct_type_execution_timeline);
    timeline->current_value = 0;
    if (!ZestPlatform->create_execution_timeline_backend(timeline)) {
        if (timeline->backend) {
            ZEST__FREE(timeline->backend);
        }
		ZEST__FREE(timeline);
		return NULL;
    }
    zest_vec_push(ZestRenderer->timeline_semaphores, timeline);
    return timeline;
}

// --End frame graph functions

// [Swapchain]
zest_swapchain zest_GetMainWindowSwapchain() {
    return ZestRenderer->main_swapchain;
}

zest_texture_format zest_GetSwapchainFormat(zest_swapchain swapchain) {
    ZEST_ASSERT_HANDLE(swapchain);  //Not a valid swapchain handle
    return swapchain->format;
}

void zest_SetSwapchainClearColor(zest_swapchain swapchain, float red, float green, float blue, float alpha) {
    swapchain->clear_color = (zest_clear_value_t){red, green, blue, alpha};
}

zest_layer_handle zest__create_instance_layer(const char *name, zest_size instance_type_size, zest_uint initial_instance_count) {
    zest_layer layer;
    zest_layer_handle handle = zest__new_layer(&layer);
    layer->name = name;
    zest__initialise_instance_layer(layer, instance_type_size, initial_instance_count);
    return handle;
}

zest_layer_handle zest_CreateMeshLayer(const char* name, zest_size vertex_type_size) {
    ZEST_ASSERT(ZestRenderer);  //Initialise Zest first!
    zest_layer layer;
    zest_layer_handle handle = zest__new_layer(&layer);
    layer->name = name;
    zest__initialise_mesh_layer(layer, sizeof(zest_textured_vertex_t), 1000);
    return handle;
}

zest_layer_handle zest_CreateInstanceMeshLayer(const char* name) {
    zest_layer layer;
    zest_layer_handle handle = zest__new_layer(&layer);
    layer->name = name;
    zest__initialise_instance_layer(layer, sizeof(zest_mesh_instance_t), 1000);
    return handle;
}

void zest__reset_query_pool(const zest_frame_graph_context context, VkQueryPool query_pool, zest_uint count) {
    ZEST_PRINT_FUNCTION;
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    zest_uint first_query = ZEST_FIF * count;
    vkCmdResetQueryPool(context->backend->command_buffer, query_pool, first_query, count);
}

VkQueryPool zest__create_query_pool(zest_uint timestamp_count) {
    ZEST_PRINT_FUNCTION;
    ZEST_ASSERT(timestamp_count);   //Must specify the number of timestamps required
    VkQueryPoolCreateInfo query_pool_info = { 0 };
    query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    query_pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    query_pool_info.queryCount = timestamp_count * ZEST_MAX_FIF; 
    VkQueryPool query_pool;
	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_query_pool);
    vkCreateQueryPool(ZestDevice->backend->logical_device, &query_pool_info, &ZestDevice->backend->allocation_callbacks, &query_pool);
    return query_pool;
}

// --Texture and Image functions
zest_image_handle zest__new_image() {
    zest_image_handle handle = { zest__add_store_resource(&ZestRenderer->images) };
    zest_image image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, handle.value);
    *image = (zest_image_t){ 0 };
    image->magic = zest_INIT_MAGIC(zest_struct_type_image);
    image->backend = zest__new_image_backend();
    for (int i = 0; i != zest_max_global_binding_number; ++i) {
        image->bindless_index[i] = ZEST_INVALID;
    }
    return handle;
}

void zest__release_all_global_texture_indexes(zest_image image) {
    if (!ZestRenderer->global_bindless_set_layout.value) {
        return;
    }
    for (int i = 0; i != zest_max_global_binding_number; ++i) {
        if (image->bindless_index[i] != ZEST_INVALID) {
            zest__release_bindless_index(ZestRenderer->global_bindless_set_layout, (zest_global_binding_number)i, image->bindless_index[i]);
            image->bindless_index[i] = ZEST_INVALID;
        }
    }
    zest_map_foreach(i, image->mip_indexes) {
        zest_mip_index_collection *mip_collection = &image->mip_indexes.data[i];
        zest_vec_foreach(j, mip_collection->mip_indexes) {
            zest_uint index = mip_collection->mip_indexes[j];
            zest_ReleaseGlobalBindlessIndex(index, mip_collection->binding_number);
        }
    }
}

zest_imgui_image_t zest_NewImGuiImage(void) {
    zest_imgui_image_t imgui_image = { 0 };
    imgui_image.magic = zest_INIT_MAGIC(zest_struct_type_imgui_image);
    return imgui_image;
}

zest_atlas_region zest_CreateAtlasRegion(zest_image_handle image_handle) {
    zest_atlas_region region = ZEST__NEW_ALIGNED(zest_atlas_region, 16);
    *region = (zest_atlas_region_t){ 0 };
    region->magic = zest_INIT_MAGIC(zest_struct_type_atlas_region);
    region->image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, image_handle.value);
    region->scale = zest_Vec2Set1(1.f);
    region->handle = zest_Vec2Set1(0.5f);
    region->uv.x = 0.f;
    region->uv.y = 0.f;
    region->uv.z = 1.f;
    region->uv.w = 1.f;
    region->layer = 0;
    region->frames = 1;
    return region;
}

void zest_FreeAtlasRegion(zest_atlas_region region) {
    if (ZEST_VALID_HANDLE(region)) {
        ZEST__FREE(region);
    }
}

zest_atlas_region zest_CreateAnimation(zest_uint frames) {
    zest_atlas_region image = (zest_atlas_region)ZEST__ALLOCATE_ALIGNED(sizeof(zest_atlas_region_t) * frames, 16);
    ZEST_ASSERT(image); //Couldn't allocate the image. Out of memory?
    image->frames = frames;
    return image;
}

void zest_AllocateBitmapMemory(zest_bitmap bitmap, zest_size size_in_bytes) {
    bitmap->meta.size = size_in_bytes;
	bitmap->data = (zest_byte*)ZEST__ALLOCATE(size_in_bytes);
}

void zest_AllocateBitmap(zest_bitmap bitmap, int width, int height, int channels) {
    bitmap->meta.size = width * height * channels;
    if (bitmap->meta.size > 0) {
        bitmap->data = (zest_byte*)ZEST__ALLOCATE(bitmap->meta.size);
        bitmap->meta.width = width;
        bitmap->meta.height = height;
        bitmap->meta.channels = channels;
        bitmap->meta.stride = width * channels;
        memset(bitmap->data, 0, bitmap->meta.size);
    }
}

void zest_AllocateBitmapAndClear(zest_bitmap bitmap, int width, int height, int channels, zest_color fill_color) {
    bitmap->meta.size = width * height * channels;
    if (bitmap->meta.size > 0) {
        bitmap->data = (zest_byte*)ZEST__ALLOCATE(bitmap->meta.size);
        bitmap->meta.width = width;
        bitmap->meta.height = height;
        bitmap->meta.channels = channels;
        bitmap->meta.stride = width * channels;
    }
    zest_FillBitmap(bitmap, fill_color);
}

void zest_LoadBitmapImage(zest_bitmap image, const char* file, int color_channels) {
    int width, height, original_no_channels;
    unsigned char* img = stbi_load(file, &width, &height, &original_no_channels, color_channels);
    if (img != NULL) {
        image->meta.width = width;
        image->meta.height = height;
        image->data = img;
        image->meta.channels = color_channels ? color_channels : original_no_channels;
        image->meta.stride = width * original_no_channels;
        image->meta.size = width * height * original_no_channels;
        zest_SetText(&image->name, file);
    }
    else {
        image->data = ZEST_NULL;
    }
}

void zest_LoadBitmapImageMemory(zest_bitmap image, const unsigned char* buffer, int size, int desired_no_channels) {
    int width, height, original_no_channels;
    unsigned char* img = stbi_load_from_memory(buffer, size, &width, &height, &original_no_channels, desired_no_channels);
    if (img != NULL) {
        image->meta.width = width;
        image->meta.height = height;
        image->data = img;
        image->meta.channels = original_no_channels;
        image->meta.stride = width * original_no_channels;
        image->meta.size = width * height * original_no_channels;
    }
    else {
        image->data = ZEST_NULL;
    }
}

void zest_FreeBitmap(zest_bitmap image) {
    if (!image->is_imported && image->data) {
        ZEST__FREE(image->data);
    }
    zest_FreeText(&image->name);
    image->data = ZEST_NULL;
    ZEST__FREE(image);
}

void zest_FreeBitmapData(zest_bitmap image) {
    if (!image->is_imported && image->data) {
        ZEST__FREE(image->data);
    }
    zest_FreeText(&image->name);
    image->data = ZEST_NULL;
}

zest_bitmap zest_NewBitmap() {
    zest_bitmap bitmap = ZEST__NEW(zest_bitmap);
    *bitmap = (zest_bitmap_t){ 0 };
    bitmap->magic = zest_INIT_MAGIC(zest_struct_type_bitmap);
    return bitmap;
}

zest_bitmap zest_CreateBitmapFromRawBuffer(const char* name, unsigned char* pixels, int size, int width, int height, int channels) {
    zest_bitmap bitmap = zest_NewBitmap();
    bitmap->is_imported = 1;
    bitmap->data = pixels;
    bitmap->meta.width = width;
    bitmap->meta.height = height;
    bitmap->meta.channels = channels;
    bitmap->meta.size = size;
    bitmap->meta.stride = width * channels;
    if (name) {
        zest_SetText(&bitmap->name, name);
    }
    return bitmap;
}

void zest_ConvertBitmapToTextureFormat(zest_bitmap src, VkFormat format) {
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

void zest_ConvertBitmapTo1Channel(zest_bitmap image) {
    if (image->meta.channels == 1) {
        return;
    }

    zest_bitmap_t converted = { 0 };
    converted.magic = zest_INIT_MAGIC(zest_struct_type_bitmap);
    zest_AllocateBitmap(&converted, image->meta.width, image->meta.height, 1);
    zest_ConvertBitmapToAlpha(image);

    zest_size pos = 0;
    zest_size converted_pos = 0;
    if (image->meta.channels == 4) {
        while (pos < image->meta.size) {
            converted.data[converted_pos++] = *(image->data + pos + 3);
            pos += image->meta.channels;
        }
    }
    else if (image->meta.channels == 3) {
        while (pos < image->meta.size) {
            converted.data[converted_pos++] = *(image->data + pos);
            pos += image->meta.channels;
        }
    }
    else if (image->meta.channels == 2) {
        while (pos < image->meta.size) {
            converted.data[converted_pos++] = *(image->data + pos + 1);
            pos += image->meta.channels;
        }
    }
    zest_FreeBitmapData(image);
    *image = converted;
}

void zest_PlotBitmap(zest_bitmap image, int x, int y, zest_color color) {

    size_t pos = y * image->meta.stride + (x * image->meta.channels);

    if (pos >= image->meta.size) {
        return;
    }

    if (image->meta.channels == 4) {
		*(image->data + pos) = color.r;
		*(image->data + pos + 1) = color.g;
		*(image->data + pos + 2) = color.b;
		*(image->data + pos + 3) = color.a;
    }
    else if (image->meta.channels == 3) {
		*(image->data + pos) = color.r;
		*(image->data + pos + 1) = color.g;
		*(image->data + pos + 2) = color.b;
    }
    else if (image->meta.channels == 2) {
		*(image->data + pos) = color.r;
		*(image->data + pos + 3) = color.a;
    }
    else if (image->meta.channels == 1) {
		*(image->data + pos) = color.r;
    }

}

void zest_FillBitmap(zest_bitmap image, zest_color color) {

    zest_size pos = 0;

    if (image->meta.channels == 4) {
        while (pos < image->meta.size) {
            *(image->data + pos) = color.r;
            *(image->data + pos + 1) = color.g;
            *(image->data + pos + 2) = color.b;
            *(image->data + pos + 3) = color.a;
            pos += image->meta.channels;
        }
    }
    else if (image->meta.channels == 3) {
        while (pos < image->meta.size) {
            *(image->data + pos) = color.r;
            *(image->data + pos + 1) = color.g;
            *(image->data + pos + 2) = color.b;
            pos += image->meta.channels;
        }
    }
    else if (image->meta.channels == 2) {
        while (pos < image->meta.size) {
            *(image->data + pos) = color.r;
            *(image->data + pos + 3) = color.a;
            pos += image->meta.channels;
        }
    }
    else if (image->meta.channels == 1) {
        while (pos < image->meta.size) {
            *(image->data + pos) = color.r;
            pos += image->meta.channels;
        }
    }
}

void zest_ConvertBitmapToAlpha(zest_bitmap  image) {

    zest_size pos = 0;

    if (image->meta.channels == 4) {
        while (pos < image->meta.size) {
            zest_byte c = (zest_byte)ZEST__MIN(((float)*(image->data + pos) * 0.3f) + ((float)*(image->data + pos + 1) * .59f) + ((float)*(image->data + pos + 2) * .11f), (float)*(image->data + pos + 3));
            *(image->data + pos) = 255;
            *(image->data + pos + 1) = 255;
            *(image->data + pos + 2) = 255;
            *(image->data + pos + 3) = c;
            pos += image->meta.channels;
        }
    }
    else if (image->meta.channels == 3) {
        while (pos < image->meta.size) {
            zest_byte c = (zest_byte)(((float)*(image->data + pos) * 0.3f) + ((float)*(image->data + pos + 1) * .59f) + ((float)*(image->data + pos + 2) * .11f));
            *(image->data + pos) = c;
            *(image->data + pos + 1) = c;
            *(image->data + pos + 2) = c;
            pos += image->meta.channels;
        }
    }
    else if (image->meta.channels == 2) {
        while (pos < image->meta.size) {
            *(image->data + pos) = 255;
            pos += image->meta.channels;
        }
    }
    else if (image->meta.channels == 1) {
        return;
    }
}

void zest_ConvertBitmap(zest_bitmap  src, zest_texture_format format, zest_byte alpha_level) {
    //Todo: simd this
    if (src->meta.channels == 4)
        return;

    ZEST_ASSERT(src->data);

    zest_byte* new_image;
    int channels = 4;
    zest_size new_size = src->meta.width * src->meta.height* channels;
    new_image = (zest_byte*)ZEST__ALLOCATE(new_size);

    zest_size pos = 0;
    zest_size new_pos = 0;

    zest_uint order[3] = { 1, 2, 3 };
    if (format == zest_format_b8g8r8a8_unorm) {
        order[0] = 3; order[2] = 1;
    }

    if (src->meta.channels == 1) {
        while (pos < src->meta.size) {
            *(new_image + new_pos) = *(src->data + pos);
            *(new_image + new_pos + order[0]) = *(src->data + pos);
            *(new_image + new_pos + order[1]) = *(src->data + pos);
            *(new_image + new_pos + order[2]) = alpha_level;
            pos += src->meta.channels;
            new_pos += 4;
        }
    }
    else if (src->meta.channels == 2) {
        while (pos < src->meta.size) {
            *(new_image + new_pos) = *(src->data + pos);
            *(new_image + new_pos + order[0]) = *(src->data + pos);
            *(new_image + new_pos + order[1]) = *(src->data + pos);
            *(new_image + new_pos + order[2]) = *(src->data + pos + 1);
            pos += src->meta.channels;
            new_pos += 4;
        }
    }
    else if (src->meta.channels == 3) {
        while (pos < src->meta.size) {
            *(new_image + new_pos) = *(src->data + pos);
            *(new_image + new_pos + order[0]) = *(src->data + pos + 1);
            *(new_image + new_pos + order[1]) = *(src->data + pos + 2);
            *(new_image + new_pos + order[2]) = alpha_level;
            pos += src->meta.channels;
            new_pos += 4;
        }
    }

    ZEST__FREE(src->data);
    src->meta.channels = channels;
    src->meta.size = new_size;
    src->meta.stride = src->meta.width * channels;
    src->data = new_image;

}

void zest_ConvertBitmapToBGRA(zest_bitmap  src, zest_byte alpha_level) {
    zest_ConvertBitmap(src, zest_format_b8g8r8a8_unorm, alpha_level);
}

void zest_ConvertBitmapToRGBA(zest_bitmap  src, zest_byte alpha_level) {
    zest_ConvertBitmap(src, zest_format_r8g8b8a8_unorm, alpha_level);
}

void zest_ConvertBGRAToRGBA(zest_bitmap  src) {
    if (src->meta.channels != 4)
        return;

    zest_size pos = 0;
    zest_size new_pos = 0;
    zest_byte* data = src->data;

    while (pos < src->meta.size) {
        zest_byte b = *(data + pos);
        *(data + pos) = *(data + pos + 2);
        *(data + pos + 2) = b;
        pos += 4;
    }
}

void zest_CopyWholeBitmap(zest_bitmap src, zest_bitmap dst) {
    ZEST_ASSERT(src->data && src->meta.size);

    zest_FreeBitmapData(dst);
    zest_SetText(&dst->name, src->name.str);
    dst->meta.channels = src->meta.channels;
    dst->meta.height = src->meta.height;
    dst->meta.width = src->meta.width;
    dst->meta.size = src->meta.size;
    dst->meta.stride = src->meta.stride;
    dst->data = ZEST_NULL;
    dst->data = (zest_byte*)ZEST__ALLOCATE(src->meta.size);
    ZEST_ASSERT(dst->data);    //out of memory;
    memcpy(dst->data, src->data, src->meta.size);

}

void zest_CopyBitmap(zest_bitmap src, int from_x, int from_y, int width, int height, zest_bitmap dst, int to_x, int to_y) {
    ZEST_ASSERT(src->data);
    ZEST_ASSERT(dst->data);
    ZEST_ASSERT(src->meta.channels == dst->meta.channels);

    if (from_x + width > src->meta.width)
        width = src->meta.width - from_x;
    if (from_y + height > src->meta.height)
        height = src->meta.height- from_y;

    if (to_x + width > dst->meta.width)
        to_x = dst->meta.width - width;
    if (to_y + height > dst->meta.height)
        to_y = dst->meta.height - height;

    if (src->data && dst->data && width > 0 && height > 0) {
        int src_row = from_y * src->meta.stride + (from_x * src->meta.channels);
        int dst_row = to_y * dst->meta.stride + (to_x * dst->meta.channels);
        size_t row_size = width * src->meta.channels;
        int rows_copied = 0;
        while (rows_copied < height) {
            memcpy(dst->data + dst_row, src->data + src_row, row_size);
            rows_copied++;
            src_row += src->meta.stride;
            dst_row += dst->meta.stride;
        }
    }
}

zest_color zest_SampleBitmap(zest_bitmap  image, int x, int y) {
    ZEST_ASSERT(image->data);

    size_t offset = y * image->meta.stride + (x * image->meta.channels);
    zest_color c = { 0 };
    if (offset < image->meta.size) {
        c.r = *(image->data + offset);

        if (image->meta.channels == 2) {
            c.a = *(image->data + offset + 1);
        }

        if (image->meta.channels == 3) {
            c.g = *(image->data + offset + 1);
            c.b = *(image->data + offset + 2);
        }

        if (image->meta.channels == 4) {
            c.g = *(image->data + offset + 1);
            c.b = *(image->data + offset + 2);
            c.a = *(image->data + offset + 3);
        }
    }

    return c;
}

float zest_FindBitmapRadius(zest_bitmap  image) {
    //Todo: optimise with SIMD
    ZEST_ASSERT(image->data);
    float max_radius = 0;
    for (int x = 0; x < image->meta.width; ++x) {
        for (int y = 0; y < image->meta.height; ++y) {
            zest_color c = zest_SampleBitmap(image, x, y);
            if (c.a) {
                max_radius = ceilf(ZEST__MAX(max_radius, zest_Distance((float)image->meta.width / 2.f, (float)image->meta.height / 2.f, (float)x, (float)y)));
            }
        }
    }
    return ceilf(max_radius);
}

void zest_DestroyBitmapArray(zest_bitmap_array_t* bitmap_array) {
    if (bitmap_array->data) {
        ZEST__FREE(bitmap_array->data);
    }
    bitmap_array->data = ZEST_NULL;
    bitmap_array->size_of_array = 0;
}

zest_bitmap zest_GetImageFromArray(zest_bitmap_array_t* bitmap_array, zest_index index) {
    zest_bitmap image = zest_NewBitmap();
    image->meta.width = bitmap_array->meta[index].width;
    image->meta.height = bitmap_array->meta[index].height;
    image->meta.channels = bitmap_array->meta[index].channels;
    image->meta.stride = bitmap_array->meta[index].width * bitmap_array->meta[index].channels;
    image->data = bitmap_array->data + bitmap_array->meta[index].offset;
    return image;
}

// --Internal Texture functions
void zest__cleanup_image(zest_image image) {
    ZestPlatform->cleanup_image_backend(image);
    zest__release_all_global_texture_indexes(image);
    zest_map_foreach(i, image->mip_indexes) {
        zest_mip_index_collection *collection = &image->mip_indexes.data[i];
		zest_vec_free(collection->mip_indexes);
    }
    zest_map_free(image->mip_indexes);
    zest__remove_store_resource(&ZestRenderer->images, image->handle.value);
    if (image->default_view) {
        ZestPlatform->cleanup_image_view_backend(image->default_view);
        ZEST__FREE(image->default_view);
    }
    image->magic = 0;
}

void zest__cleanup_sampler(zest_sampler sampler) {
    zest__cleanup_sampler_backend(sampler);
    zest__remove_store_resource(&ZestRenderer->samplers, sampler->handle.value);
    sampler->magic = 0;
}

void zest__cleanup_uniform_buffer(zest_uniform_buffer uniform_buffer) {
    ZEST_PRINT_FUNCTION;
    zest_ForEachFrameInFlight(fif) {
        zest_buffer buffer = uniform_buffer->buffer[fif];
        vkDestroyBuffer(ZestDevice->backend->logical_device, buffer->backend->vk_buffer, &ZestDevice->backend->allocation_callbacks);
        buffer->backend->vk_buffer = 0;
        zest_FreeBuffer(uniform_buffer->buffer[fif]);
        zest_set_layout layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, uniform_buffer->set_layout.value);
		ZEST__FREE(uniform_buffer->descriptor_set[fif]->backend);
        //zest__cleanup_set_layout(layout);
        ZEST__FREE(uniform_buffer->descriptor_set[fif]);
    }
    zest__cleanup_uniform_buffer_backend(uniform_buffer);
    zest__remove_store_resource(&ZestRenderer->uniform_buffers, uniform_buffer->handle.value);
}

void zest__tinyktxCallbackError(void *user, char const *msg) {
    ZEST_PRINT("Tiny_Ktx ERROR: %s", msg);
}

void *zest__tinyktxCallbackAlloc(void *user, size_t size) {
    return ZEST__ALLOCATE(size);
}

void zest__tinyktxCallbackFree(void *user, void *data) {
    ZEST__FREE(data);
}

size_t zest__tinyktxCallbackRead(void *user, void *data, size_t size) {
    FILE* handle = (FILE*)user;
    return fread(data, 1, size, handle);
}

bool zest__tinyktxCallbackSeek(void *user, int64_t offset) {
    FILE* handle = (FILE*)user;
    return fseek(handle, (long)offset, SEEK_SET) == 0;
}

int64_t zest__tinyktxCallbackTell(void *user) {
    FILE* handle = (FILE*)user;
    return ftell(handle);
}

VkFormat zest__convert_tktx_format(TinyKtx_Format ktx_format) {
    TinyImageFormat_VkFormat format = (TinyImageFormat_VkFormat)ktx_format;
    switch (format) {
        case TIF_VK_FORMAT_R4G4_UNORM_PACK8: return VK_FORMAT_R4G4_UNORM_PACK8; break;
        case TIF_VK_FORMAT_R4G4B4A4_UNORM_PACK16: return VK_FORMAT_R4G4B4A4_UNORM_PACK16; break;
        case TIF_VK_FORMAT_B4G4R4A4_UNORM_PACK16: return VK_FORMAT_B4G4R4A4_UNORM_PACK16; break;
        case TIF_VK_FORMAT_R5G6B5_UNORM_PACK16: return VK_FORMAT_R5G6B5_UNORM_PACK16; break;
        case TIF_VK_FORMAT_B5G6R5_UNORM_PACK16: return VK_FORMAT_B5G6R5_UNORM_PACK16; break;
        case TIF_VK_FORMAT_R5G5B5A1_UNORM_PACK16: return VK_FORMAT_R5G5B5A1_UNORM_PACK16; break;
        case TIF_VK_FORMAT_B5G5R5A1_UNORM_PACK16: return VK_FORMAT_B5G5R5A1_UNORM_PACK16; break;
        case TIF_VK_FORMAT_A1R5G5B5_UNORM_PACK16: return VK_FORMAT_A1R5G5B5_UNORM_PACK16; break;
        case TIF_VK_FORMAT_R8_UNORM: return VK_FORMAT_R8_UNORM; break;
        case TIF_VK_FORMAT_R8_SNORM: return VK_FORMAT_R8_SNORM; break;
        case TIF_VK_FORMAT_R8_UINT: return VK_FORMAT_R8_UINT; break;
        case TIF_VK_FORMAT_R8_SINT: return VK_FORMAT_R8_SINT; break;
        case TIF_VK_FORMAT_R8_SRGB: return VK_FORMAT_R8_SRGB; break;
        case TIF_VK_FORMAT_R8G8_UNORM: return VK_FORMAT_R8G8_UNORM; break;
        case TIF_VK_FORMAT_R8G8_SNORM: return VK_FORMAT_R8G8_SNORM; break;
        case TIF_VK_FORMAT_R8G8_UINT: return VK_FORMAT_R8G8_UINT; break;
        case TIF_VK_FORMAT_R8G8_SINT: return VK_FORMAT_R8G8_SINT; break;
        case TIF_VK_FORMAT_R8G8_SRGB: return VK_FORMAT_R8G8_SRGB; break;
        case TIF_VK_FORMAT_R8G8B8_UNORM: return VK_FORMAT_R8G8B8_UNORM; break;
        case TIF_VK_FORMAT_R8G8B8_SNORM: return VK_FORMAT_R8G8B8_SNORM; break;
        case TIF_VK_FORMAT_R8G8B8_UINT: return VK_FORMAT_R8G8B8_UINT; break;
        case TIF_VK_FORMAT_R8G8B8_SINT: return VK_FORMAT_R8G8B8_SINT; break;
        case TIF_VK_FORMAT_R8G8B8_SRGB: return VK_FORMAT_R8G8B8_SRGB; break;
        case TIF_VK_FORMAT_B8G8R8_UNORM: return VK_FORMAT_B8G8R8_UNORM; break;
        case TIF_VK_FORMAT_B8G8R8_SNORM: return VK_FORMAT_B8G8R8_SNORM; break;
        case TIF_VK_FORMAT_B8G8R8_UINT: return VK_FORMAT_B8G8R8_UINT; break;
        case TIF_VK_FORMAT_B8G8R8_SINT: return VK_FORMAT_B8G8R8_SINT; break;
        case TIF_VK_FORMAT_B8G8R8_SRGB: return VK_FORMAT_B8G8R8_SRGB; break;
        case TIF_VK_FORMAT_R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM; break;
        case TIF_VK_FORMAT_R8G8B8A8_SNORM: return VK_FORMAT_R8G8B8A8_SNORM; break;
        case TIF_VK_FORMAT_R8G8B8A8_UINT: return VK_FORMAT_R8G8B8A8_UINT; break;
        case TIF_VK_FORMAT_R8G8B8A8_SINT: return VK_FORMAT_R8G8B8A8_SINT; break;
        case TIF_VK_FORMAT_R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB; break;
        case TIF_VK_FORMAT_B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM; break;
        case TIF_VK_FORMAT_B8G8R8A8_SNORM: return VK_FORMAT_B8G8R8A8_SNORM; break;
        case TIF_VK_FORMAT_B8G8R8A8_UINT: return VK_FORMAT_B8G8R8A8_UINT; break;
        case TIF_VK_FORMAT_B8G8R8A8_SINT: return VK_FORMAT_B8G8R8A8_SINT; break;
        case TIF_VK_FORMAT_B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB; break;
        case TIF_VK_FORMAT_A8B8G8R8_UNORM_PACK32: return VK_FORMAT_A8B8G8R8_UNORM_PACK32; break;
        case TIF_VK_FORMAT_A8B8G8R8_SNORM_PACK32: return VK_FORMAT_A8B8G8R8_SNORM_PACK32; break;
        case TIF_VK_FORMAT_A8B8G8R8_UINT_PACK32: return VK_FORMAT_A8B8G8R8_UINT_PACK32; break;
        case TIF_VK_FORMAT_A8B8G8R8_SINT_PACK32: return VK_FORMAT_A8B8G8R8_SINT_PACK32; break;
        case TIF_VK_FORMAT_A8B8G8R8_SRGB_PACK32: return VK_FORMAT_A8B8G8R8_SRGB_PACK32; break;
        case TIF_VK_FORMAT_A2R10G10B10_UNORM_PACK32: return VK_FORMAT_A2R10G10B10_UNORM_PACK32; break;
        case TIF_VK_FORMAT_A2R10G10B10_SNORM_PACK32: return VK_FORMAT_A2R10G10B10_SNORM_PACK32; break;
        case TIF_VK_FORMAT_A2R10G10B10_UINT_PACK32: return VK_FORMAT_A2R10G10B10_UINT_PACK32; break;
        case TIF_VK_FORMAT_A2R10G10B10_SINT_PACK32: return VK_FORMAT_A2R10G10B10_SINT_PACK32; break;
        case TIF_VK_FORMAT_A2B10G10R10_UNORM_PACK32: return VK_FORMAT_A2B10G10R10_UNORM_PACK32; break;
        case TIF_VK_FORMAT_A2B10G10R10_SNORM_PACK32: return VK_FORMAT_A2B10G10R10_SNORM_PACK32; break;
        case TIF_VK_FORMAT_A2B10G10R10_UINT_PACK32: return VK_FORMAT_A2B10G10R10_UINT_PACK32; break;
        case TIF_VK_FORMAT_A2B10G10R10_SINT_PACK32: return VK_FORMAT_A2B10G10R10_SINT_PACK32; break;
        case TIF_VK_FORMAT_R16_UNORM: return VK_FORMAT_R16_UNORM; break;
        case TIF_VK_FORMAT_R16_SNORM: return VK_FORMAT_R16_SNORM; break;
        case TIF_VK_FORMAT_R16_UINT: return VK_FORMAT_R16_UINT; break;
        case TIF_VK_FORMAT_R16_SINT: return VK_FORMAT_R16_SINT; break;
        case TIF_VK_FORMAT_R16_SFLOAT: return VK_FORMAT_R16_SFLOAT; break;
        case TIF_VK_FORMAT_R16G16_UNORM: return VK_FORMAT_R16G16_UNORM; break;
        case TIF_VK_FORMAT_R16G16_SNORM: return VK_FORMAT_R16G16_SNORM; break;
        case TIF_VK_FORMAT_R16G16_UINT: return VK_FORMAT_R16G16_UINT; break;
        case TIF_VK_FORMAT_R16G16_SINT: return VK_FORMAT_R16G16_SINT; break;
        case TIF_VK_FORMAT_R16G16_SFLOAT: return VK_FORMAT_R16G16_SFLOAT; break;
        case TIF_VK_FORMAT_R16G16B16_UNORM: return VK_FORMAT_R16G16B16_UNORM; break;
        case TIF_VK_FORMAT_R16G16B16_SNORM: return VK_FORMAT_R16G16B16_SNORM; break;
        case TIF_VK_FORMAT_R16G16B16_UINT: return VK_FORMAT_R16G16B16_UINT; break;
        case TIF_VK_FORMAT_R16G16B16_SINT: return VK_FORMAT_R16G16B16_SINT; break;
        case TIF_VK_FORMAT_R16G16B16_SFLOAT: return VK_FORMAT_R16G16B16_SFLOAT; break;
        case TIF_VK_FORMAT_R16G16B16A16_UNORM: return VK_FORMAT_R16G16B16A16_UNORM; break;
        case TIF_VK_FORMAT_R16G16B16A16_SNORM: return VK_FORMAT_R16G16B16A16_SNORM; break;
        case TIF_VK_FORMAT_R16G16B16A16_UINT: return VK_FORMAT_R16G16B16A16_UINT; break;
        case TIF_VK_FORMAT_R16G16B16A16_SINT: return VK_FORMAT_R16G16B16A16_SINT; break;
        case TIF_VK_FORMAT_R16G16B16A16_SFLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT; break;
        case TIF_VK_FORMAT_R32_UINT: return VK_FORMAT_R32_UINT; break;
        case TIF_VK_FORMAT_R32_SINT: return VK_FORMAT_R32_SINT; break;
        case TIF_VK_FORMAT_R32_SFLOAT: return VK_FORMAT_R32_SFLOAT; break;
        case TIF_VK_FORMAT_R32G32_UINT: return VK_FORMAT_R32G32_UINT; break;
        case TIF_VK_FORMAT_R32G32_SINT: return VK_FORMAT_R32G32_SINT; break;
        case TIF_VK_FORMAT_R32G32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT; break;
        case TIF_VK_FORMAT_R32G32B32_UINT: return VK_FORMAT_R32G32B32_UINT; break;
        case TIF_VK_FORMAT_R32G32B32_SINT: return VK_FORMAT_R32G32B32_SINT; break;
        case TIF_VK_FORMAT_R32G32B32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT; break;
        case TIF_VK_FORMAT_R32G32B32A32_UINT: return VK_FORMAT_R32G32B32A32_UINT; break;
        case TIF_VK_FORMAT_R32G32B32A32_SINT: return VK_FORMAT_R32G32B32A32_SINT; break;
        case TIF_VK_FORMAT_R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT; break;
        case TIF_VK_FORMAT_R64_UINT: return VK_FORMAT_R64_UINT; break;
        case TIF_VK_FORMAT_R64_SINT: return VK_FORMAT_R64_SINT; break;
        case TIF_VK_FORMAT_R64_SFLOAT: return VK_FORMAT_R64_SFLOAT; break;
        case TIF_VK_FORMAT_R64G64_UINT: return VK_FORMAT_R64G64_UINT; break;
        case TIF_VK_FORMAT_R64G64_SINT: return VK_FORMAT_R64G64_SINT; break;
        case TIF_VK_FORMAT_R64G64_SFLOAT: return VK_FORMAT_R64G64_SFLOAT; break;
        case TIF_VK_FORMAT_R64G64B64_UINT: return VK_FORMAT_R64G64B64_UINT; break;
        case TIF_VK_FORMAT_R64G64B64_SINT: return VK_FORMAT_R64G64B64_SINT; break;
        case TIF_VK_FORMAT_R64G64B64_SFLOAT: return VK_FORMAT_R64G64B64_SFLOAT; break;
        case TIF_VK_FORMAT_R64G64B64A64_UINT: return VK_FORMAT_R64G64B64A64_UINT; break;
        case TIF_VK_FORMAT_R64G64B64A64_SINT: return VK_FORMAT_R64G64B64A64_SINT; break;
        case TIF_VK_FORMAT_R64G64B64A64_SFLOAT: return VK_FORMAT_R64G64B64A64_SFLOAT; break;
        case TIF_VK_FORMAT_B10G11R11_UFLOAT_PACK32: return VK_FORMAT_B10G11R11_UFLOAT_PACK32; break;
        case TIF_VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32; break;
        case TIF_VK_FORMAT_D16_UNORM: return VK_FORMAT_D16_UNORM; break;
        case TIF_VK_FORMAT_X8_D24_UNORM_PACK32: return VK_FORMAT_X8_D24_UNORM_PACK32; break;
        case TIF_VK_FORMAT_D32_SFLOAT: return VK_FORMAT_D32_SFLOAT; break;
        case TIF_VK_FORMAT_S8_UINT: return VK_FORMAT_S8_UINT; break;
        case TIF_VK_FORMAT_D16_UNORM_S8_UINT: return VK_FORMAT_D16_UNORM_S8_UINT; break;
        case TIF_VK_FORMAT_D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT; break;
        case TIF_VK_FORMAT_D32_SFLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT; break;
        case TIF_VK_FORMAT_BC1_RGB_UNORM_BLOCK: return VK_FORMAT_BC1_RGB_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_BC1_RGB_SRGB_BLOCK: return VK_FORMAT_BC1_RGB_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_BC2_UNORM_BLOCK: return VK_FORMAT_BC2_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_BC2_SRGB_BLOCK: return VK_FORMAT_BC2_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_BC3_UNORM_BLOCK: return VK_FORMAT_BC3_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_BC3_SRGB_BLOCK: return VK_FORMAT_BC3_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_BC4_UNORM_BLOCK: return VK_FORMAT_BC4_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_BC4_SNORM_BLOCK: return VK_FORMAT_BC4_SNORM_BLOCK; break;
        case TIF_VK_FORMAT_BC5_UNORM_BLOCK: return VK_FORMAT_BC5_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_BC5_SNORM_BLOCK: return VK_FORMAT_BC5_SNORM_BLOCK; break;
        case TIF_VK_FORMAT_BC6H_UFLOAT_BLOCK: return VK_FORMAT_BC6H_UFLOAT_BLOCK; break;
        case TIF_VK_FORMAT_BC6H_SFLOAT_BLOCK: return VK_FORMAT_BC6H_SFLOAT_BLOCK; break;
        case TIF_VK_FORMAT_BC7_UNORM_BLOCK: return VK_FORMAT_BC7_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_BC7_SRGB_BLOCK: return VK_FORMAT_BC7_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_EAC_R11_UNORM_BLOCK: return VK_FORMAT_EAC_R11_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_EAC_R11_SNORM_BLOCK: return VK_FORMAT_EAC_R11_SNORM_BLOCK; break;
        case TIF_VK_FORMAT_EAC_R11G11_UNORM_BLOCK: return VK_FORMAT_EAC_R11G11_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return VK_FORMAT_EAC_R11G11_SNORM_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_4x4_UNORM_BLOCK: return VK_FORMAT_ASTC_4x4_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return VK_FORMAT_ASTC_4x4_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_5x4_UNORM_BLOCK: return VK_FORMAT_ASTC_5x4_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return VK_FORMAT_ASTC_5x4_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_5x5_UNORM_BLOCK: return VK_FORMAT_ASTC_5x5_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return VK_FORMAT_ASTC_5x5_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_6x5_UNORM_BLOCK: return VK_FORMAT_ASTC_6x5_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return VK_FORMAT_ASTC_6x5_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_6x6_UNORM_BLOCK: return VK_FORMAT_ASTC_6x6_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return VK_FORMAT_ASTC_6x6_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_8x5_UNORM_BLOCK: return VK_FORMAT_ASTC_8x5_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return VK_FORMAT_ASTC_8x5_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_8x6_UNORM_BLOCK: return VK_FORMAT_ASTC_8x6_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return VK_FORMAT_ASTC_8x6_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_8x8_UNORM_BLOCK: return VK_FORMAT_ASTC_8x8_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return VK_FORMAT_ASTC_8x8_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_10x5_UNORM_BLOCK: return VK_FORMAT_ASTC_10x5_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return VK_FORMAT_ASTC_10x5_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_10x6_UNORM_BLOCK: return VK_FORMAT_ASTC_10x6_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return VK_FORMAT_ASTC_10x6_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_10x8_UNORM_BLOCK: return VK_FORMAT_ASTC_10x8_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return VK_FORMAT_ASTC_10x8_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_10x10_UNORM_BLOCK: return VK_FORMAT_ASTC_10x10_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return VK_FORMAT_ASTC_10x10_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_12x10_UNORM_BLOCK: return VK_FORMAT_ASTC_12x10_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return VK_FORMAT_ASTC_12x10_SRGB_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_12x12_UNORM_BLOCK: return VK_FORMAT_ASTC_12x12_UNORM_BLOCK; break;
        case TIF_VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return VK_FORMAT_ASTC_12x12_SRGB_BLOCK; break;
        default: return VK_FORMAT_UNDEFINED;
    }
    return VK_FORMAT_UNDEFINED;
}

zest_uint zest__get_format_channel_count(zest_texture_format format) {
    switch (format) {
    case zest_format_undefined:
    case zest_format_r8_unorm:
    case zest_format_r8_snorm:
    case zest_format_r8_srgb: {
        return 1;
        break;
    }
    case zest_format_r8g8_unorm:
    case zest_format_r8g8_snorm:
    case zest_format_r8g8_uint:
    case zest_format_r8g8_sint:
    case zest_format_r8g8_srgb: {
        return 2;
        break;
    }
    case zest_format_r8g8b8_unorm:
    case zest_format_r8g8b8_snorm:
    case zest_format_r8g8b8_uint:
    case zest_format_r8g8b8_sint:
    case zest_format_r8g8b8_srgb:
    case zest_format_b8g8r8_unorm:
    case zest_format_b8g8r8_snorm:
    case zest_format_b8g8r8_uint:
    case zest_format_b8g8r8_sint:
    case zest_format_b8g8r8_srgb: {
        return 3;
        break;
    }
    case zest_format_r8g8b8a8_unorm:
    case zest_format_r8g8b8a8_snorm:
    case zest_format_r8g8b8a8_uint:
    case zest_format_r8g8b8a8_sint:
    case zest_format_r8g8b8a8_srgb:
    case zest_format_b8g8r8a8_unorm:
    case zest_format_b8g8r8a8_snorm:
    case zest_format_b8g8r8a8_uint:
    case zest_format_b8g8r8a8_sint:
    case zest_format_b8g8r8a8_srgb: {
        return 4;
        break;
    }
    default:
        return 0;
        break;
    }
    return 0;
}

zest_image_collection zest__load_ktx(const char *file_path) {

    TinyKtx_Callbacks callbacks = {
            &zest__tinyktxCallbackError,
            &zest__tinyktxCallbackAlloc,
            &zest__tinyktxCallbackFree,
            zest__tinyktxCallbackRead,
            &zest__tinyktxCallbackSeek,
            &zest__tinyktxCallbackTell
    };

    FILE *file = zest__open_file(file_path, "rb");
	if (!file) {
		ZEST_PRINT("Failed to open KTX file: %s", file_path);
		return 0;
    }

    TinyKtx_ContextHandle ctx = TinyKtx_CreateContext(&callbacks, file);

    if (!TinyKtx_ReadHeader(ctx)) {
        return 0;
    }

    zest_texture_format format = (zest_texture_format)zest__convert_tktx_format(TinyKtx_GetFormat(ctx));
    if (format == zest_format_undefined) {
        TinyKtx_DestroyContext(ctx);
        return 0;
    }

	zest_image_collection image_collection = ZEST__NEW(zest_image_collection);
	*image_collection = (zest_image_collection_t){ 0 };
	image_collection->magic = zest_INIT_MAGIC(zest_struct_type_image_collection);
    image_collection->layer_count = TinyKtx_ArraySlices(ctx);
    image_collection->format = format;
    ZEST_ASSERT(image_collection->layer_count == 0);   //slice counts > 0 not handled yet!
    ZEST__MAYBE_FLAG(image_collection->flags, zest_image_collection_flag_is_cube_map, TinyKtx_IsCubemap(ctx));
	zest_uint width = TinyKtx_Width(ctx);
	zest_uint height = TinyKtx_Height(ctx);
	zest_uint depth = TinyKtx_Depth(ctx);

    zest_uint mip_count = TinyKtx_NumberOfMipmaps(ctx);

    zest_InitialiseBitmapArray(&image_collection->bitmap_array, mip_count);
    zest_bitmap_array_t *bitmap_array = &image_collection->bitmap_array;
    //First pass to set the bitmap array
    size_t offset = 0;
    for (zest_uint i = 0; i < mip_count; ++i) {
        zest_uint image_size = TinyKtx_ImageSize(ctx, i);
        bitmap_array->meta[i] = (zest_bitmap_meta_t){
            width, height,
            0, 0,
            image_size,
            offset,
        };
        offset += image_size;
        bitmap_array->total_mem_size += image_size;
        if (width > 1) width /= 2;
        if (height > 1) height /= 2;
    }

    bitmap_array->data = ZEST__ALLOCATE(bitmap_array->total_mem_size);
    ZEST_ASSERT(bitmap_array->data);        //Out of memory!
	width = TinyKtx_Width(ctx);
	height = TinyKtx_Height(ctx);

    zest_vec_foreach (i, bitmap_array->meta) {
        zloc_SafeCopyBlock(bitmap_array->data, zest_BitmapArrayLookUp(bitmap_array, i), TinyKtx_ImageRawData(ctx, i), bitmap_array->meta[i].size);
        if (width > 1) width /= 2;
        if (height > 1) height /= 2;
    }

    TinyKtx_DestroyContext(ctx);
    return image_collection;
}

void zest_InitialiseBitmapArray(zest_bitmap_array_t *images, zest_uint size_of_array) {
    zest_FreeBitmapArray(images);
    ZEST_ASSERT(size_of_array);            //must create with atleast one image in the array
    zest_vec_resize(images->meta, size_of_array);
    images->size_of_array = size_of_array;
}

void zest_CreateBitmapArray(zest_bitmap_array_t* images, int width, int height, int channels, zest_uint size_of_array) {
    ZEST_ASSERT(size_of_array);            //must create with atleast one image in the array
    if (images->data) {
        ZEST__FREE(images->data);
        images->data = ZEST_NULL;
    }
    images->size_of_array = size_of_array;
    zest_vec_resize(images->meta, size_of_array);
    size_t offset = 0;
    size_t image_size = width * height * channels;
    zest_vec_foreach(i, images->meta) {
        images->meta[i] = (zest_bitmap_meta_t){
            width, height,
            channels,
            width * channels,
			image_size,
            offset
        };
        offset += image_size;
		images->total_mem_size += images->meta[i].size;
    }
    images->data = (zest_byte*)ZEST__ALLOCATE(images->total_mem_size);
    memset(images->data, 0, images->total_mem_size);
}

void zest_FreeBitmapArray(zest_bitmap_array_t* images) {
    if (images->data) {
        ZEST__FREE(images->data);
    }
    zest_vec_free(images->meta);
    images->data = ZEST_NULL;
    images->size_of_array = 0;
}

void zest_FreeImageCollection(zest_image_collection image_collection) {
    ZEST_ASSERT_HANDLE(image_collection);   //Not a valid image collection handle/pointer!
    zest_vec_foreach(i, image_collection->image_bitmaps) {
        zest_FreeBitmapData(&image_collection->image_bitmaps[i]);
    }
    zest_uint i = 0;
    while (i < zest_vec_size(image_collection->regions)) {
        zest_atlas_region image = image_collection->regions[i];
        i += image->frames;
        zest_FreeText(&image->name);
        ZEST__FREE(image);
    }
    zest_vec_free(image_collection->regions);
    zest_vec_free(image_collection->image_bitmaps);
    zest_vec_free(image_collection->buffer_copy_regions);
    zest_FreeBitmapArray(&image_collection->bitmap_array);
    ZEST__FREE(image_collection);
}

void zest_SetImageHandle(zest_atlas_region image, float x, float y) {
    if (image->handle.x == x && image->handle.y == y)
        return;
    image->handle.x = x;
    image->handle.y = y;
    zest__update_image_vertices(image);
}

void zest__update_image_vertices(zest_atlas_region image) {
    image->min.x = image->width * (0.f - image->handle.x) * image->scale.x;
    image->min.y = image->height * (0.f - image->handle.y) * image->scale.y;
    image->max.x = image->width * (1.f - image->handle.x) * image->scale.x;
    image->max.y = image->height * (1.f - image->handle.y) * image->scale.y;
}

zest_image_handle zest_LoadCubemap(const char *name, const char *file_name) {
    zest_image_collection image_collection = zest__load_ktx(file_name);

    if (!image_collection) {
        return (zest_image_handle){ 0 };
    }

	zest_bitmap_array_t *bitmap_array = &image_collection->bitmap_array;
    zest_vec_foreach(mip_index, bitmap_array->meta) {
        zest_buffer_image_copy_t buffer_copy_region = { 0 };
        buffer_copy_region.image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        buffer_copy_region.mip_level = mip_index;
        buffer_copy_region.base_array_layer = 0;
        buffer_copy_region.layer_count = 6;
        buffer_copy_region.image_extent.width = bitmap_array->meta[mip_index].width;
        buffer_copy_region.image_extent.height = bitmap_array->meta[mip_index].height;
        buffer_copy_region.image_extent.depth = 1;
        buffer_copy_region.buffer_offset = bitmap_array->meta[mip_index].offset;

        zest_vec_push(image_collection->buffer_copy_regions, buffer_copy_region);
    }
    VkDeviceSize image_size = bitmap_array->total_mem_size;

    zest_buffer_info_t buffer_info = zest_CreateStagingBufferInfo();
    zest_buffer staging_buffer = zest_CreateBuffer(image_size, &buffer_info, ZEST_NULL);

    if (!staging_buffer) {
        goto cleanup;
    }

    memcpy(staging_buffer->data, image_collection->bitmap_array.data, bitmap_array->total_mem_size);

    zest_uint width = bitmap_array->meta[0].width;
    zest_uint height = bitmap_array->meta[0].height;

    zest_uint mip_levels = bitmap_array->size_of_array;
    zest_image_info_t create_info = zest_CreateImageInfo(width, height);
    create_info.mip_levels = mip_levels;
    create_info.format = image_collection->format;
    create_info.layer_count = 6;
    create_info.flags = zest_image_preset_texture | zest_image_flag_cubemap | zest_image_flag_transfer_src;
    zest_image_handle image_handle = zest_CreateImage(&create_info);
    zest_image image = (zest_image)zest__get_store_resource(&ZestRenderer->images, image_handle.value);

    zest__begin_single_time_commands();
    ZEST_CLEANUP_ON_FALSE(zest__transition_image_layout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, mip_levels, 0, 6));
    ZEST_CLEANUP_ON_FALSE(zest__copy_buffer_regions_to_image(image_collection->buffer_copy_regions, staging_buffer, staging_buffer->buffer_offset, image));
    ZEST_CLEANUP_ON_FALSE(zest__transition_image_layout(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, mip_levels, 0, 6));
    zest__end_single_time_commands();

    zest_FreeBitmapArray(bitmap_array);
	zest_FreeBuffer(staging_buffer);
	zest_FreeImageCollection(image_collection);

    return image_handle;

    cleanup:
    zest_FreeBitmapArray(bitmap_array);
	zest__cleanup_image(image);
	zest_FreeBuffer(staging_buffer);
	zest_FreeImageCollection(image_collection);
    return (zest_image_handle) { 0 };
}

zest_image_info_t zest_CreateImageInfo(zest_uint width, zest_uint height) {
    zest_image_info_t info = {
        {width, height, 1},
        1,
        1,
        zest_format_r8g8b8a8_unorm,
        0,
        zest_sample_count_1_bit,
        0
    };
    return info;
}

zest_image_view_type zest__get_image_view_type(zest_image image) {
    zest_image_view_type view_type;
    if (image->info.extent.depth > 1) {
        view_type = zest_image_view_type_3d;
    } else if (ZEST__FLAGGED(image->info.flags, zest_image_flag_cubemap)) {
        view_type = (image->info.layer_count > 6) ? zest_image_view_type_cube_array : zest_image_view_type_cube;
    } else {
        view_type = (image->info.layer_count > 1) ? zest_image_view_type_2d_array : zest_image_view_type_2d;
    }
    return view_type;
}

zest_image_handle zest_CreateImage(zest_image_info_t *create_info) {
    ZEST_ASSERT(create_info->extent.width * create_info->extent.height * create_info->extent.depth > 0); //Image has 0 dimensions!
    ZEST_ASSERT(create_info->flags);    //You must set flags in the image info to specify how the image will be used.
                                        //For example you could use zest_image_preset_texture. Lookup the zest_image_flag_bits emum
                                        //to see all the flags available.
	zest_image_handle handle = zest__new_image();
    zest_image image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, handle.value);
    image->info = *create_info;
    image->info.aspect_flags = zest__determine_aspect_flag(create_info->format);
    image->info.mip_levels = create_info->mip_levels > 0 ? create_info->mip_levels : 1;
    if (ZEST__FLAGGED(image->info.flags, zest_image_flag_cubemap)) {
        ZEST_ASSERT(image->info.layer_count > 0 && image->info.layer_count % 6 == 0); // Cubemap must have layers in multiples of 6!
    }
    if (ZEST__FLAGGED(create_info->flags, zest_image_flag_generate_mipmaps) && image->info.mip_levels == 1) {
        image->info.mip_levels = (zest_uint)floor(log2(ZEST__MAX(create_info->extent.width, create_info->extent.height))) + 1;
    }
    if (!zest__create_image(image, image->info.layer_count, zest_sample_count_1_bit, create_info->flags)) {
        zest__cleanup_image(image);
        return (zest_image_handle){ 0 };
    }
	image->info.layout = zest_image_layout_undefined;
    if (ZEST__FLAGGED(image->info.flags, zest_image_flag_storage)) {
        zest__begin_single_time_commands();
        zest__transition_image_layout(image, zest_image_layout_general, 0, ZEST__ALL_MIPS, 0, ZEST__ALL_LAYERS);
        zest__end_single_time_commands();
    }
    zest_image_view_type view_type = zest__get_image_view_type(image);
    image->default_view = zest__create_image_view_backend(image, view_type, image->info.mip_levels, 0, 0, image->info.layer_count, 0);
    return handle;
}

void zest_FreeImage(zest_image_handle handle) {
    zest_image image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, handle.value);
    zest_vec_push(ZestRenderer->deferred_resource_freeing_list.resources[ZEST_FIF], image);
}

zest_image_view_create_info_t zest_CreateViewImageInfo(zest_image_handle image_handle) {
    zest_image image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, image_handle.value);
    zest_image_view_type view_type = zest__get_image_view_type(image);
    zest_image_view_create_info_t info = {
        view_type,
        0,
        image->info.mip_levels,
        0,
        image->info.layer_count
    };
    return info;
}

zest_image_view_handle zest_CreateImageView(zest_image_handle image_handle, zest_image_view_create_info_t *create_info) {
    zest_image_view_handle view_handle = (zest_image_view_handle){ zest__add_store_resource(&ZestRenderer->views) };
    zest_image_view *view = (zest_image_view*)zest__get_store_resource(&ZestRenderer->views, view_handle.value);
    zest_image image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, image_handle.value);
    *view = zest__create_image_view_backend(image, create_info->view_type, create_info->level_count, 
                                    create_info->base_mip_level, create_info->base_array_layer,
									create_info->layer_count, 0);
    (*view)->handle = view_handle;
    return view_handle;
}

zest_image_view_array_handle zest_CreateImageViewsPerMip(zest_image_handle image_handle) {
    zest_image_view_array_handle view_handle = (zest_image_view_array_handle){ zest__add_store_resource(&ZestRenderer->view_arrays) };
    zest_image_view_array *view = (zest_image_view_array*)zest__get_store_resource(&ZestRenderer->view_arrays, view_handle.value);
    zest_image image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, image_handle.value);
    zest_image_view_type view_type = zest__get_image_view_type(image);
    *view = zest__create_image_views_per_mip_backend(image, view_type, 0, image->info.layer_count, 0);
    (*view)->handle = view_handle;
    return view_handle;
}

zest_byte* zest_BitmapArrayLookUp(zest_bitmap_array_t* bitmap_array, zest_index index) {
    ZEST_ASSERT((zest_uint)index < bitmap_array->size_of_array);
    return bitmap_array->data + bitmap_array->meta[index].offset;
}

zest_index zest_ImageLayerIndex(zest_atlas_region image) {
    ZEST_ASSERT_HANDLE(image);  //Not a valid image handle
    return image->layer;
}

zest_extent2d_t zest_RegionDimensions(zest_atlas_region image) {
    ZEST_ASSERT_HANDLE(image);  //Not a valid image handle
    return (zest_extent2d_t) { image->width, image->height };
}

zest_vec4 zest_ImageUV(zest_atlas_region region) {
    ZEST_ASSERT_HANDLE(region);  //Not a valid region handle
    return region->uv;
}

const zest_image_info_t *zest_ImageInfo(zest_image_handle image_handle) {
    zest_image image = (zest_image)zest__get_store_resource(&ZestRenderer->images, image_handle.value);
    return &image->info;
}

zest_uint zest_ImageDescriptorIndex(zest_image_handle image_handle, zest_global_binding_number binding_number) {
    ZEST_ASSERT(binding_number < zest_max_global_binding_number);   //Invalid binding number
    zest_image image = (zest_image)zest__get_store_resource(&ZestRenderer->images, image_handle.value);
    return image->bindless_index[binding_number];
}

int zest_ImageRawLayout(zest_image_handle image_handle) {
    zest_image image = (zest_image)zest__get_store_resource(&ZestRenderer->images, image_handle.value);
    return zest__get_image_raw_layout(image);
}
//-- End Texture and Image Functions

void zest_UploadInstanceLayerData(const zest_frame_graph_context context, void *user_data) {
	zest_layer_handle layer_handle = *(zest_layer_handle*)user_data;
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);

    ZEST__MAYBE_REPORT(!ZEST_VALID_HANDLE(layer), zest_report_invalid_layer, "Error in [%s] The zest_UploadInstanceLayerData was called with invalid layer data. Pass in a valid layer or array of layers to the zest_SetPassTask function in the frame graph.", ZestRenderer->current_frame_graph->name);

    if (ZEST_VALID_HANDLE(layer)) {  //You must pass in the zest_layer in the user data

        VkCommandBuffer command_buffer = context->backend->command_buffer;

        if (!layer->dirty[layer->fif]) {
            return;
        }

        zest_buffer staging_buffer = layer->memory_refs[layer->fif].staging_instance_data;
        zest_buffer device_buffer = ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif) ? layer->memory_refs[layer->fif].device_vertex_data : layer->vertex_buffer_node->storage_buffer;

        zest_buffer_uploader_t instance_upload = { 0, staging_buffer, device_buffer, 0 };

        layer->dirty[layer->fif] = 0;

        if (staging_buffer->memory_in_use && device_buffer) {
            zest_AddCopyCommand(&instance_upload, staging_buffer, device_buffer, device_buffer->buffer_offset);
        } else {
            return;
        }

        zest_uint vertex_size = zest_vec_size(instance_upload.buffer_copies);

        zest_cmd_UploadBuffer(context, &instance_upload);
    }
}

//-- Draw Layers
//-- internal
zest_layer_instruction_t zest__layer_instruction() {
    zest_layer_instruction_t instruction = { 0 };
    instruction.pipeline_template = ZEST_NULL;
    return instruction;
}

void zest__reset_instance_layer_drawing(zest_layer layer) {
    zest_vec_clear(layer->draw_instructions[layer->fif]);
    layer->memory_refs[layer->fif].staging_instance_data->memory_in_use = 0;
    layer->current_instruction = zest__layer_instruction();
    layer->memory_refs[layer->fif].instance_count = 0;
    layer->memory_refs[layer->fif].instance_ptr = layer->memory_refs[layer->fif].staging_instance_data->data;
    layer->memory_refs[layer->fif].staging_instance_data->memory_in_use = 0;
}

void zest_ResetInstanceLayerDrawing(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    zest_vec_clear(layer->draw_instructions[layer->fif]);
    layer->memory_refs[layer->fif].staging_instance_data->memory_in_use = 0;
    layer->current_instruction = zest__layer_instruction();
    layer->memory_refs[layer->fif].instance_count = 0;
    layer->memory_refs[layer->fif].instance_ptr = layer->memory_refs[layer->fif].staging_instance_data->data;
    layer->memory_refs[layer->fif].staging_instance_data->memory_in_use = 0;
}

zest_uint zest_GetInstanceLayerCount(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    return layer->memory_refs[layer->fif].instance_count;
}

zest_bool zest__grow_instance_buffer(zest_layer layer, zest_size type_size, zest_size minimum_size) {
    ZEST_PRINT_FUNCTION;
    zest_bool grown = 0;
    if (ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
		grown = zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_instance_data, type_size, minimum_size);
        zest_GrowBuffer(&layer->memory_refs[layer->fif].device_vertex_data, type_size, minimum_size);
		layer->memory_refs[layer->fif].staging_instance_data = layer->memory_refs[layer->fif].staging_instance_data;
        if (ZEST__FLAGGED(layer->flags, zest_layer_flag_using_global_bindless_layout) && layer->memory_refs[layer->fif].device_vertex_data->array_index != ZEST_INVALID) {
            zest_buffer instance_buffer = layer->memory_refs[layer->fif].device_vertex_data;
            VkDescriptorBufferInfo buffer_info = zest_vk_GetBufferInfo(instance_buffer);

            VkWriteDescriptorSet write = zest_CreateBufferDescriptorWriteWithType(layer->bindless_set->backend->vk_descriptor_set, &buffer_info, zest_storage_buffer_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            write.dstArrayElement = layer->memory_refs[layer->fif].device_vertex_data->array_index;
            vkUpdateDescriptorSets(ZestDevice->backend->logical_device, 1, &write, 0, 0);
        }
    } else {
		grown = zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_instance_data, type_size, minimum_size);
		layer->memory_refs[layer->fif].staging_instance_data = layer->memory_refs[layer->fif].staging_instance_data;
    }
    return grown;
}

void zest__cleanup_layer(zest_layer layer) {
	zest_ForEachFrameInFlight(fif) {
		zest_FreeBuffer(layer->memory_refs[fif].device_vertex_data);
		zest_FreeBuffer(layer->memory_refs[fif].staging_vertex_data);
		zest_FreeBuffer(layer->memory_refs[fif].staging_index_data);
		zest_vec_free(layer->draw_instructions[fif]);
	}
    zest__remove_store_resource(&ZestRenderer->layers, layer->handle.value);
}

void zest__reset_mesh_layer_drawing(zest_layer layer) {
    zest_vec_clear(layer->draw_instructions[layer->fif]);
    layer->memory_refs[layer->fif].staging_vertex_data->memory_in_use = 0;
    layer->memory_refs[layer->fif].staging_index_data->memory_in_use = 0;
    layer->current_instruction = zest__layer_instruction();
    layer->memory_refs[layer->fif].index_count = 0;
    layer->memory_refs[layer->fif].vertex_count = 0;
    layer->memory_refs[layer->fif].vertex_ptr = layer->memory_refs[layer->fif].staging_vertex_data->data;
    layer->memory_refs[layer->fif].index_ptr = layer->memory_refs[layer->fif].staging_index_data->data;
}

void zest__start_instance_instructions(zest_layer layer) {
    layer->current_instruction.start_index = layer->memory_refs[layer->fif].instance_count ? layer->memory_refs[layer->fif].instance_count : 0;
}

void zest_StartInstanceInstructions(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    layer->current_instruction.start_index = layer->memory_refs[layer->fif].instance_count ? layer->memory_refs[layer->fif].instance_count : 0;
}

void zest_ResetLayer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    layer->fif = (layer->fif + 1) % ZEST_MAX_FIF;
}

void zest_ResetInstanceLayer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    ZEST_ASSERT(ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif));   //You must have created the layer with zest_CreateFIFInstanceLayer
                                                                            //if you want to manually reset the layer
    layer->prev_fif = layer->fif;
    layer->fif = (layer->fif + 1) % ZEST_MAX_FIF;
    layer->dirty[layer->fif] = 1;
    zest__reset_instance_layer_drawing(layer);
}

void zest__start_mesh_instructions(zest_layer layer) {
    layer->current_instruction.start_index = layer->memory_refs[layer->fif].index_count ? layer->memory_refs[layer->fif].index_count : 0;
}

void zest__end_instance_instructions(zest_layer layer) {
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
        layer->fif = ZEST_FIF;
    }
    if (layer->current_instruction.total_instances) {
        layer->last_draw_mode = zest_draw_mode_none;
        zest_vec_push(layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->current_instruction.total_instances = 0;
        layer->current_instruction.start_index = 0;
    }
    else if (layer->current_instruction.draw_mode == zest_draw_mode_viewport) {
        zest_vec_push(layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->last_draw_mode = zest_draw_mode_none;
    }
    layer->memory_refs[layer->fif].staging_instance_data->memory_in_use = layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
}

void zest_EndInstanceInstructions(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    zest__end_instance_instructions(layer);
}

zest_bool zest_MaybeEndInstanceInstructions(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    if (layer->current_instruction.total_instances) {
        layer->last_draw_mode = zest_draw_mode_none;
        zest_vec_push(layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->current_instruction.total_instances = 0;
        layer->current_instruction.start_index = 0;
    }
    else if (layer->current_instruction.draw_mode == zest_draw_mode_viewport) {
        zest_vec_push(layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->last_draw_mode = zest_draw_mode_none;
    }
    return 1;
}

zest_size zest_GetLayerInstanceSize(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
	return layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
}

void zest__end_mesh_instructions(zest_layer layer) {
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

void zest__update_instance_layer_resolution(zest_layer layer) {
    layer->viewport.width = (float)zest_GetSwapChainExtent().width;
    layer->viewport.height = (float)zest_GetSwapChainExtent().height;
    layer->screen_scale.x = layer->viewport.width / layer->layer_size.x;
    layer->screen_scale.y = layer->viewport.height / layer->layer_size.y;
    layer->scissor.extent.width = (zest_uint)layer->viewport.width;
    layer->scissor.extent.height = (zest_uint)layer->viewport.height;
}

//Start general instance layer functionality -----
void zest_NextInstance(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
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

zest_draw_buffer_result zest_DrawInstanceBuffer(zest_layer_handle layer_handle, void *src, zest_uint amount) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
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

void zest_DrawInstanceInstruction(zest_layer_handle layer_handle, zest_uint amount) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    layer->memory_refs[layer->fif].instance_count += amount;
    layer->current_instruction.total_instances += amount;
    zest_size size_in_bytes_to_draw = amount * layer->instance_struct_size;
    zest_byte* instance_ptr = layer->memory_refs[layer->fif].instance_ptr;
    instance_ptr += size_in_bytes_to_draw;
    layer->memory_refs[layer->fif].instance_ptr = instance_ptr;
}

// End general instance layer functionality -----

//-- Draw Layers API
zest_layer_handle zest__new_layer(zest_layer *out_layer) {
    zest_layer_handle handle = (zest_layer_handle){zest__add_store_resource(&ZestRenderer->layers)};
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, handle.value);
    *layer = (zest_layer_t){ 0 };
    layer->magic = zest_INIT_MAGIC(zest_struct_type_layer);
    layer->handle = handle;
    *out_layer = layer;
    return handle;
}

void zest_FreeLayer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    zest_vec_push(ZestRenderer->deferred_resource_freeing_list.resources[ZEST_FIF], layer);
}

void zest_SetLayerViewPort(zest_layer_handle layer_handle, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    layer->scissor = zest_CreateRect2D(scissor_width, scissor_height, x, y);
    layer->viewport = zest_CreateViewport((float)x, (float)y, viewport_width, viewport_height, 0.f, 1.f);
}

void zest_SetLayerScissor(zest_layer_handle layer_handle, int offset_x, int offset_y, zest_uint scissor_width, zest_uint scissor_height) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    layer->scissor = zest_CreateRect2D(scissor_width, scissor_height, offset_x, offset_y);
}

void zest_SetLayerSizeToSwapchain(zest_layer_handle layer_handle, zest_swapchain swapchain) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    ZEST_ASSERT_HANDLE(swapchain);	//Not a valid swapchain handle!
    layer->scissor = zest_CreateRect2D((zest_uint)swapchain->size.width, (zest_uint)swapchain->size.height, 0, 0);
    layer->viewport = zest_CreateViewport(0.f, 0.f, (float)swapchain->size.width, (float)swapchain->size.height, 0.f, 1.f);

}

void zest_SetLayerSize(zest_layer_handle layer_handle, float width, float height) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    layer->layer_size.x = width;
    layer->layer_size.y = height;
}

void zest_SetLayerColor(zest_layer_handle layer_handle, zest_byte red, zest_byte green, zest_byte blue, zest_byte alpha) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    layer->current_color = zest_ColorSet(red, green, blue, alpha);
}

void zest_SetLayerColorf(zest_layer_handle layer_handle, float red, float green, float blue, float alpha) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    layer->current_color = zest_ColorSet((zest_byte)(red * 255.f), (zest_byte)(green * 255.f), (zest_byte)(blue * 255.f), (zest_byte)(alpha * 255.f));
}

void zest_SetLayerIntensity(zest_layer_handle layer_handle, float value) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    layer->intensity = value;
}

void zest_SetLayerChanged(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    zest_ForEachFrameInFlight(i) {
        layer->dirty[i] = 1;
    }
}

zest_bool zest_LayerHasChanged(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    return layer->dirty[layer->fif];
}

void zest_SetLayerUserData(zest_layer_handle layer_handle, void *data) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    layer->user_data = data;
}

void zest_UploadLayerStagingData(zest_layer_handle layer_handle, const zest_frame_graph_context context) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);  //This function must be called within a task callback in a frame graph
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);

    ZEST__MAYBE_REPORT(!ZEST_VALID_HANDLE(layer), zest_report_invalid_layer, "Error in [%s] The zest_UploadLayerStagingData was called with invalid layer data. Pass in a valid layer or array of layers to the zest_SetPassTask function in the frame graph.", ZestRenderer->current_frame_graph->name);

    if (ZEST_VALID_HANDLE(layer)) {  //You must pass in the zest_layer in the user data

        if (!layer->dirty[layer->fif]) {
            return;
        }

        zest_buffer staging_buffer = layer->memory_refs[layer->fif].staging_instance_data;
        zest_buffer device_buffer = ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif) ? layer->memory_refs[layer->fif].device_vertex_data : layer->vertex_buffer_node->storage_buffer;

        zest_buffer_uploader_t instance_upload = { 0, staging_buffer, device_buffer, 0 };

        layer->dirty[layer->fif] = 0;

        if (staging_buffer->memory_in_use) {
            zest_AddCopyCommand(&instance_upload, staging_buffer, device_buffer, device_buffer->buffer_offset);
        } else {
            return;
        }

        zest_uint vertex_size = zest_vec_size(instance_upload.buffer_copies);

        zest_cmd_UploadBuffer(context, &instance_upload);
    }
}

zest_buffer zest_GetLayerResourceBuffer(zest_layer_handle layer_handle) {
    ZEST_ASSERT_HANDLE(ZestRenderer->current_frame_graph);  //This function must be called within a task callback in a frame graph
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    if (ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
        return layer->memory_refs[layer->fif].device_vertex_data;
    } else if(ZEST_VALID_HANDLE(layer->vertex_buffer_node)) {
        ZEST_ASSERT_HANDLE(layer->vertex_buffer_node); //Layer does not have a valid resource node. 
                                                      //Make sure you add it to the frame graph
        ZEST__MAYBE_REPORT(layer->vertex_buffer_node->reference_count == 0, zest_report_resource_culled, "zest_GetLayerResourceBuffer was called for resourcee [%s] that has been culled. Passes will be culled (and therefore their transient resources will not be created) if they have no outputs and therefore deemed as unnecessary and also bear in mind that passes in the chain may also be culled.", layer->vertex_buffer_node->name);   
        return layer->vertex_buffer_node->storage_buffer;
    } else {
        ZEST__REPORT(zest_report_resource_culled, "zest_GetLayerResourceBuffer was called for layer [%s] but the layer doesn't have a resource node. Make sure that you add the layer to the frame graph with zest_AddInstanceLayerBufferResource", layer->name);   
    }
    return NULL;
}

zest_buffer zest_GetLayerStagingVertexBuffer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    return layer->memory_refs[layer->fif].staging_instance_data;
}

zest_buffer zest_GetLayerStagingIndexBuffer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    return layer->memory_refs[layer->fif].staging_index_data;
}
//-- End Draw Layers

//-- Start Instance Drawing API
zest_layer_handle zest_CreateInstanceLayer(const char* name, zest_size type_size) {
    zest_layer_builder_t builder = zest_NewInstanceLayerBuilder(type_size);
    return zest_FinishInstanceLayer(name, &builder);
}

zest_layer_handle zest_CreateFIFInstanceLayer(const char* name, zest_size type_size, zest_uint id) {
    zest_layer_builder_t builder = zest_NewInstanceLayerBuilder(type_size);
    builder.initial_count = 1000;
    zest_layer_handle layer_handle = zest_FinishInstanceLayer(name, &builder);
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    zest_ForEachFrameInFlight(fif) {
        layer->memory_refs[fif].device_vertex_data = zest_CreateUniqueVertexStorageBuffer(layer->memory_refs[fif].staging_instance_data->size, fif, id);
    }
    ZEST__FLAG(layer->flags, zest_layer_flag_manual_fif);
    return layer_handle;
}

zest_layer_builder_t zest_NewInstanceLayerBuilder(zest_size type_size) {
    zest_layer_builder_t builder = {
        .initial_count = 1000,
        .type_size = type_size,
    };
    return builder;
}

zest_layer_handle zest_FinishInstanceLayer(const char *name, zest_layer_builder_t *builder) {
    zest_layer_handle layer_handle = zest__create_instance_layer(name, builder->type_size, builder->initial_count);
    return layer_handle;
}

void zest__initialise_instance_layer(zest_layer layer, zest_size type_size, zest_uint instance_pool_size) {
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

    layer->scissor = zest_CreateRect2D(zest_SwapChainWidth(), zest_SwapChainHeight(), 0, 0);
    layer->viewport = zest_CreateViewport(0.f, 0.f, zest_SwapChainWidthf(), zest_SwapChainHeightf(), 0.f, 1.f);
}

//-- Start Instance Drawing API
void zest_SetInstanceDrawing(zest_layer_handle layer_handle, zest_shader_resources_handle handle, zest_pipeline_template pipeline) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(&ZestRenderer->shader_resources, handle.value);
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    zest__end_instance_instructions(layer);
    zest__start_instance_instructions(layer);
    layer->current_instruction.pipeline_template = pipeline;

    //The number of shader resources must match the number of descriptor layouts set in the pipeline.
    ZEST_ASSERT(zest_vec_size(shader_resources->backend->sets[ZEST_FIF]) == zest_vec_size(pipeline->descriptorSetLayouts));
	layer->current_instruction.shader_resources = handle;
    layer->current_instruction.draw_mode = zest_draw_mode_instance;
    layer->last_draw_mode = zest_draw_mode_instance;
}

void zest_SetLayerDrawingViewport(zest_layer_handle layer_handle, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    zest__end_instance_instructions(layer);
    zest__start_instance_instructions(layer);
    layer->current_instruction.scissor = zest_CreateRect2D(scissor_width, scissor_height, x, y);
    layer->current_instruction.viewport = zest_CreateViewport((float)x, (float)y, viewport_width, viewport_height, 0.f, 1.f);
    layer->current_instruction.draw_mode = zest_draw_mode_viewport;
}

void zest_SetLayerPushConstants(zest_layer_handle layer_handle, void *push_constants, zest_size size) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    ZEST_ASSERT(size <= 128);   //Push constant size must not exceed 128 bytes
    memcpy(layer->current_instruction.push_constant, push_constants, size);
}

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

    mesh_layer->scissor = zest_CreateRect2D(zest_SwapChainWidth(), zest_SwapChainHeight(), 0, 0);
    mesh_layer->viewport = zest_CreateViewport(0.f, 0.f, zest_SwapChainWidthf(), zest_SwapChainHeightf(), 0.f, 1.f);
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

zest_buffer zest_GetVertexWriteBuffer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    return layer->memory_refs[layer->fif].staging_vertex_data;
}

zest_buffer zest_GetIndexWriteBuffer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    return layer->memory_refs[layer->fif].staging_index_data;
}

void zest_GrowMeshVertexBuffers(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    zest_size memory_in_use = zest_GetVertexWriteBuffer(layer_handle)->memory_in_use;
    zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_vertex_data, layer->vertex_struct_size, memory_in_use);
}

void zest_GrowMeshIndexBuffers(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    zest_size memory_in_use = zest_GetVertexWriteBuffer(layer_handle)->memory_in_use;
    zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_index_data, sizeof(zest_uint), memory_in_use);
}

void zest_PushVertex(zest_layer_handle layer_handle, float pos_x, float pos_y, float pos_z, float intensity, float uv_x, float uv_y, zest_color color, zest_uint parameters) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
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

void zest_PushIndex(zest_layer_handle layer_handle, zest_uint offset) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
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

void zest_SetMeshDrawing(zest_layer_handle layer_handle, zest_shader_resources_handle resource_handle, zest_pipeline_template pipeline) {
    zest_shader_resources resources = (zest_shader_resources)zest__get_store_resource_checked(&ZestRenderer->shader_resources, resource_handle.value);
    ZEST_ASSERT_HANDLE(resources);   //Not a valid shader resource handle
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    ZEST_ASSERT_HANDLE(pipeline);	//Not a valid handle!
    zest__end_mesh_instructions(layer);
    zest__start_mesh_instructions(layer);
    layer->current_instruction.pipeline_template = pipeline;
	layer->current_instruction.shader_resources = resource_handle;
    layer->current_instruction.draw_mode = zest_draw_mode_mesh;
    layer->last_draw_mode = zest_draw_mode_mesh;
}

//-- End Mesh Drawing API

//-- Instanced_mesh_drawing
void zest_SetInstanceMeshDrawing(zest_layer_handle layer_handle, zest_shader_resources_handle resource_handle, zest_pipeline_template pipeline) {
    zest_shader_resources resources = (zest_shader_resources)zest__get_store_resource_checked(&ZestRenderer->shader_resources, resource_handle.value);
    ZEST_ASSERT_HANDLE(resources);   //Not a valid shader resource handle
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    ZEST_ASSERT_HANDLE(pipeline);	//Not a valid handle!
    zest__end_instance_instructions(layer);
    zest__start_instance_instructions(layer);
    layer->current_instruction.pipeline_template = pipeline;
	layer->current_instruction.shader_resources = resource_handle;
    layer->current_instruction.draw_mode = zest_draw_mode_mesh_instance;
    layer->current_instruction.scissor = layer->scissor;
    layer->current_instruction.viewport = layer->viewport;
    layer->last_draw_mode = zest_draw_mode_mesh_instance;
}

void zest_PushMeshVertex(zest_mesh mesh, float pos_x, float pos_y, float pos_z, zest_color color) {
    zest_vertex_t vertex = { {pos_x, pos_y, pos_z}, color, {0.f, 0.f, 0.f}, 0 };
    zest_vec_push(mesh->vertices, vertex);
}

void zest_PushMeshIndex(zest_mesh mesh, zest_uint index) {
    ZEST_ASSERT(index < zest_vec_size(mesh->vertices)); //Add vertices first before triangles to make sure you're indexing vertices that exist
    zest_vec_push(mesh->indexes, index);
}

void zest_PushMeshTriangle(zest_mesh mesh, zest_uint i1, zest_uint i2, zest_uint i3) {
    ZEST_ASSERT(i1 < zest_vec_size(mesh->vertices));	//Add vertices first before triangles to make sure you're indexing vertices that exist
    ZEST_ASSERT(i2 < zest_vec_size(mesh->vertices));	//Add vertices first before triangles to make sure you're indexing vertices that exist
    ZEST_ASSERT(i3 < zest_vec_size(mesh->vertices));	//Add vertices first before triangles to make sure you're indexing vertices that exist
    zest_vec_push(mesh->indexes, i1);
    zest_vec_push(mesh->indexes, i2);
    zest_vec_push(mesh->indexes, i3);
}

zest_mesh zest_NewMesh() {
    zest_mesh mesh = ZEST__NEW(zest_mesh);
    *mesh = (zest_mesh_t){ 0 };
    mesh->magic = zest_INIT_MAGIC(zest_struct_type_mesh);
    return mesh;
}

void zest_FreeMesh(zest_mesh mesh) {
    zest_vec_free(mesh->vertices);
    zest_vec_free(mesh->indexes);
    ZEST__FREE(mesh);
}

void zest_PositionMesh(zest_mesh mesh, zest_vec3 position) {
    zest_vec_foreach(i, mesh->vertices) {
        mesh->vertices[i].pos.x += position.x;
        mesh->vertices[i].pos.y += position.y;
        mesh->vertices[i].pos.z += position.z;
    }
}

zest_matrix4 zest_RotateMesh(zest_mesh mesh, float pitch, float yaw, float roll) {
    zest_matrix4 roll_mat = zest_Matrix4RotateZ(roll);
    zest_matrix4 pitch_mat = zest_Matrix4RotateX(pitch);
    zest_matrix4 yaw_mat = zest_Matrix4RotateY(yaw);
    zest_matrix4 rotate_mat = zest_MatrixTransform(&yaw_mat, &pitch_mat);
    rotate_mat = zest_MatrixTransform(&rotate_mat, &roll_mat);
    zest_vec_foreach(i, mesh->vertices) {
        zest_vec4 pos = { mesh->vertices[i].pos.x, mesh->vertices[i].pos.y, mesh->vertices[i].pos.z, 1.f };
        pos = zest_MatrixTransformVector(&rotate_mat, pos);
        mesh->vertices[i].pos = zest_Vec3Set(pos.x, pos.y, pos.z);
    }
    return rotate_mat;
}

zest_matrix4 zest_TransformMesh(zest_mesh mesh, float pitch, float yaw, float roll, float x, float y, float z, float sx, float sy, float sz) {
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
    zest_vec_foreach(i, mesh->vertices) {
        zest_vec4 pos = { mesh->vertices[i].pos.x, mesh->vertices[i].pos.y, mesh->vertices[i].pos.z, 1.f };
        pos = zest_MatrixTransformVector(&rotate_mat, pos);
        mesh->vertices[i].pos = zest_Vec3Set(pos.x, pos.y, pos.z);
    }
    return rotate_mat;
}

void zest_CalculateNormals(zest_mesh mesh) {
    // Zero out all normals first
    zest_vec_foreach(i, mesh->vertices) {
        mesh->vertices[i].normal = zest_Vec3Set(0.f, 0.f, 0.f);
    }

    // Calculate face normals and add them to each vertex of the face
    for (zest_uint i = 0; i < zest_vec_size(mesh->indexes); i += 3) {
        zest_uint i0 = mesh->indexes[i + 0];
        zest_uint i1 = mesh->indexes[i + 1];
        zest_uint i2 = mesh->indexes[i + 2];

        zest_vertex_t v0 = mesh->vertices[i0];
        zest_vertex_t v1 = mesh->vertices[i1];
        zest_vertex_t v2 = mesh->vertices[i2];

        zest_vec3 edge1 = zest_SubVec3(v1.pos, v0.pos);
        zest_vec3 edge2 = zest_SubVec3(v2.pos, v0.pos);
        zest_vec3 normal = zest_CrossProduct(edge1, edge2);

        mesh->vertices[i0].normal = zest_AddVec3(mesh->vertices[i0].normal, normal);
        mesh->vertices[i1].normal = zest_AddVec3(mesh->vertices[i1].normal, normal);
        mesh->vertices[i2].normal = zest_AddVec3(mesh->vertices[i2].normal, normal);
    }

    // Normalize all vertex normals
    zest_vec_foreach(i, mesh->vertices) {
        mesh->vertices[i].normal = zest_NormalizeVec3(mesh->vertices[i].normal);
    }
}

zest_bounding_box_t zest_NewBoundingBox() {
    zest_bounding_box_t bb = { 0 };
    bb.max_bounds = zest_Vec3Set( -9999999.f, -9999999.f, -9999999.f );
    bb.min_bounds = zest_Vec3Set( FLT_MAX, FLT_MAX, FLT_MAX );
    return bb;
}

zest_bounding_box_t zest_GetMeshBoundingBox(zest_mesh mesh) {
    zest_bounding_box_t bb = zest_NewBoundingBox();
    zest_vec_foreach(i, mesh->vertices) {
        bb.max_bounds.x = ZEST__MAX(mesh->vertices[i].pos.x, bb.max_bounds.x);
        bb.max_bounds.y = ZEST__MAX(mesh->vertices[i].pos.y, bb.max_bounds.y);
        bb.max_bounds.z = ZEST__MAX(mesh->vertices[i].pos.z, bb.max_bounds.z);
        bb.min_bounds.x = ZEST__MIN(mesh->vertices[i].pos.x, bb.min_bounds.x);
        bb.min_bounds.y = ZEST__MIN(mesh->vertices[i].pos.y, bb.min_bounds.y);
        bb.min_bounds.z = ZEST__MIN(mesh->vertices[i].pos.z, bb.min_bounds.z);
    }
    return bb;
}

void zest_SetMeshGroupID(zest_mesh mesh, zest_uint group_id) {
    zest_vec_foreach(i, mesh->vertices) {
        mesh->vertices[i].group = group_id;
    }
}

zest_size zest_MeshVertexDataSize(zest_mesh mesh) {
    return zest_vec_size(mesh->vertices) * sizeof(zest_vertex_t);
}

zest_size zest_MeshIndexDataSize(zest_mesh mesh) {
    return zest_vec_size(mesh->indexes) * sizeof(zest_uint);
}

void zest_AddMeshToMesh(zest_mesh dst_mesh, zest_mesh src_mesh) {
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

void zest_AddMeshToLayer(zest_layer_handle layer_handle, zest_mesh src_mesh) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    zest_buffer vertex_staging_buffer = zest_CreateStagingBuffer(zest_MeshVertexDataSize(src_mesh), src_mesh->vertices);
    zest_buffer index_staging_buffer = zest_CreateStagingBuffer(zest_MeshIndexDataSize(src_mesh), src_mesh->indexes);
    layer->index_count = zest_vec_size(src_mesh->indexes);
	zest_FreeBuffer(layer->vertex_data);
	zest_FreeBuffer(layer->index_data);
	layer->vertex_data = zest_CreateVertexBuffer(vertex_staging_buffer->size, ZEST_PERSISTENT);
	layer->index_data = zest_CreateIndexBuffer(index_staging_buffer->size, ZEST_PERSISTENT);
    zest__begin_single_time_commands();
    zest_cmd_CopyBufferOneTime(vertex_staging_buffer, layer->vertex_data, vertex_staging_buffer->size);
    zest_cmd_CopyBufferOneTime(index_staging_buffer, layer->index_data, index_staging_buffer->size);
    zest__end_single_time_commands();
    zest_FreeBuffer(vertex_staging_buffer);
    zest_FreeBuffer(index_staging_buffer);
}

void zest_DrawInstancedMesh(zest_layer_handle layer_handle, float pos[3], float rot[3], float scale[3]) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_instance);        //Call zest_StartSpriteDrawing before calling this function

    zest_mesh_instance_t* instance = (zest_mesh_instance_t*)layer->memory_refs[layer->fif].instance_ptr;

    instance->pos = zest_Vec3Set(pos[0], pos[1], pos[2]);
    instance->rotation = zest_Vec3Set(rot[0], rot[1], rot[2]);
    instance->scale = zest_Vec3Set(scale[0], scale[1], scale[2]);
    instance->color = layer->current_color;
    instance->parameters = 0;

    zest_NextInstance(layer_handle);
}

zest_mesh zest_CreateCylinder(int sides, float radius, float height, zest_color color, zest_bool cap) {
    float angle_increment = 2.0f * ZEST_PI / sides;

    int vertex_count = sides * 2 + (cap ? sides * 2 : 0);
    int index_count = sides * 2 + (cap ? sides : 0);

    zest_mesh mesh = zest_NewMesh();
    zest_vec_resize(mesh->vertices, (zest_uint)vertex_count);

    for (int i = 0; i < sides; ++i) {
        float angle = i * angle_increment;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);

        mesh->vertices[i].pos = zest_Vec3Set( x, height / 2.0f, z );
        mesh->vertices[i + sides].pos = zest_Vec3Set( x, -height / 2.0f, z );
        mesh->vertices[i].color = color;
        mesh->vertices[i + sides].color = color;
    }

    int base_index = sides * 2;

    if (cap) {
        // Generate vertices for the caps of the cylinder
        for (int i = 0; i < sides; ++i) {
            float angle = i * angle_increment;
            float x = radius * cosf(angle);
            float z = radius * sinf(angle);

            mesh->vertices[base_index + i].pos = zest_Vec3Set( x, height / 2.0f, z);
            mesh->vertices[base_index + sides + i].pos = zest_Vec3Set( x, -height / 2.0f, z);
			mesh->vertices[base_index + i].color = color;
			mesh->vertices[base_index + i + sides].color = color;
        }

        // Generate indices for the caps of the cylinder
        for (int i = 0; i < sides - 2; ++i) {
			zest_PushMeshIndex(mesh, base_index );
			zest_PushMeshIndex(mesh, base_index + i + 2);
			zest_PushMeshIndex(mesh, base_index + i + 1);

			zest_PushMeshIndex(mesh, base_index + sides);
			zest_PushMeshIndex(mesh, base_index + sides + i + 1);
			zest_PushMeshIndex(mesh, base_index + sides + i + 2);
        }
    }

    // Generate indices for the sides of the cylinder
    for (int i = 0; i < sides; ++i) {
        int next = (i + 1) % sides;

        zest_PushMeshIndex(mesh, i);
        zest_PushMeshIndex(mesh, (i + 1) % sides);
        zest_PushMeshIndex(mesh, i + sides);

        zest_PushMeshIndex(mesh, i + sides);
        zest_PushMeshIndex(mesh, (i + 1) % sides);
        zest_PushMeshIndex(mesh, (i + 1) % sides + sides );
    }

    zest_CalculateNormals(mesh);
    
    return mesh;
}

zest_mesh zest_CreateCone(int sides, float radius, float height, zest_color color) {
    // Calculate the angle between each side
    float angle_increment = 2.0f * ZEST_PI / sides;

    zest_mesh mesh = zest_NewMesh();
    zest_vec_resize(mesh->vertices, (zest_uint)sides * 2 + 2);

    // Generate vertices for the base of the cone
    for (int i = 0; i < sides; ++i) {
        float angle = i * angle_increment;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);
        mesh->vertices[i].pos = zest_Vec3Set( x, 0.0f, z);
        mesh->vertices[i].color = color;
    }

    // Generate the vertex for the tip of the cone
    mesh->vertices[sides].pos = zest_Vec3Set( 0.0f, height, 0.0f);
    mesh->vertices[sides].color = color;

    // Generate indices for the sides of the cone
    for (int i = 0; i < sides; ++i) {
        zest_PushMeshTriangle(mesh, i, sides, (i + 1) % sides);
    }

    int base_index = sides + 1;

    //Base center vertex
    mesh->vertices[base_index].pos = zest_Vec3Set( 0.0f, 0.0f, 0.0f);
    mesh->vertices[base_index].color = color;

    // Generate another set of vertices for the base of the cone
    for (int i = base_index + 1; i < base_index + sides + 1; ++i) {
        float angle = i * angle_increment;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);
        mesh->vertices[i].pos = zest_Vec3Set( x, 0.0f, z);
        mesh->vertices[i].color = color;
    }

    // Generate indices for the base of the cone
    for (int i = 0; i < sides; ++i) {
        zest_uint current_base_vertex = base_index + 1 + i;
        zest_uint next_base_vertex = base_index + 1 + ((i + 1) % sides);
        zest_PushMeshTriangle(mesh, base_index, current_base_vertex, next_base_vertex);
	}

    zest_CalculateNormals(mesh);

    return mesh;
}

zest_mesh zest_CreateSphere(int rings, int sectors, float radius, zest_color color) {
    // Calculate the angles between rings and sectors
    float ring_angle_increment = ZEST_PI / rings;
    float sector_angle_increment = 2.0f * ZEST_PI / sectors;
    float ring_angle;
    float sector_angle;

    zest_mesh mesh = zest_NewMesh();

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
            zest_PushMeshVertex(mesh, vertex.x, vertex.y, vertex.z, color);
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
                zest_PushMeshIndex(mesh, k1);
                zest_PushMeshIndex(mesh, k2);
                zest_PushMeshIndex(mesh, k1 + 1);
            }

            if (i != (rings - 1))
            {
                zest_PushMeshIndex(mesh, k1 + 1);
                zest_PushMeshIndex(mesh, k2);
                zest_PushMeshIndex(mesh, k2 + 1);
            }
        }
    }

    zest_CalculateNormals(mesh);
    
    return mesh;
}

zest_mesh zest_CreateCube(float size, zest_color color) {
    zest_mesh mesh = zest_NewMesh();
    float half_size = size * .5f;

    // Front face
    zest_PushMeshVertex(mesh, -half_size, -half_size, -half_size, color); // 0
    zest_PushMeshVertex(mesh,  half_size, -half_size, -half_size, color); // 1
    zest_PushMeshVertex(mesh,  half_size,  half_size, -half_size, color); // 2
    zest_PushMeshVertex(mesh, -half_size,  half_size, -half_size, color); // 3
    zest_PushMeshTriangle(mesh, 2, 1, 0);
    zest_PushMeshTriangle(mesh, 0, 3, 2);

    // Back face
    zest_PushMeshVertex(mesh, -half_size, -half_size,  half_size, color); // 4
    zest_PushMeshVertex(mesh, -half_size,  half_size,  half_size, color); // 5
    zest_PushMeshVertex(mesh,  half_size,  half_size,  half_size, color); // 6
    zest_PushMeshVertex(mesh,  half_size, -half_size,  half_size, color); // 7
    zest_PushMeshTriangle(mesh, 6, 5, 4);
    zest_PushMeshTriangle(mesh, 4, 7, 6);

    // Left face
    zest_PushMeshVertex(mesh, -half_size, -half_size, -half_size, color); // 8
    zest_PushMeshVertex(mesh, -half_size,  half_size, -half_size, color); // 9
    zest_PushMeshVertex(mesh, -half_size,  half_size,  half_size, color); // 10
    zest_PushMeshVertex(mesh, -half_size, -half_size,  half_size, color); // 11
    zest_PushMeshTriangle(mesh, 10, 9, 8);
    zest_PushMeshTriangle(mesh, 8, 11, 10);

    // Right face
    zest_PushMeshVertex(mesh,  half_size, -half_size, -half_size, color); // 12
    zest_PushMeshVertex(mesh,  half_size, -half_size,  half_size, color); // 13
    zest_PushMeshVertex(mesh,  half_size,  half_size,  half_size, color); // 14
    zest_PushMeshVertex(mesh,  half_size,  half_size, -half_size, color); // 15
    zest_PushMeshTriangle(mesh, 14, 13, 12);
    zest_PushMeshTriangle(mesh, 12, 15, 14);

    // Bottom face
    zest_PushMeshVertex(mesh, -half_size, -half_size, -half_size, color); // 16
    zest_PushMeshVertex(mesh, -half_size, -half_size,  half_size, color); // 17
    zest_PushMeshVertex(mesh,  half_size, -half_size,  half_size, color); // 18
    zest_PushMeshVertex(mesh,  half_size, -half_size, -half_size, color); // 19
    zest_PushMeshTriangle(mesh, 18, 17, 16);
    zest_PushMeshTriangle(mesh, 16, 19, 18);

    // Top face
    zest_PushMeshVertex(mesh, -half_size,  half_size, -half_size, color); // 20
    zest_PushMeshVertex(mesh,  half_size,  half_size, -half_size, color); // 21
    zest_PushMeshVertex(mesh,  half_size,  half_size,  half_size, color); // 22
    zest_PushMeshVertex(mesh, -half_size,  half_size,  half_size, color); // 23
    zest_PushMeshTriangle(mesh, 22, 21, 20);
    zest_PushMeshTriangle(mesh, 20, 23, 22);

    zest_CalculateNormals(mesh);

    return mesh;
}

zest_mesh zest_CreateRoundedRectangle(float width, float height, float radius, int segments, zest_bool backface, zest_color color) {
    // Calculate the number of vertices and indices needed
    int num_vertices = segments * 4 + 8;

    zest_mesh mesh = zest_NewMesh();
    //zest_vec_resize(mesh.vertices, (zest_uint)num_vertices);

    // Calculate the step angle
    float angle_increment = ZEST_PI / 2.0f / segments;

    width = ZEST__MAX(radius, width - radius * 2.f);
    height = ZEST__MAX(radius, height - radius * 2.f);

    //centre vertex;
	zest_PushMeshVertex(mesh, 0.f, 0.f, 0.0f, color);

    // Bottom left corner
    for (int i = 0; i <= segments; ++i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushMeshVertex(mesh, -width / 2.0f - x, -height / 2.0f - y, 0.0f, color);
    }

    // Bottom right corner
    for (int i = segments; i >= 0; --i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushMeshVertex(mesh, width / 2.0f + x, -height / 2.0f - y, 0.0f, color);
    }

    // Top right corner
    for (int i = 0; i <= segments; ++i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushMeshVertex(mesh, width / 2.0f + x, height / 2.0f + y, 0.0f, color);
    }

    // Top left corner
    for (int i = segments; i >= 0; --i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushMeshVertex(mesh, - width / 2.0f - x, height / 2.0f + y, 0.0f, color);
    }

    zest_uint vertex_count = zest_vec_size(mesh->vertices);

    for (zest_uint i = 1; i < vertex_count - 1; ++i) {
        zest_PushMeshTriangle(mesh, 0, i, i + 1);
    }
	zest_PushMeshTriangle(mesh, 0, vertex_count - 1, 1);

    if (backface) {
		for (zest_uint i = 1; i < vertex_count - 1; ++i) {
			zest_PushMeshTriangle(mesh, 0, i + 1, i);
		}
		zest_PushMeshTriangle(mesh, 0, 1, vertex_count - 1);
    }

    return mesh;
}
//--End Instance Draw mesh layers

//Compute shaders
zest_compute zest__new_compute(const char* name) {
    zest_compute_handle handle = (zest_compute_handle){ zest__add_store_resource(&ZestRenderer->compute_pipelines) };
    zest_compute compute = (zest_compute)zest__get_store_resource_checked(&ZestRenderer->compute_pipelines, handle.value);
    *compute = (zest_compute_t){ 0 };
    compute->magic = zest_INIT_MAGIC(zest_struct_type_compute);
    compute->backend = zest__new_compute_backend();
    compute->handle = handle;
    return compute;
}

zest_compute_builder_t zest_BeginComputeBuilder() {
    zest_compute_builder_t builder = { 0 };
	//Declare the global bindings as default
	zest_SetComputeBindlessLayout(&builder, ZestRenderer->global_bindless_set_layout);
    return builder;
}

void zest_SetComputeBindlessLayout(zest_compute_builder_t *builder, zest_set_layout_handle bindless_layout) {
    builder->bindless_layout = bindless_layout;
}

void zest_AddComputeSetLayout(zest_compute_builder_t *builder, zest_set_layout_handle layout_handle) {
    zest_vec_push(builder->non_bindless_layouts, layout_handle);
}

zest_index zest_AddComputeShader(zest_compute_builder_t* builder, zest_shader_handle shader_handle) {
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, shader_handle.value);
    zest_text_t path_text = { 0 };
    zest_vec_push(builder->shaders, shader_handle);
    return zest_vec_last_index(builder->shaders);
}

void zest_SetComputePushConstantSize(zest_compute_builder_t* builder, zest_uint size) {
    assert(size && size <= 256);        //push constants must be within 0 and 256 bytes in size
    builder->push_constant_size = size;
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

zest_compute_handle zest_FinishCompute(zest_compute_builder_t *builder, const char *name) {
    ZEST_PRINT_FUNCTION;
	zest_compute compute = zest__new_compute(name);
    compute->user_data = builder->user_data;
    compute->flags = builder->flags;

    compute->bindless_layout = builder->bindless_layout;

    ZEST__FLAG(compute->flags, zest_compute_flag_sync_required);
    ZEST__FLAG(compute->flags, zest_compute_flag_rewrite_command_buffer);
    ZEST__MAYBE_FLAG(compute->flags, zest_compute_flag_manual_fif, builder->flags & zest_compute_flag_manual_fif);

    ZEST__FLAG(compute->flags, zest_compute_flag_is_active);

    zest_bool result = zest__vk_finish_compute(builder, compute);
    zest_vec_free(builder->non_bindless_layouts);
    zest_vec_free(builder->shaders);
    if (!result) {
        zest__cleanup_compute(compute);
        return (zest_compute_handle){ 0 };
    }
    return compute->handle;
}

void zest__cleanup_compute(zest_compute compute) {
    zest__cleanup_compute_backend(compute);
    zest__remove_store_resource(&ZestRenderer->compute_pipelines, compute->handle.value);
}
//--End Compute shaders

//-- Start debug helpers
void zest_OutputMemoryUsage() {
    printf("Host Memory Pools\n");
    zest_size total_host_memory = 0;
    zest_size total_device_memory = 0;
    for (int i = 0; i != ZestDevice->memory_pool_count; ++i) {
        printf("\tMemory Pool Size: %zu\n", ZestDevice->memory_pool_sizes[i]);
        total_host_memory += ZestDevice->memory_pool_sizes[i];
    }
    printf("Device Memory Pools\n");
    zest_map_foreach(i, ZestRenderer->buffer_allocators) {
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
        zest_vec_foreach(j, buffer_allocator->memory_pools) {
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

void zest_PrintReports() {
    if (zest_map_size(ZestRenderer->reports)) {
		ZEST_PRINT("--- Zest Reports ---");
        ZEST_PRINT("");
        zest_map_foreach(i, ZestRenderer->reports) {
            zest_report_t *report = &ZestRenderer->reports.data[i];
            ZEST_PRINT("Count: %i. Message in %s on line %i: %s", report->count, report->file_name, report->line_number, report->message.str);
            ZEST_PRINT("-------------------------------------------------");
        }
        ZEST_PRINT("");
    }
}

const char *zest__struct_type_to_string(zest_struct_type struct_type) {
    switch (struct_type) {
	case zest_struct_type_view                    : return "image_view"; break;
	case zest_struct_type_view_array              : return "image_view_array"; break;
	case zest_struct_type_image                   : return "image"; break;
	case zest_struct_type_imgui_image             : return "imgui_image"; break;
	case zest_struct_type_image_collection        : return "image_collection"; break;
	case zest_struct_type_sampler                 : return "sampler"; break;
	case zest_struct_type_layer                   : return "layer"; break;
	case zest_struct_type_pipeline                : return "pipeline"; break;
	case zest_struct_type_pipeline_template       : return "pipeline_template"; break;
	case zest_struct_type_set_layout              : return "set_layout"; break;
	case zest_struct_type_descriptor_set          : return "descriptor_set"; break;
	case zest_struct_type_shader_resources        : return "shader_resources"; break;
	case zest_struct_type_uniform_buffer          : return "uniform_buffer"; break;
	case zest_struct_type_buffer_allocator        : return "buffer_allocator"; break;
	case zest_struct_type_descriptor_pool         : return "descriptor_pool"; break;
	case zest_struct_type_compute                 : return "compute"; break;
	case zest_struct_type_buffer                  : return "buffer"; break;
	case zest_struct_type_device_memory_pool      : return "device_memory_pool"; break;
	case zest_struct_type_timer                   : return "timer"; break;
	case zest_struct_type_window                  : return "window"; break;
	case zest_struct_type_shader                  : return "shader"; break;
	case zest_struct_type_imgui                   : return "imgui"; break;
	case zest_struct_type_queue                   : return "queue"; break;
	case zest_struct_type_execution_timeline      : return "execution_timeline"; break;
	case zest_struct_type_frame_graph_semaphores  : return "frame_graph_semaphores"; break;
	case zest_struct_type_swapchain               : return "swapchain"; break;
	case zest_struct_type_frame_graph             : return "frame_graph"; break;
	case zest_struct_type_pass_node               : return "pass_node"; break;
	case zest_struct_type_resource_node           : return "resource_node"; break;
	case zest_struct_type_wave_submission         : return "wave_submission"; break;
	case zest_struct_type_renderer                : return "renderer"; break;
	case zest_struct_type_device                  : return "device"; break;
	case zest_struct_type_app                     : return "app"; break;
	case zest_struct_type_vector                  : return "vector"; break;
	case zest_struct_type_bitmap                  : return "bitmap"; break;
	case zest_struct_type_render_target_group     : return "render_target_group"; break;
	case zest_struct_type_slang_info              : return "slang_info"; break;
	case zest_struct_type_render_pass             : return "render_pass"; break;
	case zest_struct_type_mesh                    : return "mesh"; break;
	case zest_struct_type_texture_asset           : return "texture_asset"; break;
	case zest_struct_type_atlas_region            : return "atlas_region"; break;
    default: return "UNKNOWN"; break;
    }
    return "UNKNOWN";
}

const char *zest__vulkan_command_to_string(zest_vulkan_command command) {
    switch (command) {
		case zest_vk_surface                : return "Surface"; break;
		case zest_vk_instance               : return "Instance"; break;
		case zest_vk_logical_device         : return "Instance"; break;
		case zest_vk_debug_messenger        : return "Debug Messenger"; break;
		case zest_vk_device_instance        : return "Device Instance"; break;
		case zest_vk_semaphore              : return "Semaphore"; break;
		case zest_vk_command_pool           : return "Command Pool"; break;
		case zest_vk_buffer                 : return "Buffer"; break;
		case zest_vk_allocate_memory_pool   : return "Allocate Memory Pool"; break;
		case zest_vk_allocate_memory_image  : return "Allocate Memory Image"; break;
		case zest_vk_fence                  : return "Fence"; break;
		case zest_vk_swapchain              : return "Swapchain"; break;
		case zest_vk_pipeline_cache         : return "Pipeline Cache"; break;
		case zest_vk_descriptor_layout      : return "Descriptor Layout"; break;
		case zest_vk_descriptor_pool        : return "Descriptor Pool"; break;
		case zest_vk_pipeline_layout        : return "Pipeline Layout"; break;
		case zest_vk_pipelines              : return "Pipelines"; break;
		case zest_vk_shader_module          : return "Shader Module"; break;
		case zest_vk_sampler                : return "Sampler"; break;
		case zest_vk_image                  : return "Image"; break;
		case zest_vk_image_view             : return "Image View"; break;
		case zest_vk_render_pass            : return "Render Pass"; break;
		case zest_vk_frame_buffer           : return "Frame Buffer"; break;
		case zest_vk_query_pool             : return "Query Pool"; break;
		case zest_vk_compute_pipeline       : return "Compute Pipeline"; break;
		default: return "UNKNOWN"; break;
    }
    return "UNKNOWN";
}

const char *zest__vulkan_context_to_string(zest_vulkan_memory_context context) {
    switch (context) {
		case zest_vk_renderer         : return "Renderer"; break;
		case zest_vk_device           : return "Device"; break;
		default: return "UNKNOWN"; break;
    }
    return "UNKNOWN";
}

void zest__print_block_info(void *allocation, zloc_header *current_block, zest_vulkan_memory_context context_filter, zest_vulkan_command command_filter) {
    if (ZEST_VALID_HANDLE(allocation)) {
		zest_struct_type struct_type = (zest_struct_type)ZEST_STRUCT_TYPE(allocation);
		switch (struct_type) {
		case zest_struct_type_vector:
			zest_vec *vector = (zest_vec*)allocation;
			ZEST_PRINT("Allocation: %p, id: %i, size: %zu, type: %s", allocation, vector->id, current_block->size, zest__struct_type_to_string(struct_type));
            break;
		default:
			ZEST_PRINT("Allocation: %p, size: %zu, type: %s", allocation, current_block->size, zest__struct_type_to_string(struct_type));
			break;
		}
    } else {
        //Is it a vulkan allocation?
        zest_vulkan_memory_info_t *vulkan_info = (zest_vulkan_memory_info_t *)allocation;
        zest_uint context = (vulkan_info->context_info & 0xff0000) >> 16;
        zest_uint command = (vulkan_info->context_info & 0xff000000) >> 24;
        if ((!command_filter || command == command_filter) && (!context_filter || context_filter == context)) {
            if (ZEST_IS_INTITIALISED(vulkan_info->context_info)) {
                ZEST_PRINT("Vulkan allocation: %p, Timestamp: %u, Category: %s, Command: %s", allocation, vulkan_info->timestamp,
                    zest__vulkan_context_to_string(context),
                    zest__vulkan_command_to_string(command)
                );
			} else if(zloc__block_size(current_block) > 0) {
				//Unknown block, print what we can about it
                ptrdiff_t offset_from_allocator = (ptrdiff_t)allocation - (ptrdiff_t)ZestDevice->allocator;
				ZEST_PRINT("Unknown Block - size: %zu (%p) Offset from allocator: %zu", zloc__block_size(current_block), allocation, offset_from_allocator);
            }
        }
    }
}

void zest_PrintMemoryBlocks(zloc_header *first_block, zest_bool output_all, zest_vulkan_memory_context context_filter, zest_vulkan_command command_filter) {
    zloc_pool_stats_t stats = { 0 };
    zest_memory_stats_t zest_stats = { 0 };
    zloc_header *current_block = first_block;
    ZEST_PRINT("Current used blocks in ZEST memory");
    while (!zloc__is_last_block_in_pool(current_block)) {
        if (zloc__is_free_block(current_block)) {
            stats.free_blocks++;
            stats.free_size += zloc__block_size(current_block);
        } else {
            stats.used_blocks++;
            zest_size block_size = zloc__block_size(current_block);
            stats.used_size += block_size;
            void *allocation = (void*)((char*)current_block + zloc__BLOCK_POINTER_OFFSET);
            if (output_all) {
                zest__print_block_info(allocation, current_block, context_filter, command_filter);
            }
			zest_vulkan_memory_info_t *vulkan_info = (zest_vulkan_memory_info_t *)allocation;
			zest_uint context = (vulkan_info->context_info & 0xff0000) >> 16;
			zest_uint command = (vulkan_info->context_info & 0xff000000) >> 24;
            if (ZEST_IS_INTITIALISED(vulkan_info->context_info)) {
                if (context == zest_vk_device) {
                    zest_stats.device_allocations++;
                } else if (context == zest_vk_renderer) {
                    zest_stats.renderer_allocations++;
                }
                if (command_filter == command && context_filter == context) {
                    zest_stats.filter_count++;
                }
            }
        }
        current_block = zloc__next_physical_block(current_block);
    }
    if (zloc__is_free_block(current_block)) {
        stats.free_blocks++;
        stats.free_size += zloc__block_size(current_block);
    } else {
		zest_size block_size = zloc__block_size(current_block);
		void *allocation = (void*)((char*)current_block + zloc__BLOCK_SIZE_OVERHEAD);
        if (block_size) {
			stats.used_blocks++;
            stats.used_size += block_size ? block_size : 0;
			ZEST_PRINT("Block size: %zu (%p)", zloc__block_size(current_block), allocation);
        }
        if (output_all) {
            zest__print_block_info(allocation, current_block, context_filter, command_filter);
        }
        zest_vulkan_memory_info_t *vulkan_info = (zest_vulkan_memory_info_t *)allocation;
        zest_uint context = (vulkan_info->context_info & 0xff0000) >> 16;
        zest_uint command = (vulkan_info->context_info & 0xff000000) >> 24;
        if (ZEST_IS_INTITIALISED(vulkan_info->context_info)) {
            if (context == zest_vk_device) {
                zest_stats.device_allocations++;
            } else if (context == zest_vk_renderer) {
                zest_stats.renderer_allocations++;
            }
            if (command_filter == command && context_filter == context) {
                zest_stats.filter_count++;
            }
        }
    }
    ZEST_PRINT("Free blocks: %u, Used blocks: %u", stats.free_blocks, stats.used_blocks);
    ZEST_PRINT("Device allocations: %u, Renderer Allocations: %u, Filter Allocations: %u", zest_stats.device_allocations, zest_stats.renderer_allocations, zest_stats.filter_count);
}

zest_uint zest_GetValidationErrorCount() {
    return zest_map_size(ZestDevice->validation_errors);
}

void zest_ResetValidationErrors() {
    zest_map_foreach(i, ZestDevice->validation_errors) {
        zest_FreeText(&ZestDevice->validation_errors.data[i]);
    }
    zest_map_free(ZestDevice->validation_errors);
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
zest_timer_handle zest_CreateTimer(double update_frequency) {
    zest_timer_handle handle = { zest__add_store_resource(&ZestRenderer->timers) };
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, handle.value);
    *timer = (zest_timer_t){ 0 };
    timer->magic = zest_INIT_MAGIC(zest_struct_type_timer);
    timer->handle = handle;
    timer->update_frequency = update_frequency;
    timer->update_tick_length = ZEST_MICROSECS_SECOND / timer->update_frequency;
    timer->update_time = 1.f / timer->update_frequency;
    timer->ticker = zest_Microsecs() - timer->update_tick_length;
    timer->max_elapsed_time = timer->update_tick_length * 4.0;
    timer->start_time = (double)zest_Microsecs();
    timer->current_time = (double)zest_Microsecs();
    return handle;
}

void zest_FreeTimer(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
    zest__remove_store_resource(&ZestRenderer->timers, timer_handle.value);
}

void zest_TimerReset(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
    timer->start_time = (double)zest_Microsecs();
    timer->current_time = (double)zest_Microsecs();
}

double zest_TimerDeltaTime(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
    return timer->delta_time;
}

void zest_TimerTick(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
    timer->delta_time = (double)zest_Microsecs() - timer->start_time;
}

double zest_TimerUpdateTime(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
    return timer->update_time;
}

double zest_TimerFrameLength(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
    return timer->update_tick_length;
}

double zest_TimerAccumulate(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
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

int zest_TimerPendingTicks(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
    return (int)(timer->accumulator / timer->update_tick_length);
}

void zest_TimerUnAccumulate(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
    timer->accumulator -= timer->update_tick_length;
    timer->accumulator_delta = timer->accumulator_delta - timer->accumulator;
    timer->update_count++;
    timer->time_passed += timer->update_time;
    timer->seconds_passed += timer->update_time * 1000.f;
}

zest_bool zest_TimerDoUpdate(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
    return timer->accumulator >= timer->update_tick_length;
}

double zest_TimerLerp(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
    return timer->update_count > 1 ? 1.f : timer->lerp;
}

void zest_TimerSet(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
    timer->lerp = timer->accumulator / timer->update_tick_length;
}

void zest_TimerSetMaxFrames(zest_timer_handle timer_handle, double frames) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
    timer->max_elapsed_time = timer->update_tick_length * frames;
}

void zest_TimerSetUpdateFrequency(zest_timer_handle timer_handle, double frequency) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
    timer->update_frequency = frequency;
    timer->update_tick_length = ZEST_MICROSECS_SECOND / timer->update_frequency;
    timer->update_time = 1.f / timer->update_frequency;
    timer->ticker = zest_Microsecs() - timer->update_tick_length;
}

double zest_TimerUpdateFrequency(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
    return timer->update_frequency;
}

zest_bool zest_TimerUpdateWasRun(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(&ZestRenderer->timers, timer_handle.value);
    return timer->update_count > 0;
}
//-- End Timer Functions
