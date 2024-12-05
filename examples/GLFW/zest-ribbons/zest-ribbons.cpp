#include "zest-ribbons.h"
#include "imgui_internal.h"

void InitImGuiApp(Ribbons *app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise(&app->imgui_layer_info);
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
	zest_imgui_RebuildFontTexture(&app->imgui_layer_info, tex_width, tex_height, font_data);

	app->sync_refresh = true;

	//Set up the compute shader
	//Create a new empty compute shader in the renderer
	app->ribbon_compute = zest_CreateCompute("ribbons");
	//A builder is used to simplify the compute shader setup process
	zest_compute_builder_t builder = zest_NewComputeBuilder();
	//Declare the bindings we want in the shader
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
	//The add the buffers for binding in the same order as the layout bindings
	zest_AddComputeBufferForBinding(&builder, app->ribbon_buffer);
	zest_AddComputeBufferForBinding(&builder, ZestRenderer->standard_uniform_buffer);
	//Set the user data so that we can use it in the callback funcitons
	zest_SetComputeUserData(&builder, app);
	//Declare the actual shader to use
	zest_AddComputeShader(&builder, "particle_comp.spv", "examples/assets/spv/");
	//Finally, make the compute shader using the builder
	zest_MakeCompute(&builder, app->ribbon_compute);

	//Modify the existing default queue
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			app->line_layer = zest_NewBuiltinLayerSetup("test lines", zest_builtin_layer_3dlines);
			//Create a Dear ImGui layer
			zest_imgui_CreateLayer(&app->imgui_layer_info);
		}
		//Don't forget to finish the queue set up
		zest_FinishQueueSetup();
	}

	//zest_SetLayerToManualFIF(app->line_layer);
	app->line_pipeline = zest_Pipeline("pipeline_line3d_instance");

	app->timer = zest_CreateTimer(60);

	app->camera = zest_CreateCamera();
	zest_CameraSetFoV(&app->camera, 60.f);

	UpdateUniform3d(app);
}

//Basic function for updating the uniform buffer
void UpdateUniform3d(Ribbons *app) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(ZestRenderer->standard_uniform_buffer);
	buffer_3d->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	buffer_3d->proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.1f, 10000.f);
	buffer_3d->proj.v[1].y *= -1.f;
	buffer_3d->screen_size.x = zest_ScreenWidthf();
	buffer_3d->screen_size.y = zest_ScreenHeightf();
	buffer_3d->millisecs = 0;
}

//Allows us to cast a ray into the screen from the mouse position to place an effect where we click
zest_vec3 ScreenRay(float x, float y, float depth_offset, zest_vec3 &camera_position, zest_descriptor_buffer buffer) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

void BuildUI(Ribbons *app) {
	//Must call the imgui GLFW implementation function
	ImGui_ImplGlfw_NewFrame();
	//Draw our imgui stuff
	ImGui::NewFrame();
	ImGui::Begin("Test Window");
	ImGui::Text("FPS %i", ZestApp->last_fps);
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
	//An imgui layer is a manual layer, meaning that you need to let it know that the buffers need updating.
	zest_ResetLayer(app->imgui_layer_info.mesh_layer);
	//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
	zest_imgui_UpdateBuffers(app->imgui_layer_info.mesh_layer);
}

void UpdateCallback(zest_microsecs elapsed, void* user_data) {
	//Set the active command queue to the default one that was created when Zest was initialised
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);
	Ribbons* app = (Ribbons*)user_data;

	UpdateUniform3d(app);

	zest_TimerAccumulate(app->timer);
	int pending_ticks = zest_TimerPendingTicks(app->timer);
	while (zest_TimerDoUpdate(app->timer)) {
		BuildUI(app);
		zest_TimerUnAccumulate(app->timer);
	}
	zest_TimerSet(app->timer);

	zest_vec3 position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 6.f, app->camera.position, ZestRenderer->standard_uniform_buffer);
	int i = app->ribbon_index++;
	app->ribbon_index = i == 99 ? 0 : app->ribbon_index;
	app->ribbon[i].position_and_width = { position.x, position.y, position.z, 2.f };
	zest_ResetInstanceLayer(app->line_layer);
	zest_Set3DLineDrawing(app->line_layer, app->line_pipeline->shader_resources, app->line_pipeline);
	zest_SetLayerColor(app->line_layer, 255, 255, 255, 50);
	for (int n = 100; n != 200; ++n) {
		if (n - 100 == app->ribbon_index) continue;
		int index = n % 100;
		int prev_index = (n - 1) % 100;
		zest_vec3 end = { app->ribbon[prev_index].position_and_width.x, app->ribbon[prev_index].position_and_width.y, app->ribbon[prev_index].position_and_width.z };
		zest_Draw3DLine(app->line_layer, &app->ribbon[index].position_and_width.x, &end.x, app->ribbon[index].position_and_width.w);
	}
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
//int main(void) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfo();
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
