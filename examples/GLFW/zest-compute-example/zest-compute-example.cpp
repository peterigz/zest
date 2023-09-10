#include "zest-compute-example.h"
#include "imgui_internal.h"
#include <random>

void InitImGuiApp(ImGuiApp *app) {

	//Initialise Imgui for zest, this function just sets up some things like display size and font texture
	zest_imgui_Initialise();

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
	app->descriptor_layout = zest_AddDescriptorLayout("Particles descriptor layout", zest_CreateDescriptorSetLayout(0, 2, 0));
	zest_descriptor_set_builder_t set_builder = zest_NewDescriptorSetBuilder();
	zest_AddBuilderDescriptorWriteImage(&set_builder, &app->particle_texture->descriptor, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	zest_AddBuilderDescriptorWriteImage(&set_builder, &app->gradient_texture->descriptor, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	app->descriptor_set = zest_BuildDescriptorSet(&set_builder, app->descriptor_layout);

	//Load the particle data with random coordinates
	std::default_random_engine rndEngine(0);
	std::uniform_real_distribution<float> rndDist(-1.0f, 1.0f);
	// Initial particle positions
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
	//We define a zest_buffer_info_t to setup the attributes of the buffer
	zest_buffer_info_t descriptor_buffer_info = zest_CreateComputeVertexBufferInfo();
	//Create buffer as a single buffer, no need to have a buffer for each frame in flight as we won't be writing to it while it
	//might be used in the GPU, it's purely for updating by the compute shader only
	app->particle_buffer = zest_CreateDescriptorBuffer(&descriptor_buffer_info, storage_buffer_size, ZEST_FALSE);
	//Copy the staging buffer to the desciptor buffer
	zest_CopyBuffer(staging_buffer, zest_GetBufferFromDescriptorBuffer(app->particle_buffer));
	//Free the staging buffer as we don't need it anymore
	zest_FreeBuffer(staging_buffer);

	//Prepare a pipeline for the compute sprites based on the built in pipeline 2d sprites
	zest_pipeline_template_create_info_t create_info = zest_CopyTemplateFromPipeline("pipeline_2d_sprites");
	//Create a new pipeline in the renderer
	app->particle_pipeline = zest_AddPipeline("particles");

	//Change some things in the create info to set up our pipeline
	create_info.bindingDescription = zest_CreateVertexInputBindingDescription(0, sizeof(Particle), VK_VERTEX_INPUT_RATE_VERTEX);
	app->vertice_attributes = zest_NewVertexInputDescriptions();
	zest_AddVertexInputDescription(&app->vertice_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Particle, pos)));
	zest_AddVertexInputDescription(&app->vertice_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Particle, gradient_pos)));

	create_info.attributeDescriptions = app->vertice_attributes;
	create_info.vertShaderFile = "examples/assets/spv/particle.spv";
	create_info.fragShaderFile = "examples/assets/spv/particle.spv";
	create_info.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	create_info.descriptorSetLayout = app->descriptor_layout;
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
	zest_AddComputeShader(&builder, "examples/assets/spv/particle_comp.spv");
	//Finally, make the compute shader using the builder
	zest_MakeCompute(&builder, app->compute);

	//Set up our own draw routine
	app->draw_routine = zest_CreateDrawRoutine("compute draw");
	app->draw_routine->draw_callback = DrawComputeSprites;
	app->draw_routine->user_data = app;

	//Modify the existing builtin command queue so we can add our new compute shader and draw routine
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		//Add the compute shader we created above. specity the command buffer function to use to build the 
		//command buffer that will dispatch the compute shader
		app->compute_commands = zest_NewComputeSetup("particles compute", app->compute, UpdateComputeCommands);
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			//Add our custom draw routine that will draw all the point sprites on the screen
			zest_AddDrawRoutine(app->draw_routine);
			//Also add a layer for dear imgui interface
			app->imgui_layer_info.mesh_layer = zest_NewMeshLayer("imgui mesh layer", sizeof(ImDrawVert));
			zest_ContextDrawRoutine()->draw_callback = zest_imgui_DrawLayer;
			zest_ContextDrawRoutine()->user_data = &app->imgui_layer_info;
		}
	}
}

void DrawComputeSprites(zest_draw_routine draw_routine, VkCommandBuffer command_buffer) {
	//Our custom draw routine. This is automatically called as part of the main loop because
	//we added it to the command queue above (zest_AddDrawRoutine(app->draw_routine);)
	ImGuiApp *app = (ImGuiApp*)draw_routine->user_data;
	//We can make use of helper functions to easily bind the pipeline/vertex buffer and make the draw call
	//to draw the sprites
	zest_BindPipeline(app->particle_pipeline, app->descriptor_set.descriptor_set[ZEST_FIF]);
	zest_vec2 screen_size = zest_Vec2Set(zest_ScreenWidthf(), zest_ScreenHeightf());
	zest_SendPushConstants(app->particle_pipeline, VK_SHADER_STAGE_VERTEX_BIT, sizeof(zest_vec2), &screen_size);
	zest_BindVertexBuffer(app->particle_buffer->buffer[0]);
	zest_Draw(PARTICLE_COUNT, 1, 0, 0);
}

void UpdateComputeCommands(zest_command_queue_compute compute_commands) {
	//The compute queue item can contain more then one compute shader to be dispatched
	//There's only one in this example though
	for (zest_foreach_i(compute_commands->compute_shaders)) {
		zest_compute compute = compute_commands->compute_shaders[i];

		//Bind the compute pipeline
		zest_BindComputePipeline(compute, 0);

		zest_DispatchCompute(compute, PARTICLE_COUNT / 256, 1, 1);
	}

	zest_ComputeToVertexBarrier();
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
	ImGui::Text("FPS %zu", ZestApp->last_fps);
	ImGui::Text("Particle Count: %u", PARTICLE_COUNT);
	ImGui::Checkbox("Repel Mouse", &app->attach_to_cursor);
	ImGui::End();
	ImGui::Render();
	zest_imgui_CopyBuffers(app->imgui_layer_info.mesh_layer);
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
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