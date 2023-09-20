#include <zest.h>

struct App {
	zest_texture texture;
	zest_image image1;
	zest_image image2;
	zest_image animation;
	zest_pipeline sprite_pipeline;
	zest_timer timer;
	zest_microsecs period;
};

void InitialiseApp(App *app) {
	app->texture = zest_CreateTexturePacked("Test texture", zest_texture_format_rgba);
	app->animation= zest_AddTextureAnimationFile(app->texture, "examples/assets/vaders/vader_bullet_64_64_16.png", 16, 16, 64, 0, 1);
	app->image1 = zest_AddTextureImageFile(app->texture, "examples/assets/texture.jpg");
	app->image2 = zest_AddTextureImageFile(app->texture, "examples/assets/wabbit_alpha.png");
	app->sprite_pipeline = zest_Pipeline("pipeline_2d_sprites");
	zest_ProcessTextureImages(app->texture);
	app->timer = zest_CreateTimer(60);
	zest_TimerReset(app->timer);
	app->period = ZEST_SECONDS_IN_MICROSECONDS(3);
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	App *app = static_cast<App*>(user_data);
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);
	zest_Update2dUniformBuffer();
	zest_SetSpriteDrawing(ZestApp->default_layer, app->texture, 0, app->sprite_pipeline);
	zest_DrawSprite(ZestApp->default_layer, app->image2, 200.f, 200.f, 0.f, (float)app->image2->width, (float)app->image2->height, 0.5f, 0.5f, 0, 0.f, 0);
	if (app->image1 && zest_Microsecs() - app->timer->start_time > app->period) {
		zest_RemoveTextureImage(app->texture, app->image1);
		app->image1 = 0;
		zest_ScheduleTextureReprocess(app->texture);
	}
	else if(app->image1) {
		zest_DrawSprite(ZestApp->default_layer, app->image1, 400.f, 200.f, 0.f, 128.f, 128.f, 0.5f, 0.5f, 0, 0.f, 0);
	}

	if (app->animation && zest_Microsecs() - app->timer->start_time > app->period * 2) {
		zest_RemoveTextureAnimation(app->texture, app->animation);
		zest_ScheduleTextureReprocess(app->texture);
		app->animation = 0;
	}
	else if (app->animation) {
		zest_DrawSprite(ZestApp->default_layer, app->animation, 600.f, 200.f, 0.f, 16.f, 16.f, 0.5f, 0.5f, 0, 0.f, 0);
	}
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);

	App app = { 0 };

	zest_Initialise(&create_info);
	zest_SetUserData(&app);
	zest_SetUserUpdateCallback(UpdateCallback);

	InitialiseApp(&app);

	zest_Start();

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);

	zest_Initialise(&create_info);
    zest_LogFPSToConsole(1);
	zest_SetUserUpdateCallback(UpdateCallback);

	zest_Start();

	return 0;
}
#endif
