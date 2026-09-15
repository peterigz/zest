#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <SDL.h>
#include <zest.h>
#include <math.h>

/**
	Demonstrates zest_image_info_t::swizzle. The same single channel r8_unorm mask is uploaded
	twice, once with an identity component mapping and once with zest_SwizzleAlphaOnly(). The
	fragment shader is the same for both quads, so the only thing producing the difference on
	screen is the component mapping baked into the image view.

	Left quad  - identity. Red comes through as red and alpha reads as 1, so the quad is an
	             opaque red disc on a hard edged square.
	Right quad - alpha only. rgb reads as 1 and alpha reads from red, so the tint colour comes
	             through and the disc blends softly into the background.
 */

#define MASK_SIZE 256

struct swizzle_push_t {
	float rect[4];		//x, y, width, height in normalised device coordinates
	float tint[4];
	zest_uint sampler_index;
	zest_uint image_index;
};

struct swizzle_app_t {
	zest_device device;
	zest_context context;
	zest_pipeline_template quad_pipeline;
	zest_image_handle identity_mask;
	zest_image_handle swizzled_mask;
	zest_sampler_handle sampler;
	swizzle_push_t identity_push;
	swizzle_push_t swizzled_push;
};

//A soft edged disc, one byte per pixel, which is the shape r8_unorm masks are normally used for.
void BuildMask(unsigned char *pixels) {
	const float centre = (float)MASK_SIZE * 0.5f;
	for (int y = 0; y != MASK_SIZE; ++y) {
		for (int x = 0; x != MASK_SIZE; ++x) {
			float dx = ((float)x + 0.5f - centre) / centre;
			float dy = ((float)y + 0.5f - centre) / centre;
			float distance = sqrtf(dx * dx + dy * dy);
			float value = 1.f - distance;
			value = value < 0.f ? 0.f : (value > 1.f ? 1.f : value);
			pixels[y * MASK_SIZE + x] = (unsigned char)(value * value * 255.f);
		}
	}
}

void InitApp(swizzle_app_t *app) {
	zest_shader_handle vert = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-image-swizzle/shaders/quad.vert", "swizzle_quad_vert", zest_vertex_shader, NULL, ZEST_TRUE);
	zest_shader_handle frag = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-image-swizzle/shaders/quad.frag", "swizzle_quad_frag", zest_fragment_shader, NULL, ZEST_TRUE);

	app->quad_pipeline = zest_CreatePipelineTemplate(app->device, "Swizzle Quad Pipeline");
	zest_SetPipelineShaders(app->quad_pipeline, vert, frag);
	zest_SetPipelineBlend(app->quad_pipeline, zest_AlphaBlendState());
	zest_SetPipelineTopology(app->quad_pipeline, zest_topology_triangle_strip);
	//The quad corners come from gl_VertexIndex, so there is no vertex input state
	zest_SetPipelineDisableVertexInput(app->quad_pipeline);

	unsigned char *pixels = (unsigned char *)malloc(MASK_SIZE * MASK_SIZE);
	BuildMask(pixels);

	zest_image_info_t info = zest_CreateImageInfo(MASK_SIZE, MASK_SIZE);
	info.format = zest_format_r8_unorm;
	info.flags = zest_image_preset_texture;
	app->identity_mask = zest_CreateImageWithPixels(app->device, pixels, MASK_SIZE * MASK_SIZE, &info);

	//A non identity swizzle is only legal because this image is sampled only. Adding a colour
	//attachment or storage flag here would trip the assert in zest_CreateImage.
	info.swizzle = zest_SwizzleAlphaOnly();
	app->swizzled_mask = zest_CreateImageWithPixels(app->device, pixels, MASK_SIZE * MASK_SIZE, &info);

	free(pixels);

	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	app->sampler = zest_CreateSampler(app->device, &sampler_info);
	zest_uint sampler_index = zest_AcquireSamplerIndex(app->device, zest_GetSampler(app->sampler));

	app->identity_push.sampler_index = sampler_index;
	app->identity_push.image_index = zest_AcquireSampledImageIndex(app->device, zest_GetImage(app->identity_mask), zest_texture_2d_binding);
	app->identity_push.rect[0] = -0.9f; app->identity_push.rect[1] = -0.6f;
	app->identity_push.rect[2] =  0.8f; app->identity_push.rect[3] =  1.2f;
	app->identity_push.tint[0] = 1.f; app->identity_push.tint[1] = 1.f;
	app->identity_push.tint[2] = 1.f; app->identity_push.tint[3] = 1.f;

	app->swizzled_push = app->identity_push;
	app->swizzled_push.image_index = zest_AcquireSampledImageIndex(app->device, zest_GetImage(app->swizzled_mask), zest_texture_2d_binding);
	app->swizzled_push.rect[0] = 0.1f;
	//A tint is only visible on the swizzled quad, where rgb reads as 1
	app->swizzled_push.tint[0] = 1.f; app->swizzled_push.tint[1] = 0.6f;
	app->swizzled_push.tint[2] = 0.1f; app->swizzled_push.tint[3] = 1.f;

	zest_SetSwapchainClearColor(app->context, 0.1f, 0.15f, 0.3f, 1.f);
}

void DrawQuads(const zest_command_list command_list, void *user_data) {
	swizzle_app_t *app = (swizzle_app_t *)user_data;

	zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);

	zest_pipeline pipeline = zest_GetPipeline(app->quad_pipeline, command_list);
	zest_cmd_BindPipeline(command_list, pipeline);

	zest_cmd_SendPushConstants(command_list, &app->identity_push, sizeof(swizzle_push_t));
	zest_cmd_Draw(command_list, 4, 1, 0, 0);

	zest_cmd_SendPushConstants(command_list, &app->swizzled_push, sizeof(swizzle_push_t));
	zest_cmd_Draw(command_list, 4, 1, 0, 0);
}

int PollSDLEvents(zest_context context, SDL_Event *event) {
	int running = 1;
	while (SDL_PollEvent(event)) {
		if (event->type == SDL_QUIT) {
			running = 0;
		}
		if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_CLOSE && event->window.windowID == SDL_GetWindowID((SDL_Window*)zest_Window(context))) {
			running = 0;
		}
	}
	return running;
}

void MainLoop(swizzle_app_t *app) {
	int running = 1;
	SDL_Event event;

	while (running) {
		running = PollSDLEvents(app->context, &event);
		zest_UpdateDevice(app->device);
		zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(app->context, 0, 0);
		if (zest_BeginFrame(app->context)) {
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);
			if (!frame_graph) {
				if (zest_BeginFrameGraph(app->context, "Render Graph", &cache_key)) {
					zest_ImportSwapchainResource();
					zest_BeginRenderPass("Draw Swizzled Quads"); {
						zest_ConnectSwapChainOutput();
						zest_SetPassTask(DrawQuads, app);
						zest_EndPass();
					}
					frame_graph = zest_EndFrameGraph();
				}
			}
			zest_EndFrame(app->context, frame_graph);
		}
	}
}

int main(int argc, char *argv[]) {
	zest_create_context_info_t create_info = zest_CreateContextInfo();

	swizzle_app_t app = {};

	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "Image Swizzle");

	app.device = zest_implsdl2_CreateVulkanDevice(&window_data, 0);
	app.context = zest_CreateContext(app.device, &window_data, &create_info);

	InitApp(&app);

	MainLoop(&app);
	zest_DestroyDevice(app.device);

	return 0;
}
