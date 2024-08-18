#include "zest-shader-compiling.h"
#include "imgui_internal.h"
#include <string>

void InitImGuiApp(ImGuiApp *app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise(&app->imgui_layer_info);
	//Implement a dark style
	DarkStyle2();

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
	app->test_texture = zest_CreateTexture("Bunny", zest_texture_storage_type_sprite_sheet, zest_texture_flag_use_filtering, zest_texture_format_rgba, 10);
	//Load in the image and add it to the texture
	app->test_image = zest_AddTextureImageFile(app->test_texture, "examples/assets/wabbit_alpha.png");
	//Process the texture so that its ready to be used
	zest_ProcessTextureImages(app->test_texture);

	app->shader_code = { 0 };
	zest_ResizeText(&app->shader_code, 2048);
	zest_SetText(&app->shader_code, "Shader Code");

	app->custom_frag_shader = zest_CopyShader("spv/image_frag.spv", "custom_frag.spv");
	FormatShaderCode(&app->custom_frag_shader->shader_code);
	zest_AddShader(app->custom_frag_shader);

	zest_pipeline_template_create_info_t custom_pipeline_template = zest_CopyTemplateFromPipeline("pipeline_2d_sprites");
	app->custom_pipeline = zest_AddPipeline("pipeline_custom_shader");
	zest_SetPipelineTemplateVertShader(&custom_pipeline_template, "sprite_vert.spv", 0);
	zest_SetPipelineTemplateFragShader(&custom_pipeline_template, "custom_frag.spv", 0);
	zest_MakePipelineTemplate(app->custom_pipeline, zest_GetStandardRenderPass(), &custom_pipeline_template);
	zest_BuildPipeline(app->custom_pipeline);
	app->validation_result = nullptr;

	//Modify the existing default queue
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			app->custom_layer = zest_NewBuiltinLayerSetup("Custom Sprites", zest_builtin_layer_sprites);
			//Create a Dear ImGui layer
			zest_imgui_CreateLayer(&app->imgui_layer_info);
		}
		//Don't forget to finish the queue set up
		zest_FinishQueueSetup();
	}

}

zest_uint ReadNextChunk(zest_text_t *text, zest_uint offset, char chunk[64]) {
	if (offset < zest_TextLength(text)) {
		zest_uint max_length = zest_TextLength(text) - offset;
		memcpy(chunk, text->str, ZEST__MIN(64, max_length));
		return max_length;
	}
	else {
		chunk[0] = '\0';
	}
	return 0;
}

void AddLine(zest_text_t *text, char current_char, zest_uint *position, zest_uint tabs) {
	ZEST_ASSERT(*position < zest_TextLength(text));
	text->str[(*position)++] = current_char;
	ZEST_ASSERT(*position < zest_TextLength(text));
	text->str[(*position)++] = '\n';
	for (int t = 0; t != tabs; ++t) {
		text->str[(*position)++] = '\t';
	}
}

void FormatShaderCode(zest_text_t *code) {
	zest_uint length = zest_TextLength(code);
	zest_uint pos = 0;
	zest_text_t formatted_code = {};
	//First pass to calculate the new formatted buffer size
	zest_uint extra_size = 0;
	int tabs = 0;
	for (zest_uint i = 0; i != length; ++i) {
		if (code->str[i] == ';') {
			extra_size++;
			extra_size += tabs;
		}
		else if (code->str[i] == '{') {
			extra_size++;
			extra_size += tabs;
			tabs++;
		}
		else if (code->str[i] == '}') {
			extra_size++;
			extra_size += tabs;
			tabs--;
		}
	}
	pos = 0;
	zest_ResizeText(&formatted_code, length + extra_size);
	zest_uint new_length = zest_TextLength(&formatted_code);
	zest_uint f_pos = 0;
	bool new_line_added = false;
	for (zest_uint i = 0; i != length; ++i) {
		if (new_line_added) {
			if (code->str[i] == ' ') {
				continue;
			}
			else {
				new_line_added = false;
			}
		}
		if (code->str[i] == ';') {
			AddLine(&formatted_code, code->str[i], &f_pos, tabs);
			new_line_added = true;
		}
		else if (code->str[i] == '{') {
			tabs++;
			AddLine(&formatted_code, code->str[i], &f_pos, tabs);
			new_line_added = true;
		}
		else if (code->str[i] == '}') {
			if (tabs > 0) {
				f_pos--;
				tabs--;
			}
			AddLine(&formatted_code, code->str[i], &f_pos, tabs);
			new_line_added = true;
		}
		else {
			ZEST_ASSERT(f_pos < new_length);
			formatted_code.str[f_pos++] = code->str[i];
		}
	}
	zest_FreeText(code);
	*code = formatted_code;
}

int EditShaderCode(ImGuiInputTextCallbackData *data) {
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		zest_text_t *code = static_cast<zest_text_t *>(data->UserData);
		if (zest_TextLength(code) < (zest_uint)data->BufSize) {
			zest_ResizeText(code, data->BufSize);
			data->Buf = code->str;
		}
	}
	else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
		zest_text_t *code = static_cast<zest_text_t *>(data->UserData);
		if (zest_TextLength(code) <= (zest_uint)data->BufTextLen) {
			if ((int)zest_TextLength(code) >= data->BufTextLen + 1) {
				zest_ResizeText(code, data->BufSize + 1);
			}
		}
	}
	return 0;
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
	if (ImGui::Button("Compile")) {
		shaderc_result_release(app->validation_result);
		app->validation_result = zest_ValidateShader(app->custom_frag_shader->shader_code.str, shaderc_fragment_shader, app->custom_frag_shader->name.str);
	}
	if (app->validation_result) {
		if (shaderc_result_get_compilation_status(app->validation_result) != shaderc_compilation_status_success) {
			const char *error_message = shaderc_result_get_error_message(app->validation_result);
			ImGui::TextWrapped("%s", error_message);
		}
		else {
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
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, .75f));
	ImGui::InputTextMultiline("##Shader Code", app->custom_frag_shader->shader_code.str, zest_vec_size(app->custom_frag_shader->shader_code.str), ImVec2(-1.f, -1.f), ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackResize, EditShaderCode, &app->custom_frag_shader->shader_code);
	ImGui::PopStyleColor();;
	//zest_imgui_DrawImage(app->test_image, 50.f, 50.f);
	ImGui::End();
	ImGui::Render();
	//Let the layer know that it needs to reupload the imgui mesh data to the GPU
	zest_SetLayerDirty(app->imgui_layer_info.mesh_layer);
	//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
	zest_imgui_UpdateBuffers(app->imgui_layer_info.mesh_layer);

	zest_SetSpriteDrawing(app->custom_layer, app->test_texture, 0, app->custom_pipeline);
	zest_DrawSprite(app->custom_layer, app->test_image, 800.f, 400.f, 0.f, 100.f, 100.f, .5f, .5f, 0, 0.f);
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
	//int main(void) {
		//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfo();
	//Don't enable vsync so we can see the FPS go higher then the refresh rate
	//ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
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
