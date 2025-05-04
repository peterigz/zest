#include <zest.h>

typedef struct zest_example {
	zest_texture texture;						//A handle to the texture that will contain the bunny image
	zest_image image;							//A handle to image in the texture for the bunny image
	zest_pipeline line_pipeline;				//The builtin sprite pipeline that will drawing sprites
	zest_layer line_layer;						//The builtin sprite layer that contains the vertex buffer for drawing the sprites
	zest_descriptor_set line_descriptor;		//Hanlde for the billboard descriptor
} zest_example;

void InitExample(zest_example *example) {
	//Create a new texture for storing the bunny image
	example->texture = zest_CreateTexture("Example Texture", zest_texture_storage_type_bank, zest_texture_flag_use_filtering, zest_texture_format_rgba_unorm, 10);
	//Load in the bunny image from file and add it to the texture
	example->image = zest_AddTextureImageFile(example->texture, "examples/assets/wabbit_alpha.png");
	//Process the texture which will create the resources on the GPU for sampling from the bunny image texture
	zest_ProcessTextureImages(example->texture);
	//To save having to lookup these handles in the mainloop, we can look them up here in advance and store the handles in our example struct
	example->line_pipeline = zest_Pipeline("pipeline_line_instance");
	//Get the sprite draw commands and set the clear color for its render pass
	zest_command_queue_draw_commands sprite_draw = zest_GetDrawCommands("Default Draw Commands");
	sprite_draw->cls_color = zest_Vec4Set1(0.25f);

	//Modify the default command queue to add a billboard and mesh layer.
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			//Create a new billboard layer in the command queue
			example->line_layer = zest_NewBuiltinLayerSetup("Lines", zest_builtin_layer_lines);
		}
		//Finish modifying the queue
		zest_FinishQueueSetup();
	}
}

void test_update_callback(zest_microsecs elapsed, void *user_data) {
	//Get the example struct from the user data we set with zest_SetUserData
	zest_example *example = (zest_example*)user_data;
	//Update the builtin 2d uniform buffer. 
	zest_Update2dUniformBuffer();
	//It's important to set the command queue you want to use each frame otherwise you will just see a blank screen
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);

	zest_SetShapeDrawing(example->line_layer, zest_shape_dashed_line, example->line_pipeline->shader_resources, example->line_pipeline);
	zest_SetLayerColor(example->line_layer, 255, 255, 255, 100);
	zest_vec2 start = zest_Vec2Set(20.f, 20.f);
	zest_vec2 end = zest_Vec2Set((float)ZestApp->mouse_x, (float)ZestApp->mouse_y);
	zest_DrawLine(example->line_layer, &start.x, &end.x, 10.f);

	zest_SetShapeDrawing(example->line_layer, zest_shape_rect, example->line_pipeline->shader_resources, example->line_pipeline);
	zest_vec2 top_left = zest_Vec2Set((float)ZestApp->mouse_x, (float)ZestApp->mouse_y);
	zest_DrawRect(example->line_layer, &top_left.x, 30.f, 50.f);
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
//int main(void) 
{
	zest_example example = { 0 };

	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_use_depth_buffer);
	create_info.log_path = "./";

	zest_Initialise(&create_info);
	zest_LogFPSToConsole(1);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(test_update_callback);

	InitExample(&example);

	zest_Start();

	return 0;
}
#else
int main(void) {
	zest_example example = { 0 };

	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);

	zest_Initialise(&create_info);
	zest_LogFPSToConsole(1);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(test_update_callback);

	InitExample(&example);

	zest_Start();

	return 0;
}
#endif
