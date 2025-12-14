#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_IMAGES_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <zest.h>
#include "imgui.h"
#include "impl_imgui.h"
#include "impl_timelinefx.h"
#include <imgui/backends/imgui_impl_glfw.h>

typedef unsigned int u32;

#define FrameLength 16.66666666667f

struct RenderCacheInfo {
	bool draw_imgui;
	bool draw_timeline_fx;
};

struct TimelineFXExample {
	zest_context context;
	zest_device device;
	tfx_render_resources_t tfx_rendering;

	tfx_library library;
	tfx_effect_manager pm;

	tfx_effect_template effect_template1;
	tfx_effect_template effect_template2;
	RenderCacheInfo cache_info;

	tfxEffectID effect_id;
	zest_imgui_t imgui_layer_info;
	bool request_graph_print;
	bool sync_refresh;

	double mouse_x, mouse_y;
	double mouse_delta_x, mouse_delta_y;

	zest_imgui_t imgui;

	void Init();
};

//Allows us to cast a ray into the screen from the mouse position to place an effect where we click
tfx_vec3_t ScreenRay(zest_context context, float x, float y, float depth_offset, zest_vec3 &camera_position, zest_uniform_buffer_handle buffer_handle) {
	zest_uniform_buffer buffer = zest_GetUniformBuffer(buffer_handle);
	tfx_uniform_buffer_data_t *uniform_buffer = (tfx_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(context), zest_ScreenHeightf(context), &uniform_buffer->proj, &uniform_buffer->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

void TimelineFXExample::Init() {
	zest_tfx_InitTimelineFXRenderResources(device, context, &tfx_rendering, "examples/assets/vaders/vadereffects.tfx");

	//Load the effects library and pass the shape loader function pointer that you created earlier. Also pass this pointer to point to this object to give the shapeloader access to the texture we're loading the particle images into
	library = tfx_LoadEffectLibrary("examples/assets/vaders/vadereffects.tfx", zest_tfx_ShapeLoader, zest_tfx_GetUV, &tfx_rendering);
	//Renderer specific
	//Process the texture with all the particle shapes that we just added to
	tfx_rendering.particle_texture = zest_CreateImageAtlas(context, &tfx_rendering.particle_images, 1024, 1024, 0);
	zest_image particle_image = zest_GetImage(tfx_rendering.particle_texture);
	tfx_rendering.particle_texture_index = zest_AcquireSampledImageIndex(context, particle_image, zest_texture_array_binding);

	/*
	Prepare a tfx_effect_template_t that you can use to customise effects in the library in various ways before adding them into a particle manager for updating and rendering. Using a template like this
	means that you can tweak an effect without editing the base effect in the library.
	*NOTE*	You must create all your templates before uploading color ramps as each effect template will create it's own emitter
			color ramps that you can edit in the template. If you don't then the fragment shader will try indexing into the text
			where there is no color data.
	* @param library					A reference to a tfx_library_t that should be loaded with LoadEffectLibraryPackage
	* @param name						The name of the effect in the library that you want to use for the template. If the effect is in a folder then use normal pathing: "My Folder/My effect"
	//Returns a handle for a tfx_effect_template.
	*/
	effect_template1 = tfx_CreateEffectTemplate(library, "Big Explosion");
	effect_template2 = tfx_CreateEffectTemplate(library, "Got Power Up");

	//Add the color ramps from the library to the color ramps texture. Color ramps in the library are stored in rgba format and can be
	//simply copied to a bitmap for uploading to the texture
	tfxU32 bitmap_count = tfx_GetColorRampBitmapCount(library);
	tfx_rendering.color_ramps_collection = zest_CreateImageAtlasCollection(zest_format_r8g8b8a8_unorm, bitmap_count);
	for (int i = 0; i != bitmap_count; ++i) {
		tfx_bitmap_t *bitmap = tfx_GetColorRampBitmap(library, i);
		zest_AddImageAtlasPixels(&tfx_rendering.color_ramps_collection, bitmap->data, bitmap->size, bitmap->width, bitmap->height, zest_format_r8g8b8a8_unorm);
	}
	//Process the color ramp texture to upload it all to the gpu
	tfx_rendering.color_ramps_texture = zest_CreateImageAtlas(context, &tfx_rendering.color_ramps_collection, 256, 256, zest_image_preset_texture);
	zest_image color_ramps_image = zest_GetImage(tfx_rendering.color_ramps_texture);
	tfx_rendering.color_ramps_index = zest_AcquireSampledImageIndex(context, color_ramps_image, zest_texture_array_binding);
	//Now that the particle shapes have been setup in the renderer, we can call this function to update the shape data in the library
	//with the correct uv texture coords ready to upload to gpu. This buffer will be accessed in the vertex shader when rendering.
	tfx_UpdateLibraryGPUImageData(library);

	//Now upload the image data to the GPU and set up the shader resources ready for rendering
	zest_tfx_UpdateTimelineFXImageData(context, &tfx_rendering, library);
	zest_tfx_CreateTimelineFXShaderResources(context, &tfx_rendering);

	/*
	Initialise a particle manager. This manages effects, emitters and the particles that they spawn. First call tfx_CreateParticleManagerInfo and pass in a setup mode to create an info object with the config we need.
	If you need to you can tweak this further before passing into InitializingParticleManager.

	In this example we'll setup a particle manager for 3d effects and group the sprites by each effect.
	*/
	tfx_effect_manager_info_t pm_info = tfx_CreateEffectManagerInfo(tfxEffectManagerSetup_group_sprites_by_effect);
	pm = tfx_CreateEffectManager(pm_info);

	zest_imgui_Initialise(context, &imgui, zest_implglfw_DestroyWindow);
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)zest_Window(context), true);
}

//Draw a Dear ImGui window to output some basic stats
void BuildUI(TimelineFXExample *game, zest_uint fps) {
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Effects");
	ImGui::Text("FPS: %i", fps);
	ImGui::Text("Particles: %i", tfx_ParticleCount(game->pm));
	ImGui::Text("Effects: %i", tfx_EffectCount(game->pm));
	ImGui::Text("Emitters: %i", tfx_EmitterCount(game->pm));
	ImGui::Text("Free Emitters: %i", game->pm->free_emitters.size());
	if (ImGui::Button("Print Render Graph")) {
		game->request_graph_print = true;
	}
	if (ImGui::Button("Toggle Refresh Rate Sync")) {
		if (game->sync_refresh) {
			zest_DisableVSync(game->context);
			game->sync_refresh = false;
		} else {
			zest_EnableVSync(game->context);
			game->sync_refresh = true;
		}
	}
	ImGui::End();

	ImGui::Render();
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

		zest_layer tfx_layer = zest_GetLayer(game->tfx_rendering.layer);

		zest_StartTimerLoop(game->tfx_rendering.timer) {
			BuildUI(game, fps);

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				//Each time you add an effect to the particle manager it generates an ID which you can use to modify the effect whilst it's being updated
				tfxEffectID effect_id;
				//Add the effect template to the particle manager
				if (tfx_AddEffectTemplateToEffectManager(game->pm, game->effect_template1, &effect_id)) {
					//Calculate a position in 3d by casting a ray into the screen using the mouse coordinates
					tfx_vec3_t position = ScreenRay(game->context, (float)game->mouse_x, (float)game->mouse_y, 10.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer);
					//Set the effect position
					tfx_SetEffectPositionVec3(game->pm, effect_id, position);
					tfx_SetEffectOveralScale(game->pm, effect_id, 2.5f);
				}
			}

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
				//Each time you add an effect to the particle manager it generates an ID which you can use to modify the effect whilst it's being updated
				tfxEffectID effect_id;
				//Add the effect template to the particle manager
				if (tfx_AddEffectTemplateToEffectManager(game->pm, game->effect_template2, &effect_id)) {
					//Calculate a position in 3d by casting a ray into the screen using the mouse coordinates
					tfx_vec3_t position = ScreenRay(game->context, (float)game->mouse_x, (float)game->mouse_y, 10.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer);
					//Set the effect position
					tfx_SetEffectPositionVec3(game->pm, effect_id, position);
					tfx_SetEffectOveralScale(game->pm, effect_id, 2.5f);
				}
			}

			//Update the particle manager but only if pending ticks is > 0. This means that if we're trying to catch up this frame
			//then rather then run the update particle manager multiple times, simply run it once but multiply the frame length
			//instead. This is important in order to keep the billboard buffer on the gpu in sync for interpolating the particles
			//with the previous frame. It's also just more efficient to do this.
			if (pending_ticks > 0) {
				tfx_UpdateEffectManager(game->pm, FrameLength * pending_ticks);
				pending_ticks = 0;
			}

		} zest_EndTimerLoop(game->tfx_rendering.timer);

		//Render the particles with our custom render function if they were updated this frame. If not then the render pipeline
		//will continue to interpolate the particle positions with the last frame update. This minimises the amount of times we
		//have to upload the latest billboards to the gpu.
		if (zest_TimerUpdateWasRun(&game->tfx_rendering.timer)) {
			zest_ResetInstanceLayer(tfx_layer);
			zest_tfx_RenderParticles(game->pm, &game->tfx_rendering);
		}

		game->cache_info.draw_imgui = zest_imgui_HasGuiToDraw(&game->imgui);
		game->cache_info.draw_timeline_fx = zest_GetLayerInstanceSize(tfx_layer) > 0;
		zest_frame_graph_cache_key_t cache_key = {};
		cache_key = zest_InitialiseCacheKey(game->context, &game->cache_info, sizeof(RenderCacheInfo));

		//Begin the render graph with the command that acquires a swap chain image (zest_BeginFrameGraphSwapchain)
		//Use the render graph we created earlier. Will return false if a swap chain image could not be acquired. This will happen
		//if the window is resized for example.
		if (zest_BeginFrame(game->context)) {
			zest_tfx_UpdateUniformBuffer(game->context, &game->tfx_rendering);
			zest_SetSwapchainClearColor(game->context, 0.f, .1f, .2f, 1.f);
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(game->context, &cache_key);
			if (!frame_graph) {
				if (zest_BeginFrameGraph(game->context, "TimelineFX Render Graphs", 0)) {
					//zest_WaitOnTimeline(game->tfx_rendering.timeline);
					//If there was no imgui data to render then zest_imgui_BeginPass will return false
					//Import our test texture with the Bunny sprite
					zest_image particle_image = zest_GetImage(game->tfx_rendering.particle_texture);
					zest_image color_ramps_image = zest_GetImage(game->tfx_rendering.color_ramps_texture);
					zest_resource_node particle_texture = zest_ImportImageResource("Particle Texture", particle_image, 0);
					zest_resource_node color_ramps_texture = zest_ImportImageResource("Color Ramps Texture", color_ramps_image, 0);
					zest_resource_node tfx_write_layer = zest_AddTransientLayerResource("Write particle buffer", tfx_layer, false);
					zest_resource_node tfx_read_layer = zest_AddTransientLayerResource("Read particle buffer", tfx_layer, true);
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
						zest_SetPassTask(zest_UploadInstanceLayerData, tfx_layer);
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
					//If there's imgui to draw then draw it
					zest_pass_node imgui_pass = zest_imgui_BeginPass(&game->imgui, game->imgui.main_viewport); {
						if (imgui_pass) {
							zest_ConnectSwapChainOutput();
						}
						zest_EndPass();
					}
					//--------------------------------------------------------------------------------------------------

					//zest_SignalTimeline(game->tfx_rendering.timeline);
					//Compile and execute the render graph. 
					frame_graph = zest_EndFrameGraph();
				}
			}
			zest_QueueFrameGraphForExecution(game->context, frame_graph);
			zest_EndFrame(game->context);
			if (game->request_graph_print) {
				//You can print out the render graph for debugging purposes
				zest_PrintCompiledFrameGraph(frame_graph);
				game->request_graph_print = false;
			}
		}
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main() {
	zest_create_context_info_t create_info = zest_CreateInfoWithValidationLayers(zest_validation_flag_enable_sync);
	ZEST__FLAG(create_info.flags, zest_init_flag_enable_vsync);
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
	game.device = zest_implglfw_CreateDevice(false);

	//Create a window using GLFW
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");
	//Initialise Zest
	game.context = zest_CreateContext(game.device, &window_handles, &create_info);

	game.Init();

	MainLoop(&game);

	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Destroy(&game.imgui);
	zest_DestroyDevice(game.device);
	tfx_EndTimelineFX();

	return 0;
}
#else
int main(void) {
	zest_create_context_info_t create_info = zest_CreateInfo();
	zest_implglfw_SetCallbacks(&create_info);

	ImGuiApp imgui_app;

	zest_CreateContext(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#endif
