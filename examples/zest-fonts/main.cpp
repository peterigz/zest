#include <zest.h>

typedef struct zest_example {
	zest_index font_index;
	zest_index font_layer;
} zest_example;

void InitExample(zest_example *example) {
	zest_ModifyCommandQueue(ZestApp->default_command_queue_index);
	{
		zest_ModifyDrawCommands(ZestApp->default_render_commands_index);
		{
			example->font_layer = zest_NewBuiltinLayerSetup("Fonts", zest_builtin_layer_fonts);
		}
		zest_FinishQueueSetup();
	}

	example->font_index = zest_LoadFont("fonts/Karla-Regular.zft");
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	zest_example *example = (zest_example*)user_data;
	zest_Update2dUniformBuffer();
	zest_SetActiveRenderQueue(0);

	zest_instance_layer *font_layer = zest_GetInstanceLayerByIndex(example->font_layer);
	zest_font *font = zest_GetFont(example->font_index);

	font_layer->multiply_blend_factor = 1.f;
	font_layer->push_constants = { 0 };
	zest_SetFontDrawing(font_layer, example->font_index, font->descriptor_set_index, font->pipeline_index);
	zest_DrawText(font_layer, "Zest fonts drawn using MSDF!", zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .5f, .5f, .5, 50.f, 0.f, 1.f);
}

int main(void) {

	zest_create_info create_info = zest_CreateInfo();

	zest_example example;

	zest_Initialise(&create_info);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(UpdateCallback);

	InitExample(&example);

	zest_Start();

	return 0;
}