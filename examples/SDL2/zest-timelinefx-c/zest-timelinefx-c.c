#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_IMAGES_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <zest.h>
#include "timelinefx.h"
#include "impl_timelinefx.h"

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
	zest_device device;
	zest_context context;
	tfx_render_resources_t tfx_rendering;

	tfx_library library;
	tfx_effect_manager pm;

	tfx_effect_template effect_template1;
	tfx_effect_template effect_template2;

	tfxEffectID effect_id;

	double mouse_x, mouse_y;
	double mouse_delta_x, mouse_delta_y;
} TimelineFXExample;

//Allows us to cast a ray into the screen from the mouse position to place an effect where we click
tfx_vec3_t ScreenRay(zest_context context, float x, float y, float depth_offset, zest_vec3 camera_position, zest_uniform_buffer_handle buffer) {
	tfx_uniform_buffer_data_t *uniform_buffer = (tfx_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(context), zest_ScreenHeightf(context), &uniform_buffer->proj, &uniform_buffer->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
	return (tfx_vec3_t){ pos.x, pos.y, pos.z };
}

void Init(TimelineFXExample *example) {
	zest_tfx_InitTimelineFXRenderResources(example->device, example->context, &example->tfx_rendering, "examples/assets/vaders/vadereffects.tfx");

	//Load the effects library and pass the shape loader function pointer that you created earlier. Also pass this pointer to point to this object to give the shapeloader access to the texture we're loading the particle images into
	example->library = tfx_LoadEffectLibrary("examples/assets/vaders/vadereffects.tfx", zest_tfx_ShapeLoader, zest_tfx_GetUV, &example->tfx_rendering);
	//Renderer specific
	//Process the texture with all the particle shapes that we just added to
	example->tfx_rendering.particle_texture = zest_CreateImageAtlas(example->context, &example->tfx_rendering.particle_images, 1024, 1024, 0);
	example->tfx_rendering.particle_texture_index = zest_AcquireSampledImageIndex(example->context, example->tfx_rendering.particle_texture, zest_texture_array_binding);

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
	example->tfx_rendering.color_ramps_collection = zest_CreateImageAtlasCollection(zest_format_r8g8b8a8_unorm, bitmap_count);
	for (int i = 0; i != bitmap_count; ++i) {
		tfx_bitmap_t *bitmap = tfx_GetColorRampBitmap(example->library, i);
		zest_AddImageAtlasPixels(&example->tfx_rendering.color_ramps_collection, bitmap->data, bitmap->size, bitmap->width, bitmap->height, zest_format_r8g8b8a8_unorm);
	}
	//Process the color ramp texture to upload it all to the gpu
	example->tfx_rendering.color_ramps_texture = zest_CreateImageAtlas(example->context, &example->tfx_rendering.color_ramps_collection, 256, 256, zest_image_preset_texture);
	example->tfx_rendering.color_ramps_index = zest_AcquireSampledImageIndex(example->context, example->tfx_rendering.color_ramps_texture, zest_texture_array_binding);
	//Now that the particle shapes have been setup in the renderer, we can call this function to update the shape data in the library
	//with the correct uv texture coords ready to upload to gpu. This buffer will be accessed in the vertex shader when rendering.
	tfx_UpdateLibraryGPUImageData(example->library);

	//Now upload the image data to the GPU and set up the shader resources ready for rendering
	zest_tfx_UpdateTimelineFXImageData(example->context, &example->tfx_rendering, example->library);
	zest_tfx_CreateTimelineFXShaderResources(example->context, &example->tfx_rendering);

	/*
	Initialise a particle manager. This manages effects, emitters and the particles that they spawn. First call tfx_CreateParticleManagerInfo and pass in a setup mode to create an info object with the config we need.
	If you need to you can tweak this further before passing into InitializingParticleManager.

	In this example we'll setup a particle manager for 3d effects and group the sprites by each effect.
	*/
	tfx_effect_manager_info_t pm_info = tfx_CreateEffectManagerInfo(tfxEffectManagerSetup_group_sprites_by_effect);
	example->pm = tfx_CreateEffectManager(pm_info);
}


void UpdateMouse(TimelineFXExample *game) {
	double mouse_x, mouse_y;
	GLFWwindow *handle = (GLFWwindow *)zest_Window(game->context);
	glfwGetCursorPos(handle, &mouse_x, &mouse_y);
	double last_mouse_x = game->mouse_x;
	double last_mouse_y = game->mouse_y;
	game->mouse_x = mouse_x;
	game->mouse_y = mouse_y;
	game->mouse_delta_x = last_mouse_x - game->mouse_x;
	game->mouse_delta_y = last_mouse_y - game->mouse_y;
}

//Application specific, this just sets the function to call each render update
void MainLoop(TimelineFXExample *game) {
	zest_microsecs running_time = zest_Microsecs();
	zest_microsecs frame_time = 0;
	zest_uint frame_count = 0;
	zest_uint fps = 0;

	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(game->context))) {
		zest_microsecs current_frame_time = zest_Microsecs() - running_time;
		running_time = zest_Microsecs();
		frame_time += current_frame_time;
		frame_count += 1;
		if (frame_time >= ZEST_MICROSECS_SECOND) {
			frame_time -= ZEST_MICROSECS_SECOND;
			fps = frame_count;
			frame_count = 0;
		}

		zest_UpdateDevice(game->device);

		glfwPollEvents();

		UpdateMouse(game);

		if (0) {
			//Each time you add an effect to the particle manager it generates an ID which you can use to modify the effect whilst it's being updated
			tfxEffectID effect_id;
			//Add the effect template to the particle manager
			if (tfx_AddEffectTemplateToEffectManager(game->pm, game->effect_template1, &effect_id)) {
				//Calculate a position in 3d by casting a ray into the screen using the mouse coordinates
				tfx_vec3_t position = ScreenRay(game->context, (float)game->mouse_x, (float)game->mouse_y, 10.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer);
				//Set the effect position
				tfx_SetEffectPositionVec3(game->pm, effect_id, position);
			}
		}

		if (0) {
			//Each time you add an effect to the particle manager it generates an ID which you can use to modify the effect whilst it's being updated
			tfxEffectID effect_id;
			//Add the effect template to the particle manager
			if (tfx_AddEffectTemplateToEffectManager(game->pm, game->effect_template2, &effect_id)) {
				//Calculate a position in 3d by casting a ray into the screen using the mouse coordinates
				tfx_vec3_t position = ScreenRay(game->context, (float)game->mouse_x, (float)game->mouse_y, 10.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer);
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

			zest_TimerUnAccumulate(&game->tfx_rendering.timer);
		} zest_EndTimerLoop(game->tfx_rendering.timer);

		//Render the particles with our custom render function if they were updated this frame. If not then the render pipeline
		//will continue to interpolate the particle positions with the last frame update. This minimises the amount of times we
		//have to upload the latest billboards to the gpu.
		if (zest_TimerUpdateWasRun(&game->tfx_rendering.timer)) {
			zest_ResetInstanceLayer(game->tfx_rendering.layer);
			zest_tfx_RenderParticles(game->pm, &game->tfx_rendering);
		}

		//Begin the render graph with the command that acquires a swap chain image (zest_BeginFrameGraphSwapchain)
		//Use the render graph we created earlier. Will return false if a swap chain image could not be acquired. This will happen
		//if the window is resized for example.

		if (zest_BeginFrame(game->context)) {
			zest_SetSwapchainClearColor(game->context, 0.f, .1f, .2f, 1.f);
			if (zest_BeginFrameGraph(game->context, "TimelineFX Render Graphs", 0)) {
				zest_WaitOnTimeline(game->tfx_rendering.timeline);
				//If there was no imgui data to render then zest_imgui_BeginPass will return false
				//Import our test texture with the Bunny sprite
				zest_resource_node particle_texture = zest_ImportImageResource("Particle Texture", game->tfx_rendering.particle_texture, 0);
				zest_resource_node color_ramps_texture = zest_ImportImageResource("Color Ramps Texture", game->tfx_rendering.color_ramps_texture, 0);
				zest_resource_node tfx_write_layer = zest_AddTransientLayerResource("Write particle buffer", game->tfx_rendering.layer, false);
				zest_resource_node tfx_read_layer = zest_AddTransientLayerResource("Read particle buffer", game->tfx_rendering.layer, true);
				zest_resource_node tfx_image_data = zest_ImportBufferResource("Image Data", game->tfx_rendering.image_data, 0);
				zest_ImportSwapchainResource();

				//Connect buffers and textures

				//-------------------------TimelineFX Transfer Pass-------------------------------------------------
				zest_BeginTransferPass("Upload TFX Pass"); {
					//Outputs
					//Note, the only reason the prev frame particle data is put as an output here is to make sure that it's
					//propertly transitioned to the graphics queue (who releases it after each frame)
					zest_ConnectOutput(tfx_write_layer);
					zest_ConnectOutput(tfx_read_layer);
					//Tasks
					zest_SetPassTask(zest_UploadInstanceLayerData, &game->tfx_rendering.layer);
					zest_EndPass();
				}
				//--------------------------------------------------------------------------------------------------

				//------------------------ Graphics Pass -----------------------------------------------------------
				zest_BeginRenderPass("Graphics Pass"); {
					//Inputs
					zest_ConnectInput(particle_texture);
					zest_ConnectInput(color_ramps_texture);
					zest_ConnectInput(tfx_write_layer);
					zest_ConnectInput(tfx_read_layer);
					//Outputs
					zest_ConnectSwapChainOutput();
					//Tasks
					zest_SetPassTask(zest_tfx_DrawParticleLayer, &game->tfx_rendering);
					zest_EndPass();
				}
				//--------------------------------------------------------------------------------------------------

				zest_SignalTimeline(game->tfx_rendering.timeline);
				//Compile and execute the render graph. 
				zest_frame_graph frame_graph = zest_EndFrameGraph();
				zest_QueueFrameGraphForExecution(game->context, frame_graph);
			}
			zest_EndFrame(game->context);
		}
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
int main(void) 
{
	//Make a config struct where you can configure zest with some options
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);

	TimelineFXExample game = {0};
	//Initialise TimelineFX with however many threads you want. Each emitter is updated in it's own thread.
	tfx_InitialiseTimelineFX(tfx_GetDefaultThreadCount(), tfxMegabyte(128));

	if (!glfwInit()) {
		return 0;
	}

	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);

	//Create the device that serves all vulkan based contexts
	zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
	zest_AddDeviceBuilderExtensions(device_builder, glfw_extensions, count);
	zest_AddDeviceBuilderValidation(device_builder);
	zest_DeviceBuilderLogToConsole(device_builder);
	game.device = zest_EndDeviceBuilder(device_builder);

	//Create a window using GLFW
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");
	//Initialise Zest
	game.context = zest_CreateContext(game.device, &window_handles, &create_info);

	Init(&game);

	MainLoop(&game);
	zest_DestroyContext(game.context);
	tfx_EndTimelineFX();

	return 0;
}
#else
int main(void) {
    //Make a config struct where you can configure zest with some options
    zest_create_context_info_t create_info = zest_CreateContextInfo();
    create_info.log_path = "./";
    ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
    ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);

    tfx_InitialiseTimelineFX(tfx_GetDefaultThreadCount(), 128 * 1024 * 1024);

    //Initialise Zest with the configuration
    zest_CreateContext(&create_info);
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
