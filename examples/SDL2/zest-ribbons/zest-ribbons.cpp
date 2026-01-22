// Define implementations in exactly one .cpp file before including headers
#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_IMAGES_IMPLEMENTATION
#include "zest-ribbons.h"
#include <zest.h>
#include "imgui_internal.h"
#include <cmath>
#include "examples/Common/sdl_controls.cpp"
#include "examples/Common/sdl_events.cpp"

/*
Compute Shader Ribbon Rendering Example
---------------------------------------
Demonstrates using a compute shader to procedurally generate ribbon/trail geometry on the GPU.

This example shows:
- Compute shader pipeline setup and dispatch
- Multi-pass frame graph: Transfer → Compute → Render
- Transient buffers for compute shader input/output
- Staging buffers per frame-in-flight for CPU→GPU data transfer
- Bindless buffer access in compute shaders via descriptor indices
- Procedural animation patterns (circular, figure-8, spiral, Lissajous curves, etc.)

Data flow:
1. CPU updates ribbon segment positions each frame (procedural animation)
2. Transfer pass: Upload segment data to GPU via staging buffers
3. Compute pass: Generate vertex/index buffers from segment data
4. Render pass: Draw the generated ribbon geometry
*/

void InitImGuiApp(Ribbons *app) {
	// Initialize Dear ImGui with Zest and GLFW backend
	zest_imgui_Initialise(app->context, &app->imgui, zest_implsdl2_DestroyWindow);
	ImGui_ImplSDL2_InitForVulkan((SDL_Window *)zest_Window(app->context));
	zest_imgui_DarkStyle(&app->imgui);

	// Custom ImGui font setup
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.Fonts->Clear();
	float font_size = 15.f;
	unsigned char *font_data;
	int tex_width, tex_height;
	ImFontConfig config;
	config.PixelSnapH = true;
	io.Fonts->AddFontFromFileTTF("examples/assets/Lato-Regular.ttf", font_size);
	io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);
	zest_imgui_RebuildFontTexture(&app->imgui, tex_width, tex_height, font_data);

	app->sync_refresh = true;

	// Configure ribbon tessellation and buffer sizing
	int tessellation = 2;
	app->ribbon_buffer_info = GenerateRibbonInfo(tessellation, SEGMENT_COUNT * 10, 10);
	app->ribbon_count = RIBBON_COUNT;

	// Create staging buffers for each frame-in-flight
	// This allows CPU to update next frame's data while GPU processes current frame
	zest_ForEachFrameInFlight(fif) {
		app->ribbon_segment_staging_buffer[fif] = zest_CreateStagingBuffer(app->device, SEGMENT_COUNT * sizeof(ribbon_segment) * 10, 0);
		app->ribbon_instance_staging_buffer[fif] = zest_CreateStagingBuffer(app->device, SEGMENT_COUNT * sizeof(ribbon_instance) * 10, 0);
	}

	// Uniform buffer for view/projection matrices
	app->uniform_buffer = zest_CreateUniformBuffer(app->context, "Ribbon Uniform", sizeof(RibbonUniform));
	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(app->uniform_buffer);

	// Load shaders: compute shader generates geometry, vert/frag render it
	app->ribbon_comp_shader = zest_CreateShaderFromFile(app->device, "examples/assets/shaders/ribbons.comp", "ribbon_comp.spv", zest_compute_shader, true);
	app->ribbon_vert_shader = zest_CreateShaderFromFile(app->device, "examples/assets/shaders/ribbon_3d.vert", "ribbon_3d_vert.spv", zest_vertex_shader, true);
	app->ribbon_frag_shader = zest_CreateShaderFromFile(app->device, "examples/assets/shaders/ribbon.frag", "ribbon_frag.spv", zest_fragment_shader, true);

	// Create compute pipeline for ribbon geometry generation
	app->ribbon_compute = zest_CreateCompute(app->device, "Ribbon Compute", app->ribbon_comp_shader);

	// Create render pipeline for drawing the generated ribbon geometry
	// Vertex format matches the output of the compute shader
	app->ribbon_pipeline = zest_CreatePipelineTemplate(app->device, "Ribbon Pipeline");
	zest_AddVertexInputBindingDescription(app->ribbon_pipeline, 0, sizeof(ribbon_vertex), zest_input_rate_vertex);
	zest_AddVertexAttribute(app->ribbon_pipeline, 0, 0, zest_format_r32g32b32a32_sfloat, offsetof(ribbon_vertex, position));
	zest_AddVertexAttribute(app->ribbon_pipeline, 0, 1, zest_format_r32g32b32a32_sfloat, offsetof(ribbon_vertex, uv));
	zest_AddVertexAttribute(app->ribbon_pipeline, 0, 2, zest_format_r8g8b8a8_unorm, offsetof(ribbon_vertex, color));
	zest_SetPipelineVertShader(app->ribbon_pipeline, app->ribbon_vert_shader);
	zest_SetPipelineFragShader(app->ribbon_pipeline, app->ribbon_frag_shader);
	zest_SetPipelineDepthTest(app->ribbon_pipeline, false, true);
	zest_SetPipelineBlend(app->ribbon_pipeline, zest_PreMultiplyBlendState());	// Pre-multiplied alpha for smooth blending
	zest_SetPipelineTopology(app->ribbon_pipeline, zest_topology_triangle_list);
	zest_SetPipelineCullMode(app->ribbon_pipeline, zest_cull_mode_back);

	// Create sampler and load ribbon texture
	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	app->sampler = zest_CreateSampler(app->context, &sampler_info);
	zest_sampler sampler = zest_GetSampler(app->sampler);
	app->sampler_index = zest_AcquireSamplerIndex(app->device, sampler);

	// Load glow texture for ribbon rendering
	int width, height, channels;
	stbi_uc *pixels = stbi_load("examples/assets/BrightGlow.png", &width, &height, &channels, 4);
	int size = width * height * 4;
	zest_image_info_t image_info = zest_CreateImageInfo(width, height);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_texture;
	app->ribbon_texture = zest_CreateImageWithPixels(app->device, pixels, size, &image_info);
	STBI_FREE(pixels);

	// Setup atlas region for bindless texture access
	zest_image image = zest_GetImage(app->ribbon_texture);
	app->ribbon_image = zest_NewAtlasRegion();
	app->ribbon_image.width = width;
	app->ribbon_image.height = height;
	zest_AcquireSampledImageIndex(app->device, image, zest_texture_2d_binding);
	zest_BindAtlasRegionToImage(&app->ribbon_image, app->sampler_index, image, zest_texture_2d_binding);

	// Store texture indices for shader access via push constants
	app->ribbon_push_constants.texture_index = app->ribbon_image.image_index;
	app->ribbon_push_constants.sampler_index = app->ribbon_image.sampler_index;

	app->timer = zest_CreateTimer(60);

	// Setup camera
	app->camera = zest_CreateCamera();
	zest_CameraSetFoV(&app->camera, 60.f);
	app->camera_push.uv_scale = 1.f;
	app->camera_push.uv_offset = 0.f;
	app->camera_push.width_scale_multiplier = 1.5f;
	app->camera_push.tessellation = tessellation;
	app->camera.position.x = -5.f;

	// Initialize ribbon lengths
	for (int i = 0; i != RIBBON_COUNT; ++i) {
		app->ribbons[i].length = SEGMENT_COUNT;
	}

	// Pre-populate ribbon history with initial segment positions
	UpdateUniform3d(app);
	for (int s = 0; s != SEGMENT_COUNT; ++s) {
		UpdateRibbons(app);
	}

	// Configure transient buffer usage hints for frame graph
	// These hints help the frame graph compiler optimize buffer layouts
	app->segment_buffer_info.usage_hints = 0;
	app->instance_buffer_info.usage_hints = 0;
	app->vertex_buffer_info.usage_hints = zest_resource_usage_hint_vertex_buffer;
	app->index_buffer_info.usage_hints = zest_resource_usage_hint_index_buffer;
}

// Calculate buffer sizes for ribbon geometry generation
// tessellation: number of quad divisions across the ribbon width (more = smoother curves)
// maxSegments: maximum number of segments the ribbon can have (trail length)
RibbonBufferInfo GenerateRibbonInfo(uint32_t tessellation, uint32_t maxSegments, uint32_t max_ribbons) {
	RibbonBufferInfo info{};
	info.verticesPerSegment = (tessellation + 1) * 2;	// Vertices across ribbon width * 2 for both sides
	info.trianglesPerSegment = tessellation * 4;		// 2 triangles per quad * 2 strips * tessellation
	info.indicesPerSegment = info.trianglesPerSegment * 3;
	return info;
}

// Transfer pass callback: Upload ribbon segment data from CPU staging buffers to GPU
// This runs before the compute pass so the compute shader has fresh data
void UploadRibbonData(zest_command_list command_list, void *user_data) {
	Ribbons *app = (Ribbons*)user_data;

	zest_context context = zest_GetContext(command_list);
	zest_uint fif = zest_CurrentFIF(context);

	// Get the transient buffers declared in the frame graph by name
	zest_buffer segment_buffer = zest_GetPassOutputBuffer(command_list, "Ribbon Segment Buffer");
	zest_buffer ribbon_instance_buffer = zest_GetPassOutputBuffer(command_list, "Ribbon Instance Buffer");

	// Copy from staging buffers (CPU-visible) to transient buffers (GPU-optimal)
	if (segment_buffer) {
		zest_size buffer_size = zest_GetBufferSize(app->ribbon_segment_staging_buffer[fif]);
		zest_cmd_CopyBuffer(command_list, app->ribbon_segment_staging_buffer[fif], segment_buffer, buffer_size);
	}
	if (ribbon_instance_buffer) {
		zest_size buffer_size = zest_GetBufferSize(app->ribbon_instance_staging_buffer[fif]);
		zest_cmd_CopyBuffer(command_list, app->ribbon_instance_staging_buffer[fif], ribbon_instance_buffer, buffer_size);
	}
}

// Render pass callback: Draw the ribbon geometry generated by the compute shader
void RecordRibbonDrawing(zest_command_list command_list, void *user_data) {
	Ribbons *app = (Ribbons*)user_data;

	zest_context context = zest_GetContext(command_list);
	zest_device device = zest_GetContextDevice(context);

	// Get the vertex/index buffers generated by the compute shader
	// These are the same transient buffers connected as outputs in the compute pass
	zest_buffer vertex_buffer = zest_GetPassInputBuffer(command_list, "Ribbon Vertex Buffer");
	zest_buffer index_buffer = zest_GetPassInputBuffer(command_list, "Ribbon Index Buffer");

	// Bind the compute-generated buffers for rendering
	zest_cmd_BindVertexBuffer(command_list, 0, 1, vertex_buffer);
	zest_cmd_BindIndexBuffer(command_list, index_buffer);

	zest_pipeline pipeline = zest_GetPipeline(app->ribbon_pipeline, command_list);
	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(app->uniform_buffer);

	// Draw all ribbon geometry in a single draw call
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_SendPushConstants(command_list, &app->ribbon_push_constants, sizeof(ribbon_drawing_push_constants));
	zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);
	zest_cmd_DrawIndexed(command_list, app->index_count, 1, 0, 0, 0);
}

// Compute pass callback: Dispatch compute shader to generate ribbon vertex/index data
// The compute shader reads segment positions and generates tessellated triangle strips
void RecordComputeCommands(zest_command_list command_list, void *user_data) {
	Ribbons *app = (Ribbons*)user_data;

	zest_context context = zest_GetContext(command_list);
	zest_device device = zest_GetContextDevice(context);

	zest_uint total_segments = CountSegments(app);
	if (!total_segments) return;

	zest_compute ribbon_compute = zest_GetCompute(app->ribbon_compute);
	zest_cmd_BindComputePipeline(command_list, ribbon_compute);

	// Get transient buffer resource nodes for bindless access
	zest_resource_node segment_buffer = zest_GetPassInputResource(command_list, "Ribbon Segment Buffer");
	zest_resource_node ribbon_instance_buffer = zest_GetPassInputResource(command_list, "Ribbon Instance Buffer");
	zest_resource_node vertex_buffer = zest_GetPassOutputResource(command_list, "Ribbon Vertex Buffer");
	zest_resource_node index_buffer = zest_GetPassOutputResource(command_list, "Ribbon Index Buffer");

	// Get bindless descriptor indices for each buffer
	// The compute shader accesses these buffers via these indices in push constants
	app->camera_push.segment_buffer_index = zest_GetTransientBufferBindlessIndex(command_list, segment_buffer);
	app->camera_push.instance_buffer_index = zest_GetTransientBufferBindlessIndex(command_list, ribbon_instance_buffer);
	app->camera_push.vertex_buffer_index = zest_GetTransientBufferBindlessIndex(command_list, vertex_buffer);
	app->camera_push.index_buffer_index = zest_GetTransientBufferBindlessIndex(command_list, index_buffer);

	zest_cmd_SendPushConstants(command_list, &app->camera_push, sizeof(camera_push_constant));

	// Dispatch compute: 128 = local_size_x in shader (workgroup size)
	// Each workgroup processes 128 segments in parallel
	zest_cmd_DispatchCompute(command_list, ((SEGMENT_COUNT * app->ribbon_count) / 128) + 1, 1, 1);
}

// Update view/projection matrices in uniform buffer
void UpdateUniform3d(Ribbons *app) {
	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(app->uniform_buffer);
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(uniform_buffer);
	buffer_3d->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	buffer_3d->proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf(app->context) / zest_ScreenHeightf(app->context), 0.1f, 10000.f);
	buffer_3d->proj.v[1].y *= -1.f;	// Flip Y for Vulkan coordinate system
	buffer_3d->screen_size.x = zest_ScreenWidthf(app->context);
	buffer_3d->screen_size.y = zest_ScreenHeightf(app->context);
	buffer_3d->millisecs = 0;
	// Store uniform buffer descriptor index for bindless access in shaders
	app->ribbon_push_constants.uniform_index = zest_GetUniformBufferDescriptorIndex(uniform_buffer);
}

zest_vec3 zest_Vec4ToVec3(zest_vec4 v) {
	return { v.x, v.y, v.z };
}

zest_vec3 mix(zest_vec3 to, zest_vec3 from, float lerp) {
	zest_vec3 lerped;
	lerped = zest_ScaleVec3(zest_AddVec3(zest_ScaleVec3(to, lerp), from), (1.f - lerp));
	lerped.x = lerped.x;
	lerped.y = lerped.y;
	lerped.z = lerped.z;
	return lerped;
}

// Build ImGui interface - exposes ribbon rendering parameters for real-time tweaking
void BuildUI(Ribbons *app) {
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Test Window");
	ImGui::Text("FPS %i", app->last_fps);
	// These parameters are passed to the compute shader via push constants
	ImGui::DragFloat("Texture Scale", &app->camera_push.uv_scale, 0.1f);
	ImGui::DragFloat("Texture Offset", &app->camera_push.uv_offset, 0.1f);
	ImGui::DragFloat("Width Scalar", &app->camera_push.width_scale_multiplier, 0.01f);
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
}

// Count total segments across all ribbons
zest_uint CountSegments(Ribbons *app) {
	int count = 0;
	for (int i = 0; i != app->ribbon_count; ++i) {
		count += app->ribbons[i].length;
	}
	return count;
}

// Update ribbon segment positions each frame using procedural animation
// Each ribbon follows a different mathematical curve pattern
// New positions are added to a circular buffer, creating a trail effect
void UpdateRibbons(Ribbons *app) {
	app->seconds_passed += (float)app->timer.update_time * 1000.f;
	for (int r = 0; r != app->ribbon_count; ++r) {
		float time = (float(app->seconds_passed) + r * 500.f) * 0.001f;
		float radius;
		zest_vec3 position;
		zest_color_t color;

		// Each ribbon follows a unique procedural animation pattern
		switch (r) {
		case 0: // Circular motion with varying radius
			radius = 2.0f + sinf(time * 0.5f);
			position = {
				radius * cosf(time * 2.0f),
				sinf(time),
				radius * sinf(time * 2.0f)
			};
			//position = { 0.f, time * 2.f, 0.f };
			color = { 255, 204, 51, 0 };
			break;

		case 1: // Figure-8 pattern
			position = {
				2.0f * sinf(time * 2.0f),
				cosf(time * 4.0f),
				2.0f * sinf(time * 4.0f) * cosf(time * 2.0f)
			};
			color = { 25, 128, 255, 0 };
			break;

		case 2: // Spiral motion
			radius = time * 0.1f;
			position = {
				radius * cosf(time * 3.0f),
				time * 0.2f,
				radius * sinf(time * 3.0f)
			};
			color = { 128, 255, 255, 0 };
			break;

		case 3: // Bouncing motion
			position = {
				2.0f * cosf(time),
				fabsf(sinf(time * 2.0f)) * 3.0f,
				2.0f * sinf(time)
			};
			color = { 255, 255, 128, 0 };
			break;

		case 4: // Lissajous curve
			position = {
				2.0f * sinf(time * 2.0f),
				2.0f * sinf(time * 3.0f),
				2.0f * sinf(time * 4.0f)
			};
			color = { 128, 51, 204, 0 };
			break;

		case 5: // Helical motion
			position = {
				2.0f * cosf(time * 3.0f),
				sinf(time * 2.0f),
				2.0f * sinf(time * 3.0f)
			};
			color = { 75, 255, 100, 0 };
			break;

		case 6: // Expanding/contracting circle
			radius = 1.0f + sinf(time);
			position = {
				radius * cosf(time * 2.5f),
				cosf(time * 1.5f),
				radius * sinf(time * 2.5f)
			};
			color = { 51, 60, 255, 0 };
			break;

		case 7: // Wave motion
			position = {
				2.0f * sinf(time * 2.0f),
				sinf(time * 4.0f + sinf(time)),
				2.0f * cosf(time * 1.5f)
			};
			color = { 115, 220, 190, 0 };
			break;

		case 8: // Flower pattern
			radius = 2.0f * (1.0f + 0.3f * cosf(5.0f * time));
			position = {
				radius * cosf(time),
				sinf(time * 3.0f),
				radius * sinf(time)
			};
			color = { 255, 255, 51, 0 };
			break;

		case 9: // Random-looking but deterministic motion
			position = {
				2.0f * sinf(time * 2.3f) * cosf(time * 1.1f),
				1.5f * sinf(time * 1.7f) * cosf(time * 2.1f),
				2.0f * sinf(time * 1.3f) * cosf(time * 1.9f)
			};
			color = { 128, 255, 255, 0 };
			break;
		}

		// Store new segment in circular buffer
		int current_index = app->ribbons[r].ribbon_index;
		int segment_offset = r * SEGMENT_COUNT;
		app->ribbons[r].length = ZEST__MIN(app->ribbons[r].length + 1, SEGMENT_COUNT);
		app->ribbon_segments[segment_offset + current_index].position_and_width = { position.x, position.y, position.z, .1f };
		app->ribbon_segments[segment_offset + current_index].color = color;

		// Per-ribbon instance data (width scale, start index for circular buffer)
		app->ribbon_instances[r].width_scale = 1.f + float(r) * .15f;
		app->ribbon_instances[r].start_index = app->ribbons[r].start_index;

		// Advance circular buffer index
		app->ribbons[r].ribbon_index = (current_index + 1) % app->ribbons[r].length;
		if (app->ribbons[r].length == SEGMENT_COUNT) {
			app->ribbons[r].start_index++;
			app->ribbons[r].start_index %= app->ribbons[r].length;
		}
	}
}

void MainLoop(Ribbons *app) {
	// FPS tracking
	zest_microsecs running_time = zest_Microsecs();
	zest_microsecs frame_time = 0;
	zest_uint frame_count = 0;
	zest_uint fps = 0;
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
			app->last_fps = frame_count;
			frame_count = 0;
		}

		zest_UpdateDevice(app->device);

		float elapsed = (float)current_frame_time;

		if (zest_BeginFrame(app->context)) {
			// Rate-limited updates for UI and ribbon animation
			zest_StartTimerLoop(app->timer) {
				BuildUI(app);
				UpdateRibbons(app);
			} zest_EndTimerLoop(app->timer);

			// Smooth camera interpolation
			app->camera.position = zest_LerpVec3(&app->old_camera_position, &app->new_camera_position, (float)zest_TimerLerp(&app->timer));
			UpdateMouse(app->context, &app->mouse, &app->camera);
			UpdateUniform3d(app);

			zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(app->uniform_buffer);
			zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(uniform_buffer);

			zest_uint fif = zest_CurrentFIF(app->context);

			// Setup compute shader push constants
			app->camera_push.position = { app->camera.position.x, app->camera.position.y, app->camera.position.z, 0.f };
			app->camera_push.segment_count = SEGMENT_COUNT;
			app->camera_push.index_offset = 0;
			app->camera_push.ribbon_count = app->ribbon_count;
			zest_uint total_segments = SEGMENT_COUNT * app->ribbon_count;

			// Stage ribbon data to frame-in-flight staging buffer
			// This prepares data for the transfer pass to upload to GPU
			app->index_count = 0;
			zest_StageData(app->ribbon_segments, app->ribbon_segment_staging_buffer[fif], SEGMENT_COUNT *RIBBON_COUNT * sizeof(ribbon_segment));
			app->index_count += (SEGMENT_COUNT * RIBBON_COUNT) * app->ribbon_buffer_info.indicesPerSegment;
			zest_StageData(app->ribbon_instances, app->ribbon_instance_staging_buffer[fif], app->ribbon_count * sizeof(ribbon_instance));

			// Configure transient buffer sizes for this frame
			app->segment_buffer_info.size = zest_GetBufferSize(app->ribbon_segment_staging_buffer[fif]);
			app->instance_buffer_info.size = zest_GetBufferSize(app->ribbon_instance_staging_buffer[fif]);
			app->vertex_buffer_info.size = app->ribbon_buffer_info.verticesPerSegment * total_segments * sizeof(ribbon_vertex);
			app->index_buffer_info.size = app->index_count * sizeof(zest_uint);

			// Frame graph caching: rebuild when fif or imgui state changes
			app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw(&app->imgui);
			app->cache_info.fif = fif;
			zest_frame_graph_cache_key_t cache_key = {};
			cache_key = zest_InitialiseCacheKey(app->context, &app->cache_info, sizeof(RenderCacheInfo));

			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);

			// Build frame graph if not cached
			if (!frame_graph) {
				if (zest_BeginFrameGraph(app->context, "Ribbons render graph", &cache_key)) {
					// Declare transient buffers - automatically allocated/aliased by frame graph
					// Input buffers: segment positions and per-ribbon instance data
					zest_resource_node ribbon_segment_buffer = zest_AddTransientBufferResource("Ribbon Segment Buffer", &app->segment_buffer_info);
					zest_resource_node ribbon_instance_buffer = zest_AddTransientBufferResource("Ribbon Instance Buffer", &app->instance_buffer_info);
					// Output buffers: vertex and index data generated by compute shader
					zest_resource_node ribbon_vertex_buffer = zest_AddTransientBufferResource("Ribbon Vertex Buffer", &app->vertex_buffer_info);
					zest_resource_node ribbon_index_buffer = zest_AddTransientBufferResource("Ribbon Index Buffer", &app->index_buffer_info);

					zest_ImportSwapchainResource();

					// Pass 1: Transfer - Upload segment data from CPU staging buffers
					zest_BeginTransferPass("Transfer Ribbon Data"); {
						zest_ConnectOutput(ribbon_segment_buffer);
						zest_ConnectOutput(ribbon_instance_buffer);
						zest_SetPassTask(UploadRibbonData, app);
						zest_EndPass();
					}

					// Pass 2: Compute - Generate vertex/index geometry from segments
					// The frame graph automatically inserts barriers between transfer and compute
					zest_compute ribbon_compute = zest_GetCompute(app->ribbon_compute);
					zest_BeginComputePass(ribbon_compute, "Compute Ribbons"); {
						zest_ConnectInput(ribbon_segment_buffer);
						zest_ConnectInput(ribbon_instance_buffer);
						zest_ConnectOutput(ribbon_vertex_buffer);
						zest_ConnectOutput(ribbon_index_buffer);
						zest_SetPassTask(RecordComputeCommands, app);
						zest_EndPass();
					}

					// Pass 3: Render - Draw the computed ribbon geometry
					zest_BeginRenderPass("Graphics Pass"); {
						zest_ConnectInput(ribbon_vertex_buffer);
						zest_ConnectInput(ribbon_index_buffer);
						zest_ConnectSwapChainOutput();
						zest_SetPassTask(RecordRibbonDrawing, app);
						zest_EndPass();
					}

					// Pass 4: ImGui overlay (if there's UI to draw)
					zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui, app->imgui.main_viewport);
					if (imgui_pass) {
						zest_ConnectSwapChainOutput();
						zest_EndPass();
					}

					frame_graph = zest_EndFrameGraph();

					// Debug: print compiled graph once
					static bool print_graph = true;
					if (print_graph) {
						zest_PrintCompiledFrameGraph(frame_graph);
						print_graph = false;
					}
				}
			}
			zest_EndFrame(app->context, frame_graph);
		}
	}
}

int main(int argc, char *argv[]) {
	Ribbons imgui_app{};

	//Create a window using SDL2. We must do this before setting up the device as it's needed to get
	//the extensions info.
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "Ribbons");

	// Create Vulkan device (one per application)
	imgui_app.device = zest_implsdl2_CreateVulkanDevice(&window_data, false);

	zest_create_context_info_t create_info = zest_CreateContextInfo();
	imgui_app.context = zest_CreateContext(imgui_app.device, &window_data, &create_info);

	InitImGuiApp(&imgui_app);
	MainLoop(&imgui_app);

	// Cleanup
	ImGui_ImplSDL2_Shutdown();
	zest_imgui_Destroy(&imgui_app.imgui);
	zest_DestroyDevice(imgui_app.device);

	return 0;
}
