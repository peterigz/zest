//Zest - A Vulkan Pocket Renderer
#ifndef ZEST_RENDERER_H
#define ZEST_RENDERER_H

/*
    Zest Pocket Renderer

    Sections in this header file, you can search for the following keywords to jump to that section :

    [Macro_Defines]                     Just a bunch of typedefs and macro definitions
    [Typedefs_for_numbers]          
    [Platform_specific_code]            Windows/Mac specific, no linux yet
    [Shader_code]                       glsl shader code for all the built in shaders
    [Enums_and_flags]                   Enums and bit flag definitions
        [pipeline_enums]
    [Forward_declarations]              Forward declarations for structs and setting up of handles
    [Pocket_dynamic_array]              Simple dynamic array
    [Pocket_bucket_array]               Simple bucket array
    [Pocket_Hasher]                     XXHash code for use in hash map
    [Pocket_hash_map]                   Simple hash map
    [Pocket_text_buffer]                Very simple struct and functions for storing strings
    [Threading]                         Simple worker queue mainly used for recording secondary command buffers
    [Structs]                           All the structs are defined here.
        [Vectors]
        [frame_graph_types]
    [Internal_functions]                Private functions only, no API access
		[Platform_dependent_functions]
        [Buffer_and_Memory_Management]
        [Renderer_functions]
        [Draw_layer_internal_functions]
        [Image_internal_functions]
        [General_layer_internal_functions]
        [Mesh_layer_internal_functions]
        [General_Helper_Functions]
        [Frame_graph_platform_functions]
        [Pipeline_Helper_Functions]
        [Buffer_allocation_funcitons]
        [Maintenance_functions]
        [Descriptor_set_functions]
        [Device_set_up]
        [App_initialise_and_run_functions]
        [Window_related_functions]

    --API functions
    [Essential_setup_functions]         Functions for initialising Zest
    [Vulkan_Helper_Functions]           Just general helper functions for working with Vulkan if you're doing more custom stuff
    [Pipeline_related_vulkan_helpers]   Vulkan helper functions for setting up your own pipeline_templates
    [Platform_dependent_callbacks]      These are used depending one whether you're using glfw, sdl or just the os directly
    [Buffer_functions]                  Functions for creating and using gpu buffers
    [General_Math_helper_functions]     Vector and matrix math functions
    [Camera_helpers]                    Functions for setting up and using a camera including frustom and screen ray etc. 
    [Images_and_textures]               Load and setup images for using in textures accessed on the GPU
    [Swapchain_helpers]                 General swapchain helpers to get, set clear color etc.
    [Fonts]                             Basic functions for loading MSDF fonts
    [Main_loop_update_functions]        Only one function currently, for setting the current command queue to render with
    [Draw_Layers_API]                   General helper functions for layers
    [Draw_sprite_layers]                Functions for drawing the builtin sprite layer pipeline
    [Draw_billboard_layers]             Functions for drawing the builtin billboard layer pipeline
    [Draw_3D_Line_layers]               Functions for drawing the builtin 3d line layer pipeline
    [Draw_MSDF_font_layers]             Functions for drawing the builtin MSDF font layer pipeline
    [Draw_mesh_layers]                  Functions for drawing the builtin mesh layer pipeline
    [Draw_instance_mesh_layers]         Functions for drawing the builtin instance mesh layer pipeline
    [Compute_shaders]                   Functions for setting up your own custom compute shaders
    [Events_and_States]                 Just one function for now
    [Timer_functions]                   High resolution timer functions
    [Window_functions]                  Open/close and edit window modes/size
    [General_Helper_functions]          General API functions - get screen dimensions some vulkan helper functions
    [Debug_Helpers]                     Functions for debugging and outputting queues to the console.
    [Command_buffer_functions]          GPU command buffer commands
*/

#define ZEST_DEBUGGING

#ifdef __cplusplus
extern "C" {
#endif

#include <vulkan/vulkan.h>
#include <shaderc/shaderc.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <errno.h>

//Macro_Defines
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64)
#define ZEST_INTEL
#include <immintrin.h>
#elif defined(__arm__) || defined(__aarch64__)
#include <arm_neon.h>
#define ZEST_ARM
#endif

#ifndef ZEST_MAX_FIF
//The maximum number of frames in flight. If you're very tight on memory then 1 will use less resources.
#define ZEST_MAX_FIF 2
#endif

#ifndef ZEST_ASSERT
#define ZEST_ASSERT assert
#endif

#ifndef ZEST_API
#define ZEST_API
#endif

//I had to add this because some dell laptops have their own drivers that are basically broken. Even though the gpu signals that
//it can write direct it actually doesn't and just crashes. Maybe there's another way to detect or do a pre-test?
#define ZEST_DISABLE_GPU_DIRECT_WRITE 1

//Helper macros
#define ZEST__MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ZEST__MAX(a, b) (((a) > (b)) ? (a) : (b))
#define ZEST__CLAMP(v, min_v, max_v) ((v < min_v) ? min_v : ((v > max_v) ? max_v : v))
#define ZEST__POW2(x) ((x) && !((x) & ((x) - 1)))
#define ZEST__NEXTPOW2(x) pow(2, ceil(log(x)/log(2)));
#define ZEST__FLAG(flag, bit) flag |= (bit)
#define ZEST__MAYBE_FLAG(flag, bit, yesno) flag |= (yesno ? bit : 0)
#define ZEST__UNFLAG(flag, bit) flag &= ~bit
#define ZEST__FLAGGED(flag, bit) (flag & (bit)) > 0
#define ZEST__NOT_FLAGGED(flag, bit) (flag & (bit)) == 0
#define ZEST__MAKE_SUBMISSION_ID(wave_index, execution_index, queue_id) (queue_id << 24) | (wave_index << 16) | execution_index
#define ZEST__EXECUTION_INDEX(id) (id & 0xFFFF)
#define ZEST__SUBMISSION_INDEX(id) ((id & 0x00FF0000) >> 16)
#define ZEST__EXECUTION_ORDER_ID(id) (id & 0xFFFFFF)
#define ZEST__QUEUE_INDEX(id) ((id & 0xFF000000) >> 24)
#define ZEST__ALL_MIPS 0xFFFFFFFF
#define ZEST__ALL_LAYERS 0xFFFFFFFF
#define ZEST_QUEUE_FAMILY_IGNORED (~0U)

static const char *zest_message_pass_culled = "Pass [%s] culled because there were no outputs.";
static const char *zest_message_pass_culled_no_work = "Pass [%s] culled because there was no work found.";
static const char *zest_message_pass_culled_not_consumed = "Pass [%s] culled because it's output was not consumed by any subsequent passes. This won't happen if the ouput is an imported buffer or image. If the resource is transient then it will be discarded immediately once it has no further use. Also note that passes at the front of a chain can be culled if ultimately nothing consumes the output from the last pass in the chain.";
static const char *zest_message_cyclic_dependency = "Cyclic dependency detected in render graph [%s] with pass [%s]. You have a situation where  Pass A depends on Pass B's output, and Pass B depends on Pass A's output.";
static const char *zest_message_invalid_reference_counts = "Graph Compile Error in Frame Graph [%s]: When culling potential pass in reference counts should never go below 0. This could be a bug in the compile logic.";
static const char *zest_message_usage_has_no_resource = "Graph Compile Error in Frame Graph [%s]: A usage node did not have a valid resource node.";
static const char *zest_message_multiple_swapchain_usage = "Graph Compile Error in Frame Graph [%s]: This is the second time that the swap chain is used as output, this should not happen. Check that the passes that use the swapchain as output have exactly the same set of other ouputs so that the passes can be properly grouped together.";
static const char *zest_message_resource_added_as_ouput_more_than_once = "Graph Compile Error in Frame Graph [%s]: A resource should only have one producer in a valid graph. Check to make sure you haven't added the same output to a pass more than once";
static const char *zest_message_resource_should_be_imported = "Graph Compile Error in Frame Graph [%s]: ";

//Override this if you'd prefer a different way to allocate the pools for sub allocation in host memory.
#ifndef ZEST__ALLOCATE_POOL
#define ZEST__ALLOCATE_POOL malloc
#endif

#ifndef ZEST__FREE
#define ZEST__FREE(memory) zloc_Free(ZestDevice->allocator, memory)
#endif

#ifndef ZEST__REALLOCATE
#define ZEST__ALLOCATE(size) zest__allocate(size)
#define ZEST__ALLOCATE_ALIGNED(size, alignment) zest__allocate_aligned(size, alignment)
#define ZEST__REALLOCATE(ptr, size) zest__reallocate(ptr, size)
#endif

#define STBI_MALLOC(sz)           ZEST__ALLOCATE(sz)
#define STBI_REALLOC(p,newsz)     ZEST__REALLOCATE(p,newsz)
#define STBI_FREE(p)              ZEST__FREE(p)

#define ZLOC_ENABLE_REMOTE_MEMORY
#define ZLOC_THREAD_SAFE
#define ZLOC_EXTRA_DEBUGGING
#define ZLOC_OUTPUT_ERROR_MESSAGES
#include "lib_bundle.h"

#ifndef ZEST_WARNING_COLOR
#define ZEST_WARNING_COLOR "\033[38;5;208m"
#endif

#ifndef ZEST_NOTICE_COLOR
#define ZEST_NOTICE_COLOR "\033[0m"
#endif

#define ZEST_PRINT(message_f, ...) printf(message_f"\n", ##__VA_ARGS__)

#ifdef ZEST_OUTPUT_WARNING_MESSAGES
#include <stdio.h>
#define ZEST_PRINT_WARNING(message_f, ...) printf(message_f"\n\033[0m", __VA_ARGS__)
#else
#define ZEST_PRINT_WARNING(message_f, ...)
#endif

#ifdef ZEST_OUTPUT_NOTICE_MESSAGES
#include <stdio.h>
#define ZEST_PRINT_NOTICE(message_f, ...) printf(message_f"\n\033[0m", __VA_ARGS__)
#else
#define ZEST_PRINT_NOTICE(message_f, ...)
#endif

//#define ZEST_PRINT_FUNCTION ZEST_PRINT("%s %i", __FUNCTION__, __LINE__)
#define ZEST_PRINT_FUNCTION 

#define ZEST_NL u8"\n"
#define ZEST_TAB u8"\t"
#define ZEST_LOG(log_file, message, ...) if(log_file) fprintf(log_file, message, ##__VA_ARGS__)
#define ZEST_APPEND_LOG(log_path, message, ...) if(log_path) { FILE *log_file = zest__open_file(log_path, "a"); fprintf(log_file, message ZEST_NL, ##__VA_ARGS__); fclose(log_file); }

#define ZEST__ARRAY(name, type, count) type *name = ZEST__REALLOCATE(0, sizeof(type) * count)
//FIF = Frame in Flight
#define ZEST_FIF ZestDevice->current_fif
#define ZEST_MICROSECS_SECOND 1000000ULL
#define ZEST_MICROSECS_MILLISECOND 1000
#define ZEST_MILLISECONDS_IN_MICROSECONDS(millisecs) millisecs * ZEST_MICROSECS_MILLISECOND
#define ZEST_MICROSECONDS_TO_MILLISECONDS(microseconds) double(microseconds) / 1000.0
#define ZEST_SECONDS_IN_MICROSECONDS(seconds) seconds * ZEST_MICROSECS_SECOND
#define ZEST_SECONDS_IN_MILLISECONDS(seconds) seconds * 1000
#define ZEST_PI 3.14159265359f
#define ZEST_FIXED_LOOP_BUFFER 0xF18

#ifndef ZEST_MAX_DEVICE_MEMORY_POOLS
#define ZEST_MAX_DEVICE_MEMORY_POOLS 64
#endif

#define ZEST_NULL 0
//For each frame in flight macro
#define zest_ForEachFrameInFlight(index) for(unsigned int index = 0; index != ZEST_MAX_FIF; ++index)

//For error checking vulkan commands
#define ZEST_VK_ASSERT_RESULT(res) do {                                                                                \
    ZestRenderer->backend->last_result = (res);                                                                                 \
    if (ZestRenderer->backend->last_result != VK_SUCCESS) {                                                                     \
        zest__log_vulkan_error(ZestRenderer->backend->last_result, __FILE__, __LINE__);                                         \
        return ZestRenderer->backend->last_result;                                                                              \
    }                                                                                                                  \
} while(0)

#define ZEST_RETURN_FALSE_ON_FAIL(res)                                                                                 \
    ZestRenderer->backend->last_result = res;                                                                                   \
    if (ZestRenderer->backend->last_result != VK_SUCCESS) {                                                                     \
        return ZEST_FALSE;                                                                                             \
    }
 

#define ZEST_RETURN_RESULT_ON_FAIL(res) do {                                                                           \
    ZestRenderer->backend->last_result = (res);                                                                                 \
    if (ZestRenderer->backend->last_result != VK_SUCCESS) {                                                                     \
        return ZestRenderer->backend->last_result;                                                                              \
    }                                                                                                                  \
} while(0)

#define ZEST_CLEANUP_ON_FALSE(res) do {                                                                           \
    if ((res) == ZEST_FALSE) {                                                                     \
        goto cleanup;                                                                              \
    }                                                                                                                  \
} while(0)
 
#define ZEST_CLEANUP_ON_FAIL(res) do {                                                                                 \
    ZestRenderer->backend->last_result = (res);                                                                                 \
    if (ZestRenderer->backend->last_result != VK_SUCCESS) {                                                                     \
        goto cleanup;                                                                                                  \
    }                                                                                                                  \
} while(0)

#define ZEST_VK_LOG(res) do {                                                                                          \
    ZestRenderer->backend->last_result = (res);                                                                                 \
    if (ZestRenderer->backend->last_result != VK_SUCCESS) {                                                                     \
        zest__log_vulkan_error(ZestRenderer->backend->last_result, __FILE__, __LINE__);                                         \
    }                                                                                                                  \
} while(0)

#define ZEST_VK_PRINT_RESULT(result) do {                                                                              \
    if (result != VK_SUCCESS) {                                                                                        \
        zest__log_vulkan_error(result, __FILE__, __LINE__);                                                            \
    }                                                                                                                  \
} while(0)

void zest__log_vulkan_error(VkResult result, const char *file, int line);
const char *zest__vulkan_error_string(VkResult errorCode);

//Typedefs_for_numbers

typedef unsigned int zest_uint;
typedef unsigned int zest_handle;
typedef int zest_index;
typedef unsigned long long zest_ull;
typedef uint16_t zest_u16;
typedef uint64_t zest_u64;
typedef zest_uint zest_id;
typedef zest_uint zest_millisecs;
typedef zest_uint zest_thread_access;
typedef zest_ull zest_microsecs;
typedef zest_ull zest_key;
typedef size_t zest_size;
typedef unsigned char zest_byte;
typedef unsigned int zest_bool;

//Handles. These are pointers that remain stable until the object is freed.
#define ZEST__MAKE_HANDLE(handle) typedef struct handle##_t* handle;

#define ZEST__MAKE_USER_HANDLE(handle) typedef struct { zest_handle value; } handle##_handle;
#define ZEST_HANDLE_INDEX(handle) (handle & 0xFFFF)
#define ZEST_HANDLE_GENERATION(handle) ((handle & 0xFFFF0000) >> 16)

#define ZEST_CREATE_HANDLE(generation, index) ((generation << 16) + index)

//For allocating a new object with handle. Only used internally.
#define ZEST__NEW(type) ZEST__ALLOCATE(sizeof(type##_t))
#define ZEST__NEW_ALIGNED(type, alignment) ZEST__ALLOCATE_ALIGNED(sizeof(type##_t), alignment)

//For global variables
#define ZEST_GLOBAL static
//For internal private functions that aren't meant to be called outside of the library
//If there's a case for where it would be useful be able to call a function externally
//then we change it to a ZEST_API function
#define ZEST_PRIVATE static

typedef union zest_packed10bit
{
    struct
    {
        int x : 10;
        int y : 10;
        int z : 10;
        int w : 2;
    } data;
    zest_uint pack;
} zest_packed10bit;

typedef union zest_packed8bit
{
    struct
    {
        int x : 8;
        int y : 8;
        int z : 8;
        int w : 8;
    } data;
    zest_uint pack;
} zest_packed8bit;

/* Platform_specific_code*/

FILE *zest__open_file(const char *file_name, const char *mode);
ZEST_API zest_millisecs zest_Millisecs(void);
ZEST_API zest_microsecs zest_Microsecs(void);

#if defined (_WIN32)
#include <windows.h>
#include "vulkan/vulkan_win32.h"
#include <direct.h>
#define zest_snprintf(buffer, bufferSize, format, ...) sprintf_s(buffer, bufferSize, format, __VA_ARGS__)
#define zest_strcat(left, size, right) strcat_s(left, size, right)
#define zest_strcpy(left, size, right) strcpy_s(left, size, right)
#define zest_strerror(buffer, size, error) strerror_s(buffer, size, error)
#define ZEST_ALIGN_PREFIX(v) __declspec(align(v))
#define ZEST_ALIGN_AFFIX(v)
#define ZEST_PROTOTYPE
ZEST_PRIVATE inline zest_thread_access zest__compare_and_exchange(volatile zest_thread_access* target, zest_thread_access value, zest_thread_access original) {
    return InterlockedCompareExchange((LONG*)target, value, original);
}

//Window creation
ZEST_PRIVATE HINSTANCE zest_window_instance;
#define ZEST_WINDOW_INSTANCE HINSTANCE
LRESULT CALLBACK zest__window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
//--
#define ZEST_CREATE_DIR(path) _mkdir(path)

#define zest__writebarrier _WriteBarrier();
#define zest__readbarrier _ReadBarrier();

#elif defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)) && (defined(__i386__) || defined(__x86_64__)) || defined(__clang__)
#include <time.h>
//We'll just use glfw on mac for now. Can maybe add basic cocoa windows later
#include <GLFW/glfw3.h>
#include <sys/stat.h>
#include <sys/types.h>
#define ZEST_ALIGN_PREFIX(v)
#define ZEST_ALIGN_AFFIX(v)  __attribute__((aligned(16)))
#define ZEST_PROTOTYPE void
#define zest_snprintf(buffer, bufferSize, format, ...) snprintf(buffer, bufferSize, format, __VA_ARGS__)
#define zest_strcat(left, size, right) strcat(left, right)
#define zest_strcpy(left, size, right) strcpy(left, right)
#define zest_strerror(buffer, size, error) strerror_r(buffer, size, error)
ZEST_PRIVATE inline zest_thread_access zest__compare_and_exchange(volatile zest_thread_access* target, zest_thread_access value, zest_thread_access original) {
    return __sync_val_compare_and_swap(target, original, value);
}
#define ZEST_CREATE_DIR(path) mkdir(path, 0777)
#define zest__writebarrier __asm__ __volatile__ ("" : : : "memory");
#define zest__readbarrier __asm__ __volatile__ ("" : : : "memory");

//Window creation
//--

#endif
/*end of platform specific code*/
#ifdef __APPLE__
#define ZEST_PORTABILITY_ENUMERATION
#endif

#define ZEST_TRUE 1
#define ZEST_FALSE 0
#define ZEST_INVALID 0xFFFFFFFF
#define ZEST_NOT_BINDLESS 0xFFFFFFFF
#define ZEST_PERSISTENT 0xFFFFFFFF
#define ZEST_U32_MAX_VALUE ((zest_uint)-1)

//Shader_code
//For nicer formatting of the shader code, but note that new lines are ignored when this becomes an actual string.
#define ZEST_GLSL(version, shader) "#version " #version "\n" "#extension GL_EXT_nonuniform_qualifier : require\n" #shader

//----------------------
//2d/3d billboard fragment shader
//----------------------
static const char *zest_shader_sprite_frag = ZEST_GLSL(450,
layout(location = 0) in vec4 in_frag_color;
layout(location = 1) in vec3 in_tex_coord;
layout(location = 0) out vec4 outColor;
layout(set = 1, binding = 0) uniform sampler2DArray texSampler[];

layout(push_constant) uniform quad_index
{
    uint index1;
    uint index2;
    uint index3;
    uint index4;
    vec4 parameters1;
    vec4 parameters2;
    vec4 parameters3;
    vec4 camera;
} pc;

void main() {
    vec4 texel = texture(texSampler[pc.index1], in_tex_coord);
    //Pre multiply alpha
    outColor.rgb = texel.rgb * in_frag_color.rgb * texel.a;
    //If in_frag_color.a is 0 then color will be additive. The higher the value of a the more alpha blended the color will be.
    outColor.a = texel.a * in_frag_color.a;
}
);

//----------------------
//2d sprite vertex shader
//----------------------
static const char *zest_shader_sprite_vert = ZEST_GLSL(450,
//Quad indexes
const int indexes[6] = int[6]( 0, 1, 2, 2, 1, 3 );
const float size_max_value = 4096.0 / 32767.0;
const float handle_max_value = 128.0 / 32767.0;

layout(set = 0, binding = 0) uniform UboView
{
    mat4 view;
    mat4 proj;
    vec4 parameters1;
    vec4 parameters2;
    vec2 res;
    uint millisecs;
} uboView;

layout(push_constant) uniform quad_index
{
    uint index1;
    uint index2;
    uint index3;
    uint index4;
    vec4 parameters1;
    vec4 parameters2;
    vec4 parameters3;
    vec4 camera;
} pc;

//Vertex
//layout(location = 0) in vec2 vertex_position;

//Instance
layout(location = 0) in vec4 uv;
layout(location = 1) in vec4 position_rotation;
layout(location = 2) in vec4 size_handle;
layout(location = 3) in vec2 alignment;
layout(location = 4) in vec4 in_color;
layout(location = 5) in uint intensity_texture_array;

layout(location = 0) out vec4 out_frag_color;
layout(location = 1) out vec3 out_tex_coord;

void main() {
    //Pluck out the intensity value from 
    float intensity = intensity_texture_array & uint(0x003fffff);
    intensity /= 524287.875;
    //Scale the size and handle to the correct values
    vec2 size = size_handle.xy * size_max_value;
    vec2 handle = size_handle.zw * handle_max_value;

    vec2 alignment_normal = normalize(vec2(alignment.x, alignment.y + 0.000001)) * position_rotation.z;

    vec2 uvs[4];
    uvs[0].x = uv.x; uvs[0].y = uv.y;
    uvs[1].x = uv.z; uvs[1].y = uv.y;
    uvs[2].x = uv.x; uvs[2].y = uv.w;
    uvs[3].x = uv.z; uvs[3].y = uv.w;

    vec2 bounds[4];
    bounds[0].x = size.x * (0 - handle.x);
    bounds[0].y = size.y * (0 - handle.y);
    bounds[3].x = size.x * (1 - handle.x);
    bounds[3].y = size.y * (1 - handle.y);
    bounds[1].x = bounds[3].x;
    bounds[1].y = bounds[0].y;
    bounds[2].x = bounds[0].x;
    bounds[2].y = bounds[3].y;

    int index = indexes[gl_VertexIndex];

    vec2 vertex_position = bounds[index];

    mat3 matrix = mat3(1.0);
    float s = sin(position_rotation.w);
    float c = cos(position_rotation.w);

    matrix[0][0] = c;
    matrix[0][1] = s;
    matrix[1][0] = -s;
    matrix[1][1] = c;

    mat4 modelView = uboView.view;
    vec3 pos = matrix * vec3(vertex_position.x, vertex_position.y, 1);
    pos.xy += alignment_normal * dot(pos.xy, alignment_normal);
    pos.xy += position_rotation.xy;
    gl_Position = uboView.proj * modelView * vec4(pos, 1.0);

    //----------------
    out_tex_coord = vec3(uvs[index], (intensity_texture_array & uint(0xFF000000)) >> 24);
    out_frag_color = in_color * intensity;
}
);

//----------------------
//2d font frag shader
//----------------------
static const char *zest_shader_font_frag = ZEST_GLSL(450,
layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec3 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2DArray texture_sampler[];

layout(push_constant) uniform quad_index
{
    vec4 shadow_color;
    vec2 shadow_vector;
    float shadow_smoothing;
    float shadow_clipping;
    float radius;
    float bleed;
    float aa_factor;
    float thickness;
    uint font_texture_index;
} font;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

vec4 blend(vec4 src, vec4 dst, float alpha) {
    // src OVER dst porter-duff blending
    float a = src.a + dst.a * (1.0 - src.a);
    vec3 rgb = (src.a * src.rgb + dst.a * dst.rgb * (1.0 - src.a)) / (a == 0.0 ? 1.0 : a);
    return vec4(rgb, a * alpha);
}

float linearstep(float a, float b, float x) {
    return clamp((x - a) / (b - a), 0.0, 1.0);
}

float get_uv_scale(vec2 uv) {
    vec2 dx = dFdx(uv);
    vec2 dy = dFdy(uv);
    return (length(dx) + length(dy)) * 0.5;
}

void main() {

    vec4 glyph = frag_color;
    float opacity;
    vec4 sampled = texture(texture_sampler[font.font_texture_index], frag_tex_coord);

    vec2 texture_size = textureSize(texture_sampler[font.font_texture_index], 0).xy;
    float scale = get_uv_scale(frag_tex_coord.xy * texture_size) * font.aa_factor;
    float d = (median(sampled.r, sampled.g, sampled.b) - 0.75) * font.radius;
    float sdf = (d + font.thickness) / scale + 0.5 + font.bleed;
    float mask = clamp(sdf, 0.0, 1.0);
    glyph = vec4(glyph.rgb, glyph.a * mask);

    if (font.shadow_color.a > 0) {
        float sd = texture(texture_sampler[font.font_texture_index], vec3(frag_tex_coord.xy - font.shadow_vector / texture_size.xy, 0)).a;
        float shadowAlpha = linearstep(0.5 - font.shadow_smoothing, 0.5 + font.shadow_smoothing, sd) * font.shadow_color.a;
        shadowAlpha *= 1.0 - mask * font.shadow_clipping;
        vec4 shadow = vec4(font.shadow_color.rgb, shadowAlpha);
        out_color = blend(blend(vec4(0), glyph, 1.0), shadow, frag_color.a);
        out_color.rgb = out_color.rgb * out_color.a;
    }
    else {
        out_color.rgb = glyph.rgb * glyph.a;
        out_color.a = glyph.a;
    }

}
);

//----------------------
//mesh vert shader
//----------------------
static const char *zest_shader_mesh_vert = ZEST_GLSL(450,
//Blendmodes
layout(set = 0, binding = 0) uniform ubo_view
{
    mat4 view;
    mat4 proj;
    vec4 parameters1;
    vec4 parameters2;
    vec2 res;
    uint millisecs;
} uboView;

layout(push_constant) uniform parameters
{
    mat4 model;
    vec4 parameters1;
    vec4 parameters2;
    vec4 parameters3;
    vec4 camera;
} pc;

layout(location = 0) in vec3 in_position;
layout(location = 1) in float in_intensity;
layout(location = 2) in vec2 in_tex_coord;
layout(location = 3) in vec4 in_color;
layout(location = 4) in uint in_texture_array_index;

layout(location = 0) out vec4 out_frag_color;
layout(location = 1) out vec3 out_tex_coord;

void main() {
    gl_Position = uboView.proj * uboView.view * vec4(in_position.xyz, 1.0);

    out_tex_coord = vec3(in_tex_coord, in_texture_array_index);

    out_frag_color = in_color * in_intensity;
}
);

//----------------------
//mesh instance (for primatives) vert shader
//----------------------
static const char *zest_shader_mesh_instance_vert = ZEST_GLSL(450,
layout(binding = 0) uniform ubo_view
{
    mat4 view;
    mat4 proj;
    vec4 parameters1;
    vec4 parameters2;
    vec2 res;
    uint millisecs;
} uboView;

layout(push_constant) uniform parameters
{
    uint index1;
    uint index2;
    uint index3;
    uint index4;
    vec4 parameters1;
    vec4 parameters2;
    vec4 parameters3;
    vec4 camera;
} pc;

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec4 vertex_color;
layout(location = 2) in uint group_id;
layout(location = 3) in vec3 vertex_normal;
layout(location = 4) in vec3 instance_position;
layout(location = 5) in vec4 instance_color;
layout(location = 6) in vec3 instance_rotation;
layout(location = 7) in vec4 instance_parameters;
layout(location = 8) in vec3 instance_scale;

layout(location = 0) out vec4 out_frag_color;

void main() {
    mat3 mx;
    mat3 my;
    mat3 mz;

    // rotate around x
    float s = sin(instance_rotation.x);
    float c = cos(instance_rotation.x);

    mx[0] = vec3(c, s, 0.0);
    mx[1] = vec3(-s, c, 0.0);
    mx[2] = vec3(0.0, 0.0, 1.0);

    // rotate around y
    s = sin(instance_rotation.y);
    c = cos(instance_rotation.y);

    my[0] = vec3(c, 0.0, s);
    my[1] = vec3(0.0, 1.0, 0.0);
    my[2] = vec3(-s, 0.0, c);

    // rot around z
    s = sin(instance_rotation.z);
    c = cos(instance_rotation.z);

    mz[0] = vec3(1.0, 0.0, 0.0);
    mz[1] = vec3(0.0, c, s);
    mz[2] = vec3(0.0, -s, c);

    mat3 rotation_matrix = mz * my * mx;
    vec3 position = vertex_position * instance_scale * rotation_matrix + instance_position;
    gl_Position = (uboView.proj * uboView.view * vec4(position, 1.0));

    out_frag_color = vertex_color * instance_color;
}
);

//----------------------
//mesh instance (for primatives) frag shader
//----------------------
static const char *zest_shader_mesh_instance_frag = ZEST_GLSL(450,
layout(location = 0) in vec4 in_frag_color;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform quad_index
{
    uint index1;
    uint index2;
    uint index3;
    uint index4;
    vec4 parameters1;
    vec4 parameters2;
    vec4 parameters3;
    vec4 camera;
} pc;

void main() {
    outColor = in_frag_color;
}
);

//----------------------
//Swap chain vert shader
//----------------------
static const char *zest_shader_swap_vert = ZEST_GLSL(450,
layout(location = 0) out vec2 outUV;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
}
);

//----------------------
//Swap chain frag shader
//----------------------
static const char *zest_shader_swap_frag = ZEST_GLSL(450,
layout(set = 0, binding = 0) uniform sampler2D samplerColor;
layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;
void main(void)
{
    outFragColor = texture(samplerColor, inUV);
}
);

//Enums_and_flags
typedef enum zest_platform_type {
    zest_platform_vulkan,
    zest_platform_d3d12,
    zest_platform_metal,
} zest_platform_type;

typedef enum zest_texture_format {
    zest_format_undefined = 0,
    zest_format_r4g4_unorm_pack8 = 1,
    zest_format_r4g4b4a4_unorm_pack16,
    zest_format_b4g4r4a4_unorm_pack16,
    zest_format_r5g6b5_unorm_pack16,
    zest_format_b5g6r5_unorm_pack16,
    zest_format_r5g5b5a1_unorm_pack16,
    zest_format_b5g5r5a1_unorm_pack16,
    zest_format_a1r5g5b5_unorm_pack16,
    zest_format_r8_unorm,
    zest_format_r8_snorm,
    zest_format_r8_uint = 13,
    zest_format_r8_sint,
    zest_format_r8_srgb,
    zest_format_r8g8_unorm,
    zest_format_r8g8_snorm,
    zest_format_r8g8_uint = 20,
    zest_format_r8g8_sint,
    zest_format_r8g8_srgb,
    zest_format_r8g8b8_unorm,
    zest_format_r8g8b8_snorm,
    zest_format_r8g8b8_uint = 27,
    zest_format_r8g8b8_sint,
    zest_format_r8g8b8_srgb,
    zest_format_b8g8r8_unorm,
    zest_format_b8g8r8_snorm,
    zest_format_b8g8r8_uint = 34,
    zest_format_b8g8r8_sint,
    zest_format_b8g8r8_srgb,
    zest_format_r8g8b8a8_unorm,
    zest_format_r8g8b8a8_snorm,
    zest_format_r8g8b8a8_uint = 41,
    zest_format_r8g8b8a8_sint,
    zest_format_r8g8b8a8_srgb,
    zest_format_b8g8r8a8_unorm,
    zest_format_b8g8r8a8_snorm,
    zest_format_b8g8r8a8_uint = 48,
    zest_format_b8g8r8a8_sint,
    zest_format_b8g8r8a8_srgb,
    zest_format_a8b8g8r8_unorm_pack32,
    zest_format_a8b8g8r8_snorm_pack32,
    zest_format_a8b8g8r8_uint_pack32 = 55,
    zest_format_a8b8g8r8_sint_pack32,
    zest_format_a8b8g8r8_srgb_pack32,
    zest_format_a2r10g10b10_unorm_pack32,
    zest_format_a2r10g10b10_snorm_pack32,
    zest_format_a2r10g10b10_uint_pack32 = 62,
    zest_format_a2r10g10b10_sint_pack32,
    zest_format_a2b10g10r10_unorm_pack32,
    zest_format_a2b10g10r10_snorm_pack32,
    zest_format_a2b10g10r10_uint_pack32 = 68,
    zest_format_a2b10g10r10_sint_pack32,
    zest_format_r16_unorm,
    zest_format_r16_snorm,
    zest_format_r16_uint = 74,
    zest_format_r16_sint,
    zest_format_r16_sfloat,
    zest_format_r16g16_unorm,
    zest_format_r16g16_snorm,
    zest_format_r16g16_uint = 81,
    zest_format_r16g16_sint,
    zest_format_r16g16_sfloat,
    zest_format_r16g16b16_unorm,
    zest_format_r16g16b16_snorm,
    zest_format_r16g16b16_uint = 88,
    zest_format_r16g16b16_sint,
    zest_format_r16g16b16_sfloat,
    zest_format_r16g16b16a16_unorm,
    zest_format_r16g16b16a16_snorm,
    zest_format_r16g16b16a16_uint = 95,
    zest_format_r16g16b16a16_sint,
    zest_format_r16g16b16a16_sfloat,
    zest_format_r32_uint,
    zest_format_r32_sint,
    zest_format_r32_sfloat,
    zest_format_r32g32_uint,
    zest_format_r32g32_sint,
    zest_format_r32g32_sfloat,
    zest_format_r32g32b32_uint,
    zest_format_r32g32b32_sint,
    zest_format_r32g32b32_sfloat,
    zest_format_r32g32b32a32_uint,
    zest_format_r32g32b32a32_sint,
    zest_format_r32g32b32a32_sfloat,
    zest_format_r64_uint,
    zest_format_r64_sint,
    zest_format_r64_sfloat,
    zest_format_r64g64_uint,
    zest_format_r64g64_sint,
    zest_format_r64g64_sfloat,
    zest_format_r64g64b64_uint,
    zest_format_r64g64b64_sint,
    zest_format_r64g64b64_sfloat,
    zest_format_r64g64b64a64_uint,
    zest_format_r64g64b64a64_sint,
    zest_format_r64g64b64a64_sfloat,
    zest_format_b10g11r11_ufloat_pack32,
    zest_format_e5b9g9r9_ufloat_pack32,
    zest_format_d16_unorm,
    zest_format_x8_d24_unorm_pack32,
    zest_format_d32_sfloat,
    zest_format_s8_uint,
    zest_format_d16_unorm_s8_uint,
    zest_format_d24_unorm_s8_uint,
    zest_format_d32_sfloat_s8_uint,
    zest_format_bc1_rgb_unorm_block,
    zest_format_bc1_rgb_srgb_block,
    zest_format_bc1_rgba_unorm_block,
    zest_format_bc1_rgba_srgb_block,
    zest_format_bc2_unorm_block,
    zest_format_bc2_srgb_block,
    zest_format_bc3_unorm_block,
    zest_format_bc3_srgb_block,
    zest_format_bc4_unorm_block,
    zest_format_bc4_snorm_block,
    zest_format_bc5_unorm_block,
    zest_format_bc5_snorm_block,
    zest_format_bc6h_ufloat_block,
    zest_format_bc6h_sfloat_block,
    zest_format_bc7_unorm_block,
    zest_format_bc7_srgb_block,
    zest_format_etc2_r8g8b8_unorm_block,
    zest_format_etc2_r8g8b8_srgb_block,
    zest_format_etc2_r8g8b8a1_unorm_block,
    zest_format_etc2_r8g8b8a1_srgb_block,
    zest_format_etc2_r8g8b8a8_unorm_block,
    zest_format_etc2_r8g8b8a8_srgb_block,
    zest_format_eac_r11_unorm_block,
    zest_format_eac_r11_snorm_block,
    zest_format_eac_r11g11_unorm_block,
    zest_format_eac_r11g11_snorm_block,
    zest_format_astc_4X4_unorm_block,
    zest_format_astc_4X4_srgb_block,
    zest_format_astc_5X4_unorm_block,
    zest_format_astc_5X4_srgb_block,
    zest_format_astc_5X5_unorm_block,
    zest_format_astc_5X5_srgb_block,
    zest_format_astc_6X5_unorm_block,
    zest_format_astc_6X5_srgb_block,
    zest_format_astc_6X6_unorm_block,
    zest_format_astc_6X6_srgb_block,
    zest_format_astc_8X5_unorm_block,
    zest_format_astc_8X5_srgb_block,
    zest_format_astc_8X6_unorm_block,
    zest_format_astc_8X6_srgb_block,
    zest_format_astc_8X8_unorm_block,
    zest_format_astc_8X8_srgb_block,
    zest_format_astc_10X5_unorm_block,
    zest_format_astc_10X5_srgb_block,
    zest_format_astc_10X6_unorm_block,
    zest_format_astc_10X6_srgb_block,
    zest_format_astc_10X8_unorm_block,
    zest_format_astc_10X8_srgb_block,
    zest_format_astc_10X10_unorm_block,
    zest_format_astc_10X10_srgb_block,
    zest_format_astc_12X10_unorm_block,
    zest_format_astc_12X10_srgb_block,
    zest_format_astc_12X12_unorm_block,
    zest_format_astc_12X12_srgb_block = 184,
    zest_format_depth = -1,
} zest_texture_format;

typedef enum {
    zest_image_usage_transfer_src_bit             = 0x00000001,
    zest_image_usage_transfer_dst_bit             = 0x00000002,
    zest_image_usage_sampled_bit                  = 0x00000004,
    zest_image_usage_storage_bit                  = 0x00000008,
    zest_image_usage_color_attachment_bit         = 0x00000010,
    zest_image_usage_depth_stencil_attachment_bit = 0x00000020,
    zest_image_usage_transient_attachment_bit     = 0x00000040,
    zest_image_usage_input_attachment_bit         = 0x00000080,
    zest_image_usage_host_transfer_bit            = 0x00400000,
} zest_image_usage_flag_bits;

typedef zest_uint zest_image_usage_flags;

typedef enum {
    zest_buffer_usage_transfer_src_bit          = 0x00000001,
    zest_buffer_usage_transfer_dst_bit          = 0x00000002,
    zest_buffer_usage_uniform_texel_buffer_bit  = 0x00000004,
    zest_buffer_usage_storage_texel_buffer_bit  = 0x00000008,
    zest_buffer_usage_uniform_buffer_bit        = 0x00000010,
    zest_buffer_usage_storage_buffer_bit        = 0x00000020,
    zest_buffer_usage_index_buffer_bit          = 0x00000040,
    zest_buffer_usage_vertex_buffer_bit         = 0x00000080,
    zest_buffer_usage_indirect_buffer_bit       = 0x00000100,
    zest_buffer_usage_shader_device_address_bit = 0x00020000,
} zest_buffer_usage_flag_bits;

typedef enum zest_resource_state {
	zest_resource_state_undefined,

    //General states
	zest_resource_state_shader_read,          // For reading in any shader stage (texture, SRV)
	zest_resource_state_unordered_access,     // For read/write in a shader (UAV)
	zest_resource_state_copy_src,
	zest_resource_state_copy_dst,

    //Images
	zest_resource_state_render_target,        // For use as a color attachment
	zest_resource_state_depth_stencil_write,  // For use as a depth/stencil attachment
	zest_resource_state_depth_stencil_read,   // For read-only depth/stencil
	zest_resource_state_present,              // For presenting to the screen

    //Buffers
    zest_resource_state_index_buffer,
    zest_resource_state_vertex_buffer,
    zest_resource_state_uniform_buffer,
    zest_resource_state_indirect_argument,
} zest_resource_state;

typedef zest_uint zest_buffer_usage_flags;

typedef enum {
	zest_memory_property_device_local_bit     = 0x00000001,
	zest_memory_property_host_visible_bit     = 0x00000002,
	zest_memory_property_host_coherent_bit    = 0x00000004,
	zest_memory_property_host_cached_bit      = 0x00000008,
	zest_memory_property_lazily_allocated_bit = 0x00000010,
	zest_memory_property_protected_bit        = 0x00000020,
} zest_memory_property_flag_bits;

typedef zest_uint zest_memory_property_flags;

typedef enum {
    zest_filter_nearest,
    zest_filter_linear,
} zest_filter_type;

typedef enum {
    zest_mipmap_mode_nearest,
    zest_mipmap_mode_linear,
} zest_mipmap_mode;

typedef enum {
    zest_sampler_address_mode_repeat,
    zest_sampler_address_mode_mirrored_repeat,
    zest_sampler_address_mode_clamp_to_edge,
    zest_sampler_address_mode_clamp_to_border,
    zest_sampler_address_mode_mirror_clamp_to_edge,
} zest_sampler_address_mode;

typedef enum zest_border_color {
    zest_border_color_float_transparent_black, // Black, (0,0,0,0)
    zest_border_color_int_transparent_black,
    zest_border_color_float_opaque_black,      // Black, (0,0,0,1)
    zest_border_color_int_opaque_black,
    zest_border_color_float_opaque_white,      // White, (1,1,1,1)
    zest_border_color_int_opaque_white
} zest_border_color;

typedef enum {
    zest_compare_op_never,
    zest_compare_op_less,
    zest_compare_op_equal,
    zest_compare_op_less_or_equal,
    zest_compare_op_greater,
    zest_compare_op_not_equal,
    zest_compare_op_greater_or_equal,
    zest_compare_op_always,
} zest_compare_op;

typedef enum {
    zest_image_tiling_optimal,
    zest_image_tiling_linear,
} zest_image_tiling;

typedef enum {
    zest_sample_count_1_bit       = 0x00000001,
    zest_sample_count_2_bit       = 0x00000002,
    zest_sample_count_4_bit       = 0x00000004,
    zest_sample_count_8_bit       = 0x00000008,
    zest_sample_count_16_bit      = 0x00000010,
    zest_sample_count_32_bit      = 0x00000020,
    zest_sample_count_64_bit      = 0x00000040,
} zest_sample_count_bits;

typedef enum {
    zest_image_aspect_none        = 0,
    zest_image_aspect_color_bit   = 0x00000001,
    zest_image_aspect_depth_bit   = 0x00000002,
    zest_image_aspect_stencil_bit = 0x00000004,
} zest_image_aspect_flag_bits;

typedef enum {
    zest_image_view_type_1d = 0,
    zest_image_view_type_2d = 1,
    zest_image_view_type_3d = 2,
    zest_image_view_type_cube = 3,
    zest_image_view_type_1d_array = 4,
    zest_image_view_type_2d_array = 5,
    zest_image_view_type_cube_array = 6,
} zest_image_view_type;

typedef enum {
    zest_image_layout_undefined                                  = 0,
    zest_image_layout_general                                    = 1,
    zest_image_layout_color_attachment_optimal                   = 2,
    zest_image_layout_depth_stencil_attachment_optimal           = 3,
    zest_image_layout_depth_stencil_read_only_optimal            = 4,
    zest_image_layout_shader_read_only_optimal                   = 5,
    zest_image_layout_transfer_src_optimal                       = 6,
    zest_image_layout_transfer_dst_optimal                       = 7,
    zest_image_layout_preinitialized                             = 8,
    zest_image_layout_depth_read_only_stencil_attachment_optimal = 1000117000,
    zest_image_layout_depth_attachment_stencil_read_only_optimal = 1000117001,
    zest_image_layout_depth_attachment_optimal                   = 1000241000,
    zest_image_layout_depth_read_only_optimal                    = 1000241001,
    zest_image_layout_stencil_attachment_optimal                 = 1000241002,
    zest_image_layout_stencil_read_only_optimal                  = 1000241003,
    zest_image_layout_read_only_optimal                          = 1000314000,
    zest_image_layout_attachment_optimal                         = 1000314001,
    zest_image_layout_present                                    = 1000001002,
} zest_image_layout;

typedef enum {
    zest_access_none                               = 0,
    zest_access_indirect_command_read_bit          = 0x00000001,
    zest_access_index_read_bit                     = 0x00000002,
    zest_access_vertex_attribute_read_bit          = 0x00000004,
    zest_access_uniform_read_bit                   = 0x00000008,
    zest_access_input_attachment_read_bit          = 0x00000010,
    zest_access_shader_read_bit                    = 0x00000020,
    zest_access_shader_write_bit                   = 0x00000040,
    zest_access_color_attachment_read_bit          = 0x00000080,
    zest_access_color_attachment_write_bit         = 0x00000100,
    zest_access_depth_stencil_attachment_read_bit  = 0x00000200,
    zest_access_depth_stencil_attachment_write_bit = 0x00000400,
    zest_access_transfer_read_bit                  = 0x00000800,
    zest_access_transfer_write_bit                 = 0x00001000,
    zest_access_host_read_bit                      = 0x00002000,
    zest_access_host_write_bit                     = 0x00004000,
    zest_access_memory_read_bit                    = 0x00008000,
    zest_access_memory_write_bit                   = 0x00010000,
} zest_access_flag_bits;

typedef enum {
	zest_pipeline_stage_top_of_pipe_bit                    = 0x00000001,
	zest_pipeline_stage_draw_indirect_bit                  = 0x00000002,
	zest_pipeline_stage_vertex_input_bit                   = 0x00000004,
	zest_pipeline_stage_vertex_shader_bit                  = 0x00000008,
	zest_pipeline_stage_tessellation_control_shader_bit    = 0x00000010,
	zest_pipeline_stage_tessellation_evaluation_shader_bit = 0x00000020,
	zest_pipeline_stage_geometry_shader_bit                = 0x00000040,
	zest_pipeline_stage_fragment_shader_bit                = 0x00000080,
	zest_pipeline_stage_early_fragment_tests_bit           = 0x00000100,
	zest_pipeline_stage_late_fragment_tests_bit            = 0x00000200,
	zest_pipeline_stage_color_attachment_output_bit        = 0x00000400,
	zest_pipeline_stage_compute_shader_bit                 = 0x00000800,
	zest_pipeline_stage_transfer_bit                       = 0x00001000,
	zest_pipeline_stage_bottom_of_pipe_bit                 = 0x00002000,
	zest_pipeline_stage_host_bit                           = 0x00004000,
	zest_pipeline_stage_all_graphics_bit                   = 0x00008000,
	zest_pipeline_stage_all_commands_bit                   = 0x00010000,
	zest_pipeline_stage_none                               = 0
} zest_pipeline_stage_flag_bits;

typedef zest_uint zest_image_aspect_flags;
typedef zest_uint zest_sample_count_flags;
typedef zest_uint zest_access_flags;
typedef zest_uint zest_pipeline_stage_flags;

typedef enum {
    zest_load_op_load,
    zest_load_op_clear,
    zest_load_op_dont_care,
} zest_load_op;

typedef enum {
    zest_store_op_store,
    zest_store_op_dont_care,
} zest_store_op;

typedef enum {
    zest_descriptor_type_sampler,
    zest_descriptor_type_sampled_image,
    zest_descriptor_type_storage_image,
    zest_descriptor_type_uniform_buffer,
    zest_descriptor_type_storage_buffer,
} zest_descriptor_type;

typedef enum {
    zest_set_layout_builder_flag_none                = 0,
    zest_set_layout_builder_flag_update_after_bind   = 1,
} zest_set_layout_builder_flag_bits;

typedef enum {
    zest_set_layout_flag_none                = 0,
    zest_set_layout_flag_bindless            = 1,
} zest_set_layout_flag_bits;

typedef zest_uint zest_set_layout_builder_flags;
typedef zest_uint zest_set_layout_flags;

typedef enum zest_frustum_side { zest_LEFT = 0, zest_RIGHT = 1, zest_TOP = 2, zest_BOTTOM = 3, zest_BACK = 4, zest_FRONT = 5 } zest_frustum_size;

typedef enum {
    ZEST_ALL_MIPS             = 0xffffffff,
    ZEST_QUEUE_COUNT          = 3,
    ZEST_GRAPHICS_QUEUE_INDEX = 0,
    ZEST_COMPUTE_QUEUE_INDEX  = 1,
    ZEST_TRANSFER_QUEUE_INDEX = 2,
    ZEST_TRANSIENT_COMPATIBLE_FLAGS = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
    ZEST_TRANSIENT_NON_COMPATIBLE_FLAGS = ~(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
} zest_constants;

//Used for memory tracking and debugging
typedef enum zest_struct_type {
    zest_struct_type_view_array              = 1 << 16,
    zest_struct_type_image                   = 2 << 16,
    zest_struct_type_imgui_image             = 3 << 16,
    zest_struct_type_image_collection        = 4 << 16,
    zest_struct_type_sampler                 = 5 << 16,
    zest_struct_type_layer                   = 7 << 16,
    zest_struct_type_pipeline                = 8 << 16,
    zest_struct_type_pipeline_template       = 9 << 16,
    zest_struct_type_set_layout              = 10 << 16,
    zest_struct_type_descriptor_set          = 11 << 16,
    zest_struct_type_shader_resources        = 12 << 16,
    zest_struct_type_uniform_buffer          = 13 << 16,
    zest_struct_type_buffer_allocator        = 14 << 16,
    zest_struct_type_descriptor_pool         = 15 << 16,
    zest_struct_type_compute                 = 16 << 16,
    zest_struct_type_buffer                  = 17 << 16,
    zest_struct_type_device_memory_pool      = 18 << 16,
    zest_struct_type_timer                   = 19 << 16,
    zest_struct_type_window                  = 20 << 16,
    zest_struct_type_shader                  = 21 << 16,
    zest_struct_type_imgui                   = 22 << 16,
    zest_struct_type_queue                   = 23 << 16,
    zest_struct_type_execution_timeline      = 24 << 16,
    zest_struct_type_frame_graph_semaphores  = 25 << 16,
    zest_struct_type_swapchain               = 26 << 16,
    zest_struct_type_frame_graph             = 27 << 16,
    zest_struct_type_pass_node               = 28 << 16,
    zest_struct_type_resource_node           = 29 << 16,
    zest_struct_type_wave_submission         = 30 << 16,
    zest_struct_type_renderer                = 31 << 16,
    zest_struct_type_device                  = 32 << 16,
    zest_struct_type_app                     = 33 << 16,
    zest_struct_type_vector                  = 34 << 16,
    zest_struct_type_bitmap                  = 35 << 16,
    zest_struct_type_render_target_group     = 36 << 16,
    zest_struct_type_slang_info              = 37 << 16,
    zest_struct_type_render_pass             = 38 << 16,
    zest_struct_type_mesh                    = 39 << 16,
    zest_struct_type_texture_asset           = 40 << 16,
    zest_struct_type_frame_graph_context     = 41 << 16,
    zest_struct_type_atlas_region            = 42 << 16,
    zest_struct_type_view                    = 43 << 16
} zest_struct_type;

typedef enum zest_vulkan_memory_context {
    zest_vk_renderer = 1,
    zest_vk_device = 2,
} zest_vulkan_memory_context; 

typedef enum zest_vulkan_command {
    zest_vk_surface = 1,
    zest_vk_instance,
    zest_vk_logical_device,
    zest_vk_debug_messenger,
    zest_vk_device_instance,
    zest_vk_semaphore,
    zest_vk_command_pool,
    zest_vk_command_buffer,
    zest_vk_buffer,
    zest_vk_allocate_memory_pool,
    zest_vk_allocate_memory_image,
    zest_vk_fence,
    zest_vk_swapchain,
    zest_vk_pipeline_cache,
    zest_vk_descriptor_layout,
    zest_vk_descriptor_pool,
    zest_vk_pipeline_layout,
    zest_vk_pipelines,
    zest_vk_shader_module,
    zest_vk_sampler,
    zest_vk_image,
    zest_vk_image_view,
    zest_vk_render_pass,
    zest_vk_frame_buffer,
    zest_vk_query_pool,
    zest_vk_compute_pipeline,
} zest_vulkan_command;

typedef enum zest_app_flag_bits {
    zest_app_flag_none =                     0,
    zest_app_flag_suspend_rendering =        1 << 0,
    zest_app_flag_shift_pressed =            1 << 1,
    zest_app_flag_control_pressed =          1 << 2,
    zest_app_flag_cmd_pressed =              1 << 3,
    zest_app_flag_record_input  =            1 << 4,
    zest_app_flag_enable_console =           1 << 5,
    zest_app_flag_quit_application =         1 << 6,
    zest_app_flag_output_fps =               1 << 7
} zest_app_flag_bits;

typedef enum zest_window_mode {
    zest_window_mode_bordered,
    zest_window_mode_borderless,
    zest_window_mode_fullscreen,
    zest_window_mode_fullscreen_borderless,
}zest_window_mode;

typedef zest_uint zest_app_flags;

enum zest__constants {
    zest__validation_layer_count = 1,
#if defined(__APPLE__)
    zest__required_extension_names_count = 2,
#else
    zest__required_extension_names_count = 1,
#endif
};

typedef enum {
    zest_left_mouse =       1,
    zest_right_mouse =       1 << 1,
    zest_middle_mouse =     1 << 2
} zest_mouse_button_e;

typedef zest_uint zest_mouse_button;

typedef enum {
    zest_render_viewport_type_scale_with_window,
    zest_render_viewport_type_fixed
} zest_render_viewport_type;

typedef enum zest_renderer_flag_bits {
    zest_renderer_flag_initialised                               = 1 << 0,
    zest_renderer_flag_schedule_recreate_textures                = 1 << 1,
    zest_renderer_flag_schedule_change_vsync                     = 1 << 2,
    zest_renderer_flag_schedule_rerecord_final_render_buffer     = 1 << 3,
    zest_renderer_flag_drawing_loop_running                      = 1 << 4,
    zest_renderer_flag_msaa_toggled                              = 1 << 5,
    zest_renderer_flag_vsync_enabled                             = 1 << 6,
    zest_renderer_flag_disable_default_uniform_update            = 1 << 7,
    zest_renderer_flag_has_depth_buffer                          = 1 << 8,
    zest_renderer_flag_swap_chain_was_acquired                   = 1 << 9,
    zest_renderer_flag_work_was_submitted                        = 1 << 10,
    zest_renderer_flag_building_frame_graph                      = 1 << 11,
    zest_renderer_flag_enable_multisampling                      = 1 << 12,
} zest_renderer_flag_bits;

typedef zest_uint zest_renderer_flags;

typedef enum zest_swapchain_flag_bits {
    zest_swapchain_flag_none             = 0,
    zest_swapchain_flag_has_depth_buffer = 1 << 0,
    zest_swapchain_flag_has_msaa         = 1 << 1,
    zest_swapchain_flag_was_recreated    = 1 << 2,
}zest_swapchain_flag_bits;

typedef zest_swapchain_flag_bits zest_swapchain_flags;

typedef enum zest_init_flag_bits {
    zest_init_flag_none                                         = 0,
    zest_init_flag_maximised                                    = 1 << 1,
    zest_init_flag_cache_shaders                                = 1 << 2,
    zest_init_flag_enable_vsync                                 = 1 << 3,
    zest_init_flag_enable_fragment_stores_and_atomics           = 1 << 4,
    zest_init_flag_disable_shaderc                              = 1 << 5,
    zest_init_flag_enable_validation_layers                     = 1 << 6,
    zest_init_flag_enable_validation_layers_with_sync           = 1 << 7,
    zest_init_flag_enable_validation_layers_with_best_practices = 1 << 8,
    zest_init_flag_log_validation_errors_to_console             = 1 << 9,
    zest_init_flag_log_validation_errors_to_memory              = 1 << 10,
} zest_init_flag_bits;

typedef zest_uint zest_init_flags;

typedef enum zest_validation_flag_bits {
    zest_validation_flag_none                                   = 0,
    zest_validation_flag_enable_sync                            = 1 << 0,
    zest_validation_flag_best_practices                         = 1 << 1,
} zest_validation_flag_bits;

typedef zest_uint zest_validation_flags;

typedef enum zest_buffer_upload_flag_bits {
    zest_buffer_upload_flag_initialised                         = 1 << 0,                //Set to true once AddCopyCommand has been run at least once
    zest_buffer_upload_flag_source_is_fif                       = 1 << 1,
    zest_buffer_upload_flag_target_is_fif                       = 1 << 2
} zest_buffer_upload_flag_bits;

typedef zest_uint zest_buffer_upload_flags;

typedef enum zest_memory_pool_flags {
    zest_memory_pool_flag_none,
    zest_memory_pool_flag_single_buffer,
} zest_memory_pool_flags;

typedef zest_uint zest_buffer_flags;

typedef enum zest_command_buffer_flag_bits {
    zest_command_buffer_flag_none                               = 0,
    zest_command_buffer_flag_outdated                           = 1 << 0,
    zest_command_buffer_flag_primary                            = 1 << 1,
    zest_command_buffer_flag_secondary                          = 1 << 2,
    zest_command_buffer_flag_recording                          = 1 << 3,
} zest_command_buffer_flag_bits;

typedef zest_uint zest_command_buffer_flags;

//Be more careful about changing these numbers as they correlate to shaders. See zest_shape_type.
typedef enum {
    zest_draw_mode_none = 0,            //Default no drawmode set when no drawing has been done yet
    zest_draw_mode_instance = 1,
    zest_draw_mode_images = 2,
    zest_draw_mode_mesh = 3,
    zest_draw_mode_line_instance = 4,
    zest_draw_mode_rect_instance = 5,
    zest_draw_mode_dashed_line = 6,
    zest_draw_mode_text = 7,
    zest_draw_mode_fill_screen = 8,
    zest_draw_mode_viewport = 9,
    zest_draw_mode_im_gui = 10,
    zest_draw_mode_mesh_instance = 11,
} zest_draw_mode;

typedef enum {
    zest_shape_line = zest_draw_mode_line_instance,
    zest_shape_dashed_line = zest_draw_mode_dashed_line,
    zest_shape_rect = zest_draw_mode_rect_instance
} zest_shape_type;

typedef enum {
    zest_imgui_blendtype_none,					//Just draw with standard alpha blend
    zest_imgui_blendtype_pass,                  //Force the alpha channel to 1
    zest_imgui_blendtype_premultiply            //Divide the color channels by the alpha channel
} zest_imgui_blendtype;

typedef enum zest_image_flag_bits {
    zest_image_flag_none = 0,

    // --- Memory & Access Flags ---
    // Resides on dedicated GPU memory for fastest access. This is the default if host_visible is not set.
    zest_image_flag_device_local = 1 << 0,
    // Accessible by the CPU (mappable). The backend will likely use linear tiling.
    zest_image_flag_host_visible = 1 << 1,
    // When used with host_visible, CPU writes are visible to the GPU without manual flushing.
    zest_image_flag_host_coherent = 1 << 2,

    // --- GPU Usage Flags ---
    // Can be sampled in a shader (e.g., as a texture).
    zest_image_flag_sampled = 1 << 3,
    // Can be used for general read/write in shaders (storage image).
    zest_image_flag_storage = 1 << 4,
    // Can be a color attachment in a render pass.
    zest_image_flag_color_attachment = 1 << 5,
    // Can be a depth or stencil attachment.
    zest_image_flag_depth_stencil_attachment = 1 << 6,
    // Can be the source of a transfer operation (copy, blit).
    zest_image_flag_transfer_src = 1 << 7,
    // Can be the destination of a transfer operation.
    zest_image_flag_transfer_dst = 1 << 8,
    // Can be used as an input attachment for sub-passes.
    zest_image_flag_input_attachment = 1 << 9,

    // --- Configuration Flags ---
    // Image is a cubemap. This will create a 2D array image with 6 layers.
    zest_image_flag_cubemap = 1 << 10,
    // The chosen image format must support linear filtering.
    zest_image_flag_allow_linear_filtering = 1 << 11,
    // Automatically generate mipmaps when image data is uploaded. Implies transfer_src and transfer_dst.
    zest_image_flag_generate_mipmaps = 1 << 12,

    // Allows the image to be viewed with a different but compatible format (e.g., UNORM vs SRGB).
    zest_image_flag_mutable_format = 1 << 13,
    // Allows a 3D image to be viewed as a 2D array.
    zest_image_flag_3d_as_2d_array = 1 << 14,
    // For multi-planar formats (e.g., video), indicating each plane can have separate memory.
    zest_image_flag_disjoint_planes = 1 << 15,

    //For transient images in a frame graph
    zest_image_flag_transient = 1 << 16,

    //Convenient preset flags for common usages
    // For a standard texture loaded from CPU.
    zest_image_preset_texture = zest_image_flag_sampled | zest_image_flag_transfer_dst | zest_image_flag_device_local,

    // For a standard texture with mipmap generation.
    zest_image_preset_texture_mipmaps = zest_image_preset_texture | zest_image_flag_transfer_src | zest_image_flag_generate_mipmaps,

    // For a render target that can be sampled (e.g., post-processing).
    zest_image_preset_color_attachment = zest_image_flag_color_attachment | zest_image_flag_sampled | zest_image_flag_device_local,

    // For a depth buffer that can be sampled (e.g., shadow mapping, SSAO).
    zest_image_preset_depth_attachment = zest_image_flag_depth_stencil_attachment | zest_image_flag_sampled | zest_image_flag_device_local,

    // For an image to be written to by a compute shader and then sampled.
    zest_image_preset_storage = zest_image_flag_storage | zest_image_flag_sampled | zest_image_flag_device_local,

    // For working on cubemaps in a compute shader, like preparing for pbr lighting
    zest_image_preset_storage_cubemap = zest_image_flag_storage | zest_image_flag_sampled | zest_image_flag_device_local | zest_image_flag_cubemap,

    // For working on cubemaps in a compute shader with mipmaps, like preparing for pbr lighting
    zest_image_preset_storage_mipped_cubemap = zest_image_flag_storage | zest_image_flag_sampled | zest_image_flag_device_local | zest_image_flag_generate_mipmaps | zest_image_flag_cubemap,
} zest_image_flag_bits;

typedef zest_uint zest_image_flags;

typedef zest_uint zest_capability_flags;

typedef enum zest_texture_storage_type {
    zest_texture_storage_type_packed,                        //Pack all of the images into a sprite sheet and onto multiple layers in an image array on the GPU
    zest_texture_storage_type_bank,                          //Packs all images one per layer, best used for repeating textures or color/bump/specular etc
    zest_texture_storage_type_sprite_sheet,                  //Packs all the images onto a single layer spritesheet
    zest_texture_storage_type_single,                        //A single image texture
    zest_texture_storage_type_storage,                       //A storage texture useful for manipulation and other things in a compute shader
    zest_texture_storage_type_stream,                        //A storage texture that you can update every frame
    zest_texture_storage_type_render_target,                 //Texture storage for a render target sampler, so that you can draw the target onto another render target
    zest_texture_storage_type_cube_map                       //Texture is a cube map
} zest_texture_storage_type;

typedef enum zest_image_collection_flag_bits {
    zest_image_collection_flag_none        = 0,
    zest_image_collection_flag_is_cube_map = 1 << 0,
    zest_image_collection_flag_ktx_data    = 1 << 1,
} zest_image_collection_flag_bits;

typedef zest_uint zest_image_collection_flags;

typedef enum zest_camera_flag_bits {
    zest_camera_flags_none = 0,
    zest_camera_flags_perspective                         = 1 << 0,
    zest_camera_flags_orthagonal                          = 1 << 1,
} zest_camera_flag_bits;

typedef zest_uint zest_camera_flags;

typedef enum zest_character_flag_bits {
    zest_character_flag_none                              = 0,
    zest_character_flag_skip                              = 1 << 0,
    zest_character_flag_new_line                          = 1 << 1,
} zest_character_flag_bits;

typedef zest_uint zest_character_flags;

typedef enum zest_compute_flag_bits {
    zest_compute_flag_none                                = 0,
    zest_compute_flag_rewrite_command_buffer              = 1 << 1,    // Command buffer for this compute shader should be rewritten
    zest_compute_flag_sync_required                       = 1 << 2,    // Compute shader requires syncing with the render target
    zest_compute_flag_is_active                           = 1 << 3,    // Compute shader is active then it will be updated when the swap chain is recreated
    zest_compute_flag_manual_fif                          = 1 << 4,    // You decide when the compute shader should be re-recorded
    zest_compute_flag_primary_recorder                    = 1 << 5,    // For using with zest_RunCompute only. You will not be able to use this compute as part of a frame render
} zest_compute_flag_bits;

typedef enum zest_layer_flag_bits {
    zest_layer_flag_none                                  = 0,
    zest_layer_flag_static                                = 1 << 0,    // Layer only uploads new buffer data when required
    zest_layer_flag_device_local_direct                   = 1 << 1,    // Upload directly to device buffer (has issues so is disabled by default for now)
    zest_layer_flag_manual_fif                            = 1 << 2,    // Manually set the frame in flight for the layer
    zest_layer_flag_using_global_bindless_layout          = 1 << 3,    // Flagged if the layer is automatically setting the descriptor array index for the device buffers
    zest_layer_flag_use_prev_fif                          = 1 << 4,    // Make the layer reference the last fif rather then the current one.
} zest_layer_flag_bits;

typedef enum zest_draw_buffer_result {
    zest_draw_buffer_result_ok,
    zest_draw_buffer_result_buffer_grew,
    zest_draw_buffer_result_failed_to_grow
} zest_draw_buffer_result;

typedef enum {
    zest_query_state_none,
    zest_query_state_ready,
} zest_query_state;

typedef enum {
    zest_queue_graphics = 1,
    zest_queue_compute  = 1 << 1,
    zest_queue_transfer = 1 << 2
} zest_device_queue_type;

typedef enum {
    zest_resource_type_none              = 0,
    zest_resource_type_image             = 1 << 0,
    zest_resource_type_buffer            = 1 << 1,
    zest_resource_type_swap_chain_image  = 1 << 2,
    zest_resource_type_depth             = 1 << 3,
    //zest_resource_type_msaa              = 1 << 4,
    zest_resource_type_is_image          = zest_resource_type_image | zest_resource_type_swap_chain_image | zest_resource_type_depth,
    zest_resource_type_is_image_or_depth = zest_resource_type_image | zest_resource_type_depth
} zest_resource_type;

typedef enum zest_resource_node_flag_bits {
    zest_resource_node_flag_none                        = 0,
    zest_resource_node_flag_transient                   = 1 << 0,
    zest_resource_node_flag_imported                    = 1 << 1,
    zest_resource_node_flag_used_in_output              = 1 << 2,
	zest_resource_node_flag_is_bindless                 = 1 << 3,
    zest_resource_node_flag_release_after_use           = 1 << 4,
    zest_resource_node_flag_essential_output            = 1 << 5,
    zest_resource_node_flag_requires_storage            = 1 << 6,
    zest_resource_node_flag_aliased                     = 1 << 7,
    zest_resource_node_flag_has_producer                = 1 << 8,
} zest_resource_node_flag_bits;

typedef zest_uint zest_resource_node_flags;

typedef enum zest_resource_usage_hint_bits {
    zest_resource_usage_hint_none              = 0,
    zest_resource_usage_hint_copy_src          = 1 << 0,
    zest_resource_usage_hint_copy_dst          = 1 << 1,
    zest_resource_usage_hint_cpu_read          = (1 << 2) | zest_resource_usage_hint_copy_src,
    zest_resource_usage_hint_cpu_write         = (1 << 3) | zest_resource_usage_hint_copy_dst,
    zest_resource_usage_hint_vertex_buffer     = 1 << 4,
    zest_resource_usage_hint_index_buffer      = 1 << 5,
    zest_resource_usage_hint_msaa              = 1 << 6,
    zest_resource_usage_hint_copyable          = zest_resource_usage_hint_copy_src | zest_resource_usage_hint_copy_dst,
    zest_resource_usage_hint_cpu_transfer      = zest_resource_usage_hint_cpu_read | zest_resource_usage_hint_cpu_write
} zest_resource_usage_hint_bits;

typedef zest_uint zest_resource_usage_hint;

typedef enum zest_frame_graph_flag_bits {
    zest_frame_graph_flag_none                     = 0,
    zest_frame_graph_expecting_swap_chain_usage    = 1 << 0,
    zest_frame_graph_force_on_graphics_queue       = 1 << 1,
    zest_frame_graph_is_compiled                   = 1 << 2,
    zest_frame_graph_is_executed                   = 1 << 3,
    zest_frame_graph_present_after_execute         = 1 << 4,
    zest_frame_graph_is_cached                     = 1 << 5,
} zest_frame_graph_flag_bits;

typedef zest_uint zest_frame_graph_flags;

typedef enum {
    zest_access_write_bits_general = zest_access_shader_write_bit | zest_access_color_attachment_write_bit | zest_access_depth_stencil_attachment_write_bit | zest_access_transfer_write_bit,
    zest_access_read_bits_general = zest_access_shader_read_bit | zest_access_color_attachment_read_bit | zest_access_depth_stencil_attachment_read_bit | zest_access_transfer_read_bit | zest_access_index_read_bit | zest_access_vertex_attribute_read_bit | zest_access_indirect_command_read_bit,
    zest_access_render_pass_bits = zest_access_depth_stencil_attachment_read_bit | zest_access_color_attachment_read_bit | zest_access_depth_stencil_attachment_write_bit | zest_access_color_attachment_write_bit,
} zest_general_access_bits;

typedef enum zest_resource_purpose {
    // Buffer Usages
    zest_purpose_vertex_buffer = 1,
    zest_purpose_index_buffer,
    zest_purpose_uniform_buffer,                      // Might need shader stage too if general
    zest_purpose_storage_buffer_read,                 // Needs shader stage
    zest_purpose_storage_buffer_write,                // Needs shader stage
    zest_purpose_storage_buffer_read_write,           // Needs shader stage
    zest_purpose_indirect_buffer,
    zest_purpose_transfer_buffer,

    // Image Usages
    zest_purpose_sampled_image,                       // Needs shader stage
    zest_purpose_storage_image_read,                  // Needs shader stage
    zest_purpose_storage_image_write,                 // Needs shader stage
    zest_purpose_storage_image_read_write,            // Needs shader stage
    zest_purpose_color_attachment_write,
    zest_purpose_color_attachment_resolve,
    zest_purpose_color_attachment_read,               // For blending or input attachments
    zest_purpose_depth_stencil_attachment_read,
    zest_purpose_depth_stencil_attachment_write,
    zest_purpose_depth_stencil_attachment_read_write, // Common
    zest_purpose_input_attachment,                    // Needs shader stage (typically fragment)
    zest_purpose_transfer_image,
    zest_purpose_present_src,                         // For swapchain final layout
} zest_resource_purpose;

typedef enum zest_pass_flag_bits {
    zest_pass_flag_none           = 0,
    zest_pass_flag_disabled       = 1,
    zest_pass_flag_do_not_cull    = 1 << 1,
    zest_pass_flag_culled         = 1 << 2,
    zest_pass_flag_output_resolve = 1 << 3,
} zest_pass_flag_bits;

typedef enum zest_pass_type {
    zest_pass_type_graphics     = 1,
    zest_pass_type_compute      = 2,
    zest_pass_type_transfer     = 3,
} zest_pass_type;

typedef enum zest_dynamic_resource_type {
    zest_dynamic_resource_none = 0,
    zest_dynamic_resource_image_available_semaphore,
    zest_dynamic_resource_render_finished_semaphore,
} zest_dynamic_resource_type;

//pipeline_enums
typedef enum zest_pipeline_set_flag_bits {
    zest_pipeline_set_flag_none = 0,
    zest_pipeline_set_flag_is_render_target_pipeline = 1 << 0,        //True if this pipeline is used for the final render of a render target to the swap chain
    zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild = 1 << 1        //True if the pipeline should update it's view extent when the swap chain is recreated (the window is resized)
} zest_pipeline_set_flag_bits;

typedef zest_uint zest_pipeline_set_flags;

typedef enum zest_supported_pipeline_stages {
    zest_pipeline_vertex_input_stage = 1 << 2,
    zest_pipeline_vertex_stage       = 1 << 3,
    zest_pipeline_fragment_stage     = 1 << 7,
    zest_pipeline_compute_stage      = 1 << 11,
    zest_pipeline_transfer_stage     = 1 << 12
} zest_supported_pipeline_stages;

typedef enum zest_front_face {
    zest_front_face_clockwise,
    zest_front_face_counter_clockwise,
} zest_front_face;

typedef enum zest_topology {
    zest_topology_point_list,
    zest_topology_line_list,
    zest_topology_line_strip,
    zest_topology_triangle_list,
    zest_topology_triangle_strip,
    zest_topology_triangle_fan,
    zest_topology_line_list_with_adjacency,
    zest_topology_line_strip_with_adjacency,
    zest_topology_triangle_list_with_adjacency,
    zest_topology_triangle_strip_with_adjacency,
    zest_topology_patch_list,
} zest_topology;

typedef enum zest_cull_mode {
    zest_cull_mode_none,
    zest_cull_mode_front,
    zest_cull_mode_back,
    zest_cull_mode_front_and_back,
} zest_cull_mode;

typedef enum zest_supported_shader_stage_bits {
    zest_shader_vertex_stage = 1,
    zest_shader_fragment_stage = 16,
    zest_shader_compute_stage = 32,
    zest_shader_render_stages = zest_shader_vertex_stage | zest_shader_fragment_stage,
    zest_shader_all_stages = zest_shader_render_stages | zest_shader_compute_stage,
} zest_supported_shader_stage_bits;

typedef enum zest_report_category {
    zest_report_unused_pass,
    zest_report_taskless_pass,
    zest_report_unconnected_resource,
    zest_report_pass_culled,
    zest_report_resource_culled,
    zest_report_invalid_layer,
    zest_report_cyclic_dependency,
    zest_report_invalid_render_pass,
    zest_report_render_pass_skipped,
    zest_report_expecting_swapchain_usage,
    zest_report_bindless_indexes,
    zest_report_invalid_reference_counts,
    zest_report_missing_end_pass,
} zest_report_category;

typedef enum zest_global_binding_number {
    zest_sampler_2d_binding = 0,
    zest_sampler_array_binding,
    zest_sampler_cube_binding,
    zest_sampler_3d_binding,
    zest_storage_buffer_binding,
    zest_sampled_image_binding,
    zest_storage_image_binding,
    zest_max_global_binding_number
} zest_global_binding_number;

typedef enum zest_frame_graph_result_bits {
    zest_fgs_success                            = 0,
    zest_fgs_no_work_to_do                      = 1 << 0,
    zest_fgs_cyclic_dependency                  = 1 << 1,
    zest_fgs_invalid_render_pass                = 1 << 2,
    zest_fgs_critical_error                     = 1 << 3,
    zest_fgs_transient_resource_failure         = 1 << 4,
    zest_fgs_unable_to_acquire_command_buffer   = 1 << 5,
} zest_frame_graph_result_bits;

typedef enum zest_pass_node_visit_state {
    zest_pass_node_unvisited = 0,
    zest_pass_node_visiting,
    zest_pass_node_visited,
} zest_pass_node_visit_state;

typedef zest_uint zest_supported_shader_stages;		         //zest_shader_stage_bits
typedef zest_uint zest_compute_flags;		                 //zest_compute_flag_bits
typedef zest_uint zest_layer_flags;                          //zest_layer_flag_bits
typedef zest_uint zest_pass_flags;                           //zest_pass_flag_bits
typedef zest_uint zest_frame_graph_result;                   //zest_frame_graph_result_bits

typedef void(*zloc__block_output)(void* ptr, size_t size, int used, void* user, int is_final_output);

static const int ZEST_STRUCT_IDENTIFIER = 0x4E57;
#define zest_INIT_MAGIC(struct_type) (struct_type | ZEST_STRUCT_IDENTIFIER);

#define ZEST_ASSERT_HANDLE(handle) ZEST_ASSERT(handle && (*((int*)handle) & 0xFFFF) == ZEST_STRUCT_IDENTIFIER)
#define ZEST_VALID_HANDLE(handle) (handle && (*((int*)handle) & 0xFFFF) == ZEST_STRUCT_IDENTIFIER)
#define ZEST_STRUCT_TYPE(handle) (*((int*)handle) & 0xFFFF0000)
#define ZEST_STRUCT_MAGIC_TYPE(magic) (magic & 0xFFFF0000)
#define ZEST_IS_INTITIALISED(magic) (magic & 0xFFFF) == ZEST_STRUCT_IDENTIFIER

#define ZEST_SET_MEMORY_CONTEXT(context, command) ZestDevice->vulkan_memory_info.timestamp = ZestDevice->allocation_id++; \
    ZestDevice->vulkan_memory_info.context_info = ZEST_STRUCT_IDENTIFIER | (context << 16) | (command << 24)


// --Forward_declarations
typedef struct zest_image_t zest_image_t;
typedef struct zest_image_view_t zest_image_view_t;
typedef struct zest_image_view_array_t zest_image_view_array_t;
typedef struct zest_image_collection_t zest_image_collection_t;
typedef struct zest_bitmap_t zest_bitmap_t;
typedef struct zest_atlas_region_t zest_atlas_region_t;
typedef struct zest_sampler_t zest_sampler_t;
typedef struct zest_layer_t zest_layer_t;
typedef struct zest_pipeline_t zest_pipeline_t;
typedef struct zest_pipeline_template_t zest_pipeline_template_t;
typedef struct zest_set_layout_t zest_set_layout_t;
typedef struct zest_descriptor_set_t zest_descriptor_set_t;
typedef struct zest_descriptor_pool_t zest_descriptor_pool_t;
typedef struct zest_shader_resources_t zest_shader_resources_t;
typedef struct zest_uniform_buffer_t zest_uniform_buffer_t;
typedef struct zest_buffer_allocator_t zest_buffer_allocator_t;
typedef struct zest_compute_t zest_compute_t;
typedef struct zest_buffer_t zest_buffer_t;
typedef struct zest_device_memory_pool_t zest_device_memory_pool_t;
typedef struct zest_timer_t zest_timer_t;
typedef struct zest_window_t zest_window_t;
typedef struct zest_shader_t zest_shader_t;
typedef struct zest_frame_graph_semaphores_t zest_frame_graph_semaphores_t;
typedef struct zest_frame_graph_t zest_frame_graph_t;
typedef struct zest_pass_node_t zest_pass_node_t;
typedef struct zest_resource_node_t zest_resource_node_t;
typedef struct zest_queue_t zest_queue_t;
typedef struct zest_execution_timeline_t zest_execution_timeline_t;
typedef struct zest_swapchain_t zest_swapchain_t;
typedef struct zest_output_group_t zest_output_group_t;
typedef struct zest_render_pass_t zest_render_pass_t;
typedef struct zest_mesh_t zest_mesh_t;
typedef struct zest_frame_graph_context_t zest_frame_graph_context_t;

//Backends
typedef struct zest_device_backend_t zest_device_backend_t;
typedef struct zest_renderer_backend_t zest_renderer_backend_t;
typedef struct zest_swapchain_backend_t zest_swapchain_backend_t;
typedef struct zest_queue_backend_t zest_queue_backend_t;
typedef struct zest_window_backend_t zest_window_backend_t;
typedef struct zest_frame_graph_context_backend_t zest_frame_graph_context_backend_t;
typedef struct zest_device_memory_pool_t zest_device_memory_pool_t;
typedef struct zest_buffer_backend_t zest_buffer_backend_t;
typedef struct zest_uniform_buffer_backend_t zest_uniform_buffer_backend_t;
typedef struct zest_shader_resources_backend_t zest_shader_resources_backend_t;
typedef struct zest_descriptor_pool_backend_t zest_descriptor_pool_backend_t;
typedef struct zest_descriptor_set_backend_t zest_descriptor_set_backend_t;
typedef struct zest_set_layout_backend_t zest_set_layout_backend_t;
typedef struct zest_pipeline_backend_t zest_pipeline_backend_t;
typedef struct zest_compute_backend_t zest_compute_backend_t;
typedef struct zest_image_backend_t zest_image_backend_t;
typedef struct zest_image_view_backend_t zest_image_view_backend_t;
typedef struct zest_sampler_backend_t zest_sampler_backend_t;
typedef struct zest_submission_batch_backend_t zest_submission_batch_backend_t;
typedef struct zest_execution_backend_t zest_execution_backend_t;
typedef struct zest_frame_graph_semaphores_backend_t zest_frame_graph_semaphores_backend_t;
typedef struct zest_render_pass_backend_t zest_render_pass_backend_t;
typedef struct zest_execution_timeline_backend_t zest_execution_timeline_backend_t;
typedef struct zest_execution_barriers_backend_t zest_execution_barriers_backend_t;
typedef struct zest_deferred_destruction_backend_t zest_deferred_destruction_backend_t;
typedef struct zest_set_layout_builder_backend_t zest_set_layout_builder_backend_t;

//Generate handles for the struct types. These are all pointers to memory where the object is stored.
ZEST__MAKE_HANDLE(zest_image)
ZEST__MAKE_HANDLE(zest_image_view)
ZEST__MAKE_HANDLE(zest_image_view_array)
ZEST__MAKE_HANDLE(zest_image_collection)
ZEST__MAKE_HANDLE(zest_bitmap)
ZEST__MAKE_HANDLE(zest_atlas_region)
ZEST__MAKE_HANDLE(zest_sampler)
ZEST__MAKE_HANDLE(zest_layer)
ZEST__MAKE_HANDLE(zest_pipeline)
ZEST__MAKE_HANDLE(zest_pipeline_template)
ZEST__MAKE_HANDLE(zest_set_layout)
ZEST__MAKE_HANDLE(zest_descriptor_set)
ZEST__MAKE_HANDLE(zest_shader_resources)
ZEST__MAKE_HANDLE(zest_uniform_buffer)
ZEST__MAKE_HANDLE(zest_buffer_allocator)
ZEST__MAKE_HANDLE(zest_descriptor_pool)
ZEST__MAKE_HANDLE(zest_compute)
ZEST__MAKE_HANDLE(zest_buffer)
ZEST__MAKE_HANDLE(zest_device_memory_pool)
ZEST__MAKE_HANDLE(zest_timer)
ZEST__MAKE_HANDLE(zest_window)
ZEST__MAKE_HANDLE(zest_shader)
ZEST__MAKE_HANDLE(zest_queue)
ZEST__MAKE_HANDLE(zest_execution_timeline)
ZEST__MAKE_HANDLE(zest_frame_graph_semaphores)
ZEST__MAKE_HANDLE(zest_swapchain)
ZEST__MAKE_HANDLE(zest_frame_graph)
ZEST__MAKE_HANDLE(zest_pass_node)
ZEST__MAKE_HANDLE(zest_resource_node)
ZEST__MAKE_HANDLE(zest_output_group);
ZEST__MAKE_HANDLE(zest_render_pass)
ZEST__MAKE_HANDLE(zest_mesh)
ZEST__MAKE_HANDLE(zest_frame_graph_context)

ZEST__MAKE_HANDLE(zest_device_backend)
ZEST__MAKE_HANDLE(zest_renderer_backend)
ZEST__MAKE_HANDLE(zest_swapchain_backend)
ZEST__MAKE_HANDLE(zest_queue_backend)
ZEST__MAKE_HANDLE(zest_window_backend)
ZEST__MAKE_HANDLE(zest_frame_graph_context_backend)
ZEST__MAKE_HANDLE(zest_frame_graph_semaphores_backend)
ZEST__MAKE_HANDLE(zest_device_memory_pool_backend)
ZEST__MAKE_HANDLE(zest_buffer_backend)
ZEST__MAKE_HANDLE(zest_uniform_buffer_backend)
ZEST__MAKE_HANDLE(zest_shader_resources_backend)
ZEST__MAKE_HANDLE(zest_descriptor_pool_backend)
ZEST__MAKE_HANDLE(zest_descriptor_set_backend)
ZEST__MAKE_HANDLE(zest_set_layout_backend)
ZEST__MAKE_HANDLE(zest_pipeline_backend)
ZEST__MAKE_HANDLE(zest_compute_backend)
ZEST__MAKE_HANDLE(zest_image_backend)
ZEST__MAKE_HANDLE(zest_image_view_backend)
ZEST__MAKE_HANDLE(zest_sampler_backend)
ZEST__MAKE_HANDLE(zest_submission_batch_backend)
ZEST__MAKE_HANDLE(zest_execution_backend)
ZEST__MAKE_HANDLE(zest_render_pass_backend)
ZEST__MAKE_HANDLE(zest_execution_timeline_backend)
ZEST__MAKE_HANDLE(zest_execution_barriers_backend)
ZEST__MAKE_HANDLE(zest_deferred_destruction_backend)
ZEST__MAKE_HANDLE(zest_set_layout_builder_backend)

ZEST__MAKE_USER_HANDLE(zest_shader_resources)
ZEST__MAKE_USER_HANDLE(zest_image)
ZEST__MAKE_USER_HANDLE(zest_image_view)
ZEST__MAKE_USER_HANDLE(zest_sampler)
ZEST__MAKE_USER_HANDLE(zest_image_view_array)
ZEST__MAKE_USER_HANDLE(zest_uniform_buffer)
ZEST__MAKE_USER_HANDLE(zest_timer)
ZEST__MAKE_USER_HANDLE(zest_layer)
ZEST__MAKE_USER_HANDLE(zest_shader)
ZEST__MAKE_USER_HANDLE(zest_compute)
ZEST__MAKE_USER_HANDLE(zest_set_layout)

// --Private structs with inline functions
typedef struct zest_queue_family_indices {
    zest_uint graphics_family_index;
    zest_uint transfer_family_index;
    zest_uint compute_family_index;
} zest_queue_family_indices;

// --Pocket Dynamic Array
typedef struct zest_vec {
    int magic;  //For allocation tracking
    int id;     //and finding leaks.
    zest_uint current_size;
    zest_uint capacity;
} zest_vec;

enum {
    zest__VEC_HEADER_OVERHEAD = sizeof(zest_vec)
};

// --Pocket_dynamic_array
#define zest__vec_header(T) ((zest_vec*)T - 1)
#define zest_vec_test(T, index) ZEST_ASSERT(index < zest__vec_header(T)->current_size)
zest_uint zest__grow_capacity(void *T, zest_uint size);
#define zest_vec_bump(T) zest__vec_header(T)->current_size++
#define zest_vec_clip(T) zest__vec_header(T)->current_size--
#define zest_vec_trim(T, amount) zest__vec_header(T)->current_size -= amount;
#define zest_vec_grow(T) ((!(T) || (zest__vec_header(T)->current_size == zest__vec_header(T)->capacity)) ? T = zest__vec_reserve((T), sizeof(*T), (T ? zest__grow_capacity(T, zest__vec_header(T)->current_size) : 8)) : 0)
#define zest_vec_linear_grow(allocator, T) ((!(T) || (zest__vec_header(T)->current_size == zest__vec_header(T)->capacity)) ? T = zest__vec_linear_reserve(allocator, (T), sizeof(*T), (T ? zest__grow_capacity(T, zest__vec_header(T)->current_size) : 16)) : 0)
#define zest_vec_empty(T) (!T || zest__vec_header(T)->current_size == 0)
#define zest_vec_size(T) ((T) ? zest__vec_header(T)->current_size : 0)
#define zest_vec_last_index(T) (zest__vec_header(T)->current_size - 1)
#define zest_vec_capacity(T) (zest__vec_header(T)->capacity)
#define zest_vec_size_in_bytes(T) (zest__vec_header(T)->current_size * sizeof(*T))
#define zest_vec_front(T) (T[0])
#define zest_vec_back(T) (T[zest__vec_header(T)->current_size - 1])
#define zest_vec_end(T) (&(T[zest_vec_size(T)]))
#define zest_vec_clear(T) if(T) zest__vec_header(T)->current_size = 0
#define zest_vec_free(T) if(T) { ZEST__FREE(zest__vec_header(T)); T = ZEST_NULL;}
#define zest_vec_reserve(T, new_size) if(!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_reserve(T, sizeof(*T), new_size == 1 ? 8 : new_size);
#define zest_vec_resize(T, new_size) if(!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_reserve(T, sizeof(*T), new_size == 1 ? 8 : new_size); zest__vec_header(T)->current_size = new_size
#define zest_vec_linear_reserve(allocator, T, new_size) if(!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_linear_reserve(allocator, T, sizeof(*T), new_size == 1 ? 8 : new_size);
#define zest_vec_linear_resize(allocator, T, new_size) if(!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_linear_reserve(allocator, T, sizeof(*T), new_size == 1 ? 8 : new_size); zest__vec_header(T)->current_size = new_size
#define zest_vec_push(T, value) zest_vec_grow(T); (T)[zest__vec_header(T)->current_size++] = value
#define zest_vec_linear_push(allocator, T, value) zest_vec_linear_grow(allocator, T); (T)[zest__vec_header(T)->current_size++] = value
#define zest_vec_pop(T) (zest__vec_header(T)->current_size--, T[zest__vec_header(T)->current_size])
#define zest_vec_insert(T, location, value) { ptrdiff_t offset = location - T; zest_vec_grow(T); if(offset < zest_vec_size(T)) memmove(T + offset + 1, T + offset, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); T[offset] = value; zest_vec_bump(T); }
#define zest_vec_linear_insert(allocator, T, location, value) { ptrdiff_t offset = location - T; zest_vec_linear_grow(allocator, T); if(offset < zest_vec_size(T)) memmove(T + offset + 1, T + offset, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); T[offset] = value; zest_vec_bump(T); }
#define zest_vec_erase(T, location) { ptrdiff_t offset = location - T; ZEST_ASSERT(T && offset >= 0 && location < zest_vec_end(T)); memmove(T + offset, T + offset + 1, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); zest_vec_clip(T); }
#define zest_vec_erase_range(T, it, it_last) { ZEST_ASSERT(T && it >= T && it < zest_vec_end(T)); const ptrdiff_t count = it_last - it; const ptrdiff_t off = it - T; memmove(T + off, T + off + count, ((size_t)zest_vec_size(T) - (size_t)off - count) * sizeof(*T)); zest_vec_trim(T, (zest_uint)count); }
#define zest_vec_set(T, index, value) ZEST_ASSERT((zest_uint)index < zest__vec_header(T)->current_size); T[index] = value;
#define zest_vec_foreach(index, T) for(int index = 0; index != zest_vec_size(T); ++index)
// --end of pocket dynamic array

// --Pocket_bucket_array
// The main purpose of this bucket array is to produce stable pointers for render graph resources
typedef struct zest_bucket_array_t {
    void** buckets;             // A zest_vec of pointers to individual buckets
    zest_uint bucket_capacity;  // Number of elements each bucket can hold
    zest_uint current_size;       // Total number of elements across all buckets
    zest_uint element_size;     // The size of a single element
} zest_bucket_array_t;

ZEST_PRIVATE inline void zest__bucket_array_init(zest_bucket_array_t *array, zest_uint element_size, zest_uint bucket_capacity);
ZEST_PRIVATE inline void zest__bucket_array_free(zest_bucket_array_t *array);
ZEST_PRIVATE inline void *zest__bucket_array_get(zest_bucket_array_t *array, zest_uint index);
ZEST_PRIVATE inline void *zest__bucket_array_add(zest_bucket_array_t *array);
ZEST_PRIVATE inline void *zest__bucket_array_linear_add(zloc_linear_allocator_t *allocator, zest_bucket_array_t *array);

#define zest_bucket_array_init(array, T, cap) zest__bucket_array_init(array, sizeof(T), cap)
#define zest_bucket_array_get(array, T, index) ((T*)zest__bucket_array_get(array, index))
#define zest_bucket_array_add(array, T) ((T*)zest__bucket_array_add(array))
#define zest_bucket_array_linear_add(allocator, array, T) ((T*)zest__bucket_array_linear_add(allocator, array))
#define zest_bucket_array_push(array, T, value) do { T* new_slot = zest_bucket_array_add(array, T); *new_slot = value; } while(0)
#define zest_bucket_array_linear_push(allocator, array, T, value) do { T* new_slot = zest_bucket_array_linear_add(allocator, array, T); *new_slot = value; } while(0)
#define zest_bucket_array_size(array) ((array)->current_size)
#define zest_bucket_array_foreach(index, array) for (int index = 0; index != array.current_size; ++index)
// --end of pocket bucket array

// --Generic container used to store resources that use int handles
typedef struct zest_resource_store_t {
    void *data;
    zest_uint current_size;
    zest_uint capacity;
    zest_uint struct_size;
    zest_uint alignment;
    zest_uint *generations;
    zest_uint *free_slots;
} zest_resource_store_t;

ZEST_PRIVATE void zest__free_store(zest_resource_store_t *store);
ZEST_PRIVATE void zest__reserve_store(zest_resource_store_t *store, zest_uint new_capacity);
ZEST_PRIVATE void zest__clear_store(zest_resource_store_t *store);
ZEST_PRIVATE zest_uint zest__grow_store_capacity(zest_resource_store_t *store, zest_uint size);
ZEST_PRIVATE void zest__resize_store(zest_resource_store_t *store, zest_uint new_size);
ZEST_PRIVATE void zest__resize_bytes_store(zest_resource_store_t *store, zest_uint new_size);
ZEST_PRIVATE zest_uint zest__size_in_bytes_store(zest_resource_store_t *store);
ZEST_PRIVATE zest_handle zest__add_store_resource(zest_resource_store_t *store);
ZEST_PRIVATE void zest__remove_store_resource(zest_resource_store_t *store, zest_handle handle);
ZEST_PRIVATE void zest__initialise_store(zest_resource_store_t *store, zest_uint struct_size);
ZEST_PRIVATE inline void *zest__get_store_resource(zest_resource_store_t *store, zest_handle handle) {
    zest_uint index = ZEST_HANDLE_INDEX(handle);
    zest_uint generation = ZEST_HANDLE_GENERATION(handle);
    if (index < store->capacity && store->generations[index] == generation) {
        return (void *)((char *)store->data + index * store->struct_size);
    }
    return NULL;
}

ZEST_PRIVATE inline void *zest__get_store_resource_checked(zest_resource_store_t *store, zest_handle handle) {
    void *resource = zest__get_store_resource(store, handle);
    ZEST_ASSERT(resource);   //Not a valid handle for the resource. Check the stack trace for the calling function and resource type
    return resource;
}


// --Pocket_Hasher, converted to c from Stephen Brumme's XXHash code (https://github.com/stbrumme/xxhash) by Peter Rigby
/*
MIT License Copyright (c) 2018 Stephan Brumme
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */
#define zest__PRIME1 11400714785074694791ULL
#define zest__PRIME2 14029467366897019727ULL
#define zest__PRIME3 1609587929392839161ULL
#define zest__PRIME4 9650029242287828579ULL
#define zest__PRIME5 2870177450012600261ULL

enum { zest__HASH_MAX_BUFFER_SIZE = 31 + 1 };

typedef struct zest_hasher_t {
    zest_ull      state[4];
    unsigned char buffer[zest__HASH_MAX_BUFFER_SIZE];
    zest_ull      buffer_size;
    zest_ull      total_length;
} zest_hasher_t;

ZEST_PRIVATE zest_bool zest__is_aligned(void *ptr, size_t alignment) {
    uintptr_t address = (uintptr_t)ptr;
    return (address % alignment) == 0;
}

ZEST_PRIVATE inline zest_ull zest__hash_rotate_left(zest_ull x, unsigned char bits) {
    return (x << bits) | (x >> (64 - bits));
}
ZEST_PRIVATE inline zest_ull zest__hash_process_single(zest_ull previous, zest_ull input) {
    return zest__hash_rotate_left(previous + input * zest__PRIME2, 31) * zest__PRIME1;
}
ZEST_PRIVATE inline void zest__hasher_process(const void* data, zest_ull *state0, zest_ull *state1, zest_ull *state2, zest_ull *state3) {
    zest_ull *block = (zest_ull*)data;
    zest_ull blocks[4];
    memcpy(blocks, data, sizeof(zest_ull) * 4);
    *state0 = zest__hash_process_single(*state0, blocks[0]);
    *state1 = zest__hash_process_single(*state1, blocks[1]);
    *state2 = zest__hash_process_single(*state2, blocks[2]);
    *state3 = zest__hash_process_single(*state3, blocks[3]);
}
ZEST_PRIVATE inline zest_bool zest__hasher_add(zest_hasher_t *hasher, const void* input, zest_ull length)
{
    if (!input || length == 0) return ZEST_FALSE;

    hasher->total_length += length;
    unsigned char* data = (unsigned char*)input;

    if (hasher->buffer_size + length < zest__HASH_MAX_BUFFER_SIZE)
    {
        while (length-- > 0)
            hasher->buffer[hasher->buffer_size++] = *data++;
        return ZEST_TRUE;
    }

    const unsigned char* stop = data + length;
    const unsigned char* stopBlock = stop - zest__HASH_MAX_BUFFER_SIZE;

    if (hasher->buffer_size > 0)
    {
        while (hasher->buffer_size < zest__HASH_MAX_BUFFER_SIZE)
            hasher->buffer[hasher->buffer_size++] = *data++;

        zest__hasher_process(hasher->buffer, &hasher->state[0], &hasher->state[1], &hasher->state[2], &hasher->state[3]);
    }

    zest_ull s0 = hasher->state[0], s1 = hasher->state[1], s2 = hasher->state[2], s3 = hasher->state[3];
    zest_bool test = zest__is_aligned(&s0, 8);
    while (data <= stopBlock)
    {
        zest__hasher_process(data, &s0, &s1, &s2, &s3);
        data += 32;
    }
    hasher->state[0] = s0; hasher->state[1] = s1; hasher->state[2] = s2; hasher->state[3] = s3;

    hasher->buffer_size = stop - data;
    for (zest_ull i = 0; i < hasher->buffer_size; i++)
        hasher->buffer[i] = data[i];

    return ZEST_TRUE;
}

ZEST_PRIVATE inline zest_ull zest__get_hash(zest_hasher_t *hasher)
{
    zest_ull result;
    if (hasher->total_length >= zest__HASH_MAX_BUFFER_SIZE)
    {
        result = zest__hash_rotate_left(hasher->state[0], 1) +
            zest__hash_rotate_left(hasher->state[1], 7) +
            zest__hash_rotate_left(hasher->state[2], 12) +
            zest__hash_rotate_left(hasher->state[3], 18);
        result = (result ^ zest__hash_process_single(0, hasher->state[0])) * zest__PRIME1 + zest__PRIME4;
        result = (result ^ zest__hash_process_single(0, hasher->state[1])) * zest__PRIME1 + zest__PRIME4;
        result = (result ^ zest__hash_process_single(0, hasher->state[2])) * zest__PRIME1 + zest__PRIME4;
        result = (result ^ zest__hash_process_single(0, hasher->state[3])) * zest__PRIME1 + zest__PRIME4;
    }
    else
    {
        result = hasher->state[2] + zest__PRIME5;
    }

    result += hasher->total_length;
    const unsigned char* data = hasher->buffer;
    const unsigned char* stop = data + hasher->buffer_size;
    for (; data + 8 <= stop; data += 8)
        result = zest__hash_rotate_left(result ^ zest__hash_process_single(0, *(zest_ull*)data), 27) * zest__PRIME1 + zest__PRIME4;
    if (data + 4 <= stop)
    {
        result = zest__hash_rotate_left(result ^ (*(zest_uint*)data) * zest__PRIME1, 23) * zest__PRIME2 + zest__PRIME3;
        data += 4;
    }
    while (data != stop)
        result = zest__hash_rotate_left(result ^ (*data++) * zest__PRIME5, 11) * zest__PRIME1;
    result ^= result >> 33;
    result *= zest__PRIME2;
    result ^= result >> 29;
    result *= zest__PRIME3;
    result ^= result >> 32;
    return result;
}
void zest__hash_initialise(zest_hasher_t *hasher, zest_ull seed);

//The only command you need for the hasher. Just used internally by the hash map.
zest_key zest_Hash(const void* input, zest_ull length, zest_ull seed);
//-- End of Pocket Hasher

// --Begin Pocket_hash_map
//  For internal use only
typedef struct {
    zest_key key;
    zest_index index;
} zest_hash_pair;

#ifndef ZEST_HASH_SEED
#define ZEST_HASH_SEED 0xABCDEF99
#endif
ZEST_PRIVATE zest_hash_pair* zest__lower_bound(zest_hash_pair *map, zest_key key) { zest_hash_pair *first = map; zest_hash_pair *last = map ? zest_vec_end(map) : 0; size_t count = (size_t)(last - first); while (count > 0) { size_t count2 = count >> 1; zest_hash_pair* mid = first + count2; if (mid->key < key) { first = ++mid; count -= count2 + 1; } else { count = count2; } } return first; }
ZEST_PRIVATE void zest__map_realign_indexes(zest_hash_pair *map, zest_index index) { zest_vec_foreach(i, map) { if (map[i].index < index) continue; map[i].index--; } }
ZEST_PRIVATE zest_index zest__map_get_index(zest_hash_pair *map, zest_key key) { zest_hash_pair *it = zest__lower_bound(map, key); return (it == zest_vec_end(map) || it->key != key) ? -1 : it->index; }
#define zest_map_hash(hash_map, name) zest_Hash(name, strlen(name), ZEST_HASH_SEED)
#define zest_map_hash_ptr(hash_map, ptr, size) zest_Hash(ptr, size, ZEST_HASH_SEED)
#define zest_hash_map(T) typedef struct { zest_hash_pair *map; T *data; zest_index *free_slots; zest_index last_index; }
#define zest_map_valid_name(hash_map, name) (hash_map.map && zest__map_get_index(hash_map.map, zest_map_hash(hash_map, name)) != -1)
#define zest_map_valid_key(hash_map, key) (hash_map.map && zest__map_get_index(hash_map.map, key) != -1)
#define zest_map_valid_index(hash_map, index) (hash_map.map && (zest_uint)index < zest_vec_size(hash_map.data))
#define zest_map_valid_hash(hash_map, ptr, size) (zest__map_get_index(hash_map.map, zest_map_hash_ptr(hash_map, ptr, size)) != -1)
#define zest_map_set_index(hash_map, hash_key, value) zest_hash_pair *it = zest__lower_bound(hash_map.map, hash_key); if(!hash_map.map || it == zest_vec_end(hash_map.map) || it->key != hash_key) { if(zest_vec_size(hash_map.free_slots)) { hash_map.last_index = zest_vec_pop(hash_map.free_slots); hash_map.data[hash_map.last_index] = value; } else {hash_map.last_index = zest_vec_size(hash_map.data); zest_vec_push(hash_map.data, value);} zest_hash_pair new_pair; new_pair.key = hash_key; new_pair.index = hash_map.last_index; zest_vec_insert(hash_map.map, it, new_pair); } else {hash_map.data[it->index] = value;}
#define zest_map_set_linear_index(allocator, hash_map, hash_key, value) zest_hash_pair *it = zest__lower_bound(hash_map.map, hash_key); if(!hash_map.map || it == zest_vec_end(hash_map.map) || it->key != hash_key) { if(zest_vec_size(hash_map.free_slots)) { hash_map.last_index = zest_vec_pop(hash_map.free_slots); hash_map.data[hash_map.last_index] = value; } else {hash_map.last_index = zest_vec_size(hash_map.data); zest_vec_linear_push(allocator, hash_map.data, value);} zest_hash_pair new_pair; new_pair.key = hash_key; new_pair.index = hash_map.last_index; zest_vec_linear_insert(allocator, hash_map.map, it, new_pair); } else {hash_map.data[it->index] = value;}
#define zest_map_insert(hash_map, name, value) { zest_key key = zest_Hash(name, strlen(name), ZEST_HASH_SEED); zest_map_set_index(hash_map, key, value); }
#define zest_map_linear_insert(allocator, hash_map, name, value) { zest_key key = zest_Hash(name, strlen(name), ZEST_HASH_SEED); zest_map_set_linear_index(allocator, hash_map, key, value); }
#define zest_map_insert_key(hash_map, hash_key, value) { zest_map_set_index(hash_map, hash_key, value) }
#define zest_map_insert_linear_key(allocator, hash_map, hash_key, value) { zest_map_set_linear_index(allocator, hash_map, hash_key, value) }
#define zest_map_insert_with_ptr_hash(hash_map, ptr, size, value) { zest_key key = zest_map_hash_ptr(hash_map, ptr, size); zest_map_set_index(hash_map, key, value) }
#define zest_map_at(hash_map, name) &hash_map.data[zest__map_get_index(hash_map.map, zest_map_hash(hash_map, name))]
#define zest_map_at_key(hash_map, key) &hash_map.data[zest__map_get_index(hash_map.map, key)]
#define zest_map_at_index(hash_map, index) &hash_map.data[index]
#define zest_map_get_index_by_key(hash_map, key) zest__map_get_index(hash_map.map, key);
#define zest_map_get_index_by_name(hash_map, name) zest__map_get_index(hash_map.map, zest_map_hash(hash_map, name));
#define zest_map_remove(hash_map, name) {zest_key key = zest_map_hash(hash_map, name); zest_hash_pair *it = zest__lower_bound(hash_map.map, key); zest_index index = it->index; zest_vec_erase(hash_map.map, it); zest_vec_push(hash_map.free_slots, index); }
#define zest_map_remove_key(hash_map, name) { zest_hash_pair *it = zest__lower_bound(hash_map.map, key); zest_index index = it->index; zest_vec_erase(hash_map.map, it); zest_vec_push(hash_map.free_slots, index); }
#define zest_map_last_index(hash_map) (hash_map.last_index)
#define zest_map_size(hash_map) (hash_map.map ? zest__vec_header(hash_map.data)->current_size : 0)
#define zest_map_clear(hash_map) zest_vec_clear(hash_map.map); zest_vec_clear(hash_map.data); zest_vec_clear(hash_map.free_slots)
#define zest_map_free(hash_map) if(hash_map.map) ZEST__FREE(zest__vec_header(hash_map.map)); if(hash_map.data) ZEST__FREE(zest__vec_header(hash_map.data)); if(hash_map.free_slots) ZEST__FREE(zest__vec_header(hash_map.free_slots)); hash_map.data = 0; hash_map.map = 0; hash_map.free_slots = 0
//Use inside a for loop to iterate over the map. The loop index should be be the index into the map array, to get the index into the data array.
#define zest_map_index(hash_map, i) (hash_map.map[i].index)
#define zest_map_foreach(index, hash_map) zest_vec_foreach(index, hash_map.map)
// --End pocket hash map

// --Begin Pocket_text_buffer
typedef struct zest_text_t {
    char *str;
} zest_text_t;

ZEST_API void zest_SetText(zest_text_t *buffer, const char *text);
ZEST_API void zest_ResizeText(zest_text_t *buffer, zest_uint size);
ZEST_API void zest_SetTextf(zest_text_t *buffer, const char *text, ...);
ZEST_API void zest_SetTextfv(zest_text_t *buffer, const char *text, va_list args);
ZEST_API void zest_AppendTextf(zest_text_t *buffer, const char *format, ...);
ZEST_API void zest_FreeText(zest_text_t *buffer);
ZEST_API zest_uint zest_TextSize(zest_text_t *buffer);      //Will include a null terminator
ZEST_API zest_uint zest_TextLength(zest_text_t *buffer);    //Uses strlen to get the length
// --End pocket text buffer

#ifndef ZEST_LOG_ENTRY_SIZE
#define ZEST_LOG_ENTRY_SIZE 256
#endif
typedef struct zest_log_entry_t {
    char str[ZEST_LOG_ENTRY_SIZE];
    zest_microsecs time;
} zest_log_entry_t;

ZEST_PRIVATE void zest__log_entry(const char *entry, ...);
ZEST_PRIVATE void zest__log_entry_v(char *str, const char *entry, ...);
ZEST_PRIVATE void zest__reset_frame_log(char *str, const char *entry, ...);

ZEST_PRIVATE void zest__add_report(zest_report_category category, int line_number, const char *entry, ...);


#ifdef ZEST_DEBUGGING
#define ZEST_FRAME_LOG(message_f, ...) zest__log_entry(message_f, ##__VA_ARGS__)
#define ZEST_RESET_LOG() zest__reset_log()
#define ZEST__MAYBE_REPORT(condition, category, entry, ...) if(condition) { zest__add_report(category, __LINE__, __FILE__, entry, ##__VA_ARGS__);}
#define ZEST__REPORT(category, entry, ...) zest__add_report(category, __LINE__, __FILE__, entry, ##__VA_ARGS__)
#else
#define ZEST_FRAME_LOG(message_f, ...) 
#define ZEST_RESET_LOG() 
#define ZEST__REPORT()
#endif

//Threading

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif

#ifndef ZEST_MAX_QUEUES
#define ZEST_MAX_QUEUES 64
#endif

#ifndef ZEST_MAX_QUEUE_ENTRIES
#define ZEST_MAX_QUEUE_ENTRIES 512
#endif

#ifndef ZEST_MAX_THREADS
#define ZEST_MAX_THREADS 64
#endif

// Forward declaration
struct zest_work_queue_t;

// Callback function type
typedef void (*zest_work_queue_callback)(struct zest_work_queue_t *queue, void *data);

typedef struct zest_work_queue_entry_t {
    zest_work_queue_callback call_back;
    void *data;
} zest_work_queue_entry_t;

typedef struct zest_work_queue_t {
    volatile zest_uint entry_completion_goal;
    volatile zest_uint entry_completion_count;
    volatile int next_read_entry;
    volatile int next_write_entry;
    zest_work_queue_entry_t entries[ZEST_MAX_QUEUE_ENTRIES];
} zest_work_queue_t;

// Platform-specific synchronization wrapper
typedef struct zest_sync_t {
#ifdef _WIN32
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE empty_condition;
    CONDITION_VARIABLE full_condition;
#else
    pthread_mutex_t mutex;
    pthread_cond_t empty_condition;
    pthread_cond_t full_condition;
#endif
} zest_sync_t;

typedef struct zest_queue_processor_t {
    zest_sync_t sync;
    zest_uint count;
    volatile zest_bool end_all_threads;
    zest_work_queue_t *queues[ZEST_MAX_QUEUES];
} zest_queue_processor_t;

typedef struct zest_storage_t {
    zest_queue_processor_t thread_queues;
#ifdef _WIN32
    HANDLE threads[ZEST_MAX_THREADS];
#else
    pthread_t threads[ZEST_MAX_THREADS];
#endif
    zest_uint thread_count;
} zest_storage_t;

// Global variables (extern declarations)
extern zest_storage_t *zest__globals;

// Platform-specific atomic operations
ZEST_PRIVATE inline zest_uint zest__atomic_increment(volatile zest_uint *value) {
#ifdef _WIN32
    return InterlockedIncrement((LONG *)value);
#else
    return __sync_fetch_and_add(value, 1) + 1;
#endif
}

ZEST_PRIVATE inline bool zest__atomic_compare_exchange(volatile int *dest, int exchange, int comparand) {
#ifdef _WIN32
    return InterlockedCompareExchange((LONG *)dest, exchange, comparand) == comparand;
#else
    return __sync_bool_compare_and_swap(dest, comparand, exchange);
#endif
}

ZEST_PRIVATE inline void zest__memory_barrier(void) {
#ifdef _WIN32
    MemoryBarrier();
#else
    __sync_synchronize();
#endif
}

// Initialize synchronization primitives
ZEST_PRIVATE inline void zest__sync_init(zest_sync_t *sync) {
#ifdef _WIN32
    InitializeCriticalSection(&sync->mutex);
    InitializeConditionVariable(&sync->empty_condition);
    InitializeConditionVariable(&sync->full_condition);
#else
    pthread_mutex_init(&sync->mutex, NULL);
    pthread_cond_init(&sync->empty_condition, NULL);
    pthread_cond_init(&sync->full_condition, NULL);
#endif
}

ZEST_PRIVATE inline void zest__sync_cleanup(zest_sync_t *sync) {
#ifdef _WIN32
    DeleteCriticalSection(&sync->mutex);
#else
    pthread_mutex_destroy(&sync->mutex);
    pthread_cond_destroy(&sync->empty_condition);
    pthread_cond_destroy(&sync->full_condition);
#endif
}

ZEST_PRIVATE inline void zest__sync_lock(zest_sync_t *sync) {
#ifdef _WIN32
    EnterCriticalSection(&sync->mutex);
#else
    pthread_mutex_lock(&sync->mutex);
#endif
}

ZEST_PRIVATE inline void zest__sync_unlock(zest_sync_t *sync) {
#ifdef _WIN32
    LeaveCriticalSection(&sync->mutex);
#else
    pthread_mutex_unlock(&sync->mutex);
#endif
}

ZEST_PRIVATE inline void zest__sync_wait_empty(zest_sync_t *sync) {
#ifdef _WIN32
    SleepConditionVariableCS(&sync->empty_condition, &sync->mutex, INFINITE);
#else
    pthread_cond_wait(&sync->empty_condition, &sync->mutex);
#endif
}

ZEST_PRIVATE inline void zest__sync_wait_full(zest_sync_t *sync) {
#ifdef _WIN32
    SleepConditionVariableCS(&sync->full_condition, &sync->mutex, INFINITE);
#else
    pthread_cond_wait(&sync->full_condition, &sync->mutex);
#endif
}

ZEST_PRIVATE inline void zest__sync_signal_empty(zest_sync_t *sync) {
#ifdef _WIN32
    WakeConditionVariable(&sync->empty_condition);
#else
    pthread_cond_signal(&sync->empty_condition);
#endif
}

ZEST_PRIVATE inline void zest__sync_signal_full(zest_sync_t *sync) {
#ifdef _WIN32
    WakeConditionVariable(&sync->full_condition);
#else
    pthread_cond_signal(&sync->full_condition);
#endif
}

// Initialize queue processor
ZEST_PRIVATE inline void zest__initialise_thread_queues(zest_queue_processor_t *queues) {
    queues->count = 0;
    memset(queues->queues, 0, ZEST_MAX_QUEUES * sizeof(void *));
    zest__sync_init(&queues->sync);
    queues->end_all_threads = false;
}

ZEST_PRIVATE inline void zest__cleanup_thread_queues(zest_queue_processor_t *queues) {
    zest__sync_cleanup(&queues->sync);
}

ZEST_PRIVATE inline zest_work_queue_t *zest__get_queue_with_work(zest_queue_processor_t *thread_processor) {
    zest__sync_lock(&thread_processor->sync);

    while (thread_processor->count == 0 && !thread_processor->end_all_threads) {
        zest__sync_wait_full(&thread_processor->sync);
    }

    zest_work_queue_t *queue = NULL;
    if (thread_processor->count > 0) {
        queue = thread_processor->queues[--thread_processor->count];
        zest__sync_signal_empty(&thread_processor->sync);
    }

    zest__sync_unlock(&thread_processor->sync);
    return queue;
}

ZEST_PRIVATE inline void zest__push_queue_work(zest_queue_processor_t *thread_processor, zest_work_queue_t *queue) {
    zest__sync_lock(&thread_processor->sync);

    while (thread_processor->count >= ZEST_MAX_QUEUES) {
        zest__sync_wait_empty(&thread_processor->sync);
    }

    thread_processor->queues[thread_processor->count++] = queue;
    zest__sync_signal_full(&thread_processor->sync);

    zest__sync_unlock(&thread_processor->sync);
}

ZEST_PRIVATE inline zest_bool zest__do_next_work_queue(zest_queue_processor_t *queue_processor) {
    zest_work_queue_t *queue = zest__get_queue_with_work(queue_processor);

    if (queue) {
        zest_uint original_read_entry = queue->next_read_entry;
        zest_uint new_original_read_entry = (original_read_entry + 1) % ZEST_MAX_QUEUE_ENTRIES;

        if (original_read_entry != queue->next_write_entry) {
            if (zest__atomic_compare_exchange(&queue->next_read_entry, new_original_read_entry, original_read_entry)) {
                zest_work_queue_entry_t entry = queue->entries[original_read_entry];
                entry.call_back(queue, entry.data);
                zest__atomic_increment(&queue->entry_completion_count);
            }
        }
    }
    return queue_processor->end_all_threads;
}

ZEST_PRIVATE inline void zest__do_next_work_queue_entry(zest_work_queue_t *queue) {
    zest_uint original_read_entry = queue->next_read_entry;
    zest_uint new_original_read_entry = (original_read_entry + 1) % ZEST_MAX_QUEUE_ENTRIES;

    if (original_read_entry != queue->next_write_entry) {
        if (zest__atomic_compare_exchange(&queue->next_read_entry, new_original_read_entry, original_read_entry)) {
            zest_work_queue_entry_t entry = queue->entries[original_read_entry];
            entry.call_back(queue, entry.data);
            zest__atomic_increment(&queue->entry_completion_count);
        }
    }
}

ZEST_PRIVATE inline void zest__add_work_queue_entry(zest_work_queue_t *queue, void *data, zest_work_queue_callback call_back) {
    if (!zest__globals->thread_count) {
        call_back(queue, data);
        return;
    }

    zest_uint new_entry_to_write = (queue->next_write_entry + 1) % ZEST_MAX_QUEUE_ENTRIES;
    while (new_entry_to_write == queue->next_read_entry) {        //Not enough room in work queue
        //We can do this because we're single producer
        zest__do_next_work_queue_entry(queue);
    }
    queue->entries[queue->next_write_entry].data = data;
    queue->entries[queue->next_write_entry].call_back = call_back;
    zest__atomic_increment(&queue->entry_completion_goal);

    zest__writebarrier;

    zest__push_queue_work(&zest__globals->thread_queues, queue);
    queue->next_write_entry = new_entry_to_write;

}

ZEST_PRIVATE inline void zest__complete_all_work(zest_work_queue_t *queue) {
    zest_work_queue_entry_t entry = { 0 };
    while (queue->entry_completion_goal != queue->entry_completion_count) {
        zest__do_next_work_queue_entry(queue);
    }
    queue->entry_completion_count = 0;
    queue->entry_completion_goal = 0;
}

#ifdef _WIN32
ZEST_PRIVATE unsigned WINAPI zest__thread_worker(void *arg);
#else
ZEST_PRIVATE void *zest__thread_worker(void *arg);
#endif

// Thread creation helper function
ZEST_PRIVATE bool zest__create_worker_thread(zest_storage_t * storage, int thread_index);

// Thread cleanup helper function
ZEST_PRIVATE inline void zest__cleanup_thread(zest_storage_t * storage, int thread_index) {
#ifdef _WIN32
    if (storage->threads[thread_index]) {
        WaitForSingleObject(storage->threads[thread_index], INFINITE);
        CloseHandle(storage->threads[thread_index]);
        storage->threads[thread_index] = NULL;
    }
#else
    pthread_join(storage->threads[thread_index], NULL);
#endif
}

ZEST_PRIVATE inline unsigned int zest_HardwareConcurrency(void) {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#else
#ifdef _SC_NPROCESSORS_ONLN
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return (count > 0) ? (unsigned int)count : 0;
#else
    return 0;
#endif
#endif
}

// Safe version that always returns at least 1
ZEST_API unsigned int zest_HardwareConcurrencySafe(void);

// Helper function to get a good default thread count for thread pools
// Usually hardware threads - 1 to leave a core for the OS/main thread
ZEST_API unsigned int zest_GetDefaultThreadCount(void);

// --Structs
// Vectors
// --Matrix and vector structs
typedef struct zest_vec2 {
    float x, y;
} zest_vec2;

typedef struct zest_ivec2 {
    int x, y;
} zest_ivec2;

typedef struct zest_vec3 {
    float x, y, z;
} zest_vec3;

typedef struct zest_ivec3 {
    int x, y, z;
} zest_ivec3;

typedef struct zest_bounding_box_t {
    zest_vec3 min_bounds;
    zest_vec3 max_bounds;
} zest_bounding_box_t;

typedef struct zest_extent2d_t {
    zest_uint width;
    zest_uint height;
} zest_extent2d_t;

typedef struct zest_extent3d_t {
    zest_uint width;
    zest_uint height;
    zest_uint depth;
} zest_extent3d_t;

typedef struct zest_scissor_rect_t {
    struct {
        int x;
        int y;
    } offset;
    struct {
        zest_uint width;
        zest_uint height;
    } extent;
} zest_scissor_rect_t;

//Note that using alignas on windows causes a crash in release mode relating to the allocator.
//Not sure why though. We need the align as on Mac otherwise metal complains about the alignment
//in the shaders
typedef struct zest_vec4 {
    union {
        struct { float x, y, z, w; };
        struct { float r, g, b, a; };
    };
} zest_vec4 ZEST_ALIGN_AFFIX(16);

typedef union zest_clear_value_t {
	zest_vec4 color;
	struct {
		float depth;
		zest_uint stencil;
	} depth_stencil;
} zest_clear_value_t;

typedef struct zest_matrix4 {
    zest_vec4 v[4];
} zest_matrix4 ZEST_ALIGN_AFFIX(16);

typedef struct zest_plane
{
    // unit vector
    zest_vec3 normal;

    // distance from origin to the nearest point in the plane
    float distance;
} zest_plane;

typedef struct zest_frustum
{
    zest_plane top_face;
    zest_plane bottom_face;

    zest_plane right_face;
    zest_plane left_face;

    zest_plane far_face;
    zest_plane near_face;
} zest_frustum;

typedef struct zest_rgba8 {
    union {
        struct { unsigned char r, g, b, a; };
        struct { zest_uint color; };
    };
} zest_rgba8;
typedef zest_rgba8 zest_color;

typedef struct zest_rgba32 {
    float r, g, b, a;
} zest_rgba32;

typedef struct zest_resource_identifier_t {
    int magic;
    zest_resource_type type;
} zest_resource_identifier_t;

typedef struct zest_memory_stats_t {
    zest_uint device_allocations;
    zest_uint renderer_allocations;
    zest_uint filter_count;
} zest_memory_stats_t;

typedef struct zest_camera_t {
    zest_vec3 position;
    zest_vec3 up;
    zest_vec3 right;
    zest_vec3 front;
    float yaw;
    float pitch;
    float fov;
    float ortho_scale;
    zest_camera_flags flags;
} zest_camera_t;

typedef struct zest_debug_t {
    zest_uint function_depth;
    zest_log_entry_t *frame_log;
} zest_debug_t;

// --Vulkan Buffer Management
typedef struct zest_buffer_type_t {
    zest_size alignment;
    zest_uint memory_type_index;
} zest_buffer_type_t;

typedef struct zest_buffer_info_t {
    zest_image_usage_flags image_usage_flags;
    zest_buffer_usage_flags usage_flags;                    //The usage state_flags of the memory block.
    zest_memory_property_flags property_flags;
    zest_uint memory_type_bits;
    zest_size alignment;
    zest_uint frame_in_flight;
    zest_memory_pool_flags flags;
    //This is an important field to avoid race conditions. Sometimes you may want to only update a buffer
    //when data has changed, like a ui buffer or any buffer that's only updated inside a fixed time loop.
    //This means that the frame in flight index for those buffers is decoupled from the main render loop 
    //frame in flight (ZEST_FIF). This can create a situation where a decoupled buffer has the same VkBuffer
    //as a transient buffer that's not decoupled. So what can happen is that a vkCmdDraw can use the same
    //VkBuffer that was used in the last frame. This should never be possible. A VkBuffer that's used in one
    //frame - it's last usage should be a minimum of 2 frames ago which means that the frame in flight fence
    //can properly act as syncronization. Therefore you can add this flag to any buffer that needs to be
    //decoupled from the main frame in flight index which will force a unique VkBuffer to be created that's 
    //separate from other buffer pools.
    //Note: I don't like this at all. Find another way.
    zest_uint unique_id;
} zest_buffer_info_t;

typedef struct zest_buffer_pool_size_t {
    const char *name;
    zest_size pool_size;
    zest_size minimum_allocation_size;
} zest_buffer_pool_size_t;

typedef struct zest_buffer_usage_t {
    zest_buffer_usage_flags usage_flags;
    zest_memory_property_flags property_flags;
    zest_image_usage_flags image_flags;
} zest_buffer_usage_t;

zest_hash_map(zest_buffer_pool_size_t) zest_map_buffer_pool_sizes;

typedef void* zest_pool_range;

typedef struct zest_buffer_details_t {
    zest_size size;
    zest_size memory_offset;
    zest_device_memory_pool memory_pool;
    zest_buffer_allocator buffer_allocator;
    zest_size memory_in_use;
} zest_buffer_details_t;

typedef struct zest_buffer_t {
    zest_size size;
    zest_size memory_offset;
    zest_size buffer_offset;
    int magic;
    zest_buffer_backend backend;
    zest_device_memory_pool memory_pool;
    zest_buffer_allocator buffer_allocator;
    zest_size memory_in_use;
    zest_uint array_index;
    void *data;
    void *end;
    //For releasing/acquiring in the frame graph:
    zest_uint owner_queue_family;
} zest_buffer_t;

typedef struct zest_uniform_buffer_t {
    int magic;
    zest_uniform_buffer_handle handle;
    zest_buffer buffer[ZEST_MAX_FIF];
    zest_descriptor_set descriptor_set[ZEST_MAX_FIF];
    zest_set_layout_handle set_layout;
    zest_uniform_buffer_backend backend;
} zest_uniform_buffer_t;

typedef struct zest_mip_index_collection {
    zest_global_binding_number binding_number;
    zest_uint *mip_indexes;
} zest_mip_index_collection;

zest_hash_map(zest_mip_index_collection) zest_map_mip_indexes;

typedef struct zest_buffer_copy_t {
    zest_size src_offset;
    zest_size dst_offset;
    zest_size size;
} zest_buffer_copy_t;

//Simple stuct for uploading buffers from the staging buffer to the device local buffers
typedef struct zest_buffer_uploader_t {
    zest_buffer_upload_flags flags;
    zest_buffer source_buffer;           //The source memory allocation (cpu visible staging buffer)
    zest_buffer target_buffer;           //The target memory allocation that we're uploading to (device local buffer)
    zest_buffer_copy_t *buffer_copies;   //List of vulkan copy info commands to upload staging buffers to the gpu each frame
} zest_buffer_uploader_t;

// --End Vulkan Buffer Management

typedef struct zest_image_view_create_info_t {
    zest_image_view_type view_type; 
    zest_uint base_mip_level;
    zest_uint level_count;
    zest_uint base_array_layer;
    zest_uint layer_count;
} zest_image_view_create_info_t;

typedef struct zest_image_view_array_t {
    int magic;
    zest_image_view_array_handle handle;
    zest_image image;
    zest_uint count;
    zest_image_view_t *views;
} zest_image_view_array_t;

typedef struct zest_image_view_t {
    zest_image image;
    zest_image_view_handle handle;
    zest_image_view_backend_t *backend;
} zest_image_view_t;

typedef struct zest_image_info_t {
    zest_extent3d_t extent;
    zest_uint mip_levels;
	zest_uint layer_count;
    zest_texture_format format;
    zest_image_aspect_flags aspect_flags;
    zest_sample_count_flags sample_count;
    zest_image_flags flags;
    zest_image_layout layout;
} zest_image_info_t;

typedef struct zest_image_t {
    int magic;
    zest_image_handle handle;
    zest_image_backend backend;
    zest_buffer buffer;
    zest_map_mip_indexes mip_indexes;
    zest_uint bindless_index[zest_max_global_binding_number];
    zest_image_info_t info;
    zest_image_view default_view;
} zest_image_t;

typedef struct zest_sampler_info_t {
    zest_filter_type mag_filter;
    zest_filter_type min_filter;
    zest_mipmap_mode mipmap_mode;
    zest_sampler_address_mode address_mode_u;
    zest_sampler_address_mode address_mode_v;
    zest_sampler_address_mode address_mode_w;
    zest_border_color border_color;
    float mip_lod_bias;
    zest_bool anisotropy_enable;
    float max_anisotropy;
    zest_bool compare_enable;
    zest_compare_op compare_op;
    float min_lod;
    float max_lod;
} zest_sampler_info_t;

typedef struct zest_sampler_t {
    int magic;
    zest_sampler_handle handle;
    zest_sampler_backend backend;
    zest_sampler_info_t create_info;
} zest_sampler_t;

typedef struct zest_execution_timeline_t {
    int magic;
    zest_execution_timeline_backend backend;
    zest_u64 current_value;
} zest_execution_timeline_t;

typedef struct zest_swapchain_t {
    int magic;
    zest_swapchain_backend backend;
    zest_window window;
    const char *name;
    zest_image_t *images;
    zest_image_view *views;
    zest_texture_format format;
    zest_vec2 resolution;
    zest_extent2d_t size;
    zest_clear_value_t clear_color;
    zest_uint current_image_frame;
    zest_uint image_count;
    zest_swapchain_flags flags;
} zest_swapchain_t;

typedef struct zest_window_t {
    int magic;
    void *window_handle;
    zest_window_backend backend;
    zest_swapchain swapchain;
    zest_uint window_width;
    zest_uint window_height;
    zest_bool framebuffer_resized;
    zest_window_mode mode;
} zest_window_t;

typedef struct zest_resource_usage_t {
    zest_resource_node resource_node;   
    zest_pipeline_stage_flags stage_mask;   // Pipeline stages this usage pertains to
    zest_access_flags access_mask;          // Vulkan access mask for barriers
    zest_image_layout image_layout;         // Required image layout if it's an image
    zest_image_aspect_flags aspect_flags; 
    zest_resource_purpose purpose;
    // For framebuffer attachments
    zest_load_op load_op;
    zest_store_op store_op;
    zest_load_op stencil_load_op;
    zest_store_op stencil_store_op;
    zest_clear_value_t clear_value;
    zest_bool is_output;
} zest_resource_usage_t;

typedef struct zest_ssbo_binding_t {
    int magic;
    const char *name;
    zest_uint max_buffers;
    zest_supported_shader_stages shader_stages;
    zest_uint binding_number;
} zest_ssbo_binding_t;

typedef struct zest_create_info_t {
    const char *title;                                  //Title that shows in the window
    const char *shader_path_prefix;                     //Prefix prepending to the shader path when loading default shaders
    const char* log_path;                               //path to the log to store log and validation messages
    zest_size memory_pool_size;                         //The size of each memory pool. More pools are added if needed
    zest_size frame_graph_allocator_size;               //The size of the linear allocator used by render graphs to store temporary data
    int screen_width, screen_height;                    //Default width and height of the window that you open
    int screen_x, screen_y;                             //Default position of the window
    int virtual_width, virtual_height;                  //The virtial width/height of the viewport
    int thread_count;                                   //The number of threads to use if multithreading. 0 if not.
    zest_millisecs fence_wait_timeout_ms;               //The amount of time the main loop fence should wait before timing out
    zest_millisecs max_fence_timeout_ms;                //The maximum amount of time to wait before giving up
    zest_texture_format color_format;                   //The format to use for the swapchain
    zest_init_flags flags;                              //Set flags to apply different initialisation options
    zest_uint maximum_textures;                         //The maximum number of textures you can load. 1024 is the default.
    zest_uint bindless_combined_sampler_2d_count;
    zest_uint bindless_combined_sampler_array_count;
    zest_uint bindless_combined_sampler_cube_count;
    zest_uint bindless_combined_sampler_3d_count;
    zest_uint bindless_sampler_count;
    zest_uint bindless_sampled_image_count;
    zest_uint bindless_storage_buffer_count;
    zest_uint bindless_storage_image_count;

    //Callbacks: use these to implement your own preferred window creation functionality
    void(*get_window_size_callback)(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
    void(*destroy_window_callback)(zest_window window, void *user_data);
    void(*poll_events_callback)(ZEST_PROTOTYPE);
    void(*add_platform_extensions_callback)(ZEST_PROTOTYPE);
    zest_window(*create_window_callback)(int x, int y, int width, int height, zest_bool maximised, const char* title);
    zest_bool(*create_window_surface_callback)(zest_window window);
    void(*set_window_mode_callback)(zest_window window, zest_window_mode mode);
    void(*set_window_size_callback)(zest_window window, int width, int height);
} zest_create_info_t;

zest_hash_map(zest_queue) zest_map_queue_value;

typedef struct zest_vulkan_memory_info_t {
    zest_millisecs timestamp;
    zest_uint context_info;
} zest_vulkan_memory_info_t;

zest_hash_map(const char *) zest_map_queue_names;
zest_hash_map(zest_text_t) zest_map_validation_errors;

typedef struct zest_queue_t {
    zest_uint family_index;
    //We decouple the frame in flight on the queue so that the counts don't get out of sync when the swap chain
    //can't be acquired for whatever reason.
    zest_uint fif;
    zest_u64 current_count[ZEST_MAX_FIF];
    zest_u64 signal_value;
    zest_bool has_waited;
    zest_uint next_buffer;
    zest_device_queue_type type;
    zest_queue_backend backend;
} zest_queue_t;

typedef struct zest_device_t {
    int magic;
    zest_uint api_version;
    zest_uint use_labels_address_bit;
    zest_uint previous_fif;
    zest_uint current_fif;
    zest_uint max_fif;
    zest_uint saved_fif;
    zest_uint max_image_size;
    void *memory_pools[ZEST_MAX_DEVICE_MEMORY_POOLS];
    zest_size memory_pool_sizes[ZEST_MAX_DEVICE_MEMORY_POOLS];
    zest_uint memory_pool_count;
    zloc_allocator *allocator;
    char **extensions;
    zest_vulkan_memory_info_t vulkan_memory_info;
    zest_uint allocation_id;
    zest_uint vector_id;

    zest_device_backend backend;

    zest_uint graphics_queue_family_index;
    zest_uint transfer_queue_family_index;
    zest_uint compute_queue_family_index;
    zest_queue_t graphics_queue;
    zest_queue_t compute_queue;
    zest_queue_t transfer_queue;
    zest_queue *queues;
    zest_map_queue_names queue_names;
    zest_text_t log_path;
    zest_create_info_t setup_info;

    zest_map_buffer_pool_sizes pool_sizes;

    zest_map_validation_errors validation_errors;
} zest_device_t;

typedef bool (*zest_wait_timeout_callback)(zest_millisecs total_wait_time_ms, zest_uint retry_count, void *user_data);

typedef struct zest_app_t {
    int magic;
    zest_create_info_t create_info;

    void(*update_callback)(zest_microsecs, void*);
    void *user_data;

    zest_microsecs current_elapsed;
    zest_microsecs current_elapsed_time;
    float update_time;
    float render_time;
    zest_microsecs frame_timer;

    double mouse_x;
    double mouse_y;
    double mouse_delta_x;
    double mouse_delta_y;
    zest_mouse_button mouse_button;
    zest_mouse_button mouse_hit;

    zest_uint frame_count;
    zest_uint last_fps;

    zest_app_flags flags;
    zest_u64 fence_wait_timeout_ns;
    zest_wait_timeout_callback fence_wait_timeout_callback;
} zest_app_t;

typedef struct zest_mip_push_constants_t {
    zest_vec4 settings;                     
    zest_vec2 source_resolution;            //the current size of the window/resolution
    zest_vec2 padding;
} zest_mip_push_constants_t;

typedef struct zest_gpu_timestamp_s {
    zest_u64 start;
    zest_u64 end;
} zest_gpu_timestamp_t;

typedef struct zest_timestamp_duration_s {
    double nanoseconds;
    double microseconds;
    double milliseconds;
} zest_timestamp_duration_t;

//frame_graph_types

typedef void (*zest_rg_execution_callback)(const zest_frame_graph_context context, void *user_data);
typedef void* zest_resource_handle;

typedef struct zest_buffer_description_t { 
    zest_size size;
    zest_buffer_info_t buffer_info;
} zest_buffer_description_t;

typedef struct zest_pass_adjacency_list_t {
    int *pass_indices;
} zest_pass_adjacency_list_t;

typedef struct zest_execution_wave_t {
    zest_uint level;
    zest_uint queue_bits;
    int *pass_indices;
} zest_execution_wave_t;

typedef struct zest_batch_key {
	zest_u64 next_pass_indexes;
	zest_uint current_family_index;
	zest_u64 next_family_indexes;
} zest_batch_key;

zest_hash_map(zest_resource_usage_t) zest_map_resource_usages;

typedef struct zest_pass_execution_callback_t {
    zest_rg_execution_callback callback;
    void *user_data;
} zest_pass_execution_callback_t;

typedef struct zest_resource_state_t {
    zest_uint pass_index;
    zest_resource_usage_t usage;
    zest_uint queue_family_index;
    zest_uint submission_id;
    bool was_released;
} zest_resource_state_t;

typedef struct zest_pass_queue_info_t {
    zest_queue queue;
    zest_uint queue_family_index;
    zest_pipeline_stage_flags timeline_wait_stage;
    zest_device_queue_type queue_type;
}zest_pass_queue_info_t;

typedef struct zest_execution_barriers_t {
    zest_execution_barriers_backend backend;
    zest_resource_node *acquire_image_barrier_nodes;
    zest_resource_node *acquire_buffer_barrier_nodes;
    zest_resource_node *release_image_barrier_nodes;
    zest_resource_node *release_buffer_barrier_nodes;
} zest_execution_barriers_t;

typedef struct zest_temp_attachment_info_t { 
    zest_resource_node resource_node; 
    zest_resource_usage_t *usage_info; 
    zest_uint attachment_slot;
} zest_temp_attachment_info_t;

typedef struct zest_image_resource_info_t { 
    zest_texture_format format; 
    zest_resource_usage_hint usage_hints;
    zest_uint width; 
    zest_uint height; 
    zest_uint mip_levels;
    zest_uint layer_count;
} zest_image_resource_info_t;

typedef struct zest_buffer_resource_info_t { 
    zest_resource_usage_hint usage_hints;
    zest_size size; 
} zest_buffer_resource_info_t;

zest_hash_map(zest_uint) attachment_idx;

typedef struct zest_execution_details_t {
	attachment_idx attachment_indexes;
	zest_temp_attachment_info_t *color_attachment_info;
	zest_temp_attachment_info_t *resolve_attachment_info;
	zest_temp_attachment_info_t depth_attachment_info;
	zest_resource_node *attachment_resource_nodes;
    zest_swapchain swapchain;
    zest_render_pass render_pass;
    zest_scissor_rect_t render_area;
    zest_clear_value_t *clear_values;

    zest_execution_barriers_t barriers;
    bool requires_dynamic_render_pass;
} zest_execution_details_t;

typedef struct zest_pass_node_t {
    int magic;
    zest_id id;
    const char *name;

    zest_pass_queue_info_t queue_info;

    zest_map_resource_usages inputs;
    zest_map_resource_usages outputs;
    zest_key output_key;
    zest_pass_execution_callback_t execution_callback;
    zest_compute compute;
    zest_pass_flags flags;
    zest_pass_type type;
    zest_pass_node_visit_state visit_state;
} zest_pass_node_t;

 typedef zest_buffer(*zest_resource_buffer_provider)(zest_resource_node resource);
 typedef zest_image_view(*zest_resource_image_provider)(zest_resource_node resource);

typedef struct zest_resource_node_t {
    int magic;
    const char *name;
    zest_resource_type type;
    zest_id id;                                     // Unique identifier within the graph
    zest_uint version;
    zest_uint original_id;
    zest_frame_graph frame_graph;
    zest_resource_node aliased_resource;
    zest_swapchain swapchain;

    zest_buffer_description_t buffer_desc; // Used if transient buffer
    zest_vec4 clear_color;

    zest_image_t image;
    zest_image_layout *linked_layout;
    zest_image_view view;
    zest_image_view_array view_array;
    zest_buffer storage_buffer;
    zest_uint bindless_index[zest_max_global_binding_number];   //The index to use in the shader
    zest_uint *mip_level_bindless_indexes;                      //The mip indexes to use in the shader

    zest_uint reference_count;

    zest_uint current_state_index;
    zest_uint current_queue_family_index;

	zest_image_layout image_layout;
	zest_access_flags access_mask;
	zest_pipeline_stage_flags last_stage_mask;

    zest_resource_state_t *journey;                 // List of the different states this resource has over the render graph, used to build barriers where needed
    int producer_pass_idx;                          // Index of the pass that writes/creates this resource (-1 if imported)
    int *consumer_pass_indices;                     // Dynamic array of pass indices that read this
    zest_resource_node_flags flags;                 // Imported/Transient Etc.
    zest_uint first_usage_pass_idx;                 // For lifetime tracking
    zest_uint last_usage_pass_idx;                  // For lifetime tracking

    zest_resource_buffer_provider buffer_provider;
    zest_resource_image_provider image_provider;
    void *user_data;
} zest_resource_node_t;

typedef struct zest_pass_group_t {
    zest_pass_queue_info_t queue_info;
    zest_map_resource_usages inputs;
    zest_map_resource_usages outputs;
    zest_resource_node *transient_resources_to_create;
    zest_resource_node *transient_resources_to_free;
    zest_uint submission_id;
    zest_execution_details_t execution_details;
    zest_pass_node *passes;
    zest_pass_flags flags;
} zest_pass_group_t;

typedef struct zest_binding_index_for_release_t {
    zest_set_layout_handle layout;
    zest_uint binding_index;
    zest_uint binding_number;
} zest_binding_index_for_release_t;

typedef struct zest_destruction_queue_t {
    void **resources[ZEST_MAX_FIF];
    zest_image_t *images[ZEST_MAX_FIF];
    zest_image_view_t *views[ZEST_MAX_FIF];
    zest_binding_index_for_release_t *binding_indexes[ZEST_MAX_FIF];
    zest_deferred_destruction_backend backend;
} zest_destruction_queue_t;

typedef struct zest_render_pass_t {
    int magic;
    zest_render_pass_backend backend;
} zest_render_pass_t;

typedef struct zest_semaphore_reference_t {
    zest_dynamic_resource_type type;
    zest_u64 semaphore;
} zest_semaphore_reference_t;

typedef struct zest_submission_batch_t {
    zest_uint magic;
    zest_queue queue;
    zest_uint queue_family_index;
    zest_device_queue_type queue_type;
    zest_uint *pass_indices;
    zest_bool waits_for_acquire_semaphore;
    zest_semaphore_reference_t *wait_semaphores;
    zest_semaphore_reference_t *signal_semaphores;
    zest_pipeline_stage_flags timeline_wait_stage;
    zest_pipeline_stage_flags queue_wait_stages;
	zest_pipeline_stage_flags *wait_stages;
    zest_pipeline_stage_flags *wait_dst_stage_masks;

    //References for printing the render graph only
	zest_u64 *wait_values;
	zest_u64 *signal_values;
    zest_bool need_timeline_wait;
    zest_bool need_timeline_signal;

    zest_submission_batch_backend backend;
} zest_submission_batch_t;

typedef struct zest_wave_submission_t {
    zest_uint queue_bits;
    zest_submission_batch_t batches[ZEST_QUEUE_COUNT];
} zest_wave_submission_t;

typedef struct zest_resource_versions_t {
    zest_resource_node *resources;
}zest_resource_versions_t;

typedef struct zest_cached_frame_graph_t {
    void *memory;
    zest_frame_graph frame_graph;
} zest_cached_frame_graph_t;

typedef struct zest_frame_graph_auto_state_t {
    zest_uint render_width;
    zest_uint render_height;
    zest_texture_format render_format;
} zest_frame_graph_auto_state_t;

typedef struct zest_frame_graph_cache_key_t {
    zest_frame_graph_auto_state_t auto_state;
    const void *user_state;
    zest_size user_state_size;
} zest_frame_graph_cache_key_t;

typedef struct zest_frame_graph_context_t {
    int magic;
    zest_frame_graph_context_backend backend;
    zest_render_pass render_pass;
    zest_frame_graph frame_graph;
    zest_pass_node pass_node;
    zest_uint submission_index;
    zest_uint queue_index;
    zest_pipeline_stage_flags timeline_wait_stage;
} zest_frame_graph_context_t;

typedef struct zest_frame_graph_semaphores_t {
    int magic;
    zest_frame_graph_semaphores_backend backend;
    zest_size values[ZEST_MAX_FIF][ZEST_QUEUE_COUNT];
} zest_frame_graph_semaphores_t;

zest_hash_map(zest_pass_group_t) zest_map_passes;
zest_hash_map(zest_resource_versions_t) zest_map_resource_versions;

typedef struct zest_frame_graph_t {
    int magic;
    zest_frame_graph_flags flags;
    zest_frame_graph_result error_status;
    zest_uint culled_passes_count;
    zest_uint culled_resources_count;
    const char *name;

    zest_bucket_array_t potential_passes;
    zest_map_passes final_passes;
    zest_bucket_array_t resources;
    zest_map_resource_versions resource_versions;
    zest_resource_node *resources_to_update;
    zest_pass_group_t **pass_execution_order;

    zest_execution_timeline *wait_on_timelines;
    zest_execution_timeline *signal_timelines;
    zest_frame_graph_semaphores semaphores;

    zest_execution_wave_t *execution_waves;         // Execution order after compilation

    zest_resource_node swapchain_resource;          // Handle to the current swapchain image resource
    zest_swapchain swapchain;                       // Handle to the current swapchain image resource
    zest_uint id_counter;
    zest_descriptor_pool descriptor_pool;           //Descriptor pool for execution nodes within the graph.
    zest_set_layout_handle bindless_layout;
    zest_descriptor_set bindless_set;

    zest_wave_submission_t *submissions;

    void *user_data;
    zest_frame_graph_context_t context;
    zest_key cache_key;

    zest_uint timestamp_count;
    zest_query_state query_state[ZEST_MAX_FIF];                      //For checking if the timestamp query is ready
    zest_gpu_timestamp_t *timestamps[ZEST_MAX_FIF];                  //The last recorded frame durations for the whole render pipeline

} zest_frame_graph_t;

// --- Internal render graph function ---
ZEST_PRIVATE zest_bool zest__is_stage_compatible_with_qfi(zest_pipeline_stage_flags stages_to_check, zest_device_queue_type queue_family_capabilities);
ZEST_PRIVATE zest_image_layout zest__determine_final_layout(zest_uint pass_index, zest_resource_node node, zest_resource_usage_t *current_usage);
ZEST_PRIVATE zest_image_aspect_flags zest__determine_aspect_flag(zest_texture_format format);
ZEST_PRIVATE void zest__interpret_hints(zest_resource_node resource, zest_resource_usage_hint usage_hints);
ZEST_PRIVATE void zest__deferr_image_destruction(zest_image image);
ZEST_PRIVATE zest_pass_node zest__add_pass_node(const char *name, zest_device_queue_type queue_type);
ZEST_PRIVATE zest_resource_node zest__add_frame_graph_resource(zest_resource_node resource);
ZEST_PRIVATE zest_resource_versions_t *zest__maybe_add_resource_version(zest_resource_node resource);
ZEST_PRIVATE zest_resource_node_t zest__create_import_image_resource_node(const char *name, zest_image image);
ZEST_PRIVATE zest_resource_node_t zest__create_import_buffer_resource_node(const char *name, zest_buffer buffer);
ZEST_PRIVATE zest_resource_node zest__import_swapchain_resource(zest_swapchain swapchain);
ZEST_PRIVATE zest_resource_node zest__add_transient_image_resource(const char *name, const zest_image_info_t *desc, zest_bool assign_bindless, zest_bool image_view_binding_only);
ZEST_PRIVATE zest_bool zest__create_transient_resource(zest_resource_node resource);
ZEST_PRIVATE void zest__free_transient_resource(zest_resource_node resource);
ZEST_PRIVATE void zest__add_pass_buffer_usage(zest_pass_node pass_node, zest_resource_node buffer_resource, zest_resource_purpose purpose, zest_pipeline_stage_flags relevant_pipeline_stages, zest_bool is_output);
ZEST_PRIVATE void zest__add_pass_image_usage(zest_pass_node pass_node, zest_resource_node image_resource, zest_resource_purpose purpose, zest_pipeline_stage_flags relevant_pipeline_stages, zest_bool is_output, zest_load_op load_op, zest_store_op store_op, zest_load_op stencil_load_op, zest_store_op stencil_store_op, zest_clear_value_t clear_value);
ZEST_PRIVATE zest_frame_graph zest__new_frame_graph(const char *name);
ZEST_PRIVATE zest_frame_graph zest__compile_frame_graph();
ZEST_PRIVATE zest_bool zest__execute_frame_graph(zest_bool is_intraframe);
ZEST_PRIVATE void zest__add_image_barriers(zest_frame_graph frame_graph, zloc_linear_allocator_t *allocator, zest_resource_node resource, zest_execution_barriers_t *barriers, 
                                           zest_resource_state_t *current_state, zest_resource_state_t *prev_state, zest_resource_state_t *next_state);
ZEST_PRIVATE zest_resource_usage_t zest__configure_image_usage(zest_resource_node resource, zest_resource_purpose purpose, zest_texture_format format, zest_load_op load_op, zest_load_op stencil_load_op, zest_pipeline_stage_flags relevant_pipeline_stages);
ZEST_PRIVATE zest_image_usage_flags zest__get_image_usage_from_state(zest_resource_state state);
ZEST_PRIVATE zest_submission_batch_t *zest__get_submission_batch(zest_uint submission_id);
ZEST_PRIVATE void zest__set_rg_error_status(zest_frame_graph frame_graph, zest_frame_graph_result result);
ZEST_PRIVATE zest_bool zest__detect_cyclic_recursion(zest_frame_graph frame_graph, zest_pass_node pass_node);
ZEST_PRIVATE void zest__cache_frame_graph(zest_frame_graph frame_graph);
ZEST_PRIVATE zest_key zest__hash_frame_graph_cache_key(zest_frame_graph_cache_key_t *cache_key);
ZEST_PRIVATE zest_frame_graph zest__get_cached_frame_graph(zest_key key);

// --- Dynamic resource callbacks ---
ZEST_PRIVATE zest_image_view zest__swapchain_resource_provider(zest_resource_node resource);
ZEST_PRIVATE zest_buffer zest__instance_layer_resource_provider(zest_resource_node resource);

// --- Utility callbacks ---
ZEST_API void zest_EmptyRenderPass(const zest_frame_graph_context context, void *user_data);

// --- General resource functions ---
ZEST_API zest_resource_node zest_GetPassInputResource(const zest_frame_graph_context context, const char *name);
ZEST_API zest_resource_node zest_GetPassOutputResource(const zest_frame_graph_context context, const char *name);
ZEST_API zest_buffer zest_GetPassInputBuffer(const zest_frame_graph_context context, const char *name);
ZEST_API zest_buffer zest_GetPassOutputBuffer(const zest_frame_graph_context context, const char *name);
ZEST_API zest_uint zest_GetResourceMipLevels(zest_resource_node resource);
ZEST_API zest_uint zest_GetResourceWidth(zest_resource_node resource);
ZEST_API zest_uint zest_GetResourceHeight(zest_resource_node resource);
ZEST_API zest_image zest_GetResourceImage(zest_resource_node resource_node);
ZEST_API zest_resource_type zest_GetResourceType(zest_resource_node resource_node);
ZEST_API zest_image_info_t zest_GetResourceImageDescription(zest_resource_node resource_node);
ZEST_API void zest_SetResourceClearColor(zest_resource_node resource, float red, float green, float blue, float alpha);

// -- Creating and Executing the render graph
ZEST_API bool zest_BeginFrameGraph(const char *name, zest_frame_graph_cache_key_t *cache_key);
ZEST_API bool zest_BeginFrameGraphSwapchain(zest_swapchain swapchain, const char *name, zest_frame_graph_cache_key_t *cache_key);
ZEST_API zest_frame_graph_cache_key_t zest_InitialiseCacheKey(zest_swapchain swapchain, const void *user_state, zest_size user_state_size);
ZEST_API void zest_ForceFrameGraphOnGraphicsQueue();
ZEST_API zest_frame_graph zest_EndFrameGraph();
ZEST_API zest_frame_graph zest_EndFrameGraphAndWait();

// --- Add pass nodes that execute user commands ---
ZEST_API zest_pass_node zest_BeginGraphicBlankScreen( const char *name);
ZEST_API zest_pass_node zest_BeginRenderPass(const char *name);
ZEST_API zest_pass_node zest_BeginComputePass(zest_compute_handle compute, const char *name);
ZEST_API zest_pass_node zest_BeginTransferPass(const char *name);
ZEST_API void zest_EndPass();


// --- Helper functions for acquiring bindless desriptor array indexes---
ZEST_API zest_uint zest_GetTransientImageBindlessIndex(const zest_frame_graph_context context, zest_resource_node resource, zest_sampler_handle sampler, zest_global_binding_number binding_number);
ZEST_API zest_uint *zest_GetTransientMipBindlessIndexes(const zest_frame_graph_context context, zest_resource_node resource, zest_sampler_handle sampler, zest_global_binding_number binding_number);
ZEST_API zest_uint zest_GetTransientBufferBindlessIndex(const zest_frame_graph_context context, zest_resource_node resource);

// --- Add callback tasks to passes
ZEST_API void zest_SetPassTask(zest_rg_execution_callback callback, void *user_data);
ZEST_API void zest_SetPassInstanceLayerUpload(zest_layer_handle layer);
ZEST_API void zest_SetPassInstanceLayer(zest_layer_handle layer);

// --- Add Transient resources ---
ZEST_API zest_resource_node zest_AddTransientImageResource(const char *name, zest_image_resource_info_t *info);
ZEST_API zest_resource_node zest_AddTransientBufferResource(const char *name, const zest_buffer_resource_info_t *info);
ZEST_API zest_resource_node zest_AddTransientLayerResource(const char *name, const zest_layer_handle layer, zest_bool prev_fif);
ZEST_API void zest_FlagResourceAsEssential(zest_resource_node resource);

// --- Render target groups ---
ZEST_API zest_output_group zest_CreateOutputGroup();
ZEST_API void zest_AddSwapchainToRenderTargetGroup(zest_output_group group);
ZEST_API void zest_AddImageToRenderTargetGroup(zest_output_group group, zest_resource_node image);

// --- Import external resouces into the render graph ---
ZEST_API zest_resource_node zest_ImportImageResource(const char *name, zest_image_handle image, zest_resource_image_provider provider);
ZEST_API zest_resource_node zest_ImportBufferResource(const char *name, zest_buffer buffer, zest_resource_buffer_provider provider);

// --- Manual Barrier Functions
ZEST_API void zest_ReleaseBufferAfterUse(zest_resource_node dst_buffer);

// --- Connect Resources to Pass Nodes ---
ZEST_API void zest_ConnectInput(zest_resource_node resource);
ZEST_API void zest_ConnectOutput(zest_resource_node resource);
ZEST_API void zest_ConnectSwapChainOutput();
ZEST_API void zest_ConnectGroupedOutput(zest_output_group group);

// --- Connect graphs to each other
ZEST_API void zest_WaitOnTimeline(zest_execution_timeline timeline);
ZEST_API void zest_SignalTimeline(zest_execution_timeline timeline);

// --- State check functions
ZEST_API bool zest_RenderGraphWasExecuted(zest_frame_graph frame_graph);

// --- Syncronization Helpers ---
ZEST_API zest_execution_timeline zest_CreateExecutionTimeline();

// -- General pass and resource getters/setters
ZEST_API zest_key zest_GetPassOutputKey(zest_pass_node pass);

// --- Render graph debug functions ---
ZEST_API zest_frame_graph_result zest_GetFrameGraphResult(zest_frame_graph frame_graph);
ZEST_API zest_uint zest_GetFrameGraphFinalPassCount(zest_frame_graph frame_graph);
ZEST_API zest_uint zest_GetFrameGraphPassTransientCreateCount(zest_frame_graph frame_graph, zest_key output_key);
ZEST_API zest_uint zest_GetFrameGraphPassTransientFreeCount(zest_frame_graph frame_graph, zest_key output_key);
ZEST_API zest_uint zest_GetFrameGraphCulledResourceCount(zest_frame_graph frame_graph);
ZEST_API zest_uint zest_GetFrameGraphCulledPassesCount(zest_frame_graph frame_graph);
ZEST_API zest_uint zest_GetFrameGraphSubmissionCount(zest_frame_graph frame_graph);
ZEST_API zest_uint zest_GetFrameGraphSubmissionBatchCount(zest_frame_graph frame_graph, zest_uint submission_index);
ZEST_API zest_uint zest_GetSubmissionBatchPassCount(const zest_submission_batch_t *batch);
ZEST_API const zest_submission_batch_t *zest_GetFrameGraphSubmissionBatch(zest_frame_graph frame_graph, zest_uint submission_index, zest_uint batch_index);
ZEST_API const zest_pass_group_t *zest_GetFrameGraphFinalPass(zest_frame_graph frame_graph, zest_uint pass_index);
ZEST_API void zest_PrintCompiledRenderGraph(zest_frame_graph frame_graph);
ZEST_API void zest_PrintCachedRenderGraph(zest_frame_graph_cache_key_t *cache_key);

// --- [Swapchain_helpers]
ZEST_API zest_swapchain zest_GetMainWindowSwapchain();
ZEST_API zest_texture_format zest_GetSwapchainFormat(zest_swapchain swapchain);
ZEST_API void zest_SetSwapchainClearColor(zest_swapchain swapchain, float red, float green, float blue, float alpha);
//End Swapchain helpers

typedef struct zest_descriptor_binding_desc_t {
    zest_uint binding;                      // The binding slot (register in HLSL, binding in GLSL, [[id(n)]] in MSL)
    zest_descriptor_type type;              // The generic resource type
    zest_uint count;                        // Number of descriptors. Use a special value (e.g., 0 or UINT32_MAX) for bindless/unbounded arrays.
    zest_supported_shader_stages stages;    // Which shader stages can access this (you already have this)
} zest_descriptor_binding_desc_t;

typedef struct zest_set_layout_builder_t {
    zest_descriptor_binding_desc_t *bindings;
    zest_u64 binding_capacity;
    zest_set_layout_builder_flags flags;
} zest_set_layout_builder_t;

typedef struct zest_layout_type_counts_t {
    zest_uint sampler_count;
    zest_uint sampler_image_count;
    zest_uint uniform_buffer_count;
    zest_uint storage_buffer_count;
    zest_uint combined_image_sampler_count;
} zest_layout_type_counts_t;

typedef struct zest_shader_t {
    int magic;
    zest_shader_handle handle;
    char *spv;
    zest_size spv_size;
    zest_text_t file_path;
    zest_text_t shader_code;
    zest_text_t name;
    shaderc_shader_kind type;
} zest_shader_t;

typedef struct zest_shader_resources_t {
    int magic;
    zest_shader_resources_backend backend;
} zest_shader_resources_t ZEST_ALIGN_AFFIX(16);

typedef struct zest_descriptor_pool_t {
    int magic;
    zest_descriptor_pool_backend backend;
    zest_uint max_sets;
    zest_uint allocations;
} zest_descriptor_pool_t;

typedef struct zest_descriptor_set_t {
    int magic;
    zest_descriptor_set_backend backend;
} zest_descriptor_set_t;

typedef struct zest_set_layout_t {
    int magic;
    zest_set_layout_backend backend;
    zest_set_layout_handle handle;
    zest_text_t name;
    zest_u64 binding_indexes;
    zest_descriptor_pool pool;
    zest_set_layout_flags flags;
} zest_set_layout_t;

typedef struct zest_descriptor_set_builder_t {
    VkWriteDescriptorSet *writes;
    zest_set_layout_handle associated_layout;
    zest_uint bindings;
    VkDescriptorImageInfo *image_infos_storage;
    VkDescriptorBufferInfo *buffer_infos_storage;
    VkBufferView *texel_buffer_view_storage;
} zest_descriptor_set_builder_t;

typedef struct zest_descriptor_infos_for_binding_t {
    VkDescriptorBufferInfo descriptor_buffer_info;
    VkDescriptorImageInfo descriptor_image_info;
    zest_image texture;
    zest_buffer buffer;
} zest_descriptor_infos_for_binding_t;

typedef struct zest_uniform_buffer_data_t {
    zest_matrix4 view;
    zest_matrix4 proj;
    zest_vec4 parameters1;
    zest_vec4 parameters2;
    zest_vec2 screen_size;
    zest_uint millisecs;
} zest_uniform_buffer_data_t;

typedef struct zest_cached_pipeline_key_t {
	zest_key pipeline_key;
	VkRenderPass render_pass;
} zest_cached_pipeline_key_t;

//Pipeline template is used with CreatePipeline to create a vulkan pipeline. Use PipelineTemplate() or SetPipelineTemplate with PipelineTemplateCreateInfo to create a PipelineTemplate
typedef struct zest_pipeline_template_t {
    int magic;
    zest_text_t name;                                                            //Name for the pipeline just for labelling it when listing all the renderer objects in debug
    VkPipelineShaderStageCreateInfo vertShaderStageInfo;
    VkPipelineShaderStageCreateInfo fragShaderStageInfo;
    VkShaderModule vertShaderCode;
    VkShaderModule fragShaderCode;
    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewportState;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    VkPipelineMultisampleStateCreateInfo multisampling;
    VkPipelineDepthStencilStateCreateInfo depthStencil;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo colorBlending;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    VkPushConstantRange pushConstantRange;
    VkPipelineDynamicStateCreateInfo dynamicState;
    zest_pipeline_set_flags flags;                                               //Flag bits
    zest_uint uniforms;                                                          //Number of uniform buffers in the pipeline, usually 1 or 0
    void *push_constants;                                                        //Pointer to user push constant data
    
    VkDescriptorSetLayout *descriptorSetLayouts;
    VkVertexInputAttributeDescription *attributeDescriptions;
    VkVertexInputBindingDescription *bindingDescriptions;
    VkDynamicState *dynamicStates;
    zest_bool no_vertex_input;
    zest_text_t vertShaderFunctionName;
    zest_text_t fragShaderFunctionName;
    zest_shader_handle vertex_shader;
    zest_shader_handle fragment_shader;

    zest_key *cached_pipeline_keys;
} zest_pipeline_template_t;

typedef struct zest_atlas_region_t {
    int magic;
    zest_index index;            //index within the QulkanTexture
    zest_text_t name;            //Filename of the image
    zest_uint width;
    zest_uint height;
    zest_vec4 uv;                //UV coords are set after the ProcessImages function is called and the images are packed into the texture
    zest_u64 uv_packed;          //UV packed into 16bit floats
    zest_index layer;            //the layer index of the image when it's packed into an image/texture array
    zest_uint frames;            //Will be one if this is a single image or more if it's part of and animation
    zest_uint top, left;         //the top left location of the image on the layer or spritesheet
    zest_vec2 min;               //The minimum coords of the vertices in the quad of the image
    zest_vec2 max;               //The maximum coords of the vertices in the quad of the image
    zest_vec2 scale;             //The scale of the image. 1.f if default, changing this will update the min/max coords
    zest_vec2 handle;            //The handle of the image. 0.5f is the center of the image
    float max_radius;            //You can load images and calculate the max radius of the image which is the furthest pixel from the center.
    zest_image image;            //Handle to the texture that this image belongs to
} zest_atlas_region_t;

typedef struct zest_bitmap_meta_t {
    int width;
    int height;
    int channels;
    int stride;
    size_t size;
    size_t offset;
} zest_bitmap_meta_t;

typedef struct zest_bitmap_t {
    int magic;
    zest_bitmap_meta_t meta;
    zest_text_t name;
    zest_byte *data;
    zest_bool is_imported;
} zest_bitmap_t;

typedef struct zest_bitmap_array_t {
    const char *name;
    zest_uint size_of_array;
    zest_bitmap_meta_t *meta;
    size_t total_mem_size;
    zest_byte *data;
} zest_bitmap_array_t;

typedef struct zest_buffer_image_copy_t {
    zest_size buffer_offset;
    zest_uint buffer_row_length;
    zest_uint buffer_image_height;
    zest_image_aspect_flags image_aspect;
    zest_uint mip_level;
    zest_uint base_array_layer;
    zest_uint layer_count;
    zest_ivec3 image_offset;
    zest_extent3d_t image_extent;
} zest_buffer_image_copy_t;

typedef struct zest_image_collection_t {
    int magic;
    zest_texture_format format;
    zest_bitmap_t *image_bitmaps;
    zest_atlas_region *regions;
    zest_bitmap_t *layers;
    zest_bitmap_array_t bitmap_array;
    zest_buffer_image_copy_t *buffer_copy_regions;
    zest_uint packed_border_size;
    zest_uint layer_count;
    zest_image_collection_flags flags;
} zest_image_collection_t;

//A pipeline set is all of the necessary things required to setup and maintain a pipeline
typedef struct zest_pipeline_t {
    int magic;
    zest_pipeline_backend backend;
    zest_pipeline_template pipeline_template;
    void(*rebuild_pipeline_function)(void*);                                     //Override the function to rebuild the pipeline when the swap chain is recreated
    zest_pipeline_set_flags flags;                                               //Flag bits
} zest_pipeline_t;

struct zest_compute_t {
    int magic;
    zest_compute_backend backend;
    zest_compute_handle handle;
    zest_set_layout_handle bindless_layout;                   // The bindless descriptor set layout to use in the compute shader
    zest_shader_handle *shaders;                              // List of compute shaders to use
    void *compute_data;                                       // Connect this to any custom data that is required to get what you need out of the compute process.
    void *user_data;                                          // Custom user data
    zest_compute_flags flags;
    zest_uint fif;                                            // Used for manual frame in flight compute
};

typedef struct zest_pipeline_descriptor_writes_t {
    VkWriteDescriptorSet *writes[ZEST_MAX_FIF];                       //Descriptor writes for creating the descriptor sets - is this needed here? only for certain pipeline_templates, textures store their own
} zest_pipeline_descriptor_writes_t;

zest_hash_map(zest_pipeline) zest_map_pipeline_variations;

typedef struct zest_sprite_instance_t {            //52 bytes
    zest_vec4 uv;                                  //The UV coords of the image in the texture packed into a u64 snorm (4 16bit floats)
    zest_vec4 position_rotation;                   //The position of the sprite with rotation in w and stretch in z
    zest_u64 size_handle;                          //Size of the sprite in pixels and the handle packed into a u64 (4 16bit floats)
    zest_uint alignment;                           //normalised alignment vector 2 floats packed into 16bits
    zest_color color;                              //The color tint of the sprite
    zest_uint intensity_texture_array;             //reference for the texture array (8bits) and intensity (24bits)
    zest_uint padding[3];
} zest_sprite_instance_t;

typedef struct zest_billboard_instance_t {         //56 bytes
    zest_vec3 position;                            //The position of the sprite
    zest_uint alignment;                           //Alignment x, y and z packed into a uint as 8bit floats
    zest_vec4 rotations_stretch;                   //Pitch, yaw, roll and stretch 
    zest_u64 uv;                                   //The UV coords of the image in the texture packed into a u64 snorm (4 16bit floats)
    zest_u64 scale_handle;                         //The scale and handle of the billboard packed into u64 (4 16bit floats)
    zest_uint intensity_texture_array;             //reference for the texture array (8bits) and intensity (24bits)
    zest_color color;                              //The color tint of the sprite
    zest_u64 padding;
} zest_billboard_instance_t;

//SDF Lines
typedef struct zest_shape_instance_t {
    zest_vec4 rect;                                //The rectangle containing the sdf shade, x,y = top left, z,w = bottom right
    zest_vec4 parameters;                          //Extra parameters, for example line widths, roundness, depending on the shape type
    zest_color start_color;                        //The color tint of the first point in the line
    zest_color end_color;                          //The color tint of the second point in the line
} zest_shape_instance_t;

//SDF 3D Lines
typedef struct zest_line_instance_t {
    zest_vec4 start;						        
    zest_vec4 end;								
    zest_color start_color;					
    zest_color end_color;				
} zest_line_instance_t;

typedef struct zest_textured_vertex_t {
    zest_vec3 pos;                                 //3d position
    float intensity;                               //Alpha level (can go over 1 to increase intensity of colors)
    zest_vec2 uv;                                  //Texture coordinates
    zest_color color;                              //packed color
    zest_uint parameters;                          //packed parameters such as texture layer
} zest_textured_vertex_t;

typedef struct zest_mesh_instance_t {
    zest_vec3 pos;                                 //3d position
    zest_color color;                              //packed color
    zest_vec3 rotation;
    zest_uint parameters;                          //packed parameters
    zest_vec3 scale;
} zest_mesh_instance_t;

typedef struct zest_vertex_t {
    zest_vec3 pos;                                 //3d position
    zest_color color;
    zest_vec3 normal;                              //3d normal
    zest_uint group;
} zest_vertex_t;

//We just have a copy of the ImGui Draw vert here so that we can setup things things for imgui
//should anyone choose to use it
typedef struct zest_ImDrawVert_t
{
    zest_vec2 pos;
    zest_vec2 uv;
    zest_uint col;
} zest_ImDrawVert_t;

typedef struct zest_push_constants_t {             //128 bytes seems to be the limit for push constants on AMD cards, NVidia 256 bytes
    zest_uint descriptor_index[4];
    zest_vec4 parameters1;                         //Can be used for anything
    zest_vec4 parameters2;                         //Can be used for anything
    zest_vec4 parameters3;                         //Can be used for anything
    zest_vec4 global;                              //Can be set every frame for all current draw instructions
} zest_push_constants_t ZEST_ALIGN_AFFIX(16);

typedef struct zest_layer_buffers_t {
    union {
		zest_buffer staging_vertex_data;
		zest_buffer staging_instance_data;
    };
	zest_buffer staging_index_data;
	zest_buffer device_vertex_data;

    union {
        struct { void *instance_ptr; };
        struct { void *vertex_ptr; };
    };

    zest_uint *index_ptr;

    zest_uint instance_count;
    zest_uint index_count;
    zest_uint index_position;
    zest_uint last_index;
    zest_uint vertex_count;
} zest_layer_buffers_t;

typedef struct zest_layer_instruction_t {
    char push_constant[128];                      //Each draw instruction can have different values in the push constants push_constants
    VkRect2D scissor;                             //The drawinstruction can also clip whats drawn
    VkViewport viewport;                          //The viewport size of the draw call
    zest_index start_index;                       //The starting index
    union {
        zest_uint total_instances;                //The total number of instances to be drawn in the draw instruction
        zest_uint total_indexes;                  //The total number of indexes to be drawn in the draw instruction
    };
    zest_index last_instance;                     //The last instance that was drawn in the previous instance instruction
    zest_pipeline_template pipeline_template;     //The pipeline template to draw the instances.
    zest_shader_resources_handle shader_resources;       //The descriptor set shader_resources used to draw with
    void *asset;                                  //Optional pointer to either texture, font etc
    zest_draw_mode draw_mode;
} zest_layer_instruction_t ZEST_ALIGN_AFFIX(16);

//Todo: do we need this now?
typedef struct zest_layer_builder_t {
    zest_size type_size;
    zest_uint initial_count;
} zest_layer_builder_t;

typedef struct zest_layer_t {
    int magic;
    zest_layer_handle handle;

    const char *name;

    zest_uint fif;
    zest_uint prev_fif;

    zest_layer_buffers_t memory_refs[ZEST_MAX_FIF];
    zest_bool dirty[ZEST_MAX_FIF];
    zest_uint initial_instance_pool_size;
    zest_buffer_uploader_t vertex_upload;
    zest_buffer_uploader_t index_upload;
    zest_descriptor_set bindless_set;

    zest_buffer vertex_data;
    zest_buffer index_data;

    zest_layer_instruction_t current_instruction;

    union {
        struct { zest_size instance_struct_size; };
        struct { zest_size vertex_struct_size; };
    };

    zest_color current_color;
    float intensity;

    zest_vec2 layer_size;
    zest_vec2 screen_scale;
    zest_uint index_count;

    VkRect2D scissor;
    VkViewport viewport;

    zest_layer_instruction_t *draw_instructions[ZEST_MAX_FIF];
    zest_draw_mode last_draw_mode;

    zest_resource_node vertex_buffer_node;
    zest_resource_node index_buffer_node;

    zest_layer_flags flags;
    void *user_data;
} zest_layer_t ZEST_ALIGN_AFFIX(16);


typedef struct zest_compute_builder_t {
    zest_set_layout_handle *non_bindless_layouts;
    zest_set_layout_handle bindless_layout;
    zest_shader_handle *shaders;
    zest_uint push_constant_size;
    zest_compute_flags flags;
    void *user_data;
} zest_compute_builder_t;

zest_hash_map(zest_descriptor_set_builder_t) zest_map_texture_descriptor_builders;

typedef struct zest_imgui_image_t {
    int magic;
    zest_atlas_region image;
    zest_pipeline_template pipeline;
    zest_shader_resources_handle shader_resources;
    zest_push_constants_t push_constants;
} zest_imgui_image_t;

typedef struct zest_framebuffer_attachment_t {
    VkImage image;
    zest_buffer_t *buffer;
    VkImageView view;
    VkFormat format;
} zest_framebuffer_attachment_t;

typedef struct zest_report_t {
    zest_text_t message;
    zest_report_category category;
    int count;
    const char *file_name;
    int line_number;
} zest_report_t;

zest_hash_map(zest_frame_graph_semaphores) zest_map_rg_semaphores;
zest_hash_map(zest_render_pass) zest_map_render_passes;
zest_hash_map(zest_pipeline) zest_map_cached_pipelines;
zest_hash_map(zest_buffer_allocator) zest_map_buffer_allocators;
zest_hash_map(zest_sampler_handle) zest_map_samplers;
zest_hash_map(zest_descriptor_pool) zest_map_descriptor_pool;
zest_hash_map(zest_frame_graph) zest_map_frame_graphs;
zest_hash_map(zest_report_t) zest_map_reports;
zest_hash_map(zest_cached_frame_graph_t) zest_map_cached_frame_graphs;

typedef struct zest_builtin_shaders_t {
    zest_shader_handle sprite_frag;
    zest_shader_handle sprite_vert;
    zest_shader_handle font_frag;
    zest_shader_handle mesh_vert;
    zest_shader_handle mesh_instance_vert;
    zest_shader_handle mesh_instance_frag;
    zest_shader_handle swap_vert;
    zest_shader_handle swap_frag;
} zest_builtin_shaders_t;

typedef struct zest_builtin_pipeline_templates_t {
    zest_pipeline_template sprites;
    zest_pipeline_template fonts;
    zest_pipeline_template mesh;
    zest_pipeline_template instanced_mesh;
    zest_pipeline_template swap;
} zest_builtin_pipeline_templates_t;

typedef struct zest_renderer_t {
    int magic;

    zest_execution_timeline *timeline_semaphores;

    zest_u64 total_frame_count;

    zest_extent2d_t window_extent;
    float dpi_scale;

    zest_set_layout_handle global_bindless_set_layout;
    zest_descriptor_set global_set;

    zest_map_buffer_allocators buffer_allocators;

    zest_renderer_backend backend;

    zest_uint fence_count[ZEST_MAX_FIF];

    //Built in shaders that I'll probably remove soon
    zest_builtin_shaders_t builtin_shaders;
    zest_builtin_pipeline_templates_t pipeline_templates;

    //Context data
    zest_frame_graph current_frame_graph;
    zest_pass_node current_pass;
    zest_frame_graph *frame_graphs;       //All the render graphs used this frame. Gets cleared at the beginning of each frame
    zest_swapchain last_acquired_swapchain;
    zest_swapchain main_swapchain;

    //Linear allocator for building the render graph each frame
    zloc_linear_allocator_t *frame_graph_allocator[ZEST_MAX_FIF];

    //Small utility allocator for per function tempory allocations
    zloc_linear_allocator_t *utility_allocator;

    //General Cache Storage
    zest_map_render_passes cached_render_passes;
    zest_map_cached_frame_graphs cached_frame_graphs;
    zest_map_cached_pipelines cached_pipelines;
    zest_map_rg_semaphores cached_frame_graph_semaphores;

    //Resource storage
    zest_resource_store_t shader_resources;
    zest_resource_store_t images;
    zest_resource_store_t views;
    zest_resource_store_t view_arrays;
    zest_resource_store_t samplers;
    zest_resource_store_t uniform_buffers;
    zest_resource_store_t timers;
    zest_resource_store_t layers;
    zest_resource_store_t shaders;
    zest_resource_store_t compute_pipelines;
    zest_resource_store_t set_layouts;

    zest_window main_window;

    //Cache of the supported depth format
    zest_texture_format depth_format;

    //For scheduled tasks
    zest_buffer *staging_buffers;
    zest_destruction_queue_t deferred_resource_freeing_list;

    //Each texture has a simple descriptor set with a combined image sampler for debugging purposes
    //allocated from this pool and using the layout
    zest_set_layout_handle texture_debug_layout;

    //Threading
    zest_work_queue_t work_queue;

    //Flags
    zest_renderer_flags flags;
    
    //Optional prefix path for loading shaders
    zest_text_t shader_path_prefix;

    //Debugging
    zest_debug_t debug;
    zest_map_reports reports;

    //Callbacks for customising window and surface creation
    void(*get_window_size_callback)(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
    void(*destroy_window_callback)(zest_window window, void *user_data);
    void(*poll_events_callback)(ZEST_PROTOTYPE);
    void(*add_platform_extensions_callback)(ZEST_PROTOTYPE);
    zest_window(*create_window_callback)(int x, int y, int width, int height, zest_bool maximised, const char* title);
    void(*create_window_surface_callback)(zest_window window);
    void(*set_window_mode_callback)(zest_window window, zest_window_mode mode);
    void(*set_window_size_callback)(zest_window window, int width, int height);

    //Slang
    void *slang_info;
} zest_renderer_t;

typedef struct zest_platform_t {
    //Frame Graph Platform Commands
    zest_bool                  (*begin_command_buffer)(const zest_frame_graph_context context);
    void                       (*end_command_buffer)(const zest_frame_graph_context context);
    zest_bool                  (*set_next_command_buffer)(const zest_frame_graph_context context, zest_queue queue);
    void                       (*acquire_barrier)(const zest_frame_graph_context context, zest_execution_details_t *exe_details);
    void                       (*release_barrier)(const zest_frame_graph_context context, zest_execution_details_t *exe_details);
    void*                      (*new_execution_backend)(zloc_linear_allocator_t *allocator);
	void                       (*set_execution_fence)(zest_execution_backend backend, zest_bool is_intraframe);
	zest_frame_graph_semaphores(*get_frame_graph_semaphores)(const char *name);
    zest_bool                  (*submit_frame_graph_batch)(zest_frame_graph frame_graph, zest_execution_backend backend, zest_submission_batch_t *batch, zest_map_queue_value *queues);
    zest_frame_graph_result    (*create_fg_render_pass)(zest_pass_group_t *pass, zest_execution_details_t *exe_details, zest_uint current_pass_index);
    zest_bool                  (*begin_render_pass)(const zest_frame_graph_context context, zest_execution_details_t *exe_details);
    void                       (*end_render_pass)(const zest_frame_graph_context context);
    void                       (*carry_over_semaphores)(zest_frame_graph frame_graph, zest_wave_submission_t *wave_submission, zest_execution_backend backend);
    zest_bool                  (*frame_graph_fence_wait)(zest_execution_backend backend);
    zest_bool                  (*create_execution_timeline_backend)(zest_execution_timeline timeline);
    void                       (*add_frame_graph_buffer_barrier)(zest_resource_node resource, zest_execution_barriers_t *barriers, 
                                     zest_bool acquire, zest_access_flags src_access, zest_access_flags dst_access,
                                     zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, 
                                     zest_pipeline_stage_flags dst_stage);
    void                       (*add_frame_graph_image_barrier)(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire,
								zest_access_flags src_access, zest_access_flags dst_access, zest_image_layout old_layout, zest_image_layout new_layout,
								zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage);
    void                       (*validate_barrier_pipeline_stages)(zest_execution_barriers_t *barriers);
    void                       (*print_compiled_frame_graph)(zest_frame_graph frame_graph);
    zest_bool                  (*present_frame)(zest_swapchain);
    zest_bool                  (*dummy_submit_for_present_only)(void);
    zest_bool                  (*acquire_swapchain_image)(zest_swapchain swapchain);
    //Set layouts
    zest_bool                  (*create_set_layout)(zest_set_layout_builder_t *builder, zest_set_layout layout, zest_bool is_bindless);
    zest_bool                  (*create_set_pool)(zest_set_layout layout, zest_uint max_set_count, zest_bool bindles);
    zest_descriptor_set        (*create_bindless_set)(zest_set_layout layout);
    //Create backends
    void*                      (*new_frame_graph_semaphores_backend)(void);
    void*                      (*new_execution_barriers_backend)(zloc_linear_allocator_t *allocator);
    void*                      (*new_deferred_desctruction_backend)(void);
    //Cleanup backends
    void                       (*cleanup_frame_graph_semaphore)(zest_frame_graph_semaphores semaphores);
    void                       (*cleanup_render_pass)(zest_render_pass render_pass);
    void                       (*cleanup_image_backend)(zest_image image);
    void                       (*cleanup_image_view_backend)(zest_image_view image_view);
    void                       (*cleanup_deferred_framebuffers)(void);
    void                       (*cleanup_deferred_destruction_backend)(void);
    //Misc
    void                       (*deferr_framebuffer_destruction)(void* frame_buffer);
} zest_platform_t;

extern zest_device_t *ZestDevice;
extern zest_app_t *ZestApp;
extern zest_renderer_t *ZestRenderer;
extern zest_platform_t *ZestPlatform;

ZEST_GLOBAL const char* zest_validation_layers[zest__validation_layer_count] = {
    "VK_LAYER_KHRONOS_validation"
};

ZEST_GLOBAL const char* zest_required_extensions[zest__required_extension_names_count] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if defined(__APPLE__)
    "VK_KHR_portability_subset"
#endif
};

//--Internal_functions

//Platform_dependent_functions
//These functions need a different implementation depending on the platform being run on
//See definitions at the top of zest.c
ZEST_PRIVATE zest_window zest__os_create_window(int x, int y, int width, int height, zest_bool maximised, const char* title);
ZEST_PRIVATE zest_bool zest__os_create_window_surface(zest_window window);
ZEST_PRIVATE void zest__os_set_window_mode(zest_window window, zest_window_mode mode);
ZEST_PRIVATE void zest__os_set_window_size(zest_window window, int width, int height);
ZEST_PRIVATE void zest__os_poll_events(ZEST_PROTOTYPE);
ZEST_PRIVATE void zest__os_add_platform_extensions(ZEST_PROTOTYPE);
ZEST_PRIVATE void zest__os_set_window_title(const char *title);
ZEST_PRIVATE bool zest__create_folder(const char *path);
//-- End Platform dependent functions

// -- Grapics_API_Backends
ZEST_PRIVATE void *zest__new_device_backend(void);
ZEST_PRIVATE void *zest__new_renderer_backend(void);
ZEST_PRIVATE void *zest__new_frame_graph_context_backend(void);
ZEST_PRIVATE void *zest__new_swapchain_backend(void);
ZEST_PRIVATE void *zest__new_buffer_backend(void);
ZEST_PRIVATE void *zest__new_uniform_buffer_backend(void);
ZEST_PRIVATE void zest__set_uniform_buffer_backend(zest_uniform_buffer buffer);
ZEST_PRIVATE void *zest__new_image_backend(void);
ZEST_PRIVATE void *zest__new_frame_graph_image_backend(zloc_linear_allocator_t *allocator);
ZEST_PRIVATE void *zest__new_compute_backend(void);
ZEST_PRIVATE void *zest__new_queue_backend(void);
ZEST_PRIVATE void *zest__new_submission_batch_backend(void);
ZEST_PRIVATE void *zest__new_set_layout_backend(void);
ZEST_PRIVATE void *zest__new_descriptor_pool_backend(void);
ZEST_PRIVATE void *zest__new_sampler_backend(void);
ZEST_PRIVATE zest_bool zest__finish_compute(zest_compute_builder_t *builder, zest_compute compute);
ZEST_PRIVATE void zest__cleanup_set_layout_backend(zest_set_layout set_layout);
ZEST_PRIVATE void zest__cleanup_swapchain_backend(zest_swapchain swapchain, zest_bool for_recreation);
ZEST_PRIVATE void zest__cleanup_window_backend(zest_window window);
ZEST_PRIVATE void zest__cleanup_buffer_backend(zest_buffer buffer);
ZEST_PRIVATE void zest__cleanup_uniform_buffer_backend(zest_uniform_buffer buffer);
ZEST_PRIVATE void zest__cleanup_compute_backend(zest_compute compute);
ZEST_PRIVATE void zest__cleanup_pipeline_backend(zest_pipeline pipeline);
ZEST_PRIVATE void zest__cleanup_sampler_backend(zest_sampler sampler);
ZEST_PRIVATE void zest__cleanup_queue_backend(zest_queue queue);
ZEST_PRIVATE void zest__cleanup_image_view_array_backend(zest_image_view_array view);
ZEST_PRIVATE void zest__cleanup_descriptor_backend(zest_set_layout layout, zest_descriptor_set set);
ZEST_PRIVATE void zest__cleanup_shader_resources_backend(zest_shader_resources shader_resources);

//Only available outside lib for some implementations like SDL2
ZEST_API void* zest__vec_reserve(void *T, zest_uint unit_size, zest_uint new_capacity);
ZEST_PRIVATE void* zest__vec_linear_reserve(zloc_linear_allocator_t *allocator, void *T, zest_uint unit_size, zest_uint new_capacity);

//Buffer_and_Memory_Management
ZEST_PRIVATE void zest__add_host_memory_pool(zest_size size);
ZEST_PRIVATE void *zest__allocate(zest_size size);
ZEST_PRIVATE void *zest__allocate_aligned(zest_size size, zest_size alignment);
ZEST_PRIVATE void *zest__reallocate(void *memory, zest_size size);
ZEST_PRIVATE VkResult zest__bind_buffer_memory(zest_buffer buffer, VkDeviceSize offset);
ZEST_PRIVATE VkResult zest__map_memory(zest_device_memory_pool memory_allocation, VkDeviceSize size, VkDeviceSize offset);
ZEST_PRIVATE void zest__unmap_memory(zest_device_memory_pool memory_allocation);
ZEST_PRIVATE void zest__destroy_memory(zest_device_memory_pool memory_allocation);
ZEST_PRIVATE VkResult zest__flush_memory(zest_device_memory_pool memory_allocation, VkDeviceSize size, VkDeviceSize offset);
ZEST_PRIVATE VkResult zest__create_vk_memory_pool(zest_buffer_info_t *buffer_info, VkImage image, zest_key key, zest_size minimum_size, zest_device_memory_pool *memory_pool);
ZEST_PRIVATE void zest__add_remote_range_pool(zest_buffer_allocator buffer_allocator, zest_device_memory_pool buffer_pool);
ZEST_PRIVATE void zest__set_buffer_details(zest_buffer_allocator buffer_allocator, zest_buffer buffer, zest_bool is_host_visible);
ZEST_PRIVATE void zest__cleanup_buffers_in_allocators();
//End Buffer Management

//Renderer_functions
ZEST_PRIVATE VkResult zest__initialise_renderer(zest_create_info_t *create_info);
ZEST_PRIVATE zest_swapchain zest__create_swapchain(const char *name);
ZEST_PRIVATE zest_bool zest__initialise_swapchain(zest_swapchain swapchain, zest_window window);
ZEST_PRIVATE VkResult zest__create_pipeline_cache();
ZEST_PRIVATE void zest__get_window_size_callback(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
ZEST_PRIVATE void zest__destroy_window_callback(zest_window window, void *user_data);
ZEST_PRIVATE void zest__cleanup_swapchain(zest_swapchain swapchain, zest_bool for_recreation);
ZEST_PRIVATE void zest__cleanup_device(void);
ZEST_PRIVATE void zest__cleanup_renderer(void);
ZEST_PRIVATE void zest__cleanup_shader_resource_store(void);
ZEST_PRIVATE void zest__cleanup_image_store(void);
ZEST_PRIVATE void zest__cleanup_sampler_store(void);
ZEST_PRIVATE void zest__cleanup_uniform_buffer_store(void);
ZEST_PRIVATE void zest__cleanup_timer_store(void);
ZEST_PRIVATE void zest__cleanup_layer_store(void);
ZEST_PRIVATE void zest__cleanup_shader_store(void);
ZEST_PRIVATE void zest__cleanup_compute_store(void);
ZEST_PRIVATE void zest__cleanup_set_layout_store(void);
ZEST_PRIVATE void zest__cleanup_view_store(void);
ZEST_PRIVATE void zest__cleanup_view_array_store(void);
ZEST_PRIVATE void zest__free_handle(void *handle);
ZEST_PRIVATE void zest__scan_memory_and_free_resources();
ZEST_PRIVATE void zest__cleanup_compute(zest_compute compute);
ZEST_PRIVATE zest_bool zest__recreate_swapchain(zest_swapchain swapchain);
ZEST_PRIVATE void zest__reset_queue_command_pool(zest_queue queue);
ZEST_PRIVATE void zest__add_line(zest_text_t *text, char current_char, zest_uint *position, zest_uint tabs);
ZEST_PRIVATE void zest__format_shader_code(zest_text_t *code);
ZEST_PRIVATE void zest__compile_builtin_shaders(zest_bool compile_shaders);
ZEST_PRIVATE void zest__create_debug_layout_and_pool(zest_uint max_texture_count);
ZEST_PRIVATE void zest__prepare_standard_pipelines(void);
ZEST_PRIVATE void zest__cleanup_pipelines(void);
ZEST_PRIVATE VkResult zest__rebuild_pipeline(zest_pipeline pipeline);
ZEST_PRIVATE zest_render_pass zest__create_render_pass(void);
// --End Renderer functions

// --Draw_layer_internal_functions
ZEST_PRIVATE zest_layer_handle zest__new_layer(zest_layer *layer);
ZEST_PRIVATE void zest__start_mesh_instructions(zest_layer layer);
ZEST_PRIVATE void zest__end_mesh_instructions(zest_layer layer);
ZEST_PRIVATE void zest__update_instance_layer_resolution(zest_layer layer);
ZEST_PRIVATE zest_layer_instruction_t zest__layer_instruction(void);
ZEST_PRIVATE void zest__reset_mesh_layer_drawing(zest_layer layer);
ZEST_PRIVATE zest_bool zest__grow_instance_buffer(zest_layer layer, zest_size type_size, zest_size minimum_size);
ZEST_PRIVATE void zest__cleanup_layer(zest_layer layer);
ZEST_PRIVATE void zest__initialise_instance_layer(zest_layer layer, zest_size type_size, zest_uint instance_pool_size);
ZEST_PRIVATE void zest__end_instance_instructions(zest_layer layer);
ZEST_PRIVATE void zest__start_instance_instructions(zest_layer layer);
ZEST_PRIVATE void zest__reset_instance_layer_drawing(zest_layer layer);

// --Image_internal_functions
ZEST_PRIVATE zest_image_handle zest__new_image(void);
ZEST_PRIVATE void zest__release_all_global_texture_indexes(zest_image image);
ZEST_PRIVATE void zest__tinyktxCallbackError(void *user, char const *msg);
ZEST_PRIVATE void *zest__tinyktxCallbackAlloc(void *user, size_t size);
ZEST_PRIVATE void zest__tinyktxCallbackFree(void *user, void *data);
ZEST_PRIVATE size_t zest__tinyktxCallbackRead(void *user, void *data, size_t size);
ZEST_PRIVATE bool zest__tinyktxCallbackSeek(void *user, int64_t offset);
ZEST_PRIVATE int64_t zest__tinyktxCallbackTell(void *user);
ZEST_API zest_image_collection zest__load_ktx(const char *file_path);
ZEST_PRIVATE VkFormat zest__convert_tktx_format(TinyKtx_Format format);
ZEST_PRIVATE void zest__update_image_vertices(zest_atlas_region image);
ZEST_PRIVATE zest_uint zest__get_format_channel_count(zest_texture_format format);
ZEST_PRIVATE void zest__cleanup_image_view(zest_image_view layout);
ZEST_PRIVATE void zest__cleanup_image_view_array(zest_image_view_array layout);

// --General_layer_internal_functions
ZEST_PRIVATE zest_layer_handle zest__create_instance_layer(const char *name, zest_size instance_type_size, zest_uint initial_instance_count);

// --Mesh_layer_internal_functions
ZEST_PRIVATE void zest__initialise_mesh_layer(zest_layer mesh_layer, zest_size vertex_struct_size, zest_size initial_vertex_capacity);

// --General_Helper_Functions
ZEST_PRIVATE zest_image_view_type zest__get_image_view_type(zest_image image);
ZEST_PRIVATE zest_image_view_t *zest__create_image_view_backend(zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator);
ZEST_PRIVATE zest_image_view_array zest__create_image_views_per_mip_backend(zest_image image, zest_image_view_type view_type, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator);
ZEST_PRIVATE VkResult zest__create_temporary_image(zest_uint width, zest_uint height, zest_uint mipLevels, VkSampleCountFlagBits num_samples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image, VkDeviceMemory *memory);
ZEST_PRIVATE zest_bool zest__create_image(zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags);
ZEST_PRIVATE zest_bool zest__create_transient_image(zest_resource_node node);
ZEST_PRIVATE void zest__create_transient_buffer(zest_resource_node node);
ZEST_PRIVATE zest_bool zest__copy_buffer_to_image(zest_buffer buffer, zest_size src_offset, zest_image image, zest_uint width, zest_uint height);
ZEST_PRIVATE zest_bool zest__copy_buffer_regions_to_image(zest_buffer_image_copy_t *regions, zest_buffer buffer, zest_size src_offset, zest_image image);
ZEST_PRIVATE zest_bool zest__create_sampler(zest_sampler sampler);
ZEST_PRIVATE int zest__get_image_raw_layout(zest_image image);
ZEST_PRIVATE zest_bool zest__transition_image_layout(zest_image image, zest_image_layout new_layout, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count);
ZEST_PRIVATE zest_bool zest__generate_mipmaps(zest_image image);
ZEST_PRIVATE VkImageMemoryBarrier zest__create_base_image_memory_barrier(VkImage image);
ZEST_PRIVATE void zest__insert_image_memory_barrier(VkCommandBuffer cmdbuffer, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange);
ZEST_PRIVATE void zest__place_image_barrier(VkCommandBuffer command_buffer, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage, VkImageMemoryBarrier *barrier);
ZEST_PRIVATE VkFormat zest__find_depth_format(void);
ZEST_PRIVATE VkFormat zest__find_supported_format(VkFormat *candidates, zest_uint candidates_count, VkImageTiling tiling, VkFormatFeatureFlags features);
ZEST_PRIVATE zest_bool zest__begin_single_time_commands();
ZEST_PRIVATE zest_bool zest__end_single_time_commands();
ZEST_PRIVATE zest_index zest__next_fif(void);
// --End General Helper Functions

// --Pipeline_Helper_Functions
ZEST_PRIVATE void zest__set_pipeline_template(zest_pipeline_template pipeline_template);
ZEST_PRIVATE void zest__update_pipeline_template(zest_pipeline_template pipeline_template);
ZEST_PRIVATE VkResult zest__create_shader_module(char *code, VkShaderModule *shader_module);
ZEST_PRIVATE zest_pipeline zest__create_pipeline(void);
ZEST_PRIVATE VkResult zest__cache_pipeline(zest_pipeline_template pipeline_template, zest_render_pass render_pass, zest_key key, zest_pipeline *out_pipeline);
ZEST_PRIVATE zest_uint zest__get_vk_format_size(VkFormat format);
ZEST_PRIVATE zest_bool zest__build_pipeline(zest_pipeline pipeline);
ZEST_PRIVATE void zest__cleanup_pipeline_template(zest_pipeline_template pipeline);
// --End Pipeline Helper Functions

// --Buffer_allocation_funcitons
ZEST_PRIVATE VkResult zest__create_device_memory_pool(VkDeviceSize size, zest_buffer_info_t *buffer_info, zest_device_memory_pool buffer, const char *name);
ZEST_PRIVATE VkResult zest__create_image_memory_pool(VkDeviceSize size_in_bytes, VkImage image, zest_buffer_info_t *buffer_info, zest_device_memory_pool buffer);
ZEST_PRIVATE zest_size zest__get_minimum_block_size(zest_size pool_size);
ZEST_PRIVATE void zest__on_add_pool(void *user_data, void *block);
ZEST_PRIVATE void zest__on_split_block(void *user_data, zloc_header* block, zloc_header *trimmed_block, zest_size remote_size);
// --End Buffer allocation funcitons

// --Uniform_buffers
// --End Uniform Buffers

// --Maintenance_functions
ZEST_PRIVATE void zest__cleanup_image(zest_image image);
ZEST_PRIVATE void zest__cleanup_sampler(zest_sampler sampler);
ZEST_PRIVATE void zest__cleanup_uniform_buffer(zest_uniform_buffer uniform_buffer);
// --End Maintenance functions

// --Shader_functions
ZEST_PRIVATE zest_shader_handle zest__new_shader(shaderc_shader_kind type);
ZEST_PRIVATE void zest__update_shader_spv(zest_shader shader, shaderc_compilation_result_t result);
ZEST_API void zest__cache_shader(zest_shader shader);
// --End Shader functions

// --Descriptor_set_functions
ZEST_PRIVATE zest_descriptor_pool zest__create_descriptor_pool(zest_uint max_sets);
ZEST_PRIVATE zest_set_layout_handle zest__new_descriptor_set_layout(const char *name);
ZEST_PRIVATE bool zest__binding_exists_in_layout_builder(zest_set_layout_builder_t *builder, zest_uint binding);
ZEST_PRIVATE VkDescriptorSetLayoutBinding *zest__get_layout_binding_info(zest_set_layout layout, zest_uint binding_index);
ZEST_PRIVATE zest_uint zest__acquire_bindless_index(zest_set_layout layout, zest_uint binding_number);
ZEST_PRIVATE void zest__release_bindless_index(zest_set_layout_handle layout_handle, zest_uint binding_number, zest_uint index_to_release);
ZEST_PRIVATE void zest__cleanup_set_layout(zest_set_layout layout);
ZEST_PRIVATE void zest__add_descriptor_set_to_resources(zest_shader_resources resources, zest_descriptor_set descriptor_set, zest_uint fif);
// --End Descriptor set functions

// --Device_set_up
ZEST_PRIVATE VKAPI_ATTR VkBool32 VKAPI_CALL zest_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
ZEST_PRIVATE VkResult zest__create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
ZEST_PRIVATE void zest__destroy_debug_messenger(void);
ZEST_PRIVATE zest_uint zest_find_memory_type(zest_uint typeFilter, VkMemoryPropertyFlags properties);
ZEST_PRIVATE zest_buffer_type_t zest__get_buffer_memory_type(VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, zest_size size);
ZEST_PRIVATE void zest__set_default_pool_sizes(void);
ZEST_PRIVATE void *zest_vk_allocate_callback(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
ZEST_PRIVATE void *zest_vk_reallocate_callback(void* pUserData, void *memory, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
ZEST_PRIVATE void zest_vk_free_callback(void* pUserData, void *memory);
//end device setup functions

//App_initialise_and_run_functions
ZEST_PRIVATE void zest__do_scheduled_tasks(void);
ZEST_PRIVATE void zest__initialise_app(zest_create_info_t *create_info);
ZEST_PRIVATE void zest__initialise_window(zest_create_info_t *create_info);
ZEST_PRIVATE void zest__initialise_platform(zest_platform_type type);
ZEST_PRIVATE void zest__initialise_platform_for_vulkan();
ZEST_PRIVATE zest_bool zest__initialise_device();
ZEST_PRIVATE void zest__destroy(void);
ZEST_PRIVATE void zest__main_loop(void);
ZEST_API void zest_Terminate(void);
ZEST_PRIVATE VkResult zest__main_loop_fence_wait();
ZEST_PRIVATE zest_microsecs zest__set_elapsed_time(void);
ZEST_PRIVATE zest_bool zest__validation_layers_are_enabled(void);
ZEST_PRIVATE zest_bool zest__validation_layers_with_sync_are_enabled(void);
ZEST_PRIVATE zest_bool zest__validation_layers_with_best_practices_are_enabled(void);
//-- end of internal functions

//-- Window_related_functions
ZEST_PRIVATE void zest__update_window_size(zest_window window, zest_uint width, zest_uint height);
//-- End Window related functions

//User API functions

//-----------------------------------------------
//        Essential_setup_functions
//-----------------------------------------------
//Create a new zest_create_info_t struct with default values for initialising Zest
ZEST_API zest_create_info_t zest_CreateInfo();
//Create a new zest_create_info_t struct with default values for initialising Zest but also enable validation layers as well
ZEST_API zest_create_info_t zest_CreateInfoWithValidationLayers(zest_validation_flags flags);
//Initialise Zest. You must call this in order to use Zest. Use zest_CreateInfo() to set up some default values to initialise the renderer.
ZEST_API zest_bool zest_Initialise(zest_create_info_t *info);
//Set the custom user data which will get passed through to the user update function each frame.
ZEST_API void zest_SetUserData(void* data);
//Set the user udpate callback that will be called each frame in the main loop of zest. You must set this or the main loop will just render a blank screen.
ZEST_API void zest_SetUserUpdateCallback(void(*callback)(zest_microsecs, void*));
//Start the main loop in the zest renderer. Must be run after zest_Initialise and also zest_SetUserUpdateCallback
ZEST_API void zest_Start(void);
//Shutdown zest and unload/free everything. Call this after zest_Start.
ZEST_API void zest_Shutdown(void);
//Free all memory used in the renderer and reset it back to an initial state.
ZEST_API void zest_ResetRenderer();
//Set the create info for the renderer, to be used optionally before a call to zest_ResetRenderer to change the configuration
//of the renderer
ZEST_API void zest_SetCreateInfo(zest_create_info_t *info);

//-----------------------------------------------
//        Vulkan_Helper_Functions
//        These functions are for more advanced customisation of the render where more knowledge of how Vulkan works is required.
//-----------------------------------------------
//Add a Vulkan instance extension. You don't really need to worry about this function unless you're looking to customise the render with some specific extensions
ZEST_API void zest_AddInstanceExtension(char *extension);
//Allocate space in memory for a zest_window_t which contains data about the window. If you're using your own method for creating a window then you can use
//this and then assign your window handle to zest_window_t.window_handle. Returns a pointer to the zest_window_t
ZEST_API zest_window zest_AllocateWindow(void);
//Create a descriptor pool based on a descriptor set layout. This will take the max sets value and create a pool 
//with enough descriptor pool types based on the bindings found in the layout
ZEST_API zest_bool zest_CreateDescriptorPoolForLayout(zest_set_layout_handle layout, zest_uint max_set_count);
//Reset a descriptor pool. This invalidates all descriptor sets current allocated from the pool so you can reallocate them.
ZEST_API void zest_ResetDescriptorPool(zest_descriptor_pool pool);
//Destroys the VkDescriptorPool and frees the zest_descriptor_pool and all contents
ZEST_API void zest_FreeDescriptorPool(zest_descriptor_pool pool);
//Create a descriptor layout builder object that you can use with the AddBuildLayout commands to put together more complex/fine tuned descriptor
//set layouts
ZEST_API zest_set_layout_builder_t zest_BeginSetLayoutBuilder();

ZEST_API void zest_AddLayoutBuilderBinding( zest_set_layout_builder_t *builder, zest_descriptor_binding_desc_t description);

//Build the descriptor set layout and add it to the renderer. This is for large global descriptor set layouts
ZEST_API zest_set_layout_handle zest_FinishDescriptorSetLayoutForBindless(zest_set_layout_builder_t *builder, zest_uint num_global_sets_this_pool_should_support, VkDescriptorPoolCreateFlags additional_pool_flags, const char *name, ...);
//Build the descriptor set layout and add it to the render. The layout is also returned from the function.
ZEST_API zest_set_layout_handle zest_FinishDescriptorSetLayout(zest_set_layout_builder_t *builder, const char *name, ...);

//Create a vulkan descriptor layout binding for use in setting up a descriptor set layout. This is a more general function for setting up whichever layout binding
//you need. Just pass in the VkDescriptorType, zest_supported_shader_stages, the binding number (which will correspond to the binding in the shader, and the number of descriptors
ZEST_API VkDescriptorSetLayoutBinding zest_CreateDescriptorLayoutBinding(VkDescriptorType type, zest_supported_shader_stages stageFlags, zest_uint binding, zest_uint descriptorCount);
//Create a vulkan descriptor layout binding specifically for uniform buffers. Just pass in the binding number that corresponds to the binding in the shader
ZEST_API VkDescriptorSetLayoutBinding zest_CreateUniformLayoutBinding(zest_uint binding);
//Create a vulkan descriptor layout binding specifically for texture samplers. Just pass in the binding number that corresponds to the binding in the shader
ZEST_API VkDescriptorSetLayoutBinding zest_CreateCombinedSamplerLayoutBinding(zest_uint binding);
//Create a vulkan descriptor layout binding specifically for texture samplers. Just pass in the binding number that corresponds to the binding in the shader
ZEST_API VkDescriptorSetLayoutBinding zest_CreateSamplerLayoutBinding(zest_uint binding);
//Create a vulkan descriptor layout binding specifically for storage buffers, more generally used for compute shaders. Just pass in the binding number that corresponds to the binding in the shader
ZEST_API VkDescriptorSetLayoutBinding zest_CreateStorageLayoutBinding(zest_uint binding);
//Create a vulkan descriptor set layout with bindings. Just pass in an array of bindings and a count of how many bindings there are.
ZEST_API VkDescriptorSetLayout zest_CreateDescriptorSetLayoutWithBindings(zest_uint count, VkDescriptorSetLayoutBinding *bindings);
//Create a vulkan descriptor write for a buffer. You need to pass the VkDescriptorSet
ZEST_API VkWriteDescriptorSet zest_CreateBufferDescriptorWriteWithType(VkDescriptorSet descriptor_set, VkDescriptorBufferInfo *buffer_info, zest_uint dst_binding, VkDescriptorType type);
//Create a vulkan descriptor write for an image.
ZEST_API VkWriteDescriptorSet zest_CreateImageDescriptorWriteWithType(VkDescriptorSet descriptor_set, VkDescriptorImageInfo *view_buffer_info, zest_uint dst_binding, VkDescriptorType type);
//To make creating a new VkDescriptorSet you can use a builder. Make sure you call this function to properly initialise the zest_descriptor_set_builder_t
//Once you have a builder, you can call the Add commands to add image and buffer bindings then call zest_FinishDescriptorSet to create the descriptor set
ZEST_API zest_descriptor_set_builder_t zest_BeginDescriptorSetBuilder(zest_set_layout_handle layout);
// Generic function to add any kind of descriptor write to a builder. If there's something that the more simple
// helper functions don't cover then you can just use this.
//    zest_uint dst_array_element,    // Starting element in the destination binding's array
//    zest_uint descriptor_count,     // Number of descriptors to write (for arrays)
//    VkDescriptorType descriptor_type,
// Provide ONE of these sets of pointers, others should be NULL:
//    const VkDescriptorImageInfo *p_image_infos,   // Pointer to an array of descriptor_count infos
//    const VkDescriptorBufferInfo *p_buffer_infos, // Pointer to an array of descriptor_count infos
//    const VkBufferView *p_texel_buffer_views      // Pointer to an array of descriptor_count views
ZEST_API void zest_AddSetBuilderWrite( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element,    zest_uint descriptor_count,     VkDescriptorType descriptor_type, const VkDescriptorImageInfo *p_image_infos,   const VkDescriptorBufferInfo *p_buffer_infos, const VkBufferView *p_texel_buffer_views);
//Add a sampler to a descriptor set builder
ZEST_API void zest_AddSetBuilderSampler( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, VkSampler sampler_handle);
//Add a combined image and sampler to a descriptor set builder
ZEST_API void zest_AddSetBuilderCombinedImageSampler(zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, VkSampler sampler_handle, VkImageView image_view_handle, VkImageLayout layout);
//Add an array of samplers to a set builder
ZEST_API void zest_AddSetBuilderCombinedImageSamplers(zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_uint count, const VkDescriptorImageInfo *p_image_infos);
//Add a VkDescriptorImageInfo from a zest_texture (or render target) to a descriptor set builder.
ZEST_API void zest_AddSetBuilderDirectImageWrite(zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, VkDescriptorType type, const VkDescriptorImageInfo *image_info);
//Add a VkDescriptorBufferInfo from a zest_descriptor_buffer to a descriptor set builder as a uniform buffer.
ZEST_API void zest_AddSetBuilderUniformBuffer(zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_uniform_buffer_handle uniform_buffer, zest_uint fif);
//Add a VkDescriptorBufferInfo from a zest_descriptor_buffer to a descriptor set builder as a storage buffer.
ZEST_API void zest_AddSetBuilderStorageBuffer( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_buffer storage_buffer);
//Build a zest_descriptor_set_t using a builder that you made using the AddBuilder command. The layout that you pass to this function must be configured properly.
//zest_descriptor_set_t will contain a VkDescriptorSet for each frame in flight as well as descriptor writes used to create the set.
ZEST_API zest_descriptor_set zest_FinishDescriptorSet(zest_descriptor_pool pool, zest_descriptor_set_builder_t *builder, zest_descriptor_set new_set_to_populate_or_update);
ZEST_API zest_descriptor_set zest_CreateBindlessSet(zest_set_layout_handle layout);
ZEST_API zest_uint zest_AcquireBindlessStorageBufferIndex(zest_buffer buffer, zest_set_layout_handle layout, zest_descriptor_set set, zest_uint target_binding_number);
ZEST_API zest_uint zest_AcquireBindlessImageIndex(zest_image_handle image, zest_image_view view, zest_sampler sampler, zest_set_layout_handle layout, zest_descriptor_set set, zest_global_binding_number target_binding_number);
ZEST_API zest_uint zest_AcquireGlobalSampler(zest_image_handle handle, zest_sampler_handle sampler, zest_global_binding_number binding_number);
ZEST_API zest_uint zest_AcquireGlobalSamplerWithView(zest_image_handle image_handle, zest_image_view_handle view_handle, zest_sampler sampler, zest_global_binding_number binding_number);
ZEST_API zest_uint zest_AcquireGlobalStorageBufferIndex(zest_buffer buffer);
ZEST_API zest_uint *zest_AcquireGlobalImageMipIndexes(zest_image_handle handle, zest_sampler_handle sampler, zest_image_view_array_handle image_views, zest_global_binding_number binding_number);
ZEST_API void zest_AcquireGlobalInstanceLayerBufferIndex(zest_layer_handle layer);
ZEST_API void zest_ReleaseGlobalStorageBufferIndex(zest_buffer buffer);
ZEST_API void zest_ReleaseGlobalImageIndex(zest_image_handle image, zest_global_binding_number binding_number);
ZEST_API void zest_ReleaseAllGlobalImageIndexes(zest_image_handle image);
ZEST_API void zest_ReleaseGlobalBindlessIndex(zest_uint index, zest_global_binding_number binding_number);
ZEST_API VkDescriptorSet zest_vk_GetGlobalBindlessSet();
ZEST_API zest_descriptor_set zest_GetGlobalBindlessSet();
ZEST_API VkDescriptorSetLayout zest_vk_GetGlobalBindlessLayout();
ZEST_API VkDescriptorSetLayout zest_vk_GetDebugLayout();
ZEST_API zest_set_layout_handle zest_GetGlobalBindlessLayout();
//Create a new descriptor set shader_resources
ZEST_API zest_shader_resources_handle zest_CreateShaderResources();
//Delete shader resources from the renderer and free the memory. This does not free or destroy the actual
//descriptor sets that you added to the resources
ZEST_API void zest_FreeShaderResources(zest_shader_resources_handle shader_resources);
//Add a descriptor set to a descriptor set shader_resources. Bundles are used for binding to a draw call so the descriptor sets can be passed in to the shaders
//according to their set and binding number. So therefore it's important that you add the descriptor sets to the shader_resources in the same order
//that you set up the descriptor set layouts. You must also specify the frame in flight for the descriptor set that you're addeding.
//Use zest_AddDescriptorSetsToResources to add all frames in flight by passing an array of desriptor sets
ZEST_API void zest_AddDescriptorSetToResources(zest_shader_resources_handle shader_resources, zest_descriptor_set descriptor_set, zest_uint fif);
//Add the descriptor set for a uniform buffer to a shader resource
ZEST_API void zest_AddUniformBufferToResources(zest_shader_resources_handle shader_resources, zest_uniform_buffer_handle buffer);
//Add the descriptor set for a uniform buffer to a shader resource
ZEST_API void zest_AddGlobalBindlessSetToResources(zest_shader_resources_handle shader_resources);
//Add the descriptor set for a uniform buffer to a shader resource
ZEST_API void zest_AddBindlessSetToResources(zest_shader_resources_handle shader_resources, zest_descriptor_set set);
//Clear all the descriptor sets in a shader resources object. This does not free the memory, call zest_FreeShaderResources to do that.
ZEST_API void zest_ClearShaderResources(zest_shader_resources_handle shader_resources);
//Update the descriptor set in a shader_resources. You'll need this whenever you update a descriptor set for whatever reason. Pass the index of the
//descriptor set in the shader_resources that you want to update.
ZEST_API void zest_UpdateShaderResources(zest_shader_resources_handle shader_resources, zest_descriptor_set descriptor_set, zest_uint index, zest_uint fif);
//Helper function to validate
ZEST_API bool zest_ValidateShaderResource(zest_shader_resources_handle shader_resources);
//Update a VkDescriptorSet with an array of descriptor writes. For when the images/buffers in a descriptor set have changed, the corresponding descriptor set will need to be updated.
ZEST_API void zest_UpdateDescriptorSet(VkWriteDescriptorSet *descriptor_writes);
//Create a VkViewport, generally used when building a render pass.
ZEST_API VkViewport zest_CreateViewport(float x, float y, float width, float height, float minDepth, float maxDepth);
//Create a VkRect2D, generally used when building a render pass.
ZEST_API VkRect2D zest_CreateRect2D(zest_uint width, zest_uint height, int offsetX, int offsetY);
//Validate a shader from a string and add it to the library of shaders in the renderer
ZEST_API shaderc_compilation_result_t zest_ValidateShader(const char *shader_code, shaderc_shader_kind type, const char *name, shaderc_compiler_t compiler);
//Creates and compiles a new shader from a string and add it to the library of shaders in the renderer
ZEST_API zest_shader_handle zest_CreateShader(const char *shader_code, shaderc_shader_kind type, const char *name, zest_bool format_code, zest_bool disable_caching, shaderc_compiler_t compiler, shaderc_compile_options_t options);
//Creates a shader from a file containing the shader glsl code
ZEST_API zest_shader_handle zest_CreateShaderFromFile(const char *file, const char *name, shaderc_shader_kind type, zest_bool disable_caching, shaderc_compiler_t compiler, shaderc_compile_options_t options);
//Creates and compiles a new shader from a string and add it to the library of shaders in the renderer
ZEST_API zest_shader_handle zest_CreateShaderSPVMemory(const unsigned char *shader_code, zest_uint spv_length, const char *name, shaderc_shader_kind type);
//Reload a shader. Use this if you edited a shader file and you want to refresh it/hot reload it
//The shader must have been created from a file with zest_CreateShaderFromFile. Once the shader is reloaded you can call
//zest_CompileShader or zest_ValidateShader to recompile it. You'll then have to call zest_SchedulePipelineRecreate to recreate
//the pipeline that uses the shader. Returns true if the shader was successfully loaded.
ZEST_API zest_bool zest_ReloadShader(zest_shader_handle shader);
//Creates and compiles a new shader from a string and add it to the library of shaders in the renderer
ZEST_API zest_bool zest_CompileShader(zest_shader_handle shader, shaderc_compiler_t compiler);
//Add a shader straight from an spv file and return a handle to the shader. Note that no prefix is added to the filename here so 
//pass in the full path to the file relative to the executable being run.
ZEST_API zest_shader_handle zest_AddShaderFromSPVFile(const char *filename, shaderc_shader_kind type);
//Add an spv shader straight from memory and return a handle to the shader. Note that the name should just be the name of the shader, 
//If a path prefix is set (ZestRenderer->shader_path_prefix, set when initialising Zest in the create_info struct, spv is default) then
//This prefix will be prepending to the name you pass in here.
ZEST_API zest_shader_handle zest_AddShaderFromSPVMemory(const char *name, const void *buffer, zest_uint size, shaderc_shader_kind type);
//Add a shader to the renderer list of shaders.
ZEST_API void zest_AddShader(zest_shader_handle shader, const char *name);
//Free the memory for a shader and remove if from the shader list in the renderer (if it exists there)
ZEST_API void zest_FreeShader(zest_shader_handle shader);

//Set up shader resources ready to be bound to a pipeline when calling zest_cmd_BindPipeline or zest_BindpipelineCB. You should always 
//pass in an empty array of VkDescriptorSets. Set this array up as simple an 0 pointer and the function will allocate the space for the
//descriptor sets. This means that it's a good idea to not use a local variable. Call zest_ClearShaderResourceDescriptorSets after you've
//bound the pipeline.
ZEST_API zest_uint zest_GetDescriptorSetsForBinding(zest_shader_resources_handle shader_resources, VkDescriptorSet **draw_sets);
ZEST_API void zest_ClearShaderResourceDescriptorSets(VkDescriptorSet *draw_sets);
ZEST_API zest_uint zest_ShaderResourceSetCount(VkDescriptorSet *draw_sets);

//-----------------------------------------------
//        Pipeline_related_vulkan_helpers
//        Pipelines are essential to drawing things on screen. There are some builtin pipeline_templates that Zest creates
//        for sprite/billboard/mesh/font drawing. You can take a look in zest__prepare_standard_pipelines to see how
//        the following functions are utilised, plus look at the exmaples for building your own custom pipeline_templates.
//-----------------------------------------------
//Add a new pipeline template to the renderer and return its handle.
ZEST_API zest_pipeline_template zest_BeginPipelineTemplate(const char *name);
//Set the name of the file to use for the vert and frag shader in the zest_pipeline_template_create_info_t
ZEST_API void zest_SetPipelineVertShader(zest_pipeline_template pipeline_template, zest_shader_handle vert_shader);
ZEST_API void zest_SetPipelineFragShader(zest_pipeline_template pipeline_template, zest_shader_handle frag_shader);
ZEST_API void zest_SetPipelineShaders(zest_pipeline_template pipeline_template, zest_shader_handle vertex_shader, zest_shader_handle fragment_shader);
ZEST_API void zest_SetPipelineFrontFace(zest_pipeline_template pipeline_template, zest_front_face front_face);
ZEST_API void zest_SetPipelineTopology(zest_pipeline_template pipeline_template, zest_topology topology);
ZEST_API void zest_SetPipelineCullMode(zest_pipeline_template pipeline_template, zest_cull_mode cull_mode);
//Set the name of both the fragment and vertex shader to the same file (frag and vertex shaders can be combined into the same spv)
ZEST_API void zest_SetPipelineShader(zest_pipeline_template pipeline_template, zest_shader_handle combined_vertex_and_fragment_shader);
//Add a new VkVertexInputBindingDescription which is used to set the size of the struct (stride) and the vertex input rate.
//You can add as many bindings as you need, just make sure you set the correct binding index for each one
ZEST_API VkVertexInputBindingDescription zest_AddVertexInputBindingDescription(zest_pipeline_template pipeline_template, zest_uint binding, zest_uint stride, VkVertexInputRate input_rate);
//Clear the input binding descriptions from the zest_pipeline_template_create_info_t bindingDescriptions array.
ZEST_API void zest_ClearVertexInputBindingDescriptions(zest_pipeline_template pipeline_template);
//Clear the input attribute descriptions from the zest_pipeline_template_create_info_t attributeDescriptions array.
ZEST_API void zest_ClearVertexAttributeDescriptions(zest_pipeline_template pipeline_template);
//Clear the push constant ranges in a pipeline. You can use this after copying a pipeline to set up your own push constants instead
//if your pipeline will use a different push constant range.
ZEST_API void zest_ClearPipelinePushConstantRanges(zest_pipeline_template pipeline_template);
//Add a VkVertexInputeAttributeDescription to a zest_vertex_input_descriptions array. You can use zest_CreateVertexInputDescription
//helper function to create the description
ZEST_API void zest_AddVertexAttribute(zest_pipeline_template pipeline_template, zest_uint binding, zest_uint location, VkFormat format, zest_uint offset);
//Create a VkVertexInputAttributeDescription for adding to a zest_vertex_input_descriptions array. Just pass the binding and location in
//the shader, the VkFormat and the offset into the struct that you're using for the vertex data. See zest__prepare_standard_pipelines
//for examples of how the builtin pipeline_templates do this
ZEST_API VkVertexInputAttributeDescription zest_CreateVertexInputDescription(zest_uint binding, zest_uint location, VkFormat format, zest_uint offset);
//Set up the push contant that you might plan to use in the pipeline. Just pass in the size of the push constant struct, the offset and the shader
//stage flags where the push constant will be used. Use this if you only want to set up a single push constant range
ZEST_API void zest_SetPipelinePushConstantRange(zest_pipeline_template create_info, zest_uint size, zest_supported_shader_stages stage_flags);
//You can set a pointer in the pipeline template to point to the push constant data that you want to pass to the shader.
//It MUST match the same data layout/size that you set with zest_SetPipelinePushConstantRange and align with the 
//push constants that you use in the shader. The point you use must be stable! Or update it if it changes for any reason.
ZEST_API void zest_SetPipelinePushConstants(zest_pipeline_template pipeline_template, void *push_constants);
ZEST_API void zest_SetPipelineBlend(zest_pipeline_template pipeline_template, VkPipelineColorBlendAttachmentState blend_attachment);
ZEST_API void zest_SetPipelineDepthTest(zest_pipeline_template pipeline_template, bool enable_test, bool write_enable);
//Add a descriptor layout to the pipeline template. Use this function only when setting up the pipeline before you call zest__build_pipeline
ZEST_API void zest_AddPipelineDescriptorLayout(zest_pipeline_template pipeline_template, VkDescriptorSetLayout layout);
//Clear the descriptor layouts in a pipeline template create info
ZEST_API void zest_ClearPipelineDescriptorLayouts(zest_pipeline_template pipeline_template);
//Make a pipeline template ready for building. Pass in the pipeline that you created with zest_BeginPipelineTemplate, the render pass that you want to
//use for the pipeline and the zest_pipeline_template_create_info_t you have setup to configure the pipeline. After you have called this
//function you can make a few more alterations to configure the pipeline further if needed before calling zest__build_pipeline.
//NOTE: the create info that you pass into this function will get copied and then freed so don't use it after calling this function. If
//you want to create another variation of this pipeline you're creating then you can call zest_CopyTemplateFromPipeline to create a new
//zest_pipeline_template_create_info_t and create another new pipeline with that
ZEST_API void zest_EndPipelineTemplate(zest_pipeline_template pipeline_template);
//Get the shader stage flags for a specific push constant range in the pipeline
ZEST_API VkShaderStageFlags zest_PipelinePushConstantStageFlags(zest_pipeline pipeline, zest_uint index);
//Get the size of a push constant range for a specific index in the pipeline
ZEST_API zest_uint zest_PipelinePushConstantSize(zest_pipeline pipeline, zest_uint index);
//Get the offset of a push constant range for a specific index in the pipeline
ZEST_API zest_uint zest_PipelinePushConstantOffset(zest_pipeline pipeline, zest_uint index);
//The following are helper functions to set color blend attachment states for various blending setups
//Just take a look inside the functions for the values being used
ZEST_API VkPipelineColorBlendAttachmentState zest_BlendStateNone(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_AdditiveBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_AdditiveBlendState2(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_AlphaOnlyBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_AlphaBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_PreMultiplyBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_PreMultiplyBlendStateForSwap(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_MaxAlphaBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_ImGuiBlendState(void);
ZEST_API zest_pipeline zest_PipelineWithTemplate(zest_pipeline_template pipeline_template, const zest_frame_graph_context context);
//Copy the zest_pipeline_template_create_info_t from an existing pipeline. This can be useful if you want to create a new pipeline based
//on an existing pipeline with just a few tweaks like setting a different shader to use.
ZEST_API zest_pipeline_template zest_CopyPipelineTemplate(const char *name, zest_pipeline_template pipeline_template);
//Delete a pipeline including the template and any cached versions of the pipeline
ZEST_API void zest_FreePipelineTemplate(zest_pipeline_template pipeline_template);
//-- End Pipeline related

//--End vulkan helper functions

//Platform_dependent_callbacks
//Depending on the platform and method you're using to create a window and poll events callbacks are used to do those things.
//You can define those callbacks with these functions
ZEST_API void zest_SetDestroyWindowCallback(void(*destroy_window_callback)(zest_window window, void *user_data));
ZEST_API void zest_SetGetWindowSizeCallback(void(*get_window_size_callback)(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height));
ZEST_API void zest_SetPollEventsCallback(void(*poll_events_callback)(void));
ZEST_API void zest_SetPlatformExtensionsCallback(void(*add_platform_extensions_callback)(void));
ZEST_API void zest_SetPlatformWindowModeCallback(void(*set_window_mode_callback)(zest_window window, zest_window_mode mode));
ZEST_API void zest_SetPlatformWindowSizeCallback(void(*set_window_size_callback)(zest_window window, int width, int height));

//-----------------------------------------------
//        Buffer_functions.
//        Use these functions to create memory buffers mainly on the GPU
//        but you can also create staging buffers that are cpu visible and
//        used to upload data to the GPU
//        All buffers are allocated from a memory pool. Generally each buffer type has it's own separate pool
//        depending on the memory requirements. You can define the size of memory pools before you initialise
//        Zest using zest_SetDeviceBufferPoolSize and zest_SetDeviceImagePoolsize. When you create a buffer, if
//        there is no memory left to allocate the buffer then it will try and create a new pool to allocate from.
//-----------------------------------------------
//Debug functions, will output info about the buffer to the console and also verify the integrity of host and device memory blocks
ZEST_API void zloc__output_buffer_info(void* ptr, size_t size, int free, void* user, int count);
ZEST_API zloc__error_codes zloc_VerifyRemoteBlocks(zloc_header *first_block, zloc__block_output output_function, void *user_data);
ZEST_API zloc__error_codes zloc_VerifyAllRemoteBlocks(zloc__block_output output_function, void *user_data);
ZEST_API zloc__error_codes zloc_VerifyBlocks(zloc_header *first_block, zloc__block_output output_function, void *user_data);
ZEST_API zest_uint zloc_CountBlocks(zloc_header *first_block);
//---------
//Create a new buffer configured with the zest_buffer_info_t that you pass into the function. If you're creating a buffer to store
//and image then pass in the VkImage as well, otherwise just pass in 0.
//You can use helper functions to create commonly used buffer types such as zest_CreateVertexBufferInfo below, and you can just use
//helper functions to create the buffers without needed to create the zest_buffer_info_t, see functions just below this one.
ZEST_API zest_buffer zest_CreateBuffer(VkDeviceSize size, zest_buffer_info_t *buffer_info, VkImage image);
//Create a staging buffer which you can use to prep data for uploading to another buffer on the GPU
ZEST_API zest_buffer zest_CreateStagingBuffer(VkDeviceSize size, void *data);
ZEST_API zest_buffer zest_CreateStagingBuffer(VkDeviceSize size, void *data);
//Helper functions to create buffers. 
//Create an index buffer.
ZEST_API zest_buffer zest_CreateIndexBuffer(VkDeviceSize size, zest_uint fif);
ZEST_API zest_buffer zest_CreateVertexBuffer(VkDeviceSize size, zest_uint fif);
ZEST_API zest_buffer zest_CreateVertexStorageBuffer(VkDeviceSize size, zest_uint fif);
//Create a general storage buffer mainly for use in a compute shader
ZEST_API zest_buffer zest_CreateStorageBuffer(VkDeviceSize size, zest_uint fif);
//Create a general storage buffer that is visible to the CPU for more convenient updating
ZEST_API zest_buffer zest_CreateCPUStorageBuffer(VkDeviceSize size, zest_uint fif);
//Create a vertex buffer that is flagged for storage so that you can use it in a compute shader
ZEST_API zest_buffer zest_CreateComputeVertexBuffer(VkDeviceSize size, zest_uint fif);
//Create an index buffer that is flagged for storage so that you can use it in a compute shader
ZEST_API zest_buffer zest_CreateComputeIndexBuffer(VkDeviceSize size, zest_uint fif);
//Create unique buffers for use when using buffers that are decoupled from the main frame in flight
// Only use unique_id when you need to decouple the frame in flight for the buffer from the main frame in flight.
// For example, imgui can it's own frame in flight because the buffers only need to be updated when the 
// zest_impl_UpdateBuffers() function is called which may only be run whenever the data changes or as in most
// examples, when the fixed update loop is run. In this situation for the sake of maintaining correct 
// syncronization the unique_id forces the buffer to use a unique VkBuffer so that it doesn't clash with other
// VkBuffers that are synced up with the main frame in flight index. 
// So basically if you intend your buffer to be updated everyframe (which will be in most cases) just use 0 for
// the id
ZEST_API zest_buffer zest_CreateUniqueIndexBuffer(VkDeviceSize size, zest_uint fif, zest_uint unique_id);
ZEST_API zest_buffer zest_CreateUniqueVertexBuffer(VkDeviceSize size, zest_uint fif, zest_uint unique_id);
ZEST_API zest_buffer zest_CreateUniqueVertexStorageBuffer(VkDeviceSize size, zest_uint fif, zest_uint unique_id);
//Create a general storage buffer mainly for use in a compute shader
ZEST_API zest_buffer zest_CreateUniqueStorageBuffer(VkDeviceSize size, zest_uint fif, zest_uint unique_id);
//Create a general storage buffer that is visible to the CPU for more convenient updating
ZEST_API zest_buffer zest_CreateUniqueCPUStorageBuffer(VkDeviceSize size, zest_uint fif, zest_uint unique_id);
//Create a vertex buffer that is flagged for storage so that you can use it in a compute shader
ZEST_API zest_buffer zest_CreateUniqueComputeVertexBuffer(VkDeviceSize size, zest_uint fif, zest_uint unique_id);
//Create an index buffer that is flagged for storage so that you can use it in a compute shader
ZEST_API zest_buffer zest_CreateUniqueComputeIndexBuffer(VkDeviceSize size, zest_uint fif, zest_uint unique_id);
//The following functions can be used to generate a zest_buffer_info_t with the corresponding buffer configuration to create buffers with
ZEST_API zest_buffer_info_t zest_CreateVertexBufferInfo(zest_bool cpu_visible);
ZEST_API zest_buffer_info_t zest_CreateVertexBufferInfoWithStorage(zest_bool cpu_visible);
ZEST_API zest_buffer_info_t zest_CreateStorageBufferInfo(zest_bool cpu_visible);
ZEST_API zest_buffer_info_t zest_CreateCPUVisibleStorageBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateStorageBufferInfoWithSrcFlag(void);
ZEST_API zest_buffer_info_t zest_CreateComputeVertexBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateComputeIndexBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateIndexBufferInfo(zest_bool cpu_visible);
ZEST_API zest_buffer_info_t zest_CreateIndexBufferInfoWithStorage(zest_bool cpu_visible);
ZEST_API zest_buffer_info_t zest_CreateStagingBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateDrawCommandsBufferInfo(zest_bool host_visible);
//Grow a buffer if the minium_bytes is more then the current buffer size.
ZEST_API zest_bool zest_GrowBuffer(zest_buffer *buffer, zest_size unit_size, zest_size minimum_bytes);
//Resize a buffer if the new size if more than the current size of the buffer. Returns true if the buffer was resized successfully.
ZEST_API zest_bool zest_ResizeBuffer(zest_buffer *buffer, zest_size new_size);
//Copy data to a staging buffer
void zest_StageData(void *src_data, zest_buffer dst_staging_buffer, zest_size size);
//Flush the memory of - in most cases - a staging buffer so that it's memory is made available immediately to the device
ZEST_API VkResult zest_FlushBuffer(zest_buffer buffer);
//Free a zest_buffer and return it's memory to the pool
ZEST_API void zest_FreeBuffer(zest_buffer buffer);
//Get the VkDeviceMemory from a zest_buffer. This is needed for some Vulkan commands
ZEST_API VkDeviceMemory zest_GetBufferDeviceMemory(zest_buffer buffer);
//Get the VkBuffer from a zest_buffer. This is needed for some Vulkan Commands
ZEST_API VkBuffer *zest_GetBufferDeviceBuffer(zest_buffer buffer);
ZEST_API VkDescriptorBufferInfo zest_vk_GetBufferInfo(zest_buffer buffer);
//When creating your own draw routines you will probably need to upload data from a staging buffer to a GPU buffer like a vertex buffer. You can
//use this command with zest_cmd_UploadBuffer to upload the buffers that you need. You can call zest_AddCopyCommand multiple times depending on
//how many buffers you need to upload data for and then call zest_cmd_UploadBuffer passing the zest_buffer_uploader_t to copy all the buffers in
//one go. For examples see the builtin draw layers that do this like: zest__update_instance_layer_buffers_callback
ZEST_API void zest_AddCopyCommand(zest_buffer_uploader_t *uploader, zest_buffer_t *source_buffer, zest_buffer_t *target_buffer, zest_size target_offset);
//Get the default pool size that is set for a specific pool hash.
ZEST_API zest_buffer_pool_size_t zest_GetDevicePoolSize(zest_key hash);
//Get the default pool size that is set for a specific combination of usage, property and image flags
ZEST_API zest_buffer_pool_size_t zest_GetDeviceBufferPoolSize(zest_buffer_usage_flags usage_flags, zest_memory_property_flags property_flags, zest_image_usage_flags image_flags);
//Get the default pool size for an image pool.
ZEST_API zest_buffer_pool_size_t zest_GetDeviceImagePoolSize(const char *name);
//Set the default pool size for a specific type of buffer set by the usage and property flags. You must call this before you call zest_Initialise
//otherwise it might not take effect on any buffers that are created during initialisation.
//Note that minimum allocation size may get overridden if it is smaller than the alignment reported by vkGetBufferMemoryRequirements at pool creation
ZEST_API void zest_SetDeviceBufferPoolSize(const char *name, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, zest_size minimum_allocation, zest_size pool_size);
//Set the default pool size for images. based on image usage and property flags.
//Note that minimum allocation size may get overridden if it is smaller than the alignment reported by vkGetImageMemoryRequirements at pool creation
ZEST_API void zest_SetDeviceImagePoolSize(const char *name, VkImageUsageFlags image_flags, VkMemoryPropertyFlags property_flags, zest_size minimum_allocation, zest_size pool_size);
//Create a buffer specifically for use as a uniform buffer. This will also create a descriptor set for the uniform
//buffers as well so it's ready for use in shaders.
ZEST_API zest_uniform_buffer_handle zest_CreateUniformBuffer(const char *name, zest_size uniform_struct_size);
//Free a uniform buffer and all it's resources
ZEST_API void zest_FreeUniformBuffer(zest_uniform_buffer_handle uniform_buffer);
//Standard builtin functions for updating a uniform buffer for use in 2d shaders where x,y coordinates represent a location on the screen. This will
//update the current frame in flight. If you need to update a specific frame in flight then call zest_UpdateUniformBufferFIF.
ZEST_API void zest_Update2dUniformBuffer(void);
//Get a pointer to a uniform buffer. This will return a void* which you can cast to whatever struct your storing in the uniform buffer. This will get the buffer
//with the current frame in flight index.
ZEST_API void *zest_GetUniformBufferData(zest_uniform_buffer_handle uniform_buffer);
//Get the VkDescriptorBufferInfo of a uniform buffer by name and specific frame in flight. Use ZEST_FIF you just want the current frame in flight
ZEST_API VkDescriptorBufferInfo *zest_GetUniformBufferInfo(zest_uniform_buffer_handle buffer);
//Get the VkDescriptorSetLayout for a uniform buffer that you can use when creating pipeline_templates. The buffer must have
//been properly initialised, use zest_CreateUniformBuffer for this.
ZEST_API VkDescriptorSetLayout zest_vk_GetUniformBufferLayout(zest_uniform_buffer_handle buffer);
ZEST_API zest_set_layout_handle zest_GetUniformBufferLayout(zest_uniform_buffer_handle buffer);
//Get the VkDescriptorSet in a uniform buffer. You can use this when binding a pipeline for a draw call or compute
//dispatch etc. 
ZEST_API VkDescriptorSet zest_vk_GetUniformBufferSet(zest_uniform_buffer_handle buffer);
ZEST_API zest_descriptor_set zest_GetUniformBufferSet(zest_uniform_buffer_handle buffer);
ZEST_API zest_descriptor_set zest_GetFIFUniformBufferSet(zest_uniform_buffer_handle buffer, zest_uint fif);
ZEST_API VkDescriptorSet zest_vk_GetFIFUniformBufferSet(zest_uniform_buffer_handle buffer, zest_uint fif);
ZEST_API zest_uint zest_GetBufferDescriptorIndex(zest_buffer buffer);
//Should only be used in zest implementations only
ZEST_API void *zest_AllocateMemory(zest_size size);
ZEST_API void zest_FreeMemory(void *allocation);
//When you free buffers the Vulkan buffer is added to a list that is either freed at the end of the program
//or you can call this to free them whenever you want.
ZEST_API void zest_FlushUsedBuffers();
//Get the mapped data of a buffer. For CPU visible buffers only
ZEST_API void *zest_BufferData(zest_buffer buffer);
//Get the capacity of a buffer
ZEST_API zest_size zest_BufferSize(zest_buffer buffer);
//Returns the amount of memory current in use by the buffer
ZEST_API zest_size zest_BufferMemoryInUse(zest_buffer buffer);
//Set the current amount of memory in the buffer that you're actually using
ZEST_API void zest_SetBufferMemoryInUse(zest_buffer buffer, zest_size size);
//--End Buffer related

//Helper functions for creating the builtin layers. these can be called separately outside of a command queue setup context
ZEST_API zest_layer_handle zest_CreateMeshLayer(const char *name, zest_size vertex_type_size);
ZEST_API zest_layer_handle zest_CreateInstanceMeshLayer(const char *name);
//-- End Command queue setup and creation

//-----------------------------------------------
//        General_Math_helper_functions
//-----------------------------------------------

//Subtract right from left vector and return the result
ZEST_API zest_vec2 zest_SubVec2(zest_vec2 left, zest_vec2 right);
ZEST_API zest_vec3 zest_SubVec3(zest_vec3 left, zest_vec3 right);
ZEST_API zest_vec4 zest_SubVec4(zest_vec4 left, zest_vec4 right);
ZEST_API zest_vec2 zest_AddVec2(zest_vec2 left, zest_vec2 right);
ZEST_API zest_vec3 zest_AddVec3(zest_vec3 left, zest_vec3 right);
ZEST_API zest_vec4 zest_AddVec4(zest_vec4 left, zest_vec4 right);
ZEST_API zest_ivec2 zest_AddiVec2(zest_ivec2 left, zest_ivec2 right);
ZEST_API zest_ivec3 zest_AddiVec3(zest_ivec3 left, zest_ivec3 right);
//Scale a vector by a scalar and return the result
ZEST_API zest_vec2 zest_ScaleVec2(zest_vec2 vec2, float v);
ZEST_API zest_vec3 zest_ScaleVec3(zest_vec3 vec3, float v);
ZEST_API zest_vec4 zest_ScaleVec4(zest_vec4 vec4, float v);
//Multiply 2 vectors and return the result
ZEST_API zest_vec3 zest_MulVec3(zest_vec3 left, zest_vec3 right);
ZEST_API zest_vec4 zest_MulVec4(zest_vec4 left, zest_vec4 right);
//Get the length of a vec without square rooting
ZEST_API float zest_LengthVec3(zest_vec3 const v);
ZEST_API float zest_LengthVec4(zest_vec4 const v);
ZEST_API float zest_Vec2Length2(zest_vec2 const v);
//Get the length of a vec
ZEST_API float zest_LengthVec(zest_vec3 const v);
ZEST_API float zest_Vec2Length(zest_vec2 const v);
//Normalise vectors
ZEST_API zest_vec2 zest_NormalizeVec2(zest_vec2 const v);
ZEST_API zest_vec3 zest_NormalizeVec3(zest_vec3 const v);
ZEST_API zest_vec4 zest_NormalizeVec4(zest_vec4 const v);
//Transform a vector by a 4x4 matrix
ZEST_API zest_vec4 zest_MatrixTransformVector(zest_matrix4 *mat, zest_vec4 vec);
//Transpose a matrix, switch columns to rows and vice versa
zest_matrix4 zest_TransposeMatrix4(zest_matrix4 *mat);
//Transform 2 matrix 4s
ZEST_API zest_matrix4 zest_MatrixTransform(zest_matrix4 *in, zest_matrix4 *m);
//Get the inverse of a 4x4 matrix
ZEST_API zest_matrix4 zest_Inverse(zest_matrix4 *m);
//Create a 4x4 matrix with rotation, position and scale applied
ZEST_API zest_matrix4 zest_CreateMatrix4(float pitch, float yaw, float roll, float x, float y, float z, float sx, float sy, float sz);
//Create a rotation matrix on the x axis
ZEST_API zest_matrix4 zest_Matrix4RotateX(float angle);
//Create a rotation matrix on the y axis
ZEST_API zest_matrix4 zest_Matrix4RotateY(float angle);
//Create a rotation matrix on the z axis
ZEST_API zest_matrix4 zest_Matrix4RotateZ(float angle);
//Get the cross product between 2 vec3s
ZEST_API zest_vec3 zest_CrossProduct(const zest_vec3 x, const zest_vec3 y);
//Get the dot product between 2 vec3s
ZEST_API float zest_DotProduct3(const zest_vec3 a, const zest_vec3 b);
//Get the dot product between 2 vec4s
ZEST_API float zest_DotProduct4(const zest_vec4 a, const zest_vec4 b);
//Create a 4x4 matrix to look at a point
ZEST_API zest_matrix4 zest_LookAt(const zest_vec3 eye, const zest_vec3 center, const zest_vec3 up);
//Create a 4x4 matrix for orthographic projection
ZEST_API zest_matrix4 zest_Ortho(float left, float right, float bottom, float top, float z_near, float z_far);
//Get the distance between 2 points
ZEST_API float zest_Distance(float fromx, float fromy, float tox, float toy);
//Pack a vec3 into an unsigned int. Store something in the extra 2 bits if needed
ZEST_API zest_uint zest_Pack10bit(zest_vec3 *v, zest_uint extra);
//Pack a vec3 into an unsigned int
ZEST_API zest_uint zest_Pack8bitx3(zest_vec3 *v);
//Pack 2 floats into an unsigned int
ZEST_API zest_uint zest_Pack16bit2SNorm(float x, float y);
ZEST_API zest_u64 zest_Pack16bit4SNorm(float x, float y, float z, float w);
//Convert 4 32bit floats into packed 16bit scaled floats. You can pass 2 max_values if you need to scale xy/zw separately
ZEST_API zest_uint zest_Pack16bit2SScaled(float x, float y, float max_value);
//Convert a 32bit float to a 16bit float packed into a 16bit uint
ZEST_API zest_uint zest_FloatToHalf(float f);
//Convert 4 32bit floats into packed 16bit floats
ZEST_API zest_u64 zest_Pack16bit4SFloat(float x, float y, float z, float w);
//Convert 4 32bit floats into packed 16bit scaled floats. You can pass 2 max_values if you need to scale xy/zw separately
ZEST_API zest_u64 zest_Pack16bit4SScaled(float x, float y, float z, float w, float max_value_xy, float max_value_zw);
//Convert 2 32bit floats into packed 16bit scaled floats and pass zw as a prepacked value. 
ZEST_API zest_u64 zest_Pack16bit4SScaledZWPacked(float x, float y, zest_uint zw, float max_value_xy);
ZEST_API zest_uint zest_Pack16bitStretch(float x, float y);
//Pack 3 floats into an unsigned int
ZEST_API zest_uint zest_Pack8bit(float x, float y, float z);
//Get the next power up from the number you pass into the function
ZEST_API zest_size zest_GetNextPower(zest_size n);
//Convert degrees into radians
ZEST_API float zest_Radians(float degrees);
//Convert radians into degrees
ZEST_API float zest_Degrees(float radians);
//Interpolate 2 vec4s and return the result
ZEST_API zest_vec4 zest_LerpVec4(zest_vec4 *from, zest_vec4 *to, float lerp);
//Interpolate 2 vec3s and return the result
ZEST_API zest_vec3 zest_LerpVec3(zest_vec3 *from, zest_vec3 *to, float lerp);
//Interpolate 2 vec2s and return the result
ZEST_API zest_vec2 zest_LerpVec2(zest_vec2 *from, zest_vec2 *to, float lerp);
//Interpolate 2 floats and return the result
ZEST_API float zest_Lerp(float from, float to, float lerp);
//Initialise a new 4x4 matrix
ZEST_API zest_matrix4 zest_M4(float v);
//Flip the signs on a vec3 and return the result
ZEST_API zest_vec3 zest_FlipVec3(zest_vec3 vec3);
//Scale a 4x4 matrix by a vec4 and return the result
ZEST_API zest_matrix4 zest_ScaleMatrix4x4(zest_matrix4 *m, zest_vec4 *v);
//Scale a 4x4 matrix by a float scalar and return the result
ZEST_API zest_matrix4 zest_ScaleMatrix4(zest_matrix4 *m, float scalar);
//Set a vec2/3/4 with a single value
ZEST_API zest_vec2 zest_Vec2Set1(float v);
ZEST_API zest_vec3 zest_Vec3Set1(float v);
ZEST_API zest_vec4 zest_Vec4Set1(float v);
//Set a Vec2/3/4 with the values you pass into the vector
ZEST_API zest_vec2 zest_Vec2Set(float x, float y);
ZEST_API zest_vec3 zest_Vec3Set(float x, float y, float z);
ZEST_API zest_vec4 zest_Vec4Set(float x, float y, float z, float w);
//Set the color with the passed in rgba values
ZEST_API zest_color zest_ColorSet(zest_byte r, zest_byte g, zest_byte b, zest_byte a);
//Set the color with a single value
ZEST_API zest_color zest_ColorSet1(zest_byte c);
//--End General Math


//-----------------------------------------------
//        Camera_helpers
//-----------------------------------------------
//Create a new zest_camera_t. This will initialise values to default values such as up, right, field of view etc.
ZEST_API zest_camera_t zest_CreateCamera(void);
//Turn the camera by a given amount scaled by the sensitivity that you pass into the function.
ZEST_API void zest_TurnCamera(zest_camera_t *camera, float turn_x, float turn_y, float sensitivity);
//Update the camera vector that points foward out of the camera. This is called automatically in zest_TurnCamera but if you update
//camera values manually then you can can call this to update
ZEST_API void zest_CameraUpdateFront(zest_camera_t *camera);
//Move the camera forward by a given speed
ZEST_API void zest_CameraMoveForward(zest_camera_t *camera, float speed);
//Move the camera backward by a given speed
ZEST_API void zest_CameraMoveBackward(zest_camera_t *camera, float speed);
//Move the camera up by a given speed
ZEST_API void zest_CameraMoveUp(zest_camera_t *camera, float speed);
//Move the camera down by a given speed
ZEST_API void zest_CameraMoveDown(zest_camera_t *camera, float speed);
//Straf the camera to the left by a given speed
ZEST_API void zest_CameraStrafLeft(zest_camera_t *camera, float speed);
//Straf the camera to the right by a given speed
ZEST_API void zest_CameraStrafRight(zest_camera_t *camera, float speed);
//Set the position of the camera
ZEST_API void zest_CameraPosition(zest_camera_t *camera, zest_vec3 position);
//Set the field of view for the camera
ZEST_API void zest_CameraSetFoV(zest_camera_t *camera, float degrees);
//Set the current pitch of the camera (looking up and down)
ZEST_API void zest_CameraSetPitch(zest_camera_t *camera, float degrees);
//Set the current yaw of the camera (looking left and right)
ZEST_API void zest_CameraSetYaw(zest_camera_t *camera, float degrees);
//Function to see if a ray intersects a plane. Returns true if the ray does intersect. If the ray intersects then the distance and intersection point is inserted
//into the pointers you pass into the function.
ZEST_API zest_bool zest_RayIntersectPlane(zest_vec3 ray_origin, zest_vec3 ray_direction, zest_vec3 plane, zest_vec3 plane_normal, float *distance, zest_vec3 *intersection);
//Project x and y screen coordinates into 3d camera space. Pass in the x and y screen position, the width and height of the screen and the projection and view matrix
//of the camera which you'd usually retrieve from the uniform buffer.
ZEST_API zest_vec3 zest_ScreenRay(float xpos, float ypos, float view_width, float view_height, zest_matrix4 *projection, zest_matrix4 *view);
//Convert world 3d coordinates into screen x and y coordinates
ZEST_API zest_vec2 zest_WorldToScreen(const float point[3], float view_width, float view_height, zest_matrix4* projection, zest_matrix4* view);
//Convert world orthographic 3d coordinates into screen x and y coordinates
ZEST_API zest_vec2 zest_WorldToScreenOrtho(const float point[3], float view_width, float view_height, zest_matrix4* projection, zest_matrix4* view);
//Create a perspective 4x4 matrix passing in the fov in radians, aspect ratio of the viewport and te near/far values
ZEST_API zest_matrix4 zest_Perspective(float fovy, float aspect, float zNear, float zFar);
//Calculate the 6 planes of the camera fustrum
ZEST_API void zest_CalculateFrustumPlanes(zest_matrix4 *view_matrix, zest_matrix4 *proj_matrix, zest_vec4 planes[6]);
//Take the 6 planes of a camera fustrum and determine if a point is inside that fustrum
ZEST_API zest_bool zest_IsPointInFrustum(const zest_vec4 planes[6], const float point[3]);
//Take the 6 planes of a camera fustrum and determine if a sphere is inside that fustrum
ZEST_API zest_bool zest_IsSphereInFrustum(const zest_vec4 planes[6], const float point[3], float radius);
//-- End camera and other helpers

//-----------------------------------------------
//        Images_and_textures
//        Use textures to load in images and sample in a shader to display on screen. They're also used in
//        render targets and any other things where you want to create images.
//-----------------------------------------------
//Create a new texture. Give the texture a unique name. There are a few ways that you can store images in the texture:
/*
zest_texture_storage_type_packed            Pack all of the images into a sprite sheet and onto multiple layers in an image array on the GPU
zest_texture_storage_type_bank              Packs all images one per layer, best used for repeating textures or color/bump/specular etc
zest_texture_storage_type_sprite_sheet      Packs all the images onto a single layer spritesheet
zest_texture_storage_type_single            A single image texture
zest_texture_storage_type_storage           A storage texture useful for manipulation and other things in a compute shader
zest_texture_storage_type_stream            A storage texture that you can update every frame (not sure if this is working currently) 
zest_texture_storage_type_render_target     Texture storage for a render target sampler, so that you can draw the target onto 
                                            another render target. Generally this is used only when creating a render_target so 
                                            you don't have to worry about it set use filtering to 1 if you want to generate mipmaps 
                                            for the texture. You can also choose from the following texture formats: 
                                            zest_texture_format_alpha = VK_FORMAT_R8_UNORM, zest_texture_format_rgba_unorm = VK_FORMAT_R8G8B8A8_UNORM, zest_texture_format_bgra_unorm = VK_FORMAT_B8G8R8A8_UNORM. */ 
ZEST_API zest_image_info_t zest_CreateImageInfo(zest_uint width, zest_uint height);
ZEST_API zest_image_view_create_info_t zest_CreateViewImageInfo(zest_image_handle image_handle);
ZEST_API zest_image_handle zest_CreateImage(zest_image_info_t *create_info);
ZEST_API zest_image_view_handle zest_CreateImageView(zest_image_handle image_handle, zest_image_view_create_info_t *create_info);
ZEST_API zest_image_view_array_handle zest_CreateImageViewsPerMip(zest_image_handle image_handle);
ZEST_API void zest_FreeImage(zest_image_handle image_handle);
ZEST_API zest_imgui_image_t zest_NewImGuiImage(void);
ZEST_API zest_atlas_region zest_CreateAtlasRegion(zest_image_handle handle);
ZEST_API void zest_FreeAtlasRegion(zest_atlas_region region);
ZEST_API zest_atlas_region zest_CreateAnimation(zest_uint frames);
zest_image_handle zest_LoadCubemap(const char *name, const char *file_name);
//Load a bitmap from a file. Set color_channels to 0 to auto detect the number of channels
ZEST_API void zest_LoadBitmapImage(zest_bitmap image, const char *file, int color_channels);
//Load a bitmap from a memory buffer. Set color_channels to 0 to auto detect the number of channels. Pass in a pointer to the memory buffer containing
//the bitmap, the size in bytes and how many channels it has.
ZEST_API void zest_LoadBitmapImageMemory(zest_bitmap image, const unsigned char *buffer, int size, int desired_no_channels);
//Free the memory used in a zest_bitmap including the bitmap handle itself
ZEST_API void zest_FreeBitmap(zest_bitmap image);
//Free the memory used in a zest_bitmap_t
ZEST_API void zest_FreeBitmapData(zest_bitmap image);
//Create a new initialise zest_bitmap_t
ZEST_API zest_bitmap zest_NewBitmap(void);
//Create a new bitmap from a pixel buffer. Pass in the name of the bitmap, a pointer to the buffer, the size in bytes of the buffer, the width and height
//and the number of color channels
ZEST_API zest_bitmap zest_CreateBitmapFromRawBuffer(const char *name, unsigned char *pixels, int size, int width, int height, int channels);
//Allocate the memory for a bitmap based on the width, height and number of color channels. You can also specify the fill color
ZEST_API void zest_AllocateBitmap(zest_bitmap bitmap, int width, int height, int channels);
//Allocate the memory for a bitmap based on the width, height and number of color channels. You can also specify the fill color
ZEST_API void zest_AllocateBitmapAndClear(zest_bitmap bitmap, int width, int height, int channels, zest_color fill_color);
//Allocate the memory for a bitmap based the number of bytes required
ZEST_API void zest_AllocateBitmapMemory(zest_bitmap bitmap, zest_size size_in_bytes);
//Copy all of a source bitmap to a destination bitmap
ZEST_API void zest_CopyWholeBitmap(zest_bitmap src, zest_bitmap dst);
//Copy an area of a source bitmap to another bitmap
ZEST_API void zest_CopyBitmap(zest_bitmap src, int from_x, int from_y, int width, int height, zest_bitmap dst, int to_x, int to_y);
//Convert a bitmap to a specific vulkan color format. Accepted formats are:
//VK_FORMAT_R8G8B8A8_UNORM
//VK_FORMAT_B8G8R8A8_UNORM
//VK_FORMAT_R8_UNORM
ZEST_API void zest_ConvertBitmapToTextureFormat(zest_bitmap src, VkFormat format);
//Convert a bitmap to a specific zest_texture_format. Accepted values are;
//zest_texture_format_alpha
//zest_texture_format_rgba_unorm
//zest_texture_format_bgra_unorm
ZEST_API void zest_ConvertBitmap(zest_bitmap src, zest_texture_format format, zest_byte alpha_level);
//Convert a bitmap to BGRA format
ZEST_API void zest_ConvertBitmapToBGRA(zest_bitmap src, zest_byte alpha_level);
//Fill a bitmap with a color
ZEST_API void zest_FillBitmap(zest_bitmap image, zest_color color);
//plot a single pixel in the bitmap with a color
ZEST_API void zest_PlotBitmap(zest_bitmap image, int x, int y, zest_color color);
//Convert a bitmap to RGBA format
ZEST_API void zest_ConvertBitmapToRGBA(zest_bitmap src, zest_byte alpha_level);
//Convert a BGRA bitmap to RGBA format
ZEST_API void zest_ConvertBGRAToRGBA(zest_bitmap src);
//Convert a bitmap to a single alpha channel
ZEST_API void zest_ConvertBitmapToAlpha(zest_bitmap image);
ZEST_API void zest_ConvertBitmapTo1Channel(zest_bitmap image);
//Sample the color of a pixel in a bitmap with the given x/y coordinates
ZEST_API zest_color zest_SampleBitmap(zest_bitmap image, int x, int y);
//Get a pointer to the first pixel in a bitmap within the bitmap array. Index must be less than the number of bitmaps in the array
ZEST_API zest_byte *zest_BitmapArrayLookUp(zest_bitmap_array_t *bitmap_array, zest_index index);
//Get the distance in pixels to the furthes pixel from the center that isn't alpha 0
ZEST_API float zest_FindBitmapRadius(zest_bitmap image);
//Destory a bitmap array and free its resources
ZEST_API void zest_DestroyBitmapArray(zest_bitmap_array_t *bitmap_array);
//Get a bitmap from a bitmap array with the given index
ZEST_API zest_bitmap zest_GetImageFromArray(zest_bitmap_array_t *bitmap_array, zest_index index);
//After adding all the images you want to a texture, you will then need to process the texture which will create all of the necessary GPU resources and upload the texture to the GPU.
//You can then use the image handles to draw the images along with the descriptor set - either the one that gets created automatically with the the texture to draw sprites and billboards
//or your own descriptor set.
//Don't call this in the middle of sending draw commands that are using the texture because it will switch the texture buffer
//index so some drawcalls will use the outdated texture, some the new. Call before any draw calls are made or better still,
//just call zest_ScheduleTextureReprocess which will recreate the texture between frames and then schedule a cleanup the next
//frame after.
ZEST_API void zest_CreateBitmapArray(zest_bitmap_array_t *images, int width, int height, int channels, zest_uint size_of_array);
ZEST_API void zest_InitialiseBitmapArray(zest_bitmap_array_t *images, zest_uint size_of_array);
//Free a bitmap array and return memory resources to the allocator
ZEST_API void zest_FreeBitmapArray(zest_bitmap_array_t *images);
//Free an image collection
ZEST_API void zest_FreeImageCollection(zest_image_collection image_collection);
//Set the handle of an image. This dictates where the image will be positioned when you draw it with zest_DrawSprite/zest_DrawBillboard. 0.5, 0.5 will center the image at the position you draw it.
ZEST_API void zest_SetImageHandle(zest_atlas_region image, float x, float y);
//Get the layer index that the image exists on in the texture
ZEST_API zest_index zest_ImageLayerIndex(zest_atlas_region image);
//Get the dimensions of the image
ZEST_API zest_extent2d_t zest_RegionDimensions(zest_atlas_region image);
//Get the uv coords of the image
ZEST_API zest_vec4 zest_ImageUV(zest_atlas_region image);
//Get the extent of the image
ZEST_API const zest_image_info_t *zest_ImageInfo(zest_image_handle image_handle);
//Get a descriptor index from an image for a specific binding number. You must have already acquired the index
//for the binding number first with zest_GetGlobalSampler.
ZEST_API zest_uint zest_ImageDescriptorIndex(zest_image_handle image_handle, zest_global_binding_number binding_number);
//Get the raw value of the current image image of the image as an int. This will be whatever layout is represented
//in the backend api you are using (vulkan, metal, dx12 etc.). You can use this if you just need the current layout
//of the image without translating it into a zest layout enum because you just need to know if the layout changed
//for frame graph caching purposes etc.
ZEST_API int zest_ImageRawLayout(zest_image_handle image_handle);

// --Sampler functions
//Gets a sampler from the sampler storage in the renderer. If no match is found for the info that you pass into the sampler
//then a new one will be created.
ZEST_API zest_sampler_handle zest_CreateSampler(zest_sampler_info_t *info);
ZEST_API zest_sampler_handle zest_CreateSamplerForImage(zest_image_handle image_handle);
ZEST_API zest_sampler_info_t zest_CreateSamplerInfo();
ZEST_API zest_sampler_info_t zest_CreateMippedSamplerInfo(zest_uint mip_levels);
//-- End Images and textures

//-----------------------------------------------
//-- Draw_Layers_API
//-----------------------------------------------

//Create a new layer for instanced drawing. This just creates a standard layer with default options and callbacks, all
//you need to pass in is the size of type used for the instance struct that you'll use with whatever pipeline you setup
//to use with the layer.
ZEST_API zest_layer_handle zest_CreateInstanceLayer(const char* name, zest_size type_size);
//Creates a layer with buffers for each frame in flight located on the device. This means that you can manually decide
//When to upload to the buffer on the render graph rather then using transient buffers each frame that will be
//discarded. In order to avoid syncing issues on the GPU, pass a unique id to generate a unique VkBuffer. This
//id can be shared with any other frame in flight layer that will flip their frame in flight index at the same
//time, like when ever the update loop is run.
ZEST_API zest_layer_handle zest_CreateFIFInstanceLayer(const char *name, zest_size type_size, zest_uint id);
//Create a new layer builder which you can use to build new custom layers to draw with using instances
ZEST_API zest_layer_builder_t zest_NewInstanceLayerBuilder(zest_size type_size);
//Once you have configured your layer you can call this to create the layer ready for adding to a command queue
ZEST_API zest_layer_handle zest_FinishInstanceLayer(const char *name, zest_layer_builder_t *builder);
//Start a new set of draw instructs for a standard zest_layer. These were internal functions but they've been made api functions for making you're own custom
//instance layers more easily
ZEST_API void zest_StartInstanceInstructions(zest_layer_handle layer);
//Set the layer frame in flight to the next layer. Use this if you're manually setting the current fif for the layer so
//that you can avoid uploading the staging buffers every frame and only do so when it's neccessary.
ZEST_API void zest_ResetLayer(zest_layer_handle layer);
//Same as ResetLayer but specifically for an instance layer
ZEST_API void zest_ResetInstanceLayer(zest_layer_handle layer);
//End a set of draw instructs for a standard zest_layer
ZEST_API void zest_EndInstanceInstructions(zest_layer_handle layer);
//Callback that can be used to upload layer data to the gpu
ZEST_API void zest_UploadInstanceLayerData(const zest_frame_graph_context context, void *user_data);
//For layers that are manually flipping the frame in flight, we can use this to only end the instructions if the last know fif for the layer
//is not equal to the current one. Returns true if the instructions were ended false if not. If true then you can assume that the staging
//buffer for the layer can then be uploaded to the gpu. This should be called in an upload buffer callback in any custom draw routine/layer.
ZEST_API zest_bool zest_MaybeEndInstanceInstructions(zest_layer_handle layer);
//Get the current size in bytes of all instances being drawn in the layer
ZEST_API zest_size zest_GetLayerInstanceSize(zest_layer_handle layer);
//Reset the drawing for an instance layer. This is called after all drawing is done and dispatched to the gpu
ZEST_API void zest_ResetInstanceLayerDrawing(zest_layer_handle layer);
//Get the current amount of instances being drawn in the layer
ZEST_API zest_uint zest_GetInstanceLayerCount(zest_layer_handle layer);
//Get the pointer to the current instance in the layer if it's an instanced based layer (meaning you're drawing instances of sprites, billboards meshes etc.)
//This will return a void* so you can cast it to whatever struct you're using for the instance data
#define zest_GetLayerInstance(type, layer, fif) (type*)layer->memory_refs[fif].instance_ptr
//Move the pointer in memory to the next instance to write to.
ZEST_API void zest_NextInstance(zest_layer_handle layer);
//Free a layer and all it's resources
ZEST_API void zest_FreeLayer(zest_layer_handle layer);
//Set the viewport of a layer. This is important to set right as when the layer is drawn it needs to be clipped correctly and in a lot of cases match how the
//uniform buffer is setup
ZEST_API void zest_SetLayerViewPort(zest_layer_handle layer, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height);
ZEST_API void zest_SetLayerScissor(zest_layer_handle layer, int offset_x, int offset_y, zest_uint scissor_width, zest_uint scissor_height);
//Update the layer viewport to match the swapchain
ZEST_API void zest_SetLayerSizeToSwapchain(zest_layer_handle layer, zest_swapchain swapchain);
//Set the size of the layer. Called internally to set it to the window size. Can this be internal?
ZEST_API void zest_SetLayerSize(zest_layer_handle layer, float width, float height);
//Set the layer color. This is used to apply color to the sprite/font/billboard that you're drawing or you can use it in your own draw routines that use zest_layers.
//Note that the alpha value here is actually a transition between additive and alpha blending. Where 0 is the most additive and 1 is the most alpha blending. Anything
//imbetween is a combination of the 2.
ZEST_API void zest_SetLayerColor(zest_layer_handle layer, zest_byte red, zest_byte green, zest_byte blue, zest_byte alpha);
ZEST_API void zest_SetLayerColorf(zest_layer_handle layer, float red, float green, float blue, float alpha);
//Set the intensity of the layer. This is used to apply an alpha value to the sprite or billboard you're drawing or you can use it in your own draw routines that
//use zest_layers. Note that intensity levels can exceed 1.f to make your sprites extra bright because of pre-multiplied blending in the sprite.
ZEST_API void zest_SetLayerIntensity(zest_layer_handle layer, float value);
//A dirty layer denotes that it's buffers have changed and therefore needs uploading to the GPU again. This is currently used for Dear Imgui layers.
ZEST_API void zest_SetLayerChanged(zest_layer_handle layer);
//Returns 1 if the layer is marked as changed
ZEST_API zest_bool zest_LayerHasChanged(zest_layer_handle layer);
//Set the user data of a layer. You can use this to extend the functionality of the layers for your own needs.
ZEST_API void zest_SetLayerUserData(zest_layer_handle layer, void *data);
//Get the user data from the layer
#define zest_GetLayerUserData(type, layer) ((type*)layer->user_data)
ZEST_API zest_uint zest_GetLayerVertexDescriptorIndex(zest_layer_handle layer, bool last_frame);
ZEST_API zest_buffer zest_GetLayerResourceBuffer(zest_layer_handle layer);
ZEST_API zest_buffer zest_GetLayerStagingVertexBuffer(zest_layer_handle layer);
ZEST_API zest_buffer zest_GetLayerStagingIndexBuffer(zest_layer_handle layer);
ZEST_API void zest_UploadLayerStagingData(zest_layer_handle layer, const zest_frame_graph_context context);
//-- End Draw Layers


//-----------------------------------------------
//        Draw_instance_layers
//        General purpose functions you can use to either draw built in instances like billboards and sprites or your own custom instances
//-----------------------------------------------
//Set the texture, descriptor set shader_resources and pipeline for any calls to the same layer that come after it. You must call this function if you want to do any instance drawing for a particular layer, and you
//must call it again if you wish to switch either the texture, descriptor set or pipeline to do the drawing. Everytime you call this function it creates a new draw instruction
//in the layer for drawing instances so each call represents a separate draw call in the render. So if you just call this function once you can call a draw instance function as many times
//as you want (like zest_DrawBillboard or your own custom draw instance function) and they will all be drawn with a single draw call.
//Pass in the zest_layer, zest_texture, zest_descriptor_set and zest_pipeline. A few things to note:
//1) The descriptor layout used to create the descriptor sets in the shader_resources must match the layout used in the pipeline.
//2) You can pass 0 in the descriptor set and it will just use the default descriptor set used in the texture.
ZEST_API void zest_SetInstanceDrawing(zest_layer_handle layer, zest_shader_resources_handle shader_resources,  zest_pipeline_template pipeline);
//Draw all the contents in a buffer. You can use this if you prepare all the instance data elsewhere in your code and then want
//to just dump it all into the staging buffer of the layer in one go. This will move the instance pointer in the layer to the next point
//in the buffer as well as bump up the instance count by the amount you pass into the function. The instance buffer will be grown if
//there is not enough room.
//Note that the struct size of the data you're copying MUST be the same size as the layer->struct_size.
ZEST_API zest_draw_buffer_result zest_DrawInstanceBuffer(zest_layer_handle layer, void *src, zest_uint amount);
//In situations where you write directly to a staging buffer you can use this function to simply tell the draw instruction
//how many instances should be drawn. You will still need to call zest_SetInstanceDrawing
ZEST_API void zest_DrawInstanceInstruction(zest_layer_handle layer, zest_uint amount);
//Set the viewport and scissors of the next draw instructions for a layer. Otherwise by default it will use either the screen size
//of or the viewport size you set with zest_SetLayerViewPort
ZEST_API void zest_SetLayerDrawingViewport(zest_layer_handle layer, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height);
//Set the current instruction push contants in the layer
ZEST_API void zest_SetLayerPushConstants(zest_layer_handle layer, void *push_constants, zest_size size);
#define zest_CastLayerPushConstants(type, layer) (type*)layer->current_instruction.push_constant


//-----------------------------------------------
//        Draw_mesh_layers
//        Mesh layers let you upload a vertex and index buffer to draw meshes. I set this up primarily for
//        use with Dear ImGui
//-----------------------------------------------
ZEST_API zest_buffer zest_GetVertexWriteBuffer(zest_layer_handle layer);
//Get the index staging buffer. You'll need to get the staging buffers to copy your mesh data to or even just record mesh data directly to the staging buffer
ZEST_API zest_buffer zest_GetIndexWriteBuffer(zest_layer_handle layer);
//Grow the mesh vertex buffers. You must update the buffer->memory_in_use so that it can decide if a buffer needs growing
ZEST_API void zest_GrowMeshVertexBuffers(zest_layer_handle layer);
//Grow the mesh index buffers. You must update the buffer->memory_in_use so that it can decide if a buffer needs growing
ZEST_API void zest_GrowMeshIndexBuffers(zest_layer_handle layer);
//Set the mesh drawing specifying any texture, descriptor set and pipeline that you want to use for the drawing
ZEST_API void zest_SetMeshDrawing(zest_layer_handle layer, zest_shader_resources_handle shader_resources, zest_pipeline_template pipeline);
//Helper funciton Push a vertex to the vertex staging buffer. It will automatically grow the buffers if needed
ZEST_API void zest_PushVertex(zest_layer_handle layer, float pos_x, float pos_y, float pos_z, float intensity, float uv_x, float uv_y, zest_color color, zest_uint parameters);
//Helper funciton Push an index to the index staging buffer. It will automatically grow the buffers if needed
ZEST_API void zest_PushIndex(zest_layer_handle layer, zest_uint offset);

//-----------------------------------------------
//        Draw_instance_mesh_layers
//        Instance mesh layers are for creating meshes and then drawing instances of them. It should be
//        one mesh per layer (and obviously many instances of that mesh can be drawn with the layer).
//        Very basic stuff currently, I'm just using them to create 3d widgets I can use in TimelineFX
//        but this can all be expanded on for general 3d models in the future.
//-----------------------------------------------
ZEST_API void zest_SetInstanceMeshDrawing(zest_layer_handle layer, zest_shader_resources_handle shader_resources, zest_pipeline_template pipeline);
//Push a zest_vertex_t to a mesh. Use this and PushMeshTriangle to build a mesh ready to be added to an instance mesh layer
ZEST_API void zest_PushMeshVertex(zest_mesh mesh, float pos_x, float pos_y, float pos_z, zest_color color);
//Push an index to a mesh to build triangles
ZEST_API void zest_PushMeshIndex(zest_mesh mesh, zest_uint index);
//Rather then PushMeshIndex you can call this to add three indexes at once to build a triangle in the mesh
ZEST_API void zest_PushMeshTriangle(zest_mesh mesh, zest_uint i1, zest_uint i2, zest_uint i3);
//Create a new mesh and return the handle to it
ZEST_API zest_mesh zest_NewMesh();
//Free the memeory used for the mesh. You can free the mesh once it's been added to the layer
ZEST_API void zest_FreeMesh(zest_mesh mesh);
//Set the position of the mesh in it's transform matrix
ZEST_API void zest_PositionMesh(zest_mesh mesh, zest_vec3 position);
//Rotate a mesh by the given pitch, yaw and roll values (in radians)
ZEST_API zest_matrix4 zest_RotateMesh(zest_mesh mesh, float pitch, float yaw, float roll);
//Transform a mesh by the given pitch, yaw and roll values (in radians), position x, y, z and scale sx, sy, sz
ZEST_API zest_matrix4 zest_TransformMesh(zest_mesh mesh, float pitch, float yaw, float roll, float x, float y, float z, float sx, float sy, float sz);
//Calculate the normals of a mesh. This will unweld the mesh to create faceted normals.
ZEST_API void zest_CalculateNormals(zest_mesh mesh);
//Initialise a new bounding box to 0
ZEST_API zest_bounding_box_t zest_NewBoundingBox();
//Calculate the bounding box of a mesh and return it
ZEST_API zest_bounding_box_t zest_GetMeshBoundingBox(zest_mesh mesh);
//Add all the vertices and indices of a mesh to another mesh to combine them into a single mesh.
ZEST_API void zest_AddMeshToMesh(zest_mesh dst_mesh, zest_mesh src_mesh);
//Set the group id for every vertex in the mesh. This can be used in the shader to identify different parts of the mesh and do different shader stuff with them.
ZEST_API void zest_SetMeshGroupID(zest_mesh mesh, zest_uint group_id);
//Add a mesh to an instanced mesh layer. Existing vertex data in the layer will be deleted.
ZEST_API void zest_AddMeshToLayer(zest_layer_handle layer, zest_mesh src_mesh);
//Get the size in bytes for the vertex data in a mesh
ZEST_API zest_size zest_MeshVertexDataSize(zest_mesh mesh);
//Get the size in bytes for the index data in a mesh
ZEST_API zest_size zest_MeshIndexDataSize(zest_mesh mesh);
//Draw an instance of a mesh with an instanced mesh layer. Pass in the position, rotations and scale to transform the instance.
//You must call zest_SetInstanceDrawing before calling this function as many times as you need.
ZEST_API void zest_DrawInstancedMesh(zest_layer_handle mesh_layer, float pos[3], float rot[3], float scale[3]);
//Create a cylinder mesh of given number of sides, radius and height. Set cap to 1 to cap the cylinder.
ZEST_API zest_mesh zest_CreateCylinder(int sides, float radius, float height, zest_color color, zest_bool cap);
//Create a cone mesh of given number of sides, radius and height.
ZEST_API zest_mesh zest_CreateCone(int sides, float radius, float height, zest_color color);
//Create a uv sphere mesh made up using a number of horizontal rings and vertical sectors of a give radius.
ZEST_API zest_mesh zest_CreateSphere(int rings, int sectors, float radius, zest_color color);
//Create a cube mesh of a given size.
ZEST_API zest_mesh zest_CreateCube(float size, zest_color color);
//Create a flat rounded rectangle of a give width and height. Pass in the radius to use for the corners and number of segments to use for the corners.
ZEST_API zest_mesh zest_CreateRoundedRectangle(float width, float height, float radius, int segments, zest_bool backface, zest_color color);
//--End Instance Draw mesh layers

//-----------------------------------------------
//        Compute_shaders
//        Build custom compute shaders and integrate into a command queue or just run separately as you need.
//        See zest-compute-example for a full working example
//-----------------------------------------------
//Create a blank ready-to-build compute object and store by name in the renderer.
ZEST_PRIVATE zest_compute zest__new_compute(const char *name);
//To build a compute shader pipeline you can use a zest_compute_builder_t and corresponding commands to add the various settings for the compute object
ZEST_API zest_compute_builder_t zest_BeginComputeBuilder();
ZEST_API void zest_SetComputeBindlessLayout(zest_compute_builder_t *builder, zest_set_layout_handle bindless_layout);
ZEST_API void zest_AddComputeSetLayout(zest_compute_builder_t *builder, zest_set_layout_handle layout);
//Add a shader to the compute builder. This will be the shader that is executed on the GPU. Pass a file path where to find the shader.
ZEST_API zest_index zest_AddComputeShader(zest_compute_builder_t *builder, zest_shader_handle shader);
//If you're using a push constant then you can set it's size in the builder here.
ZEST_API void zest_SetComputePushConstantSize(zest_compute_builder_t *builder, zest_uint size);
//Set any pointer to custom user data here. You will be able to access this in the callback functions.
ZEST_API void zest_SetComputeUserData(zest_compute_builder_t *builder, void *data);
//Sets the compute shader to manually record so you have to specify when the compute shader should be recorded
ZEST_API void zest_SetComputeManualRecord(zest_compute_builder_t *builder);
//Sets the compute shader to use a primary command buffer recorder so that you can use it with the zest_RunCompute command.
//This means that you will not be able to use this compute shader in a frame loop alongside other render routines.
ZEST_API void zest_SetComputePrimaryRecorder(zest_compute_builder_t *builder);
//Once you have finished calling the builder commands you will need to call this to actually build the compute shader. Pass a pointer to the builder and the zest_compute
//handle that you got from calling zest__new_compute. You can then use this handle to add the compute shader to a command queue with zest_NewComputeSetup in a
//command queue context (see the section on Command queue setup and creation)
ZEST_API zest_compute_handle zest_FinishCompute(zest_compute_builder_t *builder, const char *name);
//Reset compute when manual_fif is being used. This means that you can chose when you want a compute command buffer
//to be recorded again. 
ZEST_API void zest_ResetCompute(zest_compute_handle compute);
//Default always true condition for recording compute command buffers
ZEST_API int zest_ComputeConditionAlwaysTrue(zest_compute_handle compute);
//Free a compute shader and all its resources
ZEST_API void zest_FreeCompute(zest_compute_handle compute);
//--End Compute shaders

//-----------------------------------------------
//        Events_and_States
//-----------------------------------------------
//Returns true if the swap chain was recreated last frame. The swap chain will mainly be recreated if the window size changes
ZEST_API zest_bool zest_SwapchainWasRecreated(zest_swapchain swapchain);
//--End Events and States

//-----------------------------------------------
//        Timer_functions
//        This is a simple API for a high resolution timer. You can use this to implement fixed step
//        updating for your logic in the main loop, plus for anything else that you need to time.
//-----------------------------------------------
ZEST_API zest_timer_handle zest_CreateTimer(double update_frequency);                                  //Create a new timer and return its handle
ZEST_API void zest_FreeTimer(zest_timer_handle timer);                                                 //Free a timer and its memory
ZEST_API void zest_TimerSetUpdateFrequency(zest_timer_handle timer, double update_frequency);          //Set the update frequency for timing loop functions, accumulators and such
ZEST_API void zest_TimerSetMaxFrames(zest_timer_handle timer, double frames);                          //Set the maximum amount of frames that can pass each update. This helps avoid simulations blowing up
ZEST_API void zest_TimerReset(zest_timer_handle timer);                                                //Set the clock time to now
ZEST_API double zest_TimerDeltaTime(zest_timer_handle timer);                                          //The amount of time passed since the last tick
ZEST_API void zest_TimerTick(zest_timer_handle timer);                                                 //Update the delta time
ZEST_API double zest_TimerUpdateTime(zest_timer_handle timer);                                         //Gets the update time (1.f / update_frequency)
ZEST_API double zest_TimerFrameLength(zest_timer_handle timer);                                        //Gets the update_tick_length (1000.f / update_frequency)
ZEST_API double zest_TimerAccumulate(zest_timer_handle timer);                                         //Accumulate the amount of time that has passed since the last render
ZEST_API int zest_TimerPendingTicks(zest_timer_handle timer);                                          //Returns the number of times the update loop will run this frame.
ZEST_API void zest_TimerUnAccumulate(zest_timer_handle timer);                                         //Unaccumulate 1 tick length from the accumulator. While the accumulator is more then the tick length an update should be done
ZEST_API zest_bool zest_TimerDoUpdate(zest_timer_handle timer);                                        //Return true if accumulator is more or equal to the update_tick_length
ZEST_API double zest_TimerLerp(zest_timer_handle timer);                                               //Return the current tween/lerp value
ZEST_API void zest_TimerSet(zest_timer_handle timer);                                                  //Set the current tween value
ZEST_API double zest_TimerUpdateFrequency(zest_timer_handle timer);                                    //Get the update frequency set in the timer
ZEST_API zest_bool zest_TimerUpdateWasRun(zest_timer_handle timer);                                    //Returns true if an update loop was run
//Help macros for starting/ending an update loop if you prefer.
#define zest_StartTimerLoop(timer) 	zest_TimerAccumulate(timer);   \
		int pending_ticks = zest_TimerPendingTicks(timer);	\
		while (zest_TimerDoUpdate(timer)) {
#define zest_EndTimerLoop(timer) zest_TimerUnAccumulate(timer); \
	} \
	zest_TimerSet(timer);
//--End Timer Functions

//-----------------------------------------------
//        Window_functions
//-----------------------------------------------
ZEST_API void zest_SetWindowMode(zest_window window, zest_window_mode mode);
ZEST_API void zest_SetWindowSize(zest_window window, zest_uint width, zest_uint height);
ZEST_API void zest_UpdateWindowSize(zest_window window, zest_uint width, zest_uint height);
ZEST_API void zest_SetWindowHandle(zest_window window, void *handle);
ZEST_API zest_window zest_GetCurrentWindow(void);
ZEST_API void zest_CloseWindow(zest_window window);
ZEST_API void zest_CleanupWindow(zest_window window);
ZEST_API VkSurfaceKHR zest_WindowSurface(void);
ZEST_API void zest_SetWindowSurface(VkSurfaceKHR surface);
//Return a pointer to the window handle stored in the zest_window_t. This could be anything so it's up to you to cast to the right data type. For example, if you're
//using GLFW then you would cast it to a GLFWwindow*
ZEST_API void *zest_Window(void);

//-----------------------------------------------
//        General_Helper_functions
//-----------------------------------------------
//Read a file from disk into memory. Set terminate to 1 if you want to add \0 to the end of the file in memory
ZEST_API char* zest_ReadEntireFile(const char *file_name, zest_bool terminate);
//Get the swap chain extent which will basically be the size of the window returned in a VkExtend2d struct.
ZEST_API zest_extent2d_t zest_GetSwapChainExtent(void);
//Get the window size in a VkExtent2d. In most cases this is the same as the swap chain extent.
ZEST_API zest_extent2d_t zest_GetWindowExtent(void);
//Get the current swap chain width
ZEST_API zest_uint zest_SwapChainWidth(void);
//Get the current swap chain height
ZEST_API zest_uint zest_SwapChainHeight(void);
//Get the current swap chain width as a float
ZEST_API float zest_SwapChainWidthf(void);
//Get the current swap chain height as a float
ZEST_API float zest_SwapChainHeightf(void);
//Get the current screen width
ZEST_API zest_uint zest_ScreenWidth(void);
//Get the current screen height
ZEST_API zest_uint zest_ScreenHeight(void);
//Get the current screen width as a float
ZEST_API float zest_ScreenWidthf(void);
//Get the current screen height as a float
ZEST_API float zest_ScreenHeightf(void);
//Get the current mouse x screen coordinate as a float
ZEST_API float zest_MouseXf();
//Get the current mouse y screen coordinate as a float
ZEST_API float zest_MouseYf();
//Returns true if the mouse button is held down
ZEST_API bool zest_MouseDown(zest_mouse_button button);
//Returns true if the mouse button has been pressed and then released since the last frame
ZEST_API bool zest_MouseHit(zest_mouse_button button);
//For retina screens this will return the current screen DPI
ZEST_API float zest_DPIScale(void);
//Set the DPI scale
ZEST_API void zest_SetDPIScale(float scale);
//Get the current frames per second
ZEST_API zest_uint zest_FPS(void);
//Get the current frames per second as a float
ZEST_API float zest_FPSf(void);
//Get the current speed of the mouse in pixels per second. Results are stored in the floats you pass into the function as pointers.
ZEST_API void zest_GetMouseSpeed(double *x, double *y);
//Wait for the device to be idle (finish executing all commands). Only recommended if you need to do a one-off operation like change a texture that could
//still be in use by the GPU
ZEST_API void zest_WaitForIdleDevice(void);
//If you pass true to this function (or any value other then 0) then the zest_app_flag_quit_application meaning the main loop with break at the end of the next frame.
ZEST_API void zest_MaybeQuit(zest_bool condition);
//Enable vsync so that the frame rate is limited to the current monitor refresh rate. Will cause the swap chain to be rebuilt.
ZEST_API void zest_EnableVSync(void);
//Disable vsync so that the frame rate is not limited to the current monitor refresh rate, frames will render as fast as they can. Will cause the swap chain to be rebuilt.
ZEST_API void zest_DisableVSync(void);
//Log the current FPS to the console once every second
ZEST_API void zest_LogFPSToConsole(zest_bool yesno);
//Set the current frame in flight. You should only use this if you want to execute a floating command queue that contains resources that only have a single frame in flight.
//Once you're done you can call zest_RestoreFrameInFlight
ZEST_API void zest_SetFrameInFlight(zest_uint fif);
//Only use after a call to zest_SetFrameInFlight. Will restore the frame in flight that was in use before the call to zest_SetFrameInFlight.
ZEST_API void zest_RestoreFrameInFlight(void);
//Create a new VkFence
ZEST_API VkFence zest_CreateFence();
//Checks to see if a fence is signalled and if so will destroy the fence.
ZEST_API zest_bool zest_CheckFence(VkFence fence);
//Will wait until a fence has been signalled
ZEST_API void zest_WaitForFence(VkFence fence);
//Destroy a fence that you don't need any more and return its resources.
ZEST_API void zest_DestroyFence(VkFence fence);
//Reset a VkEvent to prepare it for being signalled
ZEST_API void zest_ResetEvent(VkEvent e);
//Found out if memory properties are available for the current physical device
ZEST_API zest_bool zest_IsMemoryPropertyAvailable(VkMemoryPropertyFlags flags);
//Find out if the current GPU can use memory that is both device local and cpu visible. If it can then that means that you write directly to gpu memory without the need for
//a staging buffer. AMD cards tend to be better at supporting this then nvidia as it seems nvidia only introduced it later.
ZEST_API zest_bool zest_GPUHasDeviceLocalHostVisible(VkDeviceSize mimimum_size);
//Convert a linear color value to an srgb color space value
ZEST_API float zest_LinearToSRGB(float value);
//Check for valid handles:
ZEST_API zest_bool zest_IsValidComputeHandle(zest_compute_handle compute_handle);
ZEST_API zest_bool zest_IsValidImageHandle(zest_image_handle image_handle);
//Tell the main loop that it should stop and exit the app
ZEST_API void zest_Terminate(void);
//--End General Helper functions

//-----------------------------------------------
//Debug_Helpers
//-----------------------------------------------
//About all of the current memory usage that you're using in the renderer to the console. This won't include any allocations you've done elsewhere using you own allocation
//functions or malloc/virtual_alloc etc.
ZEST_API void zest_OutputMemoryUsage();
//Set the path to the log where vulkan validation messages and other log messages can be output to. If this is not set and ZEST_VALIDATION_LAYER is 1 then validation messages and other log
//messages will be output to the console if it's available.
ZEST_API zest_bool zest_SetErrorLogPath(const char *path);
//Print out any reports that have been collected to the console
ZEST_API void zest_PrintReports();
ZEST_PRIVATE void zest__print_block_info(void *allocation, zloc_header *current_block, zest_vulkan_memory_context context_filter, zest_vulkan_command command_filter);
ZEST_API void zest_PrintMemoryBlocks(zloc_header *first_block, zest_bool output_all, zest_vulkan_memory_context context_filter, zest_vulkan_command command_filter);
ZEST_API zest_uint zest_GetValidationErrorCount();
ZEST_API void zest_ResetValidationErrors();
//--End Debug Helpers

//-----------------------------------------------
// Command_buffer_functions
// All these functions are called inside a frame graph context in callbacks in order to perform commands
// on the GPU. These all require a platform specific implementation
//-----------------------------------------------
ZEST_API void zest_cmd_BlitImageMip(const zest_frame_graph_context context, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_pipeline_stages pipeline_stage);
ZEST_API void zest_cmd_CopyImageMip(const zest_frame_graph_context context, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_pipeline_stages pipeline_stage);
// -- Helper functions to insert barrier functions within pass callbacks
ZEST_API void zest_cmd_InsertComputeImageBarrier(const zest_frame_graph_context context, zest_resource_node resource, zest_uint base_mip);
//Set a screen sized viewport and scissor command in the render pass
ZEST_API void zest_cmd_SetScreenSizedViewport(const zest_frame_graph_context context, float min_depth, float max_depth);
ZEST_API void zest_cmd_Scissor(const zest_frame_graph_context context, zest_scissor_rect_t *scissor);
//Create a scissor and view port command. Must be called within a command buffer
ZEST_API void zest_cmd_Clip(const zest_frame_graph_context context, float x, float y, float width, float height, float minDepth, float maxDepth);
//Bind a pipeline for use in a draw routing. Once you have built the pipeline at some point you will want to actually use it to draw things.
//In order to do that you can bind the pipeline using this function. Just pass in the pipeline handle and a zest_shader_resources. Note that the
//descriptor sets in the shader_resources must be compatible with the layout that is being using in the pipeline. The command buffer used in the binding will be
//whatever is defined in ZestRenderer->current_command_buffer which will be set when the command queue is recorded. If you need to specify
//a command buffer then call zest_BindPipelineCB instead.
ZEST_API void zest_cmd_BindPipeline(const zest_frame_graph_context context, zest_pipeline pipeline, VkDescriptorSet *descriptor_set, zest_uint set_count);
//Bind a pipeline for a compute shader
ZEST_API void zest_cmd_BindComputePipeline(const zest_frame_graph_context context, zest_compute_handle compute, VkDescriptorSet *descriptor_set, zest_uint set_count);
//Bind a pipeline using a shader resource object. The shader resources must match the descriptor layout used in the pipeline that
//you pass to the function. Pass in a manual frame in flight which will be used as the fif for any descriptor set in the shader
//resource that is marked as static.
ZEST_API void zest_cmd_BindPipelineShaderResource(const zest_frame_graph_context context, zest_pipeline pipeline, zest_shader_resources_handle shader_resources);
//Copy a buffer to another buffer. Generally this will be a staging buffer copying to a buffer on the GPU (device_buffer). You must specify
//the size as well that you want to copy
ZEST_API zest_bool zest_cmd_CopyBufferOneTime(zest_buffer src_buffer, zest_buffer dst_buffer, VkDeviceSize size);
//Exactly the same as zest_CopyBuffer but you can specify a command buffer to use to make the copy. This can be useful if you are doing a
//one off copy with a separate command buffer
ZEST_API void zest_cmd_CopyBuffer(const zest_frame_graph_context context, zest_buffer staging_buffer, zest_buffer device_buffer, VkDeviceSize size);
ZEST_API zest_bool zest_cmd_UploadBuffer(const zest_frame_graph_context context, zest_buffer_uploader_t *uploader);
//Bind a vertex buffer. For use inside a draw routine callback function.
ZEST_API void zest_cmd_BindVertexBuffer(const zest_frame_graph_context context, zest_buffer buffer);
//Bind an index buffer. For use inside a draw routine callback function.
ZEST_API void zest_cmd_BindIndexBuffer(const zest_frame_graph_context context, zest_buffer buffer);
//Clear an image within a frame graph
ZEST_API zest_bool zest_cmd_ImageClear(zest_image_handle image, const zest_frame_graph_context context);
//Copies an area of a zest_texture to another zest_texture
ZEST_API zest_bool zest_cmd_CopyImageToImage(zest_image_handle src_image, zest_image_handle target, int src_x, int src_y, int dst_x, int dst_y, int width, int height);
//Copies an area of a zest_texture to a zest_bitmap_t.
ZEST_API zest_bool zest_cmd_CopyTextureToBitmap(zest_image_handle src_image, zest_bitmap image, int src_x, int src_y, int dst_x, int dst_y, int width, int height);
//Copies an area of a zest_bitmap to a zest_image.
ZEST_API zest_bool zest_cmd_CopyBitmapToImage(zest_bitmap src_bitmap, zest_image_handle dst_image_handle, int src_x, int src_y, int dst_x, int dst_y, int width, int height);
//Record the secondary buffers of an instance layer
ZEST_API void zest_cmd_DrawInstanceLayer(const zest_frame_graph_context context, void *user_data);
//Get the vertex staging buffer. You'll need to get the staging buffers to copy your mesh data to or even just record mesh data directly to the staging buffer
ZEST_API void zest_cmd_BindMeshVertexBuffer(const zest_frame_graph_context context, zest_layer_handle layer);
ZEST_API void zest_cmd_BindMeshIndexBuffer(const zest_frame_graph_context context, zest_layer_handle layer);
ZEST_API void zest_cmd_DrawInstanceMeshLayer(const zest_frame_graph_context context, void *user_data);
//Send custom push constants. Use inside a compute update command buffer callback function. The push constatns you pass in to the 
//function must be the same size that you set when creating the compute shader
ZEST_API void zest_cmd_SendCustomComputePushConstants(const zest_frame_graph_context context, zest_compute_handle compute, const void *push_constant);
//Helper function to dispatch a compute shader so you can call this instead of vkCmdDispatch. Specify a command buffer for use in one off dispataches
ZEST_API void zest_cmd_DispatchCompute(const zest_frame_graph_context context, zest_compute_handle compute, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z);
//Send push constants. For use inside a draw routine callback function. pass in the pipeline,
//and a pointer to the data containing the push constants. The data MUST match the push constant range in the pipeline
ZEST_API void zest_cmd_SendPushConstants(const zest_frame_graph_context context, zest_pipeline pipeline, void *data);
//Helper function to send the standard zest_push_constants_t push constants struct.
ZEST_API void zest_cmd_SendStandardPushConstants(const zest_frame_graph_context context, zest_pipeline_t *pipeline_layout, void *data);
//Helper function to record the vulkan command vkCmdDraw. Will record with the current command buffer being used in the active command queue. For use inside
//a draw routine callback function
ZEST_API void zest_cmd_Draw(const zest_frame_graph_context context, zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance);
ZEST_API void zest_cmd_DrawLayerInstruction(const zest_frame_graph_context context, zest_uint vertex_count, zest_layer_instruction_t *instruction);
//Helper function to record the vulkan command vkCmdDrawIndexed. Will record with the current command buffer being used in the active command queue. For use inside
//a draw routine callback function
ZEST_API void zest_cmd_DrawIndexed(const zest_frame_graph_context context, zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance);

// Platform_access_functions
#if defined(ZEST_VULKAN)
ZEST_API VkInstance zest_GetVKInstance();
ZEST_API VkAllocationCallbacks *zest_GetVKAllocationCallbacks();
#endif
// -- End platform access functions

#ifdef __cplusplus
}
#endif

#endif // ! ZEST_POCKET_RENDERER

#if defined(ZEST_IMPLEMENTATION)
#endif

#ifdef ZEST_VULKAN_IMPLEMENTATION

	// --Frame_graph_platform_functions
	ZEST_PRIVATE zest_bool zest__vk_begin_command_buffer(const zest_frame_graph_context context);
	ZEST_PRIVATE zest_bool zest__vk_set_next_command_buffer(zest_frame_graph_context context, zest_queue queue);
	ZEST_PRIVATE void zest__vk_acquire_barrier(zest_frame_graph_context context, zest_execution_details_t *exe_details);
	ZEST_PRIVATE void zest__vk_release_barrier(zest_frame_graph_context context, zest_execution_details_t *exe_details);
	ZEST_PRIVATE void* zest__vk_new_execution_backend(zloc_linear_allocator_t *allocator);
	ZEST_PRIVATE void zest__vk_set_execution_fence(zest_execution_backend backend, zest_bool is_intraframe);
	ZEST_PRIVATE zest_frame_graph_semaphores zest__vk_get_frame_graph_semaphores(const char *name);
	ZEST_PRIVATE zest_bool zest__vk_submit_frame_graph_batch(zest_frame_graph frame_graph, zest_execution_backend backend, zest_submission_batch_t *batch, zest_map_queue_value *queues);
    ZEST_PRIVATE zest_frame_graph_result zest__vk_create_fg_render_pass(zest_pass_group_t *pass, zest_execution_details_t *exe_details, zest_uint current_pass_index);
    ZEST_PRIVATE zest_bool zest__vk_begin_render_pass(const zest_frame_graph_context context, zest_execution_details_t *exe_details);
    ZEST_PRIVATE void zest__vk_end_render_pass(const zest_frame_graph_context context);
    ZEST_PRIVATE void zest__vk_end_command_buffer(const zest_frame_graph_context context);
    ZEST_PRIVATE void zest__vk_carry_over_semaphores(zest_frame_graph frame_graph, zest_wave_submission_t *wave_submission, zest_execution_backend backend);
    ZEST_PRIVATE zest_bool zest__vk_frame_graph_fence_wait(zest_execution_backend backend);
    ZEST_PRIVATE void zest__vk_deferr_framebuffer_destruction(void *framebuffer);
    ZEST_PRIVATE void zest__vk_cleanup_deferred_framebuffers(void);
	ZEST_PRIVATE zest_bool zest__vk_present_frame(zest_swapchain swapchain);
	ZEST_PRIVATE zest_bool zest__vk_dummy_submit_for_present_only(void);
	ZEST_PRIVATE zest_bool zest__vk_acquire_swapchain_image(zest_swapchain swapchain);
	// --End_Frame_graph_platform_functions

	// Helper functions to convert enums to strings 
	ZEST_PRIVATE const char *zest__vk_image_layout_to_string(VkImageLayout layout);
	ZEST_PRIVATE zest_text_t zest__vk_access_flags_to_string(VkAccessFlags flags);
	ZEST_PRIVATE zest_text_t zest__vk_pipeline_stage_flags_to_string(VkPipelineStageFlags flags);

    //Set layouts
    ZEST_PRIVATE zest_bool zest__vk_create_set_layout(zest_set_layout_builder_t *builder, zest_set_layout layout, zest_bool is_bindless);
    ZEST_PRIVATE zest_bool zest__vk_create_set_pool(zest_set_layout layout, zest_uint max_set_count, zest_bool bindless);
    ZEST_PRIVATE zest_descriptor_set zest__vk_create_bindless_set(zest_set_layout layout);

	//Glue
	ZEST_PRIVATE void *zest__vk_new_device_backend(void);
	ZEST_PRIVATE void *zest__vk_new_renderer_backend(void);
	ZEST_PRIVATE void *zest__vk_new_frame_graph_context_backend(void);
	ZEST_PRIVATE void *zest__vk_new_swapchain_backend(void);
	ZEST_PRIVATE void *zest__vk_new_buffer_backend(void);
	ZEST_PRIVATE void *zest__vk_new_uniform_buffer_backend(void);
	ZEST_PRIVATE void zest__vk_set_uniform_buffer_backend(zest_uniform_buffer buffer);
	ZEST_PRIVATE void *zest__vk_new_compute_backend(void);
	ZEST_PRIVATE void *zest__vk_new_image_backend(void);
	ZEST_PRIVATE void *zest__vk_new_frame_graph_image_backend(zloc_linear_allocator_t *allocator);
	ZEST_PRIVATE void *zest__vk_new_set_layout_backend(void);
	ZEST_PRIVATE void *zest__vk_new_descriptor_pool_backend(void);
	ZEST_PRIVATE void *zest__vk_new_sampler_backend(void);
	ZEST_PRIVATE void *zest__vk_new_queue_backend(void);
	ZEST_PRIVATE void *zest__vk_new_submission_batch_backend(void);
	ZEST_PRIVATE void *zest__vk_new_frame_graph_semaphores_backend(void);
	ZEST_PRIVATE void *zest__vk_new_deferred_destruction_backend(void);

	ZEST_PRIVATE zest_bool zest__vk_finish_compute(zest_compute_builder_t *builder, zest_compute compute);
	ZEST_PRIVATE void zest__vk_cleanup_swapchain_backend(zest_swapchain swapchain, zest_bool for_recreation);
	ZEST_PRIVATE void zest__vk_cleanup_window_backend(zest_window window);
	ZEST_PRIVATE void zest__vk_cleanup_buffer(zest_buffer buffer);
	ZEST_PRIVATE void zest__vk_cleanup_uniform_buffer_backend(zest_uniform_buffer buffer);
	ZEST_PRIVATE void zest__vk_cleanup_compute_backend(zest_compute compute);
	ZEST_PRIVATE void zest__vk_cleanup_set_layout(zest_set_layout layout);
	ZEST_PRIVATE void zest__vk_cleanup_pipeline_backend(zest_pipeline pipeline);
	ZEST_PRIVATE void zest__vk_cleanup_image_backend(zest_image image);
	ZEST_PRIVATE void zest__vk_cleanup_sampler_backend(zest_sampler sampler);
	ZEST_PRIVATE void zest__vk_cleanup_queue_backend(zest_queue queue);
	ZEST_PRIVATE void zest__vk_cleanup_image_view_backend(zest_image_view view);
	ZEST_PRIVATE void zest__vk_cleanup_image_view_array_backend(zest_image_view_array view);
	ZEST_PRIVATE void zest__vk_cleanup_descriptor_backend(zest_set_layout layout, zest_descriptor_set set);
	ZEST_PRIVATE void zest__vk_cleanup_shader_resources_backend(zest_shader_resources shader_resource);
	ZEST_PRIVATE void zest__vk_cleanup_deferred_destruction_backend(void);
    ZEST_PRIVATE zest_bool zest__vk_create_window_surface(zest_window window);
    ZEST_PRIVATE zest_bool zest__vk_initialise_device();
    ZEST_PRIVATE zest_bool zest__vk_initialise_swapchain(zest_swapchain swapchain, zest_window window);
	ZEST_PRIVATE zest_bool zest__vk_create_instance();
	ZEST_PRIVATE zest_bool zest__vk_create_logical_device();
	ZEST_PRIVATE void zest__vk_set_limit_data(void);
	ZEST_PRIVATE void zest__vk_setup_validation();
    ZEST_PRIVATE void zest__vk_pick_physical_device(void);
    ZEST_PRIVATE zest_bool zest__vk_create_image(zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags);
    ZEST_PRIVATE zest_image_view_t *zest__vk_create_image_view(zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *allocator);
    ZEST_PRIVATE zest_image_view_array_t *zest__vk_create_image_views_per_mip(zest_image image, zest_image_view_type view_type, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *allocator);
    ZEST_PRIVATE zest_bool zest__vk_copy_buffer_regions_to_image(zest_buffer_image_copy_t *regions, zest_buffer buffer, zest_size src_offset, zest_image image);
    ZEST_PRIVATE zest_bool zest__vk_transition_image_layout(zest_image image, zest_image_layout new_layout, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count);
    ZEST_PRIVATE zest_bool zest__vk_create_sampler(zest_sampler sampler);
    ZEST_PRIVATE int zest__vk_get_image_raw_layout(zest_image image);
    ZEST_PRIVATE zest_bool zest__vk_copy_buffer_to_image(zest_buffer buffer, zest_size src_offset, zest_image image, zest_uint width, zest_uint height);
    ZEST_PRIVATE zest_bool zest__vk_generate_mipmaps(zest_image image);
    ZEST_PRIVATE zest_bool zest__vk_begin_single_time_commands();
    ZEST_PRIVATE zest_bool zest__vk_end_single_time_commands();
    ZEST_PRIVATE zest_bool zest__vk_create_execution_timeline_backend(zest_execution_timeline timeline);
	ZEST_PRIVATE void zest__vk_add_image_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, 
        zest_access_flags src_access, zest_access_flags dst_access, zest_image_layout old_layout, zest_image_layout new_layout, 
        zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage);
	ZEST_PRIVATE void zest__vk_add_memory_buffer_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, zest_access_flags src_access, zest_access_flags dst_access, 
		zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage);
    ZEST_PRIVATE void zest__vk_validate_barrier_pipeline_stages(zest_execution_barriers_t *barriers);
    ZEST_PRIVATE void* zest__vk_new_execution_barriers_backend(zloc_linear_allocator_t *allocator);
    ZEST_PRIVATE void zest__vk_cleanup_frame_graph_semaphore(zest_frame_graph_semaphores semaphores);
    ZEST_PRIVATE void zest__vk_cleanup_render_pass(zest_render_pass render_pass);
	ZEST_PRIVATE void zest__vk_print_compiled_frame_graph(zest_frame_graph frame_graph);

    //Create new things
	void *zest__new_device_backend(void) {
		return zest__vk_new_device_backend();
	}
	void *zest__new_renderer_backend(void) {
		return zest__vk_new_renderer_backend();
	}
	void *zest__new_frame_graph_context_backend(void) {
		return zest__vk_new_frame_graph_context_backend();
	}
	void *zest__new_swapchain_backend(void) {
		return zest__vk_new_swapchain_backend();
	}
	void *zest__new_buffer_backend(void) {
		return zest__vk_new_buffer_backend();
	}
	void *zest__new_uniform_buffer_backend(void) {
		return zest__vk_new_uniform_buffer_backend();
	}
	void zest__set_uniform_buffer_backend(zest_uniform_buffer buffer) {
		zest__vk_set_uniform_buffer_backend(buffer);
	}
	void *zest__new_image_backend(void) {
		return zest__vk_new_image_backend();
	}
    void *zest__new_frame_graph_image_backend(zloc_linear_allocator_t *allocator) {
		return zest__vk_new_frame_graph_image_backend(allocator);
	}
    void *zest__new_compute_backend(void) {
        return zest__vk_new_compute_backend();
    }
	void *zest__new_set_layout_backend(void) {
		return zest__vk_new_set_layout_backend();
	}
    void *zest__new_descriptor_pool_backend(void) {
		return zest__vk_new_descriptor_pool_backend();
    }
    void *zest__new_sampler_backend(void) {
        return zest__vk_new_sampler_backend();
    }
    void *zest__new_queue_backend(void) {
        return zest__vk_new_queue_backend();
    }
    void *zest__new_submission_batch_backend(void) {
        return zest__vk_new_submission_batch_backend();
    }
	zest_bool zest__finish_compute(zest_compute_builder_t *builder, zest_compute compute) {
		return zest__vk_finish_compute(builder, compute);
	}

    //Cleanup things
    void zest__cleanup_swapchain_backend(zest_swapchain swapchain, zest_bool for_recreation) {
        zest__vk_cleanup_swapchain_backend(swapchain, for_recreation);
    }
    void zest__cleanup_window_backend(zest_window window) {
        zest__vk_cleanup_window_backend(window);
    }
    void zest__cleanup_buffer_backend(zest_buffer buffer) {
        zest__vk_cleanup_buffer(buffer);
    }
    void zest__cleanup_uniform_buffer_backend(zest_uniform_buffer buffer) {
        zest__vk_cleanup_uniform_buffer_backend(buffer);
    }
    void zest__cleanup_compute_backend(zest_compute compute) {
        zest__vk_cleanup_compute_backend(compute);
    }
    void zest__cleanup_pipeline_backend(zest_pipeline pipeline) {
        zest__vk_cleanup_pipeline_backend(pipeline);
    }
    void zest__cleanup_sampler_backend(zest_sampler image) {
        zest__vk_cleanup_sampler_backend(image);
    }
    void zest__cleanup_queue_backend(zest_queue queue) {
        zest__vk_cleanup_queue_backend(queue);
    }
    void zest__cleanup_image_view_array_backend(zest_image_view_array view_array) {
        zest__vk_cleanup_image_view_array_backend(view_array);
    }
    void zest__cleanup_descriptor_backend(zest_set_layout layout, zest_descriptor_set set) {
        zest__vk_cleanup_descriptor_backend(layout, set);
    }
    void zest__cleanup_shader_resources_backend(zest_shader_resources shader_resources) {
        zest__vk_cleanup_shader_resources_backend(shader_resources);
    }
    void zest__cleanup_set_layout_backend(zest_set_layout layout) {
        zest__vk_cleanup_set_layout(layout);
    }

    //Initialisation
    zest_bool zest__os_create_window_surface(zest_window window) {
        return zest__vk_create_window_surface(window);
    }
    zest_bool zest__initialise_device() {
        return zest__vk_initialise_device();
    }
    zest_bool zest__initialise_swapchain(zest_swapchain swapchain, zest_window window) {
        return zest__vk_initialise_swapchain(swapchain, window);
    }

    //Images
    zest_bool zest__create_image(zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags) {
        return zest__vk_create_image(image, layer_count, num_samples, flags);
    }
    zest_image_view_t *zest__create_image_view_backend(zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator) {
        return zest__vk_create_image_view(image, view_type, mip_levels_this_view, base_mip, base_array_index, layer_count, linear_allocator);
    }
    zest_image_view_array zest__create_image_views_per_mip_backend(zest_image image, zest_image_view_type view_type, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator) {
        return zest__vk_create_image_views_per_mip(image, view_type, base_array_index, layer_count, linear_allocator);
    }
    zest_bool zest__transition_image_layout(zest_image image, zest_image_layout new_layout, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count) {
        mip_levels = ZEST__MIN(mip_levels, image->info.mip_levels);
        layer_count = ZEST__MIN(layer_count, image->info.layer_count);
        if (zest__vk_transition_image_layout(image, new_layout, base_mip_index, mip_levels, base_array_index, layer_count)) {
            image->info.layout = new_layout;
            return ZEST_TRUE;
        }
        return ZEST_FALSE;
    }
    zest_bool zest__copy_buffer_regions_to_image(zest_buffer_image_copy_t *regions, zest_buffer buffer, zest_size src_offset, zest_image image) {
        return zest__vk_copy_buffer_regions_to_image(regions, buffer, src_offset, image);
    }
    zest_bool zest__create_sampler(zest_sampler sampler) {
        return zest__vk_create_sampler(sampler);
    }
    int zest__get_image_raw_layout(zest_image image) {
        return zest__vk_get_image_raw_layout(image);
    }
    zest_bool zest__copy_buffer_to_image(zest_buffer buffer, zest_size src_offset, zest_image image, zest_uint width, zest_uint height) {
        zest__vk_copy_buffer_to_image(buffer, src_offset, image, width, height);
    }
    zest_bool zest__generate_mipmaps(zest_image image) {
        zest__vk_generate_mipmaps(image);
    }

    //General helpers
    zest_bool zest__begin_single_time_commands() {
        return zest__vk_begin_single_time_commands();
    }
    zest_bool zest__end_single_time_commands() {
        return zest__vk_end_single_time_commands();
    }
    #include "zest_vulkan.h"
#endif
#ifdef ZEST_D3D12_IMPLEMENTATION
#endif
#ifdef ZEST_METAL_IMPLEMENTATION
#endif
