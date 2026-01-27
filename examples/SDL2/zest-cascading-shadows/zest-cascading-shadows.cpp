#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_ALL_UTILITIES_IMPLEMENTATION
#include "zest-cascading-shadows.h"
#include "zest.h"
#include "imgui_internal.h"
#include "examples/Common/sdl_controls.cpp"
#include "examples/Common/sdl_events.cpp"

/*
Cascaded Shadow Mapping Example
Recreated from Sascha Willems "Cascaded shadow mapping for directional light sources"
https://github.com/SaschaWillems/Vulkan/tree/master/examples/shadowmappingcascade

This example demonstrates:
- Cascaded shadow maps for directional lights (splits the view frustum into multiple shadow maps)
- Multi-layer depth pass rendering using zest_SetPipelineViewCount
- Loading and rendering GLTF models with instanced mesh layers
- Complex frame graph with transfer, shadow, scene, and optional debug passes
- Transient image resources for depth and shadow buffers
- Resource groups for combining swapchain and depth buffer outputs
- Frame graph caching with custom cache info
- Free-look camera with WASD movement
*/

void InitCascadingShadowsExample(CascadingShadowsExample *app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise(app->context, &app->imgui, zest_implsdl2_DestroyWindow);
	ImGui_ImplSDL2_InitForVulkan((SDL_Window *)zest_Window(app->context));
	zest_imgui_DarkStyle(&app->imgui);

    /*
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
     */

	//Setup a free-look camera
	app->camera = zest_CreateCamera();
	float position[3] = { -0.12f, 1.14f, -2.25f };
	zest_CameraPosition(&app->camera, position);
	zest_CameraSetFoV(&app->camera, 45.f);
	zest_CameraSetYaw(&app->camera, 7.f);
	zest_CameraSetPitch(&app->camera, -17.f);
	zest_CameraUpdateFront(&app->camera);
	app->new_camera_position = app->camera.position;
	app->old_camera_position = app->camera.position;

	app->timer = zest_CreateTimer(60);

	//Create uniform buffers for shader data. Each buffer is automatically duplicated per frame-in-flight.
	//- vert_buffer: view/projection matrices and light direction
	//- cascade_buffer: light view-projection matrices for each cascade
	//- fragment_buffer: cascade split depths and inverse view matrix for shadow sampling
	app->vert_buffer = zest_CreateUniformBuffer(app->context, "View", sizeof(uniform_buffer_data_t));
	app->cascade_buffer = zest_CreateUniformBuffer(app->context, "Cascade", sizeof(uniform_cascade_data_t));
	app->fragment_buffer = zest_CreateUniformBuffer(app->context, "Fragment", sizeof(uniform_fragment_data_t));

	//Create samplers - one for textures (mirror repeat) and one for shadow map sampling (default/clamp)
	zest_sampler_info_t sampler_info = zest_CreateSamplerInfoMirrorRepeat();
	app->sampler_2d = zest_CreateSampler(app->context, &sampler_info);
	sampler_info = zest_CreateSamplerInfo();
	app->shadow_sampler_2d = zest_CreateSampler(app->context, &sampler_info);

	zest_sampler sampler_2d = zest_GetSampler(app->sampler_2d);
	zest_sampler shadow_sampler_2d = zest_GetSampler(app->shadow_sampler_2d);

	//Acquire descriptor indices for samplers to pass via push constants
	app->scene_push.sampler_index = zest_AcquireSamplerIndex(app->device, sampler_2d);
	app->scene_push.shadow_sampler_index = zest_AcquireSamplerIndex(app->device, shadow_sampler_2d);

	//Load shaders for scene rendering, depth pass, and debug visualization
	zest_shader_handle scene_vert = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-cascading-shadows/shaders/scene.vert", "scene_vert.spv", zest_vertex_shader, true);
	zest_shader_handle scene_frag = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-cascading-shadows/shaders/scene.frag", "scene_frag.spv", zest_fragment_shader, true);
	zest_shader_handle depthpass_vert = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-cascading-shadows/shaders/depthpass.vert", "depthpass_vert.spv", zest_vertex_shader, true);
	zest_shader_handle depthpass_frag = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-cascading-shadows/shaders/depthpass.frag", "depthpass_frag.spv", zest_fragment_shader, true);
	zest_shader_handle debug_vert = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-cascading-shadows/shaders/debugshadowmap.vert", "debugshadowmap_vert.spv", zest_vertex_shader, true);
	zest_shader_handle debug_frag = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-cascading-shadows/shaders/debugshadowmap.frag", "debugshadowmap_frag.spv", zest_fragment_shader, true);

	//Debug pipeline for visualizing shadow map cascades (uses fullscreen triangle, no vertex input)
	app->debug_pipeline = zest_CreatePipelineTemplate(app->device, "pipeline_mesh_instance");
	zest_SetPipelineDisableVertexInput(app->debug_pipeline);
	zest_SetPipelineShaders(app->debug_pipeline, debug_vert, debug_frag);
	zest_SetPipelineCullMode(app->debug_pipeline, zest_cull_mode_back);

	//Main scene pipeline with instanced mesh rendering.
	//Binding 0: per-vertex data (position, color, normal, uv)
	//Binding 1: per-instance data (position, color, rotation, scale)
	app->mesh_pipeline = zest_CreatePipelineTemplate(app->device, "pipeline_mesh_instance");
	zest_AddVertexInputBindingDescription(app->mesh_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexInputBindingDescription(app->mesh_pipeline, 1, sizeof(zest_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(app->mesh_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);
	zest_AddVertexAttribute(app->mesh_pipeline, 0, 1, zest_format_r8g8b8a8_unorm, offsetof(zest_vertex_t, color));
	zest_AddVertexAttribute(app->mesh_pipeline, 0, 2, zest_format_r32g32b32_sfloat, offsetof(zest_vertex_t, normal));
	zest_AddVertexAttribute(app->mesh_pipeline, 0, 3, zest_format_r32g32_sfloat, offsetof(zest_vertex_t, uv));
	zest_AddVertexAttribute(app->mesh_pipeline, 1, 4, zest_format_r32g32b32_sfloat, 0);
	zest_AddVertexAttribute(app->mesh_pipeline, 1, 5, zest_format_r8g8b8a8_unorm, offsetof(zest_instance_t, color));
	zest_AddVertexAttribute(app->mesh_pipeline, 1, 6, zest_format_r32g32b32_sfloat, offsetof(zest_instance_t, rotation));
	zest_AddVertexAttribute(app->mesh_pipeline, 1, 7, zest_format_r32g32b32_sfloat, offsetof(zest_instance_t, scale));
	zest_SetPipelineShaders(app->mesh_pipeline, scene_vert, scene_frag);
	zest_SetPipelineCullMode(app->mesh_pipeline, zest_cull_mode_back);
	zest_SetPipelineFrontFace(app->mesh_pipeline, zest_front_face_counter_clockwise);
	zest_SetPipelineTopology(app->mesh_pipeline, zest_topology_triangle_list);
	zest_SetPipelineDepthTest(app->mesh_pipeline, true, true);

	//Shadow pass pipeline - renders depth only to a multi-layer image (one layer per cascade).
	//Uses zest_SetPipelineViewCount to enable multi-view rendering where each view renders to
	//a different cascade layer. The vertex shader uses gl_ViewIndex to select the cascade matrix.
	app->shadow_pipeline = zest_CreatePipelineTemplate(app->device, "Shadow pipeline");
	zest_AddVertexInputBindingDescription(app->shadow_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexInputBindingDescription(app->shadow_pipeline, 1, sizeof(zest_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(app->shadow_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);
	zest_AddVertexAttribute(app->shadow_pipeline, 0, 1, zest_format_r32g32_sfloat, offsetof(zest_vertex_t, uv));
	zest_AddVertexAttribute(app->shadow_pipeline, 1, 2, zest_format_r32g32b32_sfloat, 0);
	zest_AddVertexAttribute(app->shadow_pipeline, 1, 3, zest_format_r32g32b32_sfloat, offsetof(zest_instance_t, rotation));
	zest_AddVertexAttribute(app->shadow_pipeline, 1, 4, zest_format_r32g32b32_sfloat, offsetof(zest_instance_t, scale));
	zest_SetPipelineFrontFace(app->shadow_pipeline, zest_front_face_counter_clockwise);
	zest_SetPipelineCullMode(app->shadow_pipeline, zest_cull_mode_back);
	zest_SetPipelineShaders(app->shadow_pipeline, depthpass_vert, depthpass_frag);
	zest_SetPipelineDepthTest(app->shadow_pipeline, true, true);
	zest_SetPipelineViewCount(app->shadow_pipeline, SHADOW_MAP_CASCADE_COUNT);

	//Load GLTF models (tree and terrain)
	zest_gltf_t models[] = {
		LoadGLTF(app->context, "examples/assets/gltf/oaktree.gltf"),
		LoadGLTF(app->context, "examples/assets/gltf/terrain_gridlines.gltf")
	};

	//Calculate total vertex/index capacity needed for the mesh layer
	zest_size tree_vertex_capacity, tree_index_capacity;
	zest_size terrain_vertex_capacity, terrain_index_capacity;
	zest_MeshDataSizes(models[0].meshes, models[0].mesh_count, &tree_vertex_capacity, &tree_index_capacity);
	zest_MeshDataSizes(models[1].meshes, models[1].mesh_count, &terrain_vertex_capacity, &terrain_index_capacity);

	//Create an instanced mesh layer that can hold all meshes. This manages vertex/index buffers
	//and instance data for efficient batched rendering.
	app->mesh_layer = zest_CreateInstanceMeshLayer(app->context, "Mesh Layer", sizeof(zest_instance_t), tree_vertex_capacity + terrain_vertex_capacity, tree_index_capacity + terrain_index_capacity);

	//Add meshes from each model to the layer, storing the texture descriptor index for each
	for (int i = 0; i != models[0].mesh_count; i++) {
		zest_mesh mesh = models[0].meshes[i];
		zest_image image = zest_GetImage(models[0].materials[mesh->material].image);
		zest_uint texture_index = zest_ImageDescriptorIndex(image, zest_texture_2d_binding);
		zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), models[0].meshes[i], texture_index);
	}
	for (int i = 0; i != models[1].mesh_count; i++) {
		zest_mesh mesh = models[1].meshes[i];
		zest_image image = zest_GetImage(models[1].materials[mesh->material].image);
		zest_uint texture_index = zest_ImageDescriptorIndex(image, zest_texture_2d_binding);
		zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), models[1].meshes[i], texture_index);
	}

	app->light_fov = 45;
	app->light_position = {30.f, 50.f, 25.f};
	app->scene_push.enable_pcf = 1;

	//Free CPU-side model data now that it's uploaded to GPU
	zest_FreeGLTF(&models[0]);
	zest_FreeGLTF(&models[1]);

	//Cascade shadow map parameters
	app->cascade_split_lambda = 0.95f;  //Blend between logarithmic and uniform split (0=uniform, 1=logarithmic)
	app->z_near = 1.f;
	app->z_far = 48.f;
	app->color_cascades = false;
}

/*
Calculate frustum split depths and matrices for the shadow map cascades.
Based on https://johanmedestrom.wordpress.com/2016/03/18/opengl-cascaded-shadow-maps/

Cascaded shadow mapping works by splitting the view frustum into multiple sections (cascades),
each with its own shadow map. Near cascades cover less world space but with higher resolution,
while far cascades cover more space with lower effective resolution. This provides high-quality
shadows near the camera where detail matters most.
*/
void UpdateCascades(CascadingShadowsExample *app)
{
	float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

	float nearClip = app->z_near;
	float farClip = app->z_far;
	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	//Calculate split depths using a blend of logarithmic and uniform distribution.
	//Logarithmic gives better near-field detail, uniform gives more even distribution.
	//The cascade_split_lambda parameter controls the blend (0=uniform, 1=logarithmic).
	//Based on GPU Gems 3: https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	for (zest_uint i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float p = (i + 1) / (float)SHADOW_MAP_CASCADE_COUNT;
		float log = minZ * powf(ratio, p);
		float uniform = minZ + range * p;
		float d = app->cascade_split_lambda * (log - uniform) + uniform;
		cascadeSplits[i] = (d - nearClip) / clipRange;
	}

	// Calculate orthographic projection matrix for each cascade
	float lastSplitDist = 0.0;

	// Compute view and projection matrices directly from camera (not from uniform buffer)
	zest_matrix4 view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	zest_matrix4 proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf(app->context) / zest_ScreenHeightf(app->context), app->z_near, app->z_far);
	proj.v[1].y *= -1.f;

	// Project frustum corners into world space
	zest_matrix4 persp_view = zest_MatrixTransform(&proj, &view);
	zest_matrix4 invCam = zest_Inverse(&persp_view);

	for (zest_uint i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float splitDist = cascadeSplits[i];

		zest_vec3 frustumCorners[8] = {
			{ -1.0f,  1.0f, 0.0f},
			{ 1.0f,  1.0f, 0.0f},
			{ 1.0f, -1.0f, 0.0f},
			{ -1.0f, -1.0f, 0.0f},
			{ -1.0f,  1.0f,  1.0f},
			{ 1.0f,  1.0f,  1.0f},
			{ 1.0f, -1.0f,  1.0f},
			{ -1.0f, -1.0f,  1.0f},
		};

		for (zest_uint j = 0; j < 8; j++) {
			zest_vec4 frustum = { frustumCorners[j].x, frustumCorners[j].y, frustumCorners[j].z, 1.f };
			zest_vec4 invCorner = zest_MatrixTransformVector(&invCam, frustum);
			frustum = zest_ScaleVec4(invCorner, 1.f / invCorner.w);
			frustumCorners[j] = { frustum.x, frustum.y, frustum.z };
		}

		for (zest_uint j = 0; j < 4; j++) {
			zest_vec3 dist = zest_SubVec3(frustumCorners[j + 4], frustumCorners[j]);
			frustumCorners[j + 4] = zest_AddVec3(frustumCorners[j], zest_ScaleVec3(dist, splitDist));
			frustumCorners[j] = zest_AddVec3(frustumCorners[j], zest_ScaleVec3(dist, lastSplitDist));
		}

		// Get frustum center
		zest_vec3 frustumCenter = { 0 };
		for (zest_uint j = 0; j < 8; j++) {
			frustumCenter = zest_AddVec3(frustumCenter, frustumCorners[j]);
		}
		frustumCenter = zest_ScaleVec3(frustumCenter, 1.f / 8.0f);

		float radius = 0.0f;
		for (zest_uint j = 0; j < 8; j++) {
			float distance = zest_LengthVec3(zest_SubVec3(frustumCorners[j], frustumCenter));
			radius = ZEST__MAX(radius, distance);
		}
		radius = ceilf(radius * 16.0f) / 16.0f;

		zest_vec3 maxExtents = zest_Vec3Set1(radius);
		zest_vec3 minExtents = { -maxExtents.x, -maxExtents.y, -maxExtents.z };

		zest_vec3 light_position = { -app->light_position.x, -app->light_position.y, -app->light_position.z };
		zest_vec3 lightDir = zest_NormalizeVec3(light_position);
		zest_matrix4 lightViewMatrix = zest_LookAt(zest_SubVec3(frustumCenter, zest_ScaleVec3(lightDir, -minExtents.z)), frustumCenter, { 0.0f, 1.0f, 0.0f });
		zest_matrix4 lightOrthoMatrix = zest_Ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);
		lightOrthoMatrix.v[1].y *= -1.f;  // Vulkan Y-flip for shadow map

		// Store split distance and matrix in cascade
		app->cascades[i].splitDepth = (app->z_near + splitDist * clipRange) * -1.0f;
		app->cascades[i].viewProjMatrix = zest_MatrixTransform(&lightOrthoMatrix, &lightViewMatrix);

		lastSplitDist = cascadeSplits[i];
	}
}

void UpdateLight(CascadingShadowsExample *app) {
	float angle = zest_Radians(app->frame_timer * 360.0f);
	float radius = 1.0f;
	app->light_position = { cosf(angle) * radius, 20.f, sinf(angle) * radius };
	app->light_position.x -= 20.f;
	app->light_position.z -= 20.f;
}

void UpdateUniform3d(CascadingShadowsExample *app) {
	zest_uniform_buffer vert_buffer = zest_GetUniformBuffer(app->vert_buffer);
	zest_uniform_buffer frag_buffer = zest_GetUniformBuffer(app->fragment_buffer);
	zest_uniform_buffer cascade_buffer = zest_GetUniformBuffer(app->cascade_buffer);
	/*
	Depth rendering
	*/
	uniform_cascade_data_t *cascade_ptr = (uniform_cascade_data_t *)zest_GetUniformBufferData(cascade_buffer);
	zest_matrix4 cascade_matrix[SHADOW_MAP_CASCADE_COUNT];
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		cascade_matrix[i] = app->cascades[i].viewProjMatrix;
		zest_matrix4 mat = cascade_matrix[i];
		//ZEST_PRINT("%u: [0] - %.2f\t%.2f\t%.2f\t%.2f", i, mat.v[0].x, mat.v[0].y, mat.v[0].z, mat.v[0].w);
		//ZEST_PRINT("%u: [1] - %.2f\t%.2f\t%.2f\t%.2f", i, mat.v[1].x, mat.v[1].y, mat.v[1].z, mat.v[1].w);
		//ZEST_PRINT("%u: [2] - %.2f\t%.2f\t%.2f\t%.2f", i, mat.v[2].x, mat.v[2].y, mat.v[2].z, mat.v[2].w);
		//ZEST_PRINT("%u: [3] - %.2f\t%.2f\t%.2f\t%.2f", i, mat.v[3].x, mat.v[3].y, mat.v[3].z, mat.v[3].w);
	}
	memcpy(cascade_ptr, cascade_matrix, sizeof(zest_matrix4) * SHADOW_MAP_CASCADE_COUNT);

	/*
	Scene rendering
	*/
	uniform_buffer_data_t *vert_ptr = (uniform_buffer_data_t *)zest_GetUniformBufferData(vert_buffer);
	vert_ptr->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	vert_ptr->proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf(app->context) / zest_ScreenHeightf(app->context), 0.001f, 10000.f);
	vert_ptr->proj.v[1].y *= -1.f;
	vert_ptr->model = zest_M4(1.f);
	vert_ptr->light_direction = zest_NormalizeVec3( { -app->light_position.x, -app->light_position.y, -app->light_position.z });

	uniform_fragment_data_t *frag_ptr = (uniform_fragment_data_t *)zest_GetUniformBufferData(frag_buffer);
	for (zest_uint i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		frag_ptr->cascadeSplits[i] = app->cascades[i].splitDepth;
	}
	frag_ptr->inverseViewMat = zest_Inverse(&vert_ptr->view);
	frag_ptr->light_direction = vert_ptr->light_direction;
	frag_ptr->color_cascades = app->color_cascades;
}

void DrawInstancedMesh(zest_layer layer, float pos[3], float rot[3], float scale[3]) {
    zest_instance_t* instance = (zest_instance_t*)zest_NextInstance(layer);

    instance->pos = zest_Vec3Set(pos[0], pos[1], pos[2]);
    instance->rotation = zest_Vec3Set(rot[0], rot[1], rot[2]);
    instance->scale = zest_Vec3Set(scale[0], scale[1], scale[2]);
    instance->color = layer->current_color;
}

//Transfer pass callback - uploads instance data from CPU staging buffer to GPU
void UploadMeshData(const zest_command_list context, void *user_data) {
	CascadingShadowsExample *app = (CascadingShadowsExample *)user_data;
	zest_layer layer = zest_GetLayer(app->mesh_layer);
	zest_UploadLayerStagingData(layer, context);
}

//Debug pass callback - renders a fullscreen quad showing the shadow map cascades
void DrawDebugShadowMap(const zest_command_list command_list, void *user_data) {
	CascadingShadowsExample *app = (CascadingShadowsExample *)user_data;

	//Get the shadow buffer from the frame graph inputs and acquire its descriptor index
	zest_resource_node shadow_resource = zest_GetPassInputResource(command_list, "Shadow Buffer");
	zest_uint shadow_index = zest_GetTransientSampledImageBindlessIndex(command_list, shadow_resource, zest_texture_array_binding);

	zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);
	zest_pipeline pipeline = zest_GetPipeline(app->debug_pipeline, command_list);
	app->scene_push.shadow_index = shadow_index;
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_SendPushConstants(command_list, (void*)&app->scene_push, sizeof(scene_push_constants_t));
	//Draw fullscreen triangle (3 vertices, shader generates positions)
	zest_cmd_Draw(command_list, 3, 1, 0, 0);
}

//Shadow pass callback - renders scene depth to cascaded shadow map layers
void DrawSceneDepth(const zest_command_list command_list, void *user_data) {
	CascadingShadowsExample *app = (CascadingShadowsExample *)user_data;
	zest_layer mesh_layer = zest_GetLayer(app->mesh_layer);

	//Set viewport to shadow map size (all cascades use same resolution)
	zest_uint map_size = SHADOW_MAP_TEXTURE_SIZE;
	zest_SetLayerViewPort(mesh_layer, 0, 0, map_size, map_size, (float)map_size, (float)map_size);
	//Draw using shadow pipeline which has multi-view enabled for cascade layers
	zest_DrawInstanceMeshLayerWithPipeline(command_list, mesh_layer, app->shadow_pipeline);
}

//Main scene pass callback - renders the scene with shadow sampling
void DrawSceneWithShadows(const zest_command_list command_list, void *user_data) {
	CascadingShadowsExample *app = (CascadingShadowsExample *)user_data;
	zest_layer layer = zest_GetLayer(app->mesh_layer);

	//Get shadow map descriptor index from frame graph to pass to fragment shader
	zest_resource_node shadow_resource = zest_GetPassInputResource(command_list, "Shadow Buffer");
	zest_uint shadow_index = zest_GetTransientSampledImageBindlessIndex(command_list, shadow_resource, zest_texture_array_binding);

	zest_layer mesh_layer = zest_GetLayer(app->mesh_layer);
	zest_SetLayerSizeToSwapchain(mesh_layer);

	//Bind mesh vertex and index buffers
	zest_cmd_BindMeshVertexBuffer(command_list, layer);
	zest_cmd_BindMeshIndexBuffer(command_list, layer);

	//Bind instance buffer (per-instance transforms)
	zest_buffer device_buffer = zest_GetLayerResourceBuffer(layer);
	zest_cmd_BindVertexBuffer(command_list, 1, 1, device_buffer);

	zest_pipeline pipeline = zest_GetPipeline(app->mesh_pipeline, command_list);

	//Iterate through draw instructions 
	zest_uint instruction_count = zest_GetLayerInstructionCount(layer);
	for(int i = 0; i != instruction_count; i++) {
		const zest_layer_instruction_t *current = zest_GetLayerInstruction(layer, i);
		//Get the mesh offsets for this instruction
		const zest_mesh_offset_data_t *mesh_offsets = zest_GetLayerMeshOffsets(layer, current->mesh_index);

		zest_cmd_LayerViewport(command_list, layer);
		zest_cmd_BindPipeline(command_list, pipeline);

		//Update push constants with shadow map index for this draw
		scene_push_constants_t *push = (scene_push_constants_t*)current->push_constant;
		push->shadow_index = shadow_index;
		zest_cmd_SendPushConstants(command_list, (void*)push, sizeof(scene_push_constants_t));

		zest_cmd_DrawIndexed(command_list, mesh_offsets->index_count, current->total_instances, mesh_offsets->index_offset, mesh_offsets->vertex_offset, current->start_index);
	}
}

void UpdateImGui(CascadingShadowsExample *app) {
	//We can use a timer to only update the gui every 60 times a second (or whatever you decide) for a little less
	//cpu usage.
	zest_StartTimerLoop(app->timer) {
		//Must call the imgui GLFW implementation function
		ImGui_ImplSDL2_NewFrame();
		//Draw our imgui stuff
		ImGui::NewFrame();
		ImGui::Begin("Test Window");
		ImGui::Text("FPS: %u", app->fps);
		ImGui::Checkbox("PCF Filtering", &app->scene_push.enable_pcf);
		ImGui::Checkbox("Color Cascades", &app->color_cascades);
		ImGui::SliderFloat("Split Lambda", &app->cascade_split_lambda, 0.1f, 1.f, "%.2f");
		ImGui::Checkbox("Display Shadow Map", &app->debug_draw);
		if (app->debug_draw) {
			ImGui::SliderInt("Cascade Index", &app->scene_push.display_cascade_index, 0, SHADOW_MAP_CASCADE_COUNT - 1);
		}
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

void MainLoop(CascadingShadowsExample *app) {
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
			float frame_timer = (float)elapsed / ZEST_MICROSECS_SECOND;
			app->frame_timer += 0.05f * frame_timer;
			if (app->frame_timer > 1.0)
			{
				app->frame_timer -= 1.0f;
			}

			UpdateLight(app);

			UpdateCascades(app);

			UpdateUniform3d(app);

			UpdateImGui(app);

			app->camera.position = zest_LerpVec3(&app->old_camera_position, &app->new_camera_position, (float)zest_TimerLerp(&app->timer));

			zest_layer mesh_layer = zest_GetLayer(app->mesh_layer);
			zest_uniform_buffer vert_buffer = zest_GetUniformBuffer(app->vert_buffer);
			zest_uniform_buffer frag_buffer = zest_GetUniformBuffer(app->fragment_buffer);
			zest_uniform_buffer cascade_buffer = zest_GetUniformBuffer(app->cascade_buffer);
			zest_SetLayerColor(mesh_layer, 255, 255, 255, 255);
			app->scene_push.vert_index = zest_GetUniformBufferDescriptorIndex(vert_buffer);
			app->scene_push.frag_index = zest_GetUniformBufferDescriptorIndex(frag_buffer);
			app->scene_push.cascade_index = zest_GetUniformBufferDescriptorIndex(cascade_buffer);
			zest_vec3 scale = { 1.f, 1.f, 1.f };
			float zero[3] = { 0.f, 0.f, 0.f };
			zest_vec3 positions[] = {
				{ 0.0f, 0.0f, 0.0f },
				{ 1.25f, -0.25f, 1.25f },
				{ -1.25f, -0.2f, 1.25f },
				{ 1.25f, -0.25f, -1.25f },
				{ -1.25f, -0.25f, -1.25f }
			};

			zest_StartInstanceMeshDrawing(mesh_layer, 0, app->shadow_pipeline);
			app->scene_push.texture_index = zest_GetLayerMeshTextureIndex(mesh_layer, 0);
			zest_SetLayerPushConstants(mesh_layer, &app->scene_push, sizeof(scene_push_constants_t));
			for (int i = 0; i != 5; i++) {
				DrawInstancedMesh(mesh_layer, &positions[i].x, zero, &scale.x);
			}

			zest_StartInstanceMeshDrawing(mesh_layer, 1, app->shadow_pipeline);
			app->scene_push.texture_index = zest_GetLayerMeshTextureIndex(mesh_layer, 1);
			zest_SetLayerPushConstants(mesh_layer, &app->scene_push, sizeof(scene_push_constants_t));
			for (int i = 0; i != 5; i++) {
				DrawInstancedMesh(mesh_layer, &positions[i].x, zero, &scale.x);
			}

			zest_StartInstanceMeshDrawing(mesh_layer, 2, app->shadow_pipeline);
			app->scene_push.texture_index = zest_GetLayerMeshTextureIndex(mesh_layer, 2);
			zest_SetLayerPushConstants(mesh_layer, &app->scene_push, sizeof(scene_push_constants_t));
			DrawInstancedMesh(mesh_layer, zero, zero, &scale.x);

			//Frame graph caching: the cache key includes custom data that affects graph structure.
			//When draw_imgui or draw_depth_image changes, a different frame graph variant is needed.
			app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw(&app->imgui);
			app->cache_info.draw_depth_image = app->debug_draw;
			zest_frame_graph_cache_key_t cache_key = {};
			cache_key = zest_InitialiseCacheKey(app->context, &app->cache_info, sizeof(RenderCacheInfo));

			//Try to get a cached frame graph. If found, skip rebuilding.
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);

			if (!frame_graph) {
				//No cached graph - build a new one

				//Define transient image resources (created/destroyed per-frame by the frame graph)
				zest_image_resource_info_t depth_info = {
					zest_format_depth,
					zest_resource_usage_hint_none,
					zest_ScreenWidth(app->context), zest_ScreenHeight(app->context),
					1, 1
				};

				//Shadow map: multi-layer depth image (one layer per cascade)
				zest_image_resource_info_t shadow_info = {
					zest_format_d32_sfloat_s8_uint,
					zest_resource_usage_hint_none,
					SHADOW_MAP_TEXTURE_SIZE, SHADOW_MAP_TEXTURE_SIZE,
					1,
					SHADOW_MAP_CASCADE_COUNT  //Array layers for cascades
				};

				zest_uniform_buffer vert_buffer = zest_GetUniformBuffer(app->vert_buffer);
				if (zest_BeginFrameGraph(app->context, "ImGui", &cache_key)) {
					//Import/create resources used by the frame graph
					zest_resource_node mesh_layer_resource = zest_AddTransientLayerResource("Mesh Layer", mesh_layer, false);
					zest_resource_node swapchain_node = zest_ImportSwapchainResource();
					zest_resource_node depth_buffer = zest_AddTransientImageResource("Depth Buffer", &depth_info);
					zest_resource_node shadow_buffer = zest_AddTransientImageResource("Shadow Buffer", &shadow_info);

					//Create a resource group to combine swapchain and depth buffer as outputs.
					//This allows a single pass to write to both color and depth.
					zest_resource_group group = zest_CreateResourceGroup();
					zest_AddSwapchainToGroup(group);
					zest_AddResourceToGroup(group, depth_buffer);

					//-------------------------Transfer Pass------------------------------------------------
					//Upload instance data to GPU before rendering
					zest_BeginTransferPass("Upload Mesh Data"); {
						zest_ConnectOutput(mesh_layer_resource);
						zest_SetPassTask(UploadMeshData, app);
						zest_EndPass();
					}

					//-------------------------Shadow Pass--------------------------------------------------
					//Render scene depth to cascaded shadow map (multi-layer depth texture)
					zest_BeginRenderPass("Shadow Pass"); {
						zest_ConnectInput(mesh_layer_resource);  //Depends on mesh upload completing
						zest_ConnectOutput(shadow_buffer);
						zest_SetPassTask(DrawSceneDepth, app);
						zest_EndPass();
					}

					//-------------------------Scene or Debug Pass------------------------------------------
					if (!app->debug_draw) {
						//Normal scene rendering with shadow sampling
						zest_BeginRenderPass("Mesh Layer Pass"); {
							zest_ConnectInput(mesh_layer_resource);
							zest_ConnectInput(shadow_buffer);  //Sample shadow map in fragment shader
							zest_ConnectOutputGroup(group);    //Render to swapchain + depth
							zest_SetPassTask(DrawSceneWithShadows, app);
							zest_EndPass();
						}
					} else {
						//Debug mode: visualize shadow map cascades
						zest_BeginRenderPass("Debug Pass"); {
							zest_ConnectInput(shadow_buffer);
							zest_ConnectOutputGroup(group);
							zest_SetPassTask(DrawDebugShadowMap, app);
							zest_EndPass();
						}
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

					//Compile and cache the frame graph
					frame_graph = zest_EndFrameGraph();
				}
			}

			zest_EndFrame(app->context, frame_graph);
		}

		if (zest_SwapchainWasRecreated(app->context)) {
		}
	}
}

int main(int argc, char *argv[]) {

	CascadingShadowsExample imgui_app = {};

	//Create a window using SDL2. We must do this before setting up the device as it's needed to get
	//the extensions info.
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");

	//Create a device using a helper function for SDL2.
	imgui_app.device = zest_implsdl2_CreateVulkanDevice(&window_data, false);

	zest_create_context_info_t create_info = zest_CreateContextInfo();
	imgui_app.context = zest_CreateContext(imgui_app.device, &window_data, &create_info);

	InitCascadingShadowsExample(&imgui_app);
	MainLoop(&imgui_app);

	//Cleanup
	ImGui_ImplSDL2_Shutdown();
	zest_imgui_Destroy(&imgui_app.imgui);
	zest_DestroyDevice(imgui_app.device);

	return 0;
}
