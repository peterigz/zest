#include "zest-compute-example.h"
#include "imgui_internal.h"
#include <random>

void InitImGuiApp(ImGuiApp *app) {

	//Initialise Imgui for zest, this function just sets up some things like display size and font texture
	app->imgui_info = zest_imgui_Initialise();

	app->frame_timer = 1.f;
	app->timer = 0.f;
	app->anim_start = 20.f;
	app->attach_to_cursor = false;

	//Set up the compute shader example starts here
	//Prepare a couple of textures:
	//Particle image for point sprites
	app->particle_texture = zest_CreateTextureSingle("particle", zest_texture_format_rgba_unorm);
	app->particle_texture->image_view_type = VK_IMAGE_VIEW_TYPE_2D;
	zest_AddTextureImageFile(app->particle_texture, "examples/assets/particle.png");
	zest_ProcessTextureImages(app->particle_texture);
	//A gradient texture to sample the colour from
	app->gradient_texture = zest_CreateTextureSingle("gradient", zest_texture_format_rgba_unorm);
	app->gradient_texture->image_view_type = VK_IMAGE_VIEW_TYPE_2D;
	zest_AddTextureImageFile(app->gradient_texture, "examples/assets/gradient.png");
	zest_ProcessTextureImages(app->gradient_texture);

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
	zest_buffer staging_buffer = zest_CreateStagingBuffer(storage_buffer_size,  particle_buffer.data(), 0);
	//Create a "Descriptor buffer". This is a buffer that will have info necessary for a shader - in this case a compute shader.
	//Create buffer as a single buffer, no need to have a buffer for each frame in flight as we won't be writing to it while it
	//might be used in the GPU, it's purely for updating by the compute shader only
	app->particle_buffer = zest_CreateComputeVertexDescriptorBuffer(storage_buffer_size);
	//Copy the staging buffer to the desciptor buffer
	zest_CopyBuffer(staging_buffer, zest_GetBufferFromDescriptorBuffer(app->particle_buffer), storage_buffer_size);
	//Free the staging buffer as we don't need it anymore
	zest_FreeBuffer(staging_buffer);

	//We'll need to create our own descriptor layout for the vertex and fragment shaders that can 
	//sample from both textures
	zest_descriptor_set_layout_builder_t layout_builder = zest_BeginDescriptorSetLayoutBuilder();
	zest_AddLayoutBuilderCombinedImageSamplerBindless(&layout_builder, 0, 10);
	zest_AddLayoutBuilderStorageBufferBindless(&layout_builder, 1, 10, VK_SHADER_STAGE_COMPUTE_BIT);
	app->descriptor_layout = zest_FinishDescriptorSetLayoutForBindless(&layout_builder, 1, 0, "Particles descriptor layout");
	app->descriptor_set = zest_CreateBindlessSet(app->descriptor_layout);

	zest_AssignBindlessStorageBufferIndex(app->particle_buffer, app->descriptor_layout, &app->descriptor_set, 1);
	zest_AssignBindlessTextureIndex(app->particle_texture, app->descriptor_layout, &app->descriptor_set, 0);
	zest_AssignBindlessTextureIndex(app->gradient_texture, app->descriptor_layout, &app->descriptor_set, 0);

	//Create a new pipeline in the renderer based on an existing default one
	app->particle_pipeline = zest_CopyPipelineTemplate("particles", zest_PipelineTemplate("pipeline_2d_sprites"));

	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	zest_shader frag_shader = zest_CreateShaderFromFile("examples/GLFW/zest-compute-example/shaders/particle.frag", "particle_frag.spv", shaderc_fragment_shader, 1, compiler, 0);
	zest_shader vert_shader = zest_CreateShaderFromFile("examples/GLFW/zest-compute-example/shaders/particle.vert", "particle_vert.spv", shaderc_vertex_shader, 1, compiler, 0);
	zest_shader comp_shader = zest_CreateShaderFromFile("examples/GLFW/zest-compute-example/shaders/particle.comp", "particle_comp.spv", shaderc_compute_shader, 1, compiler, 0);
	shaderc_compiler_release(compiler);

	//Change some things in the create info to set up our own pipeline
	//Clear the current vertex input binding descriptions and attribute descriptions
	zest_ClearVertexInputBindingDescriptions(app->particle_pipeline);
	zest_ClearVertexAttributeDescriptions(app->particle_pipeline);
	//Add our own vertex binding description using the Particle struct
	zest_AddVertexInputBindingDescription(app->particle_pipeline, 0, sizeof(Particle), VK_VERTEX_INPUT_RATE_VERTEX);
	//Add the descriptions for each type in the Particle struct
	zest_AddVertexInputDescription(app->particle_pipeline, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Particle, pos)));
	zest_AddVertexInputDescription(app->particle_pipeline, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Particle, gradient_pos)));

	//Set the shader file to use in the pipeline
	zest_SetPipelineTemplateFragShader(app->particle_pipeline, "particle_frag.spv", 0);
	zest_SetPipelineTemplateVertShader(app->particle_pipeline, "particle_vert.spv", 0);
	//We're going to use point sprites so set that
	app->particle_pipeline->topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	//Add the descriptor layout we created earlier, but clear the layouts in the template first as a uniform buffer is added
	//by default.
	zest_ClearPipelineTemplateDescriptorLayouts(app->particle_pipeline);
	zest_AddPipelineTemplateDescriptorLayout(app->particle_pipeline, app->descriptor_layout->vk_layout);
	//Set the push constant we'll be using
	zest_SetPipelineTemplatePushConstantRange(app->particle_pipeline, sizeof(ParticleFragmentPush), 0, VK_SHADER_STAGE_FRAGMENT_BIT);

	//Using the create_info we prepared build the template for the pipeline
	zest_FinalisePipelineTemplate(app->particle_pipeline);
	//Alter a few things in the template to tweak it to how we want
	app->particle_pipeline->rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	app->particle_pipeline->rasterizer.cullMode = VK_CULL_MODE_NONE;
	app->particle_pipeline->rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	app->particle_pipeline->colorBlendAttachment = zest_AdditiveBlendState2();
	app->particle_pipeline->depthStencil.depthWriteEnable = VK_FALSE;
	app->particle_pipeline->depthStencil.depthTestEnable = VK_FALSE;

	//Setup a uniform buffer
	zest_ForEachFrameInFlight(fif) {
		app->compute_uniform_buffer[fif] = zest_CreateUniformBuffer(sizeof(ComputeUniformBuffer));
	}

	zest_descriptor_set_layout_builder_t uniform_layout_builder = zest_BeginDescriptorSetLayoutBuilder();
	zest_AddLayoutBuilderUniformBuffer(&uniform_layout_builder, 0, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	app->uniform_layout = zest_FinishDescriptorSetLayout(&uniform_layout_builder, "Particles Compute Uniform");
	zest_CreateDescriptorPoolForLayout(app->uniform_layout, ZEST_MAX_FIF, 0);

	zest_ForEachFrameInFlight(fif) {
		zest_descriptor_set_builder_t uniform_set_builder = zest_BeginDescriptorSetBuilder(app->uniform_layout);
		zest_AddSetBuilderUniformBuffer(&uniform_set_builder, 0, 0, app->compute_uniform_buffer[fif]);
		app->uniform_set[fif].vk_descriptor_set = zest_FinishDescriptorSet(app->uniform_layout->pool, &uniform_set_builder, app->uniform_set[fif].vk_descriptor_set);
	}

	app->shader_resources = zest_CreateShaderResources();
	zest_ForEachFrameInFlight(fif) {
		zest_AddDescriptorSetToResources(app->shader_resources, &app->descriptor_set, fif);
	}
	zest_ValidateShaderResource(app->shader_resources);

	//Set up the compute shader
	//A builder is used to simplify the compute shader setup process
	zest_compute_builder_t builder = zest_BeginComputeBuilder();
	//Declare the bindings we want in the shader
	zest_AddComputeSetLayout(&builder, app->uniform_layout);
	zest_SetComputeBindlessLayout(&builder, app->descriptor_layout);
	//Set the user data so that we can use it in the callback funcitons
	zest_SetComputeUserData(&builder, app);
	//Declare the actual shader to use
	zest_AddComputeShader(&builder, comp_shader);
	//Finally, make the compute shader using the builder
	app->compute = zest_FinishCompute(&builder, "Particles Compute");

	app->render_graph = zest_NewRenderGraph("Compute Particles", app->descriptor_layout, false);

	//Create a timer for a fixed update loop
	app->loop_timer = zest_CreateTimer(60.0);
}

void RecordComputeSprites(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	ImGuiApp *app = (ImGuiApp*)user_data;
	//We can make use of helper functions to easily bind the pipeline/vertex buffer and make the draw call
	//to draw the sprites
	zest_pipeline pipeline = zest_PipelineWithTemplate(app->particle_pipeline, context->render_pass);
	VkDescriptorSet sets[] = {
		app->descriptor_set.vk_descriptor_set
	};
	zest_BindPipeline(command_buffer, pipeline, sets, 1);
	ParticleFragmentPush push;
	push.particle_index = app->particle_texture->descriptor_array_index;
	push.gradient_index = app->gradient_texture->descriptor_array_index;
	zest_SendPushConstants(command_buffer, pipeline, &push);
    VkViewport view = zest_CreateViewport(0.f, 0.f, zest_ScreenWidthf(), zest_ScreenHeightf(), 0.f, 1.f);
    VkRect2D scissor = zest_CreateRect2D(zest_ScreenWidth(), zest_ScreenHeight(), 0, 0);
    vkCmdSetViewport(command_buffer, 0, 1, &view);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
	zest_BindVertexBuffer(command_buffer, app->particle_buffer->buffer);
	zest_Draw(command_buffer, PARTICLE_COUNT, 1, 0, 0);
}

int DrawComputeSpritesCondition(zest_draw_routine draw_routine) {
	//We can just always excute the draw commands to draw the particles
	return 1;
}

int ComputeCondition(zest_compute compute_commands) {
	return 1;
}

void RecordComputeCommands(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	//The compute queue item can contain more then one compute shader to be dispatched
	//There's only one in this example though
	//Bind the compute pipeline
	ImGuiApp *app = (ImGuiApp *)user_data;
	zest_compute compute = app->compute;
	VkDescriptorSet sets[] = {
		app->descriptor_set.vk_descriptor_set,
		app->uniform_set[ZEST_FIF].vk_descriptor_set
	};
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipeline_layout, 0, 2, sets, 0, 0);
	zest_DispatchComputeCB(command_buffer, compute, PARTICLE_COUNT / 256, 1, 1);
}

void UpdateComputeUniformBuffers(ImGuiApp *app) {
	//Update our custom compute shader uniform buffer
	ComputeUniformBuffer *uniform = (ComputeUniformBuffer*)zest_GetUniformBufferData(app->compute_uniform_buffer[ZEST_FIF]);
	uniform->deltaT = app->frame_timer * 2.5f;
	uniform->particleCount = PARTICLE_COUNT;
	uniform->particle_buffer_index = app->particle_buffer->descriptor_array_index;
	if (!app->attach_to_cursor) {
		uniform->dest_x = sinf(Radians(app->timer * 360.0f)) * 0.75f;
		uniform->dest_y = 0.0f;
	} else {
		float normalizedMx = (ImGui::GetMousePos().x - static_cast<float>(zest_ScreenWidthf() / 2)) / static_cast<float>(zest_ScreenWidthf() / 2);
		float normalizedMy = (ImGui::GetMousePos().y - static_cast<float>(zest_ScreenHeightf() / 2)) / static_cast<float>(zest_ScreenHeightf() / 2);
		uniform->dest_x = normalizedMx;
		uniform->dest_y = normalizedMy;
	}
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	ImGuiApp *app = (ImGuiApp*)user_data;
	//Don't forget to update the uniform buffer!
	UpdateComputeUniformBuffers(app);

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

	zest_StartTimerLoop(app->loop_timer) {
		//Draw Imgui
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGui::Begin("Test Window");
		ImGui::Text("FPS %u", ZestApp->last_fps);
		ImGui::Text("Particle Count: %u", PARTICLE_COUNT);
		ImGui::Checkbox("Repel Mouse", &app->attach_to_cursor);
		if (ImGui::Button("Print Render Graph")) {
			app->request_graph_print = true;
		}
		ImGui::End();
		ImGui::Render();
		zest_imgui_UpdateBuffers();
	} zest_EndTimerLoop(app->loop_timer)

	if (zest_BeginRenderToScreen(app->render_graph)) {
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		zest_resource_node swapchain_output_resource = zest_ImportSwapChainResource(app->render_graph, "Swapchain Output");
		zest_pass_node imgui_pass = zest_imgui_AddToRenderGraph(app->render_graph);
		if (imgui_pass) {
			zest_resource_node particle_buffer = zest_ImportStorageBufferResource(app->render_graph, "particle buffer", app->particle_buffer);
			zest_pass_node compute_pass = zest_AddComputePassNode(app->render_graph, app->compute, "Compute Particles");
			zest_AddPassStorageBufferWrite(compute_pass, particle_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			zest_AddPassVertexBufferInput(imgui_pass, particle_buffer);
			zest_AddPassTask(compute_pass, RecordComputeCommands, app);
			zest_AddPassTask(imgui_pass, RecordComputeSprites, app);
			zest_AddPassTask(imgui_pass, zest_imgui_DrawImGuiRenderPass, app);
			zest_AddPassSwapChainOutput(imgui_pass, swapchain_output_resource, clear_color);
		} else {
			//Just render a blank screen if imgui didn't render anything
			zest_pass_node blank_pass = zest_AddGraphicBlankScreen(app->render_graph, "Draw Nothing");
			zest_AddPassSwapChainOutput(blank_pass, swapchain_output_resource, clear_color);
		}

		zest_EndRenderGraph(app->render_graph);
		if (app->request_graph_print) {
			zest_PrintCompiledRenderGraph(app->render_graph);
			app->request_graph_print = false;
		}
		zest_ExecuteRenderGraph(app->render_graph);
	}

}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers();
	//Disable vsync so we can see how fast it runs
	ZEST__FLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	create_info.log_path = ".";
	create_info.color_format = VK_FORMAT_B8G8R8A8_UNORM;
	//We're using GLFW for this example so use the following function to set that up for us. You must include
	//impl_glfw.h for this
	zest_implglfw_SetCallbacks(&create_info);

	ImGuiApp imgui_app = { 0 };

	//Initialise Zest
	zest_Initialise(&create_info);
	//Set our user data and the update callback to be called every frame
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	//Initialise Dear ImGui
	InitImGuiApp(&imgui_app);

	//Start the mainloop in Zest
	zest_Start();

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	zest_implglfw_SetCallbacks(&create_info);

	ImGuiApp imgui_app;

	zest_Initialise(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#endif
