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

struct VadersGame {
	zest_timer timer;
	zest_camera_t camera;
	zest_texture particle_texture;
	zest_texture imgui_font_texture;

	tfxLibrary library;
	tfxParticleManager pm;

	tfxEffectTemplate effect_template;

	tfxEffectID effect_id;
	zest_layer billboard_layer;
	zest_descriptor_buffer uniform_buffer_3d;
	zest_descriptor_set particle_descriptor;
	zest_pipeline billboard_pipeline;

	zest_imgui_layer_info imgui_layer_info;

	void Init();
};

void UpdateUniform3d(VadersGame *game) {
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
void ShapeLoader(const char* filename, tfxImageData &image_data, void *raw_image_data, int image_memory_size, void *custom_data) {
	//Cast your custom data, this can be anything you want
	VadersGame *game = static_cast<VadersGame*>(custom_data);

	//This shape loader example uses the STB image library to load the raw bitmap (png usually) data
	zest_bitmap_t bitmap = zest_NewBitmap();
	zest_LoadBitmapImageMemory(&bitmap, (unsigned char*)raw_image_data, image_memory_size, 0);
	//Convert the image to RGBA which is necessary for this particular renderer
	zest_ConvertBitmapToRGBA(&bitmap, 255);
	//The editor has the option to convert an bitmap to an alpha map. I will probably change this so that it gets baked into the saved effect so you won't need to apply the filter here.
	//Alpha map is where all color channels are set to 255
	if (image_data.import_filter)
		zest_ConvertBitmapToAlpha(&bitmap);

	//Get the texture where we're storing all the particle shapes
	//You'll probably need to load the image in such a way depending on whether or not it's an animation or not
	if (image_data.animation_frames > 1) {
		//Add the spritesheet to the texture in our renderer
		float max_radius = 0;
		image_data.ptr = zest_AddTextureAnimationBitmap(game->particle_texture, &bitmap, (u32)image_data.image_size.x, (u32)image_data.image_size.y, (u32)image_data.animation_frames, &max_radius, 1);
		//Important step: you need to point the ImageData.ptr to the appropriate handle in the renderer to point to the texture of the particle shape
		//You'll need to use this in your render function to tell your renderer which texture to use to draw the particle
	}
	else {
		//Add the image to the texture in our renderer
		image_data.ptr = zest_AddTextureImageBitmap(game->particle_texture, &bitmap);
		//Important step: you need to point the ImageData.ptr to the appropriate handle in the renderer to point to the texture of the particle shape
		//You'll need to use this in your render function to tell your renderer which texture to use to draw the particle
	}
}

tfxVec3 ScreenRay(float x, float y, float depth_offset, zest_vec3 &camera_position, zest_descriptor_buffer buffer) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(&camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

void VadersGame::Init() {
	//Renderer specific - initialise the texture
	float max_radius = 0;
	uniform_buffer_3d = zest_CreateUniformBuffer("3d uniform", sizeof(zest_uniform_buffer_data_t));
	billboard_pipeline = zest_Pipeline("pipeline_billboard");

	int shape_count = GetShapeCountInLibrary("examples/assets/vaders/vadereffects.tfx");
	particle_texture = zest_CreateTexture("Particle Texture", zest_texture_storage_type_packed, zest_texture_flag_use_filtering, zest_texture_format_rgba, shape_count);
	//Load the effects library and pass the shape loader function pointer that you created earlier. Also pass this pointer to point to this object to give the shapeloader access to the texture we're loading the particle images into
	LoadEffectLibraryPackage("examples/assets/effects.tfx", library, ShapeLoader, this);
	//Renderer specific
	zest_ProcessTextureImages(particle_texture);
	particle_descriptor = zest_CreateSimpleTextureDescriptorSet(particle_texture, "3d descriptor", "3d uniform");
	zest_RefreshTextureDescriptors(particle_texture);

	//Application specific, set up a timer for the update loop
	timer = zest_CreateTimer(60);

	//Make sure that timelinefx udates effects at the same frequency as your update loop.
	tfx::SetUpdateFrequency(60);

	camera = zest_CreateCamera();
	zest_CameraSetFoV(&camera, 60.f);

	UpdateUniform3d(this);

	tfxU32 layer_max_values[tfxLAYERS];
	memset(layer_max_values, 0, 16);
	layer_max_values[0] = 5000;
	layer_max_values[1] = 2500;
	layer_max_values[2] = 2500;
	layer_max_values[3] = 2500;
	//Initialise a particle manager. This manages effects, emitters and the particles that they spawn
	//Depending on your needs you can use as many particle managers as you need.
	//pm.InitFor3d(layer_max_values, 100, tfxParticleManagerMode_ordered_by_depth_guaranteed, 512);
	InitParticleManagerFor3d(&pm, &library, layer_max_values, 1000, tfxParticleManagerMode_unordered, true, false, 2048);

	//Renderer specific
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

	assert(PrepareEffectTemplate(library, "Flicker Flare", effect_template));
}

void BuildUI(VadersGame *game) {
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Effects");
	ImGui::Text("Game Particles: %i", game->pm.ParticleCount());
	ImGui::End();

	ImGui::Render();
	zest_imgui_UpdateBuffers(game->imgui_layer_info.mesh_layer);
}

void RenderParticles3d(tfxParticleManager &pm, float tween, VadersGame *game) {
	//Renderer specific, get the layer that we will draw on (there's only one layer in this example)
	tfxWideFloat lerp = tfxWideSetSingle(tween);
	zest_SetBillboardDrawing(game->billboard_layer, game->particle_texture, game->particle_descriptor, game->billboard_pipeline);
	for (unsigned int layer = 0; layer != tfxLAYERS; ++layer) {
		tfxSpriteSoA &sprites = pm.sprites[pm.current_sprite_buffer][layer];
		for (int i = 0; i != pm.sprite_buffer[pm.current_sprite_buffer][layer].current_size; ++i) {
			zest_SetLayerColor(game->billboard_layer, sprites.color[i].r, sprites.color[i].g, sprites.color[i].b, sprites.color[i].a);
			zest_SetLayerIntensity(game->billboard_layer, sprites.intensity[i]);
			zest_image image = (zest_image)pm.library->emitter_properties.image[sprites.property_indexes[i] & 0x0000FFFF]->ptr;
			tfxVec2 handle = pm.library->emitter_properties.image_handle[sprites.property_indexes[i] & 0x0000FFFF];
			const tfxSpriteTransform3d &captured = pm.GetCapturedSprite3dTransform(layer, sprites.captured_index[i]);
			tfxWideLerpTransformResult lerped = InterpolateSpriteTransform(lerp, sprites.transform_3d[i], captured);
			zest_DrawBillboard(game->billboard_layer, image + ((sprites.property_indexes[i] & 0x00FF0000) >> 16),
				lerped.position,
				sprites.alignment[i],
				lerped.rotations,
				&handle.x,
				sprites.stretch[i],
				(sprites.property_indexes[i] & 0xFF000000) >> 24,
				lerped.scale[0], lerped.scale[1]);
		}
	}
}

//Application specific, this just sets the function to call each render update
void UpdateTfxExample(zest_microsecs ellapsed, void *data) {
	VadersGame *game = static_cast<VadersGame*>(data);

	zest_TimerAccumulate(game->timer);

	//Renderer specific
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);

	UpdateUniform3d(game);
	zest_Update2dUniformBuffer();

	BuildUI(game);

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		tfxEffectID effect_id = AddEffectToParticleManager(&game->pm, game->effect_template);
		if (effect_id != tfxINVALID) {
			tfxVec3 position = ScreenRay(ZestApp->mouse_x, ZestApp->mouse_y, 6.f, game->camera.position, game->uniform_buffer_3d);
			SetEffectPosition(&game->pm, effect_id, position);
			//SetEffectBaseNoiseOffset(&game->pm, new_bullet.effect_index, game->noise_offset);
		}
	}

	while (zest_TimerDoUpdate(game->timer)) {

		UpdateParticleManager(&game->pm, tfxFRAME_LENGTH);

		zest_TimerUnAccumulate(game->timer);
	}

	zest_TimerSet(game->timer);

	RenderParticles3d(game->pm, game->timer->lerp, game);
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

	VadersGame game;
	InitialiseTimelineFX(12);

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
