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
	zest_mtl_descriptor_pool_backend_t *pool_backend = ZEST_MTL_BACKEND(descriptor_set, device->bindless_set)->pool;
	id<MTLBuffer> buffers[zest_max_global_binding_number];
	NSUInteger offsets[zest_max_global_binding_number];
	for (zest_uint binding_number = 0; binding_number != zest_max_global_binding_number; ++binding_number) {
		buffers[binding_number] = pool_backend->table;
		offsets[binding_number] = (NSUInteger)pool_backend->regions.offsets[binding_number];
	}
	[encoder setBuffers:buffers offsets:offsets withRange:NSMakeRange(ZEST__MTL_BINDLESS_BUFFER_INDEX, zest_max_global_binding_number)];
}

//Reads one slice of one mip of an image back through a blit on a Zest queue
static zest_bool metal_read_texture(zest_device device, zest_image image, zest_uint mip, zest_uint slice, zest_uint width, zest_uint height, zest_uint bytes_per_pixel, void *out_pixels) {
	zest_size size = (zest_size)width * height * bytes_per_pixel;
	id<MTLBuffer> readback = [zest_GetMetalDevice(device) newBufferWithLength:(NSUInteger)size options:MTLResourceStorageModeShared];
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	if (!queue) {
		[readback release];
		return ZEST_FALSE;
	}
	[zest__mtl_queue_blit_encoder(queue) copyFromTexture:ZEST_MTL_BACKEND(image, image)->texture sourceSlice:slice sourceLevel:mip sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(width, height, 1)
		toBuffer:readback destinationOffset:0 destinationBytesPerRow:width * bytes_per_pixel destinationBytesPerImage:(NSUInteger)size];
	zest_bool finished = zest_imm_EndCommandBuffer(queue);
	if (finished) {
		memcpy(out_pixels, [readback contents], (size_t)size);
	}
	[readback release];
	return finished;
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
		if (context) zest_DestroyContext(context);
		allocated_sizes[cycle] = (zest_size)[zest_GetMetalDevice(device) currentAllocatedSize];
	}
	if (allocated_sizes[2] != allocated_sizes[1]) {
		ZEST_PRINT("Metal's allocated size went from %llu to %llu between the second and third headless context.", (zest_ull)allocated_sizes[1], (zest_ull)allocated_sizes[2]);
		failed = 1;
	}
	return failed | finish_metal_device(device);
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
	if (metal_result_descriptor >= 0) {
		dprintf(metal_result_descriptor, "%d %d\n", passed, total);
	}
	*total_out = total;
	return passed;
}

//Metal's validation is decided once per process and MoltenVK isn't clean under it, so when the Vulkan suites run too this list gets a process
//of its own. The counts come back through a pipe.
int RunMetalSuiteInChildProcess(const char *executable, int *total_out) {
	*total_out = 1;
	int result_pipe[2];
	if (pipe(result_pipe) != 0) {
		ZEST_PRINT("\033[31mThe Metal tests couldn't be started: no pipe for their result.\033[0m");
		return 0;
	}
	char descriptor_text[16];
	snprintf(descriptor_text, sizeof(descriptor_text), "%d", result_pipe[1]);
	const char *arguments[8];
	int argument_count = 0;
	arguments[argument_count++] = executable;
	arguments[argument_count++] = "--platform";
	arguments[argument_count++] = "metal";
	arguments[argument_count++] = "--result-descriptor";
	arguments[argument_count++] = descriptor_text;
	if (metal_use_shader_validation) {
		arguments[argument_count++] = "--shader-validation";
	}
	arguments[argument_count] = NULL;
	fflush(stdout);
	pid_t child = 0;
	int spawn_result = posix_spawn(&child, executable, NULL, NULL, (char *const *)arguments, environ);
	close(result_pipe[1]);
	int passed = 0;
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
		int total = 0;
		if (sscanf(result_text, "%d %d", &passed, &total) == 2) {
			*total_out = total;
		} else {
			passed = 0;
			ZEST_PRINT("\033[31mThe Metal test process ended without reporting its result (wait status %d).\033[0m", status);
		}
	}
	close(result_pipe[0]);
	return passed;
}
