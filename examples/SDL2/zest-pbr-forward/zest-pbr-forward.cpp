#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_ALL_UTILITIES_IMPLEMENTATION
#include "zest-pbr-forward.h"
#include "zest.h"
#include "imgui_internal.h"
#include "examples/Common/pbr_functions.cpp"
#include "examples/Common/sdl_controls.cpp"
#include "examples/Common/sdl_events.cpp"

/*
Physically Based Rendering with Image Based Lighting Example
Recreated from Sascha Willems "Physically based rendering with image based lighting"
https://github.com/SaschaWillems/Vulkan/tree/master/examples/pbribl

This example demonstrates:
- PBR (Physically Based Rendering) with metallic-roughness workflow
- IBL (Image Based Lighting) using environment cubemaps
- Pre-computed IBL textures:
  - BRDF LUT (lookup table for split-sum approximation)
  - Irradiance cubemap (diffuse IBL from environment)
  - Pre-filtered environment map (specular IBL with roughness mip levels)
- Instanced mesh rendering with per-instance roughness/metallic values
- Skybox rendering using the environment cubemap
- Multiple mesh types (sphere, cube, teapot, torus, venus) with varying material properties
*/

void InitSimplePBRExample(SimplePBRExample *app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise(app->context, &app->imgui, zest_implsdl2_DestroyWindow);
	ImGui_ImplSDL2_InitForVulkan((SDL_Window *)zest_Window(app->context));
	zest_imgui_DarkStyle(&app->imgui);

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

	//Setup free-look camera
	app->camera = zest_CreateCamera();
	float position[3] = { -2.5f, 0.f, 0.f };
	zest_CameraPosition(&app->camera, position);
	zest_CameraSetFoV(&app->camera, 60.f);
	zest_CameraSetYaw(&app->camera, zest_Radians(-90.f));
	zest_CameraSetPitch(&app->camera, zest_Radians(0.f));
	zest_CameraUpdateFront(&app->camera);
	app->new_camera_position = app->camera.position;
	app->old_camera_position = app->camera.position;

	app->timer = zest_CreateTimer(60);
	app->request_graph_print = 0;
	app->reset = false;

	//Create uniform buffers for lighting and view data
	app->lights_buffer = zest_CreateUniformBuffer(app->context, "Lights", sizeof(UniformLights));
	app->view_buffer = zest_CreateUniformBuffer(app->context, "View", sizeof(uniform_buffer_data_t));

	//Default material color (green)
	app->material_push = { 0 };
	app->material_push.color.x = 0.1f;
	app->material_push.color.y = 0.8f;
	app->material_push.color.z = 0.1f;

	//Load shaders for PBR rendering and skybox
	zest_shader_handle pbr_irradiance_vert = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-pbr-forward/shaders/pbr_irradiance.vert", "pbr_irradiance_vert.spv", zest_vertex_shader, true);
	zest_shader_handle pbr_irradiance_frag = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-pbr-forward/shaders/pbr_irradiance.frag", "pbr_irradiance_frag.spv", zest_fragment_shader, true);
	zest_shader_handle skybox_vert = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-pbr-forward/shaders/sky_box.vert", "sky_box_vert.spv", zest_vertex_shader, true);
	zest_shader_handle skybox_frag = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-pbr-forward/shaders/sky_box.frag", "sky_box_frag.spv", zest_fragment_shader, true);

	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	app->sampler_2d = zest_CreateSampler(app->context, &sampler_info);
	zest_sampler sampler_2d = zest_GetSampler(app->sampler_2d);

	//Load the environment cubemap (used for skybox and IBL source)
	app->skybox_texture = zest_LoadKTX(app->device, "Pisa Cube", "examples/assets/pisa_cube.ktx");
	zest_image skybox_image = zest_GetImage(app->skybox_texture);
	app->skybox_bindless_texture_index = zest_AcquireSampledImageIndex(app->device, skybox_image, zest_texture_cube_binding);
	app->sampler_2d_index = zest_AcquireSamplerIndex(app->device, sampler_2d);

	//Generate IBL (Image Based Lighting) textures from the environment cubemap.
	//These are pre-computed once at startup using compute shaders (see pbr_functions.cpp):
	//- BRDF LUT: 2D lookup table for the split-sum approximation of the specular BRDF
	//- Irradiance cube: Diffuse irradiance from the environment (low-frequency lighting)
	//- Pre-filtered cube: Specular environment map with roughness stored in mip levels
	app->brd_texture = CreateBRDFLUT(app->context);
	app->irr_texture = CreateIrradianceCube(app->context, app->skybox_texture, app->sampler_2d_index);
	app->prefiltered_texture = CreatePrefilteredCube(app->context, app->skybox_texture, app->sampler_2d_index, &app->prefiltered_mip_indexes);

	//Get descriptor indices for the IBL textures to pass to shaders via push constants
	app->material_push.irradiance_index = zest_ImageDescriptorIndex(zest_GetImage(app->irr_texture), zest_texture_cube_binding);
	app->material_push.brd_lookup_index = zest_ImageDescriptorIndex(zest_GetImage(app->brd_texture), zest_texture_2d_binding);
	app->material_push.pre_filtered_index = zest_ImageDescriptorIndex(zest_GetImage(app->prefiltered_texture), zest_texture_cube_binding);

	zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);
	zest_uniform_buffer lights_buffer = zest_GetUniformBuffer(app->lights_buffer);

	//PBR pipeline with instanced mesh rendering.
	//Per-vertex: position, color, normal, uv, tangent, parameters
	//Per-instance: position, color, rotation, roughness, scale, metallic
	app->pbr_pipeline = zest_CreatePipelineTemplate(app->device, "pipeline_forward_mesh_instance");
	zest_AddVertexInputBindingDescription(app->pbr_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexInputBindingDescription(app->pbr_pipeline, 1, sizeof(forward_mesh_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 1, zest_format_r8g8b8a8_unorm, offsetof(zest_vertex_t, color));
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 2, zest_format_r32g32b32_sfloat, offsetof(zest_vertex_t, normal));
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 3, zest_format_r32g32_sfloat, offsetof(zest_vertex_t, uv));
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 4, zest_format_r16g16b16a16_unorm, offsetof(zest_vertex_t, tangent));
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 5, zest_format_r32_uint, offsetof(zest_vertex_t, parameters));
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 6, zest_format_r32g32b32_sfloat, 0);
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 7, zest_format_r8g8b8a8_unorm, offsetof(forward_mesh_instance_t, color));
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 8, zest_format_r32g32b32_sfloat, offsetof(forward_mesh_instance_t, rotation));
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 9, zest_format_r32_sfloat, offsetof(forward_mesh_instance_t, roughness));
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 10, zest_format_r32g32b32_sfloat, offsetof(forward_mesh_instance_t, scale));
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 11, zest_format_r32_sfloat, offsetof(forward_mesh_instance_t, metallic));
	zest_SetPipelineShaders(app->pbr_pipeline, pbr_irradiance_vert, pbr_irradiance_frag);
	zest_SetPipelineCullMode(app->pbr_pipeline, zest_cull_mode_back);
	zest_SetPipelineFrontFace(app->pbr_pipeline, zest_front_face_counter_clockwise);
	zest_SetPipelineTopology(app->pbr_pipeline, zest_topology_triangle_list);
	zest_SetPipelineDepthTest(app->pbr_pipeline, true, true);

	//Skybox pipeline - copy from PBR pipeline and modify for inside-out rendering.
	//No depth test/write since skybox is rendered first and always at infinity.
	app->skybox_pipeline = zest_CopyPipelineTemplate("sky_box", app->pbr_pipeline);
	zest_SetPipelineFrontFace(app->skybox_pipeline, zest_front_face_clockwise);  //Inside-out cube
	zest_SetPipelineShaders(app->skybox_pipeline, skybox_vert, skybox_frag);
	zest_SetPipelineDepthTest(app->skybox_pipeline, false, false);

	//Create various meshes to demonstrate PBR on different shapes
	zest_mesh cube = zest_CreateCube(app->context, 1.f, zest_ColorSet(0, 50, 100, 255));
	zest_mesh sphere = zest_CreateSphere(app->context, 100, 100, 1.f, zest_ColorSet(0, 50, 100, 255));
	zest_mesh teapot = LoadGLTFScene(app->context, "examples/assets/gltf/teapot.gltf", .5f);
	zest_mesh torus = LoadGLTFScene(app->context, "examples/assets/gltf/torusknot.gltf", 1.f);
	zest_mesh venus = LoadGLTFScene(app->context, "examples/assets/gltf/venus.gltf", 2.f);
	zest_mesh sky_box = zest_CreateCube(app->context, 1.f, zest_ColorSet(255, 255, 255, 255));

	//Calculate total capacity needed for all meshes
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

	//Create mesh layers - one for PBR objects, one for skybox (separate so skybox renders first)
	app->mesh_layer = zest_CreateInstanceMeshLayer(app->context, "Mesh Layer", sizeof(forward_mesh_instance_t), vertex_capacity, index_capacity);
	app->skybox_layer = zest_CreateInstanceMeshLayer(app->context, "Skybox Layer", sizeof(forward_mesh_instance_t), zest_MeshVertexDataSize(sky_box), zest_MeshIndexDataSize(sky_box));

	//Add meshes to layer and store indices for later reference
	app->teapot_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), teapot, 0);
	app->torus_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), torus, 0);
	app->venus_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), venus, 0);
	app->cube_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), cube, 0);
	app->sphere_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), sphere, 0);
	app->skybox_index = zest_AddMeshToLayer(zest_GetLayer(app->skybox_layer), sky_box, 0);

	//Free CPU-side mesh data now that it's uploaded
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
	buffer_data->sampler_index = app->sampler_2d_index;
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

void DrawInstancedMesh(zest_layer layer, float pos[3], float rot[3], float scale[3], float roughness, float metallic) {
    forward_mesh_instance_t* instance = (forward_mesh_instance_t*)zest_NextInstance(layer);

	instance->pos = { pos[0], pos[1], pos[2] };
	instance->rotation = { rot[0], rot[1], rot[2] };
	instance->scale = { scale[0], scale[1], scale[2] };
    instance->color = layer->current_color;
	instance->roughness = roughness;
	instance->metallic = metallic;
}

void UpdateImGui(SimplePBRExample *app) {
	//We can use a timer to only update the gui every 60 times a second (or whatever you decide). This
	//means that the buffers are uploaded less frequently and the command buffer is also re-recorded
	//less frequently.
	zest_StartTimerLoop(app->timer) {
		//Must call the imgui SDL2 implementation function
		ImGui_ImplSDL2_NewFrame();
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
		UpdateCameraPosition(&app->timer, &app->new_camera_position, &app->old_camera_position, &app->camera, 5.f);
	} zest_EndTimerLoop(app->timer);
}

void MainLoop(SimplePBRExample *app) {
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

		zest_layer mesh_layer = zest_GetLayer(app->mesh_layer);
		zest_layer skybox_layer = zest_GetLayer(app->skybox_layer);

		if (zest_BeginFrame(app->context)) {

			UpdateMouse(app->context, &app->mouse, &app->camera);

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

			//Draw a grid of meshes with varying roughness and metallic values.
			//Each row is a different mesh type, each column varies material properties.
			//This demonstrates how PBR responds to different material parameters.
			float count = 10.f;
			float zero[3] = { 0 };
			for (int m = 0; m != 5; m++) {
				zest_StartInstanceMeshDrawing(mesh_layer, m, app->pbr_pipeline);
				zest_SetLayerPushConstants(mesh_layer, &app->material_push, sizeof(pbr_consts_t));
				for (float i = 0; i < count; i++) {
					//Vary roughness from smooth (0) to rough (1) across columns
					//Vary metallic from dielectric (0) to metallic (1) across columns
					float roughness = 1.0f - ZEST__CLAMP(i / count, 0.005f, 1.0f);
					float metallic = ZEST__CLAMP(i / count, 0.005f, 1.0f);
					switch (m) {
						case 0:
						case 1:
						case 3: {
							DrawInstancedMesh(mesh_layer, &position.x, &rotation.x, &scale.x, roughness, metallic);
							break;
						}
						case 2:
						case 4: {
							DrawInstancedMesh(mesh_layer, &position.x, zero, &scale.x, roughness, metallic);
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
			zest_StartInstanceMeshDrawing(skybox_layer, 0, app->skybox_pipeline);
			zest_SetLayerColor(skybox_layer, 255, 255, 255, 255);
			DrawInstancedMesh(skybox_layer, zero, zero, zero, 0, 0);
			zest_SetLayerPushConstants(skybox_layer, sky_push, sizeof(zest_uint) * 2);

			if (app->reset) {
				app->reset = false;
				ImGui_ImplSDL2_Shutdown();
				zest_imgui_Destroy(&app->imgui);
				zest_implsdl2_DestroyWindow(app->context);
				zest_window_data_t window_handles = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "PBR Forward");
				zest_ResetContext(app->context, &window_handles);
				InitSimplePBRExample(app);
			}

			zest_swapchain swapchain = zest_GetSwapchain(app->context);

			//Frame graph caching
			app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw(&app->imgui);
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

			if (!frame_graph) {
				zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);
				zest_uniform_buffer lights_buffer = zest_GetUniformBuffer(app->lights_buffer);
				if (zest_BeginFrameGraph(app->context, "ImGui", &cache_key)) {
					zest_resource_node mesh_layer_resource = zest_AddTransientLayerResource("Mesh Layer", mesh_layer, false);
					zest_resource_node skybox_layer_resource = zest_AddTransientLayerResource("Sky Box Layer", skybox_layer, false);
					zest_resource_node depth_buffer = zest_AddTransientImageResource("Depth Buffer", &depth_info);
					zest_resource_node swapchain_node = zest_ImportSwapchainResource();
					zest_resource_group group = zest_CreateResourceGroup();
					zest_AddSwapchainToGroup(group);
					zest_AddResourceToGroup(group, depth_buffer);

					//-------------------------Transfer Pass------------------------------------------------
					zest_BeginTransferPass("Upload Mesh Data"); {
						zest_ConnectOutput(mesh_layer_resource);
						zest_ConnectOutput(skybox_layer_resource);
						zest_SetPassTask(UploadMeshData, app);
						zest_EndPass();
					}

					//-------------------------Skybox Pass--------------------------------------------------
					//Render skybox first (no depth write, always at infinity)
					zest_BeginRenderPass("Skybox Pass"); {
						zest_ConnectInput(skybox_layer_resource);
						zest_ConnectOutputGroup(group);
						zest_SetPassTask(zest_DrawInstanceMeshLayer, skybox_layer);
						zest_EndPass();
					}

					//-------------------------PBR Mesh Pass------------------------------------------------
					//Render PBR objects over skybox
					zest_BeginRenderPass("Instance Mesh Pass"); {
						zest_ConnectInput(mesh_layer_resource);
						zest_ConnectOutputGroup(group);
						zest_SetPassTask(zest_DrawInstanceMeshLayer, mesh_layer);
						zest_EndPass();
					}

					//-------------------------ImGui Pass---------------------------------------------------
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

int main(int argc, char *argv[]) {

	SimplePBRExample imgui_app = {};

	//Create a window using SDL2. We must do this before setting up the device as it's needed to get
	//the extensions info.
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "PBR Forward");

	//Create Vulkan device, window, and context
	imgui_app.device = zest_implsdl2_CreateVulkanDevice(&window_data, false);
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	imgui_app.context = zest_CreateContext(imgui_app.device, &window_data, &create_info);

	InitSimplePBRExample(&imgui_app);
	MainLoop(&imgui_app);

	//Cleanup
	ImGui_ImplSDL2_Shutdown();
	zest_imgui_Destroy(&imgui_app.imgui);
	zest_DestroyDevice(imgui_app.device);

	return 0;
}
