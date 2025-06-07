#include <zest.h>

typedef struct zest_example {
	zest_texture texture;						        //A handle to the texture that will contain the bunny image
	zest_image image;							        //A handle to image in the texture for the bunny image
	zest_pipeline_template sprite_pipeline;				//The builtin sprite pipeline that will drawing sprites
	zest_layer sprite_layer;					        //The builtin sprite layer that contains the vertex buffer for drawing the sprites
	zest_layer billboard_layer;					        //A builtin billboard layer for drawing billboards
	zest_pipeline_template billboard_pipeline;			//The pipeline for drawing billboards
	zest_camera_t camera;						        //A camera for the 3d view
	zest_uniform_buffer uniform_buffer_3d;		        //A uniform buffer to contain the projection and view matrix
	zest_vec3 last_position;
	zest_command_queue command_queue;
	zest_command_queue_draw_commands draw_commands;
	zest_shader billboard_vert_shader;
	zest_shader billboard_frag_shader;
	zest_render_graph render_graph;
	zest_shader_resources billboard_shader_resources;
	zest_shader_resources sprite_shader_resources;
} zest_example;

void UpdateUniformBuffer3d(zest_example *example) {
	//Update our 3d uniform buffer using the camera in the example
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(example->uniform_buffer_3d);
	buffer_3d->view = zest_LookAt(example->camera.position, zest_AddVec3(example->camera.position, example->camera.front), example->camera.up);
	buffer_3d->proj = zest_Perspective(example->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.1f, 10000.f);
	buffer_3d->proj.v[1].y *= -1.f;
	buffer_3d->screen_size.x = zest_ScreenWidthf();
	buffer_3d->screen_size.y = zest_ScreenHeightf();
	buffer_3d->millisecs = 0;
}

void InitExample(zest_example *example) {
	//Create a new texture for storing the bunny image
	example->texture = zest_CreateTexture("Example Texture", zest_texture_storage_type_bank, zest_texture_flag_use_filtering, zest_texture_format_rgba_srgb, 10);
	//Load in the bunny image from file and add it to the texture
	example->image = zest_AddTextureImageFile(example->texture, "examples/assets/wabbit_alpha.png");
	//Process the texture which will create the resources on the GPU for sampling from the bunny image texture
	zest_ProcessTextureImages(example->texture);
	//To save having to lookup these handles in the mainloop, we can look them up here in advance and store the handles in our example struct
	example->sprite_pipeline = zest_PipelineTemplate("pipeline_2d_sprites");
	//Create a new uniform buffer for the 3d view
	example->uniform_buffer_3d = zest_CreateUniformBuffer("3d uniform buffer", sizeof(zest_uniform_buffer_data_t));

	zest_AcquireGlobalCombinedImageSampler(example->texture);

	example->billboard_shader_resources = zest_CreateShaderResources();
	zest_AddUniformBufferToResources(example->billboard_shader_resources, example->uniform_buffer_3d);
	zest_AddGlobalBindlessSetToResources(example->billboard_shader_resources);

	example->sprite_shader_resources = zest_CreateShaderResources();
	zest_AddUniformBufferToResources(example->sprite_shader_resources, ZestRenderer->uniform_buffer);
	zest_AddGlobalBindlessSetToResources(example->sprite_shader_resources);

	//Create a camera for the 3d view
	example->camera = zest_CreateCamera();
	zest_CameraSetFoV(&example->camera, 60.f);

	//Create and compile the shaders for our custom sprite pipeline
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	example->billboard_frag_shader = zest_CreateShaderFromFile("examples/assets/shaders/billboard.frag", "billboard_frag.spv", shaderc_fragment_shader, true, compiler, 0);
	example->billboard_vert_shader = zest_CreateShaderFromFile("examples/assets/shaders/billboard.vert", "billboard_vert.spv", shaderc_vertex_shader,  true, compiler, 0);
	shaderc_compiler_release(compiler);

	//Create a pipeline that we can use to draw billboards
	example->billboard_pipeline = zest_CreatePipelineTemplate("pipeline_billboard");
	zest_AddVertexInputBindingDescription(example->billboard_pipeline, 0, sizeof(zest_billboard_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);

	zest_AddVertexInputDescription(example->billboard_pipeline, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(zest_billboard_instance_t, position)));			    // Location 0: Position
	zest_AddVertexInputDescription(example->billboard_pipeline, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R8G8B8_SNORM, offsetof(zest_billboard_instance_t, alignment)));		         	// Location 9: Alignment X, Y and Z
	zest_AddVertexInputDescription(example->billboard_pipeline, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_billboard_instance_t, rotations_stretch)));	// Location 2: Rotations + stretch
	zest_AddVertexInputDescription(example->billboard_pipeline, zest_CreateVertexInputDescription(0, 3, VK_FORMAT_R16G16B16A16_SNORM, offsetof(zest_billboard_instance_t, uv)));		    		// Location 1: uv_packed
	zest_AddVertexInputDescription(example->billboard_pipeline, zest_CreateVertexInputDescription(0, 4, VK_FORMAT_R16G16B16A16_SSCALED, offsetof(zest_billboard_instance_t, scale_handle)));		// Location 4: Scale + Handle
	zest_AddVertexInputDescription(example->billboard_pipeline, zest_CreateVertexInputDescription(0, 5, VK_FORMAT_R32_UINT, offsetof(zest_billboard_instance_t, intensity_texture_array)));		// Location 6: texture array index * intensity
	zest_AddVertexInputDescription(example->billboard_pipeline, zest_CreateVertexInputDescription(0, 6, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_billboard_instance_t, color)));			        // Location 7: Instance Color

    zest_SetPipelineTemplatePushConstantRange(example->billboard_pipeline, sizeof(zest_push_constants_t), 0, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
	zest_SetPipelineTemplateVertShader(example->billboard_pipeline, "billboard_vert.spv", "spv/");
	zest_SetPipelineTemplateFragShader(example->billboard_pipeline, "billboard_frag.spv", "spv/");
	zest_AddPipelineTemplateDescriptorLayout(example->billboard_pipeline, zest_GetUniformBufferLayout(example->uniform_buffer_3d));
	zest_AddPipelineTemplateDescriptorLayout(example->billboard_pipeline, zest_vk_GetGlobalBindlessLayout());
	zest_FinalisePipelineTemplate(example->billboard_pipeline);
	example->billboard_pipeline->depthStencil.depthWriteEnable = VK_FALSE;
	example->billboard_pipeline->depthStencil.depthTestEnable = VK_TRUE;
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Billboard pipeline");

	example->billboard_layer = zest_CreateInstanceLayer("billboards", sizeof(zest_billboard_instance_t));
	example->sprite_layer = zest_CreateInstanceLayer("sprites", sizeof(zest_sprite_instance_t));

	example->render_graph = zest_NewRenderGraph("Instance sprites example", zest_GetGlobalBindlessLayout(), ZEST_TRUE);
}

void test_update_callback(zest_microsecs elapsed, void *user_data) {
	//Get the example struct from the user data we set with zest_SetUserData
	zest_example *example = (zest_example*)user_data;
	//Update the builtin 2d uniform buffer. 
	zest_Update2dUniformBuffer();
	//Also update the 3d uniform buffer for the billboard drawing
	UpdateUniformBuffer3d(example);

	//Set the current intensity of the sprite layer
	//example->sprite_layer->intensity = 1.f;
	//You must call this command before doing any sprite draw to set the current texture, descriptor and pipeline to draw with.
	//Call this function anytime that you need to draw with a different texture. Note that a single texture and contain many images
	//you can draw a lot with a single draw call
	//zest_SetLayerViewPort(example->sprite_layer, 0, 0, 1280, 768, 1280.f, 768.f);
	//zest_SetLayerDrawingViewport(example->sprite_layer, 0, 0, 1280, 768, 1280.f, 768.f);
	zest_SetInstanceDrawing(example->sprite_layer, example->sprite_shader_resources, example->texture, example->sprite_pipeline);
	//Set the alpha of the sprite layer to 0. This means that the sprites will be additive. 1 = alpha blending and anything imbetween
	//is a mix between the two.
	example->sprite_layer->current_color.a = 0;
	for (float x = 0; x != 25; ++x) {
		for (float y = 0; y != 15; ++y) {
			example->sprite_layer->current_color.r = (zest_byte)(1 - ((y + 1) / 16.f) * 255.f);
			example->sprite_layer->current_color.g = (zest_byte)(y / 15.f * 255.f);
			example->sprite_layer->current_color.b = (zest_byte)(x / 75.f * 255.f);
			//Draw a sprite to the screen.
			zest_DrawSprite(example->sprite_layer, example->image, x * 16.f + 20.f, y * 40.f + 20.f, 0.f, 32.f, 32.f, 0.5f, 0.5f, 0, 0.f);
		}
	}

	//Draw a textured sprite to the screen (basically a textured rect). 
	zest_DrawTexturedSprite(example->sprite_layer, example->image, 600.f, 100.f, 500.f, 500.f, 1.f, 1.f, 0.f, 0.f);

	//Now lets draw a billboard. Similar to the sprite, we must call this command before any billboard drawing.
	zest_SetInstanceDrawing(example->billboard_layer, example->billboard_shader_resources, example->texture, example->billboard_pipeline);
	//Get a pointer to the uniform buffer data and cast it to the struct that we're storing there
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(example->uniform_buffer_3d);
	//Use the projection and view matrices in the buffer to project the mouse coordinates into 3d space.
	zest_vec3 ray = zest_ScreenRay((float)ZestApp->mouse_x, (float)ZestApp->mouse_y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	//Scale the ray into the 3d space by 10 and add the camera position.
	zest_vec3 position = zest_AddVec3(zest_ScaleVec3(ray, 10.f), example->camera.position);
	//Set some values to draw the billboard with
	zest_vec3 angles = { 0 };
	zest_vec3 handle = { .5f, .5f };
	float scale_x = (float)ZestApp->mouse_x * 5.f / zest_ScreenWidthf();
	float scale_y = (float)ZestApp->mouse_y * 5.f / zest_ScreenHeightf();
	//Draw the billboard
	zest_DrawBillboardSimple(example->billboard_layer, example->image, &position.x, angles.x, scale_x, scale_y);
	example->last_position = position;

	//Create the render graph
	if (zest_BeginRenderToScreen(example->render_graph)) {
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };

		//Add resources
		zest_resource_node swapchain_output_resource = zest_ImportSwapChainResource(example->render_graph, "Swapchain Output");
		zest_resource_node texture = zest_ImportImageResourceReadOnly(example->render_graph, "Bunny Texture", example->texture);
		zest_resource_node billboard_layer = zest_AddInstanceLayerBufferResource(example->render_graph, example->billboard_layer);
		zest_resource_node sprite_layer = zest_AddInstanceLayerBufferResource(example->render_graph, example->sprite_layer);

		//Add passes
		zest_pass_node graphics_pass = zest_AddRenderPassNode(example->render_graph, "Graphics Pass");
		zest_pass_node upload_instance_data = zest_AddTransferPassNode(example->render_graph, "Upload Instance Data");

		//Connect buffers and textures
		zest_ConnectTransferBufferOutput(upload_instance_data, billboard_layer);
		zest_ConnectTransferBufferOutput(upload_instance_data, sprite_layer);
		zest_ConnectVertexBufferInput(graphics_pass, billboard_layer);
		zest_ConnectVertexBufferInput(graphics_pass, sprite_layer);
		zest_ConnectSampledImageInput(graphics_pass, texture, zest_fragment_stage);
		zest_ConnectSwapChainOutput(graphics_pass, swapchain_output_resource, clear_color);

		//Add the tasks to run for the passes
		zest_AddPassTask(upload_instance_data , zest_UploadInstanceLayerData, example->billboard_layer);
		zest_AddPassTask(upload_instance_data , zest_UploadInstanceLayerData, example->sprite_layer);
		zest_AddPassTask(graphics_pass, zest_DrawInstanceLayer, example->billboard_layer);
		zest_AddPassTask(graphics_pass, zest_DrawInstanceLayer, example->sprite_layer);
	}
	zest_EndRenderGraph(example->render_graph);

	//Print the render graph
	static bool print_render_graph = true;
	if (print_render_graph) {
		zest_PrintCompiledRenderGraph(example->render_graph);
		print_render_graph = false;
	}

	//Execute the render graph
	zest_ExecuteRenderGraph(example->render_graph);
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
int main(void)
{
	zest_example example = { 0 };

	zest_uint test = zest_Pack8bit(0, 1.f, 0);

	//zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(zest_validation_flag_enable_sync);
	zest_create_info_t create_info = zest_CreateInfo();
	//ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	create_info.color_format = VK_FORMAT_B8G8R8A8_SRGB;
	create_info.thread_count = 0;
	ZEST__FLAG(create_info.flags, zest_init_flag_use_depth_buffer);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	create_info.log_path = "./";

	zest_Initialise(&create_info);
	zest_LogFPSToConsole(1);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(test_update_callback);

	InitExample(&example);

	zest_Start();

	return 0;
}
#else
int main(void) {
	zest_example example = { 0 };

	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);

	zest_Initialise(&create_info);
	zest_LogFPSToConsole(1);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(test_update_callback);

	InitExample(&example);

	zest_Start();

	return 0;
}
#endif
