#include <zest.h>

typedef struct zest_example {
	zest_texture texture;
	zest_image image;
	zest_pipeline sprite_pipeline;
	zest_layer sprite_layer;
	zest_layer billboard_layer;
	zest_pipeline billboard_pipeline;
	zest_camera_t camera;
	zest_uniform_buffer uniform_buffer_3d;
	zest_index sprite_descriptor_index;
	zest_index billboard_descriptor_index;
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
	example->texture = zest_CreateTexture("Example Texture", zest_texture_storage_type_sprite_sheet, zest_texture_flag_use_filtering, zest_texture_format_alpha, 10);
	example->image = zest_AddTextureImageFile(example->texture, "examples/wabbit_alpha.png");
	zest_ProcessTextureImages(example->texture);
	example->sprite_pipeline = zest_Pipeline("pipeline_2d_sprites_alpha");
	example->sprite_layer = zest_GetLayer("Sprite 2d Layer");
	example->sprite_descriptor_index = zest_GetTextureDescriptorSetIndex(example->texture, "Default");
	example->billboard_pipeline = zest_Pipeline("pipeline_billboard_alpha");
	example->uniform_buffer_3d = zest_CreateUniformBuffer("example 3d uniform", sizeof(zest_uniform_buffer_data_t));
	example->billboard_descriptor_index = zest_CreateTextureDescriptorSets(example->texture, "3d", "example 3d uniform");
	zest_RefreshTextureDescriptors(example->texture);
	zest_command_queue_draw_commands sprite_draw = zest_GetDrawCommands("Default Draw Commands");
	sprite_draw->cls_color = zest_Vec4Set1(0.25f);

	example->camera = zest_CreateCamera();
	zest_CameraSetFoV(&example->camera, 60.f);

	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			example->billboard_layer = zest_NewBuiltinLayerSetup("Billboards", zest_builtin_layer_billboards);
		}
		zest_FinishQueueSetup();
	}
}

void test_update_callback(zest_microsecs elapsed, void *user_data) {
	zest_example *example = (zest_example*)user_data;
	zest_Update2dUniformBuffer();
	UpdateUniformBuffer3d(example);
	zest_SetActiveRenderQueue(ZestApp->default_command_queue);
	example->sprite_layer->multiply_blend_factor = 1.f;

	zest_SetSpriteDrawing(example->sprite_layer, example->texture, example->sprite_descriptor_index, example->sprite_pipeline);
	example->sprite_layer->current_color.a = 0;
	for (float x = 0; x != 75; ++x) {
		for (float y = 0; y != 15; ++y) {
			example->sprite_layer->current_color.r = (zest_byte)(1 - ((y + 1) / 16.f) * 255.f);
			example->sprite_layer->current_color.g = (zest_byte)(y / 15.f * 255.f);
			example->sprite_layer->current_color.b = (zest_byte)(x / 75.f * 255.f);
			zest_DrawSprite(example->sprite_layer, example->image, x * 16.f + 20.f, y * 40.f + 20.f, 0.f, 32.f, 32.f, 0.5f, 0.5f, 0, 0.f, 0);
		}
	}

	zest_SetBillboardDrawing(example->billboard_layer, example->texture, example->billboard_descriptor_index, example->billboard_pipeline);
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(example->uniform_buffer_3d);
	zest_vec3 ray = zest_ScreenRay(200.f, 200.f, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	zest_vec3 position = zest_AddVec3(zest_ScaleVec3(&ray, 10.f), example->camera.position);
	zest_vec3 angles = { 0 };
	zest_vec3 handle = { .5f, .5f };
	zest_vec3 alignment = zest_Vec3Set(1.f, 0.f, 0.f);
	zest_DrawBillboard(example->billboard_layer, example->image, &position.x, zest_Pack8bitx3(&alignment), &angles.x, &handle.x, 0.f, 0, 1.f, 1.f);
}

int main(void) {

	zest_example example = { 0 };

	zest_create_info_t create_info = zest_CreateInfo();
	//create_info.flags |= zest_init_flag_use_depth_buffer;

	zest_Initialise(&create_info);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(test_update_callback);

	InitExample(&example);

	zest_Start();

	return 0;
}