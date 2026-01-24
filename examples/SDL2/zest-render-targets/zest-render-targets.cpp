#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_MSDF_IMPLEMENTATION
#include "zest-render-targets.h"
#include <zest.h>

/*
Example show how to do bloom/blur with down/up sampling in a compute shader rendering and reading from 
multiple render targets before outputting to the swapchain.
*/

void InitExample(render_target_app_t *example) {

	//Create the render targets that we will draw the layers to. The Base render target will be where we draw the images, The top render target
	//will be where we draw the result of the the blur effect.

	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	example->sampler = zest_CreateSampler(example->context, &sampler_info);
	zest_sampler sampler = zest_GetSampler(example->sampler);
	example->sampler_index = zest_AcquireSamplerIndex(example->device, sampler);

	zest_shader_handle downsampler_shader = zest_CreateShaderFromFile(example->device, "examples/SDL2/zest-render-targets/shaders/downsample.comp", "downsample_comp.spv", zest_compute_shader, 1);
	zest_shader_handle upsampler_shader = zest_CreateShaderFromFile(example->device, "examples/SDL2/zest-render-targets/shaders/upsample.comp", "upsample_comp.spv", zest_compute_shader, 1);
	zest_shader_handle blur_vert = zest_CreateShaderFromFile(example->device, "examples/SDL2/zest-render-targets/shaders/blur.vert", "blur_vert.spv", zest_vertex_shader, 1);
	zest_shader_handle pass_frag = zest_CreateShaderFromFile(example->device, "examples/SDL2/zest-render-targets/shaders/pass.frag", "pass_frag.spv", zest_fragment_shader, 1);

    example->composite_pipeline = zest_CreatePipelineTemplate(example->device, "pipeline_pass_through");
	zest_SetPipelineVertShader(example->composite_pipeline, blur_vert);
	zest_SetPipelineFragShader(example->composite_pipeline, pass_frag);
    zest_SetPipelineBlend(example->composite_pipeline, zest_AdditiveBlendState());
	zest_SetPipelineDisableVertexInput(example->composite_pipeline);

	//Load a font
	if (!zest__file_exists("examples/assets/Lato-Regular.msdf")) {
		example->font = zest_CreateMSDF(example->context, "examples/assets/Lato-Regular.ttf", example->sampler_index, 64.f, 4.f);
		zest_SaveMSDF(&example->font, "examples/assets/Lato-Regular.msdf");
	} else {
		example->font = zest_LoadMSDF(example->context, "examples/assets/Lato-Regular.msdf", example->sampler_index);
	}

	example->font_resources = zest_CreateFontResources(example->context, "examples/assets/shaders/font.vert", "examples/assets/shaders/font.frag");
	example->font_layer = zest_CreateFontLayer(example->context, "MSDF Font Example Layer", 500);

	//Create the compute shader pipeline for downsampling
	example->downsampler_compute = zest_CreateCompute(example->device, "Downsampler Compute", downsampler_shader);

	//Create the compute shader pipeline for up sampling
	example->upsampler_compute = zest_CreateCompute(example->device, "Upsampler Compute", upsampler_shader);
}

void zest_DrawRenderTarget(zest_command_list command_list, void *user_data) {
    render_target_app_t *example = (render_target_app_t*)user_data;
	zest_resource_node downsampler = zest_GetPassInputResource(command_list, "Downsampler");
	zest_resource_node render_target = zest_GetPassInputResource(command_list, "Upsampler");

	zest_uint up_bindless_index = zest_GetTransientSampledImageBindlessIndex(command_list, render_target, zest_texture_2d_binding);
	zest_uint down_bindless_index = zest_GetTransientSampledImageBindlessIndex(command_list, downsampler, zest_texture_2d_binding);

	zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);

	zest_pipeline pipeline = zest_GetPipeline(example->composite_pipeline, command_list);
	zest_cmd_BindPipeline(command_list, pipeline);

	CompositePushConstants push;
	push.base_index = down_bindless_index;
	push.bloom_index = up_bindless_index;
	push.bloom_alpha = example->bloom_constants.settings.x;
	push.sampler_index = example->sampler_index;

	zest_cmd_SendPushConstants(command_list, &push, sizeof(CompositePushConstants));

	zest_cmd_Draw(command_list, 3, 1, 0, 0);

}

void zest_DownsampleCompute(zest_command_list command_list, void* user_data) {
	render_target_app_t* example = (render_target_app_t*)user_data;
	zest_resource_node downsampler_target = zest_GetPassInputResource(command_list, "Downsampler");

	// Get separate bindless indices for each mip level for reading (sampler) and writing (storage)
	zest_uint* sampler_mip_indices = zest_GetTransientSampledMipBindlessIndexes(command_list, downsampler_target, zest_texture_2d_binding);
	zest_uint* storage_mip_indices = zest_GetTransientSampledMipBindlessIndexes(command_list, downsampler_target, zest_storage_image_binding);

	BlurPushConstants push = { 0 };

	zest_uint current_width = zest_GetResourceWidth(downsampler_target);
	zest_uint current_height = zest_GetResourceHeight(downsampler_target);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_compute downsampler_compute = zest_GetCompute(example->downsampler_compute);

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, downsampler_compute);
	push.sampler_index = example->sampler_index;

	zest_uint mip_levels = zest_GetResourceMipLevels(downsampler_target);
	for (zest_uint mip_index = 1; mip_index != mip_levels; ++mip_index) {
		// Update push constants for the current dispatch
		// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
		push.src_mip_index = sampler_mip_indices[mip_index - 1];
		push.storage_image_index = storage_mip_indices[mip_index];

		// Apply ceiling division to get the workgroup count
		zest_uint group_count_x = (current_width + local_size_x - 1) / local_size_x;
		zest_uint group_count_y = (current_height + local_size_y - 1) / local_size_y;

		current_width = ZEST__MAX(1u, current_width >> 1);
		current_height = ZEST__MAX(1u, current_height >> 1);

		zest_cmd_SendPushConstants(command_list, &push, sizeof(BlurPushConstants));

		//Dispatch the compute shader
		zest_cmd_DispatchCompute(command_list, group_count_x, group_count_y, 1);
		if (mip_index < mip_levels - 1) {
			zest_cmd_InsertComputeImageBarrier(command_list, downsampler_target, mip_index);
		}
	}
}

void zest_UpsampleCompute(zest_command_list command_list, void *user_data) {
	render_target_app_t *example = (render_target_app_t *)user_data;
	zest_resource_node upsampler_target = zest_GetPassOutputResource(command_list, "Upsampler");
	zest_resource_node downsampler_target = zest_GetPassInputResource(command_list, "Downsampler");

	// Get separate bindless indices for each mip level for reading (sampler) and writing (storage)
	zest_uint *sampler_mip_indices = zest_GetTransientSampledMipBindlessIndexes(command_list, upsampler_target, zest_texture_2d_binding);
	zest_uint *storage_mip_indices = zest_GetTransientSampledMipBindlessIndexes(command_list, upsampler_target, zest_storage_image_binding);
	zest_uint *downsampler_mip_indices = zest_GetTransientSampledMipBindlessIndexes(command_list, downsampler_target, zest_texture_2d_binding);

	BlurPushConstants push = { 0 };

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_uint mip_levels = zest_GetResourceMipLevels(upsampler_target);
	zest_uint mip_to_blit = mip_levels - 1;

	zest_cmd_CopyImageMip(command_list, downsampler_target, upsampler_target, mip_to_blit, zest_pipeline_stage_compute_shader_bit);

	zest_compute upsampler_compute = zest_GetCompute(example->upsampler_compute);

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, upsampler_compute);

	zest_uint resource_width = zest_GetResourceWidth(upsampler_target);
	zest_uint resource_height = zest_GetResourceHeight(upsampler_target);
	push.sampler_index = example->sampler_index;

	for (int mip_index = mip_levels - 2; mip_index >= 0; --mip_index) {
		zest_uint current_width = ZEST__MAX(1u, resource_width >> mip_index);
		zest_uint current_height = ZEST__MAX(1u, resource_height >> mip_index);
		// Update push constants for the current dispatch
		// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
		push.src_mip_index = sampler_mip_indices[mip_index + 1];
		push.storage_image_index = storage_mip_indices[mip_index];
		push.downsampler_mip_index = downsampler_mip_indices[mip_index + 1];

		// Apply ceiling division to get the workgroup count
		zest_uint group_count_x = (current_width + local_size_x - 1) / local_size_x;
		zest_uint group_count_y = (current_height + local_size_y - 1) / local_size_y;

		zest_cmd_SendPushConstants(command_list, &push, sizeof(BlurPushConstants));

		//Dispatch the compute shader
		zest_cmd_DispatchCompute(command_list, group_count_x, group_count_y, 1);
		if (mip_index > 0) {
			zest_cmd_InsertComputeImageBarrier(command_list, upsampler_target, mip_index);
		}
	}
}

void UpdateMouse(render_target_app_t *app) {
	int mouse_x, mouse_y;
	SDL_GetMouseState(&mouse_x, &mouse_y);
	app->mouse_x = (float)mouse_x;
	app->mouse_y = (float)mouse_y;

	int global_x, global_y;
	SDL_GetGlobalMouseState(&global_x, &global_y);

	int window_x, window_y;
	SDL_GetWindowPosition((SDL_Window*)zest_Window(app->context), &window_x, &window_y);

	app->mouse_x = global_x - window_x;
	app->mouse_y = global_y - window_y;
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

void Mainloop(render_target_app_t *example) {
	zest_microsecs running_time = zest_Microsecs();
	zest_microsecs frame_time = 0;
	zest_uint frame_count = 0;
	zest_uint fps = 0;
	int running = 1;
	SDL_Event event;

	while (running) {
		running = PollSDLEvents(example->context, &event);
		zest_microsecs current_frame_time = zest_Microsecs() - running_time;
		running_time = zest_Microsecs();
		frame_time += current_frame_time;
		frame_count += 1;
		if (frame_time >= ZEST_MICROSECS_SECOND) {
			frame_time -= ZEST_MICROSECS_SECOND;
			fps = frame_count;
			frame_count = 0;
		}

		zest_UpdateDevice(example->device);

		UpdateMouse(example);
		
		zest_layer font_layer = zest_GetLayer(example->font_layer);

		if (zest_BeginFrame(example->context)) {

			//We can adjust the alpha and blend type based on the mouse position
			float threshold = (float)example->mouse_x / zest_ScreenWidthf(example->context);
			float knee = (float)example->mouse_y / zest_ScreenHeightf(example->context);
			knee = ZEST__CLAMP(knee, 0.f, 1.f) * .5f;
			threshold = ZEST__CLAMP(threshold, 0.f, 2.f);

			example->bloom_constants.settings.x = threshold;
			example->bloom_constants.settings.y = knee;

			//Set the font to use for the font layer
			zest_SetMSDFFontDrawing(font_layer, &example->font, &example->font_resources);
			zest_SetLayerColor(font_layer, 255, 100, 255, 255);
			//Draw the text
			zest_DrawMSDFText(font_layer,  zest_ScreenWidth(example->context) * .5f, zest_ScreenHeightf(example->context) * .15f, .5f, .5f, 1.f, 0.f, "Basic Bloom Effect ...");
			zest_SetLayerColor(font_layer, 50, 255, 50, 255);
			zest_DrawMSDFText(font_layer,  zest_ScreenWidth(example->context) * .5f, zest_ScreenHeightf(example->context) * .35f, .5f, .5f, 1.f, 0.f, "Using down/up sampling");
			zest_SetLayerColor(font_layer, 50, 50, 255, 255);
			zest_DrawMSDFText(font_layer,  zest_ScreenWidth(example->context) * .5f, zest_ScreenHeightf(example->context) * .55f, .5f, .5f, 1.f, 0.f, "No thresholding just as is");
			zest_SetLayerColor(font_layer, 255, 50, 50, 255);
			zest_DrawMSDFText(font_layer, zest_ScreenWidth(example->context) * .5f, zest_ScreenHeightf(example->context) * .75f, .5f, .5f, 1.f, 0.f, "FPS: %u", fps);

			zest_image_resource_info_t image_info = {
				zest_format_r16g16b16a16_sfloat,
				zest_resource_usage_hint_copyable,
				zest_ScreenWidth(example->context),
				zest_ScreenHeight(example->context),
				7
			};

			zest_frame_graph_cache_key_t cache_key = {};
			cache_key = zest_InitialiseCacheKey(example->context, &example->cache_info, sizeof(RenderCacheInfo));

			//Create the render graph
			zest_SetSwapchainClearColor(example->context, 0.f, .1f, .2f, 1.f);
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(example->context, &cache_key);
			if (!frame_graph) {
				if (zest_BeginFrameGraph(example->context, "Bloom Example Render Graph", &cache_key)) {

					//Add resources
					zest_resource_node font_layer_resources = zest_AddTransientLayerResource("Font resources", font_layer, false);
					zest_resource_node downsampler = zest_AddTransientImageResource("Downsampler", &image_info);
					zest_resource_node upsampler = zest_AddTransientImageResource("Upsampler", &image_info);
					zest_ImportSwapchainResource();

					zest_compute downsampler_compute = zest_GetCompute(example->downsampler_compute);
					zest_compute upsampler_compute = zest_GetCompute(example->upsampler_compute);

					//---------------------------------Transfer Pass----------------------------------------------------
					zest_BeginTransferPass("Upload Font Data"); {
						//outputs
						zest_ConnectOutput(font_layer_resources);
						//tasks
						zest_SetPassTask(zest_UploadInstanceLayerData, font_layer);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//---------------------------------Draw Base Pass---------------------------------------------------
					zest_BeginRenderPass("Graphics Pass"); {
						zest_ConnectInput(font_layer_resources);
						zest_ConnectOutput(downsampler);
						//tasks
						zest_SetPassTask(zest_DrawInstanceLayer, font_layer);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//---------------------------------Downsample Pass--------------------------------------------------
					zest_BeginComputePass(downsampler_compute, "Downsampler Pass"); {
						zest_ConnectInput(downsampler);
						zest_ConnectOutput(downsampler);
						//tasks
						zest_SetPassTask(zest_DownsampleCompute, example);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//---------------------------------Upsample Pass----------------------------------------------------
					zest_BeginComputePass(upsampler_compute, "Upsampler Pass"); {
						zest_ConnectInput(downsampler);
						zest_ConnectOutput(upsampler);
						//tasks
						zest_SetPassTask(zest_UpsampleCompute, example);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//---------------------------------Render Pass------------------------------------------------------
					zest_BeginRenderPass("Final Render"); {
						//inputs
						zest_ConnectInput(upsampler);
						zest_ConnectInput(downsampler);
						//outputs
						zest_ConnectSwapChainOutput();
						//tasks
						zest_SetPassTask(zest_DrawRenderTarget, example);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//End and execute the render graph
					frame_graph = zest_EndFrameGraph();
				}
			}
			zest_EndFrame(example->context, frame_graph);
		}
		if (zest_SwapchainWasRecreated(example->context)) {
			zest_SetLayerSizeToSwapchain(font_layer);
			zest_UpdateFontTransform(&example->font);
		}
	}

}

int main(int argc, char *argv[]) {
	//Make a config struct where you can configure zest with some options
	zest_create_context_info_t create_info = zest_CreateContextInfo();

	//Create a window using SDL2. We must do this before setting up the device as it's needed to get
	//the extensions info.
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "Render Targets");

	render_target_app_t app = {};

	//Create a device using a helper function for SDL2.
	app.device = zest_implsdl2_CreateVulkanDevice(&window_data, false);

	//Initialise Zest
	app.context = zest_CreateContext(app.device, &window_data, &create_info);

	InitExample(&app);

	//Start the Zest main loop
	Mainloop(&app);
	zest_FreeFont(&app.font);
	zest_DestroyDevice(app.device);

	return 0;
}
