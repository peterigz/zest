#include <zest.h>
#include "render_targets.h"

void InitExample(RenderTargetExample *example) {

	zest_pipeline_template_create_info_t create_info = zest_CreatePipelineTemplateCreateInfo();

	//Make a pipeline to handle the blur effect
	VkPushConstantRange image_pushconstant_range;
	image_pushconstant_range.size = sizeof(zest_push_constants_t);
	image_pushconstant_range.offset = 0;
	image_pushconstant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	zest_AddPipelineTemplatePushConstantRange(&create_info, image_pushconstant_range);
	create_info.vertShaderFile = "examples/assets/spv/blur_vert.spv";
	create_info.fragShaderFile = "examples/assets/spv/blur_frag.spv";
	create_info.viewport.extent = zest_GetSwapChainExtent();
	create_info.no_vertex_input = true;
	example->blur_pipeline = zest_AddPipeline("blur effect");
	zest_MakePipelineTemplate(example->blur_pipeline, zest_GetStandardRenderPass(), &create_info);
	example->blur_pipeline->pipeline_template.depthStencil.depthWriteEnable = false;
	example->blur_pipeline->pipeline_template.multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	example->blur_pipeline->uniforms = 0;
	example->blur_pipeline->push_constant_size = sizeof(PushConstants);
	zest_BuildPipeline(example->blur_pipeline);

	//Create the render targets
	example->top_target = zest_CreateRenderTarget("Top render target");
	example->base_target = zest_CreateRenderTarget("Base render target");
	//Post render targets can be created by passing the width and height as a ratio of the screen size, which is what we do here 
	//to create the render targets for the blur effect. One target is for the vertical blur and the other is for the horizontal blur.
	//In order to reduce flicker we can blur once at a smaller size, then reduce the size again and do the blur again.
	//Note that the ratio is based on the input source, so we don't want to reduce the size on the horizontal blur - we already did that
	//with the vertical blur whose input source was the base render target
	zest_render_target vertical_blur1 = zest_AddPostProcessRenderTarget("Vertical blur render target", 0.25, 0.25, example->base_target, example, AddVerticalBlur);
	zest_render_target horizontal_blur1 = zest_AddPostProcessRenderTarget("Horizontal blur render target", 1.f, 1.f, vertical_blur1, example, AddHorizontalBlur);
	zest_render_target vertical_blur2 = zest_AddPostProcessRenderTarget("Vertical blur 2 render target", 0.5, 0.5, horizontal_blur1, example, AddVerticalBlur);
	example->final_blur = zest_AddPostProcessRenderTarget("Final blur render target", 1.f, 1.f, vertical_blur2, example, AddHorizontalBlur);

	//Create the render queue
	//For blur effect we need to draw the scene to a base render target first, then have 2 render passes that draw to a smaller texture with horizontal and then vertical blur effects
	//then we can draw the base target to the swap chain and draw the blur texture over the top by drawing it as a textured rectangle on a top layer that doesn't get blurred.

	//First create a queue and set it's dependency to the present queue. This means that it will wait on the swap chain to present the final render to the screen before rendering again.
	//But bear in mind that there are multiple frames in flight (as long as ZEST_MAX_FIF is >1) so while one queue is being executed on the gpu the next one will be created in the meantime.
	//The best explaination I've seen of this can be found here: https://software.intel.com/content/www/us/en/develop/articles/practical-approach-to-vulkan-part-1.html
	example->command_queue = zest_NewCommandQueue("Screen Blur", zest_command_queue_flag_present_dependency);
	{
		//Create draw commands that render to a base render target
		//Note: all draw commands will happen in the order that they're added here as they are a part of the same command buffer in "Screen Blur" command queue
		zest_NewDrawCommandSetup("Base render pass", example->base_target);
		{
			//base target needs a layer that we can use to draw to so we just use a built in one but you could use
			//your own custom layer and draw routine
			example->base_layer = zest_NewBuiltinLayerSetup("Base Layer", zest_builtin_layer_sprites);
		}
		//Create draw commands that applies vertical blur to the base target
		zest_NewDrawCommandSetup("Vertical blur render pass", vertical_blur1);
		{
			//Use a draw function that renders to the whole texture. Because we specified AddVerticalBlur callback when calling zest_AddPostProcessRenderTarget above
			//it means that our vertical blur function will be called to specify the draw commands that we want to happen
			zest_SetDrawCommandsCallback(zest_DrawToRenderTargetCallback);
		}
		zest_NewDrawCommandSetup("Horizontal blur render pass", horizontal_blur1);
		{
			//Use a draw function that renders to the whole texture. Because we specified AddVerticalBlur callback when calling zest_AddPostProcessRenderTarget above
			//it means that our vertical blur function will be called to specify the draw commands that we want to happen
			zest_SetDrawCommandsCallback(zest_DrawToRenderTargetCallback);
		}
		zest_NewDrawCommandSetup("Vertical blur 2 render pass", vertical_blur2);
		{
			//Use a draw function that renders to the whole texture. Because we specified AddVerticalBlur callback when calling zest_AddPostProcessRenderTarget above
			//it means that our vertical blur function will be called to specify the draw commands that we want to happen
			zest_SetDrawCommandsCallback(zest_DrawToRenderTargetCallback);
		}
		//Create draw commands that applies horizontal blur to the vertical blur target
		zest_NewDrawCommandSetup("Horizontal blur render pass", example->final_blur);
		{
			//Use a draw function that renders to the whole texture. Because we specified AddHorizontalBlur callback when calling zest_AddPostProcessRenderTarget above
			//it means that our vertical blur function will be called to specify the draw commands that we want to happen
			zest_SetDrawCommandsCallback(zest_DrawToRenderTargetCallback);
		}
		//Create draw commands that we can use to draw on top of the blur effect
		zest_NewDrawCommandSetup("Top render pass", example->top_target);
		{
			//Create a sprite layer to draw to it
			example->top_layer = zest_NewBuiltinLayerSetup("Top Layer", zest_builtin_layer_sprites);
			example->font_layer = zest_NewBuiltinLayerSetup("Fonts", zest_builtin_layer_fonts);
		}
		//Finally we won't see anything unless we tell the render queue to render to the swap chain to be presented to the screen, but we
		//need to specify which render targets we want to be drawn to the swap chain.
		//We can use zest_NewDrawCommandSetupRenderTargetSwap which sets up a render pass to the swap chain specifying the render target to draw to it
		zest_NewDrawCommandSetupRenderTargetSwap("Render render targets to swap", example->base_target);
		{
			//We can add as many other render targets as we need to get drawn to the swap chain. In this case we'll add the top target where we can
			//draw a textured rectangle that samples the blurred texture.
			zest_AddRenderTarget(example->top_target);
		}
		//Connect the render queue to the Present queue so that the swap chain has to wait until the rendering is complete before
		//presenting to the screen
		zest_ConnectQueueToPresent();
		zest_FinishQueueSetup();
	}

	zest_OutputQueues();

	//Load the images into a texture
	example->texture = zest_CreateTexturePacked("Statue texture", zest_texture_format_rgba);
	example->image = zest_AddTextureImageFile(example->texture, "examples/assets/texture.jpg");
	example->wabbit = zest_AddTextureImageFile(example->texture, "examples/assets/wabbit_alpha.png");
	zest_ProcessTextureImages(example->texture);
	//Load a font
	example->font = zest_LoadMSDFFont("examples/assets/SourceSansPro-Regular.zft");
	example->wabbit_pos.x = 10.f;
	example->wabbit_pos.y = 10.f;
	example->wabbit_pos.vx = 200.f;
	example->wabbit_pos.vy = 200.f;
}

void AddHorizontalBlur(zest_render_target_t *target, void *data) {
	RenderTargetExample *example = static_cast<RenderTargetExample*>(data);
	zest_BindPipeline(example->blur_pipeline, *zest_GetRenderTargetSourceDescriptorSet(target));
	example->push_constants.blur.x = 1.f;
	example->push_constants.blur.y = 1.f;
	example->push_constants.blur.z = 0.f;
	example->push_constants.texture_size.x = (float)target->render_width;
	example->push_constants.texture_size.y = (float)target->render_height;
	zest_SendPushConstants(example->blur_pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, example->blur_pipeline->push_constant_size, &example->push_constants);
	zest_Draw(3, 1, 0, 0);
}

void AddVerticalBlur(zest_render_target_t *target, void *data) {
	RenderTargetExample *example = static_cast<RenderTargetExample*>(data);
	zest_BindPipeline(example->blur_pipeline, *zest_GetRenderTargetSourceDescriptorSet(target));
	example->push_constants.blur.x = 1.f;
	example->push_constants.blur.y = 1.f;
	example->push_constants.blur.z = 1.f;
	example->push_constants.texture_size.x = (float)target->render_width;
	example->push_constants.texture_size.y = (float)target->render_height;
	zest_SendPushConstants(example->blur_pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, example->blur_pipeline->push_constant_size, &example->push_constants);
	zest_Draw(3, 1, 0, 0);
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	RenderTargetExample *example = static_cast<RenderTargetExample*>(user_data);
	zest_SetActiveCommandQueue(example->command_queue);
	zest_Update2dUniformBuffer();
	example->base_layer->current_color = zest_ColorSet(255, 255, 255, 255);

	float delta = (float)elapsed / ZEST_MICROSECS_SECOND;

	zest_SetSpriteDrawing(example->base_layer, example->texture, 0, zest_Pipeline("pipeline_2d_sprites"));
	example->base_layer->multiply_blend_factor = 1.f;
	zest_DrawSprite(example->base_layer, example->image, zest_ScreenWidthf() * 0.5f, zest_ScreenHeightf() * 0.5f, 0.f, zest_ScreenWidthf(), zest_ScreenHeightf(), 0.5f, 0.5f, 0, 0.f, 0);
	if (example->wabbit_pos.x >= zest_ScreenWidthf()) example->wabbit_pos.vx *= -1.f;
	if (example->wabbit_pos.y >= zest_ScreenHeightf()) example->wabbit_pos.vy *= -1.f;
	if (example->wabbit_pos.x <= 0.f) example->wabbit_pos.vx *= -1.f;
	if (example->wabbit_pos.y <= 0.f) example->wabbit_pos.vy *= -1.f;
	example->wabbit_pos.x += example->wabbit_pos.vx * delta;
	example->wabbit_pos.y += example->wabbit_pos.vy * delta;
	zest_DrawSprite(example->base_layer, example->wabbit, example->wabbit_pos.x, example->wabbit_pos.y, 0.f, 200.f, 200.f, 0.5f, 0.5f, 0, 0.f, 0);

	zest_texture target_texture = zest_GetRenderTargetTexture(example->final_blur);
	float alpha = ZestApp->mouse_x / zest_ScreenWidthf();
	float blend = (ZestApp->mouse_y < 0 ? 0 : ZestApp->mouse_y) / zest_ScreenHeightf();
	blend = ZEST__CLAMP(blend, 0.f, 1.f);
	alpha = ZEST__CLAMP(alpha, 0.f, 5.f);

	example->top_layer->multiply_blend_factor = alpha;
	zest_SetLayerColor(example->top_layer, 255, 255, 255, (zest_byte)(blend * 255));
	zest_SetSpriteDrawing(example->top_layer, target_texture, 0, zest_Pipeline("pipeline_2d_sprites"));
	zest_DrawSprite(example->top_layer, zest_GetRenderTargetImage(example->final_blur), zest_ScreenWidthf() * 0.5f, zest_ScreenHeightf() * 0.5f, 0.f, zest_ScreenWidthf(), zest_ScreenHeightf(), 0.5f, 0.5f, 0, 0.f, 0);
	zest_SetMSDFFontDrawing(example->font_layer, example->font, example->font->descriptor_set, example->font->pipeline);
	zest_SetMSDFFontShadow(example->font_layer, 2.f, .25f, 1.f);
	zest_SetMSDFFontShadowColor(example->font_layer, 0.f, 0.f, 0.f, 0.75f);
	zest_SetLayerColor(example->font_layer, 200, 200, 200, 255);
	zest_DrawMSDFText(example->font_layer, "Mouse x = alpha level, Mouse y = additive/alpha blend", zest_ScreenWidth() * .5f, zest_ScreenHeightf() * .05f, .5f, .5f, 40.f, 0.f, 1.f);
}
#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
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