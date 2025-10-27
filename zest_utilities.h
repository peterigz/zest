// zest_utilities.h
// A collection of single-file header implementations for Zest.

//To include this file in a project, include it in a c or c++ file with the necessary defines to include
//the implementations that you want to use or ZEST_ALL_UTILITIES to include everything (note that sdl and
//glfw helper functions will only be included if those libraries are included as they just depend on the
//presence of the respective version defines in those libraries

/*
GLFW Helper functions
* To use these functions make sure you include zest_utilities.h AFTER you include glfw3.h
	- [GLFW_Utilities]
SDL Helper functions
* To use these functions make sure you include zest_utilities.h AFTER you include SDL.h
	- [SDL2_Utilities]
tiny_ktx.h			Load and save ktx files		- Deano Calver https://github.com/DeanoC/tiny_ktx
* To use define ZEST_KTX_IMPLEMENTATION before including zest.h in a c/c++ file. Only do this once.
	- [Zest_ktx_helper_functions]
	- [Zest_ktx_helper_implementation]
msdf.h				Create msdf bitmaps for font rendering - From https://github.com/exezin/msdf-c
* To use define MSDF_IMPLEMENTATION or ZEST_MSDF_IMPLEMENTATION before including zest_utilities.h in a c/c++ file. Only do this once.
  Requires stb_truetype.h
	- [msdf_fonts]
*/    

#ifdef ZEST_ALL_UTILITIES_IMPLEMENTATION
#define ZEST_MSDF_IMPLEMENTATION
#define ZEST_KTX_IMPLEMENTATION
#define ZEST_IMAGE_WRITE_IMPLEMENTATION
#endif

// --- GLFW_Utilities --------------------------------------------------------
// To use this implementation, include <GLFW/glfw3.h> before this file

#if defined(ZEST_VULKAN_IMPLEMENTATION) && defined(GLFW_VERSION_MAJOR)
#ifdef _WIN32
#ifndef GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>   // for glfwGetWin32Window()
#endif
#endif

#ifndef ZEST_UTILITIES_H

#ifdef __cplusplus
extern "C" {
#endif

//GLFW Header
zest_window_data_t zest_implglfw_CreateWindow( int x, int y, int width, int height, zest_bool maximised, const char *title);
void zest_implglfw_GetWindowSizeCallback( void* native_window_handle, int* fb_width, int* fb_height, int* window_width, int* window_height );
void zest_implglfw_DestroyWindow(zest_context context);

//SDL Header
zest_window_data_t zest_implsdl2_CreateWindow(int x, int y, int width, int height, zest_bool maximised, const char* title);
void zest_implsdl2_GetWindowSizeCallback(void *window_handle, int *fb_width, int *fb_height, int *window_width, int *window_height);
void zest_implsdl2_DestroyWindow(zest_context context);

//Tiny ktx Header
ZEST_PRIVATE void zest__tinyktxCallbackError(void *user, char const *msg);
ZEST_PRIVATE void *zest__tinyktxCallbackAlloc(void *user, size_t size);
ZEST_PRIVATE void zest__tinyktxCallbackFree(void *user, void *data);
ZEST_PRIVATE size_t zest__tinyktxCallbackRead(void *user, void *data, size_t size);
ZEST_PRIVATE bool zest__tinyktxCallbackSeek(void *user, int64_t offset);
ZEST_PRIVATE int64_t zest__tinyktxCallbackTell(void *user);
ZEST_API zest_image_collection_handle zest__load_ktx(zest_context context, const char *file_path);
ZEST_API zest_image_handle zest_LoadCubemap(zest_context context, const char *name, const char *file_name);

//MSDF header
typedef enum zest_character_flag_bits {
	zest_character_flag_none = 0,
	zest_character_flag_skip = 1 << 0,
	zest_character_flag_new_line = 1 << 1,
	zest_character_flag_whitespace = 1 << 2,
} zest_character_flag_bits;
typedef zest_uint zest_character_flags;

typedef struct zest_font_resources_t {
	zest_pipeline_template pipeline;
	zest_shader_resources_handle shader_resources;
} zest_font_resources_t;

typedef struct zest_font_uniform_buffer_data_t {
    zest_matrix4 view;
    zest_matrix4 proj;
    zest_vec2 screen_size;
} zest_font_uniform_buffer_data_t;

typedef struct zest_font_character_t {
	zest_atlas_region region;
    float x_offset; 
	float y_offset;
    float width; 
	float height;
    float x_advance;
	zest_character_flags flags;
} zest_font_character_t;

typedef struct zest_msdf_font_settings_t {
	zest_vec4 transform;
    zest_vec4 font_color;
    zest_vec4 shadow_color;
    zest_vec2 shadow_offset; // In screen pixels, e.g., (2.0, 2.0)
	zest_vec2 unit_range;
	float in_bias;
	float out_bias;
	float smoothness;
	float gamma;
    zest_uint sampler_index;
    zest_uint image_index;
} zest_msdf_font_settings_t;

typedef struct zest_msdf_font_t {
	zest_image_collection_handle font_atlas;
	zest_image_handle font_image;
	zest_font_character_t characters[256];
	zest_uint font_binding_index;
	zest_msdf_font_settings_t settings;
	zest_context context;
	float size;
	float sdf_range;
	float y_max_offset;
} zest_msdf_font_t;

typedef struct zest_font_instance_t {           //48 bytes
	zest_vec2 position;                   		//The position of the sprite with rotation in w and stretch in z
	zest_u64 uv;                                //The UV coords of the image in the texture packed into a u64 snorm (4 16bit floats)
	zest_uint size; 		                    //Size of the sprite in pixels and the handle packed into a u64 (4 16bit floats)
	zest_uint texture_array;             		//Index in the texture array
	zest_uint padding[2];
} zest_font_instance_t;

ZEST_PRIVATE void* zest__msdf_allocation(size_t size, void* ctx);
ZEST_PRIVATE void zest__msdf_free(void* ptr, void* ctx);
ZEST_API zest_font_resources_t zest_CreateFontResources(zest_context context);
ZEST_API zest_layer_handle zest_CreateFontLayer(zest_context context, const char *name);
ZEST_API zest_msdf_font_t zest_CreateMSDF(zest_context context, const char *filename, zest_uint font_sampler_binding_index, float font_size, float sdf_range);
ZEST_API void zest_SaveMSDF(zest_msdf_font_t *font, const char *filename);
ZEST_API void zest_UpdateFontTransform(zest_msdf_font_t *font);
ZEST_API void zest_SetFontTransform(zest_msdf_font_t *font, float transform[4]);
ZEST_API void zest_SetFontSettings(zest_msdf_font_t *font, float inner_bias, float outer_bias, float smoothness, float gamma);
ZEST_API void zest_SetFontColor(zest_msdf_font_t *font, float r, float g, float b, float a);
ZEST_API void zest_SetFontShadowColor(zest_msdf_font_t *font, float r, float g, float b, float a);
ZEST_API void zest_SetFontShadowOffset(zest_msdf_font_t *font, float x, float y);
ZEST_API void zest_FreeFont(zest_msdf_font_t *font);
ZEST_API float zest_TextWidth(zest_msdf_font_t *font, const char* text, float font_size, float letter_spacing);
ZEST_API void zest_SetMSDFFontDrawing(zest_layer_handle layer_handle, zest_msdf_font_t *font, zest_font_resources_t *font_resources);
ZEST_API float zest_DrawMSDFText(zest_layer_handle layer_handle, const char* text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing);
ZEST_API void zest_UpdateFontUniformBuffer(zest_uniform_buffer_handle handle);

#ifdef __cplusplus
}
#endif

#endif

#ifdef GLFW_VERSION_MAJOR

zest_window_data_t zest_implglfw_CreateWindow( int x, int y, int width, int height, zest_bool maximised, const char *title) {
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	if (maximised) {
		glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
	}

	GLFWwindow *window_handle = glfwCreateWindow(width, height, title, 0, 0);
	if (!maximised) {
		glfwSetWindowPos(window_handle, x, y);
	}

	zest_window_data_t window_handles = { 0 };
	window_handles.width = width;
	window_handles.height = height;

	if (maximised) {
		glfwGetWindowSize(window_handle, &window_handles.width, &window_handles.height);
	}

	window_handles.window_handle = window_handle;
	#if defined(_WIN32)
	window_handles.native_handle = (void*)glfwGetWin32Window(window_handle);
	window_handles.display = GetModuleHandle(NULL);
	#elif defined(__linux__)
	window_handles.native_handle = (void*)glfwGetX11Window(glfw_window);
	window_handles.display = glfwGetX11Display();
	#endif

	window_handles.window_sizes_callback = zest_implglfw_GetWindowSizeCallback;

	return window_handles;
}

void zest_implglfw_GetWindowSizeCallback(void *window_handle, int *fb_width, int *fb_height, int *window_width, int *window_height) {
	GLFWwindow *handle = (GLFWwindow *)window_handle;
    glfwGetFramebufferSize(handle, fb_width, fb_height);
	glfwGetWindowSize(handle, window_width, window_height);
}

void zest_implglfw_DestroyWindow(zest_context context) {
	GLFWwindow *handle = (GLFWwindow *)zest_Window(context);
	glfwDestroyWindow(handle);
}

#endif // GLFW_VERSION_MAJOR
// --- End GLFW Utilities ----------------------------------------------------

// --- SDL2_Utilities --------------------------------------------------------
// To use this implementation, include <SDL.h> before this file

#if defined(ZEST_VULKAN_IMPLEMENTATION) && defined(SDL_MAJOR_VERSION)
#include <SDL_vulkan.h>
#endif


#if defined(SDL_MAJOR_VERSION)
#include <SDL_syswm.h>

zest_window_data_t zest_implsdl2_CreateWindow(int x, int y, int width, int height, zest_bool maximised, const char *title) {
    Uint32 flags = 0;
#if defined(ZEST_VULKAN_IMPLEMENTATION)
    flags = SDL_WINDOW_VULKAN;
#elif defined(ZEST_IMPLEMENT_DX12)
    // flags = SDL_WINDOW_DX12; // Placeholder for DX12
#elif defined(ZEST_IMPLEMENT_METAL)
    // flags = SDL_WINDOW_METAL; // Placeholder for Metal
#endif

    if (maximised) {
        flags |= SDL_WINDOW_MAXIMIZED;
    }

    SDL_Window *window_handle = SDL_CreateWindow(title, x, y, width, height, flags);

    zest_window_data_t window_handles = { 0 };
    window_handles.width = width;
    window_handles.height = height;

    if (maximised) {
        SDL_GetWindowSize(window_handle, &window_handles.width, &window_handles.height);
    }

    window_handles.window_handle = window_handle;

    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(window_handle, &wmi)) {
        // Handle error, for now returning partially filled struct
        return window_handles;
    }

#if defined(_WIN32)
    window_handles.native_handle = wmi.info.win.window;
    window_handles.display = wmi.info.win.hinstance;
#elif defined(__linux__)
#if defined(ZEST_VULKAN_IMPLEMENTATION)
    window_handles.native_handle = (void*)(uintptr_t)wmi.info.x11.window;
    window_handles.display = wmi.info.x11.display;
#endif
#endif

    window_handles.window_sizes_callback = zest_implsdl2_GetWindowSizeCallback;

    return window_handles;
}

void zest_implsdl2_GetWindowSizeCallback(void *window_handle, int *fb_width, int *fb_height, int *window_width, int *window_height) {
#if defined(ZEST_VULKAN_IMPLEMENTATION)
	SDL_Vulkan_GetDrawableSize((SDL_Window*)window_handle, fb_width, fb_height);
#elif defined(ZEST_IMPLEMENT_DX12)
    // DX12 equivalent
#elif defined(ZEST_IMPLEMENT_METAL)
    // Metal equivalent
#endif
	SDL_GetWindowSize((SDL_Window*)window_handle, window_width, window_height);
}

void zest_implsdl2_DestroyWindow(zest_context context) {
	SDL_Window *handle = (SDL_Window *)zest_Window(context);
	SDL_DestroyWindow(handle);
}

#endif // SDL_MAJOR_VERSION
// --- End SDL2 Utilities ----------------------------------------------------

//Zest_ktx_helper_functions
#ifdef __cplusplus
extern "C" {
#endif

#include "tinyktx.h"

//End Zest_ktx_helper_functions

#ifdef ZEST_KTX_IMPLEMENTATION

#define TINYKTX_IMPLEMENTATION
#include "tinyktx.h"
//Zest_ktx_helper_implementation
void zest__tinyktxCallbackError(void *user, char const *msg) {
	ZEST_PRINT("Tiny_Ktx ERROR: %s", msg);
}

void *zest__tinyktxCallbackAlloc(void *user, size_t size) {
	zest_ktx_user_context_t *user_context = (zest_ktx_user_context_t*)user;
	return ZEST__ALLOCATE(user_context->context->device->allocator, size);
}

void zest__tinyktxCallbackFree(void *user, void *data) {
	zest_ktx_user_context_t *user_context = (zest_ktx_user_context_t*)user;
	ZEST__FREE(user_context->context->device->allocator, data);
}

size_t zest__tinyktxCallbackRead(void *user, void *data, size_t size) {
	zest_ktx_user_context_t *user_context = (zest_ktx_user_context_t*)user;
	FILE* handle = user_context->file;
	return fread(data, 1, size, handle);
}

bool zest__tinyktxCallbackSeek(void *user, int64_t offset) {
	zest_ktx_user_context_t *user_context = (zest_ktx_user_context_t*)user;
	FILE* handle = user_context->file;
	return fseek(handle, (long)offset, SEEK_SET) == 0;
}

int64_t zest__tinyktxCallbackTell(void *user) {
	zest_ktx_user_context_t *user_context = (zest_ktx_user_context_t*)user;
	FILE* handle = user_context->file;
	return ftell(handle);
}

zest_format zest__convert_tktx_format(TinyKtx_Format ktx_format) {
	TinyImageFormat_VkFormat format = (TinyImageFormat_VkFormat)ktx_format;
	switch (format) {
		case TIF_VK_FORMAT_R4G4_UNORM_PACK8: return zest_format_r4g4_unorm_pack8; break;
		case TIF_VK_FORMAT_R4G4B4A4_UNORM_PACK16: return zest_format_r4g4b4a4_unorm_pack16; break;
		case TIF_VK_FORMAT_B4G4R4A4_UNORM_PACK16: return zest_format_b4g4r4a4_unorm_pack16; break;
		case TIF_VK_FORMAT_R5G6B5_UNORM_PACK16: return zest_format_r5g6b5_unorm_pack16; break;
		case TIF_VK_FORMAT_B5G6R5_UNORM_PACK16: return zest_format_b5g6r5_unorm_pack16; break;
		case TIF_VK_FORMAT_R5G5B5A1_UNORM_PACK16: return zest_format_r5g5b5a1_unorm_pack16; break;
		case TIF_VK_FORMAT_B5G5R5A1_UNORM_PACK16: return zest_format_b5g5r5a1_unorm_pack16; break;
		case TIF_VK_FORMAT_A1R5G5B5_UNORM_PACK16: return zest_format_a1r5g5b5_unorm_pack16; break;
		case TIF_VK_FORMAT_R8_UNORM: return zest_format_r8_unorm; break;
		case TIF_VK_FORMAT_R8_SNORM: return zest_format_r8_snorm; break;
		case TIF_VK_FORMAT_R8_UINT: return zest_format_r8_uint; break;
		case TIF_VK_FORMAT_R8_SINT: return zest_format_r8_sint; break;
		case TIF_VK_FORMAT_R8_SRGB: return zest_format_r8_srgb; break;
		case TIF_VK_FORMAT_R8G8_UNORM: return zest_format_r8g8_unorm; break;
		case TIF_VK_FORMAT_R8G8_SNORM: return zest_format_r8g8_snorm; break;
		case TIF_VK_FORMAT_R8G8_UINT: return zest_format_r8g8_uint; break;
		case TIF_VK_FORMAT_R8G8_SINT: return zest_format_r8g8_sint; break;
		case TIF_VK_FORMAT_R8G8_SRGB: return zest_format_r8g8_srgb; break;
		case TIF_VK_FORMAT_R8G8B8_UNORM: return zest_format_r8g8b8_unorm; break;
		case TIF_VK_FORMAT_R8G8B8_SNORM: return zest_format_r8g8b8_snorm; break;
		case TIF_VK_FORMAT_R8G8B8_UINT: return zest_format_r8g8b8_uint; break;
		case TIF_VK_FORMAT_R8G8B8_SINT: return zest_format_r8g8b8_sint; break;
		case TIF_VK_FORMAT_R8G8B8_SRGB: return zest_format_r8g8b8_srgb; break;
		case TIF_VK_FORMAT_B8G8R8_UNORM: return zest_format_b8g8r8_unorm; break;
		case TIF_VK_FORMAT_B8G8R8_SNORM: return zest_format_b8g8r8_snorm; break;
		case TIF_VK_FORMAT_B8G8R8_UINT: return zest_format_b8g8r8_uint; break;
		case TIF_VK_FORMAT_B8G8R8_SINT: return zest_format_b8g8r8_sint; break;
		case TIF_VK_FORMAT_B8G8R8_SRGB: return zest_format_b8g8r8_srgb; break;
		case TIF_VK_FORMAT_R8G8B8A8_UNORM: return zest_format_r8g8b8a8_unorm; break;
		case TIF_VK_FORMAT_R8G8B8A8_SNORM: return zest_format_r8g8b8a8_snorm; break;
		case TIF_VK_FORMAT_R8G8B8A8_UINT: return zest_format_r8g8b8a8_uint; break;
		case TIF_VK_FORMAT_R8G8B8A8_SINT: return zest_format_r8g8b8a8_sint; break;
		case TIF_VK_FORMAT_R8G8B8A8_SRGB: return zest_format_r8g8b8a8_srgb; break;
		case TIF_VK_FORMAT_B8G8R8A8_UNORM: return zest_format_b8g8r8a8_unorm; break;
		case TIF_VK_FORMAT_B8G8R8A8_SNORM: return zest_format_b8g8r8a8_snorm; break;
		case TIF_VK_FORMAT_B8G8R8A8_UINT: return zest_format_b8g8r8a8_uint; break;
		case TIF_VK_FORMAT_B8G8R8A8_SINT: return zest_format_b8g8r8a8_sint; break;
		case TIF_VK_FORMAT_B8G8R8A8_SRGB: return zest_format_b8g8r8a8_srgb; break;
		case TIF_VK_FORMAT_A8B8G8R8_UNORM_PACK32: return zest_format_a8b8g8r8_unorm_pack32; break;
		case TIF_VK_FORMAT_A8B8G8R8_SNORM_PACK32: return zest_format_a8b8g8r8_snorm_pack32; break;
		case TIF_VK_FORMAT_A8B8G8R8_UINT_PACK32: return zest_format_a8b8g8r8_uint_pack32; break;
		case TIF_VK_FORMAT_A8B8G8R8_SINT_PACK32: return zest_format_a8b8g8r8_sint_pack32; break;
		case TIF_VK_FORMAT_A8B8G8R8_SRGB_PACK32: return zest_format_a8b8g8r8_srgb_pack32; break;
		case TIF_VK_FORMAT_A2R10G10B10_UNORM_PACK32: return zest_format_a2r10g10b10_unorm_pack32; break;
		case TIF_VK_FORMAT_A2R10G10B10_SNORM_PACK32: return zest_format_a2r10g10b10_snorm_pack32; break;
		case TIF_VK_FORMAT_A2R10G10B10_UINT_PACK32: return zest_format_a2r10g10b10_uint_pack32; break;
		case TIF_VK_FORMAT_A2R10G10B10_SINT_PACK32: return zest_format_a2r10g10b10_sint_pack32; break;
		case TIF_VK_FORMAT_A2B10G10R10_UNORM_PACK32: return zest_format_a2b10g10r10_unorm_pack32; break;
		case TIF_VK_FORMAT_A2B10G10R10_SNORM_PACK32: return zest_format_a2b10g10r10_snorm_pack32; break;
		case TIF_VK_FORMAT_A2B10G10R10_UINT_PACK32: return zest_format_a2b10g10r10_uint_pack32; break;
		case TIF_VK_FORMAT_A2B10G10R10_SINT_PACK32: return zest_format_a2b10g10r10_sint_pack32; break;
		case TIF_VK_FORMAT_R16_UNORM: return zest_format_r16_unorm; break;
		case TIF_VK_FORMAT_R16_SNORM: return zest_format_r16_snorm; break;
		case TIF_VK_FORMAT_R16_UINT: return zest_format_r16_uint; break;
		case TIF_VK_FORMAT_R16_SINT: return zest_format_r16_sint; break;
		case TIF_VK_FORMAT_R16_SFLOAT: return zest_format_r16_sfloat; break;
		case TIF_VK_FORMAT_R16G16_UNORM: return zest_format_r16g16_unorm; break;
		case TIF_VK_FORMAT_R16G16_SNORM: return zest_format_r16g16_snorm; break;
		case TIF_VK_FORMAT_R16G16_UINT: return zest_format_r16g16_uint; break;
		case TIF_VK_FORMAT_R16G16_SINT: return zest_format_r16g16_sint; break;
		case TIF_VK_FORMAT_R16G16_SFLOAT: return zest_format_r16g16_sfloat; break;
		case TIF_VK_FORMAT_R16G16B16_UNORM: return zest_format_r16g16b16_unorm; break;
		case TIF_VK_FORMAT_R16G16B16_SNORM: return zest_format_r16g16b16_snorm; break;
		case TIF_VK_FORMAT_R16G16B16_UINT: return zest_format_r16g16b16_uint; break;
		case TIF_VK_FORMAT_R16G16B16_SINT: return zest_format_r16g16b16_sint; break;
		case TIF_VK_FORMAT_R16G16B16_SFLOAT: return zest_format_r16g16b16_sfloat; break;
		case TIF_VK_FORMAT_R16G16B16A16_UNORM: return zest_format_r16g16b16a16_unorm; break;
		case TIF_VK_FORMAT_R16G16B16A16_SNORM: return zest_format_r16g16b16a16_snorm; break;
		case TIF_VK_FORMAT_R16G16B16A16_UINT: return zest_format_r16g16b16a16_uint; break;
		case TIF_VK_FORMAT_R16G16B16A16_SINT: return zest_format_r16g16b16a16_sint; break;
		case TIF_VK_FORMAT_R16G16B16A16_SFLOAT: return zest_format_r16g16b16a16_sfloat; break;
		case TIF_VK_FORMAT_R32_UINT: return zest_format_r32_uint; break;
		case TIF_VK_FORMAT_R32_SINT: return zest_format_r32_sint; break;
		case TIF_VK_FORMAT_R32_SFLOAT: return zest_format_r32_sfloat; break;
		case TIF_VK_FORMAT_R32G32_UINT: return zest_format_r32g32_uint; break;
		case TIF_VK_FORMAT_R32G32_SINT: return zest_format_r32g32_sint; break;
		case TIF_VK_FORMAT_R32G32_SFLOAT: return zest_format_r32g32_sfloat; break;
		case TIF_VK_FORMAT_R32G32B32_UINT: return zest_format_r32g32b32_uint; break;
		case TIF_VK_FORMAT_R32G32B32_SINT: return zest_format_r32g32b32_sint; break;
		case TIF_VK_FORMAT_R32G32B32_SFLOAT: return zest_format_r32g32b32_sfloat; break;
		case TIF_VK_FORMAT_R32G32B32A32_UINT: return zest_format_r32g32b32a32_uint; break;
		case TIF_VK_FORMAT_R32G32B32A32_SINT: return zest_format_r32g32b32a32_sint; break;
		case TIF_VK_FORMAT_R32G32B32A32_SFLOAT: return zest_format_r32g32b32a32_sfloat; break;
		case TIF_VK_FORMAT_R64_UINT: return zest_format_r64_uint; break;
		case TIF_VK_FORMAT_R64_SINT: return zest_format_r64_sint; break;
		case TIF_VK_FORMAT_R64_SFLOAT: return zest_format_r64_sfloat; break;
		case TIF_VK_FORMAT_R64G64_UINT: return zest_format_r64g64_uint; break;
		case TIF_VK_FORMAT_R64G64_SINT: return zest_format_r64g64_sint; break;
		case TIF_VK_FORMAT_R64G64_SFLOAT: return zest_format_r64g64_sfloat; break;
		case TIF_VK_FORMAT_R64G64B64_UINT: return zest_format_r64g64b64_uint; break;
		case TIF_VK_FORMAT_R64G64B64_SINT: return zest_format_r64g64b64_sint; break;
		case TIF_VK_FORMAT_R64G64B64_SFLOAT: return zest_format_r64g64b64_sfloat; break;
		case TIF_VK_FORMAT_R64G64B64A64_UINT: return zest_format_r64g64b64a64_uint; break;
		case TIF_VK_FORMAT_R64G64B64A64_SINT: return zest_format_r64g64b64a64_sint; break;
		case TIF_VK_FORMAT_R64G64B64A64_SFLOAT: return zest_format_r64g64b64a64_sfloat; break;
		case TIF_VK_FORMAT_B10G11R11_UFLOAT_PACK32: return zest_format_b10g11r11_ufloat_pack32; break;
		case TIF_VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return zest_format_e5b9g9r9_ufloat_pack32; break;
		case TIF_VK_FORMAT_D16_UNORM: return zest_format_d16_unorm; break;
		case TIF_VK_FORMAT_X8_D24_UNORM_PACK32: return zest_format_x8_d24_unorm_pack32; break;
		case TIF_VK_FORMAT_D32_SFLOAT: return zest_format_d32_sfloat; break;
		case TIF_VK_FORMAT_S8_UINT: return zest_format_s8_uint; break;
		case TIF_VK_FORMAT_D16_UNORM_S8_UINT: return zest_format_d16_unorm_s8_uint; break;
		case TIF_VK_FORMAT_D24_UNORM_S8_UINT: return zest_format_d24_unorm_s8_uint; break;
		case TIF_VK_FORMAT_D32_SFLOAT_S8_UINT: return zest_format_d32_sfloat_s8_uint; break;
		case TIF_VK_FORMAT_BC1_RGB_UNORM_BLOCK: return zest_format_bc1_rgb_unorm_block; break;
		case TIF_VK_FORMAT_BC1_RGB_SRGB_BLOCK: return zest_format_bc1_rgb_srgb_block; break;
		case TIF_VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return zest_format_bc1_rgba_unorm_block; break;
		case TIF_VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return zest_format_bc1_rgba_srgb_block; break;
		case TIF_VK_FORMAT_BC2_UNORM_BLOCK: return zest_format_bc2_unorm_block; break;
		case TIF_VK_FORMAT_BC2_SRGB_BLOCK: return zest_format_bc2_srgb_block; break;
		case TIF_VK_FORMAT_BC3_UNORM_BLOCK: return zest_format_bc3_unorm_block; break;
		case TIF_VK_FORMAT_BC3_SRGB_BLOCK: return zest_format_bc3_srgb_block; break;
		case TIF_VK_FORMAT_BC4_UNORM_BLOCK: return zest_format_bc4_unorm_block; break;
		case TIF_VK_FORMAT_BC4_SNORM_BLOCK: return zest_format_bc4_snorm_block; break;
		case TIF_VK_FORMAT_BC5_UNORM_BLOCK: return zest_format_bc5_unorm_block; break;
		case TIF_VK_FORMAT_BC5_SNORM_BLOCK: return zest_format_bc5_snorm_block; break;
		case TIF_VK_FORMAT_BC6H_UFLOAT_BLOCK: return zest_format_bc6h_ufloat_block; break;
		case TIF_VK_FORMAT_BC6H_SFLOAT_BLOCK: return zest_format_bc6h_sfloat_block; break;
		case TIF_VK_FORMAT_BC7_UNORM_BLOCK: return zest_format_bc7_unorm_block; break;
		case TIF_VK_FORMAT_BC7_SRGB_BLOCK: return zest_format_bc7_srgb_block; break;
		case TIF_VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return zest_format_etc2_r8g8b8_unorm_block; break;
		case TIF_VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: return zest_format_etc2_r8g8b8_srgb_block; break;
		case TIF_VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: return zest_format_etc2_r8g8b8a1_unorm_block; break;
		case TIF_VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: return zest_format_etc2_r8g8b8a1_srgb_block; break;
		case TIF_VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return zest_format_etc2_r8g8b8a8_unorm_block; break;
		case TIF_VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: return zest_format_etc2_r8g8b8a8_srgb_block; break;
		case TIF_VK_FORMAT_EAC_R11_UNORM_BLOCK: return zest_format_eac_r11_unorm_block; break;
		case TIF_VK_FORMAT_EAC_R11_SNORM_BLOCK: return zest_format_eac_r11_snorm_block; break;
		case TIF_VK_FORMAT_EAC_R11G11_UNORM_BLOCK: return zest_format_eac_r11g11_unorm_block; break;
		case TIF_VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return zest_format_eac_r11g11_snorm_block; break;
		case TIF_VK_FORMAT_ASTC_4x4_UNORM_BLOCK: return zest_format_astc_4X4_unorm_block; break;
		case TIF_VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return zest_format_astc_4X4_srgb_block; break;
		case TIF_VK_FORMAT_ASTC_5x4_UNORM_BLOCK: return zest_format_astc_5X4_unorm_block; break;
		case TIF_VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return zest_format_astc_5X4_srgb_block; break;
		case TIF_VK_FORMAT_ASTC_5x5_UNORM_BLOCK: return zest_format_astc_5X5_unorm_block; break;
		case TIF_VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return zest_format_astc_5X5_srgb_block; break;
		case TIF_VK_FORMAT_ASTC_6x5_UNORM_BLOCK: return zest_format_astc_6X5_unorm_block; break;
		case TIF_VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return zest_format_astc_6X5_srgb_block; break;
		case TIF_VK_FORMAT_ASTC_6x6_UNORM_BLOCK: return zest_format_astc_6X6_unorm_block; break;
		case TIF_VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return zest_format_astc_6X6_srgb_block; break;
		case TIF_VK_FORMAT_ASTC_8x5_UNORM_BLOCK: return zest_format_astc_8X5_unorm_block; break;
		case TIF_VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return zest_format_astc_8X5_srgb_block; break;
		case TIF_VK_FORMAT_ASTC_8x6_UNORM_BLOCK: return zest_format_astc_8X6_unorm_block; break;
		case TIF_VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return zest_format_astc_8X6_srgb_block; break;
		case TIF_VK_FORMAT_ASTC_8x8_UNORM_BLOCK: return zest_format_astc_8X8_unorm_block; break;
		case TIF_VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return zest_format_astc_8X8_srgb_block; break;
		case TIF_VK_FORMAT_ASTC_10x5_UNORM_BLOCK: return zest_format_astc_10X5_unorm_block; break;
		case TIF_VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return zest_format_astc_10X5_srgb_block; break;
		case TIF_VK_FORMAT_ASTC_10x6_UNORM_BLOCK: return zest_format_astc_10X6_unorm_block; break;
		case TIF_VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return zest_format_astc_10X6_srgb_block; break;
		case TIF_VK_FORMAT_ASTC_10x8_UNORM_BLOCK: return zest_format_astc_10X8_unorm_block; break;
		case TIF_VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return zest_format_astc_10X8_srgb_block; break;
		case TIF_VK_FORMAT_ASTC_10x10_UNORM_BLOCK: return zest_format_astc_10X10_unorm_block; break;
		case TIF_VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return zest_format_astc_10X10_srgb_block; break;
		case TIF_VK_FORMAT_ASTC_12x10_UNORM_BLOCK: return zest_format_astc_12X10_unorm_block; break;
		case TIF_VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return zest_format_astc_12X10_srgb_block; break;
		case TIF_VK_FORMAT_ASTC_12x12_UNORM_BLOCK: return zest_format_astc_12X12_unorm_block; break;
		case TIF_VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return zest_format_astc_12X12_srgb_block; break;
		default: return zest_format_undefined;
	}
	return zest_format_undefined;
}

zest_image_collection_handle zest__load_ktx(zest_context context, const char *file_path) {

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
		return ZEST__ZERO_INIT(zest_image_collection_handle);
	}

	zest_ktx_user_context_t user_context;
	user_context.context = context;
	user_context.file = file;

	TinyKtx_ContextHandle ctx = TinyKtx_CreateContext(&callbacks, &user_context);

	if (!TinyKtx_ReadHeader(ctx)) {
		return ZEST__ZERO_INIT(zest_image_collection_handle);
	}

	zest_format format = zest__convert_tktx_format(TinyKtx_GetFormat(ctx));
	if (format == zest_format_undefined) {
		TinyKtx_DestroyContext(ctx);
		return ZEST__ZERO_INIT(zest_image_collection_handle);
	}

	zest_uint width = TinyKtx_Width(ctx);
	zest_uint height = TinyKtx_Height(ctx);
	zest_uint depth = TinyKtx_Depth(ctx);

	zest_uint mip_count = TinyKtx_NumberOfMipmaps(ctx);
	zest_image_collection_flags flags = TinyKtx_IsCubemap(ctx) ? zest_image_collection_flag_is_cube_map : 0;
	zest_image_collection_handle image_collection_handle = zest_CreateImageCollection(context, format, mip_count, flags);
	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource_checked(context, image_collection_handle.value);

	//First pass to set the bitmap array
	size_t offset = 0;
	for (zest_uint i = 0; i < mip_count; ++i) {
		zest_uint image_size = TinyKtx_ImageSize(ctx, i);
		zest_SetImageCollectionBitmapMeta(image_collection_handle, i, width, height, 0, 0, image_size, offset);
		offset += image_size;
		if (width > 1) width /= 2;
		if (height > 1) height /= 2;
	}

	zest_AllocateImageCollectionBitmapArray(image_collection_handle);
	width = TinyKtx_Width(ctx);
	height = TinyKtx_Height(ctx);

	zest_bitmap_array_t *bitmap_array = zest_GetImageCollectionBitmapArray(image_collection_handle);

	zest_vec_foreach (i, bitmap_array->meta) {
		zest_ImageCollectionCopyToBitmapArray(image_collection_handle, i, TinyKtx_ImageRawData(ctx, i), TinyKtx_ImageSize(ctx, i));
	}

	TinyKtx_DestroyContext(ctx);
	return image_collection_handle;
}

zest_image_handle zest_LoadCubemap(zest_context context, const char *name, const char *file_name) {
    zest_image_collection_handle image_collection_handle = zest__load_ktx(context, file_name);

    if (!image_collection_handle.context) {
        return ZEST__ZERO_INIT(zest_image_handle);
    }

	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource_checked(context, image_collection_handle.value);

	zest_bitmap_array_t *bitmap_array = &image_collection->bitmap_array;
    zest_vec_foreach(mip_index, bitmap_array->meta) {
        zest_buffer_image_copy_t buffer_copy_region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
        buffer_copy_region.image_aspect = zest_image_aspect_color_bit;
        buffer_copy_region.mip_level = mip_index;
        buffer_copy_region.base_array_layer = 0;
        buffer_copy_region.layer_count = 6;
        buffer_copy_region.image_extent.width = bitmap_array->meta[mip_index].width;
        buffer_copy_region.image_extent.height = bitmap_array->meta[mip_index].height;
        buffer_copy_region.image_extent.depth = 1;
        buffer_copy_region.buffer_offset = bitmap_array->meta[mip_index].offset;

        zest_vec_push(context->device->allocator, image_collection->buffer_copy_regions, buffer_copy_region);
    }
    zest_size image_size = bitmap_array->total_mem_size;

    zest_buffer staging_buffer = zest_CreateStagingBuffer(context, image_size, 0);

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
    zest_image_handle image_handle = zest_CreateImage(context, &create_info);
    zest_image image = (zest_image)zest__get_store_resource(image_handle.context, image_handle.value);

    context->device->platform->begin_single_time_commands(context);
    ZEST_CLEANUP_ON_FALSE(zest__transition_image_layout(image, zest_image_layout_transfer_dst_optimal, 0, mip_levels, 0, 6));
    ZEST_CLEANUP_ON_FALSE(context->device->platform->copy_buffer_regions_to_image(context, image_collection->buffer_copy_regions, staging_buffer, staging_buffer->buffer_offset, image));
    ZEST_CLEANUP_ON_FALSE(zest__transition_image_layout(image, zest_image_layout_shader_read_only_optimal, 0, mip_levels, 0, 6));
    context->device->platform->end_single_time_commands(context);

    zest_FreeBitmapArray(bitmap_array);
	zest_FreeBuffer(staging_buffer);
	zest__cleanup_image_collection(image_collection);

    return image_handle;

    cleanup:
    zest_FreeBitmapArray(bitmap_array);
	zest__cleanup_image(image);
	zest_FreeBuffer(staging_buffer);
	zest_FreeImageCollection(image_collection_handle);
    return ZEST__ZERO_INIT(zest_image_handle);
}
//End Zest_ktx_helper_implementation

#endif

#ifdef __cplusplus
}
#endif

#ifdef ZEST_MSDF_IMPLEMENTATION

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MSDF_IMPLEMENTATION
#include "msdf.h"

void* zest__msdf_allocation(size_t size, void* ctx) {
	zest_context context = (zest_context)ctx;
    return zest_AllocateMemory(context, size);
}

void zest__msdf_free(void* ptr, void* ctx) {
	zest_context context = (zest_context)ctx;
    zest_FreeMemory(context, ptr);
}

zest_font_resources_t zest_CreateFontResources(zest_context context) {
	//Create and compile the shaders for our custom sprite pipeline
	zest_shader_handle font_vert = zest_CreateShaderFromFile(context, "shaders/font.vert", "font_vert.spv", zest_vertex_shader, true);
	zest_shader_handle font_frag = zest_CreateShaderFromFile(context, "shaders/font.frag", "font_frag.spv", zest_fragment_shader, true);

	//Create a pipeline that we can use to draw billboards
	zest_pipeline_template font_pipeline = zest_BeginPipelineTemplate(context, "pipeline_billboard");
	zest_AddVertexInputBindingDescription(font_pipeline, 0, sizeof(zest_font_instance_t), zest_input_rate_instance);

    zest_AddVertexAttribute(font_pipeline, 0, 0, zest_format_r32g32_sfloat, offsetof(zest_font_instance_t, position));                  // Location 0: UV coords
    zest_AddVertexAttribute(font_pipeline, 0, 1, zest_format_r16g16b16a16_snorm, offsetof(zest_font_instance_t, uv));   // Location 1: Instance Position and rotation
    zest_AddVertexAttribute(font_pipeline, 0, 2, zest_format_r16g16_sscaled, offsetof(zest_font_instance_t, size));        // Location 2: Size of the sprite in pixels
    zest_AddVertexAttribute(font_pipeline, 0, 3, zest_format_r32_uint, offsetof(zest_font_instance_t, texture_array));        // Location 5: Instance Parameters

	zest_SetPipelinePushConstantRange(font_pipeline, sizeof(zest_msdf_font_settings_t), zest_shader_all_stages);
	zest_SetPipelineVertShader(font_pipeline, font_vert);
	zest_SetPipelineFragShader(font_pipeline, font_frag);
	zest_AddPipelineDescriptorLayout(font_pipeline, zest_GetGlobalBindlessLayout(context));
	zest_SetPipelineDepthTest(font_pipeline, true, false);

	zest_shader_resources_handle font_resources = zest_CreateShaderResources(context);
	zest_AddGlobalBindlessSetToResources(font_resources);

	zest_font_resources_t resources;
	resources.pipeline = font_pipeline;
	resources.shader_resources = font_resources;

	return resources;
}

zest_layer_handle zest_CreateFontLayer(zest_context context, const char *name) {
	ZEST_ASSERT(name, "Specify a name for the font layer");
	zest_layer_handle layer = zest_CreateInstanceLayer(context, name, sizeof(zest_font_instance_t));
	return layer;
}

zest_msdf_font_t zest_CreateMSDF(zest_context context, const char *filename, zest_uint font_sampler_binding_index, float font_size, float sdf_range) {
	zest_msdf_font_t font = ZEST__ZERO_INIT(zest_msdf_font_t);
	font.sdf_range = sdf_range;

    // --- MSDF Font Atlas Generation ---
    unsigned char* font_buffer = (unsigned char*)zest_ReadEntireFile(context, filename, ZEST_FALSE);
    if (!font_buffer) {
        printf("Error loading font file\n");
        return font;
    }

    stbtt_fontinfo font_info;
    if (!stbtt_InitFont(&font_info, font_buffer, 0)) {
        printf("Error initializing font\n");
		zest_FreeFile(context, (zest_file)font_buffer);
        return font;
    }

    // Atlas properties
    int atlas_channels = 4; 
	font.font_atlas = zest_CreateImageAtlasCollection(context, zest_format_r8g8b8a8_unorm);
	font.size = font_size;

    float scale = stbtt_ScaleForMappingEmToPixels(&font_info, font_size);

	msdf_allocation_context_t allocation_context;
	allocation_context.ctx = context;
	allocation_context.alloc = zest__msdf_allocation;
	allocation_context.free = zest__msdf_free;

	memset(font.characters, 0, 256);

	zest_uint total_area_size = 0;

    for (int char_code = 32; char_code < 127; ++char_code) {
        int glyph_index = stbtt_FindGlyphIndex(&font_info, char_code);
		zest_font_character_t* current_character = &font.characters[char_code];

        msdf_result_t result;
        if (msdf_genGlyph(&result, &font_info, glyph_index, 8, scale, sdf_range, &allocation_context)) {
			char character[2] = { (char)char_code, '\0' };
			int size = result.width * result.height * atlas_channels;
			zest_bitmap tmp_bitmap = zest_CreateBitmapFromRawBuffer(context, character, result.rgba, size, result.width, result.height, zest_format_r8g8b8a8_unorm);
			tmp_bitmap->is_imported = 0;
			zest_atlas_region region = zest_AddImageAtlasBitmap(font.font_atlas, tmp_bitmap, character);
			current_character->region = region;

            int advance, lsb;
            stbtt_GetGlyphHMetrics(&font_info, glyph_index, &advance, &lsb);
            current_character->x_advance = scale * advance;

			int x0, x1, y0, y1;
			stbtt_GetGlyphBox(&font_info, glyph_index, &x0, &y0, &x1, &y1);
			current_character->width = (float)result.width;
			current_character->height = (float)result.height;
            current_character->x_offset = (scale * x0) - sdf_range;
            current_character->y_offset = (scale * y1) + sdf_range;
			font.y_max_offset = ZEST__MAX(font.y_max_offset, current_character->y_offset);
			total_area_size += result.width * result.height;
		} else {
			//Treat as whitespace
            int advance, lsb;
            stbtt_GetGlyphHMetrics(&font_info, glyph_index, &advance, &lsb);
            current_character->x_advance = scale * advance;
			ZEST__FLAG(current_character->flags, zest_character_flag_whitespace);
		}
    }

    zest_FreeFile(context, (zest_file)font_buffer);
	zest_uint atlas_width = (zest_uint)zest_GetNextPower((zest_uint)sqrtf((float)total_area_size));
	zest_uint atlas_height = atlas_width;
	zest_SetImageCollectionPackedBorderSize(font.font_atlas, 2);
	if (!zest_GetBestFit(font.font_atlas, &atlas_width, &atlas_height)) {
		ZEST_PRINT("Unable to pack all fonts in to the image!");
		zest_FreeImageCollection(font.font_atlas);
	}
	zest_image_handle image_atlas = zest_CreateImageAtlas(font.font_atlas, atlas_width, atlas_height, zest_image_preset_texture);
	font.font_binding_index = zest_AcquireGlobalSampledImageIndex(image_atlas, zest_texture_array_binding);
	for (int i = 0; i != 95; ++i) {
		if (font.characters[i].region) {
			zest_BindAtlasRegionToImage(font.characters[i].region, font_sampler_binding_index, image_atlas, zest_texture_2d_binding);
		}
	}
	font.context = context;
	font.settings.transform = zest_Vec4Set(2.0f / zest_ScreenWidthf(context), 2.0f / zest_ScreenHeightf(context), -1.f, -1.f);
	font.settings.unit_range = ZEST_STRUCT_LITERAL(zest_vec2, font.sdf_range / 512.f, font.sdf_range / 512.f);
	font.settings.font_color = zest_Vec4Set(1.f, 1.f, 1.f, 1.f);
	font.settings.in_bias = 0.f;
	font.settings.out_bias = 0.f;
	font.settings.smoothness = 2.f;
	font.settings.gamma = 1.f;
	font.settings.shadow_color = zest_Vec4Set(0.f, 0.f, 0.f, 1.f);
	font.settings.shadow_offset = zest_Vec2Set(2.f, 2.f);
	return font;
}

void zest_SaveMSDF(zest_msdf_font_t *font, const char *filename) {
	zest_image_collection atlas = (zest_image_collection)zest__get_store_resource(font->context, font->font_atlas.value);
	zest_byte *atlas_bitmap = zest_GetImageCollectionRawBitmap(font->font_atlas, 0);
	zest_bitmap_meta_t atlas_meta = zest_ImageCollectionBitmapArrayMeta(font->font_atlas);
	stbi_write_png(filename, atlas_meta.width, atlas_meta.height, atlas_meta.channels, atlas_bitmap, atlas_meta.stride);
}

void zest_UpdateFontTransform(zest_msdf_font_t *font) {
	ZEST_ASSERT_HANDLE(font->context);	//Not a valid context in the font.
	font->settings.transform = zest_Vec4Set(2.0f / zest_ScreenWidthf(font->context), 2.0f / zest_ScreenHeightf(font->context), -1.f, -1.f);
}

void zest_SetFontTransform(zest_msdf_font_t *font, float transform[4]) {
	font->settings.transform.x = transform[0];
	font->settings.transform.y = transform[1];
	font->settings.transform.z = transform[2];
	font->settings.transform.w = transform[3];
}

void zest_SetFontSettings(zest_msdf_font_t *font, float inner_bias, float outer_bias, float smoothness, float gamma) {
	font->settings.in_bias = inner_bias;
	font->settings.out_bias = inner_bias;
	font->settings.smoothness = inner_bias;
	font->settings.gamma = inner_bias;
}

void zest_SetFontColor(zest_msdf_font_t *font, float r, float g, float b, float a) {
	font->settings.font_color = zest_Vec4Set(r, g, b, a);
}

void zest_SetFontShadowColor(zest_msdf_font_t *font, float r, float g, float b, float a) {
	font->settings.shadow_color = zest_Vec4Set(r, g, b, a);
}

void zest_SetFontShadowOffset(zest_msdf_font_t *font, float x, float y) {
	font->settings.shadow_offset = zest_Vec2Set(x, y);
}

void zest_FreeFont(zest_msdf_font_t *font) {
	if (font->characters) {
		zest_FreeImageCollection(font->font_atlas);
	}
}

float zest_TextWidth(zest_msdf_font_t *font, const char* text, float font_size, float letter_spacing) {
    float width = 0;
    float max_width = 0;

    size_t length = strlen(text);

    for (int i = 0; i != length; ++i) {
        zest_font_character_t* character = &font->characters[text[i]];

        if (character->flags == zest_character_flag_new_line) {
            width = 0;
        }

        width += character->x_advance * font_size + letter_spacing;
        max_width = ZEST__MAX(width, max_width);
    }

    return max_width;
}

void zest_SetMSDFFontDrawing(zest_layer_handle layer_handle, zest_msdf_font_t *font, zest_font_resources_t *font_resources) {
	zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    zest__end_instance_instructions(layer);
    zest__start_instance_instructions(layer);
    layer->current_instruction.pipeline_template = font_resources->pipeline;
	layer->current_instruction.shader_resources = font_resources->shader_resources;
    layer->current_instruction.draw_mode = zest_draw_mode_text;
    layer->current_instruction.asset = font;
    layer->current_instruction.scissor = layer->scissor;
    layer->current_instruction.viewport = layer->viewport;
	zest__set_layer_push_constants(layer, &font->settings, sizeof(zest_msdf_font_settings_t));
    layer->last_draw_mode = zest_draw_mode_text;
}

float zest_DrawMSDFText(zest_layer_handle layer_handle, const char* text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing) {
	zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_text);        //Call zest_StartFontDrawing before calling this function

    zest_msdf_font_t *font = (zest_msdf_font_t*)(layer->current_instruction.asset);

    size_t length = strlen(text);
    if (length <= 0) {
        return 0;
    }

    float scaled_line_height = font->size * size;
    float scaled_offset = font->y_max_offset * size;
    x -= zest_TextWidth(font, text, size, letter_spacing) * handle_x;
    y -= (scaled_line_height * handle_y) - scaled_offset;

    float xpos = x;

	zest_uint max_character_index = 255;
    for (zest_uint i = 0; i != length; ++i) {
		zest_uint character_index = (zest_uint)text[i];
		if (character_index >= (int)max_character_index) {
			continue;
		}
        zest_font_character_t* character = &font->characters[character_index];

        if (character->flags > 0) {
            xpos += character->x_advance * size + letter_spacing;
            continue;
        }

        float width = character->width * size;
        float height = character->height * size;
        float xoffset = character->x_offset * size;
        float yoffset = character->y_offset * size;

        zest_font_instance_t* font_instance = (zest_font_instance_t*)zest_NextInstance(layer);
		font_instance->size = zest_Pack16bit2SScaled(width, height, 4096.f);
        font_instance->position = zest_Vec2Set(xpos + xoffset, y - yoffset);
        font_instance->uv = character->region->uv_packed;
		font_instance->texture_array = character->region->layer_index;

        xpos += character->x_advance * size + letter_spacing;
    }

    return xpos;
}

void zest_UpdateFontUniformBuffer(zest_uniform_buffer_handle handle) {
	zest_context context = handle.context;
	ZEST_ASSERT_HANDLE(context);		//Not a valid context in the handle. Is it a valid buffer handle?
    zest_font_uniform_buffer_data_t* ubo_ptr = (zest_font_uniform_buffer_data_t*)zest_GetUniformBufferData(handle);
    zest_vec3 eye = ZEST_STRUCT_LITERAL(zest_vec3, 0.f, 0.f, -1.f );
    zest_vec3 center = ZEST__ZERO_INIT(zest_vec3);
    zest_vec3 up = ZEST_STRUCT_LITERAL( zest_vec3, 0.f, -1.f, 0.f );
    ubo_ptr->view = zest_LookAt(eye, center, up);
    ubo_ptr->proj = zest_Ortho(0.f, zest_ScreenWidthf(context) / context->dpi_scale, 0.f, -zest_ScreenHeightf(context) / context->dpi_scale, 0.f, 1000.f);
    ubo_ptr->screen_size.x = zest_ScreenWidthf(context);
    ubo_ptr->screen_size.y = zest_ScreenHeightf(context);
}
// End msdf_fonts

#endif 