#include <zest.h>
//Demonstrate deleting images from a texture that is currently in use by utilising zest_RemoveTextureImage and zest_ScheduleTextureReprocess for
//safetly deleting the images when the texture will not be in use by the renderer

struct App {
	zest_texture texture;					//Handle to the texture containing the images
	zest_image image1;						//Handle to a test image
	zest_image image2;						//Handle to a test image
	zest_image animation;					//Handle to a test animation
	zest_pipeline sprite_pipeline;			//Handle to the sprite handle to save looking it up each frame
	zest_timer timer;						//Time to delete images after a few seconds
	zest_microsecs period;					//Time between each delete
};

void InitialiseApp(App *app) {
	//Create a new texture to store the images
	app->texture = zest_CreateTexturePacked("Test texture", zest_texture_format_rgba);
	//Add an animation to the texture
	app->animation= zest_AddTextureAnimationFile(app->texture, "examples/assets/vaders/vader_bullet_64_64_16.png", 16, 16, 64, 0, 1);
	//Add a couple of images to the texture
	app->image1 = zest_AddTextureImageFile(app->texture, "examples/assets/texture.jpg");
	app->image2 = zest_AddTextureImageFile(app->texture, "examples/assets/wabbit_alpha.png");
	//Store a handle to the sprite pipeline
	app->sprite_pipeline = zest_Pipeline("pipeline_2d_sprites");
	//Process the texture to pack all the images into the texture
	zest_ProcessTextureImages(app->texture);
	//Create a timer and reset it
	app->timer = zest_CreateTimer(60);
	zest_TimerReset(app->timer);
	//Set the period to 3 seconds
	app->period = ZEST_SECONDS_IN_MICROSECONDS(3);
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	App *app = static_cast<App*>(user_data);
	//Set the active command queue to the default command queue
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);
	//Update the builtin 2d uniform buffer
	zest_Update2dUniformBuffer();
	//Start the sprite drawing
	zest_SetInstanceDrawing(ZestApp->default_layer, app->texture, 0, app->sprite_pipeline);
	//Draw the first sprite, this one won't be deleted
	zest_DrawSprite(ZestApp->default_layer, app->image2, 200.f, 200.f, 0.f, (float)app->image2->width, (float)app->image2->height, 0.5f, 0.5f, 0, 0.f);
	if (app->image1 && zest_Microsecs() - app->timer->start_time > app->period) {
		//if image1 still exists and the time is up, remove the image from the texture
		zest_RemoveTextureImage(app->texture, app->image1);
		//Null out the image handle
		app->image1 = 0;
		//Schedule the texture for reprocessing when we know the texture won't be in use
		zest_ScheduleTextureReprocess(app->texture, 0);
	}
	else if(app->image1) {
		//Draw the sprite if it still exists
		zest_DrawSprite(ZestApp->default_layer, app->image1, 400.f, 200.f, 0.f, 128.f, 128.f, 0.5f, 0.5f, 0, 0.f);
	}

	if (app->animation && zest_Microsecs() - app->timer->start_time > app->period * 2) {
		//Remove the animation if it still exists and the time is up.
		zest_RemoveTextureAnimation(app->texture, app->animation);
		//Schedule the texture for reprocessing when we know the texture won't be in use
		zest_ScheduleTextureReprocess(app->texture, 0);
		//Null out the animation handle
		app->animation = 0;
	}
	else if (app->animation) {
		//Draw the animation if it still exists
		zest_DrawSprite(ZestApp->default_layer, app->animation, 600.f, 200.f, 0.f, 16.f, 16.f, 0.5f, 0.5f, 0, 0.f);
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
int main(void) 
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
    
    App app = { 0 };

	zest_Initialise(&create_info);
    zest_SetUserData(&app);
	zest_SetUserUpdateCallback(UpdateCallback);
    
    InitialiseApp(&app);

	zest_Start();

	return 0;
}
#endif
