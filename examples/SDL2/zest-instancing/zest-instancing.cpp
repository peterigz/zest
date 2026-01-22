// Define implementations in exactly one .cpp file before including headers
#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_ALL_UTILITIES_IMPLEMENTATION
#include "zest-instancing.h"
#include "zest.h"
#include "imgui_internal.h"
#include <random>
#include "examples/Common/sdl_controls.cpp"
#include "examples/Common/sdl_events.cpp"

/*
Instanced Mesh Rendering Example
--------------------------------
Demonstrates GPU instancing to render thousands of meshes efficiently with a single draw call.
Based on Sascha Willems' Vulkan instancing example.

This example shows:
- Custom vertex input layouts for per-instance data (position, rotation, scale, texture index)
- Multiple render pipelines (rocks, planet, skybox) with different vertex formats
- Texture arrays for per-instance texture selection
- Cubemap skybox rendering
- Depth buffer usage with resource groups

Key concepts:
- Instance data is uploaded once at init, not per-frame (GPU-only buffer)
- Animation is done via uniforms (rotation speeds), not by updating instance data
*/

#define INSTANCE_COUNT 8192		// Number of asteroid instances to render

void InitInstancingExample(InstancingExample *app) {
	// Initialize Dear ImGui with Zest and GLFW backend
	zest_imgui_Initialise(app->context, &app->imgui, zest_implsdl2_DestroyWindow);
    ImGui_ImplSDL2_InitForVulkan((SDL_Window *)zest_Window(app->context));
	zest_imgui_DarkStyle(&app->imgui);

	// Custom ImGui font setup - demonstrates how to use a different font
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
	zest_imgui_RebuildFontTexture(&app->imgui, tex_width, tex_height, font_data);

	// Setup 3D camera with initial position looking at the planet
	app->camera = zest_CreateCamera();
	float position[3] = { -15.5f, 5.f, 0.f };
	zest_CameraPosition(&app->camera, position);
	zest_CameraSetFoV(&app->camera, 60.f);
	zest_CameraSetYaw(&app->camera, 0.f);
	zest_CameraSetPitch(&app->camera, -25.f);
	zest_CameraUpdateFront(&app->camera);
	app->new_camera_position = app->camera.position;
	app->old_camera_position = app->camera.position;

	// Timer for rate-limiting ImGui updates
	app->timer = zest_CreateTimer(60);

	// Uniform buffer for view/projection matrices and animation parameters
	app->view_buffer = zest_CreateUniformBuffer(app->context, "View", sizeof(uniform_buffer_data_t));

	// Create sampler for textures (repeat mode for tiling)
	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	sampler_info.address_mode_u = zest_sampler_address_mode_repeat;
	sampler_info.address_mode_v = zest_sampler_address_mode_repeat;
	sampler_info.address_mode_w = zest_sampler_address_mode_repeat;
	app->sampler_2d = zest_CreateSampler(app->context, &sampler_info);
	zest_sampler sampler_2d = zest_GetSampler(app->sampler_2d);
	// Bindless: acquire descriptor index for use in shaders via push constants
	app->push.sampler_index = zest_AcquireSamplerIndex(app->device, sampler_2d);

	zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);

	// Load shaders for each pipeline (rocks, planet, skybox)
	// Last param (true) = compile from source if .spv is outdated
	zest_shader_handle instance_vert = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-instancing/shaders/instancing.vert", "instancing_vert.spv", zest_vertex_shader, true);
	zest_shader_handle instance_frag = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-instancing/shaders/instancing.frag", "instancing_frag.spv", zest_fragment_shader, true);
	zest_shader_handle planet_vert = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-instancing/shaders/planet.vert", "planet_vert.spv", zest_vertex_shader, true);
	zest_shader_handle planet_frag = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-instancing/shaders/planet.frag", "planet_frag.spv", zest_fragment_shader, true);
	zest_shader_handle skybox_vert = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-instancing/shaders/sky_box.vert", "skybox_vert.spv", zest_vertex_shader, true);
	zest_shader_handle skybox_frag = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-instancing/shaders/sky_box.frag", "skybox_frag.spv", zest_fragment_shader, true);

	// Rock pipeline: Uses TWO vertex bindings - mesh vertices (binding 0) and instance data (binding 1)
	// This is the key to GPU instancing: per-vertex data + per-instance data
	app->rock_pipeline = zest_CreatePipelineTemplate(app->device, "pipeline_mesh_instance");
	zest_AddVertexInputBindingDescription(app->rock_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);		// Per-vertex mesh data
	zest_AddVertexInputBindingDescription(app->rock_pipeline, 1, sizeof(mesh_instance_t), zest_input_rate_instance);	// Per-instance transform data
	// Binding 0 attributes (per-vertex)
	zest_AddVertexAttribute(app->rock_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);									// Position
	zest_AddVertexAttribute(app->rock_pipeline, 0, 1, zest_format_r8g8b8a8_unorm, offsetof(zest_vertex_t, color));		// Color
	zest_AddVertexAttribute(app->rock_pipeline, 0, 2, zest_format_r32g32b32_sfloat, offsetof(zest_vertex_t, normal));	// Normal
	zest_AddVertexAttribute(app->rock_pipeline, 0, 3, zest_format_r32g32_sfloat, offsetof(zest_vertex_t, uv));			// UV
	// Binding 1 attributes (per-instance) - each instance gets unique position, rotation, scale, texture
	zest_AddVertexAttribute(app->rock_pipeline, 1, 4, zest_format_r32g32b32_sfloat, 0);									// Instance position
	zest_AddVertexAttribute(app->rock_pipeline, 1, 5, zest_format_r32g32b32_sfloat, offsetof(mesh_instance_t, rotation));	// Instance rotation
	zest_AddVertexAttribute(app->rock_pipeline, 1, 6, zest_format_r32g32b32_sfloat, offsetof(mesh_instance_t, scale));		// Instance scale
	zest_AddVertexAttribute(app->rock_pipeline, 1, 7, zest_format_r32_uint, offsetof(mesh_instance_t, texture_layer_index));	// Texture array index

	zest_SetPipelineShaders(app->rock_pipeline, instance_vert, instance_frag);
	zest_SetPipelineCullMode(app->rock_pipeline, zest_cull_mode_back);
	zest_SetPipelineFrontFace(app->rock_pipeline, zest_front_face_counter_clockwise);
	zest_SetPipelineTopology(app->rock_pipeline, zest_topology_triangle_list);
	zest_SetPipelineDepthTest(app->rock_pipeline, true, true);

	// Planet pipeline: Copy rock pipeline, then modify for single-instance rendering (no instance data)
	app->planet_pipeline = zest_CopyPipelineTemplate("Planet pipeline", app->rock_pipeline);
	zest_ClearVertexInputBindingDescriptions(app->planet_pipeline);
	zest_ClearVertexAttributeDescriptions(app->planet_pipeline);
	zest_AddVertexInputBindingDescription(app->planet_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexAttribute(app->planet_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);
	zest_AddVertexAttribute(app->planet_pipeline, 0, 1, zest_format_r8g8b8a8_unorm, offsetof(zest_vertex_t, color));
	zest_AddVertexAttribute(app->planet_pipeline, 0, 2, zest_format_r32g32b32_sfloat, offsetof(zest_vertex_t, normal));
	zest_AddVertexAttribute(app->planet_pipeline, 0, 3, zest_format_r32g32_sfloat, offsetof(zest_vertex_t, uv));
	zest_SetPipelineShaders(app->planet_pipeline, planet_vert, planet_frag);

	// Skybox pipeline: Only needs position (samples cubemap based on view direction)
	app->skybox_pipeline = zest_CopyPipelineTemplate("Skybox pipeline", app->rock_pipeline);
	zest_ClearVertexInputBindingDescriptions(app->skybox_pipeline);
	zest_ClearVertexAttributeDescriptions(app->skybox_pipeline);
	zest_AddVertexInputBindingDescription(app->skybox_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexAttribute(app->skybox_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);
	zest_SetPipelineShaders(app->skybox_pipeline, skybox_vert, skybox_frag);
	zest_SetPipelineFrontFace(app->skybox_pipeline, zest_front_face_clockwise);		// Inside-out cube
	zest_SetPipelineDepthTest(app->skybox_pipeline, false, false);					// Always behind everything

	// Load GLTF meshes and create a simple cube for the skybox
	zest_mesh planet = LoadGLTFScene(app->context, "examples/assets/gltf/lavaplanet.gltf", 1.f);
	zest_mesh rock = LoadGLTFScene(app->context, "examples/assets/gltf/rock01.gltf", 1.f);
	zest_mesh cube = zest_CreateCube(app->context, 1.f, zest_ColorSet(0, 50, 100, 255));

	zest_size planet_vertex_capacity = zest_MeshVertexDataSize(planet);
	zest_size planet_index_capacity = zest_MeshIndexDataSize(planet);
	zest_size rock_vertex_capacity = zest_MeshVertexDataSize(rock);
	zest_size rock_index_capacity = zest_MeshIndexDataSize(rock);

	// Create mesh layers for instanced rendering
	// Layers manage vertex/index buffers and can hold multiple meshes
	app->rock_layer = zest_CreateInstanceMeshLayer(app->context, "Mesh Layer", sizeof(mesh_instance_t), rock_vertex_capacity, rock_index_capacity);
	app->rock_mesh_index = zest_AddMeshToLayer(zest_GetLayer(app->rock_layer), rock, 0);

	app->skybox_layer = zest_CreateInstanceMeshLayer(app->context, "Skybox Layer", sizeof(mesh_instance_t), zest_MeshVertexDataSize(cube), zest_MeshIndexDataSize(cube));
	app->skybox_mesh_index = zest_AddMeshToLayer(zest_GetLayer(app->skybox_layer), cube, 0);

	// Planet mesh: manually create GPU-only buffers and upload via staging
	// This pattern is useful when you don't need the layer system
	zest_buffer_info_t index_info = zest_CreateBufferInfo(zest_buffer_type_index, zest_memory_usage_gpu_only);
	zest_buffer_info_t vertex_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_gpu_only);
	app->planet_mesh.index_count = zest_MeshIndexCount(planet);
	app->planet_mesh.vertex_count = zest_MeshVertexCount(planet);
	app->planet_mesh.index_buffer = zest_CreateBuffer(app->device, planet_index_capacity, &index_info);
	app->planet_mesh.vertex_buffer = zest_CreateBuffer(app->device, planet_vertex_capacity, &vertex_info);

	// Upload mesh data using immediate mode commands (blocking)
	zest_buffer index_staging = zest_CreateStagingBuffer(app->device, planet_index_capacity, zest_MeshIndexData(planet));
	zest_buffer vertex_staging = zest_CreateStagingBuffer(app->device, planet_vertex_capacity, zest_MeshVertexData(planet));
	zest_queue queue = zest_imm_BeginCommandBuffer(app->device, zest_queue_transfer);
	zest_imm_CopyBuffer(queue, index_staging, app->planet_mesh.index_buffer, planet_index_capacity);
	zest_imm_CopyBuffer(queue, vertex_staging, app->planet_mesh.vertex_buffer, planet_vertex_capacity);
	zest_imm_EndCommandBuffer(queue);

	// Load KTX textures: 2D texture, texture array (multiple rock textures), and cubemap
	app->lavaplanet_texture = zest_LoadKTX(app->device, "Lavaplanet Texture", "examples/assets/lavaplanet_rgba.ktx");
	app->rock_textures = zest_LoadKTX(app->device, "Rock Textures", "examples/assets/texturearray_rocks_rgba.ktx");
	app->skybox_texture = zest_LoadKTX(app->device, "Space Background Texture", "examples/assets/cubemap_space.ktx");

	// Bindless textures: acquire descriptor indices for each texture type
	// Shaders access textures via these indices in push constants
	app->push.planet_index = zest_AcquireSampledImageIndex(app->device, zest_GetImage(app->lavaplanet_texture), zest_texture_2d_binding);
	app->push.rock_index = zest_AcquireSampledImageIndex(app->device, zest_GetImage(app->rock_textures), zest_texture_array_binding);
	app->push.skybox_index = zest_AcquireSampledImageIndex(app->device, zest_GetImage(app->skybox_texture), zest_texture_cube_binding);

	// Free CPU-side mesh data and staging buffers (GPU buffers remain)
	zest_FreeMesh(planet);
	zest_FreeMesh(rock);
	zest_FreeMesh(cube);
	zest_FreeBuffer(index_staging);
	zest_FreeBuffer(vertex_staging);

	// Generate random instance data for the asteroid rings
	PrepareInstanceData(app);
}

// Generate random positions, rotations, scales for 8192 asteroid instances
// Data is uploaded once to a GPU-only buffer (not updated per-frame)
void PrepareInstanceData(InstancingExample *app) {
	zest_size instance_data_size = INSTANCE_COUNT * sizeof(mesh_instance_t);
	mesh_instance_t *instance_data = (mesh_instance_t*)malloc(instance_data_size);

	zest_image rock_image = zest_GetImage(app->rock_textures);
	std::default_random_engine rndGenerator(zest_Millisecs());
	std::uniform_real_distribution<float> uniformDist(0.0, 1.0);
	std::uniform_int_distribution<uint32_t> rndTextureIndex(0, zest_ImageInfo(rock_image)->layer_count);

	// Distribute asteroids randomly across two concentric rings around the planet
	for (auto i = 0; i < INSTANCE_COUNT / 2; i++) {
		float ring0[2] = { 7.0f, 11.0f };	// Inner ring: min/max radius
		float ring1[2] = { 14.0f, 18.0f };	// Outer ring: min/max radius

		float rho, theta;

		// Inner ring asteroid
		rho = sqrt((pow(ring0[1], 2.0f) - pow(ring0[0], 2.0f)) * uniformDist(rndGenerator) + pow(ring0[0], 2.0f));
		theta = static_cast<float>(2.0f * ZEST_PI * uniformDist(rndGenerator));
		instance_data[i].pos = { rho * cos(theta), uniformDist(rndGenerator) * 0.5f - 0.25f, rho * sin(theta) };
		instance_data[i].rotation = { ZEST_PI * uniformDist(rndGenerator), ZEST_PI * uniformDist(rndGenerator), ZEST_PI * uniformDist(rndGenerator) };
		float scale = (1.5f + uniformDist(rndGenerator) - uniformDist(rndGenerator)) * .75f;
		instance_data[i].texture_layer_index = rndTextureIndex(rndGenerator);	// Random texture from array
		instance_data[i].scale = { scale, scale, scale };

		// Outer ring asteroid
		rho = sqrt((pow(ring1[1], 2.0f) - pow(ring1[0], 2.0f)) * uniformDist(rndGenerator) + pow(ring1[0], 2.0f));
		theta = static_cast<float>(2.0f * ZEST_PI * uniformDist(rndGenerator));
		instance_data[i + INSTANCE_COUNT / 2].pos = { rho * cos(theta), uniformDist(rndGenerator) * 0.5f - 0.25f, rho * sin(theta) };
		instance_data[i + INSTANCE_COUNT / 2].rotation = { ZEST_PI * uniformDist(rndGenerator), ZEST_PI * uniformDist(rndGenerator), ZEST_PI * uniformDist(rndGenerator) };
		scale = (1.5f + uniformDist(rndGenerator) - uniformDist(rndGenerator)) * .75f;
		instance_data[i + INSTANCE_COUNT / 2].texture_layer_index = rndTextureIndex(rndGenerator);
		instance_data[i + INSTANCE_COUNT / 2].scale = { scale, scale, scale };
	}

	// Upload instance data to GPU-only buffer via staging buffer
	zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_gpu_only);
	app->rock_instances_buffer = zest_CreateBuffer(app->device, instance_data_size, &buffer_info);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(app->device, instance_data_size, instance_data);
	zest_queue queue = zest_imm_BeginCommandBuffer(app->device, zest_queue_transfer);
	zest_imm_CopyBuffer(queue, staging_buffer, app->rock_instances_buffer, instance_data_size);
	zest_imm_EndCommandBuffer(queue);
	zest_FreeBuffer(staging_buffer);

	free(instance_data);
}

// Update view/projection matrices and animation parameters in the uniform buffer
// Animation is done here via uniforms, not by updating instance data
void UpdateUniform3d(InstancingExample *app) {
	zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);
	uniform_buffer_data_t *ubo_ptr = static_cast<uniform_buffer_data_t *>(zest_GetUniformBufferData(view_buffer));
	ubo_ptr->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	ubo_ptr->proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf(app->context) / zest_ScreenHeightf(app->context), 0.001f, 10000.f);
	ubo_ptr->proj.v[1].y *= -1.f;	// Flip Y for Vulkan coordinate system

	// Asteroid animation: rotation speeds passed to shader via uniform buffer
	// The shader applies these to instance positions/rotations
	app->loc_speed += app->frame_timer * 0.35f;
	app->glob_speed += app->frame_timer * 0.01f;
	ubo_ptr->locSpeed = app->loc_speed;
	ubo_ptr->globSpeed = app->glob_speed;

	// Bindless: store uniform buffer descriptor index for shader access
	app->push.ubo_index = zest_GetUniformBufferDescriptorIndex(view_buffer);
}

// Callback for frame graph transfer pass (unused in this example - data uploaded at init)
void UploadMeshData(const zest_command_list command_list, void *user_data) {
	InstancingExample *app = (InstancingExample *)user_data;
	zest_layer layer = zest_GetLayer(app->rock_layer);
	zest_UploadLayerStagingData(layer, command_list);
}

// Rate-limited ImGui update
void UpdateImGui(InstancingExample *app) {
	zest_StartTimerLoop(app->timer) {
		ImGui_ImplSDL2_NewFrame();
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
		UpdateCameraPosition(&app->timer, &app->new_camera_position, &app->old_camera_position, &app->camera, 5.f);
	} zest_EndTimerLoop(app->timer);
}

// Render callback for the instancing pass - renders skybox, asteroids, and planet
void RenderGeometry(zest_command_list command_list, void *user_data) {
	InstancingExample *app = (InstancingExample*)user_data;

	// 1. Render skybox first (no depth write, always behind everything)
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

	// 2. Render asteroid instances - single draw call for 8192 meshes
	// Binding 0: mesh vertices, Binding 1: instance transform data
	zest_layer rock_layer = zest_GetLayer(app->rock_layer);
	zest_cmd_BindMeshVertexBuffer(command_list, rock_layer);
	zest_cmd_BindMeshIndexBuffer(command_list, rock_layer);
	zest_cmd_BindVertexBuffer(command_list, 1, 1, app->rock_instances_buffer);	// Instance data at binding 1

	pipeline = zest_GetPipeline(app->rock_pipeline, command_list);
	if (pipeline) {
		zest_cmd_BindPipeline(command_list, pipeline);
		const zest_mesh_offset_data_t *mesh_offsets = zest_GetLayerMeshOffsets(rock_layer, app->rock_mesh_index);
		zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);
		zest_cmd_SendPushConstants(command_list, &app->push, sizeof(instance_push_t));
		// DrawIndexed with INSTANCE_COUNT: renders the same mesh 8192 times with different transforms
		zest_cmd_DrawIndexed(command_list, mesh_offsets->index_count, INSTANCE_COUNT, mesh_offsets->index_offset, mesh_offsets->vertex_offset, 0);
	}

	// 3. Render planet - single instance, different pipeline
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
	// FPS tracking
	zest_microsecs running_time = zest_Microsecs();
	zest_microsecs frame_time = 0;
	zest_uint frame_count = 0;
	int running = 1;
	SDL_Event event;

	while (running) {
		running = PollSDLEvents(app->context, &event);
		zest_microsecs current_frame_time = zest_Microsecs() - running_time;
		running_time = zest_Microsecs();
		frame_time += current_frame_time;
		frame_count += 1;

		if (frame_time >= ZEST_MICROSECS_SECOND) {
			frame_time -= ZEST_MICROSECS_SECOND;
			app->fps = frame_count;
			frame_count = 0;
		}

		zest_UpdateDevice(app->device);

		if (zest_BeginFrame(app->context)) {
			// Update camera from mouse input
			UpdateMouse(app->context, &app->mouse, &app->camera);

			// Calculate delta time for animation
			float elapsed = (float)current_frame_time;
			app->frame_timer = (float)elapsed / ZEST_MICROSECS_SECOND;

			UpdateUniform3d(app);
			UpdateImGui(app);

			// Interpolate camera position for smooth movement
			app->camera.position = zest_LerpVec3(&app->old_camera_position, &app->new_camera_position, (float)zest_TimerLerp(&app->timer));

			// Frame graph caching: rebuild when draw_imgui state changes
			app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw(&app->imgui);
			zest_frame_graph_cache_key_t cache_key = {};
			cache_key = zest_InitialiseCacheKey(app->context, &app->cache_info, sizeof(RenderCacheInfo));

			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);

			// Build frame graph if not cached
			if (!frame_graph) {
				// Transient depth buffer: created/destroyed automatically by frame graph
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

					// Resource group: bundles swapchain + depth buffer for multi-attachment rendering
					zest_resource_group group = zest_CreateResourceGroup();
					zest_AddSwapchainToGroup(group);
					zest_AddResourceToGroup(group, depth_buffer);

					// Note: No transfer pass needed - instance data was uploaded at init

					// Pass 1: Render all geometry (skybox, asteroids, planet)
					zest_BeginRenderPass("Instancing Pass"); {
						zest_ConnectOutputGroup(group);
						zest_SetPassTask(RenderGeometry, app);
						zest_EndPass();
					}

					// Pass 2: ImGui overlay
					zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui, app->imgui.main_viewport); {
						if (imgui_pass) {
							zest_ConnectOutputGroup(group);
						} else {
							zest_BeginRenderPass("Draw Nothing");
							zest_ConnectOutputGroup(group);
							zest_SetPassTask(zest_EmptyRenderPass, 0);
						}
						zest_EndPass();
					}

					frame_graph = zest_EndFrameGraph();
				}
			}

			zest_EndFrame(app->context, frame_graph);
		}

		if (zest_SwapchainWasRecreated(app->context)) {
			// Handle window resize if needed (depth buffer is transient, auto-resizes)
		}
	}
}

int main(int argc, char *argv[]) {
	InstancingExample imgui_app = {};

	//Create a window using SDL2. We must do this before setting up the device as it's needed to get
	//the extensions info.
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "Instanced Meshes");

	// Create Vulkan device (one per application)
	imgui_app.device = zest_implsdl2_CreateVulkanDevice(&window_data, false);

	// Create window and context
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	imgui_app.context = zest_CreateContext(imgui_app.device, &window_data, &create_info);

	InitInstancingExample(&imgui_app);
	MainLoop(&imgui_app);

	// Cleanup
	ImGui_ImplSDL2_Shutdown();
	zest_imgui_Destroy(&imgui_app.imgui);
	zest_DestroyDevice(imgui_app.device);

	return 0;
}
