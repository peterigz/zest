#include "zest-ribbons.h"
#include "imgui_internal.h"
#include <cmath>

void InitImGuiApp(Ribbons *app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise();
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
	zest_imgui_RebuildFontTexture(tex_width, tex_height, font_data);

	app->sync_refresh = true;

	int tessellation = 2;
	app->ribbon_buffer_info = GenerateRibbonInfo(tessellation, SEGMENT_COUNT * 10, 10);
	app->ribbon_count = RIBBON_COUNT;

	zest_ForEachFrameInFlight(fif) {
		app->ribbon_segment_staging_buffer[fif] = zest_CreateStagingBuffer(SEGMENT_COUNT * sizeof(ribbon_segment) * 10, 0);
		app->ribbon_instance_staging_buffer[fif] = zest_CreateStagingBuffer(SEGMENT_COUNT * sizeof(ribbon_instance) * 10, 0);
	}
	//A builder is used to simplify the compute shader setup process
	zest_compute_builder_t builder = zest_BeginComputeBuilder();
	//Declare the bindings we want in the shader
	zest_SetComputeBindlessLayout(&builder, ZestRenderer->global_bindless_set_layout);
	//The add the buffers for binding in the same order as the layout bindings
	zest_SetComputePushConstantSize(&builder, sizeof(camera_push_constant));
	//Set the user data so that we can use it in the callback funcitons
	zest_SetComputeUserData(&builder, app);

	//Compile the shaders we will use for the ribbons
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	app->ribbon_comp_shader = zest_CreateShaderFromFile("examples/assets/shaders/ribbons.comp", "ribbon_comp.spv", shaderc_compute_shader, true, compiler, 0);
	app->ribbon_vert_shader = zest_CreateShaderFromFile("examples/assets/shaders/ribbon_3d.vert", "ribbon_3d_vert.spv", shaderc_vertex_shader, true, compiler, 0);
	app->ribbon_frag_shader = zest_CreateShaderFromFile("examples/assets/shaders/ribbon.frag", "ribbon_frag.spv", shaderc_fragment_shader, true, compiler, 0);
	shaderc_compiler_release(compiler);

	//Declare the actual shader to use
	app->compute_pipeline_index = zest_AddComputeShader(&builder, app->ribbon_comp_shader);
	app->ribbon_compute = zest_FinishCompute(&builder, "Ribbon Compute");

	app->ribbon_pipeline = zest_BeginPipelineTemplate("Ribbon Pipeline");
	//Set up the vertex attributes that will take in all of the billboard data stored in tfx_3d_instance_t objects
	zest_AddVertexInputBindingDescription(app->ribbon_pipeline, 0, sizeof(ribbon_vertex), VK_VERTEX_INPUT_RATE_VERTEX);
	zest_AddVertexAttribute(app->ribbon_pipeline, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(ribbon_vertex, position));	  
	zest_AddVertexAttribute(app->ribbon_pipeline, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(ribbon_vertex, uv));	
	zest_AddVertexAttribute(app->ribbon_pipeline, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ribbon_vertex, color));	
	//Set the shaders to our custom timelinefx shaders
	zest_SetPipelineVertShader(app->ribbon_pipeline, "ribbon_3d_vert.spv", 0);
	zest_SetPipelineFragShader(app->ribbon_pipeline, "ribbon_frag.spv", 0);
	zest_SetPipelinePushConstantRange(app->ribbon_pipeline, sizeof(ribbon_drawing_push_constants), zest_shader_fragment_stage);
	zest_AddPipelineDescriptorLayout(app->ribbon_pipeline, zest_vk_GetDefaultUniformBufferLayout());
	zest_AddPipelineDescriptorLayout(app->ribbon_pipeline, zest_vk_GetGlobalBindlessLayout());
	zest_SetPipelineDepthTest(app->ribbon_pipeline, false, true);
	zest_SetPipelineBlend(app->ribbon_pipeline, zest_PreMultiplyBlendState());
	zest_EndPipelineTemplate(app->ribbon_pipeline);
	app->ribbon_pipeline->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	app->ribbon_pipeline->rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;

	app->ribbon_texture = zest_CreateTextureBank("ribbon texture", zest_texture_format_rgba_unorm);
	app->ribbon_image = zest_AddTextureImageFile(app->ribbon_texture, "examples/assets/BrightGlow.png");
	zest_ProcessTextureImages(app->ribbon_texture);

	zest_AcquireGlobalCombinedImageSampler(app->ribbon_texture);

	app->timer = zest_CreateTimer("Fixed update timer", 60);

	app->camera = zest_CreateCamera();
	zest_CameraSetFoV(&app->camera, 60.f);
	app->camera_push.uv_scale = .1f;
	app->camera_push.uv_offset = 0.5f;
	app->camera_push.width_scale_multiplier = 1.5f;
	app->camera_push.tessellation = tessellation;
	app->camera.position.x = -5.f;

	for (int i = 0; i != RIBBON_COUNT; ++i) {
		//app->ribbons[i].length = rand() % 100 + 28;
		app->ribbons[i].length = SEGMENT_COUNT;
	}

	UpdateUniform3d(app);
	for (int s = 0; s != SEGMENT_COUNT; ++s) {
		UpdateRibbons(app);
	}

	app->segment_buffer_info.usage_hints = 0;
	app->instance_buffer_info.usage_hints = 0;
	app->vertex_buffer_info.usage_hints = zest_resource_usage_hint_vertex_buffer;
	app->index_buffer_info.usage_hints = zest_resource_usage_hint_index_buffer;
}

// tessellation: number of divisions across the width of the ribbon
// maxSegments: maximum number of segments the ribbon can have
RibbonBufferInfo GenerateRibbonInfo(uint32_t tessellation, uint32_t maxSegments, uint32_t max_ribbons) {
	RibbonBufferInfo info{};

	// Calculate buffer properties
	info.verticesPerSegment = (tessellation + 1) * 2; // vertices across the ribbon width * 2 for both sides
	info.trianglesPerSegment = tessellation * 4;      // 2 triangles per quad * 2 strips * tessellation
	info.indicesPerSegment = info.trianglesPerSegment * 3;

	return info;
}

void UploadRibbonData(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	Ribbons *app = (Ribbons*)user_data;

    zest_resource_node segment_buffer = zest_GetPassOutputResource(context, "Ribbon Segment Buffer");
    zest_resource_node ribbon_instance_buffer = zest_GetPassOutputResource(context, "Ribbon Instance Buffer");

	if (segment_buffer->storage_buffer) {
		zest_CopyBuffer(command_buffer, app->ribbon_segment_staging_buffer[ZEST_FIF], segment_buffer->storage_buffer, app->ribbon_segment_staging_buffer[ZEST_FIF]->memory_in_use);
	}
	if (ribbon_instance_buffer->storage_buffer) {
		zest_CopyBuffer(command_buffer, app->ribbon_instance_staging_buffer[ZEST_FIF], ribbon_instance_buffer->storage_buffer, app->ribbon_instance_staging_buffer[ZEST_FIF]->memory_in_use);
	}
}

void RecordRibbonDrawing(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	Ribbons *app = (Ribbons*)user_data;

    zest_buffer vertex_buffer = zest_GetPassInputBuffer(context, "Ribbon Vertex Buffer");
    zest_buffer index_buffer = zest_GetPassInputBuffer(context, "Ribbon Index Buffer");

	//Bind the buffer that contains the sprite instances to draw. These are updated by the compute shader on the GPU
	zest_BindVertexBuffer(command_buffer, vertex_buffer);
	zest_BindIndexBuffer(command_buffer, index_buffer);

	zest_pipeline pipeline = zest_PipelineWithTemplate(app->ribbon_pipeline, context->render_pass);

	VkDescriptorSet sets[] = {
		zest_vk_GetUniformBufferSet(ZestRenderer->uniform_buffer),
		zest_vk_GetGlobalBindlessSet()
	};
	//Draw all the sprites in the buffer that is built by the compute shader
	zest_BindPipeline(command_buffer, pipeline, sets, 2);
	zest_SendPushConstants(command_buffer, pipeline, &app->ribbon_push_constants);
	zest_SetScreenSizedViewport(context, 0.f, 1.f);

	//zest_DrawIndexedIndirect(command_buffer, app->ribbon_draw_commands);
	zest_DrawIndexed(command_buffer, app->index_count, 1, 0, 0, 0);
}

//Every frame the compute shader needs to be dispatched which means that all the commands for the compute shader
//need to be added to the command buffer
void RecordComputeCommands(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	Ribbons *app = (Ribbons*)user_data;

	zest_uint total_segments = CountSegments(app);
	if (!total_segments) return;

	VkDescriptorSet sets[] = {
		zest_vk_GetGlobalBindlessSet()
	};

	//Bind the compute shader pipeline
	zest_BindComputePipeline(command_buffer, app->ribbon_compute, sets, 1);

    zest_resource_node segment_buffer = zest_GetPassInputResource(context, "Ribbon Segment Buffer");
    zest_resource_node ribbon_instance_buffer = zest_GetPassInputResource(context, "Ribbon Instance Buffer");
    zest_resource_node vertex_buffer = zest_GetPassOutputResource(context, "Ribbon Vertex Buffer");
    zest_resource_node index_buffer = zest_GetPassOutputResource(context, "Ribbon Index Buffer");

	app->camera_push.segment_buffer_index = zest_GetTransientBufferBindlessIndex(context, segment_buffer);
	app->camera_push.instance_buffer_index = zest_GetTransientBufferBindlessIndex(context, ribbon_instance_buffer);
	app->camera_push.vertex_buffer_index = zest_GetTransientBufferBindlessIndex(context, vertex_buffer);
	app->camera_push.index_buffer_index = zest_GetTransientBufferBindlessIndex(context, index_buffer);

	//Send the push constants in the compute object to the shader
	zest_SendCustomComputePushConstants(command_buffer, app->ribbon_compute, &app->camera_push);

	//The 128 here refers to the local_size_x in the shader and is how many elements each group will work on
	//For example if there are 1024 sprites, if we divide by 128 there will be 8 groups working on 128 sprites each in parallel
	zest_DispatchCompute(command_buffer, app->ribbon_compute, ((SEGMENT_COUNT * app->ribbon_count) / 128) + 1, 1, 1);
}

//Basic function for updating the uniform buffer
void UpdateUniform3d(Ribbons *app) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(ZestRenderer->uniform_buffer);
	buffer_3d->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	buffer_3d->proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.1f, 10000.f);
	buffer_3d->proj.v[1].y *= -1.f;
	buffer_3d->screen_size.x = zest_ScreenWidthf();
	buffer_3d->screen_size.y = zest_ScreenHeightf();
	buffer_3d->millisecs = 0;
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
	zest_imgui_UpdateBuffers();
}

zest_uint CountSegments(Ribbons *app) {
	int count = 0;
	for (int i = 0; i != app->ribbon_count; ++i) {
		count += app->ribbons[i].length;
	}
	return count;
}

void UpdateRibbons(Ribbons *app) {
	app->seconds_passed += (float)app->timer->update_time * 1000.f;
	for (int r = 0; r != app->ribbon_count; ++r) {
		float time = (float(app->seconds_passed) + r * 500.f) * 0.001f;
		float radius;
		zest_vec3 position;
		zest_color color;

		// Different patterns for each ribbon
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

		int current_index = app->ribbons[r].ribbon_index;
		int segment_offset = r * SEGMENT_COUNT;	
		app->ribbons[r].length = ZEST__MIN(app->ribbons[r].length + 1, SEGMENT_COUNT);
		app->ribbon_segments[segment_offset + current_index].position_and_width = { position.x, position.y, position.z, .1f };
		app->ribbon_segments[segment_offset + current_index].color = color;

		app->ribbon_instances[r].width_scale = 1.f + float(r) * .15f;
		app->ribbon_instances[r].start_index = app->ribbons[r].start_index;

		app->ribbons[r].ribbon_index = (current_index + 1) % app->ribbons[r].length;
		if (app->ribbons[r].length == SEGMENT_COUNT) {
			app->ribbons[r].start_index++;
			app->ribbons[r].start_index %= app->ribbons[r].length;
		}
	}
}

void UpdateCallback(zest_microsecs elapsed, void* user_data) {
	//Set the active command queue to the default one that was created when Zest was initialised
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

	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(ZestRenderer->uniform_buffer);
	zest_StartTimerLoop(app->timer) {
		BuildUI(app);

		if (ImGui::IsKeyDown(ImGuiKey_Space) || ImGui::IsKeyReleased(ImGuiKey_N)) {
			UpdateRibbons(app);
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
	app->camera_push.segment_count = SEGMENT_COUNT;
	app->camera_push.index_offset = 0;
	app->camera_push.ribbon_count = app->ribbon_count;
	zest_uint total_segments = SEGMENT_COUNT * app->ribbon_count;
	app->index_count = 0;
	zest_StageData(app->ribbon_segments, app->ribbon_segment_staging_buffer[ZEST_FIF], SEGMENT_COUNT *RIBBON_COUNT * sizeof(ribbon_segment));
	app->index_count += (SEGMENT_COUNT * RIBBON_COUNT) * app->ribbon_buffer_info.indicesPerSegment;
	zest_StageData(app->ribbon_instances, app->ribbon_instance_staging_buffer[ZEST_FIF], app->ribbon_count * sizeof(ribbon_instance));

	zest_swapchain swapchain = zest_GetMainWindowSwapchain();

	app->segment_buffer_info.size = app->ribbon_segment_staging_buffer[ZEST_FIF]->memory_in_use;
	app->instance_buffer_info.size = app->ribbon_instance_staging_buffer[ZEST_FIF]->memory_in_use;
	app->vertex_buffer_info.size = app->ribbon_buffer_info.verticesPerSegment * total_segments * sizeof(ribbon_vertex);
	app->index_buffer_info.size = app->index_count * sizeof(zest_uint);

	if (zest_BeginFrameGraphSwapchain(swapchain, "Ribbons render graph", 0)) {
		//Resources
		zest_resource_node ribbon_segment_buffer = zest_AddTransientBufferResource("Ribbon Segment Buffer", &app->segment_buffer_info);
		zest_resource_node ribbon_instance_buffer = zest_AddTransientBufferResource("Ribbon Instance Buffer", &app->instance_buffer_info);
		zest_resource_node ribbon_vertex_buffer = zest_AddTransientBufferResource("Ribbon Vertex Buffer", &app->vertex_buffer_info);
		zest_resource_node ribbon_index_buffer = zest_AddTransientBufferResource("Ribbon Index Buffer", &app->index_buffer_info);

		//-------------------------Ribbon Compute Pass-------------------------------------------------
		zest_pass_node compute_pass = zest_BeginComputePass(app->ribbon_compute, "Compute Ribbons");
		//inputes
		zest_ConnectInput(compute_pass, ribbon_segment_buffer, 0);
		zest_ConnectInput(compute_pass, ribbon_instance_buffer, 0);
		//outputs
		zest_ConnectOutput(compute_pass, ribbon_vertex_buffer);
		zest_ConnectOutput(compute_pass, ribbon_index_buffer);
		//tasks
		zest_SetPassTask(compute_pass, RecordComputeCommands, app);
		//--------------------------------------------------------------------------------------------------

		//-------------------------TimelineFX Transfer Pass-------------------------------------------------
		zest_pass_node transfer_pass = zest_BeginTransferPass("Transfer Ribbon Data");
		//outputs
		zest_ConnectOutput(transfer_pass, ribbon_segment_buffer);
		zest_ConnectOutput(transfer_pass, ribbon_instance_buffer);
		//tasks
		zest_SetPassTask(transfer_pass, UploadRibbonData, app);
		//--------------------------------------------------------------------------------------------------

		//-------------------------TimelineFX Render Pass---------------------------------------------------
		zest_pass_node render_pass = zest_BeginRenderPass("Graphics Pass");
		//inputs
		zest_ConnectInput(render_pass, ribbon_vertex_buffer, 0);
		zest_ConnectInput(render_pass, ribbon_index_buffer, 0);
		//outputs
		zest_ConnectSwapChainOutput(render_pass);
		//tasks
		zest_SetPassTask(render_pass, RecordRibbonDrawing, app);
		//--------------------------------------------------------------------------------------------------

		//------------------------ ImGui Pass ----------------------------------------------------------------
		//If there's imgui to draw then draw it
		zest_pass_node imgui_pass = zest_imgui_BeginPass();
		if (imgui_pass) {
			zest_ConnectSwapChainOutput(imgui_pass);
		}
		//----------------------------------------------------------------------------------------------------

		zest_frame_graph render_graph = zest_EndFrameGraph();

		static bool print_graph = true;
		if (print_graph) {
			zest_PrintCompiledRenderGraph(render_graph);
			print_graph = false;
		}
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(zest_validation_flag_enable_sync);
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
	zest_Shutdown();

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
