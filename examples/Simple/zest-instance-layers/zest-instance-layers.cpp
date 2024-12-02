#include <zest.h>

typedef struct zest_example {
	zest_texture texture;						//A handle to the texture that will contain the bunny image
	zest_image image;							//A handle to image in the texture for the bunny image
	zest_pipeline sprite_pipeline;				//The builtin sprite pipeline that will drawing sprites
	zest_layer sprite_layer;					//The builtin sprite layer that contains the vertex buffer for drawing the sprites
	zest_layer billboard_layer;					//A builtin billboard layer for drawing billboards
	zest_layer mesh_layer;						//A builtin mesh layer that we will use to draw a texured plane
	zest_pipeline billboard_pipeline;			//The pipeline for drawing billboards
	zest_pipeline mesh_pipeline;				//The pipeline for drawing the textured plane
	zest_camera_t camera;						//A camera for the 3d view
	zest_uniform_buffer uniform_buffer_3d;		//A uniform buffer to contain the projection and view matrix
	zest_descriptor_set uniform_descriptor_set_3d;		//A descriptor set for the 3d uniform buffer
	zest_shader_resources sprite_shader_resources;		//Handle for the sprite descriptor
	zest_shader_resources billboard_shader_resources;	//Handle for the billboard shader resources
	zest_vec3 last_position;
} zest_example;

void UpdateUniformBuffer3d(zest_example *example) {
	//Update our 3d uniform buffer using the camera in the example
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(example->uniform_buffer_3d);
	buffer_3d->view = zest_LookAt(example->camera.position, zest_AddVec3(example->camera.position, example->camera.front), example->camera.up);
	buffer_3d->proj = zest_Perspective(example->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.1f, 10000.f);
	buffer_3d->proj.v[1].y *= -1.f;
	buffer_3d->screen_size.x = zest_ScreenWidthf();
	buffer_3d->screen_size.y = zest_ScreenHeightf();
	buffer_3d->millisecs = 0;
}

void InitExample(zest_example *example) {
	//Create a new texture for storing the bunny image
	example->texture = zest_CreateTexture("Example Texture", zest_texture_storage_type_bank, zest_texture_flag_use_filtering, zest_texture_format_rgba, 10);
	//Load in the bunny image from file and add it to the texture
	example->image = zest_AddTextureImageFile(example->texture, "examples/assets/wabbit_alpha.png");
	//Process the texture which will create the resources on the GPU for sampling from the bunny image texture
	zest_ProcessTextureImages(example->texture);
	//To save having to lookup these handles in the mainloop, we can look them up here in advance and store the handles in our example struct
	example->sprite_pipeline = zest_Pipeline("pipeline_2d_sprites");
	example->sprite_layer = zest_GetLayer("Sprite 2d Layer");
	example->sprite_shader_resources = zest_CombineUniformAndTextureSampler(ZestRenderer->uniform_descriptor_set, example->texture);
	example->billboard_pipeline = zest_Pipeline("pipeline_billboard");
	example->mesh_pipeline = zest_Pipeline("pipeline_mesh");
	//Create a new uniform buffer for the 3d view
	example->uniform_buffer_3d = zest_CreateUniformBuffer("example 3d uniform", sizeof(zest_uniform_buffer_data_t));
	example->uniform_descriptor_set_3d = zest_CreateUniformDescriptorSet(example->uniform_buffer_3d);
	//Create a new descriptor set to use the 3d uniform buffer
	example->billboard_shader_resources = zest_CombineUniformAndTextureSampler(example->uniform_descriptor_set_3d, example->texture);
	//Get the sprite draw commands and set the clear color for its render pass
	zest_command_queue_draw_commands sprite_draw = zest_GetDrawCommands("Default Draw Commands");
	sprite_draw->cls_color = zest_Vec4Set1(0.25f);

	//Create a camera for the 3d view
	example->camera = zest_CreateCamera();
	zest_CameraSetFoV(&example->camera, 60.f);

	//Modify the default command queue to add a billboard and mesh layer.
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			//Create a new mesh layer in the command queue. If you want depth testing to work then the layer with depth write enabled
			//should come before the layer with only depth test enabled.
			example->mesh_layer = zest_NewBuiltinLayerSetup("Meshes", zest_builtin_layer_mesh);
			//Create a new billboard layer in the command queue
			example->billboard_layer = zest_NewBuiltinLayerSetup("Billboards", zest_builtin_layer_billboards);
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
	//Also update the 3d uniform buffer for the billboard drawing
	UpdateUniformBuffer3d(example);
	//It's important to set the command queue you want to use each frame otherwise you will just see a blank screen
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);

	//Set the current intensity of the sprite layer
	example->sprite_layer->intensity = 1.f;
	//You must call this command before doing any sprite draw to set the current texture, descriptor and pipeline to draw with.
	//Call this function anytime that you need to draw with a different texture. Note that a single texture and contain many images
	//you can draw a lot with a single draw call
	zest_SetInstanceDrawing(example->sprite_layer, example->sprite_shader_resources, example->sprite_pipeline);
	//Set the alpha of the sprite layer to 0. This means that the sprites will be additive. 1 = alpha blending and anything imbetween
	//is a mix between the two.
	example->sprite_layer->current_color.a = 0;
	for (float x = 0; x != 25; ++x) {
		for (float y = 0; y != 15; ++y) {
			example->sprite_layer->current_color.r = (zest_byte)(1 - ((y + 1) / 16.f) * 255.f);
			example->sprite_layer->current_color.g = (zest_byte)(y / 15.f * 255.f);
			example->sprite_layer->current_color.b = (zest_byte)(x / 75.f * 255.f);
			//Draw a sprite to the screen.
			zest_DrawSprite(example->sprite_layer, example->image, x * 16.f + 20.f, y * 40.f + 20.f, 0.f, 32.f, 32.f, 0.5f, 0.5f, 0, 0.f);
		}
	}

	//Draw a textured sprite to the sprite (basically a textured rect). 
	zest_DrawTexturedSprite(example->sprite_layer, example->image, 600.f, 100.f, 500.f, 500.f, 1.f, 1.f, 0.f, 0.f);

	//Now lets draw a billboard. Similar to the sprite, we must call this command before any billboard drawing.
	zest_SetInstanceDrawing(example->billboard_layer, example->billboard_shader_resources, example->billboard_pipeline);
	//Get a pointer to the uniform buffer data and cast it to the struct that we're storing there
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(example->uniform_buffer_3d);
	//Use the projection and view matrices in the buffer to project the mouse coordinates into 3d space.
	zest_vec3 ray = zest_ScreenRay((float)ZestApp->mouse_x, (float)ZestApp->mouse_y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	//Scale the ray into the 3d space by 10 and add the camera position.
	zest_vec3 position = zest_AddVec3(zest_ScaleVec3(ray, 10.f), example->camera.position);
	//Set some values to draw the billboard with
	zest_vec3 angles = { 0 };
	zest_vec3 handle = { .5f, .5f };
	float scale_x = (float)ZestApp->mouse_x * 5.f / zest_ScreenWidthf();
	float scale_y = (float)ZestApp->mouse_y * 5.f / zest_ScreenHeightf();
	//Draw the billboard
	zest_DrawBillboardSimple(example->billboard_layer, example->image, &position.x, angles.x, scale_x, scale_y);
	//Now set the mesh drawing so that we can draw a textured plane
	zest_SetMeshDrawing(example->mesh_layer, example->billboard_shader_resources, example->mesh_pipeline);
	//Draw the textured plane
	zest_DrawTexturedPlane(example->mesh_layer, example->image, -500.f, -5.f, -500.f, 1000.f, 1000.f, 50.f, 50.f, 0.f, 0.f);
	example->last_position = position;
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
// int main(void)
{
	zest_example example = { 0 };

	zest_uint test = zest_Pack8bit(0, 1.f, 0);

	zest_create_info_t create_info = zest_CreateInfo();
	//ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_use_depth_buffer);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
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
