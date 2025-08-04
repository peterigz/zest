#include <zest.h>
#include "zest-render-targets.h"

void InitExample(RenderTargetExample *example) {

	//Create the render targets that we will draw the layers to. The Base render target will be where we draw the images, The top render target
	//will be where we draw the result of the the blur effect.

	VkSamplerCreateInfo sampler_info = zest_CreateSamplerInfo();
	VkSamplerCreateInfo mipped_sampler_info = zest_CreateMippedSamplerInfo(7);
	example->pass_through_sampler = zest_GetSampler(&sampler_info);
	example->mipped_sampler = zest_GetSampler(&mipped_sampler_info);

	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	zest_shader downsampler_shader = zest_CreateShaderFromFile("examples/Simple/zest-render-targets/shaders/downsample.comp", "downsample_comp.spv", shaderc_compute_shader, 1, compiler, 0);
	zest_shader upsampler_shader = zest_CreateShaderFromFile("examples/Simple/zest-render-targets/shaders/upsample.comp", "upsample_comp.spv", shaderc_compute_shader, 1, compiler, 0);
	zest_CreateShaderFromFile("examples/Simple/zest-render-targets/shaders/upsample.frag", "upsample_frag.spv", shaderc_fragment_shader, 1, compiler, 0);
	zest_CreateShaderFromFile("examples/Simple/zest-render-targets/shaders/blur.vert", "blur_vert.spv", shaderc_vertex_shader, 1, compiler, 0);
	zest_CreateShaderFromFile("examples/Simple/zest-render-targets/shaders/composite.frag", "composite_frag.spv", shaderc_fragment_shader, 1, compiler, 0);
	zest_CreateShaderFromFile("examples/Simple/zest-render-targets/shaders/bloom_pass.frag", "bloom_pass_frag.spv", shaderc_fragment_shader, 1, compiler, 0);
	zest_CreateShaderFromFile("examples/Simple/zest-render-targets/shaders/pass.frag", "pass_frag.spv", shaderc_fragment_shader, 1, compiler, 0);
	shaderc_compiler_release(compiler);

	//Make a pipeline to handle the blur effect
	example->downsample_pipeline = zest_BeginPipelineTemplate("downsampler");
	//Set the push constant range for the fragment shader 
	zest_SetPipelinePushConstantRange(example->downsample_pipeline, sizeof(zest_mip_push_constants_t), zest_shader_fragment_stage);
	//Set the vert and frag shaders for the blur effect
	zest_SetPipelineVertShader(example->downsample_pipeline, "blur_vert.spv", 0);
	zest_SetPipelineFragShader(example->downsample_pipeline, "upsampler_frag.spv", 0);
	//Add a descriptor set layout for the pipeline, it only needs a single sampler for the fragment shader.
	//Clear the current layouts that are in the pipeline.
	zest_ClearPipelineDescriptorLayouts(example->downsample_pipeline);
	zest_AddPipelineDescriptorLayout(example->downsample_pipeline, zest_vk_GetGlobalBindlessLayout());
	//Theres no vertex data for the shader so set no_vertex_input to true
	example->downsample_pipeline->no_vertex_input = true;
	//Make the pipeline template with the create_info we just set up and specify a standard render pass
	zest_EndPipelineTemplate(example->downsample_pipeline);
	//Now the template is built we can tweak it a little bit more:
	example->downsample_pipeline->depthStencil.depthWriteEnable = false;
	example->downsample_pipeline->multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	//We're not using any uniform buffers
	example->downsample_pipeline->uniforms = 0;

    example->upsample_pipeline = zest_CopyPipelineTemplate("up sampler", zest_PipelineTemplate("downsampler"));
	zest_SetPipelineVertShader(example->upsample_pipeline, "blur_vert.spv", 0);
	zest_SetPipelineFragShader(example->upsample_pipeline, "upsample_frag.spv", 0);
    zest_ClearPipelineDescriptorLayouts(example->upsample_pipeline);
	zest_AddPipelineDescriptorLayout(example->upsample_pipeline, zest_vk_GetGlobalBindlessLayout());
    zest_EndPipelineTemplate(example->upsample_pipeline);
    example->upsample_pipeline->colorBlendAttachment = zest_AdditiveBlendState();

    example->bloom_pass_pipeline = zest_CopyPipelineTemplate("pipeline_bloom_pass", zest_PipelineTemplate("downsampler"));
    zest_SetText(&example->bloom_pass_pipeline->vertShaderFile, "blur_vert.spv");
    zest_SetText(&example->bloom_pass_pipeline->fragShaderFile, "bloom_pass_frag.spv");
    zest_ClearPipelineDescriptorLayouts(example->bloom_pass_pipeline);
	zest_AddPipelineDescriptorLayout(example->bloom_pass_pipeline, zest_vk_GetGlobalBindlessLayout());
	zest_SetPipelinePushConstantRange(example->bloom_pass_pipeline, sizeof(BloomPushConstants), zest_shader_fragment_stage);
    zest_EndPipelineTemplate(example->bloom_pass_pipeline);
    example->bloom_pass_pipeline->colorBlendAttachment = zest_AdditiveBlendState();

    example->composite_pipeline = zest_CopyPipelineTemplate("pipeline_pass_through", zest_PipelineTemplate("pipeline_swap_chain"));
    zest_SetText(&example->composite_pipeline->vertShaderFile, "blur_vert.spv");
    zest_SetText(&example->composite_pipeline->fragShaderFile, "pass_frag.spv");
    zest_ClearPipelineDescriptorLayouts(example->composite_pipeline);
	zest_AddPipelineDescriptorLayout(example->composite_pipeline, zest_vk_GetGlobalBindlessLayout());
	zest_SetPipelinePushConstantRange(example->composite_pipeline, sizeof(CompositePushConstants), zest_shader_fragment_stage);
    zest_EndPipelineTemplate(example->composite_pipeline);
    example->composite_pipeline->colorBlendAttachment = zest_AdditiveBlendState();

	example->render_target_resources = zest_CreateShaderResources("Example resources");
	zest_AddGlobalBindlessSetToResources(example->render_target_resources);

	example->font_layer = zest_CreateFontLayer("Example fonts");

	//Load the images into a texture
	example->texture = zest_CreateTexturePacked("Statue texture", zest_texture_format_rgba_srgb);
	example->image = zest_AddTextureImageFile(example->texture, "examples/assets/texture.jpg");
	example->wabbit = zest_AddTextureImageFile(example->texture, "examples/assets/wabbit_alpha.png");
	zest_SetTextureLayerSize(example->texture, 2048);
	//Process the images so the texture is ready to use
	zest_ProcessTextureImages(example->texture);
	//Load a font
	example->font = zest_LoadMSDFFont("examples/assets/SourceSansPro-Regular.zft");
	//Set an initial position for the rabbit.
	example->wabbit_pos.x = 10.f;
	example->wabbit_pos.y = 10.f;
	example->wabbit_pos.vx = 200.f;
	example->wabbit_pos.vy = 200.f;

	/*
	example->composite_push_constants.tonemapping.x = 1.f;
	example->composite_push_constants.tonemapping.y = 1.f;
	example->composite_push_constants.tonemapping.z = 0.f;
	example->composite_push_constants.tonemapping.w = 1.f;
	example->composite_push_constants.composting.x = 0.1f;
	*/

	//Set up the compute shader for downsampling
	//A builder is used to simplify the compute shader setup process
	zest_compute_builder_t downsampler_builder = zest_BeginComputeBuilder();
	//Declare the bindings we want in the shader
	zest_SetComputeBindlessLayout(&downsampler_builder, ZestRenderer->global_bindless_set_layout);
	//Set the user data so that we can use it in the callback funcitons
	zest_SetComputeUserData(&downsampler_builder, example);
	zest_SetComputePushConstantSize(&downsampler_builder, sizeof(BlurPushConstants));
	//Declare the actual shader to use
	zest_AddComputeShader(&downsampler_builder, downsampler_shader);
	//Finally, make the compute shader using the downsampler_builder
	example->downsampler_compute = zest_FinishCompute(&downsampler_builder, "Downsampler Compute");

	//Set up the compute shader for up sampling
	//A builder is used to simplify the compute shader setup process
	zest_compute_builder_t upsampler_builder = zest_BeginComputeBuilder();
	//Declare the bindings we want in the shader
	zest_SetComputeBindlessLayout(&upsampler_builder, ZestRenderer->global_bindless_set_layout);
	//Set the user data so that we can use it in the callback funcitons
	zest_SetComputeUserData(&upsampler_builder, example);
	zest_SetComputePushConstantSize(&upsampler_builder, sizeof(BlurPushConstants));
	//Declare the actual shader to use
	zest_AddComputeShader(&upsampler_builder, upsampler_shader);
	//Finally, make the compute shader using the builder
	example->upsampler_compute = zest_FinishCompute(&upsampler_builder, "Upsampler Compute");

	//example->downsampler->push_constants = &example->bloom_constants;
	example->timeline = zest_CreateExecutionTimeline();
}

void zest_DrawRenderTargetSimple(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
    RenderTargetExample *example = (RenderTargetExample*)user_data;
	zest_resource_node downsampler = zest_GetPassInputResource(context->pass_node, "Downsampler");
	zest_resource_node render_target = zest_GetPassInputResource(context->pass_node, "Upsampler");

	zest_uint up_bindless_index = zest_AcquireTransientTextureIndex(context, render_target, ZEST_TRUE, zest_combined_image_sampler_binding);
	zest_uint down_bindless_index = zest_AcquireTransientTextureIndex(context, downsampler, ZEST_TRUE, zest_combined_image_sampler_binding);

	zest_SetScreenSizedViewport(context, 0.f, 1.f);

	zest_pipeline pipeline = zest_PipelineWithTemplate(example->composite_pipeline, context->render_pass);
	zest_BindPipelineShaderResource(context->command_buffer, pipeline, example->render_target_resources);

	CompositePushConstants push;
	push.base_index = down_bindless_index;
	push.bloom_index = up_bindless_index;
	push.bloom_alpha = example->bloom_constants.settings.x;

	vkCmdPushConstants(
		command_buffer,
		pipeline->pipeline_layout,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(CompositePushConstants),
		&push);

	vkCmdDraw(command_buffer, 3, 1, 0, 0);

}

void zest_DownsampleCompute(VkCommandBuffer command_buffer, const zest_render_graph_context_t* context, void* user_data) {
	RenderTargetExample* example = (RenderTargetExample*)user_data;
	zest_resource_node downsampler_target = zest_GetPassInputResource(context->pass_node, "Downsampler");

	// Get separate bindless indices for each mip level for reading (sampler) and writing (storage)
	zest_uint* sampler_mip_indices = zest_AcquireTransientMipIndexes(context, downsampler_target, zest_combined_image_sampler_binding);
	zest_uint* storage_mip_indices = zest_AcquireTransientMipIndexes(context, downsampler_target, zest_storage_image_binding);

	BlurPushConstants push = { 0 };

	zest_uint current_width = zest_GetResourceWidth(downsampler_target);
	zest_uint current_height = zest_GetResourceHeight(downsampler_target);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	VkDescriptorSet sets[] = {
		ZestRenderer->global_set->vk_descriptor_set,
	};

	// Bind the pipeline once before the loop
	zest_BindComputePipeline(command_buffer, example->downsampler_compute, sets, 1);

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

		zest_SendCustomComputePushConstants(command_buffer, example->downsampler_compute, &push);

		//Dispatch the compute shader
		zest_DispatchCompute(command_buffer, example->downsampler_compute, group_count_x, group_count_y, 1);
		if (mip_index < mip_levels - 1) {
			zest_InsertComputeImageBarrier(command_buffer, downsampler_target, mip_index);
		}
	}
}

void zest_UpsampleCompute(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	RenderTargetExample *example = (RenderTargetExample *)user_data;
	zest_resource_node upsampler_target = zest_GetPassOutputResource(context->pass_node, "Upsampler");
	zest_resource_node downsampler_target = zest_GetPassInputResource(context->pass_node, "Downsampler");

	// Get separate bindless indices for each mip level for reading (sampler) and writing (storage)
	zest_uint *sampler_mip_indices = zest_AcquireTransientMipIndexes(context, upsampler_target, zest_combined_image_sampler_binding);
	zest_uint *storage_mip_indices = zest_AcquireTransientMipIndexes(context, upsampler_target, zest_storage_image_binding);
	zest_uint *downsampler_mip_indices = zest_AcquireTransientMipIndexes(context, downsampler_target, zest_combined_image_sampler_binding);

	BlurPushConstants push = { 0 };

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	VkDescriptorSet sets[] = {
		ZestRenderer->global_set->vk_descriptor_set,
	};

	zest_uint mip_levels = zest_GetResourceMipLevels(upsampler_target);
	zest_uint mip_to_blit = mip_levels - 1;

	zest_CopyImageMip(command_buffer, downsampler_target, upsampler_target, mip_to_blit, zest_pipeline_compute_stage);

	// Bind the pipeline once before the loop
	zest_BindComputePipeline(command_buffer, example->upsampler_compute, sets, 1);

	zest_uint resource_width = zest_GetResourceWidth(upsampler_target);
	zest_uint resource_height = zest_GetResourceHeight(upsampler_target);

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

		zest_SendCustomComputePushConstants(command_buffer, example->upsampler_compute, &push);

		//Dispatch the compute shader
		zest_DispatchCompute(command_buffer, example->upsampler_compute, group_count_x, group_count_y, 1);
		if (mip_index > 0) {
			zest_InsertComputeImageBarrier(command_buffer, upsampler_target, mip_index);
		}
	}
}


void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	RenderTargetExample *example = static_cast<RenderTargetExample*>(user_data);

	//Update the standard 2d uniform buffer
	zest_Update2dUniformBuffer();
	//Set the color to draw with on the base layer
	//zest_SetLayerColor(example->base_layer, 255, 255, 255, 255);

	//delta time for calculating the speed of the rabbit
	float delta = (float)elapsed / ZEST_MICROSECS_SECOND;

	//Set the sprite drawing to use the shader resources we created earlier
	//zest_SetInstanceDrawing(example->base_layer, example->sprite_shader_resources, zest_PipelineTemplate("pipeline_2d_sprites"));
	//Set the layer intensity
	//zest_SetLayerIntensity(example->base_layer, 1.f);
	//Draw the statue sprite to cover the screen
	//zest_DrawSprite(example->base_layer, example->image, 0.f, 0.f, 0.f, zest_ScreenWidthf(), zest_ScreenHeightf(), 0.f, 0.f, 0, 0.f);
	//bounce the rabbit if it hits the screen edge
	if (example->wabbit_pos.x >= zest_ScreenWidthf()) example->wabbit_pos.vx *= -1.f;
	if (example->wabbit_pos.y >= zest_ScreenHeightf()) example->wabbit_pos.vy *= -1.f;
	if (example->wabbit_pos.x <= 0.f) example->wabbit_pos.vx *= -1.f;
	if (example->wabbit_pos.y <= 0.f) example->wabbit_pos.vy *= -1.f;
	//Update the rabbit position
	example->wabbit_pos.x += example->wabbit_pos.vx * delta;
	example->wabbit_pos.y += example->wabbit_pos.vy * delta;
	//Draw the rabbit sprite
	//zest_DrawSprite(example->base_layer, example->wabbit, example->wabbit_pos.x, example->wabbit_pos.y, 0.f, 200.f, 200.f, 0.5f, 0.5f, 0, 0.f);

	//We can adjust the alpha and blend type based on the mouse position
	float threshold = (float)ZestApp->mouse_x / zest_ScreenWidthf();
	float knee =  (float)ZestApp->mouse_y / zest_ScreenHeightf();
	knee = ZEST__CLAMP(knee, 0.f, 1.f) * .5f;
	threshold = ZEST__CLAMP(threshold, 0.f, 2.f);

	//example->composite_push_constants.composting.x = threshold;
	example->bloom_constants.settings.x = threshold;
	example->bloom_constants.settings.y = knee;
	//example->downsampler->recorder->outdated[ZEST_FIF] = 1;

	//Set the font to use for the font layer
	zest_SetMSDFFontDrawing(example->font_layer, example->font);
	//Set the shadow and color
	zest_SetMSDFFontShadow(example->font_layer, 2.f, .25f, 1.f);
	zest_SetMSDFFontShadowColor(example->font_layer, 0.f, 0.f, 0.f, 0.75f);
	zest_SetLayerColor(example->font_layer, 255, 255, 255, 255);
	//Draw the text
	zest_DrawMSDFText(example->font_layer, "Basic Bloom Effect ...", zest_ScreenWidth() * .5f, zest_ScreenHeightf() * .15f, .5f, .5f, 80.f, 0.f);
	zest_SetLayerColor(example->font_layer, 0, 255, 0, 255);
	zest_DrawMSDFText(example->font_layer, "Using down/up sampling", zest_ScreenWidth() * .5f, zest_ScreenHeightf() * .35f, .5f, .5f, 80.f, 0.f);
	zest_SetLayerColor(example->font_layer, 255, 0, 0, 255);
	zest_DrawMSDFText(example->font_layer, "No thresholding just as is", zest_ScreenWidth() * .5f, zest_ScreenHeightf() * .55f, .5f, .5f, 80.f, 0.f);
	zest_SetLayerColor(example->font_layer, 0, 0, 255, 255);
	zest_DrawMSDFText(example->font_layer, "With HDR texture", zest_ScreenWidth() * .5f, zest_ScreenHeightf() * .75f, .5f, .5f, 80.f, 0.f);

	if (zest_MouseHit(zest_left_mouse)) {
		int d = 0;
		//zest_ScheduleRenderTargetRefresh(example->final_blur);
	}

	//Create the render graph
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Bloom Example Render Graph")) {
		//zest_ForceRenderGraphOnGraphicsQueue();
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };

		//Add resources
		zest_resource_node font_layer_resources = zest_AddInstanceLayerBufferResource("Font resources", example->font_layer, false);
		zest_resource_node font_layer_texture = zest_AddFontLayerTextureResource(example->font);
		zest_resource_node downsampler = zest_AddTransientRenderTarget("Downsampler", zest_texture_format_rgba_hdr, example->mipped_sampler);
		zest_resource_node upsampler = zest_AddTransientRenderTarget("Upsampler", zest_texture_format_rgba_hdr, example->mipped_sampler);

		//---------------------------------Transfer Pass----------------------------------------------------
		zest_pass_node upload_font_data = zest_AddTransferPassNode("Upload Font Data");
		//outputs
		zest_ConnectTransferBufferOutput(upload_font_data, font_layer_resources);
		//tasks
		zest_SetPassTask(upload_font_data, zest_UploadInstanceLayerData, example->font_layer);
		//--------------------------------------------------------------------------------------------------

		//---------------------------------Draw Base Pass---------------------------------------------------
		zest_pass_node render_target_pass = zest_AddRenderPassNode("Graphics Pass");
		zest_ConnectVertexBufferInput(render_target_pass, font_layer_resources);
		zest_ConnectSampledImageInput(render_target_pass, font_layer_texture);
		zest_ConnectRenderTargetOutput(render_target_pass, downsampler, {0});
		//tasks
		zest_SetPassTask(render_target_pass, zest_DrawFonts, example->font_layer);
		//--------------------------------------------------------------------------------------------------

		//---------------------------------Downsample Pass--------------------------------------------------
		zest_pass_node downsampler_pass = zest_AddComputePassNode(example->downsampler_compute, "Downsampler Pass");
		//The stage should be assumed based on the pass queue type.
		zest_ConnectStorageImageInput(downsampler_pass, downsampler, ZEST_TRUE);
		zest_ConnectStorageImageOutput(downsampler_pass, downsampler, ZEST_FALSE);
		//tasks
		zest_SetPassTask(downsampler_pass, zest_DownsampleCompute, example);
		//--------------------------------------------------------------------------------------------------

		//---------------------------------Upsample Pass----------------------------------------------------
		zest_pass_node upsampler_pass = zest_AddComputePassNode(example->upsampler_compute, "Upsampler Pass");
		//The stage should be assumed based on the pass queue type.
		zest_ConnectStorageImageInput(upsampler_pass, downsampler, ZEST_TRUE);
		zest_ConnectStorageImageOutput(upsampler_pass, upsampler, ZEST_FALSE);
		//tasks
		zest_SetPassTask(upsampler_pass, zest_UpsampleCompute, example);
		//--------------------------------------------------------------------------------------------------

		//---------------------------------Render Pass------------------------------------------------------
		//zest_pass_node graphics_pass = zest_AddRenderPassNode("Graphics Pass");
		zest_pass_node graphics_pass = zest_AddGraphicBlankScreen("Blank Screen");
		//inputs
		zest_ConnectSampledImageInput(graphics_pass, upsampler);
		zest_ConnectSampledImageInput(graphics_pass, downsampler);
		//outputs
		zest_ConnectSwapChainOutput(graphics_pass, clear_color);
		//tasks
		zest_SetPassTask(graphics_pass, zest_DrawRenderTargetSimple, example);
		//--------------------------------------------------------------------------------------------------

		//End and execute the render graph
		zest_render_graph render_graph = zest_EndRenderGraph();
		static int print_render_graph = 0;
		if (print_render_graph < 1) {
			zest_PrintCompiledRenderGraph(render_graph);
			print_render_graph++;
		}
	}

}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
int main()
{
	//zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(zest_validation_flag_enable_sync);
	zest_create_info_t create_info = zest_CreateInfo();
	//ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	ZEST__UNFLAG(create_info.flags, zest_init_flag_cache_shaders);
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	create_info.log_path = "./";
	create_info.thread_count = 0;
	//zest_SetDescriptorPoolCount(&create_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 200);
	zest_Initialise(&create_info);
	zest_LogFPSToConsole(1);

	RenderTargetExample example;
	InitExample(&example);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(UpdateCallback);

	zest_Start();

	return 0;
}
#else
int main(void) {

	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	zest_Initialise(&create_info);
	zest_LogFPSToConsole(1);

	RenderTargetExample example;
	InitExample(&example);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(UpdateCallback);

	zest_Start();

	return 0;
}
#endif
