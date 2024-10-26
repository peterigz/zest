#include <zest.h>
#include "imgui.h"
#include "impl_imgui.h"
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
	zest_texture color_ramps_texture;
	zest_texture imgui_font_texture;
	zest_shader_resources timelinefx_shader_resource;
	zest_descriptor_set_layout timelinefx_descriptor_layout;
	zest_descriptor_set_t timelinefx_descriptor_set;

	zest_shader fragment_shader;
	zest_shader vertex_shader;

	tfx_library_t library;
	tfx_particle_manager_t pm;

	tfx_effect_template_t effect_template1;
	tfx_effect_template_t effect_template2;
	tfx_effect_template_t cube_ordered;

	tfxEffectID effect_id;
	zest_draw_routine timelinefx_draw_routine;
	zest_layer timelinefx_layer;
	zest_descriptor_buffer uniform_buffer_3d;
	zest_descriptor_set uniform_buffer_descriptor_set;
	zest_descriptor_buffer timelinefx_image_data;
	zest_descriptor_set particle_descriptor;
	zest_pipeline timelinefx_pipeline;

	tfx_vector_t<tfxEffectID> test_effects;

	zest_imgui_layer_info_t imgui_layer_info;
	tfx_random_t random;
	tfx_vector_t<tfx_pool_stats_t> memory_stats;

	float test_depth = 1.f;

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
	buffer_3d->parameters1.x = (float)zest_TimerLerp(game->timer);
}

//Before you load an effects file, you will need to define a ShapeLoader function and a GetUV function to update all the shapes in 
//the library with the correct uv coords.
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

//Here is an example GetUV function to update the image data with the correct UV coords. This is uploaded to the GPU and accessed
//in the pixel shader.
void GetUV(void *ptr, tfx_gpu_image_data_t *image_data, int offset) {
	zest_image image = (static_cast<zest_image>(ptr) + offset);
	image_data->uv = { image->uv.x, image->uv.y, image->uv.z, image->uv.w };
	image_data->texture_array_index = image->layer;
	image_data->max_radius = image->max_radius;
}


//Allows us to cast a ray into the screen from the mouse position to place an effect where we click
tfx_vec3_t ScreenRay(float x, float y, float depth_offset, zest_vec3 &camera_position, zest_descriptor_buffer buffer) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

void TimelineFXExample::Init() {
	//Renderer specific - initialise the textures for particle shapes and color ramps
	float max_radius = 0;
	uniform_buffer_3d = zest_CreateUniformBuffer("3d uniform", sizeof(zest_uniform_buffer_data_t));
	uniform_buffer_descriptor_set = zest_CreateUniformDescriptorSet(uniform_buffer_3d);


	int shape_count = GetShapeCountInLibrary("examples/assets/vaders/vadereffects.tfx");
	particle_texture = zest_CreateTexture("Particle Texture", zest_texture_storage_type_packed, zest_texture_flag_use_filtering, zest_texture_format_rgba, shape_count);
	color_ramps_texture = zest_CreateTextureBank("Particle Color Ramps", zest_texture_format_rgba);
	zest_SetTextureUseFiltering(color_ramps_texture, false);
	//Load the effects library and pass the shape loader function pointer that you created earlier. Also pass this pointer to point to this object to give the shapeloader access to the texture we're loading the particle images into
	LoadEffectLibrary("examples/assets/effects.tfx", &library, ShapeLoader, GetUV, this);
	//Renderer specific
	//Process the texture with all the particle shapes that we just added to
	zest_ProcessTextureImages(particle_texture);
	//Add the color ramps from the library to the color ramps texture. Color ramps in the library are stored in rgba format and can be
	//simply copied to a bitmap for uploading to the texture
	for (tfx_bitmap_t &bitmap : library.color_ramp_bitmaps) {
		zest_bitmap_t temp_bitmap = zest_CreateBitmapFromRawBuffer("", bitmap.data, (int)bitmap.size, bitmap.width, bitmap.height, bitmap.channels);
		zest_AddTextureImageBitmap(color_ramps_texture, &temp_bitmap);
	}
	//Process the color ramp texture to upload it all to the gpu
	zest_ProcessTextureImages(color_ramps_texture);
	//Now that the particle shapes have been setup in the renderer, we can call this function to update the shape data in the library
	//with the correct uv texture coords ready to upload to gpu. This buffer will be accessed in the vertex shader when rendering.
	UpdateLibraryGPUImageData(&library);

	//Compile the shaders we will use to render the particles
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	fragment_shader = zest_CreateShaderFromFile("examples/assets/shaders/timelinefx.frag", "tfx_frag.spv", shaderc_fragment_shader, false, compiler);
	vertex_shader = zest_CreateShaderFromFile("examples/assets/shaders/timelinefx3d.vert", "tfx_vertex3d.spv", shaderc_vertex_shader, false, compiler);
	shaderc_compiler_release(compiler);

	//To render the particles we setup a pipeline with the vertex attributes and shaders to render the particles.
	//First create a descriptor set layout, we need 2 samplers, one to sample the particle texture and another to sample the color ramps
	//We also need 2 storage buffers, one to access the image data in the vertex shader and the other to access the previous frame particles
	//so that they can be interpolated in between updates
	timelinefx_descriptor_layout = zest_AddDescriptorLayout("2 samplers 2 storage", zest_CreateDescriptorSetLayout(0, 2, 2));

	zest_pipeline_template_create_info_t instance_create_info = zest_CreatePipelineTemplateCreateInfo();
	instance_create_info.viewport.extent = zest_GetSwapChainExtent();
	//Set up the vertex attributes that will take in all of the billboard data stored in tfx_billboard_instance_t objects
	zest_AddVertexInputBindingDescription(&instance_create_info, 0, sizeof(tfx_billboard_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(tfx_billboard_instance_t, position)));	            // Location 0: Postion and stretch in w
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(tfx_billboard_instance_t, rotations)));	            // Location 1: Rotations
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R8G8B8_SNORM, offsetof(tfx_billboard_instance_t, alignment)));					// Location 2: Alignment
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 3, VK_FORMAT_R16G16B16A16_SSCALED, offsetof(tfx_billboard_instance_t, size_handle)));		    // Location 3: Size and handle of the sprite
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 4, VK_FORMAT_R16G16_SSCALED, offsetof(tfx_billboard_instance_t, intensity_life)));    		    // Location 4: 2 intensities for each color
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 5, VK_FORMAT_R16G16_SNORM, offsetof(tfx_billboard_instance_t, curved_alpha)));               	// Location 5: Sharpness and mix lerp value
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 6, VK_FORMAT_R32_UINT, offsetof(tfx_billboard_instance_t, indexes)));							// Location 6: texture indexes to sample the correct image and color ramp
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 7, VK_FORMAT_R32_UINT, offsetof(tfx_billboard_instance_t, captured_index)));   				// Location 7: index of the sprite in the previous buffer when double buffering
	//Set the shaders to our custom timelinefx shaders
	zest_SetPipelineTemplateVertShader(&instance_create_info, "tfx_vertex3d.spv", 0);
	zest_SetPipelineTemplateFragShader(&instance_create_info, "tfx_frag.spv", 0);
	zest_SetPipelineTemplatePushConstant(&instance_create_info, sizeof(zest_push_constants_t), 0, VK_SHADER_STAGE_VERTEX_BIT);
	zest_AddPipelineTemplateDescriptorLayout(&instance_create_info, timelinefx_descriptor_layout->vk_layout);
	timelinefx_pipeline = zest_AddPipeline("tfx_billboard_pipeline");
	zest_MakePipelineTemplate(timelinefx_pipeline, zest_GetStandardRenderPass(), &instance_create_info);
	timelinefx_pipeline->pipeline_template.colorBlendAttachment = zest_PreMultiplyBlendState();
	timelinefx_pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
	timelinefx_pipeline->pipeline_template.depthStencil.depthTestEnable = true;
	zest_BuildPipeline(timelinefx_pipeline);

	//Create a drawroutine specifically for the tfx_billboard_instance_t object
	timelinefx_draw_routine = zest_CreateInstanceDrawRoutine("timelinefx draw routine", sizeof(tfx_billboard_instance_t), 25000);

	//Application specific, set up a timer for the update loop
	timer = zest_CreateTimer(60);

	camera = zest_CreateCamera();
	zest_CameraPosition(&camera, { -2.5f, 0.f, 0.f });
	zest_CameraSetFoV(&camera, 60.f);

	UpdateUniform3d(this);

	//Set up the draw layers we need in the renderer
	zest_SetDrawCommandsClsColor(zest_GetCommandQueueDrawCommands("Default Draw Commands"), 0.f, 0.f, .2f, 1.f);

	zest_imgui_Initialise(&imgui_layer_info);

	imgui_layer_info.pipeline = zest_Pipeline("pipeline_imgui");
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			timelinefx_layer = zest_AddInstanceDrawRoutine(timelinefx_draw_routine);
			zest_imgui_CreateLayer(&imgui_layer_info);
		}
		zest_FinishQueueSetup();
	}
	zest_TimerReset(timer);

	//We want to be able to manually change the current frame in flight in the layer that we use to draw all the billboards.
	//This means that we are able to only change the current frame in flight if we actually updated the particle manager in the current
	//frame allowing us to dictate when to upload the instance buffer to the gpu as there's no need to do it every frame, only when 
	//the particle manager is actually updated.
	zest_SetLayerToManualFIF(timelinefx_layer);

	//Create a buffer to store the image data on the gpu. Note that we don't need this buffer to have multiple frames in flight
	timelinefx_image_data = zest_CreateStorageDescriptorBuffer(sizeof(tfx_gpu_image_data_t) * 1000, false);

	//Upload the timelinefx image data to the image data buffer created
	zest_buffer image_data_buffer = zest_GetBufferFromDescriptorBuffer(timelinefx_image_data);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(GetGPUShapesSizeInBytes(&library.gpu_shapes), GetGPUShapesPointer(&library.gpu_shapes));
	zest_CopyBuffer(staging_buffer, zest_GetBufferFromDescriptorBuffer(timelinefx_image_data), GetGPUShapesSizeInBytes(&library.gpu_shapes));
	zest_FreeBuffer(staging_buffer);

	//We need a descriptor set for the shader resources that we will use in the pipeline we created above
	zest_descriptor_set_builder_t set_builder = zest_NewDescriptorSetBuilder();
	zest_AddBuilderDescriptorWriteStorageBuffer(&set_builder, timelinefx_image_data, 0);
	zest_AddBuilderDescriptorWriteInstanceLayerLerp(&set_builder, timelinefx_layer, 1);
	zest_AddBuilderDescriptorWriteImage(&set_builder, zest_GetTextureDescriptorImageInfo(particle_texture), 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	zest_AddBuilderDescriptorWriteImage(&set_builder, zest_GetTextureDescriptorImageInfo(color_ramps_texture), 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	timelinefx_descriptor_set = zest_BuildDescriptorSet(&set_builder, timelinefx_descriptor_layout, zest_descriptor_type_static);

	//Finally, set up a shader resource to be used when sending the draw calls to the gpu in our render function
	//This will have the uniform buffer in set 0 and the texures and storage buffers in set 1
	timelinefx_shader_resource = zest_CreateShaderResources();
	zest_AddDescriptorSetToResources(timelinefx_shader_resource, uniform_buffer_descriptor_set);
	zest_AddDescriptorSetToResources(timelinefx_shader_resource, &timelinefx_descriptor_set);
	//End of render specific code

	/*
	Initialise a particle manager. This manages effects, emitters and the particles that they spawn. First call CreateParticleManagerInfo and pass in a setup mode to create an info object with the config we need.
	If you need to you can tweak this further before passing into InitializingParticleManager.
	In this example we'll setup a particle manager for 3d effects and group the sprites by each effect.
	*/
	tfx_particle_manager_info_t pm_info = CreateParticleManagerInfo(tfxParticleManagerSetup_3d_group_sprites_by_effect);
	InitializeParticleManager(&pm, &library, pm_info);
	SetPMCamera(&pm, &camera.front.x, &camera.position.x);

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
	PrepareEffectTemplate(&library, "Cube Ordered", &cube_ordered);

	random = NewRandom(zest_Millisecs());

	for (int i = 0; i != 10; ++i) {
		tfxEffectID effect_id;
		if (AddEffectToParticleManager(&pm, &cube_ordered, &effect_id)) {
			tfx_vec3_t position = {RandomRange(&random, 5.f), RandomRange(&random, -2.f, 2.f), RandomRange(&random, -4.f, 4.f)};
			SetEffectPosition(&pm, effect_id, position);
			test_effects.push_back(effect_id);
		}
	}
	test_depth++;

	/*
	memory_stats.reserve(100);
	tfx__lock_thread_access(tfxMemoryAllocator);
	for (int i = 0; i != GetGlobals()->memory_pool_count; ++i) {
		tfx_pool_stats_t stats = CreateMemorySnapshot(tfx__first_block_in_pool(GetGlobals()->memory_pools[i]));
		memory_stats.push_back(stats);
	}
	tfx__unlock_thread_access(tfxMemoryAllocator)
	*/
}

//Draw a Dear ImGui window to output some basic stats
void BuildUI(TimelineFXExample *game) {
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Effects");
	ImGui::Text("FPS: %i", zest_FPS());
	ImGui::Text("Particles: %i", ParticleCount(&game->pm));
	ImGui::Text("Effects: %i", EffectCount(&game->pm));
	ImGui::Text("Emitters: %i", EmitterCount(&game->pm));
	ImGui::Text("Free Emitters: %i", game->pm.free_emitters.size());
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
	zest_ResetLayer(game->imgui_layer_info.mesh_layer);
	zest_imgui_UpdateBuffers(game->imgui_layer_info.mesh_layer);
}

//A simple example to render the particles. This is for when the particle manager has one single list of sprites rather than grouped by effect
void RenderParticles3d(tfx_particle_manager_t& pm, TimelineFXExample* game) {
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_SetInstanceDrawing(game->timelinefx_layer, game->timelinefx_shader_resource, game->timelinefx_pipeline);

	tfx_billboard_instance_t *billboards = GetBillboardBuffer(&pm);
	zest_draw_buffer_result result = zest_DrawInstanceBuffer(game->timelinefx_layer, billboards, pm.instance_buffer.current_size);
}

//A simple example to render the particles. This is for when the particle manager groups all it's sprites by effect so that you can draw the effects in different orders if you need
void RenderParticles3dByEffect(tfx_particle_manager_t& pm, TimelineFXExample* game) {
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_SetInstanceDrawing(game->timelinefx_layer, game->timelinefx_shader_resource, game->timelinefx_pipeline);

	tfx_billboard_instance_t *billboards = nullptr;
	tfx_effect_instance_data_t *instance_data;
	tfxU32 instance_count = 0;
	bool halt = false;
	while (GetNextBillboardBuffer(&pm, &billboards, &instance_data, &instance_count)) {
		//tfxPrint("Offset: %i", instance_data->instance_start_index);
		zest_draw_buffer_result result = zest_DrawInstanceBuffer(game->timelinefx_layer, billboards, instance_count);
		/*
		if (pm.effects_in_use[0][pm.current_ebuff].current_size > 1) {
			for (int i = 0; i != instance_count; ++i) {
				tfxPrint("Range: %i - %i, count: %i -- %u", instance_data->instance_start_index, instance_data->instance_start_index + instance_count - 1, instance_count, billboards[i].captured_index & 0x0FFFFFFF);
			}
			if (pm.effect_index_position > 1 && instance_count > 200) halt = true;
		}
		*/
	}
	//tfxPrint("-------------------------");
	ResetInstanceBufferLoopIndex(&pm);
}

//Application specific, this just sets the function to call each render update
void UpdateTfxExample(zest_microsecs ellapsed, void *data) {
	TimelineFXExample *game = static_cast<TimelineFXExample*>(data);

	//Renderer specific
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);

	zest_TimerAccumulate(game->timer);
	int pending_ticks = zest_TimerPendingTicks(game->timer);

	while (zest_TimerDoUpdate(game->timer)) {
		BuildUI(game);

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			//Each time you add an effect to the particle manager it generates an ID which you can use to modify the effect whilst it's being updated
			tfxEffectID effect_id;
			//Add the effect template to the particle manager
			if(AddEffectToParticleManager(&game->pm, &game->effect_template1, &effect_id)) {
				//Calculate a position in 3d by casting a ray into the screen using the mouse coordinates
				tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 6.f, game->camera.position, game->uniform_buffer_3d);
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
				tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 6.f, game->camera.position, game->uniform_buffer_3d);
				//Set the effect position
				SetEffectPosition(&game->pm, effect_id, position);
			}
		}

		for (tfxEffectID &effect_id : game->test_effects) {
			float chance = GenerateRandom(&game->random);
			if (chance < 0.01f) {
				SoftExpireEffect(&game->pm, effect_id);
				if (AddEffectToParticleManager(&game->pm, &game->cube_ordered, &effect_id)) {
					tfx_vec3_t position = {RandomRange(&game->random, 5.f), RandomRange(&game->random, -2.f, 2.f), RandomRange(&game->random, -4.f, 4.f)};
					SetEffectPosition(&game->pm, effect_id, position);
				}
				game->test_depth++;
				/*
				tfx__lock_thread_access(tfxMemoryAllocator);
				game->memory_stats.clear();
				for (int i = 0; i != GetGlobals()->memory_pool_count; ++i) {
					tfx_pool_stats_t stats = CreateMemorySnapshot(tfx__first_block_in_pool(GetGlobals()->memory_pools[i]));
					game->memory_stats.push_back(stats);
				}
				tfx__unlock_thread_access(tfxMemoryAllocator)
				*/
			}
		}

		//Update the particle manager but only if pending ticks is > 0. This means that if we're trying to catch up this frame
		//then rather then run the update particle manager multiple times, simple run it once but multiply the frame length
		//instead. This is important in order to keep the billboard buffer on the gpu in sync for interpolating the particles
		//with the previous frame. It's also just more efficient to do this.
		if (pending_ticks > 0) {
			UpdateParticleManager(&game->pm, FrameLength);
			pending_ticks = 0;
		}

		zest_TimerUnAccumulate(game->timer);
	}

	zest_TimerSet(game->timer);

	UpdateUniform3d(game);

	//Render the particles with our custom render function if they were updated this frame. If not then the render pipeline
	//will continue to interpolate the particle positions with the last frame update. This minimises the amount of times we
	//have to upload the latest billboards to the gpu.
	if (zest_TimerUpdateWasRun(game->timer)) {
		zest_ResetInstanceLayer(game->timelinefx_layer);
		RenderParticles3dByEffect(game->pm, game);
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main() {
	zest_vec3 v = zest_Vec3Set(1.f, 0.f, 0.f);
	zest_uint packed = zest_Pack8bitx3(&v);
	zest_create_info_t create_info = zest_CreateInfo();
	create_info.log_path = "./";
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	zest_implglfw_SetCallbacks(&create_info);

	TimelineFXExample game;
	//Initialise TimelineFX with however many threads you want. Each emitter is updated it's own thread.
	InitialiseTimelineFX(std::thread::hardware_concurrency(), tfxMegabyte(128));
	//InitialiseTimelineFX(0, tfxMegabyte(128));

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
