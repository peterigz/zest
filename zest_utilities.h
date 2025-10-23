#pragma once

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
* To use define TINYKTX_IMPLEMENTATION before including zest_utilities.h in a c/c++ file. Only do this once.
	- [tiny_ktx]
	- [tiny_ktx_license]
	- [Zest_ktx_helper_functions]
	- [Zest_ktx_helper_implementation]
stb_truetype.h		Load tue type fonts		- v1.26 - public domain authored from 2009-2021 by Sean Barrett / RAD Game Tools
* To use define STB_TRUETYPE_IMPLEMENTATION before including zest_utilities.h in a c/c++ file. Only do this once.
	- [stb_truetype_fonts] 
msdf.h				Create msdf bitmaps for font rendering
* To use define MSDF_IMPLEMENTATION or ZEST_MSDF_IMPLEMENTATION before including zest_utilities.h in a c/c++ file. Only do this once.
	- [msdf_fonts]
*/

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

zest_window_data_t zest_implglfw_CreateWindow( int x, int y, int width, int height, zest_bool maximised, const char *title);
void zest_implglfw_GetWindowSizeCallback( void* native_window_handle, int* fb_width, int* fb_height, int* window_width, int* window_height );
void zest_implglfw_DestroyWindow(zest_context context);

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

zest_window_data_t zest_implsdl2_CreateWindow(int x, int y, int width, int height, zest_bool maximised, const char* title);
void zest_implsdl2_GetWindowSizeCallback(void *window_handle, int *fb_width, int *fb_height, int *window_width, int *window_height);
void zest_implsdl2_DestroyWindow(zest_context context);

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



/*
[tiny_ktx]
*/
// MIT license see full LICENSE text at end of file
#ifndef TINY_KTX_TINYKTX_H
#define TINY_KTX_TINYKTX_H

#ifndef TINYKTX_HAVE_UINTXX_T
#include <stdint.h> 	// for uint32_t and int64_t
#endif
#ifndef TINYKTX_HAVE_BOOL
#include <stdbool.h>	// for bool
#endif
#ifndef TINYKTX_HAVE_SIZE_T
#include <stddef.h>		// for size_t
#endif
#ifndef TINYKTX_HAVE_MEMCPY
#include <string.h> 	// for memcpy
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define TINYKTX_MAX_MIPMAPLEVELS 16

	typedef struct TinyKtx_Context *TinyKtx_ContextHandle;

	typedef void *(*TinyKtx_AllocFunc)(void *user, size_t size);
	typedef void (*TinyKtx_FreeFunc)(void *user, void *memory);
	typedef size_t(*TinyKtx_ReadFunc)(void *user, void *buffer, size_t byteCount);
	typedef bool (*TinyKtx_SeekFunc)(void *user, int64_t offset);
	typedef int64_t(*TinyKtx_TellFunc)(void *user);
	typedef void (*TinyKtx_ErrorFunc)(void *user, char const *msg);

	typedef struct TinyKtx_Callbacks {
		TinyKtx_ErrorFunc errorFn;
		TinyKtx_AllocFunc allocFn;
		TinyKtx_FreeFunc freeFn;
		TinyKtx_ReadFunc readFn;
		TinyKtx_SeekFunc seekFn;
		TinyKtx_TellFunc tellFn;
	} TinyKtx_Callbacks;

	TinyKtx_ContextHandle TinyKtx_CreateContext(TinyKtx_Callbacks const *callbacks, void *user);
	void TinyKtx_DestroyContext(TinyKtx_ContextHandle handle);

	// reset lets you reuse the context for another file (saves an alloc/free cycle)
	void TinyKtx_Reset(TinyKtx_ContextHandle handle);

	// call this to read the header file should already be at the start of the KTX data
	bool TinyKtx_ReadHeader(TinyKtx_ContextHandle handle);

	// this is slow linear search. TODO add iterator style reading of key value pairs
	bool TinyKtx_GetValue(TinyKtx_ContextHandle handle, char const *key, void const **value);

	bool TinyKtx_Is1D(TinyKtx_ContextHandle handle);
	bool TinyKtx_Is2D(TinyKtx_ContextHandle handle);
	bool TinyKtx_Is3D(TinyKtx_ContextHandle handle);
	bool TinyKtx_IsCubemap(TinyKtx_ContextHandle handle);
	bool TinyKtx_IsArray(TinyKtx_ContextHandle handle);

	bool TinyKtx_Dimensions(TinyKtx_ContextHandle handle, uint32_t *width, uint32_t *height, uint32_t *depth, uint32_t *slices);
	uint32_t TinyKtx_Width(TinyKtx_ContextHandle handle);
	uint32_t TinyKtx_Height(TinyKtx_ContextHandle handle);
	uint32_t TinyKtx_Depth(TinyKtx_ContextHandle handle);
	uint32_t TinyKtx_ArraySlices(TinyKtx_ContextHandle handle);

	bool TinyKtx_GetFormatGL(TinyKtx_ContextHandle handle, uint32_t *glformat, uint32_t *gltype, uint32_t *glinternalformat, uint32_t *typesize, uint32_t *glbaseinternalformat);

	bool TinyKtx_NeedsGenerationOfMipmaps(TinyKtx_ContextHandle handle);
	bool TinyKtx_NeedsEndianCorrecting(TinyKtx_ContextHandle handle);

	uint32_t TinyKtx_NumberOfMipmaps(TinyKtx_ContextHandle handle);
	uint32_t TinyKtx_ImageSize(TinyKtx_ContextHandle handle, uint32_t mipmaplevel);

	bool TinyKtx_IsMipMapLevelUnpacked(TinyKtx_ContextHandle handle, uint32_t mipmaplevel);
	// this is required to read Unpacked data correctly
	uint32_t TinyKtx_UnpackedRowStride(TinyKtx_ContextHandle handle, uint32_t mipmaplevel);

	// data return by ImageRawData is owned by the context. Don't free it!
	void const *TinyKtx_ImageRawData(TinyKtx_ContextHandle handle, uint32_t mipmaplevel);

	typedef void (*TinyKtx_WriteFunc)(void *user, void const *buffer, size_t byteCount);

	typedef struct TinyKtx_WriteCallbacks {
		TinyKtx_ErrorFunc errorFn;
		TinyKtx_AllocFunc allocFn;
		TinyKtx_FreeFunc freeFn;
		TinyKtx_WriteFunc writeFn;
	} TinyKtx_WriteCallbacks;


	bool TinyKtx_WriteImageGL(TinyKtx_WriteCallbacks const *callbacks,
		void *user,
		uint32_t width,
		uint32_t height,
		uint32_t depth,
		uint32_t slices,
		uint32_t mipmaplevels,
		uint32_t format,
		uint32_t internalFormat,
		uint32_t baseFormat,
		uint32_t type,
		uint32_t typeSize,
		bool cubemap,
		uint32_t const *mipmapsizes,
		void const **mipmaps);

	// ktx v1 is based on GL (slightly confusing imho) texture format system
	// there is format, internal format, type etc.

	// we try and expose a more dx12/vulkan/metal style of format
	// but obviously still need to GL data so bare with me.
	// a TinyKTX_Format is the equivilent to GL/KTX Format and Type
	// the API doesn't expose the actual values (which come from GL itself)
	// but provide an API call to crack them back into the actual GL values).

	// Ktx v2 is based on VkFormat and also DFD, so we now base the
	// enumeration values of TinyKtx_Format on the Vkformat values where possible

#ifndef TINYIMAGEFORMAT_VKFORMAT
#define TINYIMAGEFORMAT_VKFORMAT
	typedef enum TinyImageFormat_VkFormat {
		TIF_VK_FORMAT_UNDEFINED = 0,
		TIF_VK_FORMAT_R4G4_UNORM_PACK8 = 1,
		TIF_VK_FORMAT_R4G4B4A4_UNORM_PACK16 = 2,
		TIF_VK_FORMAT_B4G4R4A4_UNORM_PACK16 = 3,
		TIF_VK_FORMAT_R5G6B5_UNORM_PACK16 = 4,
		TIF_VK_FORMAT_B5G6R5_UNORM_PACK16 = 5,
		TIF_VK_FORMAT_R5G5B5A1_UNORM_PACK16 = 6,
		TIF_VK_FORMAT_B5G5R5A1_UNORM_PACK16 = 7,
		TIF_VK_FORMAT_A1R5G5B5_UNORM_PACK16 = 8,
		TIF_VK_FORMAT_R8_UNORM = 9,
		TIF_VK_FORMAT_R8_SNORM = 10,
		TIF_VK_FORMAT_R8_USCALED = 11,
		TIF_VK_FORMAT_R8_SSCALED = 12,
		TIF_VK_FORMAT_R8_UINT = 13,
		TIF_VK_FORMAT_R8_SINT = 14,
		TIF_VK_FORMAT_R8_SRGB = 15,
		TIF_VK_FORMAT_R8G8_UNORM = 16,
		TIF_VK_FORMAT_R8G8_SNORM = 17,
		TIF_VK_FORMAT_R8G8_USCALED = 18,
		TIF_VK_FORMAT_R8G8_SSCALED = 19,
		TIF_VK_FORMAT_R8G8_UINT = 20,
		TIF_VK_FORMAT_R8G8_SINT = 21,
		TIF_VK_FORMAT_R8G8_SRGB = 22,
		TIF_VK_FORMAT_R8G8B8_UNORM = 23,
		TIF_VK_FORMAT_R8G8B8_SNORM = 24,
		TIF_VK_FORMAT_R8G8B8_USCALED = 25,
		TIF_VK_FORMAT_R8G8B8_SSCALED = 26,
		TIF_VK_FORMAT_R8G8B8_UINT = 27,
		TIF_VK_FORMAT_R8G8B8_SINT = 28,
		TIF_VK_FORMAT_R8G8B8_SRGB = 29,
		TIF_VK_FORMAT_B8G8R8_UNORM = 30,
		TIF_VK_FORMAT_B8G8R8_SNORM = 31,
		TIF_VK_FORMAT_B8G8R8_USCALED = 32,
		TIF_VK_FORMAT_B8G8R8_SSCALED = 33,
		TIF_VK_FORMAT_B8G8R8_UINT = 34,
		TIF_VK_FORMAT_B8G8R8_SINT = 35,
		TIF_VK_FORMAT_B8G8R8_SRGB = 36,
		TIF_VK_FORMAT_R8G8B8A8_UNORM = 37,
		TIF_VK_FORMAT_R8G8B8A8_SNORM = 38,
		TIF_VK_FORMAT_R8G8B8A8_USCALED = 39,
		TIF_VK_FORMAT_R8G8B8A8_SSCALED = 40,
		TIF_VK_FORMAT_R8G8B8A8_UINT = 41,
		TIF_VK_FORMAT_R8G8B8A8_SINT = 42,
		TIF_VK_FORMAT_R8G8B8A8_SRGB = 43,
		TIF_VK_FORMAT_B8G8R8A8_UNORM = 44,
		TIF_VK_FORMAT_B8G8R8A8_SNORM = 45,
		TIF_VK_FORMAT_B8G8R8A8_USCALED = 46,
		TIF_VK_FORMAT_B8G8R8A8_SSCALED = 47,
		TIF_VK_FORMAT_B8G8R8A8_UINT = 48,
		TIF_VK_FORMAT_B8G8R8A8_SINT = 49,
		TIF_VK_FORMAT_B8G8R8A8_SRGB = 50,
		TIF_VK_FORMAT_A8B8G8R8_UNORM_PACK32 = 51,
		TIF_VK_FORMAT_A8B8G8R8_SNORM_PACK32 = 52,
		TIF_VK_FORMAT_A8B8G8R8_USCALED_PACK32 = 53,
		TIF_VK_FORMAT_A8B8G8R8_SSCALED_PACK32 = 54,
		TIF_VK_FORMAT_A8B8G8R8_UINT_PACK32 = 55,
		TIF_VK_FORMAT_A8B8G8R8_SINT_PACK32 = 56,
		TIF_VK_FORMAT_A8B8G8R8_SRGB_PACK32 = 57,
		TIF_VK_FORMAT_A2R10G10B10_UNORM_PACK32 = 58,
		TIF_VK_FORMAT_A2R10G10B10_SNORM_PACK32 = 59,
		TIF_VK_FORMAT_A2R10G10B10_USCALED_PACK32 = 60,
		TIF_VK_FORMAT_A2R10G10B10_SSCALED_PACK32 = 61,
		TIF_VK_FORMAT_A2R10G10B10_UINT_PACK32 = 62,
		TIF_VK_FORMAT_A2R10G10B10_SINT_PACK32 = 63,
		TIF_VK_FORMAT_A2B10G10R10_UNORM_PACK32 = 64,
		TIF_VK_FORMAT_A2B10G10R10_SNORM_PACK32 = 65,
		TIF_VK_FORMAT_A2B10G10R10_USCALED_PACK32 = 66,
		TIF_VK_FORMAT_A2B10G10R10_SSCALED_PACK32 = 67,
		TIF_VK_FORMAT_A2B10G10R10_UINT_PACK32 = 68,
		TIF_VK_FORMAT_A2B10G10R10_SINT_PACK32 = 69,
		TIF_VK_FORMAT_R16_UNORM = 70,
		TIF_VK_FORMAT_R16_SNORM = 71,
		TIF_VK_FORMAT_R16_USCALED = 72,
		TIF_VK_FORMAT_R16_SSCALED = 73,
		TIF_VK_FORMAT_R16_UINT = 74,
		TIF_VK_FORMAT_R16_SINT = 75,
		TIF_VK_FORMAT_R16_SFLOAT = 76,
		TIF_VK_FORMAT_R16G16_UNORM = 77,
		TIF_VK_FORMAT_R16G16_SNORM = 78,
		TIF_VK_FORMAT_R16G16_USCALED = 79,
		TIF_VK_FORMAT_R16G16_SSCALED = 80,
		TIF_VK_FORMAT_R16G16_UINT = 81,
		TIF_VK_FORMAT_R16G16_SINT = 82,
		TIF_VK_FORMAT_R16G16_SFLOAT = 83,
		TIF_VK_FORMAT_R16G16B16_UNORM = 84,
		TIF_VK_FORMAT_R16G16B16_SNORM = 85,
		TIF_VK_FORMAT_R16G16B16_USCALED = 86,
		TIF_VK_FORMAT_R16G16B16_SSCALED = 87,
		TIF_VK_FORMAT_R16G16B16_UINT = 88,
		TIF_VK_FORMAT_R16G16B16_SINT = 89,
		TIF_VK_FORMAT_R16G16B16_SFLOAT = 90,
		TIF_VK_FORMAT_R16G16B16A16_UNORM = 91,
		TIF_VK_FORMAT_R16G16B16A16_SNORM = 92,
		TIF_VK_FORMAT_R16G16B16A16_USCALED = 93,
		TIF_VK_FORMAT_R16G16B16A16_SSCALED = 94,
		TIF_VK_FORMAT_R16G16B16A16_UINT = 95,
		TIF_VK_FORMAT_R16G16B16A16_SINT = 96,
		TIF_VK_FORMAT_R16G16B16A16_SFLOAT = 97,
		TIF_VK_FORMAT_R32_UINT = 98,
		TIF_VK_FORMAT_R32_SINT = 99,
		TIF_VK_FORMAT_R32_SFLOAT = 100,
		TIF_VK_FORMAT_R32G32_UINT = 101,
		TIF_VK_FORMAT_R32G32_SINT = 102,
		TIF_VK_FORMAT_R32G32_SFLOAT = 103,
		TIF_VK_FORMAT_R32G32B32_UINT = 104,
		TIF_VK_FORMAT_R32G32B32_SINT = 105,
		TIF_VK_FORMAT_R32G32B32_SFLOAT = 106,
		TIF_VK_FORMAT_R32G32B32A32_UINT = 107,
		TIF_VK_FORMAT_R32G32B32A32_SINT = 108,
		TIF_VK_FORMAT_R32G32B32A32_SFLOAT = 109,
		TIF_VK_FORMAT_R64_UINT = 110,
		TIF_VK_FORMAT_R64_SINT = 111,
		TIF_VK_FORMAT_R64_SFLOAT = 112,
		TIF_VK_FORMAT_R64G64_UINT = 113,
		TIF_VK_FORMAT_R64G64_SINT = 114,
		TIF_VK_FORMAT_R64G64_SFLOAT = 115,
		TIF_VK_FORMAT_R64G64B64_UINT = 116,
		TIF_VK_FORMAT_R64G64B64_SINT = 117,
		TIF_VK_FORMAT_R64G64B64_SFLOAT = 118,
		TIF_VK_FORMAT_R64G64B64A64_UINT = 119,
		TIF_VK_FORMAT_R64G64B64A64_SINT = 120,
		TIF_VK_FORMAT_R64G64B64A64_SFLOAT = 121,
		TIF_VK_FORMAT_B10G11R11_UFLOAT_PACK32 = 122,
		TIF_VK_FORMAT_E5B9G9R9_UFLOAT_PACK32 = 123,
		TIF_VK_FORMAT_D16_UNORM = 124,
		TIF_VK_FORMAT_X8_D24_UNORM_PACK32 = 125,
		TIF_VK_FORMAT_D32_SFLOAT = 126,
		TIF_VK_FORMAT_S8_UINT = 127,
		TIF_VK_FORMAT_D16_UNORM_S8_UINT = 128,
		TIF_VK_FORMAT_D24_UNORM_S8_UINT = 129,
		TIF_VK_FORMAT_D32_SFLOAT_S8_UINT = 130,
		TIF_VK_FORMAT_BC1_RGB_UNORM_BLOCK = 131,
		TIF_VK_FORMAT_BC1_RGB_SRGB_BLOCK = 132,
		TIF_VK_FORMAT_BC1_RGBA_UNORM_BLOCK = 133,
		TIF_VK_FORMAT_BC1_RGBA_SRGB_BLOCK = 134,
		TIF_VK_FORMAT_BC2_UNORM_BLOCK = 135,
		TIF_VK_FORMAT_BC2_SRGB_BLOCK = 136,
		TIF_VK_FORMAT_BC3_UNORM_BLOCK = 137,
		TIF_VK_FORMAT_BC3_SRGB_BLOCK = 138,
		TIF_VK_FORMAT_BC4_UNORM_BLOCK = 139,
		TIF_VK_FORMAT_BC4_SNORM_BLOCK = 140,
		TIF_VK_FORMAT_BC5_UNORM_BLOCK = 141,
		TIF_VK_FORMAT_BC5_SNORM_BLOCK = 142,
		TIF_VK_FORMAT_BC6H_UFLOAT_BLOCK = 143,
		TIF_VK_FORMAT_BC6H_SFLOAT_BLOCK = 144,
		TIF_VK_FORMAT_BC7_UNORM_BLOCK = 145,
		TIF_VK_FORMAT_BC7_SRGB_BLOCK = 146,
		TIF_VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK = 147,
		TIF_VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK = 148,
		TIF_VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK = 149,
		TIF_VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK = 150,
		TIF_VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK = 151,
		TIF_VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK = 152,
		TIF_VK_FORMAT_EAC_R11_UNORM_BLOCK = 153,
		TIF_VK_FORMAT_EAC_R11_SNORM_BLOCK = 154,
		TIF_VK_FORMAT_EAC_R11G11_UNORM_BLOCK = 155,
		TIF_VK_FORMAT_EAC_R11G11_SNORM_BLOCK = 156,
		TIF_VK_FORMAT_ASTC_4x4_UNORM_BLOCK = 157,
		TIF_VK_FORMAT_ASTC_4x4_SRGB_BLOCK = 158,
		TIF_VK_FORMAT_ASTC_5x4_UNORM_BLOCK = 159,
		TIF_VK_FORMAT_ASTC_5x4_SRGB_BLOCK = 160,
		TIF_VK_FORMAT_ASTC_5x5_UNORM_BLOCK = 161,
		TIF_VK_FORMAT_ASTC_5x5_SRGB_BLOCK = 162,
		TIF_VK_FORMAT_ASTC_6x5_UNORM_BLOCK = 163,
		TIF_VK_FORMAT_ASTC_6x5_SRGB_BLOCK = 164,
		TIF_VK_FORMAT_ASTC_6x6_UNORM_BLOCK = 165,
		TIF_VK_FORMAT_ASTC_6x6_SRGB_BLOCK = 166,
		TIF_VK_FORMAT_ASTC_8x5_UNORM_BLOCK = 167,
		TIF_VK_FORMAT_ASTC_8x5_SRGB_BLOCK = 168,
		TIF_VK_FORMAT_ASTC_8x6_UNORM_BLOCK = 169,
		TIF_VK_FORMAT_ASTC_8x6_SRGB_BLOCK = 170,
		TIF_VK_FORMAT_ASTC_8x8_UNORM_BLOCK = 171,
		TIF_VK_FORMAT_ASTC_8x8_SRGB_BLOCK = 172,
		TIF_VK_FORMAT_ASTC_10x5_UNORM_BLOCK = 173,
		TIF_VK_FORMAT_ASTC_10x5_SRGB_BLOCK = 174,
		TIF_VK_FORMAT_ASTC_10x6_UNORM_BLOCK = 175,
		TIF_VK_FORMAT_ASTC_10x6_SRGB_BLOCK = 176,
		TIF_VK_FORMAT_ASTC_10x8_UNORM_BLOCK = 177,
		TIF_VK_FORMAT_ASTC_10x8_SRGB_BLOCK = 178,
		TIF_VK_FORMAT_ASTC_10x10_UNORM_BLOCK = 179,
		TIF_VK_FORMAT_ASTC_10x10_SRGB_BLOCK = 180,
		TIF_VK_FORMAT_ASTC_12x10_UNORM_BLOCK = 181,
		TIF_VK_FORMAT_ASTC_12x10_SRGB_BLOCK = 182,
		TIF_VK_FORMAT_ASTC_12x12_UNORM_BLOCK = 183,
		TIF_VK_FORMAT_ASTC_12x12_SRGB_BLOCK = 184,

		TIF_VK_FORMAT_G8B8G8R8_422_UNORM = 1000156000,
		TIF_VK_FORMAT_B8G8R8G8_422_UNORM = 1000156001,
		TIF_VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM = 1000156002,
		TIF_VK_FORMAT_G8_B8R8_2PLANE_420_UNORM = 1000156003,
		TIF_VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM = 1000156004,
		TIF_VK_FORMAT_G8_B8R8_2PLANE_422_UNORM = 1000156005,
		TIF_VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM = 1000156006,
		TIF_VK_FORMAT_R10X6_UNORM_PACK16 = 1000156007,
		TIF_VK_FORMAT_R10X6G10X6_UNORM_2PACK16 = 1000156008,
		TIF_VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16 = 1000156009,
		TIF_VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 = 1000156010,
		TIF_VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 = 1000156011,
		TIF_VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 = 1000156012,
		TIF_VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 = 1000156013,
		TIF_VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 = 1000156014,
		TIF_VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 = 1000156015,
		TIF_VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 = 1000156016,
		TIF_VK_FORMAT_R12X4_UNORM_PACK16 = 1000156017,
		TIF_VK_FORMAT_R12X4G12X4_UNORM_2PACK16 = 1000156018,
		TIF_VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16 = 1000156019,
		TIF_VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 = 1000156020,
		TIF_VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 = 1000156021,
		TIF_VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 = 1000156022,
		TIF_VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 = 1000156023,
		TIF_VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 = 1000156024,
		TIF_VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 = 1000156025,
		TIF_VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 = 1000156026,
		TIF_VK_FORMAT_G16B16G16R16_422_UNORM = 1000156027,
		TIF_VK_FORMAT_B16G16R16G16_422_UNORM = 1000156028,
		TIF_VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM = 1000156029,
		TIF_VK_FORMAT_G16_B16R16_2PLANE_420_UNORM = 1000156030,
		TIF_VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM = 1000156031,
		TIF_VK_FORMAT_G16_B16R16_2PLANE_422_UNORM = 1000156032,
		TIF_VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM = 1000156033,
		TIF_VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG = 1000054000,
		TIF_VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG = 1000054001,
		TIF_VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG = 1000054002,
		TIF_VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG = 1000054003,
		TIF_VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG = 1000054004,
		TIF_VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG = 1000054005,
		TIF_VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG = 1000054006,
		TIF_VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG = 1000054007,
	} TinyImageFormat_VkFormat;
#endif // TINYIMAGEFORMAT_VKFORMAT

#define TINYKTX_MEV(x) TKTX_##x = TIF_VK_FORMAT_##x
	typedef enum TinyKtx_Format {
		TINYKTX_MEV(UNDEFINED),
		TINYKTX_MEV(R4G4_UNORM_PACK8),
		TINYKTX_MEV(R4G4B4A4_UNORM_PACK16),
		TINYKTX_MEV(B4G4R4A4_UNORM_PACK16),
		TINYKTX_MEV(R5G6B5_UNORM_PACK16),
		TINYKTX_MEV(B5G6R5_UNORM_PACK16),
		TINYKTX_MEV(R5G5B5A1_UNORM_PACK16),
		TINYKTX_MEV(B5G5R5A1_UNORM_PACK16),
		TINYKTX_MEV(A1R5G5B5_UNORM_PACK16),

		TINYKTX_MEV(R8_UNORM),
		TINYKTX_MEV(R8_SNORM),
		TINYKTX_MEV(R8_UINT),
		TINYKTX_MEV(R8_SINT),
		TINYKTX_MEV(R8_SRGB),

		TINYKTX_MEV(R8G8_UNORM),
		TINYKTX_MEV(R8G8_SNORM),
		TINYKTX_MEV(R8G8_UINT),
		TINYKTX_MEV(R8G8_SINT),
		TINYKTX_MEV(R8G8_SRGB),

		TINYKTX_MEV(R8G8B8_UNORM),
		TINYKTX_MEV(R8G8B8_SNORM),
		TINYKTX_MEV(R8G8B8_UINT),
		TINYKTX_MEV(R8G8B8_SINT),
		TINYKTX_MEV(R8G8B8_SRGB),
		TINYKTX_MEV(B8G8R8_UNORM),
		TINYKTX_MEV(B8G8R8_SNORM),
		TINYKTX_MEV(B8G8R8_UINT),
		TINYKTX_MEV(B8G8R8_SINT),
		TINYKTX_MEV(B8G8R8_SRGB),

		TINYKTX_MEV(R8G8B8A8_UNORM),
		TINYKTX_MEV(R8G8B8A8_SNORM),
		TINYKTX_MEV(R8G8B8A8_UINT),
		TINYKTX_MEV(R8G8B8A8_SINT),
		TINYKTX_MEV(R8G8B8A8_SRGB),
		TINYKTX_MEV(B8G8R8A8_UNORM),
		TINYKTX_MEV(B8G8R8A8_SNORM),
		TINYKTX_MEV(B8G8R8A8_UINT),
		TINYKTX_MEV(B8G8R8A8_SINT),
		TINYKTX_MEV(B8G8R8A8_SRGB),

		TINYKTX_MEV(A8B8G8R8_UNORM_PACK32),
		TINYKTX_MEV(A8B8G8R8_SNORM_PACK32),
		TINYKTX_MEV(A8B8G8R8_UINT_PACK32),
		TINYKTX_MEV(A8B8G8R8_SINT_PACK32),
		TINYKTX_MEV(A8B8G8R8_SRGB_PACK32),

		TINYKTX_MEV(E5B9G9R9_UFLOAT_PACK32),
		TINYKTX_MEV(A2R10G10B10_UNORM_PACK32),
		TINYKTX_MEV(A2R10G10B10_UINT_PACK32),
		TINYKTX_MEV(A2B10G10R10_UNORM_PACK32),
		TINYKTX_MEV(A2B10G10R10_UINT_PACK32),
		TINYKTX_MEV(B10G11R11_UFLOAT_PACK32),

		TINYKTX_MEV(R16_UNORM),
		TINYKTX_MEV(R16_SNORM),
		TINYKTX_MEV(R16_UINT),
		TINYKTX_MEV(R16_SINT),
		TINYKTX_MEV(R16_SFLOAT),
		TINYKTX_MEV(R16G16_UNORM),
		TINYKTX_MEV(R16G16_SNORM),
		TINYKTX_MEV(R16G16_UINT),
		TINYKTX_MEV(R16G16_SINT),
		TINYKTX_MEV(R16G16_SFLOAT),
		TINYKTX_MEV(R16G16B16_UNORM),
		TINYKTX_MEV(R16G16B16_SNORM),
		TINYKTX_MEV(R16G16B16_UINT),
		TINYKTX_MEV(R16G16B16_SINT),
		TINYKTX_MEV(R16G16B16_SFLOAT),
		TINYKTX_MEV(R16G16B16A16_UNORM),
		TINYKTX_MEV(R16G16B16A16_SNORM),
		TINYKTX_MEV(R16G16B16A16_UINT),
		TINYKTX_MEV(R16G16B16A16_SINT),
		TINYKTX_MEV(R16G16B16A16_SFLOAT),
		TINYKTX_MEV(R32_UINT),
		TINYKTX_MEV(R32_SINT),
		TINYKTX_MEV(R32_SFLOAT),
		TINYKTX_MEV(R32G32_UINT),
		TINYKTX_MEV(R32G32_SINT),
		TINYKTX_MEV(R32G32_SFLOAT),
		TINYKTX_MEV(R32G32B32_UINT),
		TINYKTX_MEV(R32G32B32_SINT),
		TINYKTX_MEV(R32G32B32_SFLOAT),
		TINYKTX_MEV(R32G32B32A32_UINT),
		TINYKTX_MEV(R32G32B32A32_SINT),
		TINYKTX_MEV(R32G32B32A32_SFLOAT),

		TINYKTX_MEV(BC1_RGB_UNORM_BLOCK),
		TINYKTX_MEV(BC1_RGB_SRGB_BLOCK),
		TINYKTX_MEV(BC1_RGBA_UNORM_BLOCK),
		TINYKTX_MEV(BC1_RGBA_SRGB_BLOCK),
		TINYKTX_MEV(BC2_UNORM_BLOCK),
		TINYKTX_MEV(BC2_SRGB_BLOCK),
		TINYKTX_MEV(BC3_UNORM_BLOCK),
		TINYKTX_MEV(BC3_SRGB_BLOCK),
		TINYKTX_MEV(BC4_UNORM_BLOCK),
		TINYKTX_MEV(BC4_SNORM_BLOCK),
		TINYKTX_MEV(BC5_UNORM_BLOCK),
		TINYKTX_MEV(BC5_SNORM_BLOCK),
		TINYKTX_MEV(BC6H_UFLOAT_BLOCK),
		TINYKTX_MEV(BC6H_SFLOAT_BLOCK),
		TINYKTX_MEV(BC7_UNORM_BLOCK),
		TINYKTX_MEV(BC7_SRGB_BLOCK),

		TINYKTX_MEV(ETC2_R8G8B8_UNORM_BLOCK),
		TINYKTX_MEV(ETC2_R8G8B8A1_UNORM_BLOCK),
		TINYKTX_MEV(ETC2_R8G8B8A8_UNORM_BLOCK),
		TINYKTX_MEV(ETC2_R8G8B8_SRGB_BLOCK),
		TINYKTX_MEV(ETC2_R8G8B8A1_SRGB_BLOCK),
		TINYKTX_MEV(ETC2_R8G8B8A8_SRGB_BLOCK),
		TINYKTX_MEV(EAC_R11_UNORM_BLOCK),
		TINYKTX_MEV(EAC_R11G11_UNORM_BLOCK),
		TINYKTX_MEV(EAC_R11_SNORM_BLOCK),
		TINYKTX_MEV(EAC_R11G11_SNORM_BLOCK),

		TKTX_PVR_2BPP_UNORM_BLOCK = TIF_VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG,
		TKTX_PVR_2BPPA_UNORM_BLOCK = TIF_VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG,
		TKTX_PVR_4BPP_UNORM_BLOCK = TIF_VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG,
		TKTX_PVR_4BPPA_UNORM_BLOCK = TIF_VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG,
		TKTX_PVR_2BPP_SRGB_BLOCK = TIF_VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG,
		TKTX_PVR_2BPPA_SRGB_BLOCK = TIF_VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG,
		TKTX_PVR_4BPP_SRGB_BLOCK = TIF_VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG,
		TKTX_PVR_4BPPA_SRGB_BLOCK = TIF_VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG,

		TINYKTX_MEV(ASTC_4x4_UNORM_BLOCK),
		TINYKTX_MEV(ASTC_4x4_SRGB_BLOCK),
		TINYKTX_MEV(ASTC_5x4_UNORM_BLOCK),
		TINYKTX_MEV(ASTC_5x4_SRGB_BLOCK),
		TINYKTX_MEV(ASTC_5x5_UNORM_BLOCK),
		TINYKTX_MEV(ASTC_5x5_SRGB_BLOCK),
		TINYKTX_MEV(ASTC_6x5_UNORM_BLOCK),
		TINYKTX_MEV(ASTC_6x5_SRGB_BLOCK),
		TINYKTX_MEV(ASTC_6x6_UNORM_BLOCK),
		TINYKTX_MEV(ASTC_6x6_SRGB_BLOCK),
		TINYKTX_MEV(ASTC_8x5_UNORM_BLOCK),
		TINYKTX_MEV(ASTC_8x5_SRGB_BLOCK),
		TINYKTX_MEV(ASTC_8x6_UNORM_BLOCK),
		TINYKTX_MEV(ASTC_8x6_SRGB_BLOCK),
		TINYKTX_MEV(ASTC_8x8_UNORM_BLOCK),
		TINYKTX_MEV(ASTC_8x8_SRGB_BLOCK),
		TINYKTX_MEV(ASTC_10x5_UNORM_BLOCK),
		TINYKTX_MEV(ASTC_10x5_SRGB_BLOCK),
		TINYKTX_MEV(ASTC_10x6_UNORM_BLOCK),
		TINYKTX_MEV(ASTC_10x6_SRGB_BLOCK),
		TINYKTX_MEV(ASTC_10x8_UNORM_BLOCK),
		TINYKTX_MEV(ASTC_10x8_SRGB_BLOCK),
		TINYKTX_MEV(ASTC_10x10_UNORM_BLOCK),
		TINYKTX_MEV(ASTC_10x10_SRGB_BLOCK),
		TINYKTX_MEV(ASTC_12x10_UNORM_BLOCK),
		TINYKTX_MEV(ASTC_12x10_SRGB_BLOCK),
		TINYKTX_MEV(ASTC_12x12_UNORM_BLOCK),
		TINYKTX_MEV(ASTC_12x12_SRGB_BLOCK),

	} TinyKtx_Format;
#undef TINYKTX_MEV

	// tiny_imageformat/format needs included before tinyktx.h for this functionality
#ifdef TINYIMAGEFORMAT_BASE_H_
	TinyImageFormat TinyImageFormat_FromTinyKtxFormat(TinyKtx_Format format);
	TinyKtx_Format TinyImageFormat_ToTinyKtxFormat(TinyImageFormat format);
#endif

	TinyKtx_Format TinyKtx_GetFormat(TinyKtx_ContextHandle handle);
	bool TinyKtx_CrackFormatToGL(TinyKtx_Format format, uint32_t *glformat, uint32_t *gltype, uint32_t *glinternalformat, uint32_t *typesize);
	bool TinyKtx_WriteImage(TinyKtx_WriteCallbacks const *callbacks,
		void *user,
		uint32_t width,
		uint32_t height,
		uint32_t depth,
		uint32_t slices,
		uint32_t mipmaplevels,
		TinyKtx_Format format,
		bool cubemap,
		uint32_t const *mipmapsizes,
		void const **mipmaps);
	// GL types
#define TINYKTX_GL_TYPE_COMPRESSED                      0x0
#define TINYKTX_GL_TYPE_BYTE                            0x1400
#define TINYKTX_GL_TYPE_UNSIGNED_BYTE                    0x1401
#define TINYKTX_GL_TYPE_SHORT                            0x1402
#define TINYKTX_GL_TYPE_UNSIGNED_SHORT                  0x1403
#define TINYKTX_GL_TYPE_INT                              0x1404
#define TINYKTX_GL_TYPE_UNSIGNED_INT                    0x1405
#define TINYKTX_GL_TYPE_FLOAT                            0x1406
#define TINYKTX_GL_TYPE_DOUBLE                          0x140A
#define TINYKTX_GL_TYPE_HALF_FLOAT                      0x140B
#define TINYKTX_GL_TYPE_UNSIGNED_BYTE_3_3_2              0x8032
#define TINYKTX_GL_TYPE_UNSIGNED_SHORT_4_4_4_4          0x8033
#define TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_5_5_1          0x8034
#define TINYKTX_GL_TYPE_UNSIGNED_INT_8_8_8_8            0x8035
#define TINYKTX_GL_TYPE_UNSIGNED_INT_10_10_10_2          0x8036
#define TINYKTX_GL_TYPE_UNSIGNED_BYTE_2_3_3_REV          0x8362
#define TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_6_5            0x8363
#define TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_6_5_REV        0x8364
#define TINYKTX_GL_TYPE_UNSIGNED_SHORT_4_4_4_4_REV      0x8365
#define TINYKTX_GL_TYPE_UNSIGNED_SHORT_1_5_5_5_REV      0x8366
#define TINYKTX_GL_TYPE_UNSIGNED_INT_8_8_8_8_REV        0x8367
#define TINYKTX_GL_TYPE_UNSIGNED_INT_2_10_10_10_REV      0x8368
#define TINYKTX_GL_TYPE_UNSIGNED_INT_24_8                0x84FA
#define TINYKTX_GL_TYPE_UNSIGNED_INT_5_9_9_9_REV        0x8C3E
#define TINYKTX_GL_TYPE_UNSIGNED_INT_10F_11F_11F_REV    0x8C3B
#define TINYKTX_GL_TYPE_FLOAT_32_UNSIGNED_INT_24_8_REV  0x8DAD

// formats
#define TINYKTX_GL_FORMAT_RED                              0x1903
#define TINYKTX_GL_FORMAT_GREEN                            0x1904
#define TINYKTX_GL_FORMAT_BLUE                            0x1905
#define TINYKTX_GL_FORMAT_ALPHA                            0x1906
#define TINYKTX_GL_FORMAT_RGB                              0x1907
#define TINYKTX_GL_FORMAT_RGBA                            0x1908
#define TINYKTX_GL_FORMAT_LUMINANCE                        0x1909
#define TINYKTX_GL_FORMAT_LUMINANCE_ALPHA                  0x190A
#define TINYKTX_GL_FORMAT_ABGR                            0x8000
#define TINYKTX_GL_FORMAT_INTENSITY                        0x8049
#define TINYKTX_GL_FORMAT_BGR                              0x80E0
#define TINYKTX_GL_FORMAT_BGRA                            0x80E1
#define TINYKTX_GL_FORMAT_RG                              0x8227
#define TINYKTX_GL_FORMAT_RG_INTEGER                      0x8228
#define TINYKTX_GL_FORMAT_SRGB                            0x8C40
#define TINYKTX_GL_FORMAT_SRGB_ALPHA                      0x8C42
#define TINYKTX_GL_FORMAT_SLUMINANCE_ALPHA                0x8C44
#define TINYKTX_GL_FORMAT_SLUMINANCE                      0x8C46
#define TINYKTX_GL_FORMAT_RED_INTEGER                      0x8D94
#define TINYKTX_GL_FORMAT_GREEN_INTEGER                    0x8D95
#define TINYKTX_GL_FORMAT_BLUE_INTEGER                    0x8D96
#define TINYKTX_GL_FORMAT_ALPHA_INTEGER                    0x8D97
#define TINYKTX_GL_FORMAT_RGB_INTEGER                      0x8D98
#define TINYKTX_GL_FORMAT_RGBA_INTEGER                    0x8D99
#define TINYKTX_GL_FORMAT_BGR_INTEGER                      0x8D9A
#define TINYKTX_GL_FORMAT_BGRA_INTEGER                    0x8D9B
#define TINYKTX_GL_FORMAT_RED_SNORM                        0x8F90
#define TINYKTX_GL_FORMAT_RG_SNORM                        0x8F91
#define TINYKTX_GL_FORMAT_RGB_SNORM                        0x8F92
#define TINYKTX_GL_FORMAT_RGBA_SNORM                      0x8F93

#define TINYKTX_GL_INTFORMAT_ALPHA4                          0x803B
#define TINYKTX_GL_INTFORMAT_ALPHA8                          0x803C
#define TINYKTX_GL_INTFORMAT_ALPHA12                          0x803D
#define TINYKTX_GL_INTFORMAT_ALPHA16                          0x803E
#define TINYKTX_GL_INTFORMAT_LUMINANCE4                      0x803F
#define TINYKTX_GL_INTFORMAT_LUMINANCE8                      0x8040
#define TINYKTX_GL_INTFORMAT_LUMINANCE12                      0x8041
#define TINYKTX_GL_INTFORMAT_LUMINANCE16                      0x8042
#define TINYKTX_GL_INTFORMAT_LUMINANCE4_ALPHA4                0x8043
#define TINYKTX_GL_INTFORMAT_LUMINANCE6_ALPHA2                0x8044
#define TINYKTX_GL_INTFORMAT_LUMINANCE8_ALPHA8                0x8045
#define TINYKTX_GL_INTFORMAT_LUMINANCE12_ALPHA4              0x8046
#define TINYKTX_GL_INTFORMAT_LUMINANCE12_ALPHA12              0x8047
#define TINYKTX_GL_INTFORMAT_LUMINANCE16_ALPHA16              0x8048
#define TINYKTX_GL_INTFORMAT_INTENSITY4                      0x804A
#define TINYKTX_GL_INTFORMAT_INTENSITY8                      0x804B
#define TINYKTX_GL_INTFORMAT_INTENSITY12                      0x804C
#define TINYKTX_GL_INTFORMAT_INTENSITY16                      0x804D
#define TINYKTX_GL_INTFORMAT_RGB2                            0x804E
#define TINYKTX_GL_INTFORMAT_RGB4                            0x804F
#define TINYKTX_GL_INTFORMAT_RGB5                            0x8050
#define TINYKTX_GL_INTFORMAT_RGB8                              0x8051
#define TINYKTX_GL_INTFORMAT_RGB10                            0x8052
#define TINYKTX_GL_INTFORMAT_RGB12                            0x8053
#define TINYKTX_GL_INTFORMAT_RGB16                            0x8054
#define TINYKTX_GL_INTFORMAT_RGBA2                            0x8055
#define TINYKTX_GL_INTFORMAT_RGBA4                            0x8056
#define TINYKTX_GL_INTFORMAT_RGB5_A1                          0x8057
#define TINYKTX_GL_INTFORMAT_RGBA8                            0x8058
#define TINYKTX_GL_INTFORMAT_RGB10_A2                        0x8059
#define TINYKTX_GL_INTFORMAT_RGBA12                          0x805A
#define TINYKTX_GL_INTFORMAT_RGBA16                          0x805B
#define TINYKTX_GL_INTFORMAT_R8                              0x8229
#define TINYKTX_GL_INTFORMAT_R16                              0x822A
#define TINYKTX_GL_INTFORMAT_RG8                              0x822B
#define TINYKTX_GL_INTFORMAT_RG16                            0x822C
#define TINYKTX_GL_INTFORMAT_R16F                            0x822D
#define TINYKTX_GL_INTFORMAT_R32F                            0x822E
#define TINYKTX_GL_INTFORMAT_RG16F                            0x822F
#define TINYKTX_GL_INTFORMAT_RG32F                            0x8230
#define TINYKTX_GL_INTFORMAT_R8I                              0x8231
#define TINYKTX_GL_INTFORMAT_R8UI                            0x8232
#define TINYKTX_GL_INTFORMAT_R16I                            0x8233
#define TINYKTX_GL_INTFORMAT_R16UI                            0x8234
#define TINYKTX_GL_INTFORMAT_R32I                            0x8235
#define TINYKTX_GL_INTFORMAT_R32UI                            0x8236
#define TINYKTX_GL_INTFORMAT_RG8I                            0x8237
#define TINYKTX_GL_INTFORMAT_RG8UI                            0x8238
#define TINYKTX_GL_INTFORMAT_RG16I                            0x8239
#define TINYKTX_GL_INTFORMAT_RG16UI                          0x823A
#define TINYKTX_GL_INTFORMAT_RG32I                            0x823B
#define TINYKTX_GL_INTFORMAT_RG32UI                          0x823C
#define TINYKTX_GL_INTFORMAT_RGBA32F                          0x8814
#define TINYKTX_GL_INTFORMAT_RGB32F                          0x8815
#define TINYKTX_GL_INTFORMAT_RGBA16F                          0x881A
#define TINYKTX_GL_INTFORMAT_RGB16F                          0x881B
#define TINYKTX_GL_INTFORMAT_R11F_G11F_B10F                  0x8C3A
#define TINYKTX_GL_INTFORMAT_UNSIGNED_INT_10F_11F_11F_REV      0x8C3B
#define TINYKTX_GL_INTFORMAT_RGB9_E5                          0x8C3D
#define TINYKTX_GL_INTFORMAT_SRGB8                            0x8C41
#define TINYKTX_GL_INTFORMAT_SRGB8_ALPHA8                      0x8C43
#define TINYKTX_GL_INTFORMAT_SLUMINANCE8_ALPHA8              0x8C45
#define TINYKTX_GL_INTFORMAT_SLUMINANCE8                      0x8C47
#define TINYKTX_GL_INTFORMAT_RGB565                          0x8D62
#define TINYKTX_GL_INTFORMAT_RGBA32UI                        0x8D70
#define TINYKTX_GL_INTFORMAT_RGB32UI                          0x8D71
#define TINYKTX_GL_INTFORMAT_RGBA16UI                        0x8D76
#define TINYKTX_GL_INTFORMAT_RGB16UI                          0x8D77
#define TINYKTX_GL_INTFORMAT_RGBA8UI                          0x8D7C
#define TINYKTX_GL_INTFORMAT_RGB8UI                          0x8D7D
#define TINYKTX_GL_INTFORMAT_RGBA32I                          0x8D82
#define TINYKTX_GL_INTFORMAT_RGB32I                          0x8D83
#define TINYKTX_GL_INTFORMAT_RGBA16I                          0x8D88
#define TINYKTX_GL_INTFORMAT_RGB16I                          0x8D89
#define TINYKTX_GL_INTFORMAT_RGBA8I                          0x8D8E
#define TINYKTX_GL_INTFORMAT_RGB8I                            0x8D8F
#define TINYKTX_GL_INTFORMAT_FLOAT_32_UNSIGNED_INT_24_8_REV  0x8DAD
#define TINYKTX_GL_INTFORMAT_R8_SNORM                        0x8F94
#define TINYKTX_GL_INTFORMAT_RG8_SNORM                        0x8F95
#define TINYKTX_GL_INTFORMAT_RGB8_SNORM                      0x8F96
#define TINYKTX_GL_INTFORMAT_RGBA8_SNORM                      0x8F97
#define TINYKTX_GL_INTFORMAT_R16_SNORM                        0x8F98
#define TINYKTX_GL_INTFORMAT_RG16_SNORM                      0x8F99
#define TINYKTX_GL_INTFORMAT_RGB16_SNORM                      0x8F9A
#define TINYKTX_GL_INTFORMAT_RGBA16_SNORM                    0x8F9B
#define TINYKTX_GL_INTFORMAT_ALPHA8_SNORM                    0x9014
#define TINYKTX_GL_INTFORMAT_LUMINANCE8_SNORM                0x9015
#define TINYKTX_GL_INTFORMAT_LUMINANCE8_ALPHA8_SNORM          0x9016
#define TINYKTX_GL_INTFORMAT_INTENSITY8_SNORM                0x9017
#define TINYKTX_GL_INTFORMAT_ALPHA16_SNORM                    0x9018
#define TINYKTX_GL_INTFORMAT_LUMINANCE16_SNORM                0x9019
#define TINYKTX_GL_INTFORMAT_LUMINANCE16_ALPHA16_SNORM        0x901A
#define TINYKTX_GL_INTFORMAT_INTENSITY16_SNORM                0x901B

#define TINYKTX_GL_PALETTE4_RGB8_OES              0x8B90
#define TINYKTX_GL_PALETTE4_RGBA8_OES             0x8B91
#define TINYKTX_GL_PALETTE4_R5_G6_B5_OES          0x8B92
#define TINYKTX_GL_PALETTE4_RGBA4_OES             0x8B93
#define TINYKTX_GL_PALETTE4_RGB5_A1_OES           0x8B94
#define TINYKTX_GL_PALETTE8_RGB8_OES              0x8B95
#define TINYKTX_GL_PALETTE8_RGBA8_OES             0x8B96
#define TINYKTX_GL_PALETTE8_R5_G6_B5_OES          0x8B97
#define TINYKTX_GL_PALETTE8_RGBA4_OES             0x8B98
#define TINYKTX_GL_PALETTE8_RGB5_A1_OES           0x8B99

// compressed formats

#define TINYKTX_GL_COMPRESSED_RGB_S3TC_DXT1                  	0x83F0
#define TINYKTX_GL_COMPRESSED_RGBA_S3TC_DXT1                  0x83F1
#define TINYKTX_GL_COMPRESSED_RGBA_S3TC_DXT3                  0x83F2
#define TINYKTX_GL_COMPRESSED_RGBA_S3TC_DXT5                  0x83F3
#define TINYKTX_GL_COMPRESSED_3DC_X_AMD                       0x87F9
#define TINYKTX_GL_COMPRESSED_3DC_XY_AMD                      0x87FA
#define TINYKTX_GL_COMPRESSED_ATC_RGBA_INTERPOLATED_ALPHA    	0x87EE
#define TINYKTX_GL_COMPRESSED_SRGB_PVRTC_2BPPV1               0x8A54
#define TINYKTX_GL_COMPRESSED_SRGB_PVRTC_4BPPV1               0x8A55
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1         0x8A56
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1         0x8A57
#define TINYKTX_GL_COMPRESSED_RGB_PVRTC_4BPPV1                0x8C00
#define TINYKTX_GL_COMPRESSED_RGB_PVRTC_2BPPV1                0x8C01
#define TINYKTX_GL_COMPRESSED_RGBA_PVRTC_4BPPV1               0x8C02
#define TINYKTX_GL_COMPRESSED_RGBA_PVRTC_2BPPV1               0x8C03
#define TINYKTX_GL_COMPRESSED_SRGB_S3TC_DXT1                  0x8C4C
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1            0x8C4D
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3            0x8C4E
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5            0x8C4F
#define TINYKTX_GL_COMPRESSED_LUMINANCE_LATC1                	0x8C70
#define TINYKTX_GL_COMPRESSED_SIGNED_LUMINANCE_LATC1          0x8C71
#define TINYKTX_GL_COMPRESSED_LUMINANCE_ALPHA_LATC2           0x8C72
#define TINYKTX_GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2    0x8C73
#define TINYKTX_GL_COMPRESSED_ATC_RGB                         0x8C92
#define TINYKTX_GL_COMPRESSED_ATC_RGBA_EXPLICIT_ALPHA         0x8C93
#define TINYKTX_GL_COMPRESSED_RED_RGTC1                       0x8DBB
#define TINYKTX_GL_COMPRESSED_SIGNED_RED_RGTC1                0x8DBC
#define TINYKTX_GL_COMPRESSED_RED_GREEN_RGTC2                	0x8DBD
#define TINYKTX_GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2          0x8DBE
#define TINYKTX_GL_COMPRESSED_ETC1_RGB8_OES                   0x8D64
#define TINYKTX_GL_COMPRESSED_RGBA_BPTC_UNORM                	0x8E8C
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM          	0x8E8D
#define TINYKTX_GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT          	0x8E8E
#define TINYKTX_GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT        	0x8E8F
#define TINYKTX_GL_COMPRESSED_R11_EAC                        	0x9270
#define TINYKTX_GL_COMPRESSED_SIGNED_R11_EAC                  0x9271
#define TINYKTX_GL_COMPRESSED_RG11_EAC                        0x9272
#define TINYKTX_GL_COMPRESSED_SIGNED_RG11_EAC                	0x9273
#define TINYKTX_GL_COMPRESSED_RGB8_ETC2                      	0x9274
#define TINYKTX_GL_COMPRESSED_SRGB8_ETC2                      0x9275
#define TINYKTX_GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2   0x9276
#define TINYKTX_GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2  0x9277
#define TINYKTX_GL_COMPRESSED_RGBA8_ETC2_EAC                  0x9278
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC           0x9279
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV2         0x93F0
#define TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV2         0x93F1
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_4x4		   							0x93B0
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_5x4		              	0x93B1
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_5x5		              	0x93B2
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_6x5		              	0x93B3
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_6x6		              	0x93B4
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_8x5		              	0x93B5
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_8x6		              	0x93B6
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_8x8		              	0x93B7
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x5		              0x93B8
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x6		              0x93B9
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x8		              0x93BA
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x10	            		0x93BB
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_12x10	            		0x93BC
#define TINYKTX_GL_COMPRESSED_RGBA_ASTC_12x12	            		0x93BD
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4			      0x93D0
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4			      0x93D1
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5			      0x93D2
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5						0x93D3
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6						0x93D4
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5						0x93D5
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6						0x93D6
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8						0x93D7
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5					0x93D8
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6					0x93D9
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8					0x93DA
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10					0x93DB
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10     		0x93DC
#define TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12     		0x93DD

#endif // end header

#ifdef TINYKTX_IMPLEMENTATION

	typedef struct TinyKtx_Header {
		uint8_t identifier[12];
		uint32_t endianness;
		uint32_t glType;
		uint32_t glTypeSize;
		uint32_t glFormat;
		uint32_t glInternalFormat;
		uint32_t glBaseInternalFormat;
		uint32_t pixelWidth;
		uint32_t pixelHeight;
		uint32_t pixelDepth;
		uint32_t numberOfArrayElements;
		uint32_t numberOfFaces;
		uint32_t numberOfMipmapLevels;
		uint32_t bytesOfKeyValueData;

	} TinyKtx_Header;

	typedef struct TinyKtx_KeyValuePair {
		uint32_t size;
	} TinyKtx_KeyValuePair; // followed by at least size bytes (aligned to 4)


	typedef struct TinyKtx_Context {
		TinyKtx_Callbacks callbacks;
		void *user;
		uint64_t headerPos;
		uint64_t firstImagePos;

		TinyKtx_Header header;

		TinyKtx_KeyValuePair const *keyData;
		bool headerValid;
		bool sameEndian;

		uint32_t mipMapSizes[TINYKTX_MAX_MIPMAPLEVELS];
		uint8_t const *mipmaps[TINYKTX_MAX_MIPMAPLEVELS];

	} TinyKtx_Context;

	static uint8_t TinyKtx_fileIdentifier[12] = {
			0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
	};

	static void TinyKtx_NullErrorFunc(void *user, char const *msg) {}

	TinyKtx_ContextHandle TinyKtx_CreateContext(TinyKtx_Callbacks const *callbacks, void *user) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)callbacks->allocFn(user, sizeof(TinyKtx_Context));
		if (ctx == NULL)
			return NULL;

		memset(ctx, 0, sizeof(TinyKtx_Context));
		memcpy(&ctx->callbacks, callbacks, sizeof(TinyKtx_Callbacks));
		ctx->user = user;
		if (ctx->callbacks.errorFn == NULL) {
			ctx->callbacks.errorFn = &TinyKtx_NullErrorFunc;
		}

		if (ctx->callbacks.readFn == NULL) {
			ctx->callbacks.errorFn(user, "TinyKtx must have read callback");
			return NULL;
		}
		if (ctx->callbacks.allocFn == NULL) {
			ctx->callbacks.errorFn(user, "TinyKtx must have alloc callback");
			return NULL;
		}
		if (ctx->callbacks.freeFn == NULL) {
			ctx->callbacks.errorFn(user, "TinyKtx must have free callback");
			return NULL;
		}
		if (ctx->callbacks.seekFn == NULL) {
			ctx->callbacks.errorFn(user, "TinyKtx must have seek callback");
			return NULL;
		}
		if (ctx->callbacks.tellFn == NULL) {
			ctx->callbacks.errorFn(user, "TinyKtx must have tell callback");
			return NULL;
		}

		TinyKtx_Reset(ctx);

		return ctx;
	}

	void TinyKtx_DestroyContext(TinyKtx_ContextHandle handle) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return;
		TinyKtx_Reset(handle);

		ctx->callbacks.freeFn(ctx->user, ctx);
	}

	void TinyKtx_Reset(TinyKtx_ContextHandle handle) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return;

		// backup user provided callbacks and data
		TinyKtx_Callbacks callbacks;
		memcpy(&callbacks, &ctx->callbacks, sizeof(TinyKtx_Callbacks));
		void *user = ctx->user;

		// free memory of sub data
		if (ctx->keyData != NULL) {
			callbacks.freeFn(user, (void *)ctx->keyData);
		}

		for (int i = 0; i < TINYKTX_MAX_MIPMAPLEVELS; ++i) {
			if (ctx->mipmaps[i] != NULL) {
				callbacks.freeFn(user, (void *)ctx->mipmaps[i]);
			}
		}

		// reset to default state
		memset(ctx, 0, sizeof(TinyKtx_Context));
		memcpy(&ctx->callbacks, &callbacks, sizeof(TinyKtx_Callbacks));
		ctx->user = user;

	}


	bool TinyKtx_ReadHeader(TinyKtx_ContextHandle handle) {

		static uint32_t const sameEndianDecider = 0x04030201;
		static uint32_t const differentEndianDecider = 0x01020304;

		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return false;

		ctx->headerPos = ctx->callbacks.tellFn(ctx->user);
		ctx->callbacks.readFn(ctx->user, &ctx->header, sizeof(TinyKtx_Header));

		if (memcmp(&ctx->header.identifier, TinyKtx_fileIdentifier, 12) != 0) {
			ctx->callbacks.errorFn(ctx->user, "Not a KTX file or corrupted as identified isn't valid");
			return false;
		}

		if (ctx->header.endianness == sameEndianDecider) {
			ctx->sameEndian = true;
		} else if (ctx->header.endianness == differentEndianDecider) {
			ctx->sameEndian = false;
		} else {
			// corrupt or mid endian?
			ctx->callbacks.errorFn(ctx->user, "Endian Error");
			return false;
		}

		if (ctx->header.numberOfFaces != 1 && ctx->header.numberOfFaces != 6) {
			ctx->callbacks.errorFn(ctx->user, "no. of Faces must be 1 or 6");
			return false;
		}

		ctx->keyData = (TinyKtx_KeyValuePair const *)ctx->callbacks.allocFn(ctx->user, ctx->header.bytesOfKeyValueData);
		ctx->callbacks.readFn(ctx->user, (void *)ctx->keyData, ctx->header.bytesOfKeyValueData);

		ctx->firstImagePos = ctx->callbacks.tellFn(ctx->user);

		ctx->headerValid = true;
		return true;
	}

	bool TinyKtx_GetValue(TinyKtx_ContextHandle handle, char const *key, void const **value) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return false;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return false;
		}

		if (ctx->keyData == NULL) {
			ctx->callbacks.errorFn(ctx->user, "No key value data in this KTX");
			return false;
		}

		TinyKtx_KeyValuePair const *curKey = ctx->keyData;
		while (((uint8_t *)curKey - (uint8_t *)ctx->keyData) < ctx->header.bytesOfKeyValueData) {
			char const *kvp = (char const *)curKey;

			if (strcmp(kvp, key) == 0) {
				size_t sl = strlen(kvp);
				*value = (void const *)(kvp + sl);
				return true;
			}
			curKey = curKey + ((curKey->size + 3u) & ~3u);
		}
		return false;
	}

	bool TinyKtx_Is1D(TinyKtx_ContextHandle handle) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return false;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return false;
		}
		return (ctx->header.pixelHeight <= 1) && (ctx->header.pixelDepth <= 1);
	}
	bool TinyKtx_Is2D(TinyKtx_ContextHandle handle) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return false;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return false;
		}

		return (ctx->header.pixelHeight > 1 && ctx->header.pixelDepth <= 1);
	}
	bool TinyKtx_Is3D(TinyKtx_ContextHandle handle) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return false;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return false;
		}

		return (ctx->header.pixelHeight > 1 && ctx->header.pixelDepth > 1);
	}

	bool TinyKtx_IsCubemap(TinyKtx_ContextHandle handle) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return false;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return false;
		}

		return (ctx->header.numberOfFaces == 6);
	}
	bool TinyKtx_IsArray(TinyKtx_ContextHandle handle) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return false;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return false;
		}

		return (ctx->header.numberOfArrayElements > 1);
	}

	bool TinyKtx_Dimensions(TinyKtx_ContextHandle handle,
		uint32_t *width,
		uint32_t *height,
		uint32_t *depth,
		uint32_t *slices) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return false;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return false;
		}

		if (width)
			*width = ctx->header.pixelWidth;
		if (height)
			*height = ctx->header.pixelWidth;
		if (depth)
			*depth = ctx->header.pixelDepth;
		if (slices)
			*slices = ctx->header.numberOfArrayElements;
		return true;
	}

	uint32_t TinyKtx_Width(TinyKtx_ContextHandle handle) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return 0;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return 0;
		}
		return ctx->header.pixelWidth;

	}

	uint32_t TinyKtx_Height(TinyKtx_ContextHandle handle) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return 0;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return 0;
		}

		return ctx->header.pixelHeight;
	}

	uint32_t TinyKtx_Depth(TinyKtx_ContextHandle handle) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return 0;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return 0;
		}
		return ctx->header.pixelDepth;
	}

	uint32_t TinyKtx_ArraySlices(TinyKtx_ContextHandle handle) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return 0;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return 0;
		}

		return ctx->header.numberOfArrayElements;
	}

	uint32_t TinyKtx_NumberOfMipmaps(TinyKtx_ContextHandle handle) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return 0;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return 0;
		}

		return ctx->header.numberOfMipmapLevels ? ctx->header.numberOfMipmapLevels : 1;
	}

	bool TinyKtx_NeedsGenerationOfMipmaps(TinyKtx_ContextHandle handle) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return false;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return false;
		}

		return ctx->header.numberOfMipmapLevels == 0;
	}

	bool TinyKtx_NeedsEndianCorrecting(TinyKtx_ContextHandle handle) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return false;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return false;
		}

		return ctx->sameEndian == false;
	}

	bool TinyKtx_GetFormatGL(TinyKtx_ContextHandle handle, uint32_t *glformat, uint32_t *gltype, uint32_t *glinternalformat, uint32_t *typesize, uint32_t *glbaseinternalformat) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return false;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return false;
		}

		*glformat = ctx->header.glFormat;
		*gltype = ctx->header.glType;
		*glinternalformat = ctx->header.glInternalFormat;
		*glbaseinternalformat = ctx->header.glBaseInternalFormat;
		*typesize = ctx->header.glBaseInternalFormat;

		return true;
	}

	static uint32_t TinyKtx_imageSize(TinyKtx_ContextHandle handle, uint32_t mipmaplevel, bool seekLast) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;

		if (mipmaplevel >= ctx->header.numberOfMipmapLevels) {
			ctx->callbacks.errorFn(ctx->user, "Invalid mipmap level");
			return 0;
		}
		if (mipmaplevel >= TINYKTX_MAX_MIPMAPLEVELS) {
			ctx->callbacks.errorFn(ctx->user, "Invalid mipmap level");
			return 0;
		}
		if (!seekLast && ctx->mipMapSizes[mipmaplevel] != 0)
			return ctx->mipMapSizes[mipmaplevel];

		uint64_t currentOffset = ctx->firstImagePos;
		for (uint32_t i = 0; i <= mipmaplevel; ++i) {
			uint32_t size;
			// if we have already read this mipmaps size use it
			if (ctx->mipMapSizes[i] != 0) {
				size = ctx->mipMapSizes[i];
				if (seekLast && i == mipmaplevel) {
					ctx->callbacks.seekFn(ctx->user, currentOffset + sizeof(uint32_t));
				}
			} else {
				// otherwise seek and read it
				ctx->callbacks.seekFn(ctx->user, currentOffset);
				size_t readchk = ctx->callbacks.readFn(ctx->user, &size, sizeof(uint32_t));
				if (readchk != 4) {
					ctx->callbacks.errorFn(ctx->user, "Reading image size error");
					return 0;
				}
				// so in the really small print KTX v1 states GL_UNPACK_ALIGNMENT = 4
				// which PVR Texture Tool and I missed. It means pad to 1, 2, 4, 8
				// note 3 or 6 bytes are rounded up.
				// we rely on the loader setting this right, we should handle file with
				// it not to standard but its really the level up that has to do this

				if (ctx->header.numberOfFaces == 6 && ctx->header.numberOfArrayElements == 0) {
					size = ((size + 3u) & ~3u) * 6; // face padding and 6 faces
				}

				ctx->mipMapSizes[i] = size;
			}
			currentOffset += (size + sizeof(uint32_t) + 3u) & ~3u; // size + mip padding
		}

		return ctx->mipMapSizes[mipmaplevel];
	}

	uint32_t TinyKtx_ImageSize(TinyKtx_ContextHandle handle, uint32_t mipmaplevel) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL) return 0;

		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return 0;
		}
		return TinyKtx_imageSize(handle, mipmaplevel, false);
	}

	void const *TinyKtx_ImageRawData(TinyKtx_ContextHandle handle, uint32_t mipmaplevel) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return NULL;

		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return NULL;
		}

		if (mipmaplevel >= ctx->header.numberOfMipmapLevels) {
			ctx->callbacks.errorFn(ctx->user, "Invalid mipmap level");
			return NULL;
		}

		if (mipmaplevel >= TINYKTX_MAX_MIPMAPLEVELS) {
			ctx->callbacks.errorFn(ctx->user, "Invalid mipmap level");
			return NULL;
		}

		if (ctx->mipmaps[mipmaplevel] != NULL)
			return ctx->mipmaps[mipmaplevel];

		uint32_t size = TinyKtx_imageSize(handle, mipmaplevel, true);
		if (size == 0)
			return NULL;

		ctx->mipmaps[mipmaplevel] = (uint8_t const *)ctx->callbacks.allocFn(ctx->user, size);
		if (ctx->mipmaps[mipmaplevel]) {
			ctx->callbacks.readFn(ctx->user, (void *)ctx->mipmaps[mipmaplevel], size);
		}

		return ctx->mipmaps[mipmaplevel];
	}

#define FT(fmt, type, intfmt, size) *glformat = TINYKTX_GL_FORMAT_##fmt; \
                                    *gltype = TINYKTX_GL_TYPE_##type; \
                                    *glinternalformat = TINYKTX_GL_INTFORMAT_##intfmt; \
                                    *typesize = size; \
                                    return true;
#define FTC(fmt, intfmt) *glformat = TINYKTX_GL_FORMAT_##fmt; \
                                    *gltype = TINYKTX_GL_TYPE_COMPRESSED; \
                                    *glinternalformat = TINYKTX_GL_COMPRESSED_##intfmt; \
                                    *typesize = 1; \
                                    return true;

	bool TinyKtx_CrackFormatToGL(TinyKtx_Format format,
		uint32_t *glformat,
		uint32_t *gltype,
		uint32_t *glinternalformat,
		uint32_t *typesize) {
		switch (format) {
		case TKTX_R4G4_UNORM_PACK8: FT(RG, UNSIGNED_SHORT_4_4_4_4, RGB4, 1)
		case TKTX_R4G4B4A4_UNORM_PACK16: FT(RGBA, UNSIGNED_SHORT_4_4_4_4, RGBA4, 2)

		case TKTX_B4G4R4A4_UNORM_PACK16: FT(BGRA, UNSIGNED_SHORT_4_4_4_4_REV, RGBA4, 2)

		case TKTX_R5G6B5_UNORM_PACK16: FT(RGB, UNSIGNED_SHORT_5_6_5_REV, RGB565, 2)
		case TKTX_B5G6R5_UNORM_PACK16: FT(BGR, UNSIGNED_SHORT_5_6_5, RGB565, 2)

		case TKTX_R5G5B5A1_UNORM_PACK16: FT(RGBA, UNSIGNED_SHORT_5_5_5_1, RGB5_A1, 2)
		case TKTX_A1R5G5B5_UNORM_PACK16: FT(RGBA, UNSIGNED_SHORT_1_5_5_5_REV, RGB5_A1, 2)
		case TKTX_B5G5R5A1_UNORM_PACK16: FT(BGRA, UNSIGNED_SHORT_5_5_5_1, RGB5_A1, 2)

		case TKTX_A2R10G10B10_UNORM_PACK32: FT(BGRA, UNSIGNED_INT_2_10_10_10_REV, RGB10_A2, 4)
		case TKTX_A2R10G10B10_UINT_PACK32: FT(BGRA_INTEGER, UNSIGNED_INT_2_10_10_10_REV, RGB10_A2, 4)
		case TKTX_A2B10G10R10_UNORM_PACK32: FT(RGBA, UNSIGNED_INT_2_10_10_10_REV, RGB10_A2, 4)
		case TKTX_A2B10G10R10_UINT_PACK32: FT(RGBA_INTEGER, UNSIGNED_INT_2_10_10_10_REV, RGB10_A2, 4)

		case TKTX_R8_UNORM: FT(RED, UNSIGNED_BYTE, R8, 1)
		case TKTX_R8_SNORM: FT(RED, BYTE, R8_SNORM, 1)
		case TKTX_R8_UINT: FT(RED_INTEGER, UNSIGNED_BYTE, R8UI, 1)
		case TKTX_R8_SINT: FT(RED_INTEGER, BYTE, R8I, 1)
		case TKTX_R8_SRGB: FT(SLUMINANCE, UNSIGNED_BYTE, SRGB8, 1)

		case TKTX_R8G8_UNORM: FT(RG, UNSIGNED_BYTE, RG8, 1)
		case TKTX_R8G8_SNORM: FT(RG, BYTE, RG8_SNORM, 1)
		case TKTX_R8G8_UINT: FT(RG_INTEGER, UNSIGNED_BYTE, RG8UI, 1)
		case TKTX_R8G8_SINT: FT(RG_INTEGER, BYTE, RG8I, 1)
		case TKTX_R8G8_SRGB: FT(SLUMINANCE_ALPHA, UNSIGNED_BYTE, SRGB8, 1)

		case TKTX_R8G8B8_UNORM: FT(RGB, UNSIGNED_BYTE, RGB8, 1)
		case TKTX_R8G8B8_SNORM: FT(RGB, BYTE, RGB8_SNORM, 1)
		case TKTX_R8G8B8_UINT: FT(RGB_INTEGER, UNSIGNED_BYTE, RGB8UI, 1)
		case TKTX_R8G8B8_SINT: FT(RGB_INTEGER, BYTE, RGB8I, 1)
		case TKTX_R8G8B8_SRGB: FT(SRGB, UNSIGNED_BYTE, SRGB8, 1)

		case TKTX_B8G8R8_UNORM: FT(BGR, UNSIGNED_BYTE, RGB8, 1)
		case TKTX_B8G8R8_SNORM: FT(BGR, BYTE, RGB8_SNORM, 1)
		case TKTX_B8G8R8_UINT: FT(BGR_INTEGER, UNSIGNED_BYTE, RGB8UI, 1)
		case TKTX_B8G8R8_SINT: FT(BGR_INTEGER, BYTE, RGB8I, 1)
		case TKTX_B8G8R8_SRGB: FT(BGR, UNSIGNED_BYTE, SRGB8, 1)

		case TKTX_R8G8B8A8_UNORM:FT(RGBA, UNSIGNED_BYTE, RGBA8, 1)
		case TKTX_R8G8B8A8_SNORM:FT(RGBA, BYTE, RGBA8_SNORM, 1)
		case TKTX_R8G8B8A8_UINT: FT(RGBA_INTEGER, UNSIGNED_BYTE, RGBA8UI, 1)
		case TKTX_R8G8B8A8_SINT: FT(RGBA_INTEGER, BYTE, RGBA8I, 1)
		case TKTX_R8G8B8A8_SRGB: FT(SRGB_ALPHA, UNSIGNED_BYTE, SRGB8, 1)

		case TKTX_B8G8R8A8_UNORM: FT(BGRA, UNSIGNED_BYTE, RGBA8, 1)
		case TKTX_B8G8R8A8_SNORM: FT(BGRA, BYTE, RGBA8_SNORM, 1)
		case TKTX_B8G8R8A8_UINT: FT(BGRA_INTEGER, UNSIGNED_BYTE, RGBA8UI, 1)
		case TKTX_B8G8R8A8_SINT: FT(BGRA_INTEGER, BYTE, RGBA8I, 1)
		case TKTX_B8G8R8A8_SRGB: FT(BGRA, UNSIGNED_BYTE, SRGB8, 1)

		case TKTX_E5B9G9R9_UFLOAT_PACK32: FT(BGR, UNSIGNED_INT_5_9_9_9_REV, RGB9_E5, 4);
		case TKTX_A8B8G8R8_UNORM_PACK32: FT(ABGR, UNSIGNED_BYTE, RGBA8, 1)
		case TKTX_A8B8G8R8_SNORM_PACK32: FT(ABGR, BYTE, RGBA8, 1)
		case TKTX_A8B8G8R8_UINT_PACK32: FT(ABGR, UNSIGNED_BYTE, RGBA8UI, 1)
		case TKTX_A8B8G8R8_SINT_PACK32: FT(ABGR, BYTE, RGBA8I, 1)
		case TKTX_A8B8G8R8_SRGB_PACK32: FT(ABGR, UNSIGNED_BYTE, SRGB8, 1)
		case TKTX_B10G11R11_UFLOAT_PACK32: FT(BGR, UNSIGNED_INT_10F_11F_11F_REV, R11F_G11F_B10F, 4)

		case TKTX_R16_UNORM: FT(RED, UNSIGNED_SHORT, R16, 2)
		case TKTX_R16_SNORM: FT(RED, SHORT, R16_SNORM, 2)
		case TKTX_R16_UINT: FT(RED_INTEGER, UNSIGNED_SHORT, R16UI, 2)
		case TKTX_R16_SINT: FT(RED_INTEGER, SHORT, R16I, 2)
		case TKTX_R16_SFLOAT:FT(RED, HALF_FLOAT, R16F, 2)

		case TKTX_R16G16_UNORM: FT(RG, UNSIGNED_SHORT, RG16, 2)
		case TKTX_R16G16_SNORM: FT(RG, SHORT, RG16_SNORM, 2)
		case TKTX_R16G16_UINT: FT(RG_INTEGER, UNSIGNED_SHORT, RG16UI, 2)
		case TKTX_R16G16_SINT: FT(RG_INTEGER, SHORT, RG16I, 2)
		case TKTX_R16G16_SFLOAT:FT(RG, HALF_FLOAT, RG16F, 2)

		case TKTX_R16G16B16_UNORM: FT(RGB, UNSIGNED_SHORT, RGB16, 2)
		case TKTX_R16G16B16_SNORM: FT(RGB, SHORT, RGB16_SNORM, 2)
		case TKTX_R16G16B16_UINT: FT(RGB_INTEGER, UNSIGNED_SHORT, RGB16UI, 2)
		case TKTX_R16G16B16_SINT: FT(RGB_INTEGER, SHORT, RGB16I, 2)
		case TKTX_R16G16B16_SFLOAT: FT(RGB, HALF_FLOAT, RGB16F, 2)

		case TKTX_R16G16B16A16_UNORM: FT(RGBA, UNSIGNED_SHORT, RGBA16, 2)
		case TKTX_R16G16B16A16_SNORM: FT(RGBA, SHORT, RGBA16_SNORM, 2)
		case TKTX_R16G16B16A16_UINT: FT(RGBA_INTEGER, UNSIGNED_SHORT, RGBA16UI, 2)
		case TKTX_R16G16B16A16_SINT: FT(RGBA_INTEGER, SHORT, RGBA16I, 2)
		case TKTX_R16G16B16A16_SFLOAT:FT(RGBA, HALF_FLOAT, RGBA16F, 2)

		case TKTX_R32_UINT: FT(RED_INTEGER, UNSIGNED_INT, R32UI, 4)
		case TKTX_R32_SINT: FT(RED_INTEGER, INT, R32I, 4)
		case TKTX_R32_SFLOAT: FT(RED, FLOAT, R32F, 4)

		case TKTX_R32G32_UINT: FT(RG_INTEGER, UNSIGNED_INT, RG32UI, 4)
		case TKTX_R32G32_SINT: FT(RG_INTEGER, INT, RG32I, 4)
		case TKTX_R32G32_SFLOAT: FT(RG, FLOAT, RG32F, 4)

		case TKTX_R32G32B32_UINT: FT(RGB_INTEGER, UNSIGNED_INT, RGB32UI, 4)
		case TKTX_R32G32B32_SINT: FT(RGB_INTEGER, INT, RGB32I, 4)
		case TKTX_R32G32B32_SFLOAT: FT(RGB_INTEGER, FLOAT, RGB32F, 4)

		case TKTX_R32G32B32A32_UINT: FT(RGBA_INTEGER, UNSIGNED_INT, RGBA32UI, 4)
		case TKTX_R32G32B32A32_SINT: FT(RGBA_INTEGER, INT, RGBA32I, 4)
		case TKTX_R32G32B32A32_SFLOAT:FT(RGBA, FLOAT, RGBA32F, 4)

		case TKTX_BC1_RGB_UNORM_BLOCK: FTC(RGB, RGB_S3TC_DXT1)
		case TKTX_BC1_RGB_SRGB_BLOCK: FTC(RGB, SRGB_S3TC_DXT1)
		case TKTX_BC1_RGBA_UNORM_BLOCK: FTC(RGBA, RGBA_S3TC_DXT1)
		case TKTX_BC1_RGBA_SRGB_BLOCK: FTC(RGBA, SRGB_ALPHA_S3TC_DXT1)
		case TKTX_BC2_UNORM_BLOCK: FTC(RGBA, RGBA_S3TC_DXT3)
		case TKTX_BC2_SRGB_BLOCK: FTC(RGBA, SRGB_ALPHA_S3TC_DXT3)
		case TKTX_BC3_UNORM_BLOCK: FTC(RGBA, RGBA_S3TC_DXT5)
		case TKTX_BC3_SRGB_BLOCK: FTC(RGBA, SRGB_ALPHA_S3TC_DXT5)
		case TKTX_BC4_UNORM_BLOCK: FTC(RED, RED_RGTC1)
		case TKTX_BC4_SNORM_BLOCK: FTC(RED, SIGNED_RED_RGTC1)
		case TKTX_BC5_UNORM_BLOCK: FTC(RG, RED_GREEN_RGTC2)
		case TKTX_BC5_SNORM_BLOCK: FTC(RG, SIGNED_RED_GREEN_RGTC2)
		case TKTX_BC6H_UFLOAT_BLOCK: FTC(RGB, RGB_BPTC_UNSIGNED_FLOAT)
		case TKTX_BC6H_SFLOAT_BLOCK: FTC(RGB, RGB_BPTC_SIGNED_FLOAT)
		case TKTX_BC7_UNORM_BLOCK: FTC(RGBA, RGBA_BPTC_UNORM)
		case TKTX_BC7_SRGB_BLOCK: FTC(RGBA, SRGB_ALPHA_BPTC_UNORM)

		case TKTX_ETC2_R8G8B8_UNORM_BLOCK: FTC(RGB, RGB8_ETC2)
		case TKTX_ETC2_R8G8B8A1_UNORM_BLOCK: FTC(RGBA, RGB8_PUNCHTHROUGH_ALPHA1_ETC2)
		case TKTX_ETC2_R8G8B8A8_UNORM_BLOCK: FTC(RGBA, RGBA8_ETC2_EAC)
		case TKTX_ETC2_R8G8B8_SRGB_BLOCK: FTC(SRGB, SRGB8_ETC2)
		case TKTX_ETC2_R8G8B8A1_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_PUNCHTHROUGH_ALPHA1_ETC2)
		case TKTX_ETC2_R8G8B8A8_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ETC2_EAC)
		case TKTX_EAC_R11_UNORM_BLOCK: FTC(RED, R11_EAC)
		case TKTX_EAC_R11G11_UNORM_BLOCK: FTC(RG, RG11_EAC)
		case TKTX_EAC_R11_SNORM_BLOCK: FTC(RED, SIGNED_R11_EAC)
		case TKTX_EAC_R11G11_SNORM_BLOCK: FTC(RG, SIGNED_RG11_EAC)

		case TKTX_PVR_2BPP_UNORM_BLOCK: FTC(RGB, RGB_PVRTC_2BPPV1)
		case TKTX_PVR_2BPPA_UNORM_BLOCK: FTC(RGBA, RGBA_PVRTC_2BPPV1);
		case TKTX_PVR_4BPP_UNORM_BLOCK: FTC(RGB, RGB_PVRTC_4BPPV1)
		case TKTX_PVR_4BPPA_UNORM_BLOCK: FTC(RGB, RGB_PVRTC_4BPPV1)
		case TKTX_PVR_2BPP_SRGB_BLOCK: FTC(SRGB, SRGB_PVRTC_2BPPV1)
		case TKTX_PVR_2BPPA_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB_ALPHA_PVRTC_2BPPV1);
		case TKTX_PVR_4BPP_SRGB_BLOCK: FTC(SRGB, SRGB_PVRTC_2BPPV1)
		case TKTX_PVR_4BPPA_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB_ALPHA_PVRTC_2BPPV1);

		case TKTX_ASTC_4x4_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_4x4)
		case TKTX_ASTC_4x4_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_4x4)
		case TKTX_ASTC_5x4_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_5x4)
		case TKTX_ASTC_5x4_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_5x4)
		case TKTX_ASTC_5x5_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_5x5)
		case TKTX_ASTC_5x5_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_5x5)
		case TKTX_ASTC_6x5_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_6x5)
		case TKTX_ASTC_6x5_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_6x5)
		case TKTX_ASTC_6x6_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_6x6)
		case TKTX_ASTC_6x6_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_6x6)
		case TKTX_ASTC_8x5_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_8x5)
		case TKTX_ASTC_8x5_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_8x5)
		case TKTX_ASTC_8x6_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_8x6)
		case TKTX_ASTC_8x6_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_8x6)
		case TKTX_ASTC_8x8_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_8x8)
		case TKTX_ASTC_8x8_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_8x8)
		case TKTX_ASTC_10x5_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_10x5)
		case TKTX_ASTC_10x5_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_10x5)
		case TKTX_ASTC_10x6_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_10x6)
		case TKTX_ASTC_10x6_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_10x6)
		case TKTX_ASTC_10x8_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_10x8);
		case TKTX_ASTC_10x8_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_10x8)
		case TKTX_ASTC_10x10_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_10x10)
		case TKTX_ASTC_10x10_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_10x10)
		case TKTX_ASTC_12x10_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_12x10)
		case TKTX_ASTC_12x10_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_12x10)
		case TKTX_ASTC_12x12_UNORM_BLOCK: FTC(RGBA, RGBA_ASTC_12x12)
		case TKTX_ASTC_12x12_SRGB_BLOCK: FTC(SRGB_ALPHA, SRGB8_ALPHA8_ASTC_12x12)

		default:break;
		}
		return false;
	}
#undef FT
#undef FTC

	TinyKtx_Format TinyKtx_CrackFormatFromGL(uint32_t const glformat,
		uint32_t const gltype,
		uint32_t const glinternalformat,
		uint32_t const typesize) {
		switch (glinternalformat) {
		case TINYKTX_GL_COMPRESSED_RGB_S3TC_DXT1: return TKTX_BC1_RGB_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_S3TC_DXT1: return TKTX_BC1_RGBA_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_S3TC_DXT3: return TKTX_BC2_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_S3TC_DXT5: return TKTX_BC3_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_3DC_X_AMD: return TKTX_BC4_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_3DC_XY_AMD: return TKTX_BC5_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB_PVRTC_2BPPV1: return TKTX_PVR_2BPP_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB_PVRTC_4BPPV1: return TKTX_PVR_2BPPA_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1: return TKTX_PVR_2BPPA_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1: return TKTX_PVR_4BPPA_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGB_PVRTC_4BPPV1: return TKTX_PVR_4BPP_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGB_PVRTC_2BPPV1: return TKTX_PVR_2BPP_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_PVRTC_4BPPV1: return TKTX_PVR_4BPPA_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_PVRTC_2BPPV1: return TKTX_PVR_2BPPA_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB_S3TC_DXT1: return TKTX_BC1_RGB_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1: return TKTX_BC1_RGBA_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3: return TKTX_BC2_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5: return TKTX_BC3_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_LUMINANCE_LATC1: return TKTX_BC4_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_SIGNED_LUMINANCE_LATC1: return TKTX_BC4_SNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_LUMINANCE_ALPHA_LATC2: return TKTX_BC5_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2: return TKTX_BC5_SNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RED_RGTC1: return TKTX_BC4_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_SIGNED_RED_RGTC1: return TKTX_BC4_SNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RED_GREEN_RGTC2: return TKTX_BC5_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2: return TKTX_BC5_SNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_ETC1_RGB8_OES: return TKTX_ETC2_R8G8B8_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_BPTC_UNORM: return TKTX_BC7_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM: return TKTX_BC7_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT: return TKTX_BC6H_SFLOAT_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT: return TKTX_BC6H_UFLOAT_BLOCK;
		case TINYKTX_GL_COMPRESSED_R11_EAC: return TKTX_EAC_R11_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_SIGNED_R11_EAC: return TKTX_EAC_R11_SNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RG11_EAC: return TKTX_EAC_R11G11_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_SIGNED_RG11_EAC: return TKTX_EAC_R11G11_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGB8_ETC2: return TKTX_ETC2_R8G8B8_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ETC2: return TKTX_ETC2_R8G8B8_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2: return TKTX_ETC2_R8G8B8A1_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2: return TKTX_ETC2_R8G8B8A1_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA8_ETC2_EAC: return TKTX_ETC2_R8G8B8A8_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC: return TKTX_ETC2_R8G8B8A8_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_ASTC_4x4: return TKTX_ASTC_4x4_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_ASTC_5x4: return TKTX_ASTC_5x4_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_ASTC_5x5: return TKTX_ASTC_5x5_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_ASTC_6x5: return TKTX_ASTC_6x5_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_ASTC_6x6: return TKTX_ASTC_6x6_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_ASTC_8x5: return TKTX_ASTC_8x5_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_ASTC_8x6: return TKTX_ASTC_8x6_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_ASTC_8x8: return TKTX_ASTC_8x8_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x5: return TKTX_ASTC_10x5_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x6: return TKTX_ASTC_10x6_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x8: return TKTX_ASTC_10x8_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_ASTC_10x10: return TKTX_ASTC_10x10_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_ASTC_12x10: return TKTX_ASTC_12x10_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_RGBA_ASTC_12x12: return TKTX_ASTC_12x12_UNORM_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4: return TKTX_ASTC_4x4_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4: return TKTX_ASTC_5x4_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5: return TKTX_ASTC_5x5_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5: return TKTX_ASTC_6x5_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6: return TKTX_ASTC_6x6_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5: return TKTX_ASTC_8x5_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6: return TKTX_ASTC_8x6_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8: return TKTX_ASTC_8x8_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5: return TKTX_ASTC_10x5_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6: return TKTX_ASTC_10x6_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8: return TKTX_ASTC_10x8_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10: return TKTX_ASTC_10x10_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10: return TKTX_ASTC_12x10_SRGB_BLOCK;
		case TINYKTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12: return TKTX_ASTC_12x12_SRGB_BLOCK;

			// non compressed
		case TINYKTX_GL_INTFORMAT_RGB4:
			if (glformat == TINYKTX_GL_FORMAT_RG) {
				return TKTX_R4G4_UNORM_PACK8;
			}
			break;

		case TINYKTX_GL_INTFORMAT_RGBA4:
			if (glformat == TINYKTX_GL_FORMAT_RGBA) {
				return TKTX_R4G4B4A4_UNORM_PACK16;
			} else if (glformat == TINYKTX_GL_FORMAT_BGRA) {
				return TKTX_B4G4R4A4_UNORM_PACK16;
			}
			break;
		case TINYKTX_GL_INTFORMAT_RGB565:
			if (gltype == TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_6_5 || glformat == TINYKTX_GL_FORMAT_BGR) {
				return TKTX_B5G6R5_UNORM_PACK16;
			} else {
				return TKTX_R5G6B5_UNORM_PACK16;
			}
			break;

		case TINYKTX_GL_INTFORMAT_RGB5_A1:
			if (gltype == TINYKTX_GL_TYPE_UNSIGNED_SHORT_1_5_5_5_REV) {
				return TKTX_A1R5G5B5_UNORM_PACK16;
			} else if (gltype == TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_5_5_1) {
				if (glformat == TINYKTX_GL_FORMAT_BGRA) {
					return TKTX_B5G5R5A1_UNORM_PACK16;
				} else {
					return TKTX_R5G5B5A1_UNORM_PACK16;
				}
			}
			break;
		case TINYKTX_GL_INTFORMAT_R8_SNORM:
		case TINYKTX_GL_INTFORMAT_R8:
			if (gltype == TINYKTX_GL_TYPE_BYTE || glinternalformat == TINYKTX_GL_INTFORMAT_R8_SNORM) {
				return TKTX_R8_SNORM;
			} else {
				return TKTX_R8_UNORM;
			}
			break;
		case TINYKTX_GL_INTFORMAT_RG8_SNORM:
		case TINYKTX_GL_INTFORMAT_RG8:
			if (gltype == TINYKTX_GL_TYPE_BYTE ||
				glinternalformat == TINYKTX_GL_INTFORMAT_RG8_SNORM) {
				return TKTX_R8G8_SNORM;
			} else {
				return TKTX_R8G8_UNORM;
			}
			break;
		case TINYKTX_GL_INTFORMAT_RGB8_SNORM:
		case TINYKTX_GL_INTFORMAT_RGB8:
			if (glformat == TINYKTX_GL_FORMAT_BGR) {
				if (gltype == TINYKTX_GL_TYPE_BYTE ||
					glinternalformat == TINYKTX_GL_INTFORMAT_RGB8_SNORM) {
					return TKTX_B8G8R8_SNORM;
				} else {
					return TKTX_B8G8R8_UNORM;
				}
			} else {
				if (gltype == TINYKTX_GL_TYPE_BYTE ||
					glinternalformat == TINYKTX_GL_INTFORMAT_RGB8_SNORM) {
					return TKTX_R8G8B8_SNORM;
				} else {
					return TKTX_R8G8B8_UNORM;
				}
			}
			break;
		case TINYKTX_GL_INTFORMAT_RGBA8_SNORM:
		case TINYKTX_GL_INTFORMAT_RGBA8:
			if (glformat == TINYKTX_GL_FORMAT_RGBA) {
				if (gltype == TINYKTX_GL_TYPE_BYTE ||
					glinternalformat == TINYKTX_GL_INTFORMAT_RGBA8_SNORM) {
					return TKTX_R8G8B8A8_SNORM;
				} else if (gltype == TINYKTX_GL_TYPE_UNSIGNED_BYTE) {
					return TKTX_R8G8B8A8_UNORM;
				}
			} else if (glformat == TINYKTX_GL_FORMAT_BGRA) {
				if (gltype == TINYKTX_GL_TYPE_BYTE ||
					glinternalformat == TINYKTX_GL_INTFORMAT_RGBA8_SNORM) {
					return TKTX_B8G8R8A8_SNORM;
				} else if (gltype == TINYKTX_GL_TYPE_UNSIGNED_BYTE) {
					return TKTX_B8G8R8A8_UNORM;
				}
			} else if (glformat == TINYKTX_GL_FORMAT_ABGR) {
				if (gltype == TINYKTX_GL_TYPE_BYTE ||
					glinternalformat == TINYKTX_GL_INTFORMAT_RGBA8_SNORM) {
					return TKTX_A8B8G8R8_SNORM_PACK32;
				} else if (gltype == TINYKTX_GL_TYPE_UNSIGNED_BYTE) {
					return TKTX_A8B8G8R8_UNORM_PACK32;
				}
			}
			break;

		case TINYKTX_GL_INTFORMAT_R16: return TKTX_R16_UNORM;
		case TINYKTX_GL_INTFORMAT_RG16: return TKTX_R16G16_UNORM;
		case TINYKTX_GL_INTFORMAT_RGB16: return TKTX_R16G16B16_UNORM;
		case TINYKTX_GL_INTFORMAT_RGBA16: return TKTX_R16G16B16A16_UNORM;

		case TINYKTX_GL_INTFORMAT_R16_SNORM:return TKTX_R16_SNORM;
		case TINYKTX_GL_INTFORMAT_RG16_SNORM: return TKTX_R16G16_SNORM;
		case TINYKTX_GL_INTFORMAT_RGB16_SNORM: return TKTX_R16G16B16_SNORM;
		case TINYKTX_GL_INTFORMAT_RGBA16_SNORM: return TKTX_R16G16B16A16_SNORM;

		case TINYKTX_GL_INTFORMAT_R8I: return TKTX_R8_SINT;
		case TINYKTX_GL_INTFORMAT_RG8I: return TKTX_R8G8_SINT;
		case TINYKTX_GL_INTFORMAT_RGB8I:
			if (glformat == TINYKTX_GL_FORMAT_RGB || glformat == TINYKTX_GL_FORMAT_RGB_INTEGER) {
				return TKTX_R8G8B8_SINT;
			} else if (glformat == TINYKTX_GL_FORMAT_BGR || glformat == TINYKTX_GL_FORMAT_BGR_INTEGER) {
				return TKTX_B8G8R8_SINT;
			}
			break;

		case TINYKTX_GL_INTFORMAT_RGBA8I:
			if (glformat == TINYKTX_GL_FORMAT_RGBA || glformat == TINYKTX_GL_FORMAT_RGBA_INTEGER) {
				return TKTX_R8G8B8A8_SINT;
			} else if (glformat == TINYKTX_GL_FORMAT_BGRA || glformat == TINYKTX_GL_FORMAT_BGRA_INTEGER) {
				return TKTX_B8G8R8A8_SINT;
			} else if (glformat == TINYKTX_GL_FORMAT_ABGR) {
				return TKTX_A8B8G8R8_SINT_PACK32;
			}
			break;

		case TINYKTX_GL_INTFORMAT_R16I: return TKTX_R16_SINT;
		case TINYKTX_GL_INTFORMAT_RG16I: return TKTX_R16G16_SINT;
		case TINYKTX_GL_INTFORMAT_RGB16I:return TKTX_R16G16B16_SINT;
		case TINYKTX_GL_INTFORMAT_RGBA16I:return TKTX_R16G16B16A16_SINT;

		case TINYKTX_GL_INTFORMAT_R32I: return TKTX_R32_SINT;
		case TINYKTX_GL_INTFORMAT_RG32I: return TKTX_R32G32_SINT;
		case TINYKTX_GL_INTFORMAT_RGB32I:
			if (glformat == TINYKTX_GL_FORMAT_RGB || glformat == TINYKTX_GL_FORMAT_RGB_INTEGER) {
				return TKTX_R32G32B32_SINT;
			}
			break;
		case TINYKTX_GL_INTFORMAT_RGBA32I:return TKTX_R32G32B32A32_SINT;

		case TINYKTX_GL_INTFORMAT_R8UI: return TKTX_R8_UINT;
		case TINYKTX_GL_INTFORMAT_RG8UI: return TKTX_R8G8_UINT;
		case TINYKTX_GL_INTFORMAT_RGB8UI:
			if (glformat == TINYKTX_GL_FORMAT_RGB || glformat == TINYKTX_GL_FORMAT_RGB_INTEGER) {
				return TKTX_R8G8B8_UINT;
			} else if (glformat == TINYKTX_GL_FORMAT_BGR || glformat == TINYKTX_GL_FORMAT_BGR_INTEGER) {
				return TKTX_B8G8R8_UINT;
			}
			break;
		case TINYKTX_GL_INTFORMAT_RGBA8UI:
			if (glformat == TINYKTX_GL_FORMAT_RGBA || glformat == TINYKTX_GL_FORMAT_RGBA_INTEGER) {
				return TKTX_R8G8B8A8_UINT;
			} else if (glformat == TINYKTX_GL_FORMAT_BGRA || glformat == TINYKTX_GL_FORMAT_BGRA_INTEGER) {
				return TKTX_B8G8R8A8_UINT;
			} else if (glformat == TINYKTX_GL_FORMAT_ABGR) {
				return TKTX_A8B8G8R8_UINT_PACK32;
			}
			break;

		case TINYKTX_GL_INTFORMAT_R16UI: return TKTX_R16_UINT;
		case TINYKTX_GL_INTFORMAT_RG16UI: return TKTX_R16G16_UINT;
		case TINYKTX_GL_INTFORMAT_RGB16UI:return TKTX_R16G16B16_UINT;
		case TINYKTX_GL_INTFORMAT_RGBA16UI: return TKTX_R16G16B16A16_UINT;

		case TINYKTX_GL_INTFORMAT_R32UI: return TKTX_R32_UINT;
		case TINYKTX_GL_INTFORMAT_RG32UI: return TKTX_R32G32_UINT;
		case TINYKTX_GL_INTFORMAT_RGB32UI: return TKTX_R32G32B32_UINT;
		case TINYKTX_GL_INTFORMAT_RGBA32UI: return TKTX_R32G32B32A32_UINT;

		case TINYKTX_GL_INTFORMAT_R16F: return TKTX_R16_SFLOAT;
		case TINYKTX_GL_INTFORMAT_RG16F: return TKTX_R16G16_SFLOAT;
		case TINYKTX_GL_INTFORMAT_RGB16F: return TKTX_R16G16B16_SFLOAT;
		case TINYKTX_GL_INTFORMAT_RGBA16F: return TKTX_R16G16B16A16_SFLOAT;

		case TINYKTX_GL_INTFORMAT_R32F: return TKTX_R32_SFLOAT;
		case TINYKTX_GL_INTFORMAT_RG32F: return TKTX_R32G32_SFLOAT;
		case TINYKTX_GL_INTFORMAT_RGB32F: return TKTX_R32G32B32_SFLOAT;
		case TINYKTX_GL_INTFORMAT_RGBA32F: return TKTX_R32G32B32A32_SFLOAT;

		case TINYKTX_GL_INTFORMAT_R11F_G11F_B10F: return TKTX_B10G11R11_UFLOAT_PACK32; //??
		case TINYKTX_GL_INTFORMAT_UNSIGNED_INT_10F_11F_11F_REV: return TKTX_B10G11R11_UFLOAT_PACK32; //?
		case TINYKTX_GL_INTFORMAT_RGB9_E5: return TKTX_E5B9G9R9_UFLOAT_PACK32;
		case TINYKTX_GL_INTFORMAT_SLUMINANCE8_ALPHA8: return TKTX_R8G8_SRGB;
		case TINYKTX_GL_INTFORMAT_SLUMINANCE8: return TKTX_R8_SRGB;

		case TINYKTX_GL_INTFORMAT_ALPHA8: return TKTX_R8_UNORM;
		case TINYKTX_GL_INTFORMAT_ALPHA16: return TKTX_R16_UNORM;
		case TINYKTX_GL_INTFORMAT_LUMINANCE8: return TKTX_R8_UNORM;
		case TINYKTX_GL_INTFORMAT_LUMINANCE16: return TKTX_R16_UNORM;
		case TINYKTX_GL_INTFORMAT_LUMINANCE8_ALPHA8: return TKTX_R8G8_UNORM;
		case TINYKTX_GL_INTFORMAT_LUMINANCE16_ALPHA16: return TKTX_R16G16_UNORM;
		case TINYKTX_GL_INTFORMAT_INTENSITY8: return TKTX_R8_UNORM;
		case TINYKTX_GL_INTFORMAT_INTENSITY16: return TKTX_R16_UNORM;

		case TINYKTX_GL_INTFORMAT_ALPHA8_SNORM: return TKTX_R8_SNORM;
		case TINYKTX_GL_INTFORMAT_LUMINANCE8_SNORM: return TKTX_R8_SNORM;
		case TINYKTX_GL_INTFORMAT_LUMINANCE8_ALPHA8_SNORM: return TKTX_R8G8_SNORM;
		case TINYKTX_GL_INTFORMAT_INTENSITY8_SNORM: return TKTX_R8_SNORM;
		case TINYKTX_GL_INTFORMAT_ALPHA16_SNORM: return TKTX_R16_SNORM;
		case TINYKTX_GL_INTFORMAT_LUMINANCE16_SNORM: return TKTX_R16_SNORM;
		case TINYKTX_GL_INTFORMAT_LUMINANCE16_ALPHA16_SNORM: return TKTX_R16G16_SNORM;
		case TINYKTX_GL_INTFORMAT_INTENSITY16_SNORM: return TKTX_R16_SNORM;

		case TINYKTX_GL_INTFORMAT_SRGB8:
		case TINYKTX_GL_INTFORMAT_SRGB8_ALPHA8:
			if (glformat == TINYKTX_GL_FORMAT_SLUMINANCE) {
				return TKTX_R8_SRGB;
			} else if (glformat == TINYKTX_GL_FORMAT_SLUMINANCE_ALPHA) {
				return TKTX_R8G8_SRGB;
			} else if (glformat == TINYKTX_GL_FORMAT_SRGB) {
				return TKTX_R8G8B8_SRGB;
			} else if (glformat == TINYKTX_GL_FORMAT_SRGB_ALPHA) {
				return TKTX_R8G8B8A8_SRGB;
			} else if (glformat == TINYKTX_GL_FORMAT_BGR) {
				return TKTX_B8G8R8_SRGB;
			} else if (glformat == TINYKTX_GL_FORMAT_BGRA) {
				return TKTX_B8G8R8A8_SRGB;
			} else if (glformat == TINYKTX_GL_FORMAT_ABGR) {
				return TKTX_A8B8G8R8_SRGB_PACK32;
			}
			break;
		case TINYKTX_GL_INTFORMAT_RGB10_A2:
			if (glformat == TINYKTX_GL_FORMAT_BGRA) {
				return TKTX_A2R10G10B10_UNORM_PACK32;
			} else if (glformat == TINYKTX_GL_FORMAT_RGBA) {
				return TKTX_A2B10G10R10_UNORM_PACK32;
			} else		if (glformat == TINYKTX_GL_FORMAT_BGRA_INTEGER) {
				return TKTX_A2R10G10B10_UINT_PACK32;
			} else if (glformat == TINYKTX_GL_FORMAT_RGBA_INTEGER) {
				return TKTX_A2B10G10R10_UINT_PACK32;
			}
			break;

			// apparently we get FORMAT formats in the internal format slot sometimes
		case TINYKTX_GL_FORMAT_RED:
			switch (gltype) {
			case TINYKTX_GL_TYPE_BYTE: return TKTX_R8_SNORM;
			case TINYKTX_GL_TYPE_UNSIGNED_BYTE: return TKTX_R8_UNORM;
			case TINYKTX_GL_TYPE_SHORT: return TKTX_R16_SNORM;
			case TINYKTX_GL_TYPE_UNSIGNED_SHORT:return TKTX_R16_UNORM;
			case TINYKTX_GL_TYPE_INT: return TKTX_R32_SINT;
			case TINYKTX_GL_TYPE_UNSIGNED_INT: return TKTX_R32_UINT;
			case TINYKTX_GL_TYPE_FLOAT: return TKTX_R32_SFLOAT;
				//	case TINYKTX_GL_TYPE_DOUBLE:				return TKTX_R64G64B64A64_SFLOAT;
			case TINYKTX_GL_TYPE_HALF_FLOAT: return TKTX_R16_SFLOAT;
			default: return TKTX_UNDEFINED;
			}
		case TINYKTX_GL_FORMAT_RG:
			switch (gltype) {
			case TINYKTX_GL_TYPE_BYTE: return TKTX_R8G8_SNORM;
			case TINYKTX_GL_TYPE_UNSIGNED_BYTE: return TKTX_R8G8_UNORM;
			case TINYKTX_GL_TYPE_SHORT: return TKTX_R16G16_SNORM;
			case TINYKTX_GL_TYPE_UNSIGNED_SHORT:return TKTX_R16G16_UNORM;
			case TINYKTX_GL_TYPE_INT: return TKTX_R32G32_SINT;
			case TINYKTX_GL_TYPE_UNSIGNED_INT: return TKTX_R32G32_UINT;
			case TINYKTX_GL_TYPE_FLOAT: return TKTX_R32G32_SFLOAT;
				//	case TINYKTX_GL_TYPE_DOUBLE:				return TKTX_R64G64_SFLOAT;
			case TINYKTX_GL_TYPE_HALF_FLOAT: return TKTX_R16G16_SFLOAT;
			default: return TKTX_UNDEFINED;
			}

		case TINYKTX_GL_FORMAT_RGB:
			switch (gltype) {
			case TINYKTX_GL_TYPE_BYTE: return TKTX_R8G8B8_SNORM;
			case TINYKTX_GL_TYPE_UNSIGNED_BYTE: return TKTX_R8G8B8_UNORM;
			case TINYKTX_GL_TYPE_SHORT: return TKTX_R16G16B16_SNORM;
			case TINYKTX_GL_TYPE_UNSIGNED_SHORT:return TKTX_R16G16B16_UNORM;
			case TINYKTX_GL_TYPE_INT: return TKTX_R32G32B32_SINT;
			case TINYKTX_GL_TYPE_UNSIGNED_INT: return TKTX_R32G32B32_UINT;
			case TINYKTX_GL_TYPE_FLOAT: return TKTX_R32G32B32_SFLOAT;
				//	case TINYKTX_GL_TYPE_DOUBLE:				return TKTX_R64G64B64_SFLOAT;
			case TINYKTX_GL_TYPE_HALF_FLOAT: return TKTX_R16G16B16_SFLOAT;
			default: return TKTX_UNDEFINED;
			}
		case TINYKTX_GL_FORMAT_RGBA:
			switch (gltype) {
			case TINYKTX_GL_TYPE_BYTE: return TKTX_R8G8B8A8_SNORM;
			case TINYKTX_GL_TYPE_UNSIGNED_BYTE: return TKTX_R8G8B8A8_UNORM;
			case TINYKTX_GL_TYPE_SHORT: return TKTX_R16G16B16A16_SNORM;
			case TINYKTX_GL_TYPE_UNSIGNED_SHORT:return TKTX_R16G16B16A16_UNORM;
			case TINYKTX_GL_TYPE_INT: return TKTX_R32G32B32A32_SINT;
			case TINYKTX_GL_TYPE_UNSIGNED_INT: return TKTX_R32G32B32A32_UINT;
			case TINYKTX_GL_TYPE_FLOAT: return TKTX_R32G32B32A32_SFLOAT;
				//	case TINYKTX_GL_TYPE_DOUBLE:				return TKTX_R64G64B64A64_SFLOAT;
			case TINYKTX_GL_TYPE_HALF_FLOAT: return TKTX_R16G16B16A16_SFLOAT;
			default: return TKTX_UNDEFINED;
			}

			// can't handle yet
		case TINYKTX_GL_INTFORMAT_ALPHA4:
		case TINYKTX_GL_INTFORMAT_ALPHA12:
		case TINYKTX_GL_INTFORMAT_LUMINANCE4:
		case TINYKTX_GL_INTFORMAT_LUMINANCE12:
		case TINYKTX_GL_INTFORMAT_LUMINANCE4_ALPHA4:
		case TINYKTX_GL_INTFORMAT_LUMINANCE6_ALPHA2:
		case TINYKTX_GL_INTFORMAT_LUMINANCE12_ALPHA4:
		case TINYKTX_GL_INTFORMAT_LUMINANCE12_ALPHA12:
		case TINYKTX_GL_INTFORMAT_INTENSITY4:
		case TINYKTX_GL_INTFORMAT_INTENSITY12:
		case TINYKTX_GL_INTFORMAT_RGB2:
		case TINYKTX_GL_INTFORMAT_RGB5:
		case TINYKTX_GL_INTFORMAT_RGB10:
		case TINYKTX_GL_INTFORMAT_RGB12:
		case TINYKTX_GL_INTFORMAT_RGBA2:
		case TINYKTX_GL_INTFORMAT_RGBA12:
		case TINYKTX_GL_INTFORMAT_FLOAT_32_UNSIGNED_INT_24_8_REV:
		case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV2:
		case TINYKTX_GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV2:
		case TINYKTX_GL_COMPRESSED_ATC_RGB:
		case TINYKTX_GL_COMPRESSED_ATC_RGBA_EXPLICIT_ALPHA:
		case TINYKTX_GL_COMPRESSED_ATC_RGBA_INTERPOLATED_ALPHA:
		default: break;
		}
		return TKTX_UNDEFINED;
	}

	uint32_t TinyKtx_ElementCountFromGLFormat(uint32_t fmt) {
		switch (fmt) {
		case TINYKTX_GL_FORMAT_RED:
		case TINYKTX_GL_FORMAT_GREEN:
		case TINYKTX_GL_FORMAT_BLUE:
		case TINYKTX_GL_FORMAT_ALPHA:
		case TINYKTX_GL_FORMAT_LUMINANCE:
		case TINYKTX_GL_FORMAT_INTENSITY:
		case TINYKTX_GL_FORMAT_RED_INTEGER:
		case TINYKTX_GL_FORMAT_GREEN_INTEGER:
		case TINYKTX_GL_FORMAT_BLUE_INTEGER:
		case TINYKTX_GL_FORMAT_ALPHA_INTEGER:
		case TINYKTX_GL_FORMAT_SLUMINANCE:
		case TINYKTX_GL_FORMAT_RED_SNORM:
			return 1;

		case TINYKTX_GL_FORMAT_RG_INTEGER:
		case TINYKTX_GL_FORMAT_RG:
		case TINYKTX_GL_FORMAT_LUMINANCE_ALPHA:
		case TINYKTX_GL_FORMAT_SLUMINANCE_ALPHA:
		case TINYKTX_GL_FORMAT_RG_SNORM:
			return 2;

		case TINYKTX_GL_FORMAT_BGR:
		case TINYKTX_GL_FORMAT_RGB:
		case TINYKTX_GL_FORMAT_SRGB:
		case TINYKTX_GL_FORMAT_RGB_INTEGER:
		case TINYKTX_GL_FORMAT_BGR_INTEGER:
		case TINYKTX_GL_FORMAT_RGB_SNORM:
			return 3;

		case TINYKTX_GL_FORMAT_BGRA:
		case TINYKTX_GL_FORMAT_RGBA:
		case TINYKTX_GL_FORMAT_ABGR:
		case TINYKTX_GL_FORMAT_SRGB_ALPHA:
		case TINYKTX_GL_FORMAT_RGBA_INTEGER:
		case TINYKTX_GL_FORMAT_BGRA_INTEGER:
		case TINYKTX_GL_FORMAT_RGBA_SNORM:
			return 4;
		}

		return 0;
	}
	bool TinyKtx_ByteDividableFromGLType(uint32_t type) {
		switch (type) {
		case TINYKTX_GL_TYPE_COMPRESSED:
		case TINYKTX_GL_TYPE_UNSIGNED_BYTE_3_3_2:
		case TINYKTX_GL_TYPE_UNSIGNED_SHORT_4_4_4_4:
		case TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_5_5_1:
		case TINYKTX_GL_TYPE_UNSIGNED_INT_8_8_8_8:
		case TINYKTX_GL_TYPE_UNSIGNED_INT_10_10_10_2:
		case TINYKTX_GL_TYPE_UNSIGNED_BYTE_2_3_3_REV:
		case TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_6_5:
		case TINYKTX_GL_TYPE_UNSIGNED_SHORT_5_6_5_REV:
		case TINYKTX_GL_TYPE_UNSIGNED_SHORT_4_4_4_4_REV:
		case TINYKTX_GL_TYPE_UNSIGNED_SHORT_1_5_5_5_REV:
		case TINYKTX_GL_TYPE_UNSIGNED_INT_8_8_8_8_REV:
		case TINYKTX_GL_TYPE_UNSIGNED_INT_2_10_10_10_REV:
		case TINYKTX_GL_TYPE_UNSIGNED_INT_24_8:
		case TINYKTX_GL_TYPE_UNSIGNED_INT_5_9_9_9_REV:
		case TINYKTX_GL_TYPE_UNSIGNED_INT_10F_11F_11F_REV:
		case TINYKTX_GL_TYPE_FLOAT_32_UNSIGNED_INT_24_8_REV:
			return false;
		case TINYKTX_GL_TYPE_BYTE:
		case TINYKTX_GL_TYPE_UNSIGNED_BYTE:
		case TINYKTX_GL_TYPE_SHORT:
		case TINYKTX_GL_TYPE_UNSIGNED_SHORT:
		case TINYKTX_GL_TYPE_INT:
		case TINYKTX_GL_TYPE_UNSIGNED_INT:
		case TINYKTX_GL_TYPE_FLOAT:
		case TINYKTX_GL_TYPE_DOUBLE:
		case TINYKTX_GL_TYPE_HALF_FLOAT:
			return true;
		}
		return false;
	}
	TinyKtx_Format TinyKtx_GetFormat(TinyKtx_ContextHandle handle) {
		uint32_t glformat;
		uint32_t gltype;
		uint32_t glinternalformat;
		uint32_t typesize;
		uint32_t glbaseinternalformat;

		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return TKTX_UNDEFINED;

		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return TKTX_UNDEFINED;
		}

		if (TinyKtx_GetFormatGL(handle, &glformat, &gltype, &glinternalformat, &typesize, &glbaseinternalformat) == false)
			return TKTX_UNDEFINED;

		return TinyKtx_CrackFormatFromGL(glformat, gltype, glinternalformat, typesize);
	}
	static uint32_t TinyKtx_MipMapReduce(uint32_t value, uint32_t mipmaplevel) {

		// handle 0 being passed in
		if (value <= 1) return 1;

		// there are better ways of doing this (log2 etc.) but this doesn't require any
		// dependecies and isn't used enough to matter imho
		for (uint32_t i = 0u; i < mipmaplevel; ++i) {
			if (value <= 1) return 1;
			value = value / 2;
		}
		return value;
	}

	// KTX specifys GL_UNPACK_ALIGNMENT = 4 which means some files have unexpected padding
	// that probably means you can't just memcpy the data out if you aren't using a GL
	// texture with GL_UNPACK_ALIGNMENT of 4
	// this will be true if this mipmap level is 'unpacked' so has padding on each row
	// you will need to handle.
	bool TinyKtx_IsMipMapLevelUnpacked(TinyKtx_ContextHandle handle, uint32_t mipmaplevel) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return false;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return false;
		}

		if (ctx->header.glTypeSize < 4 &&
			TinyKtx_ByteDividableFromGLType(ctx->header.glType)) {

			uint32_t const s = ctx->header.glTypeSize;
			uint32_t const n = TinyKtx_ElementCountFromGLFormat(ctx->header.glFormat);
			if (n == 0) {
				ctx->callbacks.errorFn(ctx->user, "TinyKtx_ElementCountFromGLFormat error");
				return false;
			}

			uint32_t const w = TinyKtx_MipMapReduce(ctx->header.pixelWidth, mipmaplevel);
			uint32_t const snl = s * n * w;
			uint32_t const k = ((snl + 3u) & ~3u);

			if (k != snl) {
				return true;
			}
		}
		return false;
	}
	uint32_t TinyKtx_UnpackedRowStride(TinyKtx_ContextHandle handle, uint32_t mipmaplevel) {
		TinyKtx_Context *ctx = (TinyKtx_Context *)handle;
		if (ctx == NULL)
			return 0;
		if (ctx->headerValid == false) {
			ctx->callbacks.errorFn(ctx->user, "Header data hasn't been read yet or its invalid");
			return 0;
		}

		if (ctx->header.glTypeSize < 4 &&
			TinyKtx_ByteDividableFromGLType(ctx->header.glType)) {

			uint32_t const s = ctx->header.glTypeSize;
			uint32_t const n = TinyKtx_ElementCountFromGLFormat(ctx->header.glFormat);
			if (n == 0) {
				ctx->callbacks.errorFn(ctx->user, "TinyKtx_ElementCountFromGLFormat error");
				return 0;
			}

			uint32_t const w = TinyKtx_MipMapReduce(ctx->header.pixelWidth, mipmaplevel);
			uint32_t const snl = s * n * w;
			uint32_t const k = ((snl + 3u) & ~3u);
			return k;
		}
		return 0;
	}


	bool TinyKtx_WriteImageGL(TinyKtx_WriteCallbacks const *callbacks,
		void *user,
		uint32_t width,
		uint32_t height,
		uint32_t depth,
		uint32_t slices,
		uint32_t mipmaplevels,
		uint32_t format,
		uint32_t internalFormat,
		uint32_t baseFormat,
		uint32_t type,
		uint32_t typeSize,
		bool cubemap,
		uint32_t const *mipmapsizes,
		void const **mipmaps) {

		TinyKtx_Header header;
		memcpy(header.identifier, TinyKtx_fileIdentifier, 12);
		header.endianness = 0x04030201;
		header.glFormat = format;
		header.glInternalFormat = internalFormat;
		header.glBaseInternalFormat = baseFormat;
		header.glType = type;
		header.glTypeSize = typeSize;
		header.pixelWidth = width;
		header.pixelHeight = (height == 1) ? 0 : height;
		header.pixelDepth = (depth == 1) ? 0 : depth;
		header.numberOfArrayElements = (slices == 1) ? 0 : slices;
		header.numberOfFaces = cubemap ? 6 : 1;
		header.numberOfMipmapLevels = mipmaplevels;
		// TODO keyvalue pair data
		header.bytesOfKeyValueData = 0;
		callbacks->writeFn(user, &header, sizeof(TinyKtx_Header));

		uint32_t w = (width == 0) ? 1 : width;
		uint32_t h = (height == 0) ? 1 : height;
		uint32_t d = (depth == 0) ? 1 : depth;
		uint32_t sl = (slices == 0) ? 1 : slices;
		static uint8_t const padding[4] = { 0, 0, 0, 0 };

		for (uint32_t i = 0u; i < mipmaplevels; ++i) {

			bool writeRaw = true;

			if (typeSize < 4 &&
				TinyKtx_ByteDividableFromGLType(type)) {

				uint32_t const s = typeSize;
				uint32_t const n = TinyKtx_ElementCountFromGLFormat(format);
				if (n == 0) {
					callbacks->errorFn(user, "TinyKtx_ElementCountFromGLFormat error");
					return false;
				}
				uint32_t const snl = s * n * w;
				uint32_t const k = ((snl + 3u) & ~3u);

				uint32_t const size = (k * h * d * snl);
				if (size < mipmapsizes[i]) {
					callbacks->errorFn(user, "Internal size error, padding should only ever expand");
					return false;
				}

				// if we need to expand for padding take the slow per row write route
				if (size > mipmapsizes[i]) {
					callbacks->writeFn(user, &size, sizeof(uint32_t));

					uint8_t const *src = (uint8_t const *)mipmaps[i];
					for (uint32_t ww = 0u; ww < sl; ++ww) {
						for (uint32_t zz = 0; zz < d; ++zz) {
							for (uint32_t yy = 0; yy < h; ++yy) {
								callbacks->writeFn(user, src, snl);
								callbacks->writeFn(user, padding, k - snl);
								src += snl;
							}
						}
					}
					uint32_t paddCount = ((size + 3u) & ~3u) - size;
					if (paddCount > 3) {
						callbacks->errorFn(user, "Internal error: padding bytes > 3");
						return false;
					}

					callbacks->writeFn(user, padding, paddCount);
					writeRaw = false;
				}
			}

			if (writeRaw) {
				uint32_t const size = ((mipmapsizes[i] + 3u) & ~3u);
				callbacks->writeFn(user, mipmapsizes + i, sizeof(uint32_t));
				callbacks->writeFn(user, mipmaps[i], mipmapsizes[i]);
				callbacks->writeFn(user, padding, size - mipmapsizes[i]);
			}

			if (w > 1) w = w / 2;
			if (h > 1) h = h / 2;
			if (d > 1) d = d / 2;
		}

		return true;
	}

	bool TinyKtx_WriteImage(TinyKtx_WriteCallbacks const *callbacks,
		void *user,
		uint32_t width,
		uint32_t height,
		uint32_t depth,
		uint32_t slices,
		uint32_t mipmaplevels,
		TinyKtx_Format format,
		bool cubemap,
		uint32_t const *mipmapsizes,
		void const **mipmaps) {
		uint32_t glformat;
		uint32_t glinternalFormat;
		uint32_t gltype;
		uint32_t gltypeSize;
		if (TinyKtx_CrackFormatToGL(format, &glformat, &gltype, &glinternalFormat, &gltypeSize) == false)
			return false;

		return TinyKtx_WriteImageGL(callbacks,
			user,
			width,
			height,
			depth,
			slices,
			mipmaplevels,
			glformat,
			glinternalFormat,
			glinternalFormat, //??
			gltype,
			gltypeSize,
			cubemap,
			mipmapsizes,
			mipmaps
		);

	}

	// tiny_imageformat/tinyimageformat.h pr tinyimageformat_base.h needs included
	// before tinyktx.h for this functionality
#ifdef TINYIMAGEFORMAT_BASE_H_
	TinyImageFormat TinyImageFormat_FromTinyKtxFormat(TinyKtx_Format format)
	{
		switch (format) {
		case TKTX_UNDEFINED: return TinyImageFormat_UNDEFINED;
		case TKTX_R4G4_UNORM_PACK8: return TinyImageFormat_R4G4_UNORM;
		case TKTX_R4G4B4A4_UNORM_PACK16: return TinyImageFormat_R4G4B4A4_UNORM;
		case TKTX_B4G4R4A4_UNORM_PACK16: return TinyImageFormat_B4G4R4A4_UNORM;
		case TKTX_R5G6B5_UNORM_PACK16: return TinyImageFormat_R5G6B5_UNORM;
		case TKTX_B5G6R5_UNORM_PACK16: return TinyImageFormat_B5G6R5_UNORM;
		case TKTX_R5G5B5A1_UNORM_PACK16: return TinyImageFormat_R5G5B5A1_UNORM;
		case TKTX_B5G5R5A1_UNORM_PACK16: return TinyImageFormat_B5G5R5A1_UNORM;
		case TKTX_A1R5G5B5_UNORM_PACK16: return TinyImageFormat_A1R5G5B5_UNORM;
		case TKTX_R8_UNORM: return TinyImageFormat_R8_UNORM;
		case TKTX_R8_SNORM: return TinyImageFormat_R8_SNORM;
		case TKTX_R8_UINT: return TinyImageFormat_R8_UINT;
		case TKTX_R8_SINT: return TinyImageFormat_R8_SINT;
		case TKTX_R8_SRGB: return TinyImageFormat_R8_SRGB;
		case TKTX_R8G8_UNORM: return TinyImageFormat_R8G8_UNORM;
		case TKTX_R8G8_SNORM: return TinyImageFormat_R8G8_SNORM;
		case TKTX_R8G8_UINT: return TinyImageFormat_R8G8_UINT;
		case TKTX_R8G8_SINT: return TinyImageFormat_R8G8_SINT;
		case TKTX_R8G8_SRGB: return TinyImageFormat_R8G8_SRGB;
		case TKTX_R8G8B8_UNORM: return TinyImageFormat_R8G8B8_UNORM;
		case TKTX_R8G8B8_SNORM: return TinyImageFormat_R8G8B8_SNORM;
		case TKTX_R8G8B8_UINT: return TinyImageFormat_R8G8B8_UINT;
		case TKTX_R8G8B8_SINT: return TinyImageFormat_R8G8B8_SINT;
		case TKTX_R8G8B8_SRGB: return TinyImageFormat_R8G8B8_SRGB;
		case TKTX_B8G8R8_UNORM: return TinyImageFormat_B8G8R8_UNORM;
		case TKTX_B8G8R8_SNORM: return TinyImageFormat_B8G8R8_SNORM;
		case TKTX_B8G8R8_UINT: return TinyImageFormat_B8G8R8_UINT;
		case TKTX_B8G8R8_SINT: return TinyImageFormat_B8G8R8_SINT;
		case TKTX_B8G8R8_SRGB: return TinyImageFormat_B8G8R8_SRGB;
		case TKTX_R8G8B8A8_UNORM: return TinyImageFormat_R8G8B8A8_UNORM;
		case TKTX_R8G8B8A8_SNORM: return TinyImageFormat_R8G8B8A8_SNORM;
		case TKTX_R8G8B8A8_UINT: return TinyImageFormat_R8G8B8A8_UINT;
		case TKTX_R8G8B8A8_SINT: return TinyImageFormat_R8G8B8A8_SINT;
		case TKTX_R8G8B8A8_SRGB: return TinyImageFormat_R8G8B8A8_SRGB;
		case TKTX_B8G8R8A8_UNORM: return TinyImageFormat_B8G8R8A8_UNORM;
		case TKTX_B8G8R8A8_SNORM: return TinyImageFormat_B8G8R8A8_SNORM;
		case TKTX_B8G8R8A8_UINT: return TinyImageFormat_B8G8R8A8_UINT;
		case TKTX_B8G8R8A8_SINT: return TinyImageFormat_B8G8R8A8_SINT;
		case TKTX_B8G8R8A8_SRGB: return TinyImageFormat_B8G8R8A8_SRGB;
		case TKTX_E5B9G9R9_UFLOAT_PACK32: return TinyImageFormat_E5B9G9R9_UFLOAT;
		case TKTX_A2R10G10B10_UNORM_PACK32: return TinyImageFormat_B10G10R10A2_UNORM;
		case TKTX_A2R10G10B10_UINT_PACK32: return TinyImageFormat_B10G10R10A2_UINT;
		case TKTX_A2B10G10R10_UNORM_PACK32: return TinyImageFormat_R10G10B10A2_UNORM;
		case TKTX_A2B10G10R10_UINT_PACK32: return TinyImageFormat_R10G10B10A2_UINT;
		case TKTX_B10G11R11_UFLOAT_PACK32: return TinyImageFormat_B10G11R11_UFLOAT;
		case TKTX_R16_UNORM: return TinyImageFormat_R16_UNORM;
		case TKTX_R16_SNORM: return TinyImageFormat_R16_SNORM;
		case TKTX_R16_UINT: return TinyImageFormat_R16_UINT;
		case TKTX_R16_SINT: return TinyImageFormat_R16_SINT;
		case TKTX_R16_SFLOAT: return TinyImageFormat_R16_SFLOAT;
		case TKTX_R16G16_UNORM: return TinyImageFormat_R16G16_UNORM;
		case TKTX_R16G16_SNORM: return TinyImageFormat_R16G16_SNORM;
		case TKTX_R16G16_UINT: return TinyImageFormat_R16G16_UINT;
		case TKTX_R16G16_SINT: return TinyImageFormat_R16G16_SINT;
		case TKTX_R16G16_SFLOAT: return TinyImageFormat_R16G16_SFLOAT;
		case TKTX_R16G16B16_UNORM: return TinyImageFormat_R16G16B16_UNORM;
		case TKTX_R16G16B16_SNORM: return TinyImageFormat_R16G16B16_SNORM;
		case TKTX_R16G16B16_UINT: return TinyImageFormat_R16G16B16_UINT;
		case TKTX_R16G16B16_SINT: return TinyImageFormat_R16G16B16_SINT;
		case TKTX_R16G16B16_SFLOAT: return TinyImageFormat_R16G16B16_SFLOAT;
		case TKTX_R16G16B16A16_UNORM: return TinyImageFormat_R16G16B16A16_UNORM;
		case TKTX_R16G16B16A16_SNORM: return TinyImageFormat_R16G16B16A16_SNORM;
		case TKTX_R16G16B16A16_UINT: return TinyImageFormat_R16G16B16A16_UINT;
		case TKTX_R16G16B16A16_SINT: return TinyImageFormat_R16G16B16A16_SINT;
		case TKTX_R16G16B16A16_SFLOAT: return TinyImageFormat_R16G16B16A16_SFLOAT;
		case TKTX_R32_UINT: return TinyImageFormat_R32_UINT;
		case TKTX_R32_SINT: return TinyImageFormat_R32_SINT;
		case TKTX_R32_SFLOAT: return TinyImageFormat_R32_SFLOAT;
		case TKTX_R32G32_UINT: return TinyImageFormat_R32G32_UINT;
		case TKTX_R32G32_SINT: return TinyImageFormat_R32G32_SINT;
		case TKTX_R32G32_SFLOAT: return TinyImageFormat_R32G32_SFLOAT;
		case TKTX_R32G32B32_UINT: return TinyImageFormat_R32G32B32_UINT;
		case TKTX_R32G32B32_SINT: return TinyImageFormat_R32G32B32_SINT;
		case TKTX_R32G32B32_SFLOAT: return TinyImageFormat_R32G32B32_SFLOAT;
		case TKTX_R32G32B32A32_UINT: return TinyImageFormat_R32G32B32A32_UINT;
		case TKTX_R32G32B32A32_SINT: return TinyImageFormat_R32G32B32A32_SINT;
		case TKTX_R32G32B32A32_SFLOAT: return TinyImageFormat_R32G32B32A32_SFLOAT;
		case TKTX_BC1_RGB_UNORM_BLOCK: return TinyImageFormat_DXBC1_RGB_UNORM;
		case TKTX_BC1_RGB_SRGB_BLOCK: return TinyImageFormat_DXBC1_RGB_SRGB;
		case TKTX_BC1_RGBA_UNORM_BLOCK: return TinyImageFormat_DXBC1_RGBA_UNORM;
		case TKTX_BC1_RGBA_SRGB_BLOCK: return TinyImageFormat_DXBC1_RGBA_SRGB;
		case TKTX_BC2_UNORM_BLOCK: return TinyImageFormat_DXBC2_UNORM;
		case TKTX_BC2_SRGB_BLOCK: return TinyImageFormat_DXBC2_SRGB;
		case TKTX_BC3_UNORM_BLOCK: return TinyImageFormat_DXBC3_UNORM;
		case TKTX_BC3_SRGB_BLOCK: return TinyImageFormat_DXBC3_SRGB;
		case TKTX_BC4_UNORM_BLOCK: return TinyImageFormat_DXBC4_UNORM;
		case TKTX_BC4_SNORM_BLOCK: return TinyImageFormat_DXBC4_SNORM;
		case TKTX_BC5_UNORM_BLOCK: return TinyImageFormat_DXBC5_UNORM;
		case TKTX_BC5_SNORM_BLOCK: return TinyImageFormat_DXBC5_SNORM;
		case TKTX_BC6H_UFLOAT_BLOCK: return TinyImageFormat_DXBC6H_UFLOAT;
		case TKTX_BC6H_SFLOAT_BLOCK: return TinyImageFormat_DXBC6H_SFLOAT;
		case TKTX_BC7_UNORM_BLOCK: return TinyImageFormat_DXBC7_UNORM;
		case TKTX_BC7_SRGB_BLOCK: return TinyImageFormat_DXBC7_SRGB;
		case TKTX_PVR_2BPP_UNORM_BLOCK: return TinyImageFormat_PVRTC1_2BPP_UNORM;
		case TKTX_PVR_2BPPA_UNORM_BLOCK: return TinyImageFormat_PVRTC1_2BPP_UNORM;
		case TKTX_PVR_4BPP_UNORM_BLOCK: return TinyImageFormat_PVRTC1_4BPP_UNORM;
		case TKTX_PVR_4BPPA_UNORM_BLOCK: return TinyImageFormat_PVRTC1_4BPP_UNORM;
		case TKTX_PVR_2BPP_SRGB_BLOCK: return TinyImageFormat_PVRTC1_2BPP_SRGB;
		case TKTX_PVR_2BPPA_SRGB_BLOCK: return TinyImageFormat_PVRTC1_2BPP_SRGB;
		case TKTX_PVR_4BPP_SRGB_BLOCK: return TinyImageFormat_PVRTC1_4BPP_SRGB;
		case TKTX_PVR_4BPPA_SRGB_BLOCK: return TinyImageFormat_PVRTC1_4BPP_SRGB;

		case TKTX_ETC2_R8G8B8_UNORM_BLOCK: return TinyImageFormat_ETC2_R8G8B8_UNORM;
		case TKTX_ETC2_R8G8B8A1_UNORM_BLOCK: return TinyImageFormat_ETC2_R8G8B8A1_UNORM;
		case TKTX_ETC2_R8G8B8A8_UNORM_BLOCK: return TinyImageFormat_ETC2_R8G8B8A8_UNORM;
		case TKTX_ETC2_R8G8B8_SRGB_BLOCK: return TinyImageFormat_ETC2_R8G8B8_SRGB;
		case TKTX_ETC2_R8G8B8A1_SRGB_BLOCK: return TinyImageFormat_ETC2_R8G8B8A1_SRGB;
		case TKTX_ETC2_R8G8B8A8_SRGB_BLOCK: return TinyImageFormat_ETC2_R8G8B8A8_SRGB;
		case TKTX_EAC_R11_UNORM_BLOCK: return TinyImageFormat_ETC2_EAC_R11_UNORM;
		case TKTX_EAC_R11G11_UNORM_BLOCK: return TinyImageFormat_ETC2_EAC_R11G11_UNORM;
		case TKTX_EAC_R11_SNORM_BLOCK: return TinyImageFormat_ETC2_EAC_R11_SNORM;
		case TKTX_EAC_R11G11_SNORM_BLOCK: return TinyImageFormat_ETC2_EAC_R11G11_SNORM;
		case TKTX_ASTC_4x4_UNORM_BLOCK: return TinyImageFormat_ASTC_4x4_UNORM;
		case TKTX_ASTC_4x4_SRGB_BLOCK: return TinyImageFormat_ASTC_4x4_SRGB;
		case TKTX_ASTC_5x4_UNORM_BLOCK: return TinyImageFormat_ASTC_5x4_UNORM;
		case TKTX_ASTC_5x4_SRGB_BLOCK: return TinyImageFormat_ASTC_5x4_SRGB;
		case TKTX_ASTC_5x5_UNORM_BLOCK: return TinyImageFormat_ASTC_5x5_UNORM;
		case TKTX_ASTC_5x5_SRGB_BLOCK: return TinyImageFormat_ASTC_5x5_SRGB;
		case TKTX_ASTC_6x5_UNORM_BLOCK: return TinyImageFormat_ASTC_6x5_UNORM;
		case TKTX_ASTC_6x5_SRGB_BLOCK: return TinyImageFormat_ASTC_6x5_SRGB;
		case TKTX_ASTC_6x6_UNORM_BLOCK: return TinyImageFormat_ASTC_6x6_UNORM;
		case TKTX_ASTC_6x6_SRGB_BLOCK: return TinyImageFormat_ASTC_6x6_SRGB;
		case TKTX_ASTC_8x5_UNORM_BLOCK: return TinyImageFormat_ASTC_8x5_UNORM;
		case TKTX_ASTC_8x5_SRGB_BLOCK: return TinyImageFormat_ASTC_8x5_SRGB;
		case TKTX_ASTC_8x6_UNORM_BLOCK: return TinyImageFormat_ASTC_8x6_UNORM;
		case TKTX_ASTC_8x6_SRGB_BLOCK: return TinyImageFormat_ASTC_8x6_SRGB;
		case TKTX_ASTC_8x8_UNORM_BLOCK: return TinyImageFormat_ASTC_8x8_UNORM;
		case TKTX_ASTC_8x8_SRGB_BLOCK: return TinyImageFormat_ASTC_8x8_SRGB;
		case TKTX_ASTC_10x5_UNORM_BLOCK: return TinyImageFormat_ASTC_10x5_UNORM;
		case TKTX_ASTC_10x5_SRGB_BLOCK: return TinyImageFormat_ASTC_10x5_SRGB;
		case TKTX_ASTC_10x6_UNORM_BLOCK: return TinyImageFormat_ASTC_10x6_UNORM;
		case TKTX_ASTC_10x6_SRGB_BLOCK: return TinyImageFormat_ASTC_10x6_SRGB;
		case TKTX_ASTC_10x8_UNORM_BLOCK: return TinyImageFormat_ASTC_10x8_UNORM;
		case TKTX_ASTC_10x8_SRGB_BLOCK: return TinyImageFormat_ASTC_10x8_SRGB;
		case TKTX_ASTC_10x10_UNORM_BLOCK: return TinyImageFormat_ASTC_10x10_UNORM;
		case TKTX_ASTC_10x10_SRGB_BLOCK: return TinyImageFormat_ASTC_10x10_SRGB;
		case TKTX_ASTC_12x10_UNORM_BLOCK: return TinyImageFormat_ASTC_12x10_UNORM;
		case TKTX_ASTC_12x10_SRGB_BLOCK: return TinyImageFormat_ASTC_12x10_SRGB;
		case TKTX_ASTC_12x12_UNORM_BLOCK: return TinyImageFormat_ASTC_12x12_UNORM;
		case TKTX_ASTC_12x12_SRGB_BLOCK: return TinyImageFormat_ASTC_12x12_SRGB;

		case TKTX_A8B8G8R8_UNORM_PACK32:break;
		case TKTX_A8B8G8R8_SNORM_PACK32:break;
		case TKTX_A8B8G8R8_UINT_PACK32:break;
		case TKTX_A8B8G8R8_SINT_PACK32:break;
		case TKTX_A8B8G8R8_SRGB_PACK32:break;
		}

		return TinyImageFormat_UNDEFINED;
	}

	TinyKtx_Format TinyImageFormat_ToTinyKtxFormat(TinyImageFormat format) {

		switch (format) {
		case TinyImageFormat_UNDEFINED: return TKTX_UNDEFINED;
		case TinyImageFormat_R4G4_UNORM: return TKTX_R4G4_UNORM_PACK8;
		case TinyImageFormat_R4G4B4A4_UNORM: return TKTX_R4G4B4A4_UNORM_PACK16;
		case TinyImageFormat_B4G4R4A4_UNORM: return TKTX_B4G4R4A4_UNORM_PACK16;
		case TinyImageFormat_R5G6B5_UNORM: return TKTX_R5G6B5_UNORM_PACK16;
		case TinyImageFormat_B5G6R5_UNORM: return TKTX_B5G6R5_UNORM_PACK16;
		case TinyImageFormat_R5G5B5A1_UNORM: return TKTX_R5G5B5A1_UNORM_PACK16;
		case TinyImageFormat_B5G5R5A1_UNORM: return TKTX_B5G5R5A1_UNORM_PACK16;
		case TinyImageFormat_A1R5G5B5_UNORM: return TKTX_A1R5G5B5_UNORM_PACK16;
		case TinyImageFormat_R8_UNORM: return TKTX_R8_UNORM;
		case TinyImageFormat_R8_SNORM: return TKTX_R8_SNORM;
		case TinyImageFormat_R8_UINT: return TKTX_R8_UINT;
		case TinyImageFormat_R8_SINT: return TKTX_R8_SINT;
		case TinyImageFormat_R8_SRGB: return TKTX_R8_SRGB;
		case TinyImageFormat_R8G8_UNORM: return TKTX_R8G8_UNORM;
		case TinyImageFormat_R8G8_SNORM: return TKTX_R8G8_SNORM;
		case TinyImageFormat_R8G8_UINT: return TKTX_R8G8_UINT;
		case TinyImageFormat_R8G8_SINT: return TKTX_R8G8_SINT;
		case TinyImageFormat_R8G8_SRGB: return TKTX_R8G8_SRGB;
		case TinyImageFormat_R8G8B8_UNORM: return TKTX_R8G8B8_UNORM;
		case TinyImageFormat_R8G8B8_SNORM: return TKTX_R8G8B8_SNORM;
		case TinyImageFormat_R8G8B8_UINT: return TKTX_R8G8B8_UINT;
		case TinyImageFormat_R8G8B8_SINT: return TKTX_R8G8B8_SINT;
		case TinyImageFormat_R8G8B8_SRGB: return TKTX_R8G8B8_SRGB;
		case TinyImageFormat_B8G8R8_UNORM: return TKTX_B8G8R8_UNORM;
		case TinyImageFormat_B8G8R8_SNORM: return TKTX_B8G8R8_SNORM;
		case TinyImageFormat_B8G8R8_UINT: return TKTX_B8G8R8_UINT;
		case TinyImageFormat_B8G8R8_SINT: return TKTX_B8G8R8_SINT;
		case TinyImageFormat_B8G8R8_SRGB: return TKTX_B8G8R8_SRGB;
		case TinyImageFormat_R8G8B8A8_UNORM: return TKTX_R8G8B8A8_UNORM;
		case TinyImageFormat_R8G8B8A8_SNORM: return TKTX_R8G8B8A8_SNORM;
		case TinyImageFormat_R8G8B8A8_UINT: return TKTX_R8G8B8A8_UINT;
		case TinyImageFormat_R8G8B8A8_SINT: return TKTX_R8G8B8A8_SINT;
		case TinyImageFormat_R8G8B8A8_SRGB: return TKTX_R8G8B8A8_SRGB;
		case TinyImageFormat_B8G8R8A8_UNORM: return TKTX_B8G8R8A8_UNORM;
		case TinyImageFormat_B8G8R8A8_SNORM: return TKTX_B8G8R8A8_SNORM;
		case TinyImageFormat_B8G8R8A8_UINT: return TKTX_B8G8R8A8_UINT;
		case TinyImageFormat_B8G8R8A8_SINT: return TKTX_B8G8R8A8_SINT;
		case TinyImageFormat_B8G8R8A8_SRGB: return TKTX_B8G8R8A8_SRGB;
		case TinyImageFormat_R10G10B10A2_UNORM: return TKTX_A2B10G10R10_UNORM_PACK32;
		case TinyImageFormat_R10G10B10A2_UINT: return TKTX_A2B10G10R10_UINT_PACK32;
		case TinyImageFormat_B10G10R10A2_UNORM: return TKTX_A2R10G10B10_UNORM_PACK32;
		case TinyImageFormat_B10G10R10A2_UINT: return TKTX_A2R10G10B10_UINT_PACK32;
		case TinyImageFormat_R16_UNORM: return TKTX_R16_UNORM;
		case TinyImageFormat_R16_SNORM: return TKTX_R16_SNORM;
		case TinyImageFormat_R16_UINT: return TKTX_R16_UINT;
		case TinyImageFormat_R16_SINT: return TKTX_R16_SINT;
		case TinyImageFormat_R16_SFLOAT: return TKTX_R16_SFLOAT;
		case TinyImageFormat_R16G16_UNORM: return TKTX_R16G16_UNORM;
		case TinyImageFormat_R16G16_SNORM: return TKTX_R16G16_SNORM;
		case TinyImageFormat_R16G16_UINT: return TKTX_R16G16_UINT;
		case TinyImageFormat_R16G16_SINT: return TKTX_R16G16_SINT;
		case TinyImageFormat_R16G16_SFLOAT: return TKTX_R16G16_SFLOAT;
		case TinyImageFormat_R16G16B16_UNORM: return TKTX_R16G16B16_UNORM;
		case TinyImageFormat_R16G16B16_SNORM: return TKTX_R16G16B16_SNORM;
		case TinyImageFormat_R16G16B16_UINT: return TKTX_R16G16B16_UINT;
		case TinyImageFormat_R16G16B16_SINT: return TKTX_R16G16B16_SINT;
		case TinyImageFormat_R16G16B16_SFLOAT: return TKTX_R16G16B16_SFLOAT;
		case TinyImageFormat_R16G16B16A16_UNORM: return TKTX_R16G16B16A16_UNORM;
		case TinyImageFormat_R16G16B16A16_SNORM: return TKTX_R16G16B16A16_SNORM;
		case TinyImageFormat_R16G16B16A16_UINT: return TKTX_R16G16B16A16_UINT;
		case TinyImageFormat_R16G16B16A16_SINT: return TKTX_R16G16B16A16_SINT;
		case TinyImageFormat_R16G16B16A16_SFLOAT: return TKTX_R16G16B16A16_SFLOAT;
		case TinyImageFormat_R32_UINT: return TKTX_R32_UINT;
		case TinyImageFormat_R32_SINT: return TKTX_R32_SINT;
		case TinyImageFormat_R32_SFLOAT: return TKTX_R32_SFLOAT;
		case TinyImageFormat_R32G32_UINT: return TKTX_R32G32_UINT;
		case TinyImageFormat_R32G32_SINT: return TKTX_R32G32_SINT;
		case TinyImageFormat_R32G32_SFLOAT: return TKTX_R32G32_SFLOAT;
		case TinyImageFormat_R32G32B32_UINT: return TKTX_R32G32B32_UINT;
		case TinyImageFormat_R32G32B32_SINT: return TKTX_R32G32B32_SINT;
		case TinyImageFormat_R32G32B32_SFLOAT: return TKTX_R32G32B32_SFLOAT;
		case TinyImageFormat_R32G32B32A32_UINT: return TKTX_R32G32B32A32_UINT;
		case TinyImageFormat_R32G32B32A32_SINT: return TKTX_R32G32B32A32_SINT;
		case TinyImageFormat_R32G32B32A32_SFLOAT: return TKTX_R32G32B32A32_SFLOAT;
		case TinyImageFormat_B10G11R11_UFLOAT: return TKTX_B10G11R11_UFLOAT_PACK32;
		case TinyImageFormat_E5B9G9R9_UFLOAT: return TKTX_E5B9G9R9_UFLOAT_PACK32;

		case TinyImageFormat_DXBC1_RGB_UNORM: return TKTX_BC1_RGB_UNORM_BLOCK;
		case TinyImageFormat_DXBC1_RGB_SRGB: return TKTX_BC1_RGB_SRGB_BLOCK;
		case TinyImageFormat_DXBC1_RGBA_UNORM: return TKTX_BC1_RGBA_UNORM_BLOCK;
		case TinyImageFormat_DXBC1_RGBA_SRGB: return TKTX_BC1_RGBA_SRGB_BLOCK;
		case TinyImageFormat_DXBC2_UNORM: return TKTX_BC2_UNORM_BLOCK;
		case TinyImageFormat_DXBC2_SRGB: return TKTX_BC2_SRGB_BLOCK;
		case TinyImageFormat_DXBC3_UNORM: return TKTX_BC3_UNORM_BLOCK;
		case TinyImageFormat_DXBC3_SRGB: return TKTX_BC3_SRGB_BLOCK;
		case TinyImageFormat_DXBC4_UNORM: return TKTX_BC4_UNORM_BLOCK;
		case TinyImageFormat_DXBC4_SNORM: return TKTX_BC4_SNORM_BLOCK;
		case TinyImageFormat_DXBC5_UNORM: return TKTX_BC5_UNORM_BLOCK;
		case TinyImageFormat_DXBC5_SNORM: return TKTX_BC5_SNORM_BLOCK;
		case TinyImageFormat_DXBC6H_UFLOAT: return TKTX_BC6H_UFLOAT_BLOCK;
		case TinyImageFormat_DXBC6H_SFLOAT: return TKTX_BC6H_SFLOAT_BLOCK;
		case TinyImageFormat_DXBC7_UNORM: return TKTX_BC7_UNORM_BLOCK;
		case TinyImageFormat_DXBC7_SRGB: return TKTX_BC7_SRGB_BLOCK;
		case TinyImageFormat_PVRTC1_2BPP_UNORM: return TKTX_PVR_2BPPA_UNORM_BLOCK;
		case TinyImageFormat_PVRTC1_4BPP_UNORM: return TKTX_PVR_4BPPA_UNORM_BLOCK;
		case TinyImageFormat_PVRTC1_2BPP_SRGB: return TKTX_PVR_2BPPA_SRGB_BLOCK;
		case TinyImageFormat_PVRTC1_4BPP_SRGB: return TKTX_PVR_4BPPA_SRGB_BLOCK;

		default: return TKTX_UNDEFINED;
		};

		return TKTX_UNDEFINED;
	}
#endif // end TinyImageFormat conversion

#endif // end implementation

#ifdef __cplusplus
}
#endif

//[tiny_ktx_license]
/*
MIT License

Copyright (c) 2019 DeanoC

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

--- End of tiny_ktx
*/

//Zest_ktx_helper_functions
#ifdef __cplusplus
extern "C" {
#endif

ZEST_PRIVATE void zest__tinyktxCallbackError(void *user, char const *msg);
ZEST_PRIVATE void *zest__tinyktxCallbackAlloc(void *user, size_t size);
ZEST_PRIVATE void zest__tinyktxCallbackFree(void *user, void *data);
ZEST_PRIVATE size_t zest__tinyktxCallbackRead(void *user, void *data, size_t size);
ZEST_PRIVATE bool zest__tinyktxCallbackSeek(void *user, int64_t offset);
ZEST_PRIVATE int64_t zest__tinyktxCallbackTell(void *user);
ZEST_PRIVATE zest_format zest__convert_tktx_format(TinyKtx_Format format);
ZEST_API zest_image_collection_handle zest__load_ktx(zest_context context, const char *file_path);
ZEST_API zest_image_handle zest_LoadCubemap(zest_context context, const char *name, const char *file_name);
//End Zest_ktx_helper_functions

#ifdef TINYKTX_IMPLEMENTATION
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

// stb_truetype_fonts

// stb_truetype.h - v1.26 - public domain
// authored from 2009-2021 by Sean Barrett / RAD Game Tools
//
// =======================================================================
//
//    NO SECURITY GUARANTEE -- DO NOT USE THIS ON UNTRUSTED FONT FILES
//
// This library does no range checking of the offsets found in the file,
// meaning an attacker can use it to read arbitrary memory.
//
// =======================================================================
//
//   This library processes TrueType files:
//        parse files
//        extract glyph metrics
//        extract glyph shapes
//        render glyphs to one-channel bitmaps with antialiasing (box filter)
//        render glyphs to one-channel SDF bitmaps (signed-distance field/function)
//
//   Todo:
//        non-MS cmaps
//        crashproof on bad data
//        hinting? (no longer patented)
//        cleartype-style AA?
//        optimize: use simple memory allocator for intermediates
//        optimize: build edge-list directly from curves
//        optimize: rasterize directly from curves?
//
// ADDITIONAL CONTRIBUTORS
//
//   Mikko Mononen: compound shape support, more cmap formats
//   Tor Andersson: kerning, subpixel rendering
//   Dougall Johnson: OpenType / Type 2 font handling
//   Daniel Ribeiro Maciel: basic GPOS-based kerning
//
//   Misc other:
//       Ryan Gordon
//       Simon Glass
//       github:IntellectualKitty
//       Imanol Celaya
//       Daniel Ribeiro Maciel
//
//   Bug/warning reports/fixes:
//       "Zer" on mollyrocket       Fabian "ryg" Giesen   github:NiLuJe
//       Cass Everitt               Martins Mozeiko       github:aloucks
//       stoiko (Haemimont Games)   Cap Petschulat        github:oyvindjam
//       Brian Hook                 Omar Cornut           github:vassvik
//       Walter van Niftrik         Ryan Griege
//       David Gow                  Peter LaValle
//       David Given                Sergey Popov
//       Ivan-Assen Ivanov          Giumo X. Clanjor
//       Anthony Pesch              Higor Euripedes
//       Johan Duparc               Thomas Fields
//       Hou Qiming                 Derek Vinyard
//       Rob Loach                  Cort Stratton
//       Kenney Phillis Jr.         Brian Costabile
//       Ken Voskuil (kaesve)       Yakov Galka
//
// VERSION HISTORY
//
//   1.26 (2021-08-28) fix broken rasterizer
//   1.25 (2021-07-11) many fixes
//   1.24 (2020-02-05) fix warning
//   1.23 (2020-02-02) query SVG data for glyphs; query whole kerning table (but only kern not GPOS)
//   1.22 (2019-08-11) minimize missing-glyph duplication; fix kerning if both 'GPOS' and 'kern' are defined
//   1.21 (2019-02-25) fix warning
//   1.20 (2019-02-07) PackFontRange skips missing codepoints; GetScaleFontVMetrics()
//   1.19 (2018-02-11) GPOS kerning, STBTT_fmod
//   1.18 (2018-01-29) add missing function
//   1.17 (2017-07-23) make more arguments const; doc fix
//   1.16 (2017-07-12) SDF support
//   1.15 (2017-03-03) make more arguments const
//   1.14 (2017-01-16) num-fonts-in-TTC function
//   1.13 (2017-01-02) support OpenType fonts, certain Apple fonts
//   1.12 (2016-10-25) suppress warnings about casting away const with -Wcast-qual
//   1.11 (2016-04-02) fix unused-variable warning
//   1.10 (2016-04-02) user-defined fabs(); rare memory leak; remove duplicate typedef
//   1.09 (2016-01-16) warning fix; avoid crash on outofmem; use allocation userdata properly
//   1.08 (2015-09-13) document stbtt_Rasterize(); fixes for vertical & horizontal edges
//   1.07 (2015-08-01) allow PackFontRanges to accept arrays of sparse codepoints;
//                     variant PackFontRanges to pack and render in separate phases;
//                     fix stbtt_GetFontOFfsetForIndex (never worked for non-0 input?);
//                     fixed an assert() bug in the new rasterizer
//                     replace assert() with STBTT_assert() in new rasterizer
//
//   Full history can be found at the end of this file.
//
// LICENSE
//
//   See end of file for license information.
//
// USAGE
//
//   Include this file in whatever places need to refer to it. In ONE C/C++
//   file, write:
//      #define STB_TRUETYPE_IMPLEMENTATION
//   before the #include of this file. This expands out the actual
//   implementation into that C/C++ file.
//
//   To make the implementation private to the file that generates the implementation,
//      #define STBTT_STATIC
//
//   Simple 3D API (don't ship this, but it's fine for tools and quick start)
//           stbtt_BakeFontBitmap()               -- bake a font to a bitmap for use as texture
//           stbtt_GetBakedQuad()                 -- compute quad to draw for a given char
//
//   Improved 3D API (more shippable):
//           #include "stb_rect_pack.h"           -- optional, but you really want it
//           stbtt_PackBegin()
//           stbtt_PackSetOversampling()          -- for improved quality on small fonts
//           stbtt_PackFontRanges()               -- pack and renders
//           stbtt_PackEnd()
//           stbtt_GetPackedQuad()
//
//   "Load" a font file from a memory buffer (you have to keep the buffer loaded)
//           stbtt_InitFont()
//           stbtt_GetFontOffsetForIndex()        -- indexing for TTC font collections
//           stbtt_GetNumberOfFonts()             -- number of fonts for TTC font collections
//
//   Render a unicode codepoint to a bitmap
//           stbtt_GetCodepointBitmap()           -- allocates and returns a bitmap
//           stbtt_MakeCodepointBitmap()          -- renders into bitmap you provide
//           stbtt_GetCodepointBitmapBox()        -- how big the bitmap must be
//
//   Character advance/positioning
//           stbtt_GetCodepointHMetrics()
//           stbtt_GetFontVMetrics()
//           stbtt_GetFontVMetricsOS2()
//           stbtt_GetCodepointKernAdvance()
//
//   Starting with version 1.06, the rasterizer was replaced with a new,
//   faster and generally-more-precise rasterizer. The new rasterizer more
//   accurately measures pixel coverage for anti-aliasing, except in the case
//   where multiple shapes overlap, in which case it overestimates the AA pixel
//   coverage. Thus, anti-aliasing of intersecting shapes may look wrong. If
//   this turns out to be a problem, you can re-enable the old rasterizer with
//        #define STBTT_RASTERIZER_VERSION 1
//   which will incur about a 15% speed hit.
//
// ADDITIONAL DOCUMENTATION
//
//   Immediately after this block comment are a series of sample programs.
//
//   After the sample programs is the "header file" section. This section
//   includes documentation for each API function.
//
//   Some important concepts to understand to use this library:
//
//      Codepoint
//         Characters are defined by unicode codepoints, e.g. 65 is
//         uppercase A, 231 is lowercase c with a cedilla, 0x7e30 is
//         the hiragana for "ma".
//
//      Glyph
//         A visual character shape (every codepoint is rendered as
//         some glyph)
//
//      Glyph index
//         A font-specific integer ID representing a glyph
//
//      Baseline
//         Glyph shapes are defined relative to a baseline, which is the
//         bottom of uppercase characters. Characters extend both above
//         and below the baseline.
//
//      Current Point
//         As you draw text to the screen, you keep track of a "current point"
//         which is the origin of each character. The current point's vertical
//         position is the baseline. Even "baked fonts" use this model.
//
//      Vertical Font Metrics
//         The vertical qualities of the font, used to vertically position
//         and space the characters. See docs for stbtt_GetFontVMetrics.
//
//      Font Size in Pixels or Points
//         The preferred interface for specifying font sizes in stb_truetype
//         is to specify how tall the font's vertical extent should be in pixels.
//         If that sounds good enough, skip the next paragraph.
//
//         Most font APIs instead use "points", which are a common typographic
//         measurement for describing font size, defined as 72 points per inch.
//         stb_truetype provides a point API for compatibility. However, true
//         "per inch" conventions don't make much sense on computer displays
//         since different monitors have different number of pixels per
//         inch. For example, Windows traditionally uses a convention that
//         there are 96 pixels per inch, thus making 'inch' measurements have
//         nothing to do with inches, and thus effectively defining a point to
//         be 1.333 pixels. Additionally, the TrueType font data provides
//         an explicit scale factor to scale a given font's glyphs to points,
//         but the author has observed that this scale factor is often wrong
//         for non-commercial fonts, thus making fonts scaled in points
//         according to the TrueType spec incoherently sized in practice.
//
// DETAILED USAGE:
//
//  Scale:
//    Select how high you want the font to be, in points or pixels.
//    Call ScaleForPixelHeight or ScaleForMappingEmToPixels to compute
//    a scale factor SF that will be used by all other functions.
//
//  Baseline:
//    You need to select a y-coordinate that is the baseline of where
//    your text will appear. Call GetFontBoundingBox to get the baseline-relative
//    bounding box for all characters. SF*-y0 will be the distance in pixels
//    that the worst-case character could extend above the baseline, so if
//    you want the top edge of characters to appear at the top of the
//    screen where y=0, then you would set the baseline to SF*-y0.
//
//  Current point:
//    Set the current point where the first character will appear. The
//    first character could extend left of the current point; this is font
//    dependent. You can either choose a current point that is the leftmost
//    point and hope, or add some padding, or check the bounding box or
//    left-side-bearing of the first character to be displayed and set
//    the current point based on that.
//
//  Displaying a character:
//    Compute the bounding box of the character. It will contain signed values
//    relative to <current_point, baseline>. I.e. if it returns x0,y0,x1,y1,
//    then the character should be displayed in the rectangle from
//    <current_point+SF*x0, baseline+SF*y0> to <current_point+SF*x1,baseline+SF*y1).
//
//  Advancing for the next character:
//    Call GlyphHMetrics, and compute 'current_point += SF * advance'.
//
//
// ADVANCED USAGE
//
//   Quality:
//
//    - Use the functions with Subpixel at the end to allow your characters
//      to have subpixel positioning. Since the font is anti-aliased, not
//      hinted, this is very import for quality. (This is not possible with
//      baked fonts.)
//
//    - Kerning is now supported, and if you're supporting subpixel rendering
//      then kerning is worth using to give your text a polished look.
//
//   Performance:
//
//    - Convert Unicode codepoints to glyph indexes and operate on the glyphs;
//      if you don't do this, stb_truetype is forced to do the conversion on
//      every call.
//
//    - There are a lot of memory allocations. We should modify it to take
//      a temp buffer and allocate from the temp buffer (without freeing),
//      should help performance a lot.
//
// NOTES
//
//   The system uses the raw data found in the .ttf file without changing it
//   and without building auxiliary data structures. This is a bit inefficient
//   on little-endian systems (the data is big-endian), but assuming you're
//   caching the bitmaps or glyph shapes this shouldn't be a big deal.
//
//   It appears to be very hard to programmatically determine what font a
//   given file is in a general way. I provide an API for this, but I don't
//   recommend it.
//
//
// PERFORMANCE MEASUREMENTS FOR 1.06:
//
//                      32-bit     64-bit
//   Previous release:  8.83 s     7.68 s
//   Pool allocations:  7.72 s     6.34 s
//   Inline sort     :  6.54 s     5.65 s
//   New rasterizer  :  5.63 s     5.00 s


//////////////////////////////////////////////////////////////////////////////
////
////   INTEGRATION WITH YOUR CODEBASE
////
////   The following sections allow you to supply alternate definitions
////   of C library functions used by stb_truetype, e.g. if you don't
////   link with the C runtime library.

#if defined(STB_TRUETYPE_IMPLEMENTATION) || defined(ZEST_MSDF_IMPLEMENTATION)
// #define your own (u)stbtt_int8/16/32 before including to override this
#ifndef stbtt_uint8
typedef unsigned char   stbtt_uint8;
typedef signed   char   stbtt_int8;
typedef unsigned short  stbtt_uint16;
typedef signed   short  stbtt_int16;
typedef unsigned int    stbtt_uint32;
typedef signed   int    stbtt_int32;
#endif

typedef char stbtt__check_size32[sizeof(stbtt_int32)==4 ? 1 : -1];
typedef char stbtt__check_size16[sizeof(stbtt_int16)==2 ? 1 : -1];

// e.g. #define your own STBTT_ifloor/STBTT_iceil() to avoid math.h
#ifndef STBTT_ifloor
#include <math.h>
#define STBTT_ifloor(x)   ((int) floor(x))
#define STBTT_iceil(x)    ((int) ceil(x))
#endif

#ifndef STBTT_sqrt
#include <math.h>
#define STBTT_sqrt(x)      sqrt(x)
#define STBTT_pow(x,y)     pow(x,y)
#endif

#ifndef STBTT_fmod
#include <math.h>
#define STBTT_fmod(x,y)    fmod(x,y)
#endif

#ifndef STBTT_cos
#include <math.h>
#define STBTT_cos(x)       cos(x)
#define STBTT_acos(x)      acos(x)
#endif

#ifndef STBTT_fabs
#include <math.h>
#define STBTT_fabs(x)      fabs(x)
#endif

// #define your own functions "STBTT_malloc" / "STBTT_free" to avoid malloc.h
#ifndef STBTT_malloc
#include <stdlib.h>
#define STBTT_malloc(x,u)  ((void)(u),malloc(x))
#define STBTT_free(x,u)    ((void)(u),free(x))
#endif

#ifndef STBTT_assert
#include <assert.h>
#define STBTT_assert(x)    assert(x)
#endif

#ifndef STBTT_strlen
#include <string.h>
#define STBTT_strlen(x)    strlen(x)
#endif

#ifndef STBTT_memcpy
#include <string.h>
#define STBTT_memcpy       memcpy
#define STBTT_memset       memset
#endif
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
////
////   INTERFACE
////
////

#ifndef __STB_INCLUDE_STB_TRUETYPE_H__
#define __STB_INCLUDE_STB_TRUETYPE_H__

#ifdef STBTT_STATIC
#define STBTT_DEF static
#else
#define STBTT_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
	#endif

	// private structure
	typedef struct
	{
		unsigned char *data;
		int cursor;
		int size;
	} stbtt__buf;

	//////////////////////////////////////////////////////////////////////////////
	//
	// TEXTURE BAKING API
	//
	// If you use this API, you only have to call two functions ever.
	//

	typedef struct
	{
		unsigned short x0,y0,x1,y1; // coordinates of bbox in bitmap
		float xoff,yoff,xadvance;
	} stbtt_bakedchar;

	STBTT_DEF int stbtt_BakeFontBitmap(const unsigned char *data, int offset,  // font location (use offset=0 for plain .ttf)
									   float pixel_height,                     // height of font in pixels
									   unsigned char *pixels, int pw, int ph,  // bitmap to be filled in
									   int first_char, int num_chars,          // characters to bake
									   stbtt_bakedchar *chardata);             // you allocate this, it's num_chars long
	// if return is positive, the first unused row of the bitmap
	// if return is negative, returns the negative of the number of characters that fit
	// if return is 0, no characters fit and no rows were used
	// This uses a very crappy packing.

	typedef struct
	{
		float x0,y0,s0,t0; // top-left
		float x1,y1,s1,t1; // bottom-right
	} stbtt_aligned_quad;

	STBTT_DEF void stbtt_GetBakedQuad(const stbtt_bakedchar *chardata, int pw, int ph,  // same data as above
									  int char_index,             // character to display
									  float *xpos, float *ypos,   // pointers to current position in screen pixel space
									  stbtt_aligned_quad *q,      // output: quad to draw
									  int opengl_fillrule);       // true if opengl fill rule; false if DX9 or earlier
	// Call GetBakedQuad with char_index = 'character - first_char', and it
	// creates the quad you need to draw and advances the current position.
	//
	// The coordinate system used assumes y increases downwards.
	//
	// Characters will extend both above and below the current position;
	// see discussion of "BASELINE" above.
	//
	// It's inefficient; you might want to c&p it and optimize it.

	STBTT_DEF void stbtt_GetScaledFontVMetrics(const unsigned char *fontdata, int index, float size, float *ascent, float *descent, float *lineGap);
	// Query the font vertical metrics without having to create a font first.


	//////////////////////////////////////////////////////////////////////////////
	//
	// NEW TEXTURE BAKING API
	//
	// This provides options for packing multiple fonts into one atlas, not
	// perfectly but better than nothing.

	typedef struct
	{
		unsigned short x0,y0,x1,y1; // coordinates of bbox in bitmap
		float xoff,yoff,xadvance;
		float xoff2,yoff2;
	} stbtt_packedchar;

	typedef struct stbtt_pack_context stbtt_pack_context;
	typedef struct stbtt_fontinfo stbtt_fontinfo;
	#ifndef STB_RECT_PACK_VERSION
	typedef struct stbrp_rect stbrp_rect;
	#endif

	STBTT_DEF int  stbtt_PackBegin(stbtt_pack_context *spc, unsigned char *pixels, int width, int height, int stride_in_bytes, int padding, void *alloc_context);
	// Initializes a packing context stored in the passed-in stbtt_pack_context.
	// Future calls using this context will pack characters into the bitmap passed
	// in here: a 1-channel bitmap that is width * height. stride_in_bytes is
	// the distance from one row to the next (or 0 to mean they are packed tightly
	// together). "padding" is the amount of padding to leave between each
	// character (normally you want '1' for bitmaps you'll use as textures with
	// bilinear filtering).
	//
	// Returns 0 on failure, 1 on success.

	STBTT_DEF void stbtt_PackEnd  (stbtt_pack_context *spc);
	// Cleans up the packing context and frees all memory.

	#define STBTT_POINT_SIZE(x)   (-(x))

	STBTT_DEF int  stbtt_PackFontRange(stbtt_pack_context *spc, const unsigned char *fontdata, int font_index, float font_size,
									   int first_unicode_char_in_range, int num_chars_in_range, stbtt_packedchar *chardata_for_range);
	// Creates character bitmaps from the font_index'th font found in fontdata (use
	// font_index=0 if you don't know what that is). It creates num_chars_in_range
	// bitmaps for characters with unicode values starting at first_unicode_char_in_range
	// and increasing. Data for how to render them is stored in chardata_for_range;
	// pass these to stbtt_GetPackedQuad to get back renderable quads.
	//
	// font_size is the full height of the character from ascender to descender,
	// as computed by stbtt_ScaleForPixelHeight. To use a point size as computed
	// by stbtt_ScaleForMappingEmToPixels, wrap the point size in STBTT_POINT_SIZE()
	// and pass that result as 'font_size':
	//       ...,                  20 , ... // font max minus min y is 20 pixels tall
	//       ..., STBTT_POINT_SIZE(20), ... // 'M' is 20 pixels tall

	typedef struct
	{
		float font_size;
		int first_unicode_codepoint_in_range;  // if non-zero, then the chars are continuous, and this is the first codepoint
		int *array_of_unicode_codepoints;       // if non-zero, then this is an array of unicode codepoints
		int num_chars;
		stbtt_packedchar *chardata_for_range; // output
		unsigned char h_oversample, v_oversample; // don't set these, they're used internally
	} stbtt_pack_range;

	STBTT_DEF int  stbtt_PackFontRanges(stbtt_pack_context *spc, const unsigned char *fontdata, int font_index, stbtt_pack_range *ranges, int num_ranges);
	// Creates character bitmaps from multiple ranges of characters stored in
	// ranges. This will usually create a better-packed bitmap than multiple
	// calls to stbtt_PackFontRange. Note that you can call this multiple
	// times within a single PackBegin/PackEnd.

	STBTT_DEF void stbtt_PackSetOversampling(stbtt_pack_context *spc, unsigned int h_oversample, unsigned int v_oversample);
	// Oversampling a font increases the quality by allowing higher-quality subpixel
	// positioning, and is especially valuable at smaller text sizes.
	//
	// This function sets the amount of oversampling for all following calls to
	// stbtt_PackFontRange(s) or stbtt_PackFontRangesGatherRects for a given
	// pack context. The default (no oversampling) is achieved by h_oversample=1
	// and v_oversample=1. The total number of pixels required is
	// h_oversample*v_oversample larger than the default; for example, 2x2
	// oversampling requires 4x the storage of 1x1. For best results, render
	// oversampled textures with bilinear filtering. Look at the readme in
	// stb/tests/oversample for information about oversampled fonts
	//
	// To use with PackFontRangesGather etc., you must set it before calls
	// call to PackFontRangesGatherRects.

	STBTT_DEF void stbtt_PackSetSkipMissingCodepoints(stbtt_pack_context *spc, int skip);
	// If skip != 0, this tells stb_truetype to skip any codepoints for which
	// there is no corresponding glyph. If skip=0, which is the default, then
	// codepoints without a glyph recived the font's "missing character" glyph,
	// typically an empty box by convention.

	STBTT_DEF void stbtt_GetPackedQuad(const stbtt_packedchar *chardata, int pw, int ph,  // same data as above
									   int char_index,             // character to display
									   float *xpos, float *ypos,   // pointers to current position in screen pixel space
									   stbtt_aligned_quad *q,      // output: quad to draw
									   int align_to_integer);

	STBTT_DEF int  stbtt_PackFontRangesGatherRects(stbtt_pack_context *spc, const stbtt_fontinfo *info, stbtt_pack_range *ranges, int num_ranges, stbrp_rect *rects);
	STBTT_DEF void stbtt_PackFontRangesPackRects(stbtt_pack_context *spc, stbrp_rect *rects, int num_rects);
	STBTT_DEF int  stbtt_PackFontRangesRenderIntoRects(stbtt_pack_context *spc, const stbtt_fontinfo *info, stbtt_pack_range *ranges, int num_ranges, stbrp_rect *rects);
	// Calling these functions in sequence is roughly equivalent to calling
	// stbtt_PackFontRanges(). If you more control over the packing of multiple
	// fonts, or if you want to pack custom data into a font texture, take a look
	// at the source to of stbtt_PackFontRanges() and create a custom version
	// using these functions, e.g. call GatherRects multiple times,
	// building up a single array of rects, then call PackRects once,
	// then call RenderIntoRects repeatedly. This may result in a
	// better packing than calling PackFontRanges multiple times
	// (or it may not).

	// this is an opaque structure that you shouldn't mess with which holds
	// all the context needed from PackBegin to PackEnd.
	struct stbtt_pack_context {
		void *user_allocator_context;
		void *pack_info;
		int   width;
		int   height;
		int   stride_in_bytes;
		int   padding;
		int   skip_missing;
		unsigned int   h_oversample, v_oversample;
		unsigned char *pixels;
		void  *nodes;
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	// FONT LOADING
	//
	//

	STBTT_DEF int stbtt_GetNumberOfFonts(const unsigned char *data);
	// This function will determine the number of fonts in a font file.  TrueType
	// collection (.ttc) files may contain multiple fonts, while TrueType font
	// (.ttf) files only contain one font. The number of fonts can be used for
	// indexing with the previous function where the index is between zero and one
	// less than the total fonts. If an error occurs, -1 is returned.

	STBTT_DEF int stbtt_GetFontOffsetForIndex(const unsigned char *data, int index);
	// Each .ttf/.ttc file may have more than one font. Each font has a sequential
	// index number starting from 0. Call this function to get the font offset for
	// a given index; it returns -1 if the index is out of range. A regular .ttf
	// file will only define one font and it always be at offset 0, so it will
	// return '0' for index 0, and -1 for all other indices.

	// The following structure is defined publicly so you can declare one on
	// the stack or as a global or etc, but you should treat it as opaque.
	struct stbtt_fontinfo
	{
		void           * userdata;
		unsigned char  * data;              // pointer to .ttf file
		int              fontstart;         // offset of start of font

		int numGlyphs;                     // number of glyphs, needed for range checking

		int loca,head,glyf,hhea,hmtx,kern,gpos,svg; // table locations as offset from start of .ttf
		int index_map;                     // a cmap mapping for our chosen character encoding
		int indexToLocFormat;              // format needed to map from glyph index to glyph

		stbtt__buf cff;                    // cff font data
		stbtt__buf charstrings;            // the charstring index
		stbtt__buf gsubrs;                 // global charstring subroutines index
		stbtt__buf subrs;                  // private charstring subroutines index
		stbtt__buf fontdicts;              // array of font dicts
		stbtt__buf fdselect;               // map from glyph to fontdict
	};

	STBTT_DEF int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int offset);
	// Given an offset into the file that defines a font, this function builds
	// the necessary cached info for the rest of the system. You must allocate
	// the stbtt_fontinfo yourself, and stbtt_InitFont will fill it out. You don't
	// need to do anything special to free it, because the contents are pure
	// value data with no additional data structures. Returns 0 on failure.


	//////////////////////////////////////////////////////////////////////////////
	//
	// CHARACTER TO GLYPH-INDEX CONVERSIOn

	STBTT_DEF int stbtt_FindGlyphIndex(const stbtt_fontinfo *info, int unicode_codepoint);
	// If you're going to perform multiple operations on the same character
	// and you want a speed-up, call this function with the character you're
	// going to process, then use glyph-based functions instead of the
	// codepoint-based functions.
	// Returns 0 if the character codepoint is not defined in the font.


	//////////////////////////////////////////////////////////////////////////////
	//
	// CHARACTER PROPERTIES
	//

	STBTT_DEF float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float pixels);
	// computes a scale factor to produce a font whose "height" is 'pixels' tall.
	// Height is measured as the distance from the highest ascender to the lowest
	// descender; in other words, it's equivalent to calling stbtt_GetFontVMetrics
	// and computing:
	//       scale = pixels / (ascent - descent)
	// so if you prefer to measure height by the ascent only, use a similar calculation.

	STBTT_DEF float stbtt_ScaleForMappingEmToPixels(const stbtt_fontinfo *info, float pixels);
	// computes a scale factor to produce a font whose EM size is mapped to
	// 'pixels' tall. This is probably what traditional APIs compute, but
	// I'm not positive.

	STBTT_DEF void stbtt_GetFontVMetrics(const stbtt_fontinfo *info, int *ascent, int *descent, int *lineGap);
	// ascent is the coordinate above the baseline the font extends; descent
	// is the coordinate below the baseline the font extends (i.e. it is typically negative)
	// lineGap is the spacing between one row's descent and the next row's ascent...
	// so you should advance the vertical position by "*ascent - *descent + *lineGap"
	//   these are expressed in unscaled coordinates, so you must multiply by
	//   the scale factor for a given size

	STBTT_DEF int  stbtt_GetFontVMetricsOS2(const stbtt_fontinfo *info, int *typoAscent, int *typoDescent, int *typoLineGap);
	// analogous to GetFontVMetrics, but returns the "typographic" values from the OS/2
	// table (specific to MS/Windows TTF files).
	//
	// Returns 1 on success (table present), 0 on failure.

	STBTT_DEF void stbtt_GetFontBoundingBox(const stbtt_fontinfo *info, int *x0, int *y0, int *x1, int *y1);
	// the bounding box around all possible characters

	STBTT_DEF void stbtt_GetCodepointHMetrics(const stbtt_fontinfo *info, int codepoint, int *advanceWidth, int *leftSideBearing);
	// leftSideBearing is the offset from the current horizontal position to the left edge of the character
	// advanceWidth is the offset from the current horizontal position to the next horizontal position
	//   these are expressed in unscaled coordinates

	STBTT_DEF int  stbtt_GetCodepointKernAdvance(const stbtt_fontinfo *info, int ch1, int ch2);
	// an additional amount to add to the 'advance' value between ch1 and ch2

	STBTT_DEF int stbtt_GetCodepointBox(const stbtt_fontinfo *info, int codepoint, int *x0, int *y0, int *x1, int *y1);
	// Gets the bounding box of the visible part of the glyph, in unscaled coordinates

	STBTT_DEF void stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing);
	STBTT_DEF int  stbtt_GetGlyphKernAdvance(const stbtt_fontinfo *info, int glyph1, int glyph2);
	STBTT_DEF int  stbtt_GetGlyphBox(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1);
	// as above, but takes one or more glyph indices for greater efficiency

	typedef struct stbtt_kerningentry
	{
		int glyph1; // use stbtt_FindGlyphIndex
		int glyph2;
		int advance;
	} stbtt_kerningentry;

	STBTT_DEF int  stbtt_GetKerningTableLength(const stbtt_fontinfo *info);
	STBTT_DEF int  stbtt_GetKerningTable(const stbtt_fontinfo *info, stbtt_kerningentry* table, int table_length);
	// Retrieves a complete list of all of the kerning pairs provided by the font
	// stbtt_GetKerningTable never writes more than table_length entries and returns how many entries it did write.
	// The table will be sorted by (a.glyph1 == b.glyph1)?(a.glyph2 < b.glyph2):(a.glyph1 < b.glyph1)

	//////////////////////////////////////////////////////////////////////////////
	//
	// GLYPH SHAPES (you probably don't need these, but they have to go before
	// the bitmaps for C declaration-order reasons)
	//

	#ifndef STBTT_vmove // you can predefine these to use different values (but why?)
	enum {
		STBTT_vmove=1,
		STBTT_vline,
		STBTT_vcurve,
		STBTT_vcubic
	};
	#endif

	#ifndef stbtt_vertex // you can predefine this to use different values
	// (we share this with other code at RAD)
	#define stbtt_vertex_type short // can't use stbtt_int16 because that's not visible in the header file
	typedef struct
	{
		stbtt_vertex_type x,y,cx,cy,cx1,cy1;
		unsigned char type,padding;
	} stbtt_vertex;
	#endif

	STBTT_DEF int stbtt_IsGlyphEmpty(const stbtt_fontinfo *info, int glyph_index);
	// returns non-zero if nothing is drawn for this glyph

	STBTT_DEF int stbtt_GetCodepointShape(const stbtt_fontinfo *info, int unicode_codepoint, stbtt_vertex **vertices);
	STBTT_DEF int stbtt_GetGlyphShape(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **vertices);
	// returns # of vertices and fills *vertices with the pointer to them
	//   these are expressed in "unscaled" coordinates
	//
	// The shape is a series of contours. Each one starts with
	// a STBTT_moveto, then consists of a series of mixed
	// STBTT_lineto and STBTT_curveto segments. A lineto
	// draws a line from previous endpoint to its x,y; a curveto
	// draws a quadratic bezier from previous endpoint to
	// its x,y, using cx,cy as the bezier control point.

	STBTT_DEF void stbtt_FreeShape(const stbtt_fontinfo *info, stbtt_vertex *vertices);
	// frees the data allocated above

	STBTT_DEF unsigned char *stbtt_FindSVGDoc(const stbtt_fontinfo *info, int gl);
	STBTT_DEF int stbtt_GetCodepointSVG(const stbtt_fontinfo *info, int unicode_codepoint, const char **svg);
	STBTT_DEF int stbtt_GetGlyphSVG(const stbtt_fontinfo *info, int gl, const char **svg);
	// fills svg with the character's SVG data.
	// returns data size or 0 if SVG not found.

	//////////////////////////////////////////////////////////////////////////////
	//
	// BITMAP RENDERING
	//

	STBTT_DEF void stbtt_FreeBitmap(unsigned char *bitmap, void *userdata);
	// frees the bitmap allocated below

	STBTT_DEF unsigned char *stbtt_GetCodepointBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y, int codepoint, int *width, int *height, int *xoff, int *yoff);
	// allocates a large-enough single-channel 8bpp bitmap and renders the
	// specified character/glyph at the specified scale into it, with
	// antialiasing. 0 is no coverage (transparent), 255 is fully covered (opaque).
	// *width & *height are filled out with the width & height of the bitmap,
	// which is stored left-to-right, top-to-bottom.
	//
	// xoff/yoff are the offset it pixel space from the glyph origin to the top-left of the bitmap

	STBTT_DEF unsigned char *stbtt_GetCodepointBitmapSubpixel(const stbtt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint, int *width, int *height, int *xoff, int *yoff);
	// the same as stbtt_GetCodepoitnBitmap, but you can specify a subpixel
	// shift for the character

	STBTT_DEF void stbtt_MakeCodepointBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int codepoint);
	// the same as stbtt_GetCodepointBitmap, but you pass in storage for the bitmap
	// in the form of 'output', with row spacing of 'out_stride' bytes. the bitmap
	// is clipped to out_w/out_h bytes. Call stbtt_GetCodepointBitmapBox to get the
	// width and height and positioning info for it first.

	STBTT_DEF void stbtt_MakeCodepointBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint);
	// same as stbtt_MakeCodepointBitmap, but you can specify a subpixel
	// shift for the character

	STBTT_DEF void stbtt_MakeCodepointBitmapSubpixelPrefilter(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int oversample_x, int oversample_y, float *sub_x, float *sub_y, int codepoint);
	// same as stbtt_MakeCodepointBitmapSubpixel, but prefiltering
	// is performed (see stbtt_PackSetOversampling)

	STBTT_DEF void stbtt_GetCodepointBitmapBox(const stbtt_fontinfo *font, int codepoint, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);
	// get the bbox of the bitmap centered around the glyph origin; so the
	// bitmap width is ix1-ix0, height is iy1-iy0, and location to place
	// the bitmap top left is (leftSideBearing*scale,iy0).
	// (Note that the bitmap uses y-increases-down, but the shape uses
	// y-increases-up, so CodepointBitmapBox and CodepointBox are inverted.)

	STBTT_DEF void stbtt_GetCodepointBitmapBoxSubpixel(const stbtt_fontinfo *font, int codepoint, float scale_x, float scale_y, float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1);
	// same as stbtt_GetCodepointBitmapBox, but you can specify a subpixel
	// shift for the character

	// the following functions are equivalent to the above functions, but operate
	// on glyph indices instead of Unicode codepoints (for efficiency)
	STBTT_DEF unsigned char *stbtt_GetGlyphBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y, int glyph, int *width, int *height, int *xoff, int *yoff);
	STBTT_DEF unsigned char *stbtt_GetGlyphBitmapSubpixel(const stbtt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int glyph, int *width, int *height, int *xoff, int *yoff);
	STBTT_DEF void stbtt_MakeGlyphBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int glyph);
	STBTT_DEF void stbtt_MakeGlyphBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int glyph);
	STBTT_DEF void stbtt_MakeGlyphBitmapSubpixelPrefilter(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int oversample_x, int oversample_y, float *sub_x, float *sub_y, int glyph);
	STBTT_DEF void stbtt_GetGlyphBitmapBox(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);
	STBTT_DEF void stbtt_GetGlyphBitmapBoxSubpixel(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y,float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1);


	// @TODO: don't expose this structure
	typedef struct
	{
		int w,h,stride;
		unsigned char *pixels;
	} stbtt__bitmap;

	// rasterize a shape with quadratic beziers into a bitmap
	STBTT_DEF void stbtt_Rasterize(stbtt__bitmap *result,        // 1-channel bitmap to draw into
								   float flatness_in_pixels,     // allowable error of curve in pixels
								   stbtt_vertex *vertices,       // array of vertices defining shape
								   int num_verts,                // number of vertices in above array
								   float scale_x, float scale_y, // scale applied to input vertices
								   float shift_x, float shift_y, // translation applied to input vertices
								   int x_off, int y_off,         // another translation applied to input
								   int invert,                   // if non-zero, vertically flip shape
								   void *userdata);              // context for to STBTT_MALLOC

	//////////////////////////////////////////////////////////////////////////////
	//
	// Signed Distance Function (or Field) rendering

	STBTT_DEF void stbtt_FreeSDF(unsigned char *bitmap, void *userdata);
	// frees the SDF bitmap allocated below

	STBTT_DEF unsigned char * stbtt_GetGlyphSDF(const stbtt_fontinfo *info, float scale, int glyph, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff);
	STBTT_DEF unsigned char * stbtt_GetCodepointSDF(const stbtt_fontinfo *info, float scale, int codepoint, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff);
	// These functions compute a discretized SDF field for a single character, suitable for storing
	// in a single-channel texture, sampling with bilinear filtering, and testing against
	// larger than some threshold to produce scalable fonts.
	//        info              --  the font
	//        scale             --  controls the size of the resulting SDF bitmap, same as it would be creating a regular bitmap
	//        glyph/codepoint   --  the character to generate the SDF for
	//        padding           --  extra "pixels" around the character which are filled with the distance to the character (not 0),
	//                                 which allows effects like bit outlines
	//        onedge_value      --  value 0-255 to test the SDF against to reconstruct the character (i.e. the isocontour of the character)
	//        pixel_dist_scale  --  what value the SDF should increase by when moving one SDF "pixel" away from the edge (on the 0..255 scale)
	//                                 if positive, > onedge_value is inside; if negative, < onedge_value is inside
	//        width,height      --  output height & width of the SDF bitmap (including padding)
	//        xoff,yoff         --  output origin of the character
	//        return value      --  a 2D array of bytes 0..255, width*height in size
	//
	// pixel_dist_scale & onedge_value are a scale & bias that allows you to make
	// optimal use of the limited 0..255 for your application, trading off precision
	// and special effects. SDF values outside the range 0..255 are clamped to 0..255.
	//
	// Example:
	//      scale = stbtt_ScaleForPixelHeight(22)
	//      padding = 5
	//      onedge_value = 180
	//      pixel_dist_scale = 180/5.0 = 36.0
	//
	//      This will create an SDF bitmap in which the character is about 22 pixels
	//      high but the whole bitmap is about 22+5+5=32 pixels high. To produce a filled
	//      shape, sample the SDF at each pixel and fill the pixel if the SDF value
	//      is greater than or equal to 180/255. (You'll actually want to antialias,
	//      which is beyond the scope of this example.) Additionally, you can compute
	//      offset outlines (e.g. to stroke the character border inside & outside,
	//      or only outside). For example, to fill outside the character up to 3 SDF
	//      pixels, you would compare against (180-36.0*3)/255 = 72/255. The above
	//      choice of variables maps a range from 5 pixels outside the shape to
	//      2 pixels inside the shape to 0..255; this is intended primarily for apply
	//      outside effects only (the interior range is needed to allow proper
	//      antialiasing of the font at *smaller* sizes)
	//
	// The function computes the SDF analytically at each SDF pixel, not by e.g.
	// building a higher-res bitmap and approximating it. In theory the quality
	// should be as high as possible for an SDF of this size & representation, but
	// unclear if this is true in practice (perhaps building a higher-res bitmap
	// and computing from that can allow drop-out prevention).
	//
	// The algorithm has not been optimized at all, so expect it to be slow
	// if computing lots of characters or very large sizes.



	//////////////////////////////////////////////////////////////////////////////
	//
	// Finding the right font...
	//
	// You should really just solve this offline, keep your own tables
	// of what font is what, and don't try to get it out of the .ttf file.
	// That's because getting it out of the .ttf file is really hard, because
	// the names in the file can appear in many possible encodings, in many
	// possible languages, and e.g. if you need a case-insensitive comparison,
	// the details of that depend on the encoding & language in a complex way
	// (actually underspecified in truetype, but also gigantic).
	//
	// But you can use the provided functions in two possible ways:
	//     stbtt_FindMatchingFont() will use *case-sensitive* comparisons on
	//             unicode-encoded names to try to find the font you want;
	//             you can run this before calling stbtt_InitFont()
	//
	//     stbtt_GetFontNameString() lets you get any of the various strings
	//             from the file yourself and do your own comparisons on them.
	//             You have to have called stbtt_InitFont() first.


	STBTT_DEF int stbtt_FindMatchingFont(const unsigned char *fontdata, const char *name, int flags);
	// returns the offset (not index) of the font that matches, or -1 if none
	//   if you use STBTT_MACSTYLE_DONTCARE, use a font name like "Arial Bold".
	//   if you use any other flag, use a font name like "Arial"; this checks
	//     the 'macStyle' header field; i don't know if fonts set this consistently
	#define STBTT_MACSTYLE_DONTCARE     0
	#define STBTT_MACSTYLE_BOLD         1
	#define STBTT_MACSTYLE_ITALIC       2
	#define STBTT_MACSTYLE_UNDERSCORE   4
	#define STBTT_MACSTYLE_NONE         8   // <= not same as 0, this makes us check the bitfield is 0

	STBTT_DEF int stbtt_CompareUTF8toUTF16_bigendian(const char *s1, int len1, const char *s2, int len2);
	// returns 1/0 whether the first string interpreted as utf8 is identical to
	// the second string interpreted as big-endian utf16... useful for strings from next func

	STBTT_DEF const char *stbtt_GetFontNameString(const stbtt_fontinfo *font, int *length, int platformID, int encodingID, int languageID, int nameID);
	// returns the string (which may be big-endian double byte, e.g. for unicode)
	// and puts the length in bytes in *length.
	//
	// some of the values for the IDs are below; for more see the truetype spec:
	//     http://developer.apple.com/textfonts/TTRefMan/RM06/Chap6name.html
	//     http://www.microsoft.com/typography/otspec/name.htm

	enum { // platformID
		STBTT_PLATFORM_ID_UNICODE   =0,
		STBTT_PLATFORM_ID_MAC       =1,
		STBTT_PLATFORM_ID_ISO       =2,
		STBTT_PLATFORM_ID_MICROSOFT =3
	};

	enum { // encodingID for STBTT_PLATFORM_ID_UNICODE
		STBTT_UNICODE_EID_UNICODE_1_0    =0,
		STBTT_UNICODE_EID_UNICODE_1_1    =1,
		STBTT_UNICODE_EID_ISO_10646      =2,
		STBTT_UNICODE_EID_UNICODE_2_0_BMP=3,
		STBTT_UNICODE_EID_UNICODE_2_0_FULL=4
	};

	enum { // encodingID for STBTT_PLATFORM_ID_MICROSOFT
		STBTT_MS_EID_SYMBOL        =0,
		STBTT_MS_EID_UNICODE_BMP   =1,
		STBTT_MS_EID_SHIFTJIS      =2,
		STBTT_MS_EID_UNICODE_FULL  =10
	};

	enum { // encodingID for STBTT_PLATFORM_ID_MAC; same as Script Manager codes
		STBTT_MAC_EID_ROMAN        =0,   STBTT_MAC_EID_ARABIC       =4,
		STBTT_MAC_EID_JAPANESE     =1,   STBTT_MAC_EID_HEBREW       =5,
		STBTT_MAC_EID_CHINESE_TRAD =2,   STBTT_MAC_EID_GREEK        =6,
		STBTT_MAC_EID_KOREAN       =3,   STBTT_MAC_EID_RUSSIAN      =7
	};

	enum { // languageID for STBTT_PLATFORM_ID_MICROSOFT; same as LCID...
		// problematic because there are e.g. 16 english LCIDs and 16 arabic LCIDs
		STBTT_MS_LANG_ENGLISH     =0x0409,   STBTT_MS_LANG_ITALIAN     =0x0410,
		STBTT_MS_LANG_CHINESE     =0x0804,   STBTT_MS_LANG_JAPANESE    =0x0411,
		STBTT_MS_LANG_DUTCH       =0x0413,   STBTT_MS_LANG_KOREAN      =0x0412,
		STBTT_MS_LANG_FRENCH      =0x040c,   STBTT_MS_LANG_RUSSIAN     =0x0419,
		STBTT_MS_LANG_GERMAN      =0x0407,   STBTT_MS_LANG_SPANISH     =0x0409,
		STBTT_MS_LANG_HEBREW      =0x040d,   STBTT_MS_LANG_SWEDISH     =0x041D
	};

	enum { // languageID for STBTT_PLATFORM_ID_MAC
		STBTT_MAC_LANG_ENGLISH      =0 ,   STBTT_MAC_LANG_JAPANESE     =11,
		STBTT_MAC_LANG_ARABIC       =12,   STBTT_MAC_LANG_KOREAN       =23,
		STBTT_MAC_LANG_DUTCH        =4 ,   STBTT_MAC_LANG_RUSSIAN      =32,
		STBTT_MAC_LANG_FRENCH       =1 ,   STBTT_MAC_LANG_SPANISH      =6 ,
		STBTT_MAC_LANG_GERMAN       =2 ,   STBTT_MAC_LANG_SWEDISH      =5 ,
		STBTT_MAC_LANG_HEBREW       =10,   STBTT_MAC_LANG_CHINESE_SIMPLIFIED =33,
		STBTT_MAC_LANG_ITALIAN      =3 ,   STBTT_MAC_LANG_CHINESE_TRAD =19
	};

	#ifdef __cplusplus
}
#endif

#endif // __STB_INCLUDE_STB_TRUETYPE_H__

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
////
////   IMPLEMENTATION
////
////

#if defined(STB_TRUETYPE_IMPLEMENTATION) || defined(ZEST_MSDF_IMPLEMENTATION)

#ifndef STBTT_MAX_OVERSAMPLE
#define STBTT_MAX_OVERSAMPLE   8
#endif

#if STBTT_MAX_OVERSAMPLE > 255
#error "STBTT_MAX_OVERSAMPLE cannot be > 255"
#endif

typedef int stbtt__test_oversample_pow2[(STBTT_MAX_OVERSAMPLE & (STBTT_MAX_OVERSAMPLE-1)) == 0 ? 1 : -1];

#ifndef STBTT_RASTERIZER_VERSION
#define STBTT_RASTERIZER_VERSION 2
#endif

#ifdef _MSC_VER
#define STBTT__NOTUSED(v)  (void)(v)
#else
#define STBTT__NOTUSED(v)  (void)sizeof(v)
#endif

//////////////////////////////////////////////////////////////////////////
//
// stbtt__buf helpers to parse data from file
//

static stbtt_uint8 stbtt__buf_get8(stbtt__buf *b)
{
	if (b->cursor >= b->size)
		return 0;
	return b->data[b->cursor++];
}

static stbtt_uint8 stbtt__buf_peek8(stbtt__buf *b)
{
	if (b->cursor >= b->size)
		return 0;
	return b->data[b->cursor];
}

static void stbtt__buf_seek(stbtt__buf *b, int o)
{
	STBTT_assert(!(o > b->size || o < 0));
	b->cursor = (o > b->size || o < 0) ? b->size : o;
}

static void stbtt__buf_skip(stbtt__buf *b, int o)
{
	stbtt__buf_seek(b, b->cursor + o);
}

static stbtt_uint32 stbtt__buf_get(stbtt__buf *b, int n)
{
	stbtt_uint32 v = 0;
	int i;
	STBTT_assert(n >= 1 && n <= 4);
	for (i = 0; i < n; i++)
		v = (v << 8) | stbtt__buf_get8(b);
	return v;
}

static stbtt__buf stbtt__new_buf(const void *p, size_t size)
{
	stbtt__buf r;
	STBTT_assert(size < 0x40000000);
	r.data = (stbtt_uint8*) p;
	r.size = (int) size;
	r.cursor = 0;
	return r;
}

#define stbtt__buf_get16(b)  stbtt__buf_get((b), 2)
#define stbtt__buf_get32(b)  stbtt__buf_get((b), 4)

static stbtt__buf stbtt__buf_range(const stbtt__buf *b, int o, int s)
{
	stbtt__buf r = stbtt__new_buf(NULL, 0);
	if (o < 0 || s < 0 || o > b->size || s > b->size - o) return r;
	r.data = b->data + o;
	r.size = s;
	return r;
}

static stbtt__buf stbtt__cff_get_index(stbtt__buf *b)
{
	int count, start, offsize;
	start = b->cursor;
	count = stbtt__buf_get16(b);
	if (count) {
		offsize = stbtt__buf_get8(b);
		STBTT_assert(offsize >= 1 && offsize <= 4);
		stbtt__buf_skip(b, offsize * count);
		stbtt__buf_skip(b, stbtt__buf_get(b, offsize) - 1);
	}
	return stbtt__buf_range(b, start, b->cursor - start);
}

static stbtt_uint32 stbtt__cff_int(stbtt__buf *b)
{
	int b0 = stbtt__buf_get8(b);
	if (b0 >= 32 && b0 <= 246)       return b0 - 139;
	else if (b0 >= 247 && b0 <= 250) return (b0 - 247)*256 + stbtt__buf_get8(b) + 108;
	else if (b0 >= 251 && b0 <= 254) return -(b0 - 251)*256 - stbtt__buf_get8(b) - 108;
	else if (b0 == 28)               return stbtt__buf_get16(b);
	else if (b0 == 29)               return stbtt__buf_get32(b);
	STBTT_assert(0);
	return 0;
}

static void stbtt__cff_skip_operand(stbtt__buf *b) {
	int v, b0 = stbtt__buf_peek8(b);
	STBTT_assert(b0 >= 28);
	if (b0 == 30) {
		stbtt__buf_skip(b, 1);
		while (b->cursor < b->size) {
			v = stbtt__buf_get8(b);
			if ((v & 0xF) == 0xF || (v >> 4) == 0xF)
				break;
		}
	} else {
		stbtt__cff_int(b);
	}
}

static stbtt__buf stbtt__dict_get(stbtt__buf *b, int key)
{
	stbtt__buf_seek(b, 0);
	while (b->cursor < b->size) {
		int start = b->cursor, end, op;
		while (stbtt__buf_peek8(b) >= 28)
			stbtt__cff_skip_operand(b);
		end = b->cursor;
		op = stbtt__buf_get8(b);
		if (op == 12)  op = stbtt__buf_get8(b) | 0x100;
		if (op == key) return stbtt__buf_range(b, start, end-start);
	}
	return stbtt__buf_range(b, 0, 0);
}

static void stbtt__dict_get_ints(stbtt__buf *b, int key, int outcount, stbtt_uint32 *out)
{
	int i;
	stbtt__buf operands = stbtt__dict_get(b, key);
	for (i = 0; i < outcount && operands.cursor < operands.size; i++)
		out[i] = stbtt__cff_int(&operands);
}

static int stbtt__cff_index_count(stbtt__buf *b)
{
	stbtt__buf_seek(b, 0);
	return stbtt__buf_get16(b);
}

static stbtt__buf stbtt__cff_index_get(stbtt__buf b, int i)
{
	int count, offsize, start, end;
	stbtt__buf_seek(&b, 0);
	count = stbtt__buf_get16(&b);
	offsize = stbtt__buf_get8(&b);
	STBTT_assert(i >= 0 && i < count);
	STBTT_assert(offsize >= 1 && offsize <= 4);
	stbtt__buf_skip(&b, i*offsize);
	start = stbtt__buf_get(&b, offsize);
	end = stbtt__buf_get(&b, offsize);
	return stbtt__buf_range(&b, 2+(count+1)*offsize+start, end - start);
}

//////////////////////////////////////////////////////////////////////////
//
// accessors to parse data from file
//

// on platforms that don't allow misaligned reads, if we want to allow
// truetype fonts that aren't padded to alignment, define ALLOW_UNALIGNED_TRUETYPE

#define ttBYTE(p)     (* (stbtt_uint8 *) (p))
#define ttCHAR(p)     (* (stbtt_int8 *) (p))
#define ttFixed(p)    ttLONG(p)

static stbtt_uint16 ttUSHORT(stbtt_uint8 *p) { return p[0]*256 + p[1]; }
static stbtt_int16 ttSHORT(stbtt_uint8 *p)   { return p[0]*256 + p[1]; }
static stbtt_uint32 ttULONG(stbtt_uint8 *p)  { return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3]; }
static stbtt_int32 ttLONG(stbtt_uint8 *p)    { return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3]; }

#define stbtt_tag4(p,c0,c1,c2,c3) ((p)[0] == (c0) && (p)[1] == (c1) && (p)[2] == (c2) && (p)[3] == (c3))
#define stbtt_tag(p,str)           stbtt_tag4(p,str[0],str[1],str[2],str[3])

static int stbtt__isfont(stbtt_uint8 *font)
{
	// check the version number
	if (stbtt_tag4(font, '1',0,0,0))  return 1; // TrueType 1
	if (stbtt_tag(font, "typ1"))   return 1; // TrueType with type 1 font -- we don't support this!
	if (stbtt_tag(font, "OTTO"))   return 1; // OpenType with CFF
	if (stbtt_tag4(font, 0,1,0,0)) return 1; // OpenType 1.0
	if (stbtt_tag(font, "true"))   return 1; // Apple specification for TrueType fonts
	return 0;
}

// @OPTIMIZE: binary search
static stbtt_uint32 stbtt__find_table(stbtt_uint8 *data, stbtt_uint32 fontstart, const char *tag)
{
	stbtt_int32 num_tables = ttUSHORT(data+fontstart+4);
	stbtt_uint32 tabledir = fontstart + 12;
	stbtt_int32 i;
	for (i=0; i < num_tables; ++i) {
		stbtt_uint32 loc = tabledir + 16*i;
		if (stbtt_tag(data+loc+0, tag))
			return ttULONG(data+loc+8);
	}
	return 0;
}

static int stbtt_GetFontOffsetForIndex_internal(unsigned char *font_collection, int index)
{
	// if it's just a font, there's only one valid index
	if (stbtt__isfont(font_collection))
		return index == 0 ? 0 : -1;

	// check if it's a TTC
	if (stbtt_tag(font_collection, "ttcf")) {
		// version 1?
		if (ttULONG(font_collection+4) == 0x00010000 || ttULONG(font_collection+4) == 0x00020000) {
			stbtt_int32 n = ttLONG(font_collection+8);
			if (index >= n)
				return -1;
			return ttULONG(font_collection+12+index*4);
		}
	}
	return -1;
}

static int stbtt_GetNumberOfFonts_internal(unsigned char *font_collection)
{
	// if it's just a font, there's only one valid font
	if (stbtt__isfont(font_collection))
		return 1;

	// check if it's a TTC
	if (stbtt_tag(font_collection, "ttcf")) {
		// version 1?
		if (ttULONG(font_collection+4) == 0x00010000 || ttULONG(font_collection+4) == 0x00020000) {
			return ttLONG(font_collection+8);
		}
	}
	return 0;
}

static stbtt__buf stbtt__get_subrs(stbtt__buf cff, stbtt__buf fontdict)
{
	stbtt_uint32 subrsoff = 0, private_loc[2] = { 0, 0 };
	stbtt__buf pdict;
	stbtt__dict_get_ints(&fontdict, 18, 2, private_loc);
	if (!private_loc[1] || !private_loc[0]) return stbtt__new_buf(NULL, 0);
	pdict = stbtt__buf_range(&cff, private_loc[1], private_loc[0]);
	stbtt__dict_get_ints(&pdict, 19, 1, &subrsoff);
	if (!subrsoff) return stbtt__new_buf(NULL, 0);
	stbtt__buf_seek(&cff, private_loc[1]+subrsoff);
	return stbtt__cff_get_index(&cff);
}

// since most people won't use this, find this table the first time it's needed
static int stbtt__get_svg(stbtt_fontinfo *info)
{
	stbtt_uint32 t;
	if (info->svg < 0) {
		t = stbtt__find_table(info->data, info->fontstart, "SVG ");
		if (t) {
			stbtt_uint32 offset = ttULONG(info->data + t + 2);
			info->svg = t + offset;
		} else {
			info->svg = 0;
		}
	}
	return info->svg;
}

static int stbtt_InitFont_internal(stbtt_fontinfo *info, unsigned char *data, int fontstart)
{
	stbtt_uint32 cmap, t;
	stbtt_int32 i,numTables;

	info->data = data;
	info->fontstart = fontstart;
	info->cff = stbtt__new_buf(NULL, 0);

	cmap = stbtt__find_table(data, fontstart, "cmap");       // required
	info->loca = stbtt__find_table(data, fontstart, "loca"); // required
	info->head = stbtt__find_table(data, fontstart, "head"); // required
	info->glyf = stbtt__find_table(data, fontstart, "glyf"); // required
	info->hhea = stbtt__find_table(data, fontstart, "hhea"); // required
	info->hmtx = stbtt__find_table(data, fontstart, "hmtx"); // required
	info->kern = stbtt__find_table(data, fontstart, "kern"); // not required
	info->gpos = stbtt__find_table(data, fontstart, "GPOS"); // not required

	if (!cmap || !info->head || !info->hhea || !info->hmtx)
		return 0;
	if (info->glyf) {
		// required for truetype
		if (!info->loca) return 0;
	} else {
		// initialization for CFF / Type2 fonts (OTF)
		stbtt__buf b, topdict, topdictidx;
		stbtt_uint32 cstype = 2, charstrings = 0, fdarrayoff = 0, fdselectoff = 0;
		stbtt_uint32 cff;

		cff = stbtt__find_table(data, fontstart, "CFF ");
		if (!cff) return 0;

		info->fontdicts = stbtt__new_buf(NULL, 0);
		info->fdselect = stbtt__new_buf(NULL, 0);

		// @TODO this should use size from table (not 512MB)
		info->cff = stbtt__new_buf(data+cff, 512*1024*1024);
		b = info->cff;

		// read the header
		stbtt__buf_skip(&b, 2);
		stbtt__buf_seek(&b, stbtt__buf_get8(&b)); // hdrsize

		// @TODO the name INDEX could list multiple fonts,
		// but we just use the first one.
		stbtt__cff_get_index(&b);  // name INDEX
		topdictidx = stbtt__cff_get_index(&b);
		topdict = stbtt__cff_index_get(topdictidx, 0);
		stbtt__cff_get_index(&b);  // string INDEX
		info->gsubrs = stbtt__cff_get_index(&b);

		stbtt__dict_get_ints(&topdict, 17, 1, &charstrings);
		stbtt__dict_get_ints(&topdict, 0x100 | 6, 1, &cstype);
		stbtt__dict_get_ints(&topdict, 0x100 | 36, 1, &fdarrayoff);
		stbtt__dict_get_ints(&topdict, 0x100 | 37, 1, &fdselectoff);
		info->subrs = stbtt__get_subrs(b, topdict);

		// we only support Type 2 charstrings
		if (cstype != 2) return 0;
		if (charstrings == 0) return 0;

		if (fdarrayoff) {
			// looks like a CID font
			if (!fdselectoff) return 0;
			stbtt__buf_seek(&b, fdarrayoff);
			info->fontdicts = stbtt__cff_get_index(&b);
			info->fdselect = stbtt__buf_range(&b, fdselectoff, b.size-fdselectoff);
		}

		stbtt__buf_seek(&b, charstrings);
		info->charstrings = stbtt__cff_get_index(&b);
	}

	t = stbtt__find_table(data, fontstart, "maxp");
	if (t)
		info->numGlyphs = ttUSHORT(data+t+4);
	else
		info->numGlyphs = 0xffff;

	info->svg = -1;

	// find a cmap encoding table we understand *now* to avoid searching
	// later. (todo: could make this installable)
	// the same regardless of glyph.
	numTables = ttUSHORT(data + cmap + 2);
	info->index_map = 0;
	for (i=0; i < numTables; ++i) {
		stbtt_uint32 encoding_record = cmap + 4 + 8 * i;
		// find an encoding we understand:
		switch(ttUSHORT(data+encoding_record)) {
			case STBTT_PLATFORM_ID_MICROSOFT:
				switch (ttUSHORT(data+encoding_record+2)) {
					case STBTT_MS_EID_UNICODE_BMP:
					case STBTT_MS_EID_UNICODE_FULL:
						// MS/Unicode
						info->index_map = cmap + ttULONG(data+encoding_record+4);
						break;
            }
				break;
			case STBTT_PLATFORM_ID_UNICODE:
				// Mac/iOS has these
				// all the encodingIDs are unicode, so we don't bother to check it
				info->index_map = cmap + ttULONG(data+encoding_record+4);
				break;
		}
	}
	if (info->index_map == 0)
		return 0;

	info->indexToLocFormat = ttUSHORT(data+info->head + 50);
	return 1;
}

STBTT_DEF int stbtt_FindGlyphIndex(const stbtt_fontinfo *info, int unicode_codepoint)
{
	stbtt_uint8 *data = info->data;
	stbtt_uint32 index_map = info->index_map;

	stbtt_uint16 format = ttUSHORT(data + index_map + 0);
	if (format == 0) { // apple byte encoding
		stbtt_int32 bytes = ttUSHORT(data + index_map + 2);
		if (unicode_codepoint < bytes-6)
			return ttBYTE(data + index_map + 6 + unicode_codepoint);
		return 0;
	} else if (format == 6) {
		stbtt_uint32 first = ttUSHORT(data + index_map + 6);
		stbtt_uint32 count = ttUSHORT(data + index_map + 8);
		if ((stbtt_uint32) unicode_codepoint >= first && (stbtt_uint32) unicode_codepoint < first+count)
			return ttUSHORT(data + index_map + 10 + (unicode_codepoint - first)*2);
		return 0;
	} else if (format == 2) {
		STBTT_assert(0); // @TODO: high-byte mapping for japanese/chinese/korean
		return 0;
	} else if (format == 4) { // standard mapping for windows fonts: binary search collection of ranges
		stbtt_uint16 segcount = ttUSHORT(data+index_map+6) >> 1;
		stbtt_uint16 searchRange = ttUSHORT(data+index_map+8) >> 1;
		stbtt_uint16 entrySelector = ttUSHORT(data+index_map+10);
		stbtt_uint16 rangeShift = ttUSHORT(data+index_map+12) >> 1;

		// do a binary search of the segments
		stbtt_uint32 endCount = index_map + 14;
		stbtt_uint32 search = endCount;

		if (unicode_codepoint > 0xffff)
			return 0;

		// they lie from endCount .. endCount + segCount
		// but searchRange is the nearest power of two, so...
		if (unicode_codepoint >= ttUSHORT(data + search + rangeShift*2))
			search += rangeShift*2;

		// now decrement to bias correctly to find smallest
		search -= 2;
		while (entrySelector) {
			stbtt_uint16 end;
			searchRange >>= 1;
			end = ttUSHORT(data + search + searchRange*2);
			if (unicode_codepoint > end)
				search += searchRange*2;
			--entrySelector;
		}
		search += 2;

		{
			stbtt_uint16 offset, start, last;
			stbtt_uint16 item = (stbtt_uint16) ((search - endCount) >> 1);

			start = ttUSHORT(data + index_map + 14 + segcount*2 + 2 + 2*item);
			last = ttUSHORT(data + endCount + 2*item);
			if (unicode_codepoint < start || unicode_codepoint > last)
				return 0;

			offset = ttUSHORT(data + index_map + 14 + segcount*6 + 2 + 2*item);
			if (offset == 0)
				return (stbtt_uint16) (unicode_codepoint + ttSHORT(data + index_map + 14 + segcount*4 + 2 + 2*item));

			return ttUSHORT(data + offset + (unicode_codepoint-start)*2 + index_map + 14 + segcount*6 + 2 + 2*item);
		}
	} else if (format == 12 || format == 13) {
		stbtt_uint32 ngroups = ttULONG(data+index_map+12);
		stbtt_int32 low,high;
		low = 0; high = (stbtt_int32)ngroups;
		// Binary search the right group.
		while (low < high) {
			stbtt_int32 mid = low + ((high-low) >> 1); // rounds down, so low <= mid < high
			stbtt_uint32 start_char = ttULONG(data+index_map+16+mid*12);
			stbtt_uint32 end_char = ttULONG(data+index_map+16+mid*12+4);
			if ((stbtt_uint32) unicode_codepoint < start_char)
				high = mid;
			else if ((stbtt_uint32) unicode_codepoint > end_char)
				low = mid+1;
			else {
				stbtt_uint32 start_glyph = ttULONG(data+index_map+16+mid*12+8);
				if (format == 12)
					return start_glyph + unicode_codepoint-start_char;
				else // format == 13
					return start_glyph;
			}
		}
		return 0; // not found
	}
	// @TODO
	STBTT_assert(0);
	return 0;
}

STBTT_DEF int stbtt_GetCodepointShape(const stbtt_fontinfo *info, int unicode_codepoint, stbtt_vertex **vertices)
{
	return stbtt_GetGlyphShape(info, stbtt_FindGlyphIndex(info, unicode_codepoint), vertices);
}

static void stbtt_setvertex(stbtt_vertex *v, stbtt_uint8 type, stbtt_int32 x, stbtt_int32 y, stbtt_int32 cx, stbtt_int32 cy)
{
	v->type = type;
	v->x = (stbtt_int16) x;
	v->y = (stbtt_int16) y;
	v->cx = (stbtt_int16) cx;
	v->cy = (stbtt_int16) cy;
}

static int stbtt__GetGlyfOffset(const stbtt_fontinfo *info, int glyph_index)
{
	int g1,g2;

	STBTT_assert(!info->cff.size);

	if (glyph_index >= info->numGlyphs) return -1; // glyph index out of range
	if (info->indexToLocFormat >= 2)    return -1; // unknown index->glyph map format

	if (info->indexToLocFormat == 0) {
		g1 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index * 2) * 2;
		g2 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index * 2 + 2) * 2;
	} else {
		g1 = info->glyf + ttULONG (info->data + info->loca + glyph_index * 4);
		g2 = info->glyf + ttULONG (info->data + info->loca + glyph_index * 4 + 4);
	}

	return g1==g2 ? -1 : g1; // if length is 0, return -1
}

static int stbtt__GetGlyphInfoT2(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1);

STBTT_DEF int stbtt_GetGlyphBox(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1)
{
	if (info->cff.size) {
		stbtt__GetGlyphInfoT2(info, glyph_index, x0, y0, x1, y1);
	} else {
		int g = stbtt__GetGlyfOffset(info, glyph_index);
		if (g < 0) return 0;

		if (x0) *x0 = ttSHORT(info->data + g + 2);
		if (y0) *y0 = ttSHORT(info->data + g + 4);
		if (x1) *x1 = ttSHORT(info->data + g + 6);
		if (y1) *y1 = ttSHORT(info->data + g + 8);
	}
	return 1;
}

STBTT_DEF int stbtt_GetCodepointBox(const stbtt_fontinfo *info, int codepoint, int *x0, int *y0, int *x1, int *y1)
{
	return stbtt_GetGlyphBox(info, stbtt_FindGlyphIndex(info,codepoint), x0,y0,x1,y1);
}

STBTT_DEF int stbtt_IsGlyphEmpty(const stbtt_fontinfo *info, int glyph_index)
{
	stbtt_int16 numberOfContours;
	int g;
	if (info->cff.size)
		return stbtt__GetGlyphInfoT2(info, glyph_index, NULL, NULL, NULL, NULL) == 0;
	g = stbtt__GetGlyfOffset(info, glyph_index);
	if (g < 0) return 1;
	numberOfContours = ttSHORT(info->data + g);
	return numberOfContours == 0;
}

static int stbtt__close_shape(stbtt_vertex *vertices, int num_vertices, int was_off, int start_off,
							  stbtt_int32 sx, stbtt_int32 sy, stbtt_int32 scx, stbtt_int32 scy, stbtt_int32 cx, stbtt_int32 cy)
{
	if (start_off) {
		if (was_off)
			stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, (cx+scx)>>1, (cy+scy)>>1, cx,cy);
		stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, sx,sy,scx,scy);
	} else {
		if (was_off)
			stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve,sx,sy,cx,cy);
		else
			stbtt_setvertex(&vertices[num_vertices++], STBTT_vline,sx,sy,0,0);
	}
	return num_vertices;
}

static int stbtt__GetGlyphShapeTT(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **pvertices)
{
	stbtt_int16 numberOfContours;
	stbtt_uint8 *endPtsOfContours;
	stbtt_uint8 *data = info->data;
	stbtt_vertex *vertices=0;
	int num_vertices=0;
	int g = stbtt__GetGlyfOffset(info, glyph_index);

	*pvertices = NULL;

	if (g < 0) return 0;

	numberOfContours = ttSHORT(data + g);

	if (numberOfContours > 0) {
		stbtt_uint8 flags=0,flagcount;
		stbtt_int32 ins, i,j=0,m,n, next_move, was_off=0, off, start_off=0;
		stbtt_int32 x,y,cx,cy,sx,sy, scx,scy;
		stbtt_uint8 *points;
		endPtsOfContours = (data + g + 10);
		ins = ttUSHORT(data + g + 10 + numberOfContours * 2);
		points = data + g + 10 + numberOfContours * 2 + 2 + ins;

		n = 1+ttUSHORT(endPtsOfContours + numberOfContours*2-2);

		m = n + 2*numberOfContours;  // a loose bound on how many vertices we might need
		vertices = (stbtt_vertex *) STBTT_malloc(m * sizeof(vertices[0]), info->userdata);
		if (vertices == 0)
			return 0;

		next_move = 0;
		flagcount=0;

		// in first pass, we load uninterpreted data into the allocated array
		// above, shifted to the end of the array so we won't overwrite it when
		// we create our final data starting from the front

		off = m - n; // starting offset for uninterpreted data, regardless of how m ends up being calculated

		// first load flags

		for (i=0; i < n; ++i) {
			if (flagcount == 0) {
				flags = *points++;
				if (flags & 8)
					flagcount = *points++;
			} else
				--flagcount;
			vertices[off+i].type = flags;
		}

		// now load x coordinates
		x=0;
		for (i=0; i < n; ++i) {
			flags = vertices[off+i].type;
			if (flags & 2) {
				stbtt_int16 dx = *points++;
				x += (flags & 16) ? dx : -dx; // ???
			} else {
				if (!(flags & 16)) {
					x = x + (stbtt_int16) (points[0]*256 + points[1]);
					points += 2;
				}
			}
			vertices[off+i].x = (stbtt_int16) x;
		}

		// now load y coordinates
		y=0;
		for (i=0; i < n; ++i) {
			flags = vertices[off+i].type;
			if (flags & 4) {
				stbtt_int16 dy = *points++;
				y += (flags & 32) ? dy : -dy; // ???
			} else {
				if (!(flags & 32)) {
					y = y + (stbtt_int16) (points[0]*256 + points[1]);
					points += 2;
				}
			}
			vertices[off+i].y = (stbtt_int16) y;
		}

		// now convert them to our format
		num_vertices=0;
		sx = sy = cx = cy = scx = scy = 0;
		for (i=0; i < n; ++i) {
			flags = vertices[off+i].type;
			x     = (stbtt_int16) vertices[off+i].x;
			y     = (stbtt_int16) vertices[off+i].y;

			if (next_move == i) {
				if (i != 0)
					num_vertices = stbtt__close_shape(vertices, num_vertices, was_off, start_off, sx,sy,scx,scy,cx,cy);

				// now start the new one
				start_off = !(flags & 1);
				if (start_off) {
					// if we start off with an off-curve point, then when we need to find a point on the curve
					// where we can start, and we need to save some state for when we wraparound.
					scx = x;
					scy = y;
					if (!(vertices[off+i+1].type & 1)) {
						// next point is also a curve point, so interpolate an on-point curve
						sx = (x + (stbtt_int32) vertices[off+i+1].x) >> 1;
						sy = (y + (stbtt_int32) vertices[off+i+1].y) >> 1;
					} else {
						// otherwise just use the next point as our start point
						sx = (stbtt_int32) vertices[off+i+1].x;
						sy = (stbtt_int32) vertices[off+i+1].y;
						++i; // we're using point i+1 as the starting point, so skip it
					}
				} else {
					sx = x;
					sy = y;
				}
				stbtt_setvertex(&vertices[num_vertices++], STBTT_vmove,sx,sy,0,0);
				was_off = 0;
				next_move = 1 + ttUSHORT(endPtsOfContours+j*2);
				++j;
			} else {
				if (!(flags & 1)) { // if it's a curve
					if (was_off) // two off-curve control points in a row means interpolate an on-curve midpoint
						stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, (cx+x)>>1, (cy+y)>>1, cx, cy);
					cx = x;
					cy = y;
					was_off = 1;
				} else {
					if (was_off)
						stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, x,y, cx, cy);
					else
						stbtt_setvertex(&vertices[num_vertices++], STBTT_vline, x,y,0,0);
					was_off = 0;
				}
			}
		}
		num_vertices = stbtt__close_shape(vertices, num_vertices, was_off, start_off, sx,sy,scx,scy,cx,cy);
	} else if (numberOfContours < 0) {
		// Compound shapes.
		int more = 1;
		stbtt_uint8 *comp = data + g + 10;
		num_vertices = 0;
		vertices = 0;
		while (more) {
			stbtt_uint16 flags, gidx;
			int comp_num_verts = 0, i;
			stbtt_vertex *comp_verts = 0, *tmp = 0;
			float mtx[6] = {1,0,0,1,0,0}, m, n;

			flags = ttSHORT(comp); comp+=2;
			gidx = ttSHORT(comp); comp+=2;

			if (flags & 2) { // XY values
				if (flags & 1) { // shorts
					mtx[4] = ttSHORT(comp); comp+=2;
					mtx[5] = ttSHORT(comp); comp+=2;
				} else {
					mtx[4] = ttCHAR(comp); comp+=1;
					mtx[5] = ttCHAR(comp); comp+=1;
				}
			}
			else {
				// @TODO handle matching point
				STBTT_assert(0);
			}
			if (flags & (1<<3)) { // WE_HAVE_A_SCALE
				mtx[0] = mtx[3] = ttSHORT(comp)/16384.0f; comp+=2;
				mtx[1] = mtx[2] = 0;
			} else if (flags & (1<<6)) { // WE_HAVE_AN_X_AND_YSCALE
				mtx[0] = ttSHORT(comp)/16384.0f; comp+=2;
				mtx[1] = mtx[2] = 0;
				mtx[3] = ttSHORT(comp)/16384.0f; comp+=2;
			} else if (flags & (1<<7)) { // WE_HAVE_A_TWO_BY_TWO
				mtx[0] = ttSHORT(comp)/16384.0f; comp+=2;
				mtx[1] = ttSHORT(comp)/16384.0f; comp+=2;
				mtx[2] = ttSHORT(comp)/16384.0f; comp+=2;
				mtx[3] = ttSHORT(comp)/16384.0f; comp+=2;
			}

			// Find transformation scales.
			m = (float) STBTT_sqrt(mtx[0]*mtx[0] + mtx[1]*mtx[1]);
			n = (float) STBTT_sqrt(mtx[2]*mtx[2] + mtx[3]*mtx[3]);

			// Get indexed glyph.
			comp_num_verts = stbtt_GetGlyphShape(info, gidx, &comp_verts);
			if (comp_num_verts > 0) {
				// Transform vertices.
				for (i = 0; i < comp_num_verts; ++i) {
					stbtt_vertex* v = &comp_verts[i];
					stbtt_vertex_type x,y;
					x=v->x; y=v->y;
					v->x = (stbtt_vertex_type)(m * (mtx[0]*x + mtx[2]*y + mtx[4]));
					v->y = (stbtt_vertex_type)(n * (mtx[1]*x + mtx[3]*y + mtx[5]));
					x=v->cx; y=v->cy;
					v->cx = (stbtt_vertex_type)(m * (mtx[0]*x + mtx[2]*y + mtx[4]));
					v->cy = (stbtt_vertex_type)(n * (mtx[1]*x + mtx[3]*y + mtx[5]));
				}
				// Append vertices.
				tmp = (stbtt_vertex*)STBTT_malloc((num_vertices+comp_num_verts)*sizeof(stbtt_vertex), info->userdata);
				if (!tmp) {
					if (vertices) STBTT_free(vertices, info->userdata);
					if (comp_verts) STBTT_free(comp_verts, info->userdata);
					return 0;
				}
				if (num_vertices > 0 && vertices) STBTT_memcpy(tmp, vertices, num_vertices*sizeof(stbtt_vertex));
				STBTT_memcpy(tmp+num_vertices, comp_verts, comp_num_verts*sizeof(stbtt_vertex));
				if (vertices) STBTT_free(vertices, info->userdata);
				vertices = tmp;
				STBTT_free(comp_verts, info->userdata);
				num_vertices += comp_num_verts;
			}
			// More components ?
			more = flags & (1<<5);
		}
	} else {
		// numberOfCounters == 0, do nothing
	}

	*pvertices = vertices;
	return num_vertices;
}

typedef struct
{
	int bounds;
	int started;
	float first_x, first_y;
	float x, y;
	stbtt_int32 min_x, max_x, min_y, max_y;

	stbtt_vertex *pvertices;
	int num_vertices;
} stbtt__csctx;

#define STBTT__CSCTX_INIT(bounds) {bounds,0, 0,0, 0,0, 0,0,0,0, NULL, 0}

static void stbtt__track_vertex(stbtt__csctx *c, stbtt_int32 x, stbtt_int32 y)
{
	if (x > c->max_x || !c->started) c->max_x = x;
	if (y > c->max_y || !c->started) c->max_y = y;
	if (x < c->min_x || !c->started) c->min_x = x;
	if (y < c->min_y || !c->started) c->min_y = y;
	c->started = 1;
}

static void stbtt__csctx_v(stbtt__csctx *c, stbtt_uint8 type, stbtt_int32 x, stbtt_int32 y, stbtt_int32 cx, stbtt_int32 cy, stbtt_int32 cx1, stbtt_int32 cy1)
{
	if (c->bounds) {
		stbtt__track_vertex(c, x, y);
		if (type == STBTT_vcubic) {
			stbtt__track_vertex(c, cx, cy);
			stbtt__track_vertex(c, cx1, cy1);
		}
	} else {
		stbtt_setvertex(&c->pvertices[c->num_vertices], type, x, y, cx, cy);
		c->pvertices[c->num_vertices].cx1 = (stbtt_int16) cx1;
		c->pvertices[c->num_vertices].cy1 = (stbtt_int16) cy1;
	}
	c->num_vertices++;
}

static void stbtt__csctx_close_shape(stbtt__csctx *ctx)
{
	if (ctx->first_x != ctx->x || ctx->first_y != ctx->y)
		stbtt__csctx_v(ctx, STBTT_vline, (int)ctx->first_x, (int)ctx->first_y, 0, 0, 0, 0);
}

static void stbtt__csctx_rmove_to(stbtt__csctx *ctx, float dx, float dy)
{
	stbtt__csctx_close_shape(ctx);
	ctx->first_x = ctx->x = ctx->x + dx;
	ctx->first_y = ctx->y = ctx->y + dy;
	stbtt__csctx_v(ctx, STBTT_vmove, (int)ctx->x, (int)ctx->y, 0, 0, 0, 0);
}

static void stbtt__csctx_rline_to(stbtt__csctx *ctx, float dx, float dy)
{
	ctx->x += dx;
	ctx->y += dy;
	stbtt__csctx_v(ctx, STBTT_vline, (int)ctx->x, (int)ctx->y, 0, 0, 0, 0);
}

static void stbtt__csctx_rccurve_to(stbtt__csctx *ctx, float dx1, float dy1, float dx2, float dy2, float dx3, float dy3)
{
	float cx1 = ctx->x + dx1;
	float cy1 = ctx->y + dy1;
	float cx2 = cx1 + dx2;
	float cy2 = cy1 + dy2;
	ctx->x = cx2 + dx3;
	ctx->y = cy2 + dy3;
	stbtt__csctx_v(ctx, STBTT_vcubic, (int)ctx->x, (int)ctx->y, (int)cx1, (int)cy1, (int)cx2, (int)cy2);
}

static stbtt__buf stbtt__get_subr(stbtt__buf idx, int n)
{
	int count = stbtt__cff_index_count(&idx);
	int bias = 107;
	if (count >= 33900)
		bias = 32768;
	else if (count >= 1240)
		bias = 1131;
	n += bias;
	if (n < 0 || n >= count)
		return stbtt__new_buf(NULL, 0);
	return stbtt__cff_index_get(idx, n);
}

static stbtt__buf stbtt__cid_get_glyph_subrs(const stbtt_fontinfo *info, int glyph_index)
{
	stbtt__buf fdselect = info->fdselect;
	int nranges, start, end, v, fmt, fdselector = -1, i;

	stbtt__buf_seek(&fdselect, 0);
	fmt = stbtt__buf_get8(&fdselect);
	if (fmt == 0) {
		// untested
		stbtt__buf_skip(&fdselect, glyph_index);
		fdselector = stbtt__buf_get8(&fdselect);
	} else if (fmt == 3) {
		nranges = stbtt__buf_get16(&fdselect);
		start = stbtt__buf_get16(&fdselect);
		for (i = 0; i < nranges; i++) {
			v = stbtt__buf_get8(&fdselect);
			end = stbtt__buf_get16(&fdselect);
			if (glyph_index >= start && glyph_index < end) {
				fdselector = v;
				break;
			}
			start = end;
		}
	}
	if (fdselector == -1) stbtt__new_buf(NULL, 0);
	return stbtt__get_subrs(info->cff, stbtt__cff_index_get(info->fontdicts, fdselector));
}

static int stbtt__run_charstring(const stbtt_fontinfo *info, int glyph_index, stbtt__csctx *c)
{
	int in_header = 1, maskbits = 0, subr_stack_height = 0, sp = 0, v, i, b0;
	int has_subrs = 0, clear_stack;
	float s[48];
	stbtt__buf subr_stack[10], subrs = info->subrs, b;
	float f;

	#define STBTT__CSERR(s) (0)

	// this currently ignores the initial width value, which isn't needed if we have hmtx
	b = stbtt__cff_index_get(info->charstrings, glyph_index);
	while (b.cursor < b.size) {
		i = 0;
		clear_stack = 1;
		b0 = stbtt__buf_get8(&b);
		switch (b0) {
			// @TODO implement hinting
			case 0x13: // hintmask
			case 0x14: // cntrmask
				if (in_header)
					maskbits += (sp / 2); // implicit "vstem"
				in_header = 0;
				stbtt__buf_skip(&b, (maskbits + 7) / 8);
				break;

			case 0x01: // hstem
			case 0x03: // vstem
			case 0x12: // hstemhm
			case 0x17: // vstemhm
				maskbits += (sp / 2);
				break;

			case 0x15: // rmoveto
				in_header = 0;
				if (sp < 2) return STBTT__CSERR("rmoveto stack");
				stbtt__csctx_rmove_to(c, s[sp-2], s[sp-1]);
				break;
			case 0x04: // vmoveto
				in_header = 0;
				if (sp < 1) return STBTT__CSERR("vmoveto stack");
				stbtt__csctx_rmove_to(c, 0, s[sp-1]);
				break;
			case 0x16: // hmoveto
				in_header = 0;
				if (sp < 1) return STBTT__CSERR("hmoveto stack");
				stbtt__csctx_rmove_to(c, s[sp-1], 0);
				break;

			case 0x05: // rlineto
				if (sp < 2) return STBTT__CSERR("rlineto stack");
				for (; i + 1 < sp; i += 2)
					stbtt__csctx_rline_to(c, s[i], s[i+1]);
				break;

			// hlineto/vlineto and vhcurveto/hvcurveto alternate horizontal and vertical
			// starting from a different place.

			case 0x07: // vlineto
				if (sp < 1) return STBTT__CSERR("vlineto stack");
				goto vlineto;
			case 0x06: // hlineto
				if (sp < 1) return STBTT__CSERR("hlineto stack");
				for (;;) {
					if (i >= sp) break;
					stbtt__csctx_rline_to(c, s[i], 0);
					i++;
					vlineto:
						if (i >= sp) break;
					stbtt__csctx_rline_to(c, 0, s[i]);
					i++;
				}
				break;

			case 0x1F: // hvcurveto
				if (sp < 4) return STBTT__CSERR("hvcurveto stack");
				goto hvcurveto;
			case 0x1E: // vhcurveto
				if (sp < 4) return STBTT__CSERR("vhcurveto stack");
				for (;;) {
					if (i + 3 >= sp) break;
					stbtt__csctx_rccurve_to(c, 0, s[i], s[i+1], s[i+2], s[i+3], (sp - i == 5) ? s[i + 4] : 0.0f);
					i += 4;
					hvcurveto:
						if (i + 3 >= sp) break;
					stbtt__csctx_rccurve_to(c, s[i], 0, s[i+1], s[i+2], (sp - i == 5) ? s[i+4] : 0.0f, s[i+3]);
					i += 4;
				}
				break;

			case 0x08: // rrcurveto
				if (sp < 6) return STBTT__CSERR("rcurveline stack");
				for (; i + 5 < sp; i += 6)
					stbtt__csctx_rccurve_to(c, s[i], s[i+1], s[i+2], s[i+3], s[i+4], s[i+5]);
				break;

			case 0x18: // rcurveline
				if (sp < 8) return STBTT__CSERR("rcurveline stack");
				for (; i + 5 < sp - 2; i += 6)
					stbtt__csctx_rccurve_to(c, s[i], s[i+1], s[i+2], s[i+3], s[i+4], s[i+5]);
				if (i + 1 >= sp) return STBTT__CSERR("rcurveline stack");
				stbtt__csctx_rline_to(c, s[i], s[i+1]);
				break;

			case 0x19: // rlinecurve
				if (sp < 8) return STBTT__CSERR("rlinecurve stack");
				for (; i + 1 < sp - 6; i += 2)
					stbtt__csctx_rline_to(c, s[i], s[i+1]);
				if (i + 5 >= sp) return STBTT__CSERR("rlinecurve stack");
				stbtt__csctx_rccurve_to(c, s[i], s[i+1], s[i+2], s[i+3], s[i+4], s[i+5]);
				break;

			case 0x1A: // vvcurveto
			case 0x1B: // hhcurveto
				if (sp < 4) return STBTT__CSERR("(vv|hh)curveto stack");
				f = 0.0;
				if (sp & 1) { f = s[i]; i++; }
				for (; i + 3 < sp; i += 4) {
					if (b0 == 0x1B)
						stbtt__csctx_rccurve_to(c, s[i], f, s[i+1], s[i+2], s[i+3], 0.0);
					else
						stbtt__csctx_rccurve_to(c, f, s[i], s[i+1], s[i+2], 0.0, s[i+3]);
					f = 0.0;
				}
				break;

			case 0x0A: // callsubr
				if (!has_subrs) {
					if (info->fdselect.size)
						subrs = stbtt__cid_get_glyph_subrs(info, glyph_index);
					has_subrs = 1;
				}
				// FALLTHROUGH
			case 0x1D: // callgsubr
				if (sp < 1) return STBTT__CSERR("call(g|)subr stack");
				v = (int) s[--sp];
				if (subr_stack_height >= 10) return STBTT__CSERR("recursion limit");
				subr_stack[subr_stack_height++] = b;
				b = stbtt__get_subr(b0 == 0x0A ? subrs : info->gsubrs, v);
				if (b.size == 0) return STBTT__CSERR("subr not found");
				b.cursor = 0;
				clear_stack = 0;
				break;

			case 0x0B: // return
				if (subr_stack_height <= 0) return STBTT__CSERR("return outside subr");
				b = subr_stack[--subr_stack_height];
				clear_stack = 0;
				break;

			case 0x0E: // endchar
				stbtt__csctx_close_shape(c);
				return 1;

			case 0x0C: { // two-byte escape
				float dx1, dx2, dx3, dx4, dx5, dx6, dy1, dy2, dy3, dy4, dy5, dy6;
				float dx, dy;
				int b1 = stbtt__buf_get8(&b);
				switch (b1) {
					// @TODO These "flex" implementations ignore the flex-depth and resolution,
					// and always draw beziers.
					case 0x22: // hflex
						if (sp < 7) return STBTT__CSERR("hflex stack");
						dx1 = s[0];
						dx2 = s[1];
						dy2 = s[2];
						dx3 = s[3];
						dx4 = s[4];
						dx5 = s[5];
						dx6 = s[6];
						stbtt__csctx_rccurve_to(c, dx1, 0, dx2, dy2, dx3, 0);
						stbtt__csctx_rccurve_to(c, dx4, 0, dx5, -dy2, dx6, 0);
						break;

					case 0x23: // flex
						if (sp < 13) return STBTT__CSERR("flex stack");
						dx1 = s[0];
						dy1 = s[1];
						dx2 = s[2];
						dy2 = s[3];
						dx3 = s[4];
						dy3 = s[5];
						dx4 = s[6];
						dy4 = s[7];
						dx5 = s[8];
						dy5 = s[9];
						dx6 = s[10];
						dy6 = s[11];
						//fd is s[12]
						stbtt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, dy3);
						stbtt__csctx_rccurve_to(c, dx4, dy4, dx5, dy5, dx6, dy6);
						break;

					case 0x24: // hflex1
						if (sp < 9) return STBTT__CSERR("hflex1 stack");
						dx1 = s[0];
						dy1 = s[1];
						dx2 = s[2];
						dy2 = s[3];
						dx3 = s[4];
						dx4 = s[5];
						dx5 = s[6];
						dy5 = s[7];
						dx6 = s[8];
						stbtt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, 0);
						stbtt__csctx_rccurve_to(c, dx4, 0, dx5, dy5, dx6, -(dy1+dy2+dy5));
						break;

					case 0x25: // flex1
						if (sp < 11) return STBTT__CSERR("flex1 stack");
						dx1 = s[0];
						dy1 = s[1];
						dx2 = s[2];
						dy2 = s[3];
						dx3 = s[4];
						dy3 = s[5];
						dx4 = s[6];
						dy4 = s[7];
						dx5 = s[8];
						dy5 = s[9];
						dx6 = dy6 = s[10];
						dx = dx1+dx2+dx3+dx4+dx5;
						dy = dy1+dy2+dy3+dy4+dy5;
						if (STBTT_fabs(dx) > STBTT_fabs(dy))
							dy6 = -dy;
						else
							dx6 = -dx;
						stbtt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, dy3);
						stbtt__csctx_rccurve_to(c, dx4, dy4, dx5, dy5, dx6, dy6);
						break;

					default:
						return STBTT__CSERR("unimplemented");
				}
			} break;

			default:
				if (b0 != 255 && b0 != 28 && b0 < 32)
					return STBTT__CSERR("reserved operator");

				// push immediate
				if (b0 == 255) {
					f = (float)(stbtt_int32)stbtt__buf_get32(&b) / 0x10000;
				} else {
					stbtt__buf_skip(&b, -1);
					f = (float)(stbtt_int16)stbtt__cff_int(&b);
				}
				if (sp >= 48) return STBTT__CSERR("push stack overflow");
				s[sp++] = f;
				clear_stack = 0;
				break;
		}
		if (clear_stack) sp = 0;
	}
	return STBTT__CSERR("no endchar");

	#undef STBTT__CSERR
}

static int stbtt__GetGlyphShapeT2(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **pvertices)
{
	// runs the charstring twice, once to count and once to output (to avoid realloc)
	stbtt__csctx count_ctx = STBTT__CSCTX_INIT(1);
	stbtt__csctx output_ctx = STBTT__CSCTX_INIT(0);
	if (stbtt__run_charstring(info, glyph_index, &count_ctx)) {
		*pvertices = (stbtt_vertex*)STBTT_malloc(count_ctx.num_vertices*sizeof(stbtt_vertex), info->userdata);
		output_ctx.pvertices = *pvertices;
		if (stbtt__run_charstring(info, glyph_index, &output_ctx)) {
			STBTT_assert(output_ctx.num_vertices == count_ctx.num_vertices);
			return output_ctx.num_vertices;
		}
	}
	*pvertices = NULL;
	return 0;
}

static int stbtt__GetGlyphInfoT2(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1)
{
	stbtt__csctx c = STBTT__CSCTX_INIT(1);
	int r = stbtt__run_charstring(info, glyph_index, &c);
	if (x0)  *x0 = r ? c.min_x : 0;
	if (y0)  *y0 = r ? c.min_y : 0;
	if (x1)  *x1 = r ? c.max_x : 0;
	if (y1)  *y1 = r ? c.max_y : 0;
	return r ? c.num_vertices : 0;
}

STBTT_DEF int stbtt_GetGlyphShape(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **pvertices)
{
	if (!info->cff.size)
		return stbtt__GetGlyphShapeTT(info, glyph_index, pvertices);
	else
		return stbtt__GetGlyphShapeT2(info, glyph_index, pvertices);
}

STBTT_DEF void stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing)
{
	stbtt_uint16 numOfLongHorMetrics = ttUSHORT(info->data+info->hhea + 34);
	if (glyph_index < numOfLongHorMetrics) {
		if (advanceWidth)     *advanceWidth    = ttSHORT(info->data + info->hmtx + 4*glyph_index);
		if (leftSideBearing)  *leftSideBearing = ttSHORT(info->data + info->hmtx + 4*glyph_index + 2);
	} else {
		if (advanceWidth)     *advanceWidth    = ttSHORT(info->data + info->hmtx + 4*(numOfLongHorMetrics-1));
		if (leftSideBearing)  *leftSideBearing = ttSHORT(info->data + info->hmtx + 4*numOfLongHorMetrics + 2*(glyph_index - numOfLongHorMetrics));
	}
}

STBTT_DEF int  stbtt_GetKerningTableLength(const stbtt_fontinfo *info)
{
	stbtt_uint8 *data = info->data + info->kern;

	// we only look at the first table. it must be 'horizontal' and format 0.
	if (!info->kern)
		return 0;
	if (ttUSHORT(data+2) < 1) // number of tables, need at least 1
		return 0;
	if (ttUSHORT(data+8) != 1) // horizontal flag must be set in format
		return 0;

	return ttUSHORT(data+10);
}

STBTT_DEF int stbtt_GetKerningTable(const stbtt_fontinfo *info, stbtt_kerningentry* table, int table_length)
{
	stbtt_uint8 *data = info->data + info->kern;
	int k, length;

	// we only look at the first table. it must be 'horizontal' and format 0.
	if (!info->kern)
		return 0;
	if (ttUSHORT(data+2) < 1) // number of tables, need at least 1
		return 0;
	if (ttUSHORT(data+8) != 1) // horizontal flag must be set in format
		return 0;

	length = ttUSHORT(data+10);
	if (table_length < length)
		length = table_length;

	for (k = 0; k < length; k++)
	{
		table[k].glyph1 = ttUSHORT(data+18+(k*6));
		table[k].glyph2 = ttUSHORT(data+20+(k*6));
		table[k].advance = ttSHORT(data+22+(k*6));
	}

	return length;
}

static int stbtt__GetGlyphKernInfoAdvance(const stbtt_fontinfo *info, int glyph1, int glyph2)
{
	stbtt_uint8 *data = info->data + info->kern;
	stbtt_uint32 needle, straw;
	int l, r, m;

	// we only look at the first table. it must be 'horizontal' and format 0.
	if (!info->kern)
		return 0;
	if (ttUSHORT(data+2) < 1) // number of tables, need at least 1
		return 0;
	if (ttUSHORT(data+8) != 1) // horizontal flag must be set in format
		return 0;

	l = 0;
	r = ttUSHORT(data+10) - 1;
	needle = glyph1 << 16 | glyph2;
	while (l <= r) {
		m = (l + r) >> 1;
		straw = ttULONG(data+18+(m*6)); // note: unaligned read
		if (needle < straw)
			r = m - 1;
		else if (needle > straw)
			l = m + 1;
		else
			return ttSHORT(data+22+(m*6));
	}
	return 0;
}

static stbtt_int32 stbtt__GetCoverageIndex(stbtt_uint8 *coverageTable, int glyph)
{
	stbtt_uint16 coverageFormat = ttUSHORT(coverageTable);
	switch (coverageFormat) {
		case 1: {
			stbtt_uint16 glyphCount = ttUSHORT(coverageTable + 2);

			// Binary search.
			stbtt_int32 l=0, r=glyphCount-1, m;
			int straw, needle=glyph;
			while (l <= r) {
				stbtt_uint8 *glyphArray = coverageTable + 4;
				stbtt_uint16 glyphID;
				m = (l + r) >> 1;
				glyphID = ttUSHORT(glyphArray + 2 * m);
				straw = glyphID;
				if (needle < straw)
					r = m - 1;
				else if (needle > straw)
					l = m + 1;
				else {
					return m;
				}
			}
			break;
		}

		case 2: {
			stbtt_uint16 rangeCount = ttUSHORT(coverageTable + 2);
			stbtt_uint8 *rangeArray = coverageTable + 4;

			// Binary search.
			stbtt_int32 l=0, r=rangeCount-1, m;
			int strawStart, strawEnd, needle=glyph;
			while (l <= r) {
				stbtt_uint8 *rangeRecord;
				m = (l + r) >> 1;
				rangeRecord = rangeArray + 6 * m;
				strawStart = ttUSHORT(rangeRecord);
				strawEnd = ttUSHORT(rangeRecord + 2);
				if (needle < strawStart)
					r = m - 1;
				else if (needle > strawEnd)
					l = m + 1;
				else {
					stbtt_uint16 startCoverageIndex = ttUSHORT(rangeRecord + 4);
					return startCoverageIndex + glyph - strawStart;
				}
			}
			break;
		}

		default: return -1; // unsupported
	}

	return -1;
}

static stbtt_int32  stbtt__GetGlyphClass(stbtt_uint8 *classDefTable, int glyph)
{
	stbtt_uint16 classDefFormat = ttUSHORT(classDefTable);
	switch (classDefFormat)
	{
		case 1: {
			stbtt_uint16 startGlyphID = ttUSHORT(classDefTable + 2);
			stbtt_uint16 glyphCount = ttUSHORT(classDefTable + 4);
			stbtt_uint8 *classDef1ValueArray = classDefTable + 6;

			if (glyph >= startGlyphID && glyph < startGlyphID + glyphCount)
				return (stbtt_int32)ttUSHORT(classDef1ValueArray + 2 * (glyph - startGlyphID));
			break;
		}

		case 2: {
			stbtt_uint16 classRangeCount = ttUSHORT(classDefTable + 2);
			stbtt_uint8 *classRangeRecords = classDefTable + 4;

			// Binary search.
			stbtt_int32 l=0, r=classRangeCount-1, m;
			int strawStart, strawEnd, needle=glyph;
			while (l <= r) {
				stbtt_uint8 *classRangeRecord;
				m = (l + r) >> 1;
				classRangeRecord = classRangeRecords + 6 * m;
				strawStart = ttUSHORT(classRangeRecord);
				strawEnd = ttUSHORT(classRangeRecord + 2);
				if (needle < strawStart)
					r = m - 1;
				else if (needle > strawEnd)
					l = m + 1;
				else
					return (stbtt_int32)ttUSHORT(classRangeRecord + 4);
			}
			break;
		}

		default:
			return -1; // Unsupported definition type, return an error.
	}

	// "All glyphs not assigned to a class fall into class 0". (OpenType spec)
	return 0;
}

// Define to STBTT_assert(x) if you want to break on unimplemented formats.
#define STBTT_GPOS_TODO_assert(x)

static stbtt_int32 stbtt__GetGlyphGPOSInfoAdvance(const stbtt_fontinfo *info, int glyph1, int glyph2)
{
	stbtt_uint16 lookupListOffset;
	stbtt_uint8 *lookupList;
	stbtt_uint16 lookupCount;
	stbtt_uint8 *data;
	stbtt_int32 i, sti;

	if (!info->gpos) return 0;

	data = info->data + info->gpos;

	if (ttUSHORT(data+0) != 1) return 0; // Major version 1
	if (ttUSHORT(data+2) != 0) return 0; // Minor version 0

	lookupListOffset = ttUSHORT(data+8);
	lookupList = data + lookupListOffset;
	lookupCount = ttUSHORT(lookupList);

	for (i=0; i<lookupCount; ++i) {
		stbtt_uint16 lookupOffset = ttUSHORT(lookupList + 2 + 2 * i);
		stbtt_uint8 *lookupTable = lookupList + lookupOffset;

		stbtt_uint16 lookupType = ttUSHORT(lookupTable);
		stbtt_uint16 subTableCount = ttUSHORT(lookupTable + 4);
		stbtt_uint8 *subTableOffsets = lookupTable + 6;
		if (lookupType != 2) // Pair Adjustment Positioning Subtable
			continue;

		for (sti=0; sti<subTableCount; sti++) {
			stbtt_uint16 subtableOffset = ttUSHORT(subTableOffsets + 2 * sti);
			stbtt_uint8 *table = lookupTable + subtableOffset;
			stbtt_uint16 posFormat = ttUSHORT(table);
			stbtt_uint16 coverageOffset = ttUSHORT(table + 2);
			stbtt_int32 coverageIndex = stbtt__GetCoverageIndex(table + coverageOffset, glyph1);
			if (coverageIndex == -1) continue;

			switch (posFormat) {
				case 1: {
					stbtt_int32 l, r, m;
					int straw, needle;
					stbtt_uint16 valueFormat1 = ttUSHORT(table + 4);
					stbtt_uint16 valueFormat2 = ttUSHORT(table + 6);
					if (valueFormat1 == 4 && valueFormat2 == 0) { // Support more formats?
						stbtt_int32 valueRecordPairSizeInBytes = 2;
						stbtt_uint16 pairSetCount = ttUSHORT(table + 8);
						stbtt_uint16 pairPosOffset = ttUSHORT(table + 10 + 2 * coverageIndex);
						stbtt_uint8 *pairValueTable = table + pairPosOffset;
						stbtt_uint16 pairValueCount = ttUSHORT(pairValueTable);
						stbtt_uint8 *pairValueArray = pairValueTable + 2;

						if (coverageIndex >= pairSetCount) return 0;

						needle=glyph2;
						r=pairValueCount-1;
						l=0;

						// Binary search.
						while (l <= r) {
							stbtt_uint16 secondGlyph;
							stbtt_uint8 *pairValue;
							m = (l + r) >> 1;
							pairValue = pairValueArray + (2 + valueRecordPairSizeInBytes) * m;
							secondGlyph = ttUSHORT(pairValue);
							straw = secondGlyph;
							if (needle < straw)
								r = m - 1;
							else if (needle > straw)
								l = m + 1;
							else {
								stbtt_int16 xAdvance = ttSHORT(pairValue + 2);
								return xAdvance;
							}
						}
					} else
						return 0;
					break;
				}

				case 2: {
					stbtt_uint16 valueFormat1 = ttUSHORT(table + 4);
					stbtt_uint16 valueFormat2 = ttUSHORT(table + 6);
					if (valueFormat1 == 4 && valueFormat2 == 0) { // Support more formats?
						stbtt_uint16 classDef1Offset = ttUSHORT(table + 8);
						stbtt_uint16 classDef2Offset = ttUSHORT(table + 10);
						int glyph1class = stbtt__GetGlyphClass(table + classDef1Offset, glyph1);
						int glyph2class = stbtt__GetGlyphClass(table + classDef2Offset, glyph2);

						stbtt_uint16 class1Count = ttUSHORT(table + 12);
						stbtt_uint16 class2Count = ttUSHORT(table + 14);
						stbtt_uint8 *class1Records, *class2Records;
						stbtt_int16 xAdvance;

						if (glyph1class < 0 || glyph1class >= class1Count) return 0; // malformed
						if (glyph2class < 0 || glyph2class >= class2Count) return 0; // malformed

						class1Records = table + 16;
						class2Records = class1Records + 2 * (glyph1class * class2Count);
						xAdvance = ttSHORT(class2Records + 2 * glyph2class);
						return xAdvance;
					} else
						return 0;
					break;
				}

				default:
					return 0; // Unsupported position format
			}
		}
	}

	return 0;
}

STBTT_DEF int  stbtt_GetGlyphKernAdvance(const stbtt_fontinfo *info, int g1, int g2)
{
	int xAdvance = 0;

	if (info->gpos)
		xAdvance += stbtt__GetGlyphGPOSInfoAdvance(info, g1, g2);
	else if (info->kern)
		xAdvance += stbtt__GetGlyphKernInfoAdvance(info, g1, g2);

	return xAdvance;
}

STBTT_DEF int  stbtt_GetCodepointKernAdvance(const stbtt_fontinfo *info, int ch1, int ch2)
{
	if (!info->kern && !info->gpos) // if no kerning table, don't waste time looking up both codepoint->glyphs
		return 0;
	return stbtt_GetGlyphKernAdvance(info, stbtt_FindGlyphIndex(info,ch1), stbtt_FindGlyphIndex(info,ch2));
}

STBTT_DEF void stbtt_GetCodepointHMetrics(const stbtt_fontinfo *info, int codepoint, int *advanceWidth, int *leftSideBearing)
{
	stbtt_GetGlyphHMetrics(info, stbtt_FindGlyphIndex(info,codepoint), advanceWidth, leftSideBearing);
}

STBTT_DEF void stbtt_GetFontVMetrics(const stbtt_fontinfo *info, int *ascent, int *descent, int *lineGap)
{
	if (ascent ) *ascent  = ttSHORT(info->data+info->hhea + 4);
	if (descent) *descent = ttSHORT(info->data+info->hhea + 6);
	if (lineGap) *lineGap = ttSHORT(info->data+info->hhea + 8);
}

STBTT_DEF int  stbtt_GetFontVMetricsOS2(const stbtt_fontinfo *info, int *typoAscent, int *typoDescent, int *typoLineGap)
{
	int tab = stbtt__find_table(info->data, info->fontstart, "OS/2");
	if (!tab)
		return 0;
	if (typoAscent ) *typoAscent  = ttSHORT(info->data+tab + 68);
	if (typoDescent) *typoDescent = ttSHORT(info->data+tab + 70);
	if (typoLineGap) *typoLineGap = ttSHORT(info->data+tab + 72);
	return 1;
}

STBTT_DEF void stbtt_GetFontBoundingBox(const stbtt_fontinfo *info, int *x0, int *y0, int *x1, int *y1)
{
	*x0 = ttSHORT(info->data + info->head + 36);
	*y0 = ttSHORT(info->data + info->head + 38);
	*x1 = ttSHORT(info->data + info->head + 40);
	*y1 = ttSHORT(info->data + info->head + 42);
}

STBTT_DEF float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float height)
{
	int fheight = ttSHORT(info->data + info->hhea + 4) - ttSHORT(info->data + info->hhea + 6);
	return (float) height / fheight;
}

STBTT_DEF float stbtt_ScaleForMappingEmToPixels(const stbtt_fontinfo *info, float pixels)
{
	int unitsPerEm = ttUSHORT(info->data + info->head + 18);
	return pixels / unitsPerEm;
}

STBTT_DEF void stbtt_FreeShape(const stbtt_fontinfo *info, stbtt_vertex *v)
{
	STBTT_free(v, info->userdata);
}

STBTT_DEF stbtt_uint8 *stbtt_FindSVGDoc(const stbtt_fontinfo *info, int gl)
{
	int i;
	stbtt_uint8 *data = info->data;
	stbtt_uint8 *svg_doc_list = data + stbtt__get_svg((stbtt_fontinfo *) info);

	int numEntries = ttUSHORT(svg_doc_list);
	stbtt_uint8 *svg_docs = svg_doc_list + 2;

	for(i=0; i<numEntries; i++) {
		stbtt_uint8 *svg_doc = svg_docs + (12 * i);
		if ((gl >= ttUSHORT(svg_doc)) && (gl <= ttUSHORT(svg_doc + 2)))
			return svg_doc;
	}
	return 0;
}

STBTT_DEF int stbtt_GetGlyphSVG(const stbtt_fontinfo *info, int gl, const char **svg)
{
	stbtt_uint8 *data = info->data;
	stbtt_uint8 *svg_doc;

	if (info->svg == 0)
		return 0;

	svg_doc = stbtt_FindSVGDoc(info, gl);
	if (svg_doc != NULL) {
		*svg = (char *) data + info->svg + ttULONG(svg_doc + 4);
		return ttULONG(svg_doc + 8);
	} else {
		return 0;
	}
}

STBTT_DEF int stbtt_GetCodepointSVG(const stbtt_fontinfo *info, int unicode_codepoint, const char **svg)
{
	return stbtt_GetGlyphSVG(info, stbtt_FindGlyphIndex(info, unicode_codepoint), svg);
}

//////////////////////////////////////////////////////////////////////////////
//
// antialiasing software rasterizer
//

STBTT_DEF void stbtt_GetGlyphBitmapBoxSubpixel(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y,float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
	int x0=0,y0=0,x1,y1; // =0 suppresses compiler warning
	if (!stbtt_GetGlyphBox(font, glyph, &x0,&y0,&x1,&y1)) {
		// e.g. space character
		if (ix0) *ix0 = 0;
		if (iy0) *iy0 = 0;
		if (ix1) *ix1 = 0;
		if (iy1) *iy1 = 0;
	} else {
		// move to integral bboxes (treating pixels as little squares, what pixels get touched)?
		if (ix0) *ix0 = STBTT_ifloor( x0 * scale_x + shift_x);
		if (iy0) *iy0 = STBTT_ifloor(-y1 * scale_y + shift_y);
		if (ix1) *ix1 = STBTT_iceil ( x1 * scale_x + shift_x);
		if (iy1) *iy1 = STBTT_iceil (-y0 * scale_y + shift_y);
	}
}

STBTT_DEF void stbtt_GetGlyphBitmapBox(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
	stbtt_GetGlyphBitmapBoxSubpixel(font, glyph, scale_x, scale_y,0.0f,0.0f, ix0, iy0, ix1, iy1);
}

STBTT_DEF void stbtt_GetCodepointBitmapBoxSubpixel(const stbtt_fontinfo *font, int codepoint, float scale_x, float scale_y, float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
	stbtt_GetGlyphBitmapBoxSubpixel(font, stbtt_FindGlyphIndex(font,codepoint), scale_x, scale_y,shift_x,shift_y, ix0,iy0,ix1,iy1);
}

STBTT_DEF void stbtt_GetCodepointBitmapBox(const stbtt_fontinfo *font, int codepoint, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
	stbtt_GetCodepointBitmapBoxSubpixel(font, codepoint, scale_x, scale_y,0.0f,0.0f, ix0,iy0,ix1,iy1);
}

//////////////////////////////////////////////////////////////////////////////
//
//  Rasterizer

typedef struct stbtt__hheap_chunk
{
	struct stbtt__hheap_chunk *next;
} stbtt__hheap_chunk;

typedef struct stbtt__hheap
{
	struct stbtt__hheap_chunk *head;
	void   *first_free;
	int    num_remaining_in_head_chunk;
} stbtt__hheap;

static void *stbtt__hheap_alloc(stbtt__hheap *hh, size_t size, void *userdata)
{
	if (hh->first_free) {
		void *p = hh->first_free;
		hh->first_free = * (void **) p;
		return p;
	} else {
		if (hh->num_remaining_in_head_chunk == 0) {
			int count = (size < 32 ? 2000 : size < 128 ? 800 : 100);
			stbtt__hheap_chunk *c = (stbtt__hheap_chunk *) STBTT_malloc(sizeof(stbtt__hheap_chunk) + size * count, userdata);
			if (c == NULL)
				return NULL;
			c->next = hh->head;
			hh->head = c;
			hh->num_remaining_in_head_chunk = count;
		}
		--hh->num_remaining_in_head_chunk;
		return (char *) (hh->head) + sizeof(stbtt__hheap_chunk) + size * hh->num_remaining_in_head_chunk;
	}
}

static void stbtt__hheap_free(stbtt__hheap *hh, void *p)
{
	*(void **) p = hh->first_free;
	hh->first_free = p;
}

static void stbtt__hheap_cleanup(stbtt__hheap *hh, void *userdata)
{
	stbtt__hheap_chunk *c = hh->head;
	while (c) {
		stbtt__hheap_chunk *n = c->next;
		STBTT_free(c, userdata);
		c = n;
	}
}

typedef struct stbtt__edge {
	float x0,y0, x1,y1;
	int invert;
} stbtt__edge;


typedef struct stbtt__active_edge
{
	struct stbtt__active_edge *next;
	#if STBTT_RASTERIZER_VERSION==1
	int x,dx;
	float ey;
	int direction;
	#elif STBTT_RASTERIZER_VERSION==2
	float fx,fdx,fdy;
	float direction;
	float sy;
	float ey;
	#else
	#error "Unrecognized value of STBTT_RASTERIZER_VERSION"
	#endif
} stbtt__active_edge;

#if STBTT_RASTERIZER_VERSION == 1
#define STBTT_FIXSHIFT   10
#define STBTT_FIX        (1 << STBTT_FIXSHIFT)
#define STBTT_FIXMASK    (STBTT_FIX-1)

static stbtt__active_edge *stbtt__new_active(stbtt__hheap *hh, stbtt__edge *e, int off_x, float start_point, void *userdata)
{
	stbtt__active_edge *z = (stbtt__active_edge *) stbtt__hheap_alloc(hh, sizeof(*z), userdata);
	float dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
	STBTT_assert(z != NULL);
	if (!z) return z;

	// round dx down to avoid overshooting
	if (dxdy < 0)
		z->dx = -STBTT_ifloor(STBTT_FIX * -dxdy);
	else
		z->dx = STBTT_ifloor(STBTT_FIX * dxdy);

	z->x = STBTT_ifloor(STBTT_FIX * e->x0 + z->dx * (start_point - e->y0)); // use z->dx so when we offset later it's by the same amount
	z->x -= off_x * STBTT_FIX;

	z->ey = e->y1;
	z->next = 0;
	z->direction = e->invert ? 1 : -1;
	return z;
}
#elif STBTT_RASTERIZER_VERSION == 2
static stbtt__active_edge *stbtt__new_active(stbtt__hheap *hh, stbtt__edge *e, int off_x, float start_point, void *userdata)
{
	stbtt__active_edge *z = (stbtt__active_edge *) stbtt__hheap_alloc(hh, sizeof(*z), userdata);
	float dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
	STBTT_assert(z != NULL);
	//STBTT_assert(e->y0 <= start_point);
	if (!z) return z;
	z->fdx = dxdy;
	z->fdy = dxdy != 0.0f ? (1.0f/dxdy) : 0.0f;
	z->fx = e->x0 + dxdy * (start_point - e->y0);
	z->fx -= off_x;
	z->direction = e->invert ? 1.0f : -1.0f;
	z->sy = e->y0;
	z->ey = e->y1;
	z->next = 0;
	return z;
}
#else
#error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#endif

#if STBTT_RASTERIZER_VERSION == 1
// note: this routine clips fills that extend off the edges... ideally this
// wouldn't happen, but it could happen if the truetype glyph bounding boxes
// are wrong, or if the user supplies a too-small bitmap
static void stbtt__fill_active_edges(unsigned char *scanline, int len, stbtt__active_edge *e, int max_weight)
{
	// non-zero winding fill
	int x0=0, w=0;

	while (e) {
		if (w == 0) {
			// if we're currently at zero, we need to record the edge start point
			x0 = e->x; w += e->direction;
		} else {
			int x1 = e->x; w += e->direction;
			// if we went to zero, we need to draw
			if (w == 0) {
				int i = x0 >> STBTT_FIXSHIFT;
				int j = x1 >> STBTT_FIXSHIFT;

				if (i < len && j >= 0) {
					if (i == j) {
						// x0,x1 are the same pixel, so compute combined coverage
						scanline[i] = scanline[i] + (stbtt_uint8) ((x1 - x0) * max_weight >> STBTT_FIXSHIFT);
					} else {
						if (i >= 0) // add antialiasing for x0
							scanline[i] = scanline[i] + (stbtt_uint8) (((STBTT_FIX - (x0 & STBTT_FIXMASK)) * max_weight) >> STBTT_FIXSHIFT);
						else
							i = -1; // clip

						if (j < len) // add antialiasing for x1
							scanline[j] = scanline[j] + (stbtt_uint8) (((x1 & STBTT_FIXMASK) * max_weight) >> STBTT_FIXSHIFT);
						else
							j = len; // clip

						for (++i; i < j; ++i) // fill pixels between x0 and x1
							scanline[i] = scanline[i] + (stbtt_uint8) max_weight;
					}
				}
			}
		}

		e = e->next;
	}
}

static void stbtt__rasterize_sorted_edges(stbtt__bitmap *result, stbtt__edge *e, int n, int vsubsample, int off_x, int off_y, void *userdata)
{
	stbtt__hheap hh = { 0, 0, 0 };
	stbtt__active_edge *active = NULL;
	int y,j=0;
	int max_weight = (255 / vsubsample);  // weight per vertical scanline
	int s; // vertical subsample index
	unsigned char scanline_data[512], *scanline;

	if (result->w > 512)
		scanline = (unsigned char *) STBTT_malloc(result->w, userdata);
	else
		scanline = scanline_data;

	y = off_y * vsubsample;
	e[n].y0 = (off_y + result->h) * (float) vsubsample + 1;

	while (j < result->h) {
		STBTT_memset(scanline, 0, result->w);
		for (s=0; s < vsubsample; ++s) {
			// find center of pixel for this scanline
			float scan_y = y + 0.5f;
			stbtt__active_edge **step = &active;

			// update all active edges;
			// remove all active edges that terminate before the center of this scanline
			while (*step) {
				stbtt__active_edge * z = *step;
				if (z->ey <= scan_y) {
					*step = z->next; // delete from list
					STBTT_assert(z->direction);
					z->direction = 0;
					stbtt__hheap_free(&hh, z);
				} else {
					z->x += z->dx; // advance to position for current scanline
					step = &((*step)->next); // advance through list
				}
			}

			// resort the list if needed
			for(;;) {
				int changed=0;
				step = &active;
				while (*step && (*step)->next) {
					if ((*step)->x > (*step)->next->x) {
						stbtt__active_edge *t = *step;
						stbtt__active_edge *q = t->next;

						t->next = q->next;
						q->next = t;
						*step = q;
						changed = 1;
					}
					step = &(*step)->next;
				}
				if (!changed) break;
			}

			// insert all edges that start before the center of this scanline -- omit ones that also end on this scanline
			while (e->y0 <= scan_y) {
				if (e->y1 > scan_y) {
					stbtt__active_edge *z = stbtt__new_active(&hh, e, off_x, scan_y, userdata);
					if (z != NULL) {
						// find insertion point
						if (active == NULL)
							active = z;
						else if (z->x < active->x) {
							// insert at front
							z->next = active;
							active = z;
						} else {
							// find thing to insert AFTER
							stbtt__active_edge *p = active;
							while (p->next && p->next->x < z->x)
								p = p->next;
							// at this point, p->next->x is NOT < z->x
							z->next = p->next;
							p->next = z;
						}
					}
				}
				++e;
			}

			// now process all active edges in XOR fashion
			if (active)
				stbtt__fill_active_edges(scanline, result->w, active, max_weight);

			++y;
		}
		STBTT_memcpy(result->pixels + j * result->stride, scanline, result->w);
		++j;
	}

	stbtt__hheap_cleanup(&hh, userdata);

	if (scanline != scanline_data)
		STBTT_free(scanline, userdata);
}

#elif STBTT_RASTERIZER_VERSION == 2

// the edge passed in here does not cross the vertical line at x or the vertical line at x+1
// (i.e. it has already been clipped to those)
static void stbtt__handle_clipped_edge(float *scanline, int x, stbtt__active_edge *e, float x0, float y0, float x1, float y1)
{
	if (y0 == y1) return;
	STBTT_assert(y0 < y1);
	STBTT_assert(e->sy <= e->ey);
	if (y0 > e->ey) return;
	if (y1 < e->sy) return;
	if (y0 < e->sy) {
		x0 += (x1-x0) * (e->sy - y0) / (y1-y0);
		y0 = e->sy;
	}
	if (y1 > e->ey) {
		x1 += (x1-x0) * (e->ey - y1) / (y1-y0);
		y1 = e->ey;
	}

	if (x0 == x)
		STBTT_assert(x1 <= x+1);
	else if (x0 == x+1)
		STBTT_assert(x1 >= x);
	else if (x0 <= x)
		STBTT_assert(x1 <= x);
	else if (x0 >= x+1)
		STBTT_assert(x1 >= x+1);
	else
		STBTT_assert(x1 >= x && x1 <= x+1);

	if (x0 <= x && x1 <= x)
		scanline[x] += e->direction * (y1-y0);
	else if (x0 >= x+1 && x1 >= x+1)
		;
	else {
		STBTT_assert(x0 >= x && x0 <= x+1 && x1 >= x && x1 <= x+1);
		scanline[x] += e->direction * (y1-y0) * (1-((x0-x)+(x1-x))/2); // coverage = 1 - average x position
	}
}

static float stbtt__sized_trapezoid_area(float height, float top_width, float bottom_width)
{
	STBTT_assert(top_width >= 0);
	STBTT_assert(bottom_width >= 0);
	return (top_width + bottom_width) / 2.0f * height;
}

static float stbtt__position_trapezoid_area(float height, float tx0, float tx1, float bx0, float bx1)
{
	return stbtt__sized_trapezoid_area(height, tx1 - tx0, bx1 - bx0);
}

static float stbtt__sized_triangle_area(float height, float width)
{
	return height * width / 2;
}

static void stbtt__fill_active_edges_new(float *scanline, float *scanline_fill, int len, stbtt__active_edge *e, float y_top)
{
	float y_bottom = y_top+1;

	while (e) {
		// brute force every pixel

		// compute intersection points with top & bottom
		STBTT_assert(e->ey >= y_top);

		if (e->fdx == 0) {
			float x0 = e->fx;
			if (x0 < len) {
				if (x0 >= 0) {
					stbtt__handle_clipped_edge(scanline,(int) x0,e, x0,y_top, x0,y_bottom);
					stbtt__handle_clipped_edge(scanline_fill-1,(int) x0+1,e, x0,y_top, x0,y_bottom);
				} else {
					stbtt__handle_clipped_edge(scanline_fill-1,0,e, x0,y_top, x0,y_bottom);
				}
			}
		} else {
			float x0 = e->fx;
			float dx = e->fdx;
			float xb = x0 + dx;
			float x_top, x_bottom;
			float sy0,sy1;
			float dy = e->fdy;
			STBTT_assert(e->sy <= y_bottom && e->ey >= y_top);

			// compute endpoints of line segment clipped to this scanline (if the
			// line segment starts on this scanline. x0 is the intersection of the
			// line with y_top, but that may be off the line segment.
			if (e->sy > y_top) {
				x_top = x0 + dx * (e->sy - y_top);
				sy0 = e->sy;
			} else {
				x_top = x0;
				sy0 = y_top;
			}
			if (e->ey < y_bottom) {
				x_bottom = x0 + dx * (e->ey - y_top);
				sy1 = e->ey;
			} else {
				x_bottom = xb;
				sy1 = y_bottom;
			}

			if (x_top >= 0 && x_bottom >= 0 && x_top < len && x_bottom < len) {
				// from here on, we don't have to range check x values

				if ((int) x_top == (int) x_bottom) {
					float height;
					// simple case, only spans one pixel
					int x = (int) x_top;
					height = (sy1 - sy0) * e->direction;
					STBTT_assert(x >= 0 && x < len);
					scanline[x]      += stbtt__position_trapezoid_area(height, x_top, x+1.0f, x_bottom, x+1.0f);
					scanline_fill[x] += height; // everything right of this pixel is filled
				} else {
					int x,x1,x2;
					float y_crossing, y_final, step, sign, area;
					// covers 2+ pixels
					if (x_top > x_bottom) {
						// flip scanline vertically; signed area is the same
						float t;
						sy0 = y_bottom - (sy0 - y_top);
						sy1 = y_bottom - (sy1 - y_top);
						t = sy0, sy0 = sy1, sy1 = t;
						t = x_bottom, x_bottom = x_top, x_top = t;
						dx = -dx;
						dy = -dy;
						t = x0, x0 = xb, xb = t;
					}
					STBTT_assert(dy >= 0);
					STBTT_assert(dx >= 0);

					x1 = (int) x_top;
					x2 = (int) x_bottom;
					// compute intersection with y axis at x1+1
					y_crossing = y_top + dy * (x1+1 - x0);

					// compute intersection with y axis at x2
					y_final = y_top + dy * (x2 - x0);

					//           x1    x_top                            x2    x_bottom
					//     y_top  +------|-----+------------+------------+--------|---+------------+
					//            |            |            |            |            |            |
					//            |            |            |            |            |            |
					//       sy0  |      Txxxxx|............|............|............|............|
					// y_crossing |            *xxxxx.......|............|............|............|
					//            |            |     xxxxx..|............|............|............|
					//            |            |     /-   xx*xxxx........|............|............|
					//            |            | dy <       |    xxxxxx..|............|............|
					//   y_final  |            |     \-     |          xx*xxx.........|............|
					//       sy1  |            |            |            |   xxxxxB...|............|
					//            |            |            |            |            |            |
					//            |            |            |            |            |            |
					//  y_bottom  +------------+------------+------------+------------+------------+
					//
					// goal is to measure the area covered by '.' in each pixel

					// if x2 is right at the right edge of x1, y_crossing can blow up, github #1057
					// @TODO: maybe test against sy1 rather than y_bottom?
					if (y_crossing > y_bottom)
						y_crossing = y_bottom;

					sign = e->direction;

					// area of the rectangle covered from sy0..y_crossing
					area = sign * (y_crossing-sy0);

					// area of the triangle (x_top,sy0), (x1+1,sy0), (x1+1,y_crossing)
					scanline[x1] += stbtt__sized_triangle_area(area, x1+1 - x_top);

					// check if final y_crossing is blown up; no test case for this
					if (y_final > y_bottom) {
						y_final = y_bottom;
						dy = (y_final - y_crossing ) / (x2 - (x1+1)); // if denom=0, y_final = y_crossing, so y_final <= y_bottom
					}

					// in second pixel, area covered by line segment found in first pixel
					// is always a rectangle 1 wide * the height of that line segment; this
					// is exactly what the variable 'area' stores. it also gets a contribution
					// from the line segment within it. the THIRD pixel will get the first
					// pixel's rectangle contribution, the second pixel's rectangle contribution,
					// and its own contribution. the 'own contribution' is the same in every pixel except
					// the leftmost and rightmost, a trapezoid that slides down in each pixel.
					// the second pixel's contribution to the third pixel will be the
					// rectangle 1 wide times the height change in the second pixel, which is dy.

					step = sign * dy * 1; // dy is dy/dx, change in y for every 1 change in x,
					// which multiplied by 1-pixel-width is how much pixel area changes for each step in x
					// so the area advances by 'step' every time

					for (x = x1+1; x < x2; ++x) {
						scanline[x] += area + step/2; // area of trapezoid is 1*step/2
						area += step;
					}
					STBTT_assert(STBTT_fabs(area) <= 1.01f); // accumulated error from area += step unless we round step down
					STBTT_assert(sy1 > y_final-0.01f);

					// area covered in the last pixel is the rectangle from all the pixels to the left,
					// plus the trapezoid filled by the line segment in this pixel all the way to the right edge
					scanline[x2] += area + sign * stbtt__position_trapezoid_area(sy1-y_final, (float) x2, x2+1.0f, x_bottom, x2+1.0f);

					// the rest of the line is filled based on the total height of the line segment in this pixel
					scanline_fill[x2] += sign * (sy1-sy0);
				}
			} else {
				// if edge goes outside of box we're drawing, we require
				// clipping logic. since this does not match the intended use
				// of this library, we use a different, very slow brute
				// force implementation
				// note though that this does happen some of the time because
				// x_top and x_bottom can be extrapolated at the top & bottom of
				// the shape and actually lie outside the bounding box
				int x;
				for (x=0; x < len; ++x) {
					// cases:
					//
					// there can be up to two intersections with the pixel. any intersection
					// with left or right edges can be handled by splitting into two (or three)
					// regions. intersections with top & bottom do not necessitate case-wise logic.
					//
					// the old way of doing this found the intersections with the left & right edges,
					// then used some simple logic to produce up to three segments in sorted order
					// from top-to-bottom. however, this had a problem: if an x edge was epsilon
					// across the x border, then the corresponding y position might not be distinct
					// from the other y segment, and it might ignored as an empty segment. to avoid
					// that, we need to explicitly produce segments based on x positions.

					// rename variables to clearly-defined pairs
					float y0 = y_top;
					float x1 = (float) (x);
					float x2 = (float) (x+1);
					float x3 = xb;
					float y3 = y_bottom;

					// x = e->x + e->dx * (y-y_top)
					// (y-y_top) = (x - e->x) / e->dx
					// y = (x - e->x) / e->dx + y_top
					float y1 = (x - x0) / dx + y_top;
					float y2 = (x+1 - x0) / dx + y_top;

					if (x0 < x1 && x3 > x2) {         // three segments descending down-right
						stbtt__handle_clipped_edge(scanline,x,e, x0,y0, x1,y1);
						stbtt__handle_clipped_edge(scanline,x,e, x1,y1, x2,y2);
						stbtt__handle_clipped_edge(scanline,x,e, x2,y2, x3,y3);
					} else if (x3 < x1 && x0 > x2) {  // three segments descending down-left
						stbtt__handle_clipped_edge(scanline,x,e, x0,y0, x2,y2);
						stbtt__handle_clipped_edge(scanline,x,e, x2,y2, x1,y1);
						stbtt__handle_clipped_edge(scanline,x,e, x1,y1, x3,y3);
					} else if (x0 < x1 && x3 > x1) {  // two segments across x, down-right
						stbtt__handle_clipped_edge(scanline,x,e, x0,y0, x1,y1);
						stbtt__handle_clipped_edge(scanline,x,e, x1,y1, x3,y3);
					} else if (x3 < x1 && x0 > x1) {  // two segments across x, down-left
						stbtt__handle_clipped_edge(scanline,x,e, x0,y0, x1,y1);
						stbtt__handle_clipped_edge(scanline,x,e, x1,y1, x3,y3);
					} else if (x0 < x2 && x3 > x2) {  // two segments across x+1, down-right
						stbtt__handle_clipped_edge(scanline,x,e, x0,y0, x2,y2);
						stbtt__handle_clipped_edge(scanline,x,e, x2,y2, x3,y3);
					} else if (x3 < x2 && x0 > x2) {  // two segments across x+1, down-left
						stbtt__handle_clipped_edge(scanline,x,e, x0,y0, x2,y2);
						stbtt__handle_clipped_edge(scanline,x,e, x2,y2, x3,y3);
					} else {  // one segment
						stbtt__handle_clipped_edge(scanline,x,e, x0,y0, x3,y3);
					}
				}
			}
		}
		e = e->next;
	}
}

// directly AA rasterize edges w/o supersampling
static void stbtt__rasterize_sorted_edges(stbtt__bitmap *result, stbtt__edge *e, int n, int vsubsample, int off_x, int off_y, void *userdata)
{
	stbtt__hheap hh = { 0, 0, 0 };
	stbtt__active_edge *active = NULL;
	int y,j=0, i;
	float scanline_data[129], *scanline, *scanline2;

	STBTT__NOTUSED(vsubsample);

	if (result->w > 64)
		scanline = (float *) STBTT_malloc((result->w*2+1) * sizeof(float), userdata);
	else
		scanline = scanline_data;

	scanline2 = scanline + result->w;

	y = off_y;
	e[n].y0 = (float) (off_y + result->h) + 1;

	while (j < result->h) {
		// find center of pixel for this scanline
		float scan_y_top    = y + 0.0f;
		float scan_y_bottom = y + 1.0f;
		stbtt__active_edge **step = &active;

		STBTT_memset(scanline , 0, result->w*sizeof(scanline[0]));
		STBTT_memset(scanline2, 0, (result->w+1)*sizeof(scanline[0]));

		// update all active edges;
		// remove all active edges that terminate before the top of this scanline
		while (*step) {
			stbtt__active_edge * z = *step;
			if (z->ey <= scan_y_top) {
				*step = z->next; // delete from list
				STBTT_assert(z->direction);
				z->direction = 0;
				stbtt__hheap_free(&hh, z);
			} else {
				step = &((*step)->next); // advance through list
			}
		}

		// insert all edges that start before the bottom of this scanline
		while (e->y0 <= scan_y_bottom) {
			if (e->y0 != e->y1) {
				stbtt__active_edge *z = stbtt__new_active(&hh, e, off_x, scan_y_top, userdata);
				if (z != NULL) {
					if (j == 0 && off_y != 0) {
						if (z->ey < scan_y_top) {
							// this can happen due to subpixel positioning and some kind of fp rounding error i think
							z->ey = scan_y_top;
						}
					}
					STBTT_assert(z->ey >= scan_y_top); // if we get really unlucky a tiny bit of an edge can be out of bounds
					// insert at front
					z->next = active;
					active = z;
				}
			}
			++e;
		}

		// now process all active edges
		if (active)
			stbtt__fill_active_edges_new(scanline, scanline2+1, result->w, active, scan_y_top);

		{
			float sum = 0;
			for (i=0; i < result->w; ++i) {
				float k;
				int m;
				sum += scanline2[i];
				k = scanline[i] + sum;
				k = (float) STBTT_fabs(k)*255 + 0.5f;
				m = (int) k;
				if (m > 255) m = 255;
				result->pixels[j*result->stride + i] = (unsigned char) m;
			}
		}
		// advance all the edges
		step = &active;
		while (*step) {
			stbtt__active_edge *z = *step;
			z->fx += z->fdx; // advance to position for current scanline
			step = &((*step)->next); // advance through list
		}

		++y;
		++j;
	}

	stbtt__hheap_cleanup(&hh, userdata);

	if (scanline != scanline_data)
		STBTT_free(scanline, userdata);
}
#else
#error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#endif

#define STBTT__COMPARE(a,b)  ((a)->y0 < (b)->y0)

static void stbtt__sort_edges_ins_sort(stbtt__edge *p, int n)
{
	int i,j;
	for (i=1; i < n; ++i) {
		stbtt__edge t = p[i], *a = &t;
		j = i;
		while (j > 0) {
			stbtt__edge *b = &p[j-1];
			int c = STBTT__COMPARE(a,b);
			if (!c) break;
			p[j] = p[j-1];
			--j;
		}
		if (i != j)
			p[j] = t;
	}
}

static void stbtt__sort_edges_quicksort(stbtt__edge *p, int n)
{
	/* threshold for transitioning to insertion sort */
	while (n > 12) {
		stbtt__edge t;
		int c01,c12,c,m,i,j;

		/* compute median of three */
		m = n >> 1;
		c01 = STBTT__COMPARE(&p[0],&p[m]);
		c12 = STBTT__COMPARE(&p[m],&p[n-1]);
		/* if 0 >= mid >= end, or 0 < mid < end, then use mid */
		if (c01 != c12) {
			/* otherwise, we'll need to swap something else to middle */
			int z;
			c = STBTT__COMPARE(&p[0],&p[n-1]);
			/* 0>mid && mid<n:  0>n => n; 0<n => 0 */
			/* 0<mid && mid>n:  0>n => 0; 0<n => n */
			z = (c == c12) ? 0 : n-1;
			t = p[z];
			p[z] = p[m];
			p[m] = t;
		}
		/* now p[m] is the median-of-three */
		/* swap it to the beginning so it won't move around */
		t = p[0];
		p[0] = p[m];
		p[m] = t;

		/* partition loop */
		i=1;
		j=n-1;
		for(;;) {
			/* handling of equality is crucial here */
			/* for sentinels & efficiency with duplicates */
			for (;;++i) {
				if (!STBTT__COMPARE(&p[i], &p[0])) break;
			}
			for (;;--j) {
				if (!STBTT__COMPARE(&p[0], &p[j])) break;
			}
			/* make sure we haven't crossed */
			if (i >= j) break;
			t = p[i];
			p[i] = p[j];
			p[j] = t;

			++i;
			--j;
		}
		/* recurse on smaller side, iterate on larger */
		if (j < (n-i)) {
			stbtt__sort_edges_quicksort(p,j);
			p = p+i;
			n = n-i;
		} else {
			stbtt__sort_edges_quicksort(p+i, n-i);
			n = j;
		}
	}
}

static void stbtt__sort_edges(stbtt__edge *p, int n)
{
	stbtt__sort_edges_quicksort(p, n);
	stbtt__sort_edges_ins_sort(p, n);
}

typedef struct
{
	float x,y;
} stbtt__point;

static void stbtt__rasterize(stbtt__bitmap *result, stbtt__point *pts, int *wcount, int windings, float scale_x, float scale_y, float shift_x, float shift_y, int off_x, int off_y, int invert, void *userdata)
{
	float y_scale_inv = invert ? -scale_y : scale_y;
	stbtt__edge *e;
	int n,i,j,k,m;
	#if STBTT_RASTERIZER_VERSION == 1
	int vsubsample = result->h < 8 ? 15 : 5;
	#elif STBTT_RASTERIZER_VERSION == 2
	int vsubsample = 1;
	#else
	#error "Unrecognized value of STBTT_RASTERIZER_VERSION"
	#endif
	// vsubsample should divide 255 evenly; otherwise we won't reach full opacity

	// now we have to blow out the windings into explicit edge lists
	n = 0;
	for (i=0; i < windings; ++i)
		n += wcount[i];

	e = (stbtt__edge *) STBTT_malloc(sizeof(*e) * (n+1), userdata); // add an extra one as a sentinel
	if (e == 0) return;
	n = 0;

	m=0;
	for (i=0; i < windings; ++i) {
		stbtt__point *p = pts + m;
		m += wcount[i];
		j = wcount[i]-1;
		for (k=0; k < wcount[i]; j=k++) {
			int a=k,b=j;
			// skip the edge if horizontal
			if (p[j].y == p[k].y)
				continue;
			// add edge from j to k to the list
			e[n].invert = 0;
			if (invert ? p[j].y > p[k].y : p[j].y < p[k].y) {
				e[n].invert = 1;
				a=j,b=k;
			}
			e[n].x0 = p[a].x * scale_x + shift_x;
			e[n].y0 = (p[a].y * y_scale_inv + shift_y) * vsubsample;
			e[n].x1 = p[b].x * scale_x + shift_x;
			e[n].y1 = (p[b].y * y_scale_inv + shift_y) * vsubsample;
			++n;
		}
	}

	// now sort the edges by their highest point (should snap to integer, and then by x)
	//STBTT_sort(e, n, sizeof(e[0]), stbtt__edge_compare);
	stbtt__sort_edges(e, n);

	// now, traverse the scanlines and find the intersections on each scanline, use xor winding rule
	stbtt__rasterize_sorted_edges(result, e, n, vsubsample, off_x, off_y, userdata);

	STBTT_free(e, userdata);
}

static void stbtt__add_point(stbtt__point *points, int n, float x, float y)
{
	if (!points) return; // during first pass, it's unallocated
	points[n].x = x;
	points[n].y = y;
}

// tessellate until threshold p is happy... @TODO warped to compensate for non-linear stretching
static int stbtt__tesselate_curve(stbtt__point *points, int *num_points, float x0, float y0, float x1, float y1, float x2, float y2, float objspace_flatness_squared, int n)
{
	// midpoint
	float mx = (x0 + 2*x1 + x2)/4;
	float my = (y0 + 2*y1 + y2)/4;
	// versus directly drawn line
	float dx = (x0+x2)/2 - mx;
	float dy = (y0+y2)/2 - my;
	if (n > 16) // 65536 segments on one curve better be enough!
		return 1;
	if (dx*dx+dy*dy > objspace_flatness_squared) { // half-pixel error allowed... need to be smaller if AA
		stbtt__tesselate_curve(points, num_points, x0,y0, (x0+x1)/2.0f,(y0+y1)/2.0f, mx,my, objspace_flatness_squared,n+1);
		stbtt__tesselate_curve(points, num_points, mx,my, (x1+x2)/2.0f,(y1+y2)/2.0f, x2,y2, objspace_flatness_squared,n+1);
	} else {
		stbtt__add_point(points, *num_points,x2,y2);
		*num_points = *num_points+1;
	}
	return 1;
}

static void stbtt__tesselate_cubic(stbtt__point *points, int *num_points, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float objspace_flatness_squared, int n)
{
	// @TODO this "flatness" calculation is just made-up nonsense that seems to work well enough
	float dx0 = x1-x0;
	float dy0 = y1-y0;
	float dx1 = x2-x1;
	float dy1 = y2-y1;
	float dx2 = x3-x2;
	float dy2 = y3-y2;
	float dx = x3-x0;
	float dy = y3-y0;
	float longlen = (float) (STBTT_sqrt(dx0*dx0+dy0*dy0)+STBTT_sqrt(dx1*dx1+dy1*dy1)+STBTT_sqrt(dx2*dx2+dy2*dy2));
	float shortlen = (float) STBTT_sqrt(dx*dx+dy*dy);
	float flatness_squared = longlen*longlen-shortlen*shortlen;

	if (n > 16) // 65536 segments on one curve better be enough!
		return;

	if (flatness_squared > objspace_flatness_squared) {
		float x01 = (x0+x1)/2;
		float y01 = (y0+y1)/2;
		float x12 = (x1+x2)/2;
		float y12 = (y1+y2)/2;
		float x23 = (x2+x3)/2;
		float y23 = (y2+y3)/2;

		float xa = (x01+x12)/2;
		float ya = (y01+y12)/2;
		float xb = (x12+x23)/2;
		float yb = (y12+y23)/2;

		float mx = (xa+xb)/2;
		float my = (ya+yb)/2;

		stbtt__tesselate_cubic(points, num_points, x0,y0, x01,y01, xa,ya, mx,my, objspace_flatness_squared,n+1);
		stbtt__tesselate_cubic(points, num_points, mx,my, xb,yb, x23,y23, x3,y3, objspace_flatness_squared,n+1);
	} else {
		stbtt__add_point(points, *num_points,x3,y3);
		*num_points = *num_points+1;
	}
}

// returns number of contours
static stbtt__point *stbtt_FlattenCurves(stbtt_vertex *vertices, int num_verts, float objspace_flatness, int **contour_lengths, int *num_contours, void *userdata)
{
	stbtt__point *points=0;
	int num_points=0;

	float objspace_flatness_squared = objspace_flatness * objspace_flatness;
	int i,n=0,start=0, pass;

	// count how many "moves" there are to get the contour count
	for (i=0; i < num_verts; ++i)
		if (vertices[i].type == STBTT_vmove)
			++n;

	*num_contours = n;
	if (n == 0) return 0;

	*contour_lengths = (int *) STBTT_malloc(sizeof(**contour_lengths) * n, userdata);

	if (*contour_lengths == 0) {
		*num_contours = 0;
		return 0;
	}

	// make two passes through the points so we don't need to realloc
	for (pass=0; pass < 2; ++pass) {
		float x=0,y=0;
		if (pass == 1) {
			points = (stbtt__point *) STBTT_malloc(num_points * sizeof(points[0]), userdata);
			if (points == NULL) goto error;
		}
		num_points = 0;
		n= -1;
		for (i=0; i < num_verts; ++i) {
			switch (vertices[i].type) {
				case STBTT_vmove:
					// start the next contour
					if (n >= 0)
						(*contour_lengths)[n] = num_points - start;
					++n;
					start = num_points;

					x = vertices[i].x, y = vertices[i].y;
					stbtt__add_point(points, num_points++, x,y);
					break;
				case STBTT_vline:
					x = vertices[i].x, y = vertices[i].y;
					stbtt__add_point(points, num_points++, x, y);
					break;
				case STBTT_vcurve:
					stbtt__tesselate_curve(points, &num_points, x,y,
										   vertices[i].cx, vertices[i].cy,
										   vertices[i].x,  vertices[i].y,
										   objspace_flatness_squared, 0);
					x = vertices[i].x, y = vertices[i].y;
					break;
				case STBTT_vcubic:
					stbtt__tesselate_cubic(points, &num_points, x,y,
										   vertices[i].cx, vertices[i].cy,
										   vertices[i].cx1, vertices[i].cy1,
										   vertices[i].x,  vertices[i].y,
										   objspace_flatness_squared, 0);
					x = vertices[i].x, y = vertices[i].y;
					break;
			}
		}
		(*contour_lengths)[n] = num_points - start;
	}

	return points;
	error:
		STBTT_free(points, userdata);
	STBTT_free(*contour_lengths, userdata);
	*contour_lengths = 0;
	*num_contours = 0;
	return NULL;
}

STBTT_DEF void stbtt_Rasterize(stbtt__bitmap *result, float flatness_in_pixels, stbtt_vertex *vertices, int num_verts, float scale_x, float scale_y, float shift_x, float shift_y, int x_off, int y_off, int invert, void *userdata)
{
	float scale            = scale_x > scale_y ? scale_y : scale_x;
	int winding_count      = 0;
	int *winding_lengths   = NULL;
	stbtt__point *windings = stbtt_FlattenCurves(vertices, num_verts, flatness_in_pixels / scale, &winding_lengths, &winding_count, userdata);
	if (windings) {
		stbtt__rasterize(result, windings, winding_lengths, winding_count, scale_x, scale_y, shift_x, shift_y, x_off, y_off, invert, userdata);
		STBTT_free(winding_lengths, userdata);
		STBTT_free(windings, userdata);
	}
}

STBTT_DEF void stbtt_FreeBitmap(unsigned char *bitmap, void *userdata)
{
	STBTT_free(bitmap, userdata);
}

STBTT_DEF unsigned char *stbtt_GetGlyphBitmapSubpixel(const stbtt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int glyph, int *width, int *height, int *xoff, int *yoff)
{
	int ix0,iy0,ix1,iy1;
	stbtt__bitmap gbm;
	stbtt_vertex *vertices;
	int num_verts = stbtt_GetGlyphShape(info, glyph, &vertices);

	if (scale_x == 0) scale_x = scale_y;
	if (scale_y == 0) {
		if (scale_x == 0) {
			STBTT_free(vertices, info->userdata);
			return NULL;
		}
		scale_y = scale_x;
	}

	stbtt_GetGlyphBitmapBoxSubpixel(info, glyph, scale_x, scale_y, shift_x, shift_y, &ix0,&iy0,&ix1,&iy1);

	// now we get the size
	gbm.w = (ix1 - ix0);
	gbm.h = (iy1 - iy0);
	gbm.pixels = NULL; // in case we error

	if (width ) *width  = gbm.w;
	if (height) *height = gbm.h;
	if (xoff  ) *xoff   = ix0;
	if (yoff  ) *yoff   = iy0;

	if (gbm.w && gbm.h) {
		gbm.pixels = (unsigned char *) STBTT_malloc(gbm.w * gbm.h, info->userdata);
		if (gbm.pixels) {
			gbm.stride = gbm.w;

			stbtt_Rasterize(&gbm, 0.35f, vertices, num_verts, scale_x, scale_y, shift_x, shift_y, ix0, iy0, 1, info->userdata);
		}
	}
	STBTT_free(vertices, info->userdata);
	return gbm.pixels;
}

STBTT_DEF unsigned char *stbtt_GetGlyphBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y, int glyph, int *width, int *height, int *xoff, int *yoff)
{
	return stbtt_GetGlyphBitmapSubpixel(info, scale_x, scale_y, 0.0f, 0.0f, glyph, width, height, xoff, yoff);
}

STBTT_DEF void stbtt_MakeGlyphBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int glyph)
{
	int ix0,iy0;
	stbtt_vertex *vertices;
	int num_verts = stbtt_GetGlyphShape(info, glyph, &vertices);
	stbtt__bitmap gbm;

	stbtt_GetGlyphBitmapBoxSubpixel(info, glyph, scale_x, scale_y, shift_x, shift_y, &ix0,&iy0,0,0);
	gbm.pixels = output;
	gbm.w = out_w;
	gbm.h = out_h;
	gbm.stride = out_stride;

	if (gbm.w && gbm.h)
		stbtt_Rasterize(&gbm, 0.35f, vertices, num_verts, scale_x, scale_y, shift_x, shift_y, ix0,iy0, 1, info->userdata);

	STBTT_free(vertices, info->userdata);
}

STBTT_DEF void stbtt_MakeGlyphBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int glyph)
{
	stbtt_MakeGlyphBitmapSubpixel(info, output, out_w, out_h, out_stride, scale_x, scale_y, 0.0f,0.0f, glyph);
}

STBTT_DEF unsigned char *stbtt_GetCodepointBitmapSubpixel(const stbtt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint, int *width, int *height, int *xoff, int *yoff)
{
	return stbtt_GetGlyphBitmapSubpixel(info, scale_x, scale_y,shift_x,shift_y, stbtt_FindGlyphIndex(info,codepoint), width,height,xoff,yoff);
}

STBTT_DEF void stbtt_MakeCodepointBitmapSubpixelPrefilter(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int oversample_x, int oversample_y, float *sub_x, float *sub_y, int codepoint)
{
	stbtt_MakeGlyphBitmapSubpixelPrefilter(info, output, out_w, out_h, out_stride, scale_x, scale_y, shift_x, shift_y, oversample_x, oversample_y, sub_x, sub_y, stbtt_FindGlyphIndex(info,codepoint));
}

STBTT_DEF void stbtt_MakeCodepointBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint)
{
	stbtt_MakeGlyphBitmapSubpixel(info, output, out_w, out_h, out_stride, scale_x, scale_y, shift_x, shift_y, stbtt_FindGlyphIndex(info,codepoint));
}

STBTT_DEF unsigned char *stbtt_GetCodepointBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y, int codepoint, int *width, int *height, int *xoff, int *yoff)
{
	return stbtt_GetCodepointBitmapSubpixel(info, scale_x, scale_y, 0.0f,0.0f, codepoint, width,height,xoff,yoff);
}

STBTT_DEF void stbtt_MakeCodepointBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int codepoint)
{
	stbtt_MakeCodepointBitmapSubpixel(info, output, out_w, out_h, out_stride, scale_x, scale_y, 0.0f,0.0f, codepoint);
}

//////////////////////////////////////////////////////////////////////////////
//
// bitmap baking
//
// This is SUPER-CRAPPY packing to keep source code small

static int stbtt_BakeFontBitmap_internal(unsigned char *data, int offset,  // font location (use offset=0 for plain .ttf)
										 float pixel_height,                     // height of font in pixels
										 unsigned char *pixels, int pw, int ph,  // bitmap to be filled in
										 int first_char, int num_chars,          // characters to bake
										 stbtt_bakedchar *chardata)
{
	float scale;
	int x,y,bottom_y, i;
	stbtt_fontinfo f;
	f.userdata = NULL;
	if (!stbtt_InitFont(&f, data, offset))
		return -1;
	STBTT_memset(pixels, 0, pw*ph); // background of 0 around pixels
	x=y=1;
	bottom_y = 1;

	scale = stbtt_ScaleForPixelHeight(&f, pixel_height);

	for (i=0; i < num_chars; ++i) {
		int advance, lsb, x0,y0,x1,y1,gw,gh;
		int g = stbtt_FindGlyphIndex(&f, first_char + i);
		stbtt_GetGlyphHMetrics(&f, g, &advance, &lsb);
		stbtt_GetGlyphBitmapBox(&f, g, scale,scale, &x0,&y0,&x1,&y1);
		gw = x1-x0;
		gh = y1-y0;
		if (x + gw + 1 >= pw)
			y = bottom_y, x = 1; // advance to next row
		if (y + gh + 1 >= ph) // check if it fits vertically AFTER potentially moving to next row
			return -i;
		STBTT_assert(x+gw < pw);
		STBTT_assert(y+gh < ph);
		stbtt_MakeGlyphBitmap(&f, pixels+x+y*pw, gw,gh,pw, scale,scale, g);
		chardata[i].x0 = (stbtt_int16) x;
		chardata[i].y0 = (stbtt_int16) y;
		chardata[i].x1 = (stbtt_int16) (x + gw);
		chardata[i].y1 = (stbtt_int16) (y + gh);
		chardata[i].xadvance = scale * advance;
		chardata[i].xoff     = (float) x0;
		chardata[i].yoff     = (float) y0;
		x = x + gw + 1;
		if (y+gh+1 > bottom_y)
			bottom_y = y+gh+1;
	}
	return bottom_y;
}

STBTT_DEF void stbtt_GetBakedQuad(const stbtt_bakedchar *chardata, int pw, int ph, int char_index, float *xpos, float *ypos, stbtt_aligned_quad *q, int opengl_fillrule)
{
	float d3d_bias = opengl_fillrule ? 0 : -0.5f;
	float ipw = 1.0f / pw, iph = 1.0f / ph;
	const stbtt_bakedchar *b = chardata + char_index;
	int round_x = STBTT_ifloor((*xpos + b->xoff) + 0.5f);
	int round_y = STBTT_ifloor((*ypos + b->yoff) + 0.5f);

	q->x0 = round_x + d3d_bias;
	q->y0 = round_y + d3d_bias;
	q->x1 = round_x + b->x1 - b->x0 + d3d_bias;
	q->y1 = round_y + b->y1 - b->y0 + d3d_bias;

	q->s0 = b->x0 * ipw;
	q->t0 = b->y0 * iph;
	q->s1 = b->x1 * ipw;
	q->t1 = b->y1 * iph;

	*xpos += b->xadvance;
}

//////////////////////////////////////////////////////////////////////////////
//
// rectangle packing replacement routines if you don't have stb_rect_pack.h
//

#ifndef STB_RECT_PACK_VERSION

typedef int stbrp_coord;

////////////////////////////////////////////////////////////////////////////////////
//                                                                                //
//                                                                                //
// COMPILER WARNING ?!?!?                                                         //
//                                                                                //
//                                                                                //
// if you get a compile warning due to these symbols being defined more than      //
// once, move #include "stb_rect_pack.h" before #include "stb_truetype.h"         //
//                                                                                //
////////////////////////////////////////////////////////////////////////////////////

typedef struct
{
	int width,height;
	int x,y,bottom_y;
} stbrp_context;

typedef struct
{
	unsigned char x;
} stbrp_node;

struct stbrp_rect
{
	stbrp_coord x,y;
	int id,w,h,was_packed;
};

static void stbrp_init_target(stbrp_context *con, int pw, int ph, stbrp_node *nodes, int num_nodes)
{
	con->width  = pw;
	con->height = ph;
	con->x = 0;
	con->y = 0;
	con->bottom_y = 0;
	STBTT__NOTUSED(nodes);
	STBTT__NOTUSED(num_nodes);
}

static void stbrp_pack_rects(stbrp_context *con, stbrp_rect *rects, int num_rects)
{
	int i;
	for (i=0; i < num_rects; ++i) {
		if (con->x + rects[i].w > con->width) {
			con->x = 0;
			con->y = con->bottom_y;
		}
		if (con->y + rects[i].h > con->height)
			break;
		rects[i].x = con->x;
		rects[i].y = con->y;
		rects[i].was_packed = 1;
		con->x += rects[i].w;
		if (con->y + rects[i].h > con->bottom_y)
			con->bottom_y = con->y + rects[i].h;
	}
	for (   ; i < num_rects; ++i)
		rects[i].was_packed = 0;
}
#endif

//////////////////////////////////////////////////////////////////////////////
//
// bitmap baking
//
// This is SUPER-AWESOME (tm Ryan Gordon) packing using stb_rect_pack.h. If
// stb_rect_pack.h isn't available, it uses the BakeFontBitmap strategy.

STBTT_DEF int stbtt_PackBegin(stbtt_pack_context *spc, unsigned char *pixels, int pw, int ph, int stride_in_bytes, int padding, void *alloc_context)
{
	stbrp_context *context = (stbrp_context *) STBTT_malloc(sizeof(*context)            ,alloc_context);
	int            num_nodes = pw - padding;
	stbrp_node    *nodes   = (stbrp_node    *) STBTT_malloc(sizeof(*nodes  ) * num_nodes,alloc_context);

	if (context == NULL || nodes == NULL) {
		if (context != NULL) STBTT_free(context, alloc_context);
		if (nodes   != NULL) STBTT_free(nodes  , alloc_context);
		return 0;
	}

	spc->user_allocator_context = alloc_context;
	spc->width = pw;
	spc->height = ph;
	spc->pixels = pixels;
	spc->pack_info = context;
	spc->nodes = nodes;
	spc->padding = padding;
	spc->stride_in_bytes = stride_in_bytes != 0 ? stride_in_bytes : pw;
	spc->h_oversample = 1;
	spc->v_oversample = 1;
	spc->skip_missing = 0;

	stbrp_init_target(context, pw-padding, ph-padding, nodes, num_nodes);

	if (pixels)
		STBTT_memset(pixels, 0, pw*ph); // background of 0 around pixels

	return 1;
}

STBTT_DEF void stbtt_PackEnd  (stbtt_pack_context *spc)
{
	STBTT_free(spc->nodes    , spc->user_allocator_context);
	STBTT_free(spc->pack_info, spc->user_allocator_context);
}

STBTT_DEF void stbtt_PackSetOversampling(stbtt_pack_context *spc, unsigned int h_oversample, unsigned int v_oversample)
{
	STBTT_assert(h_oversample <= STBTT_MAX_OVERSAMPLE);
	STBTT_assert(v_oversample <= STBTT_MAX_OVERSAMPLE);
	if (h_oversample <= STBTT_MAX_OVERSAMPLE)
		spc->h_oversample = h_oversample;
	if (v_oversample <= STBTT_MAX_OVERSAMPLE)
		spc->v_oversample = v_oversample;
}

STBTT_DEF void stbtt_PackSetSkipMissingCodepoints(stbtt_pack_context *spc, int skip)
{
	spc->skip_missing = skip;
}

#define STBTT__OVER_MASK  (STBTT_MAX_OVERSAMPLE-1)

static void stbtt__h_prefilter(unsigned char *pixels, int w, int h, int stride_in_bytes, unsigned int kernel_width)
{
	unsigned char buffer[STBTT_MAX_OVERSAMPLE];
	int safe_w = w - kernel_width;
	int j;
	STBTT_memset(buffer, 0, STBTT_MAX_OVERSAMPLE); // suppress bogus warning from VS2013 -analyze
	for (j=0; j < h; ++j) {
		int i;
		unsigned int total;
		STBTT_memset(buffer, 0, kernel_width);

		total = 0;

		// make kernel_width a constant in common cases so compiler can optimize out the divide
		switch (kernel_width) {
			case 2:
				for (i=0; i <= safe_w; ++i) {
					total += pixels[i] - buffer[i & STBTT__OVER_MASK];
					buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i];
					pixels[i] = (unsigned char) (total / 2);
				}
				break;
			case 3:
				for (i=0; i <= safe_w; ++i) {
					total += pixels[i] - buffer[i & STBTT__OVER_MASK];
					buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i];
					pixels[i] = (unsigned char) (total / 3);
				}
				break;
			case 4:
				for (i=0; i <= safe_w; ++i) {
					total += pixels[i] - buffer[i & STBTT__OVER_MASK];
					buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i];
					pixels[i] = (unsigned char) (total / 4);
				}
				break;
			case 5:
				for (i=0; i <= safe_w; ++i) {
					total += pixels[i] - buffer[i & STBTT__OVER_MASK];
					buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i];
					pixels[i] = (unsigned char) (total / 5);
				}
				break;
			default:
				for (i=0; i <= safe_w; ++i) {
					total += pixels[i] - buffer[i & STBTT__OVER_MASK];
					buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i];
					pixels[i] = (unsigned char) (total / kernel_width);
				}
				break;
		}

		for (; i < w; ++i) {
			STBTT_assert(pixels[i] == 0);
			total -= buffer[i & STBTT__OVER_MASK];
			pixels[i] = (unsigned char) (total / kernel_width);
		}

		pixels += stride_in_bytes;
	}
}

static void stbtt__v_prefilter(unsigned char *pixels, int w, int h, int stride_in_bytes, unsigned int kernel_width)
{
	unsigned char buffer[STBTT_MAX_OVERSAMPLE];
	int safe_h = h - kernel_width;
	int j;
	STBTT_memset(buffer, 0, STBTT_MAX_OVERSAMPLE); // suppress bogus warning from VS2013 -analyze
	for (j=0; j < w; ++j) {
		int i;
		unsigned int total;
		STBTT_memset(buffer, 0, kernel_width);

		total = 0;

		// make kernel_width a constant in common cases so compiler can optimize out the divide
		switch (kernel_width) {
			case 2:
				for (i=0; i <= safe_h; ++i) {
					total += pixels[i*stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
					buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i*stride_in_bytes];
					pixels[i*stride_in_bytes] = (unsigned char) (total / 2);
				}
				break;
			case 3:
				for (i=0; i <= safe_h; ++i) {
					total += pixels[i*stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
					buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i*stride_in_bytes];
					pixels[i*stride_in_bytes] = (unsigned char) (total / 3);
				}
				break;
			case 4:
				for (i=0; i <= safe_h; ++i) {
					total += pixels[i*stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
					buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i*stride_in_bytes];
					pixels[i*stride_in_bytes] = (unsigned char) (total / 4);
				}
				break;
			case 5:
				for (i=0; i <= safe_h; ++i) {
					total += pixels[i*stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
					buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i*stride_in_bytes];
					pixels[i*stride_in_bytes] = (unsigned char) (total / 5);
				}
				break;
			default:
				for (i=0; i <= safe_h; ++i) {
					total += pixels[i*stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
					buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i*stride_in_bytes];
					pixels[i*stride_in_bytes] = (unsigned char) (total / kernel_width);
				}
				break;
		}

		for (; i < h; ++i) {
			STBTT_assert(pixels[i*stride_in_bytes] == 0);
			total -= buffer[i & STBTT__OVER_MASK];
			pixels[i*stride_in_bytes] = (unsigned char) (total / kernel_width);
		}

		pixels += 1;
	}
}

static float stbtt__oversample_shift(int oversample)
{
	if (!oversample)
		return 0.0f;

	// The prefilter is a box filter of width "oversample",
	// which shifts phase by (oversample - 1)/2 pixels in
	// oversampled space. We want to shift in the opposite
	// direction to counter this.
	return (float)-(oversample - 1) / (2.0f * (float)oversample);
}

// rects array must be big enough to accommodate all characters in the given ranges
STBTT_DEF int stbtt_PackFontRangesGatherRects(stbtt_pack_context *spc, const stbtt_fontinfo *info, stbtt_pack_range *ranges, int num_ranges, stbrp_rect *rects)
{
	int i,j,k;
	int missing_glyph_added = 0;

	k=0;
	for (i=0; i < num_ranges; ++i) {
		float fh = ranges[i].font_size;
		float scale = fh > 0 ? stbtt_ScaleForPixelHeight(info, fh) : stbtt_ScaleForMappingEmToPixels(info, -fh);
		ranges[i].h_oversample = (unsigned char) spc->h_oversample;
		ranges[i].v_oversample = (unsigned char) spc->v_oversample;
		for (j=0; j < ranges[i].num_chars; ++j) {
			int x0,y0,x1,y1;
			int codepoint = ranges[i].array_of_unicode_codepoints == NULL ? ranges[i].first_unicode_codepoint_in_range + j : ranges[i].array_of_unicode_codepoints[j];
			int glyph = stbtt_FindGlyphIndex(info, codepoint);
			if (glyph == 0 && (spc->skip_missing || missing_glyph_added)) {
				rects[k].w = rects[k].h = 0;
			} else {
				stbtt_GetGlyphBitmapBoxSubpixel(info,glyph,
												scale * spc->h_oversample,
												scale * spc->v_oversample,
												0,0,
												&x0,&y0,&x1,&y1);
				rects[k].w = (stbrp_coord) (x1-x0 + spc->padding + spc->h_oversample-1);
				rects[k].h = (stbrp_coord) (y1-y0 + spc->padding + spc->v_oversample-1);
				if (glyph == 0)
					missing_glyph_added = 1;
			}
			++k;
		}
	}

	return k;
}

STBTT_DEF void stbtt_MakeGlyphBitmapSubpixelPrefilter(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int prefilter_x, int prefilter_y, float *sub_x, float *sub_y, int glyph)
{
	stbtt_MakeGlyphBitmapSubpixel(info,
								  output,
								  out_w - (prefilter_x - 1),
								  out_h - (prefilter_y - 1),
								  out_stride,
								  scale_x,
								  scale_y,
								  shift_x,
								  shift_y,
								  glyph);

	if (prefilter_x > 1)
		stbtt__h_prefilter(output, out_w, out_h, out_stride, prefilter_x);

	if (prefilter_y > 1)
		stbtt__v_prefilter(output, out_w, out_h, out_stride, prefilter_y);

	*sub_x = stbtt__oversample_shift(prefilter_x);
	*sub_y = stbtt__oversample_shift(prefilter_y);
}

// rects array must be big enough to accommodate all characters in the given ranges
STBTT_DEF int stbtt_PackFontRangesRenderIntoRects(stbtt_pack_context *spc, const stbtt_fontinfo *info, stbtt_pack_range *ranges, int num_ranges, stbrp_rect *rects)
{
	int i,j,k, missing_glyph = -1, return_value = 1;

	// save current values
	int old_h_over = spc->h_oversample;
	int old_v_over = spc->v_oversample;

	k = 0;
	for (i=0; i < num_ranges; ++i) {
		float fh = ranges[i].font_size;
		float scale = fh > 0 ? stbtt_ScaleForPixelHeight(info, fh) : stbtt_ScaleForMappingEmToPixels(info, -fh);
		float recip_h,recip_v,sub_x,sub_y;
		spc->h_oversample = ranges[i].h_oversample;
		spc->v_oversample = ranges[i].v_oversample;
		recip_h = 1.0f / spc->h_oversample;
		recip_v = 1.0f / spc->v_oversample;
		sub_x = stbtt__oversample_shift(spc->h_oversample);
		sub_y = stbtt__oversample_shift(spc->v_oversample);
		for (j=0; j < ranges[i].num_chars; ++j) {
			stbrp_rect *r = &rects[k];
			if (r->was_packed && r->w != 0 && r->h != 0) {
				stbtt_packedchar *bc = &ranges[i].chardata_for_range[j];
				int advance, lsb, x0,y0,x1,y1;
				int codepoint = ranges[i].array_of_unicode_codepoints == NULL ? ranges[i].first_unicode_codepoint_in_range + j : ranges[i].array_of_unicode_codepoints[j];
				int glyph = stbtt_FindGlyphIndex(info, codepoint);
				stbrp_coord pad = (stbrp_coord) spc->padding;

				// pad on left and top
				r->x += pad;
				r->y += pad;
				r->w -= pad;
				r->h -= pad;
				stbtt_GetGlyphHMetrics(info, glyph, &advance, &lsb);
				stbtt_GetGlyphBitmapBox(info, glyph,
										scale * spc->h_oversample,
										scale * spc->v_oversample,
										&x0,&y0,&x1,&y1);
				stbtt_MakeGlyphBitmapSubpixel(info,
											  spc->pixels + r->x + r->y*spc->stride_in_bytes,
											  r->w - spc->h_oversample+1,
											  r->h - spc->v_oversample+1,
											  spc->stride_in_bytes,
											  scale * spc->h_oversample,
											  scale * spc->v_oversample,
											  0,0,
											  glyph);

				if (spc->h_oversample > 1)
					stbtt__h_prefilter(spc->pixels + r->x + r->y*spc->stride_in_bytes,
									   r->w, r->h, spc->stride_in_bytes,
									   spc->h_oversample);

				if (spc->v_oversample > 1)
					stbtt__v_prefilter(spc->pixels + r->x + r->y*spc->stride_in_bytes,
									   r->w, r->h, spc->stride_in_bytes,
									   spc->v_oversample);

				bc->x0       = (stbtt_int16)  r->x;
				bc->y0       = (stbtt_int16)  r->y;
				bc->x1       = (stbtt_int16) (r->x + r->w);
				bc->y1       = (stbtt_int16) (r->y + r->h);
				bc->xadvance =                scale * advance;
				bc->xoff     =       (float)  x0 * recip_h + sub_x;
				bc->yoff     =       (float)  y0 * recip_v + sub_y;
				bc->xoff2    =                (x0 + r->w) * recip_h + sub_x;
				bc->yoff2    =                (y0 + r->h) * recip_v + sub_y;

				if (glyph == 0)
					missing_glyph = j;
			} else if (spc->skip_missing) {
				return_value = 0;
			} else if (r->was_packed && r->w == 0 && r->h == 0 && missing_glyph >= 0) {
				ranges[i].chardata_for_range[j] = ranges[i].chardata_for_range[missing_glyph];
			} else {
				return_value = 0; // if any fail, report failure
			}

			++k;
		}
	}

	// restore original values
	spc->h_oversample = old_h_over;
	spc->v_oversample = old_v_over;

	return return_value;
}

STBTT_DEF void stbtt_PackFontRangesPackRects(stbtt_pack_context *spc, stbrp_rect *rects, int num_rects)
{
	stbrp_pack_rects((stbrp_context *) spc->pack_info, rects, num_rects);
}

STBTT_DEF int stbtt_PackFontRanges(stbtt_pack_context *spc, const unsigned char *fontdata, int font_index, stbtt_pack_range *ranges, int num_ranges)
{
	stbtt_fontinfo info;
	int i,j,n, return_value = 1;
	//stbrp_context *context = (stbrp_context *) spc->pack_info;
	stbrp_rect    *rects;

	// flag all characters as NOT packed
	for (i=0; i < num_ranges; ++i)
		for (j=0; j < ranges[i].num_chars; ++j)
			ranges[i].chardata_for_range[j].x0 =
			ranges[i].chardata_for_range[j].y0 =
			ranges[i].chardata_for_range[j].x1 =
			ranges[i].chardata_for_range[j].y1 = 0;

	n = 0;
	for (i=0; i < num_ranges; ++i)
		n += ranges[i].num_chars;

	rects = (stbrp_rect *) STBTT_malloc(sizeof(*rects) * n, spc->user_allocator_context);
	if (rects == NULL)
		return 0;

	info.userdata = spc->user_allocator_context;
	stbtt_InitFont(&info, fontdata, stbtt_GetFontOffsetForIndex(fontdata,font_index));

	n = stbtt_PackFontRangesGatherRects(spc, &info, ranges, num_ranges, rects);

	stbtt_PackFontRangesPackRects(spc, rects, n);

	return_value = stbtt_PackFontRangesRenderIntoRects(spc, &info, ranges, num_ranges, rects);

	STBTT_free(rects, spc->user_allocator_context);
	return return_value;
}

STBTT_DEF int stbtt_PackFontRange(stbtt_pack_context *spc, const unsigned char *fontdata, int font_index, float font_size,
								  int first_unicode_codepoint_in_range, int num_chars_in_range, stbtt_packedchar *chardata_for_range)
{
	stbtt_pack_range range;
	range.first_unicode_codepoint_in_range = first_unicode_codepoint_in_range;
	range.array_of_unicode_codepoints = NULL;
	range.num_chars                   = num_chars_in_range;
	range.chardata_for_range          = chardata_for_range;
	range.font_size                   = font_size;
	return stbtt_PackFontRanges(spc, fontdata, font_index, &range, 1);
}

STBTT_DEF void stbtt_GetScaledFontVMetrics(const unsigned char *fontdata, int index, float size, float *ascent, float *descent, float *lineGap)
{
	int i_ascent, i_descent, i_lineGap;
	float scale;
	stbtt_fontinfo info;
	stbtt_InitFont(&info, fontdata, stbtt_GetFontOffsetForIndex(fontdata, index));
	scale = size > 0 ? stbtt_ScaleForPixelHeight(&info, size) : stbtt_ScaleForMappingEmToPixels(&info, -size);
	stbtt_GetFontVMetrics(&info, &i_ascent, &i_descent, &i_lineGap);
	*ascent  = (float) i_ascent  * scale;
	*descent = (float) i_descent * scale;
	*lineGap = (float) i_lineGap * scale;
}

STBTT_DEF void stbtt_GetPackedQuad(const stbtt_packedchar *chardata, int pw, int ph, int char_index, float *xpos, float *ypos, stbtt_aligned_quad *q, int align_to_integer)
{
	float ipw = 1.0f / pw, iph = 1.0f / ph;
	const stbtt_packedchar *b = chardata + char_index;

	if (align_to_integer) {
		float x = (float) STBTT_ifloor((*xpos + b->xoff) + 0.5f);
		float y = (float) STBTT_ifloor((*ypos + b->yoff) + 0.5f);
		q->x0 = x;
		q->y0 = y;
		q->x1 = x + b->xoff2 - b->xoff;
		q->y1 = y + b->yoff2 - b->yoff;
	} else {
		q->x0 = *xpos + b->xoff;
		q->y0 = *ypos + b->yoff;
		q->x1 = *xpos + b->xoff2;
		q->y1 = *ypos + b->yoff2;
	}

	q->s0 = b->x0 * ipw;
	q->t0 = b->y0 * iph;
	q->s1 = b->x1 * ipw;
	q->t1 = b->y1 * iph;

	*xpos += b->xadvance;
}

//////////////////////////////////////////////////////////////////////////////
//
// sdf computation
//

#define STBTT_min(a,b)  ((a) < (b) ? (a) : (b))
#define STBTT_max(a,b)  ((a) < (b) ? (b) : (a))

static int stbtt__ray_intersect_bezier(float orig[2], float ray[2], float q0[2], float q1[2], float q2[2], float hits[2][2])
{
	float q0perp = q0[1]*ray[0] - q0[0]*ray[1];
	float q1perp = q1[1]*ray[0] - q1[0]*ray[1];
	float q2perp = q2[1]*ray[0] - q2[0]*ray[1];
	float roperp = orig[1]*ray[0] - orig[0]*ray[1];

	float a = q0perp - 2*q1perp + q2perp;
	float b = q1perp - q0perp;
	float c = q0perp - roperp;

	float s0 = 0., s1 = 0.;
	int num_s = 0;

	if (a != 0.0) {
		float discr = b*b - a*c;
		if (discr > 0.0) {
			float rcpna = -1 / a;
			float d = (float) STBTT_sqrt(discr);
			s0 = (b+d) * rcpna;
			s1 = (b-d) * rcpna;
			if (s0 >= 0.0 && s0 <= 1.0)
				num_s = 1;
			if (d > 0.0 && s1 >= 0.0 && s1 <= 1.0) {
				if (num_s == 0) s0 = s1;
				++num_s;
			}
		}
	} else {
		// 2*b*s + c = 0
		// s = -c / (2*b)
		s0 = c / (-2 * b);
		if (s0 >= 0.0 && s0 <= 1.0)
			num_s = 1;
	}

	if (num_s == 0)
		return 0;
	else {
		float rcp_len2 = 1 / (ray[0]*ray[0] + ray[1]*ray[1]);
		float rayn_x = ray[0] * rcp_len2, rayn_y = ray[1] * rcp_len2;

		float q0d =   q0[0]*rayn_x +   q0[1]*rayn_y;
		float q1d =   q1[0]*rayn_x +   q1[1]*rayn_y;
		float q2d =   q2[0]*rayn_x +   q2[1]*rayn_y;
		float rod = orig[0]*rayn_x + orig[1]*rayn_y;

		float q10d = q1d - q0d;
		float q20d = q2d - q0d;
		float q0rd = q0d - rod;

		hits[0][0] = q0rd + s0*(2.0f - 2.0f*s0)*q10d + s0*s0*q20d;
		hits[0][1] = a*s0+b;

		if (num_s > 1) {
			hits[1][0] = q0rd + s1*(2.0f - 2.0f*s1)*q10d + s1*s1*q20d;
			hits[1][1] = a*s1+b;
			return 2;
		} else {
			return 1;
		}
	}
}

static int equal(float *a, float *b)
{
	return (a[0] == b[0] && a[1] == b[1]);
}

static int stbtt__compute_crossings_x(float x, float y, int nverts, stbtt_vertex *verts)
{
	int i;
	float orig[2], ray[2] = { 1, 0 };
	float y_frac;
	int winding = 0;

	// make sure y never passes through a vertex of the shape
	y_frac = (float) STBTT_fmod(y, 1.0f);
	if (y_frac < 0.01f)
		y += 0.01f;
	else if (y_frac > 0.99f)
		y -= 0.01f;

	orig[0] = x;
	orig[1] = y;

	// test a ray from (-infinity,y) to (x,y)
	for (i=0; i < nverts; ++i) {
		if (verts[i].type == STBTT_vline) {
			int x0 = (int) verts[i-1].x, y0 = (int) verts[i-1].y;
			int x1 = (int) verts[i  ].x, y1 = (int) verts[i  ].y;
			if (y > STBTT_min(y0,y1) && y < STBTT_max(y0,y1) && x > STBTT_min(x0,x1)) {
				float x_inter = (y - y0) / (y1 - y0) * (x1-x0) + x0;
				if (x_inter < x)
					winding += (y0 < y1) ? 1 : -1;
			}
		}
		if (verts[i].type == STBTT_vcurve) {
			int x0 = (int) verts[i-1].x , y0 = (int) verts[i-1].y ;
			int x1 = (int) verts[i  ].cx, y1 = (int) verts[i  ].cy;
			int x2 = (int) verts[i  ].x , y2 = (int) verts[i  ].y ;
			int ax = STBTT_min(x0,STBTT_min(x1,x2)), ay = STBTT_min(y0,STBTT_min(y1,y2));
			int by = STBTT_max(y0,STBTT_max(y1,y2));
			if (y > ay && y < by && x > ax) {
				float q0[2],q1[2],q2[2];
				float hits[2][2];
				q0[0] = (float)x0;
				q0[1] = (float)y0;
				q1[0] = (float)x1;
				q1[1] = (float)y1;
				q2[0] = (float)x2;
				q2[1] = (float)y2;
				if (equal(q0,q1) || equal(q1,q2)) {
					x0 = (int)verts[i-1].x;
					y0 = (int)verts[i-1].y;
					x1 = (int)verts[i  ].x;
					y1 = (int)verts[i  ].y;
					if (y > STBTT_min(y0,y1) && y < STBTT_max(y0,y1) && x > STBTT_min(x0,x1)) {
						float x_inter = (y - y0) / (y1 - y0) * (x1-x0) + x0;
						if (x_inter < x)
							winding += (y0 < y1) ? 1 : -1;
					}
				} else {
					int num_hits = stbtt__ray_intersect_bezier(orig, ray, q0, q1, q2, hits);
					if (num_hits >= 1)
						if (hits[0][0] < 0)
							winding += (hits[0][1] < 0 ? -1 : 1);
					if (num_hits >= 2)
						if (hits[1][0] < 0)
							winding += (hits[1][1] < 0 ? -1 : 1);
				}
			}
		}
	}
	return winding;
}

static float stbtt__cuberoot( float x )
{
	if (x<0)
		return -(float) STBTT_pow(-x,1.0f/3.0f);
	else
		return  (float) STBTT_pow( x,1.0f/3.0f);
}

// x^3 + a*x^2 + b*x + c = 0
static int stbtt__solve_cubic(float a, float b, float c, float* r)
{
	float s = -a / 3;
	float p = b - a*a / 3;
	float q = a * (2*a*a - 9*b) / 27 + c;
	float p3 = p*p*p;
	float d = q*q + 4*p3 / 27;
	if (d >= 0) {
		float z = (float) STBTT_sqrt(d);
		float u = (-q + z) / 2;
		float v = (-q - z) / 2;
		u = stbtt__cuberoot(u);
		v = stbtt__cuberoot(v);
		r[0] = s + u + v;
		return 1;
	} else {
		float u = (float) STBTT_sqrt(-p/3);
		float v = (float) STBTT_acos(-STBTT_sqrt(-27/p3) * q / 2) / 3; // p3 must be negative, since d is negative
		float m = (float) STBTT_cos(v);
		float n = (float) STBTT_cos(v-3.141592/2)*1.732050808f;
		r[0] = s + u * 2 * m;
		r[1] = s - u * (m + n);
		r[2] = s - u * (m - n);

		//STBTT_assert( STBTT_fabs(((r[0]+a)*r[0]+b)*r[0]+c) < 0.05f);  // these asserts may not be safe at all scales, though they're in bezier t parameter units so maybe?
		//STBTT_assert( STBTT_fabs(((r[1]+a)*r[1]+b)*r[1]+c) < 0.05f);
		//STBTT_assert( STBTT_fabs(((r[2]+a)*r[2]+b)*r[2]+c) < 0.05f);
		return 3;
	}
}

STBTT_DEF unsigned char * stbtt_GetGlyphSDF(const stbtt_fontinfo *info, float scale, int glyph, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff)
{
	float scale_x = scale, scale_y = scale;
	int ix0,iy0,ix1,iy1;
	int w,h;
	unsigned char *data;

	if (scale == 0) return NULL;

	stbtt_GetGlyphBitmapBoxSubpixel(info, glyph, scale, scale, 0.0f,0.0f, &ix0,&iy0,&ix1,&iy1);

	// if empty, return NULL
	if (ix0 == ix1 || iy0 == iy1)
		return NULL;

	ix0 -= padding;
	iy0 -= padding;
	ix1 += padding;
	iy1 += padding;

	w = (ix1 - ix0);
	h = (iy1 - iy0);

	if (width ) *width  = w;
	if (height) *height = h;
	if (xoff  ) *xoff   = ix0;
	if (yoff  ) *yoff   = iy0;

	// invert for y-downwards bitmaps
	scale_y = -scale_y;

	{
		// distance from singular values (in the same units as the pixel grid)
		const float eps = 1./1024, eps2 = eps*eps;
		int x,y,i,j;
		float *precompute;
		stbtt_vertex *verts;
		int num_verts = stbtt_GetGlyphShape(info, glyph, &verts);
		data = (unsigned char *) STBTT_malloc(w * h, info->userdata);
		precompute = (float *) STBTT_malloc(num_verts * sizeof(float), info->userdata);

		for (i=0,j=num_verts-1; i < num_verts; j=i++) {
			if (verts[i].type == STBTT_vline) {
				float x0 = verts[i].x*scale_x, y0 = verts[i].y*scale_y;
				float x1 = verts[j].x*scale_x, y1 = verts[j].y*scale_y;
				float dist = (float) STBTT_sqrt((x1-x0)*(x1-x0) + (y1-y0)*(y1-y0));
				precompute[i] = (dist < eps) ? 0.0f : 1.0f / dist;
			} else if (verts[i].type == STBTT_vcurve) {
				float x2 = verts[j].x *scale_x, y2 = verts[j].y *scale_y;
				float x1 = verts[i].cx*scale_x, y1 = verts[i].cy*scale_y;
				float x0 = verts[i].x *scale_x, y0 = verts[i].y *scale_y;
				float bx = x0 - 2*x1 + x2, by = y0 - 2*y1 + y2;
				float len2 = bx*bx + by*by;
				if (len2 >= eps2)
					precompute[i] = 1.0f / len2;
				else
					precompute[i] = 0.0f;
			} else
				precompute[i] = 0.0f;
		}

		for (y=iy0; y < iy1; ++y) {
			for (x=ix0; x < ix1; ++x) {
				float val;
				float min_dist = 999999.0f;
				float sx = (float) x + 0.5f;
				float sy = (float) y + 0.5f;
				float x_gspace = (sx / scale_x);
				float y_gspace = (sy / scale_y);

				int winding = stbtt__compute_crossings_x(x_gspace, y_gspace, num_verts, verts); // @OPTIMIZE: this could just be a rasterization, but needs to be line vs. non-tesselated curves so a new path

				for (i=0; i < num_verts; ++i) {
					float x0 = verts[i].x*scale_x, y0 = verts[i].y*scale_y;

					if (verts[i].type == STBTT_vline && precompute[i] != 0.0f) {
						float x1 = verts[i-1].x*scale_x, y1 = verts[i-1].y*scale_y;

						float dist,dist2 = (x0-sx)*(x0-sx) + (y0-sy)*(y0-sy);
						if (dist2 < min_dist*min_dist)
							min_dist = (float) STBTT_sqrt(dist2);

						// coarse culling against bbox
						//if (sx > STBTT_min(x0,x1)-min_dist && sx < STBTT_max(x0,x1)+min_dist &&
						//    sy > STBTT_min(y0,y1)-min_dist && sy < STBTT_max(y0,y1)+min_dist)
						dist = (float) STBTT_fabs((x1-x0)*(y0-sy) - (y1-y0)*(x0-sx)) * precompute[i];
						STBTT_assert(i != 0);
						if (dist < min_dist) {
							// check position along line
							// x' = x0 + t*(x1-x0), y' = y0 + t*(y1-y0)
							// minimize (x'-sx)*(x'-sx)+(y'-sy)*(y'-sy)
							float dx = x1-x0, dy = y1-y0;
							float px = x0-sx, py = y0-sy;
							// minimize (px+t*dx)^2 + (py+t*dy)^2 = px*px + 2*px*dx*t + t^2*dx*dx + py*py + 2*py*dy*t + t^2*dy*dy
							// derivative: 2*px*dx + 2*py*dy + (2*dx*dx+2*dy*dy)*t, set to 0 and solve
							float t = -(px*dx + py*dy) / (dx*dx + dy*dy);
							if (t >= 0.0f && t <= 1.0f)
								min_dist = dist;
						}
					} else if (verts[i].type == STBTT_vcurve) {
						float x2 = verts[i-1].x *scale_x, y2 = verts[i-1].y *scale_y;
						float x1 = verts[i  ].cx*scale_x, y1 = verts[i  ].cy*scale_y;
						float box_x0 = STBTT_min(STBTT_min(x0,x1),x2);
						float box_y0 = STBTT_min(STBTT_min(y0,y1),y2);
						float box_x1 = STBTT_max(STBTT_max(x0,x1),x2);
						float box_y1 = STBTT_max(STBTT_max(y0,y1),y2);
						// coarse culling against bbox to avoid computing cubic unnecessarily
						if (sx > box_x0-min_dist && sx < box_x1+min_dist && sy > box_y0-min_dist && sy < box_y1+min_dist) {
							int num=0;
							float ax = x1-x0, ay = y1-y0;
							float bx = x0 - 2*x1 + x2, by = y0 - 2*y1 + y2;
							float mx = x0 - sx, my = y0 - sy;
							float res[3] = {0.f,0.f,0.f};
							float px,py,t,it,dist2;
							float a_inv = precompute[i];
							if (a_inv == 0.0) { // if a_inv is 0, it's 2nd degree so use quadratic formula
								float a = 3*(ax*bx + ay*by);
								float b = 2*(ax*ax + ay*ay) + (mx*bx+my*by);
								float c = mx*ax+my*ay;
								if (STBTT_fabs(a) < eps2) { // if a is 0, it's linear
									if (STBTT_fabs(b) >= eps2) {
										res[num++] = -c/b;
									}
								} else {
									float discriminant = b*b - 4*a*c;
									if (discriminant < 0)
										num = 0;
									else {
										float root = (float) STBTT_sqrt(discriminant);
										res[0] = (-b - root)/(2*a);
										res[1] = (-b + root)/(2*a);
										num = 2; // don't bother distinguishing 1-solution case, as code below will still work
									}
								}
							} else {
								float b = 3*(ax*bx + ay*by) * a_inv; // could precompute this as it doesn't depend on sample point
								float c = (2*(ax*ax + ay*ay) + (mx*bx+my*by)) * a_inv;
								float d = (mx*ax+my*ay) * a_inv;
								num = stbtt__solve_cubic(b, c, d, res);
							}
							dist2 = (x0-sx)*(x0-sx) + (y0-sy)*(y0-sy);
							if (dist2 < min_dist*min_dist)
								min_dist = (float) STBTT_sqrt(dist2);

							if (num >= 1 && res[0] >= 0.0f && res[0] <= 1.0f) {
								t = res[0], it = 1.0f - t;
								px = it*it*x0 + 2*t*it*x1 + t*t*x2;
								py = it*it*y0 + 2*t*it*y1 + t*t*y2;
								dist2 = (px-sx)*(px-sx) + (py-sy)*(py-sy);
								if (dist2 < min_dist * min_dist)
									min_dist = (float) STBTT_sqrt(dist2);
							}
							if (num >= 2 && res[1] >= 0.0f && res[1] <= 1.0f) {
								t = res[1], it = 1.0f - t;
								px = it*it*x0 + 2*t*it*x1 + t*t*x2;
								py = it*it*y0 + 2*t*it*y1 + t*t*y2;
								dist2 = (px-sx)*(px-sx) + (py-sy)*(py-sy);
								if (dist2 < min_dist * min_dist)
									min_dist = (float) STBTT_sqrt(dist2);
							}
							if (num >= 3 && res[2] >= 0.0f && res[2] <= 1.0f) {
								t = res[2], it = 1.0f - t;
								px = it*it*x0 + 2*t*it*x1 + t*t*x2;
								py = it*it*y0 + 2*t*it*y1 + t*t*y2;
								dist2 = (px-sx)*(px-sx) + (py-sy)*(py-sy);
								if (dist2 < min_dist * min_dist)
									min_dist = (float) STBTT_sqrt(dist2);
							}
						}
					}
				}
				if (winding == 0)
					min_dist = -min_dist;  // if outside the shape, value is negative
				val = onedge_value + pixel_dist_scale * min_dist;
				if (val < 0)
					val = 0;
				else if (val > 255)
					val = 255;
				data[(y-iy0)*w+(x-ix0)] = (unsigned char) val;
			}
		}
		STBTT_free(precompute, info->userdata);
		STBTT_free(verts, info->userdata);
	}
	return data;
}

STBTT_DEF unsigned char * stbtt_GetCodepointSDF(const stbtt_fontinfo *info, float scale, int codepoint, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff)
{
	return stbtt_GetGlyphSDF(info, scale, stbtt_FindGlyphIndex(info, codepoint), padding, onedge_value, pixel_dist_scale, width, height, xoff, yoff);
}

STBTT_DEF void stbtt_FreeSDF(unsigned char *bitmap, void *userdata)
{
	STBTT_free(bitmap, userdata);
}

//////////////////////////////////////////////////////////////////////////////
//
// font name matching -- recommended not to use this
//

// check if a utf8 string contains a prefix which is the utf16 string; if so return length of matching utf8 string
static stbtt_int32 stbtt__CompareUTF8toUTF16_bigendian_prefix(stbtt_uint8 *s1, stbtt_int32 len1, stbtt_uint8 *s2, stbtt_int32 len2)
{
	stbtt_int32 i=0;

	// convert utf16 to utf8 and compare the results while converting
	while (len2) {
		stbtt_uint16 ch = s2[0]*256 + s2[1];
		if (ch < 0x80) {
			if (i >= len1) return -1;
			if (s1[i++] != ch) return -1;
		} else if (ch < 0x800) {
			if (i+1 >= len1) return -1;
			if (s1[i++] != 0xc0 + (ch >> 6)) return -1;
			if (s1[i++] != 0x80 + (ch & 0x3f)) return -1;
		} else if (ch >= 0xd800 && ch < 0xdc00) {
			stbtt_uint32 c;
			stbtt_uint16 ch2 = s2[2]*256 + s2[3];
			if (i+3 >= len1) return -1;
			c = ((ch - 0xd800) << 10) + (ch2 - 0xdc00) + 0x10000;
			if (s1[i++] != 0xf0 + (c >> 18)) return -1;
			if (s1[i++] != 0x80 + ((c >> 12) & 0x3f)) return -1;
			if (s1[i++] != 0x80 + ((c >>  6) & 0x3f)) return -1;
			if (s1[i++] != 0x80 + ((c      ) & 0x3f)) return -1;
			s2 += 2; // plus another 2 below
			len2 -= 2;
		} else if (ch >= 0xdc00 && ch < 0xe000) {
			return -1;
		} else {
			if (i+2 >= len1) return -1;
			if (s1[i++] != 0xe0 + (ch >> 12)) return -1;
			if (s1[i++] != 0x80 + ((ch >> 6) & 0x3f)) return -1;
			if (s1[i++] != 0x80 + ((ch     ) & 0x3f)) return -1;
		}
		s2 += 2;
		len2 -= 2;
	}
	return i;
}

static int stbtt_CompareUTF8toUTF16_bigendian_internal(char *s1, int len1, char *s2, int len2)
{
	return len1 == stbtt__CompareUTF8toUTF16_bigendian_prefix((stbtt_uint8*) s1, len1, (stbtt_uint8*) s2, len2);
}

// returns results in whatever encoding you request... but note that 2-byte encodings
// will be BIG-ENDIAN... use stbtt_CompareUTF8toUTF16_bigendian() to compare
STBTT_DEF const char *stbtt_GetFontNameString(const stbtt_fontinfo *font, int *length, int platformID, int encodingID, int languageID, int nameID)
{
	stbtt_int32 i,count,stringOffset;
	stbtt_uint8 *fc = font->data;
	stbtt_uint32 offset = font->fontstart;
	stbtt_uint32 nm = stbtt__find_table(fc, offset, "name");
	if (!nm) return NULL;

	count = ttUSHORT(fc+nm+2);
	stringOffset = nm + ttUSHORT(fc+nm+4);
	for (i=0; i < count; ++i) {
		stbtt_uint32 loc = nm + 6 + 12 * i;
		if (platformID == ttUSHORT(fc+loc+0) && encodingID == ttUSHORT(fc+loc+2)
			&& languageID == ttUSHORT(fc+loc+4) && nameID == ttUSHORT(fc+loc+6)) {
			*length = ttUSHORT(fc+loc+8);
			return (const char *) (fc+stringOffset+ttUSHORT(fc+loc+10));
		}
	}
	return NULL;
}

static int stbtt__matchpair(stbtt_uint8 *fc, stbtt_uint32 nm, stbtt_uint8 *name, stbtt_int32 nlen, stbtt_int32 target_id, stbtt_int32 next_id)
{
	stbtt_int32 i;
	stbtt_int32 count = ttUSHORT(fc+nm+2);
	stbtt_int32 stringOffset = nm + ttUSHORT(fc+nm+4);

	for (i=0; i < count; ++i) {
		stbtt_uint32 loc = nm + 6 + 12 * i;
		stbtt_int32 id = ttUSHORT(fc+loc+6);
		if (id == target_id) {
			// find the encoding
			stbtt_int32 platform = ttUSHORT(fc+loc+0), encoding = ttUSHORT(fc+loc+2), language = ttUSHORT(fc+loc+4);

			// is this a Unicode encoding?
			if (platform == 0 || (platform == 3 && encoding == 1) || (platform == 3 && encoding == 10)) {
				stbtt_int32 slen = ttUSHORT(fc+loc+8);
				stbtt_int32 off = ttUSHORT(fc+loc+10);

				// check if there's a prefix match
				stbtt_int32 matchlen = stbtt__CompareUTF8toUTF16_bigendian_prefix(name, nlen, fc+stringOffset+off,slen);
				if (matchlen >= 0) {
					// check for target_id+1 immediately following, with same encoding & language
					if (i+1 < count && ttUSHORT(fc+loc+12+6) == next_id && ttUSHORT(fc+loc+12) == platform && ttUSHORT(fc+loc+12+2) == encoding && ttUSHORT(fc+loc+12+4) == language) {
						slen = ttUSHORT(fc+loc+12+8);
						off = ttUSHORT(fc+loc+12+10);
						if (slen == 0) {
							if (matchlen == nlen)
								return 1;
						} else if (matchlen < nlen && name[matchlen] == ' ') {
							++matchlen;
							if (stbtt_CompareUTF8toUTF16_bigendian_internal((char*) (name+matchlen), nlen-matchlen, (char*)(fc+stringOffset+off),slen))
								return 1;
						}
					} else {
						// if nothing immediately following
						if (matchlen == nlen)
							return 1;
					}
				}
			}

			// @TODO handle other encodings
		}
	}
	return 0;
}

static int stbtt__matches(stbtt_uint8 *fc, stbtt_uint32 offset, stbtt_uint8 *name, stbtt_int32 flags)
{
	stbtt_int32 nlen = (stbtt_int32) STBTT_strlen((char *) name);
	stbtt_uint32 nm,hd;
	if (!stbtt__isfont(fc+offset)) return 0;

	// check italics/bold/underline flags in macStyle...
	if (flags) {
		hd = stbtt__find_table(fc, offset, "head");
		if ((ttUSHORT(fc+hd+44) & 7) != (flags & 7)) return 0;
	}

	nm = stbtt__find_table(fc, offset, "name");
	if (!nm) return 0;

	if (flags) {
		// if we checked the macStyle flags, then just check the family and ignore the subfamily
		if (stbtt__matchpair(fc, nm, name, nlen, 16, -1))  return 1;
		if (stbtt__matchpair(fc, nm, name, nlen,  1, -1))  return 1;
		if (stbtt__matchpair(fc, nm, name, nlen,  3, -1))  return 1;
	} else {
		if (stbtt__matchpair(fc, nm, name, nlen, 16, 17))  return 1;
		if (stbtt__matchpair(fc, nm, name, nlen,  1,  2))  return 1;
		if (stbtt__matchpair(fc, nm, name, nlen,  3, -1))  return 1;
	}

	return 0;
}

static int stbtt_FindMatchingFont_internal(unsigned char *font_collection, char *name_utf8, stbtt_int32 flags)
{
	stbtt_int32 i;
	for (i=0;;++i) {
		stbtt_int32 off = stbtt_GetFontOffsetForIndex(font_collection, i);
		if (off < 0) return off;
		if (stbtt__matches((stbtt_uint8 *) font_collection, off, (stbtt_uint8*) name_utf8, flags))
			return off;
	}
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

STBTT_DEF int stbtt_BakeFontBitmap(const unsigned char *data, int offset,
								   float pixel_height, unsigned char *pixels, int pw, int ph,
								   int first_char, int num_chars, stbtt_bakedchar *chardata)
{
	return stbtt_BakeFontBitmap_internal((unsigned char *) data, offset, pixel_height, pixels, pw, ph, first_char, num_chars, chardata);
}

STBTT_DEF int stbtt_GetFontOffsetForIndex(const unsigned char *data, int index)
{
	return stbtt_GetFontOffsetForIndex_internal((unsigned char *) data, index);
}

STBTT_DEF int stbtt_GetNumberOfFonts(const unsigned char *data)
{
	return stbtt_GetNumberOfFonts_internal((unsigned char *) data);
}

STBTT_DEF int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int offset)
{
	return stbtt_InitFont_internal(info, (unsigned char *) data, offset);
}

STBTT_DEF int stbtt_FindMatchingFont(const unsigned char *fontdata, const char *name, int flags)
{
	return stbtt_FindMatchingFont_internal((unsigned char *) fontdata, (char *) name, flags);
}

STBTT_DEF int stbtt_CompareUTF8toUTF16_bigendian(const char *s1, int len1, const char *s2, int len2)
{
	return stbtt_CompareUTF8toUTF16_bigendian_internal((char *) s1, len1, (char *) s2, len2);
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif // STB_TRUETYPE_IMPLEMENTATION


// FULL VERSION HISTORY
//
//   1.25 (2021-07-11) many fixes
//   1.24 (2020-02-05) fix warning
//   1.23 (2020-02-02) query SVG data for glyphs; query whole kerning table (but only kern not GPOS)
//   1.22 (2019-08-11) minimize missing-glyph duplication; fix kerning if both 'GPOS' and 'kern' are defined
//   1.21 (2019-02-25) fix warning
//   1.20 (2019-02-07) PackFontRange skips missing codepoints; GetScaleFontVMetrics()
//   1.19 (2018-02-11) OpenType GPOS kerning (horizontal only), STBTT_fmod
//   1.18 (2018-01-29) add missing function
//   1.17 (2017-07-23) make more arguments const; doc fix
//   1.16 (2017-07-12) SDF support
//   1.15 (2017-03-03) make more arguments const
//   1.14 (2017-01-16) num-fonts-in-TTC function
//   1.13 (2017-01-02) support OpenType fonts, certain Apple fonts
//   1.12 (2016-10-25) suppress warnings about casting away const with -Wcast-qual
//   1.11 (2016-04-02) fix unused-variable warning
//   1.10 (2016-04-02) allow user-defined fabs() replacement
//                     fix memory leak if fontsize=0.0
//                     fix warning from duplicate typedef
//   1.09 (2016-01-16) warning fix; avoid crash on outofmem; use alloc userdata for PackFontRanges
//   1.08 (2015-09-13) document stbtt_Rasterize(); fixes for vertical & horizontal edges
//   1.07 (2015-08-01) allow PackFontRanges to accept arrays of sparse codepoints;
//                     allow PackFontRanges to pack and render in separate phases;
//                     fix stbtt_GetFontOFfsetForIndex (never worked for non-0 input?);
//                     fixed an assert() bug in the new rasterizer
//                     replace assert() with STBTT_assert() in new rasterizer
//   1.06 (2015-07-14) performance improvements (~35% faster on x86 and x64 on test machine)
//                     also more precise AA rasterizer, except if shapes overlap
//                     remove need for STBTT_sort
//   1.05 (2015-04-15) fix misplaced definitions for STBTT_STATIC
//   1.04 (2015-04-15) typo in example
//   1.03 (2015-04-12) STBTT_STATIC, fix memory leak in new packing, various fixes
//   1.02 (2014-12-10) fix various warnings & compile issues w/ stb_rect_pack, C++
//   1.01 (2014-12-08) fix subpixel position when oversampling to exactly match
//                        non-oversampled; STBTT_POINT_SIZE for packed case only
//   1.00 (2014-12-06) add new PackBegin etc. API, w/ support for oversampling
//   0.99 (2014-09-18) fix multiple bugs with subpixel rendering (ryg)
//   0.9  (2014-08-07) support certain mac/iOS fonts without an MS platformID
//   0.8b (2014-07-07) fix a warning
//   0.8  (2014-05-25) fix a few more warnings
//   0.7  (2013-09-25) bugfix: subpixel glyph bug fixed in 0.5 had come back
//   0.6c (2012-07-24) improve documentation
//   0.6b (2012-07-20) fix a few more warnings
//   0.6  (2012-07-17) fix warnings; added stbtt_ScaleForMappingEmToPixels,
//                        stbtt_GetFontBoundingBox, stbtt_IsGlyphEmpty
//   0.5  (2011-12-09) bugfixes:
//                        subpixel glyph renderer computed wrong bounding box
//                        first vertex of shape can be off-curve (FreeSans)
//   0.4b (2011-12-03) fixed an error in the font baking example
//   0.4  (2011-12-01) kerning, subpixel rendering (tor)
//                    bugfixes for:
//                        codepoint-to-glyph conversion using table fmt=12
//                        codepoint-to-glyph conversion using table fmt=4
//                        stbtt_GetBakedQuad with non-square texture (Zer)
//                    updated Hello World! sample to use kerning and subpixel
//                    fixed some warnings
//   0.3  (2009-06-24) cmap fmt=12, compound shapes (MM)
//                    userdata, malloc-from-userdata, non-zero fill (stb)
//   0.2  (2009-03-11) Fix unsigned/signed char warnings
//   0.1  (2009-03-09) First public release
//

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/

// End stb_truetype_fonts

// msdf_fonts

/* msdf
Handles multi-channel signed distance field bitmap
generation from given ttf (stb_truetype.h) font.
https://github.com/exezin/msdf-c
Depends on stb_truetype.h to load the ttf file.
This is in an unstable state, ymmv.
Based on the C++ implementation by Viktor Chlumský.
https://github.com/Chlumsky/msdfgen
*/

#ifndef MSDF_H
#define MSDF_H

#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
	
typedef struct {
	int glyphIdx;
	int left_bearing;
	int advance;
	int ix0, ix1;
	int iy0, iy1;
} ex_metrics_t;

typedef struct msdf_result_t {
	int glyphIdx;
	int left_bearing;
	int advance;
	float *rgba;
	int width;
	int height;
	int yOffset;
} msdf_result_t;

typedef struct msdf_allocation_context_t {
	void* (*alloc)(size_t size, void* ctx);
	// free is optional and will not be called if it is null (useful for area allocators that free everything at once)
	void (*free)(void* ptr, void* ctx);
	void* ctx;
} msdf_allocation_context_t;

/*
Generates a bitmap from the specified glyph index of a stbtt font
Returned result is 1 for success or 0 in case of an error
*/
int msdf_genGlyph(msdf_result_t* result, stbtt_fontinfo *font, int stbttGlyphIndex, uint32_t borderWidth, float scale, float range, msdf_allocation_context_t* alloc);

#ifdef __cplusplus
}
#endif

#if defined(MSDF_IMPLEMENTATION) || defined(ZEST_MSDF_IMPLEMENTATION)
// pixel at (x, y) in bitmap (arr)
#define msdf_pixelAt(x, y, w, arr) (MSDF_STRUCT_LITERAL(msdf_Vec4, arr[(4 * (((y)*w) + x))], arr[(4 * (((y)*w) + x)) + 1], arr[(4 * (((y)*w) + x)) + 2], arr[(4 * (((y)*w) + x)) + 3]))

#define msdf_max(x, y) (((x) > (y)) ? (x) : (y))
#define msdf_min(x, y) (((x) < (y)) ? (x) : (y))

#define MSDF_INF -1e24
#define MSDF_EDGE_THRESHOLD 0.02f

#ifndef MSDF_PI
#define MSDF_PI 3.14159265358979323846f
#endif

#ifdef __cplusplus
#define MSDF_STRUCT_LITERAL(type, ...) type { __VA_ARGS__ }
#else
#define MSDF_STRUCT_LITERAL(type, ...) (type) { __VA_ARGS__ }
#endif

typedef float msdf_Vec2[2];
typedef float msdf_Vec4[4];

typedef struct
{
    float dist;
    float d;
} msdf_signedDistance;

// the possible types:
// STBTT_vmove  = start of a contour
// STBTT_vline  = linear segment
// STBTT_vcurve = quadratic segment
// STBTT_vcubic = cubic segment
typedef struct
{
    int color;
    msdf_Vec2 p[4];
    int type;
} msdf_EdgeSegment;

// defines what color channel an edge belongs to
typedef enum
{
    msdf_edgeColor_black = 0,
    msdf_edgeColor_red = 1,
    msdf_edgeColor_green = 2,
    msdf_edgeColor_yellow = 3,
    msdf_edgeColor_blue = 4,
    msdf_edgeColor_magenta = 5,
    msdf_edgeColor_cyan = 6,
    msdf_edgeColor_white = 7
} msdf_edgeColor;

static float msdf_median(float a, float b, float c)
{
    return msdf_max(msdf_min(a, b), msdf_min(msdf_max(a, b), c));
}

static int msdf_nonZeroSign(float n)
{
    return 2 * (n > 0) - 1;
}

static float msdf_cross(msdf_Vec2 a, msdf_Vec2 b)
{
    return a[0] * b[1] - a[1] * b[0];
}

static void msdf_v2Scale(msdf_Vec2 r, msdf_Vec2 const v, float const s)
{
    int i;
    for (i = 0; i < 2; ++i)
        r[i] = v[i] * s;
}

static float msdf_v2MulInner(msdf_Vec2 const a, msdf_Vec2 const b)
{
    float p = 0.;
    int i;
    for (i = 0; i < 2; ++i)
        p += b[i] * a[i];
    return p;
}

static float msdf_v2Leng(msdf_Vec2 const v)
{
    return sqrtf(msdf_v2MulInner(v, v));
}

static void msdf_v2Norm(msdf_Vec2 r, msdf_Vec2 const v)
{
    float k = 1.f / msdf_v2Leng(v);
    msdf_v2Scale(r, v, k);
}

static void msdf_v2Sub(msdf_Vec2 r, msdf_Vec2 const a, msdf_Vec2 const b)
{
    int i;
    for (i = 0; i < 2; ++i)
        r[i] = a[i] - b[i];
}

int msdf_solveQuadratic(float x[2], float a, float b, float c)
{
    if (fabsf(a) < 1e-14)
    {
        if (fabsf(b) < 1e-14)
        {
            if (c == 0)
                return -1;
            return 0;
        }
        x[0] = -c / b;
        return 1;
    }

    float dscr = b * b - 4 * a * c;
    if (dscr > 0)
    {
        dscr = sqrtf(dscr);
        x[0] = (-b + dscr) / (2 * a);
        x[1] = (-b - dscr) / (2 * a);
        return 2;
    }
    else if (dscr == 0)
    {
        x[0] = -b / (2 * a);
        return 1;
    }
    else
    {
        return 0;
    }
}

int msdf_solveCubicNormed(float *x, float a, float b, float c)
{
    float a2 = a * a;
    float q = (a2 - 3 * b) / 9;
    float r = (a * (2 * a2 - 9 * b) + 27 * c) / 54;
    float r2 = r * r;
    float q3 = q * q * q;
    float A, B;
    if (r2 < q3)
    {
        float t = r / sqrtf(q3);
        if (t < -1)
            t = -1;
        if (t > 1)
            t = 1;
        t = acosf(t);
        a /= 3;
        q = -2 * sqrtf(q);
        x[0] = q * cosf(t / 3) - a;
        x[1] = q * cosf((t + 2 * MSDF_PI) / 3.f) - a;
        x[2] = q * cosf((t - 2 * MSDF_PI) / 3.f) - a;
        return 3;
    }
    else
    {
        A = -powf(fabsf(r) + sqrtf(r2 - q3), 1 / 3.f);
        if (r < 0)
            A = -A;
        B = A == 0 ? 0 : q / A;
        a /= 3;
        x[0] = (A + B) - a;
        x[1] = -0.5f * (A + B) - a;
        x[2] = 0.5f * sqrtf(3.) * (A - B);
        if (fabsf(x[2]) < 1e-14)
            return 2;
        return 1;
    }
}

int msdf_solveCubic(float x[3], float a, float b, float c, float d)
{
    if (fabsf(a) < 1e-14)
        return msdf_solveQuadratic(x, b, c, d);

    return msdf_solveCubicNormed(x, b / a, c / a, d / a);
}

void msdf_getOrtho(msdf_Vec2 r, msdf_Vec2 const v, int polarity, int allow_zero)
{
    float len = msdf_v2Leng(v);

    if (len == 0)
    {
        if (polarity)
        {
            r[0] = 0;
            r[1] = !allow_zero;
        }
        else
        {
            r[0] = 0.f;
            r[1] = (float)-!allow_zero;
        }
        return;
    }

    if (polarity)
    {
        r[0] = -v[1] / len;
        r[1] = v[0] / len;
    }
    else
    {
        r[0] = v[1] / len;
        r[1] = -v[0] / len;
    }
}

int msdf_pixelClash(const msdf_Vec4 a, const msdf_Vec4 b, float threshold)
{
    int aIn = (a[0] > .5f) + (a[1] > .5f) + (a[2] > .5f) >= 2;
    int bIn = (b[0] > .5f) + (b[1] > .5f) + (b[2] > .5f) >= 2;
    if (aIn != bIn)
        return 0;
    if ((a[0] > .5f && a[1] > .5f && a[2] > .5f) || (a[0] < .5f && a[1] < .5f && a[2] < .5f) || (b[0] > .5f && b[1] > .5f && b[2] > .5f) || (b[0] < .5f && b[1] < .5f && b[2] < .5f))
        return 0;
    float aa, ab, ba, bb, ac, bc;
    if ((a[0] > .5f) != (b[0] > .5f) && (a[0] < .5f) != (b[0] < .5f))
    {
        aa = a[0], ba = b[0];
        if ((a[1] > .5f) != (b[1] > .5f) && (a[1] < .5f) != (b[1] < .5f))
        {
            ab = a[1], bb = b[1];
            ac = a[2], bc = b[2];
        }
        else if ((a[2] > .5f) != (b[2] > .5f) && (a[2] < .5f) != (b[2] < .5f))
        {
            ab = a[2], bb = b[2];
            ac = a[1], bc = b[1];
        }
        else
        {
            return 0;
        }
    }
    else if ((a[1] > .5f) != (b[1] > .5f) && (a[1] < .5f) != (b[1] < .5f) && (a[2] > .5f) != (b[2] > .5f) && (a[2] < .5f) != (b[2] < .5f))
    {
        aa = a[1], ba = b[1];
        ab = a[2], bb = b[2];
        ac = a[0], bc = b[0];
    }
    else
    {
        return 0;
    }
    return (fabsf(aa - ba) >= threshold) && (fabsf(ab - bb) >= threshold) && fabsf(ac - .5f) >= fabsf(bc - .5f);
}

void msdf_mix(msdf_Vec2 r, msdf_Vec2 a, msdf_Vec2 b, float weight)
{
    r[0] = (1 - weight) * a[0] + weight * b[0];
    r[1] = (1 - weight) * a[1] + weight * b[1];
}

void msdf_linearDirection(msdf_Vec2 r, msdf_EdgeSegment *e, float param)
{
    r[0] = e->p[1][0] - e->p[0][0];
    r[1] = e->p[1][1] - e->p[0][1];
}

void msdf_quadraticDirection(msdf_Vec2 r, msdf_EdgeSegment *e, float param)
{
    msdf_Vec2 a, b;
    msdf_v2Sub(a, e->p[1], e->p[0]);
    msdf_v2Sub(b, e->p[2], e->p[1]);
    msdf_mix(r, a, b, param);
}

void msdf_cubicDirection(msdf_Vec2 r, msdf_EdgeSegment *e, float param)
{
    msdf_Vec2 a, b, c, d, t;
    msdf_v2Sub(a, e->p[1], e->p[0]);
    msdf_v2Sub(b, e->p[2], e->p[1]);
    msdf_mix(c, a, b, param);
    msdf_v2Sub(a, e->p[3], e->p[2]);
    msdf_mix(d, b, a, param);
    msdf_mix(t, c, d, param);

    if (!t[0] && !t[1])
    {
        if (param == 0)
        {
            r[0] = e->p[2][0] - e->p[0][0];
            r[1] = e->p[2][1] - e->p[0][1];
            return;
        }
        if (param == 1)
        {
            r[0] = e->p[3][0] - e->p[1][0];
            r[1] = e->p[3][1] - e->p[1][1];
            return;
        }
    }

    r[0] = t[0];
    r[1] = t[1];
}

void msdf_direction(msdf_Vec2 r, msdf_EdgeSegment *e, float param)
{
    switch (e->type)
    {
		case STBTT_vline:
		{
			msdf_linearDirection(r, e, param);
			break;
		}
		case STBTT_vcurve:
		{
			msdf_quadraticDirection(r, e, param);
			break;
		}
		case STBTT_vcubic:
		{
			msdf_cubicDirection(r, e, param);
			break;
		}
    }
}

void msdf_linearPoint(msdf_Vec2 r, msdf_EdgeSegment *e, float param)
{
    msdf_mix(r, e->p[0], e->p[1], param);
}

void msdf_quadraticPoint(msdf_Vec2 r, msdf_EdgeSegment *e, float param)
{
    msdf_Vec2 a, b;
    msdf_mix(a, e->p[0], e->p[1], param);
    msdf_mix(b, e->p[1], e->p[2], param);
    msdf_mix(r, a, b, param);
}

void msdf_cubicPoint(msdf_Vec2 r, msdf_EdgeSegment *e, float param)
{
    msdf_Vec2 p12, a, b, c, d;
    msdf_mix(p12, e->p[1], e->p[2], param);

    msdf_mix(a, e->p[0], e->p[1], param);
    msdf_mix(b, a, p12, param);

    msdf_mix(c, e->p[2], e->p[3], param);
    msdf_mix(d, p12, c, param);

    msdf_mix(r, b, d, param);
}

void msdf_point(msdf_Vec2 r, msdf_EdgeSegment *e, float param)
{
    switch (e->type)
    {
		case STBTT_vline:
		{
			msdf_linearPoint(r, e, param);
			break;
		}
		case STBTT_vcurve:
		{
			msdf_quadraticPoint(r, e, param);
			break;
		}
		case STBTT_vcubic:
		{
			msdf_cubicPoint(r, e, param);
			break;
		}
    }
}

// linear edge signed distance
msdf_signedDistance msdf_linearDist(msdf_EdgeSegment *e, msdf_Vec2 origin, float *param)
{
    msdf_Vec2 aq, ab, eq;
    msdf_v2Sub(aq, origin, e->p[0]);
    msdf_v2Sub(ab, e->p[1], e->p[0]);
    *param = msdf_v2MulInner(aq, ab) / msdf_v2MulInner(ab, ab);
    msdf_v2Sub(eq, e->p[*param > .5], origin);

    float endpoint_distance = msdf_v2Leng(eq);
    if (*param > 0 && *param < 1)
    {
        msdf_Vec2 ab_ortho;
        msdf_getOrtho(ab_ortho, ab, 0, 0);
        float ortho_dist = msdf_v2MulInner(ab_ortho, aq);
		if (fabs(ortho_dist) < endpoint_distance) {
			return MSDF_STRUCT_LITERAL(msdf_signedDistance, ortho_dist, 0);
		}
    }

    msdf_v2Norm(ab, ab);
    msdf_v2Norm(eq, eq);
    float dist = msdf_nonZeroSign(msdf_cross(aq, ab)) * endpoint_distance;
    float d = fabsf(msdf_v2MulInner(ab, eq));
    return MSDF_STRUCT_LITERAL(msdf_signedDistance, dist, d);
}

// quadratic edge signed distance
msdf_signedDistance msdf_quadraticDist(msdf_EdgeSegment *e, msdf_Vec2 origin, float *param)
{
    msdf_Vec2 qa, ab, br;
    msdf_v2Sub(qa, e->p[0], origin);
    msdf_v2Sub(ab, e->p[1], e->p[0]);
    br[0] = e->p[0][0] + e->p[2][0] - e->p[1][0] - e->p[1][0];
    br[1] = e->p[0][1] + e->p[2][1] - e->p[1][1] - e->p[1][1];

    float a = msdf_v2MulInner(br, br);
    float b = 3 * msdf_v2MulInner(ab, br);
    float c = 2 * msdf_v2MulInner(ab, ab) + msdf_v2MulInner(qa, br);
    float d = msdf_v2MulInner(qa, ab);
    float t[3];
    int solutions = msdf_solveCubic(t, a, b, c, d);

    // distance from a
    float min_distance = msdf_nonZeroSign(msdf_cross(ab, qa)) * msdf_v2Leng(qa);
    *param = -msdf_v2MulInner(qa, ab) / msdf_v2MulInner(ab, ab);
    {
        msdf_Vec2 a, b;
        msdf_v2Sub(a, e->p[2], e->p[1]);
        msdf_v2Sub(b, e->p[2], origin);

        // distance from b
        float distance = msdf_nonZeroSign(msdf_cross(a, b)) * msdf_v2Leng(b);
        if (fabs(distance) < fabs(min_distance))
        {
            min_distance = distance;

            msdf_v2Sub(a, origin, e->p[1]);
            msdf_v2Sub(b, e->p[2], e->p[1]);
            *param = msdf_v2MulInner(a, b) / msdf_v2MulInner(b, b);
        }
    }

    for (int i = 0; i < solutions; ++i)
    {
        if (t[i] > 0 && t[i] < 1)
        {
            // end_point = p[0]+2*t[i]*ab+t[i]*t[i]*br;
            msdf_Vec2 end_point, a, b;
            end_point[0] = e->p[0][0] + 2 * t[i] * ab[0] + t[i] * t[i] * br[0];
            end_point[1] = e->p[0][1] + 2 * t[i] * ab[1] + t[i] * t[i] * br[1];

            msdf_v2Sub(a, e->p[2], e->p[0]);
            msdf_v2Sub(b, end_point, origin);
            float distance = msdf_nonZeroSign(msdf_cross(a, b)) * msdf_v2Leng(b);
            if (fabs(distance) <= fabs(min_distance))
            {
                min_distance = distance;
                *param = t[i];
            }
        }
    }

    if (*param >= 0 && *param <= 1)
        return MSDF_STRUCT_LITERAL(msdf_signedDistance, min_distance, 0);

    msdf_Vec2 aa, bb;
    msdf_v2Norm(ab, ab);
    msdf_v2Norm(qa, qa);
    msdf_v2Sub(aa, e->p[2], e->p[1]);
    msdf_v2Norm(aa, aa);
    msdf_v2Sub(bb, e->p[2], origin);
    msdf_v2Norm(bb, bb);

    if (*param < .5)
        return MSDF_STRUCT_LITERAL(msdf_signedDistance, min_distance, fabsf(msdf_v2MulInner(ab, qa)));
    else
        return MSDF_STRUCT_LITERAL(msdf_signedDistance, min_distance, fabsf(msdf_v2MulInner(aa, bb)));
}

// cubic edge signed distance
msdf_signedDistance msdf_cubicDist(msdf_EdgeSegment *e, msdf_Vec2 origin, float *param)
{
    msdf_Vec2 qa, ab, br, as;
    msdf_v2Sub(qa, e->p[0], origin);
    msdf_v2Sub(ab, e->p[1], e->p[0]);
    br[0] = e->p[2][0] - e->p[1][0] - ab[0];
    br[1] = e->p[2][1] - e->p[1][1] - ab[1];
    as[0] = (e->p[3][0] - e->p[2][0]) - (e->p[2][0] - e->p[1][0]) - br[0];
    as[1] = (e->p[3][1] - e->p[2][1]) - (e->p[2][1] - e->p[1][1]) - br[1];

    msdf_Vec2 ep_dir;
    msdf_direction(ep_dir, e, 0);

    // distance from a
    float min_distance = msdf_nonZeroSign(msdf_cross(ep_dir, qa)) * msdf_v2Leng(qa);
    *param = -msdf_v2MulInner(qa, ep_dir) / msdf_v2MulInner(ep_dir, ep_dir);
    {
        msdf_Vec2 a;
        msdf_v2Sub(a, e->p[3], origin);

        msdf_direction(ep_dir, e, 1);
        // distance from b
        float distance = msdf_nonZeroSign(msdf_cross(ep_dir, a)) * msdf_v2Leng(a);
        if (fabsf(distance) < fabsf(min_distance))
        {
            min_distance = distance;

            a[0] = origin[0] + ep_dir[0] - e->p[3][0];
            a[1] = origin[1] + ep_dir[1] - e->p[3][1];
            *param = msdf_v2MulInner(a, ep_dir) / msdf_v2MulInner(ep_dir, ep_dir);
        }
    }

    const int search_starts = 4;
    for (int i = 0; i <= search_starts; ++i)
    {
        float t = (float)i / search_starts;
        for (int step = 0;; ++step)
        {
            msdf_Vec2 qpt;
            msdf_point(qpt, e, t);
            msdf_v2Sub(qpt, qpt, origin);
            msdf_Vec2 d;
            msdf_direction(d, e, t);
            float distance = msdf_nonZeroSign(msdf_cross(d, qpt)) * msdf_v2Leng(qpt);
            if (fabsf(distance) < fabsf(min_distance))
            {
                min_distance = distance;
                *param = t;
            }
            if (step == search_starts)
                break;

            msdf_Vec2 d1, d2;
            d1[0] = 3 * as[0] * t * t + 6 * br[0] * t + 3 * ab[0];
            d1[1] = 3 * as[1] * t * t + 6 * br[1] * t + 3 * ab[1];
            d2[0] = 6 * as[0] * t + 6 * br[0];
            d2[1] = 6 * as[1] * t + 6 * br[1];

            t -= msdf_v2MulInner(qpt, d1) / (msdf_v2MulInner(d1, d1) + msdf_v2MulInner(qpt, d2));
            if (t < 0 || t > 1)
                break;
        }
    }

    if (*param >= 0 && *param <= 1)
        return MSDF_STRUCT_LITERAL(msdf_signedDistance, min_distance, 0);

    msdf_Vec2 d0, d1;
    msdf_direction(d0, e, 0);
    msdf_direction(d1, e, 1);
    msdf_v2Norm(d0, d0);
    msdf_v2Norm(d1, d1);
    msdf_v2Norm(qa, qa);
    msdf_Vec2 a;
    msdf_v2Sub(a, e->p[3], origin);
    msdf_v2Norm(a, a);

    if (*param < .5)
        return MSDF_STRUCT_LITERAL(msdf_signedDistance, min_distance, fabsf(msdf_v2MulInner(d0, qa)));
    else
        return MSDF_STRUCT_LITERAL(msdf_signedDistance, min_distance, fabsf(msdf_v2MulInner(d1, a)));
}

void msdf_distToPseudo(msdf_signedDistance *distance, msdf_Vec2 origin, float param, msdf_EdgeSegment *e) {
    if (param < 0) {
        msdf_Vec2 dir, p;
        msdf_direction(dir, e, 0);
        msdf_v2Norm(dir, dir);
        msdf_Vec2 aq = {origin[0], origin[1]};
        msdf_point(p, e, 0);
        msdf_v2Sub(aq, origin, p);
        float ts = msdf_v2MulInner(aq, dir);
        if (ts < 0) {
            float pseudo_dist = msdf_cross(aq, dir);
            if (fabsf(pseudo_dist) <= fabsf(distance->dist)) {
                distance->dist = pseudo_dist;
                distance->d = 0;
            }
        }
    } else if (param > 1) {
        msdf_Vec2 dir, p;
        msdf_direction(dir, e, 1);
        msdf_v2Norm(dir, dir);
        msdf_Vec2 bq = {origin[0], origin[1]};
        msdf_point(p, e, 1);
        msdf_v2Sub(bq, origin, p);
        float ts = msdf_v2MulInner(bq, dir);
        if (ts > 0) {
            float pseudo_dist = msdf_cross(bq, dir);
            if (fabsf(pseudo_dist) <= fabsf(distance->dist)) {
                distance->dist = pseudo_dist;
                distance->d = 0;
            }
        }
    }
}

int msdf_signedCompare(msdf_signedDistance a, msdf_signedDistance b) {
    return fabsf(a.dist) < fabsf(b.dist) || (fabsf(a.dist) == fabsf(b.dist) && a.d < b.d);
}

int msdf_isCorner(msdf_Vec2 a, msdf_Vec2 b, float threshold) {
    return msdf_v2MulInner(a, b) <= 0 || fabsf(msdf_cross(a, b)) > threshold;
}

void msdf_switchColor(msdf_edgeColor *color, unsigned long long *seed, msdf_edgeColor banned)
{
    msdf_edgeColor combined = (msdf_edgeColor)(*color & banned);
    if (combined == msdf_edgeColor_red || combined == msdf_edgeColor_green || combined == msdf_edgeColor_blue) {
        *color = (msdf_edgeColor)(combined ^ msdf_edgeColor_white);
        return;
    }

    if (*color == msdf_edgeColor_black || *color == msdf_edgeColor_white) {
        static const msdf_edgeColor start[3] = {msdf_edgeColor_cyan, msdf_edgeColor_magenta, msdf_edgeColor_yellow};
        *color = start[*seed & 3];
        *seed /= 3;
        return;
    }

    int shifted = *color << (1 + (*seed & 1));
    *color = (msdf_edgeColor)((shifted | shifted >> 3) & msdf_edgeColor_white);
    *seed >>= 1;
}

void msdf_linearSplit(msdf_EdgeSegment *e, msdf_EdgeSegment *p1, msdf_EdgeSegment *p2, msdf_EdgeSegment *p3)
{
    msdf_Vec2 p;

    msdf_point(p, e, 1 / 3.f);
    memcpy(&p1->p[0], e->p[0], sizeof(msdf_Vec2));
    memcpy(&p1->p[1], p, sizeof(msdf_Vec2));
    p1->color = e->color;

    msdf_point(p, e, 1 / 3.f);
    memcpy(&p2->p[0], p, sizeof(msdf_Vec2));
    msdf_point(p, e, 2 / 3.f);
    memcpy(&p2->p[1], p, sizeof(msdf_Vec2));
    p2->color = e->color;

    msdf_point(p, e, 2 / 3.f);
    memcpy(&p3->p[0], p, sizeof(msdf_Vec2));
    msdf_point(p, e, 2 / 3.f);
    memcpy(&p3->p[1], e->p[1], sizeof(msdf_Vec2));
    p3->color = e->color;
}

void msdf_quadraticSplit(msdf_EdgeSegment *e, msdf_EdgeSegment *p1, msdf_EdgeSegment *p2, msdf_EdgeSegment *p3)
{
    msdf_Vec2 p, a, b;

    memcpy(&p1->p[0], e->p[0], sizeof(msdf_Vec2));
    msdf_mix(p, e->p[0], e->p[1], 1 / 3.f);
    memcpy(&p1->p[1], p, sizeof(msdf_Vec2));
    msdf_point(p, e, 1 / 3.f);
    memcpy(&p1->p[2], p, sizeof(msdf_Vec2));
    p1->color = e->color;

    msdf_point(p, e, 1 / 3.f);
    memcpy(&p2->p[0], p, sizeof(msdf_Vec2));
    msdf_mix(a, e->p[0], e->p[1], 5 / 9.f);
    msdf_mix(b, e->p[1], e->p[2], 4 / 9.f);
    msdf_mix(p, a, b, 0.5);
    memcpy(&p2->p[1], p, sizeof(msdf_Vec2));
    msdf_point(p, e, 2 / 3.f);
    memcpy(&p2->p[2], p, sizeof(msdf_Vec2));
    p2->color = e->color;

    msdf_point(p, e, 2 / 3.f);
    memcpy(&p3->p[0], p, sizeof(msdf_Vec2));
    msdf_mix(p, e->p[1], e->p[2], 2 / 3.f);
    memcpy(&p3->p[1], p, sizeof(msdf_Vec2));
    memcpy(&p3->p[2], e->p[2], sizeof(msdf_Vec2));
    p3->color = e->color;
}

void msdf_cubicSplit(msdf_EdgeSegment *e, msdf_EdgeSegment *p1, msdf_EdgeSegment *p2, msdf_EdgeSegment *p3)
{
    msdf_Vec2 p, a, b, c, d;

    memcpy(&p1->p[0], e->p[0], sizeof(msdf_Vec2)); // p1 0
    if (e->p[0] == e->p[1]) {
        memcpy(&p1->p[1], e->p[0], sizeof(msdf_Vec2)); // ? p1 1
    } else {
        msdf_mix(p, e->p[0], e->p[1], 1 / 3.f);
        memcpy(&p1->p[1], p, sizeof(msdf_Vec2)); // ? p1 1
    }
    msdf_mix(a, e->p[0], e->p[1], 1 / 3.f);
    msdf_mix(b, e->p[1], e->p[2], 1 / 3.f);
    msdf_mix(p, a, b, 1 / 3.f);
    memcpy(&p1->p[2], p, sizeof(msdf_Vec2)); // p1 2
    msdf_point(p, e, 1 / 3.f);
    memcpy(&p1->p[3], p, sizeof(msdf_Vec2)); // p1 3
    p1->color = e->color;

    msdf_point(p, e, 1 / 3.f);
    memcpy(&p2->p[0], p, sizeof(msdf_Vec2)); // p2 0
    msdf_mix(a, e->p[0], e->p[1], 1 / 3.f);
    msdf_mix(b, e->p[1], e->p[2], 1 / 3.f);
    msdf_mix(c, a, b, 1 / 3.f);
    msdf_mix(a, e->p[1], e->p[2], 1 / 3.f);
    msdf_mix(b, e->p[2], e->p[3], 1 / 3.f);
    msdf_mix(d, a, b, 1 / 3.f);
    msdf_mix(p, c, d, 2 / 3.f);
    memcpy(&p2->p[1], p, sizeof(msdf_Vec2)); // p2 1
    msdf_mix(a, e->p[0], e->p[1], 2 / 3.f);
    msdf_mix(b, e->p[1], e->p[2], 2 / 3.f);
    msdf_mix(c, a, b, 2 / 3.f);
    msdf_mix(a, e->p[1], e->p[2], 2 / 3.f);
    msdf_mix(b, e->p[2], e->p[3], 2 / 3.f);
    msdf_mix(d, a, b, 2 / 3.f);
    msdf_mix(p, c, d, 1 / 3.f);
    memcpy(&p2->p[2], p, sizeof(msdf_Vec2)); // p2 2
    msdf_point(p, e, 2 / 3.f);
    memcpy(&p2->p[3], p, sizeof(msdf_Vec2)); // p2 3
    p2->color = e->color;

    msdf_point(p, e, 2 / 3.f);
    memcpy(&p3->p[0], p, sizeof(msdf_Vec2)); // p3 0

    msdf_mix(a, e->p[1], e->p[2], 2 / 3.f);
    msdf_mix(b, e->p[2], e->p[3], 2 / 3.f);
    msdf_mix(p, a, b, 2 / 3.f);
    memcpy(&p3->p[1], p, sizeof(msdf_Vec2)); // p3 1

    if (e->p[2] == e->p[3]) {
        memcpy(&p3->p[2], e->p[3], sizeof(msdf_Vec2)); // ? p3 2
    } else {
        msdf_mix(p, e->p[2], e->p[3], 2 / 3.f);
        memcpy(&p3->p[2], p, sizeof(msdf_Vec2)); // ? p3 2
    }

    memcpy(&p3->p[3], e->p[3], sizeof(msdf_Vec2)); // p3 3
}

void msdf_edgeSplit(msdf_EdgeSegment *e, msdf_EdgeSegment *p1, msdf_EdgeSegment *p2, msdf_EdgeSegment *p3)
{
    switch (e->type) {
        case STBTT_vline: {
            msdf_linearSplit(e, p1, p2, p3);
            break;
        }
        case STBTT_vcurve: {
            msdf_quadraticSplit(e, p1, p2, p3);
            break;
        }
        case STBTT_vcubic: {
            msdf_cubicSplit(e, p1, p2, p3);
            break;
        }
    }
}

float msdf_shoelace(const msdf_Vec2 a, const msdf_Vec2 b)
{
    return (b[0] - a[0]) * (a[1] + b[1]);
}


void* msdf__alloc(size_t size, void* ctx) {
    return malloc(size);
}

void msdf__free(void* ptr, void* ctx) {
    free(ptr);
}

int msdf_genGlyph(msdf_result_t* result, stbtt_fontinfo *font, int stbttGlyphIndex, uint32_t borderWidth, float scale, float range, msdf_allocation_context_t* alloc) {
    msdf_allocation_context_t allocCtx;

    if (alloc) {
        allocCtx = *alloc;
    } else {
        allocCtx.alloc = msdf__alloc;
        allocCtx.free = msdf__free;
        allocCtx.ctx = NULL;
    }

    //char f = c;
    // Funit to pixel scale
    //float scale = stbtt_ScaleForMappingEmToPixels(font, h);
    int glyphIdx = stbttGlyphIndex;
    // get glyph bounding box (scaled later)
    int ix0, iy0, ix1, iy1;
    float xoff = .0, yoff = .0;
    stbtt_GetGlyphBox(font, glyphIdx, &ix0, &iy0, &ix1, &iy1);

    float glyphWidth = (float)ix1 - (float)ix0;
    float glyphHeight = (float)iy1 - (float)iy0;
    float borderWidthF32 = (float)borderWidth;
    float wF32 = ceilf(glyphWidth  * scale);
    float hF32 = ceilf(glyphHeight * scale);
    wF32 += 2.f * borderWidth;
    hF32 += 2.f * borderWidth;
    int w = (int)wF32;
    int h = (int)hF32;

    float* bitmap = (float*) allocCtx.alloc(w * h * 4 * sizeof(float), allocCtx.ctx);
    memset(bitmap, 0x0, w * h * 4 * sizeof(float));

    // em scale
    //scale = stbtt_ScaleForMappingEmToPixels(font, h);

    //if (autofit)
    //{

	// calculate new height
	//float newh = h + (h - (iy1 - iy0) * scale) - 4;

	// calculate new scale
	// see 'stbtt_ScaleForMappingEmToPixels' in stb_truetype.h
	//uint8_t *p = font->data + font->head + 18;
	//int unitsPerEm = p[0] * 256 + p[1];
	//scale = ((float)h) / ((float)unitsPerEm);

	// make sure we are centered
	//xoff = .0;
	//yoff = .0;
    //}

    // get left offset and advance
    //int left_bearing, advance;
    //stbtt_GetGlyphHMetrics(font, glyphIdx, &advance, &left_bearing);
    //left_bearing *= scale;

    int32_t glyphOrgX = (int)(ix0 * scale);
    int32_t glyphOrgY = (int)(iy0 * scale);

    int32_t borderWidthX = borderWidth;
    int32_t borderWidthY = borderWidth;

    //   org  8,8
    // - bord 4,4
    // erg:   4,4

    // calculate offset for centering glyph on bitmap
    
    //glyphOrgX >= 2 ? (glyphOrgX) : ();
    int32_t translateX = (glyphOrgX - borderWidth);//borderWidth + ((w / 2) - ((ix1 - ix0) * scale) / 2 - leftBearingScaled);
    int32_t translateY = (glyphOrgY - borderWidth);//borderWidth + ((h / 2) - ((iy1 - iy0) * scale) / 2 - ((float) iy0) * scale);
    //translateY  = 8;
    // set the glyph metrics
    // (pre-scale them)

    #if 0
    if (metrics)
    {
        metrics->left_bearing = left_bearing;
        metrics->advance = advance * scale;
        metrics->ix0 = ix0 * scale;
        metrics->ix1 = ix1 * scale;
        metrics->iy0 = iy0 * scale;
        metrics->iy1 = iy1 * scale;
        metrics->glyphIdx = glyphIdx;
    }
    #endif

    stbtt_vertex *verts;
    int num_verts = stbtt_GetGlyphShape(font, glyphIdx, &verts);

    // figure out how many contours exist
    int contour_count = 0;
    for (int i = 0; i < num_verts; i++) {
        if (verts[i].type == STBTT_vmove) {
            contour_count++;
        }
    }

    if (contour_count == 0) {
        return 0;
    }

    // determin what vertices belong to what contours
    typedef struct {
        size_t start, end;
    } msdf_Indices;
    msdf_Indices *contours = (msdf_Indices*)allocCtx.alloc(sizeof(msdf_Indices) * contour_count, allocCtx.ctx);
    int j = -1;
    for (int i = 0; i < num_verts; i++) {
        if (verts[i].type == STBTT_vmove) {
            if (j >= 0) {
                contours[j].end = i;
            }
            j++;
            contours[j].start = i;
        }
    }
    if (j >= 0) {
        contours[j].end = num_verts;
    }

    typedef struct {
        msdf_signedDistance min_distance;
        msdf_EdgeSegment *near_edge;
        float near_param;
    } msdf_EdgePoint;

    typedef struct {
        msdf_EdgeSegment *edges;
        size_t edge_count;
    } msdf_Contour;

    // process verts into series of contour-specific edge lists
    msdf_Vec2 initial = {0, 0}; // fix this?
    msdf_Contour *contour_data = (msdf_Contour*)allocCtx.alloc(sizeof(msdf_Contour) * contour_count, allocCtx.ctx);
    float cscale = 64.0;
    for (int i = 0; i < contour_count; i++) {
        size_t count = contours[i].end - contours[i].start;
        contour_data[i].edges = (msdf_EdgeSegment*)allocCtx.alloc(sizeof(msdf_EdgeSegment) * count, allocCtx.ctx);
        contour_data[i].edge_count = 0;

        size_t k = 0;
        for (int j = (int)contours[i].start; j < (int)contours[i].end; j++) {
            msdf_EdgeSegment *e = &contour_data[i].edges[k];
            stbtt_vertex *v = &verts[j];
            e->type = v->type;
            e->color = msdf_edgeColor_white;

            switch (v->type) {
                case STBTT_vmove: {
                    msdf_Vec2 p = {v->x / cscale, v->y / cscale};
                    memcpy(&initial, p, sizeof(msdf_Vec2));
                    break;
                }

                case STBTT_vline: {
                    msdf_Vec2 p = {v->x / cscale, v->y / cscale};
                    memcpy(&e->p[0], initial, sizeof(msdf_Vec2));
                    memcpy(&e->p[1], p, sizeof(msdf_Vec2));
                    memcpy(&initial, p, sizeof(msdf_Vec2));
                    contour_data[i].edge_count++;
                    k++;
                    break;
                }

                case STBTT_vcurve: {
                    msdf_Vec2 p = {v->x / cscale, v->y / cscale};
                    msdf_Vec2 c = {v->cx / cscale, v->cy / cscale};
                    memcpy(&e->p[0], initial, sizeof(msdf_Vec2));
                    memcpy(&e->p[1], c, sizeof(msdf_Vec2));
                    memcpy(&e->p[2], p, sizeof(msdf_Vec2));

                    if ((e->p[0][0] == e->p[1][0] && e->p[0][1] == e->p[1][1]) ||
                        (e->p[1][0] == e->p[2][0] && e->p[1][1] == e->p[2][1]))
                    {
                        e->p[1][0] = 0.5f * (e->p[0][0] + e->p[2][0]);
                        e->p[1][1] = 0.5f * (e->p[0][1] + e->p[2][1]);
                    }

                    memcpy(&initial, p, sizeof(msdf_Vec2));
                    contour_data[i].edge_count++;
                    k++;
                    break;
                }

                case STBTT_vcubic: {
                    msdf_Vec2 p = {v->x / cscale, v->y / cscale};
                    msdf_Vec2 c = {v->cx / cscale, v->cy / cscale};
                    msdf_Vec2 c1 = {v->cx1 / cscale, v->cy1 / cscale};
                    memcpy(&e->p[0], initial, sizeof(msdf_Vec2));
                    memcpy(&e->p[1], c, sizeof(msdf_Vec2));
                    memcpy(&e->p[2], c1, sizeof(msdf_Vec2));
                    memcpy(&e->p[3], p, sizeof(msdf_Vec2));
                    memcpy(&initial, p, sizeof(msdf_Vec2));
                    contour_data[i].edge_count++;
                    k++;
                    break;
                }
            }
        }
    }

    // calculate edge-colors
    uint64_t seed = 0;
    float anglethreshold = 3.0;
    float crossthreshold = sinf(anglethreshold);
    size_t corner_count = 0;
    for (int i = 0; i < contour_count; ++i) {
        for (int j = 0; j < contour_data[i].edge_count; ++j) {
            corner_count++;
        }
    }

    int *corners = (int*)allocCtx.alloc(sizeof(int) * corner_count, allocCtx.ctx);
    int cornerIndex = 0;
    for (int i = 0; i < contour_count; ++i) {

        if (contour_data[i].edge_count > 0) {
            msdf_Vec2 prev_dir, dir;
            msdf_direction(prev_dir, &contour_data[i].edges[contour_data[i].edge_count - 1], 1);

            int index = 0;
            for (int j = 0; j < contour_data[i].edge_count; ++j, ++index) {
                msdf_EdgeSegment *e = &contour_data[i].edges[j];
                msdf_direction(dir, e, 0);
                msdf_v2Norm(dir, dir);
                msdf_v2Norm(prev_dir, prev_dir);
                if (msdf_isCorner(prev_dir, dir, crossthreshold)) {
                    corners[cornerIndex++] = index;
                }
                msdf_direction(prev_dir, e, 1);
            }
        }

        if (cornerIndex == 0) {
            for (int j = 0; j < contour_data[i].edge_count; ++j) {
                contour_data[i].edges[j].color = msdf_edgeColor_white;
            }
        } else if (cornerIndex == 1) {
            msdf_edgeColor colors[3] = {msdf_edgeColor_white, msdf_edgeColor_white};
            msdf_switchColor(&colors[0], &seed, msdf_edgeColor_black);
            colors[2] = colors[0];
            msdf_switchColor(&colors[2], &seed, msdf_edgeColor_black);

            int corner = corners[0];
            if (contour_data[i].edge_count >= 3) {
                int m = (int)contour_data[i].edge_count;
                for (int j = 0; j < m; ++j) {
                    contour_data[i].edges[(corner + j) % m].color = (colors + 1)[(int)(3 + 2.875 * i / (m - 1) - 1.4375 + .5) - 3];
                }
            } else if (contour_data[i].edge_count >= 1) {
                msdf_EdgeSegment *parts[7] = {NULL};
                msdf_edgeSplit(&contour_data[i].edges[0], parts[0 + 3 * corner], parts[1 + 3 * corner], parts[2 + 3 * corner]);
                if (contour_data[i].edge_count >= 2) {
                    msdf_edgeSplit(&contour_data[i].edges[1], parts[3 - 3 * corner], parts[4 - 3 * corner], parts[5 - 3 * corner]);
                    parts[0]->color = parts[1]->color = colors[0];
                    parts[2]->color = parts[3]->color = colors[1];
                    parts[4]->color = parts[5]->color = colors[2];
                } else {
                    parts[0]->color = colors[0];
                    parts[1]->color = colors[1];
                    parts[2]->color = colors[2];
                }
                if (allocCtx.free) {
                    allocCtx.free(contour_data[i].edges, allocCtx.ctx);
                }
                contour_data[i].edges = (msdf_EdgeSegment*)allocCtx.alloc(sizeof(msdf_EdgeSegment) * 7, allocCtx.ctx);
                contour_data[i].edge_count = 0;
                int index = 0;
                for (int j = 0; parts[j]; ++j) {
                    memcpy(&contour_data[i].edges[index++], &parts[j], sizeof(msdf_EdgeSegment));
                    contour_data[i].edge_count++;
                }
            }
        } else {
            int spline = 0;
            int start = corners[0];
            int m = (int)contour_data[i].edge_count;
            msdf_edgeColor color = msdf_edgeColor_white;
            msdf_switchColor(&color, &seed, msdf_edgeColor_black);
            msdf_edgeColor initial_color = color;
            for (int j = 0; j < m; ++j) {
                int index = (start + j) % m;
                if (spline + 1 < corner_count && corners[spline + 1] == index) {
                    ++spline;

                    msdf_edgeColor s = (msdf_edgeColor)((spline == corner_count - 1) * initial_color);
                    msdf_switchColor(&color, &seed, s);
                }
                contour_data[i].edges[index].color = color;
            }
        }
    }

    if (allocCtx.free) {
        allocCtx.free(corners, allocCtx.ctx);
    }

    // normalize shape
    for (int i = 0; i < contour_count; i++) {
        if (contour_data[i].edge_count == 1) {
            msdf_EdgeSegment *parts[3] = {0};
            msdf_edgeSplit(&contour_data[i].edges[0], parts[0], parts[1], parts[2]);
            if (allocCtx.free) {
                allocCtx.free(contour_data[i].edges, allocCtx.ctx);
            }
            contour_data[i].edges = (msdf_EdgeSegment*)allocCtx.alloc(sizeof(msdf_EdgeSegment) * 3, allocCtx.ctx);
            contour_data[i].edge_count = 3;
            for (int j = 0; j < 3; j++) {
                memcpy(&contour_data[i].edges[j], &parts[j], sizeof(msdf_EdgeSegment));
            }
        }
    }

    // calculate windings
    int *windings = (int*)allocCtx.alloc(sizeof(int) * contour_count, allocCtx.ctx);
    for (int i = 0; i < contour_count; i++) {
        size_t edge_count = contour_data[i].edge_count;
        if (edge_count == 0) {
            windings[i] = 0;
            continue;
        }

        float total = 0;

        if (edge_count == 1) {
            msdf_Vec2 a, b, c;
            msdf_point(a, &contour_data[i].edges[0], 0);
            msdf_point(b, &contour_data[i].edges[0], 1 / 3.f);
            msdf_point(c, &contour_data[i].edges[0], 2 / 3.f);
            total += msdf_shoelace(a, b);
            total += msdf_shoelace(b, c);
            total += msdf_shoelace(c, a);
        } else if (edge_count == 2) {
            msdf_Vec2 a, b, c, d;
            msdf_point(a, &contour_data[i].edges[0], 0.f);
            msdf_point(b, &contour_data[i].edges[0], 0.5f);
            msdf_point(c, &contour_data[i].edges[1], 0.f);
            msdf_point(d, &contour_data[i].edges[1], 0.5f);
            total += msdf_shoelace(a, b);
            total += msdf_shoelace(b, c);
            total += msdf_shoelace(c, d);
            total += msdf_shoelace(d, a);
        } else {
            msdf_Vec2 prev;
            msdf_point(prev, &contour_data[i].edges[edge_count - 1], 0);
            for (int j = 0; j < edge_count; j++) {
                msdf_Vec2 cur;
                msdf_point(cur, &contour_data[i].edges[j], 0);
                total += msdf_shoelace(prev, cur);
                memcpy(prev, cur, sizeof(msdf_Vec2));
            }
        }

        windings[i] = ((0 < total) - (total < 0)); // sign
    }

    typedef struct {
        float r, g, b;
        float med;
    } msdf_MultiDistance;

    msdf_MultiDistance *contour_sd;
    contour_sd = (msdf_MultiDistance*)allocCtx.alloc(sizeof(msdf_MultiDistance) * contour_count, allocCtx.ctx);

    float invRange = 1.f / range;

    for (int y = 0; y < h; ++y) {
        int row = iy0 > iy1 ? y : h - y - 1;
        for (int x = 0; x < w; ++x) {
            float a64 = 64.f;
			msdf_Vec2 p = {(translateX + x + xoff) / (scale * a64), (translateY + y + yoff) / (scale * a64)};
            //p[0] = ;
            //p[1] = ;
            msdf_EdgePoint sr, sg, sb;
            sr.near_edge = sg.near_edge = sb.near_edge = NULL;
            sr.near_param = sg.near_param = sb.near_param = 0;
            sr.min_distance.dist = sg.min_distance.dist = sb.min_distance.dist = (float)MSDF_INF;
            sr.min_distance.d = sg.min_distance.d = sb.min_distance.d = 1;
            float d = fabsf((float)MSDF_INF);
            float neg_dist = (float)-MSDF_INF;
            float pos_dist = (float)MSDF_INF;
            int winding = 0;

            // calculate distance to contours from current point (and if its inside or outside of the shape?)
            for (int j = 0; j < contour_count; ++j) {
                msdf_EdgePoint r, g, b;
                r.near_edge = g.near_edge = b.near_edge = NULL;
                r.near_param = g.near_param = b.near_param = 0;
                r.min_distance.dist = g.min_distance.dist = b.min_distance.dist = (float)MSDF_INF;
                r.min_distance.d = g.min_distance.d = b.min_distance.d = 1;

                for (int k = 0; k < contour_data[j].edge_count; ++k) {
                    msdf_EdgeSegment *e = &contour_data[j].edges[k];
                    float param;
                    msdf_signedDistance distance;
                    distance.dist = (float)MSDF_INF;
                    distance.d = 1;

                    // calculate signed distance
                    switch (e->type) {
						case STBTT_vline: {
							distance = msdf_linearDist(e, p, &param);
							break;
						}
						case STBTT_vcurve: {
							distance = msdf_quadraticDist(e, p, &param);
							break;
						}
						case STBTT_vcubic: {
							distance = msdf_cubicDist(e, p, &param);
							break;
						}
                    }

                    if (e->color & msdf_edgeColor_red && msdf_signedCompare(distance, r.min_distance)) {
                        r.min_distance = distance;
                        r.near_edge = e;
                        r.near_param = param;
                    }
                    if (e->color & msdf_edgeColor_green && msdf_signedCompare(distance, g.min_distance)) {
                        g.min_distance = distance;
                        g.near_edge = e;
                        g.near_param = param;
                    }
                    if (e->color & msdf_edgeColor_blue && msdf_signedCompare(distance, b.min_distance)) {
                        b.min_distance = distance;
                        b.near_edge = e;
                        b.near_param = param;
                    }
                }

                if (msdf_signedCompare(r.min_distance, sr.min_distance)) {
                    sr = r;
                }
                if (msdf_signedCompare(g.min_distance, sg.min_distance)) {
                    sg = g;
                }
                if (msdf_signedCompare(b.min_distance, sb.min_distance)) {
                    sb = b;
                }

                float med_min_dist = fabsf(msdf_median(r.min_distance.dist, g.min_distance.dist, b.min_distance.dist));

                if (med_min_dist < d) {
                    d = med_min_dist;
                    winding = -windings[j];
                }

                if (r.near_edge) {
                    msdf_distToPseudo(&r.min_distance, p, r.near_param, r.near_edge);
                }
                if (g.near_edge) {
                    msdf_distToPseudo(&g.min_distance, p, g.near_param, g.near_edge);
                }
                if (b.near_edge) {
                    msdf_distToPseudo(&b.min_distance, p, b.near_param, b.near_edge);
                }

                med_min_dist = msdf_median(r.min_distance.dist, g.min_distance.dist, b.min_distance.dist);
                contour_sd[j].r = r.min_distance.dist;
                contour_sd[j].g = g.min_distance.dist;
                contour_sd[j].b = b.min_distance.dist;
                contour_sd[j].med = med_min_dist;

                if (windings[j] > 0 && med_min_dist >= 0 && fabsf(med_min_dist) < fabsf(pos_dist)) {
                    pos_dist = med_min_dist;
                }
                if (windings[j] < 0 && med_min_dist <= 0 && fabsf(med_min_dist) < fabsf(neg_dist)) {
                    neg_dist = med_min_dist;
                }
            }

            if (sr.near_edge) {
                msdf_distToPseudo(&sr.min_distance, p, sr.near_param, sr.near_edge);
            }
            if (sg.near_edge) {
                msdf_distToPseudo(&sg.min_distance, p, sg.near_param, sg.near_edge);
            }
            if (sb.near_edge) {
                msdf_distToPseudo(&sb.min_distance, p, sb.near_param, sb.near_edge);
            }

            msdf_MultiDistance msd;
            msd.r = msd.g = msd.b = msd.med = (float)MSDF_INF;
            if (pos_dist >= 0 && fabsf(pos_dist) <= fabsf(neg_dist)) {
                msd.med = (float)MSDF_INF;
                winding = 1;
                for (int i = 0; i < contour_count; ++i) {
                    if (windings[i] > 0 && contour_sd[i].med > msd.med && fabsf(contour_sd[i].med) < fabsf(neg_dist)) {
                        msd = contour_sd[i];
                    }
                }
            } else if (neg_dist <= 0 && fabsf(neg_dist) <= fabsf(pos_dist)) {
                msd.med = (float)-MSDF_INF;
                winding = -1;
                for (int i = 0; i < contour_count; ++i) {
                    if (windings[i] < 0 && contour_sd[i].med < msd.med && fabsf(contour_sd[i].med) < fabsf(pos_dist)) {
                        msd = contour_sd[i];
                    }
                }
            }

            for (int i = 0; i < contour_count; ++i) {
                if (windings[i] != winding && fabsf(contour_sd[i].med) < fabsf(msd.med)) {
                    msd = contour_sd[i];
                }
            }

            if (msdf_median(sr.min_distance.dist, sg.min_distance.dist, sb.min_distance.dist) == msd.med) {
                msd.r = sr.min_distance.dist;
                msd.g = sg.min_distance.dist;
                msd.b = sb.min_distance.dist;
            }

            size_t index = 4 * ((row * w) + x);

            float mr = ((float)msd.r) * invRange + 0.5f;
            float mg = ((float)msd.g) * invRange + 0.5f;
            float mb = ((float)msd.b) * invRange + 0.5f;
            bitmap[index + 0] = mr;
            bitmap[index + 1] = mg;
            bitmap[index + 2] = mb;
            bitmap[index + 3] = 1.f;
            
        }
    }

    if (allocCtx.free) {
        for (int i = 0; i < contour_count; i++) {
            allocCtx.free(contour_data[i].edges, allocCtx.ctx);
        }
        allocCtx.free(contour_data, allocCtx.ctx);
        allocCtx.free(contour_sd, allocCtx.ctx);
        allocCtx.free(contours, allocCtx.ctx);
        allocCtx.free(windings, allocCtx.ctx);
        allocCtx.free(verts, allocCtx.ctx);
    }

    // msdf error correction
    typedef struct {
        int x, y;
    } msdf_Clash;
    msdf_Clash *clashes = (msdf_Clash*)allocCtx.alloc(sizeof(msdf_Clash) * w * h, allocCtx.ctx);
    size_t cindex = 0;

    float tx = MSDF_EDGE_THRESHOLD / (scale * range);
    float ty = MSDF_EDGE_THRESHOLD / (scale * range);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if ((x > 0 && msdf_pixelClash(msdf_pixelAt(x, y, w, bitmap), msdf_pixelAt(msdf_max(x - 1, 0), y, w, bitmap), tx)) || (x < w - 1 && msdf_pixelClash(msdf_pixelAt(x, y, w, bitmap), msdf_pixelAt(msdf_min(x + 1, w - 1), y, w, bitmap), tx)) || (y > 0 && msdf_pixelClash(msdf_pixelAt(x, y, w, bitmap), msdf_pixelAt(x, msdf_max(y - 1, 0), w, bitmap), ty)) || (y < h - 1 && msdf_pixelClash(msdf_pixelAt(x, y, w, bitmap), msdf_pixelAt(x, msdf_min(y + 1, h - 1), w, bitmap), ty))) {
                clashes[cindex].x = x;
                clashes[cindex++].y = y;
            }
        }
    }

    for (int i = 0; i < cindex; i++) {
        size_t index = 4 * ((clashes[i].y * w) + clashes[i].x);
        float med = msdf_median(bitmap[index], bitmap[index + 1], bitmap[index + 2]);
        bitmap[index + 0] = med;
        bitmap[index + 1] = med;
        bitmap[index + 2] = med;
        bitmap[index + 3] = 1.f;
    }

    if (allocCtx.free) {
        allocCtx.free(clashes, allocCtx.ctx);
    }

    result->glyphIdx = glyphIdx;
    result->rgba = bitmap;
    result->width = w;
    result->height = h;
    result->yOffset = translateY;

    return 1;
}

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
    float radius;
    float bleed;
    float aa_factor;
    float thickness;
    zest_uint sampler_index;
    zest_uint image_index;
} zest_msdf_font_settings_t;

typedef struct zest_msdf_font_settings2_t {
	zest_vec2 unit_range;
	float in_bias;
	float out_bias;
	float supersample;
	float smoothness;
	float gamma;
    zest_uint sampler_index;
    zest_uint image_index;
} zest_msdf_font_settings2_t;

typedef struct zest_msdf_font_t {
	zest_image_collection_handle font_atlas;
	zest_image_handle font_image;
    zest_font_character_t *characters;
	zest_uint font_binding_index;
	int first_character_offset;
	zest_msdf_font_settings2_t settings;
	zest_context context;
	float size;
	float sdf_range;
	float y_max_offset;
} zest_msdf_font_t;

typedef struct zest_font_instance_t {           //48 bytes
	zest_vec2 position;                   		//The position of the sprite with rotation in w and stretch in z
	zest_u64 uv;                                //The UV coords of the image in the texture packed into a u64 snorm (4 16bit floats)
	zest_u64 size_handle;                       //Size of the sprite in pixels and the handle packed into a u64 (4 16bit floats)
	float rotation;
	float intensity;
	zest_color color;                           //The color tint of the sprite
	zest_uint texture_array;             		//reference for the texture array (8bits) and intensity (24bits)
	zest_uint padding[2];
} zest_font_instance_t;

void* zest__msdf_allocation(size_t size, void* ctx) {
	zest_context context = (zest_context)ctx;
    return zest_AllocateMemory(context, size);
}

void zest__msdf_free(void* ptr, void* ctx) {
	zest_context context = (zest_context)ctx;
    zest_FreeMemory(context, ptr);
}

zest_font_resources_t zest_CreateFontResources(zest_context context, zest_uniform_buffer_handle uniform_buffer) {
	//Create and compile the shaders for our custom sprite pipeline
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	zest_shader_handle font_vert = zest_CreateShaderFromFile(context, "shaders/font.vert", "font_vert.spv", shaderc_vertex_shader, true, compiler, 0);
	zest_shader_handle font_frag = zest_CreateShaderFromFile(context, "shaders/font2.frag", "font_frag.spv", shaderc_fragment_shader, true, compiler, 0);
	shaderc_compiler_release(compiler);

	//Create a pipeline that we can use to draw billboards
	zest_pipeline_template font_pipeline = zest_BeginPipelineTemplate(context, "pipeline_billboard");
	zest_AddVertexInputBindingDescription(font_pipeline, 0, sizeof(zest_font_instance_t), zest_input_rate_instance);

    zest_AddVertexAttribute(font_pipeline, 0, 0, zest_format_r32g32_sfloat, offsetof(zest_font_instance_t, position));                  // Location 0: UV coords
    zest_AddVertexAttribute(font_pipeline, 0, 1, zest_format_r16g16b16a16_snorm, offsetof(zest_font_instance_t, uv));   // Location 1: Instance Position and rotation
    zest_AddVertexAttribute(font_pipeline, 0, 2, zest_format_r16g16b16a16_sscaled, offsetof(zest_font_instance_t, size_handle));        // Location 2: Size of the sprite in pixels
    zest_AddVertexAttribute(font_pipeline, 0, 3, zest_format_r32_sfloat, offsetof(zest_font_instance_t, rotation));                  // Location 3: Alignment
    zest_AddVertexAttribute(font_pipeline, 0, 4, zest_format_r32_sfloat, offsetof(zest_font_instance_t, intensity));                    // Location 4: Instance Color
    zest_AddVertexAttribute(font_pipeline, 0, 5, zest_format_r8g8b8a8_unorm, offsetof(zest_font_instance_t, color));        // Location 5: Instance Parameters
    zest_AddVertexAttribute(font_pipeline, 0, 6, zest_format_r32_uint, offsetof(zest_font_instance_t, texture_array));        // Location 5: Instance Parameters

	zest_SetPipelinePushConstantRange(font_pipeline, sizeof(zest_msdf_font_settings2_t), zest_shader_fragment_stage);
	zest_SetPipelineVertShader(font_pipeline, font_vert);
	zest_SetPipelineFragShader(font_pipeline, font_frag);
	zest_AddPipelineDescriptorLayout(font_pipeline, zest_GetUniformBufferLayout(uniform_buffer));
	zest_AddPipelineDescriptorLayout(font_pipeline, zest_GetGlobalBindlessLayout(context));
	zest_SetPipelineDepthTest(font_pipeline, true, false);

	zest_shader_resources_handle font_resources = zest_CreateShaderResources(context);
	zest_AddUniformBufferToResources(font_resources, uniform_buffer);
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
    int atlas_layer_width = 512;
    int atlas_layer_height = 512;
    int atlas_channels = 4; 
	font.font_atlas = zest_CreateImageAtlasCollection(context, zest_format_r32g32b32a32_sfloat);
	font.size = font_size;

    float scale = stbtt_ScaleForMappingEmToPixels(&font_info, font_size);

	msdf_allocation_context_t allocation_context;
	allocation_context.ctx = context;
	allocation_context.alloc = zest__msdf_allocation;
	allocation_context.free = zest__msdf_free;

	zest_vec_resize(context->device->allocator, font.characters, 127 - 32);
	memset(font.characters, 0, zest_vec_size_in_bytes(font.characters));
	font.first_character_offset = 32;

    for (int char_code = 32; char_code < 127; ++char_code) {
        int glyph_index = stbtt_FindGlyphIndex(&font_info, char_code);
		zest_font_character_t* current_character = &font.characters[char_code - 32];

        msdf_result_t result;
        if (msdf_genGlyph(&result, &font_info, glyph_index, 4, scale, sdf_range, NULL)) {
			char character[2] = { (char)char_code, '\0' };
			int size = result.width * result.height * sizeof(float) * atlas_channels;
			zest_bitmap tmp_bitmap = zest_CreateBitmapFromRawBuffer(context, character, result.rgba, size, result.width, result.height, zest_format_r32g32b32a32_sfloat);
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
		} else {
			//Treat as whitespace
            int advance, lsb;
            stbtt_GetGlyphHMetrics(&font_info, glyph_index, &advance, &lsb);
            current_character->x_advance = scale * advance;
			ZEST__FLAG(current_character->flags, zest_character_flag_whitespace);
		}
    }

	zest_image_handle image_atlas = zest_CreateImageAtlas(font.font_atlas, atlas_layer_width, atlas_layer_height, zest_image_preset_texture);
	font.font_binding_index = zest_AcquireGlobalSampledImageIndex(image_atlas, zest_texture_array_binding);
	for (int i = 0; i != 95; ++i) {
		if (font.characters[i].region) {
			zest_BindAtlasRegionToImage(font.characters[i].region, font_sampler_binding_index, image_atlas, zest_texture_2d_binding);
		}
	}
    zest_FreeFile(context, (zest_file)font_buffer);
	font.context = context;
	/*
	font.settings.radius = 25.f;
	font.settings.bleed = .25f;
	font.settings.aa_factor = 5.f;
	font.settings.thickness = 5.5f;
	*/
	font.settings.unit_range = ZEST_STRUCT_LITERAL(zest_vec2, font.sdf_range / 512.f, font.sdf_range / 512.f);
	font.settings.in_bias = 0.f;
	font.settings.out_bias = 0.f;
	font.settings.supersample = 0.f;
	font.settings.smoothness = 0.f;
	font.settings.gamma = 1.f;
	return font;
}

void zest_FreeFont(zest_msdf_font_t *font) {
	if (font->characters) {
		zest_vec_free(font->context->device->allocator, font->characters);
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
	zest__set_layer_push_constants(layer, &font->settings, sizeof(zest_msdf_font_settings2_t));
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

	zest_uint max_character_index = zest_vec_size(font->characters);
	int start_offset = font->first_character_offset;
    for (zest_uint i = 0; i != length; ++i) {
		zest_uint character_index = (zest_uint)text[i] - start_offset;
		if (character_index >= (int)max_character_index) {
			continue;
		}
        zest_font_instance_t* font_instance = (zest_font_instance_t*)layer->memory_refs[layer->fif].instance_ptr;
        zest_font_character_t* character = &font->characters[character_index];

        if (character->flags > 0) {
            xpos += character->x_advance * size + letter_spacing;
            continue;
        }

        float width = character->width * size;
        float height = character->height * size;
        float xoffset = character->x_offset * size;
        float yoffset = character->y_offset * size;

		font_instance->size_handle = zest_Pack16bit4SScaledZWPacked(width, height, 0, 4096.f);
        font_instance->position = zest_Vec2Set(xpos + xoffset, y - yoffset);
        font_instance->rotation = 0.f;
        font_instance->uv = character->region->uv_packed;
        font_instance->color = layer->current_color;
		font_instance->intensity = layer->intensity;
		font_instance->texture_array = character->region->layer_index;
        layer->current_instruction.total_instances++;

        zest_NextInstance(layer);

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
#endif // MSDF_H