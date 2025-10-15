#include <zest.h>

typedef struct zest_example {
	zest_font font;
	zest_layer font_layer;
} zest_example;

void InitExample(zest_example *example) {
	example->font_layer = zest_CreateFontLayer("Example fonts");

	//Load a font and store the handle. MSDF fonts are in the zft format which you can create using zest-msdf-font-maker
	example->font = zest_LoadMSDFFont("examples/assets/KaushanScript-Regular.zft");
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
	if (zest_BeginFrameGraphSwapchain(zest_GetMainWindowSwapchain(), "Fonts Example Render Graph", 0)) {
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };

		//Add resources
		zest_resource_node font_layer_resources = zest_AddTransientLayerResource("Font resources", example->font_layer, false);
		zest_resource_node font_layer_texture = zest_ImportFontResource(example->font);

		//---------------------------------Transfer Pass------------------------------------------------------
		zest_pass_node upload_font_data = zest_BeginTransferPass("Upload Font Data");
		//outputs
		zest_ConnectOutput(upload_font_data, font_layer_resources);
		//tasks
		zest_SetPassTask(upload_font_data, zest_UploadInstanceLayerData, example->font_layer);
		//--------------------------------------------------------------------------------------------------

		//---------------------------------Render Pass------------------------------------------------------
		zest_pass_node graphics_pass = zest_BeginRenderPass("Graphics Pass");
		//inputes
		zest_ConnectInput(graphics_pass, font_layer_resources, 0);
		zest_ConnectInput(graphics_pass, font_layer_texture, 0);
		//outputs
		zest_ConnectSwapChainOutput(graphics_pass);
		//tasks
		zest_SetPassTask(graphics_pass, zest_DrawFonts, example->font_layer);
		//--------------------------------------------------------------------------------------------------

		//End and execute the render graph
		zest_EndFrameGraph();
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

	zest_CreateContext(&create_info);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(UpdateCallback);

	InitExample(&example);

	zest_Start();
	zest_DestroyContext();

	return 0;
}
#else
int main(void) {

	zest_create_info_t create_info = zest_CreateInfo();

	zest_example example;
    
	zest_CreateContext(&create_info);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(UpdateCallback);

	InitExample(&example);

	zest_Start();

	return 0;
}
#endif
