#include <zest.h>
#include "imgui.h"
#include "impl_imgui.h"
#include "impl_glfw.h"
#include "impl_imgui_glfw.h"
#include "impl_timelinefx.h"

typedef unsigned int u32;

#define FrameLength 16.66666666667f

struct TimelineFXExample {
	zest_texture imgui_font_texture;
	tfx_render_resources_t tfx_rendering;

	tfx_library library;
	tfx_effect_manager pm;

	tfx_effect_template effect_template1;
	tfx_effect_template effect_template2;

	tfxEffectID effect_id;
	zest_imgui_t imgui_layer_info;
	bool request_graph_print;
	zest_microsecs relays[10];

	void Init();
};

//Allows us to cast a ray into the screen from the mouse position to place an effect where we click
tfx_vec3_t ScreenRay(float x, float y, float depth_offset, zest_vec3 &camera_position, zest_uniform_buffer buffer) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

void TimelineFXExample::Init() {
	zest_tfx_InitTimelineFXRenderResources(&tfx_rendering, "examples/assets/vaders/vadereffects.tfx");
	float max_radius = 0;

	//Load the effects library and pass the shape loader function pointer that you created earlier. Also pass this pointer to point to this object to give the shapeloader access to the texture we're loading the particle images into
	library = tfx_LoadEffectLibrary("examples/assets/vaders/vadereffects.tfx", zest_tfx_ShapeLoader, zest_tfx_GetUV, &tfx_rendering);
	//Renderer specific
	//Process the texture with all the particle shapes that we just added to
	zest_ProcessTextureImages(tfx_rendering.particle_texture);
	zest_AcquireGlobalCombinedImageSampler(tfx_rendering.particle_texture);

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
	for (int i = 0; i != bitmap_count; ++i) {
		tfx_bitmap_t *bitmap = tfx_GetColorRampBitmap(library, i);
		zest_bitmap_t temp_bitmap = zest_CreateBitmapFromRawBuffer("", bitmap->data, (int)bitmap->size, bitmap->width, bitmap->height, bitmap->channels);
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

	/*
	Initialise a particle manager. This manages effects, emitters and the particles that they spawn. First call tfx_CreateParticleManagerInfo and pass in a setup mode to create an info object with the config we need.
	If you need to you can tweak this further before passing into InitializingParticleManager.

	In this example we'll setup a particle manager for 3d effects and group the sprites by each effect.
	*/
	tfx_effect_manager_info_t pm_info = tfx_CreateEffectManagerInfo(tfxEffectManagerSetup_group_sprites_by_effect);
	pm = tfx_CreateEffectManager(pm_info);

	zest_imgui_Initialise();
}

//Draw a Dear ImGui window to output some basic stats
void BuildUI(TimelineFXExample *game) {
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Effects");
	ImGui::Text("FPS: %i", zest_FPS());
	ImGui::Text("Particles: %i", tfx_ParticleCount(game->pm));
	ImGui::Text("Effects: %i", tfx_EffectCount(game->pm));
	ImGui::Text("Emitters: %i", tfx_EmitterCount(game->pm));
	ImGui::Text("Free Emitters: %i", game->pm->free_emitters.size());
	if (ImGui::Button("Print Render Graph")) {
		game->request_graph_print = true;
	}
	ImGui::End();

	ImGui::Render();
	zest_imgui_UpdateBuffers();
}

//Application specific, this just sets the function to call each render update
void UpdateTfxExample(zest_microsecs ellapsed, void *data) {
	TimelineFXExample *game = static_cast<TimelineFXExample*>(data);

	zest_tfx_UpdateUniformBuffer(&game->tfx_rendering);

	zest_StartTimerLoop(game->tfx_rendering.timer) {
		BuildUI(game);

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			//Each time you add an effect to the particle manager it generates an ID which you can use to modify the effect whilst it's being updated
			tfxEffectID effect_id;
			//Add the effect template to the particle manager
			if (tfx_AddEffectTemplateToEffectManager(game->pm, game->effect_template1, &effect_id)) {
				//Calculate a position in 3d by casting a ray into the screen using the mouse coordinates
				tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 10.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer);
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
				tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 10.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer);
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
		zest_resource_node swapchain_output_resource = zest_ImportSwapChainResource("Swapchain Output");
		zest_pass_node graphics_pass = zest_AddRenderPassNode("Graphics Pass");
		zest_pass_node upload_tfx_data = zest_AddTransferPassNode("Upload TFX Pass");
		//If there was no imgui data to render then zest_imgui_AddToRenderGraph will return false
		//Import our test texture with the Bunny sprite
		zest_resource_node particle_texture = zest_ImportImageResourceReadOnly("Particle Texture", game->tfx_rendering.particle_texture);
		zest_resource_node color_ramps_texture = zest_ImportImageResourceReadOnly("Color Ramps Texture", game->tfx_rendering.color_ramps_texture);
		zest_resource_node tfx_layer = zest_AddInstanceLayerBufferResource(game->tfx_rendering.layer);
		zest_resource_node tfx_image_data = zest_ImportStorageBufferResource("Image Data", game->tfx_rendering.image_data);

		//Connect buffers and textures
		zest_ConnectTransferBufferOutput(upload_tfx_data, tfx_layer);
		zest_ConnectVertexBufferInput(graphics_pass, tfx_layer);
		zest_ConnectSampledImageInput(graphics_pass, particle_texture, zest_pipeline_fragment_stage);
		zest_ConnectSampledImageInput(graphics_pass, color_ramps_texture, zest_pipeline_fragment_stage);
		zest_ConnectSwapChainOutput(graphics_pass, swapchain_output_resource, clear_color);

		zest_AddPassInstanceLayerUpload(upload_tfx_data, game->tfx_rendering.layer);
		zest_AddPassTask(graphics_pass, zest_tfx_DrawParticleLayer, &game->tfx_rendering);
		//If there's imgui to draw then draw it
		if (zest_imgui_AddToRenderGraph(graphics_pass)) {
			zest_AddPassTask(graphics_pass, zest_imgui_DrawImGuiRenderPass, NULL);
		}
		//End the render graph. This tells Zest that it can now compile the render graph ready for executing.
		zest_EndRenderGraph();
		zest_render_graph render_graph = zest_ExecuteRenderGraph();
		if (game->request_graph_print) {
			//You can print out the render graph for debugging purposes
			zest_PrintCompiledRenderGraph(render_graph);
			game->request_graph_print = false;
		}		//Execute the render graph. This must come after the EndRenderGraph function
	}
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
//int main() {
	zest_create_info_t create_info = zest_CreateInfo();
    create_info.log_path = ".";
	ZEST__FLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	zest_implglfw_SetCallbacks(&create_info);

	TimelineFXExample game = {0};
	//Initialise TimelineFX with however many threads you want. Each emitter is updated in it's own thread.
	tfx_InitialiseTimelineFX(tfx_GetDefaultThreadCount(), tfxMegabyte(128));

	zest_Initialise(&create_info);
	zest_SetUserData(&game);
	zest_SetUserUpdateCallback(UpdateTfxExample);
	game.Init();

	zest_Start();

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
