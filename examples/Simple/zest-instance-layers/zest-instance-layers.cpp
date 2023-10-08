#include <zest.h>

typedef struct zest_example {
	zest_texture texture;
	zest_image image;
	zest_pipeline sprite_pipeline;
	zest_layer sprite_layer;
	zest_layer billboard_layer;
	zest_layer mesh_layer;
	zest_pipeline billboard_pipeline;
	zest_pipeline mesh_pipeline;
	zest_camera_t camera;
	zest_descriptor_buffer uniform_buffer_3d;
	zest_descriptor_set sprite_descriptor;
	zest_descriptor_set billboard_descriptor;
} zest_example;

void UpdateUniformBuffer3d(zest_example *example) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(example->uniform_buffer_3d);
	buffer_3d->view = zest_LookAt(example->camera.position, zest_AddVec3(example->camera.position, example->camera.front), example->camera.up);
	buffer_3d->proj = zest_Perspective(example->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.1f, 10000.f);
	buffer_3d->proj.v[1].y *= -1.f;
	buffer_3d->screen_size.x = zest_ScreenWidthf();
	buffer_3d->screen_size.y = zest_ScreenHeightf();
	buffer_3d->millisecs = 0;
}

void InitExample(zest_example *example) {
	example->texture = zest_CreateTexture("Example Texture", zest_texture_storage_type_bank, zest_texture_flag_use_filtering, zest_texture_format_rgba, 10);
	example->image = zest_AddTextureImageFile(example->texture, "examples/assets/wabbit_alpha.png");
	zest_ProcessTextureImages(example->texture);
	example->sprite_pipeline = zest_Pipeline("pipeline_2d_sprites");
	example->sprite_layer = zest_GetLayer("Sprite 2d Layer");
	example->sprite_descriptor = zest_GetTextureDescriptorSet(example->texture, "Default");
	example->billboard_pipeline = zest_Pipeline("pipeline_billboard");
	example->mesh_pipeline = zest_Pipeline("pipeline_mesh");
	example->uniform_buffer_3d = zest_CreateUniformBuffer("example 3d uniform", sizeof(zest_uniform_buffer_data_t));
	example->billboard_descriptor = zest_CreateTextureSpriteDescriptorSets(example->texture, "3d", "example 3d uniform");
	zest_RefreshTextureDescriptors(example->texture);
	zest_command_queue_draw_commands sprite_draw = zest_GetDrawCommands("Default Draw Commands");
	sprite_draw->cls_color = zest_Vec4Set1(0.25f);

	example->camera = zest_CreateCamera();
	zest_CameraSetFoV(&example->camera, 60.f);

	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			example->mesh_layer = zest_NewBuiltinLayerSetup("Meshes", zest_builtin_layer_mesh);
			example->billboard_layer = zest_NewBuiltinLayerSetup("Billboards", zest_builtin_layer_billboards);
		}
		zest_FinishQueueSetup();
	}
}

void test_update_callback(zest_microsecs elapsed, void *user_data) {
	zest_example *example = (zest_example*)user_data;
	zest_Update2dUniformBuffer();
	UpdateUniformBuffer3d(example);
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);
	example->sprite_layer->intensity = 1.f;

	zest_SetSpriteDrawing(example->sprite_layer, example->texture, example->sprite_descriptor, example->sprite_pipeline);
	example->sprite_layer->current_color.a = 0;
	for (float x = 0; x != 25; ++x) {
		for (float y = 0; y != 15; ++y) {
			example->sprite_layer->current_color.r = (zest_byte)(1 - ((y + 1) / 16.f) * 255.f);
			example->sprite_layer->current_color.g = (zest_byte)(y / 15.f * 255.f);
			example->sprite_layer->current_color.b = (zest_byte)(x / 75.f * 255.f);
			zest_DrawSprite(example->sprite_layer, example->image, x * 16.f + 20.f, y * 40.f + 20.f, 0.f, 32.f, 32.f, 0.5f, 0.5f, 0, 0.f, 0);
		}
	}

	zest_DrawTexturedSprite(example->sprite_layer, example->image, 600.f, 100.f, 500.f, 500.f, 1.f, 1.f, 0.f, 0.f);

	zest_SetBillboardDrawing(example->billboard_layer, example->texture, example->billboard_descriptor, example->billboard_pipeline);
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(example->uniform_buffer_3d);
	zest_vec3 ray = zest_ScreenRay((float)ZestApp->mouse_x, (float)ZestApp->mouse_y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	zest_vec3 position = zest_AddVec3(zest_ScaleVec3(&ray, 10.f), example->camera.position);
	zest_vec3 angles = { 0 };
	zest_vec3 handle = { .5f, .5f };
	zest_vec3 alignment = zest_Vec3Set(0.5f, 0.5f, 1.f);
	alignment = zest_NormalizeVec3(alignment);
	float scale_x = (float)ZestApp->mouse_x * 5.f / zest_ScreenWidthf();
	float scale_y = (float)ZestApp->mouse_y * 5.f / zest_ScreenHeightf();
	zest_DrawBillboard(example->billboard_layer, example->image, &position.x, zest_Pack10bit(&alignment, 0), &angles.x, &handle.x, 10.f, 0, 1.f, 1.f);
	zest_SetMeshDrawing(example->mesh_layer, example->texture, example->billboard_descriptor, example->mesh_pipeline);
	zest_DrawTexturedPlane(example->mesh_layer, example->image, -500.f, -5.f, -500.f, 1000.f, 1000.f, 50.f, 50.f, 0.f, 0.f);
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
int main(void) 
{
	zest_example example = { 0 };

	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_use_depth_buffer);

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
