#include "timelinefx.h"
#include "impl_timelinefx.h"
#include <zest.h>

#define x_distance 0.078f
#define y_distance 0.158f
#define x_spacing 0.040f
#define y_spacing 0.058f

#define Rad90 1.5708f
#define Rad270 4.71239f

typedef unsigned int u32;

#define UpdateFrequency 0.016666666666f
#define FrameLength 16.66666666667f

typedef struct TimelineFXExample_s {
	zest_texture imgui_font_texture;
	tfx_render_resources_t tfx_rendering;

	tfx_library library;
	tfx_effect_manager pm;

	tfx_effect_template effect_template1;
	tfx_effect_template effect_template2;

	tfxEffectID effect_id;
	zest_imgui_t imgui_layer_info;

} TimelineFXExample;

//Allows us to cast a ray into the screen from the mouse position to place an effect where we click
tfx_vec3_t ScreenRay(float x, float y, float depth_offset, zest_vec3 *camera_position, zest_uniform_buffer buffer) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t *)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), *camera_position);
	tfx_vec3_t ray = { pos.x, pos.y, pos.z };
	return ray;
}

void Init(TimelineFXExample *example) {
	zest_tfx_InitTimelineFXRenderResources(&example->tfx_rendering, "examples/assets/effects.tfx");
	float max_radius = 0;

	//Load the effects library and pass the shape loader function pointer that you created earlier. Also pass this pointer to point to this object to give the shapeloader access to the texture we're loading the particle images into
	example->library = tfx_LoadEffectLibrary("examples/assets/effects.tfx", zest_tfx_ShapeLoader, zest_tfx_GetUV, &example->tfx_rendering);
	//Renderer specific
	//Process the texture with all the particle shapes that we just added to
	zest_ProcessTextureImages(example->tfx_rendering.particle_texture);

	/*
	Prepare a tfx_effect_template_t that you can use to customise effects in the library in various ways before adding them into a particle manager for updating and rendering. Using a template like this
	means that you can tweak an effect without editing the base effect in the library.
	*NOTE*	You must create all your templates before uploading color ramps as each effect template will create it's own emitter
			color ramps that you can edit in the template. If you don't then the fragment shader will try indexing into the text
			where there is no color data.
	* @param library					A reference to a tfx_library_t that should be loaded with LoadEffectLibraryPackage
	* @param name						The name of the effect in the library that you want to use for the template. If the effect is in a folder then use normal pathing: "My Folder/My effect"
	* @param effect_template			The empty tfx_effect_template_t object that you want the effect loading into
	//Returns a handle for a tfx_effect_template.
	*/
	example->effect_template1 = tfx_CreateEffectTemplate(example->library, "Star Burst Flash");
	example->effect_template2 = tfx_CreateEffectTemplate(example->library, "Star Burst Flash.1");

	//Add the color ramps from the library to the color ramps texture. Color ramps in the library are stored in rgba format and can be
	//simply copied to a bitmap for uploading to the texture
	tfxU32 bitmap_count = tfx_GetColorRampBitmapCount(example->library);
	for (int i = 0; i != bitmap_count; ++i) {
		tfx_bitmap_t *bitmap = tfx_GetColorRampBitmap(example->library, i);
		zest_bitmap_t temp_bitmap = zest_CreateBitmapFromRawBuffer("", bitmap->data, (int)bitmap->size, bitmap->width, bitmap->height, bitmap->channels);
		zest_AddTextureImageBitmap(example->tfx_rendering.color_ramps_texture, &temp_bitmap);
	}
	//Process the color ramp texture to upload it all to the gpu
	zest_ProcessTextureImages(example->tfx_rendering.color_ramps_texture);
	//Now that the particle shapes have been setup in the renderer, we can call this function to update the shape data in the library
	//with the correct uv texture coords ready to upload to gpu. This buffer will be accessed in the vertex shader when rendering.
	tfx_UpdateLibraryGPUImageData(example->library);

	//Now upload the image data to the GPU and set up the shader resources ready for rendering
	zest_tfx_UpdateTimelineFXImageData(&example->tfx_rendering, example->library);
	zest_tfx_CreateTimelineFXShaderResources(&example->tfx_rendering);

	/*
	Initialise a particle manager. This manages effects, emitters and the particles that they spawn. First call tfx_CreateParticleManagerInfo and pass in a setup mode to create an info object with the config we need.
	If you need to you can tweak this further before passing into InitializingParticleManager.

	In this example we'll setup a particle manager for 3d effects and group the sprites by each effect.
	*/
	tfx_effect_manager_info_t pm_info = tfx_CreateEffectManagerInfo(tfxEffectManagerSetup_group_sprites_by_effect);
	example->pm = tfx_CreateEffectManager(pm_info);
}

//Application specific, this just sets the function to call each render update
void UpdateTfxExample(zest_microsecs ellapsed, void *data) {
	TimelineFXExample *game = (TimelineFXExample*)data;

	zest_tfx_UpdateUniformBuffer(&game->tfx_rendering);

	if (zest_MouseHit(zest_left_mouse)) {
		//Each time you add an effect to the particle manager it generates an ID which you can use to modify the effect whilst it's being updated
		tfxEffectID effect_id;
		//Add the effect template to the particle manager
		if (tfx_AddEffectTemplateToEffectManager(game->pm, game->effect_template1, &effect_id)) {
			//Calculate a position in 3d by casting a ray into the screen using the mouse coordinates
			tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 12.f, &game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer);
			//Set the effect position
			tfx_SetEffectPositionVec3(game->pm, effect_id, position);
		}
	}

	if (zest_MouseHit(zest_right_mouse)) {
		//Each time you add an effect to the particle manager it generates an ID which you can use to modify the effect whilst it's being updated
		tfxEffectID effect_id;
		//Add the effect template to the particle manager
		if (tfx_AddEffectTemplateToEffectManager(game->pm, game->effect_template2, &effect_id)) {
			//Calculate a position in 3d by casting a ray into the screen using the mouse coordinates
			tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 12.f, &game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer);
			//Set the effect position
			tfx_SetEffectPositionVec3(game->pm, effect_id, position);
		}
	}

	zest_StartTimerLoop(game->tfx_rendering.timer) {

		//Update the particle manager but only if pending ticks is > 0. This means that if we're trying to catch up this frame
		//then rather then run the update particle manager multiple times, simply run it once but multiply the frame length
		//instead. This is important in order to keep the billboard buffer on the gpu in sync for interpolating the particles
		//with the previous frame. It's also just more efficient to do this.
		if (pending_ticks > 0) {
			tfx_UpdateEffectManager(game->pm, FrameLength);
			pending_ticks = 0;
		}

		zest_TimerUnAccumulate(game->tfx_rendering.timer);
	} zest_EndTimerLoop(game->tfx_rendering.timer);

	//Render the particles with our custom render function if they were updated this frame. If not then the render pipeline
	//will continue to interpolate the particle positions with the last frame update. This minimises the amount of times we
	//have to upload the latest billboards to the gpu.
	if (zest_TimerUpdateWasRun(game->tfx_rendering.timer)) {
		zest_ResetInstanceLayer(game->tfx_rendering.layer);
		zest_tfx_RenderParticles(game->pm, &game->tfx_rendering);
	}

	//Begin the render graph with the command that acquires a swap chain image (zest_BeginRenderToScreen)
	//Use the render graph we created earlier. Will return false if a swap chain image could not be acquired. This will happen
	//if the window is resized for example.
	if (zest_BeginRenderToScreen("TimelineFX Render Graphs")) {
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		//Import the swap chain into the render pass
		zest_resource_node swapchain_output_resource = zest__import_swapchain_resource("Swapchain Output");
		zest_pass_node graphics_pass = zest_AddRenderPassNode("Graphics Pass");
		zest_pass_node upload_tfx_data = zest_AddTransferPassNode("Upload TFX Pass");
		//If there was no imgui data to render then zest_imgui_AddToRenderGraph will return false
		//Import our test texture with the Bunny sprite
		zest_resource_node particle_texture = zest_ImportImageResource("Particle Texture", game->tfx_rendering.particle_texture);
		zest_resource_node color_ramps_texture = zest_ImportImageResource("Color Ramps Texture", game->tfx_rendering.color_ramps_texture);
		zest_resource_node tfx_layer = zest_AddTransientLayerResource(game->tfx_rendering.layer);
		zest_resource_node tfx_image_data = zest_ImportStorageBufferResource("Image Data", game->tfx_rendering.image_data);

		//Connect buffers and textures
		zest_ConnectTransferBufferOutput(upload_tfx_data, tfx_layer);
		zest_ConnectVertexBufferInput(graphics_pass, tfx_layer);
		zest_ConnectSampledImageInput(graphics_pass, particle_texture, zest_pipeline_fragment_stage);
		zest_ConnectSampledImageInput(graphics_pass, color_ramps_texture, zest_pipeline_fragment_stage);
		zest_ConnectSwapChainOutput(graphics_pass, swapchain_output_resource, clear_color);

		zest_AddPassInstanceLayerUpload(upload_tfx_data, game->tfx_rendering.layer);
		zest_AddPassTask(graphics_pass, zest_tfx_DrawParticleLayer, &game->tfx_rendering);
		//End the render graph. This tells Zest that it can now compile the render graph ready for executing.
		zest_EndRenderGraph();
		zest_render_graph render_graph = zest_ExecuteRenderGraph();
	}
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
//int main(void) 
{
	//Make a config struct where you can configure zest with some options
	zest_create_info_t create_info = zest_CreateInfo();
	create_info.log_path = "./";
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);

	tfx_InitialiseTimelineFX(tfx_GetDefaultThreadCount(), 128 * 1024 * 1024);

	//Initialise Zest with the configuration
	zest_Initialise(&create_info);
	zest_LogFPSToConsole(1);
	//Set the callback you want to use that will be called each frame.
	zest_SetUserUpdateCallback(UpdateTfxExample);

	TimelineFXExample game = { 0 };
	zest_SetUserData(&game);
	Init(&game);

	//Start the Zest main loop
	zest_Start();

	tfx_EndTimelineFX();

	return 0;
}
#else
int main(void) {
    //Make a config struct where you can configure zest with some options
    zest_create_info_t create_info = zest_CreateInfo();
    create_info.log_path = "./";
    ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
    ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);

    tfx_InitialiseTimelineFX(tfx_GetDefaultThreadCount(), 128 * 1024 * 1024);

    //Initialise Zest with the configuration
    zest_Initialise(&create_info);
    zest_LogFPSToConsole(1);
    //Set the callback you want to use that will be called each frame.
    zest_SetUserUpdateCallback(UpdateTfxExample);

    TimelineFXExample game = { 0 };
    zest_SetUserData(&game);
    Init(&game);

    //Start the Zest main loop
    zest_Start();

    tfx_EndTimelineFX();

    return 0;
}
#endif
