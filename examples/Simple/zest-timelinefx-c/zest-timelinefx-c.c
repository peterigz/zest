#include "timelinefx.h"
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

typedef struct tfx_render_resources_s {
	zest_texture particle_texture;
	zest_texture color_ramps_texture;
	zest_draw_routine draw_routine;
	zest_layer layer;
	zest_descriptor_buffer uniform_buffer_3d;
	zest_descriptor_set uniform_buffer_descriptor_set;
	zest_descriptor_buffer image_data;
	zest_descriptor_set particle_descriptor;
	zest_pipeline pipeline;
	zest_shader_resources shader_resource;
	zest_set_layout descriptor_layout;
	zest_descriptor_set_t descriptor_set;
	zest_shader fragment_shader;
	zest_shader vertex_shader;
} tfx_render_resources_t;

typedef struct TimelineFXExample_s {
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

} TimelineFXExample;

//Basic function for updating the uniform buffer
void UpdateUniform3d(TimelineFXExample *game) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t *)zest_GetUniformBufferData(game->tfx_rendering.uniform_buffer_3d);
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
void ShapeLoader(const char *filename, tfx_image_data_t *image_data, void *raw_image_data, int image_memory_size, void *custom_data) {
	//Cast your custom data, this can be anything you want
	TimelineFXExample *game = (TimelineFXExample*)custom_data;

	//This shape loader example uses the STB image library to load the raw bitmap (png usually) data
	zest_bitmap_t bitmap = zest_NewBitmap();
	zest_LoadBitmapImageMemory(&bitmap, (unsigned char *)raw_image_data, image_memory_size, 0);
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
	} else {
		//Add the image to the texture in our renderer
		image_data->ptr = zest_AddTextureImageBitmap(game->tfx_rendering.particle_texture, &bitmap);
		//Important step: you need to point the ImageData.ptr to the appropriate handle in the renderer to point to the texture of the particle shape
		//You'll need to use this in your render function to tell your renderer which texture to use to draw the particle
	}
}

void GetUV(void *ptr, tfx_gpu_image_data_t *image_data, int offset) {
	zest_image image = (zest_image)(ptr) + offset;
	image_data->uv.x = image->uv.x;
	image_data->uv.y = image->uv.y;
	image_data->uv.z = image->uv.z;
	image_data->uv.w = image->uv.w;
	image_data->texture_array_index = image->layer;
	image_data->uv_packed = image->uv_packed;
}

//Allows us to cast a ray into the screen from the mouse position to place an effect where we click
tfx_vec3_t ScreenRay(float x, float y, float depth_offset, zest_vec3 *camera_position, zest_descriptor_buffer buffer) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t *)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), *camera_position);
	tfx_vec3_t ray = { pos.x, pos.y, pos.z };
	return ray;
}

void InitTimelineFXRenderResources(tfx_render_resources_t *render_resources, const char *library_path) {
	render_resources->uniform_buffer_3d = zest_CreateUniformBuffer("3d uniform", sizeof(zest_uniform_buffer_data_t));
	render_resources->uniform_buffer_descriptor_set = zest_CreateUniformDescriptorSet(render_resources->uniform_buffer_3d);

	int shape_count = tfx_GetShapeCountInLibrary(library_path);
	render_resources->particle_texture = zest_CreateTexture("Particle Texture", zest_texture_storage_type_packed, zest_texture_flag_use_filtering, zest_texture_format_rgba_unorm, shape_count);
	render_resources->color_ramps_texture = zest_CreateTextureBank("Particle Color Ramps", zest_texture_format_rgba_unorm);
	zest_SetTextureUseFiltering(render_resources->color_ramps_texture, false);

	//Compile the shaders we will use to render the particles
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	render_resources->fragment_shader = zest_CreateShaderFromFile("examples/assets/shaders/timelinefx.frag", "tfx_frag.spv", shaderc_fragment_shader, true, compiler, 0);
	render_resources->vertex_shader = zest_CreateShaderFromFile("examples/assets/shaders/timelinefx3d.vert", "tfx_vertex3d.spv", shaderc_vertex_shader, true, compiler, 0);
	shaderc_compiler_release(compiler);

	//To render the particles we setup a pipeline with the vertex attributes and shaders to render the particles.
	//First create a descriptor set layout, we need 2 samplers, one to sample the particle texture and another to sample the color ramps
	//We also need 2 storage buffers, one to access the image data in the vertex shader and the other to access the previous frame particles
	//so that they can be interpolated in between updates
	render_resources->descriptor_layout = zest_AddDescriptorLayout("2 samplers 2 storage", 0, 2, 2, 0);

	zest_pipeline_template_create_info_t instance_create_info = zest_CreatePipelineTemplateCreateInfo();
	instance_create_info.viewport.extent = zest_GetSwapChainExtent();
	//Set up the vertex attributes that will take in all of the billboard data stored in tfx_instance_t objects
	zest_AddVertexInputBindingDescription(&instance_create_info, 0, sizeof(tfx_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(tfx_instance_t, position)));	            // Location 0: Postion and stretch in w
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(tfx_instance_t, rotations)));	            // Location 1: Rotations
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R8G8B8_SNORM, offsetof(tfx_instance_t, alignment)));					// Location 2: Alignment
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 3, VK_FORMAT_R16G16B16A16_SSCALED, offsetof(tfx_instance_t, size_handle)));		    // Location 3: Size and handle of the sprite
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 4, VK_FORMAT_R16G16_SSCALED, offsetof(tfx_instance_t, intensity_gradient_map)));    		    // Location 4: 2 intensities for each color
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 5, VK_FORMAT_R8G8B8_UNORM, offsetof(tfx_instance_t, curved_alpha_life)));               	// Location 5: Sharpness and mix lerp value
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 6, VK_FORMAT_R32_UINT, offsetof(tfx_instance_t, indexes)));							// Location 6: texture indexes to sample the correct image and color ramp
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 7, VK_FORMAT_R32_UINT, offsetof(tfx_instance_t, captured_index)));   				// Location 7: index of the sprite in the previous buffer when double buffering
	//Set the shaders to our custom timelinefx shaders
	zest_SetPipelineTemplateVertShader(&instance_create_info, "tfx_vertex3d.spv", 0);
	zest_SetPipelineTemplateFragShader(&instance_create_info, "tfx_frag.spv", 0);
	zest_SetPipelineTemplatePushConstant(&instance_create_info, sizeof(zest_push_constants_t), 0, VK_SHADER_STAGE_VERTEX_BIT);
	zest_AddPipelineTemplateDescriptorLayout(&instance_create_info, render_resources->descriptor_layout->vk_layout);
	render_resources->pipeline = zest_CreatePipelineTemplate("tfx_billboard_pipeline");
	zest_FinalisePipelineTemplate(render_resources->pipeline, zest_GetStandardRenderPass(), &instance_create_info);
	render_resources->pipeline->pipeline_template.colorBlendAttachment = zest_PreMultiplyBlendState();
	render_resources->pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
	render_resources->pipeline->pipeline_template.depthStencil.depthTestEnable = true;
	zest_BuildPipeline(render_resources->pipeline);

	//Create a drawroutine specifically for the tfx_instance_t object
	render_resources->draw_routine = zest_CreateInstanceDrawRoutine("timelinefx draw routine", sizeof(tfx_instance_t), 25000);


	//Set up the draw layers we need in the renderer
	zest_SetDrawCommandsClsColor(zest_GetCommandQueueDrawCommands("Default Draw Commands"), 0.f, 0.f, .2f, 1.f);


	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			render_resources->layer = zest_AddInstanceDrawRoutine(render_resources->draw_routine);
		}
		zest_FinishQueueSetup();
	}

	//We want to be able to manually change the current frame in flight in the layer that we use to draw all the billboards.
	//This means that we are able to only change the current frame in flight if we actually updated the particle manager in the current
	//frame allowing us to dictate when to upload the instance buffer to the gpu as there's no need to do it every frame, only when 
	//the particle manager is actually updated.
	zest_SetLayerToManualFIF(render_resources->layer);

	//Create a buffer to store the image data on the gpu. Note that we don't need this buffer to have multiple frames in flight
	render_resources->image_data = zest_CreateStorageDescriptorBuffer(sizeof(tfx_gpu_image_data_t) * 1000, false);

	//End of render specific code
}

void UpdateTimelineFXImageData(tfx_render_resources_t *tfx_rendering, tfx_library library) {
	//Upload the timelinefx image data to the image data buffer created
	zest_buffer image_data_buffer = zest_GetBufferFromDescriptorBuffer(tfx_rendering->image_data);
	tfx_gpu_shapes shapes = tfx_GetLibraryGPUShapes(library);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(tfx_GetGPUShapesSizeInBytes(shapes), tfx_GetGPUShapesArray(shapes));
	zest_CopyBuffer(staging_buffer, zest_GetBufferFromDescriptorBuffer(tfx_rendering->image_data), tfx_GetGPUShapesSizeInBytes(shapes));
	zest_FreeBuffer(staging_buffer);
}

void CreateTimelineFXShaderResources(tfx_render_resources_t *tfx_rendering) {
	//We need a descriptor set for the shader resources that we will use in the pipeline we created above
	zest_descriptor_set_builder_t set_builder = zest_BeginDescriptorSetBuilder();
	zest_AddSetBuilderDescriptorWriteStorageBuffer(&set_builder, tfx_rendering->image_data, 0);
	zest_AddSetBuilderDescriptorWriteInstanceLayerLerp(&set_builder, tfx_rendering->layer, 1);
	zest_AddSetBuilderDescriptorWriteImage(&set_builder, zest_GetTextureDescriptorImageInfo(tfx_rendering->particle_texture), 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	zest_AddSetBuilderDescriptorWriteImage(&set_builder, zest_GetTextureDescriptorImageInfo(tfx_rendering->color_ramps_texture), 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	tfx_rendering->descriptor_set = zest_FinishDescriptorSet(&set_builder, tfx_rendering->descriptor_layout, zest_descriptor_type_static);

	//Finally, set up a shader resource to be used when sending the draw calls to the gpu in our render function
	//This will have the uniform buffer in set 0 and the texures and storage buffers in set 1
	tfx_rendering->shader_resource = zest_CreateShaderResources();
	zest_AddDescriptorSetToResources(tfx_rendering->shader_resource, tfx_rendering->uniform_buffer_descriptor_set);
	zest_AddDescriptorSetToResources(tfx_rendering->shader_resource, &tfx_rendering->descriptor_set);
}

void Init(TimelineFXExample *example) {
	InitTimelineFXRenderResources(&example->tfx_rendering, "examples/assets/effects.tfx");
	float max_radius = 0;

	//Load the effects library and pass the shape loader function pointer that you created earlier. Also pass this pointer to point to this object to give the shapeloader access to the texture we're loading the particle images into
	example->library = tfx_LoadEffectLibrary("examples/assets/effects.tfx", ShapeLoader, GetUV, example);
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
	UpdateTimelineFXImageData(&example->tfx_rendering, example->library);
	CreateTimelineFXShaderResources(&example->tfx_rendering);

	//Application specific, set up a timer for the update loop
	example->timer = zest_CreateTimer(60);

	example->camera = zest_CreateCamera();
	zest_CameraSetFoV(&example->camera, 60.f);

	UpdateUniform3d(example);

	/*
	Initialise a particle manager. This manages effects, emitters and the particles that they spawn. First call tfx_CreateParticleManagerInfo and pass in a setup mode to create an info object with the config we need.
	If you need to you can tweak this further before passing into InitializingParticleManager.

	In this example we'll setup a particle manager for 3d effects and group the sprites by each effect.
	*/
	tfx_effect_manager_info_t pm_info = tfx_CreateEffectManagerInfo(tfxEffectManagerSetup_group_sprites_by_effect);
	example->pm = tfx_CreateEffectManager(pm_info);

	//Renderer specific that sets up some draw layers
	zest_SetDrawCommandsClsColor(zest_GetCommandQueueDrawCommands("Default Draw Commands"), 0.1f, 0.1f, .1f, 1.f);


	zest_TimerReset(example->timer);
	//End of render specific code
}

//A simple example to render the particles. This is for when the particle manager has one single list of sprites rather than grouped by effect
void RenderParticles(tfx_effect_manager_t *pm, TimelineFXExample *game) {
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_SetInstanceDrawing(game->tfx_rendering.layer, game->tfx_rendering.shader_resource, game->tfx_rendering.pipeline);

	tfx_instance_t *billboards = tfx_GetInstanceBuffer(pm);
	zest_draw_buffer_result result = zest_DrawInstanceBuffer(game->tfx_rendering.layer, billboards, tfx_GetInstanceCount(game->pm));
}

//Application specific, this just sets the function to call each render update
void UpdateTfxExample(zest_microsecs ellapsed, void *data) {
	TimelineFXExample *game = (TimelineFXExample*)data;

	//Renderer specific
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);

	UpdateUniform3d(game);
	zest_Update2dUniformBuffer();

	if (zest_MouseHit(zest_left_mouse)) {
		//Each time you add an effect to the particle manager it generates an ID which you can use to modify the effect whilst it's being updated
		tfxEffectID effect_id;
		//Add the effect template to the particle manager
		if (tfx_AddEffectTemplateToEffectManager(game->pm, game->effect_template1, &effect_id)) {
			//Calculate a position in 3d by casting a ray into the screen using the mouse coordinates
			tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 12.f, &game->camera.position, game->tfx_rendering.uniform_buffer_3d);
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
			tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 12.f, &game->camera.position, game->tfx_rendering.uniform_buffer_3d);
			//Set the effect position
			tfx_SetEffectPositionVec3(game->pm, effect_id, position);
		}
	}

	zest_TimerAccumulate(game->timer);
	int pending_ticks = zest_TimerPendingTicks(game->timer);
	while (zest_TimerDoUpdate(game->timer)) {

		//Update the particle manager but only if pending ticks is > 0. This means that if we're trying to catch up this frame
		//then rather then run the update particle manager multiple times, simply run it once but multiply the frame length
		//instead. This is important in order to keep the billboard buffer on the gpu in sync for interpolating the particles
		//with the previous frame. It's also just more efficient to do this.
		if (pending_ticks > 0) {
			tfx_UpdateEffectManager(game->pm, FrameLength);
			pending_ticks = 0;
		}

		zest_TimerUnAccumulate(game->timer);
	}

	zest_TimerSet(game->timer);

	//Render the particles with our custom render function if they were updated this frame. If not then the render pipeline
	//will continue to interpolate the particle positions with the last frame update. This minimises the amount of times we
	//have to upload the latest billboards to the gpu.
	if (zest_TimerUpdateWasRun(game->timer)) {
		zest_ResetInstanceLayer(game->tfx_rendering.layer);
		RenderParticles(game->pm, game);
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
