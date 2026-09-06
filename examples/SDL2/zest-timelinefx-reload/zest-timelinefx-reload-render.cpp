/*
	Stage 2 of the TimelineFX hot reload harness: the same reload contract with zest
	rendering on top.

	The split is the point. Every Vulkan object - device, context, swapchain,
	pipeline, atlas, layer, uniform buffer, storage buffers - is created here in
	platform.exe and never reloads. engine.dll holds nothing but TimelineFX: it
	updates the stage and hands back the instance buffer. If a reload breaks
	something, zest is not a suspect.

	That split forces two things across the boundary in the other direction:

	  - The shape loader and uv lookup have to live here, because they call zest.
	    tfx_LoadEffectLibrary takes both as arguments, so the shape loader is fine.
	    uv_lookup is *stored* on the library, and tfx_SetContext nulls it on every
	    reload on the assumption that its address died with the module. Here it did
	    not - it belongs to platform.exe - but it still has to be handed back through
	    engine_saved_state_t and re-registered. The reload loop rebuilds the GPU
	    shape data every iteration precisely so that a missed re-registration shows
	    up as a checksum mismatch rather than as silently wrong uvs on screen.

	  - Library data the renderer uploads (gpu shape data, particle properties,
	    colour ramp bitmaps) leaves engine.dll as a pointer and a length through
	    engine_library_blob, because every zest call that consumes it is here.

	Usage, from the repository root:
	  zest-timelinefx-reload-render                  50 reloads, then exits
	  --iterations=N --warmup=N --frames-between=N --effect=NAME --library=PATH
	  --windowed-forever   keep rendering after the reload loop, until the window closes
	  --verbose
*/

#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_IMAGES_IMPLEMENTATION
#include <SDL.h>
#include <zest.h>

/* For tfx_push_constants_t and tfx_uniform_buffer_data_t only - the layouts the
   stock timelinefx shaders expect. impl_timelinefx.c itself is deliberately not
   compiled into this target: it calls tfx_* functions, and TimelineFX lives in
   engine.dll. */
#include "impl_timelinefx.h"

#include "tfx_reload_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------------- */
/* Application state. All of this is platform.exe and survives every reload.  */
/* ------------------------------------------------------------------------- */

typedef struct render_app_t {
	harness_t              harness;
	zest_device            device;
	zest_context           context;

	zest_layer_handle      layer;
	zest_pipeline_template pipeline;
	zest_uniform_buffer_handle uniform_buffer;
	zest_sampler_handle    sampler;
	zest_image_collection_t particle_images;
	zest_image_collection_t color_ramps_collection;
	zest_image_handle      particle_texture;
	zest_image_handle      color_ramps_texture;
	zest_buffer            image_data;
	zest_buffer            particle_properties;
	zest_uint              particle_texture_index;
	zest_uint              color_ramps_index;
	zest_uint              image_data_index;
	zest_uint              particle_properties_index;
	zest_uint              sampler_index;
	zest_camera_t          camera;

	int                    verbose;
	int                    frames_rendered;
	int                    frames_skipped;
} render_app_t;

typedef struct render_options_t {
	int         iterations;
	int         warmup_frames;
	int         frames_between;
	const char *library_path;
	const char *effect_name;
	int         windowed_forever;
	int         verbose;
} render_options_t;

/* ------------------------------------------------------------------------- */
/* Shape loading and uv lookup. Both live here because both call zest.        */
/* ------------------------------------------------------------------------- */

static void render_shape_loader(const char *filename, tfx_image_data_t *image_data, void *raw_image_data, int image_memory_size, void *user_data) {
	(void)filename;
	render_app_t *app = (render_app_t *)user_data;

	int width, height, channels;
	stbi_uc *pixels = stbi_load_from_memory((stbi_uc *)raw_image_data, image_memory_size, &width, &height, &channels, 4);
	if (!pixels) return;
	zest_size pixels_size = (zest_size)width * height * 4;

	/* tfx_image_data_t is opaque out here and its accessors are TimelineFX code,
	   so the two things a loader needs go through the engine. */
	engine_image_info_t info;
	app->harness.module.image_info(image_data, &info);

	void *region;
	if (info.frame_count > 1) {
		region = zest_AddImageAtlasAnimationPixels(&app->particle_images, pixels, pixels_size, width, height,
		                                           info.width, info.height, info.frame_count, zest_format_r8g8b8a8_unorm);
	} else {
		region = zest_AddImageAtlasPixels(&app->particle_images, pixels, pixels_size, width, height, zest_format_r8g8b8a8_unorm);
	}
	app->harness.module.image_set_pointer(image_data, region);
	free(pixels);
}

/* tfx_gpu_image_data_t is a public struct, so this one needs nothing from the
   engine. Its address still has to survive the reload - see the header note. */
static void render_uv_lookup(void *ptr, tfx_gpu_image_data_t *image_data, int offset) {
	zest_atlas_region_t *region = (zest_atlas_region_t *)(ptr) + offset;
	image_data->uv.x = region->uv.x;
	image_data->uv.y = region->uv.y;
	image_data->uv.z = region->uv.z;
	image_data->uv.w = region->uv.w;
	image_data->texture_array_index = region->layer_index;
	image_data->uv_packed = region->uv_packed;
}

/* ------------------------------------------------------------------------- */
/* Render resources                                                           */
/* ------------------------------------------------------------------------- */

static void render_update_uniform_buffer(render_app_t *app) {
	zest_uniform_buffer buffer = zest_GetUniformBuffer(app->uniform_buffer);
	tfx_uniform_buffer_data_t *uniform = (tfx_uniform_buffer_data_t *)zest_GetUniformBufferData(buffer);
	uniform->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	uniform->proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf(app->context) / zest_ScreenHeightf(app->context), 0.1f, 10000.f);
	uniform->proj.v[1].y *= -1.f;
	uniform->screen_size.x = zest_ScreenWidthf(app->context);
	uniform->screen_size.y = zest_ScreenHeightf(app->context);
	uniform->millisecs = 0;
	uniform->timer_lerp = 1.f;
	uniform->update_time = 60.f;
}

static int render_init_resources(render_app_t *app) {
	zest_shader_handle frag = zest_CreateShaderFromFile(app->device, "examples/assets/shaders/timelinefx.frag", "tfx_reload_frag.spv", zest_fragment_shader, NULL, true);
	zest_shader_handle vert = zest_CreateShaderFromFile(app->device, "examples/assets/shaders/timelinefx3d.vert", "tfx_reload_vert.spv", zest_vertex_shader, NULL, true);
	if (!zest_IsValidHandle(&frag) || !zest_IsValidHandle(&vert)) {
		fprintf(stderr, "[platform] could not compile the timelinefx shaders\n");
		return 0;
	}

	app->uniform_buffer = zest_CreateUniformBuffer(app->context, "tfx reload uniform", sizeof(tfx_uniform_buffer_data_t));
	app->camera = zest_CreateCamera();
	zest_CameraSetFoV(&app->camera, 60.f);
	zest_CameraUpdateFront(&app->camera);
	render_update_uniform_buffer(app);

	app->pipeline = zest_CreatePipelineTemplate(app->device, "TimelineFX reload pipeline");
	zest_AddVertexInputBindingDescription(app->pipeline, 0, sizeof(tfx_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(app->pipeline, 0, 0, zest_format_r32g32b32a32_sfloat, offsetof(tfx_instance_t, position));
	zest_AddVertexAttribute(app->pipeline, 0, 1, zest_format_r16g16b16a16_snorm, offsetof(tfx_instance_t, quaternion));
	zest_AddVertexAttribute(app->pipeline, 0, 2, zest_format_r16g16_sscaled, offsetof(tfx_instance_t, size));
	zest_AddVertexAttribute(app->pipeline, 0, 3, zest_format_r8g8b8_snorm, offsetof(tfx_instance_t, alignment));
	zest_AddVertexAttribute(app->pipeline, 0, 4, zest_format_r16g16_sscaled, offsetof(tfx_instance_t, intensity_gradient_map));
	zest_AddVertexAttribute(app->pipeline, 0, 5, zest_format_r8g8b8_unorm, offsetof(tfx_instance_t, curved_alpha_life));
	zest_AddVertexAttribute(app->pipeline, 0, 6, zest_format_r32_uint, offsetof(tfx_instance_t, indexes));
	zest_AddVertexAttribute(app->pipeline, 0, 7, zest_format_r32_uint, offsetof(tfx_instance_t, captured_index));
	zest_SetPipelineShaders(app->pipeline, vert, frag);
	zest_SetPipelineDepthTest(app->pipeline, true, false);
	zest_SetPipelineBlend(app->pipeline, zest_PreMultiplyBlendState());

	app->layer = zest_CreateFIFInstanceLayer(app->context, "TimelineFX reload layer", sizeof(tfx_instance_t), 50000);
	zest_AcquireInstanceLayerBufferIndex(app->device, zest_GetLayer(app->layer));

	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	app->sampler = zest_CreateSampler(app->device, &sampler_info);
	app->sampler_index = zest_AcquireSamplerIndex(app->device, zest_GetSampler(app->sampler));

	zest_buffer_info_t storage_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	app->image_data = zest_CreateBuffer(app->device, sizeof(tfx_gpu_image_data_t) * 1000, &storage_info);
	app->image_data_index = zest_AcquireStorageBufferIndex(app->device, app->image_data);
	app->particle_properties = zest_CreateBuffer(app->device, sizeof(tfx_gpu_particle_properties_t) * 1000, &storage_info);
	app->particle_properties_index = zest_AcquireStorageBufferIndex(app->device, app->particle_properties);
	return 1;
}

static void render_upload_blob(render_app_t *app, zest_buffer destination, const void *data, size_t size) {
	if (!data || size == 0) return;
	zest_buffer staging = zest_CreateDedicatedStagingBuffer(app->device, size, (void *)data);
	zest_queue queue = zest_imm_BeginCommandBuffer(app->device, zest_queue_transfer);
	zest_imm_CopyBuffer(queue, staging, destination, size);
	zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(staging);
}

/* Cheap order-sensitive checksum over the gpu shape data, used to prove a rebuild
   produced the same result as the reference. A uv_lookup that was not
   re-registered leaves uv, uv_packed and texture_array_index at zero, which this
   catches immediately.

   Whole bytes, padding included. That only works because tfx__build_gpu_shape_data
   value-initialises each entry (`tfx_gpu_image_data_t cs = {};`); while it did not,
   the unwritten `padding` member and the struct's tail padding carried stack
   garbage into the GPU buffer and two identical rebuilds hashed differently. Keep
   it byte-wise: it now also catches that regressing. */
static unsigned long long shapes_checksum_of(const void *data, size_t size) {
	const unsigned char *bytes = (const unsigned char *)data;
	unsigned long long hash = 1469598103934665603ULL;
	for (size_t i = 0; i != size; ++i) {
		hash ^= bytes[i];
		hash *= 1099511628211ULL;
	}
	return hash;
}

/* Rebuild the library's GPU shape data (which calls back into render_uv_lookup)
   and push it plus the particle properties to the GPU. Returns the checksum of
   the shape data so the caller can compare it against the reference. */
static unsigned long long render_finalise_library(render_app_t *app) {
	app->harness.module.finalise_library();

	engine_blob_t shapes = { 0 };
	unsigned long long shapes_checksum = 0;
	if (app->harness.module.library_blob(engine_blob_gpu_shapes, 0, &shapes)) {
		shapes_checksum = shapes_checksum_of(shapes.data, shapes.size);
		render_upload_blob(app, app->image_data, shapes.data, shapes.size);
	}
	engine_blob_t properties = { 0 };
	if (app->harness.module.library_blob(engine_blob_particle_properties, 0, &properties)) {
		render_upload_blob(app, app->particle_properties, properties.data, properties.size);
	}
	return shapes_checksum;
}

static int render_build_color_ramps(render_app_t *app) {
	engine_blob_t probe = { 0 };
	app->harness.module.library_blob(engine_blob_color_ramp_bitmap, 0, &probe);
	if (probe.available <= 0) {
		fprintf(stderr, "[platform] the library reported no colour ramps\n");
		return 0;
	}
	app->color_ramps_collection = zest_CreateImageAtlasCollection(zest_format_r16g16b16a16_sfloat, probe.available);
	for (int i = 0; i != probe.available; ++i) {
		engine_blob_t bitmap = { 0 };
		if (!app->harness.module.library_blob(engine_blob_color_ramp_bitmap, i, &bitmap)) continue;
		zest_AddImageAtlasPixels(&app->color_ramps_collection, (void *)bitmap.data, bitmap.size,
		                         bitmap.width, bitmap.height, zest_format_r16g16b16a16_sfloat);
	}
	app->color_ramps_texture = zest_CreateImageAtlas(app->context, &app->color_ramps_collection, 256, 256, zest_image_preset_texture);
	app->color_ramps_index = zest_AcquireSampledImageIndex(app->device, zest_GetImage(app->color_ramps_texture), zest_texture_array_binding);
	return 1;
}

/* ------------------------------------------------------------------------- */
/* Frame graph                                                                */
/* ------------------------------------------------------------------------- */

static void render_draw_particle_layer(const zest_command_list command_list, void *user_data) {
	render_app_t *app = (render_app_t *)user_data;
	zest_layer layer = zest_GetLayer(app->layer);

	zest_cmd_BindVertexBuffer(command_list, 0, 1, zest_GetLayerVertexBuffer(layer));

	zest_pipeline current_pipeline = 0;
	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(app->uniform_buffer);

	zest_layer_instruction_t *current = zest_NextLayerInstruction(layer);
	while (current) {
		zest_cmd_LayerViewport(command_list, layer);

		zest_pipeline pipeline = zest_GetPipeline(current->pipeline_template, command_list);
		if (pipeline && current_pipeline != pipeline) {
			current_pipeline = pipeline;
			zest_cmd_BindPipeline(command_list, pipeline);
		} else if (!pipeline) {
			current = zest_NextLayerInstruction(layer);
			continue;
		}

		tfx_push_constants_t *push_constants = (tfx_push_constants_t *)current->push_constant;
		push_constants->color_ramp_texture_index    = app->color_ramps_index;
		push_constants->particle_texture_index      = app->particle_texture_index;
		push_constants->sampler_index               = app->sampler_index;
		push_constants->image_data_index            = app->image_data_index;
		push_constants->particle_properties_index   = app->particle_properties_index;
		push_constants->prev_billboards_index       = zest_GetLayerVertexDescriptorIndex(layer, true);
		push_constants->uniform_index               = zest_GetUniformBufferDescriptorIndex(uniform_buffer);

		zest_cmd_SendPushConstants(command_list, push_constants, sizeof(tfx_push_constants_t));
		zest_cmd_DrawLayerInstruction(command_list, 6, current);

		current = zest_NextLayerInstruction(layer);
	}
}

/* One rendered frame: read the instance buffer out of engine.dll, put it in the
   layer, and run the graph. Returns 1 when a frame was actually presented. */
static int render_frame(render_app_t *app) {
	zest_UpdateDevice(app->device);
	if (!zest_BeginFrame(app->context)) {
		app->frames_skipped++;
		return 0;
	}

	zest_layer layer = zest_GetLayer(app->layer);
	engine_render_data_t data;
	app->harness.module.render_data(&data);

	zest_ResetInstanceLayer(layer);
	zest_StartInstanceDrawing(layer, app->pipeline);
	if (data.instance_count > 0) {
		zest_DrawInstanceBuffer(layer, (void *)data.instances, data.instance_count);
	}

	render_update_uniform_buffer(app);
	zest_SetSwapchainClearColor(app->context, 0.f, .1f, .2f, 1.f);

	int has_particles = zest_GetLayerInstanceSize(layer) > 0;
	zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(app->context, &has_particles, sizeof(has_particles));
	zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);
	if (!frame_graph) {
		if (zest_BeginFrameGraph(app->context, "TimelineFX Reload Render", &cache_key)) {
			zest_resource_node write_layer = zest_AddTransientLayerResource("Write particle buffer", layer, false);
			zest_resource_node read_layer  = zest_AddTransientLayerResource("Read particle buffer", layer, true);
			zest_ImportSwapchainResource();

			zest_BeginTransferPass("Upload TFX Pass"); {
				zest_ConnectOutput(write_layer);
				zest_ConnectOutput(read_layer);
				zest_SetPassTask(zest_UploadInstanceLayerData, layer);
				zest_EndPass();
			}
			zest_BeginRenderPass("Graphics Pass"); {
				zest_ConnectInput(write_layer);
				zest_ConnectInput(read_layer);
				zest_ConnectSwapChainOutput();
				zest_SetPassTask(render_draw_particle_layer, app);
				zest_EndPass();
			}
			frame_graph = zest_EndFrameGraph();
		}
	}
	zest_EndFrame(app->context, frame_graph);
	app->frames_rendered++;
	return 1;
}

static int poll_sdl_events(zest_context context) {
	SDL_Event event;
	int running = 1;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) running = 0;
		if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
		    event.window.windowID == SDL_GetWindowID((SDL_Window *)zest_Window(context))) {
			running = 0;
		}
	}
	return running;
}

/* ------------------------------------------------------------------------- */
/* Options                                                                    */
/* ------------------------------------------------------------------------- */

static int option_int(const char *arg, const char *name, int *out) {
	size_t len = strlen(name);
	if (strncmp(arg, name, len) == 0 && arg[len] == '=') { *out = atoi(arg + len + 1); return 1; }
	return 0;
}

static int option_string(const char *arg, const char *name, const char **out) {
	size_t len = strlen(name);
	if (strncmp(arg, name, len) == 0 && arg[len] == '=') { *out = arg + len + 1; return 1; }
	return 0;
}

static void options_parse(render_options_t *options, int argc, char **argv) {
	options->iterations       = 50;
	options->warmup_frames    = 300;
	options->frames_between   = 30;
	options->library_path     = "examples/assets/vaders/vadereffects.tfx";
	options->effect_name      = "Background";
	options->windowed_forever = 0;
	options->verbose          = 0;
	for (int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		if (option_int(arg, "--iterations", &options->iterations)) continue;
		if (option_int(arg, "--warmup", &options->warmup_frames)) continue;
		if (option_int(arg, "--frames-between", &options->frames_between)) continue;
		if (option_string(arg, "--library", &options->library_path)) continue;
		if (option_string(arg, "--effect", &options->effect_name)) continue;
		if (strcmp(arg, "--windowed-forever") == 0) { options->windowed_forever = 1; continue; }
		if (strcmp(arg, "--verbose") == 0) { options->verbose = 1; continue; }
		fprintf(stderr, "[platform] unknown option %s\n", arg);
	}
}

/* ------------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, 0);
	platform_install_crash_reporting();

	render_options_t options;
	options_parse(&options, argc, argv);

	static render_app_t app;
	app.verbose = options.verbose;

	/* zest first, and entirely here. Nothing below this line goes into engine.dll. */
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "TimelineFX Hot Reload (stage 2)");
	app.device = zest_implsdl2_CreateVulkanDevice(&window_data, 0);
	app.context = zest_CreateContext(app.device, &window_data, &create_info);
	if (!app.device || !app.context) {
		fprintf(stderr, "[platform] could not create the zest device or context\n");
		return HARNESS_FAIL;
	}
	if (!render_init_resources(&app)) return HARNESS_FAIL;

	/* Then TimelineFX, entirely over there. */
	if (!harness_start(&app.harness, (size_t)128 * 1024 * 1024, options.library_path, options.verbose)) {
		return HARNESS_FAIL;
	}
	int shape_count = app.harness.module.shape_count_in_file(options.library_path);
	app.particle_images = zest_CreateImageAtlasCollection(zest_format_r8g8b8a8_unorm, shape_count);
	unsigned int library_error_flags = 0;
	if (!app.harness.module.load_library(options.library_path, render_shape_loader, render_uv_lookup, &app, &library_error_flags)) {
		fprintf(stderr, "[platform] could not load %s\n", options.library_path);
		return HARNESS_FAIL;
	}
	app.particle_texture = zest_CreateImageAtlas(app.context, &app.particle_images, 1024, 1024, 0);
	app.particle_texture_index = zest_AcquireSampledImageIndex(app.device, zest_GetImage(app.particle_texture), zest_texture_array_binding);

	if (!app.harness.module.start_effect(options.effect_name, TFX_RELOAD_SEED, &app.harness.saved)) {
		fprintf(stderr, "[platform] no effect named '%s' in %s\n", options.effect_name, options.library_path);
		return HARNESS_FAIL;
	}
	if (!render_build_color_ramps(&app)) return HARNESS_FAIL;
	unsigned long long reference_shapes = render_finalise_library(&app);
	/* Rebuilding with no reload in between has to produce the same checksum, or the
	   comparison in the loop below would be measuring the rebuild rather than the
	   reload. This is the premise of that assertion, so it is asserted too. */
	unsigned long long rebuilt_shapes = render_finalise_library(&app);

	printf("TimelineFX hot reload harness (stage 2, rendered)\n");
	printf("  engine dll     : %s\n", app.harness.module.source_path);
	printf("  library        : %s (%d shapes)\n", options.library_path, shape_count);
	printf("  effect         : %s\n", options.effect_name);
	printf("  pool           : 128 MB of platform memory at %p\n", (void *)app.harness.pool.base);
	printf("  reloads        : %d, %d rendered frames between each\n\n", options.iterations, options.frames_between);
	printf("  every Vulkan object lives in this process and never reloads;\n");
	printf("  engine.dll holds TimelineFX and nothing else.\n\n");

	int running = 1;
	for (int frame = 0; frame != options.warmup_frames && running; ++frame) {
		app.harness.module.update(TFX_RELOAD_FIXED_TIMESTEP);
		render_frame(&app);
		running = poll_sdl_events(app.context);
	}

	engine_sample_t warm;
	app.harness.module.sample(&warm);
	printf("Baseline after %d rendered warmup frames\n", options.warmup_frames);
	printf("  particles      : %u\n", warm.particle_count);
	printf("  instances      : %d\n", warm.instance_count);
	printf("  worker threads : %d\n", warm.thread_count);
	printf("  frames drawn   : %d (%d skipped, e.g. resize)\n\n", app.frames_rendered, app.frames_skipped);

	printf("Preconditions\n");
	platform_check(warm.particle_count > 0, "population is alive before any reload", "%u particles", warm.particle_count);
	platform_check(app.frames_rendered > 0, "the renderer is actually drawing", "%d frames presented", app.frames_rendered);
	platform_check(warm.context_in_pool, "tfx context lives in platform memory", "inside the VirtualAlloc block");
	platform_check(reference_shapes != 0, "gpu shape data was built", "checksum %016llX", reference_shapes);
	platform_check(rebuilt_shapes == reference_shapes, "rebuilding it with no reload is deterministic",
	               "%016llX vs %016llX", reference_shapes, rebuilt_shapes);

	printf("\nReloading %d times while rendering\n\n", options.iterations);

	int    reload_failures     = 0;
	int    uv_mismatches       = 0;
	int    unloads_refused     = 0;
	int    stale_globals       = 0;
	int    zero_population     = 0;
	int    extra_pools         = 0;
	tfxU32 callback_previous   = warm.callback_hits;
	int    callback_stalls     = 0;
	int    frames_at_start     = app.frames_rendered;
	int    early_blocks = 0, late_blocks = 0;

	for (int iteration = 0; iteration != options.iterations && running; ++iteration) {
		engine_sample_t pre;
		app.harness.module.sample(&pre);
		if (pre.particle_count == 0) zero_population++;

		if (!harness_reload(&app.harness, ENGINE_DETACH_DEFAULT, ENGINE_REATTACH_DEFAULT)) {
			fprintf(stderr, "[platform] reload %d failed\n", iteration);
			return HARNESS_FAIL;
		}

		/* Rebuild the gpu shape data through the re-registered uv_lookup. This is the
		   assertion the headless harness cannot make: if tfx_SetContext nulled
		   library->uv_lookup and nobody put it back, the uvs come back wrong here
		   rather than silently wrong on screen a hundred frames later. */
		unsigned long long shapes_now = render_finalise_library(&app);
		if (shapes_now != reference_shapes) uv_mismatches++;

		app.harness.module.update(TFX_RELOAD_FIXED_TIMESTEP);
		render_frame(&app);

		engine_sample_t post;
		app.harness.module.sample(&post);

		int ok = 1;
		if (post.particle_count == 0)                   ok = 0;
		if (!post.context_in_pool)                      ok = 0;
		if (post.callback_hits <= callback_previous)    { ok = 0; callback_stalls++; }
		if (app.harness.module.unload_left_it_mapped)   { ok = 0; unloads_refused++; }
		if (!post.globals_were_zeroed)                  { ok = 0; stale_globals++; }
		if (post.pool_count != 1)                       extra_pools++;
		if (!ok) reload_failures++;
		callback_previous = post.callback_hits;

		if (iteration == options.iterations / 4)     early_blocks = post.used_blocks;
		if (iteration == options.iterations - 1)     late_blocks  = post.used_blocks;

		if (options.verbose || !ok) {
			printf("  iteration %2d: %s particles %u -> %u  blocks %d  frames %d  gen %d\n",
			       iteration, ok ? "ok  " : "FAIL", pre.particle_count, post.particle_count,
			       post.used_blocks, app.frames_rendered, app.harness.module.generation - 1);
		}

		for (int frame = 0; frame != options.frames_between && running; ++frame) {
			app.harness.module.update(TFX_RELOAD_FIXED_TIMESTEP);
			render_frame(&app);
			running = poll_sdl_events(app.context);
		}
	}

	engine_sample_t final_sample;
	app.harness.module.sample(&final_sample);

	printf("\nResults over %d reloads\n", options.iterations);
	platform_check(unloads_refused == 0, "engine.dll really was unmapped every time",
	               "%d unloads left the image mapped", unloads_refused);
	platform_check(stale_globals == 0, "engine.dll came back with a zeroed data segment",
	               "%d reattaches saw the previous generation's globals", stale_globals);
	platform_check(reload_failures == 0, "every reload preserved particle state",
	               "%d of %d iterations failed", reload_failures, options.iterations);
	platform_check(zero_population == 0, "population never empty at detach", "%d empty detaches", zero_population);
	platform_check(uv_mismatches == 0, "gpu shape data rebuilt identically after every reload",
	               "%d checksum mismatches (uv_lookup re-registration)", uv_mismatches);
	platform_check(callback_stalls == 0, "re-registered update callback kept firing", "%d stalls", callback_stalls);
	platform_check(app.frames_rendered > frames_at_start, "rendering continued across the reloads",
	               "%d frames presented in total", app.frames_rendered);
	platform_check(extra_pools == 0, "TimelineFX never grew past its first pool",
	               "%d iterations reported extra pools", extra_pools);
	platform_check(late_blocks <= early_blocks + 2, "pool block count is flat across reloads",
	               "%d blocks early -> %d late", early_blocks, late_blocks);

	if (options.windowed_forever) {
		printf("\nRendering until the window is closed.\n");
		while (running) {
			app.harness.module.update(TFX_RELOAD_FIXED_TIMESTEP);
			render_frame(&app);
			running = poll_sdl_events(app.context);
		}
	}

	char shutdown_text[8192];
	int captured = harness_shutdown_and_capture(&app.harness, shutdown_text, sizeof(shutdown_text));
	int reported_success = captured && strstr(shutdown_text, "Successful shutdown of TimelineFX") != NULL;
	int reported_leak    = captured && strstr(shutdown_text, "memory leak") != NULL;

	printf("\nShutdown\n");
	platform_check(reported_success, "TimelineFX reported a clean shutdown", "%s",
	               reported_success ? "\"Successful shutdown of TimelineFX.\"" : "success message absent");
	platform_check(!reported_leak, "TimelineFX reported no leaked blocks", "%s",
	               reported_leak ? "leak report present" : "no leak report");
	platform_check(app.harness.pool.live_allocations == 0, "every platform pool block was returned",
	               "%d still outstanding of %d total", app.harness.pool.live_allocations, app.harness.pool.total_allocations);
	if (captured && (!reported_success || reported_leak)) {
		printf("\n--- engine shutdown output ---\n%s\n------------------------------\n", shutdown_text);
	}

	module_unload(&app.harness.module);
	RemoveDirectoryA(app.harness.module.scratch_dir);
	zest_DestroyDevice(app.device);
	platform_pool_destroy(&app.harness.pool);

	printf("\n%s (%d failed check%s)\n", platform_failures ? "FAILED" : "PASSED",
	       platform_failures, platform_failures == 1 ? "" : "s");
	return platform_failures ? HARNESS_FAIL : HARNESS_PASS;
}
