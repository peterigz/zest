#include "zest-ribbons.h"
#include "imgui_internal.h"
#include <cmath>

void InitImGuiApp(Ribbons *app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise(&app->imgui_layer_info);
	//Implement a dark style
	zest_imgui_DarkStyle();
	
	//This is an exmaple of how to change the font that ImGui uses
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

	//Rebuild the Zest font texture
	zest_imgui_RebuildFontTexture(&app->imgui_layer_info, tex_width, tex_height, font_data);

	app->sync_refresh = true;

	//Set up the compute shader
	//Create a new empty compute shader in the renderer
	app->ribbon_compute = zest_CreateCompute("ribbons");
	app->ribbon_buffer = zest_CreateComputeVertexDescriptorBuffer(SEGMENT_COUNT * sizeof(ribbon_segment) * 10, true);
	app->ribbon_vertex_buffer = zest_CreateComputeVertexDescriptorBuffer(SEGMENT_COUNT * sizeof(ribbon_vertex) * 100, false);
	app->ribbon_index_buffer = zest_CreateComputeIndexDescriptorBuffer(SEGMENT_COUNT * sizeof(zest_uint) * 360, false);
	app->ribbon_staging_buffer[0] = zest_CreateStagingBuffer(SEGMENT_COUNT * sizeof(ribbon_segment) * 10, 0);
	app->ribbon_staging_buffer[1] = zest_CreateStagingBuffer(SEGMENT_COUNT * sizeof(ribbon_segment) * 10, 0);
	//A builder is used to simplify the compute shader setup process
	zest_compute_builder_t builder = zest_NewComputeBuilder();
	//Declare the bindings we want in the shader
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2);
	//The add the buffers for binding in the same order as the layout bindings
	zest_AddComputeBufferForBinding(&builder, app->ribbon_buffer);
	zest_AddComputeBufferForBinding(&builder, app->ribbon_vertex_buffer);
	zest_AddComputeBufferForBinding(&builder, app->ribbon_index_buffer);
	zest_SetComputePushConstantSize(&builder, sizeof(camera_push_constant));
	//Set the user data so that we can use it in the callback funcitons
	zest_SetComputeUserData(&builder, app);

	//Compile the shaders we will use for the ribbons
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	app->ribbon_comp_shader = zest_CreateShaderFromFile("examples/assets/shaders/ribbons.comp", "ribbon_comp.spv", shaderc_compute_shader, true, compiler);
	app->ribbon_vert_shader = zest_CreateShaderFromFile("examples/assets/shaders/ribbon_3d.vert", "ribbon_3d_vert.spv", shaderc_vertex_shader, true, compiler);
	app->ribbon_frag_shader = zest_CreateShaderFromFile("examples/assets/shaders/ribbon.frag", "ribbon_frag.spv", shaderc_fragment_shader, true, compiler);
	shaderc_compiler_release(compiler);

	//Declare the actual shader to use
	app->compute_pipeline_index = zest_AddComputeShader(&builder, app->ribbon_comp_shader);
	zest_MakeCompute(&builder, app->ribbon_compute);

	zest_pipeline_template_create_info_t instance_create_info = zest_CreatePipelineTemplateCreateInfo();
	instance_create_info.viewport.extent = zest_GetSwapChainExtent();
	//Set up the vertex attributes that will take in all of the billboard data stored in tfx_3d_instance_t objects
	zest_AddVertexInputBindingDescription(&instance_create_info, 0, sizeof(ribbon_vertex), VK_VERTEX_INPUT_RATE_VERTEX);
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(ribbon_vertex, position)));	  
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(ribbon_vertex, uv)));	
	//Set the shaders to our custom timelinefx shaders
	zest_SetPipelineTemplateVertShader(&instance_create_info, "ribbon_3d_vert.spv", 0);
	zest_SetPipelineTemplateFragShader(&instance_create_info, "ribbon_frag.spv", 0);
	//zest_SetPipelineTemplatePushConstant(&instance_create_info, sizeof(zest_push_constants_t), 0, VK_SHADER_STAGE_VERTEX_BIT);
	zest_AddPipelineTemplateDescriptorLayout(&instance_create_info, *zest_GetDescriptorSetLayout("1 sampler"));
	app->ribbon_pipeline = zest_AddPipeline("tfx_billboard_pipeline");
	zest_MakePipelineTemplate(app->ribbon_pipeline, zest_GetStandardRenderPass(), &instance_create_info);
	app->ribbon_pipeline->pipeline_template.colorBlendAttachment = zest_PreMultiplyBlendState();
	app->ribbon_pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
	app->ribbon_pipeline->pipeline_template.depthStencil.depthTestEnable = VK_TRUE;
	app->ribbon_pipeline->pipeline_template.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	//app->ribbon_pipeline->pipeline_template.rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
	//app->ribbon_pipeline->pipeline_template.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	zest_BuildPipeline(app->ribbon_pipeline);

	app->ribbon_texture = zest_CreateTextureBank("ribbon texture", zest_texture_format_rgba);
	app->ribbon_image = zest_AddTextureImageFile(app->ribbon_texture, "examples/assets/BrightGlow.png");
	zest_ProcessTextureImages(app->ribbon_texture);

	app->ribbon_shader_resources = zest_CreateShaderResources();
	zest_AddDescriptorSetToResources(app->ribbon_shader_resources, ZestRenderer->uniform_descriptor_set);
	zest_AddDescriptorSetToResources(app->ribbon_shader_resources, zest_GetTextureDescriptorSet(app->ribbon_texture));

	//Set up our own draw routine
	app->ribbon_draw_routine = zest_CreateDrawRoutine("compute draw");
	app->ribbon_draw_routine->record_callback = RecordComputeRibbons;
	app->ribbon_draw_routine->condition_callback = DrawComputeRibbonsCondition;
	app->ribbon_draw_routine->user_data = app;

	//Modify the existing default queue
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_NewComputeSetup("Compute Ribbon", app->ribbon_compute, RibbonComputeFunction);
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			app->line_layer = zest_NewBuiltinLayerSetup("test lines", zest_builtin_layer_3dlines);
			zest_AddDrawRoutine(app->ribbon_draw_routine);
			//Create a Dear ImGui layer
			zest_imgui_CreateLayer(&app->imgui_layer_info);
		}
		//Don't forget to finish the queue set up
		zest_FinishQueueSetup();
	}

	zest_SetLayerToManualFIF(app->line_layer);

	//zest_SetLayerToManualFIF(app->line_layer);
	app->line_pipeline = zest_Pipeline("pipeline_line3d_instance");

	app->timer = zest_CreateTimer(60);

	int tessellation = 1;
	app->ribbon_buffer_info = GenerateRibbonIndices(app, tessellation, SEGMENT_COUNT * 10, 10);

	app->camera = zest_CreateCamera();
	zest_CameraSetFoV(&app->camera, 60.f);
	app->camera_push.uv_scale = 1.f;
	app->camera_push.uv_offset = 0.f;
	app->camera_push.width_scale_multiplier = 1.5f;
	app->camera_push.tessellation = tessellation;

	app->ribbon_count = 10;

	UpdateUniform3d(app);
}

// tessellation: number of divisions across the width of the ribbon
// maxSegments: maximum number of segments the ribbon can have
RibbonBufferInfo GenerateRibbonIndices(Ribbons *app, uint32_t tessellation, uint32_t maxSegments, uint32_t max_ribbons) {
	RibbonBufferInfo info{};

	// Calculate buffer properties
	info.verticesPerSegment = (tessellation + 1) * 2; // vertices across the ribbon width * 2 for both sides
	info.trianglesPerSegment = tessellation * 4;      // 2 triangles per quad * 2 strips * tessellation
	info.indicesPerSegment = info.trianglesPerSegment * 3;
	info.totalIndices = info.indicesPerSegment * (maxSegments - 1);
	info.vertexStrideMultiplier = info.verticesPerSegment;

	/*
	// Generate indices for each segment
	if (tessellation > 0) {
		// Reserve space for all indices
		app->ribbon_indices.reserve(info.totalIndices);
		app->ribbon_indices.clear();
		for (uint32_t segment = 0; segment < maxSegments - 1; ++segment) {
			uint32_t baseVertex = segment * info.vertexStrideMultiplier;
			uint32_t nextBaseVertex = (segment + 1) * info.vertexStrideMultiplier;

			// Generate indices for each tessellation step
			for (uint32_t t = 0; t < tessellation; ++t) {
				// Calculate vertex indices for current tessellation step
				uint32_t v0 = baseVertex + t;
				uint32_t v1 = baseVertex + t + 1;
				uint32_t v2 = nextBaseVertex + t;
				uint32_t v3 = nextBaseVertex + t + 1;

				// Left strip triangles
				app->ribbon_indices.push_back(v0);
				app->ribbon_indices.push_back(v1);
				app->ribbon_indices.push_back(v2);

				app->ribbon_indices.push_back(v2);
				app->ribbon_indices.push_back(v1);
				app->ribbon_indices.push_back(v3);

				// Right strip triangles
				// Offset by tessellation + 1 to get to the right side vertices
				v0 += (tessellation + 1);
				v1 += (tessellation + 1);
				v2 += (tessellation + 1);
				v3 += (tessellation + 1);

				app->ribbon_indices.push_back(v0);
				app->ribbon_indices.push_back(v1);
				app->ribbon_indices.push_back(v2);

				app->ribbon_indices.push_back(v2);
				app->ribbon_indices.push_back(v1);
				app->ribbon_indices.push_back(v3);
			}
		}
	} else {
		// Single strip case (no tessellation)
		info.verticesPerSegment = 2;  // Just 2 vertices per segment
		info.trianglesPerSegment = 2; // 2 triangles per segment
		info.indicesPerSegment = 6;   // 6 indices per segment (2 triangles * 3 vertices)
		info.totalIndices = info.indicesPerSegment * (maxSegments - 1);
		info.vertexStrideMultiplier = info.verticesPerSegment;

		// Reserve space for all indices
		app->ribbon_indices.reserve(info.totalIndices);
		app->ribbon_indices.clear();

		// Generate indices for each segment
		for (uint32_t segment = 0; segment < maxSegments - 1; ++segment) {
			uint32_t v0 = segment * 2;
			uint32_t v1 = v0 + 2;

			// First triangle
			app->ribbon_indices.push_back(v0);
			app->ribbon_indices.push_back(v0 + 1);
			app->ribbon_indices.push_back(v1);

			// Second triangle
			app->ribbon_indices.push_back(v1);
			app->ribbon_indices.push_back(v0 + 1);
			app->ribbon_indices.push_back(v1 + 1);
		}
		for (int i = 0; i != max_ribbons; ++i) {
			app->ribbon_indices.push_back(0xFFFFFFFF);
		}
	}

	zest_buffer staging_buffer = zest_CreateStagingBuffer(info.totalIndices * sizeof(zest_uint), app->ribbon_indices.data());
	zest_CopyBuffer(staging_buffer, app->ribbon_index_buffer->buffer[0], info.totalIndices * sizeof(zest_uint));
	zest_FreeBuffer(staging_buffer);
	*/

	return info;
}

void RecordComputeRibbons(zest_work_queue_t *queue, void *data) {
	zest_draw_routine draw_routine = (zest_draw_routine)data;
	Ribbons &app = *static_cast<Ribbons *>(draw_routine->user_data);
	if (app.ribbons[0].length == 0) return;
	VkCommandBuffer command_buffer = zest_BeginRecording(draw_routine->recorder, draw_routine->draw_commands->render_pass, ZEST_FIF);
			
	zest_SetViewport(command_buffer, draw_routine);

	//Bind the buffer that contains the sprite instances to draw. These are updated by the compute shader on the GPU
	zest_BindVertexBuffer(command_buffer, app.ribbon_vertex_buffer->buffer[0]);
	zest_BindIndexBuffer(command_buffer, app.ribbon_index_buffer->buffer[0]);

	//Draw all the sprites in the buffer that is built by the compute shader
	zest_BindPipelineShaderResource(command_buffer, app.ribbon_pipeline, app.ribbon_shader_resources, ZEST_FIF);
	//zest_SendPushConstants(command_buffer, app.ribbon_pipeline, VK_SHADER_STAGE_VERTEX_BIT, sizeof(zest_push_constants_t), &app.push_constants);

	//zest_DrawIndexedIndirect(command_buffer, app.ribbon_draw_commands);
	zest_DrawIndexed(command_buffer, app.index_count, 1, 0, 0, 0);

	zest_EndRecording(draw_routine->recorder, ZEST_FIF);
}

int DrawComputeRibbonsCondition(zest_draw_routine draw_routine) {
	//We can just always excute the draw commands to draw the ribbons
	return 1;
}

void UploadBuffers(Ribbons *app) {
	zest_buffer staging_buffer = zest_CreateStagingBuffer(SEGMENT_COUNT * sizeof(ribbon_segment), app->ribbons[0].ribbon_segments);
	zest_CopyBuffer(staging_buffer, app->ribbon_buffer->buffer[0], staging_buffer->size);
	zest_FreeBuffer(staging_buffer);
}

//Every frame the compute shader needs to be dispatched which means that all the commands for the compute shader
//need to be added to the command buffer
void RibbonComputeFunction(zest_command_queue_compute compute_routine) {

	//The compute queue item can contain more then one compute shader to be dispatched but in this case there's only one
	zest_compute compute;
	while (compute = zest_NextComputeRoutine(compute_routine)) {
		//Grab our ComputeExample struct out of the user data
		Ribbons *app = static_cast<Ribbons *>(compute->user_data);

		zest_uint total_segments = CountSegments(app);
		if (!total_segments) continue;

		//Bind the compute shader pipeline
		zest_BindComputePipeline(compute, app->compute_pipeline_index);

		//Some graphics cards don't support direct writing to the GPU buffer so we have to copy to a staging buffer first, then
		//from there we copy to the GPU.
		zest_CopyBufferCB(zest_CurrentCommandBuffer(), app->ribbon_staging_buffer[ZEST_FIF], app->ribbon_buffer->buffer[ZEST_FIF], sizeof(ribbon_segment) * total_segments, 1);

		//Send the push constants in the compute object to the shader
		zest_SendCustomComputePushConstants(ZestRenderer->current_command_buffer, compute, &app->camera_push);

		//The 128 here refers to the local_size_x in the shader and is how many elements each group will work on
		//For example if there are 1024 sprites, if we divide by 128 there will be 8 groups working on 128 sprites each in parallel
		zest_DispatchCompute(compute, (total_segments / 128) + 1, 1, 1);
	}

	//We want the compute shader to finish before the vertex shader is run so we put a barrier here.
	//zest_ComputeToVertexBarrier();
}

//Basic function for updating the uniform buffer
void UpdateUniform3d(Ribbons *app) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(ZestRenderer->standard_uniform_buffer);
	buffer_3d->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	buffer_3d->proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.1f, 10000.f);
	buffer_3d->proj.v[1].y *= -1.f;
	buffer_3d->screen_size.x = zest_ScreenWidthf();
	buffer_3d->screen_size.y = zest_ScreenHeightf();
	buffer_3d->millisecs = 0;
}

//Allows us to cast a ray into the screen from the mouse position to place an effect where we click
zest_vec3 ScreenRay(float x, float y, float depth_offset, zest_vec3 &camera_position, zest_descriptor_buffer buffer) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
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

void BuildUI(Ribbons *app) {
	//Must call the imgui GLFW implementation function
	ImGui_ImplGlfw_NewFrame();
	//Draw our imgui stuff
	ImGui::NewFrame();
	ImGui::Begin("Test Window");
	ImGui::Text("FPS %i", ZestApp->last_fps);
	ImGui::DragFloat("Texture Scale", &app->camera_push.uv_scale, 0.1f);
	ImGui::DragFloat("Texture Offset", &app->camera_push.uv_offset, 0.1f);
	ImGui::DragFloat("Width Scalar", &app->camera_push.width_scale_multiplier, 0.01f);
	if (ImGui::Button("Toggle Refresh Rate Sync")) {
		if (app->sync_refresh) {
			zest_DisableVSync();
			app->sync_refresh = false;
		} else {
			zest_EnableVSync();
			app->sync_refresh = true;
		}
	}
	ImGui::End();
	ImGui::Render();
	//An imgui layer is a manual layer, meaning that you need to let it know that the buffers need updating.
	zest_ResetLayer(app->imgui_layer_info.mesh_layer);
	//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
	zest_imgui_UpdateBuffers(app->imgui_layer_info.mesh_layer);
}

zest_uint CountSegments(Ribbons *app) {
	int count = 0;
	for (int i = 0; i != app->ribbon_count; ++i) {
		count += app->ribbons[i].length;
	}
	return count;
}

void UpdateCallback(zest_microsecs elapsed, void* user_data) {
	//Set the active command queue to the default one that was created when Zest was initialised
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);
	Ribbons* app = (Ribbons*)user_data;

	UpdateUniform3d(app);

	//First control the camera with the mosue if the right mouse is clicked
	bool camera_free_look = false;
	if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		camera_free_look = true;
		if (glfwRawMouseMotionSupported()) {
			glfwSetInputMode((GLFWwindow *)zest_Window(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		}
		ZEST__FLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
		double x_mouse_speed;
		double y_mouse_speed;
		zest_GetMouseSpeed(&x_mouse_speed, &y_mouse_speed);
		zest_TurnCamera(&app->camera, (float)ZestApp->mouse_delta_x, (float)ZestApp->mouse_delta_y, .05f);
	} else if (glfwRawMouseMotionSupported()) {
		camera_free_look = false;
		ZEST__UNFLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
		glfwSetInputMode((GLFWwindow *)zest_Window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	} else {
		camera_free_look = false;
		ZEST__UNFLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
	}

	if (ImGui::IsKeyReleased(ImGuiKey_Space)) {
		app->ribbon_built = true;
	}

	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(ZestRenderer->standard_uniform_buffer);
	zest_StartTimerLoop(app->timer) {
		BuildUI(app);

		for (int r = 0; r != 10; ++r) {
			float time = ((zest_Millisecs() + r * 500) % 1000000) * 0.001f;
			float radius;
			zest_vec3 position;

			// Different patterns for each ribbon
			switch (r) {
			case 0: // Circular motion with varying radius
				radius = 2.0f + sinf(time * 0.5f);
				position = {
					radius * cosf(time * 2.0f),
					sinf(time),
					radius * sinf(time * 2.0f)
				};
				break;

			case 1: // Figure-8 pattern
				position = {
					2.0f * sinf(time * 2.0f),
					cosf(time * 4.0f),
					2.0f * sinf(time * 4.0f) * cosf(time * 2.0f)
				};
				break;

			case 2: // Spiral motion
				radius = time * 0.1f;
				position = {
					radius * cosf(time * 3.0f),
					time * 0.2f,
					radius * sinf(time * 3.0f)
				};
				break;

			case 3: // Bouncing motion
				position = {
					2.0f * cosf(time),
					fabsf(sinf(time * 2.0f)) * 3.0f,
					2.0f * sinf(time)
				};
				break;

			case 4: // Lissajous curve
				position = {
					2.0f * sinf(time * 2.0f),
					2.0f * sinf(time * 3.0f),
					2.0f * sinf(time * 4.0f)
				};
				break;

			case 5: // Helical motion
				position = {
					2.0f * cosf(time * 3.0f),
					sinf(time * 2.0f),
					2.0f * sinf(time * 3.0f)
				};
				break;

			case 6: // Expanding/contracting circle
				radius = 1.0f + sinf(time);
				position = {
					radius * cosf(time * 2.5f),
					cosf(time * 1.5f),
					radius * sinf(time * 2.5f)
				};
				break;

			case 7: // Wave motion
				position = {
					2.0f * sinf(time * 2.0f),
					sinf(time * 4.0f + sinf(time)),
					2.0f * cosf(time * 1.5f)
				};
				break;

			case 8: // Flower pattern
				radius = 2.0f * (1.0f + 0.3f * cosf(5.0f * time));
				position = {
					radius * cosf(time),
					sinf(time * 3.0f),
					radius * sinf(time)
				};
				break;

			case 9: // Random-looking but deterministic motion
				position = {
					2.0f * sinf(time * 2.3f) * cosf(time * 1.1f),
					1.5f * sinf(time * 1.7f) * cosf(time * 2.1f),
					2.0f * sinf(time * 1.3f) * cosf(time * 1.9f)
				};
				break;
			}

			int current_index = app->ribbons[r].ribbon_index;
			app->ribbons[r].length = ZEST__MIN(app->ribbons[r].length + 1, SEGMENT_COUNT);
			app->ribbons[r].ribbon_segments[current_index].position_and_width = { position.x, position.y, position.z, .1f };
			app->ribbons[r].ribbon_segments[current_index].parameters = { (float(current_index) / app->ribbons[r].length) * 2.f, 0.f, 0.f, 0.f};

			app->ribbons[r].ribbon_index = (current_index + 1) % SEGMENT_COUNT;
		}

		float speed = 5.f * (float)app->timer->update_time;
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
			ImGui::SetWindowFocus(nullptr);

			if (ImGui::IsKeyDown(ImGuiKey_W)) {
				zest_CameraMoveForward(&app->camera, speed);
			}
			if (ImGui::IsKeyDown(ImGuiKey_S)) {
				zest_CameraMoveBackward(&app->camera, speed);
			}
			if (ImGui::IsKeyDown(ImGuiKey_UpArrow)) {
				zest_CameraMoveUp(&app->camera, speed);
			}
			if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) {
				zest_CameraMoveDown(&app->camera, speed);
			}
			if (ImGui::IsKeyDown(ImGuiKey_A)) {
				zest_CameraStrafLeft(&app->camera, speed);
			}
			if (ImGui::IsKeyDown(ImGuiKey_D)) {
				zest_CameraStrafRight(&app->camera, speed);
			}
		}

		//Restore the mouse when right mouse isn't held down
		if (camera_free_look) {
			glfwSetInputMode((GLFWwindow *)zest_Window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		} else {
			glfwSetInputMode((GLFWwindow *)zest_Window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}

	} zest_EndTimerLoop(app->timer);

	app->camera_push.position = { app->camera.position.x, app->camera.position.y, app->camera.position.z, 0.f };
	app->camera_push.segment_count = CountSegments(app);
	app->camera_push.index_offset = 0;
	app->camera_push.ribbon_count = app->ribbon_count;
	zest_uint running_segment_offset = 0;
	zest_uint running_index_offset = 0;
	app->index_count = 0;
	for (zest_uint i = 0; i != app->ribbon_count; ++i) {
		if (app->ribbons[i].ribbon_index == 0) {
			memcpy((ribbon_segment *)app->ribbon_staging_buffer[ZEST_FIF]->data + running_segment_offset, app->ribbons[i].ribbon_segments, app->ribbons[i].length * sizeof(ribbon_segment));
		} else {
			memcpy((ribbon_segment *)app->ribbon_staging_buffer[ZEST_FIF]->data + running_segment_offset, app->ribbons[i].ribbon_segments + app->ribbons[i].ribbon_index, (app->ribbons[i].length - app->ribbons[i].ribbon_index) * sizeof(ribbon_segment));
			memcpy((ribbon_segment *)app->ribbon_staging_buffer[ZEST_FIF]->data + (app->ribbons[i].length - app->ribbons[i].ribbon_index) + running_segment_offset, app->ribbons[i].ribbon_segments, app->ribbons[i].ribbon_index * sizeof(ribbon_segment));
		}
		app->index_count += (app->ribbons[i].length - 1) * app->ribbon_buffer_info.indicesPerSegment;
		running_segment_offset += app->ribbons[i].length;
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfo();
    create_info.log_path = ".";
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	//Implement GLFW for window creation
	zest_implglfw_SetCallbacks(&create_info);

	Ribbons imgui_app{};

	//Initialise Zest
	zest_Initialise(&create_info);
	//Set the Zest use data
	zest_SetUserData(&imgui_app);
	//Set the udpate callback to be called every frame
	zest_SetUserUpdateCallback(UpdateCallback);
	//Initialise our example
	InitImGuiApp(&imgui_app);

	//Start the main loop
	zest_Start();

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	zest_implglfw_SetCallbacks(&create_info);
    ZEST__FLAG(create_info.flags, zest_init_flag_maximised);

	Ribbons imgui_app;

    create_info.log_path = ".";
	zest_Initialise(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#endif
