#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_IMAGES_IMPLEMENTATION
#include "zest-compute-example.h"
#include <zest.h>
#include "imgui_internal.h"
#include "impl_slang.hpp"
#include <random>

/*
Example recreated from Sascha Willems "Attraction based compute shader particle system" 
https://github.com/SaschaWillems/Vulkan/tree/master/examples/computeparticles

This example shows how to use the frame graph to dispatch a compute shader to update particles and then
render them in a render pass.
*/

void InitComputeExample(ComputeExample *app) {

	//Initialise Imgui for zest, this function just sets up some things like display size and font texture
	zest_imgui_Initialise(app->context, &app->imgui, zest_implglfw_DestroyWindow);
	//Call the appropriate imgui implementation for vulkan which is the rendering API we're using
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)zest_Window(app->context), true);

	//Initialise some timing variables
	app->frame_timer = 1.f;
	app->timer = 0.f;
	app->anim_start = 20.f;
	app->attach_to_cursor = false;

	//Prepare a couple of textures:
	//Particle image for point sprites. Use stb_image libarry to load the images we need
	int width, height, channels;
	stbi_uc *particle_pixels = stbi_load("examples/assets/particle.png", &width, &height, &channels, 0);
	int size = width * height * channels;
	//Every image you create needs an image info to define the properties of the image.
	zest_image_info_t image_info = zest_CreateImageInfo(width, height);
	//Use this flag to generate an image with mip maps.
	image_info.flags = zest_image_preset_texture_mipmaps;
	//Create the image with the pixel data we got from the stbi_load function and store the handle to the image
	app->particle_image = zest_CreateImageWithPixels(app->device, particle_pixels, size, &image_info);
	//Do the same to load a gradient texture to sample the colour of particles from
	stbi_uc *gradient_pixels = stbi_load("examples/assets/gradient.png", &width, &height, &channels, 0);
	size = width * height * channels;
	image_info = zest_CreateImageInfo(width, height);
	//This png happens to be in srgb format so specify that here to makes sure the image has the correct colorspace.
	image_info.format = zest_format_r8g8b8a8_srgb;
	image_info.flags = zest_image_preset_texture;
	app->gradient_image = zest_CreateImageWithPixels(app->device, gradient_pixels, size, &image_info);

	//We don't need the pixel data now.
	STBI_FREE(particle_pixels);
	STBI_FREE(gradient_pixels);

	//In order to reference the correct textures in the shader, acquire an index for each image. These are flat 2d textures so make
	//sure to use the relevent binding number (zest_texture_2d_binding)
	app->particle_image_index = zest_AcquireSampledImageIndex(app->device, zest_GetImage(app->particle_image), zest_texture_2d_binding);
	app->gradient_image_index = zest_AcquireSampledImageIndex(app->device, zest_GetImage(app->gradient_image), zest_texture_2d_binding);

	//Create a sampler to sample the textures with in the fragment shader. Like the image creation we need to
	//create a sampler info and modify the properties to create the sampler that we want.
	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	sampler_info.address_mode_u = zest_sampler_address_mode_repeat;
	sampler_info.address_mode_v = zest_sampler_address_mode_repeat;
	sampler_info.address_mode_w = zest_sampler_address_mode_repeat;
    sampler_info.compare_op = zest_compare_op_never;
	sampler_info.border_color = zest_border_color_float_opaque_white;
	//Create the sampler using the sampler_info.
	app->particle_sampler = zest_CreateSampler(app->context, &sampler_info);
	//And we also need a sampler index for the sampler as well so that we can look it up in the shader.
	app->sampler_index = zest_AcquireSamplerIndex(app->device, zest_GetSampler(app->particle_sampler));

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

	zest_size storage_buffer_size = particle_buffer.size() * sizeof(Particle);

	//Create a temporary staging buffer to load the particle data into. We can passin the pointer to the particle data
	//and the function will automatically copy the data to the staging buffer for us.
	zest_buffer staging_buffer = zest_CreateStagingBuffer(app->device, storage_buffer_size,  particle_buffer.data());

	//Now we need to create the buffer that will live on the GPU. Again we need a buffer create info to configure the
	//type of buffer that we want, in this case a vertex storage buffer which will allow us to use on the compute and 
	//graphics pipeliens
	zest_buffer_info_t particle_vertex_buffer_info = zest_CreateBufferInfo(zest_buffer_type_vertex_storage, zest_memory_usage_gpu_only);
	//Create the buffer with the info and the same size as the staging buffer.
	app->particle_buffer = zest_CreateBuffer(app->device, storage_buffer_size, &particle_vertex_buffer_info);
	//We'll need a descriptor index to access the buffer in the compute shader.
	app->particle_buffer_index = zest_AcquireStorageBufferIndex(app->device, app->particle_buffer);
	//Copy the staging buffer to the gpu bound particle buffer using an "immediate" command buffer. It doesn't *have* to be
	//on the transfer queue but we specify it for good practice
	zest_queue queue = zest_imm_BeginCommandBuffer(app->device, zest_queue_transfer);
	zest_imm_CopyBuffer(queue, staging_buffer, app->particle_buffer, storage_buffer_size);
	zest_imm_EndCommandBuffer(queue);
	//When zest_imm_EndCommandBuffer is called it waits until the work is done so it's safe to free any buffers that
	//were used.
	//Free the staging buffer as we don't need it anymore
	zest_FreeBuffer(staging_buffer);

	//The shaders for this example are written in slang so we initialise that here.
	zest_slang_InitialiseSession(app->device);
	const char *path = "examples/GLFW/zest-compute-example/shaders/particle.slang";
	zest_shader_handle frag_shader = zest_slang_CreateShader(app->device, path, "particle_frag.spv", "fragmentMain", zest_fragment_shader, true);
	zest_shader_handle vert_shader = zest_slang_CreateShader(app->device, path, "particle_vert.spv", "vertexMain", zest_vertex_shader, true);
	zest_shader_handle comp_shader = zest_slang_CreateShader(app->device, path, "particle_comp.spv", "computeMain", zest_compute_shader, true);

	//Assert that they all loaded and compiled ok. If the value in a handle is 0 then the item could not be created.

	assert(frag_shader.value && vert_shader.value && comp_shader.value);

	//Setup a uniform buffer for transferring per frame data
	app->compute_uniform_buffer = zest_CreateUniformBuffer(app->context, "Compute Uniform", sizeof(ComputeUniformBuffer));

	//Create a new pipeline in the renderer for drawing the particles
	app->particle_pipeline = zest_CreatePipelineTemplate(app->device, "particles");
	//Once it's created you can begin configuring it:
	//Add our own vertex binding description using the Particle struct
	zest_AddVertexInputBindingDescription(app->particle_pipeline, 0, sizeof(Particle), zest_input_rate_vertex);
	//Add the attribute descriptions for each type in the Particle struct. These will be locations in the vertex shader
	zest_AddVertexAttribute(app->particle_pipeline, 0, 0, zest_format_r32g32_sfloat, offsetof(Particle, pos));
	zest_AddVertexAttribute(app->particle_pipeline, 0, 1, zest_format_r32g32b32a32_sfloat, offsetof(Particle, gradient_pos));

	//Set the shaders to use in the pipeline
	zest_SetPipelineFragShader(app->particle_pipeline, frag_shader);
	zest_SetPipelineVertShader(app->particle_pipeline, vert_shader);
	//Set the remaining pipeline configuration that we need to draw point sprites
	zest_SetPipelineTopology(app->particle_pipeline, zest_topology_point_list);
	zest_SetPipelineBlend(app->particle_pipeline, zest_AdditiveBlendState2());
	zest_SetPipelinePolygonFillMode(app->particle_pipeline, zest_polygon_mode_fill);
	zest_SetPipelineCullMode(app->particle_pipeline, zest_cull_mode_none);
	zest_SetPipelineFrontFace(app->particle_pipeline, zest_front_face_counter_clockwise);

	//Creating a compute shader pipeline is a simple single command:
	app->compute = zest_CreateCompute(app->device, "Particles Compute", comp_shader);

	//Create a timer for a fixed update loop for the UI.
	app->loop_timer = zest_CreateTimer(60.0);
}

void RecordComputeSprites(zest_command_list command_list, void *user_data) {
	//This is the callback for the render pass in the frame graph that we setup.
	//Grab the app object from the user_data that we set in the frame graph when adding this function callback 
	ComputeExample *app = (ComputeExample*)user_data;
	//Get the pipeline from the template that we created. This will compile and cache the pipeline if it hasn't
	//been already. Otherwise it will just fetch the cached pipeline.
	zest_pipeline pipeline = zest_PipelineWithTemplate(app->particle_pipeline, command_list);
	//Bind the pipeline 
	zest_cmd_BindPipeline(command_list, pipeline);
	//The shader needs to know the indexes into the descriptor array for the textures so we use push constants to
	//do so. You could also use a uniform buffer if you wanted.
	ParticlePushConsts push;
	push.particle_index = app->particle_image_index;
	push.gradient_index = app->gradient_image_index;
	push.sampler_index = app->sampler_index;
	//Send the push constant
	zest_cmd_SendPushConstants(command_list, &push, sizeof(ParticlePushConsts));
	//Set the viewport with this helper function. Pipelines are created with dynamic viewports by default so you
	//must always set the view port for each draw call.
	zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);
	//Bind the vertex buffer with the particle buffer containing the location of all the point sprite particles
	zest_cmd_BindVertexBuffer(command_list, 0, 1, app->particle_buffer);
	//Draw the point sprites
	zest_cmd_Draw(command_list, PARTICLE_COUNT, 1, 0, 0);
}

void RecordComputeCommands(zest_command_list command_list, void *user_data) {
	//This is the callback for the compute pass in teh frame graph that we setup.
	//Grab the app object from the user_data that we set in the render graph when adding the compute pass task
	ComputeExample *app = (ComputeExample *)user_data;
	//Bind the compute pipeline. First get the compute pointer from the handle.
	zest_compute compute = zest_GetCompute(app->compute);
	zest_cmd_BindComputePipeline(command_list, compute);

	//We need to use the uniform buffer in the compute shader so we need to get the descriptor index for it. 
	//Every uniform buffer has one buffer for each frame in flight so you must get the current descriptor index
	//for the current frame in flight of the uniform buffer.
	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(app->compute_uniform_buffer);
	ParticlePushConsts push;
	push.uniform_index = zest_GetUniformBufferDescriptorIndex(uniform_buffer);
	zest_cmd_SendPushConstants(command_list, &push, sizeof(ParticlePushConsts));
	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, PARTICLE_COUNT / 256, 1, 1);
}

void UpdateComputeUniformBuffers(ComputeExample *app) {
	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(app->compute_uniform_buffer);
	//Update our custom compute shader uniform buffer
	//Get a pointer to the mapped memory of the uniform buffer.
	ComputeUniformBuffer *uniform = (ComputeUniformBuffer*)zest_GetUniformBufferData(uniform_buffer);
	//Then we can just modify the values and they'll be visible to the compute shader when reads from the buffer
	uniform->deltaT = app->frame_timer * 2.5f;
	uniform->particleCount = PARTICLE_COUNT;
	uniform->read_particle_buffer_index = app->particle_buffer_index;
	uniform->write_particle_buffer_index = app->particle_buffer_index;
	if (!app->attach_to_cursor) {
		uniform->dest_x = sinf(Radians(app->timer * 360.0f)) * 0.75f;
		uniform->dest_y = 0.0f;
	} else {
		float normalizedMx = (app->mouse_x - static_cast<float>(zest_ScreenWidthf(app->context) / 2)) / static_cast<float>(zest_ScreenWidthf(app->context) / 2);
		float normalizedMy = (app->mouse_y - static_cast<float>(zest_ScreenHeightf(app->context) / 2)) / static_cast<float>(zest_ScreenHeightf(app->context) / 2);
		uniform->dest_x = normalizedMx;
		uniform->dest_y = normalizedMy;
	}
}

void UpdateMouse(ComputeExample *app) {
	double mouse_x, mouse_y;
	GLFWwindow *handle = (GLFWwindow *)zest_Window(app->context);
	glfwGetCursorPos(handle, &mouse_x, &mouse_y);
	app->mouse_x = mouse_x;
	app->mouse_y = mouse_y;
}

void MainLoop(ComputeExample *app) {
	zest_microsecs running_time = zest_Microsecs();
	zest_microsecs frame_time = 0;
	zest_uint frame_count = 0;
	zest_uint fps = 0;

	zest_microsecs last_time = zest_Microsecs();
	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
		//Update timings
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

		//This function must be called every frame before zest_BeginFrame
		zest_UpdateDevice(app->device);

		glfwPollEvents();

		//Set timing variables that are used to update the uniform buffer
		app->frame_timer = (float)elapsed / ZEST_MICROSECS_SECOND;
		if (!app->attach_to_cursor)
		{
			if (app->anim_start > 0.0f)
			{
				app->anim_start -= app->frame_timer * 5.0f;
			}
			else if (app->anim_start <= 0.0f)
			{
				app->timer += app->frame_timer * 0.04f;
				if (app->timer > 1.f)
					app->timer = 0.f;
			}
		}

		//A fixed timestep update loop to update the UI at 60fps and save a few clock cycles
		//Not strictly necessary but shows how you can make use of timers this way.
		zest_StartTimerLoop(app->loop_timer) {
			//Draw Imgui
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			ImGui::Begin("Test Window");
			ImGui::Text("FPS: %u", fps);
			ImGui::Text("Particle Count: %u", PARTICLE_COUNT);
			ImGui::Checkbox("Repel Mouse", &app->attach_to_cursor);
			ImGui::End();
			ImGui::Render();
		} zest_EndTimerLoop(app->loop_timer)

		//Every frame graph can be cached with a cache key. At a basic level the cache key is aware of the swap chain
		//state and will invalidate the cache if the swap chain is recreated (if the window size changes for example)
		//Otherwise you can extend the cache info with your own data to decide whether a frame graph should be invalidated.
		//This might be because a game state changed. In this case the frame graph might change if there's no imgui
		//to draw so we check for that.
		app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw(&app->imgui);
		zest_frame_graph_cache_key_t cache_key = {};
		//Create the cache key that is passed into zest_BeginFrameGraph
		cache_key = zest_InitialiseCacheKey(app->context, &app->cache_info, sizeof(RenderCacheInfo));

		//Begin a new frame. This function is essential if you want to draw anything to the window. Will return false
		//if the swapchain had to be recreated.
		if (zest_BeginFrame(app->context)) {
			//See if the frame graph is cached. Returns NULL if not.
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);

			UpdateMouse(app);

			//Don't forget to update the uniform buffer! And also update it after BeginFrame so you have the
			//correct frame in flight index if relying on that.
			UpdateComputeUniformBuffers(app);

			//Set the window/swapchain clear color to black
			zest_SetSwapchainClearColor(app->context, 0, 0, 0, 0);

			if (!frame_graph) {
				//No frame found in the cache so build it
				if (zest_BeginFrameGraph(app->context, "Compute Particles", &cache_key)) {
					//Resources
					//Import the particle buffer that we created an populated with all the particles
					zest_resource_node particle_buffer = zest_ImportBufferResource("read particle buffer", app->particle_buffer, 0);
					//Import the swapchain so we can output to it.
					zest_resource_node swapchain_node = zest_ImportSwapchainResource();
					//Get the compute pointer from the handle
					zest_compute compute = zest_GetCompute(app->compute);

					//---------------------------------Compute Pass-----------------------------------------------------
					zest_BeginComputePass(compute, "Compute Particles"); {
						//Connect the particle buffer as input and output as the compute shader will read and write
						//from/to it
						zest_ConnectInput(particle_buffer);
						zest_ConnectOutput(particle_buffer);
						//Set the pass task to the callback function. This is called when the frame graph is executed
						zest_SetPassTask(RecordComputeCommands, app);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//---------------------------------Render Pass------------------------------------------------------
					zest_BeginRenderPass("Graphics Pass"); {
						//Connect the particle buffer as input. This will create a dependency chain with the compute pass
						zest_ConnectInput(particle_buffer);
						zest_ConnectSwapChainOutput();
						zest_SetPassTask(RecordComputeSprites, app);
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//------------------------ ImGui Pass --------------------------------------------------------------
					//If there's imgui to draw then draw it
					zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui, app->imgui.main_viewport); {
						if (imgui_pass) {
							zest_ConnectSwapChainOutput();
							zest_EndPass();
						}
					}
					//---------------------------------------------------------------------------------------------------
					//You must call zest_EndFrameGraph to finalise and compile the graph, ready to be executed. If it
					//compiled ok without errors then it will also be cached (assuming a cache_key was used);
					frame_graph = zest_EndFrameGraph();
				}
			}
			//Queue the frame graph for execution
			zest_QueueFrameGraphForExecution(app->context, frame_graph);

			//Execute pending frame graphs and present to the swapchain.
			zest_EndFrame(app->context);
		}
	}
}

int main(void) {
	ComputeExample compute_example = { 0 };

	//Create a device using a helper function for GLFW.
	compute_example.device = zest_implglfw_CreateDevice(false);

	//Create a window using GLFW
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "Compute Particles Example");

	//Initialise a Zest context
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	compute_example.context = zest_CreateContext(compute_example.device, &window_handles, &create_info);

	//Initialise Dear ImGui
	InitComputeExample(&compute_example);

	//Start the mainloop in Zest
	MainLoop(&compute_example);
	//Properly shutdown things to clear memory. Zest will warn of potential memory leaks if it finds un-freed blocks
	zest_slang_Shutdown(compute_example.device);
	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Destroy(&compute_example.imgui);
	//Note: Destroying the device will also cleanup all contexts as well
	zest_DestroyDevice(compute_example.device);

	return 0;
}
