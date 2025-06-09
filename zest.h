//Zest - A Vulkan Pocket Renderer
#ifndef ZEST_RENDERER_H
#define ZEST_RENDERER_H

/*
    Zest Pocket Renderer

    Sections in this header file, you can search for the following keywords to jump to that section :

    [Macro_Defines]                     Just a bunch of typedefs and macro definitions
    [Platform_specific_code]            Windows/Mac specific, no linux yet
    [Shader_code]                       glsl shader code for all the built in shaders
    [Enums_and_flags]                   Enums and bit flag definitions
    [Forward_declarations]              Forward declarations for structs and setting up of handles
    [Pocket_dynamic_array]              Simple dynamic array
    [Pocket_Hasher]                     XXHash code for use in hash map
    [Pocket_hash_map]                   Simple hash map
    [Pocket_text_buffer]                Very simple struct and functions for storing strings
    [Threading]                         Simple worker queue mainly used for recording secondary command buffers
    [Structs]                           All the structs are defined here.
        [Vectors]
        [Render_graph_types]
    [Internal_functions]                Private functions only, no API access
		[Platform_dependent_functions]
        [Buffer_and_Memory_Management]
        [Renderer_functions]
        [Command_Queue_functions]
        [Command_Queue_Setup_functions]
        [Draw_layer_internal_functions]
        [Texture_internal_functions]
        [Render_target_internal_functions]
        [General_layer_internal_functions]
        [Font_layer_internal_functions]
        [Mesh_layer_internal_functions]
        [General_Helper_Functions]
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
    [Pipeline_related_vulkan_helpers]   Vulkan helper functions for setting up your own pipelines
    [Platform_dependent_callbacks]      These are used depending one whether you're using glfw, sdl or just the os directly
    [Buffer_functions]                  Functions for creating and using gpu buffers
    [Command_queue_setup_and_creation]  All helper functions for setting up your own custom command queues in vulkan
    [General_Math_helper_functions]     Vector and matrix math functions
    [Camera_helpers]                    Functions for setting up and using a camera including frustom and screen ray etc. 
    [Images_and_textures]               Load and setup images for using in textures accessed on the GPU
    [Fonts]                             Basic functions for loading MSDF fonts
    [Render_targets]                    Helper functions for setting up render targets for use in command queus
    [Main_loop_update_functions]        Only one function currently, for setting the current command queue to render with
    [Draw_Routines]                     Create a draw routine for custom drawing
    [Draw_Layers_API]                   Draw layers are used to draw stuff as you'd imagine, there's a bunch of functions here to
    [Draw_sprite_layers]                Functions for drawing the builtin sprite layer pipeline
    [Draw_billboard_layers]             Functions for drawing the builtin billboard layer pipeline
    [Draw_3D_Line_layers]               Functions for drawing the builtin 3d line layer pipeline
    [Draw_MSDF_font_layers]             Functions for drawing the builtin MSDF font layer pipeline
    [Draw_mesh_layers]                  Functions for drawing the builtin mesh layer pipeline
    [Draw_instance_mesh_layers]         Functions for drawing the builtin instance mesh layer pipeline
    [Compute_shaders]                   Functions for setting up your own custom compute shaders
    [Events_and_States]                 Just one function for now
    [Timer_functions]                   High resolution timer functions
    [General_Helper_functions]          General API functions - get screen dimensions some vulkan helper functions
    [Debug_Helpers]                     Functions for debugging and outputting queues to the console.
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

#ifndef ZEST_MAX_DEVICE_MEMORY_POOLS
#define ZEST_MAX_DEVICE_MEMORY_POOLS 64
#endif

#define ZEST_NULL 0
//For each frame in flight macro
#define ZEST_EACH_FIF_i unsigned int i = 0; i != ZEST_MAX_FIF; ++i
#define zest_ForEachFrameInFlight(index) for(unsigned int index = 0; index != ZEST_MAX_FIF; ++index)

//For error checking vulkan commands
#define ZEST_VK_CHECK_RESULT(res)                                                                                   \
    {                                                                                                               \
        if (res != VK_SUCCESS)                                                                                      \
        {                                                                                                           \
            printf("Fatal : VkResult is \" %s \" in %s at line %i\n", zest__vulkan_error(res), __FILE__, __LINE__); \
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Fatal : VkResult is \" %s \" in %s at line %i", zest__vulkan_error(res), __FILE__, __LINE__);     \
            ZEST_ASSERT(res == VK_SUCCESS);                                                                         \
        }                                                                                                           \
    }

const char *zest__vulkan_error(VkResult errorCode);

typedef unsigned int zest_uint;
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
typedef VkVertexInputAttributeDescription *zest_vertex_input_descriptions;

//Handles. These are pointers that remain stable until the object is freed.
#define ZEST__MAKE_HANDLE(handle) typedef struct handle##_t* handle;

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
#define ZEST_U32_MAX_VALUE ((zest_uint)-1)

//Shader_code
//For nicer formatting of the shader code, but note that new lines are ignored when this becomes an actual string.
#define ZEST_GLSL(version, shader) "#version " #version "\n" "#extension GL_EXT_nonuniform_qualifier : require\n" #shader
//----------------------
//Imgui vert shader
//----------------------
static const char *zest_shader_imgui_vert = ZEST_GLSL(450 core,

layout(push_constant) uniform quad_index
{
    uint index1;
    uint index2;
    uint index3;
    uint index4;
	vec4 transform;
	vec4 parameters;
	vec4 parameters3;
	vec4 camera;
} pc;

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_tex_coord;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec3 out_uv;

void main() {

	gl_Position = vec4(in_position * pc.transform.xy + pc.transform.zw, 0.0, 1.0);

	out_uv = vec3(in_tex_coord, pc.parameters.x);
	out_color = in_color;
}

);

//----------------------
//Imgui fragment shader
//----------------------
static const char *zest_shader_imgui_frag = ZEST_GLSL(450 core,

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec3 in_uv;

layout(location = 0) out vec4 out_color;
layout(set = 0, binding = 0) uniform sampler2DArray tex_sampler;

//Not used by default by can be used in custom imgui image shaders
layout(push_constant) uniform quad_index
{
    uint index1;
    uint index2;
    uint index3;
    uint index4;
	vec4 transform;
	vec4 parameters;
	vec4 parameters3;
	vec4 camera;
} pc;

void main()
{
	out_color = in_color * texture(tex_sampler, in_uv);
}

);

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
//2d/3d billboard fragment shader single alpha channel only
//----------------------
static const char *zest_shader_sprite_alpha_frag = ZEST_GLSL(450,
layout(location = 0) in vec4 in_frag_color;
layout(location = 1) in vec3 in_tex_coord;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2DArray texSampler;

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
    float texel = texture(texSampler, in_tex_coord).r;

    outColor.rgb = in_frag_color.rgb * texel;
    outColor.a = texel * in_frag_color.a;
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
//2d shape vertex shader
//----------------------
static const char *zest_shader_shape_vert = ZEST_GLSL(450,
//Quad indexes
const int indexes[6] = int[6]( 0, 1, 2, 2, 1, 3 );
const vec2 vertex[4] = vec2[4]( 
    vec2(-.5, -.5), vec2(-.5, .5), vec2(.5, -.5), vec2(.5, .5)
);

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

//Line instance data
layout(location = 0) in vec4 rect;
layout(location = 1) in vec4 parameters;
layout(location = 2) in vec4 start_color;

layout(location = 0) out vec4 out_frag_color;
layout(location = 1) out vec4 p1;
layout(location = 2) out vec4 p2;
layout(location = 3) out float shape_type;
layout(location = 4) out float millisecs;

void main() {
    int index = indexes[gl_VertexIndex];
    shape_type = pc.parameters1.x;

    vec2 vertex_position;
    if (shape_type == 4) {
        //line drawing
        vec2 line = rect.zw - rect.xy;
        vec2 normal = normalize(vec2(-line.y, line.x));
        vertex_position = rect.xy + line * (vertex[index].x + .5) + normal * parameters.x * vertex[index].y;
    }
    else if (shape_type == 5) {
        //Rect drawing
        vec2 size = rect.zw - rect.xy;
        vec2 position = size * .5 + rect.xy;
        vertex_position = size.xy * vertex[index] + position;
    }
    else if (shape_type == 6) {
        //line drawing
        vec2 line = rect.zw - rect.xy;
        vec2 normal = normalize(vec2(-line.y, line.x));
        vertex_position = rect.xy + line * (vertex[index].x + .5) + normal * parameters.x * vertex[index].y;
    }

    mat4 modelView = uboView.view;
    vec3 pos = vec3(vertex_position.x, vertex_position.y, 1);
    gl_Position = uboView.proj * modelView * vec4(pos, 1.0);

    //----------------
    out_frag_color = start_color;
    p1 = rect;
    p2 = parameters;
    millisecs = float(uboView.millisecs);
}
);

//----------------------
//2d shape frag shader
//----------------------
static const char *zest_shader_shape_frag = ZEST_GLSL(450,
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec4 p1;
layout(location = 2) in vec4 p2;
layout(location = 3) in float shape_type;
layout(location = 4) in float millisecs;

layout(location = 0) out vec4 outColor;

float Line(in vec2 p, in vec2 a, in vec2 b) {
    vec2 ba = b - a;
    vec2 pa = p - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0., 1.);
    return length(pa - h * ba);
}

float Fill(float sdf) {
    //return step(0, -sdf);
    return clamp(0.5 - sdf / fwidth(sdf), 0, 1);		//Anti Aliased
}

float Circle(vec2 p, float r)
{
    return length(p) - r;
}

float Stroke(float sdf, float width) {
    return Fill(abs(sdf) - width);
}

float Difference(float sdf1, float sdf2) {
    return max(sdf1, -sdf2);
}

float Union(float sdf1, float sdf2) {
    return min(sdf1, sdf2);
}

float Intersection(float sdf1, float sdf2) {
    return max(sdf1, sdf2);
}

float Trapezoid(in vec2 p, in vec2 a, in vec2 b, in float ra, float rb)
{
    float rba = rb - ra;
    float baba = dot(b - a, b - a);
    float papa = dot(p - a, p - a);
    float paba = dot(p - a, b - a) / baba;
    float x = sqrt(papa - paba * paba * baba);
    float cax = max(0.0, x - ((paba < 0.5) ? ra : rb));
    float cay = abs(paba - 0.5) - 0.5;
    float k = rba * rba + baba;
    float f = clamp((rba * (x - ra) + paba * baba) / k, 0.0, 1.0);
    float cbx = x - ra - f * rba;
    float cby = paba - f;
    float s = (cbx < 0.0 && cay < 0.0) ? -1.0 : 1.0;
    return s * sqrt(min(cax * cax + cay * cay * baba, cbx * cbx + cby * cby * baba));
}

float cro(in vec2 a, in vec2 b) { return a.x * b.y - a.y * b.x; }

float UnevenCapsule(in vec2 p, in vec2 pa, in vec2 pb, in float ra, in float rb)
{
    p -= pa;
    pb -= pa;
    float h = dot(pb, pb);
    vec2  q = vec2(dot(p, vec2(pb.y, -pb.x)), dot(p, pb)) / h;

    //-----------

    q.x = abs(q.x);

    float b = ra - rb;
    vec2  c = vec2(sqrt(h - b * b), b);

    float k = cro(c, q);
    float m = dot(c, q);
    float n = dot(q, q);

    if (k < 0.0) return sqrt(h * (n)) - ra;
    else if (k > c.x) return sqrt(h * (n + 1.0 - 2.0 * q.y)) - rb;
    return m - ra;
}

float OrientedBox(in vec2 p, in vec2 a, in vec2 b, float th)
{
    float l = length(b - a);
    vec2  d = (b - a) / l;
    vec2  q = (p - (a + b) * 0.5);
    q = mat2(d.x, -d.y, d.y, d.x) * q;
    q = abs(q) - vec2(l, th) * 0.5;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
}

void main() {

    float brightness = 0;
    if (shape_type == 4) {
        //Line sdf seems to give perfectly fine results, UnevenCapsule would be more accurate though if widths change drastically over the course of the ribbon.
        //float line_sdf = Line(gl_FragCoord.xy, p1.xy, p1.zw) - p2.x * .5; 
        //float line_sdf = OrientedBox(gl_FragCoord.xy, p1.xy, p1.zw, p2.x); 
        //float trap_sdf = UnevenCapsule(gl_FragCoord.xy, p1.xy, p1.zw, p2.x * .95, p2.x * .95); 
        //brightness = Fill(line_sdf);
        brightness = 1;
    }
    else if (shape_type == 5) {
        brightness = 1;
    }
    else if (shape_type == 6) {
        vec2 line = vec2(p1.xy - gl_FragCoord.xy);
        brightness = step(5, mod(length(line) + (millisecs * 0.05), 10));
    }

    outColor = fragColor * brightness;
    outColor.rgb *= fragColor.a;
}
);

//----------------------
//3d lines vert shader
//----------------------
static const char *zest_shader_3d_lines_vert = ZEST_GLSL(450,
//Quad indexes
const int indexes[] = int[](0, 1, 2, 0, 2, 3);
const vec3 vertices[4] = vec3[]( 
    vec3(0, -.5, 0),
	vec3(0, -.5, 1),
	vec3(0,  .5, 1),
	vec3(0,  .5, 0)
);

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
    uint font_texture_index;
    uint index2;
    uint index3;
    uint index4;
    vec4 parameters1;
    vec4 parameters2;
    vec4 parameters3;
    vec4 camera;
} pc;

//Instance
layout(location = 0) in vec4 start;
layout(location = 1) in vec4 end;
layout(location = 2) in vec4 line_color;

layout(location = 0) out vec4 out_frag_color;
layout(location = 1) out vec4 p1;
layout(location = 2) out vec4 p2;
layout(location = 3) out vec3 out_end;
layout(location = 4) out float millisecs;
layout(location = 5) out float res;

void main() {
    vec4 clip0 = uboView.proj * uboView.view * vec4(start.xyz, 1.0);
    vec4 clip1 = uboView.proj * uboView.view * vec4(end.xyz, 1.0);

    vec2 screen0 = uboView.res * (0.5 * clip0.xy / clip0.w + 0.5);
    vec2 screen1 = uboView.res * (0.5 * clip1.xy / clip1.w + 0.5);

    int index = indexes[gl_VertexIndex];
    vec3 vertex = vertices[index];

    vec2 line = screen1 - screen0;
    vec2 normal = normalize(vec2(-line.y, line.x));
    //Note: lines made a bit thicker then the asked-for width so that there's room for anti-aliasing. Need to be more precise with this!
    vec2 pt0 = screen0 + (start.w * 1.25) * (vertex.x * line + vertex.y * normal);
    vec2 pt1 = screen1 + (end.w * 1.25) * (vertex.x * line + vertex.y * normal);
    vec2 vertex_position = mix(pt0, pt1, vertex.z);
    vec4 clip = mix(clip0, clip1, vertex.z);
    gl_Position = vec4(clip.w * ((2.0 * vertex_position) / uboView.res - 1.0), clip.z, clip.w);

    //----------------
    out_frag_color = line_color;
    p1 = vec4(screen0, 0, start.w);
    p2 = vec4(screen1, 0, end.w);
    out_end = p2.xyz;
    millisecs = uboView.millisecs;
    res = 1.0 / uboView.res.y;
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
    uint font_texture_index;
    uint index2;
    uint index3;
    uint index4;
    vec4 parameters;
    vec4 shadow_parameters;
    vec4 shadow_color;
    vec4 camera;
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
    float scale = get_uv_scale(frag_tex_coord.xy * texture_size) * font.parameters.z;
    float d = (median(sampled.r, sampled.g, sampled.b) - 0.75) * font.parameters.x;
    float sdf = (d + font.parameters.w) / scale + 0.5 + font.parameters.y;
    float mask = clamp(sdf, 0.0, 1.0);
    glyph = vec4(glyph.rgb, glyph.a * mask);

    if (font.shadow_color.a > 0) {
        float sd = texture(texture_sampler[font.font_texture_index], vec3(frag_tex_coord.xy - font.shadow_parameters.xy / texture_size.xy, 0)).a;
        float shadowAlpha = linearstep(0.5 - font.shadow_parameters.z, 0.5 + font.shadow_parameters.z, sd) * font.shadow_color.a;
        shadowAlpha *= 1.0 - mask * font.shadow_parameters.w;
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
//3d lines frag shader
//----------------------
static const char *zest_shader_3d_lines_frag = ZEST_GLSL(450,
layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec4 p1;
layout(location = 2) in vec4 p2;
layout(location = 3) in vec3 end;
layout(location = 4) in float millisecs;
layout(location = 5) in float res;

layout(location = 0) out vec4 out_color;

float Line(in vec2 p, in vec2 a, in vec2 b) {
    vec2 ba = b - a;
    vec2 pa = p - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0., 1.);
    return length(pa - h * ba);
}

float Fill(float sdf) {
    //return step(0, -sdf);
    return clamp(0.5 - sdf / fwidth(sdf), 0, 1);		//Anti Aliased
    //return clamp( 0.5 - sdf, 0, 1 );
}

void main() {
    vec3 line = vec3(p1.xyz - gl_FragCoord.xyz);
    float animated_brightness = step(5, mod(length(line) + (millisecs * 0.05), 10));

    //Line sdf seems to give perfectly fine results, UnevenCapsule would be more accurate though if widths change drastically over the course of the ribbon.
    float radius = p2.w * .5;
    float line_sdf = Line(gl_FragCoord.xy, p1.xy, p2.xy) - radius;

    float brightness = Fill(line_sdf);

    out_color = frag_color * brightness;
    out_color.rgb *= frag_color.a;
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
    mat4 model;
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
typedef enum zest_frustum_side { zest_LEFT = 0, zest_RIGHT = 1, zest_TOP = 2, zest_BOTTOM = 3, zest_BACK = 4, zest_FRONT = 5 } zest_frustum_size;

typedef enum zest_struct_type {
    zest_struct_type_texture = VK_STRUCTURE_TYPE_MAX_ENUM - 1,
    zest_struct_type_image = VK_STRUCTURE_TYPE_MAX_ENUM - 2,
    zest_struct_type_imgui_image = VK_STRUCTURE_TYPE_MAX_ENUM - 3,
} zest_struct_type;

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
    zest_buffer_flag_none =      0,
    zest_buffer_flag_read_only = 1 << 0,
    zest_buffer_flag_is_fif =    1 << 1
} zest_buffer_flags_e;

typedef zest_uint zest_buffer_flags;

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

typedef enum {
    zest_setup_context_type_none,
    zest_setup_context_type_command_queue,
    zest_setup_context_type_render_pass,
    zest_setup_context_type_composite_render_pass,
    zest_setup_context_type_draw_routine,
    zest_setup_context_type_layer,
    zest_setup_context_type_compute
} zest_setup_context_type;

typedef enum zest_command_queue_flag_bits {
    zest_command_queue_flag_none                                 = 0,
    zest_command_queue_flag_present_dependency                   = 1 << 0,
    zest_command_queue_flag_command_queue_dependency             = 1 << 1,
    zest_command_queue_flag_validated                            = 1 << 2,
} zest_command_queue_flag_bits;

typedef zest_uint zest_command_queue_flags;

typedef enum zest_renderer_flag_bits {
    zest_renderer_flag_enable_multisampling                      = 1 << 0,
    zest_renderer_flag_schedule_recreate_textures                = 1 << 1,
    zest_renderer_flag_schedule_change_vsync                     = 1 << 2,
    zest_renderer_flag_schedule_rerecord_final_render_buffer     = 1 << 3,
    zest_renderer_flag_drawing_loop_running                      = 1 << 4,
    zest_renderer_flag_msaa_toggled                              = 1 << 5,
    zest_renderer_flag_vsync_enabled                             = 1 << 6,
    zest_renderer_flag_disable_default_uniform_update            = 1 << 7,
    zest_renderer_flag_swapchain_was_recreated                   = 1 << 8,
    zest_renderer_flag_has_depth_buffer                          = 1 << 9,
    zest_renderer_flag_swap_chain_was_acquired                   = 1 << 10,
    zest_renderer_flag_swap_chain_was_used                       = 1 << 11,
    zest_renderer_flag_work_was_submitted                        = 1 << 12,
    zest_renderer_flag_building_render_graph                     = 1 << 13,
} zest_renderer_flag_bits;

typedef zest_uint zest_renderer_flags;

typedef enum zest_pipeline_set_flag_bits {
    zest_pipeline_set_flag_none                                   = 0,
    zest_pipeline_set_flag_is_render_target_pipeline              = 1 << 0,        //True if this pipeline is used for the final render of a render target to the swap chain
    zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild = 1 << 1        //True if the pipeline should update it's view extent when the swap chain is recreated (the window is resized)
} zest_pipeline_set_flag_bits;

typedef zest_uint zest_pipeline_set_flags;

typedef enum zest_init_flag_bits {
    zest_init_flag_none                                         = 0,
    zest_init_flag_use_depth_buffer                             = 1 << 0,
    zest_init_flag_maximised                                    = 1 << 1,
    zest_init_flag_cache_shaders                                = 1 << 2,
    zest_init_flag_enable_vsync                                 = 1 << 3,
    zest_init_flag_enable_fragment_stores_and_atomics           = 1 << 4,
    zest_init_flag_disable_shaderc                              = 1 << 5,
    zest_init_flag_enable_validation_layers                     = 1 << 6,
    zest_init_flag_enable_validation_layers_with_sync           = 1 << 7,
    zest_init_flag_log_validation_errors_to_console             = 1 << 8,
} zest_init_flag_bits;

typedef zest_uint zest_init_flags;

typedef enum zest_validation_flag_bits {
    zest_validation_flag_none                                         = 0,
    zest_validation_flag_enable_sync                                  = 1,
} zest_validation_flag_bits;

typedef zest_uint zest_validation_flags;

typedef enum zest_buffer_upload_flag_bits {
    zest_buffer_upload_flag_initialised                           = 1 << 0,                //Set to true once AddCopyCommand has been run at least once
    zest_buffer_upload_flag_source_is_fif                         = 1 << 1,
    zest_buffer_upload_flag_target_is_fif                         = 1 << 2
} zest_buffer_upload_flag_bits;

typedef zest_uint zest_buffer_upload_flags;

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

typedef enum zest_texture_flag_bits {
    zest_texture_flag_none                                = 0,
    zest_texture_flag_premultiply_mode                    = 1 << 0,
    zest_texture_flag_use_filtering                       = 1 << 1,
    zest_texture_flag_get_max_radius                      = 1 << 2,
    zest_texture_flag_ready                               = 1 << 3,
    zest_texture_flag_dirty                               = 1 << 4,
    zest_texture_flag_descriptor_sets_created             = 1 << 5,
} zest_texture_flag_bits;

typedef zest_uint zest_texture_flags;

typedef enum zest_texture_storage_type {
    zest_texture_storage_type_packed,                        //Pack all of the images into a sprite sheet and onto multiple layers in an image array on the GPU
    zest_texture_storage_type_bank,                          //Packs all images one per layer, best used for repeating textures or color/bump/specular etc
    zest_texture_storage_type_sprite_sheet,                  //Packs all the images onto a single layer spritesheet
    zest_texture_storage_type_single,                        //A single image texture
    zest_texture_storage_type_storage,                       //A storage texture useful for manipulation and other things in a compute shader
    zest_texture_storage_type_stream,                        //A storage texture that you can update every frame
    zest_texture_storage_type_render_target                  //Texture storage for a render target sampler, so that you can draw the target onto another render target
} zest_texture_storage_type;

typedef enum zest_camera_flag_bits {
    zest_camera_flags_none = 0,
    zest_camera_flags_perspective                         = 1 << 0,
    zest_camera_flags_orthagonal                          = 1 << 1,
} zest_camera_flag_bits;

typedef zest_uint zest_camera_flags;

typedef enum zest_render_target_flag_bits {
    zest_render_target_flag_fixed_size                    = 1 << 0,    //True if the render target is not tied to the size of the window
    zest_render_target_flag_is_src                        = 1 << 1,    //True if the render target is the source of another render target - adds transfer src and dst bits
    zest_render_target_flag_use_msaa                      = 1 << 2,      //True if the render target should render with MSAA enabled
    zest_render_target_flag_sampler_size_match_texture    = 1 << 3,
	zest_render_target_flag_single_frame_buffer_only      = 1 << 4,
    zest_render_target_flag_use_depth_buffer              = 1 << 5,
    zest_render_target_flag_render_to_swap_chain          = 1 << 6,
    zest_render_target_flag_initialised                   = 1 << 7,
    zest_render_target_flag_has_imgui_pipeline            = 1 << 8,
    zest_render_target_flag_was_changed                   = 1 << 9,
    zest_render_target_flag_multi_mip                     = 1 << 10,
    zest_render_target_flag_upsampler                     = 1 << 11,
} zest_render_target_flag_bits;

typedef zest_uint zest_render_target_flags;

typedef enum zest_texture_format {
    zest_texture_format_alpha = VK_FORMAT_R8_UNORM,
    zest_texture_format_rgba_unorm = VK_FORMAT_R8G8B8A8_UNORM,
    zest_texture_format_bgra_unorm = VK_FORMAT_B8G8R8A8_UNORM,
    zest_texture_format_rgba_srgb = VK_FORMAT_R8G8B8A8_SRGB,
    zest_texture_format_bgra_srgb = VK_FORMAT_B8G8R8A8_SRGB,
} zest_texture_format;

typedef enum zest_connector_type {
    zest_connector_type_wait_present,
    zest_connector_type_signal_present,
    zest_connector_type_wait_queue,
    zest_connector_type_signal_queue,
} zest_connector_type;

typedef enum zest_character_flag_bits {
    zest_character_flag_none                              = 0,
    zest_character_flag_skip                              = 1 << 0,
    zest_character_flag_new_line                          = 1 << 1,
} zest_character_flag_bits;

typedef zest_uint zest_character_flags;

typedef enum zest_compute_flag_bits {
    zest_compute_flag_none                                = 0,
    zest_compute_flag_has_render_target                   = 1,        // Compute shader uses a render target
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
} zest_layer_flag_bits;

typedef enum zest_command_queue_type {
    zest_command_queue_type_primary                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY,            //For vulkan primary command buffers
    zest_command_queue_type_secondary                     = VK_COMMAND_BUFFER_LEVEL_SECONDARY           //For vulkan secondary command buffers
} zest_command_queue_type;

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
    zest_access_read,
    zest_access_write,
    zest_access_read_write
} zest_resource_access_type;

typedef enum {
    zest_queue_graphics,
    zest_queue_compute,
    zest_queue_transfer
} zest_device_queue_type;

typedef enum {
    zest_resource_type_image,
    zest_resource_type_buffer,
    zest_resource_type_swap_chain_image
} zest_resource_type;

typedef enum zest_resource_node_flag_bits {
    zest_resource_node_flag_none            = 0,
    zest_resource_node_flag_transient       = 1 << 0,
    zest_resource_node_flag_imported        = 1 << 1,
    zest_resource_node_flag_used_in_output  = 1 << 2,
    zest_resource_node_flag_is_bindless     = 1 << 3,
} zest_resource_node_flag_bits;

typedef zest_uint zest_resource_node_flags;

typedef enum zest_render_graph_flag_bits {
    zest_render_graph_flag_none                     = 0,
    zest_render_graph_expecting_swap_chain_usage    = 1 << 0,
    zest_render_graph_force_on_graphics_queue       = 1 << 1,
    zest_render_graph_is_compiled                   = 1 << 2,
} zest_render_graph_flag_bits;

typedef zest_uint zest_render_graph_flags;

typedef enum {
    zest_access_write_bits_general = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
    zest_access_read_bits_general = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
    zest_access_render_pass_bits = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
} zest_general_access_bits;

typedef enum zest_resource_purpose {
    // Buffer Usages
    zest_purpose_vertex_buffer,
    zest_purpose_index_buffer,
    zest_purpose_uniform_buffer,                      // Might need shader stage too if general
    zest_purpose_storage_buffer_read,                 // Needs shader stage
    zest_purpose_storage_buffer_write,                // Needs shader stage
    zest_purpose_storage_buffer_read_write,           // Needs shader stage
    zest_purpose_indirect_buffer,
    zest_purpose_transfer_src_buffer,
    zest_purpose_transfer_dst_buffer,

    // Image Usages
    zest_purpose_sampled_image,                       // Needs shader stage
    zest_purpose_storage_image_read,                  // Needs shader stage
    zest_purpose_storage_image_write,                 // Needs shader stage
    zest_purpose_storage_image_read_write,            // Needs shader stage
    zest_purpose_color_attachment_write,
    zest_purpose_color_attachment_read,               // For blending or input attachments
    zest_purpose_depth_stencil_attachment_read,
    zest_purpose_depth_stencil_attachment_write,
    zest_purpose_depth_stencil_attachment_read_write, // Common
    zest_purpose_input_attachment,                    // Needs shader stage (typically fragment)
    zest_purpose_transfer_src_image,
    zest_purpose_transfer_dst_image,
    zest_purpose_present_src,                         // For swapchain final layout
} zest_resource_purpose;

typedef enum zest_connection_type {
    zest_input,
    zest_output
} zest_connection_type;

typedef enum zest_supported_pipeline_stages {
    zest_pipeline_vertex_input_stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
    zest_pipeline_vertex_stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
    zest_pipeline_fragment_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    zest_pipeline_compute_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    zest_pipeline_transfer_stage = VK_PIPELINE_STAGE_TRANSFER_BIT
} zest_supported_pipeline_stages;

typedef enum zest_supported_shader_stages {
    zest_shader_vertex_stage = VK_SHADER_STAGE_VERTEX_BIT,
    zest_shader_fragment_stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    zest_shader_compute_stage = VK_SHADER_STAGE_COMPUTE_BIT,
    zest_shader_render_stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    zest_shader_all_stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
} zest_supported_shader_stages;

typedef zest_uint zest_compute_flags;		//zest_compute_flag_bits
typedef zest_uint zest_layer_flags;         //zest_layer_flag_bits

typedef void(*zloc__block_output)(void* ptr, size_t size, int used, void* user, int is_final_output);

static const int zest_INIT_MAGIC = 0x4E57;

#define ZEST_ASSERT_INIT(magic) ZEST_ASSERT(magic == zest_INIT_MAGIC)
#define ZEST_ASSERT_UNINIT(magic) ZEST_ASSERT(magic != zest_INIT_MAGIC)
#define ZEST_CHECK_HANDLE(handle) ZEST_ASSERT(handle && *((int*)handle) == zest_INIT_MAGIC)
#define ZEST_VALID_HANDLE(handle) (handle && *((int*)handle) == zest_INIT_MAGIC)

// --Forward_declarations
typedef struct zest_texture_t zest_texture_t;
typedef struct zest_image_t zest_image_t;
typedef struct zest_sampler_t zest_sampler_t;
typedef struct zest_draw_routine_t zest_draw_routine_t;
typedef struct zest_command_queue_draw_commands_t zest_command_queue_draw_commands_t;
typedef struct zest_command_queue_t zest_command_queue_t;
typedef struct zest_command_queue_compute_t zest_command_queue_compute_t;
typedef struct zest_font_t zest_font_t;
typedef struct zest_layer_t zest_layer_t;
typedef struct zest_pipeline_t zest_pipeline_t;
typedef struct zest_pipeline_template_t zest_pipeline_template_t;
typedef struct zest_set_layout_t zest_set_layout_t;
typedef struct zest_descriptor_set_t zest_descriptor_set_t;
typedef struct zest_descriptor_pool_t zest_descriptor_pool_t;
typedef struct zest_shader_resources_t zest_shader_resources_t;
typedef struct zest_descriptor_buffer_t zest_descriptor_buffer_t;
typedef struct zest_uniform_buffer_t zest_uniform_buffer_t;
typedef struct zest_frame_staging_buffer_t zest_frame_staging_buffer_t;
typedef struct zest_render_target_t zest_render_target_t;
typedef struct zest_buffer_allocator_t zest_buffer_allocator_t;
typedef struct zest_compute_t zest_compute_t;
typedef struct zest_buffer_t zest_buffer_t;
typedef struct zest_device_memory_pool_t zest_device_memory_pool_t;
typedef struct zest_timer_t zest_timer_t;
typedef struct zest_window_t zest_window_t;
typedef struct zest_shader_t zest_shader_t;
typedef struct zest_recorder_t zest_recorder_t;
typedef struct zest_debug_t zest_debug_t;
typedef struct zest_render_graph_t zest_render_graph_t;
typedef struct zest_pass_node_t zest_pass_node_t;
typedef struct zest_resource_node_t zest_resource_node_t;
typedef struct zest_imgui_t zest_imgui_t;
typedef struct zest_queue_t zest_queue_t;

//Generate handles for the struct types. These are all pointers to memory where the object is stored.
ZEST__MAKE_HANDLE(zest_render_graph)
ZEST__MAKE_HANDLE(zest_pass_node)
ZEST__MAKE_HANDLE(zest_resource_node)
ZEST__MAKE_HANDLE(zest_texture)
ZEST__MAKE_HANDLE(zest_image)
ZEST__MAKE_HANDLE(zest_sampler)
ZEST__MAKE_HANDLE(zest_draw_routine)
ZEST__MAKE_HANDLE(zest_command_queue_draw_commands)
ZEST__MAKE_HANDLE(zest_command_queue)
ZEST__MAKE_HANDLE(zest_command_queue_compute)
ZEST__MAKE_HANDLE(zest_font)
ZEST__MAKE_HANDLE(zest_layer)
ZEST__MAKE_HANDLE(zest_pipeline)
ZEST__MAKE_HANDLE(zest_pipeline_template)
ZEST__MAKE_HANDLE(zest_set_layout)
ZEST__MAKE_HANDLE(zest_descriptor_set)
ZEST__MAKE_HANDLE(zest_shader_resources)
ZEST__MAKE_HANDLE(zest_descriptor_buffer)
ZEST__MAKE_HANDLE(zest_uniform_buffer)
ZEST__MAKE_HANDLE(zest_frame_staging_buffer)
ZEST__MAKE_HANDLE(zest_render_target)
ZEST__MAKE_HANDLE(zest_buffer_allocator)
ZEST__MAKE_HANDLE(zest_descriptor_pool)
ZEST__MAKE_HANDLE(zest_compute)
ZEST__MAKE_HANDLE(zest_buffer)
ZEST__MAKE_HANDLE(zest_device_memory_pool)
ZEST__MAKE_HANDLE(zest_timer)
ZEST__MAKE_HANDLE(zest_window)
ZEST__MAKE_HANDLE(zest_shader)
ZEST__MAKE_HANDLE(zest_recorder)
ZEST__MAKE_HANDLE(zest_imgui)
ZEST__MAKE_HANDLE(zest_queue)

// --Private structs with inline functions
typedef struct zest_queue_family_indices {
    zest_uint graphics_family_index;
    zest_uint transfer_family_index;
    zest_uint compute_family_index;
} zest_queue_family_indices;

// --Pocket Dynamic Array
typedef struct zest_vec {
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
#define zest_vec_linear_grow(allocator, T) ((!(T) || (zest__vec_header(T)->current_size == zest__vec_header(T)->capacity)) ? T = zest__vec_linear_reserve(allocator, (T), sizeof(*T), (T ? zest__grow_capacity(T, zest__vec_header(T)->current_size) : 8)) : 0)
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
#define zest_foreach_i(T) int i = 0; i != zest_vec_size(T); ++i
#define zest_foreach_j(T) int j = 0; j != zest_vec_size(T); ++j
#define zest_foreach_k(T) int k = 0; k != zest_vec_size(T); ++k
// --end of pocket dynamic array

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
typedef struct {
    zest_key key;
    zest_index index;
} zest_hash_pair;

#ifndef ZEST_HASH_SEED
#define ZEST_HASH_SEED 0xABCDEF99
#endif
ZEST_PRIVATE zest_hash_pair* zest__lower_bound(zest_hash_pair *map, zest_key key) { zest_hash_pair *first = map; zest_hash_pair *last = map ? zest_vec_end(map) : 0; size_t count = (size_t)(last - first); while (count > 0) { size_t count2 = count >> 1; zest_hash_pair* mid = first + count2; if (mid->key < key) { first = ++mid; count -= count2 + 1; } else { count = count2; } } return first; }
ZEST_PRIVATE void zest__map_realign_indexes(zest_hash_pair *map, zest_index index) { for (zest_foreach_i(map)) { if (map[i].index < index) continue; map[i].index--; } }
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
#define zest_map_last_index(hash_map) (hash_map.last_index)
#define zest_map_size(hash_map) (hash_map.map ? zest__vec_header(hash_map.data)->current_size : 0)
#define zest_map_clear(hash_map) zest_vec_clear(hash_map.map); zest_vec_clear(hash_map.data); zest_vec_clear(hash_map.free_slots)
#define zest_map_free(hash_map) if(hash_map.map) ZEST__FREE(zest__vec_header(hash_map.map)); if(hash_map.data) ZEST__FREE(zest__vec_header(hash_map.data)); if(hash_map.free_slots) ZEST__FREE(zest__vec_header(hash_map.free_slots)); hash_map.data = 0; hash_map.map = 0; hash_map.free_slots = 0
//Use inside a for loop to iterate over the map. The loop index should be be the index into the map array, to get the index into the data array.
#define zest_map_index(hash_map, i) (hash_map.map[i].index)
#define zest_map_foreach(index, hash_map) zest_vec_foreach(index, hash_map.map)
#define zest_map_foreach_i(hash_map) zest_foreach_i(hash_map.map)
#define zest_map_foreach_j(hash_map) zest_foreach_j(hash_map.map)
// --End pocket hash map

// --Begin Pocket_text_buffer
typedef struct zest_text_t {
    char *str;
} zest_text_t;

ZEST_API void zest_SetText(zest_text_t *buffer, const char *text);
ZEST_API void zest_ResizeText(zest_text_t *buffer, zest_uint size);
ZEST_API void zest_SetTextf(zest_text_t *buffer, const char *text, ...);
ZEST_API void zest_SetTextfv(zest_text_t *buffer, const char *text, va_list args);
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
    zest_render_target render_target;
} zest_log_entry_t;

ZEST_PRIVATE void zest__log_entry(const char *entry, ...);
ZEST_PRIVATE void zest__log_entry_v(char *str, const char *entry, ...);
ZEST_PRIVATE void zest__reset_frame_log(char *str, const char *entry, ...);

#ifdef ZEST_DEBUGGING
#define ZEST_FRAME_LOG(message_f, ...) zest__log_entry(message_f, ##__VA_ARGS__)
#define ZEST_RESET_LOG() zest__reset_log()
#else
#define ZEST_FRAME_LOG(message_f, ...) 
#define ZEST_RESET_LOG() 
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

//Note that using alignas on windows causes a crash in release mode relating to the allocator.
//Not sure why though. We need the align as on Mac otherwise metal complains about the alignment
//in the shaders
typedef struct zest_vec4 {
    float x, y, z, w;
} zest_vec4 ZEST_ALIGN_AFFIX(16);

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

typedef struct zest_timer_t {
    int magic;
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

// --Vulkan Buffer Management
typedef struct zest_buffer_type_t {
    VkDeviceSize alignment;
    zest_uint memory_type_index;
} zest_buffer_type_t;

typedef struct zest_buffer_info_t {
    VkImageUsageFlags image_usage_flags;
    VkBufferUsageFlags usage_flags;                    //The usage state_flags of the memory block.
    VkMemoryPropertyFlags property_flags;
    zest_buffer_flags flags;
    zest_uint memory_type_bits;
    VkDeviceSize alignment;
    zest_uint frame_in_flight;
} zest_buffer_info_t;

typedef struct zest_buffer_pool_size_t {
    const char *name;
    zest_size pool_size;
    zest_size minimum_allocation_size;
} zest_buffer_pool_size_t;

typedef struct zest_buffer_usage_t {
    VkBufferUsageFlags usage_flags;
    VkMemoryPropertyFlags property_flags;
    VkImageUsageFlags image_flags;
} zest_buffer_usage_t;

zest_hash_map(zest_buffer_pool_size_t) zest_map_buffer_pool_sizes;

//A simple buffer struct for creating and mapping GPU memory
typedef struct zest_device_memory_pool_t {
    int magic;
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkDeviceSize minimum_allocation_size;
    VkDeviceSize alignment;
    VkImageUsageFlags usage_flags;
    VkMemoryPropertyFlags property_flags;
    VkDescriptorBufferInfo descriptor;
    zest_uint memory_type_index;
    void* mapped;
    const char *name;
} zest_device_memory_pool_t;

typedef void* zest_pool_range;

typedef struct zest_buffer_allocator_t {
    int magic;
    zest_buffer_info_t buffer_info;
    zloc_allocator *allocator;
    zest_size alignment;
    zest_device_memory_pool *memory_pools;
    zest_pool_range *range_pools;
} zest_buffer_allocator_t;

typedef struct zest_buffer_t {
    VkDeviceSize size;
    VkDeviceSize memory_offset;
    zest_device_memory_pool memory_pool;
    zest_buffer_allocator buffer_allocator;
    zest_size memory_in_use;
    void *data;
    void *end;
} zest_buffer_t;

//Simple stuct for uploading buffers from the staging buffer to the device local buffers
typedef struct zest_buffer_uploader_t {
    zest_buffer_upload_flags flags;
    zest_buffer_t *source_buffer;           //The source memory allocation (cpu visible staging buffer)
    zest_buffer_t *target_buffer;           //The target memory allocation that we're uploading to (device local buffer)
    VkBufferCopy *buffer_copies;            //List of vulkan copy info commands to upload staging buffers to the gpu each frame
} zest_buffer_uploader_t;

// --End Vulkan Buffer Management

typedef struct zest_descriptor_pool_t {
    int magic;
    VkDescriptorPool vk_descriptor_pool;
    zest_uint max_sets;
    zest_uint allocations;
    VkDescriptorPoolSize *vk_pool_sizes;
} zest_descriptor_pool_t;

typedef struct zest_swapchain_support_details_t {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *formats;
    zest_uint formats_count;
    VkPresentModeKHR *present_modes;
    zest_uint present_modes_count;
} zest_swapchain_support_details_t;

typedef struct zest_window_t {
    int magic;
    void *window_handle;
    VkSurfaceKHR surface;
    zest_uint window_width;
    zest_uint window_height;
    zest_bool framebuffer_resized;
} zest_window_t;

zest_hash_map(VkDescriptorPoolSize) zest_map_descriptor_pool_sizes;

typedef struct zest_create_info_t {
    const char *title;                                  //Title that shows in the window
    const char *shader_path_prefix;                     //Prefix prepending to the shader path when loading default shaders
    const char* log_path;                               //path to the log to store log and validation messages
    zest_size memory_pool_size;                         //The size of each memory pool. More pools are added if needed
    zest_size render_graph_allocator_size;              //The size of the linear allocator used by render graphs to store temporary data
    int screen_width, screen_height;                    //Default width and height of the window that you open
    int screen_x, screen_y;                             //Default position of the window
    int virtual_width, virtual_height;                  //The virtial width/height of the viewport
    int thread_count;                                   //The number of threads to use if multithreading. 0 if not.
    VkFormat color_format;                              //Choose between VK_FORMAT_R8G8B8A8_UNORM and VK_FORMAT_R8G8B8A8_SRGB
    VkDescriptorPoolSize pool_counts[11];               //You can define descriptor pool counts here using the zest_SetDescriptorPoolCount for each pool type. Defaults will be added for any not defined
    zest_init_flags flags;                              //Set flags to apply different initialisation options
    zest_uint maximum_textures;                         //The maximum number of textures you can load. 1024 is the default.
    zest_uint bindless_combined_sampler_count;
    zest_uint bindless_storage_buffer_count;
    zest_uint bindless_sampler_count;
    zest_uint bindless_sampled_image_count;

    //Callbacks: use these to implement your own preferred window creation functionality
    void(*get_window_size_callback)(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
    void(*destroy_window_callback)(void *user_data);
    void(*poll_events_callback)(ZEST_PROTOTYPE);
    void(*add_platform_extensions_callback)(ZEST_PROTOTYPE);
    zest_window(*create_window_callback)(int x, int y, int width, int height, zest_bool maximised, const char* title);
    void(*create_window_surface_callback)(zest_window window);
} zest_create_info_t;

typedef struct zest_queue_t {
    VkQueue vk_queue;
    VkSemaphore semaphore[ZEST_MAX_FIF];
    zest_u64 current_count[ZEST_MAX_FIF];
    VkPipelineStageFlags wait_stage_mask;
} zest_queue_t;

zest_hash_map(const char *) zest_map_queue_names;

typedef struct zest_device_t {
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
    VkAllocationCallbacks allocation_callbacks;
    char **extensions;
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkSampleCountFlagBits msaa_samples;
    zest_swapchain_support_details_t swapchain_support_details;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory_properties;
    zest_uint graphics_queue_family_index;
    zest_uint transfer_queue_family_index;
    zest_uint compute_queue_family_index;
    zest_queue_t graphics_queue;
    zest_queue_t compute_queue;
    zest_queue_t transfer_queue;
    VkQueueFamilyProperties *queue_families;
    zest_map_queue_names queue_names;
    VkCommandPool command_pool;
    VkCommandPool one_time_command_pool;
    PFN_vkSetDebugUtilsObjectNameEXT pfnSetDebugUtilsObjectNameEXT;
    VkFormat color_format;
    zest_map_buffer_pool_sizes pool_sizes;
    void *allocator_start;
    void *allocator_end;
    zest_text_t log_path;
    zest_create_info_t setup_info;
} zest_device_t;

typedef struct zest_app_t {
    zest_create_info_t create_info;

    void(*update_callback)(zest_microsecs, void*);
    void *user_data;

    zest_window window;

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
} zest_app_t;

typedef struct zest_image_buffer_t {
    VkImage image;
    zest_buffer buffer;
    VkImageView base_view;
    VkImageView *mip_views;
    VkFormat format;
} zest_image_buffer_t;

typedef struct zest_frame_buffer_t {
    zest_uint width, height;
    VkFormat format;
    VkFramebuffer *framebuffers;
    zest_image_buffer_t color_buffer, depth_buffer;
    VkSampler sampler;
} zest_frame_buffer_t;

typedef struct zest_render_target_push_constants_t {
    zest_vec2 screen_resolution;            //the current size of the window/resolution
} zest_render_target_push_constants_t;

typedef struct zest_mip_push_constants_t {
    zest_vec4 settings;                     
    zest_vec2 source_resolution;            //the current size of the window/resolution
    zest_vec2 padding;
} zest_mip_push_constants_t;

typedef struct zest_semaphore_connector_t {
    VkSemaphore *fif_incoming_semaphores;                //Must be waited on, eg a command queue waiting on present signal
    VkSemaphore *fif_outgoing_semaphores;                //To signal the next process, eg a command queue signalling the present process
    zest_connector_type type;
} zest_semaphore_connector_t;

typedef struct zest_gpu_timestamp_s {
    zest_u64 start;
    zest_u64 end;
} zest_gpu_timestamp_t;

typedef struct zest_gpu_timestamp_info_s {
    zest_render_target render_target;
} zest_gpu_timestamp_info_t;

typedef struct zest_timestamp_duration_s {
    double nanoseconds;
    double microseconds;
    double milliseconds;
} zest_timestamp_duration_t;

//Render_graph_types

typedef struct zest_render_graph_context_t {
    VkCommandBuffer command_buffer;
    VkRenderPass render_pass;
    zest_render_graph render_graph;
    zest_pass_node pass_node;
} zest_render_graph_context_t;

typedef void (*zest_rg_execution_callback)(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data);
typedef void* zest_resource_handle;

typedef struct zest_image_description_t {
	VkFormat format;
    zest_uint width;
	zest_uint height;
	zest_uint mip_levels;
	VkSampleCountFlagBits numSamples;
	VkImageTiling tiling;
	VkImageUsageFlags usage;
	VkMemoryPropertyFlags properties;
} zest_image_description_t;

typedef struct zest_buffer_description_t { 
    VkDeviceSize size;
    zest_buffer_info_t buffer_info;
} zest_buffer_description_t;

typedef struct zest_pass_adjacency_list_t {
    int *pass_indices;
} zest_pass_adjacency_list_t;

typedef struct zest_rg_pass_resource_usage_desc_t {
    zest_resource_node resource_node;
    VkImageLayout         image_layout;
    VkAccessFlags         access_mask;
    VkPipelineStageFlags  stage_mask;
    VkImageAspectFlags    aspect_flags; 

    VkAttachmentLoadOp    load_op;
    VkAttachmentStoreOp   store_op;
    VkAttachmentLoadOp    stencil_load_op;
    VkAttachmentStoreOp   stencil_store_op;
    VkClearValue          clear_value;
} zest_rg_pass_resource_usage_desc_t;

typedef struct zest_resource_usage_t {
    zest_resource_node resource_node;   
    zest_resource_access_type access_type;
    VkPipelineStageFlags stage_mask; // Pipeline stages this usage pertains to
    VkAccessFlags access_mask;       // Vulkan access mask for barriers
    VkImageLayout image_layout;      // Required VkImageLayout if it's an image
    VkImageAspectFlags    aspect_flags; 
    // For framebuffer attachments
    VkAttachmentLoadOp load_op;
    VkAttachmentStoreOp store_op;
    VkAttachmentLoadOp stencil_load_op;
    VkAttachmentStoreOp stencil_store_op;
    VkClearValue clear_value;
} zest_resource_usage_t;

zest_hash_map(zest_resource_usage_t) zest_map_resource_usages;

typedef struct zest_pass_execution_callback_t {
    zest_rg_execution_callback execution_callback;
    void *user_data;
} zest_pass_execution_callback_t;

typedef struct zest_resource_state_t {
    zest_uint pass_index;
    zest_resource_usage_t *usage;
    zest_uint queue_family_index;
    bool was_released;
} zest_resource_state_t;

typedef struct zest_pass_node_t {
    int magic;
    zest_id id;
    const char *name;
    
    zest_queue queue;
    zest_uint queue_family_index;
    zest_uint batch_index;
    zest_uint execution_order_index;
    zest_device_queue_type queue_type;
    zest_map_resource_usages inputs;
    zest_map_resource_usages outputs;
    zest_pass_execution_callback_t *execution_callbacks;
    zest_render_graph render_graph;
    zest_compute compute;
} zest_pass_node_t;

typedef struct zest_resource_node_t {
    int magic;
    const char *name;
    zest_resource_type type;
    zest_id id;                                     // Unique identifier within the graph
    zest_resource_handle *resource;
    zest_render_graph render_graph;

	zest_image_description_t image_desc;   // Used if transient image
	zest_buffer_description_t buffer_desc; // Used if transient buffer

    zest_image_buffer_t image_buffer;
    zest_buffer storage_buffer;
    zest_uint binding_number;
    zest_uint bindless_index;               //The index to use in the shader

    zest_uint current_queue_family_index;
    VkAccessFlags current_access_mask;
    VkPipelineStageFlags last_stage_mask;
    VkImageLayout current_layout;                   // The current layout of the image in the resource
    VkImageLayout final_layout;                     // Layout resource should be in after last use in this graph

    zest_resource_state_t *journey;                 // List of the different states this resource has over the render graph, used to build barriers where needed
    int producer_pass_idx;                          // Index of the pass that writes/creates this resource (-1 if imported)
    int *consumer_pass_indices;                     // Dynamic array of pass indices that read this
    zest_resource_node_flags flags;                 // Imported/Transient Etc.
    zest_uint first_usage_pass_idx;                 // For lifetime tracking
    zest_uint last_usage_pass_idx;                  // For lifetime tracking
} zest_resource_node_t;

typedef struct zest_execution_barriers_t {
    VkImageMemoryBarrier *acquire_image_barriers;
    VkBufferMemoryBarrier *acquire_buffer_barriers;
    VkImageMemoryBarrier *release_image_barriers;
    VkBufferMemoryBarrier *release_buffer_barriers;
    zest_resource_node *acquire_image_barrier_nodes;
    zest_resource_node *acquire_buffer_barrier_nodes;
    zest_resource_node *release_image_barrier_nodes;
    zest_resource_node *release_buffer_barrier_nodes;
    VkPipelineStageFlags overall_src_stage_mask_for_acquire_barriers;
    VkPipelineStageFlags overall_dst_stage_mask_for_acquire_barriers;
    VkPipelineStageFlags overall_src_stage_mask_for_release_barriers;
    VkPipelineStageFlags overall_dst_stage_mask_for_release_barriers;
} zest_execution_barriers_t;

typedef struct zest_execution_details_t {
    VkFramebuffer frame_buffer;
    VkRenderPass render_pass;
    VkRect2D render_area;
    VkClearValue *clear_values;
    zest_execution_barriers_t barriers;
    bool requires_dynamic_render_pass;
} zest_execution_details_t;

typedef struct zest_temp_attachment_info_t { 
    zest_resource_node resource_node; 
    zest_resource_usage_t *usage_info; 
    zest_uint attachment_slot;
} zest_temp_attachment_info_t;

typedef struct zest_binding_index_for_release_t {
    zest_set_layout layout;
    zest_uint binding_index;
    zest_uint binding_number;
} zest_binding_index_for_release_t;

typedef struct zest_destruction_queue_t {
    zest_buffer *buffers[ZEST_MAX_FIF];
    zest_image_buffer_t *images[ZEST_MAX_FIF];
    zest_texture *textures[ZEST_MAX_FIF];
    zest_binding_index_for_release_t *binding_indexes[ZEST_MAX_FIF];
} zest_destruction_queue_t;

typedef struct zest_submission_batch_t {
    zest_queue queue;
    zest_uint queue_family_index;
    zest_device_queue_type queue_type;
    zest_uint *pass_indices;
    VkCommandBuffer command_buffer;
    VkSemaphore internal_signal_semaphore;
    VkSemaphore *wait_semaphores;
    VkPipelineStageFlags *wait_dst_stage_masks;
    zest_uint output_pass;
} zest_submission_batch_t;

typedef struct zest_render_graph_t {
    int magic;
    zest_render_graph_flags flags;
    const char *name;

    zest_pass_node_t *passes; 
    zest_resource_node_t *resources; 

    zest_uint *compiled_execution_order;            // Execution order after compilation
    zest_execution_details_t *pass_exec_details_list;

    zest_resource_handle swapchain_resource_handle; // Handle to the current swapchain image resource
    VkImage current_swapchain_image;
    VkImageView current_swapchain_image_view;
    zest_uint current_swapchain_image_index;
    zest_uint id_counter;
    zest_descriptor_pool descriptor_pool;           //Descriptor pool for execution nodes within the graph.
    zest_set_layout bindless_layout;
    zest_descriptor_set bindless_set;

    zest_submission_batch_t *submissions;

    void *user_data;
    zest_render_graph_context_t context;

    VkQueryPool query_pool;                                          //For profiling
    zest_uint timestamp_count;
    zest_query_state query_state[ZEST_MAX_FIF];                      //For checking if the timestamp query is ready
    zest_gpu_timestamp_t *timestamps[ZEST_MAX_FIF];                  //The last recorded frame durations for the whole render pipeline
    zest_gpu_timestamp_info_t *timestamp_infos[ZEST_MAX_FIF];        //The last recorded frame durations for the whole render pipeline

    VkPipelineStageFlags *fif_wait_stage_flags[ZEST_MAX_FIF];        //Stage state_flags relavent to the incoming semaphores
} zest_render_graph_t;

ZEST_API zest_bool zest_AcquireSwapChainImage(void);

// --- Internal render graph function ---
ZEST_PRIVATE zest_bool zest__is_stage_compatible_with_qfi(VkPipelineStageFlags stages_to_check, VkQueueFlags queue_family_capabilities);
ZEST_PRIVATE VkImageLayout zest__determine_final_layout(int start_from_idx, zest_resource_node node, zest_resource_usage_t *current_usage);
ZEST_PRIVATE VkImageAspectFlags zest__determine_aspect_flag(VkFormat format);
ZEST_PRIVATE void zest__deferr_buffer_destruction(zest_buffer storage_buffer);
ZEST_PRIVATE void zest__deferr_image_destruction(zest_image_buffer_t *image_buffer);
ZEST_PRIVATE zest_pass_node zest__add_pass_node(const char *name, zest_device_queue_type queue_type);
ZEST_PRIVATE VkCommandPool zest__create_queue_command_pool(int queue_family_index);
ZEST_PRIVATE zest_resource_node_t zest__create_import_image_resource_node(const char *name, zest_texture texture);
ZEST_PRIVATE zest_resource_node_t zest__create_import_buffer_resource_node(const char *name, zest_descriptor_buffer buffer);
ZEST_PRIVATE zest_uint zest__get_image_binding_number(zest_resource_node resource, bool image_view_only);
ZEST_PRIVATE zest_uint zest__get_buffer_binding_number(zest_resource_node resource);
ZEST_PRIVATE void zest__add_pass_buffer_usage(zest_pass_node pass_node, zest_resource_node buffer_resource, zest_resource_purpose purpose, VkPipelineStageFlags relevant_pipeline_stages, zest_bool is_output);
ZEST_PRIVATE void zest__add_pass_image_usage(zest_pass_node pass_node, zest_resource_node image_resource, zest_resource_purpose purpose, VkPipelineStageFlags relevant_pipeline_stages, zest_bool is_output, VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op, VkAttachmentLoadOp stencil_load_op, VkAttachmentStoreOp stencil_store_op, VkClearValue clear_value);
ZEST_PRIVATE zest_render_graph zest__new_render_graph(const char *name);

// --- Utility callbacks ---
void zest_EmptyRenderPass(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data);

// --- Retrieve resources from a render graph
ZEST_API zest_resource_node zest_GetPassInputResource(zest_pass_node pass, const char *name);
ZEST_API zest_resource_node zest_GetPassOutputResource(zest_pass_node pass, const char *name);
ZEST_API zest_buffer zest_GetPassInputBuffer(zest_pass_node pass, const char *name);
ZEST_API zest_buffer zest_GetPassOutputBuffer(zest_pass_node pass, const char *name);

// -- Creating and Executing the render graph
ZEST_API bool zest_BeginRenderGraph(const char *name);
ZEST_API bool zest_BeginRenderToScreen(const char *name);
ZEST_API void zest_EndRenderGraph();
ZEST_API void zest_ExecuteRenderGraph();

// --- Add pass nodes that execute user commands ---
ZEST_API zest_pass_node zest_AddGraphicBlankScreen( const char *name);
ZEST_API zest_pass_node zest_AddRenderPassNode(const char *name);
ZEST_API zest_pass_node zest_AddComputePassNode(zest_compute compute, const char *name);
ZEST_API zest_pass_node zest_AddTransferPassNode(const char *name);

// --- Add callback tasks to passes
ZEST_API void zest_AddPassTask(zest_pass_node pass, zest_rg_execution_callback callback, void *user_data);
ZEST_API void zest_ClearPassTasks(zest_pass_node pass);

// --- Add Transient resources ---
ZEST_API zest_resource_node zest_AddTransientImageResource(const char *name, const zest_image_description_t *desc, zest_bool assign_bindless, zest_bool image_view_binding_only);
ZEST_API zest_resource_node zest_AddTransientBufferResource(const char *name, const zest_buffer_description_t *desc, zest_bool assign_bindless);
ZEST_API zest_resource_node zest_AddInstanceLayerBufferResource(const zest_layer layer);
ZEST_API zest_resource_node zest_AddFontLayerTextureResource(const zest_font font);

// --- Helpers for adding various types of ribbon resources
ZEST_API zest_resource_node zest_AddTransientVertexBufferResource(const char *name, zest_size size, zest_bool include_storage_flags, zest_bool assign_bindless);
ZEST_API zest_resource_node zest_AddTransientIndexBufferResource(const char *name, zest_size size, zest_bool include_storage_flags, zest_bool assign_bindless);
ZEST_API zest_resource_node zest_AddTransientStorageBufferResource(const char *name, zest_size size, zest_bool assign_bindless);

// --- Import external resouces into the render graph ---
ZEST_API zest_resource_node zest_ImportImageResource(const char *name, zest_texture texture, VkImageLayout initial_layout_at_graph_start, VkImageLayout desired_layout_after_graph_use);
ZEST_API zest_resource_node zest_ImportImageResourceReadOnly(const char *name, zest_texture texture);
ZEST_API zest_resource_node zest_ImportStorageBufferResource(const char *name, zest_descriptor_buffer buffer);
ZEST_API zest_resource_node zest_ImportLayerResource(const char *name, zest_layer layer);
ZEST_API zest_resource_node zest_ImportSwapChainResource(const char *name);

// --- Connect Buffer Helpers ---
ZEST_API void zest_ConnectVertexBufferInput(zest_pass_node pass, zest_resource_node vertex_buffer);
ZEST_API void zest_ConnectIndexBufferInput(zest_pass_node pass, zest_resource_node index_buffer);
ZEST_API void zest_ConnectUniformBufferInput(zest_pass_node pass, zest_resource_node uniform_buffer, zest_supported_pipeline_stages stages);
ZEST_API void zest_ConnectStorageBufferInput(zest_pass_node pass, zest_resource_node storage_buffer, zest_supported_pipeline_stages stages);
ZEST_API void zest_ConnectTransferBufferInput(zest_pass_node pass, zest_resource_node src_buffer);
ZEST_API void zest_ConnectStorageBufferOutput(zest_pass_node pass, zest_resource_node storage_buffer, zest_supported_pipeline_stages stages);
ZEST_API void zest_ConnectTransferBufferOutput(zest_pass_node pass, zest_resource_node dst_buffer);

// --- Connect Image Helpers ---
ZEST_API void zest_ConnectSampledImageInput(zest_pass_node pass, zest_resource_node texture, zest_supported_pipeline_stages stages);
ZEST_API void zest_ConnectSwapChainOutput(zest_pass_node pass, zest_resource_node swapchain_resource, VkClearColorValue clear_color_on_load);
ZEST_API void zest_ConnectColorAttachmentOutput(zest_pass_node pass_node, zest_resource_node color_target, VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op, VkClearColorValue clear_color_if_clearing);
ZEST_API void zest_ConnectDepthStencilOutput(zest_pass_node pass_node, zest_resource_node depth_target, VkAttachmentLoadOp depth_load_op, VkAttachmentStoreOp depth_store_op, VkAttachmentLoadOp stencil_load_op, VkAttachmentStoreOp stencil_store_op, VkClearDepthStencilValue clear_value_if_clearing);
ZEST_API void zest_ConnectDepthStencilInputReadOnly(zest_pass_node pass_node, zest_resource_node depth_target);

// Helper functions to convert enums to strings 
ZEST_PRIVATE const char *zest_vulkan_image_layout_to_string(VkImageLayout layout);
ZEST_PRIVATE zest_text_t zest_vulkan_access_flags_to_string(VkAccessFlags flags);
ZEST_PRIVATE zest_text_t zest_vulkan_pipeline_stage_flags_to_string(VkPipelineStageFlags flags);

// --- Render graph debug functions ---
ZEST_API void zest_PrintCompiledRenderGraph();

//command queues are the main thing you use to draw things to the screen. A simple app will create one for you, or you can create your own. See examples like PostEffects for a more complex example
typedef struct zest_command_queue_t {
    int magic;
    const char *name;
    VkQueryPool query_pool;                                          //For profiling
    zest_uint timestamp_count;
    zest_query_state query_state[ZEST_MAX_FIF];                      //For checking if the timestamp query is ready
    zest_gpu_timestamp_t *timestamps[ZEST_MAX_FIF];                  //The last recorded frame durations for the whole render pipeline
    zest_gpu_timestamp_info_t *timestamp_infos[ZEST_MAX_FIF];        //The last recorded frame durations for the whole render pipeline
    zest_recorder recorder;                                          //Primary command buffer for recording the whole queue
    VkPipelineStageFlags *fif_wait_stage_flags[ZEST_MAX_FIF];        //Stage state_flags relavent to the incoming semaphores
    zest_command_queue_draw_commands *draw_commands;                 //A list of draw command handles - mostly these will be draw_commands that are recorded to the command buffer
    zest_command_queue_compute *compute_commands;                    //Compute items to be recorded to the command buffer
    zest_index present_semaphore_index[ZEST_MAX_FIF];                //An index to the semaphore representing the swap chain if required. (command queues don't necessarily have to wait for the swap chain)
    zest_command_queue_flags flags;                                  //Can be either dependent on the swap chain to present or another command queue
    VkCommandBuffer *secondary_compute_command_buffers;              //All recorded secondary compute buffers each frame
} zest_command_queue_t;

typedef struct zest_set_layout_builder_t {
    VkDescriptorSetLayoutBinding *bindings;
    VkDescriptorBindingFlags *binding_flags;
    zest_u64 binding_indexes;
    VkDescriptorSetLayoutCreateFlags create_flags;
} zest_set_layout_builder_t;

typedef struct zest_layout_type_counts_t {
    zest_uint sampler_count;
    zest_uint sampler_image_count;
    zest_uint uniform_buffer_count;
    zest_uint storage_buffer_count;
    zest_uint combined_image_sampler_count;
} zest_layout_type_counts_t;

typedef struct zest_descriptor_indices_t {
    zest_uint *free_indices;
    zest_uint next_new_index;
    zest_uint capacity;
    VkDescriptorType descriptor_type;
} zest_descriptor_indices_t;

typedef struct zest_descriptor_set_t {
    int magic;
    VkDescriptorSet vk_descriptor_set;
} zest_descriptor_set_t;

typedef struct zest_set_layout_t {
    int magic;
    VkDescriptorSetLayoutBinding *layout_bindings;
    VkDescriptorSetLayout vk_layout;
    zest_text_t name;
    zest_u64 binding_indexes;
    VkDescriptorSetLayoutCreateFlags create_flags;
    zest_descriptor_pool pool;
    zest_descriptor_indices_t *descriptor_indexes;
} zest_set_layout_t;

typedef struct zest_shader_resources_t {
    int magic;
    zest_descriptor_set *sets[ZEST_MAX_FIF];
    VkDescriptorSet *binding_sets;
    VkCommandBuffer *command_buffers;
} zest_shader_resources_t ZEST_ALIGN_AFFIX(16);

typedef struct zest_descriptor_set_builder_t {
    VkWriteDescriptorSet *writes;
    zest_set_layout associated_layout;
    zest_uint bindings;
    VkDescriptorImageInfo *image_infos_storage;
    VkDescriptorBufferInfo *buffer_infos_storage;
    VkBufferView *texel_buffer_view_storage;
} zest_descriptor_set_builder_t;

typedef struct zest_descriptor_buffer_t {
    int magic;
    zest_buffer buffer;
    VkDescriptorBufferInfo descriptor_info;
    zest_descriptor_set descriptor_set;
    zest_uint descriptor_array_index;
    zest_uint binding_number;
} zest_descriptor_buffer_t;

typedef struct zest_uniform_buffer_t {
    int magic;
    zest_buffer buffer[ZEST_MAX_FIF];
    VkDescriptorBufferInfo descriptor_info[ZEST_MAX_FIF];
    zest_descriptor_set descriptor_set[ZEST_MAX_FIF];
    zest_set_layout set_layout;
} zest_uniform_buffer_t;

typedef struct zest_frame_staging_buffer_t {
    int magic;
    zest_buffer buffer[ZEST_MAX_FIF];
} zest_frame_staging_buffer_t;

typedef struct zest_descriptor_infos_for_binding_t {
    VkDescriptorBufferInfo descriptor_buffer_info;
    VkDescriptorImageInfo descriptor_image_info;
    zest_texture texture;
    zest_descriptor_buffer buffer;
} zest_descriptor_infos_for_binding_t;

typedef struct zest_uniform_buffer_data_t {
    zest_matrix4 view;
    zest_matrix4 proj;
    zest_vec4 parameters1;
    zest_vec4 parameters2;
    zest_vec2 screen_size;
    zest_uint millisecs;
} zest_uniform_buffer_data_t;

typedef struct zest_render_pass_info_s{
	VkFormat render_format;
	VkImageLayout initial_layout;
	VkImageLayout final_layout;
	VkAttachmentLoadOp load_op;
	int depth_buffer;
} zest_render_pass_info_t;

//Pipeline template is used with CreatePipeline to create a vulkan pipeline. Use PipelineTemplate() or SetPipelineTemplate with PipelineTemplateCreateInfo to create a PipelineTemplate
typedef struct zest_pipeline_template_t {
    int magic;
    zest_hasher_t hasher;
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
    zest_render_pass_info_t render_pass_info;
    zest_pipeline_set_flags flags;                                               //Flag bits
    zest_uint uniforms;                                                          //Number of uniform buffers in the pipeline, usually 1 or 0
    void *push_constants;                                                        //Pointer to user push constant data
    
    VkDescriptorSetLayout *descriptorSetLayouts;
    VkVertexInputAttributeDescription *attributeDescriptions;
    VkVertexInputBindingDescription *bindingDescriptions;
    VkDynamicState *dynamicStates;
    zest_bool no_vertex_input;
    zest_text_t shader_path_prefix;
    zest_text_t vertShaderFunctionName;
    zest_text_t fragShaderFunctionName;
    VkPrimitiveTopology topology;
    zest_text_t vertShaderFile;
    zest_text_t fragShaderFile;
} zest_pipeline_template_t;

typedef struct zest_pipeline_descriptor_writes_t {
    VkWriteDescriptorSet *writes[ZEST_MAX_FIF];                       //Descriptor writes for creating the descriptor sets - is this needed here? only for certain pipelines, textures store their own
} zest_pipeline_descriptor_writes_t;

//A pipeline set is all of the necessary things required to setup and maintain a pipeline
typedef struct zest_pipeline_t {
    int magic;
    zest_pipeline_template pipeline_template;
    VkPipeline pipeline;                                                         //The vulkan handle for the pipeline
    VkPipelineLayout pipeline_layout;                                            //The vulkan handle for the pipeline layout
    zest_uniform_buffer uniform_buffer;                                          //Handle of the uniform buffer used in the pipline. Will be set to the default 2d uniform buffer if none is specified
    void(*rebuild_pipeline_function)(void*);                                     //Override the function to rebuild the pipeline when the swap chain is recreated
    zest_pipeline_set_flags flags;                                               //Flag bits
    VkRenderPass render_pass;
} zest_pipeline_t;

zest_hash_map(zest_pipeline) zest_map_pipeline_variations;

typedef struct zest_pipeline_handles_t {                                         //Struct for storing pipeline handles for future destroying after a scheduled pipeline rebuild
    VkPipeline pipeline;                                                         //The vulkan handle for the pipeline
    VkPipelineLayout pipeline_layout;                                            //The vulkan handle for the pipeline layout
} zest_pipeline_handles_t;

typedef struct zest_command_setup_context_t {
    zest_command_queue command_queue;
    zest_command_queue_draw_commands draw_commands;
    zest_draw_routine draw_routine;
    zest_layer layer;
    zest_index compute_index;
    zest_setup_context_type type;
    zest_render_target composite_render_target;
} zest_command_setup_context_t ;

ZEST_PRIVATE inline void zest__reset_command_setup_context(zest_command_setup_context_t *context) {
    context->command_queue = ZEST_NULL;
    context->draw_commands = ZEST_NULL;
    context->draw_routine = ZEST_NULL;
    context->layer = ZEST_NULL;
    context->type = zest_setup_context_type_none;
}

typedef void(*zest_record_draw_routine_callback)(zest_work_queue_t *queue, void *data);
typedef void(*zest_update_buffers_callback)(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);
typedef int(*zest_condition_callback)(zest_draw_routine draw_routine);                    
typedef void(*zest_update_resolution_callback)(zest_draw_routine draw_routine);
typedef void(*zest_clean_up_callback)(zest_draw_routine draw_routine);

//A draw routine is used to actually draw things to the render target. Zest provides Layers that have a set of draw commands for doing this or you can develop your own
//By settting the callbacks and data pointers in the draw routine
struct zest_draw_routine_t {
    int magic;
    const char *name;
    zest_uint last_fif;                                                          //The frame in flight index that was set
    zest_uint fif;
    zest_command_queue command_queue;                                            //The index of the render queue that this draw routine is within
    void *draw_data;                                                             //The user index of the draw routine. Use this to index the routine in your own lists. For Zest layers, this is used to hold the index of the layer in the renderer
    zest_index *command_queues;                                                  //A list of the render queues that this draw routine belongs to
    void *user_data;                                                             //Pointer to some user data
    zest_recorder recorder;                                                      //For recording to a command buffer (this is a secondary command buffer)
    zest_command_queue_draw_commands draw_commands;                              //
    zest_update_buffers_callback update_buffers_callback;                        //The callback used to update and upload the buffers before rendering
    zest_condition_callback condition_callback;                                  //The callback used to determine whether there's anything to record for the draw routine
    zest_record_draw_routine_callback record_callback;                           //draw callback called by the render target when rendering
    zest_update_resolution_callback update_resolution_callback;                  //Callback used when the window size changes
    zest_clean_up_callback clean_up_callback;                                    //Clean up function call back called when the draw routine is deleted
};

//Every command queue will have either one or more render passes (unless it's purely for compute shading). Render pass contains various data for drawing things during the render pass.
//These can be draw routines that you can use to draw various things to the screen. You can set the render_pass_function to whatever you need to record the command buffer. See existing render pass functions such as:
//RenderDrawRoutine, RenderRenderTarget and RenderTargetToSwapChain.
typedef struct zest_command_queue_draw_commands_t {
    int magic;
    VkFramebuffer(*get_frame_buffer)(zest_command_queue_draw_commands_t *item);
    void(*render_pass_function)(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer);
    void(*record_base_draw_commands)(zest_command_queue_draw_commands item);
    zest_draw_routine *draw_routines;
    zest_render_target *render_targets;
    VkRect2D viewport;
    zest_render_viewport_type viewport_type;
    zest_vec2 viewport_scale;
    VkRenderPass render_pass;
    zest_vec4 cls_color;
    zest_render_target render_target;
    zest_pipeline_template composite_pipeline;
    zest_shader_resources composite_shader_resources;
    zest_set_layout composite_descriptor_layout;
    zest_descriptor_pool descriptor_pool;
    zest_descriptor_set_t composite_descriptor_set[ZEST_MAX_FIF];
    VkCommandBuffer *secondary_command_buffers;
    const char *name;
} zest_command_queue_draw_commands_t;

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

typedef struct zest_mesh_t {
    zest_vertex_t* vertices;
    zest_uint* indexes;
} zest_mesh_t;

//We just have a copy of the ImGui Draw vert here so that we can setup things things for imgui
//should anyone choose to use it
typedef struct zest_ImDrawVert_t
{
    zest_vec2 pos;
    zest_vec2 uv;
    zest_uint col;
} zest_ImDrawVert_t;

typedef struct zest_layer_staging_buffers_t {
    union {
		zest_buffer staging_vertex_data;
		zest_buffer staging_instance_data;
    };
	zest_buffer staging_index_data;

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
} zest_layer_staging_buffers_t;

typedef struct zest_push_constants_t {             //128 bytes seems to be the limit for push constants on AMD cards, NVidia 256 bytes
    zest_uint descriptor_index[4];
    zest_vec4 parameters1;                         //Can be used for anything
    zest_vec4 parameters2;                         //Can be used for anything
    zest_vec4 parameters3;                         //Can be used for anything
    zest_vec4 global;                              //Can be set every frame for all current draw instructions
} zest_push_constants_t ZEST_ALIGN_AFFIX(16);

typedef struct zest_layer_instruction_t {
    zest_push_constants_t push_constants;         //Each draw instruction can have different values in the push constants push_constants
    VkRect2D scissor;                             //The drawinstruction can also clip whats drawn
    VkViewport viewport;                          //The viewport size of the draw call
    zest_index start_index;                       //The starting index
    union {
        zest_uint total_instances;                //The total number of instances to be drawn in the draw instruction
        zest_uint total_indexes;                  //The total number of indexes to be drawn in the draw instruction
    };
    zest_index last_instance;                     //The last instance that was drawn in the previous instance instruction
    zest_pipeline_template pipeline_template;     //The pipeline template to draw the instances.
    zest_shader_resources shader_resources;       //The descriptor set shader_resources used to draw with
    void *asset;                                  //Optional pointer to either texture, font etc
    zest_draw_mode draw_mode;
} zest_layer_instruction_t ZEST_ALIGN_AFFIX(16);

typedef struct zest_layer_t {
    int magic;

    const char *name;

    zest_layer_staging_buffers_t memory_refs;
    zest_bool dirty[ZEST_MAX_FIF];
    zest_uint initial_instance_pool_size;
    zest_buffer_uploader_t vertex_upload;
    zest_buffer_uploader_t index_upload;

    zest_index sprite_pipeline_index;

    zest_layer_instruction_t current_instruction;

    union {
        struct { zest_size instance_struct_size; };
        struct { zest_size vertex_struct_size; };
    };

    zest_color current_color;
    float intensity;
    zest_push_constants_t push_constants;
    zest_vec4 global_push_values;

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
    VkDescriptorSet *draw_sets;
} zest_layer_t ZEST_ALIGN_AFFIX(16);

typedef struct zest_layer_builder_t {
    zest_size type_size;
    zest_uint initial_count;
    zest_update_buffers_callback update_buffers_callback;                        //The callback used to update and upload the buffers before rendering
    zest_condition_callback condition_callback;                                  //The callback used to determine whether there's anything to record for the draw routine
    zest_record_draw_routine_callback record_callback;                           //draw callback called by the render target when rendering
    zest_update_resolution_callback update_resolution_callback;                  //Callback used when the window size changes
    zest_clean_up_callback clean_up_callback;                                    //Clean up function call back called when the draw routine is deleted
} zest_layer_builder_t;

//This struct must be filled and attached to the draw routine that implements imgui as user data
typedef struct zest_imgui_t {
    int magic;
	zest_texture font_texture;
	zest_pipeline_template pipeline;
    zest_buffer vertex_staging_buffer;
    zest_buffer index_staging_buffer;
    zest_push_constants_t push_constants;
    zest_shader_resources shader_resources;
    VkDescriptorSet *draw_sets;
} zest_imgui_t;

typedef struct zest_font_character_t {
    char character[4];
    float width;
    float height;
    float xoffset;
    float yoffset;
    float xadvance;
    zest_uint flags;
    float reserved1;
    zest_vec4 uv;
    zest_u64 uv_packed;
    float reserved2[2];
} zest_font_character_t;

typedef struct zest_font_t {
    int magic;
    zest_text_t name;
    zest_texture texture;
    zest_pipeline_template pipeline_template;
    zest_shader_resources shader_resources;
    zest_uint bindless_texture_index;
    float pixel_range;
    float miter_limit;
    float padding;
    float font_size;
    float max_yoffset;
    zest_uint character_count;
    zest_uint character_offset;
    zest_font_character_t *characters;
} zest_font_t;

typedef struct zest_compute_push_constant_t {
    //z is reserved for the quad_count;
    zest_vec4 parameters;
} zest_compute_push_constant_t;

typedef struct zest_compute_t zest_compute_t;
struct zest_compute_t {
    int magic;
    VkQueue queue;                                            // Separate queue for compute commands (queue family may differ from the one used for graphics)
    zest_recorder recorder;
    VkFence fence[ZEST_MAX_FIF];                              // Synchronization fence to avoid rewriting compute CB if still in use
    VkSemaphore fif_outgoing_semaphore[ZEST_MAX_FIF];         // Signal semaphores
    VkSemaphore fif_incoming_semaphore[ZEST_MAX_FIF];         // Wait semaphores. The compute shader will not be executed on the GPU until these are signalled
    zest_set_layout bindless_layout;               // The bindless descriptor set layout to use in the compute shader
    VkPipelineLayout pipeline_layout;                         // Layout of the compute pipeline
    zest_shader *shaders;                                     // List of compute shaders to use
    VkPipeline pipeline;                                      // Compute pipeline
    zest_descriptor_infos_for_binding_t *descriptor_infos;    // All the buffers/images that are bound to the compute shader
    //zest_uint queue_family_index;                            // Family index of the graphics queue, used for barriers
    zest_uint group_count_x;
    zest_uint group_count_y;
    zest_uint group_count_z;
    zest_compute_push_constant_t push_constants;
    VkPushConstantRange pushConstantRange;
    zest_command_queue_compute compute_commands;

    void *compute_data;                                       // Connect this to any custom data that is required to get what you need out of the compute process.
    zest_render_target render_target;                         // The render target that this compute shader works with
    void *user_data;                                          // Custom user data
    zest_compute_flags flags;
    zest_uint fif;                                            // Used for manual frame in flight compute

    // Over ride the descriptor udpate function with a customised one
    void(*descriptor_update_callback)(zest_compute compute, zest_uint fif);
    // Over ride the command buffer update function with a customised one
    void(*command_buffer_update_callback)(zest_compute compute, VkCommandBuffer command_buffer);
    // Additional cleanup function callback for the extra compute_data you're using
    void(*extra_cleanup_callback)(zest_compute compute);
};

typedef struct zest_compute_builder_t {
    zest_map_descriptor_pool_sizes descriptor_pool_sizes;
    zest_set_layout *non_bindless_layouts;
    zest_set_layout bindless_layout;
    zest_shader *shaders;
    VkPipelineLayoutCreateInfo layout_create_info;
    zest_uint push_constant_size;
    zest_compute_flags flags;
    void *user_data;

    //void(*descriptor_update_callback)(zest_compute compute, zest_uint fif);
    void(*command_buffer_update_callback)(zest_compute compute, VkCommandBuffer command_buffer);
    void(*extra_cleanup_callback)(zest_compute compute);
} zest_compute_builder_t;

//In addition to render passes, you can also run compute shaders by adding this struct to the list of compute items in the command queue
typedef struct zest_command_queue_compute_t zest_command_queue_compute_t;
struct zest_command_queue_compute_t {
    int magic;
    void(*compute_function)(zest_compute compute);             //Call back function which will have the vkcommands to dispatch the compute shader
    int(*condition_function)(zest_compute compute);            //Call back function where you can return true/false as to whether the compute shader should be run each frame
    zest_compute compute;                                      //Handle to the compute object containing the shaders and pipelines etc.
    zest_index shader_index;
    zest_uint last_fif;
    const char *name;
};

typedef struct zest_sampler_t {
    int magic;
    VkSampler vk_sampler;
    VkSamplerCreateInfo create_info;
} zest_sampler_t;

zest_hash_map(zest_descriptor_set_builder_t) zest_map_texture_descriptor_builders;

typedef struct zest_bitmap_t {
    int width;
    int height;
    int channels;
    int stride;
    zest_text_t name;
    size_t size;
    zest_byte *data;
} zest_bitmap_t;

typedef struct zest_bitmap_array_t {
    int width;
    int height;
    int channels;
    int stride;
    const char *name;
    zest_uint size_of_array;
    size_t size_of_each_image;
    size_t total_mem_size;
    zest_byte *data;
} zest_bitmap_array_t;

typedef struct zest_image_t {
    int magic;
    zest_struct_type struct_type;
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
    zest_texture texture;        //Pointer to the texture that this image belongs to
} zest_image_t;

typedef struct zest_imgui_image_t {
    int magic;
    zest_struct_type struct_type;
    zest_image image;
    zest_pipeline_template pipeline;
    zest_shader_resources shader_resources;
    zest_push_constants_t push_constants;
} zest_imgui_image_t;

typedef struct zest_framebuffer_attachment_t {
    VkImage image;
    zest_buffer_t *buffer;
    VkImageView view;
    VkFormat format;
} zest_framebuffer_attachment_t;

typedef struct zest_texture_t {
    int magic;
    zest_struct_type struct_type;
    VkImageLayout image_layout;
    VkFormat image_format;
    VkImageViewType image_view_type;
    VkSamplerCreateInfo sampler_info;
    zest_uint mip_levels;

    zest_text_t name;

    zest_imgui_blendtype imgui_blend_type;
    zest_index image_index;                                    //Tracks the UID of image indexes in the list
    zest_uint packed_border_size;

    zest_uint texture_layer_size;
    zest_image_t texture;
    zest_bitmap_t texture_bitmap;

    // --- GPU data. When changing a texture that is in use, we double buffer then flip when ready
    zest_image_buffer_t image_buffer;
    VkDescriptorImageInfo descriptor_image_info;
    zest_descriptor_set debug_set;          //A descriptor set for simply sampling the texture
    zest_descriptor_set bindless_set;       //A descriptor set for a bindless layout
    zest_sampler sampler;
    zest_uint descriptor_array_index;
    zest_uint binding_number;
    // --- 

    //Todo: option to not keep the images in memory after they're uploaded to the graphics card
    zest_bitmap_t *image_bitmaps;
    zest_image *images;
    zest_bitmap_t *layers;
    zest_bitmap_array_t bitmap_array;
    zest_uint layer_count;
    VkBufferImageCopy *buffer_copy_regions;
    zest_texture_storage_type storage_type;

    zest_uint color_channels;

    zest_texture_flags flags;
    zest_thread_access lock;
    void *user_data;
    void(*reprocess_callback)(zest_texture texture, void *user_data);
    void(*cleanup_callback)(zest_texture texture, void *user_data);
} zest_texture_t;

typedef struct zest_shader_t {
    int magic;
    char *spv;
    zest_text_t file_path;
    zest_text_t shader_code;
    zest_text_t name;
    shaderc_shader_kind type;
} zest_shader_t;

typedef struct zest_render_target_create_info_t {
    VkRect2D viewport;                                           //The viewport/size of the render target
    zest_vec2 ratio_of_screen_size;                              //If you want the size of the target to be a ration of the current swap chain/window size
    VkFormat render_format;                                      //Always set to the swap chain render format
    zest_imgui_blendtype imgui_blend_type;                       //Set the blend type for imgui. useful to change this if you're rendering this render target to an imgui window
    VkSamplerAddressMode sampler_address_mode_u;
    VkSamplerAddressMode sampler_address_mode_v;
    VkSamplerAddressMode sampler_address_mode_w;
    zest_render_target_flags flags;
    zest_render_target input_source;
    zest_pipeline_template pipeline;
    int mip_levels;
} zest_render_target_create_info_t;

typedef struct zest_recorder_t {
    int magic;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer[ZEST_MAX_FIF];
    zest_uint outdated[ZEST_MAX_FIF];
    zest_draw_routine draw_routine;
    zest_command_buffer_flags flags;
} zest_recorder_t;

typedef struct zest_render_target_t {
    int magic;
    const char *name;

    void* push_constants;
    zest_render_target_create_info_t create_info;

    void(*post_process_record_callback)(zest_render_target_t *target, void *user_data, zest_uint fif);
    void *post_process_user_data;

    VkRenderPass render_pass;
    zest_render_pass_info_t render_pass_info;
    VkRect2D viewport;
    zest_vec4 cls_color;
    VkRect2D render_viewport;
    zest_render_target input_source;
    int render_width, render_height;
    int mip_levels;
    VkRect2D *mip_level_sizes;
    VkFormat render_format;

    zest_render_target_flags flags;

    VkSamplerCreateInfo sampler_info;
    zest_image_t sampler_image;
    int current_index;
    VkSampler sampler[2];
    zest_descriptor_pool descriptor_pool[2];
    zest_descriptor_set_t sampler_descriptor_set[2][ZEST_MAX_FIF];
    zest_descriptor_set_t *mip_level_samplers[2][ZEST_MAX_FIF];
    zest_descriptor_set_t *mip_level_descriptor_sets[2][ZEST_MAX_FIF];
    VkDescriptorImageInfo *mip_level_image_infos[ZEST_MAX_FIF];
    zest_pipeline_template pipeline_template;

    zest_pipeline_template filter_pipeline_template;
    zest_shader_resources filter_shader_resources;
    zest_set_layout filter_descriptor_layout;
    zest_descriptor_set_t filter_descriptor_set;
    zest_mip_push_constants_t mip_push_constants;
    zest_render_target *composite_layers;

    int frames_in_flight;
    int frame_buffer_dirty[ZEST_MAX_FIF];
    VkDescriptorImageInfo image_info[ZEST_MAX_FIF];
    zest_frame_buffer_t framebuffers[ZEST_MAX_FIF];

    zest_recorder recorder;     //for post processing
    zest_thread_access lock;
} zest_render_target_t;

typedef struct zest_debug_render_pass_t {
    zest_render_target render_target;
    zest_command_queue_draw_commands draw_commands;
} zest_debug_render_pass_t;

typedef struct zest_debug_t {
    zest_uint function_depth;
    zest_log_entry_t *frame_log;
} zest_debug_t;

zest_hash_map(zest_command_queue) zest_map_command_queues;
zest_hash_map(VkRenderPass) zest_map_render_passes;
zest_hash_map(zest_set_layout) zest_map_descriptor_layouts;
zest_hash_map(zest_pipeline_template) zest_map_pipelines;
zest_hash_map(zest_pipeline) zest_map_cached_pipelines;
zest_hash_map(zest_command_queue_draw_commands) zest_map_command_queue_draw_commands;
zest_hash_map(zest_command_queue_compute) zest_map_command_queue_computes;
zest_hash_map(zest_draw_routine) zest_map_draw_routines;
zest_hash_map(zest_buffer_allocator) zest_map_buffer_allocators;
zest_hash_map(zest_layer) zest_map_layers;
zest_hash_map(zest_descriptor_buffer) zest_map_uniform_buffers;
zest_hash_map(zest_texture) zest_map_textures;
zest_hash_map(zest_sampler) zest_map_samplers;
zest_hash_map(zest_render_target) zest_map_render_targets;
zest_hash_map(zest_font) zest_map_fonts;
zest_hash_map(zest_compute) zest_map_computes;
zest_hash_map(zest_shader) zest_map_shaders;
zest_hash_map(zest_render_target) zest_map_rt_refresh_queue;
zest_hash_map(zest_render_target) zest_map_rt_recreate_queue;
zest_hash_map(zest_descriptor_pool) zest_map_descriptor_pool;
zest_hash_map(zest_render_graph) zest_map_render_graph;

typedef struct zest_command_buffer_pools_t {
    VkCommandPool graphics_command_pool;
    VkCommandPool compute_command_pool;
    VkCommandPool transfer_command_pool;
    VkCommandBuffer *free_graphics;
    VkCommandBuffer *free_compute;
    VkCommandBuffer *free_transfer;
    VkCommandBuffer *graphics;
    VkCommandBuffer *compute;
    VkCommandBuffer *transfer;
} zest_command_buffer_pools_t;

typedef struct zest_renderer_t {
    VkSemaphore *render_finished_semaphore;
    VkSemaphore image_available_semaphore[ZEST_MAX_FIF];

    VkSemaphore *semaphore_pool;
    VkSemaphore *free_semaphores;

    zest_u64 total_frame_count;

    zest_command_buffer_pools_t command_buffers;
    
    zest_uint swapchain_image_count;
    VkFormat swapchain_image_format;
    VkExtent2D swapchain_extent;
    VkExtent2D window_extent;
    float dpi_scale;
    VkSwapchainKHR swapchain;

    VkFence fif_fence[ZEST_MAX_FIF];
    zest_uniform_buffer uniform_buffer;

    zest_set_layout global_bindless_set_layout;
    zest_descriptor_set global_set;

    VkImage *swapchain_images;
    VkImageView *swapchain_image_views;
    VkFramebuffer *swapchain_frame_buffers;
    zest_buffer_t *depth_resource_buffer;

    VkRenderPass final_render_pass;
    zest_image_buffer_t final_render_pass_depth_attachment;
    zest_render_target_push_constants_t push_constants;
    VkDescriptorBufferInfo view_buffer_info[ZEST_MAX_FIF];

    VkPipelineCache pipeline_cache;

    zest_map_buffer_allocators buffer_allocators;

    VkCommandBuffer utility_command_buffer[ZEST_MAX_FIF];

    //Context data
    zest_render_graph current_render_graph;
    zest_render_graph *render_graphs;       //All the render graphs used this frame. Gets cleared at the beginning of each frame
    zest_command_queue_draw_commands current_draw_commands;
    zest_command_queue_compute current_compute_routine;

    //Linear allocators for building the render graph each frame
    zloc_linear_allocator_t *render_graph_allocator;

    //General Storage
    zest_map_command_queues command_queues;
    zest_map_render_passes render_passes;
    zest_map_descriptor_layouts descriptor_layouts;
    zest_map_pipelines pipelines;
    zest_map_cached_pipelines cached_pipelines;
    zest_map_command_queue_draw_commands command_queue_draw_commands;
    zest_map_command_queue_computes command_queue_computes;
    zest_map_draw_routines draw_routines;
    zest_map_layers layers;
    zest_map_textures textures;
    zest_map_render_targets render_targets;
    zest_map_fonts fonts;
    zest_uniform_buffer *uniform_buffers;
    zest_map_computes computes;
    zest_map_shaders shaders;
    zest_map_samplers samplers;

    zest_command_queue active_command_queue;
    zest_command_queue_t empty_queue;
    zest_command_setup_context_t setup_context;

    zest_render_target current_render_target;
    zest_render_target root_render_target;

    //For scheduled tasks
    zest_map_rt_refresh_queue render_target_recreate_queue;
    zest_map_rt_recreate_queue rt_sampler_refresh_queue;
    zest_texture *texture_refresh_queue[ZEST_MAX_FIF];
    zest_buffer *staging_buffers;
    zest_map_textures texture_reprocess_queue;
    zest_texture *texture_cleanup_queue;
    zest_pipeline *pipeline_recreate_queue;
    zest_pipeline_handles_t *pipeline_destroy_queue;
    zest_uint current_image_frame;
    zest_destruction_queue_t deferred_resource_freeing_list;
	VkFramebuffer *old_frame_buffers[ZEST_MAX_FIF];             //For clearing up frame buffers from previous frames that aren't needed anymore
    VkSemaphore *used_semaphores[ZEST_MAX_FIF];                 //For returning to the semaphore pool after a render graph is finished with them from the previous frame
    VkSemaphore *used_timeline_semaphores[ZEST_MAX_FIF];        //For returning to the semaphore pool after a render graph is finished with them from the previous frame
    VkCommandBuffer *used_graphics_command_buffers[ZEST_MAX_FIF];
    VkCommandBuffer *used_compute_command_buffers[ZEST_MAX_FIF];
    VkCommandBuffer *used_transfer_command_buffers[ZEST_MAX_FIF];

    //Each texture has a simple descriptor set with a combined image sampler for debugging purposes
    //allocated from this pool and using the layout
    zest_set_layout texture_debug_layout;

    //Threading
    zest_work_queue_t work_queue;

    //Flags
    zest_renderer_flags flags;
    
    //Optional prefix path for loading shaders
    zest_text_t shader_path_prefix;

    //Debugging
    zest_debug_t debug;

    //Imgui
    zest_imgui_t imgui_info;

    //Callbacks for customising window and surface creation
    void(*get_window_size_callback)(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
    void(*destroy_window_callback)(void *user_data);
    void(*poll_events_callback)(ZEST_PROTOTYPE);
    void(*add_platform_extensions_callback)(ZEST_PROTOTYPE);
    zest_window(*create_window_callback)(int x, int y, int width, int height, zest_bool maximised, const char* title);
    void(*create_window_surface_callback)(zest_window window);

} zest_renderer_t;

extern zest_device_t *ZestDevice;
extern zest_app_t *ZestApp;
extern zest_renderer_t *ZestRenderer;

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
ZEST_PRIVATE void zest__os_create_window_surface(zest_window window);
ZEST_PRIVATE void zest__os_poll_events(ZEST_PROTOTYPE);
ZEST_PRIVATE void zest__os_add_platform_extensions(ZEST_PROTOTYPE);
ZEST_PRIVATE void zest__os_set_window_title(const char *title);
ZEST_PRIVATE bool zest__create_folder(const char *path);
//-- End Platform dependent functions

//Only available outside lib for some implementations like SDL2
ZEST_API void* zest__vec_reserve(void *T, zest_uint unit_size, zest_uint new_capacity);
ZEST_PRIVATE void* zest__vec_linear_reserve(zloc_linear_allocator_t *allocator, void *T, zest_uint unit_size, zest_uint new_capacity);

//Buffer_and_Memory_Management
ZEST_PRIVATE void zest__add_host_memory_pool(zest_size size);
ZEST_PRIVATE void *zest__allocate(zest_size size);
ZEST_PRIVATE void *zest__allocate_aligned(zest_size size, zest_size alignment);
ZEST_PRIVATE void *zest__reallocate(void *memory, zest_size size);
ZEST_PRIVATE VkResult zest__bind_memory(zest_device_memory_pool memory_allocation, VkDeviceSize offset);
ZEST_PRIVATE VkResult zest__map_memory(zest_device_memory_pool memory_allocation, VkDeviceSize size, VkDeviceSize offset);
ZEST_PRIVATE void zest__unmap_memory(zest_device_memory_pool memory_allocation);
ZEST_PRIVATE void zest__destroy_memory(zest_device_memory_pool memory_allocation);
ZEST_PRIVATE VkResult zest__flush_memory(zest_device_memory_pool memory_allocation, VkDeviceSize size, VkDeviceSize offset);
ZEST_PRIVATE zest_device_memory_pool zest__create_vk_memory_pool(zest_buffer_info_t *buffer_info, VkImage image, zest_key key, zest_size size);
ZEST_PRIVATE void zest__add_remote_range_pool(zest_buffer_allocator buffer_allocator, zest_device_memory_pool buffer_pool);
ZEST_PRIVATE void zest__set_buffer_details(zest_buffer_allocator buffer_allocator, zest_buffer_t *buffer, zest_bool is_host_visible);
ZEST_PRIVATE void zest__buffer_write_barrier(VkCommandBuffer command_buffer, zest_buffer buffer);
//End Buffer Management

//Renderer_functions
ZEST_PRIVATE void zest__initialise_renderer(zest_create_info_t *create_info);
ZEST_PRIVATE void zest__create_swapchain(void);
ZEST_PRIVATE VkSurfaceFormatKHR zest__choose_swapchain_format(VkSurfaceFormatKHR *availableFormats);
ZEST_PRIVATE VkPresentModeKHR zest_choose_present_mode(VkPresentModeKHR *available_present_modes, zest_bool use_vsync);
ZEST_PRIVATE VkExtent2D zest_choose_swap_extent(VkSurfaceCapabilitiesKHR *capabilities);
ZEST_PRIVATE void zest__create_pipeline_cache();
ZEST_PRIVATE void zest__get_window_size_callback(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
ZEST_PRIVATE void zest__destroy_window_callback(void *user_data);
ZEST_PRIVATE void zest__cleanup_swapchain(void);
ZEST_PRIVATE void zest__cleanup_device(void);
ZEST_PRIVATE void zest__cleanup_renderer(void);
ZEST_PRIVATE void zest__cleanup_draw_routines(void);
ZEST_PRIVATE void zest__clean_up_compute(zest_compute compute);
ZEST_PRIVATE void zest__recreate_swapchain(void);
ZEST_PRIVATE void zest__add_draw_routine(zest_command_queue_draw_commands command_queue_draw, zest_draw_routine draw_routine);
ZEST_PRIVATE void zest__create_swapchain_image_views(void);
ZEST_PRIVATE VkRenderPass zest__get_render_pass_with_info(zest_render_pass_info_t info);
ZEST_PRIVATE VkRenderPass zest__get_render_pass(VkFormat render_format, VkImageLayout initial_layout, VkImageLayout final_layout, VkAttachmentLoadOp load_op, zest_bool depth_buffer);
ZEST_PRIVATE zest_buffer_t *zest__create_depth_resources(void);
ZEST_PRIVATE void zest__create_swap_chain_frame_buffers(zest_bool depth_buffer);
ZEST_PRIVATE void zest__create_sync_objects(void);
ZEST_PRIVATE void zest__create_command_buffer_pools(void);
ZEST_PRIVATE VkSemaphore zest__acquire_semaphore(void);
ZEST_PRIVATE VkCommandBuffer zest__acquire_graphics_command_buffer(VkCommandBufferLevel level);
ZEST_PRIVATE VkCommandBuffer zest__acquire_compute_command_buffer(VkCommandBufferLevel level);
ZEST_PRIVATE VkCommandBuffer zest__acquire_transfer_command_buffer(VkCommandBufferLevel level);
ZEST_PRIVATE zest_uniform_buffer zest__add_uniform_buffer(zest_uniform_buffer buffer);
ZEST_PRIVATE void zest__add_line(zest_text_t *text, char current_char, zest_uint *position, zest_uint tabs);
ZEST_PRIVATE void zest__format_shader_code(zest_text_t *code);
ZEST_PRIVATE void zest__compile_builtin_shaders(zest_bool compile_shaders);
ZEST_PRIVATE void zest__create_debug_layout_and_pool(zest_uint max_texture_count);
ZEST_PRIVATE void zest__prepare_standard_pipelines(void);
ZEST_PRIVATE void zest__create_empty_command_queue(zest_command_queue command_queue);
ZEST_PRIVATE void zest__render_blank(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer);
ZEST_PRIVATE void zest__cleanup_pipelines(void);
ZEST_PRIVATE void zest__cleanup_textures(void);
ZEST_PRIVATE void zest__cleanup_framebuffer(zest_frame_buffer_t *frame_buffer);
ZEST_PRIVATE void zest__refresh_pipeline_template(zest_pipeline_template pipeline);
ZEST_PRIVATE void zest__rebuild_pipeline(zest_pipeline pipeline);
ZEST_PRIVATE void zest__present_frame(void);
ZEST_PRIVATE void zest__dummy_submit_fence_only(void);
ZEST_PRIVATE void zest__dummy_submit_for_present_only(void);
ZEST_PRIVATE void zest__draw_renderer_frame(void);
// --End Renderer functions

// --Command_Queue_functions
ZEST_PRIVATE void zest__cleanup_command_queue(zest_command_queue command_queue);
ZEST_PRIVATE zest_command_queue_draw_commands zest__create_command_queue_draw_commands(const char *name);
ZEST_PRIVATE void zest__update_command_queue_viewports(void);
ZEST_PRIVATE void zest__reset_query_pool(VkCommandBuffer command_buffer, VkQueryPool query_pool, zest_uint count);
ZEST_PRIVATE void zest__connect_command_queue_to_present(void);
// --End Command Queue functions

// --Command_Queue_Setup_functions
ZEST_PRIVATE zest_command_queue zest__create_command_queue(const char *name);
ZEST_PRIVATE void zest__set_queue_context(zest_setup_context_type context);
ZEST_PRIVATE zest_draw_routine zest__create_draw_routine_with_mesh_layer(const char *name);
ZEST_PRIVATE VkQueryPool zest__create_query_pool(zest_uint timestamp_count);

// --Draw_layer_internal_functions
ZEST_PRIVATE void zest__start_mesh_instructions(zest_layer layer);
ZEST_PRIVATE void zest__end_mesh_instructions(zest_layer layer);
ZEST_PRIVATE void zest__update_instance_layer_buffers_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);
ZEST_PRIVATE void zest__update_instance_layer_resolution(zest_layer layer);
ZEST_PRIVATE void zest__record_mesh_layer(zest_layer layer, zest_uint fif);
ZEST_PRIVATE zest_layer_instruction_t zest__layer_instruction(void);
ZEST_PRIVATE void zest__reset_mesh_layer_drawing(zest_layer layer);
ZEST_PRIVATE zest_bool zest__grow_instance_buffer(zest_layer layer, zest_size type_size, zest_size minimum_size);

// --Texture_internal_functions
ZEST_PRIVATE zest_index zest__texture_image_index(zest_texture texture);
ZEST_PRIVATE float zest__copy_animation_frames(zest_texture texture, zest_bitmap_t *spritesheet, int width, int height, zest_uint frames, zest_bool row_by_row);
ZEST_PRIVATE void zest__delete_texture_layers(zest_texture texture);
ZEST_PRIVATE void zest__generate_mipmaps(VkImage image, VkFormat image_format, int32_t texture_width, int32_t texture_height, zest_uint mip_levels, zest_uint layer_count, VkImageLayout image_layout, VkCommandBuffer cb);
ZEST_PRIVATE void zest__create_texture_image(zest_texture texture, zest_uint mip_levels, VkImageUsageFlags usage_flags, VkImageLayout image_layout, zest_bool copy_bitmap, VkCommandBuffer command_buffer);
ZEST_PRIVATE void zest__create_texture_image_array(zest_texture texture, zest_uint mip_levels, VkCommandBuffer command_buffer);
ZEST_PRIVATE zest_byte zest__calculate_texture_layers(stbrp_rect *rects, zest_uint size, const zest_uint node_count);
ZEST_PRIVATE void zest__make_image_bank(zest_texture texture, zest_uint size);
ZEST_PRIVATE void zest__make_sprite_sheet(zest_texture texture);
ZEST_PRIVATE void zest__pack_images(zest_texture texture, zest_uint size);
ZEST_PRIVATE void zest__update_image_vertices(zest_image image);
ZEST_PRIVATE void zest__update_texture_single_image_meta(zest_texture texture, zest_uint width, zest_uint height);
ZEST_PRIVATE void zest__create_texture_image_view(zest_texture texture, VkImageViewType view_type, zest_uint mip_levels, zest_uint layer_count);
ZEST_PRIVATE void zest__process_texture_images(zest_texture texture, VkCommandBuffer command_buffer);
ZEST_PRIVATE void zest__create_texture_debug_set(zest_texture texture);

// --Render_target_internal_functions
ZEST_PRIVATE void zest__initialise_render_target(zest_render_target render_target, zest_render_target_create_info_t *info);
ZEST_PRIVATE void zest__create_render_target_sampler(zest_render_target render_target);
ZEST_PRIVATE void zest__create_mip_level_render_target_samplers(zest_render_target render_target);
ZEST_PRIVATE void zest__refresh_render_target_sampler(zest_render_target render_target);
ZEST_PRIVATE void zest__refresh_render_target_mip_samplers(zest_render_target render_target);
ZEST_PRIVATE void zest__record_render_target_commands(zest_render_target render_target, zest_index fif);
ZEST_PRIVATE void zest__render_target_maintennance();

// --General_layer_internal_functions
ZEST_PRIVATE zest_layer zest__create_instance_layer(const char *name, zest_size instance_type_size, zest_uint initial_instance_count);

// --Font_layer_internal_functions
ZEST_PRIVATE void zest__setup_font_texture(zest_font font);
ZEST_PRIVATE zest_font zest__add_font(zest_font font);

// --Mesh_layer_internal_functions
ZEST_PRIVATE void zest__draw_mesh_layer_callback(struct zest_work_queue_t *queue, void *data);
ZEST_PRIVATE void zest__initialise_mesh_layer(zest_layer mesh_layer, zest_size vertex_struct_size, zest_size initial_vertex_capacity);
ZEST_PRIVATE void zest__draw_instance_mesh_layer_callback(struct zest_work_queue_t *queue, void *data);

// --General_Helper_Functions
ZEST_PRIVATE VkImageView zest__create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, zest_uint mip_levels_this_view, zest_uint base_mip, VkImageViewType viewType, zest_uint layerCount);
ZEST_PRIVATE void zest__create_temporary_image(zest_uint width, zest_uint height, zest_uint mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image, VkDeviceMemory *memory);
ZEST_PRIVATE zest_buffer zest__create_image(zest_uint width, zest_uint height, zest_uint mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image);
ZEST_PRIVATE zest_buffer zest__create_image_array(zest_uint width, zest_uint height, zest_uint mipLevels, zest_uint layers, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image);
ZEST_PRIVATE void zest__create_transient_image(zest_resource_node node);
ZEST_PRIVATE void zest__create_transient_buffer(zest_resource_node node);
ZEST_PRIVATE void zest__copy_buffer_to_image(VkBuffer buffer, VkDeviceSize src_offset, VkImage image, zest_uint width, zest_uint height, VkImageLayout image_layout, VkCommandBuffer command_buffer);
ZEST_PRIVATE void zest__copy_buffer_regions_to_image(VkBufferImageCopy *regions, VkBuffer buffer, VkDeviceSize src_offset, VkImage image, VkCommandBuffer command_buffer);
ZEST_PRIVATE void zest__transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, zest_uint mipLevels, zest_uint layerCount, VkCommandBuffer command_buffer);
ZEST_PRIVATE VkImageMemoryBarrier zest__create_image_memory_barrier(VkImage image, VkAccessFlags from_access, VkAccessFlags to_access, VkImageLayout from_layout, VkImageLayout to_layout, zest_uint target_mip_level, zest_uint mip_count);
ZEST_PRIVATE VkBufferMemoryBarrier zest__create_buffer_memory_barrier( VkBuffer buffer, VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask, VkDeviceSize offset, VkDeviceSize size);
ZEST_PRIVATE void zest__place_fragment_barrier(VkCommandBuffer command_buffer, VkImageMemoryBarrier *barrier);
ZEST_PRIVATE void zest__place_image_barrier(VkCommandBuffer command_buffer, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage, VkImageMemoryBarrier *barrier);
ZEST_PRIVATE VkRenderPass zest__create_render_pass(VkFormat render_format, VkImageLayout initial_layout, VkImageLayout final_layout, VkAttachmentLoadOp load_op, zest_bool depth_buffer);
ZEST_PRIVATE VkSampler zest__create_sampler(VkSamplerCreateInfo samplerInfo);
ZEST_PRIVATE VkFormat zest__find_depth_format(void);
ZEST_PRIVATE zest_bool zest__has_stencil_format(VkFormat format);
ZEST_PRIVATE VkFormat zest__find_supported_format(VkFormat *candidates, zest_uint candidates_count, VkImageTiling tiling, VkFormatFeatureFlags features);
ZEST_PRIVATE VkCommandBuffer zest__begin_single_time_commands(void);
ZEST_PRIVATE void zest__end_single_time_commands(VkCommandBuffer command_buffer);
ZEST_PRIVATE zest_index zest__next_fif(void);
// --End General Helper Functions

// --Pipeline_Helper_Functions
ZEST_PRIVATE void zest__set_pipeline_template(zest_pipeline_template pipeline_template);
ZEST_PRIVATE void zest__update_pipeline_template(zest_pipeline_template pipeline_template);
ZEST_PRIVATE VkShaderModule zest__create_shader_module(char *code);
ZEST_PRIVATE zest_pipeline zest__create_pipeline(void);
ZEST_PRIVATE zest_pipeline zest__cache_pipeline(zest_pipeline_template pipeline_template, VkRenderPass render_pass);
// --End Pipeline Helper Functions

// --Buffer_allocation_funcitons
ZEST_PRIVATE void zest__create_device_memory_pool(VkDeviceSize size, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, zest_device_memory_pool buffer, const char *name);
ZEST_PRIVATE void zest__create_image_memory_pool(VkDeviceSize size_in_bytes, VkImage image, VkMemoryPropertyFlags property_flags, zest_device_memory_pool buffer);
ZEST_PRIVATE zest_size zest__get_minimum_block_size(zest_size pool_size);
ZEST_PRIVATE void zest__on_add_pool(void *user_data, void *block);
ZEST_PRIVATE void zest__on_split_block(void *user_data, zloc_header* block, zloc_header *trimmed_block, zest_size remote_size);
// --End Buffer allocation funcitons

// --Maintenance_functions
ZEST_PRIVATE void zest__delete_texture(zest_texture texture);
ZEST_PRIVATE void zest__delete_font(zest_font_t *font);
ZEST_PRIVATE void zest__cleanup_texture(zest_texture texture);
ZEST_PRIVATE void zest__free_all_texture_images(zest_texture texture);
ZEST_PRIVATE void zest__reindex_texture_images(zest_texture texture);
// --End Maintenance functions

// --Descriptor_set_functions
ZEST_PRIVATE zest_descriptor_pool zest__create_descriptor_pool(zest_uint max_sets);
ZEST_PRIVATE zest_set_layout zest__add_descriptor_set_layout(const char *name, VkDescriptorSetLayout layout);
ZEST_PRIVATE bool zest__binding_exists_in_layout_builder(zest_set_layout_builder_t *builder, zest_uint binding);
ZEST_PRIVATE VkDescriptorSetLayoutBinding *zest__get_layout_binding_info(zest_set_layout layout, zest_uint binding_index);
ZEST_PRIVATE zest_uint zest__acquire_bindless_index(zest_set_layout layout_handle, zest_uint binding_number);
ZEST_PRIVATE void zest__release_bindless_index(zest_set_layout layout_handle, zest_uint binding_number, zest_uint index_to_release);
// --End Descriptor set functions

// --Device_set_up
ZEST_PRIVATE zest_bool zest__create_instance();
ZEST_PRIVATE void zest__setup_validation(void);
ZEST_PRIVATE VKAPI_ATTR VkBool32 VKAPI_CALL zest_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
ZEST_PRIVATE VkResult zest_create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
ZEST_PRIVATE void zest_destroy_debug_messenger(void);
ZEST_PRIVATE void zest__pick_physical_device(void);
ZEST_PRIVATE zest_bool zest__is_device_suitable(VkPhysicalDevice physical_device);
ZEST_PRIVATE zest_bool zest__device_is_discrete_gpu(VkPhysicalDevice physical_device);
ZEST_PRIVATE void zest__log_device_name(VkPhysicalDevice physical_device);
ZEST_PRIVATE zest_queue_family_indices zest__find_queue_families(VkPhysicalDevice physical_device, VkDeviceQueueCreateInfo *queue_create_infos);
ZEST_PRIVATE zest_bool zest__check_device_extension_support(VkPhysicalDevice physical_device);
ZEST_PRIVATE zest_swapchain_support_details_t zest__query_swapchain_support(VkPhysicalDevice physical_device);
ZEST_PRIVATE VkSampleCountFlagBits zest__get_max_useable_sample_count(void);
ZEST_PRIVATE void zest__create_logical_device();
ZEST_PRIVATE void zest__set_limit_data(void);
ZEST_PRIVATE zest_bool zest__check_validation_layer_support(void);
ZEST_PRIVATE void zest__get_required_extensions();
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
ZEST_PRIVATE zest_bool zest__initialise_device();
ZEST_PRIVATE void zest__destroy(void);
ZEST_PRIVATE void zest__main_loop(void);
ZEST_PRIVATE zest_microsecs zest__set_elapsed_time(void);
ZEST_PRIVATE zest_bool zest__validation_layers_are_enabled(void);
ZEST_PRIVATE zest_bool zest__validation_layers_with_sync_are_enabled(void);
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
//Set the pool count for a descriptor type in a zest_create_info_t. Do this for any pools that you want to be different to the default. You only
//need to call this if you're running out of descriptor pool space for a specific VkDescriptorType which you maybe doing if you have a lot of custom
//draw routines
ZEST_API void zest_SetDescriptorPoolCount(zest_create_info_t *info, VkDescriptorType descriptor_type, zest_uint count);
//Initialise Zest. You must call this in order to use Zest. Use zest_CreateInfo() to set up some default values to initialise the renderer.
ZEST_API zest_bool zest_Initialise(zest_create_info_t *info);
//Set the custom user data which will get passed through to the user update function each frame.
ZEST_API void zest_SetUserData(void* data);
//Set the user udpate callback that will be called each frame in the main loop of zest. You must set this or the main loop will just render a blank screen.
ZEST_API void zest_SetUserUpdateCallback(void(*callback)(zest_microsecs, void*));
//Start the main loop in the zest renderer. Must be run after zest_Initialise and also zest_SetUserUpdateCallback
ZEST_API void zest_Start(void);

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
ZEST_API void zest_CreateDescriptorPoolForLayout(zest_set_layout layout, zest_uint max_set_count, VkDescriptorPoolCreateFlags pool_flags);
ZEST_API void zest_CreateDescriptorPoolForLayoutBindless(zest_set_layout layout, zest_uint max_set_count, VkDescriptorPoolCreateFlags pool_flags);
//Reset a descriptor pool. This invalidates all descriptor sets current allocated from the pool so you can reallocate them.
ZEST_API void zest_ResetDescriptorPool(zest_descriptor_pool pool);
//Destroys the VkDescriptorPool and frees the zest_descriptor_pool and all contents
ZEST_API void zest_FreeDescriptorPool(zest_descriptor_pool pool);
//Create a descriptor layout builder object that you can use with the AddBuildLayout commands to put together more complex/fine tuned descriptor
//set layouts
ZEST_API zest_set_layout_builder_t zest_BeginSetLayoutBuilder();
//Set the create flags that you need for teh descriptor layout that you're building
void zest_SetLayoutBuilderCreateFlags(zest_set_layout_builder_t *builder, VkDescriptorSetLayoutCreateFlags flags);

ZEST_API void zest_AddLayoutBuilderBindingBindless( zest_set_layout_builder_t *builder, zest_uint binding_number, VkDescriptorType descriptor_type, zest_uint descriptor_count, zest_supported_shader_stages stage_flags, VkDescriptorBindingFlags binding_flags, const VkSampler *p_immutable_samplers);
//Add a descriptor set layout binding to a layout builder for a sampler in the fragment shader.
ZEST_API void zest_AddLayoutBuilderSamplerBindless(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint max_texture_count);
//Add a descriptor set layout binding to a layout builder for a sampled image in the fragment shader.
ZEST_API void zest_AddLayoutBuilderSampledImageBindless(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint max_texture_count);
//Add a descriptor set layout binding to a layout builder for a sampled image in the fragment shader.
ZEST_API void zest_AddLayoutBuilderCombinedImageSamplerBindless(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint max_texture_count);
//Add a descriptor set layout binding to a layout builder for a sampled image in the fragment shader.
ZEST_API void zest_AddLayoutBuilderUniformBufferBindless(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint max_buffer_count, zest_supported_shader_stages shader_stages);
//Add a descriptor set layout binding to a layout builder for a sampled image in the fragment shader.
ZEST_API void zest_AddLayoutBuilderStorageBufferBindless(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint max_buffer_count, zest_supported_shader_stages shader_stages);

ZEST_API void zest_AddLayoutBuilderBinding(zest_set_layout_builder_t *builder, zest_uint binding_number, VkDescriptorType descriptor_type, zest_uint descriptor_count, zest_supported_shader_stages stage_flags, const VkSampler *p_immutable_samplers);
//Add a descriptor set layout binding to a layout builder for a sampler in the fragment shader.
ZEST_API void zest_AddLayoutBuilderSampler(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint descriptor_count);
//Add a descriptor set layout binding to a layout builder for a sampled image in the fragment shader.
ZEST_API void zest_AddLayoutBuilderSampledImage(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint descriptor_count);
//Add a descriptor set layout binding to a layout builder for a sampled image in the fragment shader.
ZEST_API void zest_AddLayoutBuilderCombinedImageSampler(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint descriptor_count);
//Add a descriptor set layout binding to a layout builder for a sampled image in the fragment shader.
ZEST_API void zest_AddLayoutBuilderUniformBuffer(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint descriptor_count, zest_supported_shader_stages shader_stages);
//Add a descriptor set layout binding to a layout builder for a sampled image in the fragment shader.
ZEST_API void zest_AddLayoutBuilderStorageBuffer(zest_set_layout_builder_t *builder, zest_uint binding_number, zest_uint descriptor_count, zest_supported_shader_stages shader_stages);

//Build the descriptor set layout and add it to the renderer. This is for large global descriptor set layouts
ZEST_API zest_set_layout zest_FinishDescriptorSetLayoutForBindless(zest_set_layout_builder_t *builder, zest_uint num_global_sets_this_pool_should_support, VkDescriptorPoolCreateFlags additional_pool_flags, const char *name, ...);
//Build the descriptor set layout and add it to the render. The layout is also returned from the function.
ZEST_API zest_set_layout zest_FinishDescriptorSetLayout(zest_set_layout_builder_t *builder, const char *name, ...);
//Create a simple global layout for use that includes uniform buffer and combined image sampler. Enough for simple

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
ZEST_API zest_descriptor_set_builder_t zest_BeginDescriptorSetBuilder(zest_set_layout layout);
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
//Add a combined image and sampler to a descriptor set builder using a texture
ZEST_API void zest_AddSetBuilderTexture(zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_texture texture);
//Add a VkDescriptorImageInfo from a zest_texture (or render target) to a descriptor set builder.
ZEST_API void zest_AddSetBuilderDirectImageWrite(zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, VkDescriptorType type, const VkDescriptorImageInfo *image_info);
//Add a VkDescriptorBufferInfo from a zest_descriptor_buffer to a descriptor set builder as a uniform buffer.
ZEST_API void zest_AddSetBuilderUniformBuffer(zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_uniform_buffer uniform_buffer, zest_uint fif);
//Add a VkDescriptorBufferInfo from a zest_descriptor_buffer to a descriptor set builder as a storage buffer.
ZEST_API void zest_AddSetBuilderStorageBuffer( zest_descriptor_set_builder_t *builder, zest_uint dst_binding, zest_uint dst_array_element, zest_descriptor_buffer storage_buffer);
//Build a zest_descriptor_set_t using a builder that you made using the AddBuilder command. The layout that you pass to this function must be configured properly.
//zest_descriptor_set_t will contain a VkDescriptorSet for each frame in flight as well as descriptor writes used to create the set.
ZEST_API zest_descriptor_set zest_FinishDescriptorSet(zest_descriptor_pool pool, zest_descriptor_set_builder_t *builder, zest_descriptor_set new_set_to_populate_or_update);
ZEST_API zest_descriptor_set zest_CreateBindlessSet(zest_set_layout layout);
ZEST_API zest_uint zest_AcquireBindlessStorageBufferIndex(zest_descriptor_buffer buffer, zest_set_layout layout, zest_descriptor_set set, zest_uint target_binding_number);
ZEST_API zest_uint zest_AcquireBindlessTextureIndex(zest_texture texture, zest_set_layout layout, zest_descriptor_set set, zest_uint target_binding_number);
ZEST_API zest_uint zest_AcquireGlobalCombinedImageSampler(zest_texture texture);
ZEST_API zest_uint zest_AcquireGlobalSampledImage(zest_texture texture);
ZEST_API zest_uint zest_AcquireGlobalSampler(zest_texture texture);
ZEST_API zest_uint zest_AcquireGlobalStorageBufferIndex(zest_descriptor_buffer buffer);
ZEST_API void zest_ReleaseGlobalStorageBufferIndex(zest_descriptor_buffer buffer);
ZEST_API void zest_ReleaseGlobalTextureIndex(zest_texture texture);
ZEST_API void zest_ReleaseGlobalSamplerIndex(zest_uint index);
ZEST_API void zest_ReleaseGlobalSampledImageIndex(zest_uint index);
ZEST_API void zest_ReleaseBindlessIndex(zest_uint index, zest_uint binding_number);
ZEST_API VkDescriptorSet zest_vk_GetGlobalBindlessSet();
ZEST_API zest_descriptor_set zest_GetGlobalBindlessSet();
ZEST_API VkDescriptorSetLayout zest_vk_GetGlobalBindlessLayout();
ZEST_API zest_set_layout zest_GetGlobalBindlessLayout();
ZEST_API VkDescriptorSet zest_vk_GetGlobalUniformBufferDescriptorSet();
//Create a new descriptor set shader_resources
ZEST_API zest_shader_resources zest_CreateShaderResources();
//Add a descriptor set to a descriptor set shader_resources. Bundles are used for binding to a draw call so the descriptor sets can be passed in to the shaders
//according to their set and binding number. So therefore it's important that you add the descriptor sets to the shader_resources in the same order
//that you set up the descriptor set layouts. You must also specify the frame in flight for the descriptor set that you're addeding.
//Use zest_AddDescriptorSetsToResources to add all frames in flight by passing an array of desriptor sets
ZEST_API void zest_AddDescriptorSetToResources(zest_shader_resources shader_resources, zest_descriptor_set descriptor_set, zest_uint fif);
//Add the descriptor set for a uniform buffer to a shader resource
ZEST_API void zest_AddUniformBufferToResources(zest_shader_resources shader_resources, zest_uniform_buffer buffer);
//Add the descriptor set for a uniform buffer to a shader resource
ZEST_API void zest_AddGlobalBindlessSetToResources(zest_shader_resources shader_resources);
//Add the descriptor set for a uniform buffer to a shader resource
ZEST_API void zest_AddBindlessSetToResources(zest_shader_resources shader_resources, zest_descriptor_set set);
//Clear all the descriptor sets in a shader resources object. This does not free the memory, call zest_FreeShaderResources to do that.
ZEST_API void zest_ClearShaderResources(zest_shader_resources shader_resources);
//Update the descriptor set in a shader_resources. You'll need this whenever you update a descriptor set for whatever reason. Pass the index of the
//descriptor set in the shader_resources that you want to update.
ZEST_API void zest_UpdateShaderResources(zest_shader_resources shader_resources, zest_descriptor_set descriptor_set, zest_uint index, zest_uint fif);
//Free the memory of used to store the descriptor sets in the shader resources, this does not free the descriptor sets themselves.
ZEST_API void zest_FreeShaderResources(zest_shader_resources shader_resources);
//Helper function to validate
ZEST_API bool zest_ValidateShaderResource(zest_shader_resources shader_resources);
//Update a VkDescriptorSet with an array of descriptor writes. For when the images/buffers in a descriptor set have changed, the corresponding descriptor set will need to be updated.
ZEST_API void zest_UpdateDescriptorSet(VkWriteDescriptorSet *descriptor_writes);
//Create a VkViewport, generally used when building a render pass.
ZEST_API VkViewport zest_CreateViewport(float x, float y, float width, float height, float minDepth, float maxDepth);
//Create a VkRect2D, generally used when building a render pass.
ZEST_API VkRect2D zest_CreateRect2D(zest_uint width, zest_uint height, int offsetX, int offsetY);
//Set a screen sized viewport and scissor command in the render pass
ZEST_API void zest_SetScreenSizedViewport(VkCommandBuffer command_buffer, float min_depth, float max_depth);
//Create a scissor and view port command. Must be called within a command buffer
ZEST_API void zest_Clip(VkCommandBuffer command_buffer, float x, float y, float width, float height, float minDepth, float maxDepth);
//Create a new shader handle
ZEST_API zest_shader zest_NewShader(shaderc_shader_kind type);
//Validate a shader from a string and add it to the library of shaders in the renderer
ZEST_API shaderc_compilation_result_t zest_ValidateShader(const char *shader_code, shaderc_shader_kind type, const char *name, shaderc_compiler_t compiler);
//Creates and compiles a new shader from a string and add it to the library of shaders in the renderer
ZEST_API zest_shader zest_CreateShader(const char *shader_code, shaderc_shader_kind type, const char *name, zest_bool format_code, zest_bool disable_caching, shaderc_compiler_t compiler, shaderc_compile_options_t options);
//Creates a shader from a file containing the shader glsl code
ZEST_API zest_shader zest_CreateShaderFromFile(const char *file, const char *name, shaderc_shader_kind type, zest_bool disable_caching, shaderc_compiler_t compiler, shaderc_compile_options_t options);
//Reload a shader. Use this if you edited a shader file and you want to refresh it/hot reload it
//The shader must have been created from a file with zest_CreateShaderFromFile. Once the shader is reloaded you can call
//zest_CompileShader or zest_ValidateShader to recompile it. You'll then have to call zest_SchedulePipelineRecreate to recreate
//the pipeline that uses the shader. Returns true if the shader was successfully loaded.
ZEST_API zest_bool zest_ReloadShader(zest_shader shader);
//Creates and compiles a new shader from a string and add it to the library of shaders in the renderer
ZEST_API zest_bool zest_CompileShader(zest_shader shader, shaderc_compiler_t compiler);
//Update an existing shader with a new version
ZEST_API void zest_UpdateShaderSPV(zest_shader shader, shaderc_compilation_result_t result);
//Add a shader straight from an spv file and return a handle to the shader. Note that no prefix is added to the filename here so 
//pass in the full path to the file relative to the executable being run.
ZEST_API zest_shader zest_AddShaderFromSPVFile(const char *filename, shaderc_shader_kind type);
//Add an spv shader straight from memory and return a handle to the shader. Note that the name should just be the name of the shader, 
//If a path prefix is set (ZestRenderer->shader_path_prefix, set when initialising Zest in the create_info struct, spv is default) then
//This prefix will be prepending to the name you pass in here.
ZEST_API zest_shader zest_AddShaderFromSPVMemory(const char *name, const void *buffer, zest_uint size, shaderc_shader_kind type);
//Add a shader to the renderer list of shaders.
ZEST_API void zest_AddShader(zest_shader shader, const char *name);
//Copy a shader that's stored in the renderer
ZEST_API zest_shader zest_CopyShader(const char *name, const char *new_name);
//Free the memory for a shader and remove if from the shader list in the renderer (if it exists there)
ZEST_API void zest_FreeShader(zest_shader shader);
//Set up shader resources ready to be bound to a pipeline when calling zest_BindPipeline or zest_BindpipelineCB. You should always 
//pass in an empty array of VkDescriptorSets. Set this array up as simple an 0 pointer and the function will allocate the space for the
//descriptor sets. This means that it's a good idea to not use a local variable. Call zest_ClearShaderResourceDescriptorSets after you've
//bound the pipeline.
ZEST_API zest_uint zest_GetDescriptorSetsForBinding(zest_shader_resources shader_resources, VkDescriptorSet **draw_sets);
ZEST_API void zest_ClearShaderResourceDescriptorSets(VkDescriptorSet *draw_sets);
ZEST_API zest_uint zest_ShaderResourceSetCount(VkDescriptorSet *draw_sets);
//Create command buffers which you can record to for draw command queues
ZEST_API zest_recorder zest_CreatePrimaryRecorder();
ZEST_API zest_recorder zest_CreatePrimaryRecorderWithPool(int queue_family_index);
ZEST_API zest_recorder zest_CreateSecondaryRecorder();
//Free a zest recorder's command buffer back into the command buffer pool
ZEST_API void zest_FreeCommandBuffers(zest_recorder recorder);
//Free the recorder from memory and release the command buffers back into the pool
ZEST_API void zest_FreeRecorder(zest_recorder recorder);
//Execute all the commands in a draw routine that were previously recorded
ZEST_API void zest_ExecuteRecorderCommands(VkCommandBuffer primary_command_buffer, zest_recorder recorder, zest_uint fif);
//When setting up you're own draw routine you will need to record the command buffer where you send instructions to the GPU.
//You must use this command to begin the recording. It will return a VkCommandBuffer that you can use in your vkCmd functions
//or other zest helper functions.
ZEST_API VkCommandBuffer zest_BeginRecording(zest_recorder recorder, VkRenderPass render_pass, zest_uint fif);
//This begin record function is specifically for recording compute command buffers. You must call this if you want
//to record a compute command buffer so that the render pass and frame buffers are set to null when preparing the
//command buffer.
ZEST_API VkCommandBuffer zest_BeginComputeRecording(zest_recorder recorder, zest_uint fif);
//After a call to zest_BeginRecording you must call this function after you're done recording all the commands you want.
//You can call this for both render pass and compute command buffers.
ZEST_API void zest_EndRecording(zest_recorder recorder, zest_uint fif);
ZEST_API void zest_ComputeEndRecording(zest_recorder recorder, zest_uint fif);
//Mark a recorder as outdated. This will force the draw routine to recall te callback to re-record the command buffer.
ZEST_API void zest_MarkOutdated(zest_recorder recorder);
ZEST_API void zest_ResetDrawRoutine(zest_draw_routine draw_routine);
ZEST_API void zest_SetViewport(VkCommandBuffer command_buffer, zest_draw_routine draw_routine);

//-----------------------------------------------
//        Pipeline_related_vulkan_helpers
//        Pipelines are essential to drawing things on screen. There are some builtin pipelines that Zest creates
//        for sprite/billboard/mesh/font drawing. You can take a look in zest__prepare_standard_pipelines to see how
//        the following functions are utilised, plus look at the exmaples for building your own custom pipelines.
//-----------------------------------------------
//Add a new pipeline template to the renderer and return its handle.
ZEST_API zest_pipeline_template zest_CreatePipelineTemplate(const char *name);
//Set the name of the file to use for the vert and frag shader in the zest_pipeline_template_create_info_t
ZEST_API void zest_SetPipelineTemplateVertShader(zest_pipeline_template pipeline_template, const char *file, const char *prefix);
ZEST_API void zest_SetPipelineTemplateFragShader(zest_pipeline_template pipeline_template, const char *file, const char *prefix);
//Set the name of both the fragment and vertex shader to the same file (frag and vertex shaders can be combined into the same spv)
ZEST_API void zest_SetPipelineTemplateShader(zest_pipeline_template pipeline_template, const char *file, const char *prefix);
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
//Make a new array of vertex input descriptors for use with zest_AddVertexInputDescription. input descriptions are used
//to define the vertex input types such as position, color, uv coords etc., depending on what you need.
ZEST_API zest_vertex_input_descriptions zest_NewVertexInputDescriptions();
//Add a VkVertexInputeAttributeDescription to a zest_vertex_input_descriptions array. You can use zest_CreateVertexInputDescription
//helper function to create the description
ZEST_API void zest_AddVertexInputDescription(zest_pipeline_template pipeline_template,  VkVertexInputAttributeDescription description);
//Create a VkVertexInputAttributeDescription for adding to a zest_vertex_input_descriptions array. Just pass the binding and location in
//the shader, the VkFormat and the offset into the struct that you're using for the vertex data. See zest__prepare_standard_pipelines
//for examples of how the builtin pipelines do this
ZEST_API VkVertexInputAttributeDescription zest_CreateVertexInputDescription(zest_uint binding, zest_uint location, VkFormat format, zest_uint offset);
//Set up the push contant that you might plan to use in the pipeline. Just pass in the size of the push constant struct, the offset and the shader
//stage flags where the push constant will be used. Use this if you only want to set up a single push constant range
ZEST_API void zest_SetPipelineTemplatePushConstantRange(zest_pipeline_template create_info, zest_uint size, zest_uint offset, zest_supported_shader_stages stage_flags);
//You can set a pointer in the pipeline template to point to the push constant data that you want to pass to the shader.
//It MUST match the same data layout/size that you set with zest_SetPipelineTemplatePushConstantRange and align with the 
//push constants that you use in the shader. The point you use must be stable! Or update it if it changes for any reason.
ZEST_API void zest_SetPipelineTemplatePushConstants(zest_pipeline_template pipeline_template, void *push_constants);
//Set the uniform buffer that the pipeline should use. You must call zest_MakePipelineDescriptorWrites after setting the uniform buffer.
ZEST_API void zest_SetPipelineUniformBuffer(zest_pipeline pipeline, zest_uniform_buffer uniform_buffer);
//Add a descriptor layout to the pipeline template. Use this function only when setting up the pipeline before you call zest_BuildPipeline
ZEST_API void zest_AddPipelineTemplateDescriptorLayout(zest_pipeline_template pipeline_template, VkDescriptorSetLayout layout);
//Clear the descriptor layouts in a pipeline template create info
ZEST_API void zest_ClearPipelineTemplateDescriptorLayouts(zest_pipeline_template pipeline_template);
//Make a pipeline template ready for building. Pass in the pipeline that you created with zest_CreatePipelineTemplate, the render pass that you want to
//use for the pipeline and the zest_pipeline_template_create_info_t you have setup to configure the pipeline. After you have called this
//function you can make a few more alterations to configure the pipeline further if needed before calling zest_BuildPipeline.
//NOTE: the create info that you pass into this function will get copied and then freed so don't use it after calling this function. If
//you want to create another variation of this pipeline you're creating then you can call zest_CopyTemplateFromPipeline to create a new
//zest_pipeline_template_create_info_t and create another new pipeline with that
ZEST_API void zest_FinalisePipelineTemplate(zest_pipeline_template pipeline_template);
//Build the pipeline ready for use in your draw routines. This is the final step in building the pipeline.
ZEST_API void zest_BuildPipeline(zest_pipeline pipeline);
//Get the shader stage flags for a specific push constant range in the pipeline
ZEST_API VkShaderStageFlags zest_PipelinePushConstantStageFlags(zest_pipeline pipeline, zest_uint index);
//Get the size of a push constant range for a specific index in the pipeline
ZEST_API zest_uint zest_PipelinePushConstantSize(zest_pipeline pipeline, zest_uint index);
//Get the offset of a push constant range for a specific index in the pipeline
ZEST_API zest_uint zest_PipelinePushConstantOffset(zest_pipeline pipeline, zest_uint index);
//The following are helper functions to set color blend attachment states for various blending setups
//Just take a look inside the functions for the values being used
ZEST_API VkPipelineColorBlendAttachmentState zest_AdditiveBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_AdditiveBlendState2(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_AlphaOnlyBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_AlphaBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_PreMultiplyBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_PreMultiplyBlendStateForSwap(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_MaxAlphaBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_ImGuiBlendState(void);
//Bind a pipeline for use in a draw routing. Once you have built the pipeline at some point you will want to actually use it to draw things.
//In order to do that you can bind the pipeline using this function. Just pass in the pipeline handle and a zest_shader_resources. Note that the
//descriptor sets in the shader_resources must be compatible with the layout that is being using in the pipeline. The command buffer used in the binding will be
//whatever is defined in ZestRenderer->current_command_buffer which will be set when the command queue is recorded. If you need to specify
//a command buffer then call zest_BindPipelineCB instead.
ZEST_API void zest_BindPipeline(VkCommandBuffer command_buffer, zest_pipeline_t* pipeline, VkDescriptorSet *descriptor_set, zest_uint set_count);
//Bind a pipeline for a compute shader
ZEST_API void zest_BindComputePipeline(VkCommandBuffer command_buffer, zest_compute compute, VkDescriptorSet *descriptor_set, zest_uint set_count);
//Bind a pipeline using a shader resource object. The shader resources must match the descriptor layout used in the pipeline that
//you pass to the function. Pass in a manual frame in flight which will be used as the fif for any descriptor set in the shader
//resource that is marked as static.
ZEST_API void zest_BindPipelineShaderResource(VkCommandBuffer command_buffer, zest_pipeline pipeline, zest_shader_resources shader_resources);
//Retrieve a pipeline from the renderer storage. Just pass in the name of the pipeline you want to retrieve and the handle to the pipeline
//will be returned.
ZEST_API zest_pipeline zest_Pipeline(const char *name, VkRenderPass render_pass);
ZEST_API zest_pipeline_template zest_PipelineTemplate(const char *name);
ZEST_API zest_pipeline zest_PipelineWithTemplate(zest_pipeline_template pipeline_template, VkRenderPass render_pass);
//Copy the zest_pipeline_template_create_info_t from an existing pipeline. This can be useful if you want to create a new pipeline based
//on an existing pipeline with just a few tweaks like setting a different shader to use.
ZEST_API zest_pipeline_template zest_CopyPipelineTemplate(const char *name, zest_pipeline_template pipeline_template);
//-- End Pipeline related

//--End vulkan helper functions

//Platform_dependent_callbacks
//Depending on the platform and method you're using to create a window and poll events callbacks are used to do those things.
//You can define those callbacks with these functions
ZEST_API void zest_SetDestroyWindowCallback(void(*destroy_window_callback)(void *user_data));
ZEST_API void zest_SetGetWindowSizeCallback(void(*get_window_size_callback)(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height));
ZEST_API void zest_SetPollEventsCallback(void(*poll_events_callback)(void));
ZEST_API void zest_SetPlatformExtensionsCallback(void(*add_platform_extensions_callback)(void));

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
//Create a staging buffer which you can use to prep data for uploading to another buffer on the GPU
ZEST_API zest_frame_staging_buffer zest_CreateFrameStagingBuffer(VkDeviceSize size);
ZEST_API zest_buffer zest_GetStagingBuffer(zest_frame_staging_buffer frame_staging_buffer);
//Memset a host visible buffer to 0.
ZEST_API void zest_ClearBufferToZero(zest_buffer buffer);
//Memset a host visible buffer to 0.
ZEST_API void zest_ClearFrameStagingBufferToZero(zest_frame_staging_buffer buffer);
//Create an index buffer.
ZEST_API zest_buffer zest_CreateIndexBuffer(VkDeviceSize size, zest_buffer staging_buffer);
ZEST_API zest_buffer zest_CreateVertexBuffer(VkDeviceSize size, zest_buffer staging_buffer);
//Create a general storage buffer mainly for use in a compute shader
ZEST_API zest_buffer zest_CreateStorageBuffer(VkDeviceSize size, zest_buffer staging_buffer);
//Create a vertex buffer that is flagged for storage so that you can use it in a compute shader
ZEST_API zest_buffer zest_CreateComputeVertexBuffer(VkDeviceSize size, zest_buffer staging_buffer);
//Create an index buffer that is flagged for storage so that you can use it in a compute shader
ZEST_API zest_buffer zest_CreateComputeIndexBuffer(VkDeviceSize size, zest_buffer staging_buffer);
//The following functions can be used to generate a zest_buffer_info_t with the corresponding buffer configuration to create buffers with
ZEST_API zest_buffer_info_t zest_CreateVertexBufferInfo(zest_bool cpu_visible);
ZEST_API zest_buffer_info_t zest_CreateVertexBufferInfoWithStorage(zest_bool cpu_visible);
ZEST_API zest_buffer_info_t zest_CreateStorageBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateStorageBufferInfoWithSrcFlag(void);
ZEST_API zest_buffer_info_t zest_CreateComputeVertexBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateComputeIndexBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateIndexBufferInfo(zest_bool cpu_visible);
ZEST_API zest_buffer_info_t zest_CreateIndexBufferInfoWithStorage(zest_bool cpu_visible);
ZEST_API zest_buffer_info_t zest_CreateStagingBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateDrawCommandsBufferInfo(zest_bool host_visible);
//Create descriptor buffers with the following functions. Descriptor buffers can be used when you want to bind them in a descriptor set
//for use in a shader. When you create a descriptor buffer it also creates the descriptor info which is necessary when creating the
//descriptor set. Note that to create a uniform buffer which needs a descriptor info you can just call zest_CreateUniformBuffer.
//Pass in the size and whether or not you want a buffer for each frame in flight. If you're writing to the buffer every frame then you
//we need a buffer for each frame in flight, otherwise if the buffer is being written too ahead of time and will be read only after that
//then you can pass ZEST_FALSE or 0 for all_frames_in_flight to just create a single buffer.
ZEST_API zest_descriptor_buffer zest_CreateDescriptorBuffer(zest_buffer_info_t *buffer_info, zest_size size);
ZEST_API zest_descriptor_buffer zest_CreateStorageDescriptorBuffer(zest_size size);
ZEST_API zest_descriptor_buffer zest_CreateStorageDescriptorBufferWithSrcFlag (zest_size size);
ZEST_API zest_descriptor_buffer zest_CreateCPUVisibleStorageDescriptorBuffer(zest_size size);
ZEST_API zest_descriptor_buffer zest_CreateVertexDescriptorBuffer(zest_size size, zest_bool cpu_visible);
ZEST_API zest_descriptor_buffer zest_CreateIndexDescriptorBuffer(zest_size size, zest_bool cpu_visible);
ZEST_API zest_descriptor_buffer zest_CreateComputeVertexDescriptorBuffer(zest_size size);
ZEST_API zest_descriptor_buffer zest_CreateComputeIndexDescriptorBuffer(zest_size size);
//Use this function to get the zest_buffer from the zest_descriptor_buffer handle. If the buffer uses multiple frames in flight then it
//will retrieve the current frame in flight buffer which will be safe to write to.
ZEST_API zest_buffer zest_GetBufferFromDescriptorBuffer(zest_descriptor_buffer descriptor_buffer);
//Grow a buffer if the minium_bytes is more then the current buffer size.
ZEST_API zest_bool zest_GrowBuffer(zest_buffer *buffer, zest_size unit_size, zest_size minimum_bytes);
//Grow a descriptor buffer if the minium_bytes is more then the current buffer size.
ZEST_API zest_bool zest_GrowDescriptorBuffer(zest_descriptor_buffer buffer, zest_size unit_size, zest_size minimum_bytes);
//Resize a buffer if the new size if more than the current size of the buffer. Returns true if the buffer was resized successfully.
ZEST_API zest_bool zest_ResizeBuffer(zest_buffer *buffer, zest_size new_size);
//Resize a descriptor buffer if the new size if more than the current size of the buffer. Returns true if the buffer was resized successfully.
ZEST_API zest_bool zest_ResizeDescriptorBuffer(zest_descriptor_buffer buffer, zest_size new_size);
//Copy a buffer to another buffer. Generally this will be a staging buffer copying to a buffer on the GPU (device_buffer). You must specify
//the size as well that you want to copy
ZEST_API void zest_CopyBufferOneTime(zest_buffer src_buffer, zest_buffer dst_buffer, VkDeviceSize size);
//Exactly the same as zest_CopyBuffer but you can specify a command buffer to use to make the copy. This can be useful if you are doing a
//one off copy with a separate command buffer
ZEST_API void zest_CopyBuffer(VkCommandBuffer command_buffer, zest_buffer staging_buffer, zest_buffer device_buffer, VkDeviceSize size);
//Helper function that will upload a frame staging buffer to a GPU buffer like a vertex or storage buffer. The amount
//copied will be whatever is in use in the staging buffer
ZEST_API void zest_CopyFrameStagingBuffer(VkCommandBuffer command_buffer, zest_frame_staging_buffer staging_buffer, zest_buffer device_buffer);
ZEST_API void zest_StageFrameData(void *src_data, zest_frame_staging_buffer dst_staging_buffer, zest_size size);
ZEST_API zest_size zest_GetFrameStageBufferMemoryInUse(zest_frame_staging_buffer staging_buffer);
//Flush the memory of - in most cases - a staging buffer so that it's memory is made available immediately to the device
ZEST_API VkResult zest_FlushBuffer(zest_buffer buffer);
//Free a zest_buffer and return it's memory to the pool
ZEST_API void zest_FreeBuffer(zest_buffer buffer);
//Free a zest_descriptor_buffer and return it's memory to the pool
ZEST_API void zest_FreeDescriptorBuffer(zest_descriptor_buffer buffer);
//Get the VkDeviceMemory from a zest_buffer. This is needed for some Vulkan commands
ZEST_API VkDeviceMemory zest_GetBufferDeviceMemory(zest_buffer buffer);
//Get the VkBuffer from a zest_buffer. This is needed for some Vulkan Commands
ZEST_API VkBuffer *zest_GetBufferDeviceBuffer(zest_buffer buffer);
//Get the VkBuffer from a zest_descriptor_buffer. 
ZEST_API VkBuffer zest_GetDescriptorBufferVK(zest_descriptor_buffer buffer);
//Get the offset of a zest_descriptor_buffer. 
ZEST_API VkDeviceSize zest_GetDescriptorBufferOffset(zest_descriptor_buffer buffer);
//Get the offset of a zest_descriptor_buffer size. 
ZEST_API VkDeviceSize zest_GetDescriptorBufferSize(zest_descriptor_buffer buffer);
//When creating your own draw routines you will probably need to upload data from a staging buffer to a GPU buffer like a vertex buffer. You can
//use this command with zest_UploadBuffer to upload the buffers that you need. You can call zest_AddCopyCommand multiple times depending on
//how many buffers you need to upload data for and then call zest_UploadBuffer passing the zest_buffer_uploader_t to copy all the buffers in
//one go. For examples see the builtin draw layers that do this like: zest__update_instance_layer_buffers_callback
ZEST_API void zest_AddCopyCommand(zest_buffer_uploader_t *uploader, zest_buffer_t *source_buffer, zest_buffer_t *target_buffer, VkDeviceSize target_offset);
ZEST_API zest_bool zest_UploadBuffer(zest_buffer_uploader_t *uploader, VkCommandBuffer command_buffer);
//Get the default pool size that is set for a specific pool hash.
ZEST_API zest_buffer_pool_size_t zest_GetDevicePoolSize(zest_key hash);
//Get the default pool size that is set for a specific combination of usage, property and image flags
ZEST_API zest_buffer_pool_size_t zest_GetDeviceBufferPoolSize(VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, VkImageUsageFlags image_flags);
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
ZEST_API zest_uniform_buffer zest_CreateUniformBuffer(const char *name, zest_size uniform_struct_size);
//Standard builtin functions for updating a uniform buffer for use in 2d shaders where x,y coordinates represent a location on the screen. This will
//update the current frame in flight. If you need to update a specific frame in flight then call zest_UpdateUniformBufferFIF.
ZEST_API void zest_Update2dUniformBuffer(void);
//Get a pointer to a uniform buffer. This will return a void* which you can cast to whatever struct your storing in the uniform buffer. This will get the buffer
//with the current frame in flight index.
ZEST_API void *zest_GetUniformBufferData(zest_uniform_buffer uniform_buffer);
//Get the VkDescriptorBufferInfo of a uniform buffer by name and specific frame in flight. Use ZEST_FIF you just want the current frame in flight
ZEST_API VkDescriptorBufferInfo *zest_GetUniformBufferInfo(zest_uniform_buffer buffer);
//Get the VkDescriptorSetLayout for a uniform buffer that you can use when creating pipelines. The buffer must have
//been properly initialised, use zest_CreateUniformBuffer for this.
ZEST_API VkDescriptorSetLayout zest_vk_GetUniformBufferLayout(zest_uniform_buffer buffer);
ZEST_API zest_set_layout zest_GetUniformBufferLayout(zest_uniform_buffer buffer);
//Get the VkDescriptorSet in a uniform buffer. You can use this when binding a pipeline for a draw call or compute
//dispatch etc. 
ZEST_API VkDescriptorSet zest_vk_GetUniformBufferSet(zest_uniform_buffer buffer);
ZEST_API zest_descriptor_set zest_GetUniformBufferSet(zest_uniform_buffer buffer);
ZEST_API zest_descriptor_set zest_GetDefaultUniformBufferSet();
ZEST_API zest_set_layout zest_GetDefaultUniformBufferLayout();
ZEST_API VkDescriptorSet zest_vk_GetDefaultUniformBufferSet();
ZEST_API VkDescriptorSetLayout zest_vk_GetDefaultUniformBufferLayout();
ZEST_API zest_uniform_buffer zest_GetDefaultUniformBuffer();
//Bind a vertex buffer. For use inside a draw routine callback function.
ZEST_API void zest_BindVertexBuffer(VkCommandBuffer command_buffer, zest_buffer buffer);
//Bind an index buffer. For use inside a draw routine callback function.
ZEST_API void zest_BindIndexBuffer(VkCommandBuffer command_buffer, zest_buffer buffer);
//--End Buffer related

//-----------------------------------------------
//        Command_queue_setup_and_creation
//        Command queues are where all of the vulkan commands get written for submitting and executing
//        on the GPU. The best way to understand them would be to look at the examples, such as the render
//        targets example. Note that the command queue setup commands are contextual and must be called
//        in the correct order depending on how you want your queue setup.
//        The declarations below are indented to indicate contextual dependencies of the commands where
//        applicable
//-----------------------------------------------

// -- Contextual command queue setup commands
//Create a new command queue with the name you pass to the function. This command queue assumes that you want to render to the swap chain.
//Context:    None, this is a root set up command that sets the context to: zest_setup_context_type_command_queue
ZEST_API zest_command_queue zest_NewCommandQueue(const char *name);
//Create a new command queue that you can submit anytime you need to. This is not for rendering to the swap chain but generally for rendering
//to a zest_render_target or just running a compute shader
//Context:    None, this is a root set up command that sets the context to: zest_setup_context_type_command_queue
ZEST_API zest_command_queue zest_NewFloatingCommandQueue(const char *name);
    //Create new draw commands. Draw commands are where you can add draw routines, either builtin ones like sprite and billboard drawing, or your
    //own custom ones. With this draw commands setup you can specify a render target that you want to draw to.
    //Context:    Must be called after zest_NewCommandQueue or zest_NewFloatingCommandQueue when the context will be zest_setup_context_type_command_queue
    //            This will set the context to zest_setup_context_type_render_pass
    ZEST_API zest_command_queue_draw_commands zest_NewDrawCommandSetup(const char *name, zest_render_target render_target);
    //Create new draw commands that draw directly to the swap chain.
    //Context:    Must be called after zest_NewCommandQueue or zest_NewFloatingCommandQueue. This will add the draw commands to the current command
    //            queue being set up. This will set the current context to zest_setup_context_type_render_pass
    ZEST_API zest_command_queue_draw_commands zest_NewDrawCommandSetupSwap(const char *name);
        //Add a new draw routine to the current draw commands context. Draw routines are where you can setup your own custom drawing commands
        ZEST_API void zest_AddDrawRoutine(zest_draw_routine draw_routine);
        //You can use this to add a draw routine that you created with zest_CreateInstanceDrawRoutine to the draw commands. Returns a layer that you can
        //use to draw with in your render/update loop
        ZEST_API zest_layer zest_AddInstanceDrawRoutine(zest_draw_routine draw_routine);
            //Get the current context draw routine. Must be within a draw routine context
            ZEST_API zest_draw_routine zest_ContextDrawRoutine();
        //Add a new mesh layer to the current draw commands context. A mesh layer can be used to render anything that will use an index and vertex
        //buffer. A mesh layer is what's used to render Dear ImGui if you're using that but see the implementation and example for imgui.
        //Context:    Must be called within a draw commands context
        ZEST_API zest_layer zest_NewMeshLayer(const char *name, zest_size vertex_struct_size);
        //Add a pre-existin layer that you already created elsewhere.
        //Context:    Must be called within a draw commands context
        ZEST_API void zest_AddLayer(zest_layer layer);
        //Set the clear color of the current draw commands context
        ZEST_API void zest_ContextSetClsColor(float r, float g, float b, float a);
    //Create new compute shader to run within a command queue. See the compute shader section for all the commands relating to setting up a compute shader.
    //Also see the compute shader example
    //Context:    Must be called after zest_NewCommandQueue or zest_NewFloatingCommandQueue.
    ZEST_API zest_command_queue_compute zest_NewComputeSetup(const char *name, zest_compute compute_shader, void(*compute_function)(zest_compute compute), int(*condition_function)(zest_compute compute));
    //Create draw commands that draw render targets to the swap chain. This is useful if you are drawing to render targets elsewhere
    //in the command queue and want to draw some or all of those to the swap chain.
    //Context:    Must be called after zest_NewCommandQueue or zest_NewFloatingCommandQueue. This will add the draw commands to the current command
    //            queue being set up. This will set the current context to zest_setup_context_type_render_pass
    ZEST_API zest_command_queue_draw_commands zest_NewDrawCommandSetupRenderTargetSwap(const char *name, zest_render_target render_target);
    ZEST_API zest_command_queue_draw_commands zest_NewDrawCommandSetupCompositeToSwap(const char* name, zest_pipeline_template composite_pipeline);
    //Create draw commands that draw render targets to a composite render target. This is useful if you are drawing to render targets elsewhere and 
    //want to combine them all before doing tonemapping
    ZEST_API zest_command_queue_draw_commands zest_NewDrawCommandSetupCompositor(const char *name, zest_render_target render_target, zest_pipeline_template pipeline_template);
    //Create draw commands that take an input source which you can use a specific shader/pipeline to filter the image
    //and then down/upsample the image for blur/bloom or other effects that require this
    ZEST_API zest_command_queue_draw_commands zest_NewDrawCommandSetupDownSampler(const char *name, zest_render_target render_target, zest_render_target input_source, zest_pipeline_template pipeline_template);
    ZEST_API zest_command_queue_draw_commands zest_NewDrawCommandSetupUpSampler(const char *name, zest_render_target render_target, zest_render_target downsampler_render_target, zest_pipeline_template pipeline_template);
        //Add a render target to the render pass within a zest_NewDrawCommandSetupRenderTargetSwap
        //Context:    Must be called after zest_NewDrawCommandSetupRenderTargetSwap when the context will be zest_setup_context_type_render_pass
        ZEST_API void zest_AddRenderTarget(zest_render_target render_target);
        //Add a render target layer to the compositor. You can set the type of blend mode that you want to use
        //and an alpha level.
        ZEST_API void zest_AddCompositeLayer(zest_render_target render_target);
    //Finish setting up a command queue. You must call this after setting up a command queue to reset contexts and validate the command queue.
    //This function will also connect the command queue to the swap chain so that it gets presented to the user
    ZEST_API void zest_FinishQueueSetup(void);

//-----------------------------------------------
//        Modifying command queues
//-----------------------------------------------

//Modify an existing zest_command_queue. This will set the command queue you pass to the function as the current set up context.
//Context:    None. This must be called with no context currently set.
ZEST_API void zest_ModifyCommandQueue(zest_command_queue command_queue);
    //Modify an existing zest_command_queue_draw_commands. The draw commands must exist within the current command queue that you're editing.
    //Context:    Must be called after zest_ModifyCommandQueue
    ZEST_API void zest_ModifyDrawCommands(zest_command_queue_draw_commands draw_commands);

//Get an existing zest_command_queue by name
ZEST_API zest_command_queue zest_GetCommandQueue(const char *name);
//Get an existing zest_command_queue_draw_commands object by name
ZEST_API zest_command_queue_draw_commands zest_GetCommandQueueDrawCommands(const char *name);
//Set the clear color for the render pass that happens within a zest_command_queue_draw_commands object
//Note that this only applies if you use the default draw commands callback: zest_RenderDrawRoutinesCallback
ZEST_API void zest_SetDrawCommandsClsColor(zest_command_queue_draw_commands draw_commands, float r, float g, float b, float a);
ZEST_API void zest_SetDrawCommandsCallback(void(*render_pass_function)(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer));
//Get a zest_draw_routine by name
ZEST_API zest_draw_routine zest_GetDrawRoutine(const char *name);
//Get a zest_command_queue_draw_commands by name
ZEST_API zest_command_queue_draw_commands zest_GetDrawCommands(const char *name);
//Builtin draw routine callbacks for drawing the builtin layers and render target drawing.
ZEST_API void zest_RenderDrawRoutinesCallback(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer);
ZEST_API void zest_DrawToRenderTargetCallback(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer);
ZEST_API void zest_DrawRenderTargetsToSwapchain(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer);
ZEST_API void zest_CompositeRenderTargets(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer);
ZEST_API void zest_DownSampleRenderTarget(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer);
ZEST_API void zest_UpSampleRenderTarget(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer);
//Add an existing zest_draw_routine to a zest_command_queue_draw_commands. This just lets you add draw routines to a queue separately outside of a command queue
//setup context
ZEST_API void zest_AddDrawRoutineToDrawCommands(zest_command_queue_draw_commands draw_commands, zest_draw_routine draw_routine);
//Helper functions for creating the builtin layers. these can be called separately outside of a command queue setup context
ZEST_API zest_layer zest_CreateBuiltinSpriteLayer(const char *name);
ZEST_API zest_layer zest_CreateBuiltin3dLineLayer(const char *name);
ZEST_API zest_layer zest_CreateBuiltinBillboardLayer(const char *name);
ZEST_API zest_layer zest_CreateMeshLayer(const char *name, zest_size vertex_type_size);
ZEST_API zest_layer zest_CreateBuiltinInstanceMeshLayer(const char *name);
//Insert a layer into storage. You can use this for custom layers of you're own
ZEST_API void zest_InsertLayer(zest_layer layer);
//Get a zest_layer by name
ZEST_API zest_layer zest_GetLayer(const char *name);
//Create a zest_command_queue_compute and add it to a zest_command_queue. You can call this function outside of a command queue setup context. Otherwise
//just call zest_NewComputeSetup when you setup or modify a command queue
ZEST_API zest_command_queue_compute zest_CreateComputeItem(const char *name, zest_command_queue command_queue);
//Validate a command queue to make sure that all semaphores are connected. This is automatically called when you call zest_FinishQueueSetup but I've just left it
//here as an API function if needed
ZEST_API void zest_ValidateQueue(zest_command_queue queue);
//Reset the compute queue shader index back to 0, this is actually done automatically each frame. It's purpose is so that zest_NextComputeRoutine can iterate through
//the compute routines in a compute shader and dispatch each one starting from 0. But if you're only doing that once each frame (which you probably are) then you won't
//need to call this.
ZEST_API void zest_ResetComputeRoutinesIndex(zest_command_queue_compute compute_queue);
//Record a zest_command_queue. If you call zest_SetActiveCommandQueue then these functions are called automatically each frame but for floating command
//queues (see zest_NewFloatingCommandQueue) you can record and submite a command queue using these functions. Pass the command_queue and the frame in flight
//index to record.
ZEST_API void zest_RecordCommandQueue(zest_command_queue command_queue, zest_index fif);
//Where zest_RecordCommandQueue records and then ends the command queue ready for submitting, this will just start the recording
//allowing you to insert your own vkCmds between calling AddCommandQueue and then EndCommandQueue
ZEST_API void zest_StartCommandQueue(zest_command_queue command_queue, zest_index fif);
//End a command queue ready for submitting
ZEST_API void zest_EndCommandQueue(zest_command_queue command_queue, zest_index fif);
//Submit a command queue to be executed on the GPU. Utilise the fence commands to know when the queue has finished executing: zest_CreateFence, zest_CheckFence,
//zest_WaitForFence and zest_DestoryFence. Pass in a fence which will be signalled once the execution is done.
ZEST_API void zest_SubmitCommandQueue(zest_command_queue command_queue, VkFence fence);
//Returns the current command queue that was set with zest_SetActiveCommandQueue.
ZEST_API zest_command_queue zest_CurrentCommandQueue(void);
//Get the timestamps for the whole render pipeline
ZEST_API zest_timestamp_duration_t zest_CommandQueueRenderTimes(zest_command_queue command_queue);
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
//Get the map containing all the textures (do we need this?)
ZEST_API zest_map_textures *zest_GetTextures(void);
//Create a new blank initialised texture. You probably don't need to call this much (maybe I should move to private function) Use the Create texture functions instead for most things
ZEST_API zest_texture zest_NewTexture(void);
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
ZEST_API zest_texture zest_CreateTexture(const char *name, zest_texture_storage_type storage_type, zest_bool use_filtering, zest_texture_format format, zest_uint reserve_images);
ZEST_API zest_texture zest_ReplaceTexture(zest_texture texture, zest_texture_storage_type storage_type, zest_bool use_filtering, zest_texture_format format, zest_uint reserve_images);
//The following are helper functions to create a texture of a given type. the texture will be set to use filtering by default
ZEST_API zest_texture zest_CreateTexturePacked(const char *name, zest_texture_format format);
ZEST_API zest_texture zest_CreateTextureSpritesheet(const char *name, zest_texture_format format);
ZEST_API zest_texture zest_CreateTextureSingle(const char *name, zest_texture_format format);
ZEST_API zest_texture zest_CreateTextureBank(const char *name, zest_texture_format format);
ZEST_API zest_texture zest_CreateTextureStorage(const char *name, int width, int height, zest_texture_format format, VkImageViewType view_type);
//Delete a texture from the renderer and free its resources. You must ensure that you don't try writing to the texture while deleting.
ZEST_API void zest_DeleteTexture(zest_texture texture);
//Resetting a texture will remove all it's images and reset it back to it's "just created" status
ZEST_API void zest_ResetTexture(zest_texture texture);
//By default any images that you load into a texture will be stored so that you can rebuild the texture if you need to (for example, you may load in more images
//to pack in to the sprite sheets at a later time). But if you want to free up the memory then you can just free them with this function.
ZEST_API void zest_FreeTextureBitmaps(zest_texture texture);
//Remove a specific zest_image from the texture
ZEST_API void zest_RemoveTextureImage(zest_texture texture, zest_image image);
//Remove a specific animation from the texture. Pass in the first frame of the animation with a zest_image
ZEST_API void zest_RemoveTextureAnimation(zest_texture texture, zest_image first_image);
//Set the format of the texture. You will need to reprocess the texture if you have processed it already with zest_ProcessTextureImages
ZEST_API void zest_SetTextureImageFormat(zest_texture texture, zest_texture_format format);
//Set the callback for the texture which is used whenever a texture is scheduled for reprocessing. This allows you to make changes to textures that 
//might be in use.
ZEST_API void zest_SetTextureReprocessCallback(zest_texture texture, void(*callback)(zest_texture texture, void *user_data));
//Set the callback for a texture which is called after a texture has been scheduled for reprocessing. This lets you cleanup any out of date descriptor sets
//that you might be using for the texture.
ZEST_API void zest_SetTextureCleanupCallback(zest_texture texture, void(*callback)(zest_texture texture, void *user_data));
//Set the user data for a texture. This is passed though to a texture reprocess callback which you can use to perform tasks such as updating any 
//descriptor sets you've made that might use the texture
ZEST_API void zest_SetTextureUserData(zest_texture texture, void *user_data);
//Create a blank zest_image. This is more for internal usage, prefer the zest_AddTextureImage functions. Will leave these in the API for now though
ZEST_API zest_image_t zest_NewImage(void);
ZEST_API zest_imgui_image_t zest_NewImGuiImage(void);
ZEST_API zest_image zest_CreateImage(void);
ZEST_API zest_image zest_CreateAnimation(zest_uint frames);
//Load a bitmap from a file. Set color_channels to 0 to auto detect the number of channels
ZEST_API void zest_LoadBitmapImage(zest_bitmap_t *image, const char *file, int color_channels);
//Load a bitmap from a memory buffer. Set color_channels to 0 to auto detect the number of channels. Pass in a pointer to the memory buffer containing
//the bitmap, the size in bytes and how many channels it has.
ZEST_API void zest_LoadBitmapImageMemory(zest_bitmap_t *image, const unsigned char *buffer, int size, int desired_no_channels);
//Free the memory used in a zest_bitmap_t
ZEST_API void zest_FreeBitmap(zest_bitmap_t *image);
//Create a new initialise zest_bitmap_t
ZEST_API zest_bitmap_t zest_NewBitmap(void);
//Create a new bitmap from a pixel buffer. Pass in the name of the bitmap, a pointer to the buffer, the size in bytes of the buffer, the width and height
//and the number of color channels
ZEST_API zest_bitmap_t zest_CreateBitmapFromRawBuffer(const char *name, unsigned char *pixels, int size, int width, int height, int channels);
//Allocate the memory for a bitmap based on the width, height and number of color channels. You can also specify the fill color
ZEST_API void zest_AllocateBitmap(zest_bitmap_t *bitmap, int width, int height, int channels, zest_color fill_color);
//Copy all of a source bitmap to a destination bitmap
ZEST_API void zest_CopyWholeBitmap(zest_bitmap_t *src, zest_bitmap_t *dst);
//Copy an area of a source bitmap to another bitmap
ZEST_API void zest_CopyBitmap(zest_bitmap_t *src, int from_x, int from_y, int width, int height, zest_bitmap_t *dst, int to_x, int to_y);
//Convert a bitmap to a specific vulkan color format. Accepted formats are:
//VK_FORMAT_R8G8B8A8_UNORM
//VK_FORMAT_B8G8R8A8_UNORM
//VK_FORMAT_R8_UNORM
ZEST_API void zest_ConvertBitmapToTextureFormat(zest_bitmap_t *src, VkFormat format);
//Convert a bitmap to a specific zest_texture_format. Accepted values are;
//zest_texture_format_alpha
//zest_texture_format_rgba_unorm
//zest_texture_format_bgra_unorm
ZEST_API void zest_ConvertBitmap(zest_bitmap_t *src, zest_texture_format format, zest_byte alpha_level);
//Convert a bitmap to BGRA format
ZEST_API void zest_ConvertBitmapToBGRA(zest_bitmap_t *src, zest_byte alpha_level);
//Fill a bitmap with a color
ZEST_API void zest_FillBitmap(zest_bitmap_t *image, zest_color color);
//plot a single pixel in the bitmap with a color
ZEST_API void zest_PlotBitmap(zest_bitmap_t *image, int x, int y, zest_color color);
//Convert a bitmap to RGBA format
ZEST_API void zest_ConvertBitmapToRGBA(zest_bitmap_t *src, zest_byte alpha_level);
//Convert a BGRA bitmap to RGBA format
ZEST_API void zest_ConvertBGRAToRGBA(zest_bitmap_t *src);
//Convert a bitmap to a single alpha channel
ZEST_API void zest_ConvertBitmapToAlpha(zest_bitmap_t *image);
ZEST_API void zest_ConvertBitmapTo1Channel(zest_bitmap_t *image);
//Sample the color of a pixel in a bitmap with the given x/y coordinates
ZEST_API zest_color zest_SampleBitmap(zest_bitmap_t *image, int x, int y);
//Get a pointer to the first pixel in a bitmap within the bitmap array. Index must be less than the number of bitmaps in the array
ZEST_API zest_byte *zest_BitmapArrayLookUp(zest_bitmap_array_t *bitmap_array, zest_index index);
//Get the distance in pixels to the furthes pixel from the center that isn't alpha 0
ZEST_API float zest_FindBitmapRadius(zest_bitmap_t *image);
//Destory a bitmap array and free its resources
ZEST_API void zest_DestroyBitmapArray(zest_bitmap_array_t *bitmap_array);
//Get a bitmap from a bitmap array with the given index
ZEST_API zest_bitmap_t zest_GetImageFromArray(zest_bitmap_array_t *bitmap_array, zest_index index);
//Get a bitmap from a texture with the given index
ZEST_API zest_bitmap_t *zest_GetBitmap(zest_texture texture, zest_index bitmap_index);
//Get a zest_image from a texture by it's index
ZEST_API zest_image zest_GetImageFromTexture(zest_texture texture, zest_index index);
//Add an image to a texture from a file. This uses stb_image to load the image so must be in a format that stb image can load
ZEST_API zest_image zest_AddTextureImageFile(zest_texture texture, const char* name);
//Add an image to a texture using a zest_bitmap_t
ZEST_API zest_image zest_AddTextureImageBitmap(zest_texture texture, zest_bitmap_t *bitmap_to_load);
//Add an image to a texture from a buffer.
ZEST_API zest_image zest_AddTextureImageMemory(zest_texture texture, const char* name, const unsigned char* buffer, int buffer_size);
//Add a sequence of images (in a sprite sheet) to a texture from a file. specify the width and height of each frame (they must all be the same size), the number of frames in the animation.
//You can also pass in a pointer to a float where the max radius can be set (the max radius is the furthest pixel from the center of the frame). Lastly you can specify
//whether the animation runs row by row, set to 0 to read column by column.
ZEST_API zest_image zest_AddTextureAnimationFile(zest_texture texture, const char* filename, int width, int height, zest_uint frames, float *max_radius, zest_bool row_by_row);
//Add an animation from a bitmap.
ZEST_API zest_image zest_AddTextureAnimationBitmap(zest_texture texture, zest_bitmap_t *spritesheet, int width, int height, zest_uint frames, float *max_radius, zest_bool row_by_row);
//Add an animation from a buffer in memory.
ZEST_API zest_image zest_AddTextureAnimationMemory(zest_texture texture, const char* name, unsigned char *buffer, int buffer_size, int width, int height, zest_uint frames, float *max_radius, zest_bool row_by_row);
//After adding all the images you want to a texture, you will then need to process the texture which will create all of the necessary GPU resources and upload the texture to the GPU.
//You can then use the image handles to draw the images along with the descriptor set - either the one that gets created automatically with the the texture to draw sprites and billboards
//or your own descriptor set.
//Don't call this in the middle of sending draw commands that are using the texture because it will switch the texture buffer
//index so some drawcalls will use the outdated texture, some the new. Call before any draw calls are made or better still,
//just call zest_ScheduleTextureReprocess which will recreate the texture between frames and then schedule a cleanup the next
//frame after.
ZEST_API void zest_ProcessTextureImages(zest_texture texture);
//Get the descriptor image info for the texture that you can use to build a descriptor set with
ZEST_API VkDescriptorImageInfo *zest_GetTextureDescriptorImageInfo(zest_texture texture);
//Create a vulkan frame buffer. Maybe this should be a private function, but leaving API for now. This returns a zest_frame_buffer_t containing the frame buffer attachment and other
//details about the frame buffer. You must pass in a valid render pass that the frame buffer will use (you can use zest_GetRenderPass), the width/height of the frame buffer, a valid
//VkFormat, whether or not you want to use a depth buffer with it, and if it's a frame buffer that will be used as a src buffer for something else so that it gets the appropriate
//flags set. You shouldn't need to use this function as it's mainly used when creating zest_render_targets.
ZEST_API zest_frame_buffer_t zest_CreateFrameBuffer(VkRenderPass render_pass, int mip_levels, zest_uint width, zest_uint height, VkFormat render_format, zest_bool use_depth_buffer, zest_bool is_src);
//Create and allocate a bitmap array. Bitmap arrays are exactly as they sound, an array of bitmaps. These are used to copy an array of bitmaps into a GPU texture array. These are
//primarily used in the creation of textures that use zest_texture_storage_type_packed and zest_texture_storage_type_bank.
ZEST_API void zest_CreateBitmapArray(zest_bitmap_array_t *images, int width, int height, int channels, zest_uint size_of_array);
//Free a bitmap array and return memory resources to the allocator
ZEST_API void zest_FreeBitmapArray(zest_bitmap_array_t *images);
//Set the handle of an image. This dictates where the image will be positioned when you draw it with zest_DrawSprite/zest_DrawBillboard. 0.5, 0.5 will center the image at the position you draw it.
ZEST_API void zest_SetImageHandle(zest_image image, float x, float y);
//Change the storage type of the texture. You will need to call zest_ProcessTextureImages after calling this function.
ZEST_API void zest_SetTextureStorageType(zest_texture texture, zest_texture_storage_type value);
//Set whether you want the texture to use mip map filtering or not. You will need to call zest_ProcessTextureImages after calling this function.
ZEST_API void zest_SetTextureUseFiltering(zest_texture texture, zest_bool value);
//Add a border to the images that get packed into the texture. This can make filtered textures look better.
ZEST_API void zest_SetTexturePackedBorder(zest_texture texture, zest_uint value);
//Set the texture wrapping for sampling textures in the shader. If you want textures to repeat then this is more relavent for texture banks or single image textures.
//Just pass in the VkSamplerAddressMode for each dimension u,v,w
ZEST_API void zest_SetTextureWrapping(zest_texture texture, VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w);
//Helper functions for setting the texture wrapping to the one you want.
ZEST_API void zest_SetTextureWrappingClamp(zest_texture texture);
ZEST_API void zest_SetTextureWrappingBorder(zest_texture texture);
ZEST_API void zest_SetTextureWrappingRepeat(zest_texture texture);
//Set the size of each layer in a packed texture. When all of the images are processed in a texture, they will be packed into layers of a given size. By default that size is 1024, but you
//can change that here.
ZEST_API void zest_SetTextureLayerSize(zest_texture texture, zest_uint size);
//Toggle whether or not you want the max radius to be calculated for each image/animation that you load into the texture. The max_radius will be recorded in zest_image->max_radius.
ZEST_API void zest_SetTextureMaxRadiusOnLoad(zest_texture texture, zest_bool yesno);
//Set the zest_imgui_blendtype of the texture. If you use dear imgui and are drawing render targets then this can be useful if you need to alter how the render target is blended when drawn.
//zest_imgui_blendtype_none                Just draw with standard alpha blend
//zest_imgui_blendtype_pass                Force the alpha channel to 1
//zest_imgui_blendtype_premultiply        Divide the color channels by the alpha channel
ZEST_API void zest_SetTextureImguiBlendType(zest_texture texture, zest_imgui_blendtype blend_type);
//Resize a single or storage texture.
ZEST_API void zest_TextureResize(zest_texture texture, zest_uint width, zest_uint height);
//Clear a texture
ZEST_API void zest_TextureClear(zest_texture texture);
//For single or storage textures, get the bitmap for the texture.
ZEST_API zest_bitmap_t *zest_GetTextureSingleBitmap(zest_texture texture);
//Every texture you create is stored by name in the render, use this to retrieve one by name.
ZEST_API zest_texture zest_GetTexture(const char *name);
//Returns true if the texture has a storage type of zest_texture_storage_type_bank or zest_texture_storage_type_single.
ZEST_API zest_bool zest_TextureCanTile(zest_texture texture);
//Schedule a texture to be reprocessed. This will ensure that it only gets processed (zest_ProcessTextureImages) when not in use.
ZEST_API void zest_ScheduleTextureReprocess(zest_texture texture, void(*callback)(zest_texture texture, void *user_data));
//Schedule a texture to clean up it's unused buffers. Textures are double buffered so that they can safely be changed whilst
//in use. So you can call zest_ProcessTexture to reprocess and add any new images which will do so in the unused buffer index,
//then you can call this function to schedule the cleanup of the old buffers when it's safe to do so.
ZEST_API void zest_ScheduleTextureCleanOldBuffers(zest_texture texture);
//Schedule a pipeline to be recreated. 
ZEST_API void zest_SchedulePipelineRecreate(zest_pipeline pipeline);
//Copies an area of a frame buffer such as from a render target, to a zest_texture.
ZEST_API void zest_CopyFramebufferToTexture(zest_frame_buffer_t *src_image, zest_texture texture, int src_x, int src_y, int dst_x, int dst_y, int width, int height);
//Copies an area of a zest_texture to another zest_texture
ZEST_API void zest_CopyTextureToTexture(zest_texture src_image, zest_texture target, int src_x, int src_y, int dst_x, int dst_y, int width, int height);
//Copies an area of a zest_texture to a zest_bitmap_t.
ZEST_API void zest_CopyTextureToBitmap(zest_texture src_image, zest_bitmap_t *image, int src_x, int src_y, int dst_x, int dst_y, int width, int height, zest_bool swap_channel);

// --Sampler functions
//Gets a sampler from the sampler storage in the renderer. If no match is found for the info that you pass into the sampler
//then a new one will be created.
ZEST_API zest_sampler zest_GetSampler(VkSamplerCreateInfo *info);
//-- End Images and textures

//-----------------------------------------------
//-- Fonts
//-----------------------------------------------
//Load a .zft file for use with drawing MSDF fonts
ZEST_API zest_font zest_LoadMSDFFont(const char *filename);
//Unload a zest_font and return it's memory to the allocator.
ZEST_API void zest_UnloadFont(zest_font font);
//Get a font that you previously loaded with zest_LoadMSDFFont. It's stored in the renderer with it's full path that you used to load the font
ZEST_API zest_font zest_GetFont(const char *font);
ZEST_API void zest_UploadInstanceLayerData(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data);
ZEST_API void zest_DrawFonts(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data);
//-- End Fonts

//-----------------------------------------------
//        Render_targets
//        Render targets are objects you can use to draw to. You can set up a command queue to draw to any
//        render targets that you need and then draw those render targets to the swap chain. For a more
//        advanced example of using render targets see the zest-render-targets example.
//-----------------------------------------------
//When you create a render you can pass in a zest_render_target_create_info_t where you can configure some aspects of the render target. Use this to create an initialised
//zest_render_target_create_info_t for use with this.
ZEST_API zest_render_target_create_info_t zest_RenderTargetCreateInfo();
//Creat a new render target with a name and the zest_render_target_create_info_t containing the configuration of the render target. All render targets are stored in the
//renderer by name. Returns a handle to the render target.
ZEST_API zest_render_target zest_CreateRenderTarget(const char *name, zest_render_target_create_info_t create_info);
//Create a render target for high definition rendering.
ZEST_API zest_render_target zest_CreateHDRRenderTarget(const char *name);
//Create a render target for high definition rendering that is scaled up or down to the swap chain size
ZEST_API zest_render_target zest_CreateScaledHDRRenderTarget(const char *name, float scale);
//Create a render target designed for downsampling a source image, used for blurring. Uses mip levels for
//the downsamples. If you're creating an upsampler for use with a down sampler then you can specify the downsampler input
//source which will then create the shader resources for a shader you can use for the up sampling. Otherwise
//leave this as 0
ZEST_API zest_render_target zest_CreateMippedHDRRenderTarget(const char *name, int mip_levels);
//Create a render target for linear RGBA UNORM rendering
ZEST_API zest_render_target zest_CreateSimpleRenderTarget(const char *name);
//Create a new initialised render target.
ZEST_API zest_render_target zest_NewRenderTarget();
//Frees all the resources in the frame buffer
ZEST_API void zest_CleanUpRenderTarget(zest_render_target render_target);
//Recreate the render target resources. This is called automatically whenever the window is resized (if it's not a fixed size render target). But if you need you can
//manually call this function.
ZEST_API void zest_RecreateRenderTargetResources(zest_render_target render_target, zest_index fif);
//Add a render target for post processing. This will create a new render target with the name you specify and allow you to do post processing effects. See zest-render-targets
//for a blur effect examples.
//Pass in a ratio_width and ratio_height. This will size the render target based on the size of the input source render target that you pass into the function. You can also pass
//in some user data that you might need, this gets forwarded on to a callback function that you specify. This callback function is where you can do your post processing in
//the command queue. Again see the zest-render-targets example.
ZEST_API zest_render_target zest_AddPostProcessRenderTarget(const char *name, float ratio_width, float ratio_height, zest_render_target input_source, void *user_data, void(*render_callback)(zest_render_target target, void *user_data, zest_uint fif));
//Set the post process callback in the render target. This is the same call back specified in the zest_AddPostProcessRenderTarget function.
ZEST_API void zest_SetRenderTargetPostProcessCallback(zest_render_target render_target, void(*render_callback)(zest_render_target target, void *user_data, zest_uint fif));
//Set the user data you want to associate with the render target. This is pass through to the post process callback function.
ZEST_API void zest_SetRenderTargetPostProcessUserData(zest_render_target render_target, void *user_data);
//Get render target by name
ZEST_API zest_render_target zest_GetRenderTarget(const char *name);
//Get a pointer to the render target descriptor set. You might need this when binding a pipeline to draw the render target.
ZEST_API VkDescriptorSet *zest_GetRenderTargetSamplerDescriptorSetVK(zest_render_target render_target);
//Get a pointer to the ZEST render target descriptor set. You might need this when updating a shader resource after the window is resized
ZEST_API zest_descriptor_set zest_GetRenderTargetSamplerDescriptorSet(zest_render_target render_target);
//Get a pointer the render target's source render target descriptor set. You might need this when binding the source render target in the post processing callback function
ZEST_API VkDescriptorSet *zest_GetRenderTargetSourceDescriptorSet(zest_render_target render_target);
//Get the texture from a zest_render_target
ZEST_API zest_texture zest_GetRenderTargetTexture(zest_render_target render_target);
//Get the zest_image from a zest_render_target. You could use this image to draw a sprite or billboard with it.
ZEST_API zest_image zest_GetRenderTargetImage(zest_render_target render_target);
//Get a pointer to the render target frame buffer.
ZEST_API zest_frame_buffer_t *zest_RenderTargetFramebuffer(zest_render_target render_target);
//Get a pointer to the render target frame buffer. using a specific frame in flight
ZEST_API zest_frame_buffer_t *zest_RenderTargetFramebufferByFIF(zest_render_target render_target, zest_index fif);
//A callback that's used to get the swap chain frame buffer within a command queue
ZEST_API VkFramebuffer zest_GetRendererFrameBufferCallback(zest_command_queue_draw_commands item);
//A callback that's used to get a render target frame buffer within a command queue
ZEST_API VkFramebuffer zest_GetRenderTargetFrameBufferCallback(zest_command_queue_draw_commands item);
//If you need to you can make a render target preserver it's frame buffer between renders. In other words it won't be cleared.
ZEST_API void zest_PreserveRenderTargetFrames(zest_render_target render_target, zest_bool yesno);
//Resize the render target. Applies to fixed sized render targets only
ZEST_API void zest_ResizeRenderTarget(zest_render_target render_target, zest_uint width, zest_uint height);
//Resize the render target. Applies to fixed sized render targets only
ZEST_API void zest_ScheduleRenderTargetRefresh(zest_render_target render_target);
//Change the render target sampler texture wrapping option to clamped. The render target will be refresh next frame.
ZEST_API void zest_SetRenderTargetSamplerToClamp(zest_render_target render_target);
//Change the render target sampler texture wrapping option to repeat. The render target will be refresh next frame.
ZEST_API void zest_SetRenderTargetSamplerToRepeat(zest_render_target render_target);
//Clear a specific frame in flight of the render target.
ZEST_API void zest_RenderTargetClear(zest_render_target render_target, zest_uint fif);
//You can use this function to check if a render target was changed last frame so that you can update any
//shader resources that depending on it.
ZEST_API int zest_RenderTargetWasChanged(zest_render_target render_target);
//-- End Render targets

//-----------------------------------------------
//        Main_loop_update_functions
//-----------------------------------------------
//Set the command queue that you want the next frame to be rendered by. You must call this function every frame or else
//you will only render a blank screen. Just pass the zest_command_queue that you want to use. If you initialised the editor
//with a default command queue then you can call this function with zest_SetActiveCommandQueue(ZestApp->default_command_queue)
//Use the Command queue setup and creation functions to create your own command queues. See the examples for how to do this.
ZEST_API void zest_SetActiveCommandQueue(zest_command_queue command_queue);

//-----------------------------------------------
//        Draw_Routines
//        These are used to setup drawing in Zest. There are a few builtin draw routines
//        for drawing sprites and billboards but you can set up your own for anything else.
//-----------------------------------------------
//Create a new draw routine which you can use to set up your own custom drawing
ZEST_API zest_draw_routine zest_CreateDrawRoutine(const char *name);
ZEST_API zest_draw_routine zest_CreateInstanceDrawRoutine(const char *name, zest_size instance_size, zest_uint reserve_amount);
ZEST_API int zest_LayerHasInstructionsCallback(zest_draw_routine draw_routine);
ZEST_API int zest_AlwaysRecordCallback(zest_draw_routine draw_routine);
//-- End Draw Routines

//-----------------------------------------------
//-- Draw_Layers_API
//-----------------------------------------------

//Create a new layer for instanced drawing. This just creates a standard layer with default options and callbacks, all
//you need to pass in is the size of type used for the instance struct that you'll use with whatever pipeline you setup
//to use with the layer.
ZEST_API zest_layer zest_CreateInstanceLayer(const char* name, zest_size type_size);
//Create a new layer builder which you can use to build new custom layers to draw with using instances
ZEST_API zest_layer_builder_t zest_NewInstanceLayerBuilder(zest_size type_size);
//Once you have configured your layer you can call this to create the layer ready for adding to a command queue
ZEST_API zest_layer zest_BuildInstanceLayer(const char *name, zest_layer_builder_t *builder);
//Create a layer specifically for drawing text using msdf font rendering. See the section Draw_MSDF_font_layers for commands 
//yous can use to setup and draw text. Also see the fonts example.
ZEST_API zest_layer zest_CreateFontLayer(const char *name);
// --Initialise a layer for drawing instanced data like billboards or other mesh instances. This function will create a buffer to store instance data that can
//be uploaded to the GPU for drawing each frame
ZEST_API void zest_InitialiseInstanceLayer(zest_layer layer, zest_size type_size, zest_uint instance_pool_size);
//A standar callback function you can use to draw all instances in your layer each frame. When creating your own custom layers you can override this callback
//if needed but you should find that this one covers most uses of instance drawing.
ZEST_API void zest_RecordInstanceLayerCallback(struct zest_work_queue_t *queue, void *data);
//Start a new set of draw instructs for a standard zest_layer. These were internal functions but they've been made api functions for making you're own custom
//instance layers more easily
ZEST_API void zest_StartInstanceInstructions(zest_layer layer);
//Set the layer frame in flight to the next layer. Use this if you're manually setting the current fif for the layer so
//that you can avoid uploading the staging buffers every frame and only do so when it's neccessary.
ZEST_API void zest_ResetLayer(zest_layer layer);
//Same as ResetLayer but specifically for an instance layer
ZEST_API void zest_ResetInstanceLayer(zest_layer layer);
//Record the secondary buffers of an instance layer
ZEST_API void zest_DrawInstanceLayer(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data);
//Flags a layer to manual frame in flight so you can determine when the buffers should be uploaded to the GPU
ZEST_API void zest_SetLayerToManualFIF(zest_layer layer);
//End a set of draw instructs for a standard zest_layer
ZEST_API void zest_EndInstanceInstructions(zest_layer layer);
//For layers that are manually flipping the frame in flight, we can use this to only end the instructions if the last know fif for the layer
//is not equal to the current one. Returns true if the instructions were ended false if not. If true then you can assume that the staging
//buffer for the layer can then be uploaded to the gpu. This should be called in an upload buffer callback in any custom draw routine/layer.
ZEST_API zest_bool zest_MaybeEndInstanceInstructions(zest_layer layer);
//Reset the drawing for an instance layer. This is called after all drawing is done and dispatched to the gpu
ZEST_API void zest_ResetInstanceLayerDrawing(zest_layer layer);
//Get the current amount of instances being drawn in the layer
ZEST_API zest_uint zest_GetInstanceLayerCount(zest_layer layer);
//Get the pointer to the current instance in the layer if it's an instanced based layer (meaning you're drawing instances of sprites, billboards meshes etc.)
//This will return a void* so you can cast it to whatever struct you're using for the instance data
#define zest_GetLayerInstance(type, layer, fif) (type*)layer->memory_refs.instance_ptr
//Move the pointer in memory to the next instance to write to.
ZEST_API void zest_NextInstance(zest_layer layer);
//Allocate a new zest_layer and return it's handle. This is mainly used internally but will leave it in the API for now
ZEST_API zest_layer zest_NewLayer();
//Set the viewport of a layer. This is important to set right as when the layer is drawn it needs to be clipped correctly and in a lot of cases match how the
//uniform buffer is setup
ZEST_API void zest_SetLayerViewPort(zest_layer layer, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height);
ZEST_API void zest_SetLayerScissor(zest_layer layer, int offset_x, int offset_y, zest_uint scissor_width, zest_uint scissor_height);
//Set the size of the layer. Called internally to set it to the window size. Can this be internal?
ZEST_API void zest_SetLayerSize(zest_layer layer, float width, float height);
//Get a layer from the renderer by name.
ZEST_API zest_layer zest_GetLayer(const char *name);
//Set the layer color. This is used to apply color to the sprite/font/billboard that you're drawing or you can use it in your own draw routines that use zest_layers.
//Note that the alpha value here is actually a transition between additive and alpha blending. Where 0 is the most additive and 1 is the most alpha blending. Anything
//imbetween is a combination of the 2.
ZEST_API void zest_SetLayerColor(zest_layer layer, zest_byte red, zest_byte green, zest_byte blue, zest_byte alpha);
ZEST_API void zest_SetLayerColorf(zest_layer layer, float red, float green, float blue, float alpha);
//Set the intensity of the layer. This is used to apply an alpha value to the sprite or billboard you're drawing or you can use it in your own draw routines that
//use zest_layers. Note that intensity levels can exceed 1.f to make your sprites extra bright because of pre-multiplied blending in the sprite.
ZEST_API void zest_SetLayerIntensity(zest_layer layer, float value);
//A dirty layer denotes that it's buffers have changed and therefore needs uploading to the GPU again. This is currently used for Dear Imgui layers.
ZEST_API void zest_SetLayerDirty(zest_layer layer);
//Set the user data of a layer. You can use this to extend the functionality of the layers for your own needs.
ZEST_API void zest_SetLayerUserData(zest_layer layer, void *data);
//Get the user data from the layer
#define zest_GetLayerUserData(type, layer) ((type*)layer->user_data)
//Set the global values of a push constant. This gives you 4 values that you can set for all draw calls in a layer that will immediately 
//apply to the next frame render.
ZEST_API void zest_SetLayerGlobalPushConstants(zest_layer layer, float x, float y, float z, float w);
//Clear the draw sets in a layer. Draw sets are all the descriptor sets used when binding a pipeline. You would generally use zest_GetDescriptorSetsForBinding
//before binding a pipeline for draw calls and then after you can call this to reset the draw sets list again.
ZEST_API void zest_ClearLayerDrawSets(zest_layer layer);
//-- End Draw Layers

//-----------------------------------------------
//        Draw_sprite_layers
//        Built in layer for drawing 2d sprites on the screen. To add this type of layer to
//        a command queue see zest_NewBuiltinLayerSetup or zest_CreateBuiltinSpriteLayer
//-----------------------------------------------
//Draw a sprite on the screen using the zest_layer and zest_image you pass to the function. You must call zest_SetSpriteDrawing for the layer and the texture where the image exists.
//You must call zest_SetInstanceDrawing before calling this function.
//x, y: Coordinates on the screen to position the sprite at.
//r: Rotation of the sprite in radians
//sx, sy: The size of the sprite in pixels
//hx, hy: The handle of the sprite
//alignment: If you're stretching the sprite then this should be a normalised 2d vector. You zest_Pack16bit to pack a vector into a zest_uint
//stretch: the stretch factor of the sprite.
ZEST_API void zest_DrawSprite(zest_layer layer, zest_image image, float x, float y, float r, float sx, float sy, float hx, float hy, zest_uint alignment, float stretch);
//An alternative draw sprite function where you can pass in the size and handle pre-packed into a U64 as 16bit scaled floats
ZEST_API void zest_DrawSpritePacked(zest_layer layer, zest_image image, float x, float y, float r, zest_u64 size_handle, zest_uint alignment, float stretch);
//Draw a textured sprite which is basically a textured rect if you just want to repeat a image across an area. The texture that's used for the image should be
//zest_texture_storage_type_bank or zest_texture_storage_type_single. You must call zest_SetSpriteDrawing for th layer and the texture where the image exists.
//x,y:                    The coordinates on the screen of the top left corner of the sprite.
//width, height:        The size in pixels of the area for the textured sprite to cover.
//scale_x, scale_y:        The scale of the uv texture coordinates
//offset_x, offset_y:    The offset of the uv coordinates
ZEST_API void zest_DrawTexturedSprite(zest_layer layer, zest_image image, float x, float y, float width, float height, float scale_x, float scale_y, float offset_x, float offset_y);
//-- End Draw sprite layers

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
ZEST_API void zest_SetInstanceDrawing(zest_layer layer, zest_shader_resources shader_resources, zest_texture *textures, zest_uint texture_count, zest_pipeline_template pipeline);
//Draw all the contents in a buffer. You can use this if you prepare all the instance data elsewhere in your code and then want
//to just dump it all into the staging buffer of the layer in one go. This will move the instance pointer in the layer to the next point
//in the buffer as well as bump up the instance count by the amount you pass into the function. The instance buffer will be grown if
//there is not enough room.
//Note that the struct size of the data you're copying MUST be the same size as the layer->struct_size.
ZEST_API zest_draw_buffer_result zest_DrawInstanceBuffer(zest_layer layer, void *src, zest_uint amount);
//In situations where you write directly to a staging buffer you can use this function to simply tell the draw instruction
//how many instances should be drawn. You will still need to call zest_SetInstanceDrawing
ZEST_API void zest_DrawInstanceInstruction(zest_layer layer, zest_uint amount);
//Set the viewport and scissors of the next draw instructions for a layer. Otherwise by default it will use either the screen size
//of or the viewport size you set with zest_SetLayerViewPort
ZEST_API void zest_SetLayerDrawingViewport(zest_layer layer, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height);

//-----------------------------------------------
//        Draw_billboard_layers
//        Built in layer for drawing 3d sprites, or billboards. These can either always face the camera or be
//        aligned to a vector, or even both depending on what you need. You'll need a uniform buffer with the
//        appropriate projection and view matrix for 3d. See zest-instance-layers for examples.
//-----------------------------------------------
//Draw a billboard in 3d space using the zest_layer (must be a built in billboard layer) and zest_image. Note that you will need to use a uniform buffer that sets an appropriate. You must call
//zest_SetInstanceDrawing before calling this function
//projection and view matrix.  You must call zest_SetSpriteDrawing for the layer and the texture where the image exists.
//Position:                Should be a pointer to 3 floats representing the x, y and z coordinates to draw the sprite at.
//alignment:            A normalised 3d vector packed into a 8bit snorm uint. This is the alignment of the billboard when using stretch.
//angles:                A pointer to 3 floats representing the pitch, yaw and roll of the billboard
//handle:                A pointer to 2 floats representing the handle of the billboard which is the offset from the position, max value should be 128. 
//stretch:                How much to stretch the billboard along it's alignment.
//alignment_type:        This is a bit flag with 2 bits 00. 00 = align to the camera. 11 = align to the alignment vector. 10 = align to the alignment vector and the camera.
//sx, sy:                The size of the sprite in 3d units. Max scale value should be 256
//Note that because scale and handle is packed into 16 bit floats, the max value for scale is 256 and the max value for handle is 128
ZEST_API void zest_DrawBillboard(zest_layer layer, zest_image image, float position[3], zest_uint alignment, float angles[3], float handle[2], float stretch, zest_uint alignment_type, float sx, float sy);
//This version of draw billboard lets you pass in a pre-packed scale_handle to save computation
ZEST_API void zest_DrawBillboardPacked(zest_layer layer, zest_image image, float position[3], zest_uint alignment, float angles[3], zest_u64 scale_handle, float stretch, zest_uint alignment_type);
//A simplified version of zest_DrawBillboard where you only need to set the position, rotation and size of the billboard. The alignment will always be set to face the camera.
//Note that because scale is packed into 16 bit floats, the max value for scale is 256
ZEST_API void zest_DrawBillboardSimple(zest_layer layer, zest_image image, float position[3], float angle, float sx, float sy);
//--End Draw billboard layers

//-----------------------------------------------
//        Draw_3D_Line_layers
//        Built in layer for drawing 3d lines.
//-----------------------------------------------
//Set descriptor set and pipeline for any calls to zest_Draw3DLine that come after it. You must call this function if you want to do any line drawing, and you
//must call it again if you wish to switch either the texture, descriptor set or pipeline to do the drawing. Everytime you call this function it creates a new draw instruction
//in the layer for drawing billboards so each call represents a separate draw call in the render. So if you just call this function once you can call zest_Draw3DLine as many times
//as you want and they will all be drawn with a single draw call.
//Pass in the zest_layer, zest_texture, zest_descriptor_set and zest_pipeline. A few things to note:
//1) The descriptor layout used to create the descriptor set must match the layout used in the pipeline.
//2) You can pass 0 in the descriptor set and it will just use the default descriptor set used in the texture.
ZEST_API void zest_Set3DLineDrawing(zest_layer line_layer, zest_shader_resources shader_resources, zest_pipeline_template pipeline);
//Draw a 3d line. pass in the layer to draw to and the 3d start and end points plus the width of the line.
ZEST_API void zest_Draw3DLine(zest_layer layer, float start_point[3], float end_point[3], float width);
//--End Draw line layers

//-----------------------------------------------
//        Draw_MSDF_font_layers
//        Draw fonts at any scale using multi channel signed distance fields. This uses a zest font format
//        ".zft" which contains a character map with uv coords and a png containing the MSDF image for
//        sampling. You can use a simple application for generating these files in the examples:
//        zest-msdf-font-maker
//-----------------------------------------------
//Before drawing any fonts you must call this function in order to set the font you want to use. the zest_font handle can be used to get the vk_descriptor_set and pipeline. See
//zest-fonts for an example.
ZEST_API void zest_SetMSDFFontDrawing(zest_layer font_layer, zest_font font);
//Set the shadow parameters of your font drawing. This only applies to the current call of zest_SetMSDFFontDrawing. For each text that you want different settings for you must
//call a separate zest_SetMSDFFontDrawing to start a new font drawing instruction.
ZEST_API void zest_SetMSDFFontShadow(zest_layer font_layer, float shadow_length, float shadow_smoothing, float shadow_clipping);
ZEST_API void zest_SetMSDFFontShadowColor(zest_layer font_layer, float red, float green, float blue, float alpha);
//Alter the parameters in the shader to tweak the font details and how it's drawn. This can be useful especially for drawing small scaled fonts.
//bleed: Simulates ink bleeding so higher numbers fatten the font but in a more subtle way then altering the thickness.
//thickness: Makes the font bolder with higher numbers. around 5.f
//aa_factor: The amount to alias the font. Lower numbers work better for smaller fonts
//radius: Scale the distance measured to the font edge. Higher numbers make the font thinner
ZEST_API void zest_TweakMSDFFont(zest_layer font_layer, float bleed, float thickness, float aa_factor, float radius);
//The following functions just let you tweak an individual parameter of the font drawing
ZEST_API void zest_SetMSDFFontBleed(zest_layer font_layer, float bleed);
ZEST_API void zest_SetMSDFFontThickness(zest_layer font_layer, float thickness);
ZEST_API void zest_SetMSDFFontAAFactor(zest_layer font_layer, float aa_factor);
ZEST_API void zest_SetMSDFFontRadius(zest_layer font_layer, float radius);
//Draw a single line of text using an MSDF font. You must call zest_SetMSDFFontDrawing before calling this command to specify which font you want to use to draw with and the zest_layer
//that you pass to the function must match the same layer set with that command.
//text:                    The string that you want to display
//x, y:                    The position on the screen.
//handle_x, handle_y    How the text is justified. 0.5, 0.5 would center the text at the position. 0, 0 would left justify and 1, 0 would right justify.
//size:                    The size of the font in pixels.
//letter_spacing        The amount of spacing imbetween letters
ZEST_API float zest_DrawMSDFText(zest_layer font_layer, const char *text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing);
//Draw a paragraph of text using an MSDF font. You must call zest_SetMSDFFontDrawing before calling this command to specify which font you want to use to draw with and the zest_layer
//that you pass to the function must match the same layer set with that command. Use \n to start new lines in the paragraph.
//text:                    The string that you want to display
//x, y:                    The position on the screen.
//handle_x, handle_y    How the text is justified. 0.5, 0.5 would center the text at the position. 0, 0 would left justify and 1, 0 would right justify.
//size:                    The size of the font in pixels.
//letter_spacing        The amount of spacing imbetween letters
ZEST_API float zest_DrawMSDFParagraph(zest_layer font_layer, const char *text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing, float line_height);
//Get the width of a line of text in pixels given the zest_font, test font size and letter spacing that you pass to the function.
ZEST_API float zest_TextWidth(zest_font font, const char *text, float font_size, float letter_spacing);
//-- End Draw MSDF font layers

//-----------------------------------------------
//        Draw_mesh_layers
//        Mesh layers let you upload a vertex and index buffer to draw meshes. I set this up primarily for
//        use with Dear ImGui
//-----------------------------------------------
//A default callback function that is used to upload the staging buffers containing the vertex and index buffers to the GPU buffers
//You don't really need to call this manually as it's a callback that's assigned in the draw routine when you call zest_CreateBuiltinMeshLayer or zest_NewMeshLayer.
ZEST_API void zest_UploadMeshBuffersCallback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);
//Get the vertex staging buffer. You'll need to get the staging buffers to copy your mesh data to or even just record mesh data directly to the staging buffer
ZEST_API zest_buffer zest_GetVertexWriteBuffer(zest_layer layer);
//Get the index staging buffer. You'll need to get the staging buffers to copy your mesh data to or even just record mesh data directly to the staging buffer
ZEST_API zest_buffer zest_GetIndexWriteBuffer(zest_layer layer);
//Grow the mesh vertex buffers. You must update the buffer->memory_in_use so that it can decide if a buffer needs growing
ZEST_API void zest_GrowMeshVertexBuffers(zest_layer layer);
//Grow the mesh index buffers. You must update the buffer->memory_in_use so that it can decide if a buffer needs growing
ZEST_API void zest_GrowMeshIndexBuffers(zest_layer layer);
//Set the mesh drawing specifying any texture, descriptor set and pipeline that you want to use for the drawing
ZEST_API void zest_SetMeshDrawing(zest_layer layer, zest_shader_resources shader_resources, zest_texture texture, zest_pipeline_template pipeline);
//Helper funciton Push a vertex to the vertex staging buffer. It will automatically grow the buffers if needed
ZEST_API void zest_PushVertex(zest_layer layer, float pos_x, float pos_y, float pos_z, float intensity, float uv_x, float uv_y, zest_color color, zest_uint parameters);
//Helper funciton Push an index to the index staging buffer. It will automatically grow the buffers if needed
ZEST_API void zest_PushIndex(zest_layer layer, zest_uint offset);
//Draw a textured plan in 3d. You will need an appropriate 3d uniform buffer for this, see zest-instance-layers for an example usage for this function.
//You must call zest_SetMeshDrawing before calling this function and the layer must match and the image you pass must exist in the same texture.
//x, y, z                The position of the plane
//width, height            The size of the plane. If you make this match the zfar value in the projection matrix then it will effectively be an infinite plane.
//scale_x, scale_y        The scale of the texture repetition
//offset_x, offset_y    The texture uv offset
ZEST_API void zest_DrawTexturedPlane(zest_layer layer, zest_image image, float x, float y, float z, float width, float height, float scale_x, float scale_y, float offset_x, float offset_y);
//--End Draw mesh layers

//-----------------------------------------------
//        Draw_instance_mesh_layers
//        Instance mesh layers are for creating meshes and then drawing instances of them. It should be
//        one mesh per layer (and obviously many instances of that mesh can be drawn with the layer).
//        Very basic stuff currently, I'm just using them to create 3d widgets I can use in TimelineFX
//        but this can all be expanded on for general 3d models in the future.
//-----------------------------------------------
ZEST_API void zest_RecordInstanceMeshLayer(zest_layer layer, zest_uint fif);
ZEST_API void zest_SetInstanceMeshDrawing(zest_layer layer, zest_shader_resources shader_resources, zest_pipeline_template pipeline);
//Push a zest_vertex_t to a mesh. Use this and PushMeshTriangle to build a mesh ready to be added to an instance mesh layer
ZEST_API void zest_PushMeshVertex(zest_mesh_t *mesh, float pos_x, float pos_y, float pos_z, zest_color color);
//Push an index to a mesh to build triangles
ZEST_API void zest_PushMeshIndex(zest_mesh_t *mesh, zest_uint index);
//Rather then PushMeshIndex you can call this to add three indexes at once to build a triangle in the mesh
ZEST_API void zest_PushMeshTriangle(zest_mesh_t *mesh, zest_uint i1, zest_uint i2, zest_uint i3);
//Free the memeory used for the mesh. You can free the mesh once it's been added to the layer
ZEST_API void zest_FreeMesh(zest_mesh_t *mesh);
//Set the position of the mesh in it's transform matrix
ZEST_API void zest_PositionMesh(zest_mesh_t *mesh, zest_vec3 position);
//Rotate a mesh by the given pitch, yaw and roll values (in radians)
ZEST_API zest_matrix4 zest_RotateMesh(zest_mesh_t *mesh, float pitch, float yaw, float roll);
//Transform a mesh by the given pitch, yaw and roll values (in radians), position x, y, z and scale sx, sy, sz
ZEST_API zest_matrix4 zest_TransformMesh(zest_mesh_t *mesh, float pitch, float yaw, float roll, float x, float y, float z, float sx, float sy, float sz);
//Initialise a new bounding box to 0
ZEST_API zest_bounding_box_t zest_NewBoundingBox();
//Calculate the bounding box of a mesh and return it
ZEST_API zest_bounding_box_t zest_GetMeshBoundingBox(zest_mesh_t *mesh);
//Add all the vertices and indices of a mesh to another mesh to combine them into a single mesh.
ZEST_API void zest_AddMeshToMesh(zest_mesh_t *dst_mesh, zest_mesh_t *src_mesh);
//Set the group id for every vertex in the mesh. This can be used in the shader to identify different parts of the mesh and do different shader stuff with them.
ZEST_API void zest_SetMeshGroupID(zest_mesh_t *mesh, zest_uint group_id);
//Add a mesh to an instanced mesh layer. Existing vertex data in the layer will be deleted.
ZEST_API void zest_AddMeshToLayer(zest_layer layer, zest_mesh_t *src_mesh);
//Get the size in bytes for the vertex data in a mesh
ZEST_API zest_size zest_MeshVertexDataSize(zest_mesh_t *mesh);
//Get the size in bytes for the index data in a mesh
ZEST_API zest_size zest_MeshIndexDataSize(zest_mesh_t *mesh);
//Draw an instance of a mesh with an instanced mesh layer. Pass in the position, rotations and scale to transform the instance.
//You must call zest_SetInstanceDrawing before calling this function as many times as you need.
ZEST_API void zest_DrawInstancedMesh(zest_layer mesh_layer, float pos[3], float rot[3], float scale[3]);
//Create a cylinder mesh of given number of sides, radius and height. Set cap to 1 to cap the cylinder.
ZEST_API zest_mesh_t zest_CreateCylinderMesh(int sides, float radius, float height, zest_color color, zest_bool cap);
//Create a cone mesh of given number of sides, radius and height.
ZEST_API zest_mesh_t zest_CreateCone(int sides, float radius, float height, zest_color color);
//Create a uv sphere mesh made up using a number of horizontal rings and vertical sectors of a give radius.
ZEST_API zest_mesh_t zest_CreateSphere(int rings, int sectors, float radius, zest_color color);
//Create a cube mesh of a given size.
ZEST_API zest_mesh_t zest_CreateCube(float size, zest_color color);
//Create a flat rounded rectangle of a give width and height. Pass in the radius to use for the corners and number of segments to use for the corners.
ZEST_API zest_mesh_t zest_CreateRoundedRectangle(float width, float height, float radius, int segments, zest_bool backface, zest_color color);
//--End Instance Draw mesh layers

//-----------------------------------------------
//        Compute_shaders
//        Build custom compute shaders and integrate into a command queue or just run separately as you need.
//        See zest-compute-example for a full working example
//-----------------------------------------------
//Create a blank ready-to-build compute object and store by name in the renderer.
ZEST_API zest_compute zest__create_compute(const char *name);
//To build a compute shader pipeline you can use a zest_compute_builder_t and corresponding commands to add the various settings for the compute object
ZEST_API zest_compute_builder_t zest_BeginComputeBuilder();
ZEST_API void zest_SetComputeBindlessLayout(zest_compute_builder_t *builder, zest_set_layout bindless_layout);
ZEST_API void zest_AddComputeSetLayout(zest_compute_builder_t *builder, zest_set_layout layout);
//Add a shader to the compute builder. This will be the shader that is executed on the GPU. Pass a file path where to find the shader.
ZEST_API zest_index zest_AddComputeShader(zest_compute_builder_t *builder, zest_shader shader);
//If you're using a push constant then you can set it's size in the builder here.
ZEST_API void zest_SetComputePushConstantSize(zest_compute_builder_t *builder, zest_uint size);
//You must set a command buffer update callback. This is called when the command queue tries to execute the compute shader. Here you can bind buffers and upload
//data to the gpu for use by the compute shader and then dispatch the shader. See zest-compute-example.
ZEST_API void zest_SetComputeCommandBufferUpdateCallback(zest_compute_builder_t *builder, void(*command_buffer_update_callback)(zest_compute compute, VkCommandBuffer command_buffer));
//If you require any custom clean up done for the compute shader then you can sepcify a callback for that here.
ZEST_API void zest_SetComputeExtraCleanUpCallback(zest_compute_builder_t *builder, void(*extra_cleanup_callback)(zest_compute compute));
//Set any pointer to custom user data here. You will be able to access this in the callback functions.
ZEST_API void zest_SetComputeUserData(zest_compute_builder_t *builder, void *data);
//Sets the compute shader to manually record so you have to specify when the compute shader should be recorded
ZEST_API void zest_SetComputeManualRecord(zest_compute_builder_t *builder);
//Sets the compute shader to use a primary command buffer recorder so that you can use it with the zest_RunCompute command.
//This means that you will not be able to use this compute shader in a frame loop alongside other render routines.
ZEST_API void zest_SetComputePrimaryRecorder(zest_compute_builder_t *builder);
//Once you have finished calling the builder commands you will need to call this to actually build the compute shader. Pass a pointer to the builder and the zest_compute
//handle that you got from calling zest__create_compute. You can then use this handle to add the compute shader to a command queue with zest_NewComputeSetup in a
//command queue context (see the section on Command queue setup and creation)
ZEST_API zest_compute zest_FinishCompute(zest_compute_builder_t *builder, const char *name);
//Attach a render target to a compute shader. Currently un used. Need example
ZEST_API void zest_ComputeAttachRenderTarget(zest_compute compute, zest_render_target render_target);
//You don't have to execute the compute shader as part of a command queue, you can also use this command to run the compute shader separately
ZEST_API void zest_RunCompute(zest_compute compute);
//After calling zest_RunCompute you can call this command to wait for a message from the GPU that it's finished executing
ZEST_API void zest_WaitForCompute(zest_compute compute);
//If at anytime you resize buffers used in a compute shader then you will need to update it's descriptor infos so that the compute shader connects to the new
//resized buffer. You can call this function which will just call the callback function you specified when creating the compute shader.
ZEST_API void zest_UpdateComputeDescriptors(zest_compute compute, zest_uint fif);
//Send the push constants defined in a compute shader. Use inside a compute update command buffer callback function
ZEST_API void zest_SendComputePushConstants(VkCommandBuffer command_buffer, zest_compute compute);
//Send custom push constants. Use inside a compute update command buffer callback function. The push constatns you pass in to the 
//function must be the same size that you set when creating the compute shader
ZEST_API void zest_SendCustomComputePushConstants(VkCommandBuffer command_buffer, zest_compute compute, const void *push_constant);
//Helper function to dispatch a compute shader so you can call this instead of vkCmdDispatch. Specify a command buffer for use in one off dispataches
ZEST_API void zest_DispatchCompute(VkCommandBuffer command_buffer, zest_compute compute, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z);
//Reset compute when manual_fif is being used. This means that you can chose when you want a compute command buffer
//to be recorded again. 
ZEST_API void zest_ResetCompute(zest_compute compute);
//Default always true condition for recording compute command buffers
ZEST_API int zest_ComputeConditionAlwaysTrue(zest_compute compute);
//--End Compute shaders

//-----------------------------------------------
//        Events_and_States
//-----------------------------------------------
//Returns true if the swap chain was recreated last frame. The swap chain will mainly be recreated if the window size changes
ZEST_API zest_bool zest_SwapchainWasRecreated(void);
//--End Events and States

//-----------------------------------------------
//        Timer_functions
//        This is a simple API for a high resolution timer. You can use this to implement fixed step
//        updating for your logic in the main loop, plus for anything else that you need to time.
//-----------------------------------------------
ZEST_API zest_timer zest_CreateTimer(double update_frequency);                                  //Create a new timer and return its handle
ZEST_API void zest_FreeTimer(zest_timer timer);                                                 //Free a timer and its memory
ZEST_API void zest_TimerSetUpdateFrequency(zest_timer timer, double update_frequency);          //Set the update frequency for timing loop functions, accumulators and such
ZEST_API void zest_TimerSetMaxFrames(zest_timer timer, double frames);                          //Set the maximum amount of frames that can pass each update. This helps avoid simulations blowing up
ZEST_API void zest_TimerReset(zest_timer timer);                                                //Set the clock time to now
ZEST_API double zest_TimerDeltaTime(zest_timer timer);                                          //The amount of time passed since the last tick
ZEST_API void zest_TimerTick(zest_timer timer);                                                 //Update the delta time
ZEST_API double zest_TimerUpdateTime(zest_timer timer);                                         //Gets the update time (1.f / update_frequency)
ZEST_API double zest_TimerFrameLength(zest_timer timer);                                        //Gets the update_tick_length (1000.f / update_frequency)
ZEST_API double zest_TimerAccumulate(zest_timer timer);                                         //Accumulate the amount of time that has passed since the last render
ZEST_API int zest_TimerPendingTicks(zest_timer timer);                                          //Returns the number of times the update loop will run this frame.
ZEST_API void zest_TimerUnAccumulate(zest_timer timer);                                         //Unaccumulate 1 tick length from the accumulator. While the accumulator is more then the tick length an update should be done
ZEST_API zest_bool zest_TimerDoUpdate(zest_timer timer);                                        //Return true if accumulator is more or equal to the update_tick_length
ZEST_API double zest_TimerLerp(zest_timer timer);                                               //Return the current tween/lerp value
ZEST_API void zest_TimerSet(zest_timer timer);                                                  //Set the current tween value
ZEST_API double zest_TimerUpdateFrequency(zest_timer timer);                                    //Get the update frequency set in the timer
ZEST_API zest_bool zest_TimerUpdateWasRun(zest_timer timer);                                    //Returns true if an update loop was run
//Help macros for starting/ending an update loop if you prefer.
#define zest_StartTimerLoop(timer) 	zest_TimerAccumulate(timer);   \
		int pending_ticks = zest_TimerPendingTicks(timer);	\
		while (zest_TimerDoUpdate(timer)) {
#define zest_EndTimerLoop(timer) zest_TimerUnAccumulate(timer); \
	} \
	zest_TimerSet(timer);
//--End Timer Functions

//-----------------------------------------------
//        General_Helper_functions
//-----------------------------------------------
//Read a file from disk into memory. Set terminate to 1 if you want to add \0 to the end of the file in memory
ZEST_API char* zest_ReadEntireFile(const char *file_name, zest_bool terminate);
//Get the current swap chain frame buffer
ZEST_API VkFramebuffer zest_GetRendererFrameBuffer(zest_command_queue_draw_commands item);
//Get a descriptor set layout by name.
ZEST_API VkDescriptorSetLayout *zest_GetDescriptorSetLayoutVK(const char *name);
//Get a descriptor set layout by name.
ZEST_API zest_set_layout zest_GetDescriptorSetLayout(const char *name);
//Get the swap chain extent which will basically be the size of the window returned in a VkExtend2d struct.
ZEST_API VkExtent2D zest_GetSwapChainExtent(void);
//Get the window size in a VkExtent2d. In most cases this is the same as the swap chain extent.
ZEST_API VkExtent2D zest_GetWindowExtent(void);
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
//Send push constants. For use inside a draw routine callback function. pass in the pipeline,
//and a pointer to the data containing the push constants. The data MUST match the push constant range in the pipeline
ZEST_API void zest_SendPushConstants(VkCommandBuffer command_buffer, zest_pipeline pipeline, void *data);
//Helper function to send the standard zest_push_constants_t push constants struct.
ZEST_API void zest_SendStandardPushConstants(VkCommandBuffer command_buffer, zest_pipeline_t *pipeline_layout, void *data);
//Helper function to record the vulkan command vkCmdDraw. Will record with the current command buffer being used in the active command queue. For use inside
//a draw routine callback function
ZEST_API void zest_Draw(VkCommandBuffer command_buffer, zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance);
//Helper function to record the vulkan command vkCmdDrawIndexed. Will record with the current command buffer being used in the active command queue. For use inside
//a draw routine callback function
ZEST_API void zest_DrawIndexed(VkCommandBuffer command_buffer, zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance);
//Enable vsync so that the frame rate is limited to the current monitor refresh rate. Will cause the swap chain to be rebuilt.
ZEST_API void zest_EnableVSync(void);
//Disable vsync so that the frame rate is not limited to the current monitor refresh rate, frames will render as fast as they can. Will cause the swap chain to be rebuilt.
ZEST_API void zest_DisableVSync(void);
//Log the current FPS to the console once every second
ZEST_API void zest_LogFPSToConsole(zest_bool yesno);
//Return a pointer to the window handle stored in the zest_window_t. This could be anything so it's up to you to cast to the right data type. For example, if you're
//using GLFW then you would cast it to a GLFWwindow*
ZEST_API void *zest_Window(void);
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
//--End General Helper functions

//-----------------------------------------------
//Debug_Helpers
//-----------------------------------------------
//Convert one of the standard draw routine callback functions to a string and return it.
ZEST_API const char *zest_DrawFunctionToString(void *function);
//About all the command queues to the console so that you can see the how they're set up.
ZEST_API void zest_OutputQueues();
//About all of the current memory usage that you're using in the renderer to the console. This won't include any allocations you've done elsewhere using you own allocation
//functions or malloc/virtual_alloc etc.
ZEST_API void zest_OutputMemoryUsage();
//Set the path to the log where vulkan validation messages and other log messages can be output to. If this is not set and ZEST_VALIDATION_LAYER is 1 then validation messages and other log
//messages will be output to the console if it's available.
ZEST_API zest_bool zest_SetErrorLogPath(const char *path);
//--End Debug Helpers

#ifdef __cplusplus
}
#endif

#endif // ! ZEST_POCKET_RENDERER
