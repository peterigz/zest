#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_KTX_IMPLEMENTATION
#include "zest-pbr.h"
#include "zest.h"
#include "imgui_internal.h"

void UpdateUniform3d(SimplePBRExample *app) {
	uniform_buffer_data_t *ubo_ptr = static_cast<uniform_buffer_data_t *>(zest_GetUniformBufferData(app->view_buffer));
	ubo_ptr->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	ubo_ptr->proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf(app->context) / zest_ScreenHeightf(app->context), 0.001f, 10000.f);
	ubo_ptr->proj.v[1].y *= -1.f;
	ubo_ptr->screen_size.x = zest_ScreenWidthf(app->context);
	ubo_ptr->screen_size.y = zest_ScreenHeightf(app->context);
}

void SetupBillboards(SimplePBRExample *app) {
	//Create and compile the shaders for our custom sprite pipeline
	zest_shader_handle billboard_vert = zest_CreateShaderFromFile(app->context, "examples/assets/shaders/billboard.vert", "billboard_vert.spv", zest_vertex_shader, true);
	zest_shader_handle billboard_frag = zest_CreateShaderFromFile(app->context, "examples/assets/shaders/billboard.frag", "billboard_frag.spv", zest_fragment_shader, true);

	//Create a pipeline that we can use to draw billboards
	app->billboard_pipeline = zest_BeginPipelineTemplate(app->context, "pipeline_billboard");
	zest_AddVertexInputBindingDescription(app->billboard_pipeline, 0, sizeof(zest_billboard_instance_t), zest_input_rate_instance);

	zest_AddVertexAttribute(app->billboard_pipeline, 0, 0, zest_format_r32g32b32_sfloat, offsetof(zest_billboard_instance_t, position));			    // Location 0: Position
	zest_AddVertexAttribute(app->billboard_pipeline, 0, 1, zest_format_r8g8b8_snorm, offsetof(zest_billboard_instance_t, alignment));		         	// Location 9: Alignment X, Y and Z
	zest_AddVertexAttribute(app->billboard_pipeline, 0, 2, zest_format_r32g32b32a32_sfloat, offsetof(zest_billboard_instance_t, rotations_stretch));	// Location 2: Rotations + stretch
	zest_AddVertexAttribute(app->billboard_pipeline, 0, 3, zest_format_r16g16b16a16_snorm, offsetof(zest_billboard_instance_t, uv));		    		// Location 1: uv_packed
	zest_AddVertexAttribute(app->billboard_pipeline, 0, 4, zest_format_r16g16b16a16_sscaled, offsetof(zest_billboard_instance_t, scale_handle));		// Location 4: Scale + Handle
	zest_AddVertexAttribute(app->billboard_pipeline, 0, 5, zest_format_r32_uint, offsetof(zest_billboard_instance_t, intensity_texture_array));		// Location 6: texture array index * intensity
	zest_AddVertexAttribute(app->billboard_pipeline, 0, 6, zest_format_r8g8b8a8_unorm, offsetof(zest_billboard_instance_t, color));			        // Location 7: Instance Color

	zest_SetPipelinePushConstantRange(app->billboard_pipeline, sizeof(billboard_push_constant_t), zest_shader_render_stages);
	zest_SetPipelineVertShader(app->billboard_pipeline, billboard_vert);
	zest_SetPipelineFragShader(app->billboard_pipeline, billboard_frag);
	zest_AddPipelineDescriptorLayout(app->billboard_pipeline, zest_GetUniformBufferLayout(app->view_buffer));
	zest_AddPipelineDescriptorLayout(app->billboard_pipeline, zest_GetGlobalBindlessLayout(app->context));
	zest_SetPipelineDepthTest(app->billboard_pipeline, true, false);

	app->billboard_layer = zest_CreateInstanceLayer(app->context, "billboards", sizeof(zest_billboard_instance_t));

	app->sprite_resources = zest_CreateShaderResources(app->context);
	zest_AddUniformBufferToResources(app->sprite_resources, app->view_buffer);
	zest_AddGlobalBindlessSetToResources(app->sprite_resources);
}

void zest_DispatchBRDSetup(const zest_command_list command_list, void *user_data) {
	SimplePBRExample *app = (SimplePBRExample *)user_data;

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_descriptor_set sets[] = {
		zest_GetGlobalBindlessSet(app->context)
	};

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, app->brd_compute, sets, 1);

	zest_uint push;

	push = app->brd_bindless_texture_index;

	zest_cmd_SendCustomComputePushConstants(command_list, app->brd_compute, &push);

	zest_uint group_count_x = (512 + local_size_x - 1) / local_size_x;
	zest_uint group_count_y = (512 + local_size_y - 1) / local_size_y;

	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, app->brd_compute, group_count_x, group_count_y, 1);
}

void SetupBRDFLUT(SimplePBRExample *app) {
	zest_image_info_t image_info = zest_CreateImageInfo(512, 512);
	image_info.format = zest_format_r16g16_sfloat;
	image_info.flags = zest_image_preset_storage;
	app->brd_texture = zest_CreateImage(app->context, &image_info);

	app->brd_bindless_texture_index = zest_AcquireGlobalStorageImageIndex(app->brd_texture, zest_storage_image_binding);
	zest_AcquireGlobalSampledImageIndex(app->brd_texture, zest_texture_2d_binding);

	zest_compute_builder_t compute_builder = zest_BeginComputeBuilder(app->context);
	zest_AddComputeShader(&compute_builder, app->brd_shader);
	zest_AddComputeSetLayout(&compute_builder, zest_GetGlobalBindlessLayout(app->context));
	zest_SetComputePushConstantSize(&compute_builder, sizeof(zest_uint));
	app->brd_compute = zest_FinishCompute(&compute_builder, "brd compute");

	zest_BeginFrameGraph(app->context, "BRDFLUT", 0);
	zest_resource_node texture_resource = zest_ImportImageResource("Brd texture", app->brd_texture, 0);

	zest_BeginComputePass(app->brd_compute, "Brd compute");
	zest_ConnectOutput(texture_resource);
	zest_SetPassTask(zest_DispatchBRDSetup, app);
	zest_EndPass();

	zest_frame_graph frame_graph = zest_EndFrameGraphAndWait();
}

void zest_DispatchIrradianceSetup(const zest_command_list command_list, void *user_data) {
	SimplePBRExample *app = (SimplePBRExample *)user_data;

	const zest_uint local_size = 8;

	zest_descriptor_set sets[] = {
		zest_GetGlobalBindlessSet(app->context)
	};

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, app->irr_compute, sets, 1);

	app->irr_push_constant.source_env_index = app->skybox_bindless_texture_index;
	app->irr_push_constant.irr_index = app->irr_bindless_texture_index;
	app->irr_push_constant.sampler_index = app->sampler_2d_index;
	float delta_phi = (2.0f * float(ZEST_PI)) / 180.0f;
	float delta_theta = (0.5f * float(ZEST_PI)) / 64.0f;
	app->irr_push_constant.delta_phi = delta_phi;
	app->irr_push_constant.delta_theta = delta_theta;
	
	zest_cmd_SendCustomComputePushConstants(command_list, app->irr_compute, &app->irr_push_constant);

	const zest_image_info_t *info = zest_ImageInfo(app->irr_texture);
	zest_uint group_count_x = (info->extent.width + local_size - 1) / local_size;
	zest_uint group_count_y = (info->extent.height + local_size - 1) / local_size;

	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, app->irr_compute, group_count_x, group_count_y, 6);
}

void SetupIrradianceCube(SimplePBRExample *app) {
	zest_image_info_t image_info = zest_CreateImageInfo(64, 64);
	image_info.format = zest_format_r32g32b32a32_sfloat;
	image_info.flags = zest_image_preset_storage_cubemap;
	image_info.layer_count = 6;
	app->irr_texture = zest_CreateImage(app->context, &image_info);
	app->irr_bindless_texture_index = zest_AcquireGlobalStorageImageIndex(app->irr_texture, zest_storage_image_binding);
	zest_AcquireGlobalSampledImageIndex(app->irr_texture, zest_texture_cube_binding);

	zest_compute_builder_t compute_builder = zest_BeginComputeBuilder(app->context);
	zest_AddComputeShader(&compute_builder, app->irr_shader);
	zest_AddComputeSetLayout(&compute_builder, zest_GetGlobalBindlessLayout(app->context));
	zest_SetComputePushConstantSize(&compute_builder, sizeof(irr_push_constant_t));
	app->irr_compute = zest_FinishCompute(&compute_builder, "irradiance compute");

	zest_BeginFrameGraph(app->context, "Irradiance", 0);
	zest_resource_node skybox_resource = zest_ImportImageResource("Skybox texture", app->skybox_texture, 0);
	zest_resource_node irradiance_resource = zest_ImportImageResource("Irradiance texture", app->irr_texture, 0);

	zest_BeginComputePass(app->irr_compute, "Irradiance compute");
	zest_ConnectInput(skybox_resource);
	zest_ConnectOutput(irradiance_resource);
	zest_SetPassTask(zest_DispatchIrradianceSetup, app);
	zest_EndPass();

	zest_frame_graph frame_graph = zest_EndFrameGraphAndWait();
}

void zest_DispatchPrefilteredSetup(const zest_command_list command_list, void *user_data) {
	SimplePBRExample *app = (SimplePBRExample *)user_data;

	const zest_uint local_size = 8;

	zest_descriptor_set sets[] = {
		zest_GetGlobalBindlessSet(app->context)
	};

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, app->prefiltered_compute, sets, 1);
	
	const zest_image_info_t *image_info = zest_ImageInfo(app->prefiltered_texture);

	app->prefiltered_push_constant.source_env_index = app->skybox_bindless_texture_index;
	app->prefiltered_push_constant.num_samples = 32;
	app->prefiltered_push_constant.sampler_index = app->sampler_2d_index;
	app->prefiltered_push_constant.skybox_sampler_index = app->skybox_sampler_index;
	for (zest_uint m = 0; m < image_info->mip_levels; m++) {
		app->prefiltered_push_constant.roughness = (float)m / (float)(image_info->mip_levels - 1);
		app->prefiltered_push_constant.prefiltered_index = app->prefiltered_mip_indexes[m];

		zest_cmd_SendCustomComputePushConstants(command_list, app->prefiltered_compute, &app->prefiltered_push_constant);

		float mip_width = static_cast<float>(image_info->extent.width * powf(0.5f, (float)m));
		float mip_height = static_cast<float>(image_info->extent.height * powf(0.5f, (float)m));
		zest_uint group_count_x = (zest_uint)ceilf(mip_width / local_size);
		zest_uint group_count_y = (zest_uint)ceilf(mip_height / local_size);

		//Dispatch the compute shader
		zest_cmd_DispatchCompute(command_list, app->prefiltered_compute, group_count_x, group_count_y, 6);
	}
}

void SetupPrefilteredCube(SimplePBRExample *app) {
	zest_image_info_t image_info = zest_CreateImageInfo(512, 512);
	image_info.format = zest_format_r16g16_sfloat;
	image_info.flags = zest_image_preset_storage_mipped_cubemap;
	image_info.layer_count = 6;
	app->prefiltered_texture = zest_CreateImage(app->context, &image_info);

	app->prefiltered_view_array = zest_CreateImageViewsPerMip(app->prefiltered_texture);
	app->prefiltered_bindless_texture_index = zest_AcquireGlobalStorageImageIndex(app->prefiltered_texture, zest_storage_image_binding);
	app->prefiltered_mip_indexes = zest_AcquireGlobalImageMipIndexes(app->prefiltered_texture, app->prefiltered_view_array, zest_storage_image_binding, zest_descriptor_type_storage_image);
	zest_AcquireGlobalSampledImageIndex(app->prefiltered_texture, zest_texture_cube_binding);

	zest_compute_builder_t compute_builder = zest_BeginComputeBuilder(app->context);
	zest_AddComputeShader(&compute_builder, app->prefiltered_shader);
	zest_AddComputeSetLayout(&compute_builder, zest_GetGlobalBindlessLayout(app->context));
	zest_SetComputePushConstantSize(&compute_builder, sizeof(prefiltered_push_constant_t));
	app->prefiltered_compute = zest_FinishCompute(&compute_builder, "prefiltered compute");

	zest_BeginFrameGraph(app->context, "Prefiltered", 0);
	zest_resource_node skybox_resource = zest_ImportImageResource("Skybox texture", app->skybox_texture, 0);
	zest_resource_node prefiltered_resource = zest_ImportImageResource("Prefiltered texture", app->prefiltered_texture, 0);

	zest_BeginComputePass(app->prefiltered_compute, "Prefiltered compute");
	zest_ConnectInput(skybox_resource);
	zest_ConnectOutput(prefiltered_resource);
	zest_SetPassTask(zest_DispatchPrefilteredSetup, app);
	zest_EndPass();

	zest_frame_graph frame_graph = zest_EndFrameGraphAndWait();
}

void InitSimplePBRExample(SimplePBRExample *app) {
	//Initialise Dear ImGui
	app->imgui = zest_imgui_Initialise(app->context);
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)zest_Window(app->context), true);
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
	app->timer = zest_CreateTimer(app->context, 60);
	app->request_graph_print = false;
	app->reset = false;

	app->lights_buffer = zest_CreateUniformBuffer(app->context, "Lights", sizeof(UniformLights));
	app->view_buffer = zest_CreateUniformBuffer(app->context, "View", sizeof(uniform_buffer_data_t));
	app->material_push = { 0 };
	app->material_push.roughness = .1f;
	app->material_push.metallic = 1.f;
	app->material_push.color.x = 0.1f;
	app->material_push.color.y = 0.8f;
	app->material_push.color.z = 0.1f;

	SetupBillboards(app);


	//Compile the shaders we will use to render the particles
	zest_shader_handle pbr_irradiance_vert = zest_CreateShaderFromFile(app->context, "examples/GLFW/zest-pbr/shaders/pbr_irradiance.vert", "pbr_irradiance_vert.spv", zest_vertex_shader, true);
	zest_shader_handle pbr_irradiance_frag = zest_CreateShaderFromFile(app->context, "examples/GLFW/zest-pbr/shaders/pbr_irradiance.frag", "pbr_irradiance_frag.spv", zest_fragment_shader, true);
	zest_shader_handle skybox_vert = zest_CreateShaderFromFile(app->context, "examples/GLFW/zest-pbr/shaders/sky_box.vert", "sky_box_vert.spv", zest_vertex_shader, true);
	zest_shader_handle skybox_frag = zest_CreateShaderFromFile(app->context, "examples/GLFW/zest-pbr/shaders/sky_box.frag", "sky_box_frag.spv", zest_fragment_shader, true);
	app->brd_shader = zest_CreateShaderFromFile(app->context, "examples/GLFW/zest-pbr/shaders/genbrdflut.comp", "genbrdflut_comp.spv", zest_compute_shader, true);
	app->irr_shader = zest_CreateShaderFromFile(app->context, "examples/GLFW/zest-pbr/shaders/irradiancecube.comp", "irradiancecube_comp.spv", zest_compute_shader, true);
	app->prefiltered_shader = zest_CreateShaderFromFile(app->context, "examples/GLFW/zest-pbr/shaders/prefilterenvmap.comp", "prefilterenvmap_comp.spv", zest_compute_shader, true);

	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	app->sampler_2d = zest_CreateSampler(app->context, &sampler_info);
	app->cube_sampler = zest_CreateSampler(app->context, &sampler_info);

	app->skybox_texture = zest_LoadCubemap(app->context, "Pisa Cube", "examples/assets/pisa_cube.ktx");
	app->skybox_sampler = zest_CreateSamplerForImage(app->skybox_texture);
	app->skybox_bindless_texture_index = zest_AcquireGlobalSampledImageIndex(app->skybox_texture, zest_texture_cube_binding);

	app->sampler_2d_index = zest_AcquireGlobalSamplerIndex(app->sampler_2d);
	app->cube_sampler_index = zest_AcquireGlobalSamplerIndex(app->cube_sampler);
	app->skybox_sampler_index = zest_AcquireGlobalSamplerIndex(app->skybox_sampler);

	SetupBRDFLUT(app);
	SetupIrradianceCube(app);
	SetupPrefilteredCube(app);

	app->pbr_pipeline = zest_BeginPipelineTemplate(app->context, "pipeline_mesh_instance");
	zest_SetPipelinePushConstantRange(app->pbr_pipeline, sizeof(pbr_consts_t), zest_shader_render_stages);
	zest_AddVertexInputBindingDescription(app->pbr_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexInputBindingDescription(app->pbr_pipeline, 1, sizeof(zest_mesh_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);                                          // Location 0: Vertex Position
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 1, zest_format_r8g8b8a8_unorm, offsetof(zest_vertex_t, color));               // Location 1: Vertex Color
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 2, zest_format_r32_uint, offsetof(zest_vertex_t, group));                     // Location 2: Group id
	zest_AddVertexAttribute(app->pbr_pipeline, 0, 3, zest_format_r32g32b32_sfloat, offsetof(zest_vertex_t, normal));            // Location 3: Vertex Position
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 4, zest_format_r32g32b32_sfloat, 0);                                          // Location 4: Instance Position
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 5, zest_format_r8g8b8a8_unorm, offsetof(zest_mesh_instance_t, color));        // Location 5: Instance Color
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 6, zest_format_r32g32b32_sfloat, offsetof(zest_mesh_instance_t, rotation));   // Location 6: Instance Rotation
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 7, zest_format_r8g8b8a8_unorm, offsetof(zest_mesh_instance_t, parameters));   // Location 7: Instance Parameters
	zest_AddVertexAttribute(app->pbr_pipeline, 1, 8, zest_format_r32g32b32_sfloat, offsetof(zest_mesh_instance_t, scale));      // Location 8: Instance Scale

	zest_AddPipelineDescriptorLayout(app->pbr_pipeline, zest_GetGlobalBindlessLayout(app->context));
	zest_AddPipelineDescriptorLayout(app->pbr_pipeline, zest_GetUniformBufferLayout(app->view_buffer));
	zest_AddPipelineDescriptorLayout(app->pbr_pipeline, zest_GetUniformBufferLayout(app->lights_buffer));
	zest_SetPipelineShaders(app->pbr_pipeline, pbr_irradiance_vert, pbr_irradiance_frag);
	zest_SetPipelineCullMode(app->pbr_pipeline, zest_cull_mode_back);
	zest_SetPipelineFrontFace(app->pbr_pipeline, zest_front_face_counter_clockwise);
	zest_SetPipelineTopology(app->pbr_pipeline, zest_topology_triangle_list);

	app->skybox_pipeline = zest_CopyPipelineTemplate(app->context, "sky_box", app->pbr_pipeline);
	zest_ClearPipelineDescriptorLayouts(app->skybox_pipeline);
	zest_AddPipelineDescriptorLayout(app->skybox_pipeline, zest_GetGlobalBindlessLayout(app->context));
	zest_AddPipelineDescriptorLayout(app->skybox_pipeline, zest_GetUniformBufferLayout(app->view_buffer));
	zest_AddPipelineDescriptorLayout(app->skybox_pipeline, zest_GetUniformBufferLayout(app->lights_buffer));
	zest_SetPipelineFrontFace(app->skybox_pipeline, zest_front_face_clockwise);
	zest_SetPipelineShaders(app->skybox_pipeline, skybox_vert, skybox_frag);
	zest_SetPipelineDepthTest(app->skybox_pipeline, false, false);

	app->pbr_shader_resources = zest_CreateShaderResources(app->context);
	zest_AddGlobalBindlessSetToResources(app->pbr_shader_resources);							//Set 0
	zest_AddUniformBufferToResources(app->pbr_shader_resources, app->view_buffer);				//Set 1
	zest_AddUniformBufferToResources(app->pbr_shader_resources, app->lights_buffer);			//Set 2

	app->skybox_shader_resources = zest_CreateShaderResources(app->context);
	zest_AddGlobalBindlessSetToResources(app->skybox_shader_resources);							//Set 0
	zest_AddUniformBufferToResources(app->skybox_shader_resources, app->view_buffer);			//Set 1
	zest_AddUniformBufferToResources(app->skybox_shader_resources, app->lights_buffer);			//Set 2

	app->cube_layer = zest_CreateInstanceMeshLayer(app->context, "Cube Layer");
	app->skybox_layer = zest_CreateInstanceMeshLayer(app->context, "Sky Box Layer");
	zest_mesh cube = zest_CreateCube(app->context, 1.f, zest_ColorSet(0, 50, 100, 255));
	zest_mesh sphere = zest_CreateSphere(app->context, 100, 100, 1.f, zest_ColorSet(0, 50, 100, 255));
	zest_mesh cone = zest_CreateCone(app->context, 50, 1.f, 2.f, zest_ColorSet(0, 50, 100, 255));
	zest_mesh cylinder = zest_CreateCylinder(app->context, 100, 1.f, 2.f, zest_ColorSet(0, 50, 100, 255), true);
	zest_mesh sky_box = zest_CreateCube(app->context, 1.f, zest_ColorSet(255, 255, 255, 255));
	zest_AddMeshToLayer(app->cube_layer, cube);
	zest_AddMeshToLayer(app->skybox_layer, sky_box);
	zest_FreeMesh(cube);
	zest_FreeMesh(sphere);
	zest_FreeMesh(cone);
	zest_FreeMesh(cylinder);
	zest_FreeMesh(sky_box);
}

void UpdateLights(SimplePBRExample *app, float timer) {
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

	buffer_data->texture_index = app->skybox_bindless_texture_index;
	buffer_data->exposure = 4.5f;
	buffer_data->gamma = 2.2f;

	/*
	zest_SetInstanceDrawing(app->billboard_layer, app->sprite_resources, app->billboard_pipeline);
	zest_SetLayerColor(app->billboard_layer, 255, 255, 255, 255);
	zest_DrawBillboardSimple(app->billboard_layer, app->light, &buffer_data->lights[0].x, 0.f, 1.f, 1.f);
	zest_DrawBillboardSimple(app->billboard_layer, app->light, &buffer_data->lights[1].x, 0.f, 1.f, 1.f);
	zest_DrawBillboardSimple(app->billboard_layer, app->light, &buffer_data->lights[2].x, 0.f, 1.f, 1.f);
	zest_DrawBillboardSimple(app->billboard_layer, app->light, &buffer_data->lights[3].x, 0.f, 1.f, 1.f);
	*/
}

void UploadMeshData(const zest_command_list context, void *user_data) {
	SimplePBRExample *app = (SimplePBRExample *)user_data;

	zest_layer_handle layers[3]{
		app->cube_layer,
		app->skybox_layer,
		app->billboard_layer
	};

	for (int i = 0; i != 3; ++i) {
		zest_layer_handle layer = layers[i];
		zest_UploadLayerStagingData(layer, context);
	}
}

void UpdateCameraPosition(SimplePBRExample *app) {
	float speed = 5.f * (float)zest_TimerUpdateTime(app->timer);
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
}

void MainLoop(SimplePBRExample *app) {
	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
		glfwPollEvents();

		UpdateMouse(app);

		float elapsed = 0;

		UpdateUniform3d(app);

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

		//We can use a timer to only update the gui every 60 times a second (or whatever you decide). This
		//means that the buffers are uploaded less frequently and the command buffer is also re-recorded
		//less frequently.

		zest_StartTimerLoop(app->timer) {
			//Must call the imgui GLFW implementation function
			ImGui_ImplGlfw_NewFrame();
			//Draw our imgui stuff
			ImGui::NewFrame();
			ImGui::Begin("Test Window");
			ImGui::DragFloat("Rougness", &app->material_push.roughness, 0.01f, 0.f, 1.f);
			ImGui::DragFloat("Metallic", &app->material_push.metallic, 0.01f, 0.f, 1.f);
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
				app->request_graph_print = true;
				zloc_VerifyAllRemoteBlocks(app->context, 0, 0);
			}
			if (ImGui::Button("Reset Renderer")) {
				app->reset = true;
			}
			ImGui::End();
			ImGui::Render();
			//An imgui layer is a manual layer, meaning that you need to let it know that the buffers need updating.
			//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
			zest_imgui_UpdateBuffers(&app->imgui);

			UpdateCameraPosition(app);

			//Restore the mouse when right mouse isn't held down
			if (camera_free_look) {
				glfwSetInputMode((GLFWwindow*)zest_Window(app->context), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			}
			else {
				glfwSetInputMode((GLFWwindow*)zest_Window(app->context), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			}
		} zest_EndTimerLoop(app->timer);

		app->camera.position = zest_LerpVec3(&app->old_camera_position, &app->new_camera_position, (float)zest_TimerLerp(app->timer));

		zest_vec3 position = { 0.f, 0.f, 0.f };
		app->ellapsed_time += elapsed;
		float rotation_time = app->ellapsed_time * .000001f;
		zest_vec3 rotation = { sinf(rotation_time), cosf(rotation_time), -sinf(rotation_time) };
		zest_vec3 scale = { 1.f, 1.f, 1.f };

		UpdateLights(app, rotation_time);
		app->material_push.camera = zest_Vec4Set(app->camera.position.x, app->camera.position.y, app->camera.position.z, 0.f);
		app->material_push.irradiance_index = zest_ImageDescriptorIndex(app->irr_texture, zest_texture_cube_binding);
		app->material_push.brd_lookup_index = zest_ImageDescriptorIndex(app->brd_texture, zest_texture_2d_binding);
		app->material_push.pre_filtered_index = zest_ImageDescriptorIndex(app->prefiltered_texture, zest_texture_cube_binding);
		app->material_push.sampler_index = app->sampler_2d_index;
		app->material_push.skybox_sampler_index = app->skybox_sampler_index;
		zest_SetInstanceDrawing(app->cube_layer, app->pbr_shader_resources, app->pbr_pipeline);
		zest_SetLayerPushConstants(app->cube_layer, &app->material_push, sizeof(zest_push_constants_t));
		zest_SetLayerColor(app->cube_layer, 255, 255, 255, 255);
		zest_DrawInstancedMesh(app->cube_layer, &position.x, &rotation.x, &scale.x);

		float zero[3] = { 0 };
		zest_SetInstanceDrawing(app->skybox_layer, app->skybox_shader_resources, app->skybox_pipeline);
		zest_SetLayerColor(app->skybox_layer, 255, 255, 255, 255);
		zest_DrawInstancedMesh(app->skybox_layer, zero, zero, zero);

		if (app->reset) {
			app->reset = false;
			ImGui_ImplGlfw_Shutdown();
			zest_imgui_Destroy(&app->imgui);
			zest_implglfw_DestroyWindow(app->context);
			zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");
			zest_ResetRenderer(app->context, &window_handles);
			InitSimplePBRExample(app);
		}

		zest_swapchain swapchain = zest_GetSwapchain(app->context);
		zest_SetSwapchainClearColor(app->context, 0, 0.1f, 0.2f, 1.f);
		app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw();
		app->cache_info.brd_layout = zest_ImageRawLayout(app->brd_texture);
		app->cache_info.irradiance_layout = zest_ImageRawLayout(app->irr_texture);
		app->cache_info.prefiltered_layout = zest_ImageRawLayout(app->prefiltered_texture);
		zest_frame_graph_cache_key_t cache_key = {};
		cache_key = zest_InitialiseCacheKey(app->context, &app->cache_info, sizeof(RenderCacheInfo));

		zest_image_resource_info_t depth_info = {
			zest_format_depth,
			zest_resource_usage_hint_none,
			zest_ScreenWidth(app->context),
			zest_ScreenHeight(app->context),
			1, 1
		};

		if (zest_BeginFrame(app->context)) {
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);
			//Begin the render graph with the command that acquires a swap chain image (zest_BeginFrameGraphSwapchain)
			//Use the render graph we created earlier. Will return false if a swap chain image could not be acquired. This will happen
			//if the window is resized for example.
			if (!frame_graph) {
				if (zest_BeginFrameGraph(app->context, "ImGui", &cache_key)) {
					zest_resource_node cube_layer_resource = zest_AddTransientLayerResource("PBR Layer", app->cube_layer, false);
					zest_resource_node billboard_layer_resource = zest_AddTransientLayerResource("Billboard Layer", app->billboard_layer, false);
					zest_resource_node skybox_layer_resource = zest_AddTransientLayerResource("Sky Box Layer", app->skybox_layer, false);
					zest_resource_node skybox_texture_resource = zest_ImportImageResource("Sky Box Texture", app->skybox_texture, 0);
					zest_resource_node brd_texture_resource = zest_ImportImageResource("BRD lookup texture", app->brd_texture, 0);
					zest_resource_node irradiance_texture_resource = zest_ImportImageResource("Irradiance texture", app->irr_texture, 0);
					zest_resource_node prefiltered_texture_resource = zest_ImportImageResource("Prefiltered texture", app->prefiltered_texture, 0);
					zest_resource_node depth_buffer = zest_AddTransientImageResource("Depth Buffer", &depth_info);
					zest_resource_node swapchain_node = zest_ImportSwapchainResource();
					zest_output_group group = zest_CreateOutputGroup();
					zest_AddSwapchainToRenderTargetGroup(group);
					zest_AddImageToRenderTargetGroup(group, depth_buffer);

					//-------------------------Transfer Pass----------------------------------------------------
					zest_BeginTransferPass("Upload Mesh Data"); {
						zest_ConnectOutput(cube_layer_resource);
						zest_ConnectOutput(billboard_layer_resource);
						zest_ConnectOutput(skybox_layer_resource);
						zest_SetPassTask(UploadMeshData, app);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//------------------------ Skybox Layer Pass ------------------------------------------------------------
					zest_BeginRenderPass("Skybox Pass"); {
						zest_ConnectInput(skybox_texture_resource);
						zest_ConnectInput(skybox_layer_resource);
						zest_ConnectGroupedOutput(group);
						zest_SetPassTask(zest_DrawInstanceMeshLayer, &app->skybox_layer);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//------------------------ PBR Layer Pass ------------------------------------------------------------
					zest_BeginRenderPass("Cube Pass"); {
						zest_ConnectInput(cube_layer_resource);
						zest_ConnectInput(brd_texture_resource);
						zest_ConnectInput(irradiance_texture_resource);
						zest_ConnectInput(prefiltered_texture_resource);
						zest_ConnectGroupedOutput(group);
						zest_SetPassTask(zest_DrawInstanceMeshLayer, &app->cube_layer);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//------------------------ ImGui Pass ----------------------------------------------------------------
					//If there's imgui to draw then draw it
					zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui); {
						if (imgui_pass) {
							zest_ConnectGroupedOutput(group);
						} else {
							//If there's no ImGui to render then just render a blank screen
							zest_pass_node blank_pass = zest_BeginGraphicBlankScreen("Draw Nothing");
							//Add the swap chain as an output to the imgui render pass. This is telling the render graph where it should render to.
							zest_ConnectGroupedOutput(group);
						}
						zest_EndPass();
					}
					//----------------------------------------------------------------------------------------------------
					//End the render graph and execute it. This will submit it to the GPU.
					frame_graph = zest_EndFrameGraph();
				}
			}

			zest_QueueFrameGraphForExecution(app->context, frame_graph);
			if (app->request_graph_print) {
				//You can print out the render graph for debugging purposes
				zest_PrintCompiledFrameGraph(frame_graph);
				app->request_graph_print = false;
			}

			zest_EndFrame(app->context);
		}

		if (zest_SwapchainWasRecreated(app->context)) {
			zest_SetLayerSizeToSwapchain(app->billboard_layer);
			zest_SetLayerSizeToSwapchain(app->cube_layer);
			zest_SetLayerSizeToSwapchain(app->skybox_layer);
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
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	ZEST__UNFLAG(create_info.flags, zest_init_flag_cache_shaders);

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
	zest_device device = zest_EndDeviceBuilder(device_builder);

	//Create a window using GLFW
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");
	//Initialise Zest
	imgui_app.context = zest_CreateContext(device, &window_handles, &create_info);

	//int *test = nullptr;
	//zest_vec_push(imgui_app.context->device->allocator, test, 10);

	//Initialise our example
	InitSimplePBRExample(&imgui_app);

	//Start the main loop
	MainLoop(&imgui_app);
	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Destroy(&imgui_app.imgui);
	zest_DestroyContext(imgui_app.context);

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
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
