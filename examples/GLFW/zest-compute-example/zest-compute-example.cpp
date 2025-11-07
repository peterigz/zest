#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include "zest-compute-example.h"
#include <zest.h>
#include "imgui_internal.h"
#include "impl_slang.hpp"
#include <random>

void InitComputeExample(ComputeExample *app) {

	//Initialise Imgui for zest, this function just sets up some things like display size and font texture
	app->imgui = zest_imgui_Initialise(app->context);
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)zest_Window(app->context), true);

	app->frame_timer = 1.f;
	app->timer = 0.f;
	app->anim_start = 20.f;
	app->attach_to_cursor = false;

	//Set up the compute shader example starts here
	//Prepare a couple of textures:
	//Particle image for point sprites
	zest_bitmap particle_bitmap = zest_LoadPNG(app->context, "examples/assets/particle.png");
	app->particle_image = zest_CreateImageWithBitmap(app->context, particle_bitmap, 0);
	//A gradient texture to sample the colour from
	zest_bitmap gradient_bitmap = zest_LoadPNG(app->context, "examples/assets/gradient.png");
	app->gradient_image = zest_CreateImageWithBitmap(app->context, gradient_bitmap, zest_image_preset_texture);

	zest_FreeBitmap(particle_bitmap);
	zest_FreeBitmap(gradient_bitmap);

	app->particle_image_index = zest_AcquireSampledImageIndex(app->context, app->particle_image, zest_texture_2d_binding);
	app->gradient_image_index = zest_AcquireSampledImageIndex(app->context, app->gradient_image, zest_texture_2d_binding);

	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	app->particle_sampler = zest_CreateSampler(app->context, &sampler_info);
	app->sampler_index = zest_AcquireSamplerIndex(app->context, app->particle_sampler);

	//Load the particle data with random coordinates
	std::default_random_engine rndEngine(0);
	std::uniform_real_distribution<float> rndDist(-1.0f, 1.0f);
	//Initial particle positions
	std::vector<Particle> particle_buffer(PARTICLE_COUNT);
	for (auto& particle : particle_buffer) {
		particle.pos = zest_Vec2Set(rndDist(rndEngine), rndDist(rndEngine));
		particle.vel = zest_Vec2Set1(0.0f);
		particle.gradient_pos.x = particle.pos.x / 2.0f;
	}

	VkDeviceSize storage_buffer_size = particle_buffer.size() * sizeof(Particle);

	//Create a temporary staging buffer to load the particle data into
	zest_buffer staging_buffer = zest_CreateStagingBuffer(app->context, storage_buffer_size,  particle_buffer.data());
	//Create a "Descriptor buffer". This is a buffer that will have info necessary for a shader - in this case a compute shader.
	//Create buffer as a single buffer, no need to have a buffer for each frame in flight as we won't be writing to it while it
	//might be used in the GPU, it's purely for updating by the compute shader only
	zest_buffer_info_t particle_vertex_buffer_info = zest_CreateBufferInfo(zest_buffer_type_vertex_storage, zest_memory_usage_gpu_only);
	app->particle_buffer = zest_CreateBuffer(app->context, storage_buffer_size, &particle_vertex_buffer_info);
	//Copy the staging buffer to the desciptor buffer
	zest_BeginImmediateCommandBuffer(app->context);
	zest_imm_CopyBuffer(app->context, staging_buffer, app->particle_buffer, storage_buffer_size);
	zest_EndImmediateCommandBuffer(app->context);
	//Free the staging buffer as we don't need it anymore
	zest_FreeBuffer(staging_buffer);

	app->particle_buffer_index = zest_AcquireStorageBufferIndex(app->context, app->particle_buffer);

	/*
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	zest_shader frag_shader = zest_CreateShaderFromFile("examples/GLFW/zest-compute-example/shaders/particle.frag", "particle_frag.spv", shaderc_fragment_shader, 1, compiler, 0);
	zest_shader vert_shader = zest_CreateShaderFromFile("examples/GLFW/zest-compute-example/shaders/particle.vert", "particle_vert.spv", shaderc_vertex_shader, 1, compiler, 0);
	zest_shader comp_shader = zest_CreateShaderFromFile("examples/GLFW/zest-compute-example/shaders/particle.comp", "particle_comp.spv", shaderc_compute_shader, 1, compiler, 0);
	shaderc_compiler_release(compiler);
	*/
	zest_slang_InitialiseSession(app->context);
	const char *path = "examples/GLFW/zest-compute-example/shaders/particle.slang";
	zest_shader_handle frag_shader = zest_slang_CreateShader(app->context, path, "particle_frag.spv", "fragmentMain", zest_fragment_shader, false);
	zest_shader_handle vert_shader = zest_slang_CreateShader(app->context, path, "particle_vert.spv", "vertexMain", zest_vertex_shader, false);
	zest_shader_handle comp_shader = zest_slang_CreateShader(app->context, path, "particle_comp.spv", "computeMain", zest_compute_shader, false);

	assert(frag_shader.value && vert_shader.value && comp_shader.value);

	//Create a new pipeline in the renderer based on an existing default one
	app->particle_pipeline = zest_BeginPipelineTemplate(app->context, "particles");
	//Add our own vertex binding description using the Particle struct
	zest_AddVertexInputBindingDescription(app->particle_pipeline, 0, sizeof(Particle), zest_input_rate_vertex);
	//Add the descriptions for each type in the Particle struct
	zest_AddVertexAttribute(app->particle_pipeline, 0, 0, zest_format_r32g32_sfloat, offsetof(Particle, pos));
	zest_AddVertexAttribute(app->particle_pipeline, 0, 1, zest_format_r32g32b32a32_sfloat, offsetof(Particle, gradient_pos));

	//Set the shader file to use in the pipeline
	zest_SetPipelineFragShader(app->particle_pipeline, frag_shader);
	zest_SetPipelineVertShader(app->particle_pipeline, vert_shader);
	//We're going to use point sprites so set that
	zest_SetPipelineTopology(app->particle_pipeline, zest_topology_point_list);
	//Add the descriptor layout we created earlier, but clear the layouts in the template first as a uniform buffer is added
	//by default.
	zest_ClearPipelineDescriptorLayouts(app->particle_pipeline);
	zest_AddPipelineDescriptorLayout(app->particle_pipeline, zest_GetBindlessLayout(app->context));
	//Set the push constant we'll be using
	zest_SetPipelinePushConstantRange(app->particle_pipeline, sizeof(ParticleFragmentPush), zest_shader_fragment_stage);
	//Switch off any depth testing
	zest_SetPipelineDepthTest(app->particle_pipeline, false, false);
	zest_SetPipelineBlend(app->particle_pipeline, zest_AdditiveBlendState2());
	zest_SetPipelinePolygonFillMode(app->particle_pipeline, zest_polygon_mode_fill);
	zest_SetPipelineCullMode(app->particle_pipeline, zest_cull_mode_none);
	zest_SetPipelineFrontFace(app->particle_pipeline, zest_front_face_counter_clockwise);

	//Setup a uniform buffer
	app->compute_uniform_buffer = zest_CreateUniformBuffer(app->context, "Compute Uniform", sizeof(ComputeUniformBuffer));

	//Set up the compute shader
	//A builder is used to simplify the compute shader setup process
	zest_compute_builder_t builder = zest_BeginComputeBuilder(app->context);
	//Declare the bindings we want in the shader
	zest_AddComputeSetLayout(&builder, zest_GetUniformBufferLayout(app->compute_uniform_buffer));
	zest_SetComputeBindlessLayout(&builder, zest_GetBindlessLayout(app->context));
	//Set the user data so that we can use it in the callback funcitons
	zest_SetComputeUserData(&builder, app);
	//Declare the actual shader to use
	zest_AddComputeShader(&builder, comp_shader);
	//Finally, make the compute shader using the builder
	app->compute = zest_FinishCompute(&builder, "Particles Compute");

	//Create a timer for a fixed update loop
	app->loop_timer = zest_CreateTimer(60.0);
}

void RecordComputeSprites(zest_command_list command_list, void *user_data) {
	//Grab the app object from the user_data that we set in the render graph when adding this function callback 
	ComputeExample *app = (ComputeExample*)user_data;
	//Get the pipeline from the template that we created. 
	zest_pipeline pipeline = zest_PipelineWithTemplate(app->particle_pipeline, command_list);
	//You can mix and match descriptor sets but we only need the bindless set there containing the particle texture and gradient
	zest_descriptor_set sets[] = {
		zest_GetBindlessSet(command_list->context)
	};
	//Bind the pipeline with the descriptor set
	zest_cmd_BindPipeline(command_list, pipeline, sets, 1);
	//The shader needs to know the indexes into the descriptor array for the textures so we use push constants to
	//do. You could also use a uniform buffer if you wanted.
	ParticleFragmentPush push;
	push.particle_index = app->particle_image_index;
	push.gradient_index = app->gradient_image_index;
	push.sampler_index = app->sampler_index;
	//Send the the push constant
	zest_cmd_SendPushConstants(command_list, pipeline, &push);
	//Set the viewport with this helper function
	zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);
	//Bind the vertex buffer with the particle buffer containing the location of all the point sprite particles
	zest_cmd_BindVertexBuffer(command_list, 0, 1, app->particle_buffer);
	//Draw the point sprites
	zest_cmd_Draw(command_list, PARTICLE_COUNT, 1, 0, 0);
}

void RecordComputeCommands(zest_command_list command_list, void *user_data) {
	//Grab the app object from the user_data that we set in the render graph when adding the compute pass task
	ComputeExample *app = (ComputeExample *)user_data;
	//Mix the bindless descriptor set with the uniform buffer descriptor set
	zest_descriptor_set sets[] = {
		zest_GetBindlessSet(command_list->context),
		zest_GetUniformBufferSet(app->compute_uniform_buffer)
	};
	//Bind the compute pipeline
	zest_cmd_BindComputePipeline(command_list, app->compute, sets, 2);
	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, app->compute, PARTICLE_COUNT / 256, 1, 1);
}

void UpdateComputeUniformBuffers(ComputeExample *app) {
	//Update our custom compute shader uniform buffer
	ComputeUniformBuffer *uniform = (ComputeUniformBuffer*)zest_GetUniformBufferData(app->compute_uniform_buffer);
	uniform->deltaT = app->frame_timer * 2.5f;
	uniform->particleCount = PARTICLE_COUNT;
	uniform->particle_buffer_index = app->particle_buffer->array_index;
	if (!app->attach_to_cursor) {
		uniform->dest_x = sinf(Radians(app->timer * 360.0f)) * 0.75f;
		uniform->dest_y = 0.0f;
	} else {
		float normalizedMx = (ImGui::GetMousePos().x - static_cast<float>(zest_ScreenWidthf(app->context) / 2)) / static_cast<float>(zest_ScreenWidthf(app->context) / 2);
		float normalizedMy = (ImGui::GetMousePos().y - static_cast<float>(zest_ScreenHeightf(app->context) / 2)) / static_cast<float>(zest_ScreenHeightf(app->context) / 2);
		uniform->dest_x = normalizedMx;
		uniform->dest_y = normalizedMy;
	}
}

void MainLoop(ComputeExample *app) {
	zest_microsecs running_time = zest_Microsecs();
	zest_microsecs frame_time = 0;
	zest_uint frame_count = 0;
	zest_uint fps = 0;

	zest_microsecs last_time = zest_Microsecs();
	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
		zest_microsecs elapsed = zest_Microsecs() - last_time;
		last_time = zest_Microsecs();

		zest_microsecs current_frame_time = zest_Microsecs() - running_time;
		running_time = zest_Microsecs();
		frame_time += current_frame_time;
		frame_count += 1;
		if (frame_time >= ZEST_MICROSECS_SECOND) {
			frame_time -= ZEST_MICROSECS_SECOND;
			fps = frame_count;
			frame_count = 0;
		}

		glfwPollEvents();
		//Don't forget to update the uniform buffer!
		UpdateComputeUniformBuffers(app);

		//Set timing variables that are used to update the uniform buffer
		app->frame_timer = (float)elapsed / ZEST_MICROSECS_SECOND;
		if (!app->attach_to_cursor)
		{
			if (app->anim_start > 0.0f)
			{
				app->anim_start -= app->frame_timer * 10.0f;
			}
			else if (app->anim_start <= 0.0f)
			{
				app->timer += app->frame_timer * 0.01f;
				if (app->timer > 1.f)
					app->timer = 0.f;
			}
		}

		zest_StartTimerLoop(app->loop_timer) {
			//Draw Imgui
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			ImGui::Begin("Test Window");
			ImGui::Text("FPS: %u", fps);
			ImGui::Text("Particle Count: %u", PARTICLE_COUNT);
			ImGui::Checkbox("Repel Mouse", &app->attach_to_cursor);
			if (ImGui::Button("Print Render Graph")) {
				app->request_graph_print = true;
			}
			ImGui::End();
			ImGui::Render();
			zest_imgui_UpdateBuffers(&app->imgui);
		} zest_EndTimerLoop(app->loop_timer)

		app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw();
		zest_frame_graph_cache_key_t cache_key = {};
		cache_key = zest_InitialiseCacheKey(app->context, &app->cache_info, sizeof(RenderCacheInfo));

		if (zest_BeginFrame(app->context)) {
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);

			if (!frame_graph) {
				if (zest_BeginFrameGraph(app->context, "Compute Particles", &cache_key)) {
					//Resources
					zest_resource_node particle_buffer = zest_ImportBufferResource("particle buffer", app->particle_buffer, 0);
					zest_resource_node swapchain_node = zest_ImportSwapchainResource();

					//---------------------------------Compute Pass-----------------------------------------------------
					zest_pass_node compute_pass = zest_BeginComputePass(app->compute, "Compute Particles"); {
						zest_ConnectInput(particle_buffer);
						zest_ConnectOutput(particle_buffer);
						zest_SetPassTask(RecordComputeCommands, app);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//---------------------------------Render Pass------------------------------------------------------
					zest_pass_node render_pass = zest_BeginRenderPass("Graphics Pass"); {
						zest_ConnectInput(particle_buffer);
						zest_ConnectSwapChainOutput();
						zest_SetPassTask(RecordComputeSprites, app);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//------------------------ ImGui Pass ----------------------------------------------------------------
					//If there's imgui to draw then draw it
					zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui); {
						if (imgui_pass) {
							zest_ConnectSwapChainOutput();
							zest_EndPass();
						}
					}
					//----------------------------------------------------------------------------------------------------

					frame_graph = zest_EndFrameGraph();
					zest_QueueFrameGraphForExecution(app->context, frame_graph);
				}
			} else {
				zest_QueueFrameGraphForExecution(app->context, frame_graph);
			}
			if (app->request_graph_print) {
				zest_PrintCompiledFrameGraph(frame_graph);
				app->request_graph_print = false;
			}
			zest_EndFrame(app->context);
		}
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	//Disable vsync so we can see how fast it runs
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);

	ComputeExample compute_example = { 0 };

	zest_device device = zest_implglfw_CreateDevice(true);

	//Create a window using GLFW
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");
	//Initialise Zest
	compute_example.context = zest_CreateContext(device, &window_handles, &create_info);

	//Initialise Dear ImGui
	InitComputeExample(&compute_example);

	//Start the mainloop in Zest
	MainLoop(&compute_example);
	zest_slang_Shutdown(compute_example.context);
	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Destroy(&compute_example.imgui);
	zest_DestroyContext(compute_example.context);

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	zest_implglfw_SetCallbacks(&create_info);

	ComputeExample imgui_app;

	zest_CreateContext(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#endif
