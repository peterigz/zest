#include "zest-shader-compiling.h"
#include "imgui_internal.h"
#include <string>

void init_file_monitor(FileMonitor *monitor, const char *filepath) {
	strncpy(monitor->filepath, filepath, sizeof(monitor->filepath) - 1);
	struct stat file_stat;
	if (stat(filepath, &file_stat) == 0) {
		monitor->last_modified = file_stat.st_mtime;
	}
}

bool check_file_changed(FileMonitor *monitor) {
	struct stat file_stat;
	if (stat(monitor->filepath, &file_stat) != 0) {
		return false;  // Handle error case
	}

	if (file_stat.st_mtime > monitor->last_modified) {
		monitor->last_modified = file_stat.st_mtime;
		return true;
	}
	return false;
}

void InitImGuiApp(ImGuiApp *app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise(&app->imgui_layer_info);
	//Implement a dark style
	zest_imgui_DarkStyle();

	//This is an exmaple of how to change the font that ImGui uses
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.Fonts->Clear();
	float font_size = 17.f;
	unsigned char *font_data;
	int tex_width, tex_height;
	ImFontConfig config;
	config.PixelSnapH = true;
	io.Fonts->AddFontFromFileTTF("examples/assets/consola.ttf", font_size);
	io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

	//Rebuild the Zest font texture
	zest_imgui_RebuildFontTexture(&app->imgui_layer_info, tex_width, tex_height, font_data);

	//Create a texture to load in a test image to show drawing that image in an imgui window
	app->test_texture = zest_CreateTexture("test image", zest_texture_storage_type_sprite_sheet, zest_texture_flag_use_filtering, zest_texture_format_rgba_unorm, 10);
	//Load in the image and add it to the texture
	app->test_image = zest_AddTextureImageFile(app->test_texture, "examples/assets/glow.png");
	//Process the texture so that its ready to be used
	zest_ProcessTextureImages(app->test_texture);

	app->compiler = shaderc_compiler_initialize();
	app->custom_frag_shader = zest_CreateShaderFromFile("examples/assets/shaders/ig_sdf.frag", "custom_frag.spv", shaderc_fragment_shader, true, app->compiler);
	app->custom_vert_shader = zest_CreateShader(custom_vert_shader, shaderc_vertex_shader, "custom_vert.spv", true, true, app->compiler);

	zest_pipeline_template_create_info_t custom_pipeline_template = zest_CopyTemplateFromPipeline("pipeline_2d_sprites");
	zest_ClearPipelinePushConstantRanges(&custom_pipeline_template);
	VkPushConstantRange image_pushconstant_range;
	image_pushconstant_range.size = sizeof(zest_push_constants_t);
	image_pushconstant_range.offset = 0;
	image_pushconstant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	zest_AddPipelinePushConstantRange(&custom_pipeline_template, image_pushconstant_range);
	app->custom_pipeline = zest_CreatePipelineTemplate("pipeline_custom_shader");
	zest_SetPipelineTemplateVertShader(&custom_pipeline_template, "custom_vert.spv", 0);
	zest_SetPipelineTemplateFragShader(&custom_pipeline_template, "custom_frag.spv", 0);
	zest_FinalisePipelineTemplate(app->custom_pipeline, zest_GetStandardRenderPass(), &custom_pipeline_template);
	zest_BuildPipeline(app->custom_pipeline);
	app->validation_result = nullptr;
	app->mix_value = 0.f;

	app->shader_resources = zest_CombineUniformAndTextureSampler(ZestRenderer->uniform_descriptor_set, app->test_texture);

	//Modify the existing default queue
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			zest_ContextSetClsColor(0.2f, 0.2f, 0.2f, 1);
			app->custom_layer = zest_NewBuiltinLayerSetup("Custom Sprites", zest_builtin_layer_sprites);
			//Create a Dear ImGui layer
			zest_imgui_CreateLayer(&app->imgui_layer_info);
		}
		//Don't forget to finish the queue set up
		zest_FinishQueueSetup();
	}

	init_file_monitor(&app->shader_file, app->custom_frag_shader->file_path.str);

	app->num_cells = 32;
	app->start_time = 0;
	app->custom_layer->push_constants.parameters1.x = 400;
	app->custom_layer->push_constants.parameters1.y = 400;
}

int EditShaderCode(ImGuiInputTextCallbackData *data) {
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		zest_text_t *code = static_cast<zest_text_t *>(data->UserData);
		if (zest_TextSize(code) < (zest_uint)data->BufSize) {
			zest_ResizeText(code, data->BufSize);
			data->Buf = code->str;
		}
	}
	else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
		zest_text_t *code = static_cast<zest_text_t *>(data->UserData);
		if (zest_TextSize(code) <= (zest_uint)data->BufTextLen) {
			if ((int)zest_TextSize(code) >= data->BufTextLen + 1) {
				zest_ResizeText(code, data->BufSize + 1);
			}
		}
	}
	return 0;
}

void recompile_shader(ImGuiApp *app) {
	zest_ReloadShader(app->custom_frag_shader);
	shaderc_result_release(app->validation_result);
	app->validation_result = zest_ValidateShader(app->custom_frag_shader->shader_code.str, shaderc_fragment_shader, app->custom_frag_shader->name.str, app->compiler);
	if (app->validation_result) {
		if (shaderc_result_get_compilation_status(app->validation_result) == shaderc_compilation_status_success) {
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.1f, 1.f, .1f, 1.f));
			ImGui::Text("OK!");
			ImGui::PopStyleColor();
			zest_UpdateShaderSPV(app->custom_frag_shader, app->validation_result);
			shaderc_result_release(app->validation_result);
			app->validation_result = nullptr;
			zest_SchedulePipelineRecreate(app->custom_pipeline);
		}
	}
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	//Don't forget to update the uniform buffer!
	zest_Update2dUniformBuffer();
	//Set the active command queue to the default one that was created when Zest was initialised
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);
	ImGuiApp *app = (ImGuiApp *)user_data;

	//Must call the imgui GLFW implementation function
	ImGui_ImplGlfw_NewFrame();
	//Draw our imgui stuff
	ImGui::NewFrame();
	ImGui::Begin("Test Window");
	ImGui::DragFloat2("Size", &app->custom_layer->push_constants.parameters1.x, 1.f, 32.f, 1024.f, "%.0f");
	ImGui::SliderFloat("Cells", &app->num_cells, 0.f, 32.f);
	if (check_file_changed(&app->shader_file)) {
		recompile_shader(app);
	}
	if (app->validation_result) {
		if (shaderc_result_get_compilation_status(app->validation_result) != shaderc_compilation_status_success) {
			const char *error_message = shaderc_result_get_error_message(app->validation_result);
			ImGui::TextWrapped("%s", error_message);
		}
	}
	//zest_imgui_DrawImage(app->test_image, 50.f, 50.f);
	ImGui::End();
	ImGui::Render();
	//Let the layer know that it needs to reupload the imgui mesh data to the GPU
	zest_ResetLayer(app->imgui_layer_info.mesh_layer);
	//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
	zest_imgui_UpdateBuffers(app->imgui_layer_info.mesh_layer);

	zest_SetInstanceDrawing(app->custom_layer, app->shader_resources, app->custom_pipeline);
	app->custom_layer->push_constants.parameters2.x = float((int)app->num_cells);
	app->custom_layer->push_constants.parameters2.y += float(elapsed) / 1000000.f;
	zest_SetLayerIntensity(app->custom_layer, 3.f);
	zest_SetLayerColor(app->custom_layer, 255, 128, 64, 0);
	float width = app->custom_layer->push_constants.parameters1.x;
	float height = app->custom_layer->push_constants.parameters1.y;
	zest_DrawSprite(app->custom_layer, app->test_image, 1000.f, 400.f, 0.f, width, height, .5f, .5f, 0, 0.f);
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
//int main(void) {
		//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfo();
	//Don't enable vsync so we can see the FPS go higher then the refresh rate
	//ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	create_info.log_path = ".";
	//Implement GLFW for window creation
	zest_implglfw_SetCallbacks(&create_info);

	ImGuiApp imgui_app;

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

	shaderc_compiler_release(imgui_app.compiler);
	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	zest_implglfw_SetCallbacks(&create_info);
	ZEST__FLAG(create_info.flags, zest_init_flag_maximised);

	ImGuiApp imgui_app;

	create_info.log_path = ".";
	zest_Initialise(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#endif
