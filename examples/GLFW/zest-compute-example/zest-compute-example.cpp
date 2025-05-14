#include "zest-compute-example.h"
#include "imgui_internal.h"
#include <random>

void InitImGuiApp(ImGuiApp *app) {

	//Initialise Imgui for zest, this function just sets up some things like display size and font texture
	zest_imgui_Initialise(&app->imgui_layer_info);

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

	app->descriptor_pool = zest_CreateDescriptorPool(1);
	zest_AddDescriptorPoolSize(app->descriptor_pool, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	zest_BuildDescriptorPool(app->descriptor_pool);
	//We'll need to create our own descriptor layout for the vertex and fragment shaders that can 
	//sample from both textures
	app->descriptor_layout = zest_AddDescriptorLayout("Particles descriptor layout", 0, 0, 2, 0);
	zest_descriptor_set_builder_t set_builder = zest_NewDescriptorSetBuilder();
	zest_AddBuilderDescriptorWriteImage(&set_builder, zest_GetTextureDescriptorImageInfo(app->particle_texture), 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	zest_AddBuilderDescriptorWriteImage(&set_builder, zest_GetTextureDescriptorImageInfo(app->gradient_texture), 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	app->descriptor_set = zest_BuildDescriptorSet(app->descriptor_pool, &set_builder, app->descriptor_layout);

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
	zest_buffer staging_buffer = zest_CreateStagingBuffer(storage_buffer_size,  particle_buffer.data());
	//Create a "Descriptor buffer". This is a buffer that will have info necessary for a shader - in this case a compute shader.
	//Create buffer as a single buffer, no need to have a buffer for each frame in flight as we won't be writing to it while it
	//might be used in the GPU, it's purely for updating by the compute shader only
	app->particle_buffer = zest_CreateComputeVertexDescriptorBuffer(storage_buffer_size, ZEST_FALSE);
	//Copy the staging buffer to the desciptor buffer
	zest_CopyBuffer(staging_buffer, zest_GetBufferFromDescriptorBuffer(app->particle_buffer), storage_buffer_size);
	//Free the staging buffer as we don't need it anymore
	zest_FreeBuffer(staging_buffer);

	//Create a new pipeline in the renderer based on an existing default one
	app->particle_pipeline = zest_CopyPipelineTemplate("particles", zest_PipelineTemplate("pipeline_2d_sprites"));

	//Change some things in the create info to set up our own pipeline
	//Clear the current vertex input binding descriptions
	zest_ClearVertexInputBindingDescriptions(app->particle_pipeline);
	//Add our own vertex binding description using the Particle struct
	zest_AddVertexInputBindingDescription(app->particle_pipeline, 0, sizeof(Particle), VK_VERTEX_INPUT_RATE_VERTEX);
	//Create a new vertex input description array
	app->vertice_attributes = zest_NewVertexInputDescriptions();
	//Add the descriptions for each type in the Particle struct
	zest_AddVertexInputDescription(&app->vertice_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Particle, pos)));
	zest_AddVertexInputDescription(&app->vertice_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Particle, gradient_pos)));

	//Assign our vertex input descriptions to the attributeDescriptions in the create info of the pipeline we're building
	app->particle_pipeline->attributeDescriptions = app->vertice_attributes;
	//Set the shader file to use in the pipeline
	zest_SetPipelineTemplateShader(app->particle_pipeline, "particle.spv", "examples/assets/spv/");
	//We're going to use point sprites so set that
	app->particle_pipeline->topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	//Add the descriptor layout we created earlier, but clear the layouts in the template first as a uniform buffer is added
	//by default.
	zest_ClearPipelineTemplateDescriptorLayouts(app->particle_pipeline);
	zest_AddPipelineTemplateDescriptorLayout(app->particle_pipeline, app->descriptor_layout->vk_layout);
	//Set the push constant we'll be using
	zest_SetPipelineTemplatePushConstantRange(app->particle_pipeline, sizeof(zest_vec2), 0, VK_SHADER_STAGE_VERTEX_BIT);

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
	app->compute_uniform_buffer = zest_CreateUniformBuffer("Compute Uniform", sizeof(ComputeUniformBuffer));

	app->shader_resources = zest_CreateShaderResources();
	zest_ForEachFrameInFlight(fif) {
		zest_AddDescriptorSetToResources(app->shader_resources, &app->descriptor_set, fif);
	}
	zest_ValidateShaderResource(app->shader_resources);

	//Set up the compute shader
	//Create a new empty compute shader in the renderer
	app->compute = zest_CreateCompute("particles");
	//A builder is used to simplify the compute shader setup process
	zest_compute_builder_t builder = zest_NewComputeBuilder();
	//Declare the bindings we want in the shader
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
	//The add the buffers for binding in the same order as the layout bindings
	zest_AddComputeBufferForBinding(&builder, app->particle_buffer);
	zest_AddComputeBufferForBinding(&builder, app->compute_uniform_buffer);
	//Set the user data so that we can use it in the callback funcitons
	zest_SetComputeUserData(&builder, app);
	//Declare the actual shader to use
	zest_shader compute_shader = zest_AddShaderFromSPVFile("examples/assets/spv/particle_comp.spv", shaderc_compute_shader);
	zest_AddComputeShader(&builder, compute_shader);
	//Finally, make the compute shader using the builder
	zest_MakeCompute(&builder, app->compute);

	//Set up our own draw routine
	app->draw_routine = zest_CreateDrawRoutine("compute draw");
	app->draw_routine->record_callback = RecordComputeSprites;
	app->draw_routine->condition_callback = DrawComputeSpritesCondition;
	app->draw_routine->user_data = app;

	//Modify the existing builtin command queue so we can add our new compute shader and draw routine
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		app->compute_commands = zest_NewComputeSetup("particles compute", app->compute, RecordComputeCommands, 0);
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			//Add our custom draw routine that will draw all the point sprites on the screen
			zest_AddDrawRoutine(app->draw_routine);
			//Also add a layer for dear imgui interface
			zest_imgui_CreateLayer(&app->imgui_layer_info);
		}
	}

	//Create a timer for a fixed update loop
	app->loop_timer = zest_CreateTimer(60.0);
}

void RecordComputeSprites(zest_work_queue_t *queue, void *data) {
	zest_draw_routine draw_routine = (zest_draw_routine)data;
	if (!draw_routine->recorder->outdated) {
		return;
	}
    VkCommandBuffer command_buffer = zest_BeginRecording(draw_routine->recorder, draw_routine->draw_commands->render_pass, ZEST_FIF);
	ImGuiApp *app = (ImGuiApp*)draw_routine->user_data;
	//We can make use of helper functions to easily bind the pipeline/vertex buffer and make the draw call
	//to draw the sprites
	zest_pipeline pipeline = zest_PipelineWithTemplate(app->particle_pipeline, ZestRenderer->current_render_pass);
	zest_GetDescriptorSetsForBinding(app->shader_resources, &app->draw_sets, ZEST_FIF);
	zest_BindPipeline(command_buffer, pipeline, app->draw_sets, 1);
	zest_vec2 screen_size = zest_Vec2Set(zest_ScreenWidthf(), zest_ScreenHeightf());
	zest_SendPushConstants(command_buffer, pipeline, &screen_size);
    VkViewport view = zest_CreateViewport(0.f, 0.f, zest_ScreenWidthf(), zest_ScreenHeightf(), 0.f, 1.f);
    VkRect2D scissor = zest_CreateRect2D(zest_ScreenWidth(), zest_ScreenHeight(), 0, 0);
    vkCmdSetViewport(command_buffer, 0, 1, &view);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
	zest_BindVertexBuffer(command_buffer, app->particle_buffer->buffer[0]);
	zest_Draw(command_buffer, PARTICLE_COUNT, 1, 0, 0);
	zest_ClearShaderResourceDescriptorSets(app->draw_sets);

	zest_EndRecording(draw_routine->recorder, ZEST_FIF);
}

int DrawComputeSpritesCondition(zest_draw_routine draw_routine) {
	//We can just always excute the draw commands to draw the particles
	return 1;
}

int ComputeCondition(zest_compute compute_commands) {
	return 1;
}

void RecordComputeCommands(zest_compute compute) {
	//The compute queue item can contain more then one compute shader to be dispatched
	//There's only one in this example though
	VkCommandBuffer command_buffer = zest_BeginComputeRecording(compute->recorder, ZEST_FIF);
	//Bind the compute pipeline
	zest_BindComputePipelineCB(command_buffer, compute, 0);
	zest_DispatchComputeCB(command_buffer, compute, PARTICLE_COUNT / 256, 1, 1);
	zest_EndRecording(compute->recorder, ZEST_FIF);
}

void UpdateComputeUniformBuffers(ImGuiApp *app) {
	//Update our custom compute shader uniform buffer
	ComputeUniformBuffer *uniform = (ComputeUniformBuffer*)zest_GetUniformBufferData(app->compute_uniform_buffer);
	uniform->deltaT = app->frame_timer * 2.5f;
	uniform->particleCount = PARTICLE_COUNT;
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

	//Set the active render queue to the default one that we modified earlier
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);

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
		ImGui::End();
		ImGui::Render();
		//We must mark the layer as dirty or nothing will be uploaded to the GPU
		zest_ResetLayer(app->imgui_layer_info.mesh_layer);
		zest_imgui_UpdateBuffers(app->imgui_layer_info.mesh_layer);
	} zest_EndTimerLoop(app->loop_timer)

}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	//Disable vsync so we can see how fast it runs
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
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
