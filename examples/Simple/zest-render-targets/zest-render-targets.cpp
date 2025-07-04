#include <zest.h>
#include "zest-render-targets.h"

void InitExample(RenderTargetExample *example) {

	//Create the render targets that we will draw the layers to. The Base render target will be where we draw the images, The top render target
	//will be where we draw the result of the the blur effect.
	example->top_target = zest_CreateSimpleRenderTarget("Top render target");
	example->base_target = zest_CreateHDRRenderTarget("Base render target");
	example->compositor = zest_CreateHDRRenderTarget("Compositor render target");
	example->tonemap = zest_CreateSimpleRenderTarget("Tone map render target");
	example->downsampler = zest_CreateMippedHDRRenderTarget("Bloom pass downsampler", 8);
	example->upsampler = zest_CreateMippedHDRRenderTarget("Bloom pass upsampler", 8);
	example->downsampler->input_source = example->base_target;

	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	zest_CreateShaderFromFile("examples/Simple/zest-render-targets/shaders/downsample.frag", "downsample_frag.spv", shaderc_fragment_shader, 1, compiler, 0);
	zest_CreateShaderFromFile("examples/Simple/zest-render-targets/shaders/upsample.frag", "upsample_frag.spv", shaderc_fragment_shader, 1, compiler, 0);
	zest_CreateShaderFromFile("examples/Simple/zest-render-targets/shaders/blur.vert", "blur_vert.spv", shaderc_vertex_shader, 1, compiler, 0);
	zest_CreateShaderFromFile("examples/Simple/zest-render-targets/shaders/composite.frag", "composite_frag.spv", shaderc_fragment_shader, 1, compiler, 0);
	zest_CreateShaderFromFile("examples/Simple/zest-render-targets/shaders/bloom_pass.frag", "bloom_pass_frag.spv", shaderc_fragment_shader, 1, compiler, 0);
	shaderc_compiler_release(compiler);

	//Make a pipeline to handle the blur effect
	example->downsample_pipeline = zest_BeginPipelineTemplate("downsampler");
	//Set the push constant range for the fragment shader 
	zest_SetPipelinePushConstantRange(example->downsample_pipeline, sizeof(zest_mip_push_constants_t), 0, VK_SHADER_STAGE_FRAGMENT_BIT);
	//Set the vert and frag shaders for the blur effect
	zest_SetPipelineVertShader(example->downsample_pipeline, "blur_vert.spv", 0);
	zest_SetPipelineFragShader(example->downsample_pipeline, "downsample_frag.spv", 0);
	//Add a descriptor set layout for the pipeline, it only needs a single sampler for the fragment shader.
	//Clear the current layouts that are in the pipeline.
	zest_ClearPipelineDescriptorLayouts(example->downsample_pipeline);
    zest_AddPipelineDescriptorLayout(example->downsample_pipeline, *zest_GetDescriptorSetLayoutVK("1 sampler"));
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
    zest_AddPipelineDescriptorLayout(example->upsample_pipeline, *zest_GetDescriptorSetLayoutVK("2 sampler"));
    zest_EndPipelineTemplate(example->upsample_pipeline);
    example->upsample_pipeline->colorBlendAttachment = zest_AdditiveBlendState();

    example->composite_pipeline = zest_CopyPipelineTemplate("pipeline_compositor", zest_PipelineTemplate("pipeline_swap_chain"));
    zest_SetText(&example->composite_pipeline->vertShaderFile, "blur_vert.spv");
    zest_SetText(&example->composite_pipeline->fragShaderFile, "composite_frag.spv");
	zest_ClearPipelinePushConstantRanges(example->composite_pipeline);
    zest_ClearPipelineDescriptorLayouts(example->composite_pipeline);
    zest_AddPipelineDescriptorLayout(example->composite_pipeline, *zest_GetDescriptorSetLayoutVK("2 sampler"));
	zest_SetPipelinePushConstantRange(example->composite_pipeline, sizeof(CompositePushConstants), 0, VK_SHADER_STAGE_FRAGMENT_BIT);
	zest_SetPipelinePushConstants(example->composite_pipeline, &example->composite_push_constants);
    zest_EndPipelineTemplate(example->composite_pipeline);
    example->composite_pipeline->colorBlendAttachment = zest_AdditiveBlendState();

    example->bloom_pass_pipeline = zest_CopyPipelineTemplate("pipeline_bloom_pass", zest_PipelineTemplate("downsampler"));
    zest_SetText(&example->bloom_pass_pipeline->vertShaderFile, "blur_vert.spv");
    zest_SetText(&example->bloom_pass_pipeline->fragShaderFile, "bloom_pass_frag.spv");
    zest_ClearPipelineDescriptorLayouts(example->bloom_pass_pipeline);
    zest_AddPipelineDescriptorLayout(example->bloom_pass_pipeline, *zest_GetDescriptorSetLayoutVK("1 sampler"));
	zest_SetPipelinePushConstantRange(example->bloom_pass_pipeline, sizeof(BloomPushConstants), 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    zest_EndPipelineTemplate(example->bloom_pass_pipeline);
    example->bloom_pass_pipeline->colorBlendAttachment = zest_AdditiveBlendState();

	example->downsampler->pipeline_template = example->downsample_pipeline;
	//Create the render queue
	//For a bloom effect we will have a base render target where we can render the scene. 
	//Then that render target is passed to a down sampler render pass that will run the downsampling shader that
	//progressively blurs to the next mip level down. Then that render target is passed to an up sampler where the same
	//process happens in reverse. Finally we set up a composite draw command where we blend the base target with the 
	//up sampled blurred render target and then tonemapped and output straight to the swap chain.

	//First create a queue and set it's dependency to the present queue. This means that it will wait on the swap chain to present the final render to the screen before rendering again.
	//But bear in mind that there are multiple frames in flight (as long as ZEST_MAX_FIF is >1) so while one queue is being executed on the gpu the next one will be created in the meantime.
	//The best explaination I've seen of this can be found here: https://software.intel.com/content/www/us/en/develop/articles/practical-approach-to-vulkan-part-1.html
	example->command_queue = zest_NewCommandQueue("Screen Blur/Bloom");
	{
		//Create draw commands that render to a base render target
		//Note: all draw commands will happen in the order that they're added here as they are a part of the same command buffer in "Screen Blur" command queue
		zest_NewDrawCommandSetup("Base render pass", example->base_target);
		{
			//base target needs a layer that we can use to draw to so we just use a built in one but you could use
			//your own custom layer and draw routine
			example->base_layer = zest_NewBuiltinLayerSetup("Base Layer", zest_builtin_layer_sprites);
			//Create a font layer to draw to it
			example->font_layer = zest_NewBuiltinLayerSetup("Fonts", zest_builtin_layer_fonts);
		}
		//Create draw commands that downsamples the base render target in a mip chain applying blur as it goes
		//We can use the bloom_pass_pipeline to filter the input first, for example filter out all the dark colors.
		//Specify the downsampler render target and the input render target which will be the base render target.
		zest_NewDrawCommandSetupDownSampler("Bloom pass render pass", example->downsampler, example->base_target, example->bloom_pass_pipeline);
		{
			//We use the built in down sampler, but you could specify your own callback here
			zest_SetDrawCommandsCallback(zest_DownSampleRenderTarget);
		}
		//Create draw commands that upsample the downsampled target in a mip chain applying more blur as it goes. Each
		//mip level in the up sampler will be blended with the downsampled mip level additively to create a rich bloom
		zest_NewDrawCommandSetupUpSampler("Up sample render pass", example->upsampler, example->downsampler, example->upsample_pipeline);
		{
			//We use the built in up sampler, but you could specify your own callback here if needed. Most of the 
			//customisation you'll need can be done in the fragment shader though.
			zest_SetDrawCommandsCallback(zest_UpSampleRenderTarget);
		}
		//Finally we won't see anything unless we tell the render queue to render to the swap chain to be presented to the screen, but we
		//need to specify which render targets we want to be drawn to the swap chain.
		//We can use zest_NewDrawCommandSetupCompositeToSwap which composites our render targets with a specific pipeline
		//we want to use. This shader can blend the bloom and base render target together and then tonemap before outputtin
		//directly to the swap chain. Note that all the render targets before here are HDR (16bit float format) but in
		//this phase they will be converted automatically to 8bit srgb.
		zest_NewDrawCommandSetupCompositeToSwap("Render render targets to swap", example->composite_pipeline);
		{
			zest_AddRenderTarget(example->base_target);
			zest_AddRenderTarget(example->upsampler);
			//We can add as many other render targets as we need to get drawn to the swap chain you must ensure that
			//the shader that blends the render targets together has the right amount of samplers to do this.
		}
		//Connect the render queue to the Present queue so that the swap chain has to wait until the rendering is complete before
		//presenting to the screen
		zest_FinishQueueSetup();
	}

	zest_OutputQueues();

	//Load the images into a texture
	example->texture = zest_CreateTexturePacked("Statue texture", zest_texture_format_rgba_srgb);
	example->image = zest_AddTextureImageFile(example->texture, "examples/assets/texture.jpg");
	example->wabbit = zest_AddTextureImageFile(example->texture, "examples/assets/wabbit_alpha.png");
	zest_SetTextureLayerSize(example->texture, 2048);
	//Process the images so the texture is ready to use
	zest_ProcessTextureImages(example->texture);
	//Create the shader resources for the texture and sprite drawing. This will take in a builtin uniform buffer descriptor set
	//and the texture containing the sprites and create the shader resources for them.
	example->sprite_shader_resources = zest_CombineUniformAndTextureSampler(ZestRenderer->uniform_descriptor_set, example->texture);
	//Load a font
	example->font = zest_LoadMSDFFont("examples/assets/SourceSansPro-Regular.zft");
	//Set an initial position for the rabbit.
	example->wabbit_pos.x = 10.f;
	example->wabbit_pos.y = 10.f;
	example->wabbit_pos.vx = 200.f;
	example->wabbit_pos.vy = 200.f;

	example->composite_push_constants.tonemapping.x = 1.f;
	example->composite_push_constants.tonemapping.y = 1.f;
	example->composite_push_constants.tonemapping.z = 0.f;
	example->composite_push_constants.tonemapping.w = 1.f;
	example->composite_push_constants.composting.x = 0.1f;

	example->downsampler->push_constants = &example->bloom_constants;
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	RenderTargetExample *example = static_cast<RenderTargetExample*>(user_data);

	//Set the active command queue to our custom command queue that we created
	zest_SetActiveCommandQueue(example->command_queue);
	//Update the standard 2d uniform buffer
	zest_Update2dUniformBuffer();
	//Set the color to draw with on the base layer
	zest_SetLayerColor(example->base_layer, 255, 255, 255, 255);

	//delta time for calculating the speed of the rabbit
	float delta = (float)elapsed / ZEST_MICROSECS_SECOND;

	//Set the sprite drawing to use the shader resources we created earlier
	zest_SetInstanceDrawing(example->base_layer, example->sprite_shader_resources, zest_PipelineTemplate("pipeline_2d_sprites"));
	//Set the layer intensity
	zest_SetLayerIntensity(example->base_layer, 1.f);
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

	example->composite_push_constants.composting.x = threshold;
	example->bloom_constants.settings.x = threshold;
	example->bloom_constants.settings.y = knee;
	example->downsampler->recorder->outdated[ZEST_FIF] = 1;

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
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
int main()
{
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers();
	//ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	ZEST__UNFLAG(create_info.flags, zest_init_flag_cache_shaders);
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	create_info.log_path = "./";
	create_info.thread_count = 0;
	zest_SetDescriptorPoolCount(&create_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 200);
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
