// Define implementations in exactly one .cpp file before including headers
#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_ALL_UTILITIES_IMPLEMENTATION
#include "zest-indirect-draw.h"
#include "zest.h"
#include "imgui_internal.h"
#include <random>
#include "examples/Common/sdl_controls.cpp"
#include "examples/Common/sdl_events.cpp"

/*
Indirect Draw with GPU Frustum Culling Example
-----------------------------------------------
Extends the instancing example with a compute shader that performs GPU-side frustum culling.
Instead of drawing all 8192 asteroids every frame, a compute pass tests each instance against
the view frustum and writes only visible instances to a compacted buffer. The render pass
then uses DrawIndexedIndirect so the GPU determines how many instances to draw.

Key concepts:
- Compute shader frustum culling with atomicAdd for output compaction
- Indirect draw commands written by GPU (CPU never knows visible count)
- Storage buffers for compute read/write of instance data
- Frame graph with compute -> graphics dependency (automatic barriers)
*/

#define INSTANCE_COUNT 8192		// Number of asteroid instances to render

void InitIndirectDrawExample(IndirectDrawExample *app) {
	// Initialize Dear ImGui with Zest and SDL2 backend
	zest_imgui_Initialise(app->context, &app->imgui, zest_implsdl2_DestroyWindow);
    ImGui_ImplSDL2_InitForVulkan((SDL_Window *)zest_Window(app->context));
	zest_imgui_DarkStyle(&app->imgui);

	// Custom ImGui font setup
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

	// Setup 3D camera
	app->camera = zest_CreateCamera();
	float position[3] = { -15.5f, 5.f, 0.f };
	zest_CameraPosition(&app->camera, position);
	zest_CameraSetFoV(&app->camera, 60.f);
	zest_CameraSetYaw(&app->camera, 0.f);
	zest_CameraSetPitch(&app->camera, -25.f);
	zest_CameraUpdateFront(&app->camera);
	app->new_camera_position = app->camera.position;
	app->old_camera_position = app->camera.position;

	app->timer = zest_CreateTimer(60);
	app->culling_enabled = true;

	// Uniform buffer for view/projection matrices and animation parameters
	app->view_buffer = zest_CreateUniformBuffer(app->context, "View", sizeof(uniform_buffer_data_t));

	// Create sampler for textures
	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	sampler_info.address_mode_u = zest_sampler_address_mode_repeat;
	sampler_info.address_mode_v = zest_sampler_address_mode_repeat;
	sampler_info.address_mode_w = zest_sampler_address_mode_repeat;
	app->sampler_2d = zest_CreateSampler(app->context, &sampler_info);
	zest_sampler sampler_2d = zest_GetSampler(app->sampler_2d);
	app->push.sampler_index = zest_AcquireSamplerIndex(app->device, sampler_2d);

	zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);

	// Load shaders for render pipelines
	zest_shader_handle instance_vert = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-indirect-draw/shaders/instancing.vert", "indirect_instancing_vert.spv", zest_vertex_shader, true);
	zest_shader_handle instance_frag = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-indirect-draw/shaders/instancing.frag", "indirect_instancing_frag.spv", zest_fragment_shader, true);
	zest_shader_handle planet_vert = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-indirect-draw/shaders/planet.vert", "indirect_planet_vert.spv", zest_vertex_shader, true);
	zest_shader_handle planet_frag = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-indirect-draw/shaders/planet.frag", "indirect_planet_frag.spv", zest_fragment_shader, true);
	zest_shader_handle skybox_vert = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-indirect-draw/shaders/sky_box.vert", "indirect_skybox_vert.spv", zest_vertex_shader, true);
	zest_shader_handle skybox_frag = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-indirect-draw/shaders/sky_box.frag", "indirect_skybox_frag.spv", zest_fragment_shader, true);

	// Load compute shader for frustum culling
	zest_shader_handle cull_shader = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-indirect-draw/shaders/cull.comp", "indirect_cull_comp.spv", zest_compute_shader, true);
	app->cull_compute = zest_CreateCompute(app->device, "Frustum Cull", cull_shader);

	// Rock pipeline: TWO vertex bindings - mesh vertices (binding 0) and instance data (binding 1)
	app->rock_pipeline = zest_CreatePipelineTemplate(app->device, "pipeline_mesh_instance");
	zest_AddVertexInputBindingDescription(app->rock_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexInputBindingDescription(app->rock_pipeline, 1, sizeof(mesh_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(app->rock_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);
	zest_AddVertexAttribute(app->rock_pipeline, 0, 1, zest_format_r8g8b8a8_unorm, offsetof(zest_vertex_t, color));
	zest_AddVertexAttribute(app->rock_pipeline, 0, 2, zest_format_r32g32b32_sfloat, offsetof(zest_vertex_t, normal));
	zest_AddVertexAttribute(app->rock_pipeline, 0, 3, zest_format_r32g32_sfloat, offsetof(zest_vertex_t, uv));
	zest_AddVertexAttribute(app->rock_pipeline, 1, 4, zest_format_r32g32b32_sfloat, 0);
	zest_AddVertexAttribute(app->rock_pipeline, 1, 5, zest_format_r32g32b32_sfloat, offsetof(mesh_instance_t, rotation));
	zest_AddVertexAttribute(app->rock_pipeline, 1, 6, zest_format_r32g32b32_sfloat, offsetof(mesh_instance_t, scale));
	zest_AddVertexAttribute(app->rock_pipeline, 1, 7, zest_format_r32_uint, offsetof(mesh_instance_t, texture_layer_index));

	zest_SetPipelineShaders(app->rock_pipeline, instance_vert, instance_frag);
	zest_SetPipelineCullMode(app->rock_pipeline, zest_cull_mode_back);
	zest_SetPipelineFrontFace(app->rock_pipeline, zest_front_face_counter_clockwise);
	zest_SetPipelineTopology(app->rock_pipeline, zest_topology_triangle_list);
	zest_SetPipelineDepthTest(app->rock_pipeline, true, true);

	// Planet pipeline: single-instance rendering
	app->planet_pipeline = zest_CopyPipelineTemplate("Planet pipeline", app->rock_pipeline);
	zest_ClearVertexInputBindingDescriptions(app->planet_pipeline);
	zest_ClearVertexAttributeDescriptions(app->planet_pipeline);
	zest_AddVertexInputBindingDescription(app->planet_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexAttribute(app->planet_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);
	zest_AddVertexAttribute(app->planet_pipeline, 0, 1, zest_format_r8g8b8a8_unorm, offsetof(zest_vertex_t, color));
	zest_AddVertexAttribute(app->planet_pipeline, 0, 2, zest_format_r32g32b32_sfloat, offsetof(zest_vertex_t, normal));
	zest_AddVertexAttribute(app->planet_pipeline, 0, 3, zest_format_r32g32_sfloat, offsetof(zest_vertex_t, uv));
	zest_SetPipelineShaders(app->planet_pipeline, planet_vert, planet_frag);

	// Skybox pipeline
	app->skybox_pipeline = zest_CopyPipelineTemplate("Skybox pipeline", app->rock_pipeline);
	zest_ClearVertexInputBindingDescriptions(app->skybox_pipeline);
	zest_ClearVertexAttributeDescriptions(app->skybox_pipeline);
	zest_AddVertexInputBindingDescription(app->skybox_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexAttribute(app->skybox_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);
	zest_SetPipelineShaders(app->skybox_pipeline, skybox_vert, skybox_frag);
	zest_SetPipelineFrontFace(app->skybox_pipeline, zest_front_face_clockwise);
	zest_SetPipelineDepthTest(app->skybox_pipeline, false, false);

	// Load GLTF meshes
	zest_mesh planet = LoadGLTFScene(app->context, "examples/assets/gltf/lavaplanet.gltf", 1.f);
	zest_mesh rock = LoadGLTFScene(app->context, "examples/assets/gltf/rock01.gltf", 1.f);
	zest_mesh cube = zest_CreateCube(app->context, 1.f, zest_ColorSet(0, 50, 100, 255));

	zest_size planet_vertex_capacity = zest_MeshVertexDataSize(planet);
	zest_size planet_index_capacity = zest_MeshIndexDataSize(planet);
	zest_size rock_vertex_capacity = zest_MeshVertexDataSize(rock);
	zest_size rock_index_capacity = zest_MeshIndexDataSize(rock);

	// Create mesh layers
	app->rock_layer = zest_CreateInstanceMeshLayer(app->context, "Mesh Layer", sizeof(mesh_instance_t), rock_vertex_capacity, rock_index_capacity);
	app->rock_mesh_index = zest_AddMeshToLayer(zest_GetLayer(app->rock_layer), rock, 0);

	app->skybox_layer = zest_CreateInstanceMeshLayer(app->context, "Skybox Layer", sizeof(mesh_instance_t), zest_MeshVertexDataSize(cube), zest_MeshIndexDataSize(cube));
	app->skybox_mesh_index = zest_AddMeshToLayer(zest_GetLayer(app->skybox_layer), cube, 0);

	// Planet mesh buffers
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

	// Load textures
	app->lavaplanet_texture = zest_LoadKTX(app->device, "Lavaplanet Texture", "examples/assets/lavaplanet_rgba.ktx");
	app->rock_textures = zest_LoadKTX(app->device, "Rock Textures", "examples/assets/texturearray_rocks_rgba.ktx");
	app->skybox_texture = zest_LoadKTX(app->device, "Space Background Texture", "examples/assets/cubemap_space.ktx");

	// Bindless texture indices
	app->push.planet_index = zest_AcquireSampledImageIndex(app->device, zest_GetImage(app->lavaplanet_texture), zest_texture_2d_binding);
	app->push.rock_index = zest_AcquireSampledImageIndex(app->device, zest_GetImage(app->rock_textures), zest_texture_array_binding);
	app->push.skybox_index = zest_AcquireSampledImageIndex(app->device, zest_GetImage(app->skybox_texture), zest_texture_cube_binding);

	// Free CPU-side data
	zest_FreeMesh(planet);
	zest_FreeMesh(rock);
	zest_FreeMesh(cube);
	zest_FreeBuffer(index_staging);
	zest_FreeBuffer(vertex_staging);

	// Generate random instance data and create GPU buffers for culling
	PrepareInstanceData(app);
}

// Generate random positions, rotations, scales for asteroid instances
// Creates: all_instances_ssbo (storage), visible_instances (vertex+storage), indirect_draw_buffer (indirect+storage)
void PrepareInstanceData(IndirectDrawExample *app) {
	zest_size instance_data_size = INSTANCE_COUNT * sizeof(mesh_instance_t);
	mesh_instance_t *instance_data = (mesh_instance_t*)malloc(instance_data_size);

	zest_image rock_image = zest_GetImage(app->rock_textures);
	std::default_random_engine rndGenerator(zest_Millisecs());
	std::uniform_real_distribution<float> uniformDist(0.0, 1.0);
	std::uniform_int_distribution<uint32_t> rndTextureIndex(0, zest_ImageInfo(rock_image)->layer_count);

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

	// all_instances_ssbo: storage buffer, GPU-only, read by compute shader
	zest_buffer_info_t storage_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	app->all_instances_ssbo = zest_CreateBuffer(app->device, instance_data_size, &storage_info);

	// visible_instances: vertex+storage buffer, GPU-only, written by compute, read as vertex input
	zest_buffer_info_t visible_info = zest_CreateBufferInfo(zest_buffer_type_vertex_storage, zest_memory_usage_gpu_only);
	app->visible_instances = zest_CreateBuffer(app->device, instance_data_size, &visible_info);

	// indirect_draw_buffer: indirect+storage buffer, GPU-only, written by compute, read by indirect draw
	zest_buffer_info_t indirect_info = zest_CreateBufferInfo(zest_buffer_type_indirect, zest_memory_usage_gpu_only);
	app->indirect_draw_buffer = zest_CreateBuffer(app->device, sizeof(zest_draw_indexed_indirect_command_t), &indirect_info);

	// Upload instance data to the storage buffer via staging
	zest_buffer staging_buffer = zest_CreateStagingBuffer(app->device, instance_data_size, instance_data);
	zest_queue queue = zest_imm_BeginCommandBuffer(app->device, zest_queue_transfer);
	zest_imm_CopyBuffer(queue, staging_buffer, app->all_instances_ssbo, instance_data_size);
	zest_imm_EndCommandBuffer(queue);
	zest_FreeBuffer(staging_buffer);

	// Acquire bindless descriptor indices for storage buffers
	app->all_instances_ssbo_index = zest_AcquireStorageBufferIndex(app->device, app->all_instances_ssbo);
	app->visible_instances_index = zest_AcquireStorageBufferIndex(app->device, app->visible_instances);
	app->indirect_cmd_index = zest_AcquireStorageBufferIndex(app->device, app->indirect_draw_buffer);

	free(instance_data);
}

void UpdateUniform3d(IndirectDrawExample *app) {
	zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);
	uniform_buffer_data_t *ubo_ptr = static_cast<uniform_buffer_data_t *>(zest_GetUniformBufferData(view_buffer));
	ubo_ptr->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	ubo_ptr->proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf(app->context) / zest_ScreenHeightf(app->context), 0.001f, 10000.f);
	ubo_ptr->proj.v[1].y *= -1.f;
	zest_CalculateFrustumPlanes(&ubo_ptr->view, &ubo_ptr->proj, ubo_ptr->planes);

	app->loc_speed += app->frame_timer * 0.35f;
	app->glob_speed += app->frame_timer * 0.01f;
	ubo_ptr->locSpeed = app->loc_speed;
	ubo_ptr->globSpeed = app->glob_speed;
	ubo_ptr->light_pos = {0.f};

	app->push.ubo_index = zest_GetUniformBufferDescriptorIndex(view_buffer);
}

void UploadMeshData(const zest_command_list command_list, void *user_data) {
	IndirectDrawExample *app = (IndirectDrawExample *)user_data;
	zest_layer layer = zest_GetLayer(app->rock_layer);
	zest_UploadLayerStagingData(layer, command_list);
}

void UpdateImGui(IndirectDrawExample *app) {
	zest_StartTimerLoop(app->timer) {
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
		ImGui::Begin("Indirect Draw");
		ImGui::Text("FPS: %u", app->fps);
		ImGui::Separator();
		ImGui::Checkbox("GPU Frustum Culling", &app->culling_enabled);
		ImGui::SetNextItemWidth(50.f);
		ImGui::DragFloat("Contract Frustum", &app->contract_frustum, 0.1f, 0.f, 10.f);
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

// Compute pass callback: frustum cull instances
void CullCompute(zest_command_list command_list, void *user_data) {
	IndirectDrawExample *app = (IndirectDrawExample *)user_data;
	zest_compute compute = zest_GetCompute(app->cull_compute);
	zest_cmd_BindComputePipeline(command_list, compute);

	zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);

	// Fill push constants with buffer indices and mesh info
	zest_layer rock_layer = zest_GetLayer(app->rock_layer);
	const zest_mesh_offset_data_t *mesh_offsets = zest_GetLayerMeshOffsets(rock_layer, app->rock_mesh_index);

	cull_push_t cull_push;
	cull_push.all_instances_index = app->all_instances_ssbo_index;
	cull_push.visible_instances_index = app->visible_instances_index;
	cull_push.indirect_cmd_index = app->indirect_cmd_index;
	cull_push.ubo_index = zest_GetUniformBufferDescriptorIndex(view_buffer);
	cull_push.total_count = INSTANCE_COUNT;
	cull_push.index_count = mesh_offsets->index_count;
	cull_push.first_index = mesh_offsets->index_offset;
	cull_push.vertex_offset = mesh_offsets->vertex_offset;
	cull_push.cull_enabled = app->culling_enabled ? 1 : 0;
	cull_push.contract_frustum = app->contract_frustum;

	zest_cmd_SendPushConstants(command_list, &cull_push, sizeof(cull_push_t));
	zest_cmd_DispatchCompute(command_list, (INSTANCE_COUNT + 255) / 256, 1, 1);
}

// Render callback: renders skybox, asteroids (via indirect draw), and planet
void RenderGeometry(zest_command_list command_list, void *user_data) {
	IndirectDrawExample *app = (IndirectDrawExample*)user_data;

	// 1. Render skybox
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

	// 2. Render asteroids via indirect draw
	zest_layer rock_layer = zest_GetLayer(app->rock_layer);
	zest_cmd_BindMeshVertexBuffer(command_list, rock_layer);
	zest_cmd_BindMeshIndexBuffer(command_list, rock_layer);
	// Bind the visible_instances buffer (output of compute cull) as vertex binding 1
	zest_cmd_BindVertexBuffer(command_list, 1, 1, app->visible_instances);

	pipeline = zest_GetPipeline(app->rock_pipeline, command_list);
	if (pipeline) {
		zest_cmd_BindPipeline(command_list, pipeline);
		zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);
		zest_cmd_SendPushConstants(command_list, &app->push, sizeof(instance_push_t));
		// Indirect draw: GPU reads instance count from the indirect buffer
		zest_cmd_DrawIndexedIndirect(command_list, app->indirect_draw_buffer, 0, 1, sizeof(zest_draw_indexed_indirect_command_t));
	}

	// 3. Render planet
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

void MainLoop(IndirectDrawExample *app) {
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
			UpdateMouse(app->context, &app->mouse, &app->camera);

			float elapsed = (float)current_frame_time;
			app->frame_timer = (float)elapsed / ZEST_MICROSECS_SECOND;

			UpdateUniform3d(app);
			UpdateImGui(app);

			app->camera.position = zest_LerpVec3(&app->old_camera_position, &app->new_camera_position, (float)zest_TimerLerp(&app->timer));

			app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw(&app->imgui);
			zest_frame_graph_cache_key_t cache_key = {};
			cache_key = zest_InitialiseCacheKey(app->context, &app->cache_info, sizeof(RenderCacheInfo));

			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);

			if (!frame_graph) {
				zest_image_resource_info_t depth_info = {
					zest_format_depth,
					zest_resource_usage_hint_none,
					zest_ScreenWidth(app->context),
					zest_ScreenHeight(app->context),
					1, 1
				};

				if (zest_BeginFrameGraph(app->context, "Indirect Draw", &cache_key)) {
					// Import buffer resources for compute
					zest_resource_node all_instances_node = zest_ImportBufferResource("all instances", app->all_instances_ssbo, 0);
					zest_resource_node visible_node = zest_ImportBufferResource("visible instances", app->visible_instances, 0);
					zest_resource_node indirect_node = zest_ImportBufferResource("indirect cmd", app->indirect_draw_buffer, 0);
					zest_resource_node swapchain_node = zest_ImportSwapchainResource();
					zest_resource_node depth_buffer = zest_AddTransientImageResource("Depth Buffer", &depth_info);

					zest_resource_group group = zest_CreateResourceGroup();
					zest_AddSwapchainToGroup(group);
					zest_AddResourceToGroup(group, depth_buffer);

					// Compute pass: frustum cull
					zest_BeginComputePass("Frustum Cull"); {
						zest_ConnectInput(all_instances_node);
						zest_ConnectOutput(visible_node);
						zest_ConnectOutput(indirect_node);
						zest_SetPassTask(CullCompute, app);
						zest_EndPass();
					}

					// Render pass: draw geometry using indirect draw
					zest_BeginRenderPass("Instancing Pass"); {
						zest_ConnectInput(visible_node);
						zest_ConnectInput(indirect_node);
						zest_ConnectOutputGroup(group);
						zest_SetPassTask(RenderGeometry, app);
						zest_EndPass();
					}

					// ImGui pass
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
			// Handle window resize if needed
		}
	}
}

int main(int argc, char *argv[]) {
	IndirectDrawExample app = {};

	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "Indirect Draw - GPU Frustum Culling");

	app.device = zest_implsdl2_CreateVulkanDevice(&window_data, false);

	zest_create_context_info_t create_info = zest_CreateContextInfo();
	app.context = zest_CreateContext(app.device, &window_data, &create_info);

	InitIndirectDrawExample(&app);
	MainLoop(&app);

	ImGui_ImplSDL2_Shutdown();
	zest_imgui_Destroy(&app.imgui);
	zest_DestroyDevice(app.device);

	return 0;
}
