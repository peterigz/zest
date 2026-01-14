#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_ALL_UTILITIES_IMPLEMENTATION
#include "zest-cascading-shadows.h"
#include "zest.h"
#include "imgui_internal.h"

/*
Example recreated from Sascha Willems "Cascaded shadow mapping for directional light sources" 
https://github.com/SaschaWillems/Vulkan/tree/master/examples/shadowmappingcascade
*/

void InitCascadingShadowsExample(CascadingShadowsExample *app) {
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
	zest_CameraPosition(&app->camera, {-0.12f, 1.14f, -2.25f});
	zest_CameraSetFoV(&app->camera, 45.f);
	zest_CameraSetYaw(&app->camera, 7.f);
	zest_CameraSetPitch(&app->camera, -17.f);
	zest_CameraUpdateFront(&app->camera);
	app->new_camera_position = app->camera.position;
	app->old_camera_position = app->camera.position;

	//We can use a timer to only update imgui 60 times per second
	app->timer = zest_CreateTimer(60);

	app->vert_buffer = zest_CreateUniformBuffer(app->context, "View", sizeof(uniform_buffer_data_t));
	app->cascade_buffer = zest_CreateUniformBuffer(app->context, "Cascade", sizeof(uniform_cascade_data_t));
	app->fragment_buffer = zest_CreateUniformBuffer(app->context, "Fragment", sizeof(uniform_fragment_data_t));

	//Compile the shaders we will use to render the particles
	zest_sampler_info_t sampler_info = zest_CreateSamplerInfoMirrorRepeat();
	app->sampler_2d = zest_CreateSampler(app->context, &sampler_info);
	sampler_info = zest_CreateSamplerInfo();
	app->shadow_sampler_2d = zest_CreateSampler(app->context, &sampler_info);

	zest_sampler sampler_2d = zest_GetSampler(app->sampler_2d);
	zest_sampler shadow_sampler_2d = zest_GetSampler(app->shadow_sampler_2d);

	app->scene_push.sampler_index = zest_AcquireSamplerIndex(app->device, sampler_2d);
	app->scene_push.shadow_sampler_index = zest_AcquireSamplerIndex(app->device, shadow_sampler_2d);

	zest_shader_handle scene_vert = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-cascading-shadows/shaders/scene.vert", "scene_vert.spv", zest_vertex_shader, true);
	zest_shader_handle scene_frag = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-cascading-shadows/shaders/scene.frag", "scene_frag.spv", zest_fragment_shader, true);
	zest_shader_handle depthpass_vert = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-cascading-shadows/shaders/depthpass.vert", "depthpass_vert.spv", zest_vertex_shader, true);
	zest_shader_handle depthpass_frag = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-cascading-shadows/shaders/depthpass.frag", "depthpass_frag.spv", zest_fragment_shader, true);
	zest_shader_handle debug_vert = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-cascading-shadows/shaders/debugshadowmap.vert", "debugshadowmap_vert.spv", zest_vertex_shader, true);
	zest_shader_handle debug_frag = zest_CreateShaderFromFile(app->device, "examples/GLFW/zest-cascading-shadows/shaders/debugshadowmap.frag", "debugshadowmap_frag.spv", zest_fragment_shader, true);

	app->debug_pipeline = zest_BeginPipelineTemplate(app->device, "pipeline_mesh_instance");
	zest_SetPipelineDisableVertexInput(app->debug_pipeline);
	zest_SetPipelineShaders(app->debug_pipeline, debug_vert, debug_frag);
	zest_SetPipelineCullMode(app->debug_pipeline, zest_cull_mode_back);

	app->mesh_pipeline = zest_BeginPipelineTemplate(app->device, "pipeline_mesh_instance");
	zest_AddVertexInputBindingDescription(app->mesh_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexInputBindingDescription(app->mesh_pipeline, 1, sizeof(zest_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(app->mesh_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);                                          // Location 0: Vertex Position
	zest_AddVertexAttribute(app->mesh_pipeline, 0, 1, zest_format_r8g8b8a8_unorm, offsetof(zest_vertex_t, color));               // Location 1: Vertex Color
	zest_AddVertexAttribute(app->mesh_pipeline, 0, 2, zest_format_r32g32b32_sfloat, offsetof(zest_vertex_t, normal));            // Location 2: Vertex Position
	zest_AddVertexAttribute(app->mesh_pipeline, 0, 3, zest_format_r32g32_sfloat, offsetof(zest_vertex_t, uv));                     // Location 3: Group id
	zest_AddVertexAttribute(app->mesh_pipeline, 1, 4, zest_format_r32g32b32_sfloat, 0);                                          // Location 4: Instance Position
	zest_AddVertexAttribute(app->mesh_pipeline, 1, 5, zest_format_r8g8b8a8_unorm, offsetof(zest_instance_t, color));        // Location 5: Instance Color
	zest_AddVertexAttribute(app->mesh_pipeline, 1, 6, zest_format_r32g32b32_sfloat, offsetof(zest_instance_t, rotation));   // Location 6: Instance Rotation
	zest_AddVertexAttribute(app->mesh_pipeline, 1, 7, zest_format_r32g32b32_sfloat, offsetof(zest_instance_t, scale));      // Location 7: Instance Scale

	zest_SetPipelineShaders(app->mesh_pipeline, scene_vert, scene_frag);
	zest_SetPipelineCullMode(app->mesh_pipeline, zest_cull_mode_back);
	zest_SetPipelineFrontFace(app->mesh_pipeline, zest_front_face_counter_clockwise);
	zest_SetPipelineTopology(app->mesh_pipeline, zest_topology_triangle_list);
	zest_SetPipelineDepthTest(app->mesh_pipeline, true, true);
	
	app->shadow_pipeline = zest_BeginPipelineTemplate(app->device, "Shadow pipeline");
	zest_AddVertexInputBindingDescription(app->shadow_pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
	zest_AddVertexInputBindingDescription(app->shadow_pipeline, 1, sizeof(zest_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(app->shadow_pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);                                          // Location 0: Vertex Position
	zest_AddVertexAttribute(app->shadow_pipeline, 0, 1, zest_format_r32g32_sfloat, offsetof(zest_vertex_t, uv));                   // Location 1: UV Position
	zest_AddVertexAttribute(app->shadow_pipeline, 1, 2, zest_format_r32g32b32_sfloat, 0);                                          // Location 2: Instance Position
	zest_AddVertexAttribute(app->shadow_pipeline, 1, 3, zest_format_r32g32b32_sfloat, offsetof(zest_instance_t, rotation));        // Location 3: Instance Rotation
	zest_AddVertexAttribute(app->shadow_pipeline, 1, 4, zest_format_r32g32b32_sfloat, offsetof(zest_instance_t, scale));      	   // Location 4: Instance Scale
	zest_SetPipelineFrontFace(app->shadow_pipeline, zest_front_face_counter_clockwise);
	zest_SetPipelineCullMode(app->shadow_pipeline, zest_cull_mode_back);
	zest_SetPipelineShaders(app->shadow_pipeline, depthpass_vert, depthpass_frag);
	zest_SetPipelineDepthTest(app->shadow_pipeline, true, true);
	zest_SetPipelineViewCount(app->shadow_pipeline, SHADOW_MAP_CASCADE_COUNT);

	zest_material_t tree_mat;
	zest_material_t grid_mat;
	zest_gltf_t models[] = {
		LoadGLTF(app->context, "examples/assets/gltf/oaktree.gltf"),
		LoadGLTF(app->context, "examples/assets/gltf/terrain_gridlines.gltf")
	};

	zest_size tree_vertex_capacity, tree_index_capacity;
	zest_size terrain_vertex_capacity, terrain_index_capacity;
	zest_MeshDataSizes(models[0].meshes, models[0].mesh_count, &tree_vertex_capacity, &tree_index_capacity);
	zest_MeshDataSizes(models[1].meshes, models[1].mesh_count, &terrain_vertex_capacity, &terrain_index_capacity);

	app->mesh_layer = zest_CreateInstanceMeshLayer(app->context, "Mesh Layer", sizeof(zest_instance_t), tree_vertex_capacity + terrain_vertex_capacity, tree_index_capacity + terrain_index_capacity);
	for (int i = 0; i != models[0].mesh_count; i++) {
		zest_mesh mesh = models[0].meshes[i];
		zest_image image = zest_GetImage(models[0].materials[mesh->material].image);
		zest_uint texture_index = zest_ImageDescriptorIndex(image, zest_texture_array_binding);
		zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), models[0].meshes[i], texture_index);
	}
	for (int i = 0; i != models[1].mesh_count; i++) {
		zest_mesh mesh = models[1].meshes[i];
		zest_image image = zest_GetImage(models[1].materials[mesh->material].image);
		zest_uint texture_index = zest_ImageDescriptorIndex(image, zest_texture_array_binding);
		zest_AddMeshToLayer(zest_GetLayer(app->mesh_layer), models[1].meshes[i], texture_index);
	}
	app->light_fov = 45;
	app->light_position = {30.f, 50.f, 25.f};
	app->scene_push.enable_pcf = 1;

	zest_FreeGLTF(&models[0]);
	zest_FreeGLTF(&models[1]);

	app->cascade_split_lambda = 0.95f;
	app->z_near = 1.f;
	app->z_far = 48.f;
	app->color_cascades = false;
}

/*
Calculate frustum split depths and matrices for the shadow map cascades
Based on https://johanmedestrom.wordpress.com/2016/03/18/opengl-cascaded-shadow-maps/
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

	// Calculate split depths based on view camera frustum
	// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	for (zest_uint i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float p = (i + 1) / (float)SHADOW_MAP_CASCADE_COUNT;
		float log = minZ * powf(ratio, p);
		float uniform = minZ + range * p;
		float d = app->cascade_split_lambda * (log - uniform) + uniform;
		cascadeSplits[i] = (d - nearClip) / clipRange;
	}

	// Calculate orthographic projection matrix for each cascade
	float lastSplitDist = 0.0;
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

		// Compute view and projection matrices directly from camera (not from uniform buffer)
		zest_matrix4 view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
		zest_matrix4 proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf(app->context) / zest_ScreenHeightf(app->context), app->z_near, app->z_far);
		proj.v[1].y *= -1.f;

		// Project frustum corners into world space
		zest_matrix4 persp_view = zest_MatrixTransform(&proj, &view);
		zest_matrix4 invCam = zest_Inverse(&persp_view);
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
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_mesh_instance, "Make sure you call zest_SetInstanceMeshDrawing before calling this function.");

    zest_instance_t* instance = (zest_instance_t*)zest_NextInstance(layer);

    instance->pos = zest_Vec3Set(pos[0], pos[1], pos[2]);
    instance->rotation = zest_Vec3Set(rot[0], rot[1], rot[2]);
    instance->scale = zest_Vec3Set(scale[0], scale[1], scale[2]);
    instance->color = layer->current_color;
}

void UploadMeshData(const zest_command_list context, void *user_data) {
	CascadingShadowsExample *app = (CascadingShadowsExample *)user_data;

	zest_layer layer = zest_GetLayer(app->mesh_layer);
	zest_UploadLayerStagingData(layer, context);
}

void DrawDebugShadowMap(const zest_command_list command_list, void *user_data) {
	CascadingShadowsExample *app = (CascadingShadowsExample *)user_data;
	zest_resource_node shadow_resource = zest_GetPassInputResource(command_list, "Shadow Buffer"); 
	zest_uint shadow_index = zest_GetTransientSampledImageBindlessIndex(command_list, shadow_resource, zest_texture_array_binding);

	zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);
	zest_pipeline pipeline = zest_PipelineWithTemplate(app->debug_pipeline, command_list);
	app->scene_push.shadow_index = shadow_index;
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_SendPushConstants(command_list, (void*)&app->scene_push, sizeof(scene_push_constants_t));
	zest_cmd_Draw(command_list, 3, 1, 0, 0);
}

void DrawSceneDepth(const zest_command_list command_list, void *user_data) {
	CascadingShadowsExample *app = (CascadingShadowsExample *)user_data;
	zest_layer mesh_layer = zest_GetLayer(app->mesh_layer);

	zest_uint map_size = SHADOW_MAP_TEXTURE_SIZE;
	zest_SetLayerViewPort(mesh_layer, 0, 0, map_size, map_size, map_size, map_size);
	zest_DrawInstanceMeshLayerWithPipeline(command_list, mesh_layer, app->shadow_pipeline);
}

void DrawSceneWithShadows(const zest_command_list command_list, void *user_data) {
	CascadingShadowsExample *app = (CascadingShadowsExample *)user_data;

	zest_layer layer = zest_GetLayer(app->mesh_layer);

	zest_resource_node shadow_resource = zest_GetPassInputResource(command_list, "Shadow Buffer"); 
	zest_uint shadow_index = zest_GetTransientSampledImageBindlessIndex(command_list, shadow_resource, zest_texture_array_binding);
	zest_layer mesh_layer = zest_GetLayer(app->mesh_layer);
	zest_SetLayerSizeToSwapchain(mesh_layer);

	zest_cmd_BindMeshVertexBuffer(command_list, layer);
	zest_cmd_BindMeshIndexBuffer(command_list, layer);

	zest_buffer device_buffer = zest_GetLayerResourceBuffer(layer);
	zest_cmd_BindVertexBuffer(command_list, 1, 1, device_buffer);

	zest_pipeline pipeline = zest_PipelineWithTemplate(app->mesh_pipeline, command_list);

	zest_uint instruction_count = zest_GetLayerInstructionCount(layer);
	for(int i = 0; i != instruction_count; i++) {
        const zest_layer_instruction_t *current = zest_GetLayerInstruction(layer, i);
		const zest_mesh_offset_data_t *mesh_offsets = zest_GetLayerMeshOffsets(layer, current->mesh_index);

		zest_cmd_LayerViewport(command_list, layer);

		zest_cmd_BindPipeline(command_list, pipeline);

		scene_push_constants_t *push = (scene_push_constants_t*)current->push_constant;
		push->shadow_index = shadow_index;
		zest_cmd_SendPushConstants(command_list, (void*)push, sizeof(scene_push_constants_t));

		zest_cmd_DrawIndexed(command_list, mesh_offsets->index_count, current->total_instances, mesh_offsets->index_offset, mesh_offsets->vertex_offset, current->start_index);
    }

}

void UpdateCameraPosition(CascadingShadowsExample *app) {
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

void UpdateMouse(CascadingShadowsExample *app) {
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

void UpdateImGui(CascadingShadowsExample *app) {
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
		UpdateCameraPosition(app);
	} zest_EndTimerLoop(app->timer);
}

void MainLoop(CascadingShadowsExample *app) {
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

			zest_SetInstanceMeshDrawing(mesh_layer, 0, app->shadow_pipeline);
			app->scene_push.texture_index = zest_GetLayerMeshTextureIndex(mesh_layer, 0);
			zest_SetLayerPushConstants(mesh_layer, &app->scene_push, sizeof(scene_push_constants_t));
			for (int i = 0; i != 5; i++) {
				DrawInstancedMesh(mesh_layer, &positions[i].x, zero, &scale.x);
			}

			zest_SetInstanceMeshDrawing(mesh_layer, 1, app->shadow_pipeline);
			app->scene_push.texture_index = zest_GetLayerMeshTextureIndex(mesh_layer, 1);
			zest_SetLayerPushConstants(mesh_layer, &app->scene_push, sizeof(scene_push_constants_t));
			for (int i = 0; i != 5; i++) {
				DrawInstancedMesh(mesh_layer, &positions[i].x, zero, &scale.x);
			}

			zest_SetInstanceMeshDrawing(mesh_layer, 2, app->shadow_pipeline);
			app->scene_push.texture_index = zest_GetLayerMeshTextureIndex(mesh_layer, 2);
			zest_SetLayerPushConstants(mesh_layer, &app->scene_push, sizeof(scene_push_constants_t));
			DrawInstancedMesh(mesh_layer, zero, zero, &scale.x);

			//Initially when the 3 textures that are created using compute shaders in the setup they will be in 
			//image layout general. When they are used in the frame graph below they will be transitioned to read only
			//so we store the current layout of the image in a custom cache info struct so that when the layout changes
			//the cache key will change and a new cache will be created as a result. The other option is to transition 
			//them before hand but this is just to show an example of how the frame graph caching can work.
			app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw(&app->imgui);
			app->cache_info.draw_depth_image = app->debug_draw;
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
					zest_format_d32_sfloat_s8_uint,
					zest_resource_usage_hint_none,
					4096, 4096,
					1, 
					SHADOW_MAP_CASCADE_COUNT
				};

				zest_uniform_buffer vert_buffer = zest_GetUniformBuffer(app->vert_buffer);
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

					if (!app->debug_draw) {
						//------------------------ Mesh Layer Pass ------------------------------------------------------------
						zest_BeginRenderPass("Mesh Layer Pass"); {
							zest_ConnectInput(mesh_layer_resource);
							zest_ConnectInput(shadow_buffer);
							zest_ConnectOutputGroup(group);
							zest_SetPassTask(DrawSceneWithShadows, app);
							zest_EndPass();
						}
					} else {
						zest_BeginRenderPass("Debug Pass"); {
							zest_ConnectInput(shadow_buffer);
							zest_ConnectOutputGroup(group);
							zest_SetPassTask(DrawDebugShadowMap, app);
							zest_EndPass();
						}
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

	CascadingShadowsExample imgui_app = {};
	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);

	//Create the device that serves all vulkan based contexts
	imgui_app.device = zest_implglfw_CreateDevice(false);

	//Create a window using GLFW
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");

	//Create new config struct for Zest
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	//Initialise Zest
	imgui_app.context = zest_CreateContext(imgui_app.device, &window_handles, &create_info);

	//int *test = nullptr;
	//zest_vec_push(imgui_app.context->device->allocator, test, 10);

	//Initialise our example
	InitCascadingShadowsExample(&imgui_app);

	//Start the main loop
	MainLoop(&imgui_app);
	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Destroy(&imgui_app.imgui);
	zest_DestroyDevice(imgui_app.device);

	return 0;
}
