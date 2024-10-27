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

#define UpdateFrequency 0.016666666666f
#define FrameLength 16.66666666667f

typedef unsigned int u32;

struct Player {
	tfx_vec3_t position;
	float rate_of_fire;
	float fire_count = 1.f;
};

struct PlayerBullet {
	tfx_vec3_t position;
	float speed = 20.f;
	tfxEffectID effect_index;
	bool remove = false;
};

struct VaderBullet {
	tfx_vec3_t captured;
	tfx_vec3_t position;
	tfx_vec3_t velocity;
	float speed = 2.f;
	float frame;
	bool remove = false;
};

enum VaderFlags {
	VaderFlags_none = 0,
	VaderFlags_charging_up = 1 << 0,
	VaderFlags_firing_laser = 1 << 1,
	VaderFlags_in_position = 1 << 2,
};

struct Vader {
	tfx_vec3_t captured;
	tfx_vec3_t position;
	tfx_vec3_t start_position;
	tfx_vec3_t end_position;
	tfx_vec3_t velocity;
	float time = 0.f;
	float speed = 1.f;
	float direction;
	zest_image image;
	int turning = 0;
	int health = 5;
	float chance_to_shoot;
	float start_angle;
	float end_angle;
	float angle = 0.f;
	tfxU32 flags = 0;
	tfxEffectID laser;
};

enum GameState {
	GameState_title,
	GameState_game,
	GameState_game_over,
};

struct tfx_render_resources_t {
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
	zest_descriptor_set_layout descriptor_layout;
	zest_descriptor_set_t descriptor_set;
	zest_shader fragment_shader;
	zest_shader vertex_shader;
};

struct VadersGame {
	tfx_random_t random;
	zest_timer timer;
	zest_camera_t camera;
	zest_texture sprite_texture;
	zest_texture imgui_font_texture;
	bool paused = false;
	Player player;
	tfx_vector_t<PlayerBullet> player_bullets[2];
	tfx_vector_t<Vader> vaders[2];
	tfx_vector_t<Vader> big_vaders[2];
	tfx_vector_t<VaderBullet> vader_bullets[2];
	tfx_vector_t<tfxEffectID> power_ups[2];
	int current_buffer = 0;
	float noise_offset = 0.f;

	zest_image player_image;
	zest_image vader_image1;
	zest_image vader_image2;
	zest_image vader_image3;
	zest_image big_vader_image;
	zest_image vader_bullet_image;
	zest_image vader_bullet_glow_image;

	float margin_x;
	float margin_y;
	float spacing_x;
	float spacing_y;

	int score;
	int high_score;
	float current_wave;
	float countdown_to_big_vader;

	tfxU32 index_offset[ZEST_MAX_FIF];

	tfx_vec3_t top_left_bound;
	tfx_vec3_t bottom_right_bound;

	tfx_library_t library;
	tfx_particle_manager_t game_pm;
	tfx_particle_manager_t background_pm;
	tfx_particle_manager_t title_pm;

	tfx_effect_template_t player_bullet_effect;
	tfx_effect_template_t vader_explosion_effect;
	tfx_effect_template_t player_explosion;
	tfx_effect_template_t big_explosion;
	tfx_effect_template_t background;
	tfx_effect_template_t title;
	tfx_effect_template_t laser;
	tfx_effect_template_t charge_up;
	tfx_effect_template_t weapon_power_up;
	tfx_effect_template_t got_power_up;
	tfx_effect_template_t damage;

	tfxEffectID background_index;
	tfxEffectID title_index;
	zest_font font;
	zest_imgui_layer_info_t imgui_layer_info;
	zest_layer font_layer;
	zest_layer billboard_layer;
	zest_uint particle_ds_index;
	zest_pipeline billboard_pipeline;
	zest_shader_resources sprite_resources;
	tfx_render_resources_t tfx_rendering;

	int particle_option = 2;

	GameState state = GameState_title;
	bool wait_for_mouse_release = false;

	void Init();
	void Update(float ellapsed);
};

void UpdateUniform3d(VadersGame *game) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(game->tfx_rendering.uniform_buffer_3d);
	buffer_3d->view = zest_LookAt(game->camera.position, zest_AddVec3(game->camera.position, game->camera.front), game->camera.up);
	buffer_3d->proj = zest_Perspective(game->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.1f, 10000.f);
	buffer_3d->proj.v[1].y *= -1.f;
	buffer_3d->screen_size.x = zest_ScreenWidthf();
	buffer_3d->screen_size.y = zest_ScreenHeightf();
	buffer_3d->millisecs = 0;
	buffer_3d->parameters1.x = (float)zest_TimerLerp(game->timer);
}

//Before you load an effects file, you will need to define a ShapeLoader function that passes the following parameters:
//const char* filename			- this will be the filename of the image being loaded from the library. You don't have to do anything with this if you don't need to.
//ImageData	&image_data			- A struct containing data about the image. You will have to set image_data.ptr to point to the texture in your renderer for later use in the Render function that you will create to render the particles
//void *raw_image_data			- The raw data of the image which you can use to load the image into graphics memory
//int image_memory_size			- The size in bytes of the raw_image_data
//void *custom_data				- This allows you to pass through an object you can use to access whatever is necessary to load the image into graphics memory, depending on the renderer that you're using
void ShapeLoader(const char* filename, tfx_image_data_t *image_data, void *raw_image_data, int image_memory_size, void *custom_data) {
	//Cast your custom data, this can be anything you want
	VadersGame *game = static_cast<VadersGame*>(custom_data);

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

//Function to project 2d screen coordinates into 3d space
tfx_vec3_t ScreenRay(float x, float y, float depth_offset, zest_vec3 &camera_position, zest_descriptor_buffer buffer) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

//Update the power up effect to follow the player
void UpdateGotPowerUpEffect(tfx_particle_manager_t *pm, tfxEffectID effect_index) {
	VadersGame *game = static_cast<VadersGame*>(GetEffectUserData(pm, effect_index));
	SetEffectPosition(pm, effect_index, game->player.position);
}

void InitTimelineFXRenderResources(tfx_render_resources_t &render_resources, const char *library_path) {
	render_resources.uniform_buffer_3d = zest_CreateUniformBuffer("3d uniform", sizeof(zest_uniform_buffer_data_t));
	render_resources.uniform_buffer_descriptor_set = zest_CreateUniformDescriptorSet(render_resources.uniform_buffer_3d);

	int shape_count = GetShapeCountInLibrary(library_path);
	render_resources.particle_texture = zest_CreateTexture("Particle Texture", zest_texture_storage_type_packed, zest_texture_flag_use_filtering, zest_texture_format_rgba, shape_count);
	render_resources.color_ramps_texture = zest_CreateTextureBank("Particle Color Ramps", zest_texture_format_rgba);
	zest_SetTextureUseFiltering(render_resources.color_ramps_texture, false);

	//Compile the shaders we will use to render the particles
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	render_resources.fragment_shader = zest_CreateShaderFromFile("examples/assets/shaders/timelinefx.frag", "tfx_frag.spv", shaderc_fragment_shader, true, compiler);
	render_resources.vertex_shader = zest_CreateShaderFromFile("examples/assets/shaders/timelinefx3d.vert", "tfx_vertex3d.spv", shaderc_vertex_shader, true, compiler);
	shaderc_compiler_release(compiler);

	//To render the particles we setup a pipeline with the vertex attributes and shaders to render the particles.
	//First create a descriptor set layout, we need 2 samplers, one to sample the particle texture and another to sample the color ramps
	//We also need 2 storage buffers, one to access the image data in the vertex shader and the other to access the previous frame particles
	//so that they can be interpolated in between updates
	render_resources.descriptor_layout = zest_AddDescriptorLayout("2 samplers 2 storage", zest_CreateDescriptorSetLayout(0, 2, 2));

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
	zest_AddPipelineTemplateDescriptorLayout(&instance_create_info, render_resources.descriptor_layout->vk_layout);
	render_resources.pipeline = zest_AddPipeline("tfx_billboard_pipeline");
	zest_MakePipelineTemplate(render_resources.pipeline, zest_GetStandardRenderPass(), &instance_create_info);
	render_resources.pipeline->pipeline_template.colorBlendAttachment = zest_PreMultiplyBlendState();
	render_resources.pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
	render_resources.pipeline->pipeline_template.depthStencil.depthTestEnable = true;
	zest_BuildPipeline(render_resources.pipeline);

	//Create a drawroutine specifically for the tfx_billboard_instance_t object
	//We'll create one for each particle manager so it's easier to set up interpolation in the shader
	render_resources.draw_routine = zest_CreateInstanceDrawRoutine("timelinefx game draw routine", sizeof(tfx_billboard_instance_t), 25000);

	//Set up the draw layers we need in the renderer
	zest_SetDrawCommandsClsColor(zest_GetCommandQueueDrawCommands("Default Draw Commands"), 0.f, 0.f, .2f, 1.f);

	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			render_resources.layer = zest_AddInstanceDrawRoutine(render_resources.draw_routine);
		}
		zest_FinishQueueSetup();
	}

	//We want to be able to manually change the current frame in flight in the layer that we use to draw all the billboards.
	//This means that we are able to only change the current frame in flight if we actually updated the particle manager in the current
	//frame allowing us to dictate when to upload the instance buffer to the gpu as there's no need to do it every frame, only when 
	//the particle manager is actually updated.
	zest_SetLayerToManualFIF(render_resources.layer);

	//Create a buffer to store the image data on the gpu. Note that we don't need this buffer to have multiple frames in flight
	render_resources.image_data = zest_CreateStorageDescriptorBuffer(sizeof(tfx_gpu_image_data_t) * 1000, false);

	//End of render specific code
}

void UpdateTimelineFXImageData(tfx_render_resources_t &tfx_rendering, tfx_library_t &library) {
	//Upload the timelinefx image data to the image data buffer created
	zest_buffer image_data_buffer = zest_GetBufferFromDescriptorBuffer(tfx_rendering.image_data);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(GetGPUShapesSizeInBytes(&library.gpu_shapes), GetGPUShapesPointer(&library.gpu_shapes));
	zest_CopyBuffer(staging_buffer, zest_GetBufferFromDescriptorBuffer(tfx_rendering.image_data), GetGPUShapesSizeInBytes(&library.gpu_shapes));
	zest_FreeBuffer(staging_buffer);
}

void CreateTimelineFXShaderResources(tfx_render_resources_t &tfx_rendering) {
	//We need a descriptor set for the shader resources that we will use in the pipeline we created above
	zest_descriptor_set_builder_t set_builder = zest_NewDescriptorSetBuilder();
	zest_AddBuilderDescriptorWriteStorageBuffer(&set_builder, tfx_rendering.image_data, 0);
	zest_AddBuilderDescriptorWriteInstanceLayerLerp(&set_builder, tfx_rendering.layer, 1);
	zest_AddBuilderDescriptorWriteImage(&set_builder, zest_GetTextureDescriptorImageInfo(tfx_rendering.particle_texture), 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	zest_AddBuilderDescriptorWriteImage(&set_builder, zest_GetTextureDescriptorImageInfo(tfx_rendering.color_ramps_texture), 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	tfx_rendering.descriptor_set = zest_BuildDescriptorSet(&set_builder, tfx_rendering.descriptor_layout, zest_descriptor_type_static);

	//Finally, set up a shader resource to be used when sending the draw calls to the gpu in our render function
	//This will have the uniform buffer in set 0 and the texures and storage buffers in set 1
	tfx_rendering.shader_resource = zest_CreateShaderResources();
	zest_AddDescriptorSetToResources(tfx_rendering.shader_resource, tfx_rendering.uniform_buffer_descriptor_set);
	zest_AddDescriptorSetToResources(tfx_rendering.shader_resource, &tfx_rendering.descriptor_set);
}

//Initialise the game and set up the renderer
void VadersGame::Init() {
	InitTimelineFXRenderResources(tfx_rendering, "examples/assets/vaders/vadereffects.tfx");
	//Renderer specific - initialise the texture
	float max_radius = 0;
	//Create a texture to store our player and enemy sprite images
	sprite_texture = zest_CreateTexturePacked("Sprite Texture", zest_texture_format_rgba);
	//Add all of the images and animations into the texture
	player_image = zest_AddTextureImageFile(sprite_texture, "examples/assets/vaders/player.png");
	vader_image1 = zest_AddTextureImageFile(sprite_texture, "examples/assets/vaders/vader.png");
	vader_image2 = zest_AddTextureImageFile(sprite_texture, "examples/assets/vaders/vader2.png");
	vader_image3 = zest_AddTextureImageFile(sprite_texture, "examples/assets/vaders/vader3.png");
	big_vader_image = zest_AddTextureImageFile(sprite_texture, "examples/assets/vaders/big_vader.png");
	vader_bullet_image = zest_AddTextureAnimationFile(sprite_texture, "examples/assets/vaders/vader_bullet_64_64_16.png", 16, 16, 64, &max_radius, 1);
	vader_bullet_glow_image = zest_AddTextureImageFile(sprite_texture, "examples/assets/vaders/flare4.png");
	//Now the images are added, process the texture to make them available in the GPU
	zest_ProcessTextureImages(sprite_texture);
	//Create a descriptor set for the texture that uses the 3d uniform buffer
	particle_ds_index = 0;

	sprite_resources = zest_CombineUniformAndTextureSampler(tfx_rendering.uniform_buffer_descriptor_set, sprite_texture);

	//Store the handle of the billboard pipeline so we don't have to look it up each frame. This is for drawing
	//the player and invader ships
	billboard_pipeline = zest_Pipeline("pipeline_billboard");

	RandomReSeed(&random);

	//Load the effects library and pass the shape loader function pointer that you created earlier. Also pass this pointer to point to this object to give the shapeloader access to the texture we're loading the particle images into
	LoadEffectLibrary("examples/assets/vaders/vadereffects.tfx", &library, ShapeLoader, GetUV, this);

	//Renderer specific
	//Process the texture with all the particle shapes that we just added to
	zest_ProcessTextureImages(tfx_rendering.particle_texture);

	//Add the color ramps from the library to the color ramps texture. Color ramps in the library are stored in rgba format and can be
	//simply copied to a bitmap for uploading to the texture
	for (tfx_bitmap_t &bitmap : library.color_ramp_bitmaps) {
		zest_bitmap_t temp_bitmap = zest_CreateBitmapFromRawBuffer("", bitmap.data, (int)bitmap.size, bitmap.width, bitmap.height, bitmap.channels);
		zest_AddTextureImageBitmap(tfx_rendering.color_ramps_texture, &temp_bitmap);
	}
	//Process the color ramp texture to upload it all to the gpu
	zest_ProcessTextureImages(tfx_rendering.color_ramps_texture);
	//Now that the particle shapes have been setup in the renderer, we can call this function to update the shape data in the library
	//with the correct uv texture coords ready to upload to gpu. This buffer will be accessed in the vertex shader when rendering.
	UpdateLibraryGPUImageData(&library);

	//Now upload the image data to the GPU and set up the shader resources ready for rendering
	UpdateTimelineFXImageData(tfx_rendering, library);
	CreateTimelineFXShaderResources(tfx_rendering);
	zest_SetTextureUserData(tfx_rendering.particle_texture, this);

	//Specific, set up a timer for the update loop
	timer = zest_CreateTimer(60);

	//Create a camera for the 3d uniform buffer
	camera = zest_CreateCamera();
	zest_CameraSetFoV(&camera, 60.f);

	//Update the uniform buffer so that the projection and view matrices are set for the screen ray functions below
	UpdateUniform3d(this);

	//Get the top left and bottom right screen positions in 3d space so we know when things leave the screen
	top_left_bound = ScreenRay(0.f, 0.f, 10.f, camera.position, tfx_rendering.uniform_buffer_3d);
	bottom_right_bound = ScreenRay(zest_ScreenWidthf(), zest_ScreenHeightf(), 10.f, camera.position, tfx_rendering.uniform_buffer_3d);

	tfxU32 layer_max_values[tfxLAYERS];
	memset(layer_max_values, 0, 16);
	layer_max_values[0] = 5000;
	layer_max_values[1] = 2500;
	layer_max_values[2] = 2500;
	layer_max_values[3] = 2500;
	//Initialise a particle manager. This manages effects, emitters and the particles that they spawn
	//Depending on your needs you can use as many particle managers as you need.
	//pm.InitFor3d(layer_max_values, 100, tfx_particle_manager_tMode_ordered_by_depth_guaranteed, 512);
	tfx_particle_manager_info_t background_pm_info = CreateParticleManagerInfo(tfxParticleManagerSetup_3d_ordered_by_depth);
	InitializeParticleManager(&background_pm, &library, background_pm_info);
	tfx_particle_manager_info_t game_pm_info = CreateParticleManagerInfo(tfxParticleManagerSetup_3d_group_sprites_by_effect);
	InitializeParticleManager(&game_pm, &library, game_pm_info);
	game_pm_info.max_effects = 10;
	InitializeParticleManager(&title_pm, &library, game_pm_info);

	//Load a font we can draw text with
	font = zest_LoadMSDFFont("examples/assets/RussoOne-Regular.zft");

	//Set the clear color for the default draw commands render pass
	zest_SetDrawCommandsClsColor(zest_GetCommandQueueDrawCommands("Default Draw Commands"), 0.f, 0.f, .2f, 1.f);

	//Player setup
	player.rate_of_fire = 4.f * UpdateFrequency;
	high_score = 0;

	//Prepare all the Effect templates we need from the library
	PrepareEffectTemplate(&library, "Player Bullet", &player_bullet_effect);
	PrepareEffectTemplate(&library, "Vader Explosion", &vader_explosion_effect);
	PrepareEffectTemplate(&library, "Big Explosion", &big_explosion);
	PrepareEffectTemplate(&library, "Player Explosion", &player_explosion);
	PrepareEffectTemplate(&library, "Background", &background);
	PrepareEffectTemplate(&library, "Title", &title);
	PrepareEffectTemplate(&library, "Laser", &laser);
	PrepareEffectTemplate(&library, "Charge Up", &charge_up);
	PrepareEffectTemplate(&library, "Got Power Up", &got_power_up);
	PrepareEffectTemplate(&library, "Power Up", &weapon_power_up);
	PrepareEffectTemplate(&library, "Damage", &damage);

	//Set the user data in the got power up effect and the update callback so that we can position it each frame
	SetTemplateEffectUserData(&got_power_up, this);
	SetTemplateEffectUpdateCallback(&got_power_up, UpdateGotPowerUpEffect);

	//Add the background effect nad title effect to the particle manager and set their positions
	if (AddEffectToParticleManager(&background_pm, &background, &background_index)) {
		zest_vec3 position = zest_AddVec3(zest_ScaleVec3(camera.front, 12.f), camera.position);
		SetEffectPosition(&background_pm, background_index, { position.x, position.y, position.z });
	}
	if (AddEffectToParticleManager(&title_pm, &title, &title_index)) {
		SetEffectPosition(&title_pm, title_index, ScreenRay(zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .25f, 4.f, camera.position, tfx_rendering.uniform_buffer_3d));
	}

	//Initialise imgui
	zest_imgui_Initialise(&imgui_layer_info);

	//Set up the font in imgui
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.Fonts->Clear();
	float font_size = 15.f;
	unsigned char *font_data;
	int tex_width, tex_height;
	ImFontConfig config;
	config.PixelSnapH = true;
	io.Fonts->AddFontFromFileTTF("examples/assets/Lato-Regular.ttf", font_size);
	io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

	zest_imgui_RebuildFontTexture(&imgui_layer_info, tex_width, tex_height, font_data);

	//Modify the default command queue so we can add some extra layers
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			//Add a billboard layer to draw all the sprites and particles
			billboard_layer = zest_NewBuiltinLayerSetup("Billboards", zest_builtin_layer_billboards);
			//Add a layer for drawing fonts
			font_layer = zest_NewBuiltinLayerSetup("Fonts", zest_builtin_layer_fonts);
			//Add the imgui layer
			zest_imgui_CreateLayer(&imgui_layer_info);
		}
		//Finish the queue set up
		zest_FinishQueueSetup();
	}
	//Reset the time
	zest_TimerReset(timer);
	//Output the memory usage to the console
	zest_OutputMemoryUsage();
	TFX_ASSERT(!(background_pm.flags & tfxParticleManagerFlags_use_effect_sprite_buffers));

	for (ZEST_EACH_FIF_i) {
		index_offset[i] = 0;
	}
}

//Some helper functions
float EaseOutQuad(float t) {
	return t * (2 - t);
}

float EaseInOutQuad(float t) {
	return t < 0.5f ? 2 * t * t : t * (4 - 2 * t) - 1;
}

tfx_vec3_t RotatePoint(tfx_vec3_t p, tfx_vec3_t center, float angle) {
	float radians = DegreesToRadians(angle);  // convert angle to radians
	float cosa = cosf(radians);
	float sina = sinf(radians);
	float newz = center.z + (p.z - center.z) * cosa - (p.y - center.y) * sina;
	float newy = center.y + (p.z - center.z) * sina + (p.y - center.y) * cosa;
	return { center.x, newz, newy };
}

bool IsLineCircleCollision(tfx_vec3_t line_start, tfx_vec3_t line_end, tfx_vec3_t circleCenter, float radius) {
	float dx = line_end.z - line_start.z;
	float dy = line_end.y - line_start.y;
	float a = dx * dx + dy * dy;
	float b = 2.f * (dx * (line_start.z - circleCenter.z) + dy * (line_start.y - circleCenter.y));
	float c = circleCenter.z * circleCenter.z + circleCenter.y * circleCenter.y + line_start.z * line_start.z + line_start.y * line_start.y - 2.f * (circleCenter.z * line_start.z + circleCenter.y * line_start.y) - radius * radius;
	float discriminant = b * b - 4.f * a * c;
	if (discriminant < 0) {
		// no intersection
		return false;
	}
	else {
		// compute the two possible intersection points
		float t1 = (-b + sqrtf(discriminant)) / (2.f * a);
		float t2 = (-b - sqrtf(discriminant)) / (2.f * a);
		if ((t1 >= 0 && t1 <= 1) || (t2 >= 0 && t2 <= 1)) {
			// intersection
			return true;
		}
		else {
			// no intersection
			return false;
		}
	}
}

//Spawn a new invader wave
void SpawnInvaderWave(VadersGame *game) {
	float cols = 11;
	float rows = 5;

	float width = zest_ScreenWidthf();
	float height = zest_ScreenHeightf();
	game->margin_x = width * x_distance;
	game->margin_y = height * y_distance;
	game->spacing_x = width * x_spacing;
	game->spacing_y = height * y_spacing;

	for (float x = 0; x < cols; ++x) {
		for (float y = 0; y < rows; ++y) {
			Vader vader;
			vader.position = vader.captured = ScreenRay(game->spacing_x * x + game->margin_x, game->spacing_y * y + game->margin_y, 10.f, game->camera.position, game->tfx_rendering.uniform_buffer_3d);
			if (y == 0) {
				vader.image = game->vader_image1;
			}
			else if (y < 3) {
				vader.image = game->vader_image2;
			}
			else {
				vader.image = game->vader_image3;
			}
			vader.velocity.z = 1.f;
			vader.direction = Rad90;
			float wave = game->current_wave - 1;
			vader.speed = 1.f + (wave * .1f);
			vader.chance_to_shoot = 0.001f + (wave * 0.001f);
			game->vaders[game->current_buffer].push_back(vader);
		}
	}
}

void SpawnBigVader(VadersGame *game) {
	float width = zest_ScreenWidthf();
	float height = zest_ScreenHeightf();
	game->margin_x = width * x_distance;
	game->margin_y = height * y_distance;

	Vader vader;
	vader.end_position = ScreenRay(zest_ScreenWidthf() * .5f, game->margin_y * .45f, 10.f, game->camera.position, game->tfx_rendering.uniform_buffer_3d);
	vader.position = vader.captured = vader.start_position = ScreenRay(zest_ScreenWidthf() * .5f, -game->margin_y, 10.f, game->camera.position, game->tfx_rendering.uniform_buffer_3d);;
	vader.image = game->big_vader_image;
	float wave = game->current_wave - 1;
	vader.health = 5 + (int)(wave * 1);
	vader.start_angle = 135.f;
	vader.end_angle = 225.f;
	vader.angle = 135.f;
	game->big_vaders[game->current_buffer].push_back(vader);
}

void UpdateVaders(VadersGame *game) {
	int next_buffer = !game->current_buffer;
	game->vaders[next_buffer].clear();
	game->big_vaders[next_buffer].clear();

	//Update the grid of smaller invaders
	for (auto &vader : game->vaders[game->current_buffer]) {
		//Naive bullet collision
		bool hit = false;
		for (auto &bullet : game->player_bullets[game->current_buffer]) {
			if (GetDistance(bullet.position.z, bullet.position.y, vader.position.z, vader.position.y) < 0.3f) {
				hit = true;
				bullet.remove = true;
				//Blow up the vader. Add teh vader_explosion_effect template to the particle manager
				tfxEffectID effect_index;
				if (AddEffectToParticleManager(&game->game_pm, &game->vader_explosion_effect, &effect_index)) {
					//Set the effect position
					SetEffectPosition(&game->game_pm, effect_index, vader.position);
					//Alter the effect scale
					SetEffectOveralScale(&game->game_pm, effect_index, 2.5f);
				}
				game->score += 150;
				game->high_score = tfxMax(game->score, game->high_score);
				break;
			}
		}
		if (hit) continue;
		if (vader.position.y < game->bottom_right_bound.y) continue;
		float wave = game->current_wave - 1.f;
		if (vader.turning == 1) {
			vader.direction += DegreesToRadians(180.f + (wave * 10.f)) * UpdateFrequency;
			if (vader.direction >= Rad270) {
				vader.direction = Rad270;
				vader.turning = 0;
			}
		}
		else if (vader.turning == 2) {
			vader.direction -= DegreesToRadians(180.f + (wave * 10.f)) * UpdateFrequency;
			if (vader.direction <= Rad90) {
				vader.direction = Rad90;
				vader.turning = 0;
			}
		}
		vader.velocity.z = vader.speed * sinf(vader.direction);
		vader.velocity.y = vader.speed * cosf(vader.direction);
		vader.captured = vader.position;
		vader.position += vader.velocity * UpdateFrequency;
		if (vader.position.z < -8.f && vader.direction == Rad270) {
			vader.turning = 2;
		}
		else if (vader.position.z > 8.f && vader.direction == Rad90) {
			vader.turning = 1;
		}
		if (game->state != GameState_game_over && RandomRange(&game->random, 1.f) <= vader.chance_to_shoot) {
			VaderBullet bullet;
			bullet.position = bullet.captured = vader.position;
			bullet.velocity.y = (-4.f + (wave * 0.1f)) * UpdateFrequency;
			bullet.frame = 0;
			game->vader_bullets[game->current_buffer].push_back(bullet);
		}
		//push the vader to the next buffer
		game->vaders[next_buffer].push_back(vader);
	}

	//Udpate the bigger vader that floats down from the top
	for (auto &vader : game->big_vaders[game->current_buffer]) {
		bool dead = false;
		for (auto &bullet : game->player_bullets[game->current_buffer]) {
			if (GetDistance(bullet.position.z, bullet.position.y, vader.position.z, vader.position.y) < 0.3f) {
				bullet.remove = true;
				vader.health--;
				//Add the damage taken effect to the particle manager and set it's position
				tfxEffectID damage_index;
				if (AddEffectToParticleManager(&game->game_pm, &game->damage, &damage_index)) {
					SetEffectPosition(&game->game_pm, damage_index, bullet.position);
				}
				if (vader.health == 0) {
					dead = true;
					//blow up the big vader, add the big_explosion template to the particle manager and set position and scale
					tfxEffectID effect_index;
					if (AddEffectToParticleManager(&game->game_pm, &game->big_explosion, &effect_index)) {
						SetEffectPosition(&game->game_pm, effect_index, vader.position);
						SetEffectOveralScale(&game->game_pm, effect_index, 2.5f);
					}
					game->score += 500;
					game->high_score = tfxMax(game->score, game->high_score);
					if (vader.flags & VaderFlags_firing_laser) {
						SoftExpireEffect(&game->game_pm, vader.laser);
					}
					//Add the power up effect that floats downward to the particle manager and set its position and scale
					tfxEffectID power_up_index;
					if (AddEffectToParticleManager(&game->game_pm, &game->weapon_power_up, &power_up_index)) {
						SetEffectPosition(&game->game_pm, power_up_index, vader.position);
						SetEffectOveralScale(&game->game_pm, power_up_index, 3.f);
						game->power_ups[game->current_buffer].push_back(power_up_index);
					}
					break;
				}
			}
		}
		if (dead) continue;
		vader.time += UpdateFrequency;
		vader.captured = vader.position;
		if (!(vader.flags & VaderFlags_in_position)) {
			vader.position.y = EaseOutQuad(tfxMin(vader.time * .5f, 1.f)) * (vader.end_position.y - vader.start_position.y) + vader.start_position.y;
		}
		if (vader.time > 2.f && !(vader.flags & VaderFlags_charging_up) && !(vader.flags & VaderFlags_firing_laser)) {
			//The big vader has started to power up it's laser, add the charge up effect to the particle manager
			//Position at the tip of the vader
			tfxEffectID effect_index;
			if (AddEffectToParticleManager(&game->game_pm, &game->charge_up, &effect_index)) {
				tfx_vec3_t laser_offset = RotatePoint({ 0.f, .4f, 0.f }, { 0.f, 0.f, 0.f }, vader.angle - 90.f);
				SetEffectPosition(&game->game_pm, effect_index, vader.position + laser_offset);
			}
			vader.flags |= VaderFlags_charging_up;
			vader.flags |= VaderFlags_in_position;
		}
		else if (vader.time > 3.f && vader.flags & VaderFlags_charging_up) {
			//Big vader has finished charging up so shoot the laser
			//Add the laser effect to the particle manager and set the position
			AddEffectToParticleManager(&game->game_pm, &game->laser, &vader.laser);
			tfx_vec3_t laser_offset = RotatePoint({ 0.f, .4f, 0.f }, { 0.f, 0.f, 0.f }, vader.angle - 90.f);
			SetEffectPosition(&game->game_pm, vader.laser, vader.position + laser_offset);
			SetEffectPitch(&game->game_pm, vader.laser, DegreesToRadians(vader.angle));
			SetEffectOveralScale(&game->game_pm, vader.laser, 2.f);
			//Set the height of the laser which will change the length of it. We want it to extend off of the screen
			SetEffectHeightMultiplier(&game->game_pm, vader.laser, 3.f);
			vader.flags |= VaderFlags_firing_laser;
			vader.flags &= ~VaderFlags_charging_up;
		}
		else if (vader.flags & VaderFlags_firing_laser) {
			//While the laser is firing, rotate the laser effect and update it's position to keep it at the tip of the vader
			float time = (vader.time - 3.f) * .25f;
			vader.angle = EaseInOutQuad(tfxMin(time * .5f, 1.f)) * (vader.end_angle - vader.start_angle) + vader.start_angle;
			tfx_vec3_t laser_offset = RotatePoint({ 0.f, .4f, 0.f }, { 0.f, 0.f, 0.f }, vader.angle - 90.f);
			SetEffectPosition(&game->game_pm, vader.laser, vader.position + laser_offset);
			//Rotate the laser
			SetEffectPitch(&game->game_pm, vader.laser, DegreesToRadians(vader.angle));
			if (vader.angle == vader.end_angle) {
				vader.time = 0;
				vader.end_angle = vader.start_angle;
				vader.start_angle = vader.angle;
				//Now that we've reached the final angle of the laser, soft expire the effect so that the effect stops spawning particles 
				//and any remaining particles expire naturally
				SoftExpireEffect(&game->game_pm, vader.laser);
				vader.flags &= ~VaderFlags_firing_laser;
			}
			else {
				tfx_vec3_t laser_normal = NormalizeVec3(&laser_offset);
				//Check to see if the laser is colliding with the player
				if (game->state != GameState_game_over && IsLineCircleCollision(vader.position + laser_offset, vader.position + laser_offset + (laser_normal * 20.f), game->player.position, .3f)) {
					//Destroy the player. Add the player explosion to the particle manager and position/scale it.
					tfxEffectID effect_index;
					if (AddEffectToParticleManager(&game->game_pm, &game->player_explosion, &effect_index)) {
						SetEffectPosition(&game->game_pm, effect_index, game->player.position);
						SetEffectOveralScale(&game->game_pm, effect_index, 1.5f);
					}
					game->state = GameState_game_over;
				}
			}
		}
		//push the big vader to the next buffer
		game->big_vaders[next_buffer].push_back(vader);
	}
	if (game->big_vaders[next_buffer].size()) {
		game->countdown_to_big_vader = RandomRange(&game->random, 15.f, 35.f);
	}
}

void UpdatePowerUps(VadersGame *game) {
	int next_buffer = !game->current_buffer;
	game->power_ups[next_buffer].clear();
	for (auto &power_up : game->power_ups[game->current_buffer]) {
		tfx_vec3_t position = GetEffectPosition(&game->game_pm, power_up);
		if (GetDistance(position.z, position.y, game->player.position.z, game->player.position.y) < 0.3f) {
			HardExpireEffect(&game->game_pm, power_up);
			tfxEffectID effect_index;
			if (AddEffectToParticleManager(&game->game_pm, &game->got_power_up, &effect_index)) {
				SetEffectPosition(&game->game_pm, effect_index, game->player.position);
				SetEffectOveralScale(&game->game_pm, effect_index, 2.5f);
			}
			game->player.rate_of_fire += 1 * UpdateFrequency;
			continue;
		}
		MoveEffect(&game->game_pm, power_up, 0.f, -1.f * UpdateFrequency, 0.f);
		game->power_ups[next_buffer].push_back(power_up);
	}
}

void UpdatePlayerPosition(VadersGame *game, Player *player) {
	player->position = ScreenRay((float)ZestApp->mouse_x, (float)ZestApp->mouse_y, 10.f, game->camera.position, game->tfx_rendering.uniform_buffer_3d);
}

void UpdatePlayer(VadersGame *game, Player *player) {
	if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		player->fire_count += player->rate_of_fire;
		if (player->fire_count >= 1.f) {
			player->fire_count = 0;
			PlayerBullet new_bullet;
			if(AddEffectToParticleManager(&game->game_pm, &game->player_bullet_effect, &new_bullet.effect_index)){
				new_bullet.position = player->position;
				SetEffectPosition(&game->game_pm, new_bullet.effect_index, new_bullet.position);
				SetEffectBaseNoiseOffset(&game->game_pm, new_bullet.effect_index, game->noise_offset);
				game->player_bullets[game->current_buffer].push_back(new_bullet);
			}
		}
	}
	else {
		player->fire_count = 1.f;
	}
}

void UpdatePlayerBullets(VadersGame *game) {
	int next_buffer = !game->current_buffer;
	game->player_bullets[next_buffer].clear();
	tfx_vec3_t top_left = ScreenRay(0.f, 0.f, 10.f, game->camera.position, game->tfx_rendering.uniform_buffer_3d);
	for (auto &bullet : game->player_bullets[game->current_buffer]) {
		if (bullet.remove) {
			SoftExpireEffect(&game->game_pm, bullet.effect_index);
			continue;
		}
		bullet.position.y += bullet.speed * UpdateFrequency;
		if (bullet.position.y > top_left.y + 1.f) {
			//Expire the effect if it hits the top of the screen
			SoftExpireEffect(&game->game_pm, bullet.effect_index);
		}
		else {
			//Update the bullet effect position
			SetEffectPosition(&game->game_pm, bullet.effect_index, bullet.position);
			game->player_bullets[next_buffer].push_back(bullet);
		}
	}
}

void UpdateVaderBullets(VadersGame *game) {
	int next_buffer = !game->current_buffer;
	game->vader_bullets[next_buffer].clear();
	for (auto &bullet : game->vader_bullets[game->current_buffer]) {
		//Does the vader bullet collide with the player?
		if (game->state != GameState_game_over && GetDistance(bullet.position.z, bullet.position.y, game->player.position.z, game->player.position.y) < 0.3f) {
			//Blow up the player, add the player_explosion effect to the particle manager and set it's position/scale
			tfxEffectID effect_index = 0;
			if (AddEffectToParticleManager(&game->game_pm, &game->player_explosion, &effect_index)) {
				SetEffectPosition(&game->game_pm, effect_index, bullet.position);
				SetEffectOveralScale(&game->game_pm, effect_index, 1.5f);
			}
			game->state = GameState_game_over;
			continue;
		}
		if (bullet.remove) {
			continue;
		}
		bullet.captured = bullet.position;
		bullet.position += bullet.velocity;
		bullet.frame += 60.f * UpdateFrequency;
		if (bullet.position.y < game->top_left_bound.y + 1.f &&
			bullet.position.z > game->top_left_bound.z - 1.f &&
			bullet.position.y > game->bottom_right_bound.y - 1.f &&
			bullet.position.z < game->bottom_right_bound.z + 1.f) {
			//Push the bullet to the next buffer
			game->vader_bullets[next_buffer].push_back(bullet);
		}
	}
}

void SetParticleOption(VadersGame *game) {
	//Here we can alter the particle effects so that on slower PCs the particles can be toned down
	if (game->particle_option == 0) {
		ScaleTemplateGlobalMultiplier(&game->background, tfxGlobal_amount, .25f);
		//Restart the title/background effects
		SoftExpireEffect(&game->background_pm, game->background_index);
		HardExpireEffect(&game->title_pm, game->title_index);
		DisableTemplateEmitter(&game->player_bullet_effect, "Player Bullet/Flare");
		//Disable the flare emitter in the title effect
		DisableTemplateEmitter(&game->title, "Title/Flare");
		//Set the single spawn amount of the emitters with the big explosion effect
		SetTemplateSingleSpawnAmount(&game->big_explosion, "Big Explosion/Ring Stretch", 100);
		SetTemplateSingleSpawnAmount(&game->big_explosion, "Big Explosion/Ring Stretch.1", 100);
		//Scale the base amount of the cloud burst expand emitter
		ScaleTemplateEmitterGraph(&game->big_explosion, "Big Explosion/Cloud Burst Expand", tfxBase_amount, 0.25f);
		//Tone down the player explosion as well
		SetTemplateSingleSpawnAmount(&game->player_explosion, "Player Explosion/Ring Stretch", 100);
		SetTemplateSingleSpawnAmount(&game->player_explosion, "Player Explosion/Ring Stretch.1", 100);
		ScaleTemplateEmitterGraph(&game->player_explosion, "Player Explosion/Cloud Burst", tfxBase_amount, 0.25f);
		SetTemplateSingleSpawnAmount(&game->got_power_up, "Got Power Up/Delayed Ellipse", 100);
		ScaleTemplateGlobalMultiplier(&game->vader_explosion_effect, tfxGlobal_amount, .25f);
		//For the laser scale the global amount to adjust the amount of all emitters within the effect
		ScaleTemplateGlobalMultiplier(&game->laser, tfxGlobal_amount, .5f);
		DisableTemplateEmitter(&game->laser, "Laser/Flare");

		if (AddEffectToParticleManager(&game->background_pm, &game->background, &game->background_index)) {
			zest_vec3 position = zest_AddVec3(zest_ScaleVec3(game->camera.front, 12.f), game->camera.position);
			SetEffectPosition(&game->background_pm, game->background_index, { position.x, position.y, position.z });
		}
		if (AddEffectToParticleManager(&game->title_pm, &game->title, &game->title_index)) {
			SetEffectPosition(&game->title_pm, game->title_index, ScreenRay(zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .25f, 4.f, game->camera.position, game->tfx_rendering.uniform_buffer_3d));
		}
	}
	else if (game->particle_option == 1) {
	ScaleTemplateGlobalMultiplier(&game->background, tfxGlobal_amount, .65f);
		SoftExpireEffect(&game->background_pm, game->background_index);
		HardExpireEffect(&game->title_pm, game->title_index);
		EnableTemplateEmitter(&game->player_bullet_effect, "Player Bullet/Flare");
		ScaleTemplateEmitterGraph(&game->player_bullet_effect, "Player Bullet/Flare", tfxBase_amount, 0.5f);
		DisableTemplateEmitter(&game->title, "Title/Flare");
		SetTemplateSingleSpawnAmount(&game->big_explosion, "Big Explosion/Ring Stretch", 200);
		SetTemplateSingleSpawnAmount(&game->big_explosion, "Big Explosion/Ring Stretch.1", 250);
		ScaleTemplateEmitterGraph(&game->big_explosion, "Big Explosion/Cloud Burst Expand", tfxBase_amount, 0.65f);
		SetTemplateSingleSpawnAmount(&game->player_explosion, "Player Explosion/Ring Stretch", 200);
		SetTemplateSingleSpawnAmount(&game->player_explosion, "Player Explosion/Ring Stretch.1", 250);
		ScaleTemplateEmitterGraph(&game->player_explosion, "Player Explosion/Cloud Burst", tfxBase_amount, 0.65f);
		ScaleTemplateGlobalMultiplier(&game->vader_explosion_effect, tfxGlobal_amount, .65f);
		SetTemplateSingleSpawnAmount(&game->got_power_up, "Got Power Up/Delayed Ellipse", 250);
		ScaleTemplateGlobalMultiplier(&game->laser, tfxGlobal_amount, 1.f);
		DisableTemplateEmitter(&game->laser, "Laser/Flare");

		if (AddEffectToParticleManager(&game->background_pm, &game->background, &game->background_index)) {
			zest_vec3 position = zest_AddVec3(zest_ScaleVec3(game->camera.front, 12.f), game->camera.position);
			SetEffectPosition(&game->background_pm, game->background_index, { position.x, position.y, position.z });
		}
		if (AddEffectToParticleManager(&game->title_pm, &game->title, &game->title_index)) {
			SetEffectPosition(&game->title_pm, game->title_index, ScreenRay(zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .25f, 4.f, game->camera.position, game->tfx_rendering.uniform_buffer_3d));
		}
	}
	else if (game->particle_option == 2) {
	ScaleTemplateGlobalMultiplier(&game->background, tfxGlobal_amount, 1.f);
		SoftExpireEffect(&game->background_pm, game->background_index);
		HardExpireEffect(&game->title_pm, game->title_index);
		EnableTemplateEmitter(&game->player_bullet_effect, "Player Bullet/Flare");
		ScaleTemplateEmitterGraph(&game->player_bullet_effect, "Player Bullet/Flare", tfxBase_amount, 1.f);
		EnableTemplateEmitter(&game->title, "Title/Flare");
		SetTemplateSingleSpawnAmount(&game->big_explosion, "Big Explosion/Ring Stretch", 400);
		SetTemplateSingleSpawnAmount(&game->big_explosion, "Big Explosion/Ring Stretch.1", 500);
		ScaleTemplateEmitterGraph(&game->big_explosion, "Big Explosion/Cloud Burst Expand", tfxBase_amount, 1.f);
		SetTemplateSingleSpawnAmount(&game->player_explosion, "Player Explosion/Ring Stretch", 400);
		SetTemplateSingleSpawnAmount(&game->player_explosion, "Player Explosion/Ring Stretch.1", 500);
		ScaleTemplateEmitterGraph(&game->player_explosion, "Player Explosion/Cloud Burst", tfxBase_amount, 1.f);
		ScaleTemplateGlobalMultiplier(&game->vader_explosion_effect, tfxGlobal_amount, 1.f);
		SetTemplateSingleSpawnAmount(&game->got_power_up, "Got Power Up/Delayed Ellipse", 566);
		ScaleTemplateGlobalMultiplier(&game->laser, tfxGlobal_amount, 1.f);
		EnableTemplateEmitter(&game->laser, "Laser/Flare");

		if (AddEffectToParticleManager(&game->background_pm, &game->background, &game->background_index)) {
			zest_vec3 position = zest_AddVec3(zest_ScaleVec3(game->camera.front, 12.f), game->camera.position);
			SetEffectPosition(&game->background_pm, game->background_index, { position.x, position.y, position.z });
		}
		if (AddEffectToParticleManager(&game->title_pm, &game->title, &game->title_index)) {
			SetEffectPosition(&game->title_pm, game->title_index, ScreenRay(zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .25f, 4.f, game->camera.position, game->tfx_rendering.uniform_buffer_3d));
		}
	}
}

void BuildUI(VadersGame *game) {
	//Draw the imgui window
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Effects");
	ImGui::Text("FPS: %i", ZestApp->last_fps);
	ImGui::Text("Game Particles: %i", ParticleCount(&game->game_pm));
	ImGui::Text("Background Particles: %i", ParticleCount(&game->background_pm));
	ImGui::Text("Title Particles: %i", ParticleCount(&game->title_pm));
	ImGui::Text("Effects: %i", GetPMEffectBuffer(&game->game_pm, 0)->size());
	ImGui::Text("Emitters: %i", GetPMEmitterBuffer(&game->game_pm, 0)->size());
	ImGui::Text("Free Emitters: %i", game->game_pm.free_emitters.size());
	ImGui::Text("Position: %f, %f, %f", game->player.position.x, game->player.position.y, game->player.position.z);
	static bool filtering = false;
	static bool sync_fps = false;
	ImGui::Checkbox("Texture Filtering", &filtering);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		zest_SetTextureUseFiltering(game->tfx_rendering.particle_texture, filtering);
		zest_ProcessTextureImages(game->tfx_rendering.particle_texture);
	}
	ImGui::Checkbox("Sync FPS", &sync_fps);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		if (sync_fps) {
			zest_EnableVSync();
		}
		else {
			zest_DisableVSync();
		}
	}
	static const char *options[3] = { "Low", "Medium", "High" };
	if (ImGui::BeginCombo("Particles", options[game->particle_option])) {
		for (int i = 0; i != 3; ++i)
		{
			bool is_selected = (game->particle_option == i);
			if (ImGui::Selectable(options[i], is_selected))
				if (game->particle_option != i) {
					game->particle_option = i;
					SetParticleOption(game);

				}
			if (is_selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::Separator();
	ImGui::End();

	ImGui::Render();
	//This will let the layer know that the mesh buffer containing all of the imgui vertex data needs to be
	//uploaded to the GPU.
	zest_ResetLayer(game->imgui_layer_info.mesh_layer);
}

//Draw all the billboards for the game
void DrawPlayer(VadersGame *game) {
	zest_SetLayerColor(game->billboard_layer, 255, 255, 255, 255);
	zest_SetLayerIntensity(game->billboard_layer, 1.f);
	zest_DrawBillboardSimple(game->billboard_layer, game->player_image, &game->player.position.x, 0.f, 1.f, 1.f);
}

void DrawVaders(VadersGame *game, float lerp) {
	zest_SetLayerColor(game->billboard_layer, 255, 255, 255, 255);
	zest_SetLayerIntensity(game->billboard_layer, 1.f);
	for (auto &vader : game->vaders[game->current_buffer]) {
		tfx_vec3_t tween = InterpolateVec3(lerp, vader.captured, vader.position);
		zest_DrawBillboardSimple(game->billboard_layer, vader.image, &tween.x, DegreesToRadians(vader.angle), 0.5f, 0.5f);
	}
	for (auto &vader : game->big_vaders[game->current_buffer]) {
		tfx_vec3_t tween = InterpolateVec3(lerp, vader.captured, vader.position);
		zest_DrawBillboardSimple(game->billboard_layer, vader.image, &tween.x, DegreesToRadians(vader.angle), 1.f, 1.f);
	}
}

void DrawVaderBullets(VadersGame *game, float lerp) {
	for (auto &bullet : game->vader_bullets[game->current_buffer]) {
		int frame_offset = (int)bullet.frame % 64;
		tfx_vec3_t tween = InterpolateVec3(lerp, bullet.captured, bullet.position);
		zest_SetLayerColor(game->billboard_layer, 255, 255, 255, 0);
		zest_SetLayerIntensity(game->billboard_layer, 1.2f);
		zest_DrawBillboardSimple(game->billboard_layer, game->vader_bullet_image + frame_offset, &tween.x, 0.f, 0.25f, 0.25f);
		zest_SetLayerColor(game->billboard_layer, 255, 128, 64, 0);
		zest_SetLayerIntensity(game->billboard_layer, .5f);
		zest_DrawBillboardSimple(game->billboard_layer, game->vader_bullet_glow_image, &tween.x, 0.f, 0.65f, 0.65f);
	}
}

//A simple example to render the particles. This is for when the particle manager has one single list of sprites rather than grouped by effect
void RenderParticles3d(tfx_particle_manager_t &pm, VadersGame *game) {
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_SetInstanceDrawing(game->tfx_rendering.layer, game->tfx_rendering.shader_resource, game->tfx_rendering.pipeline);

	tfx_billboard_instance_t *billboards = GetBillboardBuffer(&pm);
	zest_draw_buffer_result result = zest_DrawInstanceBuffer(game->tfx_rendering.layer, billboards, pm.instance_buffer.current_size);
	game->index_offset[game->tfx_rendering.layer->fif ^ 1] = zest_GetInstanceLayerCount(game->tfx_rendering.layer);
}

//Render the particles by effect
void RenderEffectParticles(tfx_particle_manager_t &pm, VadersGame *game) {
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_SetInstanceDrawing(game->tfx_rendering.layer, game->tfx_rendering.shader_resource, game->tfx_rendering.pipeline);

	game->tfx_rendering.layer->current_instruction.push_constants.parameters1.x += game->index_offset[game->tfx_rendering.layer->fif];
	tfx_billboard_instance_t *billboards = nullptr;
	tfx_effect_instance_data_t *instance_data;
	tfxU32 instance_count = 0;
	bool halt = false;
	while (GetNextBillboardBuffer(&pm, &billboards, &instance_data, &instance_count)) {
		zest_draw_buffer_result result = zest_DrawInstanceBuffer(game->tfx_rendering.layer, billboards, instance_count);
	}
	ResetInstanceBufferLoopIndex(&pm);
}

void ResetGame(VadersGame *game) {
	ClearParticleManager(&game->game_pm, false, false);
	game->vaders[game->current_buffer].clear();
	game->big_vaders[game->current_buffer].clear();
	game->player_bullets[game->current_buffer].clear();
	game->vader_bullets[game->current_buffer].clear();
	game->power_ups[game->current_buffer].clear();
	game->player.rate_of_fire = 4.f * UpdateFrequency;
	game->score = 0;
	game->current_wave = 0;
	game->countdown_to_big_vader = RandomRange(&game->random, 15.f, 35.f);
}

void VadersGame::Update(float ellapsed) {
	//Accumulate the timer delta

	//Renderer specific
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);

	if (ImGui::IsKeyReleased(ImGuiKey_Space)) {
		paused = !paused;
	}

	UpdatePlayerPosition(this, &player);
	UpdateUniform3d(this);
	zest_Update2dUniformBuffer();

	zest_TimerAccumulate(timer);
	int pending_ticks = zest_TimerPendingTicks(timer);
	//Update the game logic at 60fps
	while (zest_TimerDoUpdate(timer)) {

		//Render based on the current game state
		if (state == GameState_title) {
			//Update the background particle manager
			TFX_ASSERT(!(background_pm.flags & tfxParticleManagerFlags_use_effect_sprite_buffers));
			if (pending_ticks > 0) {
				UpdateParticleManager(&background_pm, FrameLength * pending_ticks);
				//Update the title particle manager
				UpdateParticleManager(&title_pm, FrameLength * pending_ticks);
				pending_ticks = 0;
			}
			if (!wait_for_mouse_release && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				ResetGame(this);
				vaders[current_buffer].clear();
				state = GameState_game;
				wait_for_mouse_release = true;
			}
			else if (wait_for_mouse_release && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				wait_for_mouse_release = false;
			}
		}
		else if (state == GameState_game) {
			if (!paused) {
				//Update the main game particle manager and the background particle manager
				if (pending_ticks > 0) {
					UpdateParticleManager(&game_pm, FrameLength * pending_ticks);
					UpdateParticleManager(&background_pm, FrameLength * pending_ticks);
					pending_ticks = 0;
				}
				//We can alter the noise offset so that the bullet noise particles cycle their noise offsets to avoid repetition in the noise patterns
				noise_offset += 1.f * UpdateFrequency;
				//Update all the game objects
				UpdatePlayer(this, &player);
				UpdateVaders(this);
				UpdateVaderBullets(this);
				UpdatePlayerBullets(this);
				UpdatePowerUps(this);
				current_buffer = !current_buffer;
				if (vaders[current_buffer].size() == 0) {
					current_wave++;
					SpawnInvaderWave(this);
				}
				if (big_vaders[current_buffer].size() == 0 && countdown_to_big_vader <= 0) {
					SpawnBigVader(this);
				}
				else {
					countdown_to_big_vader -= UpdateFrequency;
				}
			}
		}
		else if (state == GameState_game_over) {
			//Update the main game particle manager and the background particle manager
			if (pending_ticks > 0) {
				UpdateParticleManager(&game_pm, FrameLength * pending_ticks);
				UpdateParticleManager(&background_pm, FrameLength * pending_ticks);
				pending_ticks = 0;
			}
			noise_offset += 1.f * UpdateFrequency;
			//Update all the game objects
			UpdateVaders(this);
			UpdateVaderBullets(this);
			UpdatePlayerBullets(this);
			UpdatePowerUps(this);
			current_buffer = !current_buffer;
			if (!wait_for_mouse_release && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				state = GameState_title;
				wait_for_mouse_release = true;
			}
			else if (wait_for_mouse_release && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				wait_for_mouse_release = false;
			}
		}
		//Draw the Imgui window
		BuildUI(this);

		zest_TimerUnAccumulate(timer);
	}

	zest_TimerSet(timer);

	//Do all the rendering outside of the update loop
	//Set the font drawing to the font we loaded in the Init function
	zest_SetMSDFFontDrawing(font_layer, font);
	//Render the background particles
	if (zest_TimerUpdateWasRun(timer)) {
		zest_ResetInstanceLayer(tfx_rendering.layer);
		RenderParticles3d(background_pm, this);
	}

	if (state == GameState_title) {
		//If showing the title screen, render the title particles
		if (zest_TimerUpdateWasRun(timer)) {
			RenderEffectParticles(title_pm, this);
		}
		//Draw the start text
		zest_DrawMSDFText(font_layer, "Press Button to Start", zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .5f, .5f, .5f, 30.f, 0.f);
	}
	else if (state == GameState_game) {
		//If the game is in the play state draw all the game billboards
		//Set the billboard drawing to the sprite texture
		zest_SetInstanceDrawing(billboard_layer, sprite_resources, billboard_pipeline);
		//Draw the player and vaders
		DrawPlayer(this);
		DrawVaders(this, (float)timer->lerp);
		//Render all of the game particles
		if (zest_TimerUpdateWasRun(timer)) {
			RenderEffectParticles(game_pm, this);
		}
		//Set the billboard drawing back to the sprite texture (after rendering the particles with the particle texture)
		//We want to draw the vader bullets over the top of the particles so that they're easier to see
		zest_SetInstanceDrawing(billboard_layer, sprite_resources, billboard_pipeline);
		DrawVaderBullets(this, (float)timer->lerp);
		tfx_str16_t score_text;
		tfx_str32_t high_score_text = "High Score: ";
		tfx_str16_t wave_text = "Wave: ";
		score_text.Appendf("%i", score);
		high_score_text.Appendf("%i", high_score);
		wave_text.Appendf("%i", (int)current_wave);
		//Draw some text for score and wave
		zest_DrawMSDFText(font_layer, score_text.c_str(), zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .95f, .5f, .5f, 20.f, 0.f);
		zest_DrawMSDFText(font_layer, high_score_text.c_str(), zest_ScreenWidthf() * .05f, zest_ScreenHeightf() * .95f, 0.f, .5f, 20.f, 0.f);
		zest_DrawMSDFText(font_layer, wave_text.c_str(), zest_ScreenWidthf() * .95f, zest_ScreenHeightf() * .95f, 1.f, .5f, 20.f, 0.f);
	}
	else if (state == GameState_game_over) {
		//Game over but keep drawing vaders until the player presses the mouse again to go back to the title screen
		zest_SetInstanceDrawing(billboard_layer, sprite_resources, billboard_pipeline);
		DrawVaders(this, (float)timer->lerp);
		if (zest_TimerUpdateWasRun(timer)) {
			RenderEffectParticles(game_pm, this);
		}
		zest_SetInstanceDrawing(billboard_layer, sprite_resources, billboard_pipeline);
		DrawVaderBullets(this, (float)timer->lerp);
		zest_DrawMSDFText(font_layer, "GAME OVER", zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .5f, .5f, .5f, 60.f, 0.f);
	}
	zest_imgui_UpdateBuffers(imgui_layer_info.mesh_layer);
}

//Update callback called from Zest
void UpdateTfxExample(zest_microsecs ellapsed, void *data) {
	VadersGame *game = static_cast<VadersGame*>(data);
	game->Update((float)ellapsed);
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main() {
	zest_vec3 v = zest_Vec3Set(1.f, 0.f, 0.f);
	zest_uint packed = zest_Pack8bitx3(&v);
	zest_create_info_t create_info = zest_CreateInfo();
	create_info.log_path = ".";
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	zest_implglfw_SetCallbacks(&create_info);

	VadersGame game;
	InitialiseTimelineFX(std::thread::hardware_concurrency(), tfxMegabyte(128));

	zest_Initialise(&create_info);
	zest_SetUserData(&game);
	zest_SetUserUpdateCallback(UpdateTfxExample);
	game.Init();

	zest_Start();

	return 0;
}
#else
int main(void) {
	zest_vec3 v = zest_Vec3Set(1.f, 0.f, 0.f);
	zest_uint packed = zest_Pack8bitx3(&v);
	zest_create_info_t create_info = zest_CreateInfo();
	create_info.log_path = ".";
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
#endif
