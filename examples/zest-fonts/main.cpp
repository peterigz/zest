#include <zest.h>

typedef struct zest_example {
	zest_font font;
	zest_layer font_layer;
} zest_example;

void InitExample(zest_example *example) {
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			example->font_layer = zest_NewBuiltinLayerSetup("Fonts", zest_builtin_layer_fonts);
		}
		zest_FinishQueueSetup();
	}

	example->font = zest_LoadMSDFFont("fonts/KaushanScript-Regular.zft");
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	zest_example *example = (zest_example*)user_data;
	zest_Update2dUniformBuffer();
	zest_SetActiveRenderQueue(ZestApp->default_command_queue);

	example->font_layer->multiply_blend_factor = 1.f;
	zest_SetMSDFFontDrawing(example->font_layer, example->font, example->font->descriptor_set_index, example->font->pipeline_index);
	zest_DrawMSDFParagraph(example->font_layer, "Zest fonts drawn using MSDF!\nHere are some numbers 12345\nLorem ipsum and all that\nHello Sailer!\n", 0.f, 0.f, 0.f, 0.f, 50.f, 0.f, 1.f, 1.f);
	zest_DrawMSDFText(example->font_layer, "(This should be centered)", zest_ScreenWidth() * .5f, zest_ScreenHeightf() * .5f, .5f, .5f, 50.f, 0.f, 1.f);

}

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