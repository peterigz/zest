#include <zest.h>
#include "zest-render-targets.h"

void InitExample(RenderTargetExample *example) {

	//Create a new pipeline, start with a freah zest_pipeline_template_create_info_t
	zest_pipeline_template_create_info_t create_info = zest_CreatePipelineTemplateCreateInfo();

	//Make a pipeline to handle the blur effect
	//Set up a push constance range with our custom PushConstants struct. We only need it in the frag shader
	VkPushConstantRange image_pushconstant_range;
	image_pushconstant_range.size = sizeof(PushConstants);
	image_pushconstant_range.offset = 0;
	image_pushconstant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	//Add the push constant range to the shader 
	zest_AddPipelineTemplatePushConstantRange(&create_info, image_pushconstant_range);
	//Set the vert and frag shaders for the blur effect
	zest_SetPipelineTemplateVertShader(&create_info, "examples/assets/spv/blur_vert.spv");
	zest_SetPipelineTemplateFragShader(&create_info, "examples/assets/spv/blur_frag.spv");
	//Theres no vertex data for the shader so set no_vertex_input to true
	create_info.no_vertex_input = true;
	//Add a new pipeline for the blur effect
	example->blur_pipeline = zest_AddPipeline("blur effect");
	//Make the pipeline template with the create_info we just set up and specify a standard render pass
	zest_MakePipelineTemplate(example->blur_pipeline, zest_GetStandardRenderPass(), &create_info);
	//Now the template is built we can tweak it a little bit more:
	example->blur_pipeline->pipeline_template.depthStencil.depthWriteEnable = false;
	example->blur_pipeline->pipeline_template.multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	//We're not using any uniform buffers
	example->blur_pipeline->uniforms = 0;
	//Let the pipeline know the size of the push constants
	example->blur_pipeline->push_constant_size = sizeof(PushConstants);
	//Build the pipeline so it's ready to use
	zest_BuildPipeline(example->blur_pipeline);

	//Create the render targets that we will draw the layers to. The Base render target will be where we draw the images, The top render target
	//will be where we draw the result of the the blur effect.
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
	example->command_queue = zest_NewCommandQueue("Screen Blur");
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
	//Process the images so the texture is ready to use
	zest_ProcessTextureImages(example->texture);
	//Load a font
	example->font = zest_LoadMSDFFont("examples/assets/SourceSansPro-Regular.zft");
	//Set an initial position for the rabbit.
	example->wabbit_pos.x = 10.f;
	example->wabbit_pos.y = 10.f;
	example->wabbit_pos.vx = 200.f;
	example->wabbit_pos.vy = 200.f;
}

void AddHorizontalBlur(zest_render_target_t *target, void *data) {
	//Grab the example struct from the user data set in the call to zest_AddPostProcessRenderTarget
	RenderTargetExample *example = static_cast<RenderTargetExample*>(data);
	//Bind our custom blur pipeline that we created
	zest_BindPipeline(example->blur_pipeline, *zest_GetRenderTargetSourceDescriptorSet(target));
	//Set the pusch constant to tell the shader to blur horizontally
	example->push_constants.blur.x = 1.f;
	example->push_constants.blur.y = 1.f;
	example->push_constants.blur.z = 0.f;
	//Set the texture size in the push constant, this will vary depending on the current blur stage
	example->push_constants.texture_size.x = (float)target->render_width;
	example->push_constants.texture_size.y = (float)target->render_height;
	//Send the push constants
	zest_SendPushConstants(example->blur_pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, example->blur_pipeline->push_constant_size, &example->push_constants);
	//Send a draw command. We just need to send 3 vertices to draw a triangle that covers the render target
	zest_Draw(3, 1, 0, 0);
}

void AddVerticalBlur(zest_render_target_t *target, void *data) {
	//Grab the example struct from the user data set in the call to zest_AddPostProcessRenderTarget
	RenderTargetExample *example = static_cast<RenderTargetExample*>(data);
	//Bind our custom blur pipeline that we created
	zest_BindPipeline(example->blur_pipeline, *zest_GetRenderTargetSourceDescriptorSet(target));
	//Set the pusch constant to tell the shader to blur vertically
	example->push_constants.blur.x = 1.f;
	example->push_constants.blur.y = 1.f;
	example->push_constants.blur.z = 1.f;
	//Set the texture size in the push constant, this will vary depending on the current blur stage
	example->push_constants.texture_size.x = (float)target->render_width;
	example->push_constants.texture_size.y = (float)target->render_height;
	//Send the push constants
	zest_SendPushConstants(example->blur_pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, example->blur_pipeline->push_constant_size, &example->push_constants);
	//Send a draw command. We just need to send 3 vertices to draw a triangle that covers the render target
	zest_Draw(3, 1, 0, 0);
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

	//Set the sprite drawing to use the texture with our images
	zest_SetSpriteDrawing(example->base_layer, example->texture, 0, zest_Pipeline("pipeline_2d_sprites"));
	//Set the layer intensity
	zest_SetLayerIntensity(example->base_layer, 1.f);
	//Draw the statue sprite to cover the screen
	zest_DrawSprite(example->base_layer, example->image, zest_ScreenWidthf() * 0.5f, zest_ScreenHeightf() * 0.5f, 0.f, zest_ScreenWidthf(), zest_ScreenHeightf(), 0.5f, 0.5f, 0, 0.f);
	//bounce the rabbit if it hits the screen edge
	if (example->wabbit_pos.x >= zest_ScreenWidthf()) example->wabbit_pos.vx *= -1.f;
	if (example->wabbit_pos.y >= zest_ScreenHeightf()) example->wabbit_pos.vy *= -1.f;
	if (example->wabbit_pos.x <= 0.f) example->wabbit_pos.vx *= -1.f;
	if (example->wabbit_pos.y <= 0.f) example->wabbit_pos.vy *= -1.f;
	//Update the rabbit position
	example->wabbit_pos.x += example->wabbit_pos.vx * delta;
	example->wabbit_pos.y += example->wabbit_pos.vy * delta;
	//Draw the rabbit sprite
	zest_DrawSprite(example->base_layer, example->wabbit, example->wabbit_pos.x, example->wabbit_pos.y, 0.f, 200.f, 200.f, 0.5f, 0.5f, 0, 0.f);

	//Get the texture from the final blur render target.
	zest_texture target_texture = zest_GetRenderTargetTexture(example->final_blur);
	//We can adjust the alpha and blend type based on the mouse position
	float top_layer_intensity = (float)ZestApp->mouse_x / zest_ScreenWidthf();
	float blend = ((float)ZestApp->mouse_y < 0 ? 0 : (float)ZestApp->mouse_y) / zest_ScreenHeightf();
	blend = ZEST__CLAMP(blend, 0.f, 1.f);
	top_layer_intensity = ZEST__CLAMP(top_layer_intensity, 0.f, 5.f);

	//Set the intensity of the top layer
	zest_SetLayerIntensity(example->top_layer, top_layer_intensity);
	//Set it's color and blend mix
	zest_SetLayerColor(example->top_layer, 255, 255, 255, (zest_byte)(blend * 255));
	//Set the sprite drawing for the top layer to use the final blur texture with the standard pipeline for sprites
	zest_SetSpriteDrawing(example->top_layer, target_texture, 0, zest_Pipeline("pipeline_2d_sprites"));
	//Draw the render target as a sprite to the top layer.
	zest_DrawSprite(example->top_layer, zest_GetRenderTargetImage(example->final_blur), zest_ScreenWidthf() * 0.5f, zest_ScreenHeightf() * 0.5f, 0.f, zest_ScreenWidthf(), zest_ScreenHeightf(), 0.5f, 0.5f, 0, 0.f);
	//Set the font to use for the font layer
	zest_SetMSDFFontDrawing(example->font_layer, example->font);
	//Set the shadow and color
	zest_SetMSDFFontShadow(example->font_layer, 2.f, .25f, 1.f);
	zest_SetMSDFFontShadowColor(example->font_layer, 0.f, 0.f, 0.f, 0.75f);
	zest_SetLayerColor(example->font_layer, 200, 200, 200, 255);
	//Draw the text
	zest_DrawMSDFText(example->font_layer, "Mouse x = intensity level, Mouse y = additive/alpha blend", zest_ScreenWidth() * .5f, zest_ScreenHeightf() * .05f, .5f, .5f, 40.f, 0.f);
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
int main()
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
