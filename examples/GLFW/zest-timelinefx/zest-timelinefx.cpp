#include <zest.h>
#include "imgui.h"
#include "impl_glfw.h"
#include "impl_imgui_glfw.h"
#include "timelinefx.h"

using namespace tfx;

#define x_distance 0.078f
#define y_distance 0.158f
#define x_spacing 0.040f
#define y_spacing 0.058f

#define Rad90 1.5708f
#define Rad270 4.71239f

typedef unsigned int u32;

#define UpdateFrequency 0.016666666666f
#define FrameLength 16.66666666667f

struct TimelineFXExample {
	zest_timer timer;
	zest_camera_t camera;
	zest_texture particle_texture;
	zest_texture imgui_font_texture;

	tfx_library_t library;
	tfx_particle_manager_t pm;

	tfx_effect_template_t effect_template1;
	tfx_effect_template_t effect_template2;

	tfxEffectID effect_id;
	zest_layer billboard_layer;
	zest_descriptor_buffer uniform_buffer_3d;
	zest_descriptor_set particle_descriptor;
	zest_pipeline billboard_pipeline;

	zest_imgui_layer_info imgui_layer_info;

	void Init();
};

//Basic function for updating the uniform buffer
void UpdateUniform3d(TimelineFXExample *game) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(game->uniform_buffer_3d);
	buffer_3d->view = zest_LookAt(game->camera.position, zest_AddVec3(game->camera.position, game->camera.front), game->camera.up);
	buffer_3d->proj = zest_Perspective(game->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.1f, 10000.f);
	buffer_3d->proj.v[1].y *= -1.f;
	buffer_3d->screen_size.x = zest_ScreenWidthf();
	buffer_3d->screen_size.y = zest_ScreenHeightf();
	buffer_3d->millisecs = 0;
}

//Before you load an effects file, you will need to define a ShapeLoader function that passes the following parameters:
//const char* filename			- this will be the filename of the image being loaded from the library. You don't have to do anything with this if you don't need to.
//ImageData	&image_data			- A struct containing data about the image. You will have to set image_data.ptr to point to the texture in your renderer for later use in the Render function that you will create to render the particles
//void *raw_image_data			- The raw data of the image which you can use to load the image into graphics memory
//int image_memory_size			- The size in bytes of the raw_image_data
//void *custom_data				- This allows you to pass through an object you can use to access whatever is necessary to load the image into graphics memory, depending on the renderer that you're using
void ShapeLoader(const char* filename, tfx_image_data_t *image_data, void *raw_image_data, int image_memory_size, void *custom_data) {
	//Cast your custom data, this can be anything you want
	TimelineFXExample *game = static_cast<TimelineFXExample*>(custom_data);

	//This shape loader example uses the STB image library to load the raw bitmap (png usually) data
	zest_bitmap_t bitmap = zest_NewBitmap();
	zest_LoadBitmapImageMemory(&bitmap, (unsigned char*)raw_image_data, image_memory_size, 0);
	//Convert the image to RGBA which is necessary for this particular renderer
	zest_ConvertBitmapToRGBA(&bitmap, 255);
	//The editor has the option to convert an bitmap to an alpha map. I will probably change this so that it gets baked into the saved effect so you won't need to apply the filter here.
	//Alpha map is where all color channels are set to 255
	if (image_data->import_filter)
		zest_ConvertBitmapToAlpha(&bitmap);

	//Get the texture where we're storing all the particle shapes
	//You'll probably need to load the image in such a way depending on whether or not it's an animation or not
	if (image_data->animation_frames > 1) {
		//Add the spritesheet to the texture in our renderer
		float max_radius = 0;
		image_data->ptr = zest_AddTextureAnimationBitmap(game->particle_texture, &bitmap, (u32)image_data->image_size.x, (u32)image_data->image_size.y, (u32)image_data->animation_frames, &max_radius, 1);
		//Important step: you need to point the ImageData.ptr to the appropriate handle in the renderer to point to the texture of the particle shape
		//You'll need to use this in your render function to tell your renderer which texture to use to draw the particle
	}
	else {
		//Add the image to the texture in our renderer
		image_data->ptr = zest_AddTextureImageBitmap(game->particle_texture, &bitmap);
		//Important step: you need to point the ImageData.ptr to the appropriate handle in the renderer to point to the texture of the particle shape
		//You'll need to use this in your render function to tell your renderer which texture to use to draw the particle
	}
}

void GetUV(void *ptr, tfx_gpu_image_data_t *image_data, int offset) {
	zest_image image = (static_cast<zest_image>(ptr) + offset);
	image_data->uv = { image->uv.x, image->uv.y, image->uv.z, image->uv.w };
	image_data->texture_array_index = image->layer;
	image_data->uv_packed = image->uv_packed;
}

//Allows us to cast a ray into the screen from the mouse position to place an effect where we click
tfx_vec3_t ScreenRay(float x, float y, float depth_offset, zest_vec3 &camera_position, zest_descriptor_buffer buffer) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

void TimelineFXExample::Init() {
	//Renderer specific - initialise the texture
	float max_radius = 0;
	uniform_buffer_3d = zest_CreateUniformBuffer("3d uniform", sizeof(zest_uniform_buffer_data_t));
	billboard_pipeline = zest_Pipeline("pipeline_billboard");

	int shape_count = GetShapeCountInLibrary("examples/assets/vaders/vadereffects.tfx");
	particle_texture = zest_CreateTexture("Particle Texture", zest_texture_storage_type_packed, zest_texture_flag_use_filtering, zest_texture_format_rgba, shape_count);
	//Load the effects library and pass the shape loader function pointer that you created earlier. Also pass this pointer to point to this object to give the shapeloader access to the texture we're loading the particle images into
	LoadEffectLibrary("examples/assets/effects.tfx", &library, ShapeLoader, GetUV, this);
	//Renderer specific
	zest_ProcessTextureImages(particle_texture);
	particle_descriptor = zest_CreateSimpleTextureDescriptorSet(particle_texture, "3d uniform");
	zest_RefreshTextureDescriptors(particle_texture);

	//Application specific, set up a timer for the update loop
	timer = zest_CreateTimer(60);

	camera = zest_CreateCamera();
	zest_CameraSetFoV(&camera, 60.f);

	UpdateUniform3d(this);

	/*
	Initialise a particle manager. This manages effects, emitters and the particles that they spawn. First call CreateParticleManagerInfo and pass in a setup mode to create an info object with the config we need.
	If you need to you can tweak this further before passing into InitializingParticleManager.

	In this example we'll setup a particle manager for 3d effects and group the sprites by each effect.
	*/
	tfx_particle_manager_info_t pm_info = CreateParticleManagerInfo(tfxParticleManagerSetup_3d_group_sprites_by_effect);
	InitializeParticleManager(&pm, &library, pm_info);

	//Renderer specific that sets up some draw layers
	zest_SetDrawCommandsClsColor(zest_GetCommandQueueDrawCommands("Default Draw Commands"), 0.f, 0.f, .2f, 1.f);

	zest_imgui_Initialise(&imgui_layer_info);

	imgui_layer_info.pipeline = zest_Pipeline("pipeline_imgui");
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			billboard_layer = zest_NewBuiltinLayerSetup("Billboards", zest_builtin_layer_billboards);
			zest_imgui_CreateLayer(&imgui_layer_info);
		}
		zest_FinishQueueSetup();
	}
	zest_TimerReset(timer);
	//End of render specific code

	/*
	Prepare a tfx_effect_template_t that you can use to customise effects in the library in various ways before adding them into a particle manager for updating and rendering. Using a template like this
	means that you can tweak an effect without editing the base effect in the library.
	* @param library					A reference to a tfx_library_t that should be loaded with LoadEffectLibraryPackage
	* @param name						The name of the effect in the library that you want to use for the template. If the effect is in a folder then use normal pathing: "My Folder/My effect"
	* @param effect_template			The empty tfx_effect_template_t object that you want the effect loading into
	//Returns true on success.
	*/
	PrepareEffectTemplate(&library, "Star Burst Flash", &effect_template1);
	PrepareEffectTemplate(&library, "Star Burst Flash.1", &effect_template2);
}

//Draw a Dear ImGui window to output some basic stats
void BuildUI(TimelineFXExample *game) {
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Effects");
	ImGui::Text("Particles: %i", ParticleCount(&game->pm));
	ImGui::Text("Effects: %i", EffectCount(&game->pm));
	ImGui::Text("Emitters: %i", EmitterCount(&game->pm));
	ImGui::Text("Free Emitters: %i", game->pm.free_emitters.size());
	ImGui::End();

	ImGui::Render();
	//This will let the layer know that the mesh buffer containing all of the imgui vertex data needs to be
	//uploaded to the GPU.
	zest_SetLayerDirty(game->imgui_layer_info.mesh_layer);
}

//A simple example to render the particles. This is for when the particle manager has one single list of sprites rather than grouped by effect
void RenderParticles3d(tfx_particle_manager_t& pm, float tween, TimelineFXExample* game) {
	//Let our renderer know that we want to draw to the billboard layer.
	zest_SetInstanceDrawing(game->billboard_layer, game->particle_texture, game->particle_descriptor, game->billboard_pipeline);
	//Cycle through each layer
	//There is also a macro :tfxEachLayer which you could use like so:
	//for(tfxEachLayer) {
	//and that will output exactly the same code as the below line
	for (unsigned int layer = 0; layer != tfxLAYERS; ++layer) {
		tfx_sprite_instance_t *sprites = tfxCastBufferRef(tfx_sprite_instance_t, pm.instance_buffer[pm.current_sprite_buffer][layer]);
		zest_DrawInstanceBulk(game->billboard_layer, sprites, pm.instance_buffer[pm.current_sprite_buffer][layer].current_size);
	}
}

/*
//A simple example to render the particles. This is for when the particle manager groups all it's sprites by effect so that you can draw the effects in different orders if you need
void RenderParticles3dGroupedByEffect(tfx_particle_manager_t& pm, float tween, TimelineFXExample* game) {
	//Let our renderer know that we want to draw to the billboard layer.
	zest_SetInstanceDrawing(game->billboard_layer, game->particle_texture, game->particle_descriptor, game->billboard_pipeline);
	//Cycle through each layer
	//There is also a macro :tfxEachLayer which you could use like so:
	//for(tfxEachLayer) {
	//and that will output exactly the same code as the below line
	for (unsigned int layer = 0; layer != tfxLAYERS; ++layer) {
		//Cycle through all the active effects in the particle manager and retrieve the buffers containing everything we need to render the sprites
		tfx_sprite_soa_t* sprites = nullptr;
		tfx_effect_sprites_t* effect_sprites = nullptr;
		tfxU32 sprite_count = 0;
		while (GetNextSpriteBuffer(&pm, layer, &sprites, &effect_sprites, &sprite_count)) {
			for (int i = 0; i != sprite_count; ++i) {
				//Set the color to draw the sprite using the sprite color. Note that bacuase this is a struct of arrays we access each sprite element
				//using an array lookup
				zest_SetLayerColor(game->billboard_layer, sprites->color[i].r, sprites->color[i].g, sprites->color[i].b, sprites->color[i].a);
				//Set the instensity of the sprite.
				zest_SetLayerIntensity(game->billboard_layer, sprites->intensity[i]);
				//Grab the image pointer from the emitter properties in the library
				zest_image image = (zest_image)GetSpriteImagePointer(&pm, sprites->property_indexes[i]);
				//Grab the sprite handle which is used to offset the position of the sprite
				tfx_vec2_t handle = GetSpriteHandle(&pm, sprites->property_indexes[i]);
				//Render specific functino to draw a billboard to the screen using the values in the sprite data
				zest_DrawBillboard(game->billboard_layer, image + ((sprites->property_indexes[i] & 0x00FF0000) >> 16),
					&sprites->transform_3d[i].position.x,
					sprites->alignment[i],
					&sprites->transform_3d[i].rotations.x,
					&handle.x,
					sprites->stretch[i],
					(sprites->property_indexes[i] & 0xFF000000) >> 24,
					sprites->transform_3d[i].scale.x, sprites->transform_3d[i].scale.y);
			}
		}
	}
	ResetSpriteBufferLoopIndex(&pm);
}
*/

//Application specific, this just sets the function to call each render update
void UpdateTfxExample(zest_microsecs ellapsed, void *data) {
	TimelineFXExample *game = static_cast<TimelineFXExample*>(data);

	zest_TimerAccumulate(game->timer);

	//Renderer specific
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);

	UpdateUniform3d(game);
	zest_Update2dUniformBuffer();

	BuildUI(game);

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		//Each time you add an effect to the particle manager it generates an ID which you can use to modify the effect whilst it's being updated
		tfxEffectID effect_id;
		//Add the effect template to the particle manager
		if(AddEffectToParticleManager(&game->pm, &game->effect_template1, &effect_id)) {
			//Calculate a position in 3d by casting a ray into the screen using the mouse coordinates
			tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 12.f, game->camera.position, game->uniform_buffer_3d);
			//Set the effect position
			SetEffectPosition(&game->pm, effect_id, position);
		}
	}

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		//Each time you add an effect to the particle manager it generates an ID which you can use to modify the effect whilst it's being updated
		tfxEffectID effect_id;
		//Add the effect template to the particle manager
		if(AddEffectToParticleManager(&game->pm, &game->effect_template2, &effect_id)) {
			//Calculate a position in 3d by casting a ray into the screen using the mouse coordinates
			tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 12.f, game->camera.position, game->uniform_buffer_3d);
			//Set the effect position
			SetEffectPosition(&game->pm, effect_id, position);
		}
	}

	while (zest_TimerDoUpdate(game->timer)) {

		//Update the particle manager
		UpdateParticleManager(&game->pm, FrameLength);

		zest_TimerUnAccumulate(game->timer);
	}

	zest_TimerSet(game->timer);

	//Render the particles with our custom render function
	RenderParticles3d(game->pm, (float)game->timer->lerp, game);
	zest_imgui_UpdateBuffers(game->imgui_layer_info.mesh_layer);
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
//int main() {
	zest_vec3 v = zest_Vec3Set(1.f, 0.f, 0.f);
	zest_uint packed = zest_Pack8bitx3(&v);
	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	zest_implglfw_SetCallbacks(&create_info);

	TimelineFXExample game;
	//Initialise TimelineFX with however many threads you want. Each emitter is updated it's own thread.
	InitialiseTimelineFX(std::thread::hardware_concurrency(), tfxMegabyte(128));

	zest_Initialise(&create_info);
	zest_SetUserData(&game);
	zest_SetUserUpdateCallback(UpdateTfxExample);
	game.Init();

	zest_Start();

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	zest_implglfw_SetCallbacks(&create_info);

	ImGuiApp imgui_app;

	zest_Initialise(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#endif
