#include "header.h"
#include "imgui_internal.h"
#include <random>

void InitImGuiApp(ImGuiApp *app) {

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(zest_ScreenWidthf(), zest_ScreenHeightf());
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	int upload_size = width * height * 4 * sizeof(char);

	zest_bitmap_t font_bitmap = zest_CreateBitmapFromRawBuffer("font_bitmap", pixels, upload_size, width, height, 4);
	app->imgui_font_texture = zest_CreateTexture("imgui_font", zest_texture_storage_type_single, zest_texture_flag_none, zest_texture_format_rgba, 10);
	zest_texture font_texture = zest_GetTexture("imgui_font");
	zest_image font_image = zest_AddTextureImageBitmap(font_texture, &font_bitmap);
	zest_ProcessTextureImages(font_texture);
	io.Fonts->SetTexID(font_image);
	ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)ZestApp->window->window_handle, true);

	app->imgui_layer_info.pipeline = zest_Pipeline("pipeline_imgui");

	app->particle_texture = zest_CreateTextureSingle("particle", zest_texture_format_rgba);
	app->gradient_texture = zest_CreateTextureSingle("gradient", zest_texture_format_rgba);
	app->particle_texture->image_view_type = VK_IMAGE_VIEW_TYPE_2D;
	app->gradient_texture->image_view_type = VK_IMAGE_VIEW_TYPE_2D;
	zest_AddTextureImageFile(app->particle_texture, "particle.png");
	zest_AddTextureImageFile(app->gradient_texture, "gradient.png");
	zest_ProcessTextureImages(app->particle_texture);
	zest_ProcessTextureImages(app->gradient_texture);

	app->descriptor_layout = zest_AddDescriptorLayout("Particles descriptor layout", zest_CreateDescriptorSetLayout(0, 2, 0));
	zest_descriptor_set_builder_t set_builder = { 0 };
	zest_AddBuilderDescriptorWriteImage(&set_builder, &app->particle_texture->descriptor, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	zest_AddBuilderDescriptorWriteImage(&set_builder, &app->gradient_texture->descriptor, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	app->descriptor_set = zest_BuildDescriptorSet(&set_builder, app->descriptor_layout);

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

	zest_buffer staging_buffer = zest_CreateStagingBuffer(storage_buffer_size,  particle_buffer.data());
	zest_buffer_info_t descriptor_buffer_info = zest_CreateComputeVertexBufferInfo();
	app->particle_buffer = zest_CreateDescriptorBuffer(&descriptor_buffer_info, storage_buffer_size, ZEST_FALSE);
	zest_CopyBuffer(staging_buffer, app->particle_buffer->buffer[0]);
	zest_FreeBuffer(staging_buffer);

	//Prepare a pipeline for the compute sprites based on the built in pipeline 2d sprites
	zest_pipeline sprite_pipeline = zest_Pipeline("pipeline_2d_sprites");
	zest_pipeline_template_create_info_t create_info = sprite_pipeline->create_info;
	create_info.bindingDescription = zest_CreateVertexInputBindingDescription(0, sizeof(Particle), VK_VERTEX_INPUT_RATE_VERTEX);

	app->frame_timer = 1.f;
	app->timer = 0.f;
	app->anim_start = 20.f;
	app->attach_to_cursor = false;

	app->vertice_attributes = zest_NewVertexInputDescriptions();
	zest_AddVertexInputDescription(&app->vertice_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Particle, pos)));
	zest_AddVertexInputDescription(&app->vertice_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Particle, gradient_pos)));

	create_info.attributeDescriptions = app->vertice_attributes;
	app->particle_pipeline = zest_AddPipeline("particles");
	create_info.vertShaderFile = "spv/particle.spv";
	create_info.fragShaderFile = "spv/particle.spv";
	create_info.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	create_info.descriptorSetLayout = app->descriptor_layout;
	zest_SetPipelineTemplatePushConstant(&create_info, sizeof(zest_vec2), 0, VK_SHADER_STAGE_VERTEX_BIT);
	zest_MakePipelineTemplate(app->particle_pipeline, zest_GetStandardRenderPass(), &create_info);
	app->particle_pipeline->pipeline_template.rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	app->particle_pipeline->pipeline_template.rasterizer.cullMode = VK_CULL_MODE_NONE;
	app->particle_pipeline->pipeline_template.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	app->particle_pipeline->pipeline_template.colorBlendAttachment = zest_AdditiveBlendState2();
	app->particle_pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
	app->particle_pipeline->pipeline_template.depthStencil.depthTestEnable = VK_FALSE;
	zest_BuildPipeline(app->particle_pipeline);

	//Setup a uniform buffer
	app->compute_uniform_buffer = zest_CreateUniformBuffer("Compute Uniform", sizeof(ComputeUniformBuffer));

	//Set up the compute shader
	app->compute = zest_CreateCompute("particles");
	zest_compute_builder_t builder = zest_NewComputeBuilder();
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
	zest_AddComputeBufferForBinding(&builder, app->particle_buffer);
	zest_AddComputeBufferForBinding(&builder, app->compute_uniform_buffer);
	zest_SetComputeUserData(&builder, app);
	zest_SetComputeDescriptorUpdateCallback(&builder, UpdateComputeDescriptors);
	zest_AddComputeShader(&builder, "spv/particle_comp.spv");
	zest_MakeCompute(&builder, app->compute);

	//Set up our own draw routine
	app->draw_routine = zest_CreateDrawRoutine("compute draw");
	app->draw_routine->draw_callback = DrawComputeSprites;
	app->draw_routine->user_data = app;

	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		app->compute_commands = zest_NewComputeSetup("particles compute", app->compute, UpdateComputeCommands);
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			zest_AddDrawRoutine(app->draw_routine);
			app->imgui_layer_info.mesh_layer = zest_NewMeshLayer("imgui mesh layer", sizeof(ImDrawVert));
			zest_ContextDrawRoutine()->draw_callback = zest_imgui_DrawLayer;
			zest_ContextDrawRoutine()->user_data = &app->imgui_layer_info;
		}
	}
}

void DrawComputeSprites(zest_draw_routine draw_routine, VkCommandBuffer command_buffer) {
	ImGuiApp *app = (ImGuiApp*)draw_routine->user_data;
	zest_BindPipeline(app->particle_pipeline, app->descriptor_set.descriptor_set[ZEST_FIF]);
	zest_vec2 screen_size = zest_Vec2Set(zest_ScreenWidthf(), zest_ScreenHeightf());
	zest_SendPushConstants(app->particle_pipeline, VK_SHADER_STAGE_VERTEX_BIT, sizeof(zest_vec2), &screen_size);
	zest_BindVertexBuffer(app->particle_buffer->buffer[0]);
	zest_Draw(PARTICLE_COUNT, 1, 0, 0);
}

void UpdateComputeDescriptors(zest_compute compute) {
	ImGuiApp *app = (ImGuiApp*)compute->user_data;

}

void UpdateComputeCommands(zest_command_queue_compute compute_commands) {
	//The compute queue item can contain more then one compute shader to be dispatched
	//There's only one in this example though
	for (zest_foreach_i(compute_commands->compute_shaders)) {
		zest_compute compute = compute_commands->compute_shaders[i];

		//We can update the push constants each frame if we need to
		ImGuiApp *app = static_cast<ImGuiApp*>(compute->user_data);

		//Bind the compute pipeline
		zest_BindComputePipeline(compute, 0);

		zest_DispatchCompute(compute, PARTICLE_COUNT / 256, 1, 1);
	}

	zest_ComputeToVertexBarrier();
}

void UpdateComputeUniformBuffers(ImGuiApp *app) {
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
	//Don't forget to update the uniform buffer!
	ImGuiApp *app = (ImGuiApp*)user_data;
	zest_Update2dUniformBuffer();
	UpdateComputeUniformBuffers(app);
	zest_SetActiveRenderQueue(ZestApp->default_command_queue);

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

	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Test Window");
	ImGui::Text("FPS %zu", ZestApp->last_fps);
	ImGui::Checkbox("Repel Mouse", &app->attach_to_cursor);
	ImGui::End();
	ImGui::Render();
	zest_imgui_CopyBuffers(app->imgui_layer_info.mesh_layer);
}

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