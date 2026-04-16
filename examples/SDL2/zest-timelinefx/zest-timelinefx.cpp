#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_IMAGES_IMPLEMENTATION
#include <SDL.h>
#include <zest.h>
#include "imgui.h"
#include "impl_imgui.h"
#include "impl_timelinefx.h"
#include <imgui/backends/imgui_impl_sdl2.h>
#include "examples/Common/sdl_events.cpp"

/**
	Example showing how to use the timelinefx library and implementation to render particle effects
 */

typedef unsigned int u32;

struct RenderCacheInfo {
	bool draw_imgui;
	bool draw_timeline_fx;
};

struct TimelineFXExample {
	zest_context context;
	zest_device device;
	tfx_library_render_resources_t tfx_rendering;
	tfx_ribbon_buffers_t ribbon_buffers;
	tfx_ribbon_render_dispatch_t ribbon_render_dispatch;

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
	zest_tfx_InitTimelineFXRenderResources(context, &tfx_rendering, "examples/assets/shaders/timelinefx3d.vert", "examples/assets/shaders/timelinefx.frag", "examples/assets/shaders/ribbon_3d.vert", "examples/assets/shaders/ribbon.frag", "examples/assets/shaders/ribbons.comp");

	//Load the effects library and create the particle image atlas
	library = zest_tfx_LoadLibrary(context, &tfx_rendering, "examples/assets/vaders/vadereffects.tfx", 1024, 1024);

	//Create effect templates - must be done before calling FinaliseLibrary so that color ramps are set up correctly
	effect_template1 = tfx_CreateEffectTemplate(library, "Title");
	effect_template2 = tfx_CreateEffectTemplate(library, "Got Power Up");

	//Finalise the library - uploads color ramps and GPU image data
	zest_tfx_FinaliseLibrary(context, &tfx_rendering, library);

	//Create the effect manager
	tfx_effect_manager_info_t pm_info = tfx_CreateEffectManagerInfo(tfxEffectManagerSetup_group_sprites_by_effect);
	pm = tfx_CreateEffectManager(pm_info);

	//Create shared ribbon buffers sized across all effect managers and upload lookup data
	zest_tfx_CreateRibbonBuffers(context, &ribbon_buffers);
	zest_tfx_UploadRibbonLookupData(context, &tfx_rendering, &ribbon_buffers);

	zest_imgui_Initialise(context, &imgui, zest_implsdl2_DestroyWindow);
    ImGui_ImplSDL2_InitForVulkan((SDL_Window *)zest_Window(context));
}

//Draw a Dear ImGui window to output some basic stats
void BuildUI(TimelineFXExample *game, zest_uint fps) {
	ImGui_ImplSDL2_NewFrame();
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

void UpdateMouse(TimelineFXExample *app) {
	int mouse_x, mouse_y;
	SDL_GetMouseState(&mouse_x, &mouse_y);
	app->mouse_x = (float)mouse_x;
	app->mouse_y = (float)mouse_y;
}

//Application specific, this just sets the function to call each render update
void MainLoop(TimelineFXExample *game) {
	zest_microsecs running_time = zest_Microsecs();
	zest_microsecs frame_time = 0;
	zest_uint frame_count = 0;
	zest_uint fps = 0;
	int running = 1;
	SDL_Event event;

	while (running) {
		running = PollSDLEvents(game->context, &event);
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

		//Begin the render graph with the command that acquires a swap chain image (zest_BeginFrameGraphSwapchain)
		//Use the render graph we created earlier. Will return false if a swap chain image could not be acquired. This will happen
		//if the window is resized for example.
		if (zest_BeginFrame(game->context)) {

			UpdateMouse(game);

			zest_layer tfx_layer = zest_GetLayer(game->tfx_rendering.layer);

			zest_StartTimerLoop(game->tfx_rendering.timer) {
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
					double frame_length = zest_TimerFrameLengthMillisecs(&game->tfx_rendering.timer);
					tfx_UpdateEffectManager(game->pm, frame_length * pending_ticks);
					pending_ticks = 0;
				}

				BuildUI(game, fps);
			} zest_EndTimerLoop(game->tfx_rendering.timer);

			tfx_SetPMCamera(game->pm, &game->tfx_rendering.camera.front.x, &game->tfx_rendering.camera.position.x);
			zest_tfx_UpdateRibbonStagingBuffers(game->context, &game->ribbon_buffers, game->pm);

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

			zest_tfx_UpdateUniformBuffer(game->context, &game->tfx_rendering);
			zest_SetSwapchainClearColor(game->context, 0.f, .1f, .2f, 1.f);
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(game->context, 0);
			if (!frame_graph) {
				zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(game->tfx_rendering.uniform_buffer);
				if (zest_BeginFrameGraph(game->context, "TimelineFX Render Graphs", &cache_key)) {
					//zest_WaitOnTimeline(game->tfx_rendering.timeline);
					//If there was no imgui data to render then zest_imgui_BeginPass will return false
					//Import our test texture with the Bunny sprite
					zest_resource_node tfx_write_layer = zest_AddTransientLayerResource("Write particle buffer", tfx_layer, false);
					zest_resource_node tfx_read_layer = zest_AddTransientLayerResource("Read particle buffer", tfx_layer, true);
					zest_ImportSwapchainResource();

					//Connect buffers and textures
					zest_tfx_SetRibbonRenderDispatch(&game->ribbon_render_dispatch, game->pm, &game->ribbon_buffers, &game->tfx_rendering);

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
						zest_ConnectInput(tfx_write_layer);
						zest_ConnectInput(tfx_read_layer);
						//Outputs
						zest_ConnectSwapChainOutput();
						//Tasks
						zest_SetPassTask(zest_tfx_DrawParticleLayer, &game->tfx_rendering);
						zest_EndPass();
					}

					if (tfx_HasRibbonsToDraw(game->pm)) {
						zest_tfx_AddRibbonsToFrameGraph(&game->ribbon_render_dispatch, 0);
					}

					//If there's imgui to draw then draw it
					zest_pass_node imgui_pass = zest_imgui_BeginPass(&game->imgui, game->imgui.main_viewport); {
						if (imgui_pass) {
							zest_ConnectSwapChainOutput();
							zest_EndPass();
						}
					}
					//--------------------------------------------------------------------------------------------------

					//zest_SignalTimeline(game->tfx_rendering.timeline);
					//Compile and execute the render graph. 
					frame_graph = zest_EndFrameGraph();
				}
			}
			zest_EndFrame(game->context, frame_graph);
			if (game->request_graph_print) {
				//You can print out the render graph for debugging purposes
				zest_PrintCompiledFrameGraph(frame_graph);
				game->request_graph_print = false;
			}
		}
	}
}

int main(int argc, char *argv[]) {
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	ZEST__FLAG(create_info.flags, zest_context_init_flag_enable_vsync);

	TimelineFXExample game = {0};
	//Initialise TimelineFX with however many threads you want. Each emitter is updated in it's own thread.
	tfx_InitialiseTimelineFX(tfx_GetDefaultThreadCount(), tfxMegabyte(128));

	//Create a window using SDL2. We must do this before setting up the device as it's needed to get
	//the extensions info.
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "TimelineFX Example");

	//Create the device that serves all vulkan based contexts
	game.device = zest_implsdl2_CreateVulkanDevice(&window_data, 0);

	//Initialise Zest
	game.context = zest_CreateContext(game.device, &window_data, &create_info);

	game.Init();

	MainLoop(&game);

	ImGui_ImplSDL2_Shutdown();
	zest_imgui_Destroy(&game.imgui);
	zest_DestroyDevice(game.device);
	tfx_EndTimelineFX();

	return 0;
}
