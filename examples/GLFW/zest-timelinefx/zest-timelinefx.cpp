#include <zest.h>
#include "imgui.h"
#include "impl_imgui.h"
#include "impl_glfw.h"
#include "impl_imgui_glfw.h"
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

struct tfx_push_constants_t {
	float parameters[4];
	tfxU32 particle_texture_index;
	tfxU32 color_ramp_texture_index;
	tfxU32 image_data_index;
	tfxU32 prev_billboards_index;
};

struct tfx_render_resources_t {
	zest_texture particle_texture;
	zest_texture color_ramps_texture;
	zest_layer layer;
	zest_uniform_buffer uniform_buffer_3d;
	zest_descriptor_buffer image_data;
	zest_pipeline_template pipeline;
	zest_shader fragment_shader;
	zest_shader vertex_shader;
	zest_shader_resources shader_resource;
};

struct TimelineFXExample {
	zest_timer timer;
	zest_camera_t camera;
	zest_texture imgui_font_texture;
	tfx_render_resources_t tfx_rendering;

	tfx_library library;
	tfx_effect_manager pm;

	tfx_effect_template effect_template1;
	tfx_effect_template effect_template2;

	tfxEffectID effect_id;
	zest_imgui_t imgui_layer_info;

	void Init();
};

//Basic function for updating the uniform buffer
void UpdateUniform3d(TimelineFXExample *game) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(game->tfx_rendering.uniform_buffer_3d);
	buffer_3d->view = zest_LookAt(game->camera.position, zest_AddVec3(game->camera.position, game->camera.front), game->camera.up);
	buffer_3d->proj = zest_Perspective(game->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.1f, 10000.f);
	buffer_3d->proj.v[1].y *= -1.f;
	buffer_3d->screen_size.x = zest_ScreenWidthf();
	buffer_3d->screen_size.y = zest_ScreenHeightf();
	buffer_3d->millisecs = 0;
	buffer_3d->parameters1.x = (float)zest_TimerLerp(game->timer);
	buffer_3d->parameters1.y = (float)zest_TimerUpdateTime(game->timer);
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
		image_data->ptr = zest_AddTextureAnimationBitmap(game->tfx_rendering.particle_texture, &bitmap, (u32)image_data->image_size.x, (u32)image_data->image_size.y, (u32)image_data->animation_frames, &max_radius, 1);
		//Important step: you need to point the ImageData.ptr to the appropriate handle in the renderer to point to the texture of the particle shape
		//You'll need to use this in your render function to tell your renderer which texture to use to draw the particle
	}
	else {
		//Add the image to the texture in our renderer
		image_data->ptr = zest_AddTextureImageBitmap(game->tfx_rendering.particle_texture, &bitmap);
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
tfx_vec3_t ScreenRay(float x, float y, float depth_offset, zest_vec3 &camera_position, zest_uniform_buffer buffer) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

void InitTimelineFXRenderResources(tfx_render_resources_t &render_resources, const char *library_path) {
	render_resources.uniform_buffer_3d = zest_CreateUniformBuffer("3d uniform", sizeof(zest_uniform_buffer_data_t));

	int shape_count = tfx_GetShapeCountInLibrary(library_path);
	render_resources.particle_texture = zest_CreateTexture("Particle Texture", zest_texture_storage_type_packed, zest_texture_flag_use_filtering, zest_texture_format_rgba_unorm, shape_count);
	render_resources.color_ramps_texture = zest_CreateTextureBank("Particle Color Ramps", zest_texture_format_rgba_unorm);
	zest_SetTextureUseFiltering(render_resources.color_ramps_texture, false);

	//Compile the shaders we will use to render the particles
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	render_resources.fragment_shader = zest_CreateShaderFromFile("examples/assets/shaders/timelinefx.frag", "tfx_frag.spv", shaderc_fragment_shader, true, compiler, 0);
	render_resources.vertex_shader = zest_CreateShaderFromFile("examples/assets/shaders/timelinefx3d.vert", "tfx_vertex.spv", shaderc_vertex_shader, true, compiler, 0);
	shaderc_compiler_release(compiler);

	//To render the particles we setup a pipeline with the vertex attributes and shaders to render the particles.
	//First create a descriptor set layout, we need 2 samplers, one to sample the particle texture and another to sample the color ramps
	//We also need 2 storage buffers, one to access the image data in the vertex shader and the other to access the previous frame particles
	//so that they can be interpolated in between updates

	render_resources.pipeline = zest_CreatePipelineTemplate("Timelinefx pipeline");
	//Set up the vertex attributes that will take in all of the billboard data stored in tfx_instance_t objects
	zest_AddVertexInputBindingDescription(render_resources.pipeline, 0, sizeof(tfx_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);
	zest_AddVertexInputDescription(render_resources.pipeline, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(tfx_instance_t, position)));	            // Location 0: Postion and stretch in w
	zest_AddVertexInputDescription(render_resources.pipeline, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(tfx_instance_t, rotations)));	                // Location 1: Rotations
	zest_AddVertexInputDescription(render_resources.pipeline, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R8G8B8_SNORM, offsetof(tfx_instance_t, alignment)));					    // Location 2: Alignment
	zest_AddVertexInputDescription(render_resources.pipeline, zest_CreateVertexInputDescription(0, 3, VK_FORMAT_R16G16B16A16_SSCALED, offsetof(tfx_instance_t, size_handle)));		    // Location 3: Size and handle of the sprite
	zest_AddVertexInputDescription(render_resources.pipeline, zest_CreateVertexInputDescription(0, 4, VK_FORMAT_R16G16_SSCALED, offsetof(tfx_instance_t, intensity_gradient_map)));       // Location 4: 2 intensities for each color
	zest_AddVertexInputDescription(render_resources.pipeline, zest_CreateVertexInputDescription(0, 5, VK_FORMAT_R8G8B8_UNORM, offsetof(tfx_instance_t, curved_alpha_life)));          	// Location 5: Sharpness and mix lerp value
	zest_AddVertexInputDescription(render_resources.pipeline, zest_CreateVertexInputDescription(0, 6, VK_FORMAT_R32_UINT, offsetof(tfx_instance_t, indexes)));							// Location 6: texture indexes to sample the correct image and color ramp
	zest_AddVertexInputDescription(render_resources.pipeline, zest_CreateVertexInputDescription(0, 7, VK_FORMAT_R32_UINT, offsetof(tfx_instance_t, captured_index)));   				    // Location 7: index of the sprite in the previous buffer when double buffering
	//Set the shaders to our custom timelinefx shaders
	zest_SetPipelineTemplateVertShader(render_resources.pipeline, "tfx_vertex.spv", 0);
	zest_SetPipelineTemplateFragShader(render_resources.pipeline, "tfx_frag.spv", 0);
	zest_SetPipelineTemplatePushConstantRange(render_resources.pipeline, sizeof(tfx_push_constants_t), 0, zest_shader_render_stages);
	zest_AddPipelineTemplateDescriptorLayout(render_resources.pipeline, zest_vk_GetUniformBufferLayout(render_resources.uniform_buffer_3d));
	zest_AddPipelineTemplateDescriptorLayout(render_resources.pipeline, zest_vk_GetGlobalBindlessLayout());
	zest_FinalisePipelineTemplate(render_resources.pipeline);
	render_resources.pipeline->colorBlendAttachment = zest_PreMultiplyBlendState();
	render_resources.pipeline->depthStencil.depthWriteEnable = VK_FALSE;
	render_resources.pipeline->depthStencil.depthTestEnable = true;

	//We want to be able to manually change the current frame in flight in the layer that we use to draw all the billboards.
	//This means that we are able to only change the current frame in flight if we actually updated the particle manager in the current
	//frame allowing us to dictate when to upload the instance buffer to the gpu as there's no need to do it every frame, only when 
	//the particle manager is actually updated.
	render_resources.layer = zest_CreateInstanceLayer("timelinefx draw routine", sizeof(tfx_instance_t));
	zest_SetLayerToManualFIF(render_resources.layer);

	//Create a buffer to store the image data on the gpu. 
	render_resources.image_data = zest_CreateStorageDescriptorBuffer(sizeof(tfx_gpu_image_data_t) * 1000);
	zest_AcquireGlobalStorageBufferIndex(render_resources.image_data, zest_SSBOBinding("Image Data Buffers"));

	//End of render specific code
}

void UpdateTimelineFXImageData(tfx_render_resources_t &tfx_rendering, tfx_library library) {
	//Upload the timelinefx image data to the image data buffer created
	zest_buffer image_data_buffer = zest_GetBufferFromDescriptorBuffer(tfx_rendering.image_data);
	tfx_gpu_shapes shapes = tfx_GetLibraryGPUShapes(library);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(tfx_GetGPUShapesSizeInBytes(shapes), tfx_GetGPUShapesArray(shapes));
	zest_CopyBufferOneTime(staging_buffer, zest_GetBufferFromDescriptorBuffer(tfx_rendering.image_data), tfx_GetGPUShapesSizeInBytes(shapes));
	zest_FreeBuffer(staging_buffer);
}

void CreateTimelineFXShaderResources(tfx_render_resources_t &tfx_rendering) {
	tfx_rendering.shader_resource = zest_CreateShaderResources();
	zest_ForEachFrameInFlight(fif) {
		zest_AddDescriptorSetToResources(tfx_rendering.shader_resource, zest_GetUniformBufferSet(tfx_rendering.uniform_buffer_3d), fif);
		zest_AddDescriptorSetToResources(tfx_rendering.shader_resource, zest_GetGlobalBindlessSet(), fif);
	}
}

void TimelineFXExample::Init() {
	InitTimelineFXRenderResources(tfx_rendering, "examples/assets/vaders/vadereffects.tfx");
	float max_radius = 0;

	//Load the effects library and pass the shape loader function pointer that you created earlier. Also pass this pointer to point to this object to give the shapeloader access to the texture we're loading the particle images into
	library = tfx_LoadEffectLibrary("examples/assets/vaders/vadereffects.tfx", ShapeLoader, GetUV, this);
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
	UpdateTimelineFXImageData(tfx_rendering, library);
	CreateTimelineFXShaderResources(tfx_rendering);

	//Application specific, set up a timer for the update loop
	timer = zest_CreateTimer(60);

	camera = zest_CreateCamera();
	zest_CameraSetFoV(&camera, 60.f);

	UpdateUniform3d(this);

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
	ImGui::End();

	ImGui::Render();
	zest_imgui_UpdateBuffers();
}

//A simple example to render the particles. This is for when the particle manager has one single list of sprites rather than grouped by effect
void RenderParticles(tfx_effect_manager pm, TimelineFXExample *game) {
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_texture textures[] = {
		game->tfx_rendering.particle_texture,
		game->tfx_rendering.color_ramps_texture
	};
	zest_SetInstanceDrawing(game->tfx_rendering.layer, game->tfx_rendering.shader_resource, textures, 2, game->tfx_rendering.pipeline);

	tfx_instance_t *billboards = tfx_GetInstanceBuffer(pm);
	zest_draw_buffer_result result = zest_DrawInstanceBuffer(game->tfx_rendering.layer, billboards, tfx_GetInstanceCount(pm));
}

//Application specific, this just sets the function to call each render update
void UpdateTfxExample(zest_microsecs ellapsed, void *data) {
	TimelineFXExample *game = static_cast<TimelineFXExample*>(data);

	UpdateUniform3d(game);
	zest_Update2dUniformBuffer();

	zest_StartTimerLoop(game->timer) {
		BuildUI(game);

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			//Each time you add an effect to the particle manager it generates an ID which you can use to modify the effect whilst it's being updated
			tfxEffectID effect_id;
			//Add the effect template to the particle manager
			if (tfx_AddEffectTemplateToEffectManager(game->pm, game->effect_template1, &effect_id)) {
				//Calculate a position in 3d by casting a ray into the screen using the mouse coordinates
				tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 10.f, game->camera.position, game->tfx_rendering.uniform_buffer_3d);
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
				tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 10.f, game->camera.position, game->tfx_rendering.uniform_buffer_3d);
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

	} zest_EndTimerLoop(game->timer);

	//Render the particles with our custom render function if they were updated this frame. If not then the render pipeline
	//will continue to interpolate the particle positions with the last frame update. This minimises the amount of times we
	//have to upload the latest billboards to the gpu.
	if (zest_TimerUpdateWasRun(game->timer)) {
		zest_ResetInstanceLayer(game->tfx_rendering.layer);
		RenderParticles(game->pm, game);
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
		zest_resource_node tfx_layer = zest_AddInstanceLayerBufferResource(game->tfx_rendering.layer, zest_SSBOBinding("Billboard Buffers"));
		zest_resource_node tfx_image_data = zest_ImportStorageBufferResource("Image Data", game->tfx_rendering.image_data, zest_SSBOBinding("Image Data Buffers"));

		//Connect buffers and textures
		zest_ConnectTransferBufferOutput(upload_tfx_data, tfx_layer);
		zest_ConnectVertexBufferInput(graphics_pass, tfx_layer);
		zest_ConnectSampledImageInput(graphics_pass, particle_texture, zest_pipeline_fragment_stage);
		zest_ConnectSampledImageInput(graphics_pass, color_ramps_texture, zest_pipeline_fragment_stage);
		zest_ConnectSwapChainOutput(graphics_pass, swapchain_output_resource, clear_color);

		//If there's imgui to draw then draw it
		zest_AddPassTask(upload_tfx_data , zest_UploadInstanceLayerData, game->tfx_rendering.layer);
		zest_AddPassInstanceLayer(graphics_pass, game->tfx_rendering.layer);
		if (zest_imgui_AddToRenderGraph(graphics_pass)) {
			zest_AddPassTask(graphics_pass, zest_imgui_DrawImGuiRenderPass, NULL);
		}
		//End the render graph. This tells Zest that it can now compile the render graph ready for executing.
		zest_EndRenderGraph();
		//zest_PrintCompiledRenderGraph();
		//Execute the render graph. This must come after the EndRenderGraph function
		zest_ExecuteRenderGraph();
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main() {
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(zest_validation_flag_enable_sync);
    create_info.log_path = ".";
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	zest_implglfw_SetCallbacks(&create_info);

	TimelineFXExample game;
	//Initialise TimelineFX with however many threads you want. Each emitter is updated in it's own thread.
	tfx_InitialiseTimelineFX(tfx_GetDefaultThreadCount(), tfxMegabyte(128));

	zest_RegisterSSBOBuffer(&create_info, "Billboard Buffers", 2, zest_shader_vertex_stage);
	zest_RegisterSSBOBuffer(&create_info, "Image Data Buffers", 2, zest_shader_vertex_stage);

	zest_Initialise(&create_info);
	zest_LogFPSToConsole(1);
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
