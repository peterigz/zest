#include "zest-compute-example.h"
#include "imgui_internal.h"
#include <random>

void InitImGuiApp(ImGuiApp *app) {

	//Initialise Imgui for zest, this function just sets up some things like display size and font texture
	zest_imgui_Initialise(&app->imgui_layer_info);

	//Grab the imgui pipeline so we can use it later
	app->imgui_layer_info.pipeline = zest_Pipeline("pipeline_imgui");

	app->frame_timer = 1.f;
	app->timer = 0.f;
	app->anim_start = 20.f;
	app->attach_to_cursor = false;

	//Set up the compute shader example starts here
	//Prepare a couple of textures:
	//Particle image for point sprites
	app->particle_texture = zest_CreateTextureSingle("particle", zest_texture_format_rgba);
	app->particle_texture->image_view_type = VK_IMAGE_VIEW_TYPE_2D;
	zest_AddTextureImageFile(app->particle_texture, "examples/assets/particle.png");
	zest_ProcessTextureImages(app->particle_texture);
	//A gradient texture to sample the colour from
	app->gradient_texture = zest_CreateTextureSingle("gradient", zest_texture_format_rgba);
	app->gradient_texture->image_view_type = VK_IMAGE_VIEW_TYPE_2D;
	zest_AddTextureImageFile(app->gradient_texture, "examples/assets/gradient.png");
	zest_ProcessTextureImages(app->gradient_texture);

	//We'll need to create our own descriptor layout for the vertex and fragment shaders that can 
	//sample from both textures
	app->descriptor_layout = zest_AddDescriptorLayout("Particles descriptor layout", zest_CreateDescriptorSetLayout(0, 0, 2));
	zest_descriptor_set_builder_t set_builder = zest_NewDescriptorSetBuilder();
	zest_AddBuilderDescriptorWriteImage(&set_builder, zest_GetTextureDescriptorImageInfo(app->particle_texture), 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	zest_AddBuilderDescriptorWriteImage(&set_builder, zest_GetTextureDescriptorImageInfo(app->gradient_texture), 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	app->descriptor_set = zest_BuildDescriptorSet(&set_builder, app->descriptor_layout, zest_descriptor_type_static);

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

	//Prepare a pipeline for the compute sprites based on the built in pipeline 2d sprites
	zest_pipeline_template_create_info_t create_info = zest_CopyTemplateFromPipeline("pipeline_2d_sprites");
	//Create a new pipeline in the renderer
	app->particle_pipeline = zest_AddPipeline("particles");

	//Change some things in the create info to set up our own pipeline
	//Clear the current vertex input binding descriptions
	zest_ClearVertexInputBindingDescriptions(&create_info);
	//Add our own vertex binding description using the Particle struct
	zest_AddVertexInputBindingDescription(&create_info, 0, sizeof(Particle), VK_VERTEX_INPUT_RATE_VERTEX);
	//Create a new vertex input description array
	app->vertice_attributes = zest_NewVertexInputDescriptions();
	//Add the descriptions for each type in the Particle struct
	zest_AddVertexInputDescription(&app->vertice_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Particle, pos)));
	zest_AddVertexInputDescription(&app->vertice_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Particle, gradient_pos)));

	//Assign our vertex input descriptions to the attributeDescriptions in the create info of the pipeline we're building
	create_info.attributeDescriptions = app->vertice_attributes;
	//Set the shader file to use in the pipeline
	zest_SetPipelineTemplateShader(&create_info, "particle.spv", "examples/assets/spv/");
	//We're going to use point sprites so set that
	create_info.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	//Add the descriptor layout we created earlier, but clear the layouts in the template first as a uniform buffer is added
	//by default.
	zest_ClearPipelineTemplateDescriptorLayouts(&create_info);
	zest_AddPipelineTemplateDescriptorLayout(&create_info, app->descriptor_layout->vk_layout);
	//Set the push constant we'll be using
	zest_SetPipelineTemplatePushConstant(&create_info, sizeof(zest_vec2), 0, VK_SHADER_STAGE_VERTEX_BIT);

	//Using the create_info we prepared build the template for the pipeline
	zest_MakePipelineTemplate(app->particle_pipeline, zest_GetStandardRenderPass(), &create_info);
	//Alter a few things in the template to tweak it to how we want
	zest_PipelineTemplate(app->particle_pipeline)->rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	zest_PipelineTemplate(app->particle_pipeline)->rasterizer.cullMode = VK_CULL_MODE_NONE;
	zest_PipelineTemplate(app->particle_pipeline)->rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	zest_PipelineTemplate(app->particle_pipeline)->colorBlendAttachment = zest_AdditiveBlendState2();
	zest_PipelineTemplate(app->particle_pipeline)->depthStencil.depthWriteEnable = VK_FALSE;
	zest_PipelineTemplate(app->particle_pipeline)->depthStencil.depthTestEnable = VK_FALSE;
	//Build the pipeline so it's ready to use
	zest_BuildPipeline(app->particle_pipeline);

	//Setup a uniform buffer
	app->compute_uniform_buffer = zest_CreateUniformBuffer("Compute Uniform", sizeof(ComputeUniformBuffer));

	app->shader_resources = zest_CreateShaderResources();
	zest_AddDescriptorSetToResources(app->shader_resources, &app->descriptor_set);

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
	zest_AddComputeShader(&builder, "particle_comp.spv", "examples/assets/spv/");
	//Finally, make the compute shader using the builder
	zest_MakeCompute(&builder, app->compute);

	//Set up our own draw routine
	app->draw_routine = zest_CreateDrawRoutine("compute draw");
	app->draw_routine->record_callback = DrawComputeSprites;
	app->draw_routine->user_data = app;

	//Modify the existing builtin command queue so we can add our new compute shader and draw routine
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		app->compute_commands = zest_NewComputeSetup("particles compute", app->compute, UpdateComputeCommands);
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			//Add our custom draw routine that will draw all the point sprites on the screen
			zest_AddDrawRoutine(app->draw_routine);
			//Also add a layer for dear imgui interface
			app->imgui_layer_info.mesh_layer = zest_NewMeshLayer("imgui mesh layer", sizeof(ImDrawVert));
			zest_ContextDrawRoutine()->record_callback = zest_imgui_DrawLayer;
			zest_ContextDrawRoutine()->user_data = &app->imgui_layer_info;
		}
	}

	//The command buffer won't change so we can just record it once here and then simply execute the pre-recorded commands
	//each frame in the DrawComputeSprites callback.
	for (ZEST_EACH_FIF_i) {
		RecordComputeSprites(app->draw_routine, i);
	}
}

void RecordComputeSprites(zest_draw_routine draw_routine, zest_uint fif) {
    VkCommandBuffer command_buffer = zest_BeginRecording(draw_routine->recorder, draw_routine->draw_commands->render_pass, fif);
	ImGuiApp *app = (ImGuiApp*)draw_routine->user_data;
	//We can make use of helper functions to easily bind the pipeline/vertex buffer and make the draw call
	//to draw the sprites
	zest_GetDescriptorSetsForBinding(app->shader_resources, &app->draw_sets, ZEST_FIF);
	zest_BindPipeline(command_buffer, app->particle_pipeline, app->draw_sets, 1);
	zest_vec2 screen_size = zest_Vec2Set(zest_ScreenWidthf(), zest_ScreenHeightf());
	zest_SendPushConstants(command_buffer, app->particle_pipeline, VK_SHADER_STAGE_VERTEX_BIT, sizeof(zest_vec2), &screen_size);
    VkViewport view = zest_CreateViewport(0.f, 0.f, zest_ScreenWidthf(), zest_ScreenHeightf(), 0.f, 1.f);
    VkRect2D scissor = zest_CreateRect2D(zest_ScreenWidth(), zest_ScreenHeight(), 0, 0);
    vkCmdSetViewport(command_buffer, 0, 1, &view);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
	zest_BindVertexBuffer(command_buffer, app->particle_buffer->buffer[0]);
	zest_Draw(command_buffer, PARTICLE_COUNT, 1, 0, 0);
	zest_ClearShaderResourceDescriptorSets(app->draw_sets);

	zest_EndRecording(draw_routine->recorder, fif);
}

void DrawComputeSprites(zest_draw_routine draw_routine, VkCommandBuffer command_buffer) {
	//Our custom draw routine. This is automatically called as part of the main loop because
	//we added it to the command queue above (zest_AddDrawRoutine(app->draw_routine);)
	//We only need to excute the draw commands that we recorded in the InitImGuiApp function (RecordComputeSprites)
	zest_ExecuteDrawRoutine(command_buffer, draw_routine, ZEST_FIF);
}

void UpdateComputeCommands(zest_command_queue_compute compute_commands) {
	//The compute queue item can contain more then one compute shader to be dispatched
	//There's only one in this example though
	zest_compute compute = 0;
	while (compute = zest_NextComputeRoutine(compute_commands)) {
		//Bind the compute pipeline
		zest_BindComputePipeline(compute, 0);
		zest_DispatchCompute(compute, PARTICLE_COUNT / 256, 1, 1);
	}
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
