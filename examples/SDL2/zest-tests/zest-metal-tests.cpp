#include <fts.h>
#include <spawn.h>
#include <sys/wait.h>

extern char **environ;

typedef int (*metal_test_callback)(void);

struct MetalTest {
	const char *name;
	metal_test_callback run;
};

static zest_bool metal_use_shader_validation = ZEST_FALSE;
static int metal_result_descriptor = -1;		//Where a child process writes its counts for the process that started it

static zest_device_init_flags MetalTestFlags(void) {
	zest_device_init_flags flags = zest_device_init_flag_enable_validation_layers | zest_device_init_flag_log_validation_errors_to_memory | zest_device_init_flag_log_validation_errors_to_console;
	if (metal_use_shader_validation) {
		flags |= zest_device_init_flag_enable_validation_layers_with_sync;
	}
	return flags;
}

zest_device_builder BeginMetalTestBuilder(void) {
	return zest_BeginMetalDeviceBuilder(MetalTestFlags());
}

static int finish_metal_device(zest_device device) {
	int failed = zest_GetValidationErrorCount(device) != 0;
	zest_DestroyDevice(device);
	return failed;
}

//Only safe once a Zest device has existed, as asking Metal for its device first would stop that device turning validation on
static zest_size metal_allocated_size(void) {
	id<MTLDevice> native_device = MTLCreateSystemDefaultDevice();
	zest_size allocated_size = (zest_size)[native_device currentAllocatedSize];
	[native_device release];
	return allocated_size;
}

//The table argument shape SPIRV-Cross gives a runtime array in a discrete set, so the kernels below read the table as converted shaders will
#define METAL_TEST_TABLE_TYPES \
	"#include <metal_stdlib>\n" \
	"using namespace metal;\n" \
	"template<typename T> struct spvDescriptor { T value; };\n" \
	"template<typename T> struct spvDescriptorArray {\n" \
	"	spvDescriptorArray(const device spvDescriptor<T>* ptr_) : ptr(&ptr_->value) {}\n" \
	"	spvDescriptorArray(const device void *ptr_) : spvDescriptorArray(static_cast<const device spvDescriptor<T>*>(ptr_)) {}\n" \
	"	const device T& operator [] (size_t i) const { return ptr[i]; }\n" \
	"	const device T* ptr;\n" \
	"};\n" \
	"struct Words { uint values[1]; };\n"

static id<MTLComputePipelineState> metal_new_compute_pipeline(zest_device device, const char *source, const char *function_name) {
	id<MTLDevice> native_device = zest_GetMetalDevice(device);
	MTLCompileOptions *compile_options = [[MTLCompileOptions alloc] init];
	compile_options.languageVersion = MTLLanguageVersion3_1;
	NSError *error = nil;
	id<MTLLibrary> library = [native_device newLibraryWithSource:[NSString stringWithUTF8String:source] options:compile_options error:&error];
	[compile_options release];
	if (!library) {
		ZEST_PRINT("The test kernel didn't compile: %s", [[error localizedDescription] UTF8String]);
		return nil;
	}
	id<MTLFunction> function = [library newFunctionWithName:[NSString stringWithUTF8String:function_name]];
	id<MTLComputePipelineState> pipeline = [native_device newComputePipelineStateWithFunction:function error:&error];
	if (!pipeline) {
		ZEST_PRINT("The test pipeline wasn't created: %s", [[error localizedDescription] UTF8String]);
	}
	[function release];
	[library release];
	return pipeline;
}

//Binds each slot's region of the device's bindless table at the index a converted shader expects it
static void metal_bind_table(zest_device device, id<MTLComputeCommandEncoder> encoder) {
	zest__mtl_bind_table_to_compute_encoder(device, encoder);
}

//Reads one slice of one mip of a texture back through a blit on a Zest queue
static zest_bool metal_read_native_texture(zest_device device, id<MTLTexture> texture, zest_uint mip, zest_uint slice, zest_uint width, zest_uint height, zest_uint bytes_per_pixel, void *out_pixels) {
	zest_size size = (zest_size)width * height * bytes_per_pixel;
	id<MTLBuffer> readback = [zest_GetMetalDevice(device) newBufferWithLength:(NSUInteger)size options:MTLResourceStorageModeShared];
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	if (!queue) {
		[readback release];
		return ZEST_FALSE;
	}
	[zest__mtl_queue_blit_encoder(queue) copyFromTexture:texture sourceSlice:slice sourceLevel:mip sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(width, height, 1)
		toBuffer:readback destinationOffset:0 destinationBytesPerRow:width * bytes_per_pixel destinationBytesPerImage:(NSUInteger)size];
	zest_bool finished = zest_imm_EndCommandBuffer(queue);
	if (finished) {
		memcpy(out_pixels, [readback contents], (size_t)size);
	}
	[readback release];
	return finished;
}

static zest_bool metal_read_texture(zest_device device, zest_image image, zest_uint mip, zest_uint slice, zest_uint width, zest_uint height, zest_uint bytes_per_pixel, void *out_pixels) {
	return metal_read_native_texture(device, ZEST_MTL_BACKEND(image, image)->texture, mip, slice, width, height, bytes_per_pixel, out_pixels);
}

//Records a read of mip 0 of a 16x16 image with 4 bytes a pixel into the command buffer the queue is recording. The caller ends the command
//buffer and releases the buffer. Without a fence to order them, a copy is what Metal runs ahead of a pass recorded before it.
static id<MTLBuffer> metal_record_image_read(zest_device device, zest_queue queue, zest_image image) {
	id<MTLBuffer> readback = [zest_GetMetalDevice(device) newBufferWithLength:16 * 16 * 4 options:MTLResourceStorageModeShared];
	[zest__mtl_queue_blit_encoder(queue) copyFromTexture:ZEST_MTL_BACKEND(image, image)->texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(16, 16, 1)
		toBuffer:readback destinationOffset:0 destinationBytesPerRow:16 * 4 destinationBytesPerImage:16 * 16 * 4];
	return readback;
}

//Fills every layer of an image from one tightly packed block of pixels, one layer after another
static zest_bool metal_upload_layers(zest_device device, zest_image image, const void *pixels, zest_size bytes_per_layer, zest_uint layer_count) {
	zest_buffer staging_buffer = zest_CreateDedicatedStagingBuffer(device, bytes_per_layer * layer_count, (void *)pixels);
	if (!staging_buffer) return ZEST_FALSE;
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	zest_bool recorded = queue != NULL;
	for (zest_uint layer = 0; recorded && layer != layer_count; ++layer) {
		zest_buffer_image_copy_t region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
		region.buffer_offset = bytes_per_layer * layer;
		region.image_aspect = zest_image_aspect_color_bit;
		region.base_array_layer = layer;
		region.layer_count = 1;
		region.image_extent = image->info.extent;
		recorded = zest_imm_CopyBufferRegionsToImage(queue, &region, 1, staging_buffer, image);
	}
	recorded = recorded && zest_imm_TransitionImage(queue, image, zest_resource_state_shader_read, 0, image->info.mip_levels, 0, image->info.layer_count);
	zest_bool finished = queue && zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(staging_buffer);
	return recorded && finished;
}

int test_metal_builder(void) {
	zest_device_builder builder = BeginMetalTestBuilder();
	int failed = builder->platform != zest_platform_metal;
	failed |= strcmp(builder->cached_shader_path, "./spv/") != 0;
	failed |= ZEST__NOT_FLAGGED(builder->flags, zest_device_init_flag_enable_validation_layers);
	//Ending the builder is what frees it
	zest_device device = zest_EndDeviceBuilder(builder);
	if (!device) return 1;
	//The suite's first device is the process's first Metal device, which is the only one that can turn validation on
	failed |= !ZEST_MTL_BACKEND(device, device)->api_validation;
	failed |= ZEST_MTL_BACKEND(device, device)->shader_validation != metal_use_shader_validation;
	return failed | finish_metal_device(device);
}

static int run_metal_device_lifecycle(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	int failed = zest_GetDevicePlatform(device) != zest_platform_metal;
	failed |= zest_GetMetalDevice(device) == nil;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	failed |= context == NULL;
	if (context) zest_DestroyContext(context);
	//Enough memory of each kind that a release the backend missed shows in Metal's allocated size
	enum { image_size = 256, byte_count = image_size * image_size * 4 };
	zest_buffer_info_t staging_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_cpu_to_gpu);
	zest_buffer staging_buffer = zest_CreateBuffer(device, byte_count, &staging_info);
	zest_image_info_t image_info = zest_CreateImageInfo(image_size, image_size);
	image_info.flags = zest_image_preset_texture;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(image_handle);
	failed |= !staging_buffer || !image;
	if (staging_buffer && image) {
		memset(zest_BufferData(staging_buffer), 0x7F, byte_count);
		failed |= !zest_CopyBitmapToImage(device, zest_BufferData(staging_buffer), byte_count, image, image_size, image_size);
	}
	if (image) zest_FreeImageNow(image_handle);
	if (staging_buffer) zest_FreeBufferNow(staging_buffer);
	return failed | finish_metal_device(device);
}

//Metal keeps some first-use storage and frees the first lifecycle's late, so the later lifecycles are compared with each other
int test_metal_device_lifecycle(void) {
	zest_size allocated_sizes[4];
	int failed = 0;
	for (int cycle = 0; cycle != 4; ++cycle) {
		failed |= run_metal_device_lifecycle();
		allocated_sizes[cycle] = metal_allocated_size();
	}
	if (allocated_sizes[3] != allocated_sizes[2]) {
		ZEST_PRINT("Metal's allocated size went from %llu to %llu between the third and fourth device lifecycle.", (zest_ull)allocated_sizes[2], (zest_ull)allocated_sizes[3]);
		failed = 1;
	}
	return failed;
}

int test_metal_minimum_spec_rejected(void) {
	zest_device_builder builder = BeginMetalTestBuilder();
	zest_DeviceBuilderLogPath(builder, ".");
	zest__mtl_forced_missing_feature = "a feature forced missing by the test";
	zest_device device = zest_EndDeviceBuilder(builder);
	zest__mtl_forced_missing_feature = NULL;
	int failed = device != NULL;
	if (device) zest_DestroyDevice(device);
	//The compatibility log has to name what was missing
	zest_bool named = ZEST_FALSE;
	FILE *log_file = fopen("./zest_log.txt", "rb");
	if (log_file) {
		char line[512];
		while (fgets(line, sizeof(line), log_file)) {
			if (strstr(line, "MISSING (required): a feature forced missing by the test")) named = ZEST_TRUE;
		}
		fclose(log_file);
	}
	failed |= !named;
	return failed | run_metal_device_lifecycle();
}

int test_metal_validation_unavailable(void) {
	zest__mtl_forced_validation_unavailable = ZEST_TRUE;
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	zest__mtl_forced_validation_unavailable = ZEST_FALSE;
	if (!device) return 1;
	//The device is still created, without the flag and without a claim that validation is on
	int failed = ZEST__FLAGGED(device->setup_info.flags, zest_device_init_flag_enable_validation_layers);
	failed |= ZEST_MTL_BACKEND(device, device)->api_validation;
	return failed | finish_metal_device(device);
}

int test_metal_timeline_wait(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_execution_timeline timeline = zest_CreateExecutionTimeline(device);
	if (!timeline) {
		zest_DestroyDevice(device);
		return 1;
	}
	//Nothing has signalled 1, so both a poll and a timed wait must time out, and the timed one must take its time
	timeline->current_value = 1;
	int failed = zest_WaitForSignal(timeline, 0) != zest_semaphore_status_timeout;
	zest_microsecs wait_start = zest_Microsecs();
	failed |= zest_WaitForSignal(timeline, 50000) != zest_semaphore_status_timeout;
	failed |= zest_Microsecs() - wait_start < 45000;

	//A command buffer on a Zest queue signals it, after which the wait is satisfied at once
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	failed |= queue == NULL;
	if (queue) {
		[ZEST_MTL_BACKEND(queue, queue)->command_buffer encodeSignalEvent:ZEST_MTL_BACKEND(execution_timeline, timeline)->event value:1];
		failed |= !zest_imm_EndCommandBuffer(queue);
	}
	failed |= zest_WaitForSignal(timeline, 1000000) != zest_semaphore_status_success;
	failed |= zest_WaitForSignal(timeline, 0) != zest_semaphore_status_success;
	zest_FreeExecutionTimeline(timeline);
	return failed | finish_metal_device(device);
}

//Reads back, through a device local buffer, the bytes written through the mappings of two host visible buffers in one pool
int test_metal_buffer_pool(void) {
	enum { block_size = 4096 };
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_buffer_info_t staging_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_cpu_to_gpu);
	zest_buffer_info_t device_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	zest_buffer_info_t readback_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_gpu_to_cpu);
	zest_buffer first = zest_CreateBuffer(device, block_size, &staging_info);
	zest_buffer second = zest_CreateBuffer(device, block_size, &staging_info);
	zest_buffer device_buffer = zest_CreateBuffer(device, block_size * 2, &device_info);
	zest_buffer readback = zest_CreateBuffer(device, block_size * 2, &readback_info);
	if (!first || !second || !device_buffer || !readback) {
		zest_DestroyDevice(device);
		return 1;
	}
	int failed = first->memory_pool != second->memory_pool;
	failed |= first->memory_offset == second->memory_offset;
	failed |= [zest__mtl_buffer_object(first) storageMode] != MTLStorageModeShared;
	failed |= [zest__mtl_buffer_object(device_buffer) storageMode] != MTLStorageModePrivate;
	unsigned char *first_bytes = (unsigned char *)zest_BufferData(first);
	unsigned char *second_bytes = (unsigned char *)zest_BufferData(second);
	for (int byte = 0; byte != block_size; ++byte) {
		first_bytes[byte] = (unsigned char)(byte * 7 + 3);
		second_bytes[byte] = (unsigned char)(byte * 13 + 101);
	}

	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
	failed |= queue == NULL;
	if (queue) {
		failed |= !zest_imm_CopyBufferRegion(queue, first, 0, device_buffer, 0, block_size);
		failed |= !zest_imm_CopyBufferRegion(queue, second, 0, device_buffer, block_size, block_size);
		failed |= !zest_imm_CopyBuffer(queue, device_buffer, readback, block_size * 2);
		failed |= !zest_imm_EndCommandBuffer(queue);
	}
	const unsigned char *read_bytes = (const unsigned char *)zest_BufferData(readback);
	failed |= memcmp(read_bytes, first_bytes, block_size) != 0;
	failed |= memcmp(read_bytes + block_size, second_bytes, block_size) != 0;

	zest_FreeBufferNow(readback);
	zest_FreeBufferNow(device_buffer);
	zest_FreeBufferNow(second);
	zest_FreeBufferNow(first);
	return failed | finish_metal_device(device);
}

//A dedicated buffer has a pool of its own, so creating and freeing one must add and remove one allocation of the residency set
int test_metal_residency_tracks_pools(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	id<MTLResidencySet> residency_set = ZEST_MTL_BACKEND(device, device)->residency_set;
	NSUInteger count_before = [residency_set allocationCount];
	char data[1024] = { 0 };
	zest_buffer buffer = zest_CreateDedicatedStagingBuffer(device, sizeof(data), data);
	if (!buffer) {
		zest_DestroyDevice(device);
		return 1;
	}
	int failed = [residency_set allocationCount] != count_before + 1;
	failed |= ![residency_set containsAllocation:zest__mtl_buffer_object(buffer)];
	zest_FreeBufferNow(buffer);
	failed |= [residency_set allocationCount] != count_before;
	return failed | finish_metal_device(device);
}

//Uploads a distinct byte per channel of every pixel and reads the whole image back, which must match byte for byte
static int run_metal_image_round_trip(zest_device device, zest_format format, zest_uint width, zest_uint height, zest_uint bytes_per_pixel) {
	unsigned char pixels[16 * 16 * 4];
	unsigned char read_pixels[16 * 16 * 4];
	zest_size size = (zest_size)width * height * bytes_per_pixel;
	for (zest_size byte = 0; byte != size; ++byte) {
		pixels[byte] = (unsigned char)(byte * 5 + 17);
	}
	memset(read_pixels, 0, sizeof(read_pixels));
	zest_image_info_t image_info = zest_CreateImageInfo(width, height);
	image_info.format = format;
	image_info.flags = zest_image_preset_texture | zest_image_flag_transfer_src;
	zest_image_handle handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(handle);
	if (!image) return 1;
	int failed = !zest_CopyBitmapToImage(device, pixels, size, image, width, height);
	failed |= !zest_CopyImageToBitmap(device, image, read_pixels);
	failed |= memcmp(pixels, read_pixels, (size_t)size) != 0;
	zest_FreeImageNow(handle);
	return failed;
}

int test_metal_image_round_trip(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	int failed = run_metal_image_round_trip(device, zest_format_r8g8b8a8_unorm, 16, 16, 4);
	//The debug font's layers are 8 pixels wide with one byte each
	failed |= run_metal_image_round_trip(device, zest_format_r8_unorm, 8, 8, 1);
	failed |= run_metal_image_round_trip(device, zest_format_r8g8b8a8_unorm, 1, 1, 4);
	return failed | finish_metal_device(device);
}

//A window made first is the order every windowed app has, and validation only survives it when zest_PrepareMetalValidation ran before the window
int test_metal_sdl_helpers(void) {
	zest_bool initialised_video = ZEST_FALSE;
	if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
		initialised_video = ZEST_TRUE;
	}
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 320, 200, 0, "Metal Helpers");
	int failed = window_data.window_handle == NULL;
	int framebuffer_width = 0, framebuffer_height = 0, window_width = 0, window_height = 0;
	window_data.window_sizes_callback(&window_data, &framebuffer_width, &framebuffer_height, &window_width, &window_height);
	failed |= framebuffer_width < 320 || framebuffer_height < 200 || window_width != 320 || window_height != 200;
	zest_device device = zest_implsdl2_CreateMetalDevice(&window_data, MetalTestFlags());
	if (device) {
		failed |= zest_GetDevicePlatform(device) != zest_platform_metal;
		if (!ZEST_MTL_BACKEND(device, device)->api_validation) {
			ZEST_PRINT("A Metal device made after a window has no validation, so zest_PrepareMetalValidation didn't run before the window.");
			failed = 1;
		}
		failed |= finish_metal_device(device);
	} else {
		failed = 1;
	}
	if (window_data.window_handle) {
		SDL_DestroyWindow((SDL_Window *)window_data.window_handle);
	}
	if (initialised_video) {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
	return failed;
}

//A reset keeps the Metal device and remakes its queues and residency set. The device must still upload and read back afterwards, and the old
//queue and set must have no owner left but this test.
int test_metal_device_reset(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	NSUInteger fresh_residency_count = [ZEST_MTL_BACKEND(device, device)->residency_set allocationCount];
	zest_size allocated_sizes[3];
	int failed = 0;
	for (int cycle = 0; cycle != 3; ++cycle) {
		failed |= run_metal_image_round_trip(device, zest_format_r8g8b8a8_unorm, 16, 16, 4);
		id<MTLResidencySet> old_residency_set = [ZEST_MTL_BACKEND(device, device)->residency_set retain];
		id<MTLCommandQueue> old_queue = [ZEST_MTL_BACKEND(queue, &device->queue_families[0]->queues[0])->queue retain];
		zest_ResetDevice(device);
		id<MTLResidencySet> new_residency_set = ZEST_MTL_BACKEND(device, device)->residency_set;
		id<MTLCommandQueue> new_queue = ZEST_MTL_BACKEND(queue, &device->queue_families[0]->queues[0])->queue;
		failed |= zest_GetValidationErrorCount(device) != 0;
		failed |= new_residency_set == nil || new_queue == nil;
		if ([old_residency_set retainCount] != 1 || [old_queue retainCount] != 1) {
			ZEST_PRINT("After reset %i the old residency set has %lu owners and the old queue %lu, where each should have only this test.", cycle, (unsigned long)[old_residency_set retainCount], (unsigned long)[old_queue retainCount]);
			failed = 1;
		}
		[old_residency_set release];
		[old_queue release];
		if (new_residency_set && [new_residency_set allocationCount] != fresh_residency_count) {
			ZEST_PRINT("After reset %i the residency set holds %lu allocations, where a new device holds %lu.", cycle, (unsigned long)[new_residency_set allocationCount], (unsigned long)fresh_residency_count);
			failed = 1;
		}
		allocated_sizes[cycle] = metal_allocated_size();
	}
	failed |= run_metal_image_round_trip(device, zest_format_r8g8b8a8_unorm, 16, 16, 4);
	if (allocated_sizes[2] != allocated_sizes[1]) {
		ZEST_PRINT("Metal's allocated size went from %llu to %llu between the second and third device reset.", (zest_ull)allocated_sizes[1], (zest_ull)allocated_sizes[2]);
		failed = 1;
	}
	return failed | finish_metal_device(device);
}

//A red and blue checker averages to half red and half blue in its 1x1 mip, which is read back and checked
int test_metal_mip_chain(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	unsigned char pixels[4 * 4 * 4];
	for (int pixel = 0; pixel != 16; ++pixel) {
		zest_bool red = ((pixel % 4) + (pixel / 4)) % 2 == 0;
		pixels[pixel * 4 + 0] = red ? 255 : 0;
		pixels[pixel * 4 + 1] = 0;
		pixels[pixel * 4 + 2] = red ? 0 : 255;
		pixels[pixel * 4 + 3] = 255;
	}
	zest_image_info_t image_info = zest_CreateImageInfo(4, 4);
	image_info.mip_levels = 3;
	image_info.flags = zest_image_preset_texture_mipmaps;
	zest_image_handle handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(handle);
	if (!image) {
		zest_DestroyDevice(device);
		return 1;
	}
	int failed = !zest_CopyBitmapToImage(device, pixels, sizeof(pixels), image, 4, 4);
	failed |= image->layout != zest_image_layout_shader_read_only_optimal;
	unsigned char last_mip[4] = { 0, 0, 0, 0 };
	failed |= !metal_read_texture(device, image, 2, 0, 1, 1, 4, last_mip);
	failed |= last_mip[0] < 125 || last_mip[0] > 130;
	failed |= last_mip[1] != 0;
	failed |= last_mip[2] < 125 || last_mip[2] > 130;
	failed |= last_mip[3] != 255;
	zest_FreeImageNow(handle);
	return failed | finish_metal_device(device);
}

//Every format either has a Metal form the device has, and is then creatable, or is reported unsupported. Nothing is supported and uncreatable.
int test_metal_format_table(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	const zest_mtl_feature_support_t *support = &ZEST_MTL_BACKEND(device, device)->support;
	int failed = 0;
	int supported_count = 0;
	for (int format_value = zest_format_undefined; format_value <= zest_format_astc_12X12_srgb_block; ++format_value) {
		zest_format format = (zest_format)format_value;
		zest_capability_flags capability = zest__compressed_format_capability(format);
		zest_bool expected = zest__mtl_device_has_pixel_format(support, zest__mtl_pixel_format(format)) && (!capability || ZEST__FLAGGED(device->capabilities.enabled, capability));
		zest_bool supported = zest_IsImageFormatSupported(device, format, zest_image_preset_texture);
		if (supported != expected) {
			ZEST_PRINT("Format %i: supported is %i where the table says %i.", format_value, (int)supported, (int)expected);
			failed = 1;
		}
		if (!supported) continue;
		supported_count++;
		zest_image_info_t image_info = zest_CreateImageInfo(4, 4);
		image_info.format = format;
		image_info.flags = zest_image_preset_texture;
		zest_image_handle handle = zest_CreateImage(device, &image_info);
		zest_image image = zest_GetImage(handle);
		if (!image || !ZEST_MTL_BACKEND(image, image)->texture) {
			ZEST_PRINT("Format %i is reported as supported but a 4x4 image of it wasn't created.", format_value);
			failed = 1;
		}
		if (image) zest_FreeImageNow(handle);
	}
	//A table that mapped nothing would pass every check above
	failed |= supported_count < 50;
	failed |= !zest_IsImageFormatSupported(device, zest_format_r8g8b8a8_unorm, zest_image_preset_texture);
	failed |= zest_IsImageFormatSupported(device, zest_format_r8g8b8_unorm, zest_image_preset_texture);
	failed |= zest_IsImageFormatSupported(device, zest_format_bc7_unorm_block, zest_image_preset_color_attachment);
	failed |= zest_IsImageFormatSupported(device, zest_format_d32_sfloat, zest_image_preset_storage);
	failed |= !zest_IsImageFormatSupported(device, zest_format_d32_sfloat, zest_image_preset_depth_attachment);
	//Copies are allowed between formats that share their channels and size, which is where Metal copies the bits as they are
	failed |= !zest__mtl_formats_copy_compatible(zest_format_r8g8b8a8_unorm, zest_format_r8g8b8a8_unorm);
	failed |= !zest__mtl_formats_copy_compatible(zest_format_r8g8b8a8_unorm, zest_format_r8g8b8a8_srgb);
	failed |= !zest__mtl_formats_copy_compatible(zest_format_r8g8b8a8_unorm, zest_format_b8g8r8a8_unorm);
	failed |= !zest__mtl_formats_copy_compatible(zest_format_r8g8b8a8_unorm, zest_format_r8g8b8a8_uint);
	failed |= zest__mtl_formats_copy_compatible(zest_format_r8g8b8a8_unorm, zest_format_r32_sfloat);
	failed |= zest__mtl_formats_copy_compatible(zest_format_r8g8b8a8_unorm, zest_format_r16g16_unorm);
	failed |= zest__mtl_formats_copy_compatible(zest_format_r8g8b8a8_unorm, zest_format_r16g16b16a16_sfloat);
	//The table has no size for a depth format, so one only copies to itself
	failed |= !zest__mtl_formats_copy_compatible(zest_format_d32_sfloat, zest_format_d32_sfloat);
	failed |= zest__mtl_formats_copy_compatible(zest_format_d32_sfloat, zest_format_d16_unorm);
	failed |= zest__mtl_formats_copy_compatible(zest_format_bc1_rgba_unorm_block, zest_format_bc7_unorm_block);
	return failed | finish_metal_device(device);
}

typedef struct metal_slots_push_t {
	zest_uint sampler_index;
	zest_uint texture_2d_index;
	zest_uint texture_cube_index;
	zest_uint texture_array_index;
	zest_uint texture_3d_index;
	zest_uint texture_cube_array_index;
	zest_uint read_buffer_index;
	zest_uint write_buffer_index;
	zest_uint storage_image_index;
	zest_uint uniform_index;
} metal_slots_push_t;

static const char *metal_slots_kernel = METAL_TEST_TABLE_TYPES
	"struct Results { float4 values[1]; };\n"
	"struct Params { float4 tint; uint add; };\n"
	"struct Push { uint sampler_index; uint texture_2d_index; uint texture_cube_index; uint texture_array_index; uint texture_3d_index; uint texture_cube_array_index; uint read_buffer_index; uint write_buffer_index; uint storage_image_index; uint uniform_index; };\n"
	"kernel void read_slots(constant Push& pc [[buffer(4)]], device const void* slot0 [[buffer(5)]], device const void* slot1 [[buffer(6)]], device const void* slot2 [[buffer(7)]], device const void* slot3 [[buffer(8)]],\n"
	"	device const void* slot4 [[buffer(9)]], device const void* slot5 [[buffer(10)]], device const void* slot6 [[buffer(11)]], device const void* slot7 [[buffer(12)]], device const void* slot8 [[buffer(13)]], uint3 cell [[thread_position_in_grid]]) {\n"
	"	spvDescriptorArray<sampler> samplers {slot0};\n"
	"	spvDescriptorArray<texture2d<float>> textures_2d {slot1};\n"
	"	spvDescriptorArray<texturecube<float>> textures_cube {slot2};\n"
	"	spvDescriptorArray<texture2d_array<float>> textures_array {slot3};\n"
	"	spvDescriptorArray<texture3d<float>> textures_3d {slot4};\n"
	"	spvDescriptorArray<device Words*> read_buffers {slot5};\n"
	"	spvDescriptorArray<device Results*> write_buffers {slot5};\n"
	"	spvDescriptorArray<texture2d<float, access::write>> storage_images {slot6};\n"
	"	spvDescriptorArray<constant Params*> params {slot7};\n"
	"	spvDescriptorArray<texturecube_array<float>> textures_cube_array {slot8};\n"
	"	storage_images[pc.storage_image_index].write(float4(float(cell.x) / 255.0, float(cell.y) / 255.0, 0.5, 1.0), cell.xy);\n"
	"	if (cell.x != 0u || cell.y != 0u) return;\n"
	"	sampler chosen_sampler = samplers[pc.sampler_index];\n"
	"	device Results* results = write_buffers[pc.write_buffer_index];\n"
	"	results->values[0] = float4(float(read_buffers[pc.read_buffer_index]->values[0] + params[pc.uniform_index]->add), params[pc.uniform_index]->tint.xyz);\n"
	"	results->values[1] = textures_2d[pc.texture_2d_index].sample(chosen_sampler, float2(0.5, 0.5), level(0.0));\n"
	"	results->values[2] = textures_cube[pc.texture_cube_index].sample(chosen_sampler, float3(1.0, 0.0, 0.0), level(0.0));\n"
	"	results->values[3] = textures_array[pc.texture_array_index].sample(chosen_sampler, float2(0.5, 0.5), 1u, level(0.0));\n"
	"	results->values[4] = textures_3d[pc.texture_3d_index].sample(chosen_sampler, float3(0.5, 0.5, 0.5), level(0.0));\n"
	"	results->values[5] = textures_cube_array[pc.texture_cube_array_index].sample(chosen_sampler, float3(1.0, 0.0, 0.0), 1u, level(0.0));\n"
	"}\n";

static zest_image metal_create_solid_image(zest_device device, zest_image_handle *out_handle, zest_uint size, zest_uint depth, zest_uint layer_count, zest_image_flags flags, const unsigned char *layer_colors) {
	zest_image_info_t image_info = depth > 1 ? zest_CreateImageInfo3D(size, size, depth) : zest_CreateImageInfo(size, size);
	image_info.layer_count = layer_count;
	image_info.flags = flags;
	*out_handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(*out_handle);
	if (!image) return NULL;
	if (!layer_colors) return image;
	unsigned char pixels[12 * 2 * 2 * 2 * 4];
	zest_size bytes_per_layer = (zest_size)size * size * depth * 4;
	for (zest_uint layer = 0; layer != layer_count; ++layer) {
		for (zest_size pixel = 0; pixel != bytes_per_layer / 4; ++pixel) {
			memcpy(pixels + layer * bytes_per_layer + pixel * 4, layer_colors + layer * 4, 4);
		}
	}
	return metal_upload_layers(device, image, pixels, bytes_per_layer, layer_count) ? image : NULL;
}

static zest_bool metal_color_near(const float *value, const unsigned char *expected) {
	for (int channel = 0; channel != 4; ++channel) {
		float difference = value[channel] - (float)expected[channel] / 255.f;
		if (difference > 0.004f || difference < -0.004f) return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

//Writes a real resource at one index of every binding slot, with a decoy of the same kind on each side, and reads all nine back through
//one kernel. Run at the first, a middle and the last index of every slot, so a wrong region offset or entry size reads a decoy or nothing.
int test_metal_bindless_slots(void) {
	static const unsigned char decoy_colors[12 * 4] = { 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255 };
	static const unsigned char cube_colors[6 * 4] = { 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255 };
	static const unsigned char array_colors[2 * 4] = { 200, 0, 0, 255, 40, 50, 60, 255 };
	static const unsigned char volume_color[4] = { 70, 80, 90, 255 };
	static const unsigned char cube_array_colors[12 * 4] = { 0, 200, 0, 255, 0, 200, 0, 255, 0, 200, 0, 255, 0, 200, 0, 255, 0, 200, 0, 255, 0, 200, 0, 255, 100, 110, 120, 255, 100, 110, 120, 255, 100, 110, 120, 255, 100, 110, 120, 255, 100, 110, 120, 255, 100, 110, 120, 255 };
	static const unsigned char two_texels[2 * 4] = { 255, 0, 0, 255, 0, 0, 255, 255 };
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	id<MTLComputePipelineState> pipeline = metal_new_compute_pipeline(device, metal_slots_kernel, "read_slots");
	if (!pipeline) {
		zest_DestroyDevice(device);
		return 1;
	}
	int failed = 0;

	//The real sampler blends the two texels of the 2D texture, where the decoy would return one of them
	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	sampler_info.min_filter = zest_filter_linear;
	sampler_info.mag_filter = zest_filter_linear;
	sampler_info.address_mode_u = zest_sampler_address_mode_clamp_to_edge;
	sampler_info.address_mode_v = zest_sampler_address_mode_clamp_to_edge;
	sampler_info.address_mode_w = zest_sampler_address_mode_clamp_to_edge;
	zest_sampler_handle sampler_handle = zest_CreateSampler(device, &sampler_info);
	sampler_info.min_filter = zest_filter_nearest;
	sampler_info.mag_filter = zest_filter_nearest;
	zest_sampler_handle decoy_sampler_handle = zest_CreateSampler(device, &sampler_info);
	zest_sampler sampler = zest_GetSampler(sampler_handle);
	zest_sampler decoy_sampler = zest_GetSampler(decoy_sampler_handle);

	zest_image_flags cube_flags = zest_image_preset_texture | zest_image_flag_cubemap;
	zest_image_handle image_handles[12];
	zest_image texture_2d = metal_create_solid_image(device, &image_handles[0], 2, 1, 1, zest_image_preset_texture, NULL);
	zest_image texture_cube = metal_create_solid_image(device, &image_handles[1], 1, 1, 6, cube_flags, cube_colors);
	zest_image texture_array = metal_create_solid_image(device, &image_handles[2], 1, 1, 2, zest_image_preset_texture, array_colors);
	zest_image texture_3d = metal_create_solid_image(device, &image_handles[3], 1, 2, 1, zest_image_preset_texture_3d, volume_color);
	zest_image texture_cube_array = metal_create_solid_image(device, &image_handles[4], 1, 1, 12, cube_flags, cube_array_colors);
	zest_image storage_image = metal_create_solid_image(device, &image_handles[5], 8, 1, 1, zest_image_preset_storage | zest_image_flag_transfer_src, NULL);
	zest_image decoy_2d = metal_create_solid_image(device, &image_handles[6], 2, 1, 1, zest_image_preset_texture, decoy_colors);
	zest_image decoy_cube = metal_create_solid_image(device, &image_handles[7], 1, 1, 6, cube_flags, decoy_colors);
	zest_image decoy_array = metal_create_solid_image(device, &image_handles[8], 1, 1, 2, zest_image_preset_texture, decoy_colors);
	zest_image decoy_3d = metal_create_solid_image(device, &image_handles[9], 1, 2, 1, zest_image_preset_texture_3d, decoy_colors);
	zest_image decoy_cube_array = metal_create_solid_image(device, &image_handles[10], 1, 1, 12, cube_flags, decoy_colors);
	zest_image decoy_storage_image = metal_create_solid_image(device, &image_handles[11], 8, 1, 1, zest_image_preset_storage | zest_image_flag_transfer_src, NULL);
	zest_bool images_created = sampler && decoy_sampler && texture_2d && texture_cube && texture_array && texture_3d && texture_cube_array && storage_image && decoy_2d && decoy_cube && decoy_array && decoy_3d && decoy_cube_array && decoy_storage_image;
	if (images_created) {
		//Two rows of the same two texels, as the image is 2x2
		unsigned char rows[2 * 2 * 4];
		memcpy(rows, two_texels, 8);
		memcpy(rows + 8, two_texels, 8);
		images_created = zest_CopyBitmapToImage(device, rows, sizeof(rows), texture_2d, 2, 2);
	}

	//Host visible storage and uniform buffers, so the test can fill and read them without a copy
	zest_buffer_info_t storage_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_cpu_to_gpu);
	zest_buffer_info_t uniform_info = zest_CreateBufferInfo(zest_buffer_type_uniform, zest_memory_usage_cpu_to_gpu);
	zest_buffer read_buffer = zest_CreateBuffer(device, 256, &storage_info);
	zest_buffer write_buffer = zest_CreateBuffer(device, 256, &storage_info);
	zest_buffer decoy_buffer = zest_CreateBuffer(device, 256, &storage_info);
	zest_buffer uniform_buffer = zest_CreateBuffer(device, 256, &uniform_info);
	zest_buffer decoy_uniform_buffer = zest_CreateBuffer(device, 256, &uniform_info);
	if (!images_created || !read_buffer || !write_buffer || !decoy_buffer || !uniform_buffer || !decoy_uniform_buffer) {
		[pipeline release];
		zest_DestroyDevice(device);
		return 1;
	}
	float uniform_tint[4] = { 0.25f, 0.5f, 0.75f, 1.f };
	zest_uint uniform_add = 1000;
	memset(zest_BufferData(uniform_buffer), 0, 256);
	memcpy(zest_BufferData(uniform_buffer), uniform_tint, sizeof(uniform_tint));
	memcpy((char *)zest_BufferData(uniform_buffer) + 16, &uniform_add, sizeof(uniform_add));
	memset(zest_BufferData(decoy_uniform_buffer), 0, 256);
	memset(zest_BufferData(read_buffer), 0, 256);
	*(zest_uint *)zest_BufferData(read_buffer) = 41;

	zest_descriptor_set set = device->bindless_set;
	const zest_mtl_bindless_regions_t *regions = &ZEST_MTL_BACKEND(descriptor_set, set)->pool->regions;
	zest_platform platform = device->platform;
	for (int run = 0; run != 3; ++run) {
		//Slot 5 holds two real entries, so its index stops one short of the end
		zest_uint indexes[zest_max_global_binding_number];
		for (zest_uint binding_number = 0; binding_number != zest_max_global_binding_number; ++binding_number) {
			zest_uint count = regions->counts[binding_number];
			zest_uint last = binding_number == zest_storage_buffer_binding ? count - 2 : count - 1;
			indexes[binding_number] = run == 0 ? 0 : run == 1 ? count / 2 : last;
			failed |= count < 4;
		}
		if (failed) break;
		//A decoy on each side of every real entry
		for (int side = -1; side <= 2; ++side) {
			zest_uint offset = (zest_uint)side;
			if (side == 0) continue;
			#define METAL_DECOY_INDEX(binding) (indexes[binding] + offset)
			#define METAL_DECOY_FITS(binding) (METAL_DECOY_INDEX(binding) < regions->counts[binding])
			if (METAL_DECOY_FITS(zest_sampler_binding)) platform->update_bindless_image_descriptor(device, zest_sampler_binding, METAL_DECOY_INDEX(zest_sampler_binding), zest_descriptor_type_sampler, NULL, NULL, decoy_sampler, set);
			if (METAL_DECOY_FITS(zest_texture_2d_binding)) platform->update_bindless_image_descriptor(device, zest_texture_2d_binding, METAL_DECOY_INDEX(zest_texture_2d_binding), zest_descriptor_type_sampled_image, decoy_2d, decoy_2d->default_view, NULL, set);
			if (METAL_DECOY_FITS(zest_texture_cube_binding)) platform->update_bindless_image_descriptor(device, zest_texture_cube_binding, METAL_DECOY_INDEX(zest_texture_cube_binding), zest_descriptor_type_sampled_image, decoy_cube, decoy_cube->default_view, NULL, set);
			if (METAL_DECOY_FITS(zest_texture_array_binding)) platform->update_bindless_image_descriptor(device, zest_texture_array_binding, METAL_DECOY_INDEX(zest_texture_array_binding), zest_descriptor_type_sampled_image, decoy_array, decoy_array->default_view, NULL, set);
			if (METAL_DECOY_FITS(zest_texture_3d_binding)) platform->update_bindless_image_descriptor(device, zest_texture_3d_binding, METAL_DECOY_INDEX(zest_texture_3d_binding), zest_descriptor_type_sampled_image, decoy_3d, decoy_3d->default_view, NULL, set);
			if (METAL_DECOY_FITS(zest_storage_buffer_binding)) platform->update_bindless_storage_buffer_descriptor(device, zest_storage_buffer_binding, METAL_DECOY_INDEX(zest_storage_buffer_binding), decoy_buffer, 0, set);
			if (METAL_DECOY_FITS(zest_storage_image_binding)) platform->update_bindless_image_descriptor(device, zest_storage_image_binding, METAL_DECOY_INDEX(zest_storage_image_binding), zest_descriptor_type_storage_image, decoy_storage_image, decoy_storage_image->default_view, NULL, set);
			if (METAL_DECOY_FITS(zest_uniform_buffer_binding)) platform->update_bindless_uniform_buffer_descriptor(device, zest_uniform_buffer_binding, METAL_DECOY_INDEX(zest_uniform_buffer_binding), decoy_uniform_buffer, set);
			if (METAL_DECOY_FITS(zest_texture_cube_array_binding)) platform->update_bindless_image_descriptor(device, zest_texture_cube_array_binding, METAL_DECOY_INDEX(zest_texture_cube_array_binding), zest_descriptor_type_sampled_image, decoy_cube_array, decoy_cube_array->default_view, NULL, set);
			#undef METAL_DECOY_FITS
			#undef METAL_DECOY_INDEX
		}
		metal_slots_push_t push;
		push.sampler_index = indexes[zest_sampler_binding];
		push.texture_2d_index = indexes[zest_texture_2d_binding];
		push.texture_cube_index = indexes[zest_texture_cube_binding];
		push.texture_array_index = indexes[zest_texture_array_binding];
		push.texture_3d_index = indexes[zest_texture_3d_binding];
		push.texture_cube_array_index = indexes[zest_texture_cube_array_binding];
		push.read_buffer_index = indexes[zest_storage_buffer_binding];
		push.write_buffer_index = indexes[zest_storage_buffer_binding] + 1;
		push.storage_image_index = indexes[zest_storage_image_binding];
		push.uniform_index = indexes[zest_uniform_buffer_binding];
		platform->update_bindless_image_descriptor(device, zest_sampler_binding, push.sampler_index, zest_descriptor_type_sampler, NULL, NULL, sampler, set);
		platform->update_bindless_image_descriptor(device, zest_texture_2d_binding, push.texture_2d_index, zest_descriptor_type_sampled_image, texture_2d, texture_2d->default_view, NULL, set);
		platform->update_bindless_image_descriptor(device, zest_texture_cube_binding, push.texture_cube_index, zest_descriptor_type_sampled_image, texture_cube, texture_cube->default_view, NULL, set);
		platform->update_bindless_image_descriptor(device, zest_texture_array_binding, push.texture_array_index, zest_descriptor_type_sampled_image, texture_array, texture_array->default_view, NULL, set);
		platform->update_bindless_image_descriptor(device, zest_texture_3d_binding, push.texture_3d_index, zest_descriptor_type_sampled_image, texture_3d, texture_3d->default_view, NULL, set);
		platform->update_bindless_image_descriptor(device, zest_texture_cube_array_binding, push.texture_cube_array_index, zest_descriptor_type_sampled_image, texture_cube_array, texture_cube_array->default_view, NULL, set);
		platform->update_bindless_storage_buffer_descriptor(device, zest_storage_buffer_binding, push.read_buffer_index, read_buffer, 0, set);
		platform->update_bindless_storage_buffer_descriptor(device, zest_storage_buffer_binding, push.write_buffer_index, write_buffer, 16, set);
		platform->update_bindless_image_descriptor(device, zest_storage_image_binding, push.storage_image_index, zest_descriptor_type_storage_image, storage_image, storage_image->default_view, NULL, set);
		platform->update_bindless_uniform_buffer_descriptor(device, zest_uniform_buffer_binding, push.uniform_index, uniform_buffer, set);
		memset(zest_BufferData(write_buffer), 0, 256);
		memset(zest_BufferData(decoy_buffer), 0, 256);

		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_compute);
		if (!queue) {
			failed = 1;
			break;
		}
		zest_mtl_queue_backend_t *queue_backend = ZEST_MTL_BACKEND(queue, queue);
		@autoreleasepool {
			id<MTLComputeCommandEncoder> encoder = [queue_backend->command_buffer computeCommandEncoder];
			[encoder waitForFence:queue_backend->fence];
			[encoder setComputePipelineState:pipeline];
			metal_bind_table(device, encoder);
			[encoder setBytes:&push length:sizeof(push) atIndex:ZEST__MTL_PUSH_CONSTANT_BUFFER_INDEX];
			[encoder dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
			[encoder updateFence:queue_backend->fence];
			[encoder endEncoding];
		}
		failed |= !zest_imm_EndCommandBuffer(queue);

		const float *results = (const float *)zest_BufferData(write_buffer);
		int run_failed = results[0] != 1041.f || results[1] != 0.25f || results[2] != 0.5f || results[3] != 0.75f;
		//Half way between a red and a blue texel through the linear sampler
		run_failed |= results[4] < 0.48f || results[4] > 0.52f || results[5] != 0.f || results[6] < 0.48f || results[6] > 0.52f;
		run_failed |= !metal_color_near(results + 8, cube_colors);
		run_failed |= !metal_color_near(results + 12, array_colors + 4);
		run_failed |= !metal_color_near(results + 16, volume_color);
		run_failed |= !metal_color_near(results + 20, cube_array_colors + 6 * 4);
		unsigned char written_pixels[8 * 8 * 4];
		run_failed |= !zest_CopyImageToBitmap(device, storage_image, written_pixels);
		for (int pixel = 0; pixel != 64; ++pixel) {
			run_failed |= written_pixels[pixel * 4 + 0] != pixel % 8 || written_pixels[pixel * 4 + 1] != pixel / 8 || written_pixels[pixel * 4 + 3] != 255;
		}
		//The decoy buffer is only written when the write entry was read from the wrong place
		const zest_uint *decoy_words = (const zest_uint *)zest_BufferData(decoy_buffer);
		for (int word = 0; word != 64; ++word) {
			run_failed |= decoy_words[word] != 0;
		}
		if (run_failed) {
			ZEST_PRINT("Metal Bindless Slots: run %i read back wrong (first result %.1f, 2D sample %.2f %.2f %.2f).", run, results[0], results[4], results[5], results[6]);
		}
		failed |= run_failed;
	}

	zest_FreeBufferNow(decoy_uniform_buffer);
	zest_FreeBufferNow(uniform_buffer);
	zest_FreeBufferNow(decoy_buffer);
	zest_FreeBufferNow(write_buffer);
	zest_FreeBufferNow(read_buffer);
	for (int image_index = 0; image_index != 12; ++image_index) {
		zest_FreeImageNow(image_handles[image_index]);
	}
	[pipeline release];
	return failed | finish_metal_device(device);
}

//The default images and the debug font are what a device can't be created without, so they are read back as uploaded
int test_metal_device_with_defaults(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	int failed = device->default_image_2d == NULL || device->default_image_cube == NULL;
	failed |= device->bindless_set == NULL || device->pipeline_layout == NULL;
	failed |= device->depth_format != zest_format_d32_sfloat;
	if (!failed) {
		unsigned char pixel[4] = { 0, 0, 0, 0 };
		failed |= !metal_read_texture(device, device->default_image_2d, 0, 0, 1, 1, 4, pixel);
		failed |= pixel[0] != 0xFF || pixel[1] != 0x00 || pixel[2] != 0xFF || pixel[3] != 0xFF;
	}
	//The letter A is layer 33 of the font, one byte per pixel expanded from one bit per pixel
	zest_image font_image = zest_GetImage(device->debug_font_image);
	failed |= font_image == NULL;
	if (font_image) {
		unsigned char glyph[64];
		unsigned char expected[64];
		for (int row = 0; row != 8; ++row) {
			for (int column = 0; column != 8; ++column) {
				expected[row * 8 + column] = (zest_debug_font[33 * 8 + row] & (0x80 >> column)) ? 0xFF : 0x00;
			}
		}
		failed |= !metal_read_texture(device, font_image, 0, 33, 8, 8, 1, glyph);
		failed |= memcmp(glyph, expected, sizeof(glyph)) != 0;
	}
	return failed | finish_metal_device(device);
}

int test_metal_headless_context(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_size allocated_sizes[3];
	int failed = 0;
	for (int cycle = 0; cycle != 3; ++cycle) {
		zest_create_context_info_t info = zest_CreateContextInfo();
		zest_context context = zest_CreateHeadlessContext(device, &info);
		failed |= context == NULL;
		if (context) {
			//An event holds no GPU memory, so a context that leaked its frame graph events only shows as an owner left on one
			failed |= !zest_BeginCommandGraph(context, "Empty", 0);
			zest_frame_graph frame_graph = zest_EndFrameGraph();
			failed |= frame_graph == NULL || zest_FlushFrameGraph(frame_graph) != zest_semaphore_status_success;
			zest_frame_graph_semaphores semaphores = *zest_map_at(context->cached_frame_graph_semaphores, "Empty");
			id<MTLSharedEvent> event = [ZEST_MTL_BACKEND(frame_graph_semaphores, semaphores)->events[0][0] retain];
			zest_DestroyContext(context);
			if (event == nil || [event retainCount] != 1) {
				ZEST_PRINT("A destroyed context left %lu owners on a frame graph event, where only this test should hold it.", (unsigned long)[event retainCount]);
				failed = 1;
			}
			[event release];
		}
		allocated_sizes[cycle] = (zest_size)[zest_GetMetalDevice(device) currentAllocatedSize];
	}
	if (allocated_sizes[2] != allocated_sizes[1]) {
		ZEST_PRINT("Metal's allocated size went from %llu to %llu between the second and third headless context.", (zest_ull)allocated_sizes[1], (zest_ull)allocated_sizes[2]);
		failed = 1;
	}
	return failed | finish_metal_device(device);
}

int test_metal_empty_command_graph(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	int failed = context == NULL;
	if (context) {
		failed |= !zest_BeginCommandGraph(context, "Empty", 0);
		zest_frame_graph frame_graph = zest_EndFrameGraph();
		failed |= frame_graph == NULL || ZEST__NOT_FLAGGED(zest_GetFrameGraphResult(frame_graph), zest_fgs_no_work_to_do);
		if (frame_graph) {
			failed |= zest_FlushFrameGraph(frame_graph) != zest_semaphore_status_success;
		}
		zest_DestroyContext(context);
	}
	return failed | finish_metal_device(device);
}

int test_metal_render_pass_actions(void) {
	int failed = zest__mtl_load_action(zest_load_op_clear) != MTLLoadActionClear;
	failed |= zest__mtl_load_action(zest_load_op_load) != MTLLoadActionLoad;
	failed |= zest__mtl_load_action(zest_load_op_dont_care) != MTLLoadActionDontCare;
	failed |= zest__mtl_store_action(zest_store_op_store) != MTLStoreActionStore;
	failed |= zest__mtl_store_action(zest_store_op_dont_care) != MTLStoreActionDontCare;
	//Inside the target
	MTLScissorRect rect = zest__mtl_clamped_scissor(2, 3, 4, 5, 16, 16);
	failed |= rect.x != 2 || rect.y != 3 || rect.width != 4 || rect.height != 5;
	//Hanging off the right and bottom edges
	rect = zest__mtl_clamped_scissor(10, 12, 100, 100, 16, 16);
	failed |= rect.x != 10 || rect.y != 12 || rect.width != 6 || rect.height != 4;
	//A negative origin loses what lies before the target
	rect = zest__mtl_clamped_scissor(-4, -6, 10, 10, 16, 16);
	failed |= rect.x != 0 || rect.y != 0 || rect.width != 6 || rect.height != 4;
	//Wholly outside, past the far edges and before the near ones
	rect = zest__mtl_clamped_scissor(20, 20, 8, 8, 16, 16);
	failed |= rect.x != 0 || rect.y != 0 || rect.width != 0 || rect.height != 0;
	rect = zest__mtl_clamped_scissor(-20, 0, 8, 8, 16, 16);
	failed |= rect.x != 0 || rect.y != 0 || rect.width != 0 || rect.height != 0;
	//A size at the 32 bit limit doesn't wrap
	rect = zest__mtl_clamped_scissor(8, 8, 0xFFFFFFFFu, 0xFFFFFFFFu, 16, 16);
	failed |= rect.x != 8 || rect.y != 8 || rect.width != 8 || rect.height != 8;
	return failed;
}

static int metal_pixels_match(const zest_uint *pixels, zest_uint pixel_count, const int expected[4]) {
	for (zest_uint index = 0; index != pixel_count; ++index) {
		const unsigned char *channels = (const unsigned char *)&pixels[index];
		for (int channel = 0; channel != 4; ++channel) {
			if (abs((int)channels[channel] - expected[channel]) > 1) return 0;
		}
	}
	return 1;
}

static const int metal_blue_expected[4] = { 51, 102, 153, 255 };
static const int metal_orange_expected[4] = { 153, 102, 51, 255 };

static zest_image_handle metal_create_clear_target(zest_device device, zest_format format, zest_image_flags flags) {
	zest_image_info_t image_info = zest_CreateImageInfo(16, 16);
	image_info.format = format;
	image_info.flags = flags | zest_image_flag_transfer_src;
	return zest_CreateImage(device, &image_info);
}

static zest_clear_value_t metal_colour_clear_value(float red, float green, float blue) {
	zest_clear_value_t clear_value = {};
	clear_value.color.x = red;
	clear_value.color.y = green;
	clear_value.color.z = blue;
	clear_value.color.w = 1.f;
	return clear_value;
}

//Clears the image in a command buffer of its own, and returns what the clear returned
static zest_bool run_metal_colour_clear(zest_device device, zest_image image, zest_clear_value_t clear_value) {
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	if (!queue) return ZEST_FALSE;
	zest_bool cleared = zest_imm_ClearColorImage(queue, image, clear_value);
	return zest_imm_EndCommandBuffer(queue) && cleared;
}

static zest_bool metal_validation_error_mentions(zest_device device, const char *text) {
	zest_map_foreach(error_index, device->validation_errors) {
		const char *message = device->validation_errors.data[error_index].str;
		if (message && strstr(message, text)) return ZEST_TRUE;
	}
	return ZEST_FALSE;
}

static zest_uint metal_committed_count(zest_context context, const char *graph_name) {
	zest_context_queue context_queue = *zest_map_at(context->cached_frame_graph_queues[context->graphics_queue_index], graph_name);
	zest_uint count = 0;
	zest_ForEachFrameInFlight(frame_in_flight) {
		count += zest_vec_size(ZEST_MTL_BACKEND(context_queue, context_queue)->committed[frame_in_flight]);
	}
	return count;
}

//Sets a scissor that hangs off every edge of the 16x16 target, one wholly outside it, then a viewport and a clip that leaves the target
static void metal_scissor_pass(const zest_command_list command_list, void *user_data) {
	zest_scissor_rect_t scissor = ZEST__ZERO_INIT(zest_scissor_rect_t);
	scissor.offset.x = -8;
	scissor.offset.y = -8;
	scissor.extent.width = 64;
	scissor.extent.height = 64;
	zest_cmd_Scissor(command_list, &scissor);
	scissor.offset.x = 32;
	scissor.offset.y = 32;
	scissor.extent.width = 8;
	scissor.extent.height = 8;
	zest_cmd_Scissor(command_list, &scissor);
	zest_viewport_t viewport = { 0.f, 0.f, 16.f, 16.f, 0.f, 1.f };
	zest_cmd_ViewPort(command_list, &viewport);
	zest_cmd_Clip(command_list, 4.f, 4.f, 64.f, 64.f, 0.f, 1.f);
	*(int *)user_data += 1;
}

//Clears the image to blue, or orange, in a one pass command graph named "Clear", then reads it back and checks every pixel.
//With a depth image the pass clears that too, to a depth the test puts in place of the 1.0 the API always asks for.
static int run_metal_clear_graph(zest_context context, zest_image image, zest_image depth_image, zest_bool orange, zest_bool set_scissors) {
	int callback_count = 0;
	if (!zest_BeginCommandGraph(context, "Clear", 0)) return 1;
	zest_resource_node node = zest_ImportImageResource("Target", image, 0);
	if (orange) {
		zest_SetResourceClearColor(node, 0.6f, 0.4f, 0.2f, 1.0f);
	} else {
		zest_SetResourceClearColor(node, 0.2f, 0.4f, 0.6f, 1.0f);
	}
	zest_BeginRenderPass("Clear Pass");
	zest_ConnectOutput(node);
	if (depth_image) {
		zest_ConnectOutput(zest_ImportImageResource("Depth", depth_image, 0));
	}
	if (set_scissors) {
		zest_SetPassTask(metal_scissor_pass, &callback_count);
	} else {
		zest_SetPassTask(zest_EmptyRenderPass, 0);
	}
	zest_EndPass();
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	if (!frame_graph) return 1;
	if (depth_image) {
		//1.0 is also what Metal clears depth to when it's told nothing, so only another value shows the clear depth is passed on
		zest_map_foreach(pass_index, frame_graph->final_passes) {
			frame_graph->final_passes.data[pass_index].execution_details.depth_attachment.clear_value.depth_stencil.depth = 0.25f;
		}
	}
	int failed = zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
	failed |= set_scissors && callback_count != 1;
	//The barrier is a record on Metal, and this is all it does
	failed |= image->layout != zest_image_layout_color_attachment_optimal;
	zest_uint pixels[16 * 16];
	memset(pixels, 0, sizeof(pixels));
	failed |= !zest_CopyImageToBitmap(context->device, image, pixels);
	failed |= !metal_pixels_match(pixels, 16 * 16, orange ? metal_orange_expected : metal_blue_expected);
	if (depth_image && depth_image->info.format == zest_format_d32_sfloat) {
		float depths[16 * 16];
		memset(depths, 0, sizeof(depths));
		failed |= !metal_read_texture(context->device, depth_image, 0, 0, 16, 16, 4, depths);
		for (zest_uint index = 0; index != 16 * 16; ++index) failed |= depths[index] != 0.25f;
	}
	return failed;
}

int test_metal_render_pass_clear(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	int failed = context == NULL;
	zest_image_handle image_handle = metal_create_clear_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment);
	zest_image_handle depth_handle = metal_create_clear_target(device, device->depth_format, zest_image_preset_depth_attachment);
	zest_image_handle stencil_handle = metal_create_clear_target(device, zest_format_d32_sfloat_s8_uint, zest_image_preset_depth_attachment);
	zest_image image = zest_GetImage(image_handle);
	zest_image depth_image = zest_GetImage(depth_handle);
	zest_image stencil_image = zest_GetImage(stencil_handle);
	failed |= image == NULL || depth_image == NULL || stencil_image == NULL;
	for (int iteration = 0; iteration != 3 && !failed; ++iteration) {
		failed |= run_metal_clear_graph(context, image, NULL, iteration == 1, ZEST_FALSE);
		//The flush waited on the graph's timeline, and that wait completes and releases every batch whose last signal it has seen
		if (!failed && metal_committed_count(context, "Clear") != 0) {
			ZEST_PRINT("The Clear graph's queue holds %u committed command buffers after flush %i, where the wait should have released the one just committed.", metal_committed_count(context, "Clear"), iteration);
			failed = 1;
		}
		zest_WaitForIdleDevice(device);
	}
	if (!failed) {
		//A validation count reads and releases every batch that has finished
		failed |= zest_GetValidationErrorCount(device) != 0;
		failed |= metal_committed_count(context, "Clear") != 0;
		failed |= run_metal_clear_graph(context, image, depth_image, ZEST_TRUE, ZEST_FALSE);
		//A format with stencil is attached as depth alone
		failed |= run_metal_clear_graph(context, image, stencil_image, ZEST_FALSE, ZEST_FALSE);
		failed |= run_metal_clear_graph(context, image, NULL, ZEST_TRUE, ZEST_TRUE);
		//With the queue the context prefers held, the next batch runs on another and has to wait for the slot's last batch through its ordering event
		zest_uint slot = context->graphics_queue_index;
		zest_uint preferred_index = context->preferred_queue_index[slot];
		zest_queue held_queue = zest__acquire_manager_queue(device->queue_families[context->graphics_family_index], preferred_index, ZEST_FALSE);
		failed |= held_queue == NULL || held_queue->index != preferred_index;
		failed |= run_metal_clear_graph(context, image, NULL, ZEST_FALSE, ZEST_FALSE);
		failed |= context->preferred_queue_index[slot] == preferred_index;
		if (held_queue) zest__release_queue(held_queue);
	}
	if (!failed) {
		//A flush with no timeline waits for nothing, so its batch stays committed until the device's idle wait completes and releases it
		failed |= !zest_BeginCommandGraph(context, "Clear", 0);
		zest_resource_node node = zest_ImportImageResource("Target", image, 0);
		zest_BeginRenderPass("Clear Pass");
		zest_ConnectOutput(node);
		zest_SetPassTask(zest_EmptyRenderPass, 0);
		zest_EndPass();
		zest_frame_graph frame_graph = zest_EndFrameGraph();
		failed |= frame_graph == NULL || zest_FlushFrameGraph(frame_graph) != zest_semaphore_status_success;
		failed |= metal_committed_count(context, "Clear") != 1;
		zest_WaitForIdleDevice(device);
		if (metal_committed_count(context, "Clear") != 0) {
			ZEST_PRINT("The Clear graph's queue holds %u committed command buffers after the device's idle wait, where the wait should have released them.", metal_committed_count(context, "Clear"));
			failed = 1;
		}
	}
	//The context's pass descriptor is reused, and must not keep the last pass's target alive once the image is freed
	id<MTLTexture> target_texture = image ? [ZEST_MTL_BACKEND(image, image)->texture retain] : nil;
	zest_WaitForIdleDevice(device);
	failed |= zest_GetValidationErrorCount(device) != 0;
	zest_FreeImageNow(image_handle);
	zest_FreeImageNow(depth_handle);
	zest_FreeImageNow(stencil_handle);
	if (target_texture && [target_texture retainCount] != 1) {
		ZEST_PRINT("The freed render target still has %lu owners, where only this test should hold it.", (unsigned long)[target_texture retainCount]);
		failed = 1;
	}
	[target_texture release];
	if (context) {
		zest_DestroyContext(context);
	}
	return failed | finish_metal_device(device);
}

//A depth image is refused before Metal is asked for a row of no bytes, and a colour image still reads back
int test_metal_depth_readback(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	const zest_format depth_formats[2] = { zest_format_d32_sfloat, zest_format_d32_sfloat_s8_uint };
	int failed = 0;
	for (int format_index = 0; format_index != 2; ++format_index) {
		zest_image_handle depth_handle = metal_create_clear_target(device, depth_formats[format_index], zest_image_preset_depth_attachment);
		zest_image depth_image = zest_GetImage(depth_handle);
		if (!depth_image) {
			failed = 1;
			continue;
		}
		float depths[16 * 16];
		for (zest_uint index = 0; index != 16 * 16; ++index) depths[index] = 7.f;
		failed |= zest_CopyImageToBitmap(device, depth_image, depths) != ZEST_FALSE;
		//The one error is Zest's refusal. A message from Metal's own validation would make it two.
		if (zest_GetValidationErrorCount(device) != 1) {
			ZEST_PRINT("Reading back depth format %i raised %u validation errors, where only the refusal should be recorded.", (int)depth_formats[format_index], zest_GetValidationErrorCount(device));
			failed = 1;
		}
		for (zest_uint index = 0; index != 16 * 16; ++index) failed |= depths[index] != 7.f;
		zest_ResetValidationErrors(device);
		zest_FreeImageNow(depth_handle);
	}
	failed |= run_metal_image_round_trip(device, zest_format_r8g8b8a8_unorm, 16, 16, 4);
	return failed | finish_metal_device(device);
}

//Each kind of transient arena backing is one allocation in the residency set, and all of it goes with the backing
int test_metal_arena_backing(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	id<MTLResidencySet> residency_set = ZEST_MTL_BACKEND(device, device)->residency_set;
	zest_size allocated_before = metal_allocated_size();
	NSUInteger residency_before = [residency_set allocationCount];
	int allocations_before = device->memory_allocation_count;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	int failed = context == NULL;
	if (context) {
		const zest_uint categories[3] = { ZEST_ARENA_CATEGORY_GPU_BUFFERS, ZEST_ARENA_CATEGORY_CPU_BUFFERS, ZEST_ARENA_CATEGORY_IMAGE_BASE };
		const zest_size backing_size = 1024 * 1024;
		NSUInteger residency_with_context = [residency_set allocationCount];
		int allocations_with_context = device->memory_allocation_count;
		for (int category_index = 0; category_index != 3; ++category_index) {
			zest_device_memory_pool backing = device->platform->create_arena_backing(device, context, categories[category_index], backing_size);
			if (!backing) {
				failed = 1;
				continue;
			}
			zest_mtl_device_memory_pool_backend_t *backend = ZEST_MTL_BACKEND(device_memory_pool, backing);
			failed |= backing->size != backing_size;
			failed |= [residency_set allocationCount] != residency_with_context + 1;
			failed |= device->memory_allocation_count != allocations_with_context + 1;
			if (categories[category_index] == ZEST_ARENA_CATEGORY_IMAGE_BASE) {
				failed |= backend->heap == nil || backend->buffer != nil || backing->mapped != NULL;
				failed |= backend->heap && ([backend->heap size] < backing_size || [backend->heap type] != MTLHeapTypePlacement || ![residency_set containsAllocation:backend->heap]);
			} else {
				zest_bool host_visible = categories[category_index] == ZEST_ARENA_CATEGORY_CPU_BUFFERS;
				failed |= backend->buffer == nil || backend->heap != nil || backing->alignment != ZEST_BUFFER_OFFSET_GRANULARITY;
				if (backend->buffer) {
					failed |= [backend->buffer length] != backing_size || ![residency_set containsAllocation:backend->buffer];
					failed |= [backend->buffer storageMode] != (host_visible ? MTLStorageModeShared : MTLStorageModePrivate);
					//A transient buffer is a proxy into the backing, so the buffer helpers reach the backing's buffer at the proxy's offset
					zest_buffer_t proxy = {};
					proxy.memory_pool = backing;
					proxy.memory_offset = 256;
					failed |= zest__mtl_buffer_object(&proxy) != backend->buffer;
					failed |= zest__mtl_buffer_address(&proxy) != (zest_u64)[backend->buffer gpuAddress] + 256;
				}
				if (host_visible && backing->mapped) {
					unsigned char *bytes = (unsigned char *)backing->mapped;
					for (zest_size byte = 0; byte != backing_size; ++byte) bytes[byte] = (unsigned char)(byte * 7 + 3);
					for (zest_size byte = 0; byte != backing_size; ++byte) failed |= bytes[byte] != (unsigned char)(byte * 7 + 3);
				} else {
					failed |= host_visible || backing->mapped != NULL;
				}
			}
			device->platform->free_arena_backing(device, context, backing);
			if ([residency_set allocationCount] != residency_with_context || device->memory_allocation_count != allocations_with_context) {
				ZEST_PRINT("Freeing the arena backing of category %u left %lu allocations in the residency set and a count of %i, where there were %lu and %i.", categories[category_index],
					(unsigned long)[residency_set allocationCount], device->memory_allocation_count, (unsigned long)residency_with_context, allocations_with_context);
				failed = 1;
			}
		}
		zest_DestroyContext(context);
	}
	failed |= [residency_set allocationCount] != residency_before || device->memory_allocation_count != allocations_before;
	if (metal_allocated_size() != allocated_before) {
		ZEST_PRINT("Metal's allocated size went from %llu to %llu across three arena backings and their context.", (zest_ull)allocated_before, (zest_ull)metal_allocated_size());
		failed = 1;
	}
	return failed | finish_metal_device(device);
}

//Metal lets go of what a finished command buffer held on a thread of its own, so a count that should fall to one owner is given up to 200 ms to
static NSUInteger metal_settled_owner_count(id object) {
	for (int attempt = 0; attempt != 200 && [object retainCount] != 1; ++attempt) {
		usleep(1000);
	}
	return [object retainCount];
}

typedef struct MetalTransientCapture {
	const char *name;
	id<MTLTexture> texture;		//Retained here when held, as Zest retires a transient once its graph has run
	zest_bool hold;
	zest_bool reached;
} MetalTransientCapture;

static void metal_capture_transient(const zest_command_list command_list, void *user_data) {
	MetalTransientCapture *capture = (MetalTransientCapture *)user_data;
	zest_resource_node node = zest_GetPassInputResource(command_list, capture->name);
	if (!node || !node->image.backend) return;
	capture->reached = ZEST_TRUE;
	if (capture->hold && !capture->texture) {
		capture->texture = [ZEST_MTL_BACKEND(image, &node->image)->texture retain];
	}
}

static zest_uint metal_image_arena_count(zest_context context) {
	zest_uint count = 0;
	zest_vec_foreach(arena_index, context->transient_arenas) {
		count += context->transient_arenas[arena_index]->category >= ZEST_ARENA_CATEGORY_IMAGE_BASE;
	}
	return count;
}

//A texture hands its heap back autoreleased, and one left in a pool that never drains would keep the arena's memory allocated
static zest_bool metal_texture_is_in_image_arena(zest_context context, id<MTLTexture> texture) {
	zest_bool found = ZEST_FALSE;
	@autoreleasepool {
		id<MTLHeap> heap = [texture heap];
		zest_vec_foreach(arena_index, context->transient_arenas) {
			zest_transient_arena_t *arena = context->transient_arenas[arena_index];
			if (arena->category < ZEST_ARENA_CATEGORY_IMAGE_BASE) continue;
			zest_ForEachFrameInFlight(frame_in_flight) {
				found |= arena->backing[frame_in_flight] && ZEST_MTL_BACKEND(device_memory_pool, arena->backing[frame_in_flight])->heap == heap;
			}
		}
	}
	return found;
}

static int metal_report_count(zest_device device, const char *text) {
	int count = 0;
	zest_map_foreach(report_index, device->reports) {
		zest_report_t *report = &device->reports.data[report_index];
		if (report->message.str && strstr(report->message.str, text)) count += report->count;
	}
	return count;
}

static void metal_copy_transient_pass(const zest_command_list command_list, void *user_data) {
	(void)user_data;
	zest_resource_node transient = zest_GetPassInputResource(command_list, "Transient Target");
	zest_resource_node copy_target = zest_GetPassOutputResource(command_list, "Copy Target");
	zest_cmd_CopyImageMip(command_list, transient, copy_target, 0, zest_shader_fragment_stage);
}

//Clears a 64x64 transient to blue, copies it to the imported copy target in a transfer pass, then clears the imported target to orange in a pass that reads the transient.
//Hands back the status of the flush and, when it's asked for, the transient's texture, retained. Without that only Zest holds the transient.
static zest_semaphore_status run_metal_transient_graph(zest_context context, zest_image target, zest_image copy_target, zest_resource_usage_hint usage_hints, id<MTLTexture> *out_texture) {
	MetalTransientCapture capture = { "Transient Target", nil, out_texture != NULL, ZEST_FALSE };
	if (out_texture) *out_texture = nil;
	if (!zest_BeginCommandGraph(context, "Transient Images", 0)) return zest_semaphore_status_error;
	zest_image_resource_info_t transient_info = { zest_format_r8g8b8a8_unorm };
	transient_info.usage_hints = usage_hints | zest_resource_usage_hint_copy_src;
	transient_info.width = 64;
	transient_info.height = 64;
	zest_resource_node transient = zest_AddTransientImageResource("Transient Target", &transient_info);
	zest_resource_node target_node = zest_ImportImageResource("Target", target, 0);
	zest_resource_node copy_node = zest_ImportImageResource("Copy Target", copy_target, 0);
	zest_SetResourceClearColor(transient, 0.2f, 0.4f, 0.6f, 1.0f);
	zest_SetResourceClearColor(target_node, 0.6f, 0.4f, 0.2f, 1.0f);

	zest_BeginRenderPass("Write Transient");
	zest_ConnectOutput(transient);
	zest_SetPassTask(zest_EmptyRenderPass, 0);
	zest_EndPass();

	zest_BeginTransferPass("Copy Transient");
	zest_ConnectInput(transient);
	zest_ConnectOutput(copy_node);
	zest_SetPassTask(metal_copy_transient_pass, 0);
	zest_EndPass();

	zest_BeginRenderPass("Read Transient");
	zest_ConnectInput(transient);
	zest_ConnectOutput(target_node);
	zest_SetPassTask(metal_capture_transient, &capture);
	zest_EndPass();

	zest_frame_graph frame_graph = zest_EndFrameGraph();
	if (!frame_graph || zest_GetFrameGraphResult(frame_graph) != zest_fgs_success) return zest_semaphore_status_error;
	zest_semaphore_status status = zest_FlushFrameGraphAndWait(frame_graph);
	if (out_texture) *out_texture = capture.texture;
	//A flush that worked without reaching the pass that reads the transient is not what the caller takes it for
	return status == zest_semaphore_status_success && !capture.reached ? zest_semaphore_status_error : status;
}

//Three transients in two formats. The first two are alive together, and the third starts once the first is done with, so it can take the first's bytes.
static int run_metal_transient_alias_graph(zest_context context, zest_image target) {
	zest_device device = context->device;
	MetalTransientCapture captures[3] = { { "First Transient", nil, ZEST_TRUE, ZEST_FALSE }, { "Second Transient", nil, ZEST_TRUE, ZEST_FALSE }, { "Third Transient", nil, ZEST_TRUE, ZEST_FALSE } };
	if (!zest_BeginCommandGraph(context, "Transient Aliasing", 0)) return 1;
	zest_image_resource_info_t colour_info = { zest_format_r8g8b8a8_unorm };
	colour_info.width = 64;
	colour_info.height = 64;
	zest_image_resource_info_t float_info = { zest_format_r16g16b16a16_sfloat };
	float_info.width = 32;
	float_info.height = 32;
	zest_resource_node first = zest_AddTransientImageResource("First Transient", &colour_info);
	zest_resource_node second = zest_AddTransientImageResource("Second Transient", &float_info);
	zest_resource_node third = zest_AddTransientImageResource("Third Transient", &colour_info);
	zest_resource_node target_node = zest_ImportImageResource("Target", target, 0);
	zest_SetResourceClearColor(first, 0.2f, 0.4f, 0.6f, 1.0f);
	zest_SetResourceClearColor(second, 1.0f, 0.0f, 0.5f, 1.0f);
	zest_SetResourceClearColor(third, 0.6f, 0.4f, 0.2f, 1.0f);
	zest_SetResourceClearColor(target_node, 0.2f, 0.4f, 0.6f, 1.0f);

	zest_BeginRenderPass("First Pass");
	zest_ConnectOutput(first);
	zest_SetPassTask(zest_EmptyRenderPass, 0);
	zest_EndPass();

	zest_BeginRenderPass("Second Pass");
	zest_ConnectInput(first);
	zest_ConnectOutput(second);
	zest_SetPassTask(metal_capture_transient, &captures[0]);
	zest_EndPass();

	zest_BeginRenderPass("Third Pass");
	zest_ConnectInput(second);
	zest_ConnectOutput(third);
	zest_SetPassTask(metal_capture_transient, &captures[1]);
	zest_EndPass();

	zest_BeginRenderPass("Fourth Pass");
	zest_ConnectInput(third);
	zest_ConnectOutput(target_node);
	zest_SetPassTask(metal_capture_transient, &captures[2]);
	zest_EndPass();

	zest_frame_graph frame_graph = zest_EndFrameGraph();
	int failed = frame_graph == NULL || zest_GetFrameGraphResult(frame_graph) != zest_fgs_success;
	failed |= frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
	failed |= zest_GetValidationErrorCount(device) != 0;
	id<MTLTexture> first_texture = captures[0].texture;
	id<MTLTexture> second_texture = captures[1].texture;
	id<MTLTexture> third_texture = captures[2].texture;
	if (!first_texture || !second_texture || !third_texture) {
		ZEST_PRINT("The aliasing graph didn't reach all three of its transients.");
		failed = 1;
	} else {
		NSUInteger first_offset = [first_texture heapOffset];
		NSUInteger second_offset = [second_texture heapOffset];
		NSUInteger third_offset = [third_texture heapOffset];
		zest_bool one_heap = ZEST_FALSE;
		@autoreleasepool {
			id<MTLHeap> heap = [first_texture heap];
			one_heap = heap != nil && [second_texture heap] == heap && [third_texture heap] == heap;
		}
		//The first two are in use in the same pass, so neither may start inside the other
		zest_bool apart = first_offset + [first_texture allocatedSize] <= second_offset || second_offset + [second_texture allocatedSize] <= first_offset;
		if (!one_heap || !apart || third_offset != first_offset || [second_texture pixelFormat] != MTLPixelFormatRGBA16Float) {
			ZEST_PRINT("The aliasing graph placed its transients at offsets %lu, %lu and %lu with sizes %lu, %lu and %lu, %s. The first two must be apart and the third must take the first's place.",
				(unsigned long)first_offset, (unsigned long)second_offset, (unsigned long)third_offset, (unsigned long)[first_texture allocatedSize], (unsigned long)[second_texture allocatedSize],
				(unsigned long)[third_texture allocatedSize], one_heap ? "in one heap" : "in more than one heap");
			failed = 1;
		}
		//Each still holds what its own pass cleared it to. The first's bytes went to the third.
		unsigned short halves[32 * 32 * 4];
		memset(halves, 0, sizeof(halves));
		failed |= !metal_read_native_texture(device, second_texture, 0, 0, 32, 32, 8, halves);
		for (zest_uint pixel = 0; pixel != 32 * 32; ++pixel) {
			failed |= halves[pixel * 4] != 0x3C00 || halves[pixel * 4 + 1] != 0x0000 || halves[pixel * 4 + 2] != 0x3800 || halves[pixel * 4 + 3] != 0x3C00;
		}
		zest_uint pixels[64 * 64];
		memset(pixels, 0, sizeof(pixels));
		failed |= !metal_read_native_texture(device, third_texture, 0, 0, 64, 64, 4, pixels);
		failed |= !metal_pixels_match(pixels, 64 * 64, metal_orange_expected);
	}
	for (int index = 0; index != 3; ++index) {
		[captures[index].texture release];
	}
	zest_uint target_pixels[16 * 16];
	memset(target_pixels, 0, sizeof(target_pixels));
	failed |= !zest_CopyImageToBitmap(device, target, target_pixels);
	failed |= !metal_pixels_match(target_pixels, 16 * 16, metal_blue_expected);
	return failed;
}

int test_metal_transient_images(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	id<MTLResidencySet> residency_set = ZEST_MTL_BACKEND(device, device)->residency_set;
	zest_image_handle target_handle = metal_create_clear_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment);
	zest_image target = zest_GetImage(target_handle);
	if (!target) {
		zest_DestroyDevice(device);
		return 1;
	}
	zest_image_info_t copy_info = zest_CreateImageInfo(64, 64);
	copy_info.format = zest_format_r8g8b8a8_unorm;
	copy_info.flags = zest_image_preset_texture | zest_image_flag_transfer_src;
	zest_image_handle copy_handle = zest_CreateImage(device, &copy_info);
	zest_image copy_target = zest_GetImage(copy_handle);
	if (!copy_target) {
		zest_DestroyDevice(device);
		return 1;
	}
	//The first upload and the first readback each make a staging pool of the device's, which stays, so the baselines are taken after one of each
	static zest_uint copied_pixels[64 * 64];
	zest_uint first_pixels[16 * 16];
	memset(copied_pixels, 0, sizeof(copied_pixels));
	int failed = !zest_CopyBitmapToImage(device, copied_pixels, sizeof(copied_pixels), copy_target, 64, 64);
	failed |= !zest_CopyImageToBitmap(device, target, first_pixels);
	zest_WaitForIdleDevice(device);
	zest_size allocated_before = metal_allocated_size();
	NSUInteger residency_before = [residency_set allocationCount];
	int allocations_before = device->memory_allocation_count;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	failed |= context == NULL;
	if (!failed) {
		zest_size allocated_sizes[6];
		NSUInteger residency_counts[6];
		for (int run = 0; run != 6; ++run) {
			id<MTLTexture> texture = nil;
			//The copy target is emptied first, so what it holds afterwards is what this run's transfer pass copied out of the transient
			memset(copied_pixels, 0, sizeof(copied_pixels));
			failed |= !zest_CopyBitmapToImage(device, copied_pixels, sizeof(copied_pixels), copy_target, 64, 64);
			failed |= run_metal_transient_graph(context, target, copy_target, zest_resource_usage_hint_none, &texture) != zest_semaphore_status_success;
			failed |= zest_GetValidationErrorCount(device) != 0;
			failed |= !zest_CopyImageToBitmap(device, copy_target, copied_pixels);
			if (!metal_pixels_match(copied_pixels, 64 * 64, metal_blue_expected)) {
				ZEST_PRINT("Run %i copied its transient to an image that doesn't hold the transient's clear colour.", run);
				failed = 1;
			}
			if (!texture) {
				ZEST_PRINT("Run %i of the transient graph never reached its transient.", run);
				failed = 1;
			} else {
				//The graph has run and been waited for, so Zest has let go of the transient and its view
				if ([texture retainCount] != 1) {
					ZEST_PRINT("The transient of run %i still has %lu owners after its graph was flushed, where only this test should hold it.", run, (unsigned long)[texture retainCount]);
					failed = 1;
				}
				failed |= [texture width] != 64 || [texture height] != 64 || [texture pixelFormat] != MTLPixelFormatRGBA8Unorm;
				failed |= ([texture usage] & MTLTextureUsageRenderTarget) == 0 || !metal_texture_is_in_image_arena(context, texture);
				zest_uint pixels[64 * 64];
				memset(pixels, 0, sizeof(pixels));
				failed |= !metal_read_native_texture(device, texture, 0, 0, 64, 64, 4, pixels);
				failed |= !metal_pixels_match(pixels, 64 * 64, metal_blue_expected);
				[texture release];
			}
			zest_uint target_pixels[16 * 16];
			memset(target_pixels, 0, sizeof(target_pixels));
			failed |= !zest_CopyImageToBitmap(device, target, target_pixels);
			failed |= !metal_pixels_match(target_pixels, 16 * 16, metal_orange_expected);
			zest_WaitForIdleDevice(device);
			allocated_sizes[run] = metal_allocated_size();
			residency_counts[run] = [residency_set allocationCount];
		}
		//Every run places a new texture in the arena the first runs made
		if (allocated_sizes[5] != allocated_sizes[3] || residency_counts[5] != residency_counts[3]) {
			ZEST_PRINT("Metal's allocated size went from %llu to %llu and the residency set from %lu to %lu allocations between the fourth and sixth run of one transient graph.",
				(zest_ull)allocated_sizes[3], (zest_ull)allocated_sizes[5], (unsigned long)residency_counts[3], (unsigned long)residency_counts[5]);
			failed = 1;
		}
		//With only Zest holding it, each transient is destroyed as soon as its flush has waited, which Metal allows once the batch that used it has completed
		for (int run = 0; run != 8; ++run) {
			failed |= run_metal_transient_graph(context, target, copy_target, zest_resource_usage_hint_none, NULL) != zest_semaphore_status_success;
		}
		if (zest_GetValidationErrorCount(device) != 0) {
			ZEST_PRINT("Eight transient graphs that only Zest held the transients of raised %u validation errors.", zest_GetValidationErrorCount(device));
			failed = 1;
		}
		//Every batch is committed with the ordering event it signals last, and gives it up when the batch is released
		id<MTLSharedEvent> ordering_event = ZEST_MTL_BACKEND(execution_timeline, &context->queue_ordering_timelines[context->graphics_queue_index])->event;
		if (metal_settled_owner_count(ordering_event) != 1) {
			ZEST_PRINT("The graphics queue's ordering event has %lu owners once every batch has been released, where only its timeline should hold it.", (unsigned long)[ordering_event retainCount]);
			failed = 1;
		}
		failed |= run_metal_transient_alias_graph(context, target);
		if (metal_image_arena_count(context) != 1) {
			ZEST_PRINT("The context has %u image arenas, where every transient image should share one.", metal_image_arena_count(context));
			failed = 1;
		}
		//A transient image can't be host visible, so the graph fails where it places its transients and says why once
		id<MTLTexture> refused_texture = nil;
		failed |= run_metal_transient_graph(context, target, copy_target, zest_resource_usage_hint_cpu_transfer, &refused_texture) != zest_semaphore_status_error;
		failed |= refused_texture != nil;
		[refused_texture release];
		if (metal_report_count(device, "zest_resource_usage_hint_cpu_transfer") != 1) {
			ZEST_PRINT("A host visible transient image was reported %i times, where it should be reported once.", metal_report_count(device, "zest_resource_usage_hint_cpu_transfer"));
			failed = 1;
		}
		failed |= zest_GetValidationErrorCount(device) != 0;
		//The refused graph left nothing behind that stops the next one
		failed |= run_metal_transient_graph(context, target, copy_target, zest_resource_usage_hint_none, NULL) != zest_semaphore_status_success;
	}
	zest_WaitForIdleDevice(device);
	failed |= zest_GetValidationErrorCount(device) != 0;
	if (context) {
		zest_DestroyContext(context);
	}
	if ([residency_set allocationCount] != residency_before || device->memory_allocation_count != allocations_before) {
		ZEST_PRINT("After the context went the residency set holds %lu allocations and the device counts %i, where there were %lu and %i.", (unsigned long)[residency_set allocationCount], device->memory_allocation_count, (unsigned long)residency_before, allocations_before);
		failed = 1;
	}
	//Metal can keep 256 KB of its own from the first aliasing graph in a process, even past the device, so only a second context's growth fails
	zest_size allocated_after_first = metal_allocated_size();
	zest_context second_context = zest_CreateHeadlessContext(device, &info);
	failed |= second_context == NULL;
	if (second_context) {
		failed |= run_metal_transient_graph(second_context, target, copy_target, zest_resource_usage_hint_none, NULL) != zest_semaphore_status_success;
		failed |= run_metal_transient_alias_graph(second_context, target);
		zest_WaitForIdleDevice(device);
		zest_DestroyContext(second_context);
	}
	if ([residency_set allocationCount] != residency_before || device->memory_allocation_count != allocations_before || metal_allocated_size() != allocated_after_first) {
		ZEST_PRINT("After a second context went the residency set holds %lu allocations, the device counts %i and Metal has %llu bytes allocated, where there were %lu, %i and %llu after the first. Before the first there were %llu bytes.",
			(unsigned long)[residency_set allocationCount], device->memory_allocation_count, (zest_ull)metal_allocated_size(), (unsigned long)residency_before, allocations_before, (zest_ull)allocated_after_first, (zest_ull)allocated_before);
		failed = 1;
	}
	zest_FreeImageNow(target_handle);
	zest_FreeImageNow(copy_handle);
	return failed | finish_metal_device(device);
}

typedef struct MetalBufferCopyTask {
	zest_buffer source;
	zest_buffer target;
	zest_size size;
	zest_buffer_uploader_t uploader;
	zest_bool uploaded;					//What zest_cmd_UploadBuffer returned
	zest_uint copies_left;				//The uploader's list once the pass has run, read there as the list is the graph's memory
	zest_bool uploader_was_reset;
} MetalBufferCopyTask;

static void metal_copy_buffer_pass(const zest_command_list command_list, void *user_data) {
	MetalBufferCopyTask *task = (MetalBufferCopyTask *)user_data;
	zest_cmd_CopyBuffer(command_list, task->source, task->target, task->size);
}

static void metal_upload_buffer_pass(const zest_command_list command_list, void *user_data) {
	MetalBufferCopyTask *task = (MetalBufferCopyTask *)user_data;
	task->uploaded = zest_cmd_UploadBuffer(command_list, &task->uploader);
	task->copies_left = zest_vec_size(task->uploader.buffer_copies);
	task->uploader_was_reset = task->uploader.flags == 0 && task->uploader.source_buffer == NULL && task->uploader.target_buffer == NULL;
}

//Both copy commands from inside a render pass, where neither can record
static void metal_copy_in_render_pass(const zest_command_list command_list, void *user_data) {
	metal_copy_buffer_pass(command_list, user_data);
	metal_upload_buffer_pass(command_list, user_data);
}

static void metal_fill_buffer_bytes(zest_buffer buffer, zest_uint seed) {
	unsigned char *bytes = (unsigned char *)zest_BufferData(buffer);
	for (zest_size byte = 0; byte != buffer->size; ++byte) bytes[byte] = (unsigned char)(byte * 7 + seed);
}

//Brings a device local buffer's bytes back through an immediate copy
static zest_bool metal_read_device_buffer(zest_device device, zest_buffer device_buffer, zest_buffer readback, zest_size size) {
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	if (!queue) return ZEST_FALSE;
	zest_bool recorded = zest_imm_CopyBuffer(queue, device_buffer, readback, size);
	return zest_imm_EndCommandBuffer(queue) && recorded;
}

//How many batches the graph's passes were put in, and the most passes any one of them holds
static zest_uint metal_graph_batch_count(zest_frame_graph frame_graph, zest_uint *out_most_passes) {
	zest_uint batch_count = 0;
	*out_most_passes = 0;
	zest_vec_foreach(submission_index, frame_graph->submissions) {
		for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
			zest_submission_batch_t *batch = &frame_graph->submissions[submission_index].batches[queue_index];
			if (!batch->magic) continue;
			batch_count++;
			*out_most_passes = ZEST__MAX(*out_most_passes, zest_vec_size(batch->pass_indices));
		}
	}
	return batch_count;
}

int test_metal_frame_graph_buffer_copies(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	zest_buffer_info_t upload_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_cpu_to_gpu);
	zest_buffer_info_t readback_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_gpu_to_cpu);
	zest_buffer_info_t device_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	//The first buffer of each kind sits at the start of its pool, so the ones the test copies between all have an offset to get wrong
	zest_buffer blockers[3] = { zest_CreateBuffer(device, 1024, &upload_info), zest_CreateBuffer(device, 1024, &readback_info), zest_CreateBuffer(device, 1024, &device_info) };
	zest_buffer source = zest_CreateBuffer(device, 1024, &upload_info);
	zest_buffer readback = zest_CreateBuffer(device, 1024, &readback_info);
	zest_buffer device_buffer = zest_CreateBuffer(device, 1024, &device_info);
	zest_image_handle target_handle = metal_create_clear_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment);
	zest_image target = zest_GetImage(target_handle);
	int failed = context == NULL || !blockers[0] || !blockers[1] || !blockers[2] || !source || !readback || !device_buffer || target == NULL;
	if (!failed) {
		failed |= source->memory_offset == 0 || readback->memory_offset == 0 || device_buffer->memory_offset == 0;
		memset(zest_BufferData(blockers[0]), 0, blockers[0]->size);
		metal_fill_buffer_bytes(source, 3);

		//Up to the device buffer in one transfer pass and back down in the next, which opens an encoder of its own. 510 bytes is no multiple of 4.
		MetalBufferCopyTask up = {};
		up.source = source;
		up.target = device_buffer;
		up.size = 510;
		MetalBufferCopyTask down = {};
		down.source = device_buffer;
		down.target = readback;
		down.size = 510;
		memset(zest_BufferData(readback), 0xEE, readback->size);
		failed |= !zest_BeginCommandGraph(context, "Buffer Copies", 0);
		zest_resource_node device_node = zest_ImportBufferResource("Device Buffer", device_buffer, 0);
		zest_BeginTransferPass("Copy Up");
		zest_ConnectOutput(device_node);
		zest_SetPassTask(metal_copy_buffer_pass, &up);
		zest_EndPass();
		zest_BeginTransferPass("Copy Down");
		zest_ConnectInput(device_node);
		zest_DoNotCull();
		zest_SetPassTask(metal_copy_buffer_pass, &down);
		zest_EndPass();
		zest_frame_graph frame_graph = zest_EndFrameGraph();
		failed |= frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
		const unsigned char *source_bytes = (const unsigned char *)zest_BufferData(source);
		const unsigned char *readback_bytes = (const unsigned char *)zest_BufferData(readback);
		if (memcmp(readback_bytes, source_bytes, 510) != 0 || readback_bytes[510] != 0xEE || readback_bytes[511] != 0xEE) {
			ZEST_PRINT("A buffer copied up to a device buffer and back down came back different, or the copy ran past its 510 bytes.");
			failed = 1;
		}
		failed |= zest_GetValidationErrorCount(device) != 0;

		//Three regions through an uploader, each at its own offset in both buffers
		const zest_size regions[3][3] = { { 0, 0, 96 }, { 128, 256, 100 }, { 512, 700, 64 } };
		MetalBufferCopyTask upload = {};
		memset(zest_BufferData(readback), 0xEE, readback->size);
		failed |= !zest_BeginCommandGraph(context, "Buffer Upload", 0);
		zest_AddCopyCommand(context, &upload.uploader, source, readback, (zest_size)regions[0][2]);
		for (int region = 1; region != 3; ++region) {
			zest_buffer_copy_t copy = { source->memory_offset + regions[region][0], readback->memory_offset + regions[region][1], regions[region][2] };
			zest_vec_linear_push(zest__frame_graph_builder->allocator, upload.uploader.buffer_copies, copy);
		}
		zest_BeginTransferPass("Upload");
		zest_DoNotCull();
		zest_SetPassTask(metal_upload_buffer_pass, &upload);
		zest_EndPass();
		frame_graph = zest_EndFrameGraph();
		failed |= frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
		failed |= !upload.uploaded || upload.copies_left != 0 || !upload.uploader_was_reset;
		zest_size checked = 0;
		for (int region = 0; region != 3; ++region) {
			//What lies before each region was left alone
			for (zest_size byte = checked; byte != regions[region][1]; ++byte) failed |= readback_bytes[byte] != 0xEE;
			failed |= memcmp(readback_bytes + regions[region][1], source_bytes + regions[region][0], (size_t)regions[region][2]) != 0;
			checked = regions[region][1] + regions[region][2];
		}
		for (zest_size byte = checked; byte != 1024; ++byte) failed |= readback_bytes[byte] != 0xEE;
		failed |= zest_GetValidationErrorCount(device) != 0;

		//A transfer pass that copies and the render pass after it share a batch, so the copy's encoder has to be ended before the pass opens its own
		metal_fill_buffer_bytes(source, 91);
		up.size = 512;
		failed |= !zest_BeginCommandGraph(context, "Copy Then Clear", 0);
		device_node = zest_ImportBufferResource("Device Buffer", device_buffer, 0);
		zest_resource_node target_node = zest_ImportImageResource("Target", target, 0);
		zest_SetResourceClearColor(target_node, 0.6f, 0.4f, 0.2f, 1.0f);
		zest_BeginTransferPass("Copy Up");
		zest_ConnectOutput(device_node);
		zest_SetPassTask(metal_copy_buffer_pass, &up);
		zest_EndPass();
		zest_BeginRenderPass("Clear Pass");
		zest_ConnectInput(device_node);
		zest_ConnectOutput(target_node);
		zest_SetPassTask(zest_EmptyRenderPass, 0);
		zest_EndPass();
		frame_graph = zest_EndFrameGraph();
		zest_uint most_passes = 0;
		zest_uint batch_count = frame_graph ? metal_graph_batch_count(frame_graph, &most_passes) : 0;
		if (batch_count != 1 || most_passes != 2) {
			ZEST_PRINT("The copy and the render pass after it went into %u batches with at most %u passes, where the test needs both in one.", batch_count, most_passes);
			failed = 1;
		}
		failed |= frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
		memset(zest_BufferData(readback), 0xEE, readback->size);
		failed |= !metal_read_device_buffer(device, device_buffer, readback, 512);
		failed |= memcmp(readback_bytes, source_bytes, 512) != 0;
		zest_uint pixels[16 * 16];
		memset(pixels, 0, sizeof(pixels));
		failed |= !zest_CopyImageToBitmap(device, target, pixels);
		failed |= !metal_pixels_match(pixels, 16 * 16, metal_orange_expected);
		failed |= zest_GetValidationErrorCount(device) != 0;

		//Neither copy command can record with a render pass open. Both say so with the one message, and leave their buffers and the uploader alone.
		MetalBufferCopyTask refused = {};
		refused.source = source;
		refused.target = readback;
		refused.size = 256;
		memset(zest_BufferData(readback), 0xEE, readback->size);
		failed |= !zest_BeginCommandGraph(context, "Copy In Render Pass", 0);
		zest_AddCopyCommand(context, &refused.uploader, source, readback, 128);
		target_node = zest_ImportImageResource("Target", target, 0);
		zest_SetResourceClearColor(target_node, 0.2f, 0.4f, 0.6f, 1.0f);
		zest_BeginRenderPass("Clear Pass");
		zest_ConnectOutput(target_node);
		zest_SetPassTask(metal_copy_in_render_pass, &refused);
		zest_EndPass();
		frame_graph = zest_EndFrameGraph();
		failed |= frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
		if (zest_GetValidationErrorCount(device) != 1) {
			ZEST_PRINT("Two copy commands inside a render pass raised %u validation errors, where they share one message.", zest_GetValidationErrorCount(device));
			failed = 1;
		}
		zest_ResetValidationErrors(device);
		failed |= refused.uploaded || refused.copies_left != 1 || refused.uploader_was_reset;
		for (zest_size byte = 0; byte != 1024; ++byte) failed |= readback_bytes[byte] != 0xEE;
		memset(pixels, 0, sizeof(pixels));
		failed |= !zest_CopyImageToBitmap(device, target, pixels);
		failed |= !metal_pixels_match(pixels, 16 * 16, metal_blue_expected);
	}
	zest_WaitForIdleDevice(device);
	failed |= zest_GetValidationErrorCount(device) != 0;
	for (int index = 0; index != 3; ++index) {
		zest_FreeBufferNow(blockers[index]);
	}
	zest_FreeBufferNow(source);
	zest_FreeBufferNow(readback);
	zest_FreeBufferNow(device_buffer);
	zest_FreeImageNow(target_handle);
	if (context) {
		zest_DestroyContext(context);
	}
	return failed | finish_metal_device(device);
}

//One 16x16 image with 3 mips and 4 bytes a pixel
static zest_image_handle metal_create_mip_image(zest_device device, zest_format format) {
	zest_image_info_t image_info = zest_CreateImageInfo(16, 16);
	image_info.format = format;
	image_info.mip_levels = 3;
	image_info.flags = zest_image_preset_texture | zest_image_flag_transfer_src;
	return zest_CreateImage(device, &image_info);
}

static void metal_mip_pattern(unsigned char *bytes, zest_uint mip, zest_uint seed) {
	zest_uint mip_size = 16u >> mip;
	for (zest_uint byte = 0; byte != mip_size * mip_size * 4; ++byte) bytes[byte] = (unsigned char)(byte * 3 + mip * 40 + seed);
}

//Gives every mip its own bytes, which differ from image to image with the seed
static zest_bool metal_upload_mip_patterns(zest_device device, zest_image image, zest_uint seed) {
	unsigned char bytes[16 * 16 * 4 + 8 * 8 * 4 + 4 * 4 * 4];
	zest_buffer_image_copy_t regions[3];
	zest_size offset = 0;
	for (zest_uint mip = 0; mip != 3; ++mip) {
		zest_uint mip_size = 16u >> mip;
		metal_mip_pattern(bytes + offset, mip, seed);
		regions[mip] = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
		regions[mip].buffer_offset = offset;
		regions[mip].image_aspect = zest_image_aspect_color_bit;
		regions[mip].mip_level = mip;
		regions[mip].layer_count = 1;
		regions[mip].image_extent.width = mip_size;
		regions[mip].image_extent.height = mip_size;
		regions[mip].image_extent.depth = 1;
		offset += (zest_size)mip_size * mip_size * 4;
	}
	zest_buffer staging_buffer = zest_CreateDedicatedStagingBuffer(device, sizeof(bytes), bytes);
	if (!staging_buffer) return ZEST_FALSE;
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	zest_bool recorded = queue != NULL && zest_imm_CopyBufferRegionsToImage(queue, regions, 3, staging_buffer, image);
	recorded = recorded && zest_imm_TransitionImage(queue, image, zest_resource_state_shader_read, 0, 3, 0, 1);
	zest_bool finished = queue && zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(staging_buffer);
	return recorded && finished;
}

//Whether one mip of the image holds the pattern the seed gives that mip
static zest_bool metal_mip_holds_pattern(zest_device device, zest_image image, zest_uint mip, zest_uint seed) {
	unsigned char read_bytes[16 * 16 * 4];
	unsigned char expected_bytes[16 * 16 * 4];
	zest_uint mip_size = 16u >> mip;
	memset(read_bytes, 0, sizeof(read_bytes));
	metal_mip_pattern(expected_bytes, mip, seed);
	if (!metal_read_texture(device, image, mip, 0, mip_size, mip_size, 4, read_bytes)) return ZEST_FALSE;
	return memcmp(read_bytes, expected_bytes, mip_size * mip_size * 4) == 0;
}

typedef struct MetalMipCommandTask {
	zest_bool blit;
	zest_uint mip;
	int run_count;
	zest_buffer copy_source;			//When set, 256 bytes are copied from it before the mip command and again after
	zest_buffer copy_targets[2];
} MetalMipCommandTask;

static void metal_mip_command_pass(const zest_command_list command_list, void *user_data) {
	MetalMipCommandTask *task = (MetalMipCommandTask *)user_data;
	zest_resource_node source = zest_GetPassInputResource(command_list, "Mip Source");
	zest_resource_node destination = zest_GetPassOutputResource(command_list, "Mip Destination");
	task->run_count++;
	if (task->copy_source) {
		zest_cmd_CopyBuffer(command_list, task->copy_source, task->copy_targets[0], 256);
	}
	if (task->blit) {
		zest_cmd_BlitImageMip(command_list, source, destination, task->mip, zest_shader_fragment_stage);
	} else {
		zest_cmd_CopyImageMip(command_list, source, destination, task->mip, zest_shader_fragment_stage);
	}
	if (task->copy_source) {
		zest_cmd_CopyBuffer(command_list, task->copy_source, task->copy_targets[1], 256);
	}
}

//One transfer pass that runs the task's mip command from the source into the destination
static int run_metal_mip_task_graph(zest_context context, zest_image source, zest_image destination, MetalMipCommandTask *task) {
	if (!zest_BeginCommandGraph(context, "Image Mip Commands", 0)) return 1;
	zest_resource_node source_node = zest_ImportImageResource("Mip Source", source, 0);
	zest_resource_node destination_node = zest_ImportImageResource("Mip Destination", destination, 0);
	zest_BeginTransferPass("Mip Pass");
	zest_ConnectInput(source_node);
	zest_ConnectOutput(destination_node);
	zest_SetPassTask(metal_mip_command_pass, task);
	zest_EndPass();
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	int failed = frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
	return failed | (task->run_count != 1);
}

//Copies, or blits, one mip of the source into the destination
static int run_metal_mip_command_graph(zest_context context, zest_image source, zest_image destination, zest_bool blit, zest_uint mip) {
	MetalMipCommandTask task = {};
	task.blit = blit;
	task.mip = mip;
	return run_metal_mip_task_graph(context, source, destination, &task);
}

//A blit of mip 1 from inside a render pass that draws to the destination
static int run_metal_mip_blit_in_render_pass(zest_context context, zest_image source, zest_image destination) {
	MetalMipCommandTask task = {};
	task.blit = ZEST_TRUE;
	task.mip = 1;
	if (!zest_BeginCommandGraph(context, "Mip Blit In Render Pass", 0)) return 1;
	zest_resource_node source_node = zest_ImportImageResource("Mip Source", source, 0);
	zest_resource_node destination_node = zest_ImportImageResource("Mip Destination", destination, 0);
	zest_BeginRenderPass("Mip Pass");
	zest_ConnectInput(source_node);
	zest_ConnectOutput(destination_node);
	zest_SetPassTask(metal_mip_command_pass, &task);
	zest_EndPass();
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	int failed = frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
	return failed | (task.run_count != 1);
}

static float metal_half_to_float(unsigned short half) {
	int exponent = (half >> 10) & 0x1F;
	float mantissa = (float)(half & 0x3FF);
	float magnitude = exponent ? ldexpf(1.f + mantissa / 1024.f, exponent - 15) : ldexpf(mantissa / 1024.f, -14);
	return (half & 0x8000) ? -magnitude : magnitude;
}

//Whether one mip of a 16x16 half float image holds the seed's pattern for that mip, each byte converted to a float
static zest_bool metal_mip_holds_converted_pattern(zest_device device, zest_image image, zest_uint mip, zest_uint seed) {
	unsigned short read_channels[16 * 16 * 4];
	unsigned char expected_bytes[16 * 16 * 4];
	zest_uint mip_size = 16u >> mip;
	memset(read_channels, 0, sizeof(read_channels));
	metal_mip_pattern(expected_bytes, mip, seed);
	if (!metal_read_texture(device, image, mip, 0, mip_size, mip_size, 8, read_channels)) return ZEST_FALSE;
	for (zest_uint channel = 0; channel != mip_size * mip_size * 4; ++channel) {
		if (fabsf(metal_half_to_float(read_channels[channel]) * 255.f - (float)expected_bytes[channel]) > 0.5f) return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

//Whether every pixel of one mip of a 16x16 half float image is the colour
static zest_bool metal_half_mip_is(zest_device device, zest_image image, zest_uint mip, zest_uint layer, const float expected[4]) {
	unsigned short read_channels[16 * 16 * 4];
	zest_uint mip_size = 16u >> mip;
	memset(read_channels, 0, sizeof(read_channels));
	if (!metal_read_texture(device, image, mip, layer, mip_size, mip_size, 8, read_channels)) return ZEST_FALSE;
	for (zest_uint channel = 0; channel != mip_size * mip_size * 4; ++channel) {
		if (fabsf(metal_half_to_float(read_channels[channel]) - expected[channel % 4]) > 0.005f) return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

static zest_image_handle metal_create_half_float_target(zest_device device, zest_uint layer_count) {
	zest_image_info_t image_info = zest_CreateImageInfo(16, 16);
	image_info.format = zest_format_r16g16b16a16_sfloat;
	image_info.mip_levels = 3;
	image_info.layer_count = layer_count;
	image_info.flags = zest_image_preset_color_attachment | zest_image_flag_transfer_src;
	return zest_CreateImage(device, &image_info);
}

int test_metal_image_mip_commands(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	const zest_uint source_seed = 11;
	const zest_uint untouched_seed = 97;
	zest_image_handle source_handle = metal_create_mip_image(device, zest_format_r8g8b8a8_unorm);
	zest_image_handle destination_handle = metal_create_mip_image(device, zest_format_r8g8b8a8_unorm);
	zest_image_handle srgb_handle = metal_create_mip_image(device, zest_format_r8g8b8a8_srgb);
	zest_image_handle float_handle = metal_create_mip_image(device, zest_format_r32_sfloat);
	zest_image_handle half_handle = metal_create_half_float_target(device, 1);
	zest_image_handle half_array_handle = metal_create_half_float_target(device, 2);
	zest_image_handle target_handle = metal_create_clear_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment);
	zest_buffer_info_t upload_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_cpu_to_gpu);
	zest_buffer_info_t readback_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_gpu_to_cpu);
	zest_buffer copy_source = zest_CreateBuffer(device, 256, &upload_info);
	zest_buffer copy_targets[2] = { zest_CreateBuffer(device, 256, &readback_info), zest_CreateBuffer(device, 256, &readback_info) };
	zest_image source = zest_GetImage(source_handle);
	zest_image destination = zest_GetImage(destination_handle);
	zest_image srgb_destination = zest_GetImage(srgb_handle);
	zest_image float_destination = zest_GetImage(float_handle);
	zest_image half_destination = zest_GetImage(half_handle);
	zest_image half_array_destination = zest_GetImage(half_array_handle);
	zest_image target = zest_GetImage(target_handle);
	int failed = context == NULL || !source || !destination || !srgb_destination || !float_destination || !half_destination || !half_array_destination || !target;
	failed |= !copy_source || !copy_targets[0] || !copy_targets[1];
	if (!failed) {
		failed |= !metal_upload_mip_patterns(device, source, source_seed);
		failed |= !metal_upload_mip_patterns(device, destination, untouched_seed);
		failed |= !metal_upload_mip_patterns(device, srgb_destination, untouched_seed);
		failed |= !metal_upload_mip_patterns(device, float_destination, untouched_seed);

		//A copy moves the one mip it's given and no other
		failed |= run_metal_mip_command_graph(context, source, destination, ZEST_FALSE, 1);
		failed |= !metal_mip_holds_pattern(device, destination, 1, source_seed);
		failed |= !metal_mip_holds_pattern(device, destination, 0, untouched_seed) || !metal_mip_holds_pattern(device, destination, 2, untouched_seed);
		//A blit between two images of one format is that copy
		failed |= run_metal_mip_command_graph(context, source, destination, ZEST_TRUE, 2);
		failed |= !metal_mip_holds_pattern(device, destination, 2, source_seed);
		failed |= !metal_mip_holds_pattern(device, destination, 0, untouched_seed);
		//A copy between two formats with the same channels and size moves the bytes as they are
		failed |= run_metal_mip_command_graph(context, source, srgb_destination, ZEST_FALSE, 0);
		failed |= !metal_mip_holds_pattern(device, srgb_destination, 0, source_seed);
		failed |= zest_GetValidationErrorCount(device) != 0;

		//Metal would copy four 8 bit channels into one 32 bit float wrongly and say nothing, so the copy is refused
		failed |= run_metal_mip_command_graph(context, source, float_destination, ZEST_FALSE, 0);
		if (zest_GetValidationErrorCount(device) != 1 || !metal_mip_holds_pattern(device, float_destination, 0, untouched_seed)) {
			ZEST_PRINT("A copy from r8g8b8a8_unorm into r32_sfloat raised %u validation errors and %s its destination, where it should be refused once.", zest_GetValidationErrorCount(device),
				metal_mip_holds_pattern(device, float_destination, 0, untouched_seed) ? "left" : "changed");
			failed = 1;
		}
		zest_ResetValidationErrors(device);

		//A blit between two formats converts, which it does by drawing the one mip into a destination that can be a render target
		const float blue_floats[4] = { 0.2f, 0.4f, 0.6f, 1.f };
		failed |= !run_metal_colour_clear(device, half_destination, metal_colour_clear_value(0.2f, 0.4f, 0.6f));
		failed |= !run_metal_colour_clear(device, half_array_destination, metal_colour_clear_value(0.2f, 0.4f, 0.6f));
		//With a buffer copy either side of it in the one pass, the blit has to end the first copy's encoder and the second copy opens another
		MetalMipCommandTask converting = {};
		converting.blit = ZEST_TRUE;
		converting.mip = 1;
		converting.copy_source = copy_source;
		converting.copy_targets[0] = copy_targets[0];
		converting.copy_targets[1] = copy_targets[1];
		metal_fill_buffer_bytes(copy_source, 29);
		memset(zest_BufferData(copy_targets[0]), 0xEE, 256);
		memset(zest_BufferData(copy_targets[1]), 0xEE, 256);
		failed |= run_metal_mip_task_graph(context, source, half_destination, &converting);
		if (!metal_mip_holds_converted_pattern(device, half_destination, 1, source_seed)) {
			ZEST_PRINT("A blit of mip 1 from r8g8b8a8_unorm into r16g16b16a16_sfloat didn't leave the source's bytes there as floats.");
			failed = 1;
		}
		failed |= memcmp(zest_BufferData(copy_targets[0]), zest_BufferData(copy_source), 256) != 0 || memcmp(zest_BufferData(copy_targets[1]), zest_BufferData(copy_source), 256) != 0;
		failed |= !metal_half_mip_is(device, half_destination, 0, 0, blue_floats) || !metal_half_mip_is(device, half_destination, 2, 0, blue_floats);
		//The blit pointed the context's pass descriptor at mip 1, where the next pass must not be left
		failed |= run_metal_clear_graph(context, target, NULL, ZEST_TRUE, ZEST_FALSE);
		failed |= zest_GetValidationErrorCount(device) != 0;

		//A sampled texture can't be drawn into, so the blit says which flags its destination needs
		failed |= run_metal_mip_command_graph(context, source, srgb_destination, ZEST_TRUE, 1);
		if (zest_GetValidationErrorCount(device) != 1 || !metal_validation_error_mentions(device, "zest_image_flag_color_attachment") || !metal_mip_holds_pattern(device, srgb_destination, 1, untouched_seed)) {
			ZEST_PRINT("A blit from r8g8b8a8_unorm into a sampled r8g8b8a8_srgb texture raised %u validation errors and %s its destination, where it should be refused once.", zest_GetValidationErrorCount(device),
				metal_mip_holds_pattern(device, srgb_destination, 1, untouched_seed) ? "left" : "changed");
			failed = 1;
		}
		zest_ResetValidationErrors(device);
		//A layer of an array would need a view made for the blit, which a frame graph batch can't keep alive
		failed |= run_metal_mip_command_graph(context, source, half_array_destination, ZEST_TRUE, 1);
		failed |= zest_GetValidationErrorCount(device) != 1 || !metal_validation_error_mentions(device, "one layer");
		zest_ResetValidationErrors(device);
		failed |= !metal_half_mip_is(device, half_array_destination, 1, 0, blue_floats) || !metal_half_mip_is(device, half_array_destination, 1, 1, blue_floats);
		//Inside a render pass the pass's encoder is open, so the blit can't open its own
		failed |= run_metal_mip_blit_in_render_pass(context, source, half_destination);
		failed |= zest_GetValidationErrorCount(device) != 1 || !metal_validation_error_mentions(device, "inside a render pass");
		zest_ResetValidationErrors(device);
		failed |= !metal_mip_holds_converted_pattern(device, half_destination, 1, source_seed);

		//A blit points the context's pass descriptor at its destination, which must not keep the image alive. Checked with no pass after the blit.
		failed |= run_metal_mip_command_graph(context, source, half_destination, ZEST_TRUE, 2);
		failed |= !metal_mip_holds_converted_pattern(device, half_destination, 2, source_seed);
		id<MTLTexture> blitted_texture = [ZEST_MTL_BACKEND(image, half_destination)->texture retain];
		zest_WaitForIdleDevice(device);
		zest_FreeImageNow(half_handle);
		half_handle.value = 0;
		if (metal_settled_owner_count(blitted_texture) != 1) {
			ZEST_PRINT("The freed image that zest_cmd_BlitImageMip drew into still has %lu owners, where only this test should hold it.", (unsigned long)[blitted_texture retainCount]);
			failed = 1;
		}
		[blitted_texture release];
	}
	zest_WaitForIdleDevice(device);
	failed |= zest_GetValidationErrorCount(device) != 0;
	if (source_handle.value) zest_FreeImageNow(source_handle);
	if (destination_handle.value) zest_FreeImageNow(destination_handle);
	if (srgb_handle.value) zest_FreeImageNow(srgb_handle);
	if (float_handle.value) zest_FreeImageNow(float_handle);
	if (half_handle.value) zest_FreeImageNow(half_handle);
	if (half_array_handle.value) zest_FreeImageNow(half_array_handle);
	if (target_handle.value) zest_FreeImageNow(target_handle);
	if (copy_source) zest_FreeBufferNow(copy_source);
	if (copy_targets[0]) zest_FreeBufferNow(copy_targets[0]);
	if (copy_targets[1]) zest_FreeBufferNow(copy_targets[1]);
	if (context) {
		zest_DestroyContext(context);
	}
	return failed | finish_metal_device(device);
}

static zest_bool metal_words_equal(const void *bytes, zest_size size, zest_uint value) {
	const zest_uint *words = (const zest_uint *)bytes;
	for (zest_size word = 0; word != size / 4; ++word) {
		if (words[word] != value) return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

static int run_metal_fill(zest_device device, zest_buffer buffer, zest_uint value) {
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	if (!queue) return 1;
	zest_imm_FillBuffer(queue, buffer, value);
	return !zest_imm_EndCommandBuffer(queue);
}

static int run_metal_update(zest_device device, zest_buffer buffer, const void *data, zest_size intended_size) {
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	if (!queue) return 1;
	zest_imm_UpdateBuffer(queue, buffer, (void *)data, intended_size);
	return !zest_imm_EndCommandBuffer(queue);
}

int test_metal_buffer_fills(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_buffer_info_t readback_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_gpu_to_cpu);
	zest_buffer_info_t device_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	//The buffers either side show a fill that starts early or runs on, and are large enough to share the filled buffer's pool.
	//200,000 bytes is three whole staging blocks and a short one.
	const zest_size guard_size = 70000;
	zest_buffer before = zest_CreateBuffer(device, guard_size, &device_info);
	zest_buffer device_buffer = zest_CreateBuffer(device, 200000, &device_info);
	zest_buffer after = zest_CreateBuffer(device, guard_size, &device_info);
	zest_buffer readback = zest_CreateBuffer(device, 200000, &readback_info);
	//Holds the start of the staging pool, so the block a fill or an update copies from has an offset to get wrong
	zest_buffer staging_blocker = zest_CreateStagingBuffer(device, 1024, NULL);
	int failed = !before || !device_buffer || !after || !readback || !staging_blocker;
	if (!failed) {
		memset(zest_BufferData(staging_blocker), 0xEE, 1024);
		const unsigned char *bytes = (const unsigned char *)zest_BufferData(readback);
		zest_size whole_size = device_buffer->size;
		if (device_buffer->memory_offset == 0 || whole_size % ZEST__MTL_FILL_BLOCK_SIZE == 0 || whole_size < ZEST__MTL_FILL_BLOCK_SIZE * 3) {
			ZEST_PRINT("The filled buffer is %llu bytes at offset %llu, where the test needs an offset and a size that ends on a short block.", (unsigned long long)whole_size, (unsigned long long)device_buffer->memory_offset);
			failed = 1;
		}
		failed |= run_metal_fill(device, before, 0x77777777u);
		failed |= run_metal_fill(device, after, 0x77777777u);

		//One byte repeated, zero, then four different bytes, so every fill changes what the last one left
		const zest_uint fill_values[3] = { 0xABABABABu, 0, 0x11223344u };
		for (int fill = 0; fill != 3; ++fill) {
			failed |= run_metal_fill(device, device_buffer, fill_values[fill]);
			memset(zest_BufferData(readback), 0x5A, readback->size);
			failed |= !metal_read_device_buffer(device, device_buffer, readback, whole_size);
			if (!metal_words_equal(bytes, whole_size, fill_values[fill])) {
				ZEST_PRINT("A buffer of %llu bytes filled with 0x%08X doesn't hold it in every word.", (unsigned long long)whole_size, fill_values[fill]);
				failed = 1;
			}
		}

		//Zest rounds every buffer's size up, so only a size the test shrinks ends part way through a word
		device_buffer->size = 1022;
		failed |= run_metal_fill(device, device_buffer, 0x55667788u);
		device_buffer->size = whole_size;
		failed |= !metal_read_device_buffer(device, device_buffer, readback, 2048);
		failed |= !metal_words_equal(bytes, 1020, 0x55667788u);
		if (bytes[1020] != 0x88 || bytes[1021] != 0x77 || bytes[1022] != 0x22 || bytes[1023] != 0x11) {
			ZEST_PRINT("A 1022 byte fill ended on bytes %02X %02X %02X %02X, where its last two bytes are 88 77 and the two after it are left as 22 11.", bytes[1020], bytes[1021], bytes[1022], bytes[1023]);
			failed = 1;
		}
		failed |= !metal_words_equal(bytes + 1024, 1024, 0x11223344u);

		//An update is rounded up to a whole word, and the bytes that adds are zero
		const unsigned char update_data[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
		failed |= run_metal_update(device, device_buffer, update_data, 10);
		failed |= !metal_read_device_buffer(device, device_buffer, readback, 2048);
		if (memcmp(bytes, update_data, 10) != 0 || bytes[10] != 0 || bytes[11] != 0 || bytes[12] != 0x88) {
			ZEST_PRINT("A 10 byte update left bytes 10 to 12 as %02X %02X %02X, where they should be 00 00 88.", bytes[10], bytes[11], bytes[12]);
			failed = 1;
		}

		//An update stops at 65,536 bytes however many it's given
		static unsigned char large_update[70000];
		memset(large_update, 0xC3, sizeof(large_update));
		failed |= run_metal_update(device, device_buffer, large_update, sizeof(large_update));
		failed |= !metal_read_device_buffer(device, device_buffer, readback, whole_size);
		failed |= !metal_words_equal(bytes, 65536, 0xC3C3C3C3u);
		if (!metal_words_equal(bytes + 65536, whole_size - 65536, 0x11223344u)) {
			ZEST_PRINT("A 70,000 byte update changed bytes past its first 65,536.");
			failed = 1;
		}

		failed |= !metal_read_device_buffer(device, before, readback, guard_size) || !metal_words_equal(bytes, guard_size, 0x77777777u);
		failed |= !metal_read_device_buffer(device, after, readback, guard_size) || !metal_words_equal(bytes, guard_size, 0x77777777u);
		failed |= zest_GetValidationErrorCount(device) != 0;
	}
	if (before) zest_FreeBufferNow(before);
	if (device_buffer) zest_FreeBufferNow(device_buffer);
	if (after) zest_FreeBufferNow(after);
	if (readback) zest_FreeBufferNow(readback);
	if (staging_blocker) zest_FreeBufferNow(staging_blocker);
	return failed | finish_metal_device(device);
}

//Whether the texture Zest would make for the format and flags can be a render target
static zest_bool metal_descriptor_is_target(zest_device device, zest_format format, zest_image_flags flags) {
	zest_image_t image = {};
	image.info = zest_CreateImageInfo(16, 16);
	image.info.format = format;
	MTLTextureDescriptor *descriptor = zest__mtl_new_texture_descriptor(device, &image, 1, 1, flags);
	zest_bool is_target = ([descriptor usage] & MTLTextureUsageRenderTarget) != 0;
	[descriptor release];
	return is_target;
}

//Attachments can be render targets, and so can a storage image in an uncompressed colour format. Nothing else can.
int test_metal_render_target_usage(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	int failed = metal_descriptor_is_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_texture);
	failed |= !metal_descriptor_is_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment);
	failed |= !metal_descriptor_is_target(device, zest_format_d32_sfloat, zest_image_preset_depth_attachment);
	failed |= !metal_descriptor_is_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_storage);
	failed |= !metal_descriptor_is_target(device, zest_format_r32_uint, zest_image_preset_storage);
	failed |= metal_descriptor_is_target(device, zest_format_bc7_unorm_block, zest_image_preset_texture);
	failed |= metal_descriptor_is_target(device, zest_format_bc7_unorm_block, zest_image_preset_storage);
	failed |= metal_descriptor_is_target(device, zest_format_astc_4X4_unorm_block, zest_image_preset_storage);
	failed |= metal_descriptor_is_target(device, zest_format_d32_sfloat, zest_image_preset_storage);
	//The compressed descriptors above were made, so their answers aren't those of a format the device lacks
	zest_image_t compressed_image = {};
	compressed_image.info = zest_CreateImageInfo(16, 16);
	compressed_image.info.format = zest_format_bc7_unorm_block;
	MTLTextureDescriptor *compressed_descriptor = zest__mtl_new_texture_descriptor(device, &compressed_image, 1, 1, zest_image_preset_storage);
	failed |= compressed_descriptor == nil;
	[compressed_descriptor release];
	return failed | finish_metal_device(device);
}

//Whether every pixel of every layer of the first mip_count mips of a 16x16 image with 4 bytes a pixel is the colour
static zest_bool metal_mips_match(zest_device device, zest_image image, zest_uint mip_count, zest_uint layer_count, const int expected[4]) {
	for (zest_uint mip = 0; mip != mip_count; ++mip) {
		zest_uint mip_size = 16u >> mip;
		for (zest_uint layer = 0; layer != layer_count; ++layer) {
			zest_uint pixels[16 * 16];
			memset(pixels, 0x5A, sizeof(pixels));
			if (!metal_read_texture(device, image, mip, layer, mip_size, mip_size, 4, pixels)) return ZEST_FALSE;
			if (!metal_pixels_match(pixels, mip_size * mip_size, expected)) return ZEST_FALSE;
		}
	}
	return ZEST_TRUE;
}

//An integer image clears to the colour's bits read as integers, as on the other backends
static int metal_integer_clear_matches(zest_device device, zest_format format, zest_uint value) {
	zest_image_handle handle = metal_create_clear_target(device, format, zest_image_preset_color_attachment);
	zest_image image = zest_GetImage(handle);
	if (!image) return 1;
	zest_clear_value_t clear_value = {};
	zest_uint bits[4] = { value, value, value, value };
	memcpy(&clear_value.color, bits, sizeof(bits));
	int failed = !run_metal_colour_clear(device, image, clear_value);
	zest_uint pixels[16 * 16];
	memset(pixels, 0x5A, sizeof(pixels));
	failed |= !metal_read_texture(device, image, 0, 0, 16, 16, 4, pixels);
	for (zest_uint index = 0; index != 16 * 16; ++index) failed |= pixels[index] != value;
	zest_FreeImageNow(handle);
	return failed;
}

int test_metal_clear_colour_image(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	const int black_expected[4] = { 0, 0, 0, 255 };
	zest_uint black_pixels[16 * 16];
	for (zest_uint index = 0; index != 16 * 16; ++index) black_pixels[index] = 0xFF000000u;
	zest_image_info_t attachment_info = zest_CreateImageInfo(16, 16);
	attachment_info.format = zest_format_r8g8b8a8_unorm;
	attachment_info.mip_levels = 3;
	attachment_info.layer_count = 2;
	attachment_info.flags = zest_image_preset_color_attachment | zest_image_flag_transfer_src;
	zest_image_handle attachment_handle = zest_CreateImage(device, &attachment_info);
	zest_image_info_t storage_info = zest_CreateImageInfo(16, 16);
	storage_info.format = zest_format_r8g8b8a8_unorm;
	storage_info.mip_levels = 3;
	storage_info.flags = zest_image_preset_storage | zest_image_flag_transfer_src;
	zest_image_handle storage_handle = zest_CreateImage(device, &storage_info);
	zest_image_info_t texture_info = zest_CreateImageInfo(16, 16);
	texture_info.format = zest_format_r8g8b8a8_unorm;
	texture_info.flags = zest_image_preset_texture | zest_image_flag_transfer_src;
	zest_image_handle texture_handle = zest_CreateImageWithPixels(device, black_pixels, sizeof(black_pixels), &texture_info);
	zest_image_info_t volume_info = zest_CreateImageInfo3D(16, 16, 4);
	volume_info.format = zest_format_r8g8b8a8_unorm;
	volume_info.flags = zest_image_preset_storage_3d;
	zest_image_handle volume_handle = zest_CreateImage(device, &volume_info);
	zest_buffer_info_t upload_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_cpu_to_gpu);
	zest_buffer source = zest_CreateBuffer(device, 16 * 16 * 4, &upload_info);
	int failed = !attachment_handle.value || !storage_handle.value || !texture_handle.value || !volume_handle.value || !source;
	if (!failed) {
		zest_image attachment = zest_GetImage(attachment_handle);
		zest_image storage = zest_GetImage(storage_handle);
		zest_image texture = zest_GetImage(texture_handle);
		failed |= !run_metal_colour_clear(device, attachment, metal_colour_clear_value(0.2f, 0.4f, 0.6f));
		if (!metal_mips_match(device, attachment, 3, 2, metal_blue_expected)) {
			ZEST_PRINT("A colour attachment with 3 mips and 2 layers wasn't cleared in every mip of both layers.");
			failed = 1;
		}

		//A storage image carries the usage a clear needs too, and keeps the one a shader writes it through
		MTLTextureUsage storage_usage = [ZEST_MTL_BACKEND(image, storage)->texture usage];
		failed |= !(storage_usage & MTLTextureUsageRenderTarget) || !(storage_usage & MTLTextureUsageShaderWrite);
		failed |= !run_metal_colour_clear(device, storage, metal_colour_clear_value(0.6f, 0.4f, 0.2f));
		failed |= !metal_mips_match(device, storage, 3, 1, metal_orange_expected);

		//An upload, two clears and another upload in one command buffer each open an encoder of their own, and have to run in the order they were
		//recorded in: the first clear covers the upload before it, and the second upload covers the clear before it
		metal_fill_buffer_bytes(source, 5);
		zest_buffer_image_copy_t region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
		region.image_aspect = zest_image_aspect_color_bit;
		region.layer_count = 1;
		region.image_extent = attachment->info.extent;
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= queue == NULL || !zest_imm_CopyBufferRegionsToImage(queue, &region, 1, source, attachment);
		failed |= !zest_imm_ClearColorImage(queue, attachment, metal_colour_clear_value(0.6f, 0.4f, 0.2f));
		failed |= !zest_imm_ClearColorImage(queue, storage, metal_colour_clear_value(0.2f, 0.4f, 0.6f));
		failed |= !zest_imm_CopyBufferRegionsToImage(queue, &region, 1, source, storage);
		failed |= !zest_imm_EndCommandBuffer(queue);
		if (!metal_mips_match(device, attachment, 3, 2, metal_orange_expected)) {
			ZEST_PRINT("A clear recorded after an upload to the same image didn't cover it.");
			failed = 1;
		}
		unsigned char uploaded_bytes[16 * 16 * 4];
		zest_uint mip_pixels[8 * 8];
		memset(uploaded_bytes, 0, sizeof(uploaded_bytes));
		failed |= !metal_read_texture(device, storage, 0, 0, 16, 16, 4, uploaded_bytes);
		if (memcmp(uploaded_bytes, zest_BufferData(source), sizeof(uploaded_bytes)) != 0) {
			ZEST_PRINT("An upload recorded after a clear of the same image didn't cover it.");
			failed = 1;
		}
		for (zest_uint mip = 1; mip != 3; ++mip) {
			memset(mip_pixels, 0, sizeof(mip_pixels));
			failed |= !metal_read_texture(device, storage, mip, 0, 16u >> mip, 16u >> mip, 4, mip_pixels) || !metal_pixels_match(mip_pixels, (16u >> mip) * (16u >> mip), metal_blue_expected);
		}

		//Above 2^24, so a value that went through a float on the way would come back changed
		failed |= metal_integer_clear_matches(device, zest_format_r32_uint, 0x89ABCDEFu);
		failed |= metal_integer_clear_matches(device, zest_format_r32_sint, (zest_uint)-123456789);
		failed |= zest_GetValidationErrorCount(device) != 0;

		//A sampled texture can't be a render target, so Zest refuses before Metal's own validation would, and names the flags that make one
		failed |= run_metal_colour_clear(device, texture, metal_colour_clear_value(0.2f, 0.4f, 0.6f)) != ZEST_FALSE;
		if (zest_GetValidationErrorCount(device) != 1 || !metal_validation_error_mentions(device, "zest_image_flag_color_attachment") || !metal_validation_error_mentions(device, "zest_image_flag_storage")) {
			ZEST_PRINT("Clearing a sampled texture raised %u validation errors, where the one refusal should name the flags to add.", zest_GetValidationErrorCount(device));
			failed = 1;
		}
		zest_ResetValidationErrors(device);
		failed |= !metal_mips_match(device, texture, 1, 1, black_expected);

		//A pass targets one depth plane of a 3D image, so the whole image can't be cleared this way
		failed |= run_metal_colour_clear(device, zest_GetImage(volume_handle), metal_colour_clear_value(0.2f, 0.4f, 0.6f)) != ZEST_FALSE;
		failed |= zest_GetValidationErrorCount(device) != 1 || !metal_validation_error_mentions(device, "3D image");
		zest_ResetValidationErrors(device);
	}
	if (attachment_handle.value) zest_FreeImageNow(attachment_handle);
	if (storage_handle.value) zest_FreeImageNow(storage_handle);
	if (texture_handle.value) zest_FreeImageNow(texture_handle);
	if (volume_handle.value) zest_FreeImageNow(volume_handle);
	if (source) zest_FreeBufferNow(source);
	return failed | finish_metal_device(device);
}

//Reads the depth or the stencil plane of mip 0 of a 16x16 depth image
static zest_bool metal_read_depth_plane(zest_device device, zest_image image, zest_bool stencil, void *out_values) {
	zest_uint bytes_per_pixel = stencil ? 1 : 4;
	id<MTLBuffer> readback = [zest_GetMetalDevice(device) newBufferWithLength:16 * 16 * bytes_per_pixel options:MTLResourceStorageModeShared];
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	if (!queue) {
		[readback release];
		return ZEST_FALSE;
	}
	[zest__mtl_queue_blit_encoder(queue) copyFromTexture:ZEST_MTL_BACKEND(image, image)->texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(16, 16, 1)
		toBuffer:readback destinationOffset:0 destinationBytesPerRow:16 * bytes_per_pixel destinationBytesPerImage:16 * 16 * bytes_per_pixel
		options:zest__mtl_blit_option(image->info.format, stencil ? zest_image_aspect_stencil_bit : zest_image_aspect_depth_bit)];
	zest_bool finished = zest_imm_EndCommandBuffer(queue);
	if (finished) {
		memcpy(out_values, [readback contents], 16 * 16 * bytes_per_pixel);
	}
	[readback release];
	return finished;
}

static zest_bool metal_depth_plane_is(zest_device device, zest_image image, float expected) {
	float depths[16 * 16];
	memset(depths, 0, sizeof(depths));
	if (!metal_read_depth_plane(device, image, ZEST_FALSE, depths)) return ZEST_FALSE;
	for (zest_uint index = 0; index != 16 * 16; ++index) {
		if (depths[index] != expected) return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

static zest_bool run_metal_depth_clear(zest_device device, zest_device_queue_type queue_type, zest_image image, float depth, zest_uint stencil) {
	zest_queue queue = zest_imm_BeginCommandBuffer(device, queue_type);
	if (!queue) return ZEST_FALSE;
	zest_bool cleared = zest_imm_ClearDepthStencilImage(queue, image, depth, stencil);
	return zest_imm_EndCommandBuffer(queue) && cleared;
}

int test_metal_clear_depth_image(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_image_handle depth_handle = metal_create_clear_target(device, zest_format_d32_sfloat, zest_image_preset_depth_attachment);
	zest_image_handle stencil_handle = metal_create_clear_target(device, zest_format_d32_sfloat_s8_uint, zest_image_preset_depth_attachment);
	zest_image_handle sampled_handle = metal_create_clear_target(device, zest_format_d32_sfloat, zest_image_flag_sampled | zest_image_flag_device_local);
	zest_image_handle colour_handle = metal_create_clear_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment);
	int failed = !depth_handle.value || !stencil_handle.value || !sampled_handle.value || !colour_handle.value;
	if (!failed) {
		zest_image depth_image = zest_GetImage(depth_handle);
		zest_image stencil_image = zest_GetImage(stencil_handle);
		failed |= !run_metal_depth_clear(device, zest_queue_graphics, depth_image, 0.25f, 0);
		failed |= !metal_depth_plane_is(device, depth_image, 0.25f);
		failed |= zest_GetValidationErrorCount(device) != 0;

		//Any Metal queue could clear it, and the call is refused off a graphics queue so one app behaves the same on every backend
		failed |= run_metal_depth_clear(device, zest_queue_compute, depth_image, 1.f, 0) != ZEST_FALSE;
		if (zest_GetValidationErrorCount(device) != 1 || !metal_depth_plane_is(device, depth_image, 0.25f)) {
			ZEST_PRINT("A depth clear on a compute queue raised %u validation errors, where it should be refused once and leave the image alone.", zest_GetValidationErrorCount(device));
			failed = 1;
		}
		zest_ResetValidationErrors(device);

		//A format with stencil clears both planes in the one pass
		failed |= !run_metal_depth_clear(device, zest_queue_graphics, stencil_image, 0.5f, 7);
		failed |= !metal_depth_plane_is(device, stencil_image, 0.5f);
		unsigned char stencils[16 * 16];
		memset(stencils, 0, sizeof(stencils));
		failed |= !metal_read_depth_plane(device, stencil_image, ZEST_TRUE, stencils);
		for (zest_uint index = 0; index != 16 * 16; ++index) failed |= stencils[index] != 7;
		failed |= zest_GetValidationErrorCount(device) != 0;

		//A depth image that is no attachment, and each kind of image given to the other kind's clear
		failed |= run_metal_depth_clear(device, zest_queue_graphics, zest_GetImage(sampled_handle), 0.5f, 0) != ZEST_FALSE;
		failed |= zest_GetValidationErrorCount(device) != 1 || !metal_validation_error_mentions(device, "zest_image_flag_depth_stencil_attachment");
		zest_ResetValidationErrors(device);
		failed |= run_metal_depth_clear(device, zest_queue_graphics, zest_GetImage(colour_handle), 0.5f, 0) != ZEST_FALSE;
		failed |= zest_GetValidationErrorCount(device) != 1;
		zest_ResetValidationErrors(device);
		failed |= run_metal_colour_clear(device, depth_image, metal_colour_clear_value(0.2f, 0.4f, 0.6f)) != ZEST_FALSE;
		failed |= zest_GetValidationErrorCount(device) != 1;
		zest_ResetValidationErrors(device);
		failed |= !metal_depth_plane_is(device, depth_image, 0.25f);
	}
	if (depth_handle.value) zest_FreeImageNow(depth_handle);
	if (stencil_handle.value) zest_FreeImageNow(stencil_handle);
	if (sampled_handle.value) zest_FreeImageNow(sampled_handle);
	if (colour_handle.value) zest_FreeImageNow(colour_handle);
	return failed | finish_metal_device(device);
}

typedef struct MetalImageClearTask {
	zest_buffer source;
	zest_buffer target_before;
	zest_buffer target_after;
	zest_bool copy_around_clear;
	zest_bool cleared;					//What zest_cmd_ImageClear returned
	int run_count;
} MetalImageClearTask;

//Clears the pass's target, between two buffer copies when the task asks for them
static void metal_image_clear_pass(const zest_command_list command_list, void *user_data) {
	MetalImageClearTask *task = (MetalImageClearTask *)user_data;
	task->run_count++;
	if (task->copy_around_clear) {
		zest_cmd_CopyBuffer(command_list, task->source, task->target_before, 512);
	}
	zest_resource_node target = zest_GetPassOutputResource(command_list, "Clear Target");
	task->cleared = zest_cmd_ImageClear(command_list, zest_GetResourceImage(target));
	if (task->copy_around_clear) {
		zest_cmd_CopyBuffer(command_list, task->source, task->target_after, 512);
	}
}

static int run_metal_image_clear_graph(zest_context context, zest_image image, zest_bool render_pass, MetalImageClearTask *task) {
	if (!zest_BeginCommandGraph(context, "Image Clear", 0)) return 1;
	zest_resource_node node = zest_ImportImageResource("Clear Target", image, 0);
	zest_SetResourceClearColor(node, 0.6f, 0.4f, 0.2f, 1.0f);
	if (render_pass) {
		zest_BeginRenderPass("Clear Pass");
	} else {
		zest_BeginTransferPass("Clear Pass");
	}
	zest_ConnectOutput(node);
	zest_SetPassTask(metal_image_clear_pass, task);
	zest_EndPass();
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	int failed = frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
	return failed | (task->run_count != 1);
}

int test_metal_image_clear_command(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	const int zero_expected[4] = { 0, 0, 0, 0 };
	zest_image_info_t image_info = zest_CreateImageInfo(16, 16);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.mip_levels = 2;
	image_info.flags = zest_image_preset_color_attachment | zest_image_flag_transfer_src;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	zest_image_handle target_handle = metal_create_clear_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment);
	zest_buffer_info_t upload_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_cpu_to_gpu);
	zest_buffer_info_t readback_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_gpu_to_cpu);
	zest_buffer source = zest_CreateBuffer(device, 512, &upload_info);
	zest_buffer readbacks[2] = { zest_CreateBuffer(device, 512, &readback_info), zest_CreateBuffer(device, 512, &readback_info) };
	int failed = context == NULL || !image_handle.value || !target_handle.value || !source || !readbacks[0] || !readbacks[1];
	if (!failed) {
		zest_image image = zest_GetImage(image_handle);
		zest_image target = zest_GetImage(target_handle);
		failed |= !run_metal_colour_clear(device, image, metal_colour_clear_value(0.2f, 0.4f, 0.6f));
		failed |= !metal_mips_match(device, image, 2, 1, metal_blue_expected);

		//The clear ends the encoder the copy before it opened, and the copy after it opens another. Only mip 0 is cleared, to zero.
		MetalImageClearTask task = {};
		task.source = source;
		task.target_before = readbacks[0];
		task.target_after = readbacks[1];
		task.copy_around_clear = ZEST_TRUE;
		metal_fill_buffer_bytes(source, 17);
		memset(zest_BufferData(readbacks[0]), 0xEE, 512);
		memset(zest_BufferData(readbacks[1]), 0xEE, 512);
		failed |= run_metal_image_clear_graph(context, image, ZEST_FALSE, &task);
		failed |= !task.cleared;
		failed |= !metal_mips_match(device, image, 1, 1, zero_expected);
		zest_uint mip_pixels[8 * 8];
		memset(mip_pixels, 0, sizeof(mip_pixels));
		failed |= !metal_read_texture(device, image, 1, 0, 8, 8, 4, mip_pixels) || !metal_pixels_match(mip_pixels, 8 * 8, metal_blue_expected);
		failed |= memcmp(zest_BufferData(readbacks[0]), zest_BufferData(source), 512) != 0 || memcmp(zest_BufferData(readbacks[1]), zest_BufferData(source), 512) != 0;
		failed |= zest_GetValidationErrorCount(device) != 0;

		//The clear pointed the context's pass descriptor at the image, which must not keep it alive. Checked before another pass refills the descriptor.
		id<MTLTexture> cleared_texture = [ZEST_MTL_BACKEND(image, image)->texture retain];
		zest_WaitForIdleDevice(device);
		zest_FreeImageNow(image_handle);
		image_handle.value = 0;
		if (metal_settled_owner_count(cleared_texture) != 1) {
			ZEST_PRINT("The freed image that zest_cmd_ImageClear cleared still has %lu owners, where only this test should hold it.", (unsigned long)[cleared_texture retainCount]);
			failed = 1;
		}
		[cleared_texture release];

		//Inside a render pass the pass's own encoder is open, so the command is refused and the pass's clear stands
		MetalImageClearTask refused = {};
		failed |= run_metal_image_clear_graph(context, target, ZEST_TRUE, &refused);
		failed |= refused.cleared != ZEST_FALSE;
		if (zest_GetValidationErrorCount(device) != 1 || !metal_validation_error_mentions(device, "zest_cmd_ImageClear")) {
			ZEST_PRINT("zest_cmd_ImageClear inside a render pass raised %u validation errors, where it should be refused once.", zest_GetValidationErrorCount(device));
			failed = 1;
		}
		zest_ResetValidationErrors(device);
		failed |= !metal_mips_match(device, target, 1, 1, metal_orange_expected);
	}
	zest_WaitForIdleDevice(device);
	failed |= zest_GetValidationErrorCount(device) != 0;
	if (image_handle.value) zest_FreeImageNow(image_handle);
	if (target_handle.value) zest_FreeImageNow(target_handle);
	if (source) zest_FreeBufferNow(source);
	if (readbacks[0]) zest_FreeBufferNow(readbacks[0]);
	if (readbacks[1]) zest_FreeBufferNow(readbacks[1]);
	if (context) {
		zest_DestroyContext(context);
	}
	return failed | finish_metal_device(device);
}

static const zest_uint metal_red_pixel = 0xFF0000FFu;
static const zest_uint metal_green_pixel = 0xFF00FF00u;
static const zest_uint metal_blue_pixel = 0xFFFF0000u;
static const zest_uint metal_black_pixel = 0xFF000000u;

//Left half red, right half blue and the top row green, so a blit that lands flipped, shifted or unfiltered shows
static void metal_blit_source_pixels(zest_uint *pixels) {
	for (zest_uint index = 0; index != 16 * 16; ++index) {
		pixels[index] = index < 16 ? metal_green_pixel : ((index % 16) < 8 ? metal_red_pixel : metal_blue_pixel);
	}
}

static zest_bool run_metal_blit(zest_device device, zest_device_queue_type queue_type, zest_image source, zest_image destination, const int source_rect[4], const int destination_rect[4], zest_filter_type filter) {
	zest_queue queue = zest_imm_BeginCommandBuffer(device, queue_type);
	if (!queue) return ZEST_FALSE;
	zest_bool blitted = zest_imm_BlitImage(queue, source, destination, source_rect[0], source_rect[1], source_rect[2], source_rect[3], destination_rect[0], destination_rect[1], destination_rect[2], destination_rect[3], filter);
	return zest_imm_EndCommandBuffer(queue) && blitted;
}

//Whether mip 0 of a 16x16 image with 4 bytes a pixel holds the pixels, give or take the tolerance in each channel
static zest_bool metal_image_holds(zest_device device, zest_image image, const zest_uint *expected, int tolerance) {
	zest_uint pixels[16 * 16];
	memset(pixels, 0x5A, sizeof(pixels));
	if (!metal_read_texture(device, image, 0, 0, 16, 16, 4, pixels)) return ZEST_FALSE;
	const unsigned char *read_channels = (const unsigned char *)pixels;
	const unsigned char *expected_channels = (const unsigned char *)expected;
	for (zest_uint channel = 0; channel != 16 * 16 * 4; ++channel) {
		if (abs((int)read_channels[channel] - (int)expected_channels[channel]) > tolerance) {
			ZEST_PRINT("Pixel %u, %u is %08X, where %08X was expected.", (channel / 4) % 16, channel / 64, pixels[channel / 4], expected[channel / 4]);
			return ZEST_FALSE;
		}
	}
	return ZEST_TRUE;
}

static void metal_fill_pixels(zest_uint *pixels, zest_uint value) {
	for (zest_uint index = 0; index != 16 * 16; ++index) pixels[index] = value;
}

static zest_image_handle metal_create_blit_source(zest_device device, const zest_uint *pixels) {
	zest_image_info_t source_info = zest_CreateImageInfo(16, 16);
	source_info.format = zest_format_r8g8b8a8_unorm;
	source_info.flags = zest_image_preset_texture | zest_image_flag_transfer_src;
	return zest_CreateImageWithPixels(device, (void *)pixels, 16 * 16 * 4, &source_info);
}

int test_metal_blit_image(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_mtl_device_backend_t *device_backend = ZEST_MTL_BACKEND(device, device);
	zest_uint source_pixels[16 * 16];
	zest_uint black_pixels[16 * 16];
	zest_uint expected[16 * 16];
	zest_uint array_pixels[2 * 16 * 16];
	metal_blit_source_pixels(source_pixels);
	metal_fill_pixels(black_pixels, metal_black_pixel);
	metal_blit_source_pixels(array_pixels);
	metal_fill_pixels(array_pixels + 16 * 16, 0xFFFFFFFFu);
	zest_image_handle source_handle = metal_create_blit_source(device, source_pixels);
	zest_image_handle destination_handle = metal_create_clear_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment);
	zest_image_handle swapped_handle = metal_create_clear_target(device, zest_format_b8g8r8a8_unorm, zest_image_preset_color_attachment);
	zest_image_handle texture_handle = metal_create_blit_source(device, black_pixels);
	zest_image_handle integer_handle = metal_create_clear_target(device, zest_format_r8g8b8a8_uint, zest_image_preset_texture);
	zest_image_info_t array_info = zest_CreateImageInfo(16, 16);
	array_info.format = zest_format_r8g8b8a8_unorm;
	array_info.layer_count = 2;
	array_info.flags = zest_image_preset_texture;
	zest_image_handle array_handle = zest_CreateImage(device, &array_info);
	int failed = !source_handle.value || !destination_handle.value || !swapped_handle.value || !texture_handle.value || !integer_handle.value || !array_handle.value;
	const int whole[4] = { 0, 0, 16, 16 };
	if (!failed) {
		zest_image source = zest_GetImage(source_handle);
		zest_image destination = zest_GetImage(destination_handle);
		zest_image swapped = zest_GetImage(swapped_handle);
		zest_image texture = zest_GetImage(texture_handle);
		failed |= !metal_upload_layers(device, zest_GetImage(array_handle), array_pixels, 16 * 16 * 4, 2);

		//Nothing is built until something blits. A nearest blit between two rects of one size is an exact copy with the top row still at the top.
		failed |= device_backend->blit_pipeline_count != 0 || device_backend->blit_library != nil;
		failed |= !run_metal_blit(device, zest_queue_graphics, source, destination, whole, whole, zest_filter_nearest);
		failed |= !metal_image_holds(device, destination, source_pixels, 0);
		failed |= device_backend->blit_pipeline_count != 1;

		//A linear blit to half the size averages each 2x2 block, so the green row comes through at half strength, and nothing outside the rect is written
		const int half_rect[4] = { 4, 4, 8, 8 };
		failed |= !run_metal_colour_clear(device, destination, metal_colour_clear_value(0.f, 0.f, 0.f));
		failed |= !run_metal_blit(device, zest_queue_graphics, source, destination, whole, half_rect, zest_filter_linear);
		metal_fill_pixels(expected, metal_black_pixel);
		for (zest_uint row = 0; row != 8; ++row) {
			for (zest_uint column = 0; column != 8; ++column) {
				zest_uint half_green = column < 4 ? 0xFF008080u : 0xFF808000u;
				expected[(row + 4) * 16 + column + 4] = row == 0 ? half_green : (column < 4 ? metal_red_pixel : metal_blue_pixel);
			}
		}
		failed |= !metal_image_holds(device, destination, expected, 1);

		//A source rect drawn into a destination rect lands in that rect and leaves the rest
		const int right_half[4] = { 8, 0, 8, 16 };
		const int small_rect[4] = { 6, 2, 4, 4 };
		failed |= !run_metal_colour_clear(device, destination, metal_colour_clear_value(0.f, 1.f, 0.f));
		failed |= !run_metal_blit(device, zest_queue_graphics, source, destination, right_half, small_rect, zest_filter_nearest);
		metal_fill_pixels(expected, metal_green_pixel);
		for (zest_uint row = 2; row != 6; ++row) {
			for (zest_uint column = 6; column != 10; ++column) expected[row * 16 + column] = metal_blue_pixel;
		}
		failed |= !metal_image_holds(device, destination, expected, 0);

		//A destination rect that hangs off the image writes the part inside it, and one wholly outside or with no size writes nothing
		const int off_far_corner[4] = { 12, 12, 8, 8 };
		const int off_near_corner[4] = { -4, -4, 8, 8 };
		const int outside[4] = { 20, 20, 8, 8 };
		const int no_width[4] = { 0, 0, 0, 8 };
		failed |= !run_metal_colour_clear(device, destination, metal_colour_clear_value(0.f, 0.f, 0.f));
		failed |= !run_metal_blit(device, zest_queue_graphics, source, destination, whole, off_far_corner, zest_filter_nearest);
		failed |= !run_metal_blit(device, zest_queue_graphics, source, destination, whole, off_near_corner, zest_filter_nearest);
		failed |= !run_metal_blit(device, zest_queue_graphics, source, destination, whole, outside, zest_filter_nearest);
		failed |= !run_metal_blit(device, zest_queue_graphics, source, destination, whole, no_width, zest_filter_nearest);
		metal_fill_pixels(expected, metal_black_pixel);
		for (zest_uint row = 0; row != 4; ++row) {
			for (zest_uint column = 0; column != 4; ++column) {
				expected[row * 16 + column] = metal_blue_pixel;
				expected[(row + 12) * 16 + column + 12] = metal_red_pixel;
			}
		}
		failed |= !metal_image_holds(device, destination, expected, 0);
		if (zest_GetValidationErrorCount(device) != 0) {
			ZEST_PRINT("Blits into rects that hang off the destination raised %u validation errors.", zest_GetValidationErrorCount(device));
			failed = 1;
		}

		//A negative width runs the rect back from its far edge, as vkCmdBlitImage takes it, which mirrors the image
		const int mirrored[4] = { 16, 0, -16, 16 };
		failed |= !run_metal_blit(device, zest_queue_graphics, source, destination, whole, mirrored, zest_filter_nearest);
		for (zest_uint index = 0; index != 16 * 16; ++index) expected[index] = source_pixels[(index / 16) * 16 + 15 - (index % 16)];
		failed |= !metal_image_holds(device, destination, expected, 0);

		//An upload, two blits and another upload in one command buffer. Each blit ends the encoder before it, and the passes have to run in the order
		//they were recorded in, as the first blit reads the upload and the second reads what the first drew.
		zest_buffer staging_buffer = zest_CreateStagingBuffer(device, sizeof(source_pixels), source_pixels);
		zest_buffer_image_copy_t region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
		region.image_aspect = zest_image_aspect_color_bit;
		region.layer_count = 1;
		region.image_extent = texture->info.extent;
		failed |= !run_metal_colour_clear(device, destination, metal_colour_clear_value(0.f, 0.f, 0.f));
		failed |= !run_metal_colour_clear(device, swapped, metal_colour_clear_value(0.f, 0.f, 0.f));
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= !staging_buffer || queue == NULL || !zest_imm_CopyBufferRegionsToImage(queue, &region, 1, staging_buffer, texture);
		failed |= !zest_imm_BlitImage(queue, texture, destination, 0, 0, 16, 16, 0, 0, 16, 16, zest_filter_nearest);
		failed |= !zest_imm_BlitImage(queue, destination, swapped, 0, 0, 16, 16, 0, 0, 16, 16, zest_filter_nearest);
		id<MTLBuffer> blitted_readback = queue ? metal_record_image_read(device, queue, swapped) : nil;
		failed |= !zest_imm_CopyBufferRegionsToImage(queue, &region, 1, staging_buffer, zest_GetImage(integer_handle));
		failed |= !zest_imm_EndCommandBuffer(queue);
		if (staging_buffer) zest_FreeBufferNow(staging_buffer);
		for (zest_uint index = 0; index != 16 * 16; ++index) expected[index] = (source_pixels[index] & 0xFF00FF00u) | ((source_pixels[index] & 0xFFu) << 16) | ((source_pixels[index] >> 16) & 0xFFu);
		if (!blitted_readback || memcmp([blitted_readback contents], expected, sizeof(expected)) != 0) {
			ZEST_PRINT("A copy recorded after a blit in the same command buffer didn't read what the blit drew.");
			failed = 1;
		}
		[blitted_readback release];
		if (!metal_image_holds(device, destination, source_pixels, 0) || !metal_image_holds(device, swapped, expected, 0)) {
			ZEST_PRINT("An upload and two blits recorded in one command buffer didn't run in the order they were recorded in.");
			failed = 1;
		}
		failed |= !run_metal_colour_clear(device, swapped, metal_colour_clear_value(0.f, 0.f, 0.f));
		failed |= !zest_CopyBitmapToImage(device, black_pixels, sizeof(black_pixels), texture, 16, 16);

		//The first layer of an array is read through a view made for the call
		failed |= !run_metal_blit(device, zest_queue_graphics, zest_GetImage(array_handle), destination, whole, whole, zest_filter_nearest);
		failed |= !metal_image_holds(device, destination, source_pixels, 0);

		//A blit converts, so red and blue change places in a BGRA destination, and each destination format has one pipeline however often it's used
		failed |= !run_metal_blit(device, zest_queue_graphics, source, swapped, whole, whole, zest_filter_nearest);
		for (zest_uint index = 0; index != 16 * 16; ++index) expected[index] = (source_pixels[index] & 0xFF00FF00u) | ((source_pixels[index] & 0xFFu) << 16) | ((source_pixels[index] >> 16) & 0xFFu);
		failed |= !metal_image_holds(device, swapped, expected, 0);
		failed |= device_backend->blit_pipeline_count != 2;
		failed |= !run_metal_blit(device, zest_queue_graphics, source, destination, whole, whole, zest_filter_linear);
		failed |= !run_metal_blit(device, zest_queue_graphics, source, swapped, whole, whole, zest_filter_linear);
		if (device_backend->blit_pipeline_count != 2) {
			ZEST_PRINT("Blits to two destination formats left %i blit pipelines, where each format has one.", device_backend->blit_pipeline_count);
			failed = 1;
		}
		failed |= zest_GetValidationErrorCount(device) != 0;

		//A sampled texture can't be drawn into, an integer source can't be sampled as floats, and the call needs a graphics queue as it does elsewhere
		failed |= run_metal_blit(device, zest_queue_graphics, source, texture, whole, whole, zest_filter_nearest) != ZEST_FALSE;
		failed |= zest_GetValidationErrorCount(device) != 1 || !metal_validation_error_mentions(device, "zest_image_flag_color_attachment");
		zest_ResetValidationErrors(device);
		failed |= run_metal_blit(device, zest_queue_graphics, zest_GetImage(integer_handle), destination, whole, whole, zest_filter_nearest) != ZEST_FALSE;
		failed |= zest_GetValidationErrorCount(device) != 1 || !metal_validation_error_mentions(device, "integer");
		zest_ResetValidationErrors(device);
		failed |= run_metal_blit(device, zest_queue_compute, source, destination, whole, whole, zest_filter_nearest) != ZEST_FALSE;
		failed |= zest_GetValidationErrorCount(device) != 1 || !metal_validation_error_mentions(device, "graphics queue");
		zest_ResetValidationErrors(device);
		failed |= !metal_image_holds(device, destination, source_pixels, 0);
		failed |= !metal_image_holds(device, texture, black_pixels, 0);
	}
	if (source_handle.value) zest_FreeImageNow(source_handle);
	if (destination_handle.value) zest_FreeImageNow(destination_handle);
	if (swapped_handle.value) zest_FreeImageNow(swapped_handle);
	if (texture_handle.value) zest_FreeImageNow(texture_handle);
	if (integer_handle.value) zest_FreeImageNow(integer_handle);
	if (array_handle.value) zest_FreeImageNow(array_handle);

	//The library, pipelines and samplers belong to the Metal device, which a reset keeps
	zest_ResetDevice(device);
	failed |= device_backend != ZEST_MTL_BACKEND(device, device) || device_backend->blit_pipeline_count != 2;
	source_handle = metal_create_blit_source(device, source_pixels);
	destination_handle = metal_create_clear_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment);
	failed |= !source_handle.value || !destination_handle.value;
	if (source_handle.value && destination_handle.value) {
		failed |= !run_metal_blit(device, zest_queue_graphics, zest_GetImage(source_handle), zest_GetImage(destination_handle), whole, whole, zest_filter_nearest);
		failed |= !metal_image_holds(device, zest_GetImage(destination_handle), source_pixels, 0);
		failed |= device_backend->blit_pipeline_count != 2;
	}
	if (source_handle.value) zest_FreeImageNow(source_handle);
	if (destination_handle.value) zest_FreeImageNow(destination_handle);

	//The table holds 16 destination formats. A blit to one more gets no pipeline and says so, and the table is left as it was.
	const MTLPixelFormat more_formats[15] = { MTLPixelFormatR8Unorm, MTLPixelFormatRG8Unorm, MTLPixelFormatRGBA8Unorm_sRGB, MTLPixelFormatBGRA8Unorm_sRGB, MTLPixelFormatR16Unorm, MTLPixelFormatRG16Unorm, MTLPixelFormatRGBA16Unorm,
		MTLPixelFormatR16Float, MTLPixelFormatRG16Float, MTLPixelFormatRGBA16Float, MTLPixelFormatR32Float, MTLPixelFormatRG32Float, MTLPixelFormatRGBA32Float, MTLPixelFormatRGB10A2Unorm, MTLPixelFormatRG11B10Float };
	for (int format_index = 0; format_index != 14; ++format_index) {
		failed |= zest__mtl_blit_pipeline(device, more_formats[format_index]) == nil;
	}
	failed |= device_backend->blit_pipeline_count != ZEST__MTL_MAX_BLIT_PIPELINES || metal_report_count(device, "destination formats") != 0;
	id<MTLRenderPipelineState> one_too_many = zest__mtl_blit_pipeline(device, more_formats[14]);
	if (one_too_many != nil || device_backend->blit_pipeline_count != ZEST__MTL_MAX_BLIT_PIPELINES || metal_report_count(device, "destination formats") != 1) {
		ZEST_PRINT("A blit to a seventeenth destination format %s a pipeline, left a count of %i and made %i reports.", one_too_many ? "got" : "got no", device_backend->blit_pipeline_count, metal_report_count(device, "destination formats"));
		failed = 1;
	}
	failed |= zest__mtl_blit_pipeline(device, MTLPixelFormatBGRA8Unorm) == nil || zest__mtl_blit_pipeline(device, more_formats[13]) == nil;

	//The device's teardown lets go of what the blits built
	id<MTLLibrary> blit_library = [device_backend->blit_library retain];
	failed |= blit_library == nil;
	failed |= finish_metal_device(device);
	if (blit_library && metal_settled_owner_count(blit_library) != 1) {
		ZEST_PRINT("The blit library has %lu owners after its device was destroyed, where only this test should hold it.", (unsigned long)[blit_library retainCount]);
		failed = 1;
	}
	[blit_library release];
	return failed;
}

//zest_CreateImage only makes single sampled images, so this follows its steps with a sample count
static zest_image_handle metal_create_multisampled_image(zest_device device, zest_uint size, zest_sample_count_flags sample_count, zest_image_flags flags) {
	zest_image_handle handle = zest__new_image(device);
	zest_image image = zest__get_image_unsafe(handle);
	image->info = zest_CreateImageInfo(size, size);
	image->info.flags = flags;
	image->info.sample_count = sample_count;
	image->info.aspect_flags = zest_image_aspect_color_bit;
	if (!device->platform->create_image(device, 0, image, 1, sample_count, image->info.flags)) {
		zest__cleanup_image(image);
		return ZEST__ZERO_INIT(zest_image_handle);
	}
	image->default_view = device->platform->create_image_view(device, image, zest_image_view_type_2d, 1, 0, 0, 1, 0);
	image->default_view->handle.store = &device->resource_stores[zest_handle_type_views];
	zest__activate_resource(handle.store, handle.value);
	return handle;
}

static zest_bool run_metal_resolve(zest_device device, zest_device_queue_type queue_type, zest_image source, zest_image destination) {
	zest_queue queue = zest_imm_BeginCommandBuffer(device, queue_type);
	if (!queue) return ZEST_FALSE;
	zest_bool resolved = zest_imm_ResolveImage(queue, source, destination);
	return zest_imm_EndCommandBuffer(queue) && resolved;
}

int test_metal_resolve_image(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	const int black_expected[4] = { 0, 0, 0, 255 };
	zest_uint black_pixels[16 * 16];
	metal_fill_pixels(black_pixels, metal_black_pixel);
	zest_image_handle multisampled_handle = metal_create_multisampled_image(device, 16, zest_sample_count_4_bit, zest_image_preset_color_attachment);
	zest_image_handle second_multisampled_handle = metal_create_multisampled_image(device, 16, zest_sample_count_4_bit, zest_image_preset_color_attachment);
	zest_image_handle sampled_multisampled_handle = metal_create_multisampled_image(device, 16, zest_sample_count_4_bit, zest_image_flag_sampled | zest_image_flag_device_local);
	zest_image_handle resolved_handle = metal_create_blit_source(device, black_pixels);
	zest_image_handle single_handle = metal_create_clear_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment);
	zest_image_handle attachment_handle = metal_create_clear_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment);
	zest_image_handle swapped_handle = metal_create_clear_target(device, zest_format_b8g8r8a8_unorm, zest_image_preset_color_attachment);
	zest_image_info_t small_info = zest_CreateImageInfo(8, 8);
	small_info.flags = zest_image_preset_color_attachment;
	zest_image_handle small_handle = zest_CreateImage(device, &small_info);
	int failed = !multisampled_handle.value || !second_multisampled_handle.value || !sampled_multisampled_handle.value || !resolved_handle.value || !single_handle.value || !attachment_handle.value || !swapped_handle.value || !small_handle.value;
	if (!failed) {
		zest_image multisampled = zest_GetImage(multisampled_handle);
		zest_image resolved = zest_GetImage(resolved_handle);
		zest_image single = zest_GetImage(single_handle);
		zest_image attachment = zest_GetImage(attachment_handle);
		failed |= [ZEST_MTL_BACKEND(image, multisampled)->texture sampleCount] != 4;
		failed |= !run_metal_colour_clear(device, multisampled, metal_colour_clear_value(0.2f, 0.4f, 0.6f));

		//Into a sampled texture, which needs no render target usage to be resolved into. The upload before it in the same command buffer leaves
		//an encoder open that the resolve has to end.
		zest_buffer staging_buffer = zest_CreateStagingBuffer(device, sizeof(black_pixels), black_pixels);
		zest_buffer_image_copy_t region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
		region.image_aspect = zest_image_aspect_color_bit;
		region.layer_count = 1;
		region.image_extent = resolved->info.extent;
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= !staging_buffer || queue == NULL || !zest_imm_CopyBufferRegionsToImage(queue, &region, 1, staging_buffer, resolved);
		failed |= !zest_imm_ResolveImage(queue, multisampled, resolved);
		failed |= !zest_imm_EndCommandBuffer(queue);
		if (staging_buffer) zest_FreeBufferNow(staging_buffer);
		if (!metal_mips_match(device, resolved, 1, 1, metal_blue_expected)) {
			ZEST_PRINT("A multisampled image cleared to blue didn't resolve to blue.");
			failed = 1;
		}
		//The resolve keeps its source, so a second one from it gives the same pixels
		failed |= !run_metal_resolve(device, zest_queue_graphics, multisampled, attachment);
		if (!metal_mips_match(device, attachment, 1, 1, metal_blue_expected)) {
			ZEST_PRINT("A second resolve from the same multisampled image didn't give its pixels, so the first one didn't keep its source.");
			failed = 1;
		}
		//A copy and a blit recorded after a resolve read what it wrote. The BGRA destination's bytes are the blue's with red and blue changed over.
		failed |= !run_metal_colour_clear(device, attachment, metal_colour_clear_value(0.f, 0.f, 0.f));
		queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= queue == NULL || !zest_imm_ResolveImage(queue, multisampled, attachment);
		id<MTLBuffer> resolved_readback = queue ? metal_record_image_read(device, queue, attachment) : nil;
		failed |= !zest_imm_BlitImage(queue, attachment, zest_GetImage(swapped_handle), 0, 0, 16, 16, 0, 0, 16, 16, zest_filter_nearest);
		failed |= !zest_imm_EndCommandBuffer(queue);
		if (!resolved_readback || !metal_pixels_match((const zest_uint *)[resolved_readback contents], 16 * 16, metal_blue_expected) || !metal_mips_match(device, zest_GetImage(swapped_handle), 1, 1, metal_orange_expected)) {
			ZEST_PRINT("A copy and a blit recorded after a resolve in the same command buffer didn't read what the resolve wrote.");
			failed = 1;
		}
		[resolved_readback release];
		failed |= zest_GetValidationErrorCount(device) != 0;

		//A source with one sample, a destination with four, a compute queue, another format, another size and a source that is no attachment
		//are each refused before Metal sees them
		zest_image refused_sources[6] = { single, multisampled, multisampled, multisampled, multisampled, zest_GetImage(sampled_multisampled_handle) };
		zest_image refused_destinations[6] = { attachment, zest_GetImage(second_multisampled_handle), attachment, zest_GetImage(swapped_handle), zest_GetImage(small_handle), attachment };
		for (int refusal = 0; refusal != 6; ++refusal) {
			failed |= !run_metal_colour_clear(device, attachment, metal_colour_clear_value(0.f, 0.f, 0.f));
			failed |= run_metal_resolve(device, refusal == 2 ? zest_queue_compute : zest_queue_graphics, refused_sources[refusal], refused_destinations[refusal]) != ZEST_FALSE;
			if (zest_GetValidationErrorCount(device) != 1) {
				ZEST_PRINT("Refused resolve %i raised %u validation errors, where it should raise one.", refusal, zest_GetValidationErrorCount(device));
				failed = 1;
			}
			zest_ResetValidationErrors(device);
			failed |= !metal_mips_match(device, attachment, 1, 1, black_expected);
		}
	}
	if (multisampled_handle.value) zest_FreeImageNow(multisampled_handle);
	if (second_multisampled_handle.value) zest_FreeImageNow(second_multisampled_handle);
	if (sampled_multisampled_handle.value) zest_FreeImageNow(sampled_multisampled_handle);
	if (resolved_handle.value) zest_FreeImageNow(resolved_handle);
	if (single_handle.value) zest_FreeImageNow(single_handle);
	if (attachment_handle.value) zest_FreeImageNow(attachment_handle);
	if (swapped_handle.value) zest_FreeImageNow(swapped_handle);
	if (small_handle.value) zest_FreeImageNow(small_handle);
	return failed | finish_metal_device(device);
}

//Each view of a per mip array is one mip of the image at that mip's size, read here through the view's own texture
int test_metal_per_mip_views(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	const zest_uint seed = 43;
	zest_image_handle image_handle = metal_create_mip_image(device, zest_format_r8g8b8a8_unorm);
	zest_image image = zest_GetImage(image_handle);
	int failed = image == NULL || !metal_upload_mip_patterns(device, image, seed);
	zest_image_view_array_handle views_handle = ZEST__ZERO_INIT(zest_image_view_array_handle);
	if (!failed) {
		views_handle = zest_CreateImageViewsPerMip(device, image);
		zest_image_view_array view_array = views_handle.value ? zest_GetImageViewArray(views_handle) : NULL;
		failed |= view_array == NULL || view_array->count != 3;
		for (zest_uint mip = 0; !failed && mip != 3; ++mip) {
			zest_uint mip_size = 16u >> mip;
			id<MTLTexture> view_texture = ZEST_MTL_BACKEND(image_view, &view_array->views[mip])->texture;
			unsigned char read_bytes[16 * 16 * 4];
			unsigned char expected_bytes[16 * 16 * 4];
			memset(read_bytes, 0, sizeof(read_bytes));
			metal_mip_pattern(expected_bytes, mip, seed);
			if (view_texture == nil || [view_texture width] != mip_size || [view_texture height] != mip_size || [view_texture mipmapLevelCount] != 1) {
				ZEST_PRINT("The view of mip %u is %lu by %lu with %lu mips, where it should be %u square with one.", mip, (unsigned long)[view_texture width], (unsigned long)[view_texture height], (unsigned long)[view_texture mipmapLevelCount], mip_size);
				failed = 1;
				continue;
			}
			failed |= !metal_read_native_texture(device, view_texture, 0, 0, mip_size, mip_size, 4, read_bytes);
			if (memcmp(read_bytes, expected_bytes, mip_size * mip_size * 4) != 0) {
				ZEST_PRINT("The view of mip %u doesn't hold what was uploaded to that mip.", mip);
				failed = 1;
			}
		}
		failed |= zest_GetValidationErrorCount(device) != 0;
	}
	if (views_handle.value) zest_FreeImageViewArrayNow(views_handle);
	if (image_handle.value) zest_FreeImageNow(image_handle);
	return failed | finish_metal_device(device);
}

int test_metal_swapchain_formats(void) {
	MTLPixelFormat pixel_format = MTLPixelFormatInvalid;
	zest_format reported = zest_format_undefined;
	int failed = zest__mtl_swapchain_format(zest_format_b8g8r8a8_unorm, &pixel_format, &reported) != ZEST_TRUE;
	failed |= pixel_format != MTLPixelFormatBGRA8Unorm || reported != zest_format_b8g8r8a8_unorm;
	failed |= zest__mtl_swapchain_format(zest_format_b8g8r8a8_srgb, &pixel_format, &reported) != ZEST_TRUE;
	failed |= pixel_format != MTLPixelFormatBGRA8Unorm_sRGB || reported != zest_format_b8g8r8a8_srgb;
	failed |= zest__mtl_swapchain_format(zest_format_r16g16b16a16_sfloat, &pixel_format, &reported) != ZEST_TRUE;
	failed |= pixel_format != MTLPixelFormatRGBA16Float || reported != zest_format_r16g16b16a16_sfloat;
	failed |= zest__mtl_swapchain_format(zest_format_a2b10g10r10_unorm_pack32, &pixel_format, &reported) != ZEST_TRUE;
	failed |= pixel_format != MTLPixelFormatRGB10A2Unorm || reported != zest_format_a2b10g10r10_unorm_pack32;
	failed |= zest__mtl_swapchain_format(zest_format_r8_unorm, &pixel_format, &reported) != ZEST_FALSE;
	failed |= pixel_format != MTLPixelFormatBGRA8Unorm || reported != zest_format_b8g8r8a8_unorm;
	return failed;
}

//A window of its own, with SDL's video started first when nothing else has started it
struct MetalTestWindow {
	zest_window_data_t window_data;
	zest_bool initialised_video;
};

static zest_bool metal_open_test_window(MetalTestWindow *window, const char *title) {
	window->window_data = ZEST__ZERO_INIT(zest_window_data_t);
	window->initialised_video = ZEST_FALSE;
	if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		if (SDL_Init(SDL_INIT_VIDEO) != 0) return ZEST_FALSE;
		window->initialised_video = ZEST_TRUE;
	}
	window->window_data = zest_implsdl2_CreateWindow(50, 50, 320, 200, 0, title);
	return window->window_data.window_handle != NULL;
}

//zest_implsdl2_DestroyWindow needs a context, and the context is gone by now
static void metal_close_test_window(MetalTestWindow *window) {
	if (window->window_data.window_handle) {
		SDL_DestroyWindow((SDL_Window *)window->window_data.window_handle);
	}
	if (window->initialised_video) {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
}

static void metal_set_test_clear_color(zest_context context) {
	zest_SetSwapchainClearColor(context, 0.1f, 0.2f, 0.3f, 1.0f);
}

static int metal_viewport_pass_count = 0;

static void metal_screen_viewport_pass(const zest_command_list command_list, void *user_data) {
	(void)user_data;
	zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);
	metal_viewport_pass_count++;
}

struct MetalFrameRun {
	int began_count;
	int recreate_count;
	int bad_present_count;			//Frames that didn't end with exactly one present encoded and the drawable let go
	zest_microsecs slowest_begin;
};

//A frame that held on to its drawable makes a later zest_BeginFrame wait a second for the layer, so this is the bound that shows one
static const zest_microsecs metal_begin_frame_limit = 500000;

//A frame has to end with one present encoded, its drawable let go, and the swapchain image back in the present layout
static void metal_check_frame_presented(zest_context context, MetalFrameRun *run) {
	zest_mtl_context_backend_t *backend = ZEST_MTL_BACKEND(context, context);
	zest_image_layout layout = context->swapchain->images[0].layout;
	if (backend->present_count != 1 || backend->drawable != nil || layout != zest_image_layout_present) {
		ZEST_PRINT("A frame ended with %u presents encoded, %s and the swapchain image in layout %i.", backend->present_count, backend->drawable ? "its drawable still held" : "no drawable held", (int)layout);
		run->bad_present_count++;
	}
}

//Ends a frame with a cached graph that imports the swapchain and clears it
static void metal_end_windowed_frame(zest_context context, zest_bool set_viewport) {
	zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(context, &set_viewport, sizeof(set_viewport));
	zest_frame_graph frame_graph = zest_GetCachedFrameGraph(context, &cache_key);
	if (!frame_graph && zest_BeginFrameGraph(context, "Windowed Clear", &cache_key)) {
		zest_ImportSwapchainResource();
		zest_BeginRenderPass("Clear Swapchain");
		zest_ConnectSwapChainOutput();
		if (set_viewport) {
			zest_SetPassTask(metal_screen_viewport_pass, 0);
		} else {
			zest_SetPassTask(zest_EmptyRenderPass, 0);
		}
		zest_EndPass();
		frame_graph = zest_EndFrameGraph();
	}
	zest_EndFrame(context, frame_graph);
}

//One whole frame. Returns whether zest_BeginFrame acquired a drawable.
static zest_bool run_metal_windowed_frame(zest_context context, zest_bool set_viewport, MetalFrameRun *run) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {}
	zest_UpdateDevice(context->device);
	zest_microsecs start = zest_Microsecs();
	zest_bool began = zest_BeginFrame(context);
	run->slowest_begin = ZEST__MAX(run->slowest_begin, zest_Microsecs() - start);
	if (began) {
		run->began_count++;
		metal_end_windowed_frame(context, set_viewport);
		metal_check_frame_presented(context, run);
	}
	if (zest_SwapchainWasRecreated(context)) {
		run->recreate_count++;
		//A recreate loses the clear colour
		metal_set_test_clear_color(context);
	}
	return began;
}

static MetalFrameRun run_metal_windowed_frames(zest_context context, int frame_count, zest_bool stop_at_recreate) {
	MetalFrameRun run = {};
	for (int frame = 0; frame != frame_count; ++frame) {
		run_metal_windowed_frame(context, ZEST_FALSE, &run);
		if (stop_at_recreate && run.recreate_count) break;
	}
	return run;
}

//Every frame presented once and let its drawable go, no zest_BeginFrame stalled, and Metal's validation said nothing
static int metal_frame_run_failed(zest_context context, const MetalFrameRun *run, const char *phase) {
	int failed = run->bad_present_count != 0 || run->slowest_begin > metal_begin_frame_limit || zest_GetValidationErrorCount(context->device) != 0;
	if (failed) {
		ZEST_PRINT("Windowed phase %s: %i frames began, %i recreates, %i bad presents, slowest zest_BeginFrame %llu microseconds, %u validation errors.", phase, run->began_count, run->recreate_count, run->bad_present_count, (zest_ull)run->slowest_begin, zest_GetValidationErrorCount(context->device));
	}
	return failed;
}

static int metal_resize_test_window(zest_context context, SDL_Window *window, int width, int height) {
	SDL_SetWindowSize(window, width, height);
	MetalFrameRun run = run_metal_windowed_frames(context, 10, ZEST_TRUE);
	//The first zest_BeginFrame after the resize has to see it, before any pass runs at the old size on a drawable of the new one
	int failed = run.recreate_count != 1 || run.began_count != 0;
	int framebuffer_width = 0, framebuffer_height = 0, window_width = 0, window_height = 0;
	context->window_data.window_sizes_callback(&context->window_data, &framebuffer_width, &framebuffer_height, &window_width, &window_height);
	zest_extent2d_t extent = zest_GetSwapChainExtent(context);
	CGSize drawable_size = [ZEST_MTL_BACKEND(context, context)->layer drawableSize];
	failed |= window_width != width || window_height != height;
	failed |= extent.width != (zest_uint)framebuffer_width || extent.height != (zest_uint)framebuffer_height;
	failed |= extent.width != (zest_uint)drawable_size.width || extent.height != (zest_uint)drawable_size.height;
	failed |= metal_frame_run_failed(context, &run, "resize");
	run = run_metal_windowed_frames(context, 5, ZEST_FALSE);
	failed |= run.began_count != 5 || run.recreate_count != 0;
	failed |= metal_frame_run_failed(context, &run, "after resize");
	return failed;
}

//Both vsync calls schedule a toggle, which the layer takes in place with no new swapchain
static int metal_toggle_test_vsync(zest_context context, zest_bool enable) {
	CAMetalLayer *layer = ZEST_MTL_BACKEND(context, context)->layer;
	BOOL sync_before = [layer displaySyncEnabled];
	if (enable) {
		zest_EnableVSync(context);
	} else {
		zest_DisableVSync(context);
	}
	MetalFrameRun run = run_metal_windowed_frames(context, 5, ZEST_FALSE);
	int failed = run.began_count != 5 || run.recreate_count != 0;
	failed |= [layer displaySyncEnabled] == sync_before;
	failed |= ([layer displaySyncEnabled] == YES) != (ZEST__FLAGGED(context->flags, zest_context_flag_vsync_enabled) != 0);
	failed |= metal_frame_run_failed(context, &run, "vsync");
	return failed;
}

int test_metal_windowed_frames(void) {
	MetalTestWindow window;
	if (!metal_open_test_window(&window, "Metal Windowed Frames")) {
		metal_close_test_window(&window);
		return 1;
	}
	SDL_Window *sdl_window = (SDL_Window *)window.window_data.window_handle;
	zest_device device = zest_implsdl2_CreateMetalDevice(&window.window_data, MetalTestFlags());
	if (!device) {
		metal_close_test_window(&window);
		return 1;
	}
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateContext(device, &window.window_data, &info);
	int failed = context == NULL;
	if (context) {
		metal_set_test_clear_color(context);
		//The first frame's drawable is held on to here, to see who else still owns it once later frames have been shown
		zest_UpdateDevice(device);
		failed |= !zest_BeginFrame(context);
		id<CAMetalDrawable> first_drawable = [ZEST_MTL_BACKEND(context, context)->drawable retain];
		metal_end_windowed_frame(context, ZEST_FALSE);
		MetalFrameRun run = run_metal_windowed_frames(context, 10, ZEST_FALSE);
		failed |= run.began_count != 10 || run.recreate_count != 0;
		failed |= metal_frame_run_failed(context, &run, "first frames");
		zest_WaitForIdleDevice(device);
		if ([first_drawable retainCount] != 1) {
			ZEST_PRINT("The first frame's drawable still has %lu owners ten frames later, where only this test should hold it.", (unsigned long)[first_drawable retainCount]);
			failed = 1;
		}
		[first_drawable release];
		failed |= zest_GetSwapChainExtent(context).width < 320 || zest_GetSwapChainExtent(context).height < 200;

		//Every resize gives the layer new drawables, and the old size's textures have to go once nothing shows them
		zest_size resized_sizes[3];
		for (int round = 0; round != 3; ++round) {
			failed |= metal_resize_test_window(context, sdl_window, 400, 300);
			failed |= metal_resize_test_window(context, sdl_window, 320, 200);
			zest_WaitForIdleDevice(device);
			resized_sizes[round] = metal_allocated_size();
		}
		if (resized_sizes[2] != resized_sizes[1]) {
			ZEST_PRINT("Metal's allocated size went from %llu to %llu between the second and third round of resizes.", (zest_ull)resized_sizes[1], (zest_ull)resized_sizes[2]);
			failed = 1;
		}

		failed |= metal_toggle_test_vsync(context, ZEST_TRUE);
		failed |= metal_toggle_test_vsync(context, ZEST_FALSE);

		//A minimised or hidden window's layer still hands out drawables
		SDL_MinimizeWindow(sdl_window);
		run = run_metal_windowed_frames(context, 5, ZEST_FALSE);
		failed |= metal_frame_run_failed(context, &run, "minimised");
		SDL_RestoreWindow(sdl_window);
		run = run_metal_windowed_frames(context, 5, ZEST_FALSE);
		failed |= metal_frame_run_failed(context, &run, "restored");
		SDL_HideWindow(sdl_window);
		run = run_metal_windowed_frames(context, 5, ZEST_FALSE);
		failed |= metal_frame_run_failed(context, &run, "hidden");
		SDL_ShowWindow(sdl_window);
		run = run_metal_windowed_frames(context, 5, ZEST_FALSE);
		failed |= metal_frame_run_failed(context, &run, "shown");

		//The viewport and its scissor are the swapchain's size, which the drawable's has to match
		run = ZEST__ZERO_INIT(MetalFrameRun);
		metal_viewport_pass_count = 0;
		for (int frame = 0; frame != 3; ++frame) {
			run_metal_windowed_frame(context, ZEST_TRUE, &run);
		}
		failed |= run.began_count != 3 || metal_viewport_pass_count != 3;
		failed |= metal_frame_run_failed(context, &run, "screen sized viewport");

		//Frames run on whichever device queue the slot is given
		zest_uint slot = context->graphics_queue_index;
		run = ZEST__ZERO_INIT(MetalFrameRun);
		for (int frame = 0; frame != 6; ++frame) {
			zest_uint preferred_index = (zest_uint)(frame % 2);
			context->preferred_queue_index[slot] = preferred_index;
			context->has_preferred_queue[slot] = ZEST_TRUE;
			failed |= !run_metal_windowed_frame(context, ZEST_FALSE, &run);
			failed |= context->preferred_queue_index[slot] != preferred_index;
		}
		context->has_preferred_queue[slot] = ZEST_FALSE;
		failed |= metal_frame_run_failed(context, &run, "queue switch");

		//A frame with no work still clears and presents its drawable, and advances the frame timeline
		run = ZEST__ZERO_INIT(MetalFrameRun);
		for (int frame = 0; frame != 3; ++frame) {
			SDL_Event event;
			while (SDL_PollEvent(&event)) {}
			zest_UpdateDevice(device);
			zest_microsecs start = zest_Microsecs();
			zest_bool began = zest_BeginFrame(context);
			run.slowest_begin = ZEST__MAX(run.slowest_begin, zest_Microsecs() - start);
			failed |= !began;
			if (began) {
				zest_EndFrame(context, 0);
				metal_check_frame_presented(context, &run);
			}
		}
		failed |= zest_WaitForSignal(&context->queue_ordering_timelines[slot], ZEST_SECONDS_IN_MICROSECONDS(1)) != zest_semaphore_status_success;
		failed |= metal_frame_run_failed(context, &run, "no work");

		//The swapchain can also go out of date between the acquire and the present, which is the present's to see
		zest_UpdateDevice(device);
		failed |= !zest_BeginFrame(context);
		SDL_SetWindowSize(sdl_window, 360, 240);
		metal_end_windowed_frame(context, ZEST_FALSE);
		failed |= !zest_SwapchainWasRecreated(context);
		metal_set_test_clear_color(context);
		run = run_metal_windowed_frames(context, 3, ZEST_FALSE);
		failed |= run.began_count != 3 || run.recreate_count != 0 || zest_GetSwapChainExtent(context).width != (zest_uint)[ZEST_MTL_BACKEND(context, context)->layer drawableSize].width;
		failed |= metal_frame_run_failed(context, &run, "resize inside a frame");

		//A frame that never reaches its present leaves the drawable with the context, and the next acquire has to let it go.
		//With display sync on, a layer makes the frame after two held drawables wait a second.
		failed |= metal_toggle_test_vsync(context, ZEST_TRUE);
		run = ZEST__ZERO_INIT(MetalFrameRun);
		for (int frame = 0; frame != 4; ++frame) {
			zest_UpdateDevice(device);
			zest_microsecs start = zest_Microsecs();
			failed |= !zest_BeginFrame(context);
			run.slowest_begin = ZEST__MAX(run.slowest_begin, zest_Microsecs() - start);
			failed |= ZEST_MTL_BACKEND(context, context)->drawable == nil;
			ZEST__UNFLAG(context->flags, zest_context_flag_swap_chain_was_acquired);
		}
		for (int frame = 0; frame != 3; ++frame) {
			failed |= !run_metal_windowed_frame(context, ZEST_FALSE, &run);
		}
		failed |= metal_frame_run_failed(context, &run, "abandoned frames");
		failed |= metal_toggle_test_vsync(context, ZEST_FALSE);

		//A second zest_BeginFrame with no zest_EndFrame is refused, and the drawable the first one took is still presented
		run = ZEST__ZERO_INIT(MetalFrameRun);
		zest_UpdateDevice(device);
		failed |= !zest_BeginFrame(context);
		zest_UpdateDevice(device);
		failed |= zest_BeginFrame(context) != ZEST_FALSE;
		failed |= zest_GetValidationErrorCount(device) != 1;
		zest_ResetValidationErrors(device);
		failed |= ZEST_MTL_BACKEND(context, context)->drawable == nil;
		zest_EndFrame(context, 0);
		metal_check_frame_presented(context, &run);
		for (int frame = 0; frame != 3; ++frame) {
			failed |= !run_metal_windowed_frame(context, ZEST_FALSE, &run);
		}
		failed |= metal_frame_run_failed(context, &run, "no end frame");
		zest_DestroyContext(context);
	}
	failed |= finish_metal_device(device);
	metal_close_test_window(&window);
	return failed;
}

//A context's layer, its SDL view and its drawables must all go with it
int test_metal_windowed_context(void) {
	MetalTestWindow window;
	if (!metal_open_test_window(&window, "Metal Windowed Context")) {
		metal_close_test_window(&window);
		return 1;
	}
	zest_device device = zest_implsdl2_CreateMetalDevice(&window.window_data, MetalTestFlags());
	if (!device) {
		metal_close_test_window(&window);
		return 1;
	}
	zest_size allocated_sizes[5];
	int failed = 0;
	for (int cycle = 0; cycle != 5; ++cycle) {
		zest_create_context_info_t info = zest_CreateContextInfo();
		zest_context context = zest_CreateContext(device, &window.window_data, &info);
		failed |= context == NULL;
		if (context) {
			metal_set_test_clear_color(context);
			MetalFrameRun run = run_metal_windowed_frames(context, 4, ZEST_FALSE);
			failed |= run.began_count != 4;
			failed |= metal_frame_run_failed(context, &run, "context lifecycle");
			zest_DestroyContext(context);
		}
		allocated_sizes[cycle] = metal_allocated_size();
	}
	//Metal can hand a context's memory back a cycle late, so a fall is not a leak and only growth fails
	if (allocated_sizes[4] > allocated_sizes[3] || allocated_sizes[3] > allocated_sizes[2]) {
		ZEST_PRINT("Metal's allocated size grew across the third, fourth and fifth windowed context: %llu, %llu and %llu.", (zest_ull)allocated_sizes[2], (zest_ull)allocated_sizes[3], (zest_ull)allocated_sizes[4]);
		failed = 1;
	}
	failed |= finish_metal_device(device);
	metal_close_test_window(&window);
	return failed;
}

static const char *metal_ordering_shaders = METAL_TEST_TABLE_TYPES
	"struct Push { uint source_index; uint seed; };\n"
	"struct Varyings { float4 position [[position]]; };\n"
	"kernel void write_seed(constant Push& pc [[buffer(4)]], device const void* slot5 [[buffer(10)]], uint3 cell [[thread_position_in_grid]]) {\n"
	"	spvDescriptorArray<device Words*> buffers {slot5};\n"
	"	uint word = cell.y * 8u + cell.x;\n"
	"	buffers[pc.source_index]->values[word] = pc.seed * 1000u + word;\n"
	"}\n"
	"vertex Varyings cover_target(uint vertex_index [[vertex_id]]) {\n"
	"	Varyings out;\n"
	"	float2 corner = float2(float((vertex_index << 1) & 2u), float(vertex_index & 2u));\n"
	"	out.position = float4(corner * 2.0 - 1.0, 0.0, 1.0);\n"
	"	return out;\n"
	"}\n"
	"fragment float4 read_words(Varyings in [[stage_in]], constant Push& pc [[buffer(4)]], device const void* slot5 [[buffer(10)]]) {\n"
	"	spvDescriptorArray<device Words*> buffers {slot5};\n"
	"	uint2 cell = uint2(in.position.xy);\n"
	"	uint value = buffers[pc.source_index]->values[cell.y * 8u + cell.x];\n"
	"	return float4(float(value & 255u), float((value >> 8u) & 255u), float((value >> 16u) & 255u), 255.0) / 255.0;\n"
	"}\n";

typedef struct metal_ordering_push_t {
	zest_uint source_index;
	zest_uint seed;
} metal_ordering_push_t;

//A compute pass writes a buffer through the table, a render pass reads it through the table and a copy takes the result out, 200 times back
//to back on one queue with nothing declared but the queue's fence. A pass that overtook the one before it would carry the last iteration's
//seed, which the readback would show. Compute and render passes do overlap on Apple GPUs when nothing orders them.
int test_metal_untracked_pass_ordering(void) {
	enum { iteration_count = 200, target_size = 8, result_size = target_size * target_size * 4 };
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	id<MTLDevice> native_device = zest_GetMetalDevice(device);
	id<MTLComputePipelineState> write_pipeline = metal_new_compute_pipeline(device, metal_ordering_shaders, "write_seed");
	id<MTLRenderPipelineState> read_pipeline = nil;
	id<MTLTexture> target = nil;
	id<MTLBuffer> results = [native_device newBufferWithLength:iteration_count * result_size options:MTLResourceStorageModeShared | MTLResourceHazardTrackingModeUntracked];
	@autoreleasepool {
		MTLCompileOptions *compile_options = [[MTLCompileOptions alloc] init];
		compile_options.languageVersion = MTLLanguageVersion3_1;
		NSError *error = nil;
		id<MTLLibrary> library = [native_device newLibraryWithSource:[NSString stringWithUTF8String:metal_ordering_shaders] options:compile_options error:&error];
		[compile_options release];
		id<MTLFunction> vertex_function = [library newFunctionWithName:@"cover_target"];
		id<MTLFunction> fragment_function = [library newFunctionWithName:@"read_words"];
		MTLRenderPipelineDescriptor *pipeline_descriptor = [[MTLRenderPipelineDescriptor alloc] init];
		pipeline_descriptor.vertexFunction = vertex_function;
		pipeline_descriptor.fragmentFunction = fragment_function;
		pipeline_descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
		read_pipeline = library ? [native_device newRenderPipelineStateWithDescriptor:pipeline_descriptor error:&error] : nil;
		[pipeline_descriptor release];
		[fragment_function release];
		[vertex_function release];
		[library release];
		MTLTextureDescriptor *target_descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:target_size height:target_size mipmapped:NO];
		target_descriptor.usage = MTLTextureUsageRenderTarget;
		target_descriptor.storageMode = MTLStorageModePrivate;
		target_descriptor.hazardTrackingMode = MTLHazardTrackingModeUntracked;
		target = [native_device newTextureWithDescriptor:target_descriptor];
	}
	//Device local, so the only way to its contents is the GPU address in the table
	zest_buffer_info_t device_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	zest_buffer source_buffer = zest_CreateBuffer(device, target_size * target_size * sizeof(zest_uint), &device_info);
	if (!write_pipeline || !read_pipeline || !target || !results || !source_buffer) {
		[results release];
		[target release];
		[read_pipeline release];
		[write_pipeline release];
		zest_DestroyDevice(device);
		return 1;
	}
	memset([results contents], 0xEE, iteration_count * result_size);
	metal_ordering_push_t push;
	push.source_index = 2;
	device->platform->update_bindless_storage_buffer_descriptor(device, zest_storage_buffer_binding, push.source_index, source_buffer, 0, device->bindless_set);
	zest_mtl_descriptor_pool_backend_t *pool_backend = ZEST_MTL_BACKEND(descriptor_set, device->bindless_set)->pool;
	id<MTLBuffer> table_buffers[zest_max_global_binding_number];
	NSUInteger table_offsets[zest_max_global_binding_number];
	for (zest_uint binding_number = 0; binding_number != zest_max_global_binding_number; ++binding_number) {
		table_buffers[binding_number] = pool_backend->table;
		table_offsets[binding_number] = (NSUInteger)pool_backend->regions.offsets[binding_number];
	}

	//Holding an immediate command buffer keeps the queue, and ending it waits for everything committed to the queue before it
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	int failed = queue == NULL;
	if (queue) {
		zest_mtl_queue_backend_t *queue_backend = ZEST_MTL_BACKEND(queue, queue);
		for (int iteration = 0; iteration != iteration_count; ++iteration) {
			@autoreleasepool {
				push.seed = (zest_uint)iteration + 1;
				id<MTLCommandBuffer> command_buffer = [queue_backend->queue commandBuffer];
				id<MTLComputeCommandEncoder> write_encoder = [command_buffer computeCommandEncoder];
				[write_encoder waitForFence:queue_backend->fence];
				[write_encoder setComputePipelineState:write_pipeline];
				metal_bind_table(device, write_encoder);
				[write_encoder setBytes:&push length:sizeof(push) atIndex:ZEST__MTL_PUSH_CONSTANT_BUFFER_INDEX];
				[write_encoder dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(target_size, target_size, 1)];
				[write_encoder updateFence:queue_backend->fence];
				[write_encoder endEncoding];

				MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
				pass.colorAttachments[0].texture = target;
				pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
				pass.colorAttachments[0].storeAction = MTLStoreActionStore;
				id<MTLRenderCommandEncoder> read_encoder = [command_buffer renderCommandEncoderWithDescriptor:pass];
				[read_encoder waitForFence:queue_backend->fence beforeStages:MTLRenderStageVertex];
				[read_encoder setRenderPipelineState:read_pipeline];
				[read_encoder setFragmentBuffers:table_buffers offsets:table_offsets withRange:NSMakeRange(ZEST__MTL_BINDLESS_BUFFER_INDEX, zest_max_global_binding_number)];
				[read_encoder setFragmentBytes:&push length:sizeof(push) atIndex:ZEST__MTL_PUSH_CONSTANT_BUFFER_INDEX];
				[read_encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
				[read_encoder updateFence:queue_backend->fence afterStages:MTLRenderStageFragment];
				[read_encoder endEncoding];

				id<MTLBlitCommandEncoder> copy_encoder = [command_buffer blitCommandEncoder];
				[copy_encoder waitForFence:queue_backend->fence];
				[copy_encoder copyFromTexture:target sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(target_size, target_size, 1)
					toBuffer:results destinationOffset:(NSUInteger)iteration * result_size destinationBytesPerRow:target_size * 4 destinationBytesPerImage:result_size];
				[copy_encoder updateFence:queue_backend->fence];
				[copy_encoder endEncoding];
				[command_buffer commit];
			}
		}
		failed |= !zest_imm_EndCommandBuffer(queue);
	}
	const unsigned char *result_bytes = (const unsigned char *)[results contents];
	int wrong_iterations = 0;
	for (int iteration = 0; iteration != iteration_count; ++iteration) {
		for (int word = 0; word != target_size * target_size; ++word) {
			zest_uint expected = (zest_uint)(iteration + 1) * 1000 + word;
			const unsigned char *pixel = result_bytes + iteration * result_size + word * 4;
			if (pixel[0] != (expected & 255) || pixel[1] != ((expected >> 8) & 255) || pixel[2] != ((expected >> 16) & 255) || pixel[3] != 255) {
				wrong_iterations++;
				break;
			}
		}
	}
	if (wrong_iterations) {
		ZEST_PRINT("Metal Untracked Pass Ordering: %i of %i iterations read back the wrong seed.", wrong_iterations, (int)iteration_count);
		failed = 1;
	}
	zest_FreeBufferNow(source_buffer);
	[results release];
	[target release];
	[read_pipeline release];
	[write_pipeline release];
	return failed | finish_metal_device(device);
}

static const char *metal_out_of_bounds_kernel =
	"#include <metal_stdlib>\n"
	"using namespace metal;\n"
	"kernel void read_past_end(device uint* words [[buffer(0)]], device uint* result [[buffer(1)]], constant uint& word_index [[buffer(2)]]) {\n"
	"	result[0] = words[word_index];\n"
	"}\n";

//One deliberate API mistake must give a count of exactly one, and with shader validation one deliberate out-of-bounds read must add one more
int test_metal_validation_reports(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_mtl_device_backend_t *backend = ZEST_MTL_BACKEND(device, device);
	int failed = !backend->api_validation;
	failed |= zest_GetValidationErrorCount(device) != 0;
	[[backend->device newBufferWithLength:0 options:MTLResourceStorageModeShared] release];
	failed |= zest_GetValidationErrorCount(device) != 1;
	//The same mistake again is the same message, which is counted once
	[[backend->device newBufferWithLength:0 options:MTLResourceStorageModeShared] release];
	failed |= zest_GetValidationErrorCount(device) != 1;

	if (backend->shader_validation) {
		id<MTLComputePipelineState> pipeline = metal_new_compute_pipeline(device, metal_out_of_bounds_kernel, "read_past_end");
		id<MTLBuffer> words = [backend->device newBufferWithLength:64 options:MTLResourceStorageModeShared];
		id<MTLBuffer> result = [backend->device newBufferWithLength:64 options:MTLResourceStorageModeShared];
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_compute);
		failed |= !pipeline || !queue;
		if (pipeline && queue) {
			zest_mtl_queue_backend_t *queue_backend = ZEST_MTL_BACKEND(queue, queue);
			zest_uint word_index = 100000;
			@autoreleasepool {
				id<MTLComputeCommandEncoder> encoder = [queue_backend->command_buffer computeCommandEncoder];
				[encoder waitForFence:queue_backend->fence];
				[encoder setComputePipelineState:pipeline];
				[encoder setBuffer:words offset:0 atIndex:0];
				[encoder setBuffer:result offset:0 atIndex:1];
				[encoder setBytes:&word_index length:sizeof(word_index) atIndex:2];
				[encoder dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
				[encoder updateFence:queue_backend->fence];
				[encoder endEncoding];
			}
			//Shader validation leaves the command buffer completed and puts the fault in its logs
			failed |= !zest_imm_EndCommandBuffer(queue);
			failed |= zest_GetValidationErrorCount(device) != 2;
		} else if (queue) {
			zest_imm_EndCommandBuffer(queue);
		}
		[result release];
		[words release];
		[pipeline release];
	}
	zest_ResetValidationErrors(device);
	return failed | finish_metal_device(device);
}

#define METAL_SPIRV_MAGIC 0x07230203
#define METAL_COMPILE_SOURCE_COUNT 8
#define METAL_COMPILE_THREAD_COUNT 4

static const char *metal_compile_test_files[METAL_COMPILE_SOURCE_COUNT] = {
	"buffer_verify.comp", "buffer_write.comp", "image_verify.comp", "image_write2.comp", "instance.vert", "instance.frag", "vertex.vert", "vertex.frag",
};

static const char *metal_macro_compute_source =
	"#version 450\n"
	"layout(local_size_x = 1) in;\n"
	"#if FIRST != 3\n"
	"#error FIRST must be 3\n"
	"#endif\n"
	"#ifndef SECOND\n"
	"#error SECOND is missing\n"
	"#endif\n"
	"void main() {}\n";

static const char *metal_stage_vertex_source =
	"#version 450\n"
	"layout(location = 0) in vec3 position;\n"
	"void main() { gl_Position = vec4(position, 1.0); }\n";

static const char *metal_stage_fragment_source =
	"#version 450\n"
	"layout(location = 0) out vec4 color;\n"
	"void main() { color = vec4(1.0); }\n";

static zest_shader_type metal_shader_type_of_file(const char *file_name) {
	const char *extension = strrchr(file_name, '.');
	if (strcmp(extension, ".vert") == 0) return zest_vertex_shader;
	if (strcmp(extension, ".frag") == 0) return zest_fragment_shader;
	return zest_compute_shader;
}

struct MetalCompileWorker {
	zest_device device;
	zest_file *sources;				//METAL_COMPILE_SOURCE_COUNT, in the order of metal_compile_test_files
	zest_uint compiled_count;
};

//Each compile asks for the compiler as compile_shader does, so the first ones race to create it
static void *metal_compile_worker(void *parameter) {
	MetalCompileWorker *worker = (MetalCompileWorker *)parameter;
	for (zest_uint source_index = 0; source_index != METAL_COMPILE_SOURCE_COUNT; ++source_index) {
		const char *file_name = metal_compile_test_files[source_index];
		shaderc_compiler_t compiler = zest__mtl_shader_compiler(worker->device);
		if (!compiler) continue;
		shaderc_compilation_result_t result = zest__mtl_compile_glsl(compiler, worker->sources[source_index], strlen(worker->sources[source_index]), metal_shader_type_of_file(file_name), file_name, NULL);
		if (!result) continue;
		zest_uint magic = 0;
		if (shaderc_result_get_compilation_status(result) == shaderc_compilation_status_success && shaderc_result_get_length(result) >= sizeof(magic)) {
			memcpy(&magic, shaderc_result_get_bytes(result), sizeof(magic));
		}
		if (magic == METAL_SPIRV_MAGIC) worker->compiled_count++;
		shaderc_result_release(result);
	}
	return NULL;
}

int test_metal_shader_compile(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_mtl_device_backend_t *backend = ZEST_MTL_BACKEND(device, device);
	//The threads below only race to create the compiler on a device that hasn't compiled yet
	int failed = backend->shaderc_compiler != NULL;

	zest_file sources[METAL_COMPILE_SOURCE_COUNT] = {};
	for (zest_uint source_index = 0; source_index != METAL_COMPILE_SOURCE_COUNT; ++source_index) {
		char path[256];
		snprintf(path, sizeof(path), "examples/SDL2/zest-tests/shaders/glsl/%s", metal_compile_test_files[source_index]);
		sources[source_index] = zest_ReadEntireFile(device, path, ZEST_TRUE);
		failed |= sources[source_index] == NULL;
	}
	if (!failed) {
		MetalCompileWorker workers[METAL_COMPILE_THREAD_COUNT] = {};
		pthread_t threads[METAL_COMPILE_THREAD_COUNT];
		zest_uint thread_count = 0;
		for (; thread_count != METAL_COMPILE_THREAD_COUNT; ++thread_count) {
			workers[thread_count].device = device;
			workers[thread_count].sources = sources;
			if (pthread_create(&threads[thread_count], NULL, metal_compile_worker, &workers[thread_count]) != 0) break;
		}
		zest_uint compiled_count = 0;
		for (zest_uint thread = 0; thread != thread_count; ++thread) {
			pthread_join(threads[thread], NULL);
			compiled_count += workers[thread].compiled_count;
		}
		ZEST_PRINT("\t%u of %u concurrent compiles gave SPIR-V", compiled_count, METAL_COMPILE_THREAD_COUNT * METAL_COMPILE_SOURCE_COUNT);
		failed |= compiled_count != METAL_COMPILE_THREAD_COUNT * METAL_COMPILE_SOURCE_COUNT;
	}
	for (zest_uint source_index = 0; source_index != METAL_COMPILE_SOURCE_COUNT; ++source_index) {
		if (sources[source_index]) zest_FreeFile(device, sources[source_index]);
	}

	const char *stage_sources[3] = { metal_stage_vertex_source, metal_stage_fragment_source, glsl_empty_compute };
	zest_shader_type stage_types[3] = { zest_vertex_shader, zest_fragment_shader, zest_compute_shader };
	for (int stage = 0; stage != 3; ++stage) {
		zest_shader_handle stage_handle = zest_CreateShader(device, stage_sources[stage], stage_types[stage], "zest_stage_test", NULL, ZEST_TRUE);
		zest_uint magic = shader_binary_magic(zest_GetShader(stage_handle));
		if (magic != METAL_SPIRV_MAGIC) ZEST_PRINT("\tStage %i compiled to a binary starting 0x%08X, not SPIR-V", stage, magic);
		failed |= magic != METAL_SPIRV_MAGIC;
		zest_FreeShader(stage_handle);
	}

	//One macro with a value and one without
	zest_shader_handle macro_handle = zest_CreateShader(device, glsl_empty_compute, zest_compute_shader, "zest_macro_test", NULL, ZEST_TRUE);
	zest_shader_options options = zest_CreateShaderOptions(device);
	zest_AddMacroDefinition(options, "FIRST", "3");
	zest_AddMacroDefinition(options, "SECOND", NULL);
	failed |= !compile_test_source(device, macro_handle, metal_macro_compute_source, options);
	zest_shader macro_shader = zest_GetShader(macro_handle);
	char *binary_before = macro_shader->binary;
	zest_size size_before = macro_shader->binary_size;
	zest_key hash_before = zest_Hash(macro_shader->binary, (zest_uint)size_before, ZEST_HASH_SEED);

	//A failed compile keeps the last binary, names its error, and isn't a validation error
	zest_shader_options no_macros = zest_CreateShaderOptions(device);
	ZEST_PRINT("\tThe next shader compilation error is expected");
	failed |= zest_CompileShader(macro_handle, no_macros) != ZEST_FALSE;
	zest_FreeShaderOptions(no_macros);
	failed |= macro_shader->binary != binary_before || macro_shader->binary_size != size_before || zest_Hash(macro_shader->binary, (zest_uint)size_before, ZEST_HASH_SEED) != hash_before;
	failed |= zest_GetShaderLastError(macro_handle)[0] == 0;
	failed |= zest_GetValidationErrorCount(device) != 0;
	failed |= !zest_CompileShader(macro_handle, options);
	failed |= zest_GetShaderLastError(macro_handle)[0] != 0;
	zest_FreeShaderOptions(options);
	zest_FreeShader(macro_handle);
	return failed | finish_metal_device(device);
}

static const char *metal_reflect_vertex_source =
	"#version 450\n"
	"layout(location = 0) in vec3 position;\n"
	"layout(location = 1) in vec2 uv;\n"
	"layout(location = 3) in vec4 color;\n"
	"void main() { gl_Position = vec4(position + vec3(uv, 0.0), 1.0) * color; }\n";

static const char *metal_constant_index_source =
	"#version 450\n"
	"#extension GL_EXT_nonuniform_qualifier : require\n"
	"layout(set = 0, binding = 0) uniform sampler constant_samplers[];\n"
	"layout(set = 0, binding = 1) uniform texture2D textures[];\n"
	"layout(push_constant) uniform constants { uint texture_index; } push;\n"
	"layout(location = 0) out vec4 color;\n"
	"void main() { color = texture(sampler2D(textures[push.texture_index], constant_samplers[1]), vec2(0.5)); }\n";

static const char *metal_buffer_length_source =
	"#version 450\n"
	"#extension GL_EXT_nonuniform_qualifier : require\n"
	"layout(local_size_x = 1) in;\n"
	"layout(set = 0, binding = 5) buffer words_buffer { uint values[]; } words[];\n"
	"layout(push_constant) uniform constants { uint buffer_index; } push;\n"
	"void main() { words[push.buffer_index].values[0] = words[push.buffer_index].values.length(); }\n";

//Compiles GLSL into one of the test's shaders and converts its SPIR-V
static zest_bool metal_convert_source(zest_device device, zest_shader_handle shader_handle, const char *source, zest_mtl_shader_function_t *out_function, char *out_error, zest_size error_size) {
	if (!compile_test_source(device, shader_handle, source, NULL)) {
		snprintf(out_error, error_size, "%s", zest_GetShaderLastError(shader_handle));
		return ZEST_FALSE;
	}
	zest_shader shader = zest_GetShader(shader_handle);
	return zest__mtl_shader_function(device, shader, shader->type, ZEST_FALSE, out_function, out_error, error_size);
}

static zest_bool metal_convert_file(zest_device device, zest_shader_handle shader_handle, const char *path, zest_mtl_shader_function_t *out_function, char *out_error, zest_size error_size) {
	zest_file source = zest_ReadEntireFile(device, path, ZEST_TRUE);
	if (!source) {
		snprintf(out_error, error_size, "The file couldn't be read.");
		return ZEST_FALSE;
	}
	zest_bool converted = metal_convert_source(device, shader_handle, source, out_function, out_error, error_size);
	zest_FreeFile(device, source);
	return converted;
}

//Every GLSL file under examples, so a new example's shaders are converted too. Returns how many of the two alias shaders converted.
static int metal_convert_example_shaders(zest_device device, zest_shader_handle *stage_handles, zest_uint *out_found_count, zest_uint *out_converted_count) {
	char examples_folder[] = "examples";
	char *roots[] = { examples_folder, NULL };
	FTS *walk = fts_open(roots, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
	if (!walk) return 0;
	int alias_count = 0;
	FTSENT *entry = NULL;
	while ((entry = fts_read(walk)) != NULL) {
		if (entry->fts_info != FTS_F) continue;
		const char *extension = strrchr(entry->fts_name, '.');
		if (!extension) continue;
		zest_shader_type type = zest_compute_shader;
		if (strcmp(extension, ".vert") == 0) type = zest_vertex_shader;
		else if (strcmp(extension, ".frag") == 0) type = zest_fragment_shader;
		else if (strcmp(extension, ".comp") != 0) continue;
		(*out_found_count)++;
		zest_mtl_shader_function_t function = {};
		char error[1024];
		if (!metal_convert_file(device, stage_handles[type], entry->fts_path, &function, error, sizeof(error))) {
			ZEST_PRINT("\t%s didn't convert: %s", entry->fts_path, error);
			continue;
		}
		(*out_converted_count)++;
		if (strstr(entry->fts_path, "zest-cascading-shadows/shaders/glsl/scene.frag") || strstr(entry->fts_path, "zest-pbr-deferred/shaders/glsl/deferred_lighting.frag")) alias_count++;
	}
	fts_close(walk);
	return alias_count;
}

int test_metal_shader_conversion(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_mtl_device_backend_t *backend = ZEST_MTL_BACKEND(device, device);
	int failed = 0;
	char error[1024];
	zest_shader_handle stage_handles[3];
	stage_handles[zest_vertex_shader] = zest_CreateShader(device, metal_stage_vertex_source, zest_vertex_shader, "zest_conversion_vertex", NULL, ZEST_TRUE);
	stage_handles[zest_fragment_shader] = zest_CreateShader(device, metal_stage_fragment_source, zest_fragment_shader, "zest_conversion_fragment", NULL, ZEST_TRUE);
	stage_handles[zest_compute_shader] = zest_CreateShader(device, glsl_empty_compute, zest_compute_shader, "zest_conversion_compute", NULL, ZEST_TRUE);

	//The cost of a shader's first conversion, then of finding it in the cache
	zest_file timed_source = zest_ReadEntireFile(device, "examples/SDL2/zest-pbr-texture/shaders/glsl/pbr_texture.frag", ZEST_TRUE);
	failed |= !timed_source || !compile_test_source(device, stage_handles[zest_fragment_shader], timed_source, NULL);
	if (timed_source) zest_FreeFile(device, timed_source);
	zest_shader fragment_shader = zest_GetShader(stage_handles[zest_fragment_shader]);
	zest_mtl_shader_function_t first = {};
	zest_mtl_shader_function_t again = {};
	zest_microsecs first_start = zest_Microsecs();
	failed |= !zest__mtl_shader_function(device, fragment_shader, zest_fragment_shader, ZEST_FALSE, &first, error, sizeof(error));
	zest_microsecs first_time = zest_Microsecs() - first_start;
	zest_microsecs again_start = zest_Microsecs();
	failed |= !zest__mtl_shader_function(device, fragment_shader, zest_fragment_shader, ZEST_FALSE, &again, error, sizeof(error));
	zest_microsecs again_time = zest_Microsecs() - again_start;
	ZEST_PRINT("\tpbr_texture.frag converted in %llu us, and came from the cache in %llu us", (unsigned long long)first_time, (unsigned long long)again_time);
	failed |= !first.function || again.function != first.function || zest_map_size(backend->shader_functions) != 1;

	//Another binary is another entry
	zest_mtl_shader_function_t other = {};
	failed |= !metal_convert_source(device, stage_handles[zest_fragment_shader], metal_stage_fragment_source, &other, error, sizeof(error));
	failed |= other.function == first.function || zest_map_size(backend->shader_functions) != 2;

	zest_uint found_count = 0;
	zest_uint converted_count = 0;
	zest_microsecs walk_start = zest_Microsecs();
	int alias_count = metal_convert_example_shaders(device, stage_handles, &found_count, &converted_count);
	zest_microsecs walk_time = zest_Microsecs() - walk_start;
	ZEST_PRINT("\t%u of %u example shaders converted, %i of the 2 alias shaders among them, %u distinct in the cache", converted_count, found_count, alias_count, zest_map_size(backend->shader_functions));
	ZEST_PRINT("\tEach took %llu us on average to compile from GLSL and convert", (unsigned long long)(found_count ? walk_time / found_count : 0));
	failed |= found_count == 0 || converted_count != found_count || alias_count != 2;

	zest_mtl_shader_function_t function = {};
	failed |= !metal_convert_file(device, stage_handles[zest_compute_shader], "examples/SDL2/zest-tests/shaders/glsl/buffer_write.comp", &function, error, sizeof(error));
	failed |= function.local_size[0] != 8 || function.local_size[1] != 8 || function.local_size[2] != 1;
	failed |= !metal_convert_file(device, stage_handles[zest_compute_shader], "examples/SDL2/zest-tests/shaders/glsl/stride_check.comp", &function, error, sizeof(error));
	failed |= function.local_size[0] != 64 || function.local_size[1] != 1 || function.local_size[2] != 1;

	failed |= !metal_convert_source(device, stage_handles[zest_vertex_shader], metal_reflect_vertex_source, &function, error, sizeof(error));
	ZEST_PRINT("\tVertex inputs at locations 0, 1 and 3 give the mask 0x%X", function.vertex_input_mask);
	failed |= function.vertex_input_mask != 0xB;

	//A sized set 0 array and .length() on a bindless buffer are refused, with an error that says what to change
	zest_bool constant_index_converted = metal_convert_source(device, stage_handles[zest_fragment_shader], metal_constant_index_source, &function, error, sizeof(error));
	ZEST_PRINT("\tConstant index: %s", constant_index_converted ? "converted" : error);
	failed |= constant_index_converted || !strstr(error, "constant_samplers");
	zest_bool length_converted = metal_convert_source(device, stage_handles[zest_compute_shader], metal_buffer_length_source, &function, error, sizeof(error));
	ZEST_PRINT("\tBuffer length: %s", length_converted ? "converted" : error);
	failed |= length_converted || !strstr(error, "OpArrayLength");

	//16 bytes that start like SPIR-V
	zest_shader compute_shader = zest_GetShader(stage_handles[zest_compute_shader]);
	zest_uint short_binary[4] = { METAL_SPIRV_MAGIC, 0x00010000, 0, 0 };
	zest_vec_resize(device->allocator, compute_shader->binary, sizeof(short_binary));
	memcpy(compute_shader->binary, short_binary, sizeof(short_binary));
	compute_shader->binary_size = sizeof(short_binary);
	zest_bool short_converted = zest__mtl_shader_function(device, compute_shader, zest_compute_shader, ZEST_FALSE, &function, error, sizeof(error));
	ZEST_PRINT("\tShort binary: %s", short_converted ? "converted" : error);
	failed |= short_converted;

	for (int stage = 0; stage != 3; ++stage) {
		zest_FreeShader(stage_handles[stage]);
	}
	zest_ResetDevice(device);
	failed |= zest_map_size(backend->shader_functions) != 0;
	return failed | finish_metal_device(device);
}

static const char *metal_translation_vertex_source =
	"#version 450\n"
	"layout(location = 0) in vec3 position;\n"
	"layout(location = 1) in vec2 uv;\n"
	"layout(location = 2) in vec4 color;\n"
	"void main() { gl_Position = vec4(position + vec3(uv, float(gl_InstanceIndex)), 1.0) * color; }\n";

static const char *metal_no_input_vertex_source =
	"#version 450\n"
	"void main() { gl_Position = vec4(float(gl_VertexIndex & 1), float(gl_VertexIndex >> 1), 0.0, 1.0); }\n";

static const char *metal_position_vertex_source =
	"#version 450\n"
	"layout(location = 0) in vec3 position;\n"
	"void main() { gl_Position = vec4(position, 1.0); }\n";

static zest_rendering_info_t metal_colour_rendering_info(zest_format depth_format) {
	zest_rendering_info_t rendering_info = {};
	rendering_info.color_attachment_count = 1;
	rendering_info.color_attachment_formats[0] = zest_format_r8g8b8a8_unorm;
	rendering_info.depth_attachment_format = depth_format;
	return rendering_info;
}

//Location 3 sits on a per instance binding that the translation vertex shader never reads
static zest_pipeline_template metal_translation_template(zest_device device, const char *name, zest_bool with_uv) {
	zest_pipeline_template pipeline_template = zest_CreatePipelineTemplate(device, name);
	zest_AddVertexInputBindingDescription(pipeline_template, 0, 36, zest_input_rate_vertex);
	zest_AddVertexInputBindingDescription(pipeline_template, 1, 16, zest_input_rate_instance);
	zest_AddVertexAttribute(pipeline_template, 0, 0, zest_format_r32g32b32_sfloat, 0);
	if (with_uv) zest_AddVertexAttribute(pipeline_template, 0, 1, zest_format_r32g32_sfloat, 12);
	zest_AddVertexAttribute(pipeline_template, 0, 2, zest_format_r8g8b8a8_unorm, 20);
	zest_AddVertexAttribute(pipeline_template, 1, 3, zest_format_r32g32b32a32_sfloat, 0);
	return pipeline_template;
}

//What a description gave, kept until the next description or the end of the test
struct MetalDescription {
	MTLRenderPipelineDescriptor *descriptor;
	MTLDepthStencilDescriptor *depth_stencil;
	zest_mtl_pipeline_backend_t backend;
	zest_key key;
	char error[1024];
};

static zest_bool metal_describe(zest_device device, zest_pipeline_template pipeline_template, const zest_rendering_info_t *rendering_info, const zest_mtl_shader_function_t *vertex_function, const zest_mtl_shader_function_t *fragment_function, MetalDescription *out_description) {
	[out_description->descriptor release];
	[out_description->depth_stencil release];
	out_description->descriptor = [[MTLRenderPipelineDescriptor alloc] init];
	out_description->depth_stencil = [[MTLDepthStencilDescriptor alloc] init];
	out_description->backend = {};
	out_description->error[0] = 0;
	return zest__mtl_pipeline_description(device, pipeline_template, rendering_info, vertex_function, fragment_function, out_description->descriptor, out_description->depth_stencil, &out_description->backend, &out_description->key, out_description->error, sizeof(out_description->error));
}

static MTLVertexAttributeDescriptor *metal_described_attribute(MetalDescription *description, zest_uint location) {
	return [[[description->descriptor vertexDescriptor] attributes] objectAtIndexedSubscript:location];
}

static MTLVertexBufferLayoutDescriptor *metal_described_layout(MetalDescription *description, zest_uint binding) {
	return [[[description->descriptor vertexDescriptor] layouts] objectAtIndexedSubscript:binding];
}

static zest_bool metal_create_converted_shader(zest_device device, zest_shader_handle *out_handle, const char *source, const char *name, zest_shader_type type, zest_mtl_shader_function_t *out_function) {
	char error[1024];
	*out_handle = zest_CreateShader(device, source, type, name, NULL, ZEST_TRUE);
	if (zest__mtl_shader_function(device, zest_GetShader(*out_handle), type, ZEST_FALSE, out_function, error, sizeof(error))) return ZEST_TRUE;
	ZEST_PRINT("\t%s", error);
	return ZEST_FALSE;
}

int test_metal_pipeline_state_translation(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_shader_handle handles[4];
	zest_mtl_shader_function_t translation_function = {};
	zest_mtl_shader_function_t simple_function = {};
	zest_mtl_shader_function_t position_function = {};
	zest_mtl_shader_function_t fragment_function = {};
	int failed = !metal_create_converted_shader(device, &handles[0], metal_translation_vertex_source, "translation_vertex", zest_vertex_shader, &translation_function);
	failed |= !metal_create_converted_shader(device, &handles[1], metal_no_input_vertex_source, "translation_simple", zest_vertex_shader, &simple_function);
	failed |= !metal_create_converted_shader(device, &handles[2], metal_position_vertex_source, "translation_position", zest_vertex_shader, &position_function);
	failed |= !metal_create_converted_shader(device, &handles[3], metal_stage_fragment_source, "translation_fragment", zest_fragment_shader, &fragment_function);
	zest_rendering_info_t colour_only = metal_colour_rendering_info(zest_format_undefined);
	zest_rendering_info_t with_depth = metal_colour_rendering_info(zest_format_d32_sfloat);
	MetalDescription description = {};
	@autoreleasepool {
		//Vertex input
		zest_pipeline_template input_template = metal_translation_template(device, "Translation Input", ZEST_TRUE);
		if (metal_describe(device, input_template, &colour_only, &translation_function, NULL, &description)) {
			failed |= [metal_described_attribute(&description, 0) format] != MTLVertexFormatFloat3 || [metal_described_attribute(&description, 1) format] != MTLVertexFormatFloat2;
			failed |= [metal_described_attribute(&description, 2) format] != MTLVertexFormatUChar4Normalized || [metal_described_attribute(&description, 2) offset] != 20;
			for (zest_uint location = 0; location != 3; ++location) {
				failed |= [metal_described_attribute(&description, location) bufferIndex] != 0;
			}
			failed |= [metal_described_attribute(&description, 3) format] != MTLVertexFormatInvalid;
			failed |= [metal_described_layout(&description, 0) stride] != 36 || [metal_described_layout(&description, 0) stepFunction] != MTLVertexStepFunctionPerVertex;
		} else {
			ZEST_PRINT("\t%s", description.error);
			failed = 1;
		}
		input_template->attribute_descriptions[2].binding = 1;
		if (metal_describe(device, input_template, &colour_only, &translation_function, NULL, &description)) {
			MTLVertexBufferLayoutDescriptor *instance_layout = metal_described_layout(&description, 1);
			failed |= [metal_described_attribute(&description, 2) bufferIndex] != 1;
			failed |= [instance_layout stride] != 16 || [instance_layout stepFunction] != MTLVertexStepFunctionPerInstance || [instance_layout stepRate] != 1;
		} else {
			ZEST_PRINT("\t%s", description.error);
			failed = 1;
		}
		//Half3, which D3D12 lacks, and a scaled format, which Metal lacks
		input_template->attribute_descriptions[0].format = zest_format_r16g16b16_sfloat;
		failed |= !metal_describe(device, input_template, &colour_only, &translation_function, NULL, &description) || [metal_described_attribute(&description, 0) format] != MTLVertexFormatHalf3;
		input_template->attribute_descriptions[0].format = zest_format_r16g16b16a16_uscaled;
		failed |= metal_describe(device, input_template, &colour_only, &translation_function, NULL, &description) || !strstr(description.error, "location 0");
		ZEST_PRINT("\tScaled format: %s", description.error);
		input_template->attribute_descriptions[0].format = zest_format_r32g32b32_sfloat;
		//Binding 4 is where the push constants go
		zest_AddVertexInputBindingDescription(input_template, 4, 16, zest_input_rate_vertex);
		failed |= metal_describe(device, input_template, &colour_only, &translation_function, NULL, &description) || !strstr(description.error, "binding 4");
		zest_FreePipelineTemplate(input_template);
		zest_pipeline_template missing_template = metal_translation_template(device, "Translation Missing", ZEST_FALSE);
		failed |= metal_describe(device, missing_template, &colour_only, &translation_function, NULL, &description) || !strstr(description.error, "location 1");
		ZEST_PRINT("\tMissing attribute: %s", description.error);
		zest_FreePipelineTemplate(missing_template);

		//Blend, with the write mask's bits reversed
		zest_pipeline_template state_template = zest_CreatePipelineTemplate(device, "Translation State");
		zest_SetPipelineDisableVertexInput(state_template);
		zest_color_blend_attachment_t *blend = &state_template->color_blend_attachment;
		blend->blend_enable = ZEST_TRUE;
		blend->src_color_blend_factor = zest_blend_factor_dst_color;
		blend->dst_color_blend_factor = zest_blend_factor_one_minus_src_color;
		blend->color_blend_op = zest_blend_op_reverse_subtract;
		blend->alpha_blend_op = zest_blend_op_min;
		blend->color_write_mask = zest_color_component_r_bit | zest_color_component_a_bit;
		if (metal_describe(device, state_template, &colour_only, &simple_function, &fragment_function, &description)) {
			MTLRenderPipelineColorAttachmentDescriptor *attachment = [[description.descriptor colorAttachments] objectAtIndexedSubscript:0];
			failed |= ![attachment isBlendingEnabled] || [attachment pixelFormat] != MTLPixelFormatRGBA8Unorm;
			failed |= [attachment sourceRGBBlendFactor] != MTLBlendFactorDestinationColor || [attachment destinationRGBBlendFactor] != MTLBlendFactorOneMinusSourceColor;
			failed |= [attachment rgbBlendOperation] != MTLBlendOperationReverseSubtract || [attachment alphaBlendOperation] != MTLBlendOperationMin;
			failed |= [attachment writeMask] != (MTLColorWriteMaskRed | MTLColorWriteMaskAlpha);
		} else {
			ZEST_PRINT("\t%s", description.error);
			failed = 1;
		}

		//Depth, and the sample count of 0 the rendering info has
		zest_SetPipelineDepthTest(state_template, ZEST_TRUE, ZEST_TRUE);
		state_template->depth_stencil.depth_compare_op = zest_compare_op_less;
		failed |= !metal_describe(device, state_template, &colour_only, &simple_function, &fragment_function, &description);
		failed |= [description.depth_stencil depthCompareFunction] != MTLCompareFunctionAlways || [description.depth_stencil isDepthWriteEnabled];
		failed |= [description.descriptor depthAttachmentPixelFormat] != MTLPixelFormatInvalid || [description.descriptor rasterSampleCount] != 1;
		failed |= !metal_describe(device, state_template, &with_depth, &simple_function, &fragment_function, &description);
		failed |= [description.depth_stencil depthCompareFunction] != MTLCompareFunctionLess || ![description.depth_stencil isDepthWriteEnabled];
		failed |= [description.descriptor depthAttachmentPixelFormat] != MTLPixelFormatDepth32Float;

		//Both front faces map straight across, and depth clamp is a clip mode
		state_template->rasterization.front_face = zest_front_face_clockwise;
		failed |= !metal_describe(device, state_template, &colour_only, &simple_function, &fragment_function, &description) || description.backend.winding != MTLWindingClockwise;
		state_template->rasterization.front_face = zest_front_face_counter_clockwise;
		failed |= !metal_describe(device, state_template, &colour_only, &simple_function, &fragment_function, &description) || description.backend.winding != MTLWindingCounterClockwise;
		failed |= description.backend.depth_clip_mode != MTLDepthClipModeClip;
		state_template->rasterization.depth_clamp_enable = ZEST_TRUE;
		failed |= !metal_describe(device, state_template, &colour_only, &simple_function, &fragment_function, &description) || description.backend.depth_clip_mode != MTLDepthClipModeClamp;
		zest_FreePipelineTemplate(state_template);

		//Errors, each naming the state
		const char *unsupported_states[11] = { "zest_cull_mode_front_and_back", "zest_polygon_mode_point", "zest_topology_triangle_fan", "zest_topology_line_list_with_adjacency", "zest_topology_line_strip_with_adjacency",
			"zest_topology_triangle_list_with_adjacency", "zest_topology_triangle_strip_with_adjacency", "zest_topology_patch_list", "stencil_test_enable", "depth_bounds_test_enable", "draws 2 views but its render pass has 0" };
		zest_topology unsupported_topologies[6] = { zest_topology_triangle_fan, zest_topology_line_list_with_adjacency, zest_topology_line_strip_with_adjacency, zest_topology_triangle_list_with_adjacency, zest_topology_triangle_strip_with_adjacency, zest_topology_patch_list };
		for (int case_index = 0; case_index != 11; ++case_index) {
			zest_pipeline_template error_template = zest_CreatePipelineTemplate(device, "Translation Error");
			zest_SetPipelineDisableVertexInput(error_template);
			if (case_index == 0) error_template->rasterization.cull_mode = zest_cull_mode_front_and_back;
			else if (case_index == 1) error_template->rasterization.polygon_mode = zest_polygon_mode_point;
			else if (case_index < 8) error_template->primitive_topology = unsupported_topologies[case_index - 2];
			else if (case_index == 8) error_template->depth_stencil.stencil_test_enable = ZEST_TRUE;
			else if (case_index == 9) error_template->depth_stencil.depth_bounds_test_enable = ZEST_TRUE;
			else error_template->view_mask = 3;
			if (metal_describe(device, error_template, &colour_only, &simple_function, &fragment_function, &description) || !strstr(description.error, unsupported_states[case_index])) {
				ZEST_PRINT("\t%s wasn't refused by name", unsupported_states[case_index]);
				failed = 1;
			}
			zest_FreePipelineTemplate(error_template);
		}

		//Metal takes a stride that isn't a multiple of 4, so the description doesn't refuse one
		zest_pipeline_template stride_template = zest_CreatePipelineTemplate(device, "Translation Stride");
		zest_AddVertexInputBindingDescription(stride_template, 0, 6, zest_input_rate_vertex);
		zest_AddVertexAttribute(stride_template, 0, 0, zest_format_r16g16b16_sfloat, 0);
		if (metal_describe(device, stride_template, &colour_only, &position_function, &fragment_function, &description)) {
			NSError *stride_error = nil;
			id<MTLRenderPipelineState> stride_state = [zest_GetMetalDevice(device) newRenderPipelineStateWithDescriptor:description.descriptor error:&stride_error];
			if (!stride_state) ZEST_PRINT("\tA stride of 6: %s", stride_error ? [[stride_error localizedDescription] UTF8String] : "Metal gave no error");
			failed |= stride_state == nil;
			[stride_state release];
		} else {
			ZEST_PRINT("\tA stride of 6: %s", description.error);
			failed = 1;
		}
		zest_FreePipelineTemplate(stride_template);
		[description.descriptor release];
		[description.depth_stencil release];
	}
	for (int handle_index = 0; handle_index != 4; ++handle_index) {
		zest_FreeShader(handles[handle_index]);
	}
	return failed | finish_metal_device(device);
}

//A command list with no backend, which is all a pipeline build reads
static zest_command_list_t metal_colour_command_list(zest_context context, zest_key render_pass_key, zest_format depth_format) {
	zest_command_list_t command_list = ZEST__ZERO_INIT(zest_command_list_t);
	command_list.context = context;
	command_list.device = context->device;
	command_list.rendering_info = metal_colour_rendering_info(depth_format);
	command_list.rendering_info.render_pass_key = render_pass_key;
	return command_list;
}

static zest_pipeline_template metal_no_input_template(zest_device device, const char *name, zest_shader_handle vertex_shader, zest_shader_handle fragment_shader) {
	zest_pipeline_template pipeline_template = zest_CreatePipelineTemplate(device, name);
	zest_SetPipelineShaders(pipeline_template, vertex_shader, fragment_shader);
	zest_SetPipelineDisableVertexInput(pipeline_template);
	zest_SetPipelineBlend(pipeline_template, zest_BlendStateOpaque());
	return pipeline_template;
}

static zest_mtl_pipeline_backend_t *metal_pipeline_backend(zest_pipeline pipeline) {
	static zest_mtl_pipeline_backend_t empty_backend = {};
	return pipeline ? ZEST_MTL_BACKEND(pipeline, pipeline) : &empty_backend;
}

#define METAL_PIPELINE_THREAD_COUNT 4

struct MetalPipelineWorker {
	zest_device device;
	zest_pipeline_template pipeline_template;
	zest_rendering_info_t rendering_info;
	zest_mtl_shader_function_t vertex_function;
	zest_mtl_shader_function_t fragment_function;
	volatile int *start;					//Raised once every thread exists, so their cache misses overlap
	id<MTLRenderPipelineState> pipeline_state;
};

static void *metal_pipeline_worker(void *parameter) {
	MetalPipelineWorker *worker = (MetalPipelineWorker *)parameter;
	while (!__atomic_load_n(worker->start, __ATOMIC_ACQUIRE)) {}
	@autoreleasepool {
		MTLRenderPipelineDescriptor *descriptor = [[MTLRenderPipelineDescriptor alloc] init];
		MTLDepthStencilDescriptor *depth_stencil = [[MTLDepthStencilDescriptor alloc] init];
		zest_mtl_pipeline_backend_t backend = {};
		zest_key key = 0;
		char error[1024];
		if (zest__mtl_pipeline_description(worker->device, worker->pipeline_template, &worker->rendering_info, &worker->vertex_function, &worker->fragment_function, descriptor, depth_stencil, &backend, &key, error, sizeof(error))) {
			worker->pipeline_state = zest__mtl_render_pipeline_state(worker->device, key, descriptor, error, sizeof(error));
		}
		[descriptor release];
		[depth_stencil release];
	}
	return NULL;
}

//Every thread asks for one new pipeline state at once, and all must get the one the cache keeps
static int run_metal_concurrent_pipelines(zest_device device, zest_pipeline_template pipeline_template, zest_shader_handle vertex_shader, zest_shader_handle fragment_shader) {
	zest_mtl_device_backend_t *backend = ZEST_MTL_BACKEND(device, device);
	MetalPipelineWorker workers[METAL_PIPELINE_THREAD_COUNT] = {};
	pthread_t threads[METAL_PIPELINE_THREAD_COUNT];
	volatile int start = 0;
	char error[1024];
	zest_mtl_shader_function_t vertex_function = {};
	zest_mtl_shader_function_t fragment_function = {};
	int failed = !zest__mtl_shader_function(device, zest_GetShader(vertex_shader), zest_vertex_shader, ZEST_FALSE, &vertex_function, error, sizeof(error));
	failed |= !zest__mtl_shader_function(device, zest_GetShader(fragment_shader), zest_fragment_shader, ZEST_FALSE, &fragment_function, error, sizeof(error));
	if (failed) return failed;
	zest_uint state_count = zest_map_size(backend->render_pipeline_states);
	zest_uint thread_count = 0;
	for (; thread_count != METAL_PIPELINE_THREAD_COUNT; ++thread_count) {
		MetalPipelineWorker *worker = &workers[thread_count];
		worker->device = device;
		worker->pipeline_template = pipeline_template;
		worker->rendering_info = metal_colour_rendering_info(zest_format_undefined);
		worker->vertex_function = vertex_function;
		worker->fragment_function = fragment_function;
		worker->start = &start;
		if (pthread_create(&threads[thread_count], NULL, metal_pipeline_worker, worker) != 0) break;
	}
	__atomic_store_n(&start, 1, __ATOMIC_RELEASE);
	for (zest_uint thread = 0; thread != thread_count; ++thread) {
		pthread_join(threads[thread], NULL);
		failed |= workers[thread].pipeline_state == nil || workers[thread].pipeline_state != workers[0].pipeline_state;
	}
	failed |= thread_count != METAL_PIPELINE_THREAD_COUNT || zest_map_size(backend->render_pipeline_states) != state_count + 1;
	ZEST_PRINT("\t%u threads asked for one pipeline state and the cache grew by %u", thread_count, zest_map_size(backend->render_pipeline_states) - state_count);
	return failed;
}

int test_metal_pipeline_cache(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_mtl_device_backend_t *backend = ZEST_MTL_BACKEND(device, device);
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_metal_device(device);
	zest_shader_handle vertex_shader = zest_CreateShader(device, metal_no_input_vertex_source, zest_vertex_shader, "cache_vertex", NULL, ZEST_TRUE);
	zest_shader_handle fragment_shader = zest_CreateShader(device, metal_stage_fragment_source, zest_fragment_shader, "cache_fragment", NULL, ZEST_TRUE);

	//Rendering infos that differ only in their render pass key share a pipeline state
	zest_pipeline_template base_template = metal_no_input_template(device, "Cache Base", vertex_shader, fragment_shader);
	zest_command_list_t first_list = metal_colour_command_list(context, 1, zest_format_undefined);
	zest_command_list_t second_list = metal_colour_command_list(context, 2, zest_format_undefined);
	zest_pipeline first_pipeline = zest_GetPipeline(base_template, &first_list);
	zest_pipeline second_pipeline = zest_GetPipeline(base_template, &second_list);
	id<MTLRenderPipelineState> first_state = metal_pipeline_backend(first_pipeline)->pipeline_state;
	int failed = first_state == nil || second_pipeline == NULL || first_pipeline == second_pipeline || metal_pipeline_backend(second_pipeline)->pipeline_state != first_state;

	zest_pipeline_template blend_template = zest_CopyPipelineTemplate("Cache Blend", base_template);
	zest_SetPipelineBlend(blend_template, zest_AlphaBlendState());
	id<MTLRenderPipelineState> blend_state = metal_pipeline_backend(zest_GetPipeline(blend_template, &first_list))->pipeline_state;
	failed |= blend_state == nil || blend_state == first_state;

	//What a swapchain recreate does
	zest_uint state_count = zest_map_size(backend->render_pipeline_states);
	zest__cleanup_pipelines(context);
	zest_map_free(context->allocator, context->cached_pipelines);
	zest_pipeline recreated_pipeline = zest_GetPipeline(base_template, &first_list);
	failed |= metal_pipeline_backend(recreated_pipeline)->pipeline_state != first_state || zest_map_size(backend->render_pipeline_states) != state_count;
	ZEST_PRINT("\t%u pipeline states cached after the recreate", zest_map_size(backend->render_pipeline_states));

	zest_pipeline_template culled_template = zest_CopyPipelineTemplate("Cache Cull Both", base_template);
	zest_SetPipelineCullMode(culled_template, zest_cull_mode_front_and_back);
	zest_uint error_count = zest_GetValidationErrorCount(device);
	failed |= zest_GetPipeline(culled_template, &first_list) != NULL || zest_GetValidationErrorCount(device) != error_count + 1 || zest_PipelineIsValid(culled_template);

	//Two templates with the same depth settings share a depth-stencil state
	zest_command_list_t depth_list = metal_colour_command_list(context, 3, zest_format_d32_sfloat);
	zest_pipeline_template first_depth_template = zest_CopyPipelineTemplate("Cache Depth", base_template);
	zest_pipeline_template second_depth_template = zest_CopyPipelineTemplate("Cache Depth Blend", blend_template);
	zest_SetPipelineDepthTest(first_depth_template, ZEST_TRUE, ZEST_TRUE);
	zest_SetPipelineDepthTest(second_depth_template, ZEST_TRUE, ZEST_TRUE);
	id<MTLDepthStencilState> first_depth_state = metal_pipeline_backend(zest_GetPipeline(first_depth_template, &depth_list))->depth_stencil_state;
	id<MTLDepthStencilState> second_depth_state = metal_pipeline_backend(zest_GetPipeline(second_depth_template, &depth_list))->depth_stencil_state;
	failed |= first_depth_state == nil || first_depth_state != second_depth_state || first_depth_state == metal_pipeline_backend(recreated_pipeline)->depth_stencil_state;

	zest_pipeline_template concurrent_template = zest_CopyPipelineTemplate("Cache Concurrent", base_template);
	zest_SetPipelineBlend(concurrent_template, zest_AdditiveBlendState());
	failed |= run_metal_concurrent_pipelines(device, concurrent_template, vertex_shader, fragment_shader);

	zest_FreePipelineTemplate(base_template);
	zest_FreePipelineTemplate(blend_template);
	zest_FreePipelineTemplate(culled_template);
	zest_FreePipelineTemplate(first_depth_template);
	zest_FreePipelineTemplate(second_depth_template);
	zest_FreePipelineTemplate(concurrent_template);
	zest_FreeShader(vertex_shader);
	zest_FreeShader(fragment_shader);

	zest_ResetValidationErrors(device);
	zest_ResetDevice(device);
	failed |= zest_map_size(backend->render_pipeline_states) != 0;
	for (int state_index = 0; state_index != ZEST__MTL_DEPTH_STENCIL_STATE_COUNT; ++state_index) {
		failed |= backend->depth_stencil_states[state_index] != nil;
	}
	return failed | finish_metal_device(device);
}

struct MetalLazyDraws {
	VertexDrawResources resources;
	zest_pipeline_template wide_template;			//The vertex draw's pipeline with a stride of 24
	zest_pipeline_template no_push_template;
	zest_pipeline_template winding_templates[2];	//Back faces culled, clockwise then counter-clockwise, which share a pipeline state
	zest_pipeline pipeline_after_blit;				//What the app had bound when the pass after the blit began
	zest_pipeline recorded_after_blit;				//What that pass's encoder held then
};

//Pushes and buffers come before the pipeline. The wide pipeline then reads every other vertex from the same buffer, with no new bind.
static void metal_lazy_order_task(const zest_command_list command_list, void *user_data) {
	MetalLazyDraws *draws = (MetalLazyDraws *)user_data;
	float left_tint[4] = { 1.f, 0.5f, 1.f, 1.f };
	zest_cmd_SendPushConstants(command_list, left_tint, sizeof(left_tint));
	zest_cmd_BindVertexBuffer(command_list, 0, 1, draws->resources.vertex_buffer);
	zest_cmd_BindIndexBuffer(command_list, draws->resources.index_buffer);
	set_full_viewport(command_list);
	zest_pipeline pipeline = zest_GetPipeline(draws->resources.pipeline_template, command_list);
	zest_pipeline wide_pipeline = zest_GetPipeline(draws->wide_template, command_list);
	if (!pipeline || !wide_pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_DrawIndexed(command_list, 6, 1, 0, 0, 0);
	//Vertices 2, 4 and 6: a triangle in the right half under the line from (8, 0) to (16, 16). Every vertex instead would draw over the left half.
	float right_tint[4] = { 0.f, 1.f, 1.f, 1.f };
	zest_cmd_SendPushConstants(command_list, right_tint, sizeof(right_tint));
	zest_cmd_BindPipeline(command_list, wide_pipeline);
	zest_cmd_Draw(command_list, 3, 1, 1, 0);
}

//Draws of nothing record nothing, as on Vulkan
static void metal_lazy_no_push_task(const zest_command_list command_list, void *user_data) {
	MetalLazyDraws *draws = (MetalLazyDraws *)user_data;
	char push_data[ZEST_MAX_PUSH_SIZE];
	memset(push_data, 0x3F, sizeof(push_data));
	zest_cmd_SendPushConstants(command_list, push_data, sizeof(push_data));
	set_full_viewport(command_list);
	zest_pipeline pipeline = zest_GetPipeline(draws->no_push_template, command_list);
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_Draw(command_list, 3, 1, 0, 0);
	zest_cmd_Draw(command_list, 0, 1, 0, 0);
	zest_cmd_Draw(command_list, 3, 0, 0, 0);
}

//The clockwise pipeline keeps the triangle in the left half, and the counter-clockwise one, with the same pipeline state, culls it in the right
static void metal_lazy_shared_state_task(const zest_command_list command_list, void *user_data) {
	MetalLazyDraws *draws = (MetalLazyDraws *)user_data;
	set_full_viewport(command_list);
	zest_pipeline clockwise = zest_GetPipeline(draws->winding_templates[0], command_list);
	zest_pipeline counter_clockwise = zest_GetPipeline(draws->winding_templates[1], command_list);
	if (!clockwise || !counter_clockwise) return;
	zest_scissor_rect_t left_half = zest_CreateRect2D(8, 16, 0, 0);
	zest_scissor_rect_t right_half = zest_CreateRect2D(8, 16, 8, 0);
	zest_cmd_Scissor(command_list, &left_half);
	zest_cmd_BindPipeline(command_list, clockwise);
	zest_cmd_Draw(command_list, 3, 1, 0, 0);
	zest_cmd_Scissor(command_list, &right_half);
	zest_cmd_BindPipeline(command_list, counter_clockwise);
	zest_cmd_Draw(command_list, 3, 1, 0, 0);
}

static void metal_lazy_blit_task(const zest_command_list command_list, void *user_data) {
	zest_resource_node source = zest_GetPassInputResource(command_list, "Blit Source");
	zest_resource_node destination = zest_GetPassOutputResource(command_list, "Blit Destination");
	zest_cmd_BlitImageMip(command_list, source, destination, 0, zest_shader_fragment_stage);
}

static void metal_lazy_after_blit_task(const zest_command_list command_list, void *user_data) {
	MetalLazyDraws *draws = (MetalLazyDraws *)user_data;
	draws->pipeline_after_blit = ZEST_MTL_BACKEND(command_list, command_list)->bind_state.pipeline;
	draws->recorded_after_blit = ZEST_MTL_BACKEND(command_list, command_list)->bind_state.recorded_pipeline;
	set_full_viewport(command_list);
	zest_cmd_BindVertexBuffer(command_list, 0, 1, draws->resources.vertex_buffer);
	zest_cmd_BindIndexBuffer(command_list, draws->resources.index_buffer);
	zest_pipeline pipeline = zest_GetPipeline(draws->resources.pipeline_template, command_list);
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	float left_tint[4] = { 0.5f, 1.f, 1.f, 1.f };
	zest_cmd_SendPushConstants(command_list, left_tint, sizeof(left_tint));
	zest_cmd_DrawIndexed(command_list, 6, 1, 0, 0, 0);
	float right_tint[4] = { 1.f, 0.f, 1.f, 1.f };
	zest_cmd_SendPushConstants(command_list, right_tint, sizeof(right_tint));
	zest_cmd_DrawIndexed(command_list, 6, 1, 0, 4, 0);
}

static void metal_lazy_no_pipeline_task(const zest_command_list command_list, void *user_data) {
	set_full_viewport(command_list);
	zest_cmd_Draw(command_list, 3, 1, 0, 0);
}

//A draw, a blit that opens a render encoder of its own, then a draw into another target, all in one batch
static int run_metal_blit_between_draws(zest_context context, MetalLazyDraws *draws, zest_image first_target, zest_image converted, zest_image second_target) {
	if (!zest_BeginCommandGraph(context, "Lazy Blit", 0)) return 1;
	zest_resource_node first = zest_ImportImageResource("Blit Source", first_target, 0);
	zest_resource_node converted_node = zest_ImportImageResource("Blit Destination", converted, 0);
	zest_resource_node second = zest_ImportImageResource("Second Target", second_target, 0);
	zest_SetResourceClearColor(first, 0.f, 0.f, 0.f, 1.f);
	zest_SetResourceClearColor(second, 0.f, 0.f, 0.f, 1.f);
	zest_BeginRenderPass("Draw Before Blit");
	zest_ConnectOutput(first);
	zest_SetPassTask(vertex_draw_task, &draws->resources);
	zest_EndPass();
	zest_BeginTransferPass("Blit Between Draws");
	zest_ConnectInput(first);
	zest_ConnectOutput(converted_node);
	zest_SetPassTask(metal_lazy_blit_task, NULL);
	zest_EndPass();
	zest_BeginRenderPass("Draw After Blit");
	zest_ConnectInput(converted_node);
	zest_ConnectOutput(second);
	zest_SetPassTask(metal_lazy_after_blit_task, draws);
	zest_EndPass();
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	zest_uint most_passes = 0;
	zest_uint batch_count = frame_graph ? metal_graph_batch_count(frame_graph, &most_passes) : 0;
	int failed = batch_count != 1 || most_passes != 3;
	if (failed) ZEST_PRINT("\tThe draws and the blit went into %u batches with at most %u passes, where the test needs all three in one.", batch_count, most_passes);
	return failed | (frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success);
}

static zest_pipeline_template metal_winding_template(zest_device device, const char *name, zest_shader_handle vertex_shader, zest_shader_handle fragment_shader, zest_cull_mode cull_mode, zest_front_face front_face) {
	zest_pipeline_template pipeline_template = zest_CreatePipelineTemplate(device, name);
	zest_SetPipelineShaders(pipeline_template, vertex_shader, fragment_shader);
	zest_SetPipelineDisableVertexInput(pipeline_template);
	zest_SetPipelineBlend(pipeline_template, zest_BlendStateOpaque());
	zest_SetPipelineCullMode(pipeline_template, cull_mode);
	zest_SetPipelineFrontFace(pipeline_template, front_face);
	return pipeline_template;
}

static zest_bool metal_pixel_is(const zest_uint *pixels, zest_uint x, zest_uint y, const int expected[4]) {
	const unsigned char *channels = (const unsigned char *)&pixels[y * 16 + x];
	for (int channel = 0; channel != 4; ++channel) {
		int difference = (int)channels[channel] - expected[channel];
		if (difference > 1 || difference < -1) {
			ZEST_PRINT("\tPixel (%u, %u) is (%u, %u, %u, %u), expected (%i, %i, %i, %i)", x, y, channels[0], channels[1], channels[2], channels[3], expected[0], expected[1], expected[2], expected[3]);
			return ZEST_FALSE;
		}
	}
	return ZEST_TRUE;
}

int test_metal_lazy_binding_order(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_metal_device(device);
	MetalLazyDraws draws = {};
	int failed = !create_vertex_draw_resources(device, &draws.resources);
	draws.wide_template = zest_CopyPipelineTemplate("Lazy Wide", draws.resources.pipeline_template);
	draws.wide_template->binding_descriptions[0].stride = 24;
	zest_shader_handle winding_vertex = zest_CreateShader(device, winding_vertex_source, zest_vertex_shader, "lazy_winding_vertex", NULL, ZEST_TRUE);
	zest_shader_handle winding_fragment = zest_CreateShader(device, winding_fragment_source, zest_fragment_shader, "lazy_winding_fragment", NULL, ZEST_TRUE);
	draws.no_push_template = metal_winding_template(device, "Lazy No Push", winding_vertex, winding_fragment, zest_cull_mode_none, zest_front_face_clockwise);
	draws.winding_templates[0] = metal_winding_template(device, "Lazy Clockwise", winding_vertex, winding_fragment, zest_cull_mode_back, zest_front_face_clockwise);
	draws.winding_templates[1] = metal_winding_template(device, "Lazy Counter Clockwise", winding_vertex, winding_fragment, zest_cull_mode_back, zest_front_face_counter_clockwise);
	zest_image_handle target_handle = create_draw_target(device);
	zest_image_handle second_handle = create_draw_target(device);
	zest_image_info_t converted_info = zest_CreateImageInfo(16, 16);
	converted_info.format = zest_format_r16g16b16a16_sfloat;
	converted_info.flags = zest_image_preset_storage | zest_image_flag_color_attachment;
	zest_image_handle converted_handle = zest_CreateImage(device, &converted_info);
	zest_image target = zest_GetImage(target_handle);
	zest_image second = zest_GetImage(second_handle);
	zest_image converted = zest_GetImage(converted_handle);
	failed |= !target || !second || !converted;
	const int black[4] = { 0, 0, 0, 255 };
	zest_uint pixels[256];
	if (!failed) {
		failed |= render_draw_pass(context, "Lazy Order", target, ZEST_FALSE, metal_lazy_order_task, &draws, pixels);
		const int wide_colour[4] = { 0, 128, 0, 255 };
		failed |= !columns_hold(pixels, 0, 8, vertex_draw_left) || !metal_pixel_is(pixels, 9, 14, wide_colour) || !metal_pixel_is(pixels, 15, 2, black);
		failed |= zest_GetValidationErrorCount(device) != 0;

		failed |= render_draw_pass(context, "Lazy No Push", target, ZEST_FALSE, metal_lazy_no_push_task, &draws, pixels);
		failed |= !winding_covers_triangle(winding_coverage(pixels));
		failed |= zest_GetValidationErrorCount(device) != 0;

		failed |= render_draw_pass(context, "Lazy Shared State", target, ZEST_FALSE, metal_lazy_shared_state_task, &draws, pixels);
		WindingCoverage coverage = winding_coverage(pixels);
		zest_uint right_red_count = 0;
		for (zest_uint y = 0; y != 16; ++y) {
			for (zest_uint x = 8; x != 16; ++x) right_red_count += pixels[y * 16 + x] == 0xFF0000FF;
		}
		ZEST_PRINT("\tShared pipeline state: %u red pixels, %u of them in the culled right half", coverage.red_count, right_red_count);
		failed |= coverage.red_count == 0 || right_red_count != 0 || coverage.other_count != 0;

		failed |= run_metal_blit_between_draws(context, &draws, target, converted, second);
		const int after_blit_left[4] = { 128, 128, 0, 255 };
		const int after_blit_right[4] = { 255, 0, 0, 255 };
		failed |= !zest_CopyImageToBitmap(device, second, pixels) || !columns_hold(pixels, 0, 8, after_blit_left) || !columns_hold(pixels, 8, 8, after_blit_right);
		ZEST_PRINT("\tThe pass after the blit began with %s pipeline bound and %s recorded", draws.pipeline_after_blit ? "a" : "no", draws.recorded_after_blit ? "one" : "none");
		failed |= draws.pipeline_after_blit == NULL || draws.recorded_after_blit != NULL;
		failed |= zest_GetValidationErrorCount(device) != 0;

		zest_uint error_count = zest_GetValidationErrorCount(device);
		failed |= render_draw_pass(context, "Lazy No Pipeline", target, ZEST_FALSE, metal_lazy_no_pipeline_task, NULL, pixels);
		failed |= zest_GetValidationErrorCount(device) != error_count + 1 || !columns_hold(pixels, 0, 16, black);
		zest_ResetValidationErrors(device);
	}
	if (target) zest_FreeImageNow(target_handle);
	if (second) zest_FreeImageNow(second_handle);
	if (converted) zest_FreeImageNow(converted_handle);
	zest_FreePipelineTemplate(draws.wide_template);
	zest_FreePipelineTemplate(draws.no_push_template);
	zest_FreePipelineTemplate(draws.winding_templates[0]);
	zest_FreePipelineTemplate(draws.winding_templates[1]);
	zest_FreeShader(winding_vertex);
	zest_FreeShader(winding_fragment);
	free_vertex_draw_resources(&draws.resources);
	zest_DestroyContext(context);
	return failed | finish_metal_device(device);
}

#define METAL_IMMEDIATE_ELEMENTS 64
#define METAL_BUFFER_WRITE_PATH "examples/SDL2/zest-tests/shaders/glsl/buffer_write.comp"

//An 8x1x1 dispatch of buffer_write stores (x + added, 0, 0, 0) at element x, with the push sent before the bind as pbr_functions.cpp does
static int run_metal_immediate_write(zest_device device, zest_device_queue_type queue_type, zest_compute compute, zest_buffer buffer, zest_uint buffer_index, float added) {
	memset(zest_BufferData(buffer), 0xFF, METAL_IMMEDIATE_ELEMENTS * 16);
	zest_queue queue = zest_imm_BeginCommandBuffer(device, queue_type);
	if (!queue) return 1;
	TestPushConstants push = {};
	push.index1 = (int)buffer_index;
	zest_imm_SendPushConstants(queue, &push, sizeof(push));
	zest_imm_BindComputePipeline(queue, compute);
	int failed = !zest_imm_DispatchCompute(queue, 8, 1, 1);
	failed |= !zest_imm_EndCommandBuffer(queue);
	const float *values = (const float *)zest_BufferData(buffer);
	for (zest_uint element = 0; element != METAL_IMMEDIATE_ELEMENTS; ++element) {
		const float *value = &values[element * 4];
		if (value[0] != (float)element + added || value[1] != 0.f || value[2] != 0.f || value[3] != 0.f) {
			ZEST_PRINT("\tElement %u holds (%f, %f, %f, %f), expected %f", element, value[0], value[1], value[2], value[3], (float)element + added);
			return 1;
		}
	}
	return failed;
}

int test_metal_immediate_compute(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_shader_handle shader = zest_CreateShaderFromFile(device, METAL_BUFFER_WRITE_PATH, "immediate_buffer_write", zest_compute_shader, NULL, ZEST_TRUE);
	zest_compute_handle compute_handle = zest_CreateCompute(device, "Immediate Buffer Write", shader);
	zest_compute compute = compute_handle.value ? zest_GetCompute(compute_handle) : NULL;
	zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_to_cpu);
	zest_buffer buffer = zest_CreateBuffer(device, METAL_IMMEDIATE_ELEMENTS * 16, &buffer_info);
	int failed = compute == NULL || buffer == NULL;
	zest_uint buffer_index = buffer ? zest_AcquireStorageBufferIndex(device, buffer) : ZEST_INVALID;
	if (!failed) {
		failed |= run_metal_immediate_write(device, zest_queue_graphics, compute, buffer, buffer_index, 0.f);
		failed |= run_metal_immediate_write(device, zest_queue_compute, compute, buffer, buffer_index, 0.f);
		failed |= zest_GetValidationErrorCount(device) != 0;

		//A dispatch of nothing records nothing, as on Vulkan
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_compute);
		failed |= queue == NULL;
		if (queue) {
			zest_imm_BindComputePipeline(queue, compute);
			failed |= !zest_imm_DispatchCompute(queue, 0, 1, 1);
			failed |= !zest_imm_EndCommandBuffer(queue);
		}
		failed |= zest_GetValidationErrorCount(device) != 0;

		zest_uint error_count = zest_GetValidationErrorCount(device);
		queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= queue == NULL || zest_imm_DispatchCompute(queue, 1, 1, 1) != ZEST_FALSE;
		failed |= queue == NULL || !zest_imm_EndCommandBuffer(queue);
		failed |= zest_GetValidationErrorCount(device) != error_count + 1;

		queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
		failed |= queue == NULL;
		if (queue && !(queue->manager->type & zest_queue_compute)) {
			error_count = zest_GetValidationErrorCount(device);
			zest_imm_BindComputePipeline(queue, compute);
			failed |= zest_imm_DispatchCompute(queue, 8, 1, 1) != ZEST_FALSE;
			failed |= zest_GetValidationErrorCount(device) != error_count + 1;
		} else {
			ZEST_PRINT("\tThe transfer queue phase was skipped, as the queue given can run compute");
			failed = 1;
		}
		if (queue) failed |= !zest_imm_EndCommandBuffer(queue);

		//Hot reload, through the same compute handle
		const char *hot_path = "zest_hot_reload_compute.comp";
		zest_file original = zest_ReadEntireFile(device, METAL_BUFFER_WRITE_PATH, ZEST_TRUE);
		const char *write_value = original ? strstr(original, "float(index)") : NULL;
		failed |= write_value == NULL || !macro_test__write_file(hot_path, original, 0);
		if (write_value) {
			zest_shader_handle hot_shader = zest_CreateShaderFromFile(device, hot_path, "hot_buffer_write", zest_compute_shader, NULL, ZEST_TRUE);
			zest_SetShaderHotReload(hot_shader, ZEST_TRUE);
			zest_compute_handle hot_handle = zest_CreateCompute(device, "Hot Buffer Write", hot_shader);
			zest_compute hot_compute = hot_handle.value ? zest_GetCompute(hot_handle) : NULL;
			failed |= hot_compute == NULL;
			if (hot_compute) {
				failed |= run_metal_immediate_write(device, zest_queue_compute, hot_compute, buffer, buffer_index, 0.f);
				char edited[4096];
				snprintf(edited, sizeof(edited), "%.*sfloat(index + 1000u)%s", (int)(write_value - original), original, write_value + strlen("float(index)"));
				failed |= !macro_test__write_file(hot_path, edited, zest_GetShader(hot_shader)->last_mtime);
				//The rebuild cleans up the compute's backend, which must let go of the old pipeline state
				id<MTLComputePipelineState> old_state = [ZEST_MTL_BACKEND(compute, hot_compute)->pipeline_state retain];
				zest_uint reload_count = zest_CheckShaderHotReload(device);
				NSUInteger old_state_owners = metal_settled_owner_count(old_state);
				ZEST_PRINT("\t%u shader reloaded, and the old pipeline state has %lu owners besides the test", reload_count, (unsigned long)(old_state_owners - 1));
				failed |= reload_count != 1 || old_state_owners != 1 || ZEST_MTL_BACKEND(compute, hot_compute)->pipeline_state == old_state;
				[old_state release];
				failed |= run_metal_immediate_write(device, zest_queue_compute, hot_compute, buffer, buffer_index, 1000.f);
				zest_FreeCompute(hot_handle);
			}
			zest_FreeShader(hot_shader);
		}
		remove(hot_path);
		if (original) zest_FreeFile(device, original);
		zest_ResetValidationErrors(device);
	}
	if (buffer) zest_FreeBufferNow(buffer);
	if (compute) zest_FreeCompute(compute_handle);
	zest_FreeShader(shader);
	return failed | finish_metal_device(device);
}

static const char *metal_reads_descriptors_source =
	"#version 450\n"
	"#extension GL_EXT_nonuniform_qualifier : require\n"
	"layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;\n"
	"layout(set = 0, binding = 0) uniform sampler samplers[];\n"
	"layout(set = 0, binding = 1) uniform texture2D textures_2d[];\n"
	"layout(set = 0, binding = 2) uniform textureCube textures_cube[];\n"
	"layout(set = 0, binding = 3) uniform texture2DArray textures_array[];\n"
	"layout(set = 0, binding = 4) uniform texture3D textures_3d[];\n"
	"layout(std430, set = 0, binding = 5) readonly buffer raw_block { uint words[]; } raw_buffers[];\n"
	"struct structured_element { uint index; uint padding0; uint padding1; uint padding2; vec4 padding3; vec4 padding4; };\n"
	"layout(std430, set = 0, binding = 5) readonly buffer structured_block { structured_element elements[]; } structured_buffers[];\n"
	"layout(std430, set = 0, binding = 5) buffer result_block { vec4 values[]; } result_buffers[];\n"
	"layout(set = 0, binding = 6, rgba8) uniform writeonly image2D storage_images[];\n"
	"layout(set = 0, binding = 7) uniform params_block { vec4 tint; uint add; } params[];\n"
	"layout(set = 0, binding = 8) uniform textureCubeArray textures_cube_array[];\n"
	"layout(push_constant) uniform push_block {\n"
	"	uint sampler_index; uint texture_2d_index; uint texture_cube_index; uint texture_array_index; uint texture_3d_index; uint texture_cube_array_index;\n"
	"	uint raw_index; uint structured_index; uint result_index; uint storage_image_index; uint uniform_index; uint structured_count; uint mip_view_index;\n"
	"} pc;\n"
	"void main() {\n"
	"	ivec2 cell = ivec2(gl_GlobalInvocationID.xy);\n"
	"	imageStore(storage_images[pc.storage_image_index], cell, vec4(float(cell.x) / 255.0, float(cell.y) / 255.0, 0.5, 1.0));\n"
	"	if (cell.x != 0 || cell.y != 0) return;\n"
	"	uint structured_sum = 0u;\n"
	"	for (uint element = 0u; element != pc.structured_count; ++element) structured_sum += structured_buffers[pc.structured_index].elements[element].index;\n"
	"	result_buffers[pc.result_index].values[0] = vec4(float(raw_buffers[pc.raw_index].words[0] + params[pc.uniform_index].add), params[pc.uniform_index].tint.xyz);\n"
	"	result_buffers[pc.result_index].values[1] = textureLod(sampler2D(textures_2d[pc.texture_2d_index], samplers[pc.sampler_index]), vec2(0.5), 0.0);\n"
	"	result_buffers[pc.result_index].values[2] = textureLod(samplerCube(textures_cube[pc.texture_cube_index], samplers[pc.sampler_index]), vec3(1.0, 0.0, 0.0), 0.0);\n"
	"	result_buffers[pc.result_index].values[3] = textureLod(sampler2DArray(textures_array[pc.texture_array_index], samplers[pc.sampler_index]), vec3(0.5, 0.5, 1.0), 0.0);\n"
	"	result_buffers[pc.result_index].values[4] = textureLod(sampler3D(textures_3d[pc.texture_3d_index], samplers[pc.sampler_index]), vec3(0.5), 0.0);\n"
	"	result_buffers[pc.result_index].values[5] = textureLod(samplerCubeArray(textures_cube_array[pc.texture_cube_array_index], samplers[pc.sampler_index]), vec4(1.0, 0.0, 0.0, 1.0), 0.0);\n"
	"	result_buffers[pc.result_index].values[6] = vec4(float(structured_sum), 0.0, 0.0, 0.0);\n"
	"	result_buffers[pc.result_index].values[7] = texelFetch(sampler2D(textures_2d[pc.mip_view_index], samplers[pc.sampler_index]), ivec2(2, 5), 0);\n"
	"}\n";

#define METAL_STRUCTURED_COUNT 37

struct MetalReadsPush {
	zest_uint sampler_index;
	zest_uint texture_2d_index;
	zest_uint texture_cube_index;
	zest_uint texture_array_index;
	zest_uint texture_3d_index;
	zest_uint texture_cube_array_index;
	zest_uint raw_index;
	zest_uint structured_index;
	zest_uint result_index;
	zest_uint storage_image_index;
	zest_uint uniform_index;
	zest_uint structured_count;
	zest_uint mip_view_index;
};

struct MetalReadsTask {
	zest_compute compute;
	MetalReadsPush push;
	zest_buffer copy_source;
	zest_buffer copy_target;
};

//The first dispatch writes the decoy storage image, and the copy ends its encoder, so the mip view has to be declared again on a second one
static void metal_reads_descriptors_task(const zest_command_list command_list, void *user_data) {
	MetalReadsTask *task = (MetalReadsTask *)user_data;
	MetalReadsPush decoy_push = task->push;
	decoy_push.storage_image_index = task->push.storage_image_index + 1;
	zest_cmd_BindComputePipeline(command_list, task->compute);
	zest_cmd_SendPushConstants(command_list, &decoy_push, sizeof(decoy_push));
	zest_cmd_DispatchCompute(command_list, 1, 1, 1);
	zest_cmd_CopyBuffer(command_list, task->copy_source, task->copy_target, 256);
	zest_cmd_SendPushConstants(command_list, &task->push, sizeof(task->push));
	zest_cmd_DispatchCompute(command_list, 1, 1, 1);
}

//One converted kernel reads every slot through the table the backend binds, with a decoy of its kind on either side
int test_metal_compute_reads_descriptors(void) {
	static const unsigned char decoy_colors[12 * 4] = { 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255 };
	static const unsigned char cube_colors[6 * 4] = { 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255 };
	static const unsigned char array_colors[2 * 4] = { 200, 0, 0, 255, 40, 50, 60, 255 };
	static const unsigned char volume_color[4] = { 70, 80, 90, 255 };
	static const unsigned char cube_array_colors[12 * 4] = { 0, 200, 0, 255, 0, 200, 0, 255, 0, 200, 0, 255, 0, 200, 0, 255, 0, 200, 0, 255, 0, 200, 0, 255, 100, 110, 120, 255, 100, 110, 120, 255, 100, 110, 120, 255, 100, 110, 120, 255, 100, 110, 120, 255, 100, 110, 120, 255 };
	static const unsigned char two_texels[2 * 4] = { 255, 0, 0, 255, 0, 0, 255, 255 };
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_metal_device(device);
	zest_shader_handle shader = zest_CreateShader(device, metal_reads_descriptors_source, zest_compute_shader, "reads_descriptors", NULL, ZEST_TRUE);
	zest_compute_handle compute_handle = zest_CreateCompute(device, "Reads Descriptors", shader);
	MetalReadsTask task = {};
	task.compute = compute_handle.value ? zest_GetCompute(compute_handle) : NULL;

	//The real sampler blends the two texels of the 2D texture, where the decoy would return one of them
	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	sampler_info.min_filter = zest_filter_linear;
	sampler_info.mag_filter = zest_filter_linear;
	sampler_info.address_mode_u = zest_sampler_address_mode_clamp_to_edge;
	sampler_info.address_mode_v = zest_sampler_address_mode_clamp_to_edge;
	sampler_info.address_mode_w = zest_sampler_address_mode_clamp_to_edge;
	zest_sampler_handle sampler_handle = zest_CreateSampler(device, &sampler_info);
	sampler_info.min_filter = zest_filter_nearest;
	sampler_info.mag_filter = zest_filter_nearest;
	zest_sampler_handle decoy_sampler_handle = zest_CreateSampler(device, &sampler_info);
	zest_sampler sampler = zest_GetSampler(sampler_handle);
	zest_sampler decoy_sampler = zest_GetSampler(decoy_sampler_handle);

	zest_image_flags cube_flags = zest_image_preset_texture | zest_image_flag_cubemap;
	zest_image_handle image_handles[11];
	zest_image texture_2d = metal_create_solid_image(device, &image_handles[0], 2, 1, 1, zest_image_preset_texture, NULL);
	zest_image texture_cube = metal_create_solid_image(device, &image_handles[1], 1, 1, 6, cube_flags, cube_colors);
	zest_image texture_array = metal_create_solid_image(device, &image_handles[2], 1, 1, 2, zest_image_preset_texture, array_colors);
	zest_image texture_3d = metal_create_solid_image(device, &image_handles[3], 1, 2, 1, zest_image_preset_texture_3d, volume_color);
	zest_image texture_cube_array = metal_create_solid_image(device, &image_handles[4], 1, 1, 12, cube_flags, cube_array_colors);
	zest_image decoy_2d = metal_create_solid_image(device, &image_handles[5], 2, 1, 1, zest_image_preset_texture, decoy_colors);
	zest_image decoy_cube = metal_create_solid_image(device, &image_handles[6], 1, 1, 6, cube_flags, decoy_colors);
	zest_image decoy_array = metal_create_solid_image(device, &image_handles[7], 1, 1, 2, zest_image_preset_texture, decoy_colors);
	zest_image decoy_3d = metal_create_solid_image(device, &image_handles[8], 1, 2, 1, zest_image_preset_texture_3d, decoy_colors);
	zest_image decoy_cube_array = metal_create_solid_image(device, &image_handles[9], 1, 1, 12, cube_flags, decoy_colors);
	zest_image decoy_storage_image = metal_create_solid_image(device, &image_handles[10], 8, 1, 1, zest_image_preset_storage | zest_image_flag_transfer_src, NULL);
	zest_image_info_t storage_info = zest_CreateImageInfo(16, 16);
	storage_info.format = zest_format_r8g8b8a8_unorm;
	storage_info.flags = zest_image_preset_storage | zest_image_flag_transfer_dst | zest_image_flag_transfer_src;
	storage_info.mip_levels = 2;
	zest_image_handle storage_handle = zest_CreateImage(device, &storage_info);
	zest_image storage_image = zest_GetImage(storage_handle);
	zest_image_handle mip_texture_handle = metal_create_mip_image(device, zest_format_r8g8b8a8_unorm);
	zest_image mip_texture = zest_GetImage(mip_texture_handle);
	int failed = task.compute == NULL || !sampler || !decoy_sampler || !storage_image || !mip_texture;
	failed |= !texture_2d || !texture_cube || !texture_array || !texture_3d || !texture_cube_array;
	failed |= !decoy_2d || !decoy_cube || !decoy_array || !decoy_3d || !decoy_cube_array || !decoy_storage_image;
	zest_uint upload[256];
	for (zest_uint pixel = 0; pixel != 256; ++pixel) upload[pixel] = 10u | (20u << 8) | (30u << 16) | (255u << 24);
	if (!failed) {
		unsigned char rows[2 * 2 * 4];
		memcpy(rows, two_texels, 8);
		memcpy(rows + 8, two_texels, 8);
		failed |= !zest_CopyBitmapToImage(device, rows, sizeof(rows), texture_2d, 2, 2);
		failed |= !zest_CopyBitmapToImage(device, upload, sizeof(upload), storage_image, 16, 16);
		failed |= !metal_upload_mip_patterns(device, mip_texture, 43);
	}

	zest_buffer_info_t upload_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_cpu_to_gpu);
	zest_buffer_info_t uniform_info = zest_CreateBufferInfo(zest_buffer_type_uniform, zest_memory_usage_cpu_to_gpu);
	zest_buffer raw_buffer = zest_CreateBuffer(device, 256, &upload_info);
	zest_buffer structured_buffer = zest_CreateBuffer(device, 48 * METAL_STRUCTURED_COUNT, &upload_info);
	zest_buffer result_buffer = zest_CreateBuffer(device, 256, &upload_info);
	zest_buffer decoy_buffer = zest_CreateBuffer(device, 48 * METAL_STRUCTURED_COUNT, &upload_info);
	zest_buffer uniform_buffer = zest_CreateBuffer(device, 256, &uniform_info);
	zest_buffer decoy_uniform_buffer = zest_CreateBuffer(device, 256, &uniform_info);
	zest_buffer copy_target = zest_CreateBuffer(device, 256, &upload_info);
	failed |= !raw_buffer || !structured_buffer || !result_buffer || !decoy_buffer || !uniform_buffer || !decoy_uniform_buffer || !copy_target;
	zest_image_view_array_handle view_array_handle = {};
	zest_image_view_array_handle texture_views_handle = {};
	zest_mtl_device_backend_t *device_backend = ZEST_MTL_BACKEND(device, device);
	zest_uint views_listed_before = zest_vec_size(device_backend->texture_views);
	if (!failed) {
		memset(zest_BufferData(raw_buffer), 0, 256);
		*(zest_uint *)zest_BufferData(raw_buffer) = 41;
		unsigned char *structured_bytes = (unsigned char *)zest_BufferData(structured_buffer);
		memset(structured_bytes, 0xCD, 48 * METAL_STRUCTURED_COUNT);
		zest_uint expected_structured_sum = 0;
		for (zest_uint element = 0; element != METAL_STRUCTURED_COUNT; ++element) {
			memcpy(structured_bytes + element * 48, &element, sizeof(element));
			expected_structured_sum += element;
		}
		//The decoys hold the most a wrong read could add up to, so they can't pass for the real thing
		memset(zest_BufferData(decoy_buffer), 0x7F, 48 * METAL_STRUCTURED_COUNT);
		memset(zest_BufferData(result_buffer), 0, 256);
		float uniform_tint[4] = { 0.25f, 0.5f, 0.75f, 1.f };
		zest_uint uniform_add = 1000;
		memset(zest_BufferData(uniform_buffer), 0, 256);
		memcpy(zest_BufferData(uniform_buffer), uniform_tint, sizeof(uniform_tint));
		memcpy((char *)zest_BufferData(uniform_buffer) + 16, &uniform_add, sizeof(uniform_add));
		memset(zest_BufferData(decoy_uniform_buffer), 0, 256);
		memset(zest_BufferData(copy_target), 0, 256);
		task.copy_source = raw_buffer;
		task.copy_target = copy_target;

		view_array_handle = zest_CreateImageViewsPerMip(device, storage_image);
		zest_image_view_array view_array = view_array_handle.value ? zest_GetImageViewArray(view_array_handle) : NULL;
		zest_uint *mip_indexes = view_array ? zest_AcquireImageMipIndexes(device, storage_image, view_array, zest_storage_image_binding, zest_descriptor_type_storage_image) : NULL;
		failed |= zest_vec_size(mip_indexes) != 2;
		texture_views_handle = zest_CreateImageViewsPerMip(device, mip_texture);
		zest_image_view_array texture_views = texture_views_handle.value ? zest_GetImageViewArray(texture_views_handle) : NULL;
		zest_uint *texture_mip_indexes = texture_views ? zest_AcquireImageMipIndexes(device, mip_texture, texture_views, zest_texture_2d_binding, zest_descriptor_type_sampled_image) : NULL;
		failed |= zest_vec_size(texture_mip_indexes) != 3;
		//Under shader validation all five mip views are listed, and every other image here is its own default view
		failed |= zest_vec_size(device_backend->texture_views) != views_listed_before + (device_backend->declares_texture_views ? 5 : 0);

		zest_descriptor_set set = device->bindless_set;
		const zest_mtl_bindless_regions_t *regions = &ZEST_MTL_BACKEND(descriptor_set, set)->pool->regions;
		zest_platform platform = device->platform;
		zest_uint indexes[zest_max_global_binding_number];
		for (zest_uint binding_number = 0; binding_number != zest_max_global_binding_number; ++binding_number) {
			indexes[binding_number] = regions->counts[binding_number] / 2;
			failed |= regions->counts[binding_number] < 16;
		}
		indexes[zest_storage_image_binding] = mip_indexes ? mip_indexes[1] : 0;
		failed |= indexes[zest_storage_image_binding] == 0 || indexes[zest_storage_image_binding] + 1 >= regions->counts[zest_storage_image_binding];
		if (!failed) {
			for (int side = -1; side <= 1; side += 2) {
				zest_uint offset = (zest_uint)side;
				platform->update_bindless_image_descriptor(device, zest_sampler_binding, indexes[zest_sampler_binding] + offset, zest_descriptor_type_sampler, NULL, NULL, decoy_sampler, set);
				platform->update_bindless_image_descriptor(device, zest_texture_2d_binding, indexes[zest_texture_2d_binding] + offset, zest_descriptor_type_sampled_image, decoy_2d, decoy_2d->default_view, NULL, set);
				platform->update_bindless_image_descriptor(device, zest_texture_cube_binding, indexes[zest_texture_cube_binding] + offset, zest_descriptor_type_sampled_image, decoy_cube, decoy_cube->default_view, NULL, set);
				platform->update_bindless_image_descriptor(device, zest_texture_array_binding, indexes[zest_texture_array_binding] + offset, zest_descriptor_type_sampled_image, decoy_array, decoy_array->default_view, NULL, set);
				platform->update_bindless_image_descriptor(device, zest_texture_3d_binding, indexes[zest_texture_3d_binding] + offset, zest_descriptor_type_sampled_image, decoy_3d, decoy_3d->default_view, NULL, set);
				platform->update_bindless_image_descriptor(device, zest_texture_cube_array_binding, indexes[zest_texture_cube_array_binding] + offset, zest_descriptor_type_sampled_image, decoy_cube_array, decoy_cube_array->default_view, NULL, set);
				platform->update_bindless_image_descriptor(device, zest_storage_image_binding, indexes[zest_storage_image_binding] + offset, zest_descriptor_type_storage_image, decoy_storage_image, decoy_storage_image->default_view, NULL, set);
				platform->update_bindless_uniform_buffer_descriptor(device, zest_uniform_buffer_binding, indexes[zest_uniform_buffer_binding] + offset, decoy_uniform_buffer, set);
			}
			//Slot 5 alternates decoys and the three real buffers: decoy, raw, decoy, structured, decoy, result, decoy
			zest_uint buffer_base = indexes[zest_storage_buffer_binding];
			zest_buffer slot_buffers[7] = { decoy_buffer, raw_buffer, decoy_buffer, structured_buffer, decoy_buffer, result_buffer, decoy_buffer };
			for (zest_uint entry = 0; entry != 7; ++entry) {
				platform->update_bindless_storage_buffer_descriptor(device, zest_storage_buffer_binding, buffer_base - 1 + entry, slot_buffers[entry], 0, set);
			}
			task.push.sampler_index = indexes[zest_sampler_binding];
			task.push.texture_2d_index = indexes[zest_texture_2d_binding];
			task.push.texture_cube_index = indexes[zest_texture_cube_binding];
			task.push.texture_array_index = indexes[zest_texture_array_binding];
			task.push.texture_3d_index = indexes[zest_texture_3d_binding];
			task.push.texture_cube_array_index = indexes[zest_texture_cube_array_binding];
			task.push.raw_index = buffer_base;
			task.push.structured_index = buffer_base + 2;
			task.push.result_index = buffer_base + 4;
			task.push.storage_image_index = indexes[zest_storage_image_binding];
			task.push.uniform_index = indexes[zest_uniform_buffer_binding];
			task.push.structured_count = METAL_STRUCTURED_COUNT;
			task.push.mip_view_index = texture_mip_indexes ? texture_mip_indexes[1] : 0;
			platform->update_bindless_image_descriptor(device, zest_sampler_binding, task.push.sampler_index, zest_descriptor_type_sampler, NULL, NULL, sampler, set);
			platform->update_bindless_image_descriptor(device, zest_texture_2d_binding, task.push.texture_2d_index, zest_descriptor_type_sampled_image, texture_2d, texture_2d->default_view, NULL, set);
			platform->update_bindless_image_descriptor(device, zest_texture_cube_binding, task.push.texture_cube_index, zest_descriptor_type_sampled_image, texture_cube, texture_cube->default_view, NULL, set);
			platform->update_bindless_image_descriptor(device, zest_texture_array_binding, task.push.texture_array_index, zest_descriptor_type_sampled_image, texture_array, texture_array->default_view, NULL, set);
			platform->update_bindless_image_descriptor(device, zest_texture_3d_binding, task.push.texture_3d_index, zest_descriptor_type_sampled_image, texture_3d, texture_3d->default_view, NULL, set);
			platform->update_bindless_image_descriptor(device, zest_texture_cube_array_binding, task.push.texture_cube_array_index, zest_descriptor_type_sampled_image, texture_cube_array, texture_cube_array->default_view, NULL, set);
			platform->update_bindless_uniform_buffer_descriptor(device, zest_uniform_buffer_binding, task.push.uniform_index, uniform_buffer, set);

			if (zest_BeginCommandGraph(context, "Reads Descriptors", 0)) {
				zest_resource_node image_node = zest_ImportImageResource("Mip Image", storage_image, 0);
				zest_BeginComputePass("Read Descriptors");
				zest_ConnectOutput(image_node);
				zest_SetPassTask(metal_reads_descriptors_task, &task);
				zest_EndPass();
				zest_frame_graph frame_graph = zest_EndFrameGraph();
				failed |= frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
			} else {
				failed = 1;
			}
			const float *results = (const float *)zest_BufferData(result_buffer);
			ZEST_PRINT("\tRaw and uniform %.1f (expected 1041), 2D sample %.2f %.2f %.2f, structured sum %.0f (expected %u)", results[0], results[4], results[5], results[6], results[24], expected_structured_sum);
			failed |= results[0] != 1041.f || results[1] != 0.25f || results[2] != 0.5f || results[3] != 0.75f;
			//Half way between a red and a blue texel through the linear sampler
			failed |= results[4] < 0.48f || results[4] > 0.52f || results[5] != 0.f || results[6] < 0.48f || results[6] > 0.52f;
			failed |= !metal_color_near(results + 8, cube_colors);
			failed |= !metal_color_near(results + 12, array_colors + 4);
			failed |= !metal_color_near(results + 16, volume_color);
			failed |= !metal_color_near(results + 20, cube_array_colors + 6 * 4);
			failed |= results[24] != (float)expected_structured_sum;
			unsigned char mip_pattern[8 * 8 * 4];
			metal_mip_pattern(mip_pattern, 1, 43);
			const unsigned char *expected_texel = mip_pattern + (5 * 8 + 2) * 4;
			ZEST_PRINT("\tTexel (2, 5) through the mip 1 view %.0f %.0f %.0f %.0f (expected %u %u %u %u)", results[28] * 255.f, results[29] * 255.f, results[30] * 255.f, results[31] * 255.f, expected_texel[0], expected_texel[1], expected_texel[2], expected_texel[3]);
			failed |= !metal_color_near(results + 28, expected_texel);
			unsigned char mip_pixels[8 * 8 * 4];
			memset(mip_pixels, 0, sizeof(mip_pixels));
			failed |= !metal_read_texture(device, storage_image, 1, 0, 8, 8, 4, mip_pixels);
			zest_uint wrong_written = 0;
			for (int pixel = 0; pixel != 64; ++pixel) {
				wrong_written += mip_pixels[pixel * 4 + 0] != pixel % 8 || mip_pixels[pixel * 4 + 1] != pixel / 8 || mip_pixels[pixel * 4 + 3] != 255;
			}
			unsigned char base_pixels[16 * 16 * 4];
			memset(base_pixels, 0, sizeof(base_pixels));
			failed |= !metal_read_texture(device, storage_image, 0, 0, 16, 16, 4, base_pixels);
			zest_uint changed_base = 0;
			for (int pixel = 0; pixel != 256; ++pixel) changed_base += memcmp(base_pixels + pixel * 4, &upload[pixel], 4) != 0;
			const zest_uint *decoy_words = (const zest_uint *)zest_BufferData(decoy_buffer);
			zest_uint changed_decoy = 0;
			for (zest_uint word = 0; word != 48 * METAL_STRUCTURED_COUNT / 4; ++word) changed_decoy += decoy_words[word] != 0x7F7F7F7F;
			ZEST_PRINT("\t%u of 64 mip 1 pixels wrong, %u of 256 mip 0 pixels changed, %u decoy buffer words changed", wrong_written, changed_base, changed_decoy);
			failed |= wrong_written != 0 || changed_base != 0 || changed_decoy != 0;
			failed |= *(const zest_uint *)zest_BufferData(copy_target) != 41;
		}
	}
	failed |= zest_GetValidationErrorCount(device) != 0;
	if (view_array_handle.value) zest_FreeImageViewArrayNow(view_array_handle);
	if (texture_views_handle.value) zest_FreeImageViewArrayNow(texture_views_handle);
	failed |= zest_vec_size(device_backend->texture_views) != views_listed_before;
	if (mip_texture) zest_FreeImageNow(mip_texture_handle);
	zest_buffer buffers[7] = { raw_buffer, structured_buffer, result_buffer, decoy_buffer, uniform_buffer, decoy_uniform_buffer, copy_target };
	for (int buffer_index = 0; buffer_index != 7; ++buffer_index) {
		if (buffers[buffer_index]) zest_FreeBufferNow(buffers[buffer_index]);
	}
	if (storage_image) zest_FreeImageNow(storage_handle);
	for (int image_index = 0; image_index != 11; ++image_index) {
		if (image_handles[image_index].value) zest_FreeImageNow(image_handles[image_index]);
	}
	zest_FreeSamplerNow(sampler_handle);
	zest_FreeSamplerNow(decoy_sampler_handle);
	if (task.compute) zest_FreeCompute(compute_handle);
	zest_FreeShader(shader);
	zest_DestroyContext(context);
	return failed | finish_metal_device(device);
}

static const char *metal_sample_vertex_source =
	"#version 450\n"
	"void main() {\n"
	"	vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);\n"
	"	gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);\n"
	"}\n";

static const char *metal_sample_fragment_source =
	"#version 450\n"
	"#extension GL_EXT_nonuniform_qualifier : require\n"
	"layout(set = 0, binding = 0) uniform sampler samplers[];\n"
	"layout(set = 0, binding = 1) uniform texture2D textures[];\n"
	"layout(push_constant) uniform push_block { uint texture_index; uint sampler_index; } push;\n"
	"layout(location = 0) out vec4 out_colour;\n"
	"void main() { out_colour = texelFetch(sampler2D(textures[push.texture_index], samplers[push.sampler_index]), ivec2(gl_FragCoord.xy), 0); }\n";

struct MetalEncoderOrder {
	zest_pipeline_template pipeline_template;
	zest_uint indexes[2];				//The source's texture index and the sampler's
	zest_compute compute;
	TestPushConstants pushes[2];		//Writing the first buffer, then the second
	zest_buffer written_buffers[2];
	zest_buffer readback_buffers[2];	//What the dispatch and copy pass copied out, then what the last transfer pass did
};

//Ends on a dispatch, so the render pass after it needs the compute encoder ended by the pass's release
static void metal_order_dispatch_task(const zest_command_list command_list, void *user_data) {
	MetalEncoderOrder *order = (MetalEncoderOrder *)user_data;
	zest_cmd_BindComputePipeline(command_list, order->compute);
	zest_cmd_SendPushConstants(command_list, &order->pushes[0], sizeof(order->pushes[0]));
	zest_cmd_DispatchCompute(command_list, 8, 1, 1);
}

//A copy straight after a dispatch in one pass, so the blit encoder has to end the compute encoder
static void metal_order_dispatch_copy_task(const zest_command_list command_list, void *user_data) {
	MetalEncoderOrder *order = (MetalEncoderOrder *)user_data;
	zest_cmd_BindComputePipeline(command_list, order->compute);
	zest_cmd_SendPushConstants(command_list, &order->pushes[1], sizeof(order->pushes[1]));
	zest_cmd_DispatchCompute(command_list, 8, 1, 1);
	zest_cmd_CopyBuffer(command_list, order->written_buffers[1], order->readback_buffers[0], METAL_IMMEDIATE_ELEMENTS * 16);
}

static void metal_order_buffer_copy_task(const zest_command_list command_list, void *user_data) {
	MetalEncoderOrder *order = (MetalEncoderOrder *)user_data;
	zest_cmd_CopyBuffer(command_list, order->written_buffers[0], order->readback_buffers[1], METAL_IMMEDIATE_ELEMENTS * 16);
}

static zest_uint metal_elements_holding_index(zest_buffer buffer) {
	const float *values = (const float *)zest_BufferData(buffer);
	zest_uint matching_elements = 0;
	for (zest_uint element = 0; element != METAL_IMMEDIATE_ELEMENTS; ++element) {
		matching_elements += values[element * 4] == (float)element && values[element * 4 + 1] == 0.f && values[element * 4 + 2] == 0.f && values[element * 4 + 3] == 0.f;
	}
	return matching_elements;
}

static void metal_order_copy_task(const zest_command_list command_list, void *user_data) {
	zest_resource_node source = zest_GetPassInputResource(command_list, "Order Source");
	zest_resource_node destination = zest_GetPassOutputResource(command_list, "Order Destination");
	zest_cmd_CopyImageMip(command_list, source, destination, 0, zest_shader_fragment_stage);
}

static void metal_order_readback_task(const zest_command_list command_list, void *user_data) {
	zest_resource_node source = zest_GetPassInputResource(command_list, "Order Target");
	zest_resource_node destination = zest_GetPassOutputResource(command_list, "Order Readback");
	zest_cmd_CopyImageMip(command_list, source, destination, 0, zest_shader_fragment_stage);
}

static void metal_order_sample_task(const zest_command_list command_list, void *user_data) {
	MetalEncoderOrder *order = (MetalEncoderOrder *)user_data;
	set_full_viewport(command_list);
	zest_pipeline pipeline = zest_GetPipeline(order->pipeline_template, command_list);
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_SendPushConstants(command_list, order->indexes, sizeof(order->indexes));
	zest_cmd_Draw(command_list, 3, 1, 0, 0);
}

struct MetalIndirectDraws {
	VertexDrawResources resources;
	zest_buffer commands;
	zest_uint stride;
	zest_uint draw_count;
};

static void metal_indirect_task(const zest_command_list command_list, void *user_data) {
	MetalIndirectDraws *draws = (MetalIndirectDraws *)user_data;
	set_full_viewport(command_list);
	zest_cmd_BindVertexBuffer(command_list, 0, 1, draws->resources.vertex_buffer);
	zest_cmd_BindIndexBuffer(command_list, draws->resources.index_buffer);
	zest_pipeline pipeline = zest_GetPipeline(draws->resources.pipeline_template, command_list);
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	float tint[4] = { 1.f, 0.5f, 1.f, 1.f };
	zest_cmd_SendPushConstants(command_list, tint, sizeof(tint));
	zest_cmd_DrawIndexedIndirect(command_list, draws->commands, 0, draws->draw_count, draws->stride);
}

//Packed and padded commands both draw, a stride shorter than the command is refused, and no commands draws nothing
int test_metal_indirect_draws(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_metal_device(device);
	MetalIndirectDraws draws = {};
	int failed = !create_vertex_draw_resources(device, &draws.resources);
	zest_buffer_info_t indirect_info = zest_CreateBufferInfo(zest_buffer_type_indirect, zest_memory_usage_cpu_to_gpu);
	draws.commands = zest_CreateBuffer(device, 64, &indirect_info);
	zest_image_handle target_handle = create_draw_target(device);
	zest_image target = zest_GetImage(target_handle);
	failed |= !draws.commands || !target;
	if (!failed) {
		const zest_draw_indexed_indirect_command_t left = { 6, 1, 0, 0, 0 };
		const zest_draw_indexed_indirect_command_t right = { 6, 1, 0, 4, 0 };
		const zest_uint strides[2] = { (zest_uint)sizeof(zest_draw_indexed_indirect_command_t), 32 };
		const int black[4] = { 0, 0, 0, 255 };
		zest_uint pixels[256];
		draws.draw_count = 2;
		for (int stride_index = 0; stride_index != 2; ++stride_index) {
			char *commands = (char *)zest_BufferData(draws.commands);
			memset(commands, 0, 64);
			memcpy(commands, &left, sizeof(left));
			memcpy(commands + strides[stride_index], &right, sizeof(right));
			draws.stride = strides[stride_index];
			failed |= render_draw_pass(context, "Indirect Stride", target, ZEST_FALSE, metal_indirect_task, &draws, pixels);
			failed |= !columns_hold(pixels, 0, 16, vertex_draw_left);
		}
		failed |= zest_GetValidationErrorCount(device) != 0;
		draws.stride = 12;
		failed |= render_draw_pass(context, "Indirect Short Stride", target, ZEST_FALSE, metal_indirect_task, &draws, pixels);
		failed |= zest_GetValidationErrorCount(device) != 1 || !columns_hold(pixels, 0, 16, black);
		draws.stride = 32;
		draws.draw_count = 0;
		failed |= render_draw_pass(context, "Indirect No Commands", target, ZEST_FALSE, metal_indirect_task, &draws, pixels);
		failed |= zest_GetValidationErrorCount(device) != 1 || !columns_hold(pixels, 0, 16, black);
		zest_ResetValidationErrors(device);
	}
	if (target) zest_FreeImageNow(target_handle);
	if (draws.commands) zest_FreeBufferNow(draws.commands);
	free_vertex_draw_resources(&draws.resources);
	zest_DestroyContext(context);
	return failed | finish_metal_device(device);
}

//Copies, dispatches and a render pass that samples what was copied, taking turns in one command buffer
int test_metal_pass_encoder_order(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_metal_device(device);
	zest_image_info_t texture_info = zest_CreateImageInfo(16, 16);
	texture_info.format = zest_format_r8g8b8a8_unorm;
	texture_info.flags = zest_image_preset_texture | zest_image_flag_transfer_src;
	zest_image_handle readback_handle = zest_CreateImage(device, &texture_info);
	//The render pass samples the copy through a mip view, and a mip copy needs the pattern's mip count to match
	texture_info.mip_levels = 2;
	zest_image_handle pattern_handle = zest_CreateImage(device, &texture_info);
	zest_image_handle copy_handle = zest_CreateImage(device, &texture_info);
	zest_image_handle target_handle = create_draw_target(device);
	zest_image pattern = zest_GetImage(pattern_handle);
	zest_image copy = zest_GetImage(copy_handle);
	zest_image readback = zest_GetImage(readback_handle);
	zest_image target = zest_GetImage(target_handle);
	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	zest_sampler_handle sampler_handle = zest_CreateSampler(device, &sampler_info);
	zest_shader_handle vertex_shader = zest_CreateShader(device, metal_sample_vertex_source, zest_vertex_shader, "order_vertex", NULL, ZEST_TRUE);
	zest_shader_handle fragment_shader = zest_CreateShader(device, metal_sample_fragment_source, zest_fragment_shader, "order_fragment", NULL, ZEST_TRUE);
	MetalEncoderOrder order = {};
	order.pipeline_template = metal_no_input_template(device, "Order Sample", vertex_shader, fragment_shader);
	zest_shader_handle compute_shader = zest_CreateShaderFromFile(device, "examples/SDL2/zest-tests/shaders/glsl/buffer_write.comp", "order_buffer_write", zest_compute_shader, NULL, ZEST_TRUE);
	zest_compute_handle compute_handle = zest_CreateCompute(device, "Order Buffer Write", compute_shader);
	order.compute = compute_handle.value ? zest_GetCompute(compute_handle) : NULL;
	zest_buffer_info_t written_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	zest_buffer_info_t readback_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_gpu_to_cpu);
	int failed = !pattern || !copy || !readback || !target || !order.compute;
	for (int buffer_index = 0; buffer_index != 2; ++buffer_index) {
		order.written_buffers[buffer_index] = zest_CreateBuffer(device, METAL_IMMEDIATE_ELEMENTS * 16, &written_info);
		order.readback_buffers[buffer_index] = zest_CreateBuffer(device, METAL_IMMEDIATE_ELEMENTS * 16, &readback_info);
		failed |= !order.written_buffers[buffer_index] || !order.readback_buffers[buffer_index];
	}
	unsigned char pattern_bytes[16 * 16 * 4];
	unsigned char zero_bytes[16 * 16 * 4] = {};
	metal_mip_pattern(pattern_bytes, 0, 17);
	//The copy starts as zeros, which also gives it the layout a sampled index needs
	failed |= failed || !metal_upload_layers(device, pattern, pattern_bytes, sizeof(pattern_bytes), 1) || !metal_upload_layers(device, copy, zero_bytes, sizeof(zero_bytes), 1);
	zest_image_view_array_handle copy_views_handle = {};
	if (!failed) {
		copy_views_handle = zest_CreateImageViewsPerMip(device, copy);
		zest_image_view_array copy_views = copy_views_handle.value ? zest_GetImageViewArray(copy_views_handle) : NULL;
		zest_uint *copy_mip_indexes = copy_views ? zest_AcquireImageMipIndexes(device, copy, copy_views, zest_texture_2d_binding, zest_descriptor_type_sampled_image) : NULL;
		failed |= zest_vec_size(copy_mip_indexes) != 2;
		order.indexes[0] = copy_mip_indexes ? copy_mip_indexes[0] : 0;
		order.indexes[1] = zest_AcquireSamplerIndex(device, zest_GetSampler(sampler_handle));
		zest_resource_node written_nodes[2];
		zest_resource_node buffer_readback_nodes[2];
		for (int buffer_index = 0; buffer_index != 2; ++buffer_index) {
			order.pushes[buffer_index].index1 = (int)zest_AcquireStorageBufferIndex(device, order.written_buffers[buffer_index]);
			memset(zest_BufferData(order.readback_buffers[buffer_index]), 0xFF, METAL_IMMEDIATE_ELEMENTS * 16);
		}
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginCommandGraph(context, "Pass Encoder Order", 0)) {
			zest_resource_node pattern_node = zest_ImportImageResource("Order Source", pattern, 0);
			zest_resource_node copy_node = zest_ImportImageResource("Order Destination", copy, 0);
			zest_resource_node target_node = zest_ImportImageResource("Order Target", target, 0);
			zest_resource_node readback_node = zest_ImportImageResource("Order Readback", readback, 0);
			written_nodes[0] = zest_ImportBufferResource("Order First Written Buffer", order.written_buffers[0], 0);
			written_nodes[1] = zest_ImportBufferResource("Order Second Written Buffer", order.written_buffers[1], 0);
			buffer_readback_nodes[0] = zest_ImportBufferResource("Order First Buffer Readback", order.readback_buffers[0], 0);
			buffer_readback_nodes[1] = zest_ImportBufferResource("Order Second Buffer Readback", order.readback_buffers[1], 0);
			zest_SetResourceClearColor(target_node, 0.f, 0.f, 0.f, 1.f);
			//Every pass reads what the one before it made, so each wave has one queue type and all six stay in the graphics batch
			zest_BeginTransferPass("Copy The Pattern");
			zest_ConnectInput(pattern_node);
			zest_ConnectOutput(copy_node);
			zest_SetPassTask(metal_order_copy_task, NULL);
			zest_EndPass();
			zest_BeginComputePass("Write A Buffer");
			zest_ConnectInput(copy_node);
			zest_ConnectOutput(written_nodes[0]);
			zest_SetPassTask(metal_order_dispatch_task, &order);
			zest_EndPass();
			zest_BeginRenderPass("Sample The Copy");
			zest_ConnectInput(copy_node);
			zest_ConnectInput(written_nodes[0]);
			zest_ConnectOutput(target_node);
			zest_SetPassTask(metal_order_sample_task, &order);
			zest_EndPass();
			zest_BeginTransferPass("Copy What Was Drawn");
			zest_ConnectInput(target_node);
			zest_ConnectOutput(readback_node);
			zest_SetPassTask(metal_order_readback_task, NULL);
			zest_EndPass();
			zest_BeginComputePass("Write And Copy A Buffer");
			zest_ConnectInput(readback_node);
			zest_ConnectOutput(written_nodes[1]);
			zest_ConnectOutput(buffer_readback_nodes[0]);
			zest_SetPassTask(metal_order_dispatch_copy_task, &order);
			zest_EndPass();
			zest_BeginTransferPass("Copy The First Buffer");
			zest_ConnectInput(written_nodes[0]);
			zest_ConnectInput(buffer_readback_nodes[0]);
			zest_ConnectOutput(buffer_readback_nodes[1]);
			zest_SetPassTask(metal_order_buffer_copy_task, &order);
			zest_EndPass();
			frame_graph = zest_EndFrameGraph();
		}
		zest_uint most_passes = 0;
		zest_uint batch_count = frame_graph ? metal_graph_batch_count(frame_graph, &most_passes) : 0;
		if (batch_count != 1 || most_passes != 6) {
			ZEST_PRINT("\tThe six passes went into %u batches with at most %u passes, where the test needs all six in one.", batch_count, most_passes);
			failed = 1;
		}
		failed |= frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
		unsigned char read_bytes[16 * 16 * 4];
		memset(read_bytes, 0, sizeof(read_bytes));
		failed |= !metal_read_texture(device, readback, 0, 0, 16, 16, 4, read_bytes);
		zest_uint matching_bytes = 0;
		for (zest_uint byte = 0; byte != sizeof(read_bytes); ++byte) matching_bytes += read_bytes[byte] == pattern_bytes[byte];
		zest_uint copied_in_pass = metal_elements_holding_index(order.readback_buffers[0]);
		zest_uint copied_after = metal_elements_holding_index(order.readback_buffers[1]);
		ZEST_PRINT("\t%u of %u bytes of the image readback hold the pattern. Of %u elements, %u copied in the dispatch's pass and %u copied by a later pass hold their index.", matching_bytes, (zest_uint)sizeof(read_bytes), METAL_IMMEDIATE_ELEMENTS, copied_in_pass, copied_after);
		failed |= matching_bytes != sizeof(read_bytes) || copied_in_pass != METAL_IMMEDIATE_ELEMENTS || copied_after != METAL_IMMEDIATE_ELEMENTS;
	}
	if (order.compute) zest_FreeCompute(compute_handle);
	zest_FreeShader(compute_shader);
	for (int buffer_index = 0; buffer_index != 2; ++buffer_index) {
		if (order.written_buffers[buffer_index]) zest_FreeBufferNow(order.written_buffers[buffer_index]);
		if (order.readback_buffers[buffer_index]) zest_FreeBufferNow(order.readback_buffers[buffer_index]);
	}
	zest_FreePipelineTemplate(order.pipeline_template);
	zest_FreeShader(vertex_shader);
	zest_FreeShader(fragment_shader);
	zest_FreeSamplerNow(sampler_handle);
	if (pattern) zest_FreeImageNow(pattern_handle);
	if (copy_views_handle.value) zest_FreeImageViewArrayNow(copy_views_handle);
	if (copy) zest_FreeImageNow(copy_handle);
	if (readback) zest_FreeImageNow(readback_handle);
	if (target) zest_FreeImageNow(target_handle);
	zest_DestroyContext(context);
	return failed | finish_metal_device(device);
}

//Text and a rectangle before each frame, which the frame's swapchain pass draws and then resets
static int run_metal_overlay_frames(zest_context context, int frame_count) {
	int failed = 0;
	MetalFrameRun run = {};
	for (int frame = 0; frame != frame_count; ++frame) {
		zest_DrawDebugText(context, 4.0f, 4.0f, 0xFFFFFFFF, "Zest %d", frame);
		zest_DrawDebugRect(context, 4.0f, 16.0f, 64.0f, 8.0f, 0xCC000000);
		failed |= !run_metal_windowed_frame(context, ZEST_FALSE, &run);
		failed |= context->db_overlay.vertex_count != 0;
	}
	failed |= metal_frame_run_failed(context, &run, "debug overlay");
	return failed;
}

//The overlay's template is valid, and the context holds a pipeline built from it with a Metal pipeline state
static int metal_overlay_pipeline_failed(zest_context context) {
	int built_count = 0;
	zest_map_foreach(pipeline_index, context->cached_pipelines) {
		zest_pipeline pipeline = *zest_map_at_index(context->cached_pipelines, zest_map_index(context->cached_pipelines, pipeline_index));
		built_count += pipeline->pipeline_template == context->db_overlay.pipeline && ZEST_MTL_BACKEND(pipeline, pipeline)->pipeline_state != nil;
	}
	int failed = !zest_PipelineIsValid(context->db_overlay.pipeline) || built_count != 1;
	if (failed) ZEST_PRINT("The overlay's template is %s, and %i of the context's pipelines were built from it.", zest_PipelineIsValid(context->db_overlay.pipeline) ? "valid" : "invalid", built_count);
	return failed;
}

int test_metal_debug_overlay(void) {
	MetalTestWindow window;
	if (!metal_open_test_window(&window, "Metal Debug Overlay")) {
		metal_close_test_window(&window);
		return 1;
	}
	zest_device device = zest_implsdl2_CreateMetalDevice(&window.window_data, MetalTestFlags());
	if (!device) {
		metal_close_test_window(&window);
		return 1;
	}
	zest_create_context_info_t info = zest_CreateContextInfo();
	info.flags |= zest_context_init_flag_debug_overlay;
	zest_context context = zest_CreateContext(device, &window.window_data, &info);
	int failed = context == NULL;
	if (context) {
		metal_set_test_clear_color(context);
		failed |= run_metal_overlay_frames(context, 8);
		failed |= metal_overlay_pipeline_failed(context);
		//The reset destroys the context and empties the function cache, so the next overlay context converts its shaders again
		zest_ResetDevice(device);
		context = zest_CreateContext(device, &window.window_data, &info);
		failed |= context == NULL;
		if (context) {
			metal_set_test_clear_color(context);
			failed |= run_metal_overlay_frames(context, 2);
			failed |= metal_overlay_pipeline_failed(context);
			zest_DestroyContext(context);
		}
	}
	failed |= finish_metal_device(device);
	metal_close_test_window(&window);
	return failed;
}

#define METAL_GATE_DELAY 200000			//Microseconds a gate holds its queue
#define METAL_CROSS_WORD_COUNT 256

//A queue held behind a command buffer that waits for the gate's event, which a thread sets after a delay
struct MetalGate {
	id<MTLSharedEvent> event;			//The held queue waits for value 1
	pthread_t thread;
	zest_bool thread_started;
	zest_microsecs delay;				//How long the thread waits before it opens the gate
	id<MTLSharedEvent> watched_event;	//Read just before the gate opens, when set
	zest_u64 watched_value;
	zest_bool watched_reached;
};

static void *metal_gate_thread(void *user_data) {
	MetalGate *gate = (MetalGate *)user_data;
	struct timespec wait_time = { (time_t)(gate->delay / 1000000), (long)(gate->delay % 1000000) * 1000 };
	nanosleep(&wait_time, NULL);
	if (gate->watched_event) gate->watched_reached = [gate->watched_event signaledValue] >= gate->watched_value;
	[gate->event setSignaledValue:1];
	return NULL;
}

//Everything committed to the queue after this waits until the gate opens
static zest_bool metal_close_gate(zest_device device, MetalGate *gate, id<MTLCommandQueue> native_queue) {
	gate->event = [zest_GetMetalDevice(device) newSharedEvent];
	if (!gate->event) return ZEST_FALSE;
	@autoreleasepool {
		id<MTLCommandBuffer> command_buffer = [native_queue commandBuffer];
		[command_buffer encodeWaitForEvent:gate->event value:1];
		[command_buffer commit];
	}
	return ZEST_TRUE;
}

static zest_bool metal_open_gate_after(MetalGate *gate, zest_microsecs delay) {
	gate->delay = delay;
	gate->thread_started = pthread_create(&gate->thread, NULL, metal_gate_thread, gate) == 0;
	return gate->thread_started;
}

//Opens the gate if it's still closed, so a failed check never leaves a queue held
static void metal_finish_gate(MetalGate *gate) {
	if (gate->thread_started) {
		pthread_join(gate->thread, NULL);
		gate->thread_started = ZEST_FALSE;
	}
	if (gate->event && [gate->event signaledValue] < 1) [gate->event setSignaledValue:1];
	ZEST_MTL_RELEASE(gate->event);
}

//The family that owns the queue, or ZEST_INVALID
static zest_uint metal_queue_family(zest_device device, id<MTLCommandQueue> native_queue) {
	zest_vec_foreach(family_index, device->queue_families) {
		zest_queue_manager manager = device->queue_families[family_index];
		zest_vec_foreach(queue_index, manager->queues) {
			if (ZEST_MTL_BACKEND(queue, &manager->queues[queue_index])->queue == native_queue) return (zest_uint)family_index;
		}
	}
	return ZEST_INVALID;
}

//The queue the batch being recorded will be committed to
static id<MTLCommandQueue> metal_task_queue(const zest_command_list command_list) {
	id<MTLCommandQueue> native_queue = nil;
	@autoreleasepool {
		native_queue = [ZEST_MTL_BACKEND(command_list, command_list)->command_buffer commandQueue];
	}
	return native_queue;
}

//The record that hands the named image between queues, from the release or the acquire side, in a pass on the given queue type
static const zest_mtl_frame_graph_image_barrier_t *metal_crossing_record(zest_frame_graph frame_graph, const char *name, zest_bool acquire, zest_device_queue_type queue_type) {
	for (zest_uint pass_index = 0; pass_index != zest_GetFrameGraphFinalPassCount(frame_graph); ++pass_index) {
		const zest_pass_group_t *pass_group = zest_GetFrameGraphFinalPass(frame_graph, pass_index);
		if (pass_group->compiled_queue_info.queue_type != queue_type) continue;
		zest_mtl_execution_barriers_backend_t *backend = ZEST_MTL_BACKEND(execution_barriers, &pass_group->execution_details.barriers);
		zest_mtl_frame_graph_image_barrier_t *records = acquire ? backend->acquire_images : backend->release_images;
		zest_vec_foreach(record_index, records) {
			if (records[record_index].crosses_queues && strcmp(records[record_index].resource->name, name) == 0) return &records[record_index];
		}
	}
	return NULL;
}

struct MetalCrossQueue {
	zest_compute compute;
	zest_uint image_index;					//The storage image's, for image_write2.comp
	zest_buffer staging_buffer;
	zest_buffer buffer;
	id<MTLCommandQueue> compute_queue;		//The queue each pass's batch was committed to
	id<MTLCommandQueue> transfer_queue;
	id<MTLCommandQueue> render_queue;
};

static void metal_cross_queue_write_task(const zest_command_list command_list, void *user_data) {
	MetalCrossQueue *cross = (MetalCrossQueue *)user_data;
	cross->compute_queue = metal_task_queue(command_list);
	TestPushConstants push = {};
	push.index1 = (int)cross->image_index;
	zest_cmd_BindComputePipeline(command_list, cross->compute);
	zest_cmd_SendPushConstants(command_list, &push, sizeof(push));
	zest_cmd_DispatchCompute(command_list, 2, 2, 1);
}

static void metal_cross_queue_copy_task(const zest_command_list command_list, void *user_data) {
	MetalCrossQueue *cross = (MetalCrossQueue *)user_data;
	cross->transfer_queue = metal_task_queue(command_list);
	zest_cmd_CopyBuffer(command_list, cross->staging_buffer, cross->buffer, sizeof(zest_uint) * METAL_CROSS_WORD_COUNT);
}

static void metal_cross_queue_render_task(const zest_command_list command_list, void *user_data) {
	((MetalCrossQueue *)user_data)->render_queue = metal_task_queue(command_list);
}

//A compute pass and a transfer pass share the first wave, so they run on the compute and transfer queues, and a render pass on the graphics
//queue reads both. A gate holds the compute queue: the transfer batch finishes while it's held, and the render batch only once it opens.
int test_metal_cross_queue_order(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_metal_device(device);
	zest_shader_handle shader = zest_CreateShaderFromFile(device, "examples/SDL2/zest-tests/shaders/glsl/image_write2.comp", "cross_queue_image_write", zest_compute_shader, NULL, ZEST_TRUE);
	zest_compute_handle compute_handle = zest_CreateCompute(device, "Cross Queue Image Write", shader);
	zest_uint words[METAL_CROSS_WORD_COUNT];
	for (zest_uint index = 0; index != METAL_CROSS_WORD_COUNT; ++index) words[index] = index * 7u + 3u;
	MetalCrossQueue cross = {};
	cross.compute = compute_handle.value ? zest_GetCompute(compute_handle) : NULL;
	cross.staging_buffer = zest_CreateStagingBuffer(device, sizeof(words), words);
	zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_gpu_to_cpu);
	cross.buffer = zest_CreateBuffer(device, sizeof(words), &buffer_info);
	zest_image_info_t storage_info = zest_CreateImageInfo(16, 16);
	storage_info.format = zest_format_r8g8b8a8_unorm;
	storage_info.flags = zest_image_preset_storage;
	zest_image_handle storage_handle = zest_CreateImage(device, &storage_info);
	zest_image_handle target_handle = create_draw_target(device);
	zest_image storage_image = zest_GetImage(storage_handle);
	zest_image target = zest_GetImage(target_handle);
	MetalGate gate = {};
	int failed = !cross.compute || !cross.staging_buffer || !cross.buffer || !storage_image || !target;
	zest_frame_graph frame_graph = NULL;
	if (!failed) {
		cross.image_index = zest_AcquireStorageImageIndex(device, storage_image, zest_storage_image_binding);
		memset(zest_BufferData(cross.buffer), 0, sizeof(words));
		//The compute slot takes queue 0 of its family, which the gate holds
		zest_uint compute_slot = context->compute_queue_index;
		context->preferred_queue_index[compute_slot] = 0;
		context->has_preferred_queue[compute_slot] = ZEST_TRUE;
		zest_queue held_queue = &device->queue_families[context->compute_family_index]->queues[0];
		failed |= !metal_close_gate(device, &gate, ZEST_MTL_BACKEND(queue, held_queue)->queue);
		if (!failed && zest_BeginCommandGraph(context, "Cross Queue Order", 0)) {
			zest_resource_node image_node = zest_ImportImageResource("Cross Image", storage_image, 0);
			zest_resource_node buffer_node = zest_ImportBufferResource("Cross Buffer", cross.buffer, 0);
			zest_resource_node target_node = zest_ImportImageResource("Cross Target", target, 0);
			zest_BeginComputePass("Cross Compute");
			zest_ConnectOutput(image_node);
			zest_SetPassTask(metal_cross_queue_write_task, &cross);
			zest_EndPass();
			zest_BeginTransferPass("Cross Transfer");
			zest_ConnectOutput(buffer_node);
			zest_SetPassTask(metal_cross_queue_copy_task, &cross);
			zest_EndPass();
			zest_BeginRenderPass("Cross Render");
			zest_ConnectInput(image_node);
			zest_ConnectInput(buffer_node);
			zest_ConnectOutput(target_node);
			zest_SetPassTask(metal_cross_queue_render_task, &cross);
			zest_EndPass();
			frame_graph = zest_EndFrameGraph();
		}
		failed |= frame_graph == NULL;
	}
	if (frame_graph) {
		zest_uint frame_in_flight = context->current_fif;
		zest_uint transfer_slot = context->transfer_queue_index;
		zest_wave_submission_t *first_wave = zest_vec_size(frame_graph->submissions) ? &frame_graph->submissions[0] : NULL;
		zest_bool wave_crosses = first_wave && first_wave->batches[context->compute_queue_index].magic && first_wave->batches[transfer_slot].magic;
		//The compiler records the crossing when the graph is built, and the flush frees the memory the records are in
		const zest_mtl_frame_graph_image_barrier_t *release = metal_crossing_record(frame_graph, "Cross Image", ZEST_FALSE, zest_queue_compute);
		const zest_mtl_frame_graph_image_barrier_t *acquire = metal_crossing_record(frame_graph, "Cross Image", ZEST_TRUE, zest_queue_graphics);
		zest_image_layout handed_layout = acquire ? acquire->new_layout : zest_image_layout_undefined;
		if (!wave_crosses || !release || !acquire) {
			ZEST_PRINT("\tThe first wave %s a compute and a transfer batch, and the image has %s release and %s acquire record that crosses queues.", wave_crosses ? "holds" : "doesn't hold", release ? "a" : "no", acquire ? "an" : "no");
			failed = 1;
		}
		//The transfer batch signals its wave event once, and the thread looks for that while the compute queue is still held
		zest_mtl_frame_graph_semaphores_backend_t *semaphores_backend = ZEST_MTL_BACKEND(frame_graph_semaphores, frame_graph->semaphores);
		gate.watched_event = semaphores_backend->events[frame_in_flight][transfer_slot];
		gate.watched_value = frame_graph->semaphores->values[frame_in_flight][transfer_slot] + 1;
		zest_microsecs start = zest_Microsecs();
		if (!metal_open_gate_after(&gate, METAL_GATE_DELAY)) {
			//The ended graph still has to be flushed, and with no thread to open the gate it would wait on the held queue for good
			[gate.event setSignaledValue:1];
			failed = 1;
		}
		failed |= zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
		zest_microsecs elapsed = zest_Microsecs() - start;
		zest_bool gate_was_open = [gate.event signaledValue] == 1;
		metal_finish_gate(&gate);
		if (!gate_was_open || elapsed < METAL_GATE_DELAY || !gate.watched_reached) {
			ZEST_PRINT("\tThe flush returned after %llu us with the gate %s, and the transfer batch %s while the compute queue was held.", (zest_ull)elapsed, gate_was_open ? "open" : "still closed", gate.watched_reached ? "had finished" : "hadn't finished");
			failed = 1;
		}
		zest_uint compute_family = metal_queue_family(device, cross.compute_queue);
		zest_uint transfer_family = metal_queue_family(device, cross.transfer_queue);
		zest_uint render_family = metal_queue_family(device, cross.render_queue);
		zest_bool held_queue_ran = cross.compute_queue == ZEST_MTL_BACKEND(queue, &device->queue_families[context->compute_family_index]->queues[0])->queue;
		if (compute_family != context->compute_family_index || transfer_family != context->transfer_family_index || render_family != context->graphics_family_index || !held_queue_ran) {
			ZEST_PRINT("\tThe compute, transfer and render passes ran on queues of families %u, %u and %u, where %u, %u and %u were expected, and the compute pass %s on the held queue.",
				compute_family, transfer_family, render_family, context->compute_family_index, context->transfer_family_index, context->graphics_family_index, held_queue_ran ? "ran" : "didn't run");
			failed = 1;
		}
		//A release to another queue leaves the layout to the acquire
		if (storage_image->layout != handed_layout) {
			ZEST_PRINT("\tThe image's layout is %u after the graph, where the acquire on the graphics queue set %u.", (zest_uint)storage_image->layout, (zest_uint)handed_layout);
			failed = 1;
		}
		failed |= memcmp(zest_BufferData(cross.buffer), words, sizeof(words)) != 0;
		failed |= zest_GetValidationErrorCount(device) != 0;
	}
	metal_finish_gate(&gate);
	if (storage_image) zest_FreeImageNow(storage_handle);
	if (target) zest_FreeImageNow(target_handle);
	if (cross.buffer) zest_FreeBufferNow(cross.buffer);
	if (cross.staging_buffer) zest_FreeBufferNow(cross.staging_buffer);
	if (cross.compute) zest_FreeCompute(compute_handle);
	zest_FreeShader(shader);
	zest_DestroyContext(context);
	return failed | finish_metal_device(device);
}

//Graph A runs unwaited on frame in flight 0 ahead of a gate on the queue it used, and graph B unwaited on frame in flight 1 behind it. The drain
//for frame in flight 0 mustn't wait for B, and the drain for frame in flight 1 mustn't release B's views and indexes before the gate opens.
int test_metal_unwaited_graph_drain(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	//A drain that waits for the held graph gives up after this, so a broken wait fails quickly
	info.max_semaphore_timeout_ms = 2000;
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_metal_device(device);
	zest_context_destruction_queue_t *lists = &context->deferred_resource_freeing_list;
	MetalGate gate = {};
	int acquired_count = 0;
	int failed = 0;
	zest_u64 slot_values[ZEST_QUEUE_COUNT];
	for (zest_uint queue_slot = 0; queue_slot != ZEST_QUEUE_COUNT; ++queue_slot) {
		slot_values[queue_slot] = context->queue_ordering_timelines[queue_slot].current_value;
	}

	zest_SetFrameInFlight(context, 0);
	zest_frame_graph first_graph = tst__build_mip_view_graph(context, "Drain First", &acquired_count);
	failed |= !first_graph || zest_FlushFrameGraph(first_graph) != zest_semaphore_status_success;
	zest_RestoreFrameInFlight(context);
	//A graph with one compute pass may run on the graphics slot, so the gate holds the queue of whichever slot's ordering value moved
	zest_uint used_slot = ZEST_INVALID;
	for (zest_uint queue_slot = 0; queue_slot != ZEST_QUEUE_COUNT; ++queue_slot) {
		if (context->queue_ordering_timelines[queue_slot].current_value != slot_values[queue_slot]) used_slot = queue_slot;
	}
	failed |= used_slot == ZEST_INVALID;
	if (!failed) {
		zest_queue_manager manager = context->queues[used_slot]->queue_manager;
		zest_queue held_queue = &manager->queues[context->preferred_queue_index[used_slot]];
		failed |= !metal_close_gate(device, &gate, ZEST_MTL_BACKEND(queue, held_queue)->queue);
	}
	zest_SetFrameInFlight(context, 1);
	zest_frame_graph second_graph = failed ? NULL : tst__build_mip_view_graph(context, "Drain Second", &acquired_count);
	failed |= !second_graph || zest_FlushFrameGraph(second_graph) != zest_semaphore_status_success;
	zest_RestoreFrameInFlight(context);

	if (!failed) {
		zest_SetFrameInFlight(context, 0);
		zest_microsecs start = zest_Microsecs();
		zest_DrainContextResources(context);
		zest_microsecs first_drain = zest_Microsecs() - start;
		zest_uint first_left = zest_vec_size(lists->transient_view_arrays[0]);
		zest_RestoreFrameInFlight(context);
		if (first_drain >= METAL_GATE_DELAY || first_left) {
			ZEST_PRINT("\tThe drain for frame in flight 0 took %llu us and left %u view arrays while the gate held only frame in flight 1's graph.", (zest_ull)first_drain, first_left);
			failed = 1;
		}

		zest_SetFrameInFlight(context, 1);
		zest_uint second_queued = zest_vec_size(lists->transient_view_arrays[1]);
		start = zest_Microsecs();
		if (!metal_open_gate_after(&gate, METAL_GATE_DELAY)) {
			[gate.event setSignaledValue:1];
			failed = 1;
		}
		zest_DrainContextResources(context);
		zest_microsecs second_drain = zest_Microsecs() - start;
		zest_bool gate_was_open = [gate.event signaledValue] == 1;
		zest_uint second_left = zest_vec_size(lists->transient_view_arrays[1]) + zest_vec_size(lists->transient_binding_indexes[1]);
		zest_RestoreFrameInFlight(context);
		if (second_queued != 1 || !gate_was_open || second_drain < METAL_GATE_DELAY || second_left) {
			ZEST_PRINT("\tThe drain for frame in flight 1 had %u view arrays queued and returned after %llu us with the gate %s, leaving %u entries.", second_queued, (zest_ull)second_drain, gate_was_open ? "open" : "still closed", second_left);
			failed = 1;
		}
	}
	metal_finish_gate(&gate);
	failed |= acquired_count != 2;
	failed |= zest_GetValidationErrorCount(device) != 0;
	zest_DestroyContext(context);
	return failed | finish_metal_device(device);
}

//A gate holds an unwaited graph past the drain's 50 ms timeout. The first drain waits out the timeout, reports and flags a critical error, the
//next only checks, and once the gate opens a drain releases the graph's views.
int test_metal_unwaited_graph_timeout(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	info.max_semaphore_timeout_ms = 50;
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_metal_device(device);
	zest_context_destruction_queue_t *lists = &context->deferred_resource_freeing_list;
	zest_uint fif = context->current_fif;
	MetalGate gate = {};
	int acquired_count = 0;
	zest_u64 slot_values[ZEST_QUEUE_COUNT];
	for (zest_uint queue_slot = 0; queue_slot != ZEST_QUEUE_COUNT; ++queue_slot) {
		slot_values[queue_slot] = context->queue_ordering_timelines[queue_slot].current_value;
	}

	//Waited, so it finds the slot that runs the graph and leaves nothing queued
	zest_frame_graph first_graph = tst__build_mip_view_graph(context, "Timeout First", &acquired_count);
	int failed = !first_graph || zest_FlushFrameGraphAndWait(first_graph) != zest_semaphore_status_success;
	zest_uint used_slot = ZEST_INVALID;
	for (zest_uint queue_slot = 0; queue_slot != ZEST_QUEUE_COUNT; ++queue_slot) {
		if (context->queue_ordering_timelines[queue_slot].current_value != slot_values[queue_slot]) used_slot = queue_slot;
	}
	failed |= used_slot == ZEST_INVALID;
	if (!failed) {
		zest_queue_manager manager = context->queues[used_slot]->queue_manager;
		zest_queue held_queue = &manager->queues[context->preferred_queue_index[used_slot]];
		failed |= !metal_close_gate(device, &gate, ZEST_MTL_BACKEND(queue, held_queue)->queue);
	}
	zest_frame_graph second_graph = failed ? NULL : tst__build_mip_view_graph(context, "Timeout Second", &acquired_count);
	failed |= !second_graph || zest_FlushFrameGraph(second_graph) != zest_semaphore_status_success;

	if (!failed) {
		zest_microsecs start = zest_Microsecs();
		zest_DrainContextResources(context);
		zest_microsecs first_drain = zest_Microsecs() - start;
		zest_bool flagged = ZEST__FLAGGED(context->flags, zest_context_flag_critical_error);
		zest_uint queued_after_first = zest_vec_size(lists->transient_view_arrays[fif]);
		start = zest_Microsecs();
		zest_DrainContextResources(context);
		zest_microsecs second_drain = zest_Microsecs() - start;
		zest_uint queued_after_second = zest_vec_size(lists->transient_view_arrays[fif]);
		int report_count = metal_report_count(device, "didn't finish within");
		if (first_drain < 40000 || first_drain >= METAL_GATE_DELAY || !flagged || queued_after_first != 1) {
			ZEST_PRINT("\tThe first drain took %llu us, %s the critical error, and left %u view arrays, where about 50 ms, flagged and 1 were expected.", (zest_ull)first_drain, flagged ? "flagged" : "didn't flag", queued_after_first);
			failed = 1;
		}
		if (second_drain >= 20000 || queued_after_second != 1 || report_count != 1) {
			ZEST_PRINT("\tThe second drain took %llu us and left %u view arrays, with %i timeout reports, where it should only check, keep 1 and add no report.", (zest_ull)second_drain, queued_after_second, report_count);
			failed = 1;
		}
		[gate.event setSignaledValue:1];
		failed |= zest_WaitForSignal(&context->queue_ordering_timelines[used_slot], ZEST_SECONDS_IN_MICROSECONDS(1)) != zest_semaphore_status_success;
		zest_DrainContextResources(context);
		zest_uint queued_after_open = zest_vec_size(lists->transient_view_arrays[fif]);
		if (queued_after_open || context->unwaited_ordering_values[fif][used_slot]) {
			ZEST_PRINT("\tOnce the graph finished the drain left %u view arrays and the slot's ordering value recorded.", queued_after_open);
			failed = 1;
		}
	}
	metal_finish_gate(&gate);
	failed |= acquired_count != 2;
	failed |= zest_GetValidationErrorCount(device) != 0;
	zest_DestroyContext(context);
	return failed | finish_metal_device(device);
}

struct MetalQueueSwitch {
	zest_image image;
	id<MTLCommandQueue> native_queue;		//The queue the clear's batch was committed to
};

static void metal_queue_switch_task(const zest_command_list command_list, void *user_data) {
	((MetalQueueSwitch *)user_data)->native_queue = metal_task_queue(command_list);
}

//Clears the image in a render pass on the graphics slot. Without wait_for_gpu the flush returns once the batch is committed.
static zest_semaphore_status metal_flush_switch_clear(zest_context context, const char *name, MetalQueueSwitch *clear, float red, float green, zest_bool wait_for_gpu) {
	if (!zest_BeginCommandGraph(context, name, 0)) return zest_semaphore_status_error;
	zest_resource_node node = zest_ImportImageResource("Switch Target", clear->image, 0);
	zest_SetResourceClearColor(node, red, green, 0.f, 1.f);
	zest_BeginRenderPass(name);
	zest_ConnectOutput(node);
	zest_SetPassTask(metal_queue_switch_task, clear);
	zest_EndPass();
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	if (!frame_graph) return zest_semaphore_status_error;
	return wait_for_gpu ? zest_FlushFrameGraphAndWait(frame_graph) : zest_FlushFrameGraph(frame_graph);
}

//Graph A clears on the graphics family's queue 0 behind a gate. The test then holds queue 0, so graph B's slot takes queue 1, and B must not finish
//before the gate opens.
int test_metal_queue_switch_order(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_metal_device(device);
	zest_image_handle first_handle = create_draw_target(device);
	zest_image_handle second_handle = create_draw_target(device);
	MetalQueueSwitch first = { zest_GetImage(first_handle), nil };
	MetalQueueSwitch second = { zest_GetImage(second_handle), nil };
	zest_uint slot = context->graphics_queue_index;
	zest_queue_manager graphics_family = device->queue_families[context->graphics_family_index];
	id<MTLCommandQueue> first_queue = ZEST_MTL_BACKEND(queue, &graphics_family->queues[0])->queue;
	id<MTLCommandQueue> second_queue = ZEST_MTL_BACKEND(queue, &graphics_family->queues[1])->queue;
	MetalGate gate = {};
	zest_queue held_queue = NULL;
	int failed = !first.image || !second.image;
	if (!failed) {
		context->preferred_queue_index[slot] = 0;
		context->has_preferred_queue[slot] = ZEST_TRUE;
		failed |= !metal_close_gate(device, &gate, first_queue);
		failed |= failed || metal_flush_switch_clear(context, "Switch First", &first, 1.f, 0.f, ZEST_FALSE) != zest_semaphore_status_success;
		//A slot moves to another queue only when the one it used last is busy
		held_queue = zest__acquire_manager_queue(graphics_family, 0, ZEST_FALSE);
		failed |= held_queue == NULL || held_queue->index != 0;
		//The reap waits for the first clear once the slot's ordering event passes it, so the flush's timing can't show the order. The second
		//clear's own signal can: it must not be reached while the gate holds the first.
		zest_execution_timeline second_timeline = zest_GetUtilityTimeline(context);
		gate.watched_event = ZEST_MTL_BACKEND(execution_timeline, second_timeline)->event;
		gate.watched_value = second_timeline->current_value + 1;
		failed |= !metal_open_gate_after(&gate, METAL_GATE_DELAY);
		failed |= failed || metal_flush_switch_clear(context, "Switch Second", &second, 0.f, 1.f, ZEST_TRUE) != zest_semaphore_status_success;
		metal_finish_gate(&gate);
		if (held_queue) zest__release_queue(held_queue);
		if (first.native_queue != first_queue || second.native_queue != second_queue) {
			ZEST_PRINT("\tThe first clear ran on %s and the second on %s, where queues 0 and 1 of the graphics family were expected.",
				first.native_queue == first_queue ? "queue 0" : "another queue", second.native_queue == second_queue ? "queue 1" : "another queue");
			failed = 1;
		}
		if (gate.watched_reached) {
			ZEST_PRINT("\tThe second clear finished while the gate still held the first.");
			failed = 1;
		}
		failed |= zest_WaitForSignal(&context->queue_ordering_timelines[slot], ZEST_SECONDS_IN_MICROSECONDS(1)) != zest_semaphore_status_success;
		const int red[4] = { 255, 0, 0, 255 };
		const int green[4] = { 0, 255, 0, 255 };
		zest_uint pixels[256];
		failed |= !zest_CopyImageToBitmap(device, first.image, pixels) || !columns_hold(pixels, 0, 16, red);
		failed |= !zest_CopyImageToBitmap(device, second.image, pixels) || !columns_hold(pixels, 0, 16, green);
		//The first flush can't wait, which a headless context reports once
		failed |= metal_report_count(device, "without a signal timeline") != 1;
		failed |= zest_GetValidationErrorCount(device) != 0;
		context->has_preferred_queue[slot] = ZEST_FALSE;
	}
	metal_finish_gate(&gate);
	if (first.image) zest_FreeImageNow(first_handle);
	if (second.image) zest_FreeImageNow(second_handle);
	zest_DestroyContext(context);
	return failed | finish_metal_device(device);
}

#define METAL_FENCE_ITERATIONS 32
#define METAL_FENCE_GRAPHS 8
#define METAL_FENCE_WORDS 65536
#define METAL_FENCE_RESULT_WORDS 3			//Per iteration: the first and last copied word, then the first written word
#define METAL_FENCE_WRITE_SPIN 64			//Steps each compute thread takes before it writes
#define METAL_FENCE_READ_SPIN 20000			//Steps the reading fragment takes before it reads the written buffer

//The spin keeps the kernel busy long enough for a pass that doesn't wait for it to read the buffer first. The mask is always 0, which the compiler
//can't know, so the store has to come after the spin.
static const char *metal_fence_write_source =
	"#version 450\n"
	"#extension GL_EXT_nonuniform_qualifier : require\n"
	"layout(local_size_x = 64) in;\n"
	"layout(push_constant) uniform push_block { uint buffer_index; uint seed; uint spin; uint mask; } push;\n"
	"layout(std430, set = 0, binding = 5) buffer Words { uint values[]; } buffers[];\n"
	"void main() {\n"
	"	uint value = gl_GlobalInvocationID.x;\n"
	"	for (uint step = 0u; step != push.spin; ++step) value = value * 1664525u + 1013904223u;\n"
	"	buffers[push.buffer_index].values[gl_GlobalInvocationID.x] = push.seed + (value & push.mask);\n"
	"}\n";

//One fragment reads the copy straight away, then spins and reads the written buffer, which the next compute pass overwrites
static const char *metal_fence_read_source =
	"#version 450\n"
	"#extension GL_EXT_nonuniform_qualifier : require\n"
	"layout(push_constant) uniform push_block { uint written_index; uint copied_index; uint results_index; uint iteration; uint word_count; uint spin; uint mask; } push;\n"
	"layout(std430, set = 0, binding = 5) buffer Words { uint values[]; } buffers[];\n"
	"layout(location = 0) out vec4 out_colour;\n"
	"void main() {\n"
	"	if (gl_FragCoord.x < 1.0 && gl_FragCoord.y < 1.0) {\n"
	"		uint first = push.iteration * 3u;\n"
	"		buffers[push.results_index].values[first] = buffers[push.copied_index].values[0];\n"
	"		buffers[push.results_index].values[first + 1u] = buffers[push.copied_index].values[push.word_count - 1u];\n"
	"		uint value = push.iteration;\n"
	"		for (uint step = 0u; step != push.spin; ++step) value = value * 1664525u + 1013904223u;\n"
	"		buffers[push.results_index].values[first + 2u] = buffers[push.written_index].values[value & push.mask];\n"
	"	}\n"
	"	out_colour = vec4(1.0);\n"
	"}\n";

struct MetalFenceOrder;

struct MetalFenceStep {
	MetalFenceOrder *order;
	zest_uint iteration;
};

struct MetalFenceOrder {
	zest_compute compute;
	zest_pipeline_template pipeline_template;
	zest_buffer written;					//Every word holds the iteration's seed after its compute pass
	zest_buffer copied;						//The transfer pass's copy of the written buffer, which the render pass reads
	zest_buffer results;					//METAL_FENCE_RESULT_WORDS per iteration, from the render pass
	zest_uint written_index;
	zest_uint copied_index;
	zest_uint results_index;
	zest_uint seed_base;
	MetalFenceStep steps[METAL_FENCE_ITERATIONS];
	char target_names[METAL_FENCE_ITERATIONS][24];
};

static void metal_fence_write_task(const zest_command_list command_list, void *user_data) {
	MetalFenceStep *step = (MetalFenceStep *)user_data;
	zest_uint push[4] = { step->order->written_index, step->order->seed_base + step->iteration, METAL_FENCE_WRITE_SPIN, 0 };
	zest_cmd_BindComputePipeline(command_list, step->order->compute);
	zest_cmd_SendPushConstants(command_list, push, sizeof(push));
	zest_cmd_DispatchCompute(command_list, METAL_FENCE_WORDS / 64, 1, 1);
}

static void metal_fence_copy_task(const zest_command_list command_list, void *user_data) {
	MetalFenceStep *step = (MetalFenceStep *)user_data;
	zest_cmd_CopyBuffer(command_list, step->order->written, step->order->copied, METAL_FENCE_WORDS * sizeof(zest_uint));
}

static void metal_fence_read_task(const zest_command_list command_list, void *user_data) {
	MetalFenceStep *step = (MetalFenceStep *)user_data;
	zest_viewport_t viewport = zest_CreateViewport(0.f, 0.f, 8.f, 8.f, 0.f, 1.f);
	zest_scissor_rect_t scissor = zest_CreateRect2D(8, 8, 0, 0);
	zest_cmd_ViewPort(command_list, &viewport);
	zest_cmd_Scissor(command_list, &scissor);
	zest_pipeline pipeline = zest_GetPipeline(step->order->pipeline_template, command_list);
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_uint push[7] = { step->order->written_index, step->order->copied_index, step->order->results_index, step->iteration, METAL_FENCE_WORDS, METAL_FENCE_READ_SPIN, 0 };
	zest_cmd_SendPushConstants(command_list, push, sizeof(push));
	zest_cmd_Draw(command_list, 3, 1, 0, 0);
}

//Each iteration writes, copies and reads, and the next one overwrites what the last one read, all in one batch on the graphics queue
static zest_frame_graph metal_build_fence_graph(zest_context context, MetalFenceOrder *order) {
	if (!zest_BeginCommandGraph(context, "Fence Order", 0)) return NULL;
	zest_resource_node written_node = zest_ImportBufferResource("Fence Written", order->written, 0);
	zest_resource_node copied_node = zest_ImportBufferResource("Fence Copied", order->copied, 0);
	zest_image_resource_info_t target_info = { zest_format_r8g8b8a8_unorm };
	target_info.width = 8;
	target_info.height = 8;
	zest_resource_node previous_target = NULL;
	for (zest_uint iteration = 0; iteration != METAL_FENCE_ITERATIONS; ++iteration) {
		//Render passes with the same outputs merge into one, so each draws to a target of its own, which the next iteration's compute pass reads
		zest_resource_node target = zest_AddTransientImageResource(order->target_names[iteration], &target_info);
		zest_FlagResourceAsEssential(target);
		zest_BeginComputePass("Fence Write");
		if (previous_target) zest_ConnectInput(previous_target);
		zest_ConnectOutput(written_node);
		zest_SetPassTask(metal_fence_write_task, &order->steps[iteration]);
		zest_EndPass();
		zest_BeginTransferPass("Fence Copy");
		zest_ConnectInput(written_node);
		zest_ConnectOutput(copied_node);
		zest_SetPassTask(metal_fence_copy_task, &order->steps[iteration]);
		zest_EndPass();
		zest_BeginRenderPass("Fence Read");
		zest_ConnectInput(copied_node);
		zest_ConnectOutput(target);
		zest_SetPassTask(metal_fence_read_task, &order->steps[iteration]);
		zest_EndPass();
		previous_target = target;
	}
	return zest_EndFrameGraph();
}

//Compute, copy and render passes take turns on one queue with nothing but the queue's fence to order them. Metal runs them out of order without
//it, which would leave a result word holding another iteration's seed.
int test_metal_frame_graph_fence_order(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	//A command graph's builder allocator doesn't grow, and 96 passes need more than the default
	info.frame_graph_allocator_size = zloc__MEGABYTE(2);
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_metal_device(device);
	MetalFenceOrder fence_order = {};
	MetalFenceOrder *order = &fence_order;
	zest_shader_handle compute_shader = zest_CreateShader(device, metal_fence_write_source, zest_compute_shader, "fence_write", NULL, ZEST_TRUE);
	zest_shader_handle vertex_shader = zest_CreateShader(device, metal_sample_vertex_source, zest_vertex_shader, "fence_vertex", NULL, ZEST_TRUE);
	zest_shader_handle fragment_shader = zest_CreateShader(device, metal_fence_read_source, zest_fragment_shader, "fence_read", NULL, ZEST_TRUE);
	zest_compute_handle compute_handle = zest_CreateCompute(device, "Fence Write", compute_shader);
	order->compute = compute_handle.value ? zest_GetCompute(compute_handle) : NULL;
	order->pipeline_template = metal_no_input_template(device, "Fence Read", vertex_shader, fragment_shader);
	zest_buffer_info_t device_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	zest_buffer_info_t results_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_to_cpu);
	order->written = zest_CreateBuffer(device, METAL_FENCE_WORDS * sizeof(zest_uint), &device_info);
	order->copied = zest_CreateBuffer(device, METAL_FENCE_WORDS * sizeof(zest_uint), &device_info);
	order->results = zest_CreateBuffer(device, METAL_FENCE_ITERATIONS * METAL_FENCE_RESULT_WORDS * sizeof(zest_uint), &results_info);
	int failed = !order->compute || !order->written || !order->copied || !order->results;
	if (!failed) {
		order->written_index = zest_AcquireStorageBufferIndex(device, order->written);
		order->copied_index = zest_AcquireStorageBufferIndex(device, order->copied);
		order->results_index = zest_AcquireStorageBufferIndex(device, order->results);
		for (zest_uint iteration = 0; iteration != METAL_FENCE_ITERATIONS; ++iteration) {
			order->steps[iteration].order = order;
			order->steps[iteration].iteration = iteration;
			snprintf(order->target_names[iteration], sizeof(order->target_names[iteration]), "Fence Target %u", iteration);
		}
		zest_uint wrong_chains = 0;
		for (zest_uint graph_index = 0; graph_index != METAL_FENCE_GRAPHS && !failed; ++graph_index) {
			order->seed_base = (graph_index + 1) * 1000;
			memset(zest_BufferData(order->results), 0xFF, METAL_FENCE_ITERATIONS * METAL_FENCE_RESULT_WORDS * sizeof(zest_uint));
			zest_frame_graph frame_graph = metal_build_fence_graph(context, order);
			zest_uint most_passes = 0;
			zest_uint batch_count = frame_graph ? metal_graph_batch_count(frame_graph, &most_passes) : 0;
			zest_uint pass_count = frame_graph ? zest_GetFrameGraphFinalPassCount(frame_graph) : 0;
			if (batch_count != 1 || most_passes != METAL_FENCE_ITERATIONS * 3 || pass_count != METAL_FENCE_ITERATIONS * 3) {
				ZEST_PRINT("\tThe fence graph has %u passes in %u batches, at most %u in one, where all %u must be in one batch.", pass_count, batch_count, most_passes, METAL_FENCE_ITERATIONS * 3);
				failed = 1;
			}
			failed |= frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
			const zest_uint *results = (const zest_uint *)zest_BufferData(order->results);
			for (zest_uint iteration = 0; iteration != METAL_FENCE_ITERATIONS; ++iteration) {
				zest_uint seed = order->seed_base + iteration;
				const zest_uint *words = results + iteration * METAL_FENCE_RESULT_WORDS;
				wrong_chains += words[0] != seed || words[1] != seed || words[2] != seed;
			}
		}
		if (wrong_chains) {
			ZEST_PRINT("\tFence Order: %u of %u of the chains read another iteration's seed.", wrong_chains, METAL_FENCE_ITERATIONS * METAL_FENCE_GRAPHS);
			failed = 1;
		}
		failed |= zest_GetValidationErrorCount(device) != 0;
	}
	if (order->written) zest_FreeBufferNow(order->written);
	if (order->copied) zest_FreeBufferNow(order->copied);
	if (order->results) zest_FreeBufferNow(order->results);
	if (order->compute) zest_FreeCompute(compute_handle);
	zest_FreePipelineTemplate(order->pipeline_template);
	zest_FreeShader(compute_shader);
	zest_FreeShader(vertex_shader);
	zest_FreeShader(fragment_shader);
	zest_DestroyContext(context);
	return failed | finish_metal_device(device);
}

//Red for view 0 and green for view 1
static const char *metal_view_colour_fragment_source =
	"#version 450\n"
	"#extension GL_EXT_multiview : enable\n"
	"layout(location = 0) out vec4 out_colour;\n"
	"void main() { out_colour = gl_ViewIndex == 0 ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 1.0, 0.0, 1.0); }\n";

//The view index times 8 in red, so 32 views give 32 distinct bytes
static const char *metal_view_index_fragment_source =
	"#version 450\n"
	"#extension GL_EXT_multiview : enable\n"
	"layout(location = 0) out vec4 out_colour;\n"
	"void main() { out_colour = vec4(float(gl_ViewIndex * 8u) / 255.0, 0.0, 0.0, 1.0); }\n";

//Each instance covers a four pixel wide column of a 16x16 target, at the x its element gives
static const char *metal_view_instance_vertex_source =
	"#version 450\n"
	"#extension GL_EXT_multiview : enable\n"
	"layout(location = 0) in vec2 instance_offset;\n"
	"layout(location = 1) in vec4 instance_colour;\n"
	"layout(location = 0) out vec4 out_colour;\n"
	"layout(location = 1) flat out uint out_instance;\n"
	"void main() {\n"
	"	const vec2 corners[6] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));\n"
	"	vec2 corner = corners[gl_VertexIndex];\n"
	"	gl_Position = vec4(instance_offset.x + corner.x * 0.5, -1.0 + corner.y * 2.0, 0.0, 1.0);\n"
	"	out_colour = vec4(instance_colour.rgb, float(gl_ViewIndex));\n"
	"	out_instance = uint(gl_InstanceIndex);\n"
	"}\n";

//Red from the element, green the instance index and blue the view index, as bytes
static const char *metal_view_instance_fragment_source =
	"#version 450\n"
	"#extension GL_EXT_multiview : enable\n"
	"layout(location = 0) in vec4 in_colour;\n"
	"layout(location = 1) flat in uint in_instance;\n"
	"layout(location = 0) out vec4 out_colour;\n"
	"void main() { out_colour = vec4(in_colour.r, float(in_instance) / 255.0, float(gl_ViewIndex) / 255.0, 1.0); }\n";

//A full screen triangle at a depth set by the view
static const char *metal_view_depth_vertex_source =
	"#version 450\n"
	"#extension GL_EXT_multiview : enable\n"
	"void main() {\n"
	"	vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);\n"
	"	gl_Position = vec4(corner * 2.0 - 1.0, 0.25 + 0.5 * float(gl_ViewIndex), 1.0);\n"
	"}\n";

struct MetalViewInstance {			//32 bytes, as metal_view_instance_vertex_source reads it
	float offset[2];
	float padding[2];
	float colour[4];
};

struct MetalViewDraw {
	zest_pipeline_template pipeline_template;
	zest_buffer instances;					//Bound at binding 1 when set
	zest_buffer indirect;					//Drawn from indirectly when set
	zest_uint vertex_count;
	zest_uint instance_count;
	zest_uint first_instance;
	zest_bool built;						//zest_GetPipeline gave a pipeline
};

//The viewport covers the open pass's attachments, as a headless graph has no swapchain to size it by
static void metal_view_draw_task(const zest_command_list command_list, void *user_data) {
	MetalViewDraw *draw = (MetalViewDraw *)user_data;
	zest_mtl_command_list_backend_t *backend = ZEST_MTL_BACKEND(command_list, command_list);
	zest_viewport_t viewport = zest_CreateViewport(0.f, 0.f, (float)backend->target_width, (float)backend->target_height, 0.f, 1.f);
	zest_scissor_rect_t scissor = zest_CreateRect2D(backend->target_width, backend->target_height, 0, 0);
	zest_cmd_ViewPort(command_list, &viewport);
	zest_cmd_Scissor(command_list, &scissor);
	zest_pipeline pipeline = zest_GetPipeline(draw->pipeline_template, command_list);
	draw->built = pipeline != NULL;
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	if (draw->instances) zest_cmd_BindVertexBuffer(command_list, 1, 1, draw->instances);
	if (draw->indirect) {
		zest_cmd_DrawIndirect(command_list, draw->indirect, 0, 1, sizeof(zest_draw_indirect_command_t));
	} else {
		zest_cmd_Draw(command_list, draw->vertex_count, draw->instance_count, 0, draw->first_instance);
	}
}

//One render pass that draws into every layer of the target, cleared to zero first, and the graph's result
static zest_frame_graph_result run_metal_view_pass(zest_context context, zest_image target, MetalViewDraw *draw) {
	if (!zest_BeginCommandGraph(context, "Multiview", 0)) return zest_fgs_critical_error;
	zest_resource_node node = zest_ImportImageResource("View Target", target, 0);
	zest_SetResourceClearColor(node, 0.f, 0.f, 0.f, 0.f);
	zest_BeginRenderPass("Multiview Pass");
	zest_ConnectOutput(node);
	zest_SetPassTask(metal_view_draw_task, draw);
	zest_EndPass();
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	if (!frame_graph) return zest_fgs_critical_error;
	zest_FlushFrameGraphAndWait(frame_graph);
	return zest_GetFrameGraphResult(frame_graph);
}

static zest_image_handle metal_create_layered_target(zest_device device, zest_format format, zest_image_flags flags, zest_uint size, zest_uint layer_count) {
	zest_image_info_t image_info = zest_CreateImageInfo(size, size);
	image_info.format = format;
	image_info.layer_count = layer_count;
	image_info.flags = flags | zest_image_flag_transfer_src;
	return zest_CreateImage(device, &image_info);
}

static zest_pipeline_template metal_view_instance_template(zest_device device, const char *name, zest_shader_handle vertex_shader, zest_shader_handle fragment_shader, zest_uint view_count) {
	zest_pipeline_template pipeline_template = zest_CreatePipelineTemplate(device, name);
	zest_SetPipelineShaders(pipeline_template, vertex_shader, fragment_shader);
	zest_AddVertexInputBindingDescription(pipeline_template, 1, sizeof(MetalViewInstance), zest_input_rate_instance);
	zest_AddVertexAttribute(pipeline_template, 1, 0, zest_format_r32g32_sfloat, offsetof(MetalViewInstance, offset));
	zest_AddVertexAttribute(pipeline_template, 1, 1, zest_format_r32g32b32a32_sfloat, offsetof(MetalViewInstance, colour));
	zest_SetPipelineCullMode(pipeline_template, zest_cull_mode_none);
	zest_SetPipelineBlend(pipeline_template, zest_BlendStateOpaque());
	if (view_count > 1) zest_SetPipelineViewCount(pipeline_template, view_count);
	return pipeline_template;
}

//Three elements, each a column of its own red. The draws start at element 1, so element 0's column stays clear.
static zest_buffer metal_create_view_instances(zest_device device) {
	zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_cpu_to_gpu);
	zest_buffer buffer = zest_CreateBuffer(device, sizeof(MetalViewInstance) * 3, &buffer_info);
	if (!buffer) return NULL;
	MetalViewInstance instances[3] = {
		{ { -1.0f, 0.f }, { 0.f, 0.f }, { 0.25f, 0.f, 0.f, 1.f } },
		{ { -0.5f, 0.f }, { 0.f, 0.f }, { 0.5f, 0.f, 0.f, 1.f } },
		{ { 0.0f, 0.f }, { 0.f, 0.f }, { 0.75f, 0.f, 0.f, 1.f } },
	};
	memcpy(zest_BufferData(buffer), instances, sizeof(instances));
	return buffer;
}

//Columns 4 to 7 hold element 1 and columns 8 to 11 element 2, drawn as instances 1 and 2 into the given view, and the rest is clear
static zest_bool metal_view_instances_hold(zest_device device, zest_image target, zest_uint layer) {
	zest_uint pixels[256];
	memset(pixels, 0, sizeof(pixels));
	if (!metal_read_texture(device, target, 0, layer, 16, 16, 4, pixels)) return ZEST_FALSE;
	const int clear[4] = { 0, 0, 0, 0 };
	const int first[4] = { 128, 1, (int)layer, 255 };
	const int second[4] = { 191, 2, (int)layer, 255 };
	return columns_hold(pixels, 0, 4, clear) && columns_hold(pixels, 4, 4, first) && columns_hold(pixels, 8, 4, second) && columns_hold(pixels, 12, 4, clear);
}

static zest_bool metal_layer_holds(zest_device device, zest_image target, zest_uint layer, const int expected[4]) {
	zest_uint pixels[256];
	memset(pixels, 0, sizeof(pixels));
	return metal_read_texture(device, target, 0, layer, 16, 16, 4, pixels) && columns_hold(pixels, 0, 16, expected);
}

//Views are drawn as instances into the layers of the pass's attachments: from a vertex shader that never reads gl_ViewIndex, with per-instance
//data from a base instance, into a pass with only depth, and as many views as the device reports
int test_metal_multiview(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_metal_device(device);
	int failed = device->max_multiview_view_count != 32;
	zest_shader_handle plain_vertex = zest_CreateShader(device, metal_sample_vertex_source, zest_vertex_shader, "view_plain_vertex", NULL, ZEST_TRUE);
	zest_shader_handle colour_fragment = zest_CreateShader(device, metal_view_colour_fragment_source, zest_fragment_shader, "view_colour_fragment", NULL, ZEST_TRUE);
	zest_shader_handle index_fragment = zest_CreateShader(device, metal_view_index_fragment_source, zest_fragment_shader, "view_index_fragment", NULL, ZEST_TRUE);
	zest_shader_handle instance_vertex = zest_CreateShader(device, metal_view_instance_vertex_source, zest_vertex_shader, "view_instance_vertex", NULL, ZEST_TRUE);
	zest_shader_handle instance_fragment = zest_CreateShader(device, metal_view_instance_fragment_source, zest_fragment_shader, "view_instance_fragment", NULL, ZEST_TRUE);
	zest_shader_handle depth_vertex = zest_CreateShader(device, metal_view_depth_vertex_source, zest_vertex_shader, "view_depth_vertex", NULL, ZEST_TRUE);
	zest_pipeline_template colour_template = metal_no_input_template(device, "View Colour", plain_vertex, colour_fragment);
	zest_SetPipelineViewCount(colour_template, 2);
	zest_pipeline_template instance_template = metal_view_instance_template(device, "View Instances", instance_vertex, instance_fragment, 2);
	zest_pipeline_template depth_template = zest_CreatePipelineTemplate(device, "View Depth");
	zest_SetPipelineVertShader(depth_template, depth_vertex);
	zest_SetPipelineDisableVertexInput(depth_template);
	zest_SetPipelineDepthTest(depth_template, ZEST_TRUE, ZEST_TRUE);
	depth_template->depth_stencil.depth_compare_op = zest_compare_op_always;
	zest_SetPipelineViewCount(depth_template, 2);
	zest_pipeline_template index_template = metal_no_input_template(device, "View Index", plain_vertex, index_fragment);
	zest_SetPipelineViewCount(index_template, 32);
	zest_image_handle colour_handle = metal_create_layered_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment, 16, 2);
	zest_image_handle depth_handle = metal_create_layered_target(device, zest_format_d32_sfloat, zest_image_preset_depth_attachment, 16, 2);
	zest_image_handle index_handle = metal_create_layered_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment, 4, 32);
	zest_image colour_target = zest_GetImage(colour_handle);
	zest_image depth_target = zest_GetImage(depth_handle);
	zest_image index_target = zest_GetImage(index_handle);
	zest_buffer instances = metal_create_view_instances(device);
	failed |= !colour_target || !depth_target || !index_target || !instances;
	if (!failed) {
		const int red[4] = { 255, 0, 0, 255 };
		const int green[4] = { 0, 255, 0, 255 };
		MetalViewDraw colour_draw = { colour_template, NULL, NULL, 3, 1, 0, ZEST_FALSE };
		failed |= run_metal_view_pass(context, colour_target, &colour_draw) != zest_fgs_success || !colour_draw.built;
		if (!metal_layer_holds(device, colour_target, 0, red) || !metal_layer_holds(device, colour_target, 1, green)) {
			ZEST_PRINT("\tA vertex shader that never reads gl_ViewIndex didn't draw red into layer 0 and green into layer 1.");
			failed = 1;
		}

		MetalViewDraw instance_draw = { instance_template, instances, NULL, 6, 2, 1, ZEST_FALSE };
		failed |= run_metal_view_pass(context, colour_target, &instance_draw) != zest_fgs_success || !instance_draw.built;
		if (!metal_view_instances_hold(device, colour_target, 0) || !metal_view_instances_hold(device, colour_target, 1)) {
			ZEST_PRINT("\tTwo instances from first instance 1 didn't draw elements 1 and 2 into both layers.");
			failed = 1;
		}

		MetalViewDraw depth_draw = { depth_template, NULL, NULL, 3, 1, 0, ZEST_FALSE };
		failed |= run_metal_view_pass(context, depth_target, &depth_draw) != zest_fgs_success || !depth_draw.built;
		for (zest_uint layer = 0; layer != 2; ++layer) {
			float depths[256];
			memset(depths, 0, sizeof(depths));
			failed |= !metal_read_texture(device, depth_target, 0, layer, 16, 16, 4, depths);
			float expected = 0.25f + 0.5f * (float)layer;
			zest_uint matching = 0;
			for (zest_uint index = 0; index != 256; ++index) matching += depths[index] == expected;
			if (matching != 256) {
				ZEST_PRINT("\t%u of 256 depths in layer %u are %f.", matching, layer, expected);
				failed = 1;
			}
		}

		MetalViewDraw index_draw = { index_template, NULL, NULL, 3, 1, 0, ZEST_FALSE };
		failed |= run_metal_view_pass(context, index_target, &index_draw) != zest_fgs_success || !index_draw.built;
		zest_uint right_layers = 0;
		for (zest_uint layer = 0; layer != 32; ++layer) {
			unsigned char pixels[4 * 4 * 4];
			memset(pixels, 0, sizeof(pixels));
			failed |= !metal_read_texture(device, index_target, 0, layer, 4, 4, 4, pixels);
			zest_bool layer_right = ZEST_TRUE;
			for (zest_uint pixel = 0; pixel != 16; ++pixel) layer_right &= pixels[pixel * 4] == layer * 8 && pixels[pixel * 4 + 3] == 255;
			right_layers += layer_right;
		}
		if (right_layers != 32) {
			ZEST_PRINT("\t%u of 32 layers hold their view's index.", right_layers);
			failed = 1;
		}
		failed |= zest_GetValidationErrorCount(device) != 0;
	}
	if (instances) zest_FreeBufferNow(instances);
	if (colour_target) zest_FreeImageNow(colour_handle);
	if (depth_target) zest_FreeImageNow(depth_handle);
	if (index_target) zest_FreeImageNow(index_handle);
	zest_FreePipelineTemplate(colour_template);
	zest_FreePipelineTemplate(instance_template);
	zest_FreePipelineTemplate(depth_template);
	zest_FreePipelineTemplate(index_template);
	zest_FreeShader(plain_vertex);
	zest_FreeShader(colour_fragment);
	zest_FreeShader(index_fragment);
	zest_FreeShader(instance_vertex);
	zest_FreeShader(instance_fragment);
	zest_FreeShader(depth_vertex);
	zest_DestroyContext(context);
	return failed | finish_metal_device(device);
}

//One shader pair, which reads gl_ViewIndex, in a single view template and in a two view template, drawn in the given order on a new context
static int run_metal_view_cache(zest_device device, zest_bool single_view_first) {
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1;
	zest_shader_handle vertex_shader = zest_CreateShader(device, metal_view_instance_vertex_source, zest_vertex_shader, "view_cache_vertex", NULL, ZEST_TRUE);
	zest_shader_handle fragment_shader = zest_CreateShader(device, metal_view_instance_fragment_source, zest_fragment_shader, "view_cache_fragment", NULL, ZEST_TRUE);
	zest_pipeline_template single_template = metal_view_instance_template(device, "View Cache Single", vertex_shader, fragment_shader, 1);
	zest_pipeline_template double_template = metal_view_instance_template(device, "View Cache Double", vertex_shader, fragment_shader, 2);
	zest_image_handle single_handle = metal_create_layered_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment, 16, 1);
	zest_image_handle double_handle = metal_create_layered_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment, 16, 2);
	zest_image single_target = zest_GetImage(single_handle);
	zest_image double_target = zest_GetImage(double_handle);
	zest_buffer instances = metal_create_view_instances(device);
	int failed = !single_target || !double_target || !instances;
	if (!failed) {
		MetalViewDraw single_draw = { single_template, instances, NULL, 6, 2, 1, ZEST_FALSE };
		MetalViewDraw double_draw = { double_template, instances, NULL, 6, 2, 1, ZEST_FALSE };
		for (int draw_index = 0; draw_index != 2; ++draw_index) {
			zest_bool single_now = (draw_index == 0) == single_view_first;
			MetalViewDraw *draw = single_now ? &single_draw : &double_draw;
			failed |= run_metal_view_pass(context, single_now ? single_target : double_target, draw) != zest_fgs_success || !draw->built;
		}
		zest_bool single_right = metal_view_instances_hold(device, single_target, 0);
		zest_bool double_right = metal_view_instances_hold(device, double_target, 0) && metal_view_instances_hold(device, double_target, 1);
		if (!single_right || !double_right) {
			ZEST_PRINT("\tWith the %s template built first, the single view draw is %s and the two view draw is %s.", single_view_first ? "single view" : "two view", single_right ? "right" : "wrong", double_right ? "right" : "wrong");
			failed = 1;
		}
		failed |= zest_GetValidationErrorCount(device) != 0;
	}
	if (instances) zest_FreeBufferNow(instances);
	if (single_target) zest_FreeImageNow(single_handle);
	if (double_target) zest_FreeImageNow(double_handle);
	zest_FreePipelineTemplate(single_template);
	zest_FreePipelineTemplate(double_template);
	zest_FreeShader(vertex_shader);
	zest_FreeShader(fragment_shader);
	zest_DestroyContext(context);
	return failed;
}

struct MetalViewChain {
	MetalViewDraw layered;
	MetalViewDraw single;
	zest_bool cleared;
};

static void metal_view_chain_layered_task(const zest_command_list command_list, void *user_data) {
	metal_view_draw_task(command_list, &((MetalViewChain *)user_data)->layered);
}

static void metal_view_chain_single_task(const zest_command_list command_list, void *user_data) {
	metal_view_draw_task(command_list, &((MetalViewChain *)user_data)->single);
}

static void metal_view_chain_clear_task(const zest_command_list command_list, void *user_data) {
	zest_resource_node target = zest_GetPassOutputResource(command_list, "Chain Clear Target");
	((MetalViewChain *)user_data)->cleared = zest_cmd_ImageClear(command_list, zest_GetResourceImage(target));
}

//What a multiview pass asks of the rest: a pipeline's views must match its pass's, a shader converted for one view count isn't served to the other, a
//layered pass leaves nothing on the context's shared pass descriptor, and an indirect draw is refused
int test_metal_multiview_rules(void) {
	zest_device device = zest_EndDeviceBuilder(BeginMetalTestBuilder());
	if (!device) return 1;
	//The reset empties the function cache, so the second run converts in the other order
	int failed = run_metal_view_cache(device, ZEST_TRUE);
	zest_ResetDevice(device);
	failed |= run_metal_view_cache(device, ZEST_FALSE);

	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_metal_device(device);
	zest_shader_handle plain_vertex = zest_CreateShader(device, metal_sample_vertex_source, zest_vertex_shader, "rules_vertex", NULL, ZEST_TRUE);
	zest_shader_handle colour_fragment = zest_CreateShader(device, metal_view_colour_fragment_source, zest_fragment_shader, "rules_fragment", NULL, ZEST_TRUE);
	zest_pipeline_template double_template = metal_no_input_template(device, "Rules Two Views", plain_vertex, colour_fragment);
	zest_SetPipelineViewCount(double_template, 2);
	zest_pipeline_template single_template = metal_no_input_template(device, "Rules One View", plain_vertex, colour_fragment);
	//A refused build marks its template invalid for good, so the mismatches have templates of their own
	zest_pipeline_template mismatch_templates[2] = {
		metal_no_input_template(device, "Rules Mismatch Two Views", plain_vertex, colour_fragment),
		metal_no_input_template(device, "Rules Mismatch One View", plain_vertex, colour_fragment),
	};
	zest_SetPipelineViewCount(mismatch_templates[0], 2);
	zest_image_handle layered_handle = metal_create_layered_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment, 16, 2);
	zest_image_handle single_handle = metal_create_layered_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment, 16, 1);
	zest_image_handle clear_handle = metal_create_clear_target(device, zest_format_r8g8b8a8_unorm, zest_image_preset_color_attachment);
	zest_image layered_target = zest_GetImage(layered_handle);
	zest_image single_target = zest_GetImage(single_handle);
	zest_image clear_target = zest_GetImage(clear_handle);
	zest_buffer_info_t indirect_info = zest_CreateBufferInfo(zest_buffer_type_indirect, zest_memory_usage_cpu_to_gpu);
	zest_buffer indirect = zest_CreateBuffer(device, sizeof(zest_draw_indirect_command_t), &indirect_info);
	failed |= !layered_target || !single_target || !clear_target || !indirect;
	if (!failed) {
		const int red[4] = { 255, 0, 0, 255 };
		const int green[4] = { 0, 255, 0, 255 };
		const int clear[4] = { 0, 0, 0, 0 };
		zest_ResetValidationErrors(device);
		//Each mismatch is refused once, and the graph still runs
		MetalViewDraw mismatch_draws[2] = { { mismatch_templates[0], NULL, NULL, 3, 1, 0, ZEST_FALSE }, { mismatch_templates[1], NULL, NULL, 3, 1, 0, ZEST_FALSE } };
		zest_image mismatch_targets[2] = { single_target, layered_target };
		for (int mismatch_index = 0; mismatch_index != 2; ++mismatch_index) {
			zest_uint error_count = zest_GetValidationErrorCount(device);
			failed |= run_metal_view_pass(context, mismatch_targets[mismatch_index], &mismatch_draws[mismatch_index]) != zest_fgs_success;
			if (mismatch_draws[mismatch_index].built || zest_GetValidationErrorCount(device) != error_count + 1) {
				ZEST_PRINT("\tThe %s template in a %s pass %s built, with %u validation errors.", mismatch_index ? "one view" : "two view", mismatch_index ? "two layer" : "single layer",
					mismatch_draws[mismatch_index].built ? "was" : "wasn't", zest_GetValidationErrorCount(device) - error_count);
				failed = 1;
			}
		}
		zest_ResetValidationErrors(device);

		//The single layer pass and the clear use the descriptor the layered pass used before them in the same batch. The clear writes zero over a colour.
		failed |= !run_metal_colour_clear(device, clear_target, metal_colour_clear_value(0.6f, 0.4f, 0.2f));
		MetalViewChain chain = {};
		chain.layered = { double_template, NULL, NULL, 3, 1, 0, ZEST_FALSE };
		chain.single = { single_template, NULL, NULL, 3, 1, 0, ZEST_FALSE };
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginCommandGraph(context, "View Chain", 0)) {
			zest_resource_node layered_node = zest_ImportImageResource("Chain Layered Target", layered_target, 0);
			zest_resource_node single_node = zest_ImportImageResource("Chain Single Target", single_target, 0);
			zest_resource_node clear_node = zest_ImportImageResource("Chain Clear Target", clear_target, 0);
			zest_SetResourceClearColor(layered_node, 0.f, 0.f, 0.f, 0.f);
			zest_SetResourceClearColor(single_node, 0.f, 0.f, 0.f, 0.f);
			zest_BeginRenderPass("Chain Layered");
			zest_ConnectOutput(layered_node);
			zest_SetPassTask(metal_view_chain_layered_task, &chain);
			zest_EndPass();
			zest_BeginRenderPass("Chain Single");
			zest_ConnectInput(layered_node);
			zest_ConnectOutput(single_node);
			zest_SetPassTask(metal_view_chain_single_task, &chain);
			zest_EndPass();
			zest_BeginTransferPass("Chain Clear");
			zest_ConnectInput(single_node);
			zest_ConnectOutput(clear_node);
			zest_SetPassTask(metal_view_chain_clear_task, &chain);
			zest_EndPass();
			frame_graph = zest_EndFrameGraph();
		}
		failed |= frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
		zest_bool layered_right = metal_layer_holds(device, layered_target, 0, red) && metal_layer_holds(device, layered_target, 1, green);
		zest_bool single_right = metal_layer_holds(device, single_target, 0, red);
		zest_bool clear_right = chain.cleared && metal_layer_holds(device, clear_target, 0, clear);
		if (!layered_right || !single_right || !clear_right || !chain.layered.built || !chain.single.built) {
			ZEST_PRINT("\tAfter a layered pass, the layered target is %s, the single layer pass is %s and the clear is %s.", layered_right ? "right" : "wrong", single_right ? "right" : "wrong", clear_right ? "right" : "wrong");
			failed = 1;
		}
		failed |= zest_GetValidationErrorCount(device) != 0;

		//An indirect draw can't be multiplied by the view count, so it's refused and draws nothing
		const zest_draw_indirect_command_t command = { 3, 1, 0, 0 };
		memcpy(zest_BufferData(indirect), &command, sizeof(command));
		MetalViewDraw indirect_draw = { double_template, NULL, indirect, 3, 1, 0, ZEST_FALSE };
		failed |= run_metal_view_pass(context, layered_target, &indirect_draw) != zest_fgs_success;
		if (zest_GetValidationErrorCount(device) != 1 || !metal_layer_holds(device, layered_target, 0, clear) || !metal_layer_holds(device, layered_target, 1, clear)) {
			ZEST_PRINT("\tAn indirect draw in a multiview pass raised %u validation errors, where it should be refused once and draw nothing.", zest_GetValidationErrorCount(device));
			failed = 1;
		}
		zest_ResetValidationErrors(device);
	}
	if (indirect) zest_FreeBufferNow(indirect);
	if (layered_target) zest_FreeImageNow(layered_handle);
	if (single_target) zest_FreeImageNow(single_handle);
	if (clear_target) zest_FreeImageNow(clear_handle);
	zest_FreePipelineTemplate(double_template);
	zest_FreePipelineTemplate(single_template);
	zest_FreePipelineTemplate(mismatch_templates[0]);
	zest_FreePipelineTemplate(mismatch_templates[1]);
	zest_FreeShader(plain_vertex);
	zest_FreeShader(colour_fragment);
	zest_DestroyContext(context);
	return failed | finish_metal_device(device);
}

static MetalTest metal_tests[] = {
	{ "Metal Builder", test_metal_builder },
	{ "Metal Device Lifecycle", test_metal_device_lifecycle },
	{ "Metal Minimum Spec Rejected", test_metal_minimum_spec_rejected },
	{ "Metal Validation Unavailable", test_metal_validation_unavailable },
	{ "Metal Timeline Wait", test_metal_timeline_wait },
	{ "Metal Buffer Pool", test_metal_buffer_pool },
	{ "Metal Residency Tracks Pools", test_metal_residency_tracks_pools },
	{ "Metal Image Round Trip", test_metal_image_round_trip },
	{ "Metal Mip Chain", test_metal_mip_chain },
	{ "Metal Format Table", test_metal_format_table },
	{ "Metal Bindless Slots", test_metal_bindless_slots },
	{ "Metal Device With Defaults", test_metal_device_with_defaults },
	{ "Metal Headless Context", test_metal_headless_context },
	{ "Metal Untracked Pass Ordering", test_metal_untracked_pass_ordering },
	{ "Metal Validation Reports", test_metal_validation_reports },
	{ "Metal SDL Helpers", test_metal_sdl_helpers },
	{ "Metal Device Reset", test_metal_device_reset },
	{ "Metal Empty Command Graph", test_metal_empty_command_graph },
	{ "Metal Render Pass Actions", test_metal_render_pass_actions },
	{ "Metal Render Pass Clear", test_metal_render_pass_clear },
	{ "Metal Depth Readback", test_metal_depth_readback },
	{ "Metal Arena Backing", test_metal_arena_backing },
	{ "Metal Transient Images", test_metal_transient_images },
	{ "Metal Frame Graph Buffer Copies", test_metal_frame_graph_buffer_copies },
	{ "Metal Image Mip Commands", test_metal_image_mip_commands },
	{ "Metal Buffer Fills", test_metal_buffer_fills },
	{ "Metal Render Target Usage", test_metal_render_target_usage },
	{ "Metal Clear Colour Image", test_metal_clear_colour_image },
	{ "Metal Clear Depth Image", test_metal_clear_depth_image },
	{ "Metal Image Clear Command", test_metal_image_clear_command },
	{ "Metal Blit Image", test_metal_blit_image },
	{ "Metal Resolve Image", test_metal_resolve_image },
	{ "Metal Per Mip Views", test_metal_per_mip_views },
	{ "Metal Swapchain Formats", test_metal_swapchain_formats },
	{ "Metal Windowed Frames", test_metal_windowed_frames },
	{ "Metal Windowed Context", test_metal_windowed_context },
	{ "Metal Shader Compile", test_metal_shader_compile },
	{ "Metal Shader Conversion", test_metal_shader_conversion },
	{ "Metal Pipeline State Translation", test_metal_pipeline_state_translation },
	{ "Metal Pipeline Cache", test_metal_pipeline_cache },
	{ "Metal Lazy Binding Order", test_metal_lazy_binding_order },
	{ "Metal Pass Encoder Order", test_metal_pass_encoder_order },
	{ "Metal Indirect Draws", test_metal_indirect_draws },
	{ "Metal Immediate Compute", test_metal_immediate_compute },
	{ "Metal Compute Reads Descriptors", test_metal_compute_reads_descriptors },
	{ "Metal Debug Overlay", test_metal_debug_overlay },
	{ "Metal Cross Queue Order", test_metal_cross_queue_order },
	{ "Metal Unwaited Graph Drain", test_metal_unwaited_graph_drain },
	{ "Metal Unwaited Graph Timeout", test_metal_unwaited_graph_timeout },
	{ "Metal Queue Switch Order", test_metal_queue_switch_order },
	{ "Metal Frame Graph Fence Order", test_metal_frame_graph_fence_order },
	{ "Metal Multiview", test_metal_multiview },
	{ "Metal Multiview Rules", test_metal_multiview_rules },
};

int RunMetalSuite(int *total_out) {
	int passed = 0;
	int total = (int)(sizeof(metal_tests) / sizeof(metal_tests[0]));
	for (int index = 0; index != total; ++index) {
		if (metal_tests[index].run() == 0) {
			ZEST_PRINT("\033[32m%s test passed\033[0m", metal_tests[index].name);
			passed++;
		} else {
			ZEST_PRINT("\033[31m%s test failed\033[0m", metal_tests[index].name);
		}
	}
	ZEST_PRINT("%sMetal tests completed: %i / %i\033[0m", passed < total ? "\033[31m" : "\033[32m", passed, total);
	*total_out = total;
	return passed;
}

//A child process gives its counts to the process that started it
void WriteMetalRunCounts(const MetalRunCounts *counts) {
	if (metal_result_descriptor < 0) return;
	dprintf(metal_result_descriptor, "%d %d %d %d\n", counts->suite_passed, counts->suite_total, counts->backend_passed, counts->backend_total);
}

//Metal's validation is decided once per process and MoltenVK isn't clean under it, so when the Vulkan suites run too the Metal suite and
//backend list get a process of their own. The counts come back through a pipe. A run that can't report counts as one failure in each.
MetalRunCounts RunMetalTestsInChildProcess(const char *executable, const char *test_filter) {
	MetalRunCounts counts = { 0, 1, 0, 1 };
	int result_pipe[2];
	if (pipe(result_pipe) != 0) {
		ZEST_PRINT("\033[31mThe Metal tests couldn't be started: no pipe for their result.\033[0m");
		return counts;
	}
	char descriptor_text[16];
	snprintf(descriptor_text, sizeof(descriptor_text), "%d", result_pipe[1]);
	const char *arguments[10];
	int argument_count = 0;
	arguments[argument_count++] = executable;
	arguments[argument_count++] = "--platform";
	arguments[argument_count++] = "metal";
	arguments[argument_count++] = "--result-descriptor";
	arguments[argument_count++] = descriptor_text;
	if (metal_use_shader_validation) {
		arguments[argument_count++] = "--shader-validation";
	}
	if (test_filter) {
		arguments[argument_count++] = "--test";
		arguments[argument_count++] = test_filter;
	}
	arguments[argument_count] = NULL;
	fflush(stdout);
	pid_t child = 0;
	int spawn_result = posix_spawn(&child, executable, NULL, NULL, (char *const *)arguments, environ);
	close(result_pipe[1]);
	if (spawn_result != 0) {
		ZEST_PRINT("\033[31mThe Metal tests couldn't be started: posix_spawn returned %d for %s.\033[0m", spawn_result, executable);
	} else {
		char result_text[64] = { 0 };
		size_t result_length = 0;
		ssize_t read_size = 0;
		while (result_length != sizeof(result_text) - 1 && (read_size = read(result_pipe[0], result_text + result_length, sizeof(result_text) - 1 - result_length)) > 0) {
			result_length += (size_t)read_size;
		}
		int status = 0;
		waitpid(child, &status, 0);
		MetalRunCounts reported = {};
		if (sscanf(result_text, "%d %d %d %d", &reported.suite_passed, &reported.suite_total, &reported.backend_passed, &reported.backend_total) == 4) {
			counts = reported;
		} else {
			ZEST_PRINT("\033[31mThe Metal test process ended without reporting its result (wait status %d).\033[0m", status);
		}
	}
	close(result_pipe[0]);
	return counts;
}
