#include <zest.h>
#include "imgui.h"
#include "impl_imgui.h"
#include "impl_glfw.h"
#include "impl_imgui_glfw.h"
#include "impl_timelinefx.h"
#include "timelinefx.h"

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
	zest_texture imgui_font_texture;

	tfx_render_resources_t tfx_rendering;

	tfx_library library;
	tfx_effect_manager pm;

	tfx_effect_template effect_template1;
	tfx_effect_template effect_template2;
	tfx_effect_template cube_ordered;

	tfxEffectID effect_id;

	tfx_vector_t<tfxEffectID> test_effects;

	zest_imgui_t imgui_layer_info;
	tfx_random_t random;
	tfx_vector_t<tfx_pool_stats_t> memory_stats;
	bool sync_refresh;
	bool request_graph_print;
	bool pause = false;

	float test_depth = 1.f;
	float test_lerp = 0;

	void Init();
};

//Allows us to cast a ray into the screen from the mouse position to place an effect where we click
tfx_vec3_t ScreenRay(float x, float y, float depth_offset, zest_vec3 &camera_position, zest_uniform_buffer buffer) {
	tfx_uniform_buffer_data_t *uniform_buffer = (tfx_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &uniform_buffer->proj, &uniform_buffer->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

void TimelineFXExample::Init() {
	zest_tfx_InitTimelineFXRenderResources(&tfx_rendering, "examples/assets/effects.tfx");
	float max_radius = 0;

	//Load the effects library and pass the shape loader function pointer that you created earlier. Also pass this pointer to point to this object to give the shapeloader access to the texture we're loading the particle images into
	library = tfx_LoadEffectLibrary("examples/assets/effects.tfx", zest_tfx_ShapeLoader, zest_tfx_GetUV, &tfx_rendering);
	//Renderer specific
	//Process the texture with all the particle shapes that we just added to
	zest_ProcessTextureImages(tfx_rendering.particle_texture);
	zest_AcquireGlobalCombinedImageSampler(tfx_rendering.particle_texture);

	effect_template1 = tfx_CreateEffectTemplate(library, "Star Burst Flash");
	effect_template2 = tfx_CreateEffectTemplate(library, "Star Burst Flash.1");
	cube_ordered = tfx_CreateEffectTemplate(library, "Cube Ordered.1");

	//Add the color ramps from the library to the color ramps texture. Color ramps in the library are stored in rgba format and can be
	//simply copied to a bitmap for uploading to the texture
	for (tfx_bitmap_t &bitmap : library->color_ramps.color_ramp_bitmaps) {
		zest_bitmap_t temp_bitmap = zest_CreateBitmapFromRawBuffer("", bitmap.data, (int)bitmap.size, bitmap.width, bitmap.height, bitmap.channels);
		zest_AddTextureImageBitmap(tfx_rendering.color_ramps_texture, &temp_bitmap);
	}
	//Process the color ramp texture to upload it all to the gpu
	zest_ProcessTextureImages(tfx_rendering.color_ramps_texture);
	zest_AcquireGlobalCombinedImageSampler(tfx_rendering.color_ramps_texture);
	//Now that the particle shapes have been setup in the renderer, we can call this function to update the shape data in the library
	//with the correct uv texture coords ready to upload to gpu. This buffer will be accessed in the vertex shader when rendering.
	tfx_UpdateLibraryGPUImageData(library);

	//Now upload the image data to the GPU and set up the shader resources ready for rendering
	zest_tfx_UpdateTimelineFXImageData(&tfx_rendering, library);
	zest_tfx_CreateTimelineFXShaderResources(&tfx_rendering);

	//Initialise Dear ImGui for Zest
	zest_imgui_Initialise();

	/*
	Initialise a particle manager. This manages effects, emitters and the particles that they spawn. First call tfx_CreateEffectManagerInfo and pass in a setup mode to create an info object with the config we need.
	If you need to you can tweak this further before passing into InitializingEffectManager.
	In this example we'll setup a particle manager for 3d effects and group the sprites by each effect.
	*/
	tfx_effect_manager_info_t pm_info = tfx_CreateEffectManagerInfo(tfxEffectManagerSetup_group_sprites_by_effect);
	pm = tfx_CreateEffectManager(pm_info);
	tfx_SetPMCamera(pm, &tfx_rendering.camera.front.x, &tfx_rendering.camera.position.x);

	random = tfx_NewRandom(zest_Millisecs());

	for (int i = 0; i != 3; ++i) {
		tfxEffectID effect_id;
		if (tfx_AddEffectTemplateToEffectManager(pm, cube_ordered, &effect_id)) {
			tfx_vec3_t position = {tfx_RandomRangeZeroToMax(&random, 5.f), tfx_RandomRangeFromTo(&random, -2.f, 2.f), tfx_RandomRangeFromTo(&random, -4.f, 4.f)};
			tfx_SetEffectPositionVec3(pm, effect_id, position);
			test_effects.push_back(effect_id);
		}
	}

}

//Draw a Dear ImGui window to output some basic stats
void BuildUI(TimelineFXExample *game) {
	ImGui_ImplGlfw_NewFrame();
	tfx_uniform_buffer_data_t *uniform_buffer = (tfx_uniform_buffer_data_t*)zest_GetUniformBufferData(game->tfx_rendering.uniform_buffer);
	ImGui::NewFrame();
	ImGui::Begin("Effects");
	ImGui::Text("FPS: %i", zest_FPS());
	ImGui::Text("Particles: %i", tfx_ParticleCount(game->pm));
	ImGui::Text("Effects: %i", tfx_EffectCount(game->pm));
	ImGui::Text("Emitters: %i", tfx_EmitterCount(game->pm));
	ImGui::Text("Free Emitters: %i", game->pm->free_emitters.size());
	ImGui::Text("Lerp: %f", uniform_buffer->timer_lerp);
	ImGui::Text("Update Time: %f", uniform_buffer->update_time);
	ImGui::DragFloat("Lerp", &game->test_lerp, 0.001f, 0.f, 1.f);
	if (ImGui::Button("Toggle Refresh Rate Sync")) {
		if (game->sync_refresh) {
			zest_DisableVSync();
			game->sync_refresh = false;
			ZEST_PRINT("Disable VSync");
		} else {
			zest_EnableVSync();
			game->sync_refresh = true;
			ZEST_PRINT("Enable VSync");
		}
	}
	if (ImGui::Button("Print Render Graph")) {
		game->request_graph_print = true;
	}
	if (ImGui::Button("Pause")) {
		game->pause = game->pause ^ 1;
	}
	/*
	int i = 0;
	for (tfx_effect_index_t effect_index : game->pm.effects_in_use[0][game->pm.current_ebuff]) {
		ImGui::Text("%i) Effect Index %i", i, effect_index.index);
		i++;
	}
	for (tfx_pool_stats_t& stats : game->memory_stats) {
		ImGui::Text("Free Blocks: %i, Used Blocks: %i", stats.free_blocks, stats.used_blocks);
		ImGui::Text("Free Memory: %zu(bytes) %zu(kb) %zu(mb)", stats.free_size, stats.free_size / 1024, stats.free_size / 1024 / 1024);
		ImGui::Text("Used Memory: %zu(bytes) %zu(kb) %zu(mb)", stats.used_size, stats.used_size / 1024, stats.used_size / 1024 / 1024);
	}
	*/
	ImGui::End();

	ImGui::Render();
	zest_imgui_UpdateBuffers();
}

void ManualRenderGraph() {

}

//Application specific, this just sets the function to call each render update
void UpdateTfxExample(zest_microsecs ellapsed, void *data) {
	TimelineFXExample *game = static_cast<TimelineFXExample*>(data);

	zest_tfx_UpdateUniformBuffer(&game->tfx_rendering);
	tfx_uniform_buffer_data_t *uniform_buffer = (tfx_uniform_buffer_data_t*)zest_GetUniformBufferData(game->tfx_rendering.uniform_buffer);
	uniform_buffer->timer_lerp = game->test_lerp;

	zest_StartTimerLoop(game->tfx_rendering.timer) {
		BuildUI(game);

		/*
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			//Each time you add an effect to the particle manager it generates an ID which you can use to modify the effect whilst it's being updated
			tfxEffectID effect_id;
			//Add the effect template to the particle manager
			if(tfx_AddEffectTemplateToEffectManager(game->pm, game->effect_template1, &effect_id)) {
				//Calculate a position in 3d by casting a ray into the screen using the mouse coordinates
				tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 6.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer);
				//Set the effect position
				tfx_SetEffectPositionVec3(game->pm, effect_id, position);
			}
		}

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			//Each time you add an effect to the particle manager it generates an ID which you can use to modify the effect whilst it's being updated
			tfxEffectID effect_id;
			//Add the effect template to the particle manager
			if(tfx_AddEffectTemplateToEffectManager(game->pm, game->effect_template2, &effect_id)) {
				//Calculate a position in 3d by casting a ray into the screen using the mouse coordinates
				tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 6.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer); //Set the effect position
				tfx_SetEffectPositionVec3(game->pm, effect_id, position);
			}
		}
		*/

		/*
		if (!game->pause) {
			for (tfxEffectID &effect_id : game->test_effects) {
				float chance = tfx_GenerateRandom(&game->random);
				if (chance < 0.01f) {
					tfx_SoftExpireEffect(game->pm, effect_id);
					if (tfx_AddEffectTemplateToEffectManager(game->pm, game->cube_ordered, &effect_id)) {
						tfx_vec3_t position = { tfx_RandomRangeZeroToMax(&game->random, 5.f), tfx_RandomRangeFromTo(&game->random, -2.f, 2.f), tfx_RandomRangeFromTo(&game->random, -4.f, 4.f) };
						tfx_SetEffectPositionVec3(game->pm, effect_id, position);
					}
					game->test_depth++;
				}
			}
		}
		*/

		//Update the particle manager but only if pending ticks is > 0. This means that if we're trying to catch up this frame
		//then rather then run the update particle manager multiple times, simple run it once but multiply the frame length
		//instead. This is important in order to keep the billboard buffer on the gpu in sync for interpolating the particles
		//with the previous frame. It's also just more efficient to do this.
		if (pending_ticks > 0 && !game->pause) {
			tfx_UpdateEffectManager(game->pm, FrameLength * pending_ticks);
			pending_ticks = 0;
		}
	} zest_EndTimerLoop(game->tfx_rendering.timer);

	//Render the particles with our custom render function if they were updated this frame. If not then the render pipeline
	//will continue to interpolate the particle positions with the last frame update. This minimises the amount of times we
	//have to upload the latest billboards to the gpu.
	if (zest_TimerUpdateWasRun(game->tfx_rendering.timer)) {
		//ZEST_PRINT("Layer flipped: %u / %u", game->tfx_rendering.layer->prev_fif, game->tfx_rendering.layer->fif);
		if (!game->pause) {
			zest_ResetInstanceLayer(game->tfx_rendering.layer);
			zest_tfx_RenderParticlesByEffect(game->pm, &game->tfx_rendering);
		}
	}

	//Begin the render graph with the command that acquires a swap chain image (zest_BeginRenderToScreen)
	//Use the render graph we created earlier. Will return false if a swap chain image could not be acquired. This will happen
	//if the window is resized for example.
	if (zest_BeginRenderToScreen("TimelineFX Render Graphs")) {
		//zest_ForceRenderGraphOnGraphicsQueue();
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		//Import the swap chain into the render pass
		zest_resource_node swapchain_output_resource = zest_ImportSwapChainResource("Swapchain Output");
		//Import our test texture with the Bunny sprite
		zest_resource_node particle_texture = zest_ImportImageResourceReadOnly("Particle Texture", game->tfx_rendering.particle_texture);
		zest_resource_node color_ramps_texture = zest_ImportImageResourceReadOnly("Color Ramps Texture", game->tfx_rendering.color_ramps_texture);
		zest_resource_node tfx_layer = zest_AddInstanceLayerBufferResource("current particles", game->tfx_rendering.layer, false);
		zest_resource_node tfx_image_data = zest_ImportStorageBufferResource("Image Data", game->tfx_rendering.image_data);
		zest_resource_node tfx_layer_prev = zest_AddInstanceLayerBufferResource("last frame particles", game->tfx_rendering.layer, true);

		//Connect buffers and textures

		//------------------------ Graphics Pass -----------------------------------------------------------
		zest_pass_node graphics_pass = zest_AddRenderPassNode("Graphics Pass");
		//Inputs
		zest_ConnectVertexBufferInput(graphics_pass, tfx_layer);
		//zest_ConnectStorageBufferInput(graphics_pass, tfx_layer_prev, zest_pipeline_vertex_stage);
		zest_ConnectSampledImageInput(graphics_pass, particle_texture, zest_pipeline_fragment_stage);
		zest_ConnectSampledImageInput(graphics_pass, color_ramps_texture, zest_pipeline_fragment_stage);
		//Outputs
		zest_ConnectSwapChainOutput(graphics_pass, swapchain_output_resource, clear_color);
		//Tasks
		zest_tfx_AddPassTask(graphics_pass, &game->tfx_rendering);
		//If there's imgui to draw then draw it
		if (zest_imgui_AddToRenderGraph(graphics_pass)) {
			zest_AddPassTask(graphics_pass, zest_imgui_DrawImGuiRenderPass, NULL);
		}
		//--------------------------------------------------------------------------------------------------

		//-------------------------TimelineFX Transfer Pass-------------------------------------------------
		//if (zest_TimerUpdateWasRun(game->tfx_rendering.timer) && !game->pause) {
			zest_pass_node upload_tfx_data = zest_AddTransferPassNode("Upload TFX Pass");
			//Outputs
			zest_ConnectTransferBufferOutput(upload_tfx_data, tfx_layer);
			//Tasks
			zest_AddPassInstanceLayerUpload(upload_tfx_data, game->tfx_rendering.layer);
		//}
		//--------------------------------------------------------------------------------------------------

		//End the render graph. This tells Zest that it can now compile the render graph ready for executing.
		zest_EndRenderGraph();

		zest_render_graph render_graph = zest_ExecuteRenderGraph();
		if (game->request_graph_print) {
			//You can print out the render graph for debugging purposes
			zest_PrintCompiledRenderGraph(render_graph);
			game->request_graph_print = false;
		}
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main() {
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(zest_validation_flag_enable_sync);
	create_info.log_path = "./";
	create_info.thread_count = 0;
	ZEST__FLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	zest_implglfw_SetCallbacks(&create_info);

	TimelineFXExample game = { 0 };
	//Initialise TimelineFX with however many threads you want. Each emitter is updated it's own thread.
	tfx_InitialiseTimelineFX(tfx_GetDefaultThreadCount(), tfxMegabyte(128));
	//InitialiseTimelineFX(0, tfxMegabyte(128));

	zest_Initialise(&create_info);
	zest_SetUserData(&game);
	zest_SetUserUpdateCallback(UpdateTfxExample);
	game.Init();

	zest_Start();

	game.test_effects.free();
	game.memory_stats.free();
	tfx_EndTimelineFX();

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
