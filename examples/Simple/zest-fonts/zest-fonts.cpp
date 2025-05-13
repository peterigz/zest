#include <zest.h>

typedef struct zest_example {
	zest_font font;
	zest_layer font_layer;
} zest_example;

void InitExample(zest_example *example) {
	//Modify the existing builtin command queue
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			//Add a new draw routine layer for our font drawing
			example->font_layer = zest_NewBuiltinLayerSetup("Fonts", zest_builtin_layer_fonts);
		}
		//That's it, finish the queue setup
		zest_FinishQueueSetup();
	}

	//Load a font and store the handle. MSDF fonts are in the zft format which you can create using zest-msdf-font-maker
	example->font = zest_LoadMSDFFont("examples/assets/KaushanScript-Regular.zft");
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	//Get the example struct from the user data we set with zest_SetUserData
	zest_example *example = (zest_example*)user_data;
	//Update the uniform buffer.
	zest_Update2dUniformBuffer();
	//Set the current active command queue that we modified in InitExample
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);

	//Set the instensity of the font
	example->font_layer->intensity = 1.f;
	//Let the layer know that we want to draw text with the following font
	zest_SetMSDFFontDrawing(example->font_layer, example->font);
	//Draw a paragraph of text
	zest_DrawMSDFParagraph(example->font_layer, "Zest fonts drawn using MSDF!\nHere are some numbers 12345\nLorem ipsum and all that\nHello Sailer!\n", 0.f, 0.f, 0.f, 0.f, 50.f, 0.f, 1.f);
	//Draw a single line of text
	zest_DrawMSDFText(example->font_layer, "(This should be centered)", zest_ScreenWidth() * .5f, zest_ScreenHeightf() * .5f, .5f, .5f, 50.f, 0.f);

}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
//int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
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
