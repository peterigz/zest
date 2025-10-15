#include "zest-custom-sprite-instance.h"
#include "imgui_internal.h"
#include <string>

void InitImGuiApp(ImGuiApp *app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise();
	//Implement a dark style
	zest_imgui_DarkStyle();

	//This is an exmaple of how to change the font that ImGui uses
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.Fonts->Clear();
	float font_size = 17.f;
	unsigned char *font_data;
	int tex_width, tex_height;
	ImFontConfig config;
	config.PixelSnapH = true;
	io.Fonts->AddFontFromFileTTF("examples/assets/consola.ttf", font_size);
	io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

	//Rebuild the Zest font texture
	zest_imgui_RebuildFontTexture(tex_width, tex_height, font_data);

	//Create a texture to load in a test image to show drawing that image in an imgui window
	app->test_texture = zest_CreateTexture("test image", zest_texture_storage_type_sprite_sheet, zest_image_flag_use_filtering, zest_format_r8g8b8a8_unorm, 10);
	//Load in the image and add it to the texture
	app->test_image = zest_AddTextureImageFile(app->test_texture, "examples/assets/glow.png");
	//Process the texture so that its ready to be used
	zest_ProcessTextureImages(app->test_texture);

	app->color_ramps_texture = zest_CreateTexture("Color Ramps", zest_texture_storage_type_bank, 0, zest_format_r8g8b8a8_unorm, 10);
	app->color_ramps_texture->vk_sampler_info.minFilter = VK_FILTER_NEAREST;
	app->color_ramps_texture->vk_sampler_info.magFilter = VK_FILTER_NEAREST;
	app->color_ramps_texture->vk_sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	app->color_ramps_bitmap = zest_NewBitmap();
	zest_vec4 start_color1 = {255.f, 255.f, 0.f, 255.f};
	zest_vec4 end_color1 = {0.f, 255.f, 255.f, 255.f};
	zest_vec4 start_color2 = {0.f, 255.f, 0.f, 255.f};
	zest_vec4 end_color2 = {255.f, 0.f, 255.f, 255.f};
	zest_AllocateBitmap(app->color_ramps_bitmap, 256, 256, 4, { 0 });
	for (int x = 0; x != 256; ++x) {
		float lerp = (float)x / 255.f;
		zest_vec4 color1 = zest_LerpVec4(&start_color1, &end_color1, lerp);
		zest_vec4 color2 = zest_LerpVec4(&start_color2, &end_color2, lerp);
		zest_PlotBitmap(app->color_ramps_bitmap, x, 0, { (zest_byte)color1.x, (zest_byte)color1.y, (zest_byte)color1.z, (zest_byte)color1.w});
		zest_PlotBitmap(app->color_ramps_bitmap, x, 1, { (zest_byte)color2.x, (zest_byte)color2.y, (zest_byte)color2.z, (zest_byte)color2.w});
	}
	app->color_ramps_image = zest_AddTextureImageBitmap(app->color_ramps_texture, app->color_ramps_bitmap);
	zest_ProcessTextureImages(app->color_ramps_texture);
	zest_FreeBitmap(app->color_ramps_bitmap);

	app->lerp_value = 0.f;
	app->mix_value = 0.f;

	//Create and compile the shaders for our custom sprite pipeline
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	app->custom_frag_shader = zest_CreateShader(custom_frag_shader, shaderc_fragment_shader, "custom_sprite_frag.spv", true, true, compiler, 0);
	app->custom_vert_shader = zest_CreateShader(custom_vert_shader, shaderc_vertex_shader, "custom_sprite_vert.spv", true, true, compiler, 0);
	shaderc_compiler_release(compiler);

	//Create a new pipeline using our custom sprite struct for the binding descriptions
	//Start with creating a fresh pipeline template
	app->custom_pipeline = zest_BeginPipelineTemplate("Custom sprite pipeline");

	zest_AddPipelineDescriptorLayout(app->custom_pipeline, context->renderer->uniform_buffer->set_layout->vk_layout);
	zest_AddPipelineDescriptorLayout(app->custom_pipeline, zest_vk_GetGlobalBindlessLayout());
	//Add a vertex input binding description specifying the size of the custom sprite instance struct
	zest_AddVertexInputBindingDescription(app->custom_pipeline, 0, sizeof(zest_custom_sprite_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);
	//Add each input description to bind the layout in the shader to the offset in the custom sprite instance struct.
	zest_AddVertexAttribute(app->custom_pipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_custom_sprite_instance_t, position_rotation));	// Location 0: Postion, rotation and stretch
	zest_AddVertexAttribute(app->custom_pipeline, 0, 1, VK_FORMAT_R16G16B16A16_SSCALED, offsetof(zest_custom_sprite_instance_t, size_handle));			// Location 1: Size and handle of the sprite
	zest_AddVertexAttribute(app->custom_pipeline, 0, 2, VK_FORMAT_R16G16B16A16_SNORM, offsetof(zest_custom_sprite_instance_t, uv));					// Location 2: Instance Position and rotation
	zest_AddVertexAttribute(app->custom_pipeline, 0, 3, VK_FORMAT_R16G16_SNORM, offsetof(zest_custom_sprite_instance_t, alignment));					// Location 3: Alignment
	zest_AddVertexAttribute(app->custom_pipeline, 0, 4, VK_FORMAT_R16G16_SSCALED, offsetof(zest_custom_sprite_instance_t, intensity));					// Location 4: 2 intensities for each color
	zest_AddVertexAttribute(app->custom_pipeline, 0, 5, VK_FORMAT_R16G16_SNORM, offsetof(zest_custom_sprite_instance_t, lerp_values));					// Location 5: Interpolation values for mixing and sampling the colors
	zest_AddVertexAttribute(app->custom_pipeline, 0, 6, VK_FORMAT_R32_UINT, offsetof(zest_custom_sprite_instance_t, texture_indexes)); 				// Location 6: texture indexes to sample the correct texture and 2 color rampls

	//Specify the custom shaders we made
	zest_SetPipelineVertShader(app->custom_pipeline, "custom_sprite_vert.spv", 0);
	zest_SetPipelineFragShader(app->custom_pipeline, "custom_sprite_frag.spv", 0);
	//We don't need a push constant for this example but we can set the push constants using the standard zest push constant struct
	zest_SetPipelinePushConstantRange(app->custom_pipeline, sizeof(zest_push_constants_t), zest_shader_render_stages);

	zest_SetPipelineBlend(app->custom_pipeline, zest_PreMultiplyBlendState());
	zest_SetPipelineDepthTest(app->custom_pipeline, false, false);

	//Finalise the pipeline template ready for building the pipeline
	zest_EndPipelineTemplate(app->custom_pipeline);
	//------------------------------------------

	app->layer_data = {};

	app->custom_layer = zest_CreateInstanceLayer("Custom layer", sizeof(zest_custom_sprite_instance_t));
	zest_SetLayerUserData(app->custom_layer, &app->layer_data);

	app->shader_resources = zest_CreateShaderResources("Sprite resources");
	zest_AddUniformBufferToResources(app->shader_resources, context->renderer->uniform_buffer);
	zest_AddGlobalBindlessSetToResources(app->shader_resources);

	zest_AcquireGlobalCombinedImageSampler(app->test_texture);
	zest_AcquireGlobalCombinedImageSampler(app->color_ramps_texture);
}

void zest_DrawCustomSprite(zest_layer layer, zest_atlas_region image, float x, float y, float r, float sx, float sy, float hx, float hy, zest_uint alignment, float stretch, zest_vec2 lerp_values) {
	zest_custom_sprite_instance_t *sprite = zest_GetLayerInstance(zest_custom_sprite_instance_t, layer, context->current_fif);

	sprite->size_handle = zest_Pack16bit4SScaled(sx, sy, hx, hy, 4096.f, 128.f);
	sprite->position_rotation = zest_Vec4Set(x, y, stretch, r);
	sprite->uv = image->uv_packed;
	sprite->alignment = alignment;
	sprite->lerp_values = zest_Pack16bit2SNorm(lerp_values.x, lerp_values.y);
	sprite->texture_indexes = (image->layer << 24) | (0 << 16) | (1 << 8) | 0;
	sprite->intensity = zest_Pack16bit2SScaled(layer->intensity, zest_GetLayerUserData(extra_layer_data, layer)->intensity2, 128.f);

	zest_NextInstance(layer);
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	//Don't forget to update the uniform buffer!
	zest_Update2dUniformBuffer();
	//Set the active command queue to the default one that was created when Zest was initialised
	ImGuiApp *app = (ImGuiApp *)user_data;

	//Must call the imgui GLFW implementation function
	ImGui_ImplGlfw_NewFrame();
	//Draw our imgui stuff
	ImGui::NewFrame();
	ImGui::Begin("Test Window");
	ImGui::SliderFloat("Mix", &app->mix_value, 0.f, 1.f);
	ImGui::SliderFloat("Interpolation", &app->lerp_value, 0.f, 1.f);
	ImGui::SliderFloat("Intensity 1", &app->custom_layer->intensity, 0.f, 10.f);
	ImGui::SliderFloat("Intensity 2", &zest_GetLayerUserData(extra_layer_data, app->custom_layer)->intensity2, 0.f, 10.f);
	ImGui::End();
	ImGui::Render();
	//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
	zest_imgui_UpdateBuffers();

	//Set the pipeline and descriptor set to use in the layer for our custom sprite drawing
	zest_SetInstanceDrawing(app->custom_layer, app->shader_resources, app->custom_pipeline);
	zest_push_constants_t *push = (zest_push_constants_t*)app->custom_layer->current_instruction.push_constant;
	push->descriptor_index[0] = zest_GetTextureDescriptorIndex(app->test_texture);
	push->descriptor_index[1] = zest_GetTextureDescriptorIndex(app->color_ramps_texture);
	zest_DrawCustomSprite(app->custom_layer, app->test_image, 800.f, 400.f, 0.f, 256.f, 256.f, .5f, .5f, 0, 0.f, {app->lerp_value, app->mix_value});

	zest_swapchain swapchain = zest_GetMainWindowSwapchain();
	if (zest_BeginFrameGraphSwapchain(swapchain, "Custom Sprite Render Graph", 0)) {
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };

		//Add resources
		zest_resource_node texture = zest_ImportImageResource("Test Texture", app->test_texture, 0);
		zest_resource_node billboard_layer = zest_AddTransientLayerResource("Billboard layer", app->custom_layer, false);

		//---------------------------------Transfer Pass----------------------------------------------------
		zest_pass_node upload_instance_data = zest_BeginTransferPass("Upload Instance Data");
		//Outputs
		zest_ConnectOutput(upload_instance_data, billboard_layer);
		//Tasks
		zest_SetPassTask(upload_instance_data, zest_UploadInstanceLayerData, app->custom_layer);
		//--------------------------------------------------------------------------------------------------

		//---------------------------------Render Pass------------------------------------------------------
		zest_pass_node graphics_pass = zest_BeginRenderPass("Graphics Pass");
		//Inputs
		zest_ConnectInput(graphics_pass, billboard_layer, 0);
		zest_ConnectInput(graphics_pass, texture, 0);
		//Outputs
		zest_ConnectSwapChainOutput(graphics_pass);
		//Tasks
		zest_SetPassTask(graphics_pass, zest_cmd_DrawInstanceLayer, app->custom_layer);
		//--------------------------------------------------------------------------------------------------

		//------------------------ ImGui Pass ----------------------------------------------------------------
		//If there's imgui to draw then draw it
		zest_pass_node imgui_pass = zest_imgui_BeginPass();
		if (imgui_pass) {
			zest_ConnectSwapChainOutput(imgui_pass);
		}
		//----------------------------------------------------------------------------------------------------

		zest_EndFrameGraph();
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(0);
	//Don't enable vsync so we can see the FPS go higher then the refresh rate
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	//ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	create_info.log_path = "./";
	//Implement GLFW for window creation
	zest_implglfw_SetCallbacks(&create_info);

	ImGuiApp imgui_app;

	//Initialise Zest
	zest_CreateContext(&create_info);
	//Set the Zest use data
	zest_SetUserData(&imgui_app);
	//Set the udpate callback to be called every frame
	zest_SetUserUpdateCallback(UpdateCallback);
	//Initialise our example
	InitImGuiApp(&imgui_app);

	//Start the main loop
	zest_Start();
	zest_DestroyContext();

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	zest_implglfw_SetCallbacks(&create_info);
	ZEST__FLAG(create_info.flags, zest_init_flag_maximised);

	ImGuiApp imgui_app;

	create_info.log_path = ".";
	zest_CreateContext(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#endif
