#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_ALL_UTILITIES_IMPLEMENTATION
#define ZEST_TEST_MODE
#include "zest-pbr-deferred.h"
#include "zest.h"
#include "imgui_internal.h"

void SetupBRDFLUT(SimplePBRExample *app) {
	zest_image_info_t image_info = zest_CreateImageInfo(512, 512);
	image_info.format = zest_format_r16g16_sfloat;
	image_info.flags = zest_image_preset_storage;
	app->brd_texture = zest_CreateImage(app->context, &image_info);
	zest_image brd_image = zest_GetImage(app->brd_texture);

	app->brd_bindless_texture_index = zest_AcquireStorageImageIndex(app->device, brd_image, zest_storage_image_binding);
	zest_AcquireSampledImageIndex(app->device, brd_image, zest_texture_2d_binding);

	app->brd_compute = zest_CreateCompute(app->device, "Brd Compute", app->brd_shader, app);
	zest_compute compute = zest_GetCompute(app->brd_compute);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_uint group_count_x = (512 + local_size_x - 1) / local_size_x;
	zest_uint group_count_y = (512 + local_size_y - 1) / local_size_y;

	zest_uint push;
	push = app->brd_bindless_texture_index;

	zest_queue queue = zest_imm_BeginCommandBuffer(app->device, zest_queue_compute);
	zest_imm_SendPushConstants(queue, &push, sizeof(zest_uint));
	zest_imm_BindComputePipeline(queue, compute);
	zest_imm_DispatchCompute(queue, group_count_x, group_count_y, 6);
	zest_imm_EndCommandBuffer(queue);
}

void SetupIrradianceCube(SimplePBRExample *app) {
	zest_image_info_t image_info = zest_CreateImageInfo(64, 64);
	image_info.format = zest_format_r32g32b32a32_sfloat;
	image_info.flags = zest_image_preset_storage_cubemap;
	image_info.layer_count = 6;
	app->irr_texture = zest_CreateImage(app->context, &image_info);
	zest_image irr_image = zest_GetImage(app->irr_texture);
	zest_image skybox_image = zest_GetImage(app->skybox_texture);
	app->irr_bindless_texture_index = zest_AcquireStorageImageIndex(app->device, irr_image, zest_storage_image_binding);
	zest_AcquireSampledImageIndex(app->device, irr_image, zest_texture_cube_binding);

	app->irr_compute = zest_CreateCompute(app->device, "irradiance compute", app->irr_shader, app);
	zest_compute compute = zest_GetCompute(app->irr_compute);

	irr_push_constant_t push;
	push.source_env_index = app->skybox_bindless_texture_index;
	push.irr_index = app->irr_bindless_texture_index;
	push.sampler_index = app->sampler_2d_index;
	float delta_phi = (2.0f * float(ZEST_PI)) / 180.0f;
	float delta_theta = (0.5f * float(ZEST_PI)) / 64.0f;
	push.delta_phi = delta_phi;
	push.delta_theta = delta_theta;
	zest_uint local_size = 8;
	zest_uint group_count_x = (64 + local_size - 1) / local_size;
	zest_uint group_count_y = (64 + local_size - 1) / local_size;

	zest_queue queue = zest_imm_BeginCommandBuffer(app->device, zest_queue_compute);
	zest_imm_SendPushConstants(queue, &push, sizeof(irr_push_constant_t));
	zest_imm_BindComputePipeline(queue, compute);
	zest_imm_DispatchCompute(queue, group_count_x, group_count_y, 6);
	zest_imm_EndCommandBuffer(queue);
}

void SetupPrefilteredCube(SimplePBRExample *app) {
	zest_image_info_t image_info = zest_CreateImageInfo(512, 512);
	image_info.format = zest_format_r16g16_sfloat;
	image_info.flags = zest_image_preset_storage_mipped_cubemap;
	image_info.layer_count = 6;
	app->prefiltered_texture = zest_CreateImage(app->context, &image_info);
	zest_image prefiltered_image = zest_GetImage(app->prefiltered_texture);
	zest_image skybox_image = zest_GetImage(app->skybox_texture);

	app->prefiltered_view_array = zest_CreateImageViewsPerMip(app->context, prefiltered_image);
	zest_image_view_array prefiltered_view_array = zest_GetImageViewArray(app->prefiltered_view_array);
	app->prefiltered_bindless_texture_index = zest_AcquireStorageImageIndex(app->device, prefiltered_image, zest_storage_image_binding);
	app->prefiltered_mip_indexes = zest_AcquireImageMipIndexes(app->device, prefiltered_image, prefiltered_view_array, zest_storage_image_binding, zest_descriptor_type_storage_image);
	zest_AcquireSampledImageIndex(app->device, prefiltered_image, zest_texture_cube_binding);

	app->prefiltered_compute = zest_CreateCompute(app->device, "prefiltered compute", app->prefiltered_shader, app);
	zest_compute compute = zest_GetCompute(app->prefiltered_compute);

	const zest_uint local_size = 8;
	prefiltered_push_constant_t push;
	push.source_env_index = app->skybox_bindless_texture_index;
	push.num_samples = 32;
	push.sampler_index = app->sampler_2d_index;
	push.skybox_sampler_index = app->sampler_2d_index;

	const zest_image_info_t *prefiltered_image_info = zest_ImageInfo(prefiltered_image);

	zest_queue queue = zest_imm_BeginCommandBuffer(app->device, zest_queue_compute);
	zest_imm_SendPushConstants(queue, &push, sizeof(irr_push_constant_t));
	zest_imm_BindComputePipeline(queue, compute);
	for (zest_uint m = 0; m < prefiltered_image_info->mip_levels; m++) {
		push.roughness = (float)m / (float)(prefiltered_image_info->mip_levels - 1);
		push.prefiltered_index = app->prefiltered_mip_indexes[m];

		zest_imm_SendPushConstants(queue, &push, sizeof(prefiltered_push_constant_t));

		float mip_width = static_cast<float>(512 * powf(0.5f, (float)m));
		float mip_height = static_cast<float>(512 * powf(0.5f, (float)m));
		zest_uint group_count_x = (zest_uint)ceilf(mip_width / local_size);
		zest_uint group_count_y = (zest_uint)ceilf(mip_height / local_size);

		//Dispatch the compute shader
		zest_imm_DispatchCompute(queue, group_count_x, group_count_y, 6);
	}
	zest_imm_EndCommandBuffer(queue);
}

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
	zest_shader_handle pbr_irradiance_vert = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-pbr-deferred/shaders/pbr_irradiance.vert", "pbr_irradiance_vert.spv", zest_vertex_shader, true);
	zest_shader_handle pbr_irradiance_frag = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-pbr-deferred/shaders/pbr_irradiance.frag", "pbr_irradiance_frag.spv", zest_fragment_shader, true);
	zest_shader_handle skybox_vert = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-pbr-deferred/shaders/sky_box.vert", "sky_box_vert.spv", zest_vertex_shader, true);
	zest_shader_handle skybox_frag = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-pbr-deferred/shaders/sky_box.frag", "sky_box_frag.spv", zest_fragment_shader, true);
	app->brd_shader = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-pbr-deferred/shaders/genbrdflut.comp", "genbrdflut_comp.spv", zest_compute_shader, true);
	app->irr_shader = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-pbr-deferred/shaders/irradiancecube.comp", "irradiancecube_comp.spv", zest_compute_shader, true);
	app->prefiltered_shader = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-pbr-deferred/shaders/prefilterenvmap.comp", "prefilterenvmap_comp.spv", zest_compute_shader, true);

	// Deferred rendering shaders
	app->gbuffer_vert = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-pbr-deferred/shaders/gbuffer.vert", "gbuffer_vert.spv", zest_vertex_shader, true);
	app->gbuffer_frag = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-pbr-deferred/shaders/gbuffer.frag", "gbuffer_frag.spv", zest_fragment_shader, true);
	app->lighting_vert = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-pbr-deferred/shaders/deferred_lighting.vert", "deferred_lighting_vert.spv", zest_vertex_shader, true);
	app->lighting_frag = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-pbr-deferred/shaders/deferred_lighting.frag", "deferred_lighting_frag.spv", zest_fragment_shader, true);

	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	app->cube_sampler = zest_CreateSampler(app->context, &sampler_info);
	app->sampler_2d = zest_CreateSampler(app->context, &sampler_info);

	zest_sampler cube_sampler = zest_GetSampler(app->cube_sampler);
	zest_sampler sampler_2d = zest_GetSampler(app->sampler_2d);

	app->skybox_texture = zest_LoadCubemap(app->context, "Pisa Cube", "examples/assets/pisa_cube.ktx");
	zest_image skybox_image = zest_GetImage(app->skybox_texture);
	app->skybox_bindless_texture_index = zest_AcquireSampledImageIndex(app->device, skybox_image, zest_texture_cube_binding);

	app->sampler_2d_index = zest_AcquireSamplerIndex(app->device, sampler_2d);
	app->cube_sampler_index = zest_AcquireSamplerIndex(app->device, cube_sampler);

	app->timeline = zest_CreateExecutionTimeline(app->device);
	SetupBRDFLUT(app);
	SetupIrradianceCube(app);
	SetupPrefilteredCube(app);

	zest_uniform_buffer view_buffer = zest_GetUniformBuffer(app->view_buffer);
	zest_uniform_buffer lights_buffer = zest_GetUniformBuffer(app->lights_buffer);

	app->pbr_pipeline = zest_BeginPipelineTemplate(app->device, "pipeline_mesh_instance");
	zest_AddVertexInputBindingDescription(app->pbr_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexInputBindingDescription(app->pbr_pipeline, 1, sizeof(zest_mesh_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);                                          // Location 0: Vertex Position
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 1, zest_format_r8g8b8a8_unorm, offsetof(zest_vertex_t, color));               // Location 1: Vertex Color
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 2, zest_format_r32g32b32_sfloat, offsetof(zest_vertex_t, normal));            // Location 3: Vertex Position
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 3, zest_format_r16g16_unorm, offsetof(zest_vertex_t, uv));                     // Location 2: Group id
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 4, zest_format_r16g16b16a16_unorm, offsetof(zest_vertex_t, tangent));                     // Location 2: Group id
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 5, zest_format_r32_uint, offsetof(zest_vertex_t, group_id));                     // Location 2: Group id
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
	zest_SetPipelineDepthTest(app->skybox_pipeline, true, false);  // Depth test ON, depth write OFF - skybox only renders where nothing else is

	// G-buffer pipeline (same vertex layout as PBR pipeline)
	app->gbuffer_pipeline = zest_CopyPipelineTemplate("pipeline_gbuffer", app->pbr_pipeline);
	zest_SetPipelineShaders(app->gbuffer_pipeline, app->gbuffer_vert, app->gbuffer_frag);
	zest_SetPipelineDepthTest(app->gbuffer_pipeline, true, true);  // Enable depth test and write for proper occlusion

	// Deferred lighting pipeline (fullscreen, no vertex input)
	app->lighting_pipeline = zest_BeginPipelineTemplate(app->device, "pipeline_deferred_lighting");
	zest_SetPipelineShaders(app->lighting_pipeline, app->lighting_vert, app->lighting_frag);
	zest_SetPipelineDisableVertexInput(app->lighting_pipeline);
	zest_SetPipelineDepthTest(app->lighting_pipeline, false, false);

	zest_mesh cube = zest_CreateCube(app->context, 1.f, zest_ColorSet(0, 50, 100, 255));
	zest_mesh sphere = zest_CreateSphere(app->context, 100, 100, 1.f, zest_ColorSet(0, 50, 100, 255));
	zest_mesh teapot = LoadGLTFMesh(app->context, "examples/assets/gltf/teapot.gltf", .5f);
	zest_mesh torus = LoadGLTFMesh(app->context, "examples/assets/gltf/torusknot.gltf", .05f);
	zest_mesh venus = LoadGLTFMesh(app->context, "examples/assets/gltf/venus.gltf", .5f);
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

	app->teapot_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), teapot);
	app->torus_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), torus);
	app->venus_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), venus);
	app->cube_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), cube);
	app->sphere_index = zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), sphere);
	app->skybox_index = zest_AddMeshToLayer(zest_GetLayer(app->skybox_layer), sky_box);

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

void DrawGBufferPass(const zest_command_list command_list, void *user_data) {
	SimplePBRExample *app = (SimplePBRExample *)user_data;
	zest_layer mesh_layer = zest_GetLayer(app->mesh_layer);
	zest_DrawInstanceMeshLayer(command_list, mesh_layer);
}

void DrawLightingPass(const zest_command_list command_list, void *user_data) {
	SimplePBRExample *app = (SimplePBRExample *)user_data;

	// Get G-buffer bindless indices
	zest_resource_node gPosition = zest_GetPassInputResource(command_list, "GBuffer_Position");
	zest_resource_node gNormal = zest_GetPassInputResource(command_list, "GBuffer_Normal");
	zest_resource_node gAlbedo = zest_GetPassInputResource(command_list, "GBuffer_Albedo");
	zest_resource_node gPBR = zest_GetPassInputResource(command_list, "GBuffer_PBR");

	app->lighting_push.gPosition_index = zest_GetTransientSampledImageBindlessIndex(command_list, gPosition, zest_texture_2d_binding);
	app->lighting_push.gNormal_index = zest_GetTransientSampledImageBindlessIndex(command_list, gNormal, zest_texture_2d_binding);
	app->lighting_push.gAlbedo_index = zest_GetTransientSampledImageBindlessIndex(command_list, gAlbedo, zest_texture_2d_binding);
	app->lighting_push.gPBR_index = zest_GetTransientSampledImageBindlessIndex(command_list, gPBR, zest_texture_2d_binding);

	zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);
	zest_pipeline pipeline = zest_PipelineWithTemplate(app->lighting_pipeline, command_list);
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_SendPushConstants(command_list, &app->lighting_push, sizeof(deferred_lighting_push_t));
	zest_cmd_Draw(command_list, 3, 1, 0, 0);  // Fullscreen triangle
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

			// Set up G-buffer push constants for mesh layer
			app->gbuffer_push.color = app->material_push.color;
			app->gbuffer_push.view_buffer_index = app->material_push.view_buffer_index;
			zest_SetLayerPushConstants(mesh_layer, &app->gbuffer_push, sizeof(gbuffer_push_t));

			// Set up lighting push constants (G-buffer indices set in callback)
			app->lighting_push.camera = zest_Vec4Set(app->camera.position.x, app->camera.position.y, app->camera.position.z, 0.f);
			app->lighting_push.color = app->material_push.color;
			app->lighting_push.irradiance_index = zest_ImageDescriptorIndex(irr_image, zest_texture_cube_binding);
			app->lighting_push.brd_lookup_index = zest_ImageDescriptorIndex(brd_image, zest_texture_2d_binding);
			app->lighting_push.pre_filtered_index = zest_ImageDescriptorIndex(prefiltered_image, zest_texture_cube_binding);
			app->lighting_push.sampler_index = app->sampler_2d_index;
			app->lighting_push.view_index = app->material_push.view_buffer_index;
			app->lighting_push.lights_index = app->material_push.lights_buffer_index;

			zest_SetLayerColor(mesh_layer, 255, 255, 255, 255);
			float count = 10.f;
			float zero[3] = { 0 };
			float upright[3] = { 0, 0, -ZEST_PI * .5f };
			for (int m = 0; m != 5; m++) {
				zest_SetMeshInstanceDrawing(mesh_layer, m, app->gbuffer_pipeline);
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
							zest_DrawInstancedMesh(mesh_layer, &position.x, upright, &scale.x, roughness, metallic);
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
			zest_SetInstanceDrawing(skybox_layer, app->skybox_pipeline);
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

			// G-buffer resource definitions for deferred rendering
			zest_image_resource_info_t gbuffer_position_info = {
				zest_format_r16g16b16a16_sfloat,
				zest_resource_usage_hint_none,
				zest_ScreenWidth(app->context),
				zest_ScreenHeight(app->context),
				1, 1
			};
			zest_image_resource_info_t gbuffer_normal_info = {
				zest_format_r16g16b16a16_sfloat,
				zest_resource_usage_hint_none,
				zest_ScreenWidth(app->context),
				zest_ScreenHeight(app->context),
				1, 1
			};
			zest_image_resource_info_t gbuffer_albedo_info = {
				zest_format_r8g8b8a8_unorm,
				zest_resource_usage_hint_none,
				zest_ScreenWidth(app->context),
				zest_ScreenHeight(app->context),
				1, 1
			};
			zest_image_resource_info_t gbuffer_pbr_info = {
				zest_format_r8g8b8a8_unorm,
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
				if (zest_BeginFrameGraph(app->context, "Deferred PBR", &cache_key)) {
					// Layer resources
					zest_resource_node mesh_layer_resource = zest_AddTransientLayerResource("Mesh Layer", mesh_layer, false);
					zest_resource_node skybox_layer_resource = zest_AddTransientLayerResource("Sky Box Layer", skybox_layer, false);

					// G-buffer transient resources
					zest_resource_node gPosition = zest_AddTransientImageResource("GBuffer_Position", &gbuffer_position_info);
					zest_resource_node gNormal = zest_AddTransientImageResource("GBuffer_Normal", &gbuffer_normal_info);
					zest_resource_node gAlbedo = zest_AddTransientImageResource("GBuffer_Albedo", &gbuffer_albedo_info);
					zest_resource_node gPBR = zest_AddTransientImageResource("GBuffer_PBR", &gbuffer_pbr_info);
					zest_resource_node depth_buffer = zest_AddTransientImageResource("Depth Buffer", &depth_info);

					// IBL and skybox texture imports
					zest_resource_node skybox_texture_resource = zest_ImportImageResource("Sky Box Texture", skybox_image, 0);
					zest_resource_node brd_texture_resource = zest_ImportImageResource("BRD lookup texture", brd_image, 0);
					zest_resource_node irradiance_texture_resource = zest_ImportImageResource("Irradiance texture", irr_image, 0);
					zest_resource_node prefiltered_texture_resource = zest_ImportImageResource("Prefiltered texture", prefiltered_image, 0);
					zest_resource_node swapchain_node = zest_ImportSwapchainResource();

					// G-buffer output group (MRT)
					zest_output_group gbuffer_group = zest_CreateOutputGroup();
					zest_AddImageToRenderTargetGroup(gbuffer_group, gPosition);
					zest_AddImageToRenderTargetGroup(gbuffer_group, gNormal);
					zest_AddImageToRenderTargetGroup(gbuffer_group, gAlbedo);
					zest_AddImageToRenderTargetGroup(gbuffer_group, gPBR);
					zest_AddImageToRenderTargetGroup(gbuffer_group, depth_buffer);

					// Final output group - ALL passes writing to swapchain must use the same group
					zest_output_group final_group = zest_CreateOutputGroup();
					zest_AddSwapchainToRenderTargetGroup(final_group);
					zest_AddImageToRenderTargetGroup(final_group, depth_buffer);

					//-------------------------Transfer Pass----------------------------------------------------
					zest_BeginTransferPass("Upload Mesh Data"); {
						zest_ConnectOutput(mesh_layer_resource);
						zest_ConnectOutput(skybox_layer_resource);
						zest_SetPassTask(UploadMeshData, app);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//------------------------ G-Buffer Pass ------------------------------------------------------------
					zest_BeginRenderPass("G-Buffer Pass"); {
						zest_ConnectInput(mesh_layer_resource);
						zest_ConnectGroupedOutput(gbuffer_group);
						zest_SetPassTask(DrawGBufferPass, app);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//------------------------ Deferred Lighting Pass ---------------------------------------------------
					zest_BeginRenderPass("Lighting Pass"); {
						zest_ConnectInput(gPosition);
						zest_ConnectInput(gNormal);
						zest_ConnectInput(gAlbedo);
						zest_ConnectInput(gPBR);
						zest_ConnectInput(brd_texture_resource);
						zest_ConnectInput(irradiance_texture_resource);
						zest_ConnectInput(prefiltered_texture_resource);
						zest_ConnectGroupedOutput(final_group);
						zest_SetPassTask(DrawLightingPass, app);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//------------------------ Skybox Layer Pass (after lighting) ---------------------------------------
					zest_BeginRenderPass("Skybox Pass"); {
						zest_ConnectInput(skybox_texture_resource);
						zest_ConnectInput(skybox_layer_resource);
						zest_ConnectGroupedOutput(final_group);
						zest_SetPassTask(zest_DrawInstanceMeshLayer, skybox_layer);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//------------------------ ImGui Pass ----------------------------------------------------------------
					//If there's imgui to draw then draw it
					zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui, app->imgui.main_viewport); {
						if (imgui_pass) {
							zest_ConnectGroupedOutput(final_group);
						} else {
							//If there's no ImGui to render then just render a blank screen
							zest_BeginRenderPass("Draw Nothing");
							//Add the swap chain as an output to the imgui render pass. This is telling the render graph where it should render to.
							zest_ConnectGroupedOutput(final_group);
							zest_SetPassTask(zest_EmptyRenderPass, 0);
						}
						zest_EndPass();
					}
					//----------------------------------------------------------------------------------------------------

					//End the render graph and execute it. This will submit it to the GPU.
					frame_graph = zest_EndFrameGraph();
					int d = 0;
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

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
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
	zest_AddDeviceBuilderValidation(device_builder);
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
#else
int main(void) {
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	zest_implglfw_SetCallbacks(&create_info);
    ZEST__FLAG(create_info.flags, zest_init_flag_maximised);

	SimplePBRExample imgui_app;

    create_info.log_path = ".";
	zest_CreateContext(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitSimplePBRExample(&imgui_app);

	zest_Start();

	return 0;
}
#endif
