#include <zest.h>
#include "render_targets.h"

void InitExample(RenderTargetExample *example) {

	//QulkanRenderTarget &base_target = GetCurrentRenderTarget();
	//base_target.HideTargetFromRender();

	zest_pipeline_template_create_info create_info = zest_CreatePipelineTemplateCreateInfo();

	VkPushConstantRange image_pushconstant_range;
	image_pushconstant_range.size = sizeof(zest_push_constants);
	image_pushconstant_range.offset = 0;
	image_pushconstant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	zest_AddPipelineTemplatePushConstantRange(&create_info, image_pushconstant_range);
	create_info.vertShaderFile = "spv/blur_vert.spv";
	create_info.fragShaderFile = "spv/blur_frag.spv";
	create_info.viewport.extent = zest_GetSwapChainExtent();
	create_info.no_vertex_input = true;
	example->blur_pipeline_index = zest_AddPipeline("blur effect");
	zest_pipeline_set *blur_pipeline = zest_Pipeline(example->blur_pipeline_index);
	zest_MakePipelineTemplate(blur_pipeline, zest_GetStandardRenderPass(), &create_info);
	blur_pipeline->pipeline_template.depthStencil.depthWriteEnable = false;
	blur_pipeline->pipeline_template.multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	blur_pipeline->uniforms = 0;
	blur_pipeline->push_constant_size = sizeof(PushConstants);
	zest_BuildPipeline(blur_pipeline);

	//Create the render targets
	example->top_target_index = zest_CreateRenderTarget("Top render target");
	example->base_target_index = zest_CreateRenderTarget("Base render target");
	//Post render targets can be created by passing the width and height as a ratio of the screen size, which is what we do here 
	//to create the render targets for the blur effect. One target is for the vertical blur and the other is for the horizontal blur.
	zest_index vertical_blur_index = zest_AddPostProcessRenderTarget("Vertical blur render target", 0.0625, 0.0625, example->base_target_index, example, AddVerticalBlur);
	example->final_blur_index = zest_AddPostProcessRenderTarget("Horizontal blur render target", 0.0625, 0.0625, vertical_blur_index, example, AddHorizontalBlur);

	//Create the render queue
	//For blur effect we need to draw the scene to a base render target first, then have 2 render passes that draw to a smaller texture with horizontal and then vertical blur effects
	//then we can draw the base target to the swap chain and draw the blur texture over the top by drawing it as a textured rectangle on a top layer that doesn't get blurred.

	//First create a queue and set it's dependency to the present queue. This means that it will wait on the swap chain to present the final render to the screen before rendering again.
	//But bear in mind that there are multiple frames in flight (as long as ZEST_MAX_FIF is >1) so while one queue is being executed on the gpu the next one will be created in the meantime.
	//The best explaination I've seen of this can be found here: https://software.intel.com/content/www/us/en/develop/articles/practical-approach-to-vulkan-part-1.html
	example->render_queue_index = zest_NewCommandQueue("Screen Blur", zest_command_queue_flag_present_dependency);
	{
		//Create draw commands that render to a base render target
		//Note: all draw commands will happen in the order that they're added here as they are a part of the same command buffer in "Screen Blur" command queue
		zest_NewDrawCommandSetup("Base render pass", example->base_target_index);
		{
			//base target needs a layer that we can use to draw to so we just use a built in one but you could use
			//your own custom layer and draw routine
			example->base_layer_index = zest_NewBuiltinLayerSetup("Base Layer", zest_builtin_layer_sprites);
		}
		//Create draw commands that applies vertical blur to the base target
		zest_NewDrawCommandSetup("Vertical blur render pass", vertical_blur_index);
		{
			//Use a draw function that renders to the whole texture
			zest_SetDrawCommandsCallback(zest_DrawToRenderTargetCallback);
		}
		//Create draw commands that applies horizontal blur to the vertical blur target
		zest_NewDrawCommandSetup("Horizontal blur render pass", example->final_blur_index);
		{
			//Use a draw function that renders to the whole texture
			zest_SetDrawCommandsCallback(zest_DrawToRenderTargetCallback);
		}
		//Create draw commands that we can use to draw on top of the blur effect
		zest_NewDrawCommandSetup("Top render pass", example->top_target_index);
		{
			//Create a sprite layer to draw to it
			example->top_target_layer_index = zest_NewBuiltinLayerSetup("Top Layer", zest_builtin_layer_sprites);
		}
		//Finally we won't see anything unless we tell the render queue to render to the swap chain to be presented to the screen, but we
		//need to specify which render targets we want to be drawn to the swap chain.
		//We can use zest_NewDrawCommandSetupRenderTargetSwap which sets up a render pass to the swap chain specifying the render target to draw to it
		zest_NewDrawCommandSetupRenderTargetSwap("Render render targets to swap", example->base_target_index);
		{
			//We can add as many other render targets as we need to get drawn to the swap chain. In this case we'll add the top target where we can
			//draw a textured rectangle that samples the blurred texture.
			zest_AddRenderTarget(example->top_target_index);
		}
		//Connect the render queue to the Present queue so that the swap chain has to wait until the rendering is complete before
		//presenting to the screen
		zest_ConnectQueueToPresent();
		zest_FinishQueueSetup();
	}

	example->texture_index = zest_CreateTexturePacked("Statue texture");
	example->image_index = zest_AddTextureImageFile(zest_GetTextureByIndex(example->texture_index), "images/texture.jpg");
	zest_ProcessTextureImages(zest_GetTextureByIndex(example->texture_index));
}

void AddHorizontalBlur(zest_render_target *target, void *data) {
	RenderTargetExample *example = static_cast<RenderTargetExample*>(data);
	zest_pipeline_set *pipeline = zest_Pipeline(example->blur_pipeline_index);
	zest_BindPipeline(pipeline, *zest_GetRenderTargetSourceDescriptorSet(target));
	example->push_constants.blur.x = 1.f;
	example->push_constants.blur.y = 1.f;
	example->push_constants.blur.z = 0.f;
	example->push_constants.texture_size.x = (float)target->render_width;
	example->push_constants.texture_size.y = (float)target->render_height;
	zest_SendPushConstants(pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, pipeline->push_constant_size, &example->push_constants);
	zest_Draw(3, 1, 0, 0);
}

void AddVerticalBlur(zest_render_target *target, void *data) {
	RenderTargetExample *example = static_cast<RenderTargetExample*>(data);
	zest_pipeline_set *pipeline = zest_Pipeline(example->blur_pipeline_index);
	zest_BindPipeline(pipeline, *zest_GetRenderTargetSourceDescriptorSet(target));
	example->push_constants.blur.x = 1.f;
	example->push_constants.blur.y = 1.f;
	example->push_constants.blur.z = 1.f;
	example->push_constants.texture_size.x = (float)target->render_width;
	example->push_constants.texture_size.y = (float)target->render_height;
	zest_SendPushConstants(pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, pipeline->push_constant_size, &example->push_constants);
	zest_Draw(3, 1, 0, 0);
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	RenderTargetExample *example = static_cast<RenderTargetExample*>(user_data);
	zest_SetActiveRenderQueue(example->render_queue_index);
	zest_UpdateStandardUniformBuffer();
	zest_instance_layer *base_layer = zest_GetInstanceLayerByIndex(example->base_layer_index);
	zest_instance_layer *top_layer = zest_GetInstanceLayerByIndex(example->top_target_layer_index);
	base_layer->current_color = zest_ColorSet(255, 255, 255, 255);
	zest_texture *texture = zest_GetTextureByIndex(example->texture_index);

	zest_SetSpriteDrawing(base_layer, texture, 0, zest_PipelineIndex("pipeline_2d_sprites"));
	base_layer->multiply_blend_factor = 1.f;
	zest_DrawSprite(base_layer, zest_GetImageFromTexture(texture, example->image_index), zest_ScreenWidthf() * 0.5f, zest_ScreenHeightf() * 0.5f, 0.f, zest_ScreenWidthf(), zest_ScreenHeightf(), 0.5f, 0.5f, 0, 0.f, 0);

	zest_texture *target_texture = zest_GetRenderTargetTexture(zest_GetRenderTargetByIndex(example->final_blur_index));
	//base_layer.DrawImage(GetRenderTarget(example->hb_target_index).GetRenderTargetImage(), fMouseX(), fMouseY());
	//top_layer.SetBlendType(BlendType_pre_multiply);
	//float alpha = fMouseX() / fScreenWidth();
	float alpha = 1.f;
	if (alpha > 1.f) alpha = 1.f;
	if (alpha < 0) alpha = 0;
	top_layer->multiply_blend_factor = alpha;
	top_layer->current_color = zest_ColorSet(255, 255, 255, 255);
	zest_SetSpriteDrawing(top_layer, target_texture, 0, zest_PipelineIndex("pipeline_2d_sprites"));
	zest_DrawSprite(top_layer, zest_GetRenderTargetImage(zest_GetRenderTargetByIndex(example->final_blur_index)), zest_ScreenWidthf() * 0.5f, zest_ScreenHeightf() * 0.5f, 0.f, zest_ScreenWidthf(), zest_ScreenHeightf(), 0.5f, 0.5f, 0, 0.f, 0);
	//top_layer->SetColor(255, 255, 255, MouseDown(App.Mouse.LeftButton) ? 150 : 255);
	//top_layer->DrawTexturedRect(GetRenderTarget(example->final_blur_index).GetRenderTargetImage(), 0.f, 0.f, fScreenWidth(), fScreenHeight(), true, QVec2(1.f, 1.f), QVec2(0.f, 0.f));
}

int main(void) {

	zest_create_info create_info = zest_CreateInfo();
	zest_Initialise(&create_info);

	RenderTargetExample example;
	InitExample(&example);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(UpdateCallback);

	zest_Start();

	return 0;
}