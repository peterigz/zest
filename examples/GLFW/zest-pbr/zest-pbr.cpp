#include "zest-pbr.h"
#include "imgui_internal.h"

void UpdateUniform3d(ImGuiApp *app) {
	uniform_buffer_data_t *ubo_ptr = static_cast<uniform_buffer_data_t *>(zest_GetUniformBufferData(app->view_buffer));
	ubo_ptr->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	ubo_ptr->proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.001f, 10000.f);
	ubo_ptr->proj.v[1].y *= -1.f;
	ubo_ptr->screen_size.x = zest_ScreenWidthf();
	ubo_ptr->screen_size.y = zest_ScreenHeightf();
}

void SetupBillboards(ImGuiApp *app) {
	//Create and compile the shaders for our custom sprite pipeline
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	zest_CreateShaderFromFile("examples/assets/shaders/billboard.frag", "billboard_frag.spv", shaderc_fragment_shader, true, compiler, 0);
	zest_CreateShaderFromFile("examples/assets/shaders/billboard.vert", "billboard_vert.spv", shaderc_vertex_shader, true, compiler, 0);
	shaderc_compiler_release(compiler);

	//Create a pipeline that we can use to draw billboards
	app->billboard_pipeline = zest_BeginPipelineTemplate("pipeline_billboard");
	zest_AddVertexInputBindingDescription(app->billboard_pipeline, 0, sizeof(zest_billboard_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);

	zest_AddVertexAttribute(app->billboard_pipeline, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(zest_billboard_instance_t, position));			    // Location 0: Position
	zest_AddVertexAttribute(app->billboard_pipeline, 1, VK_FORMAT_R8G8B8_SNORM, offsetof(zest_billboard_instance_t, alignment));		         	// Location 9: Alignment X, Y and Z
	zest_AddVertexAttribute(app->billboard_pipeline, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_billboard_instance_t, rotations_stretch));	// Location 2: Rotations + stretch
	zest_AddVertexAttribute(app->billboard_pipeline, 3, VK_FORMAT_R16G16B16A16_SNORM, offsetof(zest_billboard_instance_t, uv));		    		// Location 1: uv_packed
	zest_AddVertexAttribute(app->billboard_pipeline, 4, VK_FORMAT_R16G16B16A16_SSCALED, offsetof(zest_billboard_instance_t, scale_handle));		// Location 4: Scale + Handle
	zest_AddVertexAttribute(app->billboard_pipeline, 5, VK_FORMAT_R32_UINT, offsetof(zest_billboard_instance_t, intensity_texture_array));		// Location 6: texture array index * intensity
	zest_AddVertexAttribute(app->billboard_pipeline, 6, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_billboard_instance_t, color));			        // Location 7: Instance Color

	zest_SetPipelinePushConstantRange(app->billboard_pipeline, sizeof(billboard_push_constant_t), zest_shader_render_stages);
	zest_SetPipelineVertShader(app->billboard_pipeline, "billboard_vert.spv", "spv/");
	zest_SetPipelineFragShader(app->billboard_pipeline, "billboard_frag.spv", "spv/");
	zest_AddPipelineDescriptorLayout(app->billboard_pipeline, zest_vk_GetUniformBufferLayout(app->view_buffer));
	zest_AddPipelineDescriptorLayout(app->billboard_pipeline, zest_vk_GetGlobalBindlessLayout());
	zest_SetPipelineDepthTest(app->billboard_pipeline, false, false);
	zest_EndPipelineTemplate(app->billboard_pipeline);

	app->sprites_texture = zest_CreateTexturePacked("Sprites Texture", zest_texture_format_rgba_unorm);
	app->light = zest_AddTextureImageFile(app->sprites_texture, "examples/assets/glow.png");
	zest_ProcessTextureImages(app->sprites_texture);

	zest_AcquireGlobalCombinedImageSampler(app->sprites_texture);
	app->billboard_push.texture_index = zest_GetTextureDescriptorIndex(app->sprites_texture);
	app->billboard_layer = zest_CreateInstanceLayer("billboards", sizeof(zest_billboard_instance_t));

	app->sprite_resources = zest_CreateShaderResources("Sprite resources");
	zest_AddUniformBufferToResources(app->sprite_resources, app->view_buffer);
	zest_AddGlobalBindlessSetToResources(app->sprite_resources);
}

void InitImGuiApp(ImGuiApp *app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise();
	//Implement a dark style
	zest_imgui_DarkStyle();
	
	//This is an exmaple of how to change the font that ImGui uses
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.Fonts->Clear();
	float font_size = 16.f;
	unsigned char *font_data;
	int tex_width, tex_height;
	ImFontConfig config;
	config.PixelSnapH = true;
	io.Fonts->AddFontFromFileTTF("examples/assets/Lato-Regular.ttf", font_size);
	io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

	//Rebuild the Zest font texture
	zest_imgui_RebuildFontTexture(tex_width, tex_height, font_data);

	app->camera = zest_CreateCamera();
	zest_CameraPosition(&app->camera, { -2.5f, 0.f, 0.f });
	zest_CameraSetFoV(&app->camera, 60.f);
	zest_CameraSetYaw(&app->camera, zest_Radians(-90.f));
	zest_CameraSetPitch(&app->camera, zest_Radians(0.f));
	zest_CameraUpdateFront(&app->camera);

	//We can use a timer to only update imgui 60 times per second
	app->timer = zest_CreateTimer("Main loop timer", 60);
	app->request_graph_print = false;
	app->reset = false;

	app->lights_buffer = zest_CreateUniformBuffer("Lights", sizeof(UniformLights));
	app->view_buffer = zest_CreateUniformBuffer("View", sizeof(uniform_buffer_data_t));
	app->material_push = { 0 };
	app->material_push.parameters1.x = .1f;
	app->material_push.parameters2.y = 1.f;
	app->material_push.parameters2.x = 0.1f;
	app->material_push.parameters2.y = 0.8f;
	app->material_push.parameters2.z = 0.1f;

	SetupBillboards(app);

	//Compile the shaders we will use to render the particles
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	zest_CreateShaderFromFile("examples/GLFW/zest-pbr/shaders/pbr_simple.vert", "pbr_simple_vert.spv", shaderc_vertex_shader, true, compiler, 0);
	zest_CreateShaderFromFile("examples/GLFW/zest-pbr/shaders/pbr_simple.frag", "pbr_simple_frag.spv", shaderc_fragment_shader, true, compiler, 0);
	shaderc_compiler_release(compiler);

	app->pbr_pipeline = zest_CopyPipelineTemplate("pbr_mesh_pipeline", zest_PipelineTemplate("pipeline_mesh_instance"));
	zest_ClearPipelineDescriptorLayouts(app->pbr_pipeline);
	zest_AddPipelineDescriptorLayout(app->pbr_pipeline, zest_vk_GetUniformBufferLayout(app->view_buffer));
	zest_AddPipelineDescriptorLayout(app->pbr_pipeline, zest_vk_GetUniformBufferLayout(app->lights_buffer));
	zest_SetPipelineShaders(app->pbr_pipeline, "pbr_simple_vert.spv", "pbr_simple_frag.spv", 0);
	zest_EndPipelineTemplate(app->pbr_pipeline);

    app->pbr_pipeline->rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    app->pbr_pipeline->rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    app->pbr_pipeline->rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    app->pbr_pipeline->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	app->pbr_shader_resources = zest_CreateShaderResources("Mesh resources");
	zest_AddUniformBufferToResources(app->pbr_shader_resources, app->view_buffer);				//Set 0
	zest_AddUniformBufferToResources(app->pbr_shader_resources, app->lights_buffer);			//Set 1

	app->cube_layer = zest_CreateBuiltinInstanceMeshLayer("Cube Layer");
	zest_mesh cube = zest_CreateCube(1.f, zest_ColorSet(0, 50, 100, 255));
	zest_mesh sphere = zest_CreateSphere(50, 50, 1.f, zest_ColorSet(0, 50, 100, 255));
	zest_mesh cone = zest_CreateCone(50, 1.f, 2.f, zest_ColorSet(0, 50, 100, 255));
	zest_mesh cylinder = zest_CreateCylinder(50, 1.f, 2.f, zest_ColorSet(0, 50, 100, 255), true);
	zest_AddMeshToLayer(app->cube_layer, cylinder);
	zest_FreeMesh(cube);
	zest_FreeMesh(sphere);
	zest_FreeMesh(cone);
	zest_FreeMesh(cylinder);
}

void UpdateLights(ImGuiApp *app, float timer) {
	const float p = 15.0f;

	UniformLights *buffer_data = (UniformLights*)zest_GetUniformBufferData(app->lights_buffer);

	buffer_data->lights[0] = zest_Vec4Set(-p, -p * 0.5f, -p, 1.0f);
	buffer_data->lights[1] = zest_Vec4Set(-p, -p * 0.5f, p, 1.0f);
	buffer_data->lights[2] = zest_Vec4Set(p, -p * 0.5f, p, 1.0f);
	buffer_data->lights[3] = zest_Vec4Set(p, -p * 0.5f, -p, 1.0f);

	buffer_data->lights[0].x = sinf(zest_Radians(timer * 90.f)) * 20.0f;
	buffer_data->lights[0].z = cosf(zest_Radians(timer * 90.f)) * 20.0f;
	buffer_data->lights[1].x = cosf(zest_Radians(timer * 90.f)) * 20.0f;
	buffer_data->lights[1].y = sinf(zest_Radians(timer * 90.f)) * 20.0f;

	zest_SetInstanceDrawing(app->billboard_layer, app->sprite_resources, app->billboard_pipeline);
	zest_SetLayerColor(app->billboard_layer, 255, 255, 255, 255);
	zest_DrawBillboardSimple(app->billboard_layer, app->light, &buffer_data->lights[0].x, 0.f, 1.f, 1.f);
	zest_DrawBillboardSimple(app->billboard_layer, app->light, &buffer_data->lights[1].x, 0.f, 1.f, 1.f);
	zest_DrawBillboardSimple(app->billboard_layer, app->light, &buffer_data->lights[2].x, 0.f, 1.f, 1.f);
	zest_DrawBillboardSimple(app->billboard_layer, app->light, &buffer_data->lights[3].x, 0.f, 1.f, 1.f);
}

void UpdateCallback(zest_microsecs elapsed, void* user_data) {
	//The struct for this example app from the user data we set when initialising Zest
	ImGuiApp* app = (ImGuiApp*)user_data;
	UpdateUniform3d(app);

	bool camera_free_look = false;
	if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		camera_free_look = true;
		if (glfwRawMouseMotionSupported()) {
			glfwSetInputMode((GLFWwindow *)zest_Window(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		}
		ZEST__FLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
		double x_mouse_speed;
		double y_mouse_speed;
		zest_GetMouseSpeed(&x_mouse_speed, &y_mouse_speed);
		zest_TurnCamera(&app->camera, (float)ZestApp->mouse_delta_x, (float)ZestApp->mouse_delta_y, .05f);
	} else if (glfwRawMouseMotionSupported()) {
		camera_free_look = false;
		ZEST__UNFLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
		glfwSetInputMode((GLFWwindow *)zest_Window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	} else {
		camera_free_look = false;
		ZEST__UNFLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
	}

	//We can use a timer to only update the gui every 60 times a second (or whatever you decide). This
	//means that the buffers are uploaded less frequently and the command buffer is also re-recorded
	//less frequently.

	zest_StartTimerLoop(app->timer) {
		//Must call the imgui GLFW implementation function
		ImGui_ImplGlfw_NewFrame();
		//Draw our imgui stuff
		ImGui::NewFrame();
		ImGui::Begin("Test Window");
		ImGui::Text("FPS %i", ZestApp->last_fps);
		ImGui::DragFloat("Rougness", &app->material_push.parameters1.x, 0.01f, 0.f, 1.f);
		ImGui::DragFloat("Metallic", &app->material_push.parameters1.y, 0.01f, 0.f, 1.f);
		ImGui::ColorPicker3("Color", &app->material_push.parameters2.x);
		ImGui::Separator();
		if (ImGui::Button("Toggle Refresh Rate Sync")) {
			if (app->sync_refresh) {
				zest_DisableVSync();
				app->sync_refresh = false;
			} else {
				zest_EnableVSync();
				app->sync_refresh = true;
			}
		}
		if (ImGui::Button("Print Render Graph")) {
			app->request_graph_print = true;
			zloc_VerifyAllRemoteBlocks(0, 0);
		}
		if (ImGui::Button("Reset Renderer")) {
			app->reset = true;
		}
		ImGui::End();
		ImGui::Render();
		//An imgui layer is a manual layer, meaning that you need to let it know that the buffers need updating.
		//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
		zest_imgui_UpdateBuffers();
	} zest_EndTimerLoop(app->timer);

	zest_vec3 position = { 0.f, 0.f, 0.f };
	app->ellapsed_time += elapsed;
	float rotation_time = app->ellapsed_time * .000001f;
	zest_vec3 rotation = { sinf(rotation_time), cosf(rotation_time), -sinf(rotation_time)};
	zest_vec3 scale = { 1.f, 1.f, 1.f };

	UpdateLights(app, rotation_time);
	app->material_push.global = zest_Vec4Set(app->camera.position.x, app->camera.position.y, app->camera.position.z, 0.f);
	zest_SetInstanceDrawing(app->cube_layer, app->pbr_shader_resources, app->pbr_pipeline);
	zest_SetLayerPushConstants(app->cube_layer, &app->material_push, sizeof(zest_push_constants_t));
	zest_SetLayerColor(app->cube_layer, 255, 255, 255, 255);
	zest_DrawInstancedMesh(app->cube_layer, &position.x, &rotation.x, &scale.x);

	if (app->reset) {
		app->reset = false;
		zest_imgui_Shutdown();
		zest_ResetRenderer();
		InitImGuiApp(app);
	}

	zest_swapchain swapchain = zest_GetMainWindowSwapchain();
	app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw();
	zest_render_graph_cache_key_t cache_key = {};
	cache_key = zest_InitialiseCacheKey(swapchain, &app->cache_info, sizeof(RenderCacheInfo));

	zest_SetSwapchainClearColor(swapchain, 0, 0.1f, 0.2f, 1.f);
	//Begin the render graph with the command that acquires a swap chain image (zest_BeginRenderToScreen)
	//Use the render graph we created earlier. Will return false if a swap chain image could not be acquired. This will happen
	//if the window is resized for example.
	if (zest_BeginRenderToScreen(swapchain, "ImGui", 0)) {
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };

		zest_resource_node cube_layer_resource = zest_AddTransientLayerResource("PBR Layer", app->cube_layer, false);
		zest_resource_node billboard_layer_resource = zest_AddTransientLayerResource("Billboard Layer", app->billboard_layer, false);
		zest_resource_node billboard_texture_resource = zest_ImportImageResource("Sprite Texture", app->sprites_texture, 0);

		//-------------------------Transfer Pass----------------------------------------------------
		zest_pass_node upload_mesh_data = zest_AddTransferPassNode("Upload Mesh Data");
		zest_ConnectOutput(upload_mesh_data, cube_layer_resource);
		zest_SetPassTask(upload_mesh_data, zest_UploadInstanceLayerData, app->cube_layer);
		//--------------------------------------------------------------------------------------------------

		//-------------------------Transfer Pass----------------------------------------------------
		zest_pass_node upload_billboard_data = zest_AddTransferPassNode("Upload Billboard Data");
		zest_ConnectOutput(upload_billboard_data, billboard_layer_resource);
		zest_SetPassTask(upload_billboard_data, zest_UploadInstanceLayerData, app->billboard_layer);
		//--------------------------------------------------------------------------------------------------

		//------------------------ PBR Layer Pass ------------------------------------------------------------
		zest_pass_node cube_pass = zest_AddRenderPassNode("Cube Pass");
		zest_ConnectInput(cube_pass, cube_layer_resource, 0);
		zest_ConnectSwapChainOutput(cube_pass);
		zest_SetPassTask(cube_pass, zest_DrawInstanceMeshLayer, app->cube_layer);
		//--------------------------------------------------------------------------------------------------

		//------------------------ Billboard Layer Pass ------------------------------------------------------------
		zest_pass_node billboard_pass = zest_AddRenderPassNode("Billboard Pass");
		zest_ConnectInput(billboard_pass, billboard_layer_resource, 0);
		zest_ConnectInput(billboard_pass, billboard_texture_resource, 0);
		zest_ConnectSwapChainOutput(billboard_pass);
		zest_SetPassTask(billboard_pass, zest_DrawInstanceLayer, app->billboard_layer);
		//--------------------------------------------------------------------------------------------------

		//------------------------ ImGui Pass ----------------------------------------------------------------
		//If there's imgui to draw then draw it
		zest_pass_node imgui_pass = zest_imgui_AddToRenderGraph();
		if (imgui_pass) {
			zest_ConnectSwapChainOutput(imgui_pass);
		} else {
			//If there's no ImGui to render then just render a blank screen
			zest_pass_node blank_pass = zest_AddGraphicBlankScreen("Draw Nothing");
			//Add the swap chain as an output to the imgui render pass. This is telling the render graph where it should render to.
			zest_ConnectSwapChainOutput(blank_pass);
		}
		//----------------------------------------------------------------------------------------------------
		//End the render graph and execute it. This will submit it to the GPU.
		zest_render_graph render_graph = zest_EndRenderGraph();
		if (app->request_graph_print) {
			//You can print out the render graph for debugging purposes
			zest_PrintCompiledRenderGraph(render_graph);
			app->request_graph_print = false;
		}
	} else {
		if (app->request_graph_print) {
			//You can print out the render graph for debugging purposes
			zest_PrintCachedRenderGraph(&cache_key);
			app->request_graph_print = false;
		}
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(zest_validation_flag_enable_sync);
	//zest_create_info_t create_info = zest_CreateInfo();
    create_info.log_path = ".";
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	ZEST__UNFLAG(create_info.flags, zest_init_flag_cache_shaders);
	//Implement GLFW for window creation
	zest_implglfw_SetCallbacks(&create_info);

	ImGuiApp imgui_app = {};

	//Initialise Zest
	zest_Initialise(&create_info);
	//Set the Zest use data
	zest_SetUserData(&imgui_app);
	//Set the udpate callback to be called every frame
	zest_SetUserUpdateCallback(UpdateCallback);
	//Initialise our example
	InitImGuiApp(&imgui_app);

	//Start the main loop
	zest_Start();
	zest_Shutdown();

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	zest_implglfw_SetCallbacks(&create_info);
    ZEST__FLAG(create_info.flags, zest_init_flag_maximised);

	ImGuiApp imgui_app;

    create_info.log_path = ".";
	zest_Initialise(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#endif
