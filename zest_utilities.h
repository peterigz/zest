// zest_utilities.h
// A collection of single-file header implementations for Zest.

//NOTE: this is not an official part of zest, this is just to support the examples to show how Zest
//can be used. Overtime some elements maybe made an official part of zest.h and moved there instead.

//To include this file in a project, include it in a c or c++ file with the necessary defines to include
//the implementations that you want to use or ZEST_ALL_UTILITIES to include everything (note that sdl and
//glfw helper functions will only be included if those libraries are included as they just depend on the
//presence of the respective version defines in those libraries

/*
tiny_ktx.h			Load and save ktx files		- Deano Calver https://github.com/DeanoC/tiny_ktx
* To use define ZEST_KTX_IMPLEMENTATION before including zest.h in a c/c++ file. Only do this once.
	- [Zest_ktx_helper_functions]
	- [Zest_ktx_helper_implementation]
msdf.h				Create msdf bitmaps for font rendering - From https://github.com/exezin/msdf-c
* To use define MSDF_IMPLEMENTATION or ZEST_MSDF_IMPLEMENTATION before including zest_utilities.h in a c/c++ file. Only do this once.
  Requires stb_truetype.h
	- [msdf_fonts]
Bitmap and image collection functions
	-[Image_collection]
Loader function for GLTF files using cglft.h - https://github.com/jkuhlmann/cgltf
	-[gltf_loader]
Helpers for creating simple mesh primitives with draw batch objects
	-[Mesh_helpers]
*/    

#ifdef ZEST_ALL_UTILITIES_IMPLEMENTATION
#define ZEST_MSDF_IMPLEMENTATION
#define ZEST_KTX_IMPLEMENTATION
#define ZEST_IMAGES_IMPLEMENTATION
#define ZEST_IMAGE_WRITE_IMPLEMENTATION
#define ZEST_MESH_HELPERS_IMPLEMENTATION
#define ZEST_GLTF_LOADER
#endif

#if !defined(ZEST_UTILITIES_MALLOC) && !defined(ZEST_UTILITIES_REALLOC) && !defined(ZEST_UTILITIES_FREE)
#include <stdlib.h>
#define ZEST_UTILITIES_MALLOC(sz) malloc(sz)
#define ZEST_UTILITIES_REALLOC(ptr, sz) realloc(ptr, sz)
#define ZEST_UTILITIES_FREE(ptr) free(ptr)
#endif

#ifndef ZEST_UTILITIES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum zest_image_collection_flag_bits {
	zest_image_collection_flag_none = 0,
	zest_image_collection_flag_initialised = 1 << 0,
	zest_image_collection_flag_is_cube_map = 1 << 1,
	zest_image_collection_flag_ktx_data = 1 << 2,
	zest_image_collection_flag_atlas = 1 << 3,
	zest_image_collection_flag_is_array = 1 << 4,
} zest_image_collection_flag_bits;
typedef zest_uint zest_image_collection_flags;

typedef struct zest_bitmap_meta_t {
	int width;
	int height;
	int channels;
	int bytes_per_pixel;
	int stride;
	zest_size size;
	zest_size offset;
	zest_format format;
} zest_bitmap_meta_t;

typedef struct zest_bitmap_t {
	zest_bitmap_meta_t meta;
	zest_byte *data;
	zest_bool is_imported;
} zest_bitmap_t;

typedef struct zest_bitmap_array_t {
	zest_uint size_of_array;
	zest_bitmap_meta_t *meta;
	size_t total_mem_size;
	zest_byte *data;
} zest_bitmap_array_t;

typedef struct zest_image_collection_t {
	zest_format format;
	zest_bitmap_t *image_bitmaps;
	zest_atlas_region_t *regions;
	zest_bitmap_array_t bitmap_array;
	zest_buffer_image_copy_t *buffer_copy_regions;
	zest_uint packed_border_size;
	zest_uint max_images;
	zest_uint image_count;
	zest_uint array_layers;
	zest_image_collection_flags flags;
} zest_image_collection_t;

typedef struct zest_imgui_image_t {
	int magic;
	zest_atlas_region_t *image;
	zest_pipeline_template pipeline;
	void *push_constants;
} zest_imgui_image_t;

//Tiny ktx Header

#include <stdbool.h>

typedef struct zest_ktx_user_context_t {
	zest_device device;
	FILE *file;
} zest_ktx_user_context_t;

ZEST_PRIVATE void zest__tinyktxCallbackError(void *user, char const *msg);
ZEST_PRIVATE void *zest__tinyktxCallbackAlloc(void *user, size_t size);
ZEST_PRIVATE void zest__tinyktxCallbackFree(void *user, void *data);
ZEST_PRIVATE size_t zest__tinyktxCallbackRead(void *user, void *data, size_t size);
ZEST_PRIVATE bool zest__tinyktxCallbackSeek(void *user, int64_t offset);
ZEST_PRIVATE int64_t zest__tinyktxCallbackTell(void *user);
ZEST_API zest_image_collection_t zest__load_ktx(zest_device device, const char *file_path);
ZEST_API zest_image_handle zest_LoadKTX(zest_device device, const char *name, const char *file_name);

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
} zest_font_resources_t;

typedef struct zest_font_uniform_buffer_data_t {
    zest_matrix4 view;
    zest_matrix4 proj;
    zest_vec2 screen_size;
} zest_font_uniform_buffer_data_t;

typedef struct zest_font_character_t {
	zest_atlas_region_t region;
    float x_offset; 
	float y_offset;
    float width; 
	float height;
    float x_advance;
	zest_u64 uv_packed;
	zest_character_flags flags;
} zest_font_character_t;

typedef struct zest_msdf_font_settings_t {
	zest_vec4 transform;
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
	zest_image_collection_t font_atlas;
	zest_image_handle font_image;
	zest_font_character_t characters[256];
	zest_uint font_binding_index;
	zest_msdf_font_settings_t settings;
	zest_context context;
	float size;
	float sdf_range;
	float y_max_offset;
	zest_bool is_loaded_from_file;
} zest_msdf_font_t;

typedef struct zest_msdf_font_file_t {
	zest_uint magic;
	zest_msdf_font_t font_details;
	zest_uint png_size;
} zest_msdf_font_file_t;

typedef struct zest_font_instance_t {           //48 bytes
	zest_vec2 position;                   		//The position of the sprite with rotation in w and stretch in z
	zest_u64 uv;                                //The UV coords of the image in the texture packed into a u64 snorm (4 16bit floats)
	zest_color_t color;							//The color of the font instance in rgba8
	zest_uint size; 		                    //Size of the sprite in pixels and the handle packed into a u64 (4 16bit floats)
	zest_uint texture_array;             		//Index in the texture array
	zest_uint padding;							//Pad up to 48 bytes
} zest_font_instance_t;

typedef struct zest_stbi_mem_context_t {
	int last_pos;
	int file_size;
	void *context;
} zest_stbi_mem_context_t ;

ZEST_PRIVATE void* zest__msdf_allocation(size_t size, void* ctx);
ZEST_PRIVATE void zest__msdf_free(void* ptr, void* ctx);
ZEST_API zest_font_resources_t zest_CreateFontResources(zest_context context, const char *vert_shader, const char *frag_shader);
ZEST_API zest_layer_handle zest_CreateFontLayer(zest_context context, const char *name, zest_uint max_characters);
ZEST_API zest_msdf_font_t zest_CreateMSDF(zest_context context, const char *filename, zest_uint font_sampler_handle, float font_size, float sdf_range);
ZEST_API void zest_SaveMSDF(zest_msdf_font_t *font, const char *filename);
ZEST_API zest_msdf_font_t zest_LoadMSDF(zest_context context, const char *filename, zest_uint font_sampler_sampler);
ZEST_API void zest_UpdateFontTransform(zest_msdf_font_t *font);
ZEST_API void zest_SetFontTransform(zest_msdf_font_t *font, float transform[4]);
ZEST_API void zest_SetFontSettings(zest_msdf_font_t *font, float inner_bias, float outer_bias, float smoothness, float gamma);
ZEST_API void zest_SetFontShadowColor(zest_msdf_font_t *font, float r, float g, float b, float a);
ZEST_API void zest_SetFontShadowOffset(zest_msdf_font_t *font, float x, float y);
ZEST_API void zest_FreeFont(zest_msdf_font_t *font);
ZEST_API float zest_TextWidth(zest_msdf_font_t *font, const char* text, float font_size, float letter_spacing);
ZEST_API void zest_SetMSDFFontDrawing(zest_layer layer, zest_msdf_font_t *font, zest_font_resources_t *font_resources);
ZEST_API float zest_DrawMSDFText(zest_layer layer, float x, float y, float handle_x, float handle_y, float size, float letter_spacing, const char* format, ...);

//Bitmaps and images
ZEST_API zest_bitmap_t zest_CreateBitmapFromRawBuffer(void *pixels, int size, int width, int height, zest_format format);
ZEST_API void zest_ConvertBitmap(zest_bitmap_t *src, zest_format format, zest_byte alpha_level);
ZEST_API void zest_ConvertBitmapToAlpha(zest_bitmap_t *image);
ZEST_API zest_byte *zest_BitmapArrayLookUp(zest_bitmap_array_t *bitmap_array, zest_uint index);
ZEST_API zest_bitmap_array_t zest_CreateBitmapArray(int width, int height, zest_format format, zest_uint size_of_array);
ZEST_API void zest_FreeBitmap(zest_bitmap_t *image);
ZEST_API void zest_FreeBitmapArray(zest_bitmap_array_t *images);
ZEST_API void zest_FreeBitmapData(zest_bitmap_t *image);
ZEST_API zest_bool zest_AllocateBitmap(zest_bitmap_t *bitmap, int width, int height, zest_format format);
ZEST_API void zest_AllocateBitmapMemory(zest_bitmap_t *bitmap, zest_size size_in_bytes);
ZEST_API void zest_CopyBitmap(zest_bitmap_t *src, int from_x, int from_y, int width, int height, zest_bitmap_t *dst, int to_x, int to_y);

// Image_collection_header
ZEST_API zest_image_collection_t zest_CreateImageCollection(zest_format format, zest_uint max_images, zest_uint array_count, zest_image_collection_flags flags);
ZEST_API zest_image_collection_t zest_CreateImageAtlasCollection(zest_format format, int max_images);
ZEST_API zest_image_handle zest_CreateImageAtlas(zest_context context, zest_image_collection_t *atlas_handle, zest_uint layer_width, zest_uint layer_height, zest_image_flags flags);
ZEST_API void zest_FreeImageCollection(zest_image_collection_t *image_collection);
ZEST_API zest_atlas_region_t *zest_AddImageAtlasBitmap(zest_image_collection_t *image_collection, zest_bitmap_t *bitmap);
ZEST_API zest_atlas_region_t *zest_AddImageAtlasPixels(zest_image_collection_t *image_collection, void *pixels, zest_size size, int width, int height, zest_format format);
ZEST_API zest_atlas_region_t *zest_AddImageAtlasAnimationPixels(zest_image_collection_t *image_collection, void *pixels, zest_size size, int width, int height, int frame_width, int frame_height, int frames, zest_format format);
ZEST_API zest_bitmap_t *zest_GetLastBitmap(zest_image_collection_t *image_collection);
ZEST_API zest_bool zest_GetBestFit(zest_context context, zest_image_collection_t *image_collection, zest_uint *width, zest_uint *height);
ZEST_API zest_byte *zest_GetImageCollectionRawBitmap(zest_image_collection_t *image_collection, zest_uint bitmap_index);
ZEST_API zest_bool zest_AllocateImageCollectionCopyRegions(zest_image_collection_t *image_collection);
ZEST_API zest_bool zest_AllocateImageCollectionBitmapArray(zest_image_collection_t *image_collection);
ZEST_API zest_bitmap_meta_t zest_ImageCollectionBitmapArrayMeta(zest_image_collection_t *image_collection);
ZEST_API void zest_SetImageCollectionBitmapMeta(zest_image_collection_t *image_collection, zest_uint bitmap_index, zest_uint width, zest_uint height, zest_uint channels, zest_uint stride, zest_size size_in_bytes, zest_size offset);
ZEST_API zest_bitmap_array_t *zest_GetImageCollectionBitmapArray(zest_image_collection_t *image_collection);
ZEST_API zest_bool zest_ImageCollectionCopyToBitmapArray(zest_image_collection_t *image_collection, zest_uint bitmap_index, const void *src_data, zest_size src_size);

ZEST_API zest_atlas_region_t zest_NewAtlasRegion();

//Standard vertex format for mesh rendering (optional - users can define their own vertex types)
typedef struct zest_vertex_t {
    zest_vec3 pos;              // 3d position (12 bytes)
    zest_color_t color;         // Packed color rgba (4 bytes)
    zest_vec3 normal;           // 3d normal (12 bytes)
    zest_vec2 uv;               // uv coords (8 bytes)
    zest_u64 tangent;           // Tangent packed vec4 (8 bytes)
    zest_uint parameters;       // Additional parameters (4 bytes)
} zest_vertex_t;                // Total: 44 bytes

//Mesh_helpers_header
//Create a mesh using the standard zest_vertex_t vertex format
ZEST_API zest_mesh zest_NewStandardMesh(zest_context context);
//Push a fully-featured vertex to the mesh (requires zest_vertex_t format)
ZEST_API void zest_PushMeshVertex(zest_mesh mesh, float pos[3], float normal[3], float uv[2], zest_u64 tangent, zest_color_t color, zest_uint group);
//Push a vertex with only position and color to the mesh (requires zest_vertex_t format)
ZEST_API void zest_PushVertexPositionOnly(zest_mesh mesh, float pos_x, float pos_y, float pos_z, zest_color_t color);
//Set the position of the mesh in its transform matrix (requires zest_vertex_t format)
ZEST_API void zest_PositionMesh(zest_mesh mesh, zest_vec3 position);
//Rotate a mesh by the given pitch, yaw and roll values (requires zest_vertex_t format)
ZEST_API zest_matrix4 zest_RotateMesh(zest_mesh mesh, float pitch, float yaw, float roll);
//Transform a mesh by the given pitch, yaw, roll, position and scale (requires zest_vertex_t format)
ZEST_API zest_matrix4 zest_TransformMesh(zest_mesh mesh, float pitch, float yaw, float roll, float x, float y, float z, float sx, float sy, float sz);
//Calculate normals for a mesh (requires zest_vertex_t format)
ZEST_API void zest_CalculateNormals(zest_mesh mesh);
//Calculate the bounding box of a mesh (requires zest_vertex_t format)
ZEST_API zest_bounding_box_t zest_GetMeshBoundingBox(zest_mesh mesh);
//Set the group id for every vertex in the mesh (requires zest_vertex_t format)
ZEST_API void zest_SetMeshGroupID(zest_mesh mesh, zest_uint group_id);
//Add all vertices and indices from one mesh to another (requires same vertex format)
ZEST_API void zest_AddMeshToMesh(zest_mesh dst_mesh, zest_mesh src_mesh);
//Create a cylinder mesh of given number of sides, radius and height. Set cap to 1 to cap the cylinder.
ZEST_API zest_mesh zest_CreateCylinder(zest_context context, int sides, float radius, float height, zest_color_t color, zest_bool cap);
//Create a cone mesh of given number of sides, radius and height.
ZEST_API zest_mesh zest_CreateCone(zest_context context, int sides, float radius, float height, zest_color_t color);
//Create a uv sphere mesh made up using a number of horizontal rings and vertical sectors of a give radius.
ZEST_API zest_mesh zest_CreateSphere(zest_context context, int rings, int sectors, float radius, zest_color_t color);
//Create a cube mesh of a given size.
ZEST_API zest_mesh zest_CreateCube(zest_context context, float size, zest_color_t color);
//Create a flat rounded rectangle of a give width and height. Pass in the radius to use for the corners and number of segments to use for the corners.
ZEST_API zest_mesh zest_CreateRoundedRectangle(zest_context context, float width, float height, float radius, int segments, zest_bool backface, zest_color_t color);

//gltf_loader tools

typedef struct zest_material_t {
	zest_image_handle image;	//A texture array of the material
	//Texture array index in the image
	zest_uint base_color;		
	zest_uint metallic_roughness;
	zest_uint metallic;
	zest_uint roughness;
	zest_uint normal;
	zest_uint emissive;
	zest_uint occlusion;
} zest_material_t;

typedef struct zest_gltf_t {
	zest_mesh *meshes;
	zest_uint mesh_count;
	zest_material_t *materials;
	zest_uint material_count;
} zest_gltf_t;

zest_mesh LoadGLTFScene(zest_context context, const char* filepath, float adjust_scale);
zest_gltf_t LoadGLTF(zest_context context, const char* filepath);
void zest_FreeGLTF(zest_gltf_t *model);

/*

//Begin a new image collection.
ZEST_API zest_image_handle zest_CreateImageWithBitmap(zest_context context, zest_bitmap_t *bitmap, zest_image_flags flags);
ZEST_API zest_atlas_region_t zest_AddImageAtlasPNG(zest_image_collection_t *image_collection, const char *filename, const char *name);
ZEST_API void zest_SetImageCollectionPackedBorderSize(zest_image_collection_t *image_collection, zest_uint border_size);
ZEST_API zest_imgui_image_t zest_NewImGuiImage(void);
ZEST_API zest_atlas_region_t zest_CreateAtlasRegion();
ZEST_API void zest_FreeAtlasRegion(zest_atlas_region region);
ZEST_API zest_atlas_region_t zest_CreateAnimation(zest_context context, zest_uint frames);
//Free the memory used in a zest_bitmap_t
//Create a new initialise zest_bitmap_t
ZEST_API zest_bitmap_t zest_NewBitmap();
//Create a new bitmap from a pixel buffer. Pass in the name of the bitmap, a pointer to the buffer, the size in bytes of the buffer, the width and height
//and the number of color channels
//Allocate the memory for a bitmap based on the width, height and number of color channels. You can also specify the fill color
//Allocate the memory for a bitmap based the number of bytes required
//Copy all of a source bitmap to a destination bitmap
ZEST_API void zest_CopyWholeBitmap(zest_bitmap_t *src, zest_bitmap_t *dst);
//Copy an area of a source bitmap to another bitmap
//Convert a bitmap to a specific zest_format. Accepted values are;
//zest_texture_format_alpha
//zest_texture_format_rgba_unorm
//zest_texture_format_bgra_unorm
//Convert a bitmap to BGRA format
ZEST_API void zest_ConvertBitmapToBGRA(zest_bitmap_t *src, zest_byte alpha_level);
//Convert a bitmap to RGBA format
ZEST_API void zest_ConvertBitmapToRGBA(zest_bitmap_t *src, zest_byte alpha_level);
//Convert a BGRA bitmap to RGBA format
ZEST_API void zest_ConvertBGRAToRGBA(zest_bitmap_t *src);
//Convert a bitmap to a single alpha channel
//Sample the color of a pixel in a bitmap with the given x/y coordinates
ZEST_API zest_color_t zest_SampleBitmap(zest_bitmap_t *image, int x, int y);
//Get a pointer to the first pixel in a bitmap within the bitmap array. Index must be less than the number of bitmaps in the array
//Get the size of a bitmap at a specific index
ZEST_API zest_size zest_BitmapArrayLookUpSize(zest_bitmap_array_t *bitmap_array, zest_uint index);
//Get the distance in pixels to the furthes pixel from the center that isn't alpha 0
ZEST_API float zest_FindBitmapRadius(zest_bitmap_t *image);
//Destory a bitmap array and free its resources
ZEST_API void zest_DestroyBitmapArray(zest_bitmap_array_t *bitmap_array);
//Get a bitmap from a bitmap array with the given index
ZEST_API zest_bitmap_t zest_GetImageFromArray(zest_bitmap_array_t *bitmap_array, zest_uint index);
//After adding all the images you want to a texture, you will then need to process the texture which will create all of the necessary GPU resources and upload the texture to the GPU.
//You can then use the image handles to draw the images along with the descriptor set - either the one that gets created automatically with the the texture to draw sprites and billboards
//or your own descriptor set.
//Don't call this in the middle of sending draw commands that are using the texture because it will switch the texture buffer
//index so some drawcalls will use the outdated texture, some the new. Call before any draw calls are made or better still,
//just call zest_ScheduleTextureReprocess which will recreate the texture between frames and then schedule a cleanup the next
//frame after.
ZEST_API void zest_InitialiseBitmapArray(zest_context context, zest_bitmap_array_t *images, zest_uint size_of_array);
//Free a bitmap array and return memory resources to the allocator
//Free an image collection
ZEST_PRIVATE void zest__cleanup_image_collection(zest_image_collection image_collection);
//Set the handle of an image. This dictates where the image will be positioned when you draw it with zest_DrawSprite/zest_DrawBillboard. 0.5, 0.5 will center the image at the position you draw it.
ZEST_API void zest_SetImageHandle(zest_atlas_region image, float x, float y);
//Get the layer index that the image exists on in the texture
ZEST_API zest_uint zest_RegionLayerIndex(zest_atlas_region image);
//Get the dimensions of the image
//Get the uv coords of the image
ZEST_API zest_vec4 zest_ImageUV(zest_atlas_region image);
ZEST_API void zest_AddImageCollectionPNG(zest_image_collection_t *image_collection, const char *filename);
//Copies an area of a zest_bitmap to a zest_image.
*/

#ifdef __cplusplus
}
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

//Zest_ktx_helper_functions
//End Zest_ktx_helper_functions
#ifdef ZEST_KTX_IMPLEMENTATION

#define ZEST_IMAGES_IMPLEMENTATION

#define TINYKTX_IMPLEMENTATION
#include "tinyktx.h"

#ifdef __cplusplus
extern "C" {
#endif
//Zest_ktx_helper_implementation
void zest__tinyktxCallbackError(void *user, char const *msg) {
	ZEST_PRINT("Tiny_Ktx ERROR: %s", msg);
}

void *zest__tinyktxCallbackAlloc(void *user, size_t size) {
	return ZEST_UTILITIES_MALLOC(size);
}

void zest__tinyktxCallbackFree(void *user, void *data) {
	ZEST_UTILITIES_FREE(data);
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

zest_image_collection_t zest__load_ktx(zest_device device, const char *file_path) {

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
		return ZEST__ZERO_INIT(zest_image_collection_t);
	}

	zest_ktx_user_context_t user_context;
	user_context.device = device;
	user_context.file = file;

	TinyKtx_ContextHandle ctx = TinyKtx_CreateContext(&callbacks, &user_context);

	if (!TinyKtx_ReadHeader(ctx)) {
		return ZEST__ZERO_INIT(zest_image_collection_t);
	}

	zest_format format = zest__convert_tktx_format(TinyKtx_GetFormat(ctx));
	if (format == zest_format_undefined) {
		TinyKtx_DestroyContext(ctx);
		return ZEST__ZERO_INIT(zest_image_collection_t);
	}

	zest_uint width = TinyKtx_Width(ctx);
	zest_uint height = TinyKtx_Height(ctx);
	zest_uint depth = TinyKtx_Depth(ctx);

	zest_uint mip_count = TinyKtx_NumberOfMipmaps(ctx);
	zest_bool is_cubemap = TinyKtx_IsCubemap(ctx);
	zest_bool is_array = TinyKtx_IsArray(ctx);
	zest_uint array_slices = is_array ? TinyKtx_ArraySlices(ctx) : 1;

	zest_image_collection_flags flags = 0;
	if (is_cubemap) flags |= zest_image_collection_flag_is_cube_map;
	if (is_array) flags |= zest_image_collection_flag_is_array;

	zest_image_collection_t image_collection = zest_CreateImageCollection(format, 0, mip_count, flags);

	if (is_cubemap) {
		image_collection.array_layers = is_array ? 6 * array_slices : 6;
	} else {
		image_collection.array_layers = is_array ? array_slices : 1;
	}

	//First pass to set the bitmap array
	size_t offset = 0;
	for (zest_uint i = 0; i < mip_count; ++i) {
		zest_uint image_size = TinyKtx_ImageSize(ctx, i);
		zest_SetImageCollectionBitmapMeta(&image_collection, i, width, height, 0, 0, image_size, offset);
		offset += image_size;
		if (width > 1) width /= 2;
		if (height > 1) height /= 2;
	}

	zest_AllocateImageCollectionBitmapArray(&image_collection);
	width = TinyKtx_Width(ctx);
	height = TinyKtx_Height(ctx);

	zest_bitmap_array_t *bitmap_array = zest_GetImageCollectionBitmapArray(&image_collection);

	for (zest_uint i = 0; i != bitmap_array->size_of_array; ++i) {
		zest_ImageCollectionCopyToBitmapArray(&image_collection, i, TinyKtx_ImageRawData(ctx, i), TinyKtx_ImageSize(ctx, i));
	}

	TinyKtx_DestroyContext(ctx);
	image_collection.flags |= zest_image_collection_flag_initialised;
	return image_collection;
}

zest_image_handle zest_LoadKTX(zest_device device, const char *name, const char *file_name) {
    zest_image_collection_t image_collection = zest__load_ktx(device, file_name);

    if (!(image_collection.flags & zest_image_collection_flag_initialised)) {
        return ZEST__ZERO_INIT(zest_image_handle);
    }

    zest_bool is_cubemap = (image_collection.flags & zest_image_collection_flag_is_cube_map) != 0;

	zest_bitmap_array_t *bitmap_array = &image_collection.bitmap_array;
	zest_AllocateImageCollectionCopyRegions(&image_collection);

	zest_uint layer_count = image_collection.array_layers;

    for(zest_uint i = 0; i != bitmap_array->size_of_array; ++i) {
        zest_buffer_image_copy_t buffer_copy_region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
        buffer_copy_region.image_aspect = zest_image_aspect_color_bit;
        buffer_copy_region.mip_level = i;
        buffer_copy_region.base_array_layer = 0;
        buffer_copy_region.layer_count = layer_count;
        buffer_copy_region.image_extent.width = bitmap_array->meta[i].width;
        buffer_copy_region.image_extent.height = bitmap_array->meta[i].height;
        buffer_copy_region.image_extent.depth = 1;
        buffer_copy_region.buffer_offset = bitmap_array->meta[i].offset;

		image_collection.buffer_copy_regions[i] = buffer_copy_region;
    }
    zest_size image_size = bitmap_array->total_mem_size;

    zest_buffer staging_buffer = zest_CreateStagingBuffer(device, image_size, image_collection.bitmap_array.data);
    zest_image_handle image_handle = ZEST__ZERO_INIT(zest_image_handle);

    if (!staging_buffer) {
        goto cleanup;
    }

    zest_uint width = bitmap_array->meta[0].width;
    zest_uint height = bitmap_array->meta[0].height;

    zest_uint mip_levels = bitmap_array->size_of_array;
    zest_image_info_t create_info = zest_CreateImageInfo(width, height);
    create_info.mip_levels = mip_levels;
    create_info.format = image_collection.format;
    create_info.layer_count = layer_count;
    create_info.flags = zest_image_preset_texture | zest_image_flag_transfer_src;
	if (is_cubemap) {
		create_info.flags |= zest_image_flag_cubemap;
	}
    image_handle = zest_CreateImage(device, &create_info);
	zest_image image = zest_GetImage(image_handle);

	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	zest_imm_TransitionImage(queue, image, zest_image_layout_transfer_dst_optimal, 0, mip_levels, 0, layer_count);
	zest_imm_CopyBufferRegionsToImage(queue, image_collection.buffer_copy_regions, bitmap_array->size_of_array, staging_buffer, image);
    zest_imm_TransitionImage(queue, image, zest_image_layout_shader_read_only_optimal, 0, mip_levels, 0, layer_count);
	zest_imm_EndCommandBuffer(queue);

    zest_FreeBitmapArray(bitmap_array);
	zest_FreeBuffer(staging_buffer);
	zest_FreeImageCollection(&image_collection);

    return image_handle;

    cleanup:
    zest_FreeBitmapArray(bitmap_array);
	zest_FreeImageNow(image_handle);
	zest_FreeBuffer(staging_buffer);
	zest_FreeImageCollection(&image_collection);
    return ZEST__ZERO_INIT(zest_image_handle);
}
//End Zest_ktx_helper_implementation

#ifdef __cplusplus
}
#endif

#endif

#include <stdarg.h>
#include <stdio.h>

#if defined(ZEST_MSDF_IMPLEMENTATION) || defined(ZEST_IMAGES_IMPLEMENTATION)

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#endif

#ifdef ZEST_MSDF_IMPLEMENTATION

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MSDF_IMPLEMENTATION
#include "msdf.h"

void* zest__msdf_allocation(size_t size, void* ctx) {
    return ZEST_UTILITIES_MALLOC(size);
}

void zest__msdf_free(void* ptr, void* ctx) {
    ZEST_UTILITIES_FREE(ptr);
}

zest_font_resources_t zest_CreateFontResources(zest_context context, const char *vert_shader, const char *frag_shader) {
	zest_device device = zest_GetContextDevice(context);
	//Create and compile the shaders for our custom sprite pipeline
	zest_shader_handle font_vert = zest_CreateShaderFromFile(device, vert_shader, "font_vert.spv", zest_vertex_shader, true);
	zest_shader_handle font_frag = zest_CreateShaderFromFile(device, frag_shader, "font_frag.spv", zest_fragment_shader, true);

	//Create a pipeline that we can use to draw billboards
	zest_pipeline_template font_pipeline = zest_CreatePipelineTemplate(device, "pipeline_billboard");
	zest_AddVertexInputBindingDescription(font_pipeline, 0, sizeof(zest_font_instance_t), zest_input_rate_instance);

    zest_AddVertexAttribute(font_pipeline, 0, 0, zest_format_r32g32_sfloat, offsetof(zest_font_instance_t, position));  
    zest_AddVertexAttribute(font_pipeline, 0, 1, zest_format_r16g16b16a16_snorm, offsetof(zest_font_instance_t, uv));  
    zest_AddVertexAttribute(font_pipeline, 0, 2, zest_format_r8g8b8a8_unorm, offsetof(zest_font_instance_t, color));
    zest_AddVertexAttribute(font_pipeline, 0, 3, zest_format_r16g16_sscaled, offsetof(zest_font_instance_t, size));  
    zest_AddVertexAttribute(font_pipeline, 0, 4, zest_format_r32_uint, offsetof(zest_font_instance_t, texture_array));

	zest_SetPipelineVertShader(font_pipeline, font_vert);
	zest_SetPipelineFragShader(font_pipeline, font_frag);
	zest_SetPipelineDepthTest(font_pipeline, false, false);
	zest_SetPipelineLayout(font_pipeline, zest_GetDefaultPipelineLayout(device));

	zest_font_resources_t resources;
	resources.pipeline = font_pipeline;

	return resources;
}

zest_layer_handle zest_CreateFontLayer(zest_context context, const char *name, zest_uint max_characters) {
	ZEST_ASSERT(name, "Specify a name for the font layer");
	zest_layer_handle layer = zest_CreateInstanceLayer(context, name, sizeof(zest_font_instance_t), max_characters);
	return layer;
}

zest_msdf_font_t zest_CreateMSDF(zest_context context, const char *filename, zest_uint font_sampler_index, float font_size, float sdf_range) {
	zest_msdf_font_t font = ZEST__ZERO_INIT(zest_msdf_font_t);
	font.sdf_range = sdf_range;

    // --- MSDF Font Atlas Generation ---
    unsigned char* font_buffer = (unsigned char*)zest_ReadEntireFile(context->device, filename, ZEST_FALSE);
	ZEST_ASSERT(font_buffer, "Error loading font file\n");

    stbtt_fontinfo font_info;
    if (!stbtt_InitFont(&font_info, font_buffer, 0)) {
        ZEST_PRINT("Error initializing font\n");
		zest_FreeFile(context->device, (zest_file)font_buffer);
        return font;
    }

    // Atlas properties
    int atlas_channels = 4; 
	font.font_atlas = zest_CreateImageAtlasCollection(zest_format_r8g8b8a8_unorm, 255);
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
			int size = result.width * result.height * atlas_channels;
			zest_bitmap_t tmp_bitmap = zest_CreateBitmapFromRawBuffer(result.rgba, size, result.width, result.height, zest_format_r8g8b8a8_unorm);
			tmp_bitmap.is_imported = 0;
			zest_AddImageAtlasBitmap(&font.font_atlas, &tmp_bitmap);

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
			char c[2] = { (char)char_code, '\0' };
		} else {
			//Treat as whitespace
            int advance, lsb;
            stbtt_GetGlyphHMetrics(&font_info, glyph_index, &advance, &lsb);
            current_character->x_advance = scale * advance;
			ZEST__FLAG(current_character->flags, zest_character_flag_whitespace);
		}
    }

    zest_FreeFile(context->device, (zest_file)font_buffer);
	zest_uint atlas_width = (zest_uint)zest_GetNextPower((zest_uint)sqrtf((float)total_area_size));
	zest_uint atlas_height = atlas_width;
	font.font_atlas.packed_border_size = 2;
	if (!zest_GetBestFit(context, &font.font_atlas, &atlas_width, &atlas_height)) {
		ZEST_PRINT("Unable to pack all fonts in to the image!");
		zest_FreeImageCollection(&font.font_atlas);
		return font;
	}
	zest_image_handle image_atlas_handle = zest_CreateImageAtlas(context, &font.font_atlas, atlas_width, atlas_height, zest_image_preset_texture);
	zest_image image_atlas = zest_GetImage(image_atlas_handle);
	font.font_binding_index = zest_AcquireSampledImageIndex(context->device, image_atlas, zest_texture_array_binding);
	font.settings.sampler_index = font_sampler_index;
	zest_uint bitmap_index = 0;
	for (int i = 32; i < 127; ++i) {
		if (ZEST__NOT_FLAGGED(font.characters[i].flags, zest_character_flag_whitespace)) {
			font.characters[i].region = font.font_atlas.regions[bitmap_index++];
			char c[2] = { (char)i, '\0' };
		}
	}
	for (int i = 0; i != 256; ++i) {
		if (font.characters[i].region.width && font.characters[i].region.height) {
			font.characters[i].uv_packed = font.characters[i].region.uv_packed;
			zest_BindAtlasRegionToImage(&font.characters[i].region, font.settings.sampler_index, image_atlas, zest_texture_array_binding);
		}
	}
	font.context = context;
	font.settings.transform = zest_Vec4Set(2.0f / zest_ScreenWidthf(context), 2.0f / zest_ScreenHeightf(context), -1.f, -1.f);
	font.settings.unit_range = ZEST_STRUCT_LITERAL(zest_vec2, font.sdf_range / 512.f, font.sdf_range / 512.f);
	font.settings.in_bias = 0.f;
	font.settings.out_bias = 0.f;
	font.settings.smoothness = 0.f;
	font.settings.gamma = 1.f;
	font.settings.shadow_color = zest_Vec4Set(0.f, 0.f, 0.f, 1.f);
	font.settings.shadow_offset = zest_Vec2Set(2.f, 2.f);
	font.settings.image_index = font.font_binding_index;
	return font;
}

static void zest__stbi_write_mem(void *context, void *data, int size) {
	zest_stbi_mem_context_t *c = (zest_stbi_mem_context_t*)context;
	char *buffer = (char*)(c->context);
	char *src = (char *)data;
	int cur_pos = c->last_pos;
	ZEST_ASSERT(c->file_size + size < zloc__MEGABYTE(4));
	memcpy(buffer + cur_pos, src, size);
	c->file_size += size;
	c->last_pos = cur_pos;
}

void zest_SaveMSDF(zest_msdf_font_t *font, const char *filename) {
	zest_image_collection_t *atlas = &font->font_atlas;
	zest_byte *atlas_bitmap = zest_GetImageCollectionRawBitmap(&font->font_atlas, 0);
	zest_bitmap_meta_t atlas_meta = zest_ImageCollectionBitmapArrayMeta(&font->font_atlas);
	zest_msdf_font_file_t file;
	file.magic = zest_INIT_MAGIC(zest_struct_type_file);
	memcpy(&file.font_details, font, sizeof(zest_msdf_font_t));
	
	zest_stbi_mem_context_t mem_context = ZEST__ZERO_INIT(zest_stbi_mem_context_t);
	mem_context.context = ZEST_UTILITIES_MALLOC(zloc__MEGABYTE(4));
	stbi_write_png_to_func(zest__stbi_write_mem, &mem_context, atlas_meta.width, atlas_meta.height, atlas_meta.channels, atlas_bitmap, atlas_meta.stride);
    FILE *font_file = zest__open_file(filename, "wb");
	file.png_size = mem_context.file_size;
    size_t written = fwrite(&file.magic, sizeof(zest_uint), 1, font_file);
	written += fwrite(&file.font_details, sizeof(zest_msdf_font_t), 1, font_file);
	written += fwrite(&file.png_size, sizeof(zest_uint), 1, font_file);
	written += fwrite(mem_context.context, sizeof(char), mem_context.file_size, font_file);
	if (written != 3 + mem_context.file_size) {
		ZEST_PRINT("Failed to save the font to disk!");
	}
	ZEST_UTILITIES_FREE(mem_context.context);
	fclose(font_file);
}

zest_msdf_font_t zest_LoadMSDF(zest_context context, const char *filename, zest_uint font_sampler_index) {
	zest_msdf_font_file_t file;
    FILE *font_file = zest__open_file(filename, "rb");

    fread(&file.magic, sizeof(zest_uint), 1, font_file);
	unsigned char *png_buffer = 0;
	
	if ((*((int*)&file) & 0xFFFF) == 0x4E57) {
		fread(&file.font_details, sizeof(zest_msdf_font_t), 1, font_file);
		fread(&file.png_size, sizeof(zest_uint), 1, font_file);
		png_buffer = (unsigned char*)ZEST_UTILITIES_MALLOC(file.png_size);
		size_t read = fread(png_buffer, sizeof(char), file.png_size, font_file);
	} else {
		ZEST_PRINT("Unable to read font file");
		return ZEST__ZERO_INIT(zest_msdf_font_t);
	}

	zest_device device = zest_GetContextDevice(context);

	int width, height, channels;
	stbi_uc *bitmap_buffer = stbi_load_from_memory(png_buffer, file.png_size, &width, &height, &channels, 0);
	ZEST_ASSERT(bitmap_buffer, "Unable to load the font bitmap.");
	int size = width * height * channels;
	zest_bitmap_t font_bitmap = zest_CreateBitmapFromRawBuffer(bitmap_buffer, size, width, height, zest_format_r8g8b8a8_unorm);
	ZEST_UTILITIES_FREE(png_buffer);
	zest_msdf_font_t font = file.font_details;
	fclose(font_file);

	zest_image_info_t image_info = zest_CreateImageInfo(font_bitmap.meta.width, font_bitmap.meta.height);
	image_info.flags = zest_image_preset_texture;
	font.font_image = zest_CreateImage(device, &image_info);
	zest_image font_image = zest_GetImage(font.font_image);
	zest_CopyBitmapToImage(device, font_bitmap.data, font_bitmap.meta.size, font_image, font_bitmap.meta.width, font_bitmap.meta.height);
	STBI_FREE(bitmap_buffer);

	font.font_binding_index = zest_AcquireSampledImageIndex(device, font_image, zest_texture_array_binding);
	font.settings.sampler_index = font_sampler_index;
	for (int i = 0; i != 255; ++i) {
		if (font.characters[i].width && font.characters[i].height) {
			zest_BindAtlasRegionToImage(&font.characters[i].region, font.settings.sampler_index, font_image, zest_texture_array_binding);
		}
	}
	font.context = context;
	font.is_loaded_from_file = ZEST_TRUE;
	font.settings.transform = zest_Vec4Set(2.0f / zest_ScreenWidthf(context), 2.0f / zest_ScreenHeightf(context), -1.f, -1.f);
	font.settings.unit_range = ZEST_STRUCT_LITERAL(zest_vec2, font.sdf_range / 512.f, font.sdf_range / 512.f);
	font.settings.in_bias = 0.f;
	font.settings.out_bias = 0.f;
	font.settings.smoothness = 0.f;
	font.settings.gamma = 1.f;
	font.settings.shadow_color = zest_Vec4Set(0.f, 0.f, 0.f, 1.f);
	font.settings.shadow_offset = zest_Vec2Set(2.f, 2.f);
	font.settings.image_index = font.font_binding_index;
	return font;
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

void zest_SetFontShadowColor(zest_msdf_font_t *font, float r, float g, float b, float a) {
	font->settings.shadow_color = zest_Vec4Set(r, g, b, a);
}

void zest_SetFontShadowOffset(zest_msdf_font_t *font, float x, float y) {
	font->settings.shadow_offset = zest_Vec2Set(x, y);
}

void zest_FreeFont(zest_msdf_font_t *font) {
	if (!font->is_loaded_from_file) {
		if (font->characters) {
			zest_FreeImageCollection(&font->font_atlas);
		}
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

void zest_SetMSDFFontDrawing(zest_layer layer, zest_msdf_font_t *font, zest_font_resources_t *font_resources) {
	ZEST_ASSERT_HANDLE(layer);	//Not a valid layer handle
    zest__end_instance_instructions(layer);
    zest__start_instance_instructions(layer);
    layer->current_instruction.pipeline_template = font_resources->pipeline;
    layer->current_instruction.draw_mode = zest_draw_mode_text;
    layer->current_instruction.asset = font;
    layer->current_instruction.scissor = layer->scissor;
    layer->current_instruction.viewport = layer->viewport;
	zest__set_layer_push_constants(layer, &font->settings, sizeof(zest_msdf_font_settings_t));
    layer->last_draw_mode = zest_draw_mode_text;
}

float zest__draw_msdf_text(zest_layer layer, const char* text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing) {
	ZEST_ASSERT_HANDLE(layer);	//Not a valid layer handle
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
		font_instance->color = layer->current_color;
        font_instance->position = zest_Vec2Set(xpos + xoffset, y - yoffset);
        font_instance->uv = character->region.uv_packed;
		font_instance->texture_array = character->region.layer_index;

        xpos += character->x_advance * size + letter_spacing;
    }

    return xpos;
}

float zest_DrawMSDFText(zest_layer layer, float x, float y, float handle_x, float handle_y, float size, float letter_spacing, const char* format, ...) {
	ZEST_ASSERT_HANDLE(layer);	//Not a valid layer handle
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    return zest__draw_msdf_text(layer, buffer, x, y, handle_x, handle_y, size, letter_spacing);
}

// End msdf_fonts

/*
// Bitmap_helpers
zest_atlas_region zest_CreateAnimation(zest_context context, zest_uint frames) {
    zest_atlas_region image = (zest_atlas_region)ZEST__ALLOCATE_ALIGNED(context->device->allocator, sizeof(zest_atlas_region_t) * frames, 16);
    ZEST_ASSERT(image); //Couldn't allocate the image. Out of memory?
	image->magic = zest_INIT_MAGIC(zest_struct_type_atlas_region);
	image->context = context;
    image->frames = frames;
    return image;
}

zest_bitmap_t zest_NewBitmap() {
    zest_bitmap_t bitmap = (zest_bitmap)ZEST__NEW(allocator, zest_bitmap);
    *bitmap = ZEST__ZERO_INIT(zest_bitmap_t);
    return bitmap;
}

void zest_ConvertBitmapToBGRA(zest_bitmap_t *src, zest_byte alpha_level) {
    zest_ConvertBitmap(src, zest_format_b8g8r8a8_unorm, alpha_level);
}

void zest_ConvertBitmapToRGBA(zest_bitmap_t *src, zest_byte alpha_level) {
    zest_ConvertBitmap(src, zest_format_r8g8b8a8_unorm, alpha_level);
}

void zest_ConvertBGRAToRGBA(zest_bitmap_t * src) {
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

void zest_CopyWholeBitmap(zest_bitmap_t *src, zest_bitmap_t *dst) {
	ZEST_ASSERT_HANDLE(src);		//Not a valid src bitmap handle
	ZEST_ASSERT_HANDLE(dst);		//Not a valid dst bitmap handle
    ZEST_ASSERT(src->data && src->meta.size);

    zest_FreeBitmapData(dst);
    zest_SetText(dst->allocator, &dst->name, src->name.str);
    dst->meta.channels = src->meta.channels;
    dst->meta.height = src->meta.height;
    dst->meta.width = src->meta.width;
    dst->meta.size = src->meta.size;
    dst->meta.stride = src->meta.stride;
    dst->data = 0;
    dst->data = (zest_byte*)ZEST_UTILITIES_MALLOC(src->allocator, src->meta.size);
    ZEST_ASSERT(dst->data);    //out of memory;
    memcpy(dst->data, src->data, src->meta.size);

}

zest_color_t zest_SampleBitmap(zest_bitmap_t *image, int x, int y) {
    ZEST_ASSERT(image->data);

    size_t offset = y * image->meta.stride + (x * image->meta.channels);
    zest_color_t c = ZEST__ZERO_INIT(zest_color_t);
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

float zest_FindBitmapRadius(zest_bitmap_t *image) {
    //Todo: optimise with SIMD
    ZEST_ASSERT(image->data);
    float max_radius = 0;
    for (int x = 0; x < image->meta.width; ++x) {
        for (int y = 0; y < image->meta.height; ++y) {
            zest_color_t c = zest_SampleBitmap(image, x, y);
            if (c.a) {
                max_radius = ceilf(ZEST__MAX(max_radius, zest_Distance((float)image->meta.width / 2.f, (float)image->meta.height / 2.f, (float)x, (float)y)));
            }
        }
    }
    return ceilf(max_radius);
}

void zest_DestroyBitmapArray(zest_bitmap_array_t* bitmap_array) {
    if (bitmap_array->data) {
        ZEST_UTILITIES_FREE(bitmap_array->data);
    }
    bitmap_array->data = 0;
    bitmap_array->size_of_array = 0;
}

zest_bitmap_t zest_GetImageFromArray(zest_bitmap_array_t* bitmap_array, zest_uint index) {
    zest_bitmap_t bitmap = zest_NewBitmap();
    bitmap.meta.width = bitmap_array->meta[index].width;
    bitmap.meta.height = bitmap_array->meta[index].height;
    bitmap.meta.channels = bitmap_array->meta[index].channels;
    bitmap.meta.stride = bitmap_array->meta[index].width * bitmap_array->meta[index].bytes_per_pixel;
    bitmap.data = bitmap_array->data + bitmap_array->meta[index].offset;
    return bitmap;
}

zest_size zest_BitmapArrayLookUpSize(zest_bitmap_array_t *bitmap_array, zest_uint index) {
    ZEST_ASSERT((zest_uint)index < bitmap_array->size_of_array);	//index out of bounds
    return bitmap_array->meta[index].size;
}
// End_Bitmap_helpers

// Image_collections

zest_atlas_region zest_AddImageAtlasPNG(zest_image_collection_t *image_collection, const char *filename, const char *name) {
	zest_bitmap_t bitmap = zest_LoadPNG(context, filename);
	if (bitmap) {
		if (bitmap->meta.format != image_collection->format) {
			zest_ConvertBitmap(bitmap, image_collection->format, 255);
		}
		zest_atlas_region region = zest_NewAtlasRegion();
		bitmap->atlas_region = region;
		region->width = bitmap->meta.width;
		region->height = bitmap->meta.height;
		zest_map_insert(device->allocator, image_collection->image_bitmaps, name, bitmap);
		return region;
	}
	return NULL;
}

void zest_SetImageCollectionPackedBorderSize(zest_image_collection_t *image_collection, zest_uint border_size) {
	image_collection->packed_border_size = border_size;
}

zest_bitmap_array_t *zest_GetImageCollectionBitmapArray(zest_image_collection_t *image_collection_handle) {
	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource_checked(image_collection_handle.store, image_collection_handle.value);
	return &image_collection->bitmap_array;
}

void zest__cleanup_image_collection(zest_image_collection_t *image_collection) {
	ZEST_ASSERT_HANDLE(image_collection);   //Not a valid image collection handle/pointer!
	zest_map_foreach(i, image_collection->image_bitmaps) {
		zest_bitmap_t bitmap = image_collection->image_bitmaps.data[i];
		zest_FreeAtlasRegion(bitmap->atlas_region);
        zest_FreeBitmap(bitmap);
	}
    zest_vec_foreach(i, image_collection->layers) {
        zest_FreeBitmapData(&image_collection->layers[i]);
    }
	zest_resource_store_t *store = image_collection->handle.store;
    zest_vec_free(store->allocator, image_collection->regions);
	zest_map_free(store->allocator, image_collection->image_bitmaps);
    zest_vec_free(store->allocator, image_collection->buffer_copy_regions);
    zest_vec_free(store->allocator, image_collection->layers);
    zest_FreeBitmapArray(&image_collection->bitmap_array);
	if (image_collection->handle.value) {
		zest__remove_store_resource(store, image_collection->handle.value);
	}
}

void zest_SetImageHandle(zest_atlas_region image, float x, float y) {
    if (image->handle.x == x && image->handle.y == y)
        return;
    image->handle.x = x;
    image->handle.y = y;
    image->min.x = image->width * (0.f - image->handle.x);
    image->min.y = image->height * (0.f - image->handle.y);
    image->max.x = image->width * (1.f - image->handle.x);
    image->max.y = image->height * (1.f - image->handle.y);
}


zest_image_handle zest_CreateImageWithBitmap(zest_context context, zest_bitmap_t *bitmap, zest_image_flags flags) {
	ZEST_ASSERT(bitmap->data);	//No data in the bitmap
	zest_image_info_t image_info = zest_CreateImageInfo(bitmap->meta.width, bitmap->meta.height);
	image_info.format = bitmap->meta.format;

	if (flags == 0) {
		image_info.flags = zest_image_preset_texture_mipmaps;
	} else {
		ZEST_ASSERT(flags & zest_image_preset_texture, "If you pass in flags to the zest_CreateImageWithBitmap function then it must at lease contain zest_image_preset_texture flags. You can leave as 0 and an image with mipmaps will be created.");
        image_info.flags = flags;
	}

	zest_image_handle image_handle = zest_CreateImage(context, &image_info);
	if (!image_handle.value) {
		ZEST_PRINT("Unable to create the image. Check for validation errors.");
		return image_handle;
	}
	if (!zest_CopyBitmapToImage(context, bitmap, image_handle, 0, 0, 0, 0, image_info.extent.width, image_info.extent.height)) {
		ZEST_PRINT("Unable to copy the bitmap to the image. Check for validation errors.");
	}
    zest_image image = (zest_image)zest__get_store_resource(image_handle.store, image_handle.value);
	zest_uint mip_levels = image->info.mip_levels;
    if (mip_levels > 1) {
		context->device->platform->begin_single_time_commands(context);
		ZEST_CLEANUP_ON_FALSE(zest__transition_image_layout(image, zest_image_layout_transfer_dst_optimal, 0, mip_levels, 0, 1));
        ZEST_CLEANUP_ON_FALSE(context->device->platform->generate_mipmaps(image));
		context->device->platform->end_single_time_commands(context);
    }
	return image_handle;

	cleanup:
	zest__cleanup_image(image);
    return ZEST__ZERO_INIT(zest_image_handle);
}

zest_imgui_image_t zest_NewImGuiImage(void) {
    zest_imgui_image_t imgui_image = ZEST__ZERO_INIT(zest_imgui_image_t);
    imgui_image.magic = zest_INIT_MAGIC(zest_struct_type_imgui_image);
    return imgui_image;
}

zest_atlas_region_t zest_CreateAtlasRegion(zest_context context) {
    *region = ZEST__ZERO_INIT(zest_atlas_region_t);
    region->magic = zest_INIT_MAGIC(zest_struct_type_atlas_region);
	region->context = context;
    region->uv.x = 0.f;
    region->uv.y = 0.f;
    region->uv.z = 1.f;
    region->uv.w = 1.f;
    region->layer_index = 0;
    region->frames = 1;
    return region;
}

void zest_FreeAtlasRegion(zest_atlas_region region) {
    if (ZEST_VALID_HANDLE(region)) {
        ZEST__FREE(region->context->device->allocator, region);
    }
}

*/
#endif 

#if defined(ZEST_MSDF_IMPLEMENTATION) || defined(ZEST_IMAGES_IMPLEMENTATION)

zest_bitmap_t zest_CreateBitmapFromRawBuffer(void* pixels, int size, int width, int height, zest_format format) {
    zest_bitmap_t bitmap = ZEST__ZERO_INIT(zest_bitmap_t);
    bitmap.is_imported = 1;
	zest_GetFormatPixelData(format, &bitmap.meta.channels, &bitmap.meta.bytes_per_pixel);
    bitmap.data = (zest_byte*)pixels;
    bitmap.meta.width = width;
    bitmap.meta.height = height;
    bitmap.meta.size = size;
    bitmap.meta.stride = width * bitmap.meta.bytes_per_pixel;
    bitmap.meta.format = format;
	zest_size expected_size = height * bitmap.meta.stride;
	ZEST_ASSERT(expected_size == size, "The size of the allocated bitmap is not the same size as the raw pixel data, check to make sure you're passing in the correct format.");
    return bitmap;
}

void zest_ConvertBitmap(zest_bitmap_t *src, zest_format new_format, zest_byte alpha_level) {
	if (src->meta.format == new_format) {
		return;
	}

    ZEST_ASSERT(src->data);	//no valid bitmap data found

	int to_channels;
	int bytes_per_pixel;
	zest_GetFormatPixelData(new_format, &to_channels, &bytes_per_pixel);
	ZEST_ASSERT(to_channels);	//Not a valid format, must be an 8bit unorm format with 1 to 4 channels.
    int from_channels = src->meta.channels;

    zest_size new_size = src->meta.width * src->meta.height * to_channels * bytes_per_pixel;
    zest_byte* new_image = (zest_byte*)ZEST_UTILITIES_MALLOC(new_size);

    zest_byte *from_data = src->data;
    zest_byte *to_data = new_image;

    for (int y = 0; y < src->meta.height; ++y) {
        for (int x = 0; x < src->meta.width; ++x) {
            zest_color_t source_pixel = { 0, 0, 0, alpha_level };
            zest_size from_idx = (y * src->meta.width + x) * from_channels;
            zest_size to_idx = (y * src->meta.width + x) * to_channels;

            switch (from_channels) {
                case 1:
                    source_pixel.r = from_data[from_idx];
                    source_pixel.g = from_data[from_idx];
                    source_pixel.b = from_data[from_idx];
                    break;
                case 2:
                    source_pixel.r = from_data[from_idx];
                    source_pixel.g = from_data[from_idx];
                    source_pixel.b = from_data[from_idx];
					source_pixel.a = from_data[from_idx + 1];
                    break;
                case 3:
                    source_pixel.r = from_data[from_idx];
                    source_pixel.g = from_data[from_idx + 1];
                    source_pixel.b = from_data[from_idx + 2];
                    break;
                case 4:
                    source_pixel.r = from_data[from_idx];
                    source_pixel.g = from_data[from_idx + 1];
                    source_pixel.b = from_data[from_idx + 2];
                    source_pixel.a = from_data[from_idx + 3];
                    break;
            }

            switch (to_channels) {
                case 1:
                    to_data[to_idx] = source_pixel.r;
                    break;
                case 2:
                    to_data[to_idx] = source_pixel.r;
                    to_data[to_idx + 1] = source_pixel.a;
                    break;
                case 3:
                    to_data[to_idx] = source_pixel.r;
                    to_data[to_idx + 1] = source_pixel.g;
                    to_data[to_idx + 2] = source_pixel.b;
                    break;
                case 4:
                    to_data[to_idx] = source_pixel.r;
                    to_data[to_idx + 1] = source_pixel.g;
                    to_data[to_idx + 2] = source_pixel.b;
                    to_data[to_idx + 3] = source_pixel.a;
                    break;
            }
        }
    }

    ZEST_UTILITIES_FREE(src->data);
    src->meta.channels = to_channels;
    src->meta.bytes_per_pixel = bytes_per_pixel;
    src->meta.format = new_format;
    src->meta.size = new_size;
    src->meta.stride = src->meta.width * bytes_per_pixel;
    src->data = new_image;

}

void zest_ConvertBitmapToAlpha(zest_bitmap_t *image) {

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

zest_byte* zest_BitmapArrayLookUp(zest_bitmap_array_t* bitmap_array, zest_uint index) {
    ZEST_ASSERT((zest_uint)index < bitmap_array->size_of_array);	//index out of bounds
    return bitmap_array->data + bitmap_array->meta[index].offset;
}

zest_bitmap_array_t zest_CreateBitmapArray(int width, int height, zest_format format, zest_uint size_of_array) {
    ZEST_ASSERT(size_of_array);            //must create with atleast one image in the array
	zest_bitmap_array_t bitmap_array = ZEST__ZERO_INIT(zest_bitmap_array_t);
    if (bitmap_array.data) {
        ZEST_UTILITIES_FREE(bitmap_array.data);
        bitmap_array.data = ZEST_NULL;
    }
    bitmap_array.size_of_array = size_of_array;
    bitmap_array.meta = (zest_bitmap_meta_t*)ZEST_UTILITIES_REALLOC(bitmap_array.meta, size_of_array * sizeof(zest_bitmap_meta_t));
    size_t offset = 0;
	int channels, bytes_per_pixel;
	zest_GetFormatPixelData(format, &channels, &bytes_per_pixel);
	ZEST_ASSERT(channels, "Not a supported bitmap format.");
    size_t image_size = width * height * bytes_per_pixel;
	int stride = bytes_per_pixel * width;
    for(int i = 0; i != size_of_array; ++i) {
        bitmap_array.meta[i] = ZEST_STRUCT_LITERAL(zest_bitmap_meta_t,
            width, height,
            channels,
			bytes_per_pixel,
            stride,
			image_size,
            offset,
			format
        );
        offset += image_size;
		bitmap_array.total_mem_size += bitmap_array.meta[i].size;
    }
    bitmap_array.data = (zest_byte*)ZEST_UTILITIES_MALLOC(bitmap_array.total_mem_size);
    memset(bitmap_array.data, 0, bitmap_array.total_mem_size);
	return bitmap_array;
}

void zest_FreeBitmapArray(zest_bitmap_array_t* images) {
    if (images->data) {
        ZEST_UTILITIES_FREE(images->data);
    }
    ZEST_UTILITIES_FREE(images->meta);
    images->data = 0;
	images->meta = 0;
    images->size_of_array = 0;
}

zest_bool zest_AllocateBitmap(zest_bitmap_t *bitmap, int width, int height, zest_format format) {
	ZEST_ASSERT(width && height);
	zest_GetFormatPixelData(format, &bitmap->meta.channels, &bitmap->meta.bytes_per_pixel);
    bitmap->meta.size = width * height * bitmap->meta.channels * bitmap->meta.bytes_per_pixel;
    if (bitmap->meta.size > 0) {
        bitmap->data = (zest_byte*)ZEST_UTILITIES_MALLOC(bitmap->meta.size);
        bitmap->meta.width = width;
        bitmap->meta.height = height;
        bitmap->meta.format = format;
        bitmap->meta.stride = width * bitmap->meta.bytes_per_pixel;
        memset(bitmap->data, 0, bitmap->meta.size);
		return ZEST_TRUE;
    }
	ZEST_PRINT("Format not supported for bitmaps");
	return ZEST_FALSE;
}

void zest_AllocateBitmapMemory(zest_bitmap_t *bitmap, zest_size size_in_bytes) {
	ZEST_ASSERT(size_in_bytes);
    bitmap->meta.size = size_in_bytes;
	bitmap->data = (zest_byte*)ZEST_UTILITIES_MALLOC(size_in_bytes);
}

zest_image_collection_t zest_CreateImageCollection(zest_format format, zest_uint max_images, zest_uint bitmap_array_count, zest_image_collection_flags flags) {
	zest_image_collection_t image_collection = ZEST__ZERO_INIT(zest_image_collection_t);
    image_collection.format = format;
	image_collection.flags = flags;
	if (max_images) {
		image_collection.image_bitmaps = (zest_bitmap_t *)ZEST_UTILITIES_MALLOC(sizeof(zest_bitmap_t) * max_images);
		image_collection.regions = (zest_atlas_region_t *)ZEST_UTILITIES_MALLOC(sizeof(zest_atlas_region_t) * max_images);
	}
	image_collection.max_images = max_images;
	image_collection.bitmap_array.size_of_array = bitmap_array_count;
	image_collection.bitmap_array.meta = (zest_bitmap_meta_t*)ZEST_UTILITIES_MALLOC(sizeof(zest_bitmap_meta_t) * bitmap_array_count);
	return image_collection;
}

zest_image_collection_t zest_CreateImageAtlasCollection(zest_format format, int max_images) {
	ZEST_ASSERT_TILING_FORMAT(format);	//Format not supported for tiled images that will be sampled in a shader
	ZEST_ASSERT(max_images > 0);
	zest_image_collection_t image_collection = ZEST__ZERO_INIT(zest_image_collection_t);
    image_collection.format = format;
	image_collection.flags = zest_image_collection_flag_atlas;
	image_collection.max_images = max_images;
	image_collection.image_bitmaps = (zest_bitmap_t*)ZEST_UTILITIES_MALLOC(sizeof(zest_bitmap_t) * max_images);
	image_collection.regions = (zest_atlas_region_t *)ZEST_UTILITIES_MALLOC(sizeof(zest_atlas_region_t) * max_images);
	return image_collection;
}

zest_bool zest_AllocateImageCollectionBitmapArray(zest_image_collection_t *image_collection) {
	if (image_collection->bitmap_array.size_of_array == 0) {
		ZEST_PRINT("Nothing to allocate in the image collection."); 
		return ZEST_FALSE;
	}
	zest_size total_size = 0;
	for(int i = 0; i != image_collection->bitmap_array.size_of_array; ++i) {
		zest_bitmap_meta_t *meta = &image_collection->bitmap_array.meta[i];
		total_size += meta->size;
	}
    zest_bitmap_array_t *bitmap_array = &image_collection->bitmap_array;
	if (bitmap_array->data) {
		ZEST_UTILITIES_FREE(bitmap_array->data);
	}
	bitmap_array->total_mem_size = total_size;
    bitmap_array->data = (zest_byte*)ZEST_UTILITIES_MALLOC(bitmap_array->total_mem_size);
	return ZEST_TRUE;
}

zest_bool zest_AllocateImageCollectionCopyRegions(zest_image_collection_t *image_collection) {
	ZEST_ASSERT(image_collection->bitmap_array.size_of_array);
	zest_size total_size = 0;
	if (image_collection->buffer_copy_regions) {
		ZEST_UTILITIES_FREE(image_collection->buffer_copy_regions);
	}
    image_collection->buffer_copy_regions = (zest_buffer_image_copy_t*)ZEST_UTILITIES_MALLOC(image_collection->bitmap_array.size_of_array * sizeof(zest_buffer_image_copy_t));
	return ZEST_TRUE;
}

void zest_FreeImageCollection(zest_image_collection_t *image_collection) {
    if(image_collection->regions) ZEST_UTILITIES_FREE(image_collection->regions);
	if(image_collection->image_bitmaps) ZEST_UTILITIES_FREE(image_collection->image_bitmaps);
    if(image_collection->buffer_copy_regions) ZEST_UTILITIES_FREE(image_collection->buffer_copy_regions);
    if(image_collection->bitmap_array.data) ZEST_UTILITIES_FREE(image_collection->bitmap_array.data);
}

zest_bitmap_meta_t zest_ImageCollectionBitmapArrayMeta(zest_image_collection_t *image_collection) {
	if (image_collection->bitmap_array.size_of_array) {
		return image_collection->bitmap_array.meta[0];
	} else {
		return ZEST__ZERO_INIT(zest_bitmap_meta_t);
	}
}

zest_atlas_region_t *zest_AddImageAtlasBitmap(zest_image_collection_t *image_collection, zest_bitmap_t *bitmap) {
	ZEST_ASSERT(bitmap->meta.width, "Width has not been set in the bitmap");		//Not a valid bitmap handle
	ZEST_ASSERT(bitmap->meta.height, "Height has not been set in the bitmap");		//Not a valid bitmap handle
	ZEST_ASSERT(bitmap->meta.format, "Format has not been set in the bitmap");		//Not a valid bitmap handle
	ZEST_ASSERT(image_collection->image_count < image_collection->max_images);		//Run out of room in the image collection!
	if (bitmap) {
		if (bitmap->meta.format != image_collection->format) {
			zest_ConvertBitmap(bitmap, image_collection->format, 255);
		}
		zest_atlas_region_t *region = &image_collection->regions[image_collection->image_count];
		*region = ZEST__ZERO_INIT(zest_atlas_region_t);
		region->width = bitmap->meta.width;
		region->height = bitmap->meta.height;
		region->atlas_index = image_collection->image_count;
		image_collection->image_bitmaps[image_collection->image_count] = *bitmap;
		image_collection->image_count++;
		return region;
	}
	return NULL;
}

zest_atlas_region_t *zest_AddImageAtlasPixels(zest_image_collection_t *image_collection, void *pixels, zest_size size, int width, int height, zest_format format) {
	ZEST_ASSERT(image_collection->image_count < image_collection->max_images, "No more room for new images in the image collection");
	int channels, bytes_per_pixel;
	zest_GetFormatPixelData(format, &channels, &bytes_per_pixel);
	ZEST_ASSERT(size == width * height * bytes_per_pixel, "The pixel data size does not match the expected size based on the width, height and format of the image. Make sure you're passing in the correct parameters to the function.");
	zest_bitmap_t bitmap = ZEST__ZERO_INIT(zest_bitmap_t);
	bitmap.data = (zest_byte*)ZEST_UTILITIES_MALLOC(size);
	memcpy(bitmap.data, pixels, size);
	bitmap.meta.width = width;
	bitmap.meta.height = height;
	bitmap.meta.channels = channels;
	bitmap.meta.bytes_per_pixel = bytes_per_pixel;
	bitmap.meta.stride = width * bytes_per_pixel;
	bitmap.meta.size = size;
	bitmap.meta.format = format;
	if (format != image_collection->format) {
		zest_ConvertBitmap(&bitmap, image_collection->format, 255);
	}
	zest_atlas_region_t *region = &image_collection->regions[image_collection->image_count];
	*region = ZEST__ZERO_INIT(zest_atlas_region_t);
	region->width = bitmap.meta.width;
	region->height = bitmap.meta.height;
	region->atlas_index = image_collection->image_count;
	image_collection->image_bitmaps[image_collection->image_count] = bitmap;
	image_collection->image_count++;
	return region;
}

zest_atlas_region_t *zest_AddImageAtlasAnimationPixels(zest_image_collection_t *image_collection, void *pixels, zest_size size, int width, int height, int frame_width, int frame_height, int frames, zest_format format) {
	ZEST_ASSERT(image_collection->image_count + frames < image_collection->max_images, "No more room for new images in the image collection");
	int channels, bytes_per_pixel;
	zest_GetFormatPixelData(format, &channels, &bytes_per_pixel);

	zest_uint rows = height / frame_height;
	zest_uint cols = width / frame_width;

	int frame_count = 0;

	zest_bitmap_t atlas = ZEST__ZERO_INIT(zest_bitmap_t);
	atlas.data = (zest_byte*)pixels;
	atlas.meta.width = width;
	atlas.meta.height = height;
	atlas.meta.bytes_per_pixel = bytes_per_pixel;
	atlas.meta.channels = channels;
	atlas.meta.stride = width * bytes_per_pixel;
	atlas.meta.format = format;
	zest_atlas_region_t *first_region = &image_collection->regions[image_collection->image_count];

	for (zest_uint r = 0; r != rows; ++r) {
		for (zest_uint c = 0; c != cols; ++c) {
			if (frame_count >= frames) {
				break;
			}
			zest_bitmap_t bitmap_frame = ZEST__ZERO_INIT(zest_bitmap_t);
			zest_atlas_region_t *region = &image_collection->regions[image_collection->image_count];
			*region = ZEST__ZERO_INIT(zest_atlas_region_t);
			zest_AllocateBitmap(&bitmap_frame, frame_width, frame_height, format);
			zest_CopyBitmap(&atlas, c * frame_width, r * frame_height, frame_width, frame_height, &bitmap_frame, 0, 0);
			region->width = bitmap_frame.meta.width;
			region->height = bitmap_frame.meta.height;
			region->frames = frames;
			region->atlas_index = image_collection->image_count;
			image_collection->image_bitmaps[image_collection->image_count] = bitmap_frame;
			image_collection->image_count++;
			frame_count++;
		}
	}
	return first_region;
}

zest_bitmap_t *zest_GetLastBitmap(zest_image_collection_t *image_collection) {
	if (image_collection->image_count > 0) {
		return &image_collection->image_bitmaps[image_collection->image_count - 1];
	}
	return NULL;
}

void zest_FreeBitmapData(zest_bitmap_t *image) {
    if (!image->is_imported && image->data) {
        ZEST_UTILITIES_FREE(image->data);
    }
    image->data = 0;
}

void zest_FreeBitmap(zest_bitmap_t *bitmap) {
    if (!bitmap->is_imported && bitmap->data) {
        ZEST_UTILITIES_FREE(bitmap->data);
    }
    bitmap->data = ZEST_NULL;
}

void zest_CopyBitmap(zest_bitmap_t *src, int from_x, int from_y, int width, int height, zest_bitmap_t *dst, int to_x, int to_y) {
    ZEST_ASSERT(src->data, "No data was found in the source image");
	ZEST_ASSERT(dst->data, "No data was found in the destination image");
    ZEST_ASSERT(src->meta.format == dst->meta.format, "Both bitmaps must be the same format");

	if (from_x + width > src->meta.width) {
		width = src->meta.width - from_x;
	}
	if (from_y + height > src->meta.height) {
		height = src->meta.height - from_y;
	}

	if (to_x + width > dst->meta.width) {
		to_x = dst->meta.width - width;
	}
	if (to_y + height > dst->meta.height) {
		to_y = dst->meta.height - height;
	}

    if (src->data && dst->data && width > 0 && height > 0) {
        int src_row = from_y * src->meta.stride + (from_x * src->meta.bytes_per_pixel);
        int dst_row = to_y * dst->meta.stride + (to_x * dst->meta.bytes_per_pixel);
        size_t row_size = width * src->meta.bytes_per_pixel;
        int rows_copied = 0;
        while (rows_copied < height) {
            memcpy(dst->data + dst_row, src->data + src_row, row_size);
            rows_copied++;
            src_row += src->meta.stride;
            dst_row += dst->meta.stride;
        }
    }
}

zest_bool zest_GetBestFit(zest_context context, zest_image_collection_t *image_collection, zest_uint *width, zest_uint *height) {
    stbrp_rect* rects = (stbrp_rect*)ZEST_UTILITIES_MALLOC(image_collection->image_count * sizeof(stbrp_rect));
    stbrp_rect* rects_to_process = (stbrp_rect*)ZEST_UTILITIES_MALLOC(image_collection->image_count * sizeof(stbrp_rect));

	zest_uint max_image_size = zest_GetMaxImageSize(context);

	ZEST_ASSERT(max_image_size > *width, "Width you passed in is greater then the maximum available image size");
	ZEST_ASSERT(max_image_size > *height, "Height you passed in is greater then the maximum available image size");

	zest_uint image_count = image_collection->image_count;

    for(int i = 0; i != image_count; ++i) {
        zest_bitmap_t *bitmap = &image_collection->image_bitmaps[i];
        stbrp_rect rect;

        rect.w = bitmap->meta.width + image_collection->packed_border_size * 2;
        rect.h = bitmap->meta.height + image_collection->packed_border_size * 2;
        rect.x = 0;
        rect.y = 0;
        rect.was_packed = 0;
        rect.id = i;

		rects[i] = rect;
    }

	zest_uint dim_flip = 1;

	while (1) {
		const zest_uint node_count = *width;
		stbrp_node* nodes = (stbrp_node*)ZEST_UTILITIES_MALLOC(node_count * sizeof(stbrp_node));

		stbrp_context rp_context;
		stbrp_init_target(&rp_context, *width, *height, nodes, node_count);
		if (stbrp_pack_rects(&rp_context, rects, image_count)) {
			*width = 0;
			*height = 0;
			for(int i = 0; i != image_count; ++i) {
				*width = ZEST__MAX(*width, (zest_uint)rects[i].x + rects[i].w);
				*height = ZEST__MAX(*height, (zest_uint)rects[i].y + rects[i].h);
			}
			*width = (zest_uint)zest_GetNextPower(*width);
			*height = (zest_uint)zest_GetNextPower(*height);
			break;
		} else {
			if (dim_flip) {
				*width *= 2;
			} else {
				*height *= 2;
			}
			dim_flip = dim_flip ^ 1;
		}
		ZEST_UTILITIES_FREE(nodes);
	}
	return ZEST_TRUE;
}

zest_byte zest__calculate_texture_layers(stbrp_rect* rects, zest_uint image_count, zest_uint width, zest_uint height, const zest_uint node_count) {
    stbrp_node* nodes = (stbrp_node*)ZEST_UTILITIES_MALLOC(sizeof(stbrp_node) * node_count);
    zest_byte layers = 0;
    stbrp_rect* current_rects = (stbrp_rect*)ZEST_UTILITIES_MALLOC(sizeof(stbrp_rect) * image_count);
    stbrp_rect* rects_copy = (stbrp_rect*)ZEST_UTILITIES_MALLOC(sizeof(stbrp_rect) * image_count);
    memcpy(rects_copy, rects, sizeof(stbrp_rect) * image_count);
	zest_uint current_rects_count = 0;
	zest_uint copy_count = image_count;
    while (copy_count > 0 && layers <= 255) {

        stbrp_context context;
        stbrp_init_target(&context, width, height, nodes, node_count);
        stbrp_pack_rects(&context, rects_copy, (int)copy_count);

        for(zest_uint i = 0; i != copy_count; ++i) {
			current_rects[current_rects_count++] = rects_copy[i];
        }

		copy_count = 0;

        for(zest_uint i = 0; i != current_rects_count; ++i) {
            if (!current_rects[i].was_packed) {
				rects_copy[i] = current_rects[i];
            }
        }

		current_rects_count = 0;
        layers++;
    }

	ZEST_UTILITIES_FREE(nodes);
	ZEST_UTILITIES_FREE(rects_copy);
	ZEST_UTILITIES_FREE(current_rects);
    return layers;
}

void zest__pack_images(zest_image_collection_t *atlas, zest_uint layer_width, zest_uint layer_height, zest_uint max_image_size) {
    stbrp_rect* rects = (stbrp_rect*)ZEST_UTILITIES_MALLOC(atlas->image_count * sizeof(stbrp_rect));
    stbrp_rect* rects_to_process = (stbrp_rect*)ZEST_UTILITIES_MALLOC(atlas->image_count * sizeof(stbrp_rect));

	ZEST_ASSERT(max_image_size > layer_width, "Width you passed in is greater then the maximum available image size");
	ZEST_ASSERT(max_image_size > layer_height, "Height you passed in is greater then the maximum available image size");

	zest_uint image_count = atlas->image_count;

    for(int i = 0; i != image_count; ++i) {
        zest_bitmap_t bitmap = atlas->image_bitmaps[i];
        stbrp_rect rect;

        rect.w = bitmap.meta.width + atlas->packed_border_size * 2;
        rect.h = bitmap.meta.height + atlas->packed_border_size * 2;
        rect.x = 0;
        rect.y = 0;
        rect.was_packed = 0;
        rect.id = i;

        ZEST_ASSERT(layer_width >= (zest_uint)rect.w, "A bitmap in the collection is too big for the layer size in the image");		
        ZEST_ASSERT(layer_height >= (zest_uint)rect.h, "A bitmap in the collection is too big for the layer size in the image");

		rects[i] = rect;
		rects_to_process[i] = rect;
    }

    const zest_uint node_count = layer_width;
	stbrp_node* nodes = (stbrp_node*)ZEST_UTILITIES_MALLOC(node_count * sizeof(stbrp_node));

	zest_uint layer_count = zest__calculate_texture_layers(rects, image_count, layer_width, layer_height, node_count);

	zest_FreeBitmapArray(&atlas->bitmap_array);
	zest_format format = atlas->format;
	atlas->bitmap_array = zest_CreateBitmapArray(layer_width, layer_height, format, layer_count);
	zest_AllocateImageCollectionCopyRegions(atlas);

    zest_uint current_layer = 0;

    zest_color_t fillcolor;
    fillcolor.r = 0;
    fillcolor.g = 0;
    fillcolor.b = 0;
    fillcolor.a = 0;

    stbrp_rect* current_rects = (stbrp_rect*)ZEST_UTILITIES_MALLOC(sizeof(stbrp_rect) * image_count);
	zest_uint rect_count = image_count;
    while (rect_count > 0 && current_layer < layer_count) {
        stbrp_context rp_context;
        stbrp_init_target(&rp_context, layer_width, layer_height, nodes, node_count);
        stbrp_pack_rects(&rp_context, rects, (int)rect_count);

		memcpy(current_rects, rects, sizeof(stbrp_rect) * rect_count);

        rect_count = 0;

        zest_bitmap_t tmp_image = { 0 };
        zest_AllocateBitmap(&tmp_image, layer_width, layer_height, format);
        int count = 0;

        for(zest_uint i = 0; i != image_count; ++i) {
            stbrp_rect* rect = &current_rects[i];

            if (rect->was_packed) {
                zest_atlas_region_t *region = &atlas->regions[rect->id];

                float rect_x = (float)rect->x + atlas->packed_border_size;
                float rect_y = (float)rect->y + atlas->packed_border_size;

                region->uv.x = (rect_x + 0.5f) / (float)layer_width;
                region->uv.y = (rect_y + 0.5f) / (float)layer_height;
                region->uv.z = ((float)region->width + (rect_x - 0.5f)) / (float)layer_width;
                region->uv.w = ((float)region->height + (rect_y - 0.5f)) / (float)layer_height;
                region->uv_packed = zest_Pack16bit4SNorm(region->uv.x, region->uv.y, region->uv.z, region->uv.w);
                region->left = (zest_uint)rect_x;
                region->top = (zest_uint)rect_y;
                region->layer_index = current_layer;

                zest_CopyBitmap(&atlas->image_bitmaps[rect->id], 0, 0, region->width, region->height, &tmp_image, region->left, region->top);

                count++;
            }
            else {
				rects[rect_count++] = *rect;
            }
        }
		image_count = rect_count;

        memcpy(zest_BitmapArrayLookUp(&atlas->bitmap_array, current_layer), tmp_image.data, atlas->bitmap_array.meta[current_layer].size);

        current_layer++;
    }

    size_t offset = 0;

	zest_bitmap_array_t *bitmap_array = &atlas->bitmap_array;
    for(zest_uint i = 0; i != bitmap_array->size_of_array; ++i) {
        zest_buffer_image_copy_t buffer_copy_region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
        buffer_copy_region.image_aspect = zest_image_aspect_color_bit;
        buffer_copy_region.mip_level = 0;
        buffer_copy_region.base_array_layer = i;
        buffer_copy_region.layer_count = 1;
        buffer_copy_region.image_extent.width = bitmap_array->meta[i].width;
        buffer_copy_region.image_extent.height = bitmap_array->meta[i].height;
        buffer_copy_region.image_extent.depth = 1;
        buffer_copy_region.buffer_offset = bitmap_array->meta[i].offset;

		atlas->buffer_copy_regions[i] = buffer_copy_region;
    }

	ZEST_UTILITIES_FREE(nodes);
	ZEST_UTILITIES_FREE(rects);
	ZEST_UTILITIES_FREE(rects_to_process);
	ZEST_UTILITIES_FREE(current_rects);
}

zest_atlas_region_t zest_NewAtlasRegion() {
    zest_atlas_region_t region = ZEST__ZERO_INIT(zest_atlas_region_t);
    region.uv.x = 0.f;
    region.uv.y = 0.f;
    region.uv.z = 1.f;
    region.uv.w = 1.f;
    region.layer_index = 0;
    region.frames = 1;
    return region;
}

zest_image_handle zest_CreateImageAtlas(zest_context context, zest_image_collection_t *atlas, zest_uint layer_width, zest_uint layer_height, zest_image_flags flags) {
	zest_image_info_t image_info = zest_CreateImageInfo(layer_width, layer_height);
	image_info.format = atlas->format;
	zest__pack_images(atlas, layer_width, layer_height, zest_GetMaxImageSize(context));
	image_info.layer_count = atlas->bitmap_array.size_of_array;

	if (flags == 0) {
		image_info.flags = zest_image_preset_texture_mipmaps;
	} else {
		ZEST_ASSERT(flags & zest_image_preset_texture, "If you pass in flags to the zest_CreateImageAtlas function then it must at lease contain zest_image_preset_texture. You can leave as 0 an image with mipmaps will be created.");
        image_info.flags = flags;
	}

	zest_device device = zest_GetContextDevice(context);

	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(image_handle);

    zest_size image_size = atlas->bitmap_array.total_mem_size;

    zest_buffer staging_buffer = zest_CreateStagingBuffer(device, image_size, atlas->bitmap_array.data);

    if (!staging_buffer) {
        goto cleanup;
    }

    zest_uint width = atlas->bitmap_array.meta[0].width;
    zest_uint height = atlas->bitmap_array.meta[0].height;

    zest_uint mip_levels =  zest_ImageInfo(image)->mip_levels;
	zest_uint layer_count = atlas->bitmap_array.size_of_array;

	zest_queue queue = zest_imm_BeginCommandBuffer(context->device, zest_queue_graphics);
    zest_imm_TransitionImage(queue, image, zest_image_layout_transfer_dst_optimal, 0, mip_levels, 0, layer_count);
	zest_imm_CopyBufferRegionsToImage(queue, atlas->buffer_copy_regions, atlas->bitmap_array.size_of_array, staging_buffer, image);
	if (mip_levels > 1) {
		zest_imm_GenerateMipMaps(queue, image);
	} else {
		zest_imm_TransitionImage(queue, image, zest_image_layout_shader_read_only_optimal, 0, mip_levels, 0, layer_count);
	}
	zest_imm_EndCommandBuffer(queue);

	zest_FreeBuffer(staging_buffer);
    return image_handle;

    cleanup:
	zest_FreeImage(image_handle);
	zest_FreeBuffer(staging_buffer);
    return ZEST__ZERO_INIT(zest_image_handle);
}

zest_byte *zest_GetImageCollectionRawBitmap(zest_image_collection_t *image_collection, zest_uint bitmap_index) {
	return zest_BitmapArrayLookUp(&image_collection->bitmap_array, bitmap_index);
}

void zest_SetImageCollectionBitmapMeta(zest_image_collection_t *image_collection, zest_uint bitmap_index, zest_uint width, zest_uint height, zest_uint channels, zest_uint stride, zest_size size_in_bytes, zest_size offset) {
	ZEST_ASSERT(bitmap_index < image_collection->bitmap_array.size_of_array);	//bitmap index is out of bounds
    zest_bitmap_array_t *bitmap_array = &image_collection->bitmap_array;
	bitmap_array->meta[bitmap_index].width = width;
	bitmap_array->meta[bitmap_index].height = height;
	bitmap_array->meta[bitmap_index].width = width;
	bitmap_array->meta[bitmap_index].channels = channels;
	bitmap_array->meta[bitmap_index].stride = stride;
	bitmap_array->meta[bitmap_index].offset = offset;
	bitmap_array->meta[bitmap_index].size = size_in_bytes;
}

zest_bitmap_array_t *zest_GetImageCollectionBitmapArray(zest_image_collection_t *image_collection) {
	return &image_collection->bitmap_array;
}

zest_bool zest_ImageCollectionCopyToBitmapArray(zest_image_collection_t *image_collection, zest_uint bitmap_index, const void *src_data, zest_size src_size) {
	zest_byte *raw_bitmap_data = zest_GetImageCollectionRawBitmap(image_collection, bitmap_index);
	memcpy(raw_bitmap_data, src_data, src_size);
	return ZEST_TRUE;
}

// End Image_collections

#endif

//-- gltf_loader
#ifdef ZEST_GLTF_LOADER

#define CGLTF_IMPLEMENTATION
#include "examples/libs/cgltf.h"

// Transform point by column-major 4x4 matrix (cgltf format)
static inline void zest__transform_point_cgltf(const float m[16], float p[3]) {
	float x = p[0], y = p[1], z = p[2];
	p[0] = m[0]*x + m[4]*y + m[8]*z  + m[12];
	p[1] = m[1]*x + m[5]*y + m[9]*z  + m[13];
	p[2] = m[2]*x + m[6]*y + m[10]*z + m[14];
}

// Transform direction by column-major 4x4 matrix (rotation/scale only)
static inline void zest__transform_direction_cgltf(const float m[16], float d[3]) {
	float x = d[0], y = d[1], z = d[2];
	d[0] = m[0]*x + m[4]*y + m[8]*z;
	d[1] = m[1]*x + m[5]*y + m[9]*z;
	d[2] = m[2]*x + m[6]*y + m[10]*z;
}

// Normalize vec3 in-place
static inline void zest__normalize_vec3_cgltf(float v[3]) {
	float len = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
	if (len > 1e-8f) {
		float inv_len = 1.0f / len;
		v[0] *= inv_len;
		v[1] *= inv_len;
		v[2] *= inv_len;
	}
}

zest_mesh LoadGLTFScene(zest_context context, const char* filepath, float adjust_scale) {
	cgltf_options options = {0};
	cgltf_data* data = NULL;

	// Parse and load buffer data
	if (cgltf_parse_file(&options, filepath, &data) != cgltf_result_success) {
		return NULL;
	}

	if (cgltf_load_buffers(&options, data, filepath) != cgltf_result_success) {
		cgltf_free(data);
		return NULL;
	}

	zest_mesh mesh = zest_NewStandardMesh(context);
	zest_color_t packed_color = zest_ColorSet(255, 255, 255, 255);
	zest_u64 packed_tangent = 0;

	// Load all meshes from scene nodes (applies world transforms)
	for (cgltf_size n = 0; n < data->nodes_count; n++) {
		cgltf_node* node = &data->nodes[n];
		if (!node->mesh) continue;

		cgltf_mesh* gltf_mesh = node->mesh;

		// Compute world transform for this node
		float world_matrix[16];
		cgltf_node_transform_world(node, world_matrix);

		for (cgltf_size p = 0; p < gltf_mesh->primitives_count; p++) {
			cgltf_primitive* prim = &gltf_mesh->primitives[p];

			// Find accessors
			cgltf_accessor* pos_acc = NULL;
			cgltf_accessor* norm_acc = NULL;
			cgltf_accessor* tangent_acc = NULL;
			cgltf_accessor* uv_acc = NULL;
			cgltf_accessor* color_acc = NULL;
			for (cgltf_size a = 0; a < prim->attributes_count; a++) {
				if (prim->attributes[a].type == cgltf_attribute_type_position) {
					pos_acc = prim->attributes[a].data;
				}
				if (prim->attributes[a].type == cgltf_attribute_type_normal) {
					norm_acc = prim->attributes[a].data;
				}
				if (prim->attributes[a].type == cgltf_attribute_type_tangent) {
					tangent_acc = prim->attributes[a].data;
				}
				if (prim->attributes[a].type == cgltf_attribute_type_texcoord) {
					uv_acc = prim->attributes[a].data;
				}
				if (prim->attributes[a].type == cgltf_attribute_type_color) {
					color_acc = prim->attributes[a].data;
				}
			}
			if (!pos_acc) continue;

			// Track base vertex for indexing
			zest_uint base_vertex = zest_MeshVertexCount(mesh);

			// Read vertices
			for (cgltf_size v = 0; v < pos_acc->count; v++) {
				float pos[3], norm[3] = { 0, 0, 1 };
				float tangent[4] = { 0 };
				float color[4] = { 0 };
				float uv[2] = { 0 };
				cgltf_accessor_read_float(pos_acc, v, pos, 3);
				zest__transform_point_cgltf(world_matrix, pos);
				pos[0] *= adjust_scale;
				pos[1] *= adjust_scale;
				pos[2] *= adjust_scale;
				if (norm_acc) {
					cgltf_accessor_read_float(norm_acc, v, norm, 3);
					zest__transform_direction_cgltf(world_matrix, norm);
					zest__normalize_vec3_cgltf(norm);
				}
				if (tangent_acc) {
					cgltf_accessor_read_float(tangent_acc, v, tangent, 4);
					float handedness = tangent[3];
					zest__transform_direction_cgltf(world_matrix, tangent);
					zest__normalize_vec3_cgltf(tangent);
					tangent[3] = handedness;
					packed_tangent = zest_Pack16bit4SNorm(tangent[0], tangent[1], tangent[2], tangent[3]);
				}
				if (uv_acc) {
					cgltf_accessor_read_float(uv_acc, v, uv, 2);
				}
				if (color_acc) {
					cgltf_accessor_read_float(color_acc, v, color, 4);
					packed_color = zest_ColorSet((zest_byte)(color[0] * 255.f), (zest_byte)(color[1] * 255.f), (zest_byte)(color[2] * 255.f), (zest_byte)(color[3] * 255.f));
				} else if(prim->material && prim->material->has_pbr_metallic_roughness) {
					cgltf_float *base = prim->material->pbr_metallic_roughness.base_color_factor;
					packed_color = zest_ColorSet((zest_byte)(base[0] * 255.f), (zest_byte)(base[1] * 255.f), (zest_byte)(base[2] * 255.f), (zest_byte)(base[3] * 255.f));
				} else {
					packed_color = zest_ColorSet(255, 255, 255, 255);
				}

				zest_PushMeshVertex(mesh, pos, norm, uv, packed_tangent, packed_color, 0);
			}

			// Read indices
			if (prim->indices) {
				for (cgltf_size i = 0; i < prim->indices->count; i++) {
					cgltf_size idx = cgltf_accessor_read_index(prim->indices, i);
					zest_PushMeshIndex(mesh, (zest_uint)(base_vertex + idx));
				}
			}
		}
	}

	cgltf_free(data);
	return mesh;
}

zest_gltf_t LoadGLTF(zest_context context, const char* filepath) {
	cgltf_options options = {0};
	cgltf_data* data = NULL;

	// Parse and load buffer data
	if (cgltf_parse_file(&options, filepath, &data) != cgltf_result_success) {
		return ZEST__ZERO_INIT(zest_gltf_t);
	}

	if (cgltf_load_buffers(&options, data, filepath) != cgltf_result_success) {
		cgltf_free(data);
		return ZEST__ZERO_INIT(zest_gltf_t);
	}

	if (data->meshes_count == 0) {
		cgltf_free(data);
		return ZEST__ZERO_INIT(zest_gltf_t);
	}

	zest_uint mesh_count = data->materials_count ? data->materials_count : 1;
	zest_uint *primitive_counts;
	if (data->materials_count) {
		zest_size size = sizeof(zest_uint) * data->materials_count;
		primitive_counts = (zest_uint*)ZEST_UTILITIES_MALLOC(size);
		memset(primitive_counts, 0, size);
	} else {
		primitive_counts = (zest_uint*)ZEST_UTILITIES_MALLOC(sizeof(zest_uint));
		primitive_counts[0] = 0;
	}

	for (cgltf_size n = 0; n < data->meshes_count; n++) {
		cgltf_mesh* gltf_mesh = &data->meshes[n];
		for (cgltf_size p = 0; p < gltf_mesh->primitives_count; p++) {
			cgltf_primitive* prim = &gltf_mesh->primitives[p];
			zest_uint material_index = (zest_uint)(prim->material - data->materials);
			primitive_counts[material_index]++;
		}
	}

	zest_color_t packed_color = zest_ColorSet(255, 255, 255, 255);
	zest_u64 packed_tangent = 0;
	zest_gltf_t model = ZEST__ZERO_INIT(zest_gltf_t);
	model.meshes = (zest_mesh*)ZEST_UTILITIES_MALLOC(sizeof(zest_mesh) * mesh_count);
	memset(model.meshes, 0, sizeof(zest_mesh) * mesh_count);
	model.mesh_count = mesh_count;

	for (cgltf_size n = 0; n < data->nodes_count; n++) {
		cgltf_node* node = &data->nodes[n];
		if (!node->mesh) continue;
		cgltf_mesh* gltf_mesh = node->mesh;

		// Compute world transform for this node
		float world_matrix[16];
		cgltf_node_transform_world(node, world_matrix);

		for (cgltf_size p = 0; p < gltf_mesh->primitives_count; p++) {
			cgltf_primitive* prim = &gltf_mesh->primitives[p];

			zest_uint material_index = (zest_uint)(prim->material - data->materials);
			if (!model.meshes[material_index]) {
				zest_mesh mesh = zest_NewStandardMesh(context);
				mesh->material = material_index;
				model.meshes[n] = mesh;
			}
			zest_mesh mesh = model.meshes[material_index];

			// Find accessors
			cgltf_accessor* pos_acc = NULL;
			cgltf_accessor* norm_acc = NULL;
			cgltf_accessor* tangent_acc = NULL;
			cgltf_accessor* uv_acc = NULL;
			cgltf_accessor* color_acc = NULL;
			for (cgltf_size a = 0; a < prim->attributes_count; a++) {
				if (prim->attributes[a].type == cgltf_attribute_type_position) {
					pos_acc = prim->attributes[a].data;
				}
				if (prim->attributes[a].type == cgltf_attribute_type_normal) {
					norm_acc = prim->attributes[a].data;
				}
				if (prim->attributes[a].type == cgltf_attribute_type_tangent) {
					tangent_acc = prim->attributes[a].data;
				}
				if (prim->attributes[a].type == cgltf_attribute_type_texcoord) {
					uv_acc = prim->attributes[a].data;
				}
				if (prim->attributes[a].type == cgltf_attribute_type_color) {
					color_acc = prim->attributes[a].data;
				}
			}
			if (!pos_acc) continue;

			// Track base vertex for indexing
			zest_uint base_vertex = zest_MeshVertexCount(mesh);

			// Read vertices
			for (cgltf_size v = 0; v < pos_acc->count; v++) {
				float pos[3], norm[3] = { 0, 0, 1 };
				float tangent[4] = { 0 };
				float color[4] = { 0 };
				float uv[2] = { 0 };
				cgltf_accessor_read_float(pos_acc, v, pos, 3);
				zest__transform_point_cgltf(world_matrix, pos);
				if (norm_acc) {
					cgltf_accessor_read_float(norm_acc, v, norm, 3);
					zest__transform_direction_cgltf(world_matrix, norm);
					zest__normalize_vec3_cgltf(norm);
				}
				if (tangent_acc) {
					cgltf_accessor_read_float(tangent_acc, v, tangent, 4);
					float handedness = tangent[3];
					zest__transform_direction_cgltf(world_matrix, tangent);
					zest__normalize_vec3_cgltf(tangent);
					tangent[3] = handedness;
					packed_tangent = zest_Pack16bit4SNorm(tangent[0], tangent[1], tangent[2], tangent[3]);
				}
				if (uv_acc) {
					cgltf_accessor_read_float(uv_acc, v, uv, 2);
				}
				if (color_acc) {
					cgltf_accessor_read_float(color_acc, v, color, 4);
					packed_color = zest_ColorSet((zest_byte)(color[0] * 255.f), (zest_byte)(color[1] * 255.f), (zest_byte)(color[2] * 255.f), (zest_byte)(color[3] * 255.f));
				} else if(prim->material && prim->material->has_pbr_metallic_roughness) {
					cgltf_float *base = prim->material->pbr_metallic_roughness.base_color_factor;
					packed_color = zest_ColorSet((zest_byte)(base[0] * 255.f), (zest_byte)(base[1] * 255.f), (zest_byte)(base[2] * 255.f), (zest_byte)(base[3] * 255.f));
				} else {
					packed_color = zest_ColorSet(255, 255, 255, 255);
				}

				zest_PushMeshVertex(mesh, pos, norm, uv, packed_tangent, packed_color, 0);
			}

			// Read indices
			if (prim->indices) {
				for (cgltf_size i = 0; i < prim->indices->count; i++) {
					cgltf_size idx = cgltf_accessor_read_index(prim->indices, i);
					zest_PushMeshIndex(mesh, (zest_uint)(base_vertex + idx));
				}
			}
		}
	}


	//Load the material. This is very simple and just loads the first material for example purposes
	if (data->materials_count > 0) {
		model.materials = (zest_material_t*)ZEST_UTILITIES_MALLOC(sizeof(zest_material_t) * data->materials_count);
		memset(model.materials, 0, sizeof(zest_material_t) * data->materials_count);
		model.mesh_count = mesh_count;
		for (int i = 0; i != data->materials_count; i++) {
			cgltf_material *material = &data->materials[i];
			if (material->has_pbr_metallic_roughness) {
				cgltf_texture_view *base_color_texture = &material->pbr_metallic_roughness.base_color_texture;

				if (base_color_texture->texture) {
					cgltf_texture *texture = base_color_texture->texture;

					if (texture->image) {
						cgltf_image *image = texture->image;
						if (image->buffer_view) {
							cgltf_buffer_view *view = image->buffer_view;
							cgltf_buffer *buffer = view->buffer;
							unsigned char *pixel_data = (unsigned char *)buffer->data + view->offset;
							size_t data_size = view->size;

							int width, height, channels;
							stbi_uc *bitmap_buffer = stbi_load_from_memory(pixel_data, data_size, &width, &height, &channels, 0);
							if (channels == 3) {
								STBI_FREE(bitmap_buffer);
								bitmap_buffer = stbi_load_from_memory(pixel_data, data_size, &width, &height, &channels, 0);
							}
							ZEST_ASSERT(bitmap_buffer, "Unable to load the font bitmap.");
							zest_size size = width * height * channels;
							zest_image_info_t image_info = zest_CreateImageInfo(width, height);
							image_info.flags = zest_image_preset_texture_mipmaps;
							model.materials[i].image = zest_CreateImageWithPixels(context->device, bitmap_buffer, size, &image_info);
							zest_image material_image = zest_GetImage(model.materials[i].image);
							model.materials[i].base_color = zest_AcquireSampledImageIndex(context->device, material_image, zest_texture_array_binding);
							STBI_FREE(bitmap_buffer);
						} else if (image->uri) {
							ZEST_PRINT("Image is external: %s\n", image->uri);
						}
					}
				}
			}
		}
	}

	cgltf_free(data);
	return model;
}

void zest_FreeGLTF(zest_gltf_t *model) {
	for (int i = 0; i != model->mesh_count; i++) {
		zest_FreeMesh(model->meshes[i]);
	}
	ZEST_UTILITIES_FREE(model->meshes);
}

#endif
//End gltf_loader

// Mesh_helpers
#ifdef ZEST_MESH_HELPERS_IMPLEMENTATION

zest_mesh zest_NewStandardMesh(zest_context context) {
    return zest_NewMesh(context, sizeof(zest_vertex_t));
}

void zest_PushVertexPositionOnly(zest_mesh mesh, float pos_x, float pos_y, float pos_z, zest_color_t color) {
    ZEST_ASSERT(mesh->vertex_struct_size == sizeof(zest_vertex_t));
    zest_vertex_t vertex = { {pos_x, pos_y, pos_z}, color, {0, 0, 0}, {0, 0}, 0, 0 };
    zest_PushMeshVertexData(mesh, &vertex);
}

void zest_PushMeshVertex(zest_mesh mesh, float pos[3], float normal[3], float uv[2], zest_u64 tangent, zest_color_t color, zest_uint group) {
    ZEST_ASSERT(mesh->vertex_struct_size == sizeof(zest_vertex_t));
    zest_vertex_t vertex = { { pos[0], pos[1], pos[2] }, color, { normal[0], normal[1], normal[2] }, { uv[0], uv[1] }, tangent, group };
    zest_PushMeshVertexData(mesh, &vertex);
}

void zest_PositionMesh(zest_mesh mesh, zest_vec3 position) {
    ZEST_ASSERT(mesh->vertex_struct_size == sizeof(zest_vertex_t));
    zest_vertex_t* vertices = (zest_vertex_t*)mesh->vertex_data;
    for (zest_uint i = 0; i < mesh->vertex_count; ++i) {
        vertices[i].pos.x += position.x;
        vertices[i].pos.y += position.y;
        vertices[i].pos.z += position.z;
    }
}

zest_matrix4 zest_RotateMesh(zest_mesh mesh, float pitch, float yaw, float roll) {
    ZEST_ASSERT(mesh->vertex_struct_size == sizeof(zest_vertex_t));
    zest_matrix4 roll_mat = zest_Matrix4RotateZ(roll);
    zest_matrix4 pitch_mat = zest_Matrix4RotateX(pitch);
    zest_matrix4 yaw_mat = zest_Matrix4RotateY(yaw);
    zest_matrix4 rotate_mat = zest_MatrixTransform(&pitch_mat, &yaw_mat);
    rotate_mat = zest_MatrixTransform(&roll_mat, &rotate_mat);
    zest_vertex_t* vertices = (zest_vertex_t*)mesh->vertex_data;
    for (zest_uint i = 0; i < mesh->vertex_count; ++i) {
        zest_vec4 pos = { vertices[i].pos.x, vertices[i].pos.y, vertices[i].pos.z, 1.f };
        pos = zest_MatrixTransformVector(&rotate_mat, pos);
        vertices[i].pos = zest_Vec3Set(pos.x, pos.y, pos.z);
    }
    return rotate_mat;
}

zest_matrix4 zest_TransformMesh(zest_mesh mesh, float pitch, float yaw, float roll, float x, float y, float z, float sx, float sy, float sz) {
    ZEST_ASSERT(mesh->vertex_struct_size == sizeof(zest_vertex_t));
    zest_matrix4 roll_mat = zest_Matrix4RotateZ(roll);
    zest_matrix4 pitch_mat = zest_Matrix4RotateX(pitch);
    zest_matrix4 yaw_mat = zest_Matrix4RotateY(yaw);
    zest_matrix4 rotate_mat = zest_MatrixTransform(&pitch_mat, &yaw_mat);
    rotate_mat = zest_MatrixTransform(&roll_mat, &rotate_mat);
    rotate_mat.v[0].w = x;
    rotate_mat.v[1].w = y;
    rotate_mat.v[2].w = z;
    rotate_mat.v[3].x = sx;
    rotate_mat.v[3].y = sy;
    rotate_mat.v[3].z = sz;
    zest_vertex_t* vertices = (zest_vertex_t*)mesh->vertex_data;
    for (zest_uint i = 0; i < mesh->vertex_count; ++i) {
        zest_vec4 pos = { vertices[i].pos.x, vertices[i].pos.y, vertices[i].pos.z, 1.f };
        pos = zest_MatrixTransformVector(&rotate_mat, pos);
        vertices[i].pos = zest_Vec3Set(pos.x, pos.y, pos.z);
    }
    return rotate_mat;
}

void zest_CalculateNormals(zest_mesh mesh) {
    ZEST_ASSERT(mesh->vertex_struct_size == sizeof(zest_vertex_t));
    zest_vertex_t* vertices = (zest_vertex_t*)mesh->vertex_data;

    // Zero out all normals first
    for (zest_uint i = 0; i < mesh->vertex_count; ++i) {
        vertices[i].normal = zest_Vec3Set(0.f, 0.f, 0.f);
    }

    // Calculate face normals and add them to each vertex of the face
    for (zest_uint i = 0; i < zest_MeshIndexCount(mesh); i += 3) {
        zest_uint i0 = mesh->indexes[i + 0];
        zest_uint i1 = mesh->indexes[i + 1];
        zest_uint i2 = mesh->indexes[i + 2];

        zest_vertex_t v0 = vertices[i0];
        zest_vertex_t v1 = vertices[i1];
        zest_vertex_t v2 = vertices[i2];

        zest_vec3 edge1 = zest_SubVec3(v1.pos, v0.pos);
        zest_vec3 edge2 = zest_SubVec3(v2.pos, v0.pos);

        zest_vec3 normal = zest_CrossProduct(edge1, edge2);
        normal = zest_NormalizeVec3(normal);

        vertices[i0].normal = zest_AddVec3(vertices[i0].normal, normal);
        vertices[i1].normal = zest_AddVec3(vertices[i1].normal, normal);
        vertices[i2].normal = zest_AddVec3(vertices[i2].normal, normal);
    }

    // Normalize all the vertex normals
    for (zest_uint i = 0; i < mesh->vertex_count; ++i) {
        vertices[i].normal = zest_NormalizeVec3(vertices[i].normal);
    }
}

zest_bounding_box_t zest_GetMeshBoundingBox(zest_mesh mesh) {
    ZEST_ASSERT(mesh->vertex_struct_size == sizeof(zest_vertex_t));
    zest_bounding_box_t bb = zest_NewBoundingBox();
    zest_vertex_t* vertices = (zest_vertex_t*)mesh->vertex_data;
    for (zest_uint i = 0; i < mesh->vertex_count; ++i) {
        zest_vec3 pos = vertices[i].pos;
        bb.min_bounds.x = ZEST__MIN(bb.min_bounds.x, pos.x);
        bb.min_bounds.y = ZEST__MIN(bb.min_bounds.y, pos.y);
        bb.min_bounds.z = ZEST__MIN(bb.min_bounds.z, pos.z);
        bb.max_bounds.x = ZEST__MAX(bb.max_bounds.x, pos.x);
        bb.max_bounds.y = ZEST__MAX(bb.max_bounds.y, pos.y);
        bb.max_bounds.z = ZEST__MAX(bb.max_bounds.z, pos.z);
    }
    return bb;
}

void zest_SetMeshGroupID(zest_mesh mesh, zest_uint group_id) {
    ZEST_ASSERT(mesh->vertex_struct_size == sizeof(zest_vertex_t));
    zest_vertex_t* vertices = (zest_vertex_t*)mesh->vertex_data;
    for (zest_uint i = 0; i < mesh->vertex_count; ++i) {
        vertices[i].parameters = group_id;
    }
}

void zest_AddMeshToMesh(zest_mesh dst_mesh, zest_mesh src_mesh) {
    ZEST_ASSERT(dst_mesh->vertex_struct_size == src_mesh->vertex_struct_size);
    zest_uint dst_vertex_count = dst_mesh->vertex_count;
    zest_uint src_vertex_count = src_mesh->vertex_count;
    zest_uint dst_index_size = zest_MeshIndexCount(dst_mesh);
    zest_uint src_index_size = zest_MeshIndexCount(src_mesh);

    // Reserve space for combined vertices
    zest_ReserveMeshVertices(dst_mesh, dst_vertex_count + src_vertex_count);

    // Copy source vertices to destination
    void* dst_vertex_ptr = (char*)dst_mesh->vertex_data + (dst_vertex_count * dst_mesh->vertex_struct_size);
    memcpy(dst_vertex_ptr, src_mesh->vertex_data, src_vertex_count * src_mesh->vertex_struct_size);
    dst_mesh->vertex_count = dst_vertex_count + src_vertex_count;

    // Copy and offset indices
    for (zest_uint i = 0; i < src_index_size; ++i) {
        zest_PushMeshIndex(dst_mesh, src_mesh->indexes[i] + dst_vertex_count);
    }
}

zest_mesh zest_CreateCylinder(zest_context context, int sides, float radius, float height, zest_color_t color, zest_bool cap) {
    float angle_increment = 2.0f * ZEST_PI / sides;

    int vertex_count = sides * 2 + (cap ? sides * 2 : 0);

    zest_mesh mesh = zest_NewStandardMesh(context);
    zest_ReserveMeshVertices(mesh, (zest_uint)vertex_count);
    mesh->vertex_count = (zest_uint)vertex_count;
    zest_vertex_t* vertices = (zest_vertex_t*)mesh->vertex_data;

    for (int i = 0; i < sides; ++i) {
        float angle = i * angle_increment;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);

        vertices[i].pos = zest_Vec3Set( x, height / 2.0f, z );
        vertices[i + sides].pos = zest_Vec3Set( x, -height / 2.0f, z );
    }

    int base_index = sides * 2;

    if (cap) {
        // Generate vertices for the caps of the cylinder
        for (int i = 0; i < sides; ++i) {
            float angle = i * angle_increment;
            float x = radius * cosf(angle);
            float z = radius * sinf(angle);

            vertices[base_index + i].pos = zest_Vec3Set( x, height / 2.0f, z);
            vertices[base_index + sides + i].pos = zest_Vec3Set( x, -height / 2.0f, z);
			vertices[base_index + i].color = color;
			vertices[base_index + i + sides].color = color;
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

zest_mesh zest_CreateCone(zest_context context, int sides, float radius, float height, zest_color_t color) {
    // Calculate the angle between each side
    float angle_increment = 2.0f * ZEST_PI / sides;

    zest_mesh mesh = zest_NewStandardMesh(context);
    zest_uint vertex_count = (zest_uint)sides * 2 + 2;
    zest_ReserveMeshVertices(mesh, vertex_count);
    mesh->vertex_count = vertex_count;
    zest_vertex_t* vertices = (zest_vertex_t*)mesh->vertex_data;

    // Generate vertices for the base of the cone
    for (int i = 0; i < sides; ++i) {
        float angle = i * angle_increment;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);
        vertices[i].pos = zest_Vec3Set( x, 0.0f, z);
        vertices[i].color = color;
    }

    // Generate the vertex for the tip of the cone
    vertices[sides].pos = zest_Vec3Set( 0.0f, height, 0.0f);
    vertices[sides].color = color;

    // Generate indices for the sides of the cone
    for (int i = 0; i < sides; ++i) {
        zest_PushMeshTriangle(mesh, i, sides, (i + 1) % sides);
    }

    int base_index = sides + 1;

    //Base center vertex
    vertices[base_index].pos = zest_Vec3Set( 0.0f, 0.0f, 0.0f);
    vertices[base_index].color = color;

    // Generate another set of vertices for the base of the cone
    for (int i = base_index + 1; i < base_index + sides + 1; ++i) {
        float angle = i * angle_increment;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);
        vertices[i].pos = zest_Vec3Set( x, 0.0f, z);
        vertices[i].color = color;
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

zest_mesh zest_CreateSphere(zest_context context, int rings, int sectors, float radius, zest_color_t color) {
    // Calculate the angles between rings and sectors
    float ring_angle_increment = ZEST_PI / rings;
    float sector_angle_increment = 2.0f * ZEST_PI / sectors;
    float ring_angle;
    float sector_angle;

    zest_mesh mesh = zest_NewStandardMesh(context);

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
            zest_PushVertexPositionOnly(mesh, vertex.x, vertex.y, vertex.z, color);
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

zest_mesh zest_CreateCube(zest_context context, float size, zest_color_t color) {
    zest_mesh mesh = zest_NewStandardMesh(context);
    float half_size = size * .5f;

    // Front face
    zest_PushVertexPositionOnly(mesh, -half_size, -half_size, -half_size, color); // 0
    zest_PushVertexPositionOnly(mesh,  half_size, -half_size, -half_size, color); // 1
    zest_PushVertexPositionOnly(mesh,  half_size,  half_size, -half_size, color); // 2
    zest_PushVertexPositionOnly(mesh, -half_size,  half_size, -half_size, color); // 3
    zest_PushMeshTriangle(mesh, 2, 1, 0);
    zest_PushMeshTriangle(mesh, 0, 3, 2);

    // Back face
    zest_PushVertexPositionOnly(mesh, -half_size, -half_size,  half_size, color); // 4
    zest_PushVertexPositionOnly(mesh, -half_size,  half_size,  half_size, color); // 5
    zest_PushVertexPositionOnly(mesh,  half_size,  half_size,  half_size, color); // 6
    zest_PushVertexPositionOnly(mesh,  half_size, -half_size,  half_size, color); // 7
    zest_PushMeshTriangle(mesh, 6, 5, 4);
    zest_PushMeshTriangle(mesh, 4, 7, 6);

    // Left face
    zest_PushVertexPositionOnly(mesh, -half_size, -half_size, -half_size, color); // 8
    zest_PushVertexPositionOnly(mesh, -half_size,  half_size, -half_size, color); // 9
    zest_PushVertexPositionOnly(mesh, -half_size,  half_size,  half_size, color); // 10
    zest_PushVertexPositionOnly(mesh, -half_size, -half_size,  half_size, color); // 11
    zest_PushMeshTriangle(mesh, 10, 9, 8);
    zest_PushMeshTriangle(mesh, 8, 11, 10);

    // Right face
    zest_PushVertexPositionOnly(mesh,  half_size, -half_size, -half_size, color); // 12
    zest_PushVertexPositionOnly(mesh,  half_size, -half_size,  half_size, color); // 13
    zest_PushVertexPositionOnly(mesh,  half_size,  half_size,  half_size, color); // 14
    zest_PushVertexPositionOnly(mesh,  half_size,  half_size, -half_size, color); // 15
    zest_PushMeshTriangle(mesh, 14, 13, 12);
    zest_PushMeshTriangle(mesh, 12, 15, 14);

    // Bottom face
    zest_PushVertexPositionOnly(mesh, -half_size, -half_size, -half_size, color); // 16
    zest_PushVertexPositionOnly(mesh, -half_size, -half_size,  half_size, color); // 17
    zest_PushVertexPositionOnly(mesh,  half_size, -half_size,  half_size, color); // 18
    zest_PushVertexPositionOnly(mesh,  half_size, -half_size, -half_size, color); // 19
    zest_PushMeshTriangle(mesh, 18, 17, 16);
    zest_PushMeshTriangle(mesh, 16, 19, 18);

    // Top face
    zest_PushVertexPositionOnly(mesh, -half_size,  half_size, -half_size, color); // 20
    zest_PushVertexPositionOnly(mesh,  half_size,  half_size, -half_size, color); // 21
    zest_PushVertexPositionOnly(mesh,  half_size,  half_size,  half_size, color); // 22
    zest_PushVertexPositionOnly(mesh, -half_size,  half_size,  half_size, color); // 23
    zest_PushMeshTriangle(mesh, 22, 21, 20);
    zest_PushMeshTriangle(mesh, 20, 23, 22);

    zest_CalculateNormals(mesh);

    return mesh;
}

zest_mesh zest_CreateRoundedRectangle(zest_context context, float width, float height, float radius, int segments, zest_bool backface, zest_color_t color) {
    zest_mesh mesh = zest_NewStandardMesh(context);

    // Calculate the step angle
    float angle_increment = ZEST_PI / 2.0f / segments;

    width = ZEST__MAX(radius, width - radius * 2.f);
    height = ZEST__MAX(radius, height - radius * 2.f);

    //centre vertex;
	zest_PushVertexPositionOnly(mesh, 0.f, 0.f, 0.0f, color);

    // Bottom left corner
    for (int i = 0; i <= segments; ++i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushVertexPositionOnly(mesh, -width / 2.0f - x, -height / 2.0f - y, 0.0f, color);
    }

    // Bottom right corner
    for (int i = segments; i >= 0; --i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushVertexPositionOnly(mesh, width / 2.0f + x, -height / 2.0f - y, 0.0f, color);
    }

    // Top right corner
    for (int i = 0; i <= segments; ++i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushVertexPositionOnly(mesh, width / 2.0f + x, height / 2.0f + y, 0.0f, color);
    }

    // Top left corner
    for (int i = segments; i >= 0; --i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushVertexPositionOnly(mesh, - width / 2.0f - x, height / 2.0f + y, 0.0f, color);
    }

    zest_uint vertex_count = mesh->vertex_count;

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

#endif
// End Mesh_helpers

#ifdef __cplusplus

}

#endif
