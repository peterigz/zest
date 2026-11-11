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
//The most global color ramp layers TimelineFX hands out
#define ZEST__TFX_MAX_COLOR_RAMP_LAYERS 256

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
void zest_tfx_UpdateUniformBuffer(zest_context context, tfx_library_render_resources_t *resources, tfx_stage stage) {
	zest_uniform_buffer buffer = zest_GetUniformBuffer(resources->uniform_buffer);
	tfx_uniform_buffer_data_t *uniform_buffer = (tfx_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	tfx_uniform_buffer_data_t data;
	data.view = zest_LookAt(resources->camera.position, zest_AddVec3(resources->camera.position, resources->camera.front), resources->camera.up);
	data.proj = zest_Perspective(resources->camera.fov, zest_ScreenWidthf(context) / zest_ScreenHeightf(context), 0.1f, 10000.f);
	data.proj.v[1].y *= -1.f;
	data.screen_size.x = zest_ScreenWidthf(context);
	data.screen_size.y = zest_ScreenHeightf(context);
	data.millisecs = 0;
	data.timer_lerp = stage ? tfx_GetStageInterpolation(stage, (float)resources->timer.lerp, (float)zest_TimerFrameLengthMillisecs(&resources->timer)) : 1.f;
	data.update_time = (float)zest_TimerUpdateTime(&resources->timer);
	data.light_padding[0] = data.light_padding[1] = data.light_padding[2] = 0.f;
	data.light = resources->light;
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
	resources->library = library;
	zest__tfx_upload_pending_shapes(context, resources);
	//The uv lookup reads the bindless indexes the upload just acquired
	tfx_UpdateLibraryGPUImageData(library);
	return library;
}

//The library hands out colour ramp indexes by position in its bitmap list, and a refresh rebuilds that
//list from scratch, so the texture has to be built again from the current bitmaps whenever it changes.
static void zest__tfx_build_color_ramps_texture(zest_context context, tfx_library_render_resources_t *resources, tfx_library library) {
	zest_device device = zest_GetContextDevice(context);
	resources->uploaded_color_ramps_version = tfx_GetColorRampsVersion(library);
	if (resources->color_ramps_texture.value) {
		zest_FreeImage(resources->color_ramps_texture);
		zest_FreeImageCollection(&resources->color_ramps_collection);
	}
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
	resources->library = library;
	zest__tfx_build_color_ramps_texture(context, resources, library);

	tfx_UpdateLibraryGPUImageData(library);
	zest_tfx_UpdateTimelineFXImageData(context, resources, tfx_GetLibraryGPUShapes(library));
	zest_tfx_UpdateTimelineFXParticleProperties(context, resources, library);
}

static zest_bool zest__tfx_gpu_data_changed(tfx_library_render_resources_t *resources, tfx_global_library_buffers_t *global_buffers) {
	tfx_library library = resources->library;
	if (library && (tfx_GetColorRampsVersion(library) != resources->uploaded_color_ramps_version
		|| tfx_GetParticlePropertiesVersion(library) != resources->uploaded_particle_properties_version)) {
		return ZEST_TRUE;
	}
	return global_buffers && tfx_GetGPUGraphLookupsVersion() != global_buffers->uploaded_lookups_version;
}

//Frames in flight read everything uploaded here, so the device has to be idle
static void zest__tfx_upload_changed_gpu_data(zest_context context, tfx_library_render_resources_t *resources, tfx_global_library_buffers_t *global_buffers) {
	tfx_library library = resources->library;
	if (library && tfx_GetColorRampsVersion(library) != resources->uploaded_color_ramps_version) {
		zest__tfx_build_color_ramps_texture(context, resources, library);
	}
	if (library && tfx_GetParticlePropertiesVersion(library) != resources->uploaded_particle_properties_version) {
		zest_tfx_UpdateTimelineFXParticleProperties(context, resources, library);
	}
	if (global_buffers && tfx_GetGPUGraphLookupsVersion() != global_buffers->uploaded_lookups_version) {
		zest_tfx_InitialiseGlobalData(context, global_buffers);
	}
}

zest_bool zest_tfx_UpdateLibraryGPUData(zest_context context, tfx_library_render_resources_t *resources, tfx_global_library_buffers_t *global_buffers) {
	if (!zest__tfx_gpu_data_changed(resources, global_buffers)) {
		return ZEST_FALSE;
	}
	zest_WaitForIdleDevice(zest_GetContextDevice(context));
	zest__tfx_upload_changed_gpu_data(context, resources, global_buffers);
	return ZEST_TRUE;
}

static zest_bool zest__tfx_global_data_changed(const tfx_global_library_buffers_t *buffers) {
	return !buffers->library_data_uploaded
		|| tfx_GetGlobalParticlePropertiesVersion() != buffers->uploaded_particle_properties_version
		|| tfx_GetGlobalGPUShapesVersion() != buffers->uploaded_shapes_version
		|| tfx_GetGlobalColorRampsVersion() != buffers->uploaded_color_ramps_version
		|| tfx_GetGPUGraphLookupsVersion() != buffers->uploaded_lookups_version;
}

static void zest__tfx_report_stale_gpu_data(tfx_library_render_resources_t *resources, tfx_global_library_buffers_t *global_buffers) {
	if (resources->stale_data_reported) {
		return;
	}
	if (resources->global_library_data) {
		if (zest__tfx_global_data_changed(resources->global_library_data)) {
			resources->stale_data_reported = ZEST_TRUE;
			ZEST_PRINT("TimelineFX global library data changed since it was uploaded, so particles can draw with the wrong properties or colors. Call zest_tfx_UpdateGlobalGPUData before zest_BeginFrame each frame.");
		}
		return;
	}
	if (!zest__tfx_gpu_data_changed(resources, global_buffers)) {
		return;
	}
	resources->stale_data_reported = ZEST_TRUE;
	ZEST_PRINT("TimelineFX GPU data changed since it was uploaded, so particles can draw with the wrong properties or colors. Call zest_tfx_UpdateLibraryGPUData before zest_BeginFrame each frame.");
}

typedef struct zest__tfx_draw_indexes_t {
	zest_uint color_ramps;
	zest_uint image_data;
	zest_uint particle_properties;
} zest__tfx_draw_indexes_t;

static zest__tfx_draw_indexes_t zest__tfx_get_draw_indexes(const tfx_library_render_resources_t *resources) {
	zest__tfx_draw_indexes_t indexes;
	const tfx_global_library_buffers_t *global_data = resources->global_library_data;
	if (global_data) {
		indexes.color_ramps = global_data->color_ramps_index;
		indexes.image_data = global_data->image_data_index;
		indexes.particle_properties = global_data->particle_properties_index;
	} else {
		indexes.color_ramps = resources->color_ramps_index;
		indexes.image_data = resources->image_data_index;
		indexes.particle_properties = resources->particle_properties_index;
	}
	return indexes;
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

void zest_tfx_AddBillboardVertexAttributes(zest_pipeline_template pipeline, zest_bool reads_previous_instances) {
	zest_AddVertexAttribute(pipeline, 0, 0, zest_format_r32g32b32a32_sfloat, offsetof(tfx_instance_t, position));	            // Location 0: Postion and stretch in w
	zest_AddVertexAttribute(pipeline, 0, 1, zest_format_r16g16b16a16_snorm, offsetof(tfx_instance_t, quaternion));	                // Location 1: Quaternion (packed)
	zest_AddVertexAttribute(pipeline, 0, 2, zest_format_r16g16_sint, offsetof(tfx_instance_t, size));		    // Location 3: Size and handle of the sprite
	zest_AddVertexAttribute(pipeline, 0, 3, zest_format_r8g8b8a8_snorm, offsetof(tfx_instance_t, alignment));					    // Location 2: Alignment
	zest_AddVertexAttribute(pipeline, 0, 4, zest_format_r16g16_sint, offsetof(tfx_instance_t, intensity_gradient_map));       // Location 4: 2 intensities for each color
	zest_AddVertexAttribute(pipeline, 0, 5, zest_format_r8g8b8a8_unorm, offsetof(tfx_instance_t, curved_alpha_life));          	// Location 5: Sharpness and mix lerp value
	zest_AddVertexAttribute(pipeline, 0, 6, zest_format_r32_uint, offsetof(tfx_instance_t, indexes));							// Location 6: texture indexes to sample the correct image and color ramp
	if (reads_previous_instances) {
		zest_AddVertexAttribute(pipeline, 0, 7, zest_format_r32_uint, offsetof(tfx_instance_t, captured_index));   				    // Location 7: index of the sprite in the previous buffer when double buffering
	}
}

void zest_tfx_InitTimelineFXRenderResources(zest_context context, tfx_library_render_resources_t *resources, zest_shader_handle vert_shader, zest_shader_handle frag_shader, zest_shader_handle ribbon_vert, zest_shader_handle ribbon_frag, zest_shader_handle ribbon_comp) {
	zest_device device = zest_GetContextDevice(context);
	resources->device = device;
	resources->uniform_buffer = zest_CreateUniformBuffer(context, "tfx uniform", sizeof(tfx_uniform_buffer_data_t));

	resources->timer = zest_CreateTimer(60);
	resources->light.direction = zest_Vec4Set(0.72f, 0.643f, 0.262f, 0.f);
	resources->light.color = zest_Vec4Set(0.85f, 0.85f, 0.85f, 0.f);
	resources->light.sky_color = zest_Vec4Set(0.25f, 0.27f, 0.3f, 0.f);
	resources->light.ground_color = zest_Vec4Set(0.1f, 0.09f, 0.08f, 0.f);

	//The TimelineFX editor's default preview camera, so effects at the origin show at their authored size
	resources->camera = zest_CreateCamera();
	zest_CameraSetFoV(&resources->camera, 60.f);
	float camera_position[3] = { 0.f, 3.5f, 7.5f };
	zest_CameraPosition(&resources->camera, camera_position);
	zest_CameraSetYaw(&resources->camera, -90.f);
	zest_CameraSetPitch(&resources->camera, -30.f);
	zest_CameraUpdateFront(&resources->camera);

	zest_tfx_UpdateUniformBuffer(context, resources, NULL);

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
	zest_tfx_AddBillboardVertexAttributes(resources->particles.pipeline, ZEST_TRUE);
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
    //No attribute for tfx_ribbon_vertex_t::normal, which only the lit pipelines read. The binding stride stays the full struct size
    //Set the shaders to our custom timelinefx shaders
    zest_SetPipelineVertShader(ribbon_pipeline, resources->ribbon_rendering.vert_shader);
    zest_SetPipelineFragShader(ribbon_pipeline, resources->ribbon_rendering.frag_shader);
	zest_SetPipelineBlend(ribbon_pipeline, zest_PreMultiplyBlendState());
	zest_SetPipelineDepthTest(ribbon_pipeline, true, false);
	resources->ribbon_rendering.pipeline = ribbon_pipeline;

	for (int writes_depth = 0; writes_depth != 2; ++writes_depth) {
		for (int lit = 0; lit != 2; ++lit) {
			resources->particles.layer_pipelines[writes_depth][lit] = resources->particles.pipeline;
			resources->ribbon_rendering.layer_pipelines[writes_depth][lit] = ribbon_pipeline;
		}
	}
	for (int blend_index = 0; blend_index != 3; ++blend_index) {
		for (int lit = 0; lit != 2; ++lit) {
			resources->particles.blend_pipelines[blend_index][lit] = resources->particles.pipeline;
			resources->ribbon_rendering.blend_pipelines[blend_index][lit] = ribbon_pipeline;
		}
	}
}

void zest_tfx_InitLayerPipelines(tfx_library_render_resources_t *resources, const zest_tfx_layer_shaders_t *shaders) {
	tfx_particle_rendering_t *particles = &resources->particles;
	tfx_ribbon_rendering_t *ribbons = &resources->ribbon_rendering;
	const zest_shader_handle billboard_vertex_shaders[2] = { particles->vert_shader, shaders->billboard_lit_vertex };
	const zest_shader_handle billboard_fragment_shaders[2][2] = { { particles->frag_shader, shaders->billboard_lit_fragment }, { shaders->billboard_cutout_fragment, shaders->billboard_lit_cutout_fragment } };
	const zest_shader_handle ribbon_vertex_shaders[2] = { ribbons->vert_shader, shaders->ribbon_lit_vertex };
	const zest_shader_handle ribbon_fragment_shaders[2][2] = { { ribbons->frag_shader, shaders->ribbon_lit_fragment }, { shaders->ribbon_cutout_fragment, shaders->ribbon_lit_cutout_fragment } };
	const char *billboard_names[2][2] = { { "Timelinefx pipeline", "Timelinefx lit pipeline" }, { "Timelinefx depth pipeline", "Timelinefx depth lit pipeline" } };
	const char *ribbon_names[2][2] = { { "ribbon render pipeline", "lit ribbon render pipeline" }, { "depth ribbon render pipeline", "depth lit ribbon render pipeline" } };
	for (int writes_depth = 0; writes_depth != 2; ++writes_depth) {
		for (int lit = 0; lit != 2; ++lit) {
			if (!writes_depth && !lit) continue;
			zest_pipeline_template billboard_pipeline = zest_CopyPipelineTemplate(billboard_names[writes_depth][lit], particles->pipeline);
			zest_SetPipelineShaders(billboard_pipeline, billboard_vertex_shaders[lit], billboard_fragment_shaders[writes_depth][lit]);
			zest_pipeline_template ribbon_pipeline = zest_CopyPipelineTemplate(ribbon_names[writes_depth][lit], ribbons->pipeline);
			zest_SetPipelineShaders(ribbon_pipeline, ribbon_vertex_shaders[lit], ribbon_fragment_shaders[writes_depth][lit]);
			if (lit) {
				zest_AddVertexAttribute(ribbon_pipeline, 0, 4, zest_format_r32_uint, offsetof(tfx_ribbon_vertex_t, normal));
			}
			//Opaque cutouts, drawn before the blended layers so they hide what's behind them
			if (writes_depth) {
				zest_SetPipelineBlend(billboard_pipeline, zest_BlendStateOpaque());
				zest_SetPipelineDepthTest(billboard_pipeline, true, true);
				zest_SetPipelineBlend(ribbon_pipeline, zest_BlendStateOpaque());
				zest_SetPipelineDepthTest(ribbon_pipeline, true, true);
			}
			particles->layer_pipelines[writes_depth][lit] = billboard_pipeline;
			ribbons->layer_pipelines[writes_depth][lit] = ribbon_pipeline;
		}
	}
	//Copies of the blended pipelines with only the color blend changed. The alpha blends ZERO, ONE so the target keeps its alpha
	const char *billboard_blend_names[3][2] = { { "Timelinefx multiply pipeline", "Timelinefx multiply lit pipeline" }, { "Timelinefx subtract pipeline", "Timelinefx subtract lit pipeline" }, { "Timelinefx absorb pipeline", "Timelinefx absorb lit pipeline" } };
	const char *ribbon_blend_names[3][2] = { { "multiply ribbon render pipeline", "multiply lit ribbon render pipeline" }, { "subtract ribbon render pipeline", "subtract lit ribbon render pipeline" }, { "absorb ribbon render pipeline", "absorb lit ribbon render pipeline" } };
	const zest_blend_factor source_factors[3] = { zest_blend_factor_dst_color, zest_blend_factor_one, zest_blend_factor_zero };
	const zest_blend_factor destination_factors[3] = { zest_blend_factor_one_minus_src_alpha, zest_blend_factor_one, zest_blend_factor_one_minus_src_color };
	for (int blend_index = 0; blend_index != 3; ++blend_index) {
		zest_color_blend_attachment_t blend_state = zest_PreMultiplyBlendState();
		blend_state.src_color_blend_factor = source_factors[blend_index];
		blend_state.dst_color_blend_factor = destination_factors[blend_index];
		blend_state.color_blend_op = blend_index == 1 ? zest_blend_op_reverse_subtract : zest_blend_op_add;
		blend_state.src_alpha_blend_factor = zest_blend_factor_zero;
		blend_state.dst_alpha_blend_factor = zest_blend_factor_one;
		for (int lit = 0; lit != 2; ++lit) {
			zest_pipeline_template billboard_pipeline = zest_CopyPipelineTemplate(billboard_blend_names[blend_index][lit], particles->layer_pipelines[0][lit]);
			zest_SetPipelineBlend(billboard_pipeline, blend_state);
			particles->blend_pipelines[blend_index][lit] = billboard_pipeline;
			zest_pipeline_template ribbon_pipeline = zest_CopyPipelineTemplate(ribbon_blend_names[blend_index][lit], ribbons->layer_pipelines[0][lit]);
			zest_SetPipelineBlend(ribbon_pipeline, blend_state);
			ribbons->blend_pipelines[blend_index][lit] = ribbon_pipeline;
		}
	}
}

//The layer's flags once zest_tfx_InitLayerPipelines has made pipelines for them, and none before, so every layer draws as layer 0
static tfxLayerFlags zest__tfx_layer_flags(const tfx_library_render_resources_t *resources, tfxU32 layer) {
	if (!resources->library || resources->particles.layer_pipelines[1][0] == resources->particles.pipeline) {
		return 0;
	}
	return tfx_GetLayerFlags(resources->library, layer);
}

//Without a library every layer is drawn as layer 0
static tfxU32 zest__tfx_layer_count(const tfx_library_render_resources_t *resources) {
	return resources->library ? tfx_GetLayerCount(resources->library) : tfxLAYERS;
}

static zest_bool zest__tfx_writes_depth(tfxLayerFlags flags) {
	return (flags & tfxLayerFlags_writes_depth) != 0;
}

//A blend mode only counts on a layer that doesn't write depth
static zest_pipeline_template zest__tfx_layer_pipeline(zest_pipeline_template layer_pipelines[2][2], zest_pipeline_template blend_pipelines[3][2], tfxLayerFlags flags) {
	const int writes_depth = (flags & tfxLayerFlags_writes_depth) != 0;
	const int lit = (flags & tfxLayerFlags_lit) != 0;
	const tfxU32 blend_mode = (flags & tfxLayerFlags_blend_mask) / tfxLayerFlags_blend_multiply;
	return blend_mode && !writes_depth ? blend_pipelines[blend_mode - 1][lit] : layer_pipelines[writes_depth][lit];
}

static void zest__tfx_free_storage_buffer(zest_context context, zest_buffer buffer, zest_uint descriptor_index) {
	if (!buffer) return;
	zest_ReleaseStorageBufferIndexDeferred(context, descriptor_index);
	zest_FreeBuffer(buffer);
}

void zest_tfx_FreeRenderResources(zest_context context, tfx_library_render_resources_t *resources) {
	//Cached graphs hold resource nodes for the layer freed here
	zest_FlushCachedFrameGraphs(context);
	zest_tfx_FreeLibraryImages(resources);
	zest__tfx_free_storage_buffer(context, resources->image_data, resources->image_data_index);
	zest__tfx_free_storage_buffer(context, resources->particle_properties, resources->particle_properties_index);
	zest__tfx_free_storage_buffer(context, resources->sprite_data_buffer, resources->sprite_data_index);
	zest__tfx_free_storage_buffer(context, resources->emitter_properties_buffer, resources->emitter_properties_index);
	if (resources->layer.value) {
		zest_FreeLayer(resources->layer);
	}
	zest_FreeUniformBuffer(resources->uniform_buffer);
	zest_FreeSampler(resources->sampler);
	if (resources->ribbon_rendering.ribbon_compute.value) {
		zest_FreeCompute(resources->ribbon_rendering.ribbon_compute);
	}
	if (resources->timeline) {
		zest_FreeExecutionTimeline(resources->timeline);
	}
	for (int writes_depth = 0; writes_depth != 2; ++writes_depth) {
		for (int lit = 0; lit != 2; ++lit) {
			if (resources->particles.layer_pipelines[writes_depth][lit] != resources->particles.pipeline) {
				zest_FreePipelineTemplate(resources->particles.layer_pipelines[writes_depth][lit]);
			}
			if (resources->ribbon_rendering.layer_pipelines[writes_depth][lit] != resources->ribbon_rendering.pipeline) {
				zest_FreePipelineTemplate(resources->ribbon_rendering.layer_pipelines[writes_depth][lit]);
			}
		}
	}
	for (int blend_index = 0; blend_index != 3; ++blend_index) {
		for (int lit = 0; lit != 2; ++lit) {
			if (resources->particles.blend_pipelines[blend_index][lit] != resources->particles.pipeline) {
				zest_FreePipelineTemplate(resources->particles.blend_pipelines[blend_index][lit]);
			}
			if (resources->ribbon_rendering.blend_pipelines[blend_index][lit] != resources->ribbon_rendering.pipeline) {
				zest_FreePipelineTemplate(resources->ribbon_rendering.blend_pipelines[blend_index][lit]);
			}
		}
	}
	if (resources->particles.pipeline) {
		zest_FreePipelineTemplate(resources->particles.pipeline);
	}
	if (resources->ribbon_rendering.pipeline) {
		zest_FreePipelineTemplate(resources->ribbon_rendering.pipeline);
	}
	memset(resources, 0, sizeof(tfx_library_render_resources_t));
}

void zest_tfx_CreateRibbonRenderer(zest_context context, tfx_ribbon_renderer_t *renderer, tfx_library_render_resources_t *resources, tfx_global_library_buffers_t *global_buffers) {
	memset(renderer, 0, sizeof(tfx_ribbon_renderer_t));
	renderer->context = context;
	renderer->render_resources = resources;
	renderer->global_buffers = global_buffers;
}

void zest_tfx_FreeRibbonRenderer(tfx_ribbon_renderer_t *renderer) {
	//Cached graphs hold the renderer as their pass and buffer provider user data
	if (renderer->context) {
		zest_FlushCachedFrameGraphs(renderer->context);
	}
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
	zest__tfx_report_stale_gpu_data(renderer->render_resources, renderer->global_buffers);
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
	zest_timer_t *timer = &renderer->render_resources->timer;
	renderer->stage_lerps[stage_count] = tfx_GetStageInterpolation(stage, (float)timer->lerp, (float)zest_TimerFrameLengthMillisecs(timer));
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
    }
}

void zest_tfx_FreeGlobalBuffers(zest_context context, tfx_global_library_buffers_t *buffers) {
	zest_ForEachFrameInFlight(fif) {
		zest__tfx_free_storage_buffer(context, buffers->lookup_buffer[fif], buffers->lookup_index[fif]);
	}
	zest__tfx_free_storage_buffer(context, buffers->particle_properties, buffers->particle_properties_index);
	zest__tfx_free_storage_buffer(context, buffers->image_data, buffers->image_data_index);
	zest_FreeImage(buffers->color_ramps_texture);
	ZEST_UTILITIES_FREE(buffers->uploaded_color_ramp_layer_versions);
	memset(buffers, 0, sizeof(tfx_global_library_buffers_t));
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
	//which changes the particle properties version.
	tfx_UpdateLibraryGPUImageData(library);
	zest_tfx_UpdateTimelineFXImageData(context, resources, tfx_GetLibraryGPUShapes(library));

	//The refresh reassigns ramp indexes and moves every emitter's graph lookup offset, which the versions pick up
	resources->library = library;
	zest__tfx_upload_changed_gpu_data(context, resources, global_buffers);

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
	zest_FreeImage(resources->color_ramps_texture);
	resources->color_ramps_texture = ZEST__ZERO_INIT(zest_image_handle);
	resources->color_ramps_index = 0;
	zest_FreeImageCollection(&resources->color_ramps_collection);
}

//False if the buffer can't hold minimum_bytes; the old index is only released once frames in flight are done with it
static zest_bool zest__tfx_grow_storage_buffer(zest_context context, zest_buffer *buffer, zest_uint *descriptor_index, zest_size unit_size, zest_size minimum_bytes) {
	zest_buffer previous_buffer = *buffer;
	zest_size previous_size = zest_GetBufferSize(previous_buffer);
	if (!zest_GrowBuffer(buffer, unit_size, minimum_bytes)) {
		return zest_GetBufferSize(*buffer) >= minimum_bytes;
	}
	if (*buffer == previous_buffer && zest_GetBufferSize(*buffer) == previous_size) {
		return ZEST_TRUE;
	}
	zest_uint previous_index = *descriptor_index;
	*descriptor_index = zest_AcquireStorageBufferIndex(zest_GetContextDevice(context), *buffer);
	zest_ReleaseStorageBufferIndexDeferred(context, previous_index);
	return ZEST_TRUE;
}

//Creates the buffer when there's none yet. The device has to be idle, frames in flight read the buffer
static zest_bool zest__tfx_upload_storage_buffer(zest_context context, zest_buffer *buffer, zest_uint *descriptor_index, zest_size unit_size, const void *data, zest_size upload_size, const char *name) {
	zest_device device = zest_GetContextDevice(context);
	if (!*buffer) {
		zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
		*buffer = zest_CreateBuffer(device, upload_size, &storage_buffer_info);
		if (*buffer) {
			*descriptor_index = zest_AcquireStorageBufferIndex(device, *buffer);
		}
	}
	if (!*buffer || !zest__tfx_grow_storage_buffer(context, buffer, descriptor_index, unit_size, upload_size)) {
		ZEST_PRINT("Unable to grow the TimelineFX %s buffer to %zu bytes, it was not uploaded.", name, (size_t)upload_size);
		return ZEST_FALSE;
	}
	zest_buffer staging_buffer = zest_CreateDedicatedStagingBuffer(device, upload_size, (void *)data);
	if (!staging_buffer) {
		ZEST_PRINT("Unable to create a %zu byte staging buffer, the TimelineFX %s buffer was not uploaded.", (size_t)upload_size, name);
		return ZEST_FALSE;
	}
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
	zest_imm_CopyBuffer(queue, staging_buffer, *buffer, upload_size);
	zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(staging_buffer);
	return ZEST_TRUE;
}

void zest_tfx_UpdateTimelineFXImageData(zest_context context, tfx_library_render_resources_t *tfx_rendering, tfx_gpu_shapes shapes) {
	zest_size upload_size = tfx_GetGPUShapesSizeInBytes(shapes);
	if (upload_size) {
		zest__tfx_upload_storage_buffer(context, &tfx_rendering->image_data, &tfx_rendering->image_data_index, sizeof(tfx_gpu_image_data_t), tfx_GetGPUShapesArray(shapes), upload_size, "image data");
	}
}

void zest_tfx_UpdateTimelineFXParticleProperties(zest_context context, tfx_library_render_resources_t *tfx_rendering, tfx_library library) {
	tfx_rendering->uploaded_particle_properties_version = tfx_GetParticlePropertiesVersion(library);
	zest_size upload_size = tfx_GetParticlePropertiesBufferSizeInBytes(library);
	if (upload_size) {
		zest__tfx_upload_storage_buffer(context, &tfx_rendering->particle_properties, &tfx_rendering->particle_properties_index, sizeof(tfx_gpu_particle_properties_t), tfx_GetParticlePropertiesBuffer(library), upload_size, "particle properties");
	}
}

//Global layers never move while their library is loaded, so only the layers whose version changed go up unless the texture
//array has to grow. A free layer is never sampled, so whatever it holds can stay.
static zest_bool zest__tfx_upload_global_color_ramps(zest_context context, tfx_global_library_buffers_t *buffers) {
	zest_uint layer_count = tfx_GetGlobalColorRampLayerCount();
	tfx_bitmap_t *first_bitmap = NULL;
	for (zest_uint layer = 0; layer != layer_count && !first_bitmap; ++layer) {
		first_bitmap = tfx_GetGlobalColorRampLayer(layer);
	}
	if (!first_bitmap) {
		return ZEST_TRUE;
	}
	zest_device device = zest_GetContextDevice(context);
	zest_size layer_size = tfx_GetBitmapSize(first_bitmap);
	zest_bool recreate = layer_count > buffers->color_ramp_layer_capacity;
	if (recreate) {
		//Headroom so that loading another library doesn't always recreate it
		zest_uint capacity = ZEST__MIN(layer_count + layer_count / 2 + 1, ZEST__TFX_MAX_COLOR_RAMP_LAYERS);
		tfxU32 *layer_versions = (tfxU32 *)ZEST_UTILITIES_REALLOC(buffers->uploaded_color_ramp_layer_versions, sizeof(tfxU32) * capacity);
		if (!layer_versions) {
			return ZEST_FALSE;
		}
		buffers->uploaded_color_ramp_layer_versions = layer_versions;
		//Releases the old bindless index once the frames that sampled it are done
		zest_FreeImage(buffers->color_ramps_texture);
		buffers->color_ramp_layer_capacity = 0;
		zest_image_info_t image_info = zest_CreateImageInfo((zest_uint)tfx_GetBitmapWidth(first_bitmap), (zest_uint)tfx_GetBitmapHeight(first_bitmap));
		image_info.format = zest_format_r16g16b16a16_sfloat;
		image_info.layer_count = capacity;
		image_info.flags = zest_image_preset_texture | zest_image_flag_force_image_array;
		buffers->color_ramps_texture = zest_CreateImage(device, &image_info);
		if (!buffers->color_ramps_texture.value) {
			ZEST_PRINT("Unable to create a TimelineFX color ramp texture of %u layers.", capacity);
			return ZEST_FALSE;
		}
		buffers->color_ramp_layer_capacity = capacity;
	}

	zest_buffer_image_copy_t copy_regions[ZEST__TFX_MAX_COLOR_RAMP_LAYERS];
	zest_uint region_count = 0;
	for (zest_uint layer = 0; layer != layer_count; ++layer) {
		if ((recreate || tfx_GetGlobalColorRampLayerVersion(layer) != buffers->uploaded_color_ramp_layer_versions[layer]) && tfx_GetGlobalColorRampLayer(layer)) {
			zest_buffer_image_copy_t copy_region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
			copy_region.buffer_offset = layer_size * region_count;
			copy_region.image_aspect = zest_image_aspect_color_bit;
			copy_region.base_array_layer = layer;
			copy_region.layer_count = 1;
			copy_region.image_extent.width = (zest_uint)tfx_GetBitmapWidth(first_bitmap);
			copy_region.image_extent.height = (zest_uint)tfx_GetBitmapHeight(first_bitmap);
			copy_region.image_extent.depth = 1;
			copy_regions[region_count++] = copy_region;
		}
	}
	if (region_count) {
		zest_buffer staging_buffer = zest_CreateDedicatedStagingBuffer(device, layer_size * region_count, 0);
		if (!staging_buffer) {
			ZEST_PRINT("Unable to create a staging buffer for %u TimelineFX color ramp layers.", region_count);
			return ZEST_FALSE;
		}
		zest_byte *staging_data = (zest_byte *)zest_BufferData(staging_buffer);
		for (zest_uint region_index = 0; region_index != region_count; ++region_index) {
			tfx_bitmap_t *bitmap = tfx_GetGlobalColorRampLayer(copy_regions[region_index].base_array_layer);
			memcpy(staging_data + layer_size * region_index, tfx_GetBitmapData(bitmap), layer_size);
		}
		zest_image image = zest_GetImage(buffers->color_ramps_texture);
		zest_uint mip_levels = zest_ImageInfo(image)->mip_levels;
		zest_uint capacity = buffers->color_ramp_layer_capacity;
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		zest_imm_TransitionImage(queue, image, zest_resource_state_copy_dst, 0, mip_levels, 0, capacity);
		zest_imm_CopyBufferRegionsToImage(queue, copy_regions, region_count, staging_buffer, image);
		zest_imm_TransitionImage(queue, image, zest_resource_state_shader_read, 0, mip_levels, 0, capacity);
		zest_imm_EndCommandBuffer(queue);
		zest_FreeBufferNow(staging_buffer);
	}
	for (zest_uint layer = 0; layer != layer_count; ++layer) {
		buffers->uploaded_color_ramp_layer_versions[layer] = tfx_GetGlobalColorRampLayerVersion(layer);
	}
	//The index can only be acquired once the image is in a layout valid for sampling
	if (recreate) {
		buffers->color_ramps_index = zest_AcquireSampledImageIndex(device, zest_GetImage(buffers->color_ramps_texture), zest_texture_array_binding);
	}
	return ZEST_TRUE;
}

zest_bool zest_tfx_UpdateGlobalGPUData(zest_context context, tfx_global_library_buffers_t *global_buffers) {
	ZEST_ASSERT(global_buffers->lookup_buffer[0]);	//Call zest_tfx_CreateGlobalBuffers first
	zest_bool all_libraries_placed = tfx_UpdateGlobalGPUData();
	if (!zest__tfx_global_data_changed(global_buffers)) {
		return all_libraries_placed;
	}
	//Frames in flight read everything uploaded here
	zest_WaitForIdleDevice(zest_GetContextDevice(context));
	zest_bool uploaded = ZEST_TRUE;
	tfxU32 properties_version = tfx_GetGlobalParticlePropertiesVersion();
	if (!global_buffers->library_data_uploaded || properties_version != global_buffers->uploaded_particle_properties_version) {
		zest_size upload_size = tfx_GetGlobalParticlePropertiesBufferSizeInBytes();
		if (!upload_size || zest__tfx_upload_storage_buffer(context, &global_buffers->particle_properties, &global_buffers->particle_properties_index, sizeof(tfx_gpu_particle_properties_t), tfx_GetGlobalParticlePropertiesBuffer(), upload_size, "global particle properties")) {
			global_buffers->uploaded_particle_properties_version = properties_version;
		} else {
			uploaded = ZEST_FALSE;
		}
	}
	tfxU32 shapes_version = tfx_GetGlobalGPUShapesVersion();
	if (!global_buffers->library_data_uploaded || shapes_version != global_buffers->uploaded_shapes_version) {
		tfx_gpu_shapes shapes = tfx_GetGlobalGPUShapes();
		zest_size upload_size = tfx_GetGPUShapesSizeInBytes(shapes);
		if (!upload_size || zest__tfx_upload_storage_buffer(context, &global_buffers->image_data, &global_buffers->image_data_index, sizeof(tfx_gpu_image_data_t), tfx_GetGPUShapesArray(shapes), upload_size, "global image data")) {
			global_buffers->uploaded_shapes_version = shapes_version;
		} else {
			uploaded = ZEST_FALSE;
		}
	}
	tfxU32 color_ramps_version = tfx_GetGlobalColorRampsVersion();
	if (!global_buffers->library_data_uploaded || color_ramps_version != global_buffers->uploaded_color_ramps_version) {
		if (zest__tfx_upload_global_color_ramps(context, global_buffers)) {
			global_buffers->uploaded_color_ramps_version = color_ramps_version;
		} else {
			uploaded = ZEST_FALSE;
		}
	}
	if (tfx_GetGPUGraphLookupsVersion() != global_buffers->uploaded_lookups_version) {
		zest_tfx_InitialiseGlobalData(context, global_buffers);
	}
	if (uploaded) {
		global_buffers->library_data_uploaded = ZEST_TRUE;
	}
	return all_libraries_placed;
}

void zest_tfx_UseGlobalLibraryData(tfx_library_render_resources_t *resources, tfx_global_library_buffers_t *global_buffers) {
	resources->global_library_data = global_buffers;
}

//The draw helpers take one group, the layers that write depth or the blended ones, and share the bound pipeline so billboards and ribbons can
//alternate in one pass. Each binds its buffers before its first draw, since the other kind binds its own in between.

static void zest__tfx_draw_layer_instructions(const zest_command_list command_list, tfx_library_render_resources_t *resources, zest_bool depth_group, zest_pipeline_template *bound_pipeline) {
	zest_layer layer = zest_GetLayer(resources->layer);
	const tfx_particle_rendering_t *particles = &resources->particles;
	zest__tfx_draw_indexes_t draw_indexes = zest__tfx_get_draw_indexes(resources);
	zest_uint uniform_index = zest_GetUniformBufferDescriptorIndex(zest_GetUniformBuffer(resources->uniform_buffer));
	zest_bool buffers_bound = ZEST_FALSE;
	zest_uint instruction_count = zest_GetLayerInstructionCount(layer);
	for (zest_uint instruction_index = 0; instruction_index != instruction_count; ++instruction_index) {
		zest_layer_instruction_t *current = zest_GetLayerInstruction(layer, instruction_index);
		zest_pipeline_template pipeline_template = current->pipeline_template;
		zest_bool writes_depth = pipeline_template != particles->pipeline && (pipeline_template == particles->layer_pipelines[1][0] || pipeline_template == particles->layer_pipelines[1][1]);
		if (writes_depth != depth_group) continue;
		if (pipeline_template != *bound_pipeline) {
			zest_pipeline pipeline = zest_GetPipeline(pipeline_template, command_list);
			if (!pipeline) continue;
			zest_cmd_BindPipeline(command_list, pipeline);
			*bound_pipeline = pipeline_template;
		}
		if (!buffers_bound) {
			zest_cmd_BindVertexBuffer(command_list, 0, 1, zest_GetLayerVertexBuffer(layer));
			zest_cmd_LayerViewport(command_list, layer);
			buffers_bound = ZEST_TRUE;
		}

		tfx_push_constants_t *push_constants = (tfx_push_constants_t *)current->push_constant;
		push_constants->color_ramp_texture_index = draw_indexes.color_ramps;
		push_constants->sampler_index = resources->sampler_index;
		push_constants->image_data_index = draw_indexes.image_data;
		push_constants->particle_properties_index = draw_indexes.particle_properties;
		push_constants->prev_billboards_index = zest_GetLayerVertexDescriptorIndex(layer, true);
		push_constants->uniform_index = uniform_index;
		zest_cmd_SendPushConstants(command_list, push_constants, sizeof(tfx_push_constants_t));

		zest_cmd_DrawLayerInstruction(command_list, 6, current);
	}
}

void zest_tfx_DrawParticleLayer(const zest_command_list command_list, void *user_data) {
	tfx_library_render_resources_t *resources = (tfx_library_render_resources_t *)user_data;
	zest_pipeline_template bound_pipeline = NULL;
	//Instructions with a pipeline that writes depth first, so they hide the blended particles behind them
	for (int group = 0; group != 2; ++group) {
		zest__tfx_draw_layer_instructions(command_list, resources, group == 0, &bound_pipeline);
	}
}

//An instruction for each run of layers that share a pipeline. The layers are copied in the stage's order, which is what captured_index points into
void zest_tfx_RenderParticles(tfx_stage pm, tfx_library_render_resources_t *resources) {
	zest__tfx_report_stale_gpu_data(resources, NULL);
	zest_layer layer = zest_GetLayer(resources->layer);
	zest_pipeline_template current_pipeline = NULL;
	const tfxU32 layer_count = zest__tfx_layer_count(resources);
	for (tfxU32 layer_index = 0; layer_index != layer_count; ++layer_index) {
		tfxU32 instance_count = tfx_GetInstanceCountByLayer(pm, layer_index);
		if (!instance_count) continue;
		zest_pipeline_template pipeline = zest__tfx_layer_pipeline(resources->particles.layer_pipelines, resources->particles.blend_pipelines,zest__tfx_layer_flags(resources, layer_index));
		if (pipeline != current_pipeline) {
			zest_StartInstanceDrawing(layer, pipeline);
			current_pipeline = pipeline;
		}
		zest_DrawInstanceBuffer(layer, tfx_GetInstanceBufferByLayer(pm, layer_index), instance_count);
	}
}

void zest_tfx_RenderParticlesByEffect(tfx_stage pm, tfx_library_render_resources_t *resources) {
	zest__tfx_report_stale_gpu_data(resources, NULL);
	zest_layer layer = zest_GetLayer(resources->layer);
	zest_pipeline_template current_pipeline = NULL;

	tfx_instance_t *billboards = NULL;
	tfx_effect_instance_data_t *instance_data;
	tfxU32 instance_count = 0;
	//The instance buffer is grouped by layer, so loop over the effects once per layer to get each instance buffer to render
	const tfxU32 layer_count = zest__tfx_layer_count(resources);
	for (tfxU32 layer_index = 0; layer_index != layer_count; ++layer_index) {
		zest_pipeline_template pipeline = zest__tfx_layer_pipeline(resources->particles.layer_pipelines, resources->particles.blend_pipelines,zest__tfx_layer_flags(resources, layer_index));
		if (pipeline != current_pipeline) {
			zest_StartInstanceDrawing(layer, pipeline);
			current_pipeline = pipeline;
		}
		while (tfx_GetNextInstanceBuffer(pm, layer_index, &billboards, &instance_data, &instance_count)) {
			zest_draw_buffer_result result = zest_DrawInstanceBuffer(layer, billboards, instance_count);
		}
		tfx_ResetInstanceBufferLoopIndex(pm);
	}
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

void zest_tfx_InitialiseGlobalData(zest_context context, tfx_global_library_buffers_t *buffers) {
	buffers->uploaded_lookups_version = tfx_GetGPUGraphLookupsVersion();
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
	float time = (float)render_resources->timer.seconds_passed;

	for (zest_uint stage_index = 0; stage_index != renderer->batch.stage_count; ++stage_index) {
		float lerp = renderer->stage_lerps[stage_index];
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

			zest_cmd_DispatchCompute(command_list, (ribbon_dispatch.total_segments + 255) / 256, 1, 1);
		}
	}
}

//Every bucket is walked, because each dispatch carries the offsets to the next
static void zest__tfx_draw_ribbon_group(const zest_command_list command_list, tfx_ribbon_renderer_t *renderer, zest_bool depth_group, zest_pipeline_template *bound_pipeline) {
	tfx_library_render_resources_t *render_resources = renderer->render_resources;
	zest__tfx_draw_indexes_t draw_indexes = zest__tfx_get_draw_indexes(render_resources);
	//A dispatch can be on any layer when the stage holds effects from a library with more layers
	tfxLayerFlags layer_flags[tfxLAYERS] = { 0 };
	const tfxU32 layer_count = zest__tfx_layer_count(render_resources);
	for (tfxU32 layer_index = 0; layer_index != layer_count; ++layer_index) {
		layer_flags[layer_index] = zest__tfx_layer_flags(render_resources, layer_index);
	}
	zest_bool buffers_bound = ZEST_FALSE;
	for (zest_uint stage_index = 0; stage_index != renderer->batch.stage_count; ++stage_index) {
		tfx_ribbon_dispatch_t ribbon_dispatch = tfx_CreateRibbonBatchDispatch(&renderer->stage_offsets[stage_index]);
		while (tfx_NextRibbonDispatch(renderer->stages[stage_index], &ribbon_dispatch)) {
			tfxLayerFlags flags = layer_flags[ribbon_dispatch.layer];
			if (zest__tfx_writes_depth(flags) != depth_group) continue;
			zest_pipeline_template pipeline_template = zest__tfx_layer_pipeline(render_resources->ribbon_rendering.layer_pipelines, render_resources->ribbon_rendering.blend_pipelines, flags);
			if (pipeline_template != *bound_pipeline) {
				zest_pipeline pipeline = zest_GetPipeline(pipeline_template, command_list);
				if (!pipeline) continue;
				zest_cmd_BindPipeline(command_list, pipeline);
				*bound_pipeline = pipeline_template;
			}
			if (!buffers_bound) {
				//Written by the ribbon compute pass
				zest_cmd_BindVertexBuffer(command_list, 0, 1, zest_GetPassInputBuffer(command_list, TFX_RIBBON_VERTEX_BUFFER_NAME));
				zest_cmd_BindIndexBuffer(command_list, zest_GetPassInputBuffer(command_list, TFX_RIBBON_INDEX_BUFFER_NAME));
				zest_cmd_LayerViewport(command_list, zest_GetLayer(render_resources->layer));
				buffers_bound = ZEST_TRUE;
			}
			tfx_ribbon_bucket_globals_t *push = tfx_GetRibbonDispatchGlobals(&ribbon_dispatch);
			push->sampler_index = render_resources->sampler_index;
			push->color_ramp_texture_index = draw_indexes.color_ramps;
			push->image_data_index = draw_indexes.image_data;
			zest_cmd_SendPushConstants(command_list, push, sizeof(tfx_ribbon_bucket_globals_t));
			zest_cmd_DrawIndexed(command_list, ribbon_dispatch.index_count, 1, ribbon_dispatch.index_offset, 0, 0);
		}
	}
}

void zest_tfx_RenderRibbons(const zest_command_list command_list, void *user_data) {
	tfx_ribbon_renderer_t *renderer = (tfx_ribbon_renderer_t *)user_data;
	if (!zest_tfx_HasRibbonsToDraw(renderer)) return;
	zest_pipeline_template bound_pipeline = NULL;
	//The buckets on layers that write depth first
	for (int group = 0; group != 2; ++group) {
		zest__tfx_draw_ribbon_group(command_list, renderer, group == 0, &bound_pipeline);
	}
}

static zest_resource_node zest__tfx_add_ribbon_buffer(tfx_ribbon_renderer_t *renderer, const char *name, zest_resource_usage_hint usage_hint, zest_size size, zest_resource_buffer_provider provider) {
	zest_buffer_resource_info_t info = { usage_hint, size };
	zest_resource_node node = zest_AddTransientBufferResource(name, &info);
	zest_SetResourceUserData(node, renderer);
	zest_SetResourceBufferProvider(node, provider);
	return node;
}

static void zest__tfx_connect_draw_output(zest_resource_node output_resource) {
	if (output_resource) {
		zest_ConnectOutput(output_resource);
	} else {
		zest_ConnectSwapChainOutput();
	}
}

//The upload and compute passes, giving back the vertex and index buffers the draw reads
static void zest__tfx_add_ribbon_passes(tfx_ribbon_renderer_t *renderer, zest_resource_node *vertex_buffer, zest_resource_node *index_buffer) {
	tfx_ribbon_batch_sizes_t sizes = tfx_GetRibbonBatchSizes(&renderer->batch, 0);
	zest_resource_node segment_buffer = zest__tfx_add_ribbon_buffer(renderer, TFX_RIBBON_SEGMENT_BUFFER_NAME, 0, sizes.segment_buffer_size_in_bytes, zest__tfx_ribbon_segment_size);
	zest_resource_node ribbon_instance_buffer = zest__tfx_add_ribbon_buffer(renderer, TFX_RIBBON_INSTANCE_BUFFER_NAME, 0, sizes.ribbon_buffer_size_in_bytes, zest__tfx_ribbon_instance_size);
	zest_resource_node emitter_buffer = zest__tfx_add_ribbon_buffer(renderer, TFX_RIBBON_EMITTER_BUFFER_NAME, 0, sizes.emitter_buffer_size_in_bytes, zest__tfx_ribbon_emitter_size);
	*vertex_buffer = zest__tfx_add_ribbon_buffer(renderer, TFX_RIBBON_VERTEX_BUFFER_NAME, zest_resource_usage_hint_vertex_buffer, sizes.vertex_buffer_size_in_bytes, zest__tfx_ribbon_vertex_size);
	*index_buffer = zest__tfx_add_ribbon_buffer(renderer, TFX_RIBBON_INDEX_BUFFER_NAME, zest_resource_usage_hint_index_buffer, sizes.index_buffer_size_in_bytes, zest__tfx_ribbon_index_size);

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
		zest_ConnectOutput(*vertex_buffer);
		zest_ConnectOutput(*index_buffer);
		zest_SetPassTask(zest_tfx_RibbonComputeFunction, renderer);
		zest_EndPass();
	}
}

void zest_tfx_AddRibbonsToFrameGraph(tfx_ribbon_renderer_t *renderer, zest_resource_node output_resource) {
	zest_resource_node vertex_buffer;
	zest_resource_node index_buffer;
	zest__tfx_add_ribbon_passes(renderer, &vertex_buffer, &index_buffer);
	zest_BeginRenderPass("Draw Ribbons Pass"); {
		zest_ConnectInput(vertex_buffer);
		zest_ConnectInput(index_buffer);
		zest__tfx_connect_draw_output(output_resource);
		zest_SetPassTask(zest_tfx_RenderRibbons, renderer);
		zest_EndPass();
	}
}

void zest_tfx_CreateGPUParticleRenderer(zest_context context, tfx_gpu_particle_renderer_t *renderer, tfx_library_render_resources_t *resources, tfx_global_library_buffers_t *global_buffers, tfx_stage stage, zest_shader_handle simulate_shader) {
	memset(renderer, 0, sizeof(tfx_gpu_particle_renderer_t));
	renderer->context = context;
	renderer->render_resources = resources;
	renderer->global_buffers = global_buffers;
	renderer->stage = stage;

	tfx_gpu_particle_buffer_sizes_t pool_sizes = tfx_GetGPUParticleBufferSizes(stage);
	if (!pool_sizes.pool_bytes) {
		return;
	}
	zest_device device = zest_GetContextDevice(context);
	renderer->compute = zest_CreateCompute(device, "TFX Simulate Particles", simulate_shader);

	//The compute shader relies on the pool starting zeroed
	zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	renderer->pool = zest_CreateBuffer(device, pool_sizes.pool_bytes, &storage_buffer_info);
	void *zeroes = calloc(1, pool_sizes.pool_bytes);
	zest_buffer zeroes_staging_buffer = zest_CreateDedicatedStagingBuffer(device, pool_sizes.pool_bytes, zeroes);
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
	zest_imm_CopyBuffer(queue, zeroes_staging_buffer, renderer->pool, pool_sizes.pool_bytes);
	zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(zeroes_staging_buffer);
	free(zeroes);
	renderer->pool_index = zest_AcquireStorageBufferIndex(device, renderer->pool);

	//The hot, cold and noise sections all live in the one pool buffer, so they share an index and the bases say where each starts
	renderer->push.hot_buffer_index = renderer->pool_index;
	renderer->push.cold_buffer_index = renderer->pool_index;
	renderer->push.noise_buffer_index = renderer->pool_index;
	renderer->push.cold_base = pool_sizes.cold_base;
	renderer->push.noise_base = pool_sizes.noise_base;
}

zest_bool zest_tfx_SimulatesOnGPU(const tfx_gpu_particle_renderer_t *renderer) {
	return renderer->pool != NULL;
}

void zest_tfx_FreeGPUParticleRenderer(tfx_gpu_particle_renderer_t *renderer) {
	//Cached graphs hold resource nodes for the pool freed here and the renderer as user data
	if (renderer->context) {
		zest_FlushCachedFrameGraphs(renderer->context);
	}
	zest_ForEachFrameInFlight(fif) {
		zest_FreeBuffer(renderer->upload_staging_buffer[fif]);
	}
	if (renderer->pool) {
		zest_ReleaseStorageBufferIndexDeferred(renderer->context, renderer->pool_index);
		zest_FreeBuffer(renderer->pool);
		zest_FreeCompute(renderer->compute);
	}
	ZEST_UTILITIES_FREE(renderer->key_ranges);
	memset(renderer, 0, sizeof(tfx_gpu_particle_renderer_t));
}

//Without a fetched frame the kernel doesn't write the GPU instances, so only the CPU ranges are kept
static void zest__tfx_record_key_ranges(tfx_gpu_particle_renderer_t *renderer, zest_bool has_gpu_frame) {
	renderer->key_range_count = 0;
	renderer->layer_count = zest__tfx_layer_count(renderer->render_resources);
	tfx_instance_key_range_t range;
	for (zest_uint layer_index = 0; layer_index != renderer->layer_count; ++layer_index) {
		tfx_ResetInstanceBufferLoopIndex(renderer->stage);
		while (tfx_GetNextInstanceKeyRange(renderer->stage, layer_index, &range)) {
			if (!has_gpu_frame) {
				range.gpu_instance_count = 0;
			}
			if (!range.instance_count && !range.gpu_instance_count) {
				continue;
			}
			if (renderer->key_range_count == renderer->key_range_capacity) {
				zest_uint capacity = renderer->key_range_capacity ? renderer->key_range_capacity * 2 : 32;
				tfx_instance_key_range_t *key_ranges = (tfx_instance_key_range_t *)ZEST_UTILITIES_REALLOC(renderer->key_ranges, sizeof(tfx_instance_key_range_t) * capacity);
				if (!key_ranges) {
					break;
				}
				renderer->key_ranges = key_ranges;
				renderer->key_range_capacity = capacity;
			}
			renderer->key_ranges[renderer->key_range_count++] = range;
		}
		renderer->layer_key_range_end[layer_index] = renderer->key_range_count;
	}
	tfx_ResetInstanceBufferLoopIndex(renderer->stage);
}

void zest_tfx_RenderGPUParticleStage(tfx_gpu_particle_renderer_t *renderer) {
	tfx_stage stage = renderer->stage;
	zest__tfx_report_stale_gpu_data(renderer->render_resources, renderer->global_buffers);
	zest_layer layer = zest_GetLayer(renderer->render_resources->layer);
	zest_ResetInstanceLayer(layer);
	zest_StartInstanceDrawing(layer, renderer->render_resources->particles.pipeline);
	tfx_instance_t *instances = tfx_GetInstanceBuffer(stage);
	zest_DrawInstanceBuffer(layer, instances, (zest_uint)tfx_GetInstanceCount(stage));
	renderer->layer_upload_pending = ZEST_TRUE;

	//Every frame that's fetched has to be dispatched because the next one carries on from the particles this one leaves in the pool
	renderer->pending_dispatch_count = 0;
	zest_bool has_gpu_frame = renderer->pool && tfx_GetGPUParticleFrame(stage, &renderer->frame);
	zest__tfx_record_key_ranges(renderer, has_gpu_frame);
	if (!has_gpu_frame) {
		return;
	}
	const tfx_gpu_particle_frame_t *frame = &renderer->frame;

	//Only the CPU instances are uploaded, the compute shader writes the GPU instances after them in the same buffer
	zest_ReserveInstanceLayerDeviceCapacity(layer, frame->gpu_instance_start + frame->gpu_instance_count);

	zest_uint fif = zest_CurrentFIF(renderer->context);
	zest_buffer *staging_buffer = &renderer->upload_staging_buffer[fif];
	if (!*staging_buffer || zest_BufferSize(*staging_buffer) < frame->upload_bytes) {
		zest_FreeBuffer(*staging_buffer);
		*staging_buffer = zest_CreateStagingBuffer(zest_GetContextDevice(renderer->context), frame->upload_bytes + frame->upload_bytes / 2, 0);
	}
	tfx_CopyGPUParticleUpload(frame, zest_BufferData(*staging_buffer));

	renderer->push.tick_base = frame->tick_base;
	renderer->push.spawn_base = frame->spawn_base;
	renderer->push.location_base = frame->location_base;
	renderer->pending_dispatch_count = frame->dispatch_count;
}

void zest_tfx_ResetGPUParticleDraws(tfx_gpu_particle_renderer_t *renderer) {
	renderer->key_range_count = 0;
	memset(renderer->layer_key_range_end, 0, sizeof(renderer->layer_key_range_end));
}

static zest_buffer zest__tfx_gpu_particle_upload_size(zest_context context, zest_resource_node node) {
	tfx_gpu_particle_renderer_t *renderer = (tfx_gpu_particle_renderer_t *)zest_GetResourceUserData(node);
	zest_SetResourceBufferSize(node, renderer->pending_dispatch_count ? renderer->frame.upload_bytes : 0);
	return NULL;
}

void zest_tfx_UploadGPUParticleData(const zest_command_list command_list, void *user_data) {
	tfx_gpu_particle_renderer_t *renderer = (tfx_gpu_particle_renderer_t *)user_data;
	zest_bool layer_uploaded = renderer->layer_upload_pending;
	renderer->layer_upload_pending = ZEST_FALSE;
	zest_UploadInstanceLayerData(command_list, zest_GetLayer(renderer->render_resources->layer));

	zest_resource_node upload_node = renderer->pending_dispatch_count ? zest_GetPassOutputResource(command_list, TFX_GPU_PARTICLE_UPLOAD_BUFFER_NAME) : NULL;
	if (!upload_node || !zest_ResourceBufferIsValid(upload_node)) {
		if (!layer_uploaded) zest_MarkProfileIdle(command_list);
		return;
	}
	zest_uint fif = zest_CurrentFIF(zest_GetContext(command_list));
	zest_cmd_CopyBuffer(command_list, renderer->upload_staging_buffer[fif], zest_GetResourceBuffer(upload_node), renderer->frame.upload_bytes);
}

void zest_tfx_SimulateGPUParticles(const zest_command_list command_list, void *user_data) {
	tfx_gpu_particle_renderer_t *renderer = (tfx_gpu_particle_renderer_t *)user_data;
	zest_uint dispatch_count = renderer->pending_dispatch_count;
	//Consumed here so a frame without a stage update doesn't run the same ticks again
	renderer->pending_dispatch_count = 0;
	zest_resource_node upload_node = zest_GetPassInputResource(command_list, TFX_GPU_PARTICLE_UPLOAD_BUFFER_NAME);
	if (!dispatch_count || !zest_ResourceBufferIsValid(upload_node)) {
		zest_MarkProfileIdle(command_list);
		return;
	}
	zest_uint upload_index = zest_GetTransientBufferBindlessIndex(command_list, upload_node);

	//Every array of the frame is in the one upload buffer, at the bases set in zest_tfx_RenderGPUParticleStage
	tfx_gpu_particle_push_t push = renderer->push;
	push.instance_buffer_index = zest_GetLayerVertexDescriptorIndex(zest_GetLayer(renderer->render_resources->layer), ZEST_FALSE);
	push.graph_buffer_index = renderer->global_buffers->lookup_index[zest_CurrentFIF(zest_GetContext(command_list))];
	push.dispatch_buffer_index = upload_index;
	push.tick_buffer_index = upload_index;
	push.spawn_buffer_index = upload_index;
	push.location_buffer_index = upload_index;

	zest_cmd_BindComputePipeline(command_list, zest_GetCompute(renderer->compute));
	//Each dispatch loops over its own ticks so there's no need for barriers between them
	const tfx_gpu_particle_dispatch_t *dispatches = renderer->frame.dispatches;
	for (zest_uint dispatch_index = 0; dispatch_index != dispatch_count; ++dispatch_index) {
		zest_uint slot_count = dispatches[dispatch_index].slot_count;
		if (!slot_count) continue;
		push.dispatch_index = dispatch_index;
		zest_cmd_SendPushConstants(command_list, &push, sizeof(tfx_gpu_particle_push_t));
		zest_cmd_DispatchCompute(command_list, (slot_count + 255) / 256, 1, 1);
	}
}

static void zest__tfx_draw_key_ranges(const zest_command_list command_list, tfx_gpu_particle_renderer_t *renderer, zest_bool depth_group, zest_pipeline_template *bound_pipeline) {
	tfx_library_render_resources_t *resources = renderer->render_resources;
	zest_layer layer = zest_GetLayer(resources->layer);
	zest__tfx_draw_indexes_t draw_indexes = zest__tfx_get_draw_indexes(resources);
	tfx_push_constants_t push = { 0 };
	push.color_ramp_texture_index = draw_indexes.color_ramps;
	push.image_data_index = draw_indexes.image_data;
	push.particle_properties_index = draw_indexes.particle_properties;
	push.sampler_index = resources->sampler_index;
	push.prev_billboards_index = zest_GetLayerVertexDescriptorIndex(layer, ZEST_TRUE);
	push.uniform_index = zest_GetUniformBufferDescriptorIndex(zest_GetUniformBuffer(resources->uniform_buffer));

	zest_bool buffers_bound = ZEST_FALSE;
	zest_uint range_begin = 0;
	for (zest_uint layer_index = 0; layer_index != renderer->layer_count; ++layer_index) {
		zest_uint range_end = renderer->layer_key_range_end[layer_index];
		tfxLayerFlags flags = zest__tfx_layer_flags(resources, layer_index);
		zest_uint first_range = range_begin;
		range_begin = range_end;
		if (first_range == range_end || zest__tfx_writes_depth(flags) != depth_group) continue;
		zest_pipeline_template pipeline_template = zest__tfx_layer_pipeline(resources->particles.layer_pipelines, resources->particles.blend_pipelines,flags);
		if (pipeline_template != *bound_pipeline) {
			zest_pipeline pipeline = zest_GetPipeline(pipeline_template, command_list);
			if (!pipeline) continue;
			zest_cmd_BindPipeline(command_list, pipeline);
			zest_cmd_SendPushConstants(command_list, &push, sizeof(tfx_push_constants_t));
			*bound_pipeline = pipeline_template;
		}
		if (!buffers_bound) {
			zest_cmd_BindVertexBuffer(command_list, 0, 1, zest_GetLayerVertexBuffer(layer));
			zest_cmd_LayerViewport(command_list, layer);
			buffers_bound = ZEST_TRUE;
		}
		for (zest_uint range_index = first_range; range_index != range_end; ++range_index) {
			const tfx_instance_key_range_t *range = &renderer->key_ranges[range_index];
			if (range->instance_count) {
				zest_cmd_Draw(command_list, 6, range->instance_count, 0, range->instance_start);
			}
			if (range->gpu_instance_count) {
				zest_cmd_Draw(command_list, 6, range->gpu_instance_count, 0, range->gpu_instance_start);
			}
		}
	}
}

void zest_tfx_DrawGPUParticleStage(const zest_command_list command_list, void *user_data) {
	tfx_gpu_particle_renderer_t *renderer = (tfx_gpu_particle_renderer_t *)user_data;
	zest_pipeline_template bound_pipeline = NULL;
	//Layers that write depth first, so they hide the blended particles behind them
	for (int group = 0; group != 2; ++group) {
		zest__tfx_draw_key_ranges(command_list, renderer, group == 0, &bound_pipeline);
	}
}

void zest_tfx_DrawParticlesAndRibbons(const zest_command_list command_list, void *user_data) {
	tfx_ribbon_renderer_t *ribbon_renderer = (tfx_ribbon_renderer_t *)user_data;
	tfx_gpu_particle_renderer_t *particle_renderer = ribbon_renderer->particle_renderer;
	//A graph built while there were no ribbons has no ribbon buffers
	zest_bool draw_ribbons = zest_tfx_HasRibbonsToDraw(ribbon_renderer) && zest_GetPassInputBuffer(command_list, TFX_RIBBON_VERTEX_BUFFER_NAME);
	zest_pipeline_template bound_pipeline = NULL;
	//Everything on layers that write depth first, so it hides the blended billboards and ribbons behind it
	for (int group = 0; group != 2; ++group) {
		if (particle_renderer) {
			zest__tfx_draw_key_ranges(command_list, particle_renderer, group == 0, &bound_pipeline);
		} else {
			zest__tfx_draw_layer_instructions(command_list, ribbon_renderer->render_resources, group == 0, &bound_pipeline);
		}
		if (draw_ribbons) {
			zest__tfx_draw_ribbon_group(command_list, ribbon_renderer, group == 0, &bound_pipeline);
		}
	}
}

//The upload and simulate passes, giving back the layer's instances and previous instances for the draw to read
static void zest__tfx_add_gpu_particle_passes(tfx_gpu_particle_renderer_t *renderer, zest_resource_node *write_layer, zest_resource_node *read_layer) {
	zest_layer layer = zest_GetLayer(renderer->render_resources->layer);
	*write_layer = zest_AddTransientLayerResource(TFX_GPU_PARTICLE_LAYER_NAME, layer, ZEST_FALSE);
	*read_layer = zest_AddTransientLayerResource(TFX_GPU_PARTICLE_PREVIOUS_LAYER_NAME, layer, ZEST_TRUE);
	zest_resource_node pool = 0;
	zest_resource_node upload = 0;
	if (renderer->pool) {
		pool = zest_ImportBufferResource(TFX_GPU_PARTICLE_POOL_NAME, renderer->pool, 0);
		zest_buffer_resource_info_t upload_info = { zest_resource_usage_hint_copy_dst, renderer->frame.upload_bytes };
		upload = zest_AddTransientBufferResource(TFX_GPU_PARTICLE_UPLOAD_BUFFER_NAME, &upload_info);
		zest_SetResourceBufferProvider(upload, zest__tfx_gpu_particle_upload_size);
		zest_SetResourceUserData(upload, renderer);
	}

	zest_BeginTransferPass("Upload TFX Stage Data"); {
		//The previous instances are only an output so that they're transitioned for the graphics queue
		zest_ConnectOutput(*write_layer);
		zest_ConnectOutput(*read_layer);
		if (upload) {
			zest_ConnectOutput(upload);
		}
		zest_SetPassTask(zest_tfx_UploadGPUParticleData, renderer);
		zest_EndPass();
	}

	if (pool) {
		//Writes the GPU instances after the CPU instances that were just uploaded
		zest_BeginComputePass("Simulate TFX GPU Particles"); {
			zest_ConnectInput(upload);
			zest_ConnectOutput(pool);
			zest_ConnectOutput(*write_layer);
			zest_SetPassTask(zest_tfx_SimulateGPUParticles, renderer);
			zest_EndPass();
		}
	}
}

void zest_tfx_AddGPUParticlesToFrameGraph(tfx_gpu_particle_renderer_t *renderer, zest_resource_node output_resource) {
	zest_resource_node write_layer;
	zest_resource_node read_layer;
	zest__tfx_add_gpu_particle_passes(renderer, &write_layer, &read_layer);
	zest_BeginRenderPass("Draw TFX Stage Particles"); {
		zest_ConnectInput(write_layer);
		zest_ConnectInput(read_layer);
		zest__tfx_connect_draw_output(output_resource);
		zest_SetPassTask(zest_tfx_DrawGPUParticleStage, renderer);
		zest_EndPass();
	}
}

void zest_tfx_AddParticlesAndRibbonsToFrameGraph(tfx_ribbon_renderer_t *ribbon_renderer, tfx_gpu_particle_renderer_t *particle_renderer, zest_resource_node output_resource) {
	ribbon_renderer->particle_renderer = particle_renderer;
	zest_resource_node write_layer;
	zest_resource_node read_layer;
	if (particle_renderer) {
		zest__tfx_add_gpu_particle_passes(particle_renderer, &write_layer, &read_layer);
	} else {
		zest_layer layer = zest_GetLayer(ribbon_renderer->render_resources->layer);
		write_layer = zest_AddTransientLayerResource(TFX_GPU_PARTICLE_LAYER_NAME, layer, ZEST_FALSE);
		read_layer = zest_AddTransientLayerResource(TFX_GPU_PARTICLE_PREVIOUS_LAYER_NAME, layer, ZEST_TRUE);
		zest_BeginTransferPass("Upload TFX Layer"); {
			//The previous instances are only an output so that they're transitioned for the graphics queue
			zest_ConnectOutput(write_layer);
			zest_ConnectOutput(read_layer);
			zest_SetPassTask(zest_UploadInstanceLayerData, layer);
			zest_EndPass();
		}
	}
	zest_resource_node vertex_buffer = NULL;
	zest_resource_node index_buffer = NULL;
	if (zest_tfx_HasRibbonsToDraw(ribbon_renderer)) {
		zest__tfx_add_ribbon_passes(ribbon_renderer, &vertex_buffer, &index_buffer);
	}
	zest_BeginRenderPass("Draw TFX Particles and Ribbons"); {
		zest_ConnectInput(write_layer);
		zest_ConnectInput(read_layer);
		if (vertex_buffer) {
			zest_ConnectInput(vertex_buffer);
			zest_ConnectInput(index_buffer);
		}
		zest__tfx_connect_draw_output(output_resource);
		zest_SetPassTask(zest_tfx_DrawParticlesAndRibbons, ribbon_renderer);
		zest_EndPass();
	}
}
