#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_MSDF_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include "zest-render-targets.h"
#include <zest.h>

void InitExample(render_target_app_t *example) {

	//Create the render targets that we will draw the layers to. The Base render target will be where we draw the images, The top render target
	//will be where we draw the result of the the blur effect.

	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	zest_sampler_info_t mipped_sampler_info = zest_CreateMippedSamplerInfo(7);
	example->pass_through_sampler = zest_CreateSampler(example->context, &sampler_info);
	example->mipped_sampler = zest_CreateSampler(example->context, &mipped_sampler_info);
	example->pass_through_sampler_index = zest_AcquireGlobalSamplerIndex(example->pass_through_sampler);
	example->mipped_sampler_index = zest_AcquireGlobalSamplerIndex(example->mipped_sampler);

	zest_shader_handle downsampler_shader = zest_CreateShaderFromFile(example->context, "examples/GLFW/zest-render-targets/shaders/downsample.comp", "downsample_comp.spv", zest_compute_shader, 1);
	zest_shader_handle upsampler_shader = zest_CreateShaderFromFile(example->context, "examples/GLFW/zest-render-targets/shaders/upsample.comp", "upsample_comp.spv", zest_compute_shader, 1);
	zest_shader_handle blur_vert = zest_CreateShaderFromFile(example->context, "examples/GLFW/zest-render-targets/shaders/blur.vert", "blur_vert.spv", zest_vertex_shader, 1);
	zest_shader_handle pass_frag = zest_CreateShaderFromFile(example->context, "examples/GLFW/zest-render-targets/shaders/pass.frag", "pass_frag.spv", zest_fragment_shader, 1);

    example->composite_pipeline = zest_BeginPipelineTemplate(example->context, "pipeline_pass_through");
	zest_SetPipelineVertShader(example->composite_pipeline, blur_vert);
	zest_SetPipelineFragShader(example->composite_pipeline, pass_frag);
    zest_ClearPipelineDescriptorLayouts(example->composite_pipeline);
	zest_AddPipelineDescriptorLayout(example->composite_pipeline, zest_GetGlobalBindlessLayout(example->context));
	zest_SetPipelinePushConstantRange(example->composite_pipeline, sizeof(CompositePushConstants), zest_shader_fragment_stage);
    zest_SetPipelineBlend(example->composite_pipeline, zest_AdditiveBlendState());
	zest_SetPipelineDisableVertexInput(example->composite_pipeline);

	example->render_target_resources = zest_CreateShaderResources(example->context);
	zest_AddGlobalBindlessSetToResources(example->render_target_resources);

	example->font_layer = zest_CreateFontLayer(example->context, "MSDF Font Example Layer");

	//Load a font
	if (!zest__file_exists("examples/assets/Lato-Regular.msdf")) {
		example->font = zest_CreateMSDF(example->context, "examples/assets/Lato-Regular.ttf", example->pass_through_sampler_index, 64.f, 4.f);
		zest_SaveMSDF(&example->font, "examples/assets/Lato-Regular.msdf");
	} else {
		example->font = zest_LoadMSDF(example->context, "examples/assets/Lato-Regular.msdf", example->pass_through_sampler_index);
	}

	example->font_resources = zest_CreateFontResources(example->context, "shaders/font.vert", "shaders/font.frag");
	example->font_layer = zest_CreateFontLayer(example->context, "MSDF Font Example Layer");

	//Set up the compute shader for downsampling
	//A builder is used to simplify the compute shader setup process
	zest_compute_builder_t downsampler_builder = zest_BeginComputeBuilder(example->context);
	//Set the user data so that we can use it in the callback funcitons
	zest_SetComputeUserData(&downsampler_builder, example);
	zest_SetComputePushConstantSize(&downsampler_builder, sizeof(BlurPushConstants));
	//Declare the actual shader to use
	zest_AddComputeShader(&downsampler_builder, downsampler_shader);
	//Finally, make the compute shader using the downsampler_builder
	example->downsampler_compute = zest_FinishCompute(&downsampler_builder, "Downsampler Compute");

	//Set up the compute shader for up sampling
	//A builder is used to simplify the compute shader setup process
	zest_compute_builder_t upsampler_builder = zest_BeginComputeBuilder(example->context);
	//Set the user data so that we can use it in the callback funcitons
	zest_SetComputeUserData(&upsampler_builder, example);
	zest_SetComputePushConstantSize(&upsampler_builder, sizeof(BlurPushConstants));
	//Declare the actual shader to use
	zest_AddComputeShader(&upsampler_builder, upsampler_shader);
	//Finally, make the compute shader using the builder
	example->upsampler_compute = zest_FinishCompute(&upsampler_builder, "Upsampler Compute");
}

void zest_DrawRenderTargetSimple(zest_command_list command_list, void *user_data) {
    render_target_app_t *example = (render_target_app_t*)user_data;
	zest_resource_node downsampler = zest_GetPassInputResource(command_list, "Downsampler");
	zest_resource_node render_target = zest_GetPassInputResource(command_list, "Upsampler");

	zest_uint up_bindless_index = zest_GetTransientSampledImageBindlessIndex(command_list, render_target, zest_texture_2d_binding);
	zest_uint down_bindless_index = zest_GetTransientSampledImageBindlessIndex(command_list, downsampler, zest_texture_2d_binding);

	zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);

	zest_pipeline pipeline = zest_PipelineWithTemplate(example->composite_pipeline, command_list);
	zest_cmd_BindPipelineShaderResource(command_list, pipeline, example->render_target_resources);

	CompositePushConstants push;
	push.base_index = down_bindless_index;
	push.bloom_index = up_bindless_index;
	push.bloom_alpha = example->bloom_constants.settings.x;
	push.sampler_index = example->pass_through_sampler_index;

	zest_cmd_SendPushConstants(command_list, pipeline, &push);

	zest_cmd_Draw(command_list, 3, 1, 0, 0);

}

void zest_DownsampleCompute(zest_command_list command_list, void* user_data) {
	render_target_app_t* example = (render_target_app_t*)user_data;
	zest_resource_node downsampler_target = zest_GetPassInputResource(command_list, "Downsampler");

	// Get separate bindless indices for each mip level for reading (sampler) and writing (storage)
	zest_uint* sampler_mip_indices = zest_GetTransientMipBindlessIndexes(command_list, downsampler_target, zest_texture_2d_binding);
	zest_uint* storage_mip_indices = zest_GetTransientMipBindlessIndexes(command_list, downsampler_target, zest_storage_image_binding);

	BlurPushConstants push = { 0 };

	zest_uint current_width = zest_GetResourceWidth(downsampler_target);
	zest_uint current_height = zest_GetResourceHeight(downsampler_target);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_descriptor_set sets[] = {
		zest_GetGlobalBindlessSet(example->context)
	};

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, example->downsampler_compute, sets, 1);
	push.sampler_index = example->mipped_sampler_index;

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

		zest_cmd_SendCustomComputePushConstants(command_list, example->downsampler_compute, &push);

		//Dispatch the compute shader
		zest_cmd_DispatchCompute(command_list, example->downsampler_compute, group_count_x, group_count_y, 1);
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
	zest_uint *sampler_mip_indices = zest_GetTransientMipBindlessIndexes(command_list, upsampler_target, zest_texture_2d_binding);
	zest_uint *storage_mip_indices = zest_GetTransientMipBindlessIndexes(command_list, upsampler_target, zest_storage_image_binding);
	zest_uint *downsampler_mip_indices = zest_GetTransientMipBindlessIndexes(command_list, downsampler_target, zest_texture_2d_binding);

	BlurPushConstants push = { 0 };

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_descriptor_set sets[] = {
		zest_GetGlobalBindlessSet(example->context),
	};

	zest_uint mip_levels = zest_GetResourceMipLevels(upsampler_target);
	zest_uint mip_to_blit = mip_levels - 1;

	zest_cmd_CopyImageMip(command_list, downsampler_target, upsampler_target, mip_to_blit, zest_pipeline_compute_stage);

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, example->upsampler_compute, sets, 1);

	zest_uint resource_width = zest_GetResourceWidth(upsampler_target);
	zest_uint resource_height = zest_GetResourceHeight(upsampler_target);
	push.sampler_index = example->mipped_sampler_index;

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

		zest_cmd_SendCustomComputePushConstants(command_list, example->upsampler_compute, &push);

		//Dispatch the compute shader
		zest_cmd_DispatchCompute(command_list, example->upsampler_compute, group_count_x, group_count_y, 1);
		if (mip_index > 0) {
			zest_cmd_InsertComputeImageBarrier(command_list, upsampler_target, mip_index);
		}
	}
}

void UpdateMouse(render_target_app_t *example) {
	double mouse_x, mouse_y;
	GLFWwindow *handle = (GLFWwindow *)zest_Window(example->context);
	glfwGetCursorPos(handle, &mouse_x, &mouse_y);
	double last_mouse_x = example->mouse_x;
	double last_mouse_y = example->mouse_y;
	example->mouse_x = mouse_x;
	example->mouse_y = mouse_y;
	example->mouse_delta_x = last_mouse_x - example->mouse_x;
	example->mouse_delta_y = last_mouse_y - example->mouse_y;
}

void Mainloop(render_target_app_t *example) {
	zest_microsecs running_time = zest_Microsecs();
	zest_microsecs frame_time = 0;
	zest_uint frame_count = 0;
	zest_uint fps = 0;

	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(example->context))) {
		zest_microsecs current_frame_time = zest_Microsecs() - running_time;
		running_time = zest_Microsecs();
		frame_time += current_frame_time;
		frame_count += 1;
		if (frame_time >= ZEST_MICROSECS_SECOND) {
			frame_time -= ZEST_MICROSECS_SECOND;
			fps = frame_count;
			frame_count = 0;
		}

		glfwPollEvents();

		UpdateMouse(example);

		//We can adjust the alpha and blend type based on the mouse position
		float threshold = (float)example->mouse_x / zest_ScreenWidthf(example->context);
		float knee = (float)example->mouse_y / zest_ScreenHeightf(example->context);
		knee = ZEST__CLAMP(knee, 0.f, 1.f) * .5f;
		threshold = ZEST__CLAMP(threshold, 0.f, 2.f);

		example->bloom_constants.settings.x = threshold;
		example->bloom_constants.settings.y = knee;
		//example->downsampler->recorder->outdated[context->current_fif] = 1;

		//Set the font to use for the font layer
		/*
		zest_SetMSDFFontDrawing(example->font_layer, &example->font, &example->font_resources);
		//Set the shadow and color
		zest_SetFontColor(&example->font, 1.f, 1.f, 1.f, 1.f);
		//Draw the text
		zest_DrawMSDFText(example->font_layer,  zest_ScreenWidth(example->context) * .5f, zest_ScreenHeightf(example->context) * .15f, .5f, .5f, 1.f, 0.f, "Basic Bloom Effect ...");
		zest_SetMSDFFontDrawing(example->font_layer, &example->font, &example->font_resources);
		zest_SetFontColor(&example->font, 0.f, 1.f, 0.f, 1.f);
		zest_DrawMSDFText(example->font_layer,  zest_ScreenWidth(example->context) * .5f, zest_ScreenHeightf(example->context) * .35f, .5f, .5f, 1.f, 0.f, "Using down/up sampling");
		zest_SetMSDFFontDrawing(example->font_layer, &example->font, &example->font_resources);
		zest_SetFontColor(&example->font, 1.f, 0.f, 0.f, 1.f);
		zest_DrawMSDFText(example->font_layer,  zest_ScreenWidth(example->context) * .5f, zest_ScreenHeightf(example->context) * .55f, .5f, .5f, 1.f, 0.f, "No thresholding just as is");
		*/
		zest_SetMSDFFontDrawing(example->font_layer, &example->font, &example->font_resources);
		zest_SetFontColor(&example->font, 0.f, 0.f, 1.f, 1.f);
		zest_DrawMSDFText(example->font_layer, zest_ScreenWidth(example->context) * .5f, zest_ScreenHeightf(example->context) * .75f, .5f, .5f, 1.f, 0.f, "FPS: %u", fps);

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
		if (zest_BeginFrame(example->context)) {
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(example->context, &cache_key);
			if(!frame_graph) {
				if (zest_BeginFrameGraph(example->context, "Bloom Example Render Graph", &cache_key)) {

					//Add resources
					zest_resource_node font_layer_resources = zest_AddTransientLayerResource("Font resources", example->font_layer, false);
					zest_resource_node downsampler = zest_AddTransientImageResource("Downsampler", &image_info);
					zest_resource_node upsampler = zest_AddTransientImageResource("Upsampler", &image_info);
					zest_ImportSwapchainResource();

					//---------------------------------Transfer Pass----------------------------------------------------
					zest_BeginTransferPass("Upload Font Data"); {
						//outputs
						zest_ConnectOutput(font_layer_resources);
						//tasks
						zest_SetPassTask(zest_UploadInstanceLayerData, &example->font_layer);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//---------------------------------Draw Base Pass---------------------------------------------------
					zest_BeginRenderPass("Graphics Pass"); {
						zest_ConnectInput(font_layer_resources);
						zest_ConnectOutput(downsampler);
						//tasks
						zest_SetPassTask(zest_DrawInstanceLayer, &example->font_layer);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//---------------------------------Downsample Pass--------------------------------------------------
					zest_BeginComputePass(example->downsampler_compute, "Downsampler Pass"); {
						//The stage should be assumed based on the pass queue type.
						zest_ConnectInput(downsampler);
						zest_ConnectOutput(downsampler);
						//tasks
						zest_SetPassTask(zest_DownsampleCompute, example);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//---------------------------------Upsample Pass----------------------------------------------------
					zest_BeginComputePass(example->upsampler_compute, "Upsampler Pass"); {
						//The stage should be assumed based on the pass queue type.
						zest_ConnectInput(downsampler);
						zest_ConnectOutput(upsampler);
						//tasks
						zest_SetPassTask(zest_UpsampleCompute, example);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//---------------------------------Render Pass------------------------------------------------------
					//zest_pass_node graphics_pass = zest_BeginRenderPass("Graphics Pass");
					zest_BeginGraphicBlankScreen("Blank Screen"); {
						//inputs
						zest_ConnectInput(upsampler);
						zest_ConnectInput(downsampler);
						//outputs
						zest_ConnectSwapChainOutput();
						//tasks
						zest_SetPassTask(zest_DrawRenderTargetSimple, example);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//End and execute the render graph
					frame_graph = zest_EndFrameGraph();
					zest_QueueFrameGraphForExecution(example->context, frame_graph);
					zest_PrintCompiledFrameGraph(frame_graph);
				}
			} else {
				zest_QueueFrameGraphForExecution(example->context, frame_graph);
			}
			zest_EndFrame(example->context);
		}
	}

}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
int main()
{
	//Make a config struct where you can configure zest with some options
	//zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(zest_validation_flag_enable_sync);
	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);

	render_target_app_t app = {};
	zest_device device = zest_implglfw_CreateDevice(0);
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "Minimal Example");
	//Initialise Zest
	app.context = zest_CreateContext(device, &window_handles, &create_info);

	InitExample(&app);

	//Start the Zest main loop
	Mainloop(&app);
	zest_FreeFont(&app.font);
	zest_DestroyContext(app.context);

	return 0;
}
#else
int main(void) {

	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	zest_CreateContext(&create_info);
	zest_LogFPSToConsole(1);

	render_target_app_t example;
	InitExample(&example);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(UpdateCallback);

	zest_Start();

	return 0;
}
#endif
