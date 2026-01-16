#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_ALL_UTILITIES_IMPLEMENTATION
#define ZEST_TEST_MODE
#include "zest-shadow-mapping.h"
#include "zest.h"
#include "imgui_internal.h"

/*
Example recreated from Sascha Willems "Shadow mapping for directional light sources" 
https://github.com/SaschaWillems/Vulkan/tree/master/examples/shadowmapping
*/

void InitShadowMappingExample(ShadowMappingExample *app) {
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
	float position[3] = { 6.f, 6.f, 7.5f };
	zest_CameraPosition(&app->camera, position);
	zest_CameraSetFoV(&app->camera, 60.f);
	zest_CameraSetYaw(&app->camera, -135.f);
	zest_CameraSetPitch(&app->camera, -27.f);
	zest_CameraUpdateFront(&app->camera);
	app->new_camera_position = app->camera.position;
	app->old_camera_position = app->camera.position;

	//We can use a timer to only update imgui 60 times per second
	app->timer = zest_CreateTimer(60);

	app->view_buffer = zest_CreateUniformBuffer(app->context, "View", sizeof(uniform_buffer_data_t));
	app->offscreen_uniform_buffer = zest_CreateUniformBuffer(app->context, "Offscreen", sizeof(uniform_data_offscreen_t));

	//Compile the shaders we will use to render the particles
	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	app->sampler_2d = zest_CreateSampler(app->context, &sampler_info);

	zest_sampler sampler_2d = zest_GetSampler(app->sampler_2d);

	app->scene_push.sampler_index = zest_AcquireSamplerIndex(app->device, sampler_2d);

	zest_shader_handle scene_vert = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-shadow-mapping/shaders/scene.vert", "scene_vert.spv", zest_vertex_shader, true);
	zest_shader_handle scene_frag = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-shadow-mapping/shaders/scene.frag", "scene_frag.spv", zest_fragment_shader, true);
	zest_shader_handle offscreen_vert = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-shadow-mapping/shaders/offscreen.vert", "offscreen_vert.spv", zest_vertex_shader, true);

	app->mesh_pipeline = zest_CreatePipelineTemplate(app->device, "pipeline_mesh_instance");
	zest_AddVertexInputBindingDescription(app->mesh_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexInputBindingDescription(app->mesh_pipeline, 1, sizeof(mesh_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(app->mesh_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);                                          // Location 0: Vertex Position
	zest_AddVertexAttribute(app->mesh_pipeline, 0, 1, zest_format_r8g8b8a8_unorm, offsetof(zest_vertex_t, color));               // Location 1: Vertex Color
	zest_AddVertexAttribute(app->mesh_pipeline, 0, 2, zest_format_r32g32b32_sfloat, offsetof(zest_vertex_t, normal));            // Location 2: Vertex Position
	zest_AddVertexAttribute(app->mesh_pipeline, 0, 3, zest_format_r16g16_unorm, offsetof(zest_vertex_t, uv));                     // Location 3: Group id
	zest_AddVertexAttribute(app->mesh_pipeline, 1, 4, zest_format_r32g32b32_sfloat, 0);                                          // Location 4: Instance Position
	zest_AddVertexAttribute(app->mesh_pipeline, 1, 5, zest_format_r8g8b8a8_unorm, offsetof(mesh_instance_t, color));        // Location 5: Instance Color
	zest_AddVertexAttribute(app->mesh_pipeline, 1, 6, zest_format_r32g32b32_sfloat, offsetof(mesh_instance_t, rotation));   // Location 6: Instance Rotation
	zest_AddVertexAttribute(app->mesh_pipeline, 1, 7, zest_format_r32g32b32_sfloat, offsetof(mesh_instance_t, scale));      // Location 7: Instance Scale

	zest_SetPipelineShaders(app->mesh_pipeline, scene_vert, scene_frag);
	zest_SetPipelineCullMode(app->mesh_pipeline, zest_cull_mode_back);
	zest_SetPipelineFrontFace(app->mesh_pipeline, zest_front_face_counter_clockwise);
	zest_SetPipelineTopology(app->mesh_pipeline, zest_topology_triangle_list);
	zest_SetPipelineDepthTest(app->mesh_pipeline, true, true);
	
	app->shadow_pipeline = zest_CreatePipelineTemplate(app->device, "Shadow pipeline");
	zest_AddVertexInputBindingDescription(app->shadow_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexInputBindingDescription(app->shadow_pipeline, 1, sizeof(mesh_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(app->shadow_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);                                          // Location 0: Vertex Position
	zest_AddVertexAttribute(app->shadow_pipeline, 1, 1, zest_format_r32g32b32_sfloat, 0);                                          // Location 1: Instance Position
	zest_AddVertexAttribute(app->shadow_pipeline, 1, 2, zest_format_r8g8b8a8_unorm, offsetof(mesh_instance_t, color));        // Location 2: Instance Color
	zest_AddVertexAttribute(app->shadow_pipeline, 1, 3, zest_format_r32g32b32_sfloat, offsetof(mesh_instance_t, rotation));   // Location 3: Instance Rotation
	zest_AddVertexAttribute(app->shadow_pipeline, 1, 4, zest_format_r32g32b32_sfloat, offsetof(mesh_instance_t, scale));      // Location 4: Instance Scale
	zest_SetPipelineFrontFace(app->shadow_pipeline, zest_front_face_counter_clockwise);
	zest_SetPipelineVertShader(app->shadow_pipeline, offscreen_vert);
	zest_SetPipelineDepthBias(app->shadow_pipeline, ZEST_TRUE);
	zest_SetPipelineDepthTest(app->shadow_pipeline, true, true);

	zest_mesh vulkan_scene = LoadGLTFScene(app->context, "examples/assets/gltf/vulkanscene_shadow.gltf", 1.f);
	zest_size vertex_capacity = zest_MeshVertexDataSize(vulkan_scene);
	zest_size index_capacity = zest_MeshIndexDataSize(vulkan_scene);

	app->mesh_layer = zest_CreateInstanceMeshLayer(app->context, "Mesh Layer", sizeof(mesh_instance_t), vertex_capacity, index_capacity);
	app->vulkanscene_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), vulkan_scene, 0);
	app->light_fov = 45;
	app->light_position = {30.f, 50.f, 25.f};
	app->scene_push.enable_pcf = 1;

	zest_FreeMesh(vulkan_scene);
}

void UpdateLight(ShadowMappingExample *app) {
	app->light_position.x = cos(zest_Radians(app->frame_timer * 360.0f)) * 40.0f;
	app->light_position.y = 50.0f + sin(zest_Radians(app->frame_timer * 360.0f)) * 20.0f;
	app->light_position.z = 25.0f + sin(zest_Radians(app->frame_timer * 360.0f)) * 5.0f;
}

void UpdateUniform3d(ShadowMappingExample *app) {
	zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);
	zest_uniform_buffer offscreen_uniform_buffer = zest_GetUniformBuffer(app->offscreen_uniform_buffer);
	uniform_data_offscreen_t *offscreen_ptr = static_cast<uniform_data_offscreen_t *>(zest_GetUniformBufferData(offscreen_uniform_buffer));
	// Matrix from light's point of view for generating the shadow map
	zest_matrix4 depthProjectionMatrix = zest_Perspective(zest_Radians(app->light_fov), 1.0f, app->z_near, app->z_far);
	zest_matrix4 depthViewMatrix = zest_LookAt(app->light_position, { 0.f }, {0, 1, 0});
	zest_matrix4 depthModelMatrix = zest_M4(1.f);
	offscreen_ptr->depth_mvp = zest_MatrixTransform(&depthProjectionMatrix, &depthViewMatrix); 
	offscreen_ptr->depth_mvp = zest_MatrixTransform(&depthModelMatrix, &offscreen_ptr->depth_mvp);

	// Uniform data for drawing the scene
	uniform_buffer_data_t *view_ptr = static_cast<uniform_buffer_data_t *>(zest_GetUniformBufferData(view_buffer));
	view_ptr->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	view_ptr->projection = zest_Perspective(app->camera.fov, zest_ScreenWidthf(app->context) / zest_ScreenHeightf(app->context), 0.001f, 10000.f);
	view_ptr->projection.v[1].y *= -1.f;
	view_ptr->model = zest_M4(1.f);
	view_ptr->light_pos = { app->light_position.x, app->light_position.y, app->light_position.z, 1.f };
	view_ptr->light_space = offscreen_ptr->depth_mvp;
	view_ptr->z_near = app->z_near;
	view_ptr->z_far = app->z_far;
}

void UploadMeshData(const zest_command_list context, void *user_data) {
	ShadowMappingExample *app = (ShadowMappingExample *)user_data;

	zest_layer layer = zest_GetLayer(app->mesh_layer);
	zest_UploadLayerStagingData(layer, context);
}

void DrawSceneDepth(const zest_command_list command_list, void *user_data) {
	ShadowMappingExample *app = (ShadowMappingExample *)user_data;
	zest_layer mesh_layer = zest_GetLayer(app->mesh_layer);

	zest_cmd_SetDepthBias(command_list, 1.25f, 0.f, 1.75f);
	zest_SetLayerViewPort(mesh_layer, 0, 0, 2048, 2048, 2048, 2048);
	zest_DrawInstanceMeshLayer(command_list, mesh_layer);
}

void DrawSceneWithShadows(const zest_command_list command_list, void *user_data) {
	ShadowMappingExample *app = (ShadowMappingExample *)user_data;

	zest_resource_node shadow_resource = zest_GetPassInputResource(command_list, "Shadow Buffer"); 

	app->scene_push.shadow_index = zest_GetTransientSampledImageBindlessIndex(command_list, shadow_resource, zest_texture_2d_binding);
	zest_layer mesh_layer = zest_GetLayer(app->mesh_layer);
	zest_SetLayerPushConstants(mesh_layer, &app->scene_push, sizeof(scene_push_constants_t));
	zest_SetLayerSizeToSwapchain(mesh_layer);
	zest_DrawInstanceMeshLayerWithPipeline(command_list, mesh_layer, app->mesh_pipeline);
}

void UpdateCameraPosition(ShadowMappingExample *app) {
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

void UpdateMouse(ShadowMappingExample *app) {
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

void DrawInstancedMesh(zest_layer layer, float pos[3], float rot[3], float scale[3]) {
    mesh_instance_t* instance = (mesh_instance_t*)zest_NextInstance(layer);
	instance->pos = { pos[0], pos[1], pos[2] };
	instance->rotation = { rot[0], rot[1], rot[2] };
	instance->scale = { scale[0], scale[1], scale[2] };
    instance->color = layer->current_color;
}

void UpdateImGui(ShadowMappingExample *app) {
	//We can use a timer to only update the gui every 60 times a second (or whatever you decide) for a little less
	//cpu usage.
	zest_StartTimerLoop(app->timer) {
		//Must call the imgui GLFW implementation function
		ImGui_ImplGlfw_NewFrame();
		//Draw our imgui stuff
		ImGui::NewFrame();
		ImGui::Begin("Test Window");
		ImGui::Text("FPS: %u", app->fps);
		ImGui::Checkbox("PCF Filtering", &app->scene_push.enable_pcf);
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

void MainLoop(ShadowMappingExample *app) {
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
			float frame_timer = (float)elapsed / ZEST_MICROSECS_SECOND;
			app->frame_timer += 0.25f * frame_timer;
			if (app->frame_timer > 1.0)
			{
				app->frame_timer -= 1.0f;
			}

			UpdateLight(app);

			UpdateUniform3d(app);

			UpdateImGui(app);

			app->camera.position = zest_LerpVec3(&app->old_camera_position, &app->new_camera_position, (float)zest_TimerLerp(&app->timer));

			zest_layer mesh_layer = zest_GetLayer(app->mesh_layer);
			zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);
			zest_uniform_buffer offscreen_buffer = zest_GetUniformBuffer(app->offscreen_uniform_buffer);
			zest_StartInstanceMeshDrawing(mesh_layer, 0, app->shadow_pipeline);
			zest_SetLayerColor(mesh_layer, 255, 255, 255, 255);
			app->scene_push.view_index = zest_GetUniformBufferDescriptorIndex(view_buffer);
			app->scene_push.offscreen_index = zest_GetUniformBufferDescriptorIndex(offscreen_buffer);
			zest_SetLayerPushConstants(mesh_layer, &app->scene_push, sizeof(scene_push_constants_t));
			zest_vec3 position = { 0.f };
			zest_vec3 scale = { 1.f, 1.f, 1.f };
			float zero[3] = { 0.f, 0.f, 0.f };
			DrawInstancedMesh(mesh_layer, &position.x, zero, &scale.x);

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
					zest_ScreenWidth(app->context), zest_ScreenHeight(app->context),
					1, 1
				};

				zest_image_resource_info_t shadow_info = {
					zest_format_d16_unorm,
					zest_resource_usage_hint_none,
					2048, 2048,
					1, 1
				};

				zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);
				if (zest_BeginFrameGraph(app->context, "ImGui", &cache_key)) {
					zest_resource_node mesh_layer_resource = zest_AddTransientLayerResource("Mesh Layer", mesh_layer, false);
					zest_resource_node swapchain_node = zest_ImportSwapchainResource();
					zest_resource_node depth_buffer = zest_AddTransientImageResource("Depth Buffer", &depth_info);
					zest_resource_node shadow_buffer = zest_AddTransientImageResource("Shadow Buffer", &shadow_info);
					zest_resource_group group = zest_CreateResourceGroup();
					zest_AddSwapchainToGroup(group);
					zest_AddResourceToGroup(group, depth_buffer);

					//-------------------------Transfer Pass----------------------------------------------------
					zest_BeginTransferPass("Upload Mesh Data"); {
						zest_ConnectOutput(mesh_layer_resource);
						zest_SetPassTask(UploadMeshData, app);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					zest_BeginRenderPass("Shadow Pass"); {
						zest_ConnectInput(mesh_layer_resource);
						zest_ConnectOutput(shadow_buffer);
						zest_SetPassTask(DrawSceneDepth, app);
						zest_EndPass();
					}

					//------------------------ Mesh Layer Pass ------------------------------------------------------------
					zest_BeginRenderPass("Mesh Layer Pass"); {
						zest_ConnectInput(mesh_layer_resource);
						zest_ConnectInput(shadow_buffer);
						zest_ConnectOutputGroup(group);
						zest_SetPassTask(DrawSceneWithShadows, app);
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

	ShadowMappingExample imgui_app = {};

	//Create the device that serves all vulkan based contexts
	imgui_app.device = zest_implglfw_CreateVulkanDevice(false);

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
	InitShadowMappingExample(&imgui_app);

	//Start the main loop
	MainLoop(&imgui_app);
	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Destroy(&imgui_app.imgui);
	zest_DestroyDevice(imgui_app.device);

	return 0;
}
