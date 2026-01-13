#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_ALL_UTILITIES_IMPLEMENTATION
#define ZEST_TEST_MODE
#include "zest-pbr-forward.h"
#include "zest.h"
#include "imgui_internal.h"
#include "examples/Common/pbr_functions.cpp"

void InitSimplePBRExample(SimplePBRExample *app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise(app->context, &app->imgui, zest_implglfw_DestroyWindow);
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)zest_Window(app->context), true);
	//Implement a dark style
	zest_imgui_DarkStyle(&app->imgui);
	
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
	zest_imgui_RebuildFontTexture(&app->imgui, tex_width, tex_height, font_data);

	app->camera = zest_CreateCamera();
	zest_CameraPosition(&app->camera, { -2.5f, 0.f, 0.f });
	zest_CameraSetFoV(&app->camera, 60.f);
	zest_CameraSetYaw(&app->camera, zest_Radians(-90.f));
	zest_CameraSetPitch(&app->camera, zest_Radians(0.f));
	zest_CameraUpdateFront(&app->camera);
	app->new_camera_position = app->camera.position;
	app->old_camera_position = app->camera.position;

	//We can use a timer to only update imgui 60 times per second
	app->timer = zest_CreateTimer(60);
	app->request_graph_print = 0;
	app->reset = false;

	app->lights_buffer = zest_CreateUniformBuffer(app->context, "Lights", sizeof(UniformLights));
	app->view_buffer = zest_CreateUniformBuffer(app->context, "View", sizeof(uniform_buffer_data_t));
	app->material_push = { 0 };
	app->material_push.color.x = 0.1f;
	app->material_push.color.y = 0.8f;
	app->material_push.color.z = 0.1f;

	//Compile the shaders we will use to render the particles
	zest_shader_handle pbr_irradiance_vert = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-pbr-forward/shaders/pbr_irradiance.vert", "pbr_irradiance_vert.spv", zest_vertex_shader, true);
	zest_shader_handle pbr_irradiance_frag = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-pbr-forward/shaders/pbr_irradiance.frag", "pbr_irradiance_frag.spv", zest_fragment_shader, true);
	zest_shader_handle skybox_vert = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-pbr-forward/shaders/sky_box.vert", "sky_box_vert.spv", zest_vertex_shader, true);
	zest_shader_handle skybox_frag = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-pbr-forward/shaders/sky_box.frag", "sky_box_frag.spv", zest_fragment_shader, true);

	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	app->sampler_2d = zest_CreateSampler(app->context, &sampler_info);

	zest_sampler sampler_2d = zest_GetSampler(app->sampler_2d);

	app->skybox_texture = zest_LoadKTX(app->device, "Pisa Cube", "examples/assets/pisa_cube.ktx");
	zest_image skybox_image = zest_GetImage(app->skybox_texture);
	app->skybox_bindless_texture_index = zest_AcquireSampledImageIndex(app->device, skybox_image, zest_texture_cube_binding);

	app->sampler_2d_index = zest_AcquireSamplerIndex(app->device, sampler_2d);

	app->brd_texture = CreateBRDFLUT(app->context);
	app->irr_texture = CreateIrradianceCube(app->context, app->skybox_texture, app->sampler_2d_index);
	app->prefiltered_texture = CreatePrefilteredCube(app->context, app->skybox_texture, app->sampler_2d_index, &app->prefiltered_mip_indexes);

	app->material_push.irradiance_index = zest_ImageDescriptorIndex(zest_GetImage(app->irr_texture), zest_texture_cube_binding);
	app->material_push.brd_lookup_index = zest_ImageDescriptorIndex(zest_GetImage(app->brd_texture), zest_texture_2d_binding);
	app->material_push.pre_filtered_index = zest_ImageDescriptorIndex(zest_GetImage(app->prefiltered_texture), zest_texture_cube_binding);

	zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);
	zest_uniform_buffer lights_buffer = zest_GetUniformBuffer(app->lights_buffer);

	app->pbr_pipeline = zest_BeginPipelineTemplate(app->device, "pipeline_mesh_instance");
	zest_AddVertexInputBindingDescription(app->pbr_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexInputBindingDescription(app->pbr_pipeline, 1, sizeof(zest_mesh_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);                                          // Location 0: Vertex Position
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 1, zest_format_r8g8b8a8_unorm, offsetof(zest_vertex_t, color));               // Location 1: Vertex Color
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 2, zest_format_r32g32b32_sfloat, offsetof(zest_vertex_t, normal));            // Location 3: Vertex Position
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 3, zest_format_r32g32_sfloat, offsetof(zest_vertex_t, uv));                     // Location 2: Group id
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 4, zest_format_r16g16b16a16_unorm, offsetof(zest_vertex_t, tangent));                     // Location 2: Group id
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 5, zest_format_r32_uint, offsetof(zest_vertex_t, parameters));                     // Location 2: Group id
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 6, zest_format_r32g32b32_sfloat, 0);                                          // Location 4: Instance Position
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 7, zest_format_r8g8b8a8_unorm, offsetof(zest_mesh_instance_t, color));        // Location 5: Instance Color
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 8, zest_format_r32g32b32_sfloat, offsetof(zest_mesh_instance_t, rotation));   // Location 6: Instance Rotation
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 9, zest_format_r32_sfloat, offsetof(zest_mesh_instance_t, roughness));   // Location 7: Instance Parameters
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 10, zest_format_r32g32b32_sfloat, offsetof(zest_mesh_instance_t, scale));      // Location 8: Instance Scale
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 11, zest_format_r32_sfloat, offsetof(zest_mesh_instance_t, metallic));   // Location 7: Instance Parameters

	zest_SetPipelineShaders(app->pbr_pipeline, pbr_irradiance_vert, pbr_irradiance_frag);
	zest_SetPipelineCullMode(app->pbr_pipeline, zest_cull_mode_back);
	zest_SetPipelineFrontFace(app->pbr_pipeline, zest_front_face_counter_clockwise);
	zest_SetPipelineTopology(app->pbr_pipeline, zest_topology_triangle_list);
	zest_SetPipelineDepthTest(app->pbr_pipeline, true, true);

	app->skybox_pipeline = zest_CopyPipelineTemplate("sky_box", app->pbr_pipeline);
	zest_SetPipelineFrontFace(app->skybox_pipeline, zest_front_face_clockwise);
	zest_SetPipelineShaders(app->skybox_pipeline, skybox_vert, skybox_frag);
	zest_SetPipelineDepthTest(app->skybox_pipeline, false, false);

	zest_mesh cube = zest_CreateCube(app->context, 1.f, zest_ColorSet(0, 50, 100, 255));
	zest_mesh sphere = zest_CreateSphere(app->context, 100, 100, 1.f, zest_ColorSet(0, 50, 100, 255));
	zest_mesh teapot = LoadGLTFScene(app->context, "examples/assets/gltf/teapot.gltf", .5f);
	zest_mesh torus = LoadGLTFScene(app->context, "examples/assets/gltf/torusknot.gltf", 1.f);
	zest_mesh venus = LoadGLTFScene(app->context, "examples/assets/gltf/venus.gltf", 2.f);
	zest_mesh sky_box = zest_CreateCube(app->context, 1.f, zest_ColorSet(255, 255, 255, 255));

	zest_size vertex_capacity = zest_MeshVertexDataSize(cube);
	vertex_capacity += zest_MeshVertexDataSize(sphere);
	vertex_capacity += zest_MeshVertexDataSize(teapot);
	vertex_capacity += zest_MeshVertexDataSize(torus);
	vertex_capacity += zest_MeshVertexDataSize(venus);

	zest_size index_capacity = zest_MeshIndexDataSize(cube);
	index_capacity += zest_MeshIndexDataSize(sphere);
	index_capacity += zest_MeshIndexDataSize(teapot);
	index_capacity += zest_MeshIndexDataSize(torus);
	index_capacity += zest_MeshIndexDataSize(venus);

	app->mesh_layer = zest_CreateInstanceMeshLayer(app->context, "Mesh Layer", sizeof(zest_mesh_instance_t), vertex_capacity, index_capacity);
	app->skybox_layer = zest_CreateInstanceMeshLayer(app->context, "Skybox Layer", sizeof(zest_mesh_instance_t), zest_MeshVertexDataSize(sky_box), zest_MeshIndexDataSize(sky_box));

	app->teapot_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), teapot, 0);
	app->torus_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), torus, 0);
	app->venus_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), venus, 0);
	app->cube_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), cube, 0);
	app->sphere_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), sphere, 0);
	app->skybox_index = zest_AddMeshToLayer(zest_GetLayer(app->skybox_layer), sky_box, 0);

	zest_FreeMesh(sky_box);
	zest_FreeMesh(cube);
	zest_FreeMesh(sphere);
	zest_FreeMesh(venus);
	zest_FreeMesh(torus);
	zest_FreeMesh(teapot);
}

void UpdateUniform3d(SimplePBRExample *app) {
	zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);
	uniform_buffer_data_t *ubo_ptr = static_cast<uniform_buffer_data_t *>(zest_GetUniformBufferData(view_buffer));
	ubo_ptr->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	ubo_ptr->proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf(app->context) / zest_ScreenHeightf(app->context), 0.001f, 10000.f);
	ubo_ptr->proj.v[1].y *= -1.f;
	ubo_ptr->screen_size.x = zest_ScreenWidthf(app->context);
	ubo_ptr->screen_size.y = zest_ScreenHeightf(app->context);
	app->material_push.view_buffer_index = zest_GetUniformBufferDescriptorIndex(view_buffer);
}

void UpdateLights(SimplePBRExample *app, float timer) {
	const float p = 15.0f;

	zest_uniform_buffer lights_buffer = zest_GetUniformBuffer(app->lights_buffer);
	UniformLights *buffer_data = (UniformLights*)zest_GetUniformBufferData(lights_buffer);

	buffer_data->lights[0] = zest_Vec4Set(-p, -p * 0.5f, -p, 1.0f);
	buffer_data->lights[1] = zest_Vec4Set(-p, -p * 0.5f, p, 1.0f);
	buffer_data->lights[2] = zest_Vec4Set(p, -p * 0.5f, p, 1.0f);
	buffer_data->lights[3] = zest_Vec4Set(p, -p * 0.5f, -p, 1.0f);

	buffer_data->lights[0].x = sinf(zest_Radians(timer * 90.f)) * 20.0f;
	buffer_data->lights[0].z = cosf(zest_Radians(timer * 90.f)) * 20.0f;
	buffer_data->lights[1].x = cosf(zest_Radians(timer * 90.f)) * 20.0f;
	buffer_data->lights[1].y = sinf(zest_Radians(timer * 90.f)) * 20.0f;

	buffer_data->texture_index = app->skybox_bindless_texture_index;
	//buffer_data->sampler_index = app->sampler_2d_index;
	buffer_data->sampler_index = 0;
	buffer_data->exposure = 4.5f;
	buffer_data->gamma = 2.2f;

	app->material_push.lights_buffer_index = zest_GetUniformBufferDescriptorIndex(lights_buffer);
}

void UploadMeshData(const zest_command_list context, void *user_data) {
	SimplePBRExample *app = (SimplePBRExample *)user_data;

	zest_layer_handle layers[2]{
		app->mesh_layer,
		app->skybox_layer
	};

	for (int i = 0; i != 2; ++i) {
		zest_layer layer = zest_GetLayer(layers[i]);
		zest_UploadLayerStagingData(layer, context);
	}
}

void UpdateCameraPosition(SimplePBRExample *app) {
	float speed = 5.f * (float)zest_TimerUpdateTime(&app->timer);
	app->old_camera_position = app->camera.position;
	if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		ImGui::SetWindowFocus(nullptr);

		if (ImGui::IsKeyDown(ImGuiKey_W)) {
			app->new_camera_position = zest_AddVec3(app->new_camera_position, zest_ScaleVec3(app->camera.front, speed));
		}
		if (ImGui::IsKeyDown(ImGuiKey_S)) {
			app->new_camera_position = zest_SubVec3(app->new_camera_position, zest_ScaleVec3(app->camera.front, speed));
		}
		if (ImGui::IsKeyDown(ImGuiKey_A)) {
			zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(app->camera.front, app->camera.up));
			app->new_camera_position = zest_SubVec3(app->new_camera_position, zest_ScaleVec3(cross, speed));
		}
		if (ImGui::IsKeyDown(ImGuiKey_D)) {
			zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(app->camera.front, app->camera.up));
			app->new_camera_position = zest_AddVec3(app->new_camera_position, zest_ScaleVec3(cross, speed));
		}
	}
}

void UpdateMouse(SimplePBRExample *app) {
	double mouse_x, mouse_y;
	GLFWwindow *handle = (GLFWwindow *)zest_Window(app->context);
	glfwGetCursorPos(handle, &mouse_x, &mouse_y);
	double last_mouse_x = app->mouse_x;
	double last_mouse_y = app->mouse_y;
	app->mouse_x = mouse_x;
	app->mouse_y = mouse_y;
	app->mouse_delta_x = last_mouse_x - app->mouse_x;
	app->mouse_delta_y = last_mouse_y - app->mouse_y;

	bool camera_free_look = false;
	if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		camera_free_look = true;
		if (glfwRawMouseMotionSupported()) {
			glfwSetInputMode((GLFWwindow *)zest_Window(app->context), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		}
		ZEST__FLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
		zest_TurnCamera(&app->camera, (float)app->mouse_delta_x, (float)app->mouse_delta_y, .05f);
	} else if (glfwRawMouseMotionSupported()) {
		camera_free_look = false;
		ZEST__UNFLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
		glfwSetInputMode((GLFWwindow *)zest_Window(app->context), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	} else {
		camera_free_look = false;
		ZEST__UNFLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
	}

	//Restore the mouse when right mouse isn't held down
	if (camera_free_look) {
		glfwSetInputMode((GLFWwindow*)zest_Window(app->context), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
	else {
		glfwSetInputMode((GLFWwindow*)zest_Window(app->context), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
}

void UpdateImGui(SimplePBRExample *app) {
	//We can use a timer to only update the gui every 60 times a second (or whatever you decide). This
	//means that the buffers are uploaded less frequently and the command buffer is also re-recorded
	//less frequently.
	zest_StartTimerLoop(app->timer) {
		//Must call the imgui GLFW implementation function
		ImGui_ImplGlfw_NewFrame();
		//Draw our imgui stuff
		ImGui::NewFrame();
		ImGui::Begin("Test Window");
		ImGui::Text("FPS: %u", app->fps);
		ImGui::ColorPicker3("Color", &app->material_push.color.x);
		ImGui::Separator();
		if (ImGui::Button("Toggle Refresh Rate Sync")) {
			if (app->sync_refresh) {
				zest_DisableVSync(app->context);
				app->sync_refresh = false;
			} else {
				zest_EnableVSync(app->context);
				app->sync_refresh = true;
			}
		}
		if (ImGui::Button("Print Render Graph")) {
			app->request_graph_print = 1;
			zloc_VerifyAllRemoteBlocks(app->context, 0, 0);
		}
		if (ImGui::Button("Reset Renderer")) {
			app->reset = true;
		}
		ImGui::End();
		ImGui::Render();
		//An imgui layer is a manual layer, meaning that you need to let it know that the buffers need updating.
		//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
		UpdateCameraPosition(app);
	} zest_EndTimerLoop(app->timer);
}

void MainLoop(SimplePBRExample *app) {
	zest_microsecs running_time = zest_Microsecs();
	zest_microsecs frame_time = 0;
	zest_uint frame_count = 0;

	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
		zest_microsecs current_frame_time = zest_Microsecs() - running_time;
		running_time = zest_Microsecs();
		frame_time += current_frame_time;
		frame_count += 1;
		if (frame_time >= ZEST_MICROSECS_SECOND) {
			frame_time -= ZEST_MICROSECS_SECOND;
			app->fps = frame_count;
			frame_count = 0;
		}

		glfwPollEvents();

		zest_UpdateDevice(app->device);

		zest_layer mesh_layer = zest_GetLayer(app->mesh_layer);
		zest_layer skybox_layer = zest_GetLayer(app->skybox_layer);

		if (zest_BeginFrame(app->context)) {

			UpdateMouse(app);

			float elapsed = (float)current_frame_time;

			UpdateUniform3d(app);

			UpdateImGui(app);

			app->camera.position = zest_LerpVec3(&app->old_camera_position, &app->new_camera_position, (float)zest_TimerLerp(&app->timer));

			zest_vec3 position = { 0.f, 0.f, 0.f };
			app->ellapsed_time += elapsed;
			float rotation_time = app->ellapsed_time * .000001f;
			zest_vec3 rotation = { sinf(rotation_time), cosf(rotation_time), -sinf(rotation_time) };
			zest_vec3 scale = { 1.f, 1.f, 1.f };

			zest_image brd_image = zest_GetImage(app->brd_texture);
			zest_image irr_image = zest_GetImage(app->irr_texture);
			zest_image prefiltered_image = zest_GetImage(app->prefiltered_texture);
			zest_image skybox_image = zest_GetImage(app->skybox_texture);

			UpdateLights(app, rotation_time);
			app->material_push.camera = zest_Vec4Set(app->camera.position.x, app->camera.position.y, app->camera.position.z, 0.f);
			app->material_push.irradiance_index = zest_ImageDescriptorIndex(irr_image, zest_texture_cube_binding);
			app->material_push.brd_lookup_index = zest_ImageDescriptorIndex(brd_image, zest_texture_2d_binding);
			app->material_push.pre_filtered_index = zest_ImageDescriptorIndex(prefiltered_image, zest_texture_cube_binding);
			app->material_push.sampler_index = app->sampler_2d_index;
			zest_SetLayerColor(mesh_layer, 255, 255, 255, 255);
			float count = 10.f;
			float zero[3] = { 0 };
			for (int m = 0; m != 5; m++) {
				zest_SetInstanceMeshDrawing(mesh_layer, m, app->pbr_pipeline);
				zest_SetLayerPushConstants(mesh_layer, &app->material_push, sizeof(pbr_consts_t));
				for (float i = 0; i < count; i++) {
					float roughness = 1.0f - ZEST__CLAMP(i / count, 0.005f, 1.0f);
					float metallic = ZEST__CLAMP(i / count, 0.005f, 1.0f);
					switch (m) {
						case 0:
						case 1:
						case 3: {
							zest_DrawInstancedMesh(mesh_layer, &position.x, &rotation.x, &scale.x, roughness, metallic);
							break;
						}
						case 2:
						case 4: {
							zest_DrawInstancedMesh(mesh_layer, &position.x, zero, &scale.x, roughness, metallic);
							break;
						}
					}
					position.x += 3.f;
				}
				position.x = 0.f;
				position.z += 3.f;
			}

			zest_uint sky_push[] = {
				app->material_push.view_buffer_index,
				app->material_push.lights_buffer_index,
			};
			zest_SetInstanceMeshDrawing(skybox_layer, 0, app->skybox_pipeline);
			zest_SetLayerColor(skybox_layer, 255, 255, 255, 255);
			zest_DrawInstancedMesh(skybox_layer, zero, zero, zero, 0, 0);
			zest_SetLayerPushConstants(skybox_layer, sky_push, sizeof(zest_uint) * 2);

			if (app->reset) {
				app->reset = false;
				ImGui_ImplGlfw_Shutdown();
				zest_imgui_Destroy(&app->imgui);
				zest_implglfw_DestroyWindow(app->context);
				zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");
				zest_ResetContext(app->context, &window_handles);
				InitSimplePBRExample(app);
			}

			zest_swapchain swapchain = zest_GetSwapchain(app->context);

			//Initially when the 3 textures that are created using compute shaders in the setup they will be in 
			//image layout general. When they are used in the frame graph below they will be transitioned to read only
			//so we store the current layout of the image in a custom cache info struct so that when the layout changes
			//the cache key will change and a new cache will be created as a result. The other option is to transition 
			//them before hand but this is just to show an example of how the frame graph caching can work.
			app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw(&app->imgui);
			app->cache_info.brd_layout = zest_ImageRawLayout(brd_image);
			app->cache_info.irradiance_layout = zest_ImageRawLayout(irr_image);
			app->cache_info.prefiltered_layout = zest_ImageRawLayout(prefiltered_image);
			zest_frame_graph_cache_key_t cache_key = {};
			cache_key = zest_InitialiseCacheKey(app->context, &app->cache_info, sizeof(RenderCacheInfo));

			zest_image_resource_info_t depth_info = {
				zest_format_depth,
				zest_resource_usage_hint_none,
				zest_ScreenWidth(app->context),
				zest_ScreenHeight(app->context),
				1, 1
			};

			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);
			//Begin the render graph with the command that acquires a swap chain image (zest_BeginFrameGraphSwapchain)
			//Use the render graph we created earlier. Will return false if a swap chain image could not be acquired. This will happen
			//if the window is resized for example.
			if (!frame_graph) {
				zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);
				zest_uniform_buffer lights_buffer = zest_GetUniformBuffer(app->lights_buffer);
				if (zest_BeginFrameGraph(app->context, "ImGui", &cache_key)) {
					zest_resource_node mesh_layer_resource = zest_AddTransientLayerResource("Mesh Layer", mesh_layer, false);
					zest_resource_node skybox_layer_resource = zest_AddTransientLayerResource("Sky Box Layer", skybox_layer, false);
					zest_resource_node skybox_texture_resource = zest_ImportImageResource("Sky Box Texture", skybox_image, 0);
					zest_resource_node brd_texture_resource = zest_ImportImageResource("BRD lookup texture", brd_image, 0);
					zest_resource_node irradiance_texture_resource = zest_ImportImageResource("Irradiance texture", irr_image, 0);
					zest_resource_node prefiltered_texture_resource = zest_ImportImageResource("Prefiltered texture", prefiltered_image, 0);
					zest_resource_node depth_buffer = zest_AddTransientImageResource("Depth Buffer", &depth_info);
					zest_resource_node swapchain_node = zest_ImportSwapchainResource();
					zest_resource_group group = zest_CreateResourceGroup();
					zest_AddSwapchainToGroup(group);
					zest_AddResourceToGroup(group, depth_buffer);

					//-------------------------Transfer Pass----------------------------------------------------
					zest_BeginTransferPass("Upload Mesh Data"); {
						zest_ConnectOutput(mesh_layer_resource);
						zest_ConnectOutput(skybox_layer_resource);
						zest_SetPassTask(UploadMeshData, app);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//------------------------ Skybox Layer Pass ------------------------------------------------------------
					zest_BeginRenderPass("Skybox Pass"); {
						zest_ConnectInput(skybox_texture_resource);
						zest_ConnectInput(skybox_layer_resource);
						zest_ConnectOutputGroup(group);
						zest_SetPassTask(zest_DrawInstanceMeshLayer, skybox_layer);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//------------------------ PBR Layer Pass ------------------------------------------------------------
					zest_BeginRenderPass("Instance Mesh Pass"); {
						zest_ConnectInput(mesh_layer_resource);
						zest_ConnectInput(brd_texture_resource);
						zest_ConnectInput(irradiance_texture_resource);
						zest_ConnectInput(prefiltered_texture_resource);
						zest_ConnectOutputGroup(group);
						zest_SetPassTask(zest_DrawInstanceMeshLayer, mesh_layer);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//------------------------ ImGui Pass ----------------------------------------------------------------
					//If there's imgui to draw then draw it
					zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui, app->imgui.main_viewport); {
						if (imgui_pass) {
							zest_ConnectOutputGroup(group);
						} else {
							//If there's no ImGui to render then just render a blank screen
							zest_BeginRenderPass("Draw Nothing");
							//Add the swap chain as an output to the imgui render pass. This is telling the render graph where it should render to.
							zest_ConnectOutputGroup(group);
							zest_SetPassTask(zest_EmptyRenderPass, 0);
						}
						zest_EndPass();
					}
					//----------------------------------------------------------------------------------------------------

					//End the render graph and execute it. This will submit it to the GPU.
					frame_graph = zest_EndFrameGraph();
				}
			}

			zest_QueueFrameGraphForExecution(app->context, frame_graph);
			zest_EndFrame(app->context);
			if (app->request_graph_print > 0) {
				//You can print out the render graph for debugging purposes
				zest_PrintCompiledFrameGraph(frame_graph);
				app->request_graph_print--;
			}
		}

		if (zest_SwapchainWasRecreated(app->context)) {
			zest_SetLayerSizeToSwapchain(mesh_layer);
			zest_SetLayerSizeToSwapchain(skybox_layer);
		}
	}
}

int main(void) {

	if (!glfwInit()) {
		return 0;
	}

	SimplePBRExample imgui_app = {};
	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);

	//Create the device that serves all vulkan based contexts
	zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
	zest_AddDeviceBuilderExtensions(device_builder, glfw_extensions, count);
	//zest_AddDeviceBuilderValidation(device_builder);
	zest_DeviceBuilderLogToConsole(device_builder);
	imgui_app.device = zest_EndDeviceBuilder(device_builder);

	zest_SetStagingBufferPoolSize(imgui_app.device, zloc__KILOBYTE(256), zloc__MEGABYTE(128));

	//Create a window using GLFW
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");

	//Create new config struct for Zest
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	//Initialise Zest
	imgui_app.context = zest_CreateContext(imgui_app.device, &window_handles, &create_info);

	//int *test = nullptr;
	//zest_vec_push(imgui_app.context->device->allocator, test, 10);

	//Initialise our example
	InitSimplePBRExample(&imgui_app);

	//Start the main loop
	MainLoop(&imgui_app);
	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Destroy(&imgui_app.imgui);
	zest_DestroyDevice(imgui_app.device);

	return 0;
}
