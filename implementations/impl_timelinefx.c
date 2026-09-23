#include "impl_timelinefx.h"
#include "stb_image.h"
#include <zstd.h>
#define BCDEC_STATIC
#define BCDEC_IMPLEMENTATION
#include "bcdec.h"

#define ZEST__TFX_KTX2_HEADER_SIZE 80
#define ZEST__TFX_KTX2_LEVEL_ENTRY_SIZE 24
#define ZEST__TFX_KTX2_MAX_LEVELS 32
#define ZEST__TFX_KTX2_SUPERCOMPRESSION_NONE 0
#define ZEST__TFX_KTX2_SUPERCOMPRESSION_ZSTD 2
//Every copy offset has to be a multiple of its texel or block size, and 16 covers all of the shape formats
#define ZEST__TFX_STAGING_ALIGNMENT 16
//Force the array view so that single frame shapes still sample as a texture2DArray in the shader
#define ZEST__TFX_PIXEL_IMAGE_FLAGS (zest_image_preset_texture_mipmaps | zest_image_flag_force_image_array)
#define ZEST__TFX_BLOCK_IMAGE_FLAGS (zest_image_preset_texture | zest_image_flag_force_image_array)

static const unsigned char zest__tfx_ktx2_identifier[12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };

//How a tfx shape format is read and what it is uploaded as
typedef struct zest__tfx_shape_layout_t {
	zest_format block_format;		//zest_format_undefined for the formats stb_image decodes
	zest_format pixel_format;		//The upload format for decoded pixels
	zest_uint channels;
	zest_component_mapping_t swizzle;
} zest__tfx_shape_layout_t;

//The level pointers point into the file the shape loader was handed
typedef struct zest__tfx_ktx2_t {
	zest_uint width;
	zest_uint height;
	zest_uint layers;
	zest_uint level_count;
	zest_uint supercompression;
	const unsigned char *stored_data[ZEST__TFX_KTX2_MAX_LEVELS];
	zest_size stored_sizes[ZEST__TFX_KTX2_MAX_LEVELS];
	zest_size level_sizes[ZEST__TFX_KTX2_MAX_LEVELS];
} zest__tfx_ktx2_t;

static zest_component_mapping_t zest__tfx_swizzle(zest_component_swizzle r, zest_component_swizzle g, zest_component_swizzle b, zest_component_swizzle a) {
	zest_component_mapping_t mapping = { r, g, b, a };
	return mapping;
}

static zest_bool zest__tfx_shape_layout(tfx_image_format format, zest__tfx_shape_layout_t *layout) {
	*layout = ZEST__ZERO_INIT(zest__tfx_shape_layout_t);
	layout->swizzle = zest__tfx_swizzle(zest_component_swizzle_identity, zest_component_swizzle_identity, zest_component_swizzle_identity, zest_component_swizzle_identity);
	zest_component_mapping_t luminance = zest__tfx_swizzle(zest_component_swizzle_r, zest_component_swizzle_r, zest_component_swizzle_r, zest_component_swizzle_one);
	switch (format) {
	case tfx_image_format_unknown:
	case tfx_image_format_rgba8_png:
	case tfx_image_format_rgba8_raw:
		layout->pixel_format = zest_format_r8g8b8a8_unorm;
		layout->channels = 4;
		return ZEST_TRUE;
	case tfx_image_format_la8_png:
		layout->pixel_format = zest_format_r8g8_unorm;
		layout->channels = 2;
		layout->swizzle = zest_SwizzleLuminanceAlpha();
		return ZEST_TRUE;
	case tfx_image_format_l8_png:
		layout->pixel_format = zest_format_r8_unorm;
		layout->channels = 1;
		layout->swizzle = luminance;
		return ZEST_TRUE;
	case tfx_image_format_a8_png:
		layout->pixel_format = zest_format_r8_unorm;
		layout->channels = 1;
		layout->swizzle = zest_SwizzleAlphaOnly();
		return ZEST_TRUE;
	case tfx_image_format_a_bc4_ktx2:
		layout->block_format = zest_format_bc4_unorm_block;
		layout->pixel_format = zest_format_r8_unorm;
		layout->channels = 1;
		layout->swizzle = zest_SwizzleAlphaOnly();
		return ZEST_TRUE;
	case tfx_image_format_l_bc4_ktx2:
		layout->block_format = zest_format_bc4_unorm_block;
		layout->pixel_format = zest_format_r8_unorm;
		layout->channels = 1;
		layout->swizzle = luminance;
		return ZEST_TRUE;
	case tfx_image_format_la_bc5_ktx2:
		layout->block_format = zest_format_bc5_unorm_block;
		layout->pixel_format = zest_format_r8g8_unorm;
		layout->channels = 2;
		layout->swizzle = zest_SwizzleLuminanceAlpha();
		return ZEST_TRUE;
	case tfx_image_format_rgba_bc7_ktx2:
		layout->block_format = zest_format_bc7_unorm_block;
		layout->pixel_format = zest_format_r8g8b8a8_unorm;
		layout->channels = 4;
		return ZEST_TRUE;
	default:
		return ZEST_FALSE;
	}
}

static zest_uint zest__tfx_read_u32(const unsigned char *bytes) {
	return (zest_uint)bytes[0] | ((zest_uint)bytes[1] << 8) | ((zest_uint)bytes[2] << 16) | ((zest_uint)bytes[3] << 24);
}

static zest_u64 zest__tfx_read_u64(const unsigned char *bytes) {
	return (zest_u64)zest__tfx_read_u32(bytes) | ((zest_u64)zest__tfx_read_u32(bytes + 4) << 32);
}

static zest_uint zest__tfx_block_bytes(zest_format format) {
	int channels, bytes_per_pixel, block_width, block_height, bytes_per_block;
	zest_GetFormatPixelData(format, &channels, &bytes_per_pixel, &block_width, &block_height, &bytes_per_block);
	return (zest_uint)bytes_per_block;
}

static zest_size zest__tfx_block_level_size(zest_uint width, zest_uint height, zest_uint level, zest_uint layers, zest_uint block_bytes) {
	zest_uint level_width = ZEST__MAX(width >> level, 1u);
	zest_uint level_height = ZEST__MAX(height >> level, 1u);
	return (zest_size)((level_width + 3) / 4) * ((level_height + 3) / 4) * block_bytes * layers;
}

//Returns why the file can't be used, or NULL
static const char *zest__tfx_parse_ktx2(const unsigned char *data, zest_size size, zest_format block_format, zest__tfx_ktx2_t *ktx2) {
	*ktx2 = ZEST__ZERO_INIT(zest__tfx_ktx2_t);
	if (!data || size < ZEST__TFX_KTX2_HEADER_SIZE || memcmp(data, zest__tfx_ktx2_identifier, sizeof(zest__tfx_ktx2_identifier)) != 0) {
		return "it is not a KTX2 file";
	}
	if (zest__tfx_read_u32(data + 12) != (zest_uint)block_format) {
		return "the KTX2 vkFormat does not match its shape format";
	}
	if (zest__tfx_read_u32(data + 28) != 0 || zest__tfx_read_u32(data + 36) != 1) {
		return "it is not a 2D KTX2 file with a single face";
	}
	ktx2->width = zest__tfx_read_u32(data + 20);
	ktx2->height = zest__tfx_read_u32(data + 24);
	ktx2->layers = ZEST__MAX(zest__tfx_read_u32(data + 32), 1u);
	ktx2->level_count = ZEST__MAX(zest__tfx_read_u32(data + 40), 1u);
	ktx2->supercompression = zest__tfx_read_u32(data + 44);
	if (ktx2->supercompression != ZEST__TFX_KTX2_SUPERCOMPRESSION_NONE && ktx2->supercompression != ZEST__TFX_KTX2_SUPERCOMPRESSION_ZSTD) {
		return "it uses a KTX2 supercompression scheme this loader can't inflate";
	}
	if (!ktx2->width || !ktx2->height) {
		return "the KTX2 file has no size";
	}
	zest_uint largest = ZEST__MAX(ktx2->width, ktx2->height);
	zest_uint full_level_count = 1;
	while (largest > 1) {
		largest >>= 1;
		full_level_count++;
	}
	if (ktx2->level_count > full_level_count) {
		return "the KTX2 file has more mip levels than its size allows";
	}
	if (ZEST__TFX_KTX2_HEADER_SIZE + (zest_size)ktx2->level_count * ZEST__TFX_KTX2_LEVEL_ENTRY_SIZE > size) {
		return "the KTX2 level index is truncated";
	}
	zest_uint block_bytes = zest__tfx_block_bytes(block_format);
	for (zest_uint level = 0; level != ktx2->level_count; ++level) {
		const unsigned char *entry = data + ZEST__TFX_KTX2_HEADER_SIZE + level * ZEST__TFX_KTX2_LEVEL_ENTRY_SIZE;
		zest_u64 offset = zest__tfx_read_u64(entry);
		zest_u64 length = zest__tfx_read_u64(entry + 8);
		zest_u64 uncompressed_length = zest__tfx_read_u64(entry + 16);
		if (uncompressed_length != zest__tfx_block_level_size(ktx2->width, ktx2->height, level, ktx2->layers, block_bytes)) {
			return "a KTX2 mip level has the wrong size";
		}
		if (!length || offset > size || length > size - offset) {
			return "a KTX2 mip level lies outside the file";
		}
		if (ktx2->supercompression == ZEST__TFX_KTX2_SUPERCOMPRESSION_NONE && length != uncompressed_length) {
			return "a KTX2 mip level has the wrong size";
		}
		ktx2->stored_data[level] = data + offset;
		ktx2->stored_sizes[level] = (zest_size)length;
		ktx2->level_sizes[level] = (zest_size)uncompressed_length;
	}
	return NULL;
}

static zest_bool zest__tfx_inflate_ktx2_level(const zest__tfx_ktx2_t *ktx2, zest_uint level, zest_byte *destination) {
	if (ktx2->supercompression == ZEST__TFX_KTX2_SUPERCOMPRESSION_NONE) {
		memcpy(destination, ktx2->stored_data[level], ktx2->level_sizes[level]);
		return ZEST_TRUE;
	}
	size_t inflated_size = ZSTD_decompress(destination, ktx2->level_sizes[level], ktx2->stored_data[level], ktx2->stored_sizes[level]);
	return !ZSTD_isError(inflated_size) && inflated_size == ktx2->level_sizes[level];
}

//Edge blocks are clipped to the image size
static void zest__tfx_decode_blocks(const zest__tfx_shape_layout_t *layout, const zest_byte *blocks, zest_uint width, zest_uint height, zest_byte *pixels) {
	zest_uint block_bytes = zest__tfx_block_bytes(layout->block_format);
	zest_uint blocks_wide = (width + 3) / 4;
	zest_uint blocks_high = (height + 3) / 4;
	zest_uint channels = layout->channels;
	zest_byte texels[16 * 4];
	for (zest_uint block_y = 0; block_y != blocks_high; ++block_y) {
		for (zest_uint block_x = 0; block_x != blocks_wide; ++block_x) {
			const zest_byte *block = blocks + ((zest_size)block_y * blocks_wide + block_x) * block_bytes;
			switch (layout->block_format) {
			case zest_format_bc4_unorm_block: bcdec_bc4(block, texels, 4); break;
			case zest_format_bc5_unorm_block: bcdec_bc5(block, texels, 4 * 2); break;
			default: bcdec_bc7(block, texels, 4 * 4); break;
			}
			for (zest_uint texel_y = 0; texel_y != 4; ++texel_y) {
				zest_uint y = block_y * 4 + texel_y;
				if (y >= height) {
					break;
				}
				for (zest_uint texel_x = 0; texel_x != 4; ++texel_x) {
					zest_uint x = block_x * 4 + texel_x;
					if (x < width) {
						memcpy(pixels + ((zest_size)y * width + x) * channels, texels + (texel_y * 4 + texel_x) * channels, channels);
					}
				}
			}
		}
	}
}

//Each layer of the KTX2 is an animation frame. When the device can sample the blocks the stored mip chain is
//uploaded as it is, otherwise the largest level is decoded and stacked into a one column sheet.
static const char *zest__tfx_load_block_shape(zest_device device, const zest__tfx_shape_layout_t *layout, const unsigned char *data, zest_size size, zest_tfx_shape_image_t *record) {
	zest__tfx_ktx2_t ktx2;
	const char *error = zest__tfx_parse_ktx2(data, size, layout->block_format, &ktx2);
	if (error) {
		return error;
	}
	record->frames = ktx2.layers;
	record->frame_width = ktx2.width;
	record->frame_height = ktx2.height;
	record->swizzle = layout->swizzle;

	if (zest_IsImageFormatSupported(device, layout->block_format, ZEST__TFX_BLOCK_IMAGE_FLAGS)) {
		zest_size total_size = 0;
		for (zest_uint level = 0; level != ktx2.level_count; ++level) {
			total_size += ktx2.level_sizes[level];
		}
		zest_byte *levels = (zest_byte *)malloc(total_size);
		if (!levels) {
			return "there was not enough memory to load it";
		}
		zest_byte *level_bytes = levels;
		for (zest_uint level = 0; level != ktx2.level_count; ++level) {
			if (!zest__tfx_inflate_ktx2_level(&ktx2, level, level_bytes)) {
				free(levels);
				return "a zstd supercompressed mip level could not be inflated";
			}
			level_bytes += ktx2.level_sizes[level];
		}
		record->format = layout->block_format;
		record->stored_mip_levels = ktx2.level_count;
		record->pixels.data = levels;
		record->pixels.meta.width = ktx2.width;
		record->pixels.meta.height = ktx2.height;
		record->pixels.meta.size = total_size;
		record->pixels.meta.format = layout->block_format;
		return NULL;
	}

	zest_byte *largest_level = (zest_byte *)malloc(ktx2.level_sizes[0]);
	zest_size frame_size = (zest_size)ktx2.width * ktx2.height * layout->channels;
	zest_byte *pixels = (zest_byte *)malloc(frame_size * ktx2.layers);
	if (!largest_level || !pixels) {
		free(largest_level);
		free(pixels);
		return "there was not enough memory to decode it";
	}
	if (!zest__tfx_inflate_ktx2_level(&ktx2, 0, largest_level)) {
		free(largest_level);
		free(pixels);
		return "a zstd supercompressed mip level could not be inflated";
	}
	zest_size layer_size = ktx2.level_sizes[0] / ktx2.layers;
	for (zest_uint layer = 0; layer != ktx2.layers; ++layer) {
		zest__tfx_decode_blocks(layout, largest_level + layer * layer_size, ktx2.width, ktx2.height, pixels + layer * frame_size);
	}
	free(largest_level);
	record->format = layout->pixel_format;
	record->stored_mip_levels = 0;
	record->pixels.data = pixels;
	record->pixels.meta.width = ktx2.width;
	record->pixels.meta.height = ktx2.height * ktx2.layers;
	record->pixels.meta.channels = layout->channels;
	record->pixels.meta.bytes_per_pixel = layout->channels;
	record->pixels.meta.stride = ktx2.width * layout->channels;
	record->pixels.meta.size = frame_size * ktx2.layers;
	record->pixels.meta.format = layout->pixel_format;
	return NULL;
}

//The frames of an animated shape are laid out in a grid on one sheet
static const char *zest__tfx_load_pixel_shape(const zest__tfx_shape_layout_t *layout, tfx_image_format format, tfx_image_data_t *image_data, const unsigned char *data, int size, zest_tfx_shape_image_t *record) {
	int width = 0;
	int height = 0;
	int file_channels = 0;
	zest_byte *pixels = NULL;
	if (format != tfx_image_format_rgba8_raw) {
		pixels = (zest_byte *)stbi_load_from_memory(data, size, &width, &height, &file_channels, (int)layout->channels);
	}
	zest_size sheet_size;
	if (pixels) {
		sheet_size = (zest_size)width * height * layout->channels;
	} else {
		//Libraries saved before the format was recorded can hold raw sheets, so only they get the raw reading
		if (format != tfx_image_format_rgba8_raw && format != tfx_image_format_unknown) {
			return "its png could not be decoded";
		}
		width = tfx_GetImageWidth(image_data);
		height = tfx_GetImageHeight(image_data);
		sheet_size = (zest_size)width * height * 4;
		pixels = (zest_byte *)malloc(sheet_size);
		if (!pixels) {
			return "there was not enough memory to load it";
		}
		//tfx frees its buffer as soon as the loader returns, so it is copied. The source is usually shorter than
		//the sheet, so the tail stays blank.
		zest_size available = (zest_size)size < sheet_size ? (zest_size)size : sheet_size;
		memset(pixels, 0, sheet_size);
		memcpy(pixels, data, available);
	}

	record->format = layout->pixel_format;
	record->swizzle = layout->swizzle;
	record->stored_mip_levels = 0;
	record->pixels.data = pixels;
	record->pixels.meta.width = width;
	record->pixels.meta.height = height;
	record->pixels.meta.channels = layout->channels;
	record->pixels.meta.bytes_per_pixel = layout->channels;
	record->pixels.meta.stride = width * layout->channels;
	record->pixels.meta.size = sheet_size;
	record->pixels.meta.format = layout->pixel_format;
	record->pixels.is_imported = ZEST_TRUE;

	zest_uint frames = (zest_uint)tfx_GetImageFrameCount(image_data);
	record->frames = frames > 1 ? frames : 1;
	record->frame_width = record->frames > 1 ? (zest_uint)tfx_GetImageWidth(image_data) : (zest_uint)width;
	record->frame_height = record->frames > 1 ? (zest_uint)tfx_GetImageHeight(image_data) : (zest_uint)height;
	return NULL;
}

//tfx keeps the pointer the shape loader hands it for as long as the shape is in the library, so a record
//is allocated on its own and never moves. Only the pointer list grows, which it is free to do.
ZEST_PRIVATE zest_tfx_shape_image_t *zest__tfx_add_shape_record(tfx_library_render_resources_t *resources) {
	if (resources->shape_image_count == resources->shape_image_capacity) {
		zest_uint capacity = resources->shape_image_capacity ? resources->shape_image_capacity * 2 : 64;
		zest_tfx_shape_image_t **images = (zest_tfx_shape_image_t **)ZEST_UTILITIES_MALLOC(sizeof(zest_tfx_shape_image_t *) * capacity);
		if (!images) {
			return NULL;
		}
		if (resources->shape_images) {
			memcpy(images, resources->shape_images, sizeof(zest_tfx_shape_image_t *) * resources->shape_image_count);
			ZEST_UTILITIES_FREE(resources->shape_images);
		}
		resources->shape_images = images;
		resources->shape_image_capacity = capacity;
	}
	zest_tfx_shape_image_t *record = (zest_tfx_shape_image_t *)ZEST_UTILITIES_MALLOC(sizeof(zest_tfx_shape_image_t));
	if (!record) {
		return NULL;
	}
	*record = ZEST__ZERO_INIT(zest_tfx_shape_image_t);
	resources->shape_images[resources->shape_image_count++] = record;
	return record;
}

//stb_image and the shape loader both allocate the pixels with malloc
ZEST_PRIVATE void zest__tfx_free_shape_pixels(zest_tfx_shape_image_t *record) {
	free(record->pixels.data);
	record->pixels.data = NULL;
}

ZEST_PRIVATE void zest__tfx_free_shape_record(tfx_library_render_resources_t *resources, zest_uint index) {
	zest_tfx_shape_image_t *record = resources->shape_images[index];
	//zest_FreeImage releases the image's bindless index as part of the deferred cleanup, which is the right
	//time: the descriptor can still be read by frames in flight until then. Releasing it here as well would
	//hand the index out again while the cleanup still means to free it.
	if (record->image.value) {
		zest_FreeImage(record->image);
	}
	zest__tfx_free_shape_pixels(record);
	ZEST_UTILITIES_FREE(record);
	//The shapes still waiting to be uploaded are the tail of this array, and a refresh removes shapes after
	//the loader has already appended them, so the order has to hold: swapping the last record into the hole
	//would carry a pending shape out of that tail and it would never be uploaded.
	if (index >= resources->shape_image_count - resources->pending_count) {
		resources->pending_count--;
	}
	resources->shape_image_count--;
	for (zest_uint i = index; i != resources->shape_image_count; ++i) {
		resources->shape_images[i] = resources->shape_images[i + 1];
	}
}

//Before you load an effects file, you will need to define a ShapeLoader function that passes the following parameters:
//const char* filename			- this will be the filename of the image being loaded from the library. You don't have to do anything with this if you don't need to.
//ImageData	&image_data			- A struct containing data about the image. You will have to set image_data.ptr to point to the texture in your renderer for later use in the Render function that you will create to render the particles
//void *raw_image_data			- The raw data of the image which you can use to load the image into graphics memory
//int image_memory_size			- The size in bytes of the raw_image_data
//void *custom_data				- This allows you to pass through an object you can use to access whatever is necessary to load the image into graphics memory, depending on the renderer that you're using
void zest_tfx_ShapeLoader(const char *filename, tfx_image_data_t *image_data, void *raw_image_data, int image_memory_size, void *custom_data) {
	//Cast your custom data, this can be anything you want
	tfx_library_render_resources_t *resources = (tfx_library_render_resources_t *)custom_data;

	zest_tfx_shape_image_t *record = zest__tfx_add_shape_record(resources);
	if (!record) {
		return;
	}

	//The format the shape was saved in decides how it is decoded and what the image is created with
	tfx_image_format format = tfx_GetImageFormat(image_data);
	zest__tfx_shape_layout_t layout;
	const char *error = NULL;
	if (!zest__tfx_shape_layout(format, &layout)) {
		error = "its image format is not one this loader recognises";
	} else if (layout.block_format != zest_format_undefined) {
		error = zest__tfx_load_block_shape(resources->device, &layout, (const unsigned char *)raw_image_data, (zest_size)image_memory_size, record);
	} else {
		error = zest__tfx_load_pixel_shape(&layout, format, image_data, (const unsigned char *)raw_image_data, image_memory_size, record);
	}
	if (error) {
		zest__tfx_free_shape_pixels(record);
		record->unreadable = ZEST_TRUE;
		record->frames = 1;
		record->frame_width = (zest_uint)ZEST__MAX(tfx_GetImageWidth(image_data), 1);
		record->frame_height = (zest_uint)ZEST__MAX(tfx_GetImageHeight(image_data), 1);
		resources->unreadable_shapes++;
		ZEST_PRINT("Skipped particle shape %s (image format %i) because %s. It will render as the default image.", filename ? filename : "", (int)format, error);
	}
	resources->pending_count++;

	//Important step: the record is what the uv lookup is handed for this shape, which is how a particle
	//finds the image made for it below.
	tfx_SetImagePointer(image_data, record);
}

static zest_size zest__tfx_shape_upload_size(zest_tfx_shape_image_t *record) {
	if (record->unreadable) {
		return 0;
	}
	zest_size size = record->stored_mip_levels ? record->pixels.meta.size
		: (zest_size)record->frames * record->frame_width * record->frame_height * record->pixels.meta.bytes_per_pixel;
	return (size + ZEST__TFX_STAGING_ALIGNMENT - 1) & ~(zest_size)(ZEST__TFX_STAGING_ALIGNMENT - 1);
}

//Create one layered image per particle shape. The frames of an animated shape become the array layers of
//that one image, so an animation costs a single descriptor like any other shape. Only the shapes the loader
//has just queued are processed, so a refresh only uploads what it added.
static void zest__tfx_upload_pending_shapes(zest_context context, tfx_library_render_resources_t *resources) {
	if (!resources->pending_count) {
		return;
	}
	zest_device device = zest_GetContextDevice(context);
	zest_uint first_pending = resources->shape_image_count - resources->pending_count;

	zest_size total_size = 0;
	for (zest_uint i = first_pending; i != resources->shape_image_count; ++i) {
		total_size += zest__tfx_shape_upload_size(resources->shape_images[i]);
	}

	if (total_size) {
		zest_buffer staging_buffer = zest_CreateDedicatedStagingBuffer(device, total_size, 0);
		if (!staging_buffer) {
			return;
		}

		//A shape's frames go into the staging buffer back to back so that they upload as one region. A shape
		//with a single frame is the same copy with nothing to step over. Stored mip levels are already packed.
		zest_byte *staging_data = (zest_byte *)zest_BufferData(staging_buffer);
		//A sheet whose grid does not hold every frame it claims leaves the frames past the end of the grid
		//uncopied below, so the buffer starts blank rather than uploading whatever was in host memory.
		memset(staging_data, 0, total_size);
		zest_size staging_offset = 0;
		for (zest_uint i = first_pending; i != resources->shape_image_count; ++i) {
			zest_tfx_shape_image_t *record = resources->shape_images[i];
			if (record->unreadable) {
				continue;
			}
			zest_size shape_offset = staging_offset;
			staging_offset += zest__tfx_shape_upload_size(record);
			if (record->stored_mip_levels) {
				memcpy(staging_data + shape_offset, record->pixels.data, record->pixels.meta.size);
				continue;
			}
			zest_uint bytes_per_pixel = record->pixels.meta.bytes_per_pixel;
			zest_uint columns = record->frame_width ? record->pixels.meta.width / record->frame_width : 1;
			if (!columns) {
				columns = 1;
			}
			for (zest_uint f = 0; f != record->frames; ++f) {
				zest_bitmap_t frame = ZEST__ZERO_INIT(zest_bitmap_t);
				frame.meta = record->pixels.meta;
				frame.meta.width = record->frame_width;
				frame.meta.height = record->frame_height;
				frame.meta.stride = record->frame_width * bytes_per_pixel;
				frame.meta.size = (zest_size)frame.meta.stride * record->frame_height;
				frame.data = staging_data + shape_offset;
				frame.is_imported = ZEST_TRUE;
				zest_CopyBitmap(&record->pixels, (f % columns) * record->frame_width, (f / columns) * record->frame_height,
					record->frame_width, record->frame_height, &frame, 0, 0);
				shape_offset += frame.meta.size;
			}
		}

		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		zest_size buffer_offset = 0;
		for (zest_uint i = first_pending; i != resources->shape_image_count; ++i) {
			zest_tfx_shape_image_t *record = resources->shape_images[i];
			if (record->unreadable) {
				continue;
			}
			zest_size shape_size = zest__tfx_shape_upload_size(record);

			zest_image_info_t image_info = zest_CreateImageInfo(record->frame_width, record->frame_height);
			image_info.format = record->format;
			image_info.layer_count = record->frames;
			image_info.swizzle = record->swizzle;
			if (record->stored_mip_levels) {
				image_info.mip_levels = record->stored_mip_levels;
				image_info.flags = ZEST__TFX_BLOCK_IMAGE_FLAGS;
			} else {
				image_info.flags = ZEST__TFX_PIXEL_IMAGE_FLAGS;
			}
			record->image = zest_CreateImage(device, &image_info);
			if (!record->image.value) {
				//Nothing can be done for the shape now, tfx already has the record. Its bindless index is still
				//the zero it was created with, which addresses the device's default image.
				ZEST_PRINT("Unable to create an image for a particle shape. It will render as the default image.");
				buffer_offset += shape_size;
				continue;
			}
			zest_image image = zest_GetImage(record->image);
			zest_uint mip_levels = zest_ImageInfo(image)->mip_levels;

			zest_imm_TransitionImage(queue, image, zest_resource_state_copy_dst, 0, mip_levels, 0, record->frames);
			if (record->stored_mip_levels) {
				zest_buffer_image_copy_t copy_regions[ZEST__TFX_KTX2_MAX_LEVELS];
				zest_uint block_bytes = zest__tfx_block_bytes(record->format);
				zest_size level_offset = buffer_offset;
				for (zest_uint level = 0; level != record->stored_mip_levels; ++level) {
					zest_buffer_image_copy_t copy_region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
					copy_region.buffer_offset = level_offset;
					copy_region.image_aspect = zest_image_aspect_color_bit;
					copy_region.mip_level = level;
					copy_region.layer_count = record->frames;
					copy_region.image_extent.width = ZEST__MAX(record->frame_width >> level, 1u);
					copy_region.image_extent.height = ZEST__MAX(record->frame_height >> level, 1u);
					copy_region.image_extent.depth = 1;
					copy_regions[level] = copy_region;
					level_offset += zest__tfx_block_level_size(record->frame_width, record->frame_height, level, record->frames, block_bytes);
				}
				zest_imm_CopyBufferRegionsToImage(queue, copy_regions, record->stored_mip_levels, staging_buffer, image);
				zest_imm_TransitionImage(queue, image, zest_resource_state_shader_read, 0, mip_levels, 0, record->frames);
			} else {
				zest_buffer_image_copy_t copy_region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
				copy_region.buffer_offset = buffer_offset;
				copy_region.image_aspect = zest_image_aspect_color_bit;
				copy_region.layer_count = record->frames;
				copy_region.image_extent.width = record->frame_width;
				copy_region.image_extent.height = record->frame_height;
				copy_region.image_extent.depth = 1;
				zest_imm_CopyBufferRegionsToImage(queue, &copy_region, 1, staging_buffer, image);
				if (mip_levels > 1) {
					zest_imm_GenerateMipMaps(queue, image);
				} else {
					zest_imm_TransitionImage(queue, image, zest_resource_state_shader_read, 0, mip_levels, 0, record->frames);
				}
			}
			buffer_offset += shape_size;
		}
		zest_imm_EndCommandBuffer(queue);
		zest_FreeBufferNow(staging_buffer);
	}

	//Descriptor indexes can only be acquired once an image is in a layout valid for sampling, so the shapes
	//get walked a second time now that the uploads have run. The sheets have done their job by this point.
	for (zest_uint i = first_pending; i != resources->shape_image_count; ++i) {
		zest_tfx_shape_image_t *record = resources->shape_images[i];
		if (record->image.value) {
			record->bindless_index = zest_AcquireSampledImageIndex(device, zest_GetImage(record->image), zest_texture_array_binding);
		}
		zest__tfx_free_shape_pixels(record);
	}
	resources->pending_count = 0;
}

//tfx calls this once for each shape a refresh took out of the library, handing back the record the shape
//loader set with tfx_SetImagePointer. Freeing here rather than after the refresh returns is safe because
//zest_FreeImage defers the release until the frames that can still be reading the descriptor have finished,
//and a refresh is only ever run between frames.
void zest_tfx_ShapeRemover(tfx_image_data_t *image_data, void *custom_data) {
	tfx_library_render_resources_t *resources = (tfx_library_render_resources_t *)custom_data;
	zest_tfx_shape_image_t *record = (zest_tfx_shape_image_t *)tfx_GetImagePointer(image_data);
	if (!record) {
		//The shape loader never took this one, so there is nothing of ours to free
		return;
	}
	//The records are held by pointer in an unordered array, so the one to drop has to be found by identity
	for (zest_uint i = 0; i != resources->shape_image_count; ++i) {
		if (resources->shape_images[i] == record) {
			zest__tfx_free_shape_record(resources, i);
			resources->images_removed++;
			return;
		}
	}
}

//Basic function for updating the uniform buffer
void zest_tfx_UpdateUniformBuffer(zest_context context, tfx_library_render_resources_t *resources) {
	zest_uniform_buffer buffer = zest_GetUniformBuffer(resources->uniform_buffer);
	tfx_uniform_buffer_data_t *uniform_buffer = (tfx_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	tfx_uniform_buffer_data_t data;
	data.view = zest_LookAt(resources->camera.position, zest_AddVec3(resources->camera.position, resources->camera.front), resources->camera.up);
	data.proj = zest_Perspective(resources->camera.fov, zest_ScreenWidthf(context) / zest_ScreenHeightf(context), 0.1f, 10000.f);
	data.proj.v[1].y *= -1.f;
	data.screen_size.x = zest_ScreenWidthf(context);
	data.screen_size.y = zest_ScreenHeightf(context);
	data.millisecs = 0;
	data.timer_lerp = (float)zest_TimerLerp(&resources->timer);
	data.update_time = (float)zest_TimerUpdateTime(&resources->timer);
	zest_CalculateFrustumPlanes(&data.view, &data.proj, resources->planes);
	*uniform_buffer = data;
}

void zest_tfx_GetUV(void *ptr, tfx_gpu_image_data_t *image_data, int offset) {
	zest_tfx_shape_image_t *shape = (zest_tfx_shape_image_t *)ptr;
	//Each shape owns its image so the rect is the whole shape and the descriptor index travels with the
	//particle instead of arriving in the push constants. The frame is the array layer of that image.
	//Inset by half a texel the way the atlas packer did, so the outermost texel centres land on the quad
	//edges. Running the full 0..1 samples past them into the clamped edge and hardens the falloff.
	float half_texel_x = shape->frame_width > 0 ? 0.5f / (float)shape->frame_width : 0.f;
	float half_texel_y = shape->frame_height > 0 ? 0.5f / (float)shape->frame_height : 0.f;
	image_data->uv.x = half_texel_x;
	image_data->uv.y = half_texel_y;
	image_data->uv.z = 1.f - half_texel_x;
	image_data->uv.w = 1.f - half_texel_y;
	image_data->uv_packed = zest_Pack16bit4SNorm(image_data->uv.x, image_data->uv.y, image_data->uv.z, image_data->uv.w);
	image_data->texture_array_index = (shape->bindless_index << 16) | ((zest_uint)offset & 0xFFFF);
}

tfx_library zest_tfx_LoadLibrary(zest_context context, tfx_library_render_resources_t *resources, const char *library_path) {
	resources->device = zest_GetContextDevice(context);
	tfx_library library = tfx_LoadEffectLibrary(library_path, zest_tfx_ShapeLoader, zest_tfx_GetUV, resources);
	zest__tfx_upload_pending_shapes(context, resources);
	return library;
}

//The library hands out colour ramp indexes by position in its bitmap list, and a refresh rebuilds that
//list from scratch, so the texture has to be built again from the current bitmaps whenever it changes.
static void zest__tfx_build_color_ramps_texture(zest_context context, tfx_library_render_resources_t *resources, tfx_library library) {
	zest_device device = zest_GetContextDevice(context);
	tfxU32 bitmap_count = tfx_GetColorRampBitmapCount(library);
	resources->color_ramps_collection = zest_CreateImageAtlasCollection(zest_format_r16g16b16a16_sfloat, bitmap_count);
	for (tfxU32 i = 0; i != bitmap_count; ++i) {
		tfx_bitmap_t *bitmap = tfx_GetColorRampBitmap(library, i);
		zest_AddImageAtlasPixels(&resources->color_ramps_collection, tfx_GetBitmapData(bitmap), tfx_GetBitmapSize(bitmap), tfx_GetBitmapWidth(bitmap), tfx_GetBitmapHeight(bitmap), zest_format_r16g16b16a16_sfloat);
	}
	resources->color_ramps_texture = zest_CreateImageAtlas(context, &resources->color_ramps_collection, 256, 256, zest_image_preset_texture);
	zest_image color_ramps_image = zest_GetImage(resources->color_ramps_texture);
	resources->color_ramps_index = zest_AcquireSampledImageIndex(device, color_ramps_image, zest_texture_array_binding);
}

void zest_tfx_FinaliseLibrary(zest_context context, tfx_library_render_resources_t *resources, tfx_library library) {
	zest__tfx_build_color_ramps_texture(context, resources, library);

	tfx_UpdateLibraryGPUImageData(library);
	zest_tfx_UpdateTimelineFXImageData(context, resources, tfx_GetLibraryGPUShapes(library));
	zest_tfx_UpdateTimelineFXParticleProperties(context, resources, library);
}

tfxErrorFlags zest_tfx_LoadSpriteData(zest_context context, tfx_library_render_resources_t *resources, const char *path, tfx_animation_manager animation_manager) {
	resources->device = zest_GetContextDevice(context);

	tfxErrorFlags result = tfx_LoadSpriteData(path, animation_manager, zest_tfx_ShapeLoader, resources);
	if (result != 0) return result;

	zest__tfx_upload_pending_shapes(context, resources);
	return result;
}

void zest_tfx_FinaliseSpriteData(zest_context context, tfx_library_render_resources_t *resources, tfx_animation_manager animation_manager, tfx_gpu_shapes gpu_image_data) {
	zest_device device = zest_GetContextDevice(context);

	tfxU32 bitmap_count = tfx_GetAnimationColorRampBitmapCount(animation_manager);
	resources->color_ramps_collection = zest_CreateImageAtlasCollection(zest_format_r16g16b16a16_sfloat, bitmap_count);
	for (tfxU32 i = 0; i != bitmap_count; ++i) {
		tfx_bitmap_t *bitmap = tfx_GetAnimationColorRampBitmap(animation_manager, i);
		zest_AddImageAtlasPixels(&resources->color_ramps_collection, tfx_GetBitmapData(bitmap), tfx_GetBitmapSize(bitmap), tfx_GetBitmapWidth(bitmap), tfx_GetBitmapHeight(bitmap), zest_format_r16g16b16a16_sfloat);
	}
	resources->color_ramps_texture = zest_CreateImageAtlas(context, &resources->color_ramps_collection, 256, 256, zest_image_preset_texture);
	zest_image color_ramps_image = zest_GetImage(resources->color_ramps_texture);
	resources->color_ramps_index = zest_AcquireSampledImageIndex(device, color_ramps_image, zest_texture_array_binding);

	tfx_BuildAnimationManagerGPUShapeData(animation_manager, gpu_image_data, zest_tfx_GetUV);
	zest_tfx_UpdateTimelineFXImageData(context, resources, gpu_image_data);

	//Create GPU storage buffers for sprite data and emitter properties
	zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	resources->sprite_data_buffer = zest_CreateBuffer(device, tfx_GetSpriteDataSizeInBytes(animation_manager), &storage_buffer_info);
	resources->emitter_properties_buffer = zest_CreateBuffer(device, tfx_GetAnimationEmitterPropertySizeInBytes(animation_manager), &storage_buffer_info);

	//Upload sprite data and emitter properties to the GPU
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
	zest_buffer sprite_staging = zest_CreateDedicatedStagingBuffer(device, tfx_GetSpriteDataSizeInBytes(animation_manager), tfx_GetSpriteDataBufferPointer(animation_manager));
	zest_imm_CopyBuffer(queue, sprite_staging, resources->sprite_data_buffer, tfx_GetSpriteDataSizeInBytes(animation_manager));
	zest_buffer emitter_staging = zest_CreateDedicatedStagingBuffer(device, tfx_GetAnimationEmitterPropertySizeInBytes(animation_manager), tfx_GetAnimationEmitterPropertiesBufferPointer(animation_manager));
	zest_imm_CopyBuffer(queue, emitter_staging, resources->emitter_properties_buffer, tfx_GetAnimationEmitterPropertySizeInBytes(animation_manager));
	zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(sprite_staging);
	zest_FreeBufferNow(emitter_staging);

	resources->sprite_data_index = zest_AcquireStorageBufferIndex(device, resources->sprite_data_buffer);
	resources->emitter_properties_index = zest_AcquireStorageBufferIndex(device, resources->emitter_properties_buffer);
}

void zest_tfx_InitTimelineFXRenderResources(zest_context context, tfx_library_render_resources_t *resources, zest_shader_handle vert_shader, zest_shader_handle frag_shader, zest_shader_handle ribbon_vert, zest_shader_handle ribbon_frag, zest_shader_handle ribbon_comp) {
	zest_device device = zest_GetContextDevice(context);
	resources->device = device;
	resources->uniform_buffer = zest_CreateUniformBuffer(context, "tfx uniform", sizeof(tfx_uniform_buffer_data_t));

	resources->timer = zest_CreateTimer(60);

	resources->camera = zest_CreateCamera();
	zest_CameraSetFoV(&resources->camera, 60.f);
	//float cam_pos[3] = {0.f, 3.f, 7.5};
	//zest_CameraPosition(&resources->camera, cam_pos);
	zest_CameraUpdateFront(&resources->camera);

	zest_tfx_UpdateUniformBuffer(context, resources);

	//Compile the shaders we will use to render the particles and ribbons
	resources->particles.frag_shader = frag_shader;
	resources->particles.vert_shader = vert_shader;
	resources->ribbon_rendering.frag_shader = ribbon_frag;
	resources->ribbon_rendering.vert_shader = ribbon_vert;
	resources->ribbon_rendering.comp_shader = ribbon_comp;

	//To render the particles we setup a pipeline with the vertex attributes and shaders to render the particles.
	//First create a descriptor set layout, we need 2 samplers, one to sample the particle texture and another to sample the color ramps
	//We also need 2 storage buffers, one to access the image data in the vertex shader and the other to access the previous frame particles
	//so that they can be interpolated in between updates

	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(resources->uniform_buffer);

	resources->particles.pipeline = zest_CreatePipelineTemplate(device, "Timelinefx pipeline");
	//Set up the vertex attributes that will take in all of the billboard data stored in tfx_instance_t objects
	zest_AddVertexInputBindingDescription(resources->particles.pipeline, 0, sizeof(tfx_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 0, zest_format_r32g32b32a32_sfloat, offsetof(tfx_instance_t, position));	            // Location 0: Postion and stretch in w
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 1, zest_format_r16g16b16a16_snorm, offsetof(tfx_instance_t, quaternion));	                // Location 1: Quaternion (packed)
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 2, zest_format_r16g16_sscaled, offsetof(tfx_instance_t, size));		    // Location 3: Size and handle of the sprite
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 3, zest_format_r8g8b8_snorm, offsetof(tfx_instance_t, alignment));					    // Location 2: Alignment
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 4, zest_format_r16g16_sscaled, offsetof(tfx_instance_t, intensity_gradient_map));       // Location 4: 2 intensities for each color
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 5, zest_format_r8g8b8_unorm, offsetof(tfx_instance_t, curved_alpha_life));          	// Location 5: Sharpness and mix lerp value
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 6, zest_format_r32_uint, offsetof(tfx_instance_t, indexes));							// Location 6: texture indexes to sample the correct image and color ramp
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 7, zest_format_r32_uint, offsetof(tfx_instance_t, captured_index));   				    // Location 7: index of the sprite in the previous buffer when double buffering
	//Set the shaders to our custom timelinefx shaders
	zest_SetPipelineShaders(resources->particles.pipeline, resources->particles.vert_shader, resources->particles.frag_shader);
	zest_SetPipelineDepthTest(resources->particles.pipeline, true, false);
	zest_SetPipelineBlend(resources->particles.pipeline, zest_PreMultiplyBlendState());

	//We want to be able to manually change the current frame in flight in the layer that we use to draw all the billboards.
	//This means that we are able to only change the current frame in flight if we actually updated the particle manager in the current
	//frame allowing us to dictate when to upload the instance buffer to the gpu as there's no need to do it every frame, only when 
	//the particle manager is actually updated.
	resources->layer = zest_CreateFIFInstanceLayer(context, "TimelineFX Layer", sizeof(tfx_instance_t), 50000);
	zest_AcquireInstanceLayerBufferIndex(device, zest_GetLayer(resources->layer));

	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	resources->sampler = zest_CreateSampler(device, &sampler_info);
	zest_sampler sampler = zest_GetSampler(resources->sampler);
	resources->sampler_index = zest_AcquireSamplerIndex(device, sampler);

	//Create a buffer to store the image data on the gpu.
	zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	resources->image_data = zest_CreateBuffer(device, sizeof(tfx_gpu_image_data_t) * 1000, &storage_buffer_info);
	resources->image_data_index = zest_AcquireStorageBufferIndex(device, resources->image_data);

	resources->particle_properties = zest_CreateBuffer(device, sizeof(tfx_gpu_particle_properties_t) * 1000, &storage_buffer_info);
	resources->particle_properties_index = zest_AcquireStorageBufferIndex(device, resources->particle_properties);

	//Don't actually need this, can remove?
	resources->timeline = zest_CreateExecutionTimeline(device);

	//Configure Ribbons

    //Set up the compute shader
    //Create a new empty compute shader in the renderer

    resources->ribbon_rendering.ribbon_compute = zest_CreateCompute(device, "ribbon_emitters", resources->ribbon_rendering.comp_shader);

    zest_pipeline_template ribbon_pipeline  = zest_CreatePipelineTemplate(device, "ribbon render pipeline");
    //Set up the vertex attributes that will take in all of the billboard data stored in tfx_instance_t objects
    zest_AddVertexInputBindingDescription(ribbon_pipeline, 0, sizeof(tfx_ribbon_vertex_t), zest_input_rate_vertex);
	zest_AddVertexAttribute(ribbon_pipeline, 0, 0, zest_format_r32g32b32_sfloat, offsetof(tfx_ribbon_vertex_t, position));
    zest_AddVertexAttribute(ribbon_pipeline, 0, 1, zest_format_r32_uint, offsetof(tfx_ribbon_vertex_t, segment_index));
    zest_AddVertexAttribute(ribbon_pipeline, 0, 2, zest_format_r32g32_sfloat, offsetof(tfx_ribbon_vertex_t, uv_offset_scale));
    zest_AddVertexAttribute(ribbon_pipeline, 0, 3, zest_format_r32_uint, offsetof(tfx_ribbon_vertex_t, ribbon_index));
    //No attribute for tfx_ribbon_vertex_t::clipped - the vertex shader stopped reading it when clipping
    //moved to the graph lookups. The binding stride stays the full struct size, the compute shader still writes it.
    //Set the shaders to our custom timelinefx shaders
    zest_SetPipelineVertShader(ribbon_pipeline, resources->ribbon_rendering.vert_shader);
    zest_SetPipelineFragShader(ribbon_pipeline, resources->ribbon_rendering.frag_shader);
	zest_SetPipelineBlend(ribbon_pipeline, zest_PreMultiplyBlendState());
	zest_SetPipelineDepthTest(ribbon_pipeline, true, false);
	resources->ribbon_rendering.pipeline = ribbon_pipeline;

}

void zest_tfx_CreateRibbonRenderer(zest_context context, tfx_ribbon_renderer_t *renderer, tfx_library_render_resources_t *resources, tfx_global_library_buffers_t *global_buffers) {
	memset(renderer, 0, sizeof(tfx_ribbon_renderer_t));
	renderer->context = context;
	renderer->render_resources = resources;
	renderer->global_buffers = global_buffers;
}

void zest_tfx_FreeRibbonRenderer(tfx_ribbon_renderer_t *renderer) {
	zest_ForEachFrameInFlight(fif) {
		zest_FreeBuffer(renderer->segment_staging_buffer[fif]);
		zest_FreeBuffer(renderer->ribbon_staging_buffer[fif]);
		zest_FreeBuffer(renderer->emitter_staging_buffer[fif]);
	}
	memset(renderer, 0, sizeof(tfx_ribbon_renderer_t));
}

//Staging only has to hold the stages that exist, so grow it when stages are created later on
static void zest__tfx_ensure_staging_size(zest_device device, zest_buffer *buffer, zest_size required) {
	if (*buffer && zest_BufferSize(*buffer) >= required) return;
	zest_FreeBuffer(*buffer);
	*buffer = zest_CreateStagingBuffer(device, required, 0);
}

void zest_tfx_BeginRibbons(tfx_ribbon_renderer_t *renderer) {
	zest_device device = zest_GetContextDevice(renderer->context);
	zest_uint fif = zest_CurrentFIF(renderer->context);
	zest__tfx_ensure_staging_size(device, &renderer->segment_staging_buffer[fif], ZEST__MAX(tfx_GetTotalSegmentBufferMaxSizeInBytes(), 1));
	zest__tfx_ensure_staging_size(device, &renderer->ribbon_staging_buffer[fif], ZEST__MAX(tfx_GetTotalRibbonBufferMaxSizeInBytes(), 1));
	zest__tfx_ensure_staging_size(device, &renderer->emitter_staging_buffer[fif], ZEST__MAX(tfx_GetTotalEmitterBufferMaxSizeInBytes(), 1));
	renderer->batch = tfx_BeginRibbonBatch(
		zest_BufferData(renderer->segment_staging_buffer[fif]), zest_BufferSize(renderer->segment_staging_buffer[fif]),
		zest_BufferData(renderer->ribbon_staging_buffer[fif]), zest_BufferSize(renderer->ribbon_staging_buffer[fif]),
		zest_BufferData(renderer->emitter_staging_buffer[fif]), zest_BufferSize(renderer->emitter_staging_buffer[fif]));
}

zest_bool zest_tfx_AddRibbonStage(tfx_ribbon_renderer_t *renderer, tfx_stage stage) {
	zest_uint stage_count = renderer->batch.stage_count;
	ZEST_ASSERT(renderer->batch.segments_dst);	//Call zest_tfx_BeginRibbons first
	ZEST_ASSERT(stage_count < ZEST_TFX_MAX_RIBBON_STAGES);	//Define ZEST_TFX_MAX_RIBBON_STAGES higher if you need more
	for (zest_uint index = 0; index != stage_count; ++index) {
		ZEST_ASSERT(renderer->stages[index] != stage);	//A stage can only be added once per frame
	}
	if (!tfx_AddStageToRibbonBatch(&renderer->batch, stage, &renderer->stage_offsets[stage_count])) {
		return ZEST_FALSE;
	}
	renderer->stages[stage_count] = stage;
	return ZEST_TRUE;
}

zest_bool zest_tfx_HasRibbonsToDraw(tfx_ribbon_renderer_t *renderer) {
	return renderer->batch.totals.index_offset > 0;
}

static zest_buffer zest__tfx_ribbon_segment_size(zest_context context, zest_resource_node node) {
	tfx_ribbon_renderer_t *renderer = (tfx_ribbon_renderer_t *)zest_GetResourceUserData(node);
	zest_SetResourceBufferSize(node, tfx_GetRibbonBatchSizes(&renderer->batch, 0).segment_buffer_size_in_bytes);
	return NULL;
}

static zest_buffer zest__tfx_ribbon_instance_size(zest_context context, zest_resource_node node) {
	tfx_ribbon_renderer_t *renderer = (tfx_ribbon_renderer_t *)zest_GetResourceUserData(node);
	zest_SetResourceBufferSize(node, tfx_GetRibbonBatchSizes(&renderer->batch, 0).ribbon_buffer_size_in_bytes);
	return NULL;
}

static zest_buffer zest__tfx_ribbon_emitter_size(zest_context context, zest_resource_node node) {
	tfx_ribbon_renderer_t *renderer = (tfx_ribbon_renderer_t *)zest_GetResourceUserData(node);
	zest_SetResourceBufferSize(node, tfx_GetRibbonBatchSizes(&renderer->batch, 0).emitter_buffer_size_in_bytes);
	return NULL;
}

static zest_buffer zest__tfx_ribbon_vertex_size(zest_context context, zest_resource_node node) {
	tfx_ribbon_renderer_t *renderer = (tfx_ribbon_renderer_t *)zest_GetResourceUserData(node);
	zest_SetResourceBufferSize(node, tfx_GetRibbonBatchSizes(&renderer->batch, 0).vertex_buffer_size_in_bytes);
	return NULL;
}

static zest_buffer zest__tfx_ribbon_index_size(zest_context context, zest_resource_node node) {
	tfx_ribbon_renderer_t *renderer = (tfx_ribbon_renderer_t *)zest_GetResourceUserData(node);
	zest_SetResourceBufferSize(node, tfx_GetRibbonBatchSizes(&renderer->batch, 0).index_buffer_size_in_bytes);
	return NULL;
}

void zest_tfx_CreateGlobalBuffers(zest_context context, tfx_global_library_buffers_t *buffers) {
	zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	zest_device device = zest_GetContextDevice(context);
    zest_ForEachFrameInFlight(i) {
		buffers->lookup_buffer[i] = zest_CreateBuffer(device, sizeof(tfx_gpu_graph_data_t) * 1000, &storage_buffer_info);
		buffers->lookup_index[i] = zest_AcquireStorageBufferIndex(device, buffers->lookup_buffer[i]);
		buffers->lookup_table_dirty[i] = true;
    }
}

zest_bool zest_tfx_RefreshLibrary(zest_context context, tfx_library_render_resources_t *resources, tfx_library library, tfx_global_library_buffers_t *global_buffers, tfx_library_refresh_t *refresh) {
	zest_device device = zest_GetContextDevice(context);
	memset(refresh, 0, sizeof(tfx_library_refresh_t));

	zest_uint images_before = resources->shape_image_count;
	resources->images_removed = 0;

	//The shape remover runs inside this call, so any shape the refresh drops has already had its image and
	//its descriptor index handed back by the time it returns
	tfx_RefreshLibrary(library, zest_tfx_ShapeLoader, zest_tfx_GetUV, zest_tfx_ShapeRemover, resources, &refresh->result);

	tfxRefreshFlags applied = tfxRefreshFlags_shapes_changed | tfxRefreshFlags_merged
		| tfxRefreshFlags_effects_added | tfxRefreshFlags_effects_removed;
	if ((refresh->result.flags & applied) == 0) {
		//Either the file has not moved or the change needs a full reload, and nothing on the gpu has to move
		return ZEST_FALSE;
	}

	//What follows frees and overwrites resources that frames still in flight are reading
	zest_WaitForIdleDevice(device);

	refresh->images_removed = resources->images_removed;

	zest_bool shapes_changed = (refresh->result.flags & tfxRefreshFlags_shapes_changed) != 0;
	if (shapes_changed) {
		//Only the shapes the refresh just added get uploaded, everything already resident is left alone
		zest__tfx_upload_pending_shapes(context, resources);
		refresh->images_added = resources->shape_image_count - (images_before - refresh->images_removed);
	}

	//The refresh built the gpu shape list while the new shapes still had no image, so it gets rebuilt now
	//that the uv lookup can answer for all of them. That also rebuilds each emitter's start_frame_index,
	//which is why the properties buffer goes up with it.
	tfx_UpdateLibraryGPUImageData(library);

	//The refresh rebuilds the library's colour ramp bitmaps from scratch, which reassigns every ramp index
	//even for effects it did not touch, so the texture those indexes address has to be rebuilt with them.
	zest_FreeImage(resources->color_ramps_texture);
	zest_FreeImageCollection(&resources->color_ramps_collection);
	zest__tfx_build_color_ramps_texture(context, resources, library);

	zest_tfx_UpdateTimelineFXImageData(context, resources, tfx_GetLibraryGPUShapes(library));
	zest_tfx_UpdateTimelineFXParticleProperties(context, resources, library);

	//The refresh also rebuilds the global graph lookup table, moving every emitter's lookup offset, and the
	//ribbon compute shader indexes that table by offset. Nothing marks the buffer dirty on its own, so the
	//table is uploaded again here or the ribbons read curves belonging to some other emitter.
	if (global_buffers) {
		zest_tfx_InitialiseGlobalData(context, global_buffers);
	}

	return ZEST_TRUE;
}

void zest_tfx_FreeLibraryImages(tfx_library_render_resources_t *resources) {
	while (resources->shape_image_count) {
		zest__tfx_free_shape_record(resources, resources->shape_image_count - 1);
	}
	if (resources->shape_images) {
		ZEST_UTILITIES_FREE(resources->shape_images);
		resources->shape_images = 0;
		resources->shape_image_capacity = 0;
	}
	resources->pending_count = 0;
	zest_FreeImageCollection(&resources->color_ramps_collection);
}

void zest_tfx_UpdateTimelineFXImageData(zest_context context, tfx_library_render_resources_t *tfx_rendering, tfx_gpu_shapes shapes) {
	//Upload the timelinefx image data to the image data buffer created
	zest_buffer image_data_buffer = tfx_rendering->image_data;
	zest_device device = zest_GetContextDevice(context);
	zest_buffer staging_buffer = zest_CreateDedicatedStagingBuffer(device, tfx_GetGPUShapesSizeInBytes(shapes), tfx_GetGPUShapesArray(shapes));
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
	zest_imm_CopyBuffer(queue, staging_buffer, image_data_buffer, tfx_GetGPUShapesSizeInBytes(shapes));
	zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(staging_buffer);
}

void zest_tfx_UpdateTimelineFXParticleProperties(zest_context context, tfx_library_render_resources_t *tfx_rendering, tfx_library library) {
	//Upload the timelinefx image data to the image data buffer created
	zest_buffer property_buffer = tfx_rendering->particle_properties;
	zest_device device = zest_GetContextDevice(context);
	zest_buffer staging_buffer = zest_CreateDedicatedStagingBuffer(device, tfx_GetParticlePropertiesBufferSizeInBytes(library), tfx_GetParticlePropertiesBuffer(library));
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
	zest_imm_CopyBuffer(queue, staging_buffer, property_buffer, tfx_GetParticlePropertiesBufferSizeInBytes(library));
	zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(staging_buffer);
}

void zest_tfx_DrawParticleLayer(const zest_command_list command_list, void *user_data) {
	tfx_library_render_resources_t *tfx_resources = (tfx_library_render_resources_t *)user_data;
	zest_layer layer = zest_GetLayer(tfx_resources->layer);

	zest_buffer device_buffer = zest_GetLayerVertexBuffer(layer);
	zest_cmd_BindVertexBuffer(command_list, 0, 1, device_buffer);

	zest_pipeline current_pipeline = 0;
	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(tfx_resources->uniform_buffer);

	zest_layer_instruction_t *current = zest_NextLayerInstruction(layer);
	while(current) {

		zest_cmd_LayerViewport(command_list, layer);

		zest_pipeline pipeline = zest_GetPipeline(current->pipeline_template, command_list);
		if (pipeline && current_pipeline != pipeline) {
			current_pipeline = pipeline;
			zest_cmd_BindPipeline(command_list, pipeline);
		} else if(!pipeline) {
			current = zest_NextLayerInstruction(layer);
			continue;
		}

		tfx_push_constants_t *push_constants = (tfx_push_constants_t *)current->push_constant;
		push_constants->color_ramp_texture_index = tfx_resources->color_ramps_index;
		push_constants->sampler_index = tfx_resources->sampler_index;
		push_constants->image_data_index = tfx_resources->image_data_index;
		push_constants->particle_properties_index = tfx_resources->particle_properties_index;
		push_constants->prev_billboards_index = zest_GetLayerVertexDescriptorIndex(layer, true);
		push_constants->uniform_index = zest_GetUniformBufferDescriptorIndex(uniform_buffer);

		zest_cmd_SendPushConstants(command_list, push_constants, sizeof(tfx_push_constants_t));

		zest_cmd_DrawLayerInstruction(command_list, 6, current);

		current = zest_NextLayerInstruction(layer);
	}
}

//A simple example to render the particles. This is for when the particle manager has one single list of sprites rather than grouped by effect
void zest_tfx_RenderParticles(tfx_stage pm, tfx_library_render_resources_t *resources) {
	zest_layer layer = zest_GetLayer(resources->layer);
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_StartInstanceDrawing(layer, resources->particles.pipeline);

	tfx_instance_t *billboards = tfx_GetInstanceBuffer(pm);
	int instance_count = tfx_GetInstanceCount(pm);
	zest_draw_buffer_result result = zest_DrawInstanceBuffer(layer, billboards, tfx_GetInstanceCount(pm));
}

void zest_tfx_RenderParticlesByEffect(tfx_stage pm, tfx_library_render_resources_t *resources) {
	zest_layer layer = zest_GetLayer(resources->layer);
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_StartInstanceDrawing(layer, resources->particles.pipeline);

	tfx_instance_t *billboards = NULL;
	tfx_effect_instance_data_t *instance_data;
	tfxU32 instance_count = 0;
	//Loop over the effects to get each instance buffer to render
	while (tfx_GetNextInstanceBuffer(pm, &billboards, &instance_data, &instance_count)) {
		zest_draw_buffer_result result = zest_DrawInstanceBuffer(layer, billboards, instance_count);
	}
	tfx_ResetInstanceBufferLoopIndex(pm);
}

void zest_tfx_UploadRibbonData(const zest_command_list command_list, void *user_data) {
	tfx_ribbon_renderer_t *renderer = (tfx_ribbon_renderer_t *)user_data;
	zest_uint fif = zest_CurrentFIF(zest_GetContext(command_list));

	tfx_ribbon_batch_sizes_t sizes = tfx_GetRibbonBatchSizes(&renderer->batch, 0);
	zest_buffer segment_buffer = zest_GetPassOutputBuffer(command_list, TFX_RIBBON_SEGMENT_BUFFER_NAME);
	zest_buffer ribbon_instance_buffer = zest_GetPassOutputBuffer(command_list, TFX_RIBBON_INSTANCE_BUFFER_NAME);
	zest_buffer emitter_buffer = zest_GetPassOutputBuffer(command_list, TFX_RIBBON_EMITTER_BUFFER_NAME);
	if (segment_buffer && sizes.segment_buffer_size_in_bytes) {
		zest_cmd_CopyBuffer(command_list, renderer->segment_staging_buffer[fif], segment_buffer, sizes.segment_buffer_size_in_bytes);
	}
	if (ribbon_instance_buffer && sizes.ribbon_buffer_size_in_bytes) {
		zest_cmd_CopyBuffer(command_list, renderer->ribbon_staging_buffer[fif], ribbon_instance_buffer, sizes.ribbon_buffer_size_in_bytes);
	}
	if (emitter_buffer && sizes.emitter_buffer_size_in_bytes) {
		zest_cmd_CopyBuffer(command_list, renderer->emitter_staging_buffer[fif], emitter_buffer, sizes.emitter_buffer_size_in_bytes);
	}
}

void zest_tfx_UploadGraphData(const zest_command_list command_list, void *user_data) {
	tfx_global_library_buffers_t *buffers = (tfx_global_library_buffers_t*)(user_data);

	zest_context context = zest_GetContext(command_list);
	zest_uint fif = zest_CurrentFIF(context);
	
	if (buffers->lookup_table_dirty[fif]) {
		zest_cmd_CopyBuffer(command_list, buffers->lookup_tables_staging, buffers->lookup_buffer[fif], tfx_GetGPUGraphLookupsBufferSizeInBytes());
		buffers->lookup_table_dirty[fif] = false;
	}
}

void zest_tfx_InitialiseGlobalData(zest_context context, tfx_global_library_buffers_t *buffers) {
	zest_uint fif = zest_CurrentFIF(context);
	zest_ForEachFrameInFlight(fif) {
		zest_device device = zest_GetContextDevice(context);
		zest_buffer staging = zest_CreateDedicatedStagingBuffer(device, tfx_GetGPUGraphLookupsBufferSizeInBytes(), tfx_GetGPUGraphLookupsBuffer());
		if (zest_ResizeBuffer(&buffers->lookup_buffer[fif], tfx_GetGPUGraphLookupsBufferSizeInBytes())) {
			zest_ReleaseStorageBufferIndex(device, buffers->lookup_index[fif]);
			buffers->lookup_index[fif] = zest_AcquireStorageBufferIndex(device, buffers->lookup_buffer[fif]);
		}
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
		zest_imm_CopyBuffer(queue, staging, buffers->lookup_buffer[fif], zest_BufferSize(staging));
		zest_imm_EndCommandBuffer(queue);
		zest_FreeBufferNow(staging);
		buffers->lookup_table_dirty[fif] = false;
	}
}

void zest_tfx_RibbonComputeFunction(const zest_command_list command_list, void *user_data) {
	tfx_ribbon_renderer_t *renderer = (tfx_ribbon_renderer_t *)user_data;
	//An empty batch leaves the transients unplaced, so there is nothing to bind
	if (!zest_tfx_HasRibbonsToDraw(renderer)) return;
	tfx_library_render_resources_t *render_resources = renderer->render_resources;

	zest_context context = zest_GetContext(command_list);
	zest_uint fif = zest_CurrentFIF(context);

	zest_cmd_BindComputePipeline(command_list, zest_GetCompute(render_resources->ribbon_rendering.ribbon_compute));

	zest_uint emitters_index = zest_GetTransientBufferBindlessIndex(command_list, zest_GetPassInputResource(command_list, TFX_RIBBON_EMITTER_BUFFER_NAME));
	zest_uint ribbon_segments_index = zest_GetTransientBufferBindlessIndex(command_list, zest_GetPassInputResource(command_list, TFX_RIBBON_SEGMENT_BUFFER_NAME));
	zest_uint ribbons_index = zest_GetTransientBufferBindlessIndex(command_list, zest_GetPassInputResource(command_list, TFX_RIBBON_INSTANCE_BUFFER_NAME));
	zest_uint vertexes_index = zest_GetTransientBufferBindlessIndex(command_list, zest_GetPassOutputResource(command_list, TFX_RIBBON_VERTEX_BUFFER_NAME));
	zest_uint indexes_index = zest_GetTransientBufferBindlessIndex(command_list, zest_GetPassOutputResource(command_list, TFX_RIBBON_INDEX_BUFFER_NAME));

	zest_uint uniform_index = zest_GetUniformBufferDescriptorIndex(zest_GetUniformBuffer(render_resources->uniform_buffer));
	float lerp = (float)zest_TimerLerp(&render_resources->timer);
	float time = (float)render_resources->timer.seconds_passed;

	for (zest_uint stage_index = 0; stage_index != renderer->batch.stage_count; ++stage_index) {
		tfx_ribbon_dispatch_t ribbon_dispatch = tfx_CreateRibbonBatchDispatch(&renderer->stage_offsets[stage_index]);
		while (tfx_NextRibbonDispatch(renderer->stages[stage_index], &ribbon_dispatch)) {
			tfx_ribbon_bucket_globals_t *push = tfx_GetRibbonDispatchGlobals(&ribbon_dispatch);
			push->lerp = lerp;
			push->time = time;
			push->uniform_index = uniform_index;
			push->graphs_index = renderer->global_buffers->lookup_index[fif];
			push->emitters_index = emitters_index;
			push->ribbon_segments_index = ribbon_segments_index;
			push->ribbons_index = ribbons_index;
			push->vertexes_index = vertexes_index;
			push->indexes_index = indexes_index;
			zest_cmd_SendPushConstants(command_list, push, sizeof(tfx_ribbon_bucket_globals_t));

			zest_cmd_DispatchCompute(command_list, (ribbon_dispatch.total_segments / 1024) + 1, 1, 1);
		}
	}
}

void zest_tfx_RenderRibbons(const zest_command_list command_list, void *user_data) {
	tfx_ribbon_renderer_t *renderer = (tfx_ribbon_renderer_t *)user_data;
	if (!zest_tfx_HasRibbonsToDraw(renderer)) return;
	tfx_library_render_resources_t *render_resources = renderer->render_resources;

	//Bind the buffer that contains the sprite instances to draw. These are updated by the compute shader on the GPU
	zest_cmd_BindVertexBuffer(command_list, 0, 1, zest_GetPassInputBuffer(command_list, TFX_RIBBON_VERTEX_BUFFER_NAME));
	zest_cmd_BindIndexBuffer(command_list, zest_GetPassInputBuffer(command_list, TFX_RIBBON_INDEX_BUFFER_NAME));

	//Draw all the sprites in the buffer that is built by the compute shader
	zest_pipeline pipeline = zest_GetPipeline(render_resources->ribbon_rendering.pipeline, command_list);
	zest_cmd_BindPipeline(command_list, pipeline);

	zest_layer layer = zest_GetLayer(render_resources->layer);
	zest_cmd_LayerViewport(command_list, layer);

	for (zest_uint stage_index = 0; stage_index != renderer->batch.stage_count; ++stage_index) {
		tfx_ribbon_dispatch_t ribbon_dispatch = tfx_CreateRibbonBatchDispatch(&renderer->stage_offsets[stage_index]);
		while (tfx_NextRibbonDispatch(renderer->stages[stage_index], &ribbon_dispatch)) {
			tfx_ribbon_bucket_globals_t *push = tfx_GetRibbonDispatchGlobals(&ribbon_dispatch);
			push->sampler_index = render_resources->sampler_index;
			push->color_ramp_texture_index = render_resources->color_ramps_index;
			push->image_data_index = render_resources->image_data_index;
			zest_cmd_SendPushConstants(command_list, push, sizeof(tfx_ribbon_bucket_globals_t));
			zest_cmd_DrawIndexed(command_list, ribbon_dispatch.index_count, 1, ribbon_dispatch.index_offset, 0, 0);
		}
	}
}

static zest_resource_node zest__tfx_add_ribbon_buffer(tfx_ribbon_renderer_t *renderer, const char *name, zest_resource_usage_hint usage_hint, zest_size size, zest_resource_buffer_provider provider) {
	zest_buffer_resource_info_t info = { usage_hint, size };
	zest_resource_node node = zest_AddTransientBufferResource(name, &info);
	zest_SetResourceUserData(node, renderer);
	zest_SetResourceBufferProvider(node, provider);
	return node;
}

void zest_tfx_AddRibbonsToFrameGraph(tfx_ribbon_renderer_t *renderer, zest_resource_node output_resource) {
	tfx_ribbon_batch_sizes_t sizes = tfx_GetRibbonBatchSizes(&renderer->batch, 0);
	zest_resource_node segment_buffer = zest__tfx_add_ribbon_buffer(renderer, TFX_RIBBON_SEGMENT_BUFFER_NAME, 0, sizes.segment_buffer_size_in_bytes, zest__tfx_ribbon_segment_size);
	zest_resource_node ribbon_instance_buffer = zest__tfx_add_ribbon_buffer(renderer, TFX_RIBBON_INSTANCE_BUFFER_NAME, 0, sizes.ribbon_buffer_size_in_bytes, zest__tfx_ribbon_instance_size);
	zest_resource_node emitter_buffer = zest__tfx_add_ribbon_buffer(renderer, TFX_RIBBON_EMITTER_BUFFER_NAME, 0, sizes.emitter_buffer_size_in_bytes, zest__tfx_ribbon_emitter_size);
	zest_resource_node vertex_buffer = zest__tfx_add_ribbon_buffer(renderer, TFX_RIBBON_VERTEX_BUFFER_NAME, zest_resource_usage_hint_vertex_buffer, sizes.vertex_buffer_size_in_bytes, zest__tfx_ribbon_vertex_size);
	zest_resource_node index_buffer = zest__tfx_add_ribbon_buffer(renderer, TFX_RIBBON_INDEX_BUFFER_NAME, zest_resource_usage_hint_index_buffer, sizes.index_buffer_size_in_bytes, zest__tfx_ribbon_index_size);

	zest_BeginTransferPass("Transfer Ribbon Data"); {
		zest_ConnectOutput(segment_buffer);
		zest_ConnectOutput(ribbon_instance_buffer);
		zest_ConnectOutput(emitter_buffer);
		zest_SetPassTask(zest_tfx_UploadRibbonData, renderer);
		zest_EndPass();
	}

	zest_BeginComputePass("Compute Ribbons"); {
		zest_ConnectInput(segment_buffer);
		zest_ConnectInput(ribbon_instance_buffer);
		zest_ConnectInput(emitter_buffer);
		zest_ConnectOutput(vertex_buffer);
		zest_ConnectOutput(index_buffer);
		zest_SetPassTask(zest_tfx_RibbonComputeFunction, renderer);
		zest_EndPass();
	}

	zest_BeginRenderPass("Draw Ribbons Pass"); {
		zest_ConnectInput(vertex_buffer);
		zest_ConnectInput(index_buffer);
		if (output_resource) {
			zest_ConnectOutput(output_resource);
		} else {
			zest_ConnectSwapChainOutput();
		}
		zest_SetPassTask(zest_tfx_RenderRibbons, renderer);
		zest_EndPass();
	}
}
