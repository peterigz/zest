#include <zest.h>

typedef struct zest_example {
	zest_font font;
	zest_layer font_layer;
	zest_draw_routine draw_routine;
	zest_render_graph render_graph;
} zest_example;

void InitExample(zest_example *example) {
	example->font_layer = zest_CreateFontLayer("Example fonts");

	//Load a font and store the handle. MSDF fonts are in the zft format which you can create using zest-msdf-font-maker
	example->font = zest_LoadMSDFFont("examples/assets/KaushanScript-Regular.zft");

	example->render_graph = zest_NewRenderGraph("Fonts Example Render Graph", 0, false);

	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Font pipeline");
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	//Get the example struct from the user data we set with zest_SetUserData
	zest_example *example = (zest_example*)user_data;
	//Update the uniform buffer.
	zest_Update2dUniformBuffer();

	//Set the instensity of the font
	example->font_layer->intensity = 1.f;
	//Let the layer know that we want to draw text with the following font
	zest_SetMSDFFontDrawing(example->font_layer, example->font);
	//Draw a paragraph of text
	zest_DrawMSDFParagraph(example->font_layer, "Zest fonts drawn using MSDF!\nHere are some numbers 12345\nLorem ipsum and all that\nHello Sailer!\n", 0.f, 0.f, 0.f, 0.f, 50.f, 0.f, 1.f);
	//Draw a single line of text
	zest_DrawMSDFText(example->font_layer, "(This should be centered)", zest_ScreenWidth() * .5f, zest_ScreenHeightf() * .5f, .5f, .5f, 50.f, 0.f);

	//Create the render graph
	if (zest_BeginRenderToScreen(example->render_graph)) {
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };

		//Add resources
		zest_resource_node swapchain_output_resource = zest_ImportSwapChainResource(example->render_graph, "Swapchain Output");
		zest_resource_node font_layer_resources = zest_AddInstanceLayerBufferResource(example->render_graph, example->font_layer);
		zest_resource_node font_layer_texture = zest_AddFontLayerTextureResource(example->render_graph, example->font);

		//Add passes
		zest_pass_node graphics_pass = zest_AddRenderPassNode(example->render_graph, "Graphics Pass");
		zest_pass_node upload_font_data = zest_AddTransferPassNode(example->render_graph, "Upload Font Data");

		//Connect buffers and textures
		zest_ConnectTransferBufferOutput(upload_font_data, font_layer_resources);
		zest_ConnectVertexBufferInput(graphics_pass, font_layer_resources);
		zest_ConnectSampledImageInput(graphics_pass, font_layer_texture, zest_fragment_stage);
		zest_ConnectSwapChainOutput(graphics_pass, swapchain_output_resource, clear_color);

		//Add the tasks to run for the passes
		zest_AddPassTask(upload_font_data, zest_UploadInstanceLayerData, example->font_layer);
		zest_AddPassTask(graphics_pass, zest_DrawFonts, example->font_layer);
		zest_EndRenderGraph(example->render_graph);

		//Print the render graph
		static bool print_render_graph = true;
		if (print_render_graph) {
			zest_PrintCompiledRenderGraph(example->render_graph);
			print_render_graph = false;
		}

		//Execute the render graph
		zest_ExecuteRenderGraph(example->render_graph);
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(zest_validation_flag_enable_sync);
	create_info.log_path = "./";
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	zest_example example;

	zest_Initialise(&create_info);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(UpdateCallback);

	InitExample(&example);

	zest_Start();

	return 0;
}
#else
int main(void) {

	zest_create_info_t create_info = zest_CreateInfo();

	zest_example example;
    
	zest_Initialise(&create_info);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(UpdateCallback);

	InitExample(&example);

	zest_Start();

	return 0;
}
#endif
