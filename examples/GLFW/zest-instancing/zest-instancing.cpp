#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_ALL_UTILITIES_IMPLEMENTATION
#define ZEST_TEST_MODE
#include "zest-instancing.h"
#include "zest.h"
#include "imgui_internal.h"
#include <random>

/*
Example recreated from Sascha Willems "Instanced mesh rendering" 
https://github.com/SaschaWillems/Vulkan/tree/master/examples/instancing
*/

#define INSTANCE_COUNT 8192

void InitInstancingExample(InstancingExample *app) {
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
	float position[3] = { -15.5f, 5.f, 0.f };
	zest_CameraPosition(&app->camera, position);
	zest_CameraSetFoV(&app->camera, 60.f);
	zest_CameraSetYaw(&app->camera, 0.f);
	zest_CameraSetPitch(&app->camera, -25.f);
	zest_CameraUpdateFront(&app->camera);
	app->new_camera_position = app->camera.position;
	app->old_camera_position = app->camera.position;

	//We can use a timer to only update imgui 60 times per second
	app->timer = zest_CreateTimer(60);

	app->view_buffer = zest_CreateUniformBuffer(app->context, "View", sizeof(uniform_buffer_data_t));

	//Compile the shaders we will use to render the particles
	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	sampler_info.address_mode_u = zest_sampler_address_mode_repeat;
	sampler_info.address_mode_v = zest_sampler_address_mode_repeat;
	sampler_info.address_mode_w = zest_sampler_address_mode_repeat;
	app->sampler_2d = zest_CreateSampler(app->context, &sampler_info);
	zest_sampler sampler_2d = zest_GetSampler(app->sampler_2d);
	app->push.sampler_index = zest_AcquireSamplerIndex(app->device, sampler_2d);

	zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);

	zest_shader_handle instance_vert = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-instancing/shaders/instancing.vert", "instancing_vert.spv", zest_vertex_shader, true);
	zest_shader_handle instance_frag = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-instancing/shaders/instancing.frag", "instancing_frag.spv", zest_fragment_shader, true);
	zest_shader_handle planet_vert = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-instancing/shaders/planet.vert", "planet_vert.spv", zest_vertex_shader, true);
	zest_shader_handle planet_frag = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-instancing/shaders/planet.frag", "planet_frag.spv", zest_fragment_shader, true);
	zest_shader_handle skybox_vert = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-instancing/shaders/sky_box.vert", "skybox_vert.spv", zest_vertex_shader, true);
	zest_shader_handle skybox_frag = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-instancing/shaders/sky_box.frag", "skybox_frag.spv", zest_fragment_shader, true);

	app->rock_pipeline = zest_CreatePipelineTemplate(app->device, "pipeline_mesh_instance");
	zest_AddVertexInputBindingDescription(app->rock_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexInputBindingDescription(app->rock_pipeline, 1, sizeof(mesh_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(app->rock_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);                                         // Location 0: Vertex Position
	zest_AddVertexAttribute(app->rock_pipeline, 0, 1, zest_format_r8g8b8a8_unorm, offsetof(zest_vertex_t, color));              // Location 1: Vertex Color
	zest_AddVertexAttribute(app->rock_pipeline, 0, 2, zest_format_r32g32b32_sfloat, offsetof(zest_vertex_t, normal));           // Location 2: Vertex Position
	zest_AddVertexAttribute(app->rock_pipeline, 0, 3, zest_format_r32g32_sfloat, offsetof(zest_vertex_t, uv));                  // Location 3: Group id
	zest_AddVertexAttribute(app->rock_pipeline, 1, 4, zest_format_r32g32b32_sfloat, 0);                                         // Location 4: Instance Position
	zest_AddVertexAttribute(app->rock_pipeline, 1, 5, zest_format_r32g32b32_sfloat, offsetof(mesh_instance_t, rotation));  // Location 5: Instance Rotation
	zest_AddVertexAttribute(app->rock_pipeline, 1, 6, zest_format_r32g32b32_sfloat, offsetof(mesh_instance_t, scale));     // Location 6: Instance Scale
	zest_AddVertexAttribute(app->rock_pipeline, 1, 7, zest_format_r32_uint, offsetof(mesh_instance_t, texture_layer_index));   	// Location 7: Instance Parameters

	zest_SetPipelineShaders(app->rock_pipeline, instance_vert, instance_frag);
	zest_SetPipelineCullMode(app->rock_pipeline, zest_cull_mode_back);
	zest_SetPipelineFrontFace(app->rock_pipeline, zest_front_face_counter_clockwise);
	zest_SetPipelineTopology(app->rock_pipeline, zest_topology_triangle_list);
	zest_SetPipelineDepthTest(app->rock_pipeline, true, true);

	app->planet_pipeline = zest_CopyPipelineTemplate("Planet pipeline", app->rock_pipeline);
	zest_ClearVertexInputBindingDescriptions(app->planet_pipeline);
	zest_ClearVertexAttributeDescriptions(app->planet_pipeline);
	zest_AddVertexInputBindingDescription(app->planet_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexAttribute(app->planet_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);                                         // Location 0: Vertex Position
	zest_AddVertexAttribute(app->planet_pipeline, 0, 1, zest_format_r8g8b8a8_unorm, offsetof(zest_vertex_t, color));              // Location 1: Vertex Color
	zest_AddVertexAttribute(app->planet_pipeline, 0, 2, zest_format_r32g32b32_sfloat, offsetof(zest_vertex_t, normal));           // Location 2: Vertex Position
	zest_AddVertexAttribute(app->planet_pipeline, 0, 3, zest_format_r32g32_sfloat, offsetof(zest_vertex_t, uv));                  // Location 3: Group id
	zest_SetPipelineShaders(app->planet_pipeline, planet_vert, planet_frag);

	app->skybox_pipeline = zest_CopyPipelineTemplate("Skybox pipeline", app->rock_pipeline);
	zest_ClearVertexInputBindingDescriptions(app->skybox_pipeline);
	zest_ClearVertexAttributeDescriptions(app->skybox_pipeline);
	zest_AddVertexInputBindingDescription(app->skybox_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexAttribute(app->skybox_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);                                         // Location 0: Vertex Position
	zest_SetPipelineShaders(app->skybox_pipeline, skybox_vert, skybox_frag);
	zest_SetPipelineFrontFace(app->skybox_pipeline, zest_front_face_clockwise);
	zest_SetPipelineDepthTest(app->skybox_pipeline, false, false);

	zest_mesh planet = LoadGLTFScene(app->context, "examples/assets/gltf/lavaplanet.gltf", 1.f);
	zest_mesh rock = LoadGLTFScene(app->context, "examples/assets/gltf/rock01.gltf", 1.f);
	zest_mesh cube = zest_CreateCube(app->context, 1.f, zest_ColorSet(0, 50, 100, 255));

	zest_size planet_vertex_capacity = zest_MeshVertexDataSize(planet);
	zest_size planet_index_capacity = zest_MeshIndexDataSize(planet);
	zest_size rock_vertex_capacity = zest_MeshVertexDataSize(rock);
	zest_size rock_index_capacity = zest_MeshIndexDataSize(rock);

	app->rock_layer = zest_CreateInstanceMeshLayer(app->context, "Mesh Layer", sizeof(mesh_instance_t), rock_vertex_capacity, rock_index_capacity);
	app->rock_mesh_index = zest_AddMeshToLayer(zest_GetLayer(app->rock_layer), rock, 0);

	app->skybox_layer = zest_CreateInstanceMeshLayer(app->context, "Skybox Layer", sizeof(mesh_instance_t), zest_MeshVertexDataSize(cube), zest_MeshIndexDataSize(cube));
	app->skybox_mesh_index = zest_AddMeshToLayer(zest_GetLayer(app->skybox_layer), cube, 0);

	zest_buffer_info_t index_info = zest_CreateBufferInfo(zest_buffer_type_index, zest_memory_usage_gpu_only);
	zest_buffer_info_t vertex_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_gpu_only);
	app->planet_mesh.index_count = zest_MeshIndexCount(planet);
	app->planet_mesh.vertex_count = zest_MeshVertexCount(planet);
	app->planet_mesh.index_buffer = zest_CreateBuffer(app->device, planet_index_capacity, &index_info);
	app->planet_mesh.vertex_buffer = zest_CreateBuffer(app->device, planet_vertex_capacity, &vertex_info);
	zest_buffer index_staging = zest_CreateStagingBuffer(app->device, planet_index_capacity, zest_MeshIndexData(planet));
	zest_buffer vertex_staging = zest_CreateStagingBuffer(app->device, planet_vertex_capacity, zest_MeshVertexData(planet));
	zest_queue queue = zest_imm_BeginCommandBuffer(app->device, zest_queue_transfer);
	zest_imm_CopyBuffer(queue, index_staging, app->planet_mesh.index_buffer, planet_index_capacity);
	zest_imm_CopyBuffer(queue, vertex_staging, app->planet_mesh.vertex_buffer, planet_vertex_capacity);
	zest_imm_EndCommandBuffer(queue);

	app->lavaplanet_texture = zest_LoadKTX(app->device, "Lavaplanet Texture", "examples/assets/lavaplanet_rgba.ktx");
	app->rock_textures = zest_LoadKTX(app->device, "Rock Textures", "examples/assets/texturearray_rocks_rgba.ktx");
	app->skybox_texture = zest_LoadKTX(app->device, "Space Background Texture", "examples/assets/cubemap_space.ktx");

	app->push.planet_index = zest_AcquireSampledImageIndex(app->device, zest_GetImage(app->lavaplanet_texture), zest_texture_2d_binding);
	app->push.rock_index = zest_AcquireSampledImageIndex(app->device, zest_GetImage(app->rock_textures), zest_texture_array_binding);
	app->push.skybox_index = zest_AcquireSampledImageIndex(app->device, zest_GetImage(app->skybox_texture), zest_texture_cube_binding);

	zest_FreeMesh(planet);
	zest_FreeMesh(rock);
	zest_FreeMesh(cube);
	zest_FreeBuffer(index_staging);
	zest_FreeBuffer(vertex_staging);

	PrepareInstanceData(app);
}

void PrepareInstanceData(InstancingExample *app) {
	zest_size instance_data_size = INSTANCE_COUNT * sizeof(mesh_instance_t);
	mesh_instance_t *instance_data = (mesh_instance_t*)malloc(instance_data_size);

	zest_image rock_image = zest_GetImage(app->rock_textures);
	std::default_random_engine rndGenerator(zest_Millisecs());
	std::uniform_real_distribution<float> uniformDist(0.0, 1.0);
	std::uniform_int_distribution<uint32_t> rndTextureIndex(0, zest_ImageInfo(rock_image)->layer_count);

	// Distribute rocks randomly on two different rings
	for (auto i = 0; i < INSTANCE_COUNT / 2; i++) {
		float ring0[2] = { 7.0f, 11.0f };
		float ring1[2] = { 14.0f, 18.0f };

		float rho, theta;

		// Inner ring
		rho = sqrt((pow(ring0[1], 2.0f) - pow(ring0[0], 2.0f)) * uniformDist(rndGenerator) + pow(ring0[0], 2.0f));
		theta = static_cast<float>(2.0f * ZEST_PI * uniformDist(rndGenerator));
		instance_data[i].pos = { rho * cos(theta), uniformDist(rndGenerator) * 0.5f - 0.25f, rho * sin(theta) };
		instance_data[i].rotation = { ZEST_PI * uniformDist(rndGenerator), ZEST_PI * uniformDist(rndGenerator), ZEST_PI * uniformDist(rndGenerator) };
		float scale = (1.5f + uniformDist(rndGenerator) - uniformDist(rndGenerator)) * .75f;
		instance_data[i].texture_layer_index = rndTextureIndex(rndGenerator);
		instance_data[i].scale = { scale, scale, scale };

		// Outer ring
		rho = sqrt((pow(ring1[1], 2.0f) - pow(ring1[0], 2.0f)) * uniformDist(rndGenerator) + pow(ring1[0], 2.0f));
		theta = static_cast<float>(2.0f * ZEST_PI * uniformDist(rndGenerator));
		instance_data[i + INSTANCE_COUNT / 2].pos = { rho * cos(theta), uniformDist(rndGenerator) * 0.5f - 0.25f, rho * sin(theta) };
		instance_data[i + INSTANCE_COUNT / 2].rotation = { ZEST_PI * uniformDist(rndGenerator), ZEST_PI * uniformDist(rndGenerator), ZEST_PI * uniformDist(rndGenerator) };
		scale = (1.5f + uniformDist(rndGenerator) - uniformDist(rndGenerator)) * .75f;
		instance_data[i + INSTANCE_COUNT / 2].texture_layer_index = rndTextureIndex(rndGenerator);
		instance_data[i + INSTANCE_COUNT / 2].scale = { scale, scale, scale };
	}

	zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_gpu_only);
	app->rock_instances_buffer = zest_CreateBuffer(app->device, instance_data_size, &buffer_info);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(app->device, instance_data_size, instance_data);
	zest_queue queue = zest_imm_BeginCommandBuffer(app->device, zest_queue_transfer);
	zest_imm_CopyBuffer(queue, staging_buffer, app->rock_instances_buffer, instance_data_size);
	zest_imm_EndCommandBuffer(queue);
	zest_FreeBuffer(staging_buffer);

	free(instance_data);
}

void UpdateUniform3d(InstancingExample *app) {
	zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);
	uniform_buffer_data_t *ubo_ptr = static_cast<uniform_buffer_data_t *>(zest_GetUniformBufferData(view_buffer));
	ubo_ptr->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	ubo_ptr->proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf(app->context) / zest_ScreenHeightf(app->context), 0.001f, 10000.f);
	ubo_ptr->proj.v[1].y *= -1.f;
	// Animate asteroids
	app->loc_speed += app->frame_timer * 0.35f;
	app->glob_speed += app->frame_timer * 0.01f;
	ubo_ptr->locSpeed = app->loc_speed;
	ubo_ptr->globSpeed = app->glob_speed;
	app->push.ubo_index = zest_GetUniformBufferDescriptorIndex(view_buffer);
}

void UpdateCameraPosition(InstancingExample *app) {
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

void UploadMeshData(const zest_command_list command_list, void *user_data) {
	InstancingExample *app = (InstancingExample *)user_data;

	zest_layer layer = zest_GetLayer(app->rock_layer);
	zest_UploadLayerStagingData(layer, command_list);
}

void UpdateMouse(InstancingExample *app) {
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

void UpdateImGui(InstancingExample *app) {
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
		ImGui::End();
		ImGui::Render();
		UpdateCameraPosition(app);
	} zest_EndTimerLoop(app->timer);
}

void RenderGeometry(zest_command_list command_list, void *user_data) {
	InstancingExample *app = (InstancingExample*)user_data;

	//Render the skybox
	zest_layer skybox_layer = zest_GetLayer(app->skybox_layer);
	zest_cmd_BindMeshVertexBuffer(command_list, skybox_layer);
	zest_cmd_BindMeshIndexBuffer(command_list, skybox_layer);

	zest_pipeline pipeline = zest_GetPipeline(app->skybox_pipeline, command_list);
	if (pipeline) {
		zest_cmd_BindPipeline(command_list, pipeline);
		zest_cmd_SendPushConstants(command_list, &app->push, sizeof(instance_push_t));

		const zest_mesh_offset_data_t *mesh_offsets = zest_GetLayerMeshOffsets(skybox_layer, app->skybox_mesh_index);

		zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);
		zest_cmd_DrawIndexed(command_list, mesh_offsets->index_count, 1, mesh_offsets->index_offset, mesh_offsets->vertex_offset, 0);
	}

	//Render the rock instances
	zest_layer rock_layer = zest_GetLayer(app->rock_layer);

	zest_cmd_BindMeshVertexBuffer(command_list, rock_layer);
	zest_cmd_BindMeshIndexBuffer(command_list, rock_layer);
	zest_cmd_BindVertexBuffer(command_list, 1, 1, app->rock_instances_buffer);

	pipeline = zest_GetPipeline(app->rock_pipeline, command_list);
	if (pipeline) {
		zest_cmd_BindPipeline(command_list, pipeline);

		const zest_mesh_offset_data_t *mesh_offsets = zest_GetLayerMeshOffsets(rock_layer, app->rock_mesh_index);

		zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);
		zest_cmd_SendPushConstants(command_list, &app->push, sizeof(instance_push_t));
		zest_cmd_DrawIndexed(command_list, mesh_offsets->index_count, INSTANCE_COUNT, mesh_offsets->index_offset, mesh_offsets->vertex_offset, 0);
	}

	//Render the planet mesh
	zest_cmd_BindVertexBuffer(command_list, 0, 1, app->planet_mesh.vertex_buffer);
	zest_cmd_BindIndexBuffer(command_list, app->planet_mesh.index_buffer);

	pipeline = zest_GetPipeline(app->planet_pipeline, command_list);
	if (!pipeline) {
		return;
	}
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_SendPushConstants(command_list, &app->push, sizeof(instance_push_t));

	zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);
	zest_cmd_DrawIndexed(command_list, app->planet_mesh.index_count, 1, 0, 0, 0);
}

void MainLoop(InstancingExample *app) {
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

		if (zest_BeginFrame(app->context)) {

			UpdateMouse(app);

			float elapsed = (float)current_frame_time;
			app->frame_timer = (float)elapsed / ZEST_MICROSECS_SECOND;

			UpdateUniform3d(app);

			UpdateImGui(app);

			app->camera.position = zest_LerpVec3(&app->old_camera_position, &app->new_camera_position, (float)zest_TimerLerp(&app->timer));

			//Initially when the 3 textures that are created using compute shaders in the setup they will be in 
			//image layout general. When they are used in the frame graph below they will be transitioned to read only
			//so we store the current layout of the image in a custom cache info struct so that when the layout changes
			//the cache key will change and a new cache will be created as a result. The other option is to transition 
			//them before hand but this is just to show an example of how the frame graph caching can work.
			app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw(&app->imgui);
			zest_frame_graph_cache_key_t cache_key = {};
			cache_key = zest_InitialiseCacheKey(app->context, &app->cache_info, sizeof(RenderCacheInfo));

			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);
			//Begin the render graph with the command that acquires a swap chain image (zest_BeginFrameGraphSwapchain)
			//Use the render graph we created earlier. Will return false if a swap chain image could not be acquired. This will happen
			//if the window is resized for example.
			if (!frame_graph) {
				zest_image_resource_info_t depth_info = {
					zest_format_depth,
					zest_resource_usage_hint_none,
					zest_ScreenWidth(app->context),
					zest_ScreenHeight(app->context),
					1, 1
				};

				zest_layer rock_layer = zest_GetLayer(app->rock_layer);
				zest_layer skybox_layer = zest_GetLayer(app->skybox_layer);

				zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);
				if (zest_BeginFrameGraph(app->context, "ImGui", &cache_key)) {
					zest_resource_node swapchain_node = zest_ImportSwapchainResource();
					zest_resource_node depth_buffer = zest_AddTransientImageResource("Depth Buffer", &depth_info);
					zest_resource_group group = zest_CreateResourceGroup();
					zest_AddSwapchainToGroup(group);
					zest_AddResourceToGroup(group, depth_buffer);

					//Note: There's no need for a transfer pass here because all the instancing/geometry data
					//is uploaded at the start in initialisation.

					//------------------------ Instancing Layer Pass ------------------------------------------------------------
					zest_BeginRenderPass("Instancing Pass"); {
						zest_ConnectOutputGroup(group);
						zest_SetPassTask(RenderGeometry, app);
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
		}

		if (zest_SwapchainWasRecreated(app->context)) {
		}
	}
}

int main(void) {

	if (!glfwInit()) {
		return 0;
	}

	InstancingExample imgui_app = {};

	//Create the device that serves all vulkan based contexts
	imgui_app.device = zest_implglfw_CreateVulkanDevice(false);

	zest_SetStagingBufferPoolSize(imgui_app.device, zloc__KILOBYTE(256), zloc__MEGABYTE(128));

	//Create a window using GLFW
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");

	//Create new config struct for Zest
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	//Initialise Zest
	imgui_app.context = zest_CreateContext(imgui_app.device, &window_handles, &create_info);

	//Initialise our example
	InitInstancingExample(&imgui_app);

	//Start the main loop
	MainLoop(&imgui_app);
	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Destroy(&imgui_app.imgui);
	zest_DestroyDevice(imgui_app.device);

	return 0;
}
