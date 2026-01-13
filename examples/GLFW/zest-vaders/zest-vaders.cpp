#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_MSDF_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <zest.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include "impl_timelinefx.h"
#include "timelinefx.h"

#define x_distance 0.078f
#define y_distance 0.158f
#define x_spacing 0.040f
#define y_spacing 0.058f

#define Rad90 1.5708f
#define Rad270 4.71239f

#define UpdateFrequency 0.016666666666f
#define FrameLength 16.66666666667f

typedef unsigned int u32;

struct RenderCacheInfo {
	bool draw_imgui;
	bool draw_timeline_fx;
	bool draw_sprites;
};

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
	zest_atlas_region_t *image;
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

struct billboard_push_constant_t {
	tfxU32 texture_index;
	tfxU32 sampler_index;
	tfxU32 uniform_index;
};

struct VadersGame {
	tfx_random_t random;
	zest_image_handle sprite_texture;
	zest_image_handle imgui_font_texture;
	zest_uint sprite_texture_index;

	bool paused = false;
	Player player;
	tfx_vector_t<PlayerBullet> player_bullets[2];
	tfx_vector_t<Vader> vaders[2];
	tfx_vector_t<Vader> big_vaders[2];
	tfx_vector_t<VaderBullet> vader_bullets[2];
	tfx_vector_t<tfxEffectID> power_ups[2];
	int current_buffer = 0;
	float noise_offset = 0.f;

	zest_atlas_region_t *player_image;
	zest_atlas_region_t *vader_image1;
	zest_atlas_region_t *vader_image2;
	zest_atlas_region_t *vader_image3;
	zest_atlas_region_t *big_vader_image;
	zest_atlas_region_t *vader_bullet_image;
	zest_atlas_region_t *vader_bullet_glow_image;

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

	tfx_library library;
	tfx_effect_manager game_pm;
	tfx_effect_manager background_pm;
	tfx_effect_manager title_pm;

	tfx_effect_template player_bullet_effect;
	tfx_effect_template vader_explosion_effect;
	tfx_effect_template player_explosion;
	tfx_effect_template big_explosion;
	tfx_effect_template background;
	tfx_effect_template title;
	tfx_effect_template laser;
	tfx_effect_template charge_up;
	tfx_effect_template weapon_power_up;
	tfx_effect_template got_power_up;
	tfx_effect_template damage;

	tfxEffectID background_index;
	tfxEffectID title_index;
	zest_context context;
	zest_device device;
	zest_msdf_font_t font;
	zest_imgui_t imgui;
	zest_layer_handle font_layer_handle;
	zest_font_resources_t font_resources;
	zest_layer_handle billboard_layer_handle;
	zest_uint particle_ds_index;
	zest_pipeline_template billboard_pipeline;
	tfx_render_resources_t tfx_rendering;
	billboard_push_constant_t billboard_push;
	zest_image_collection_t game_sprites;
	zest_sampler_handle sampler_handle;
	zest_uint sampler_index;

	RenderCacheInfo cache_info;

	zest_shader_handle billboard_frag_shader;
	zest_shader_handle billboard_vert_shader;

	int particle_option = 2;

	GameState state = GameState_title;
	bool wait_for_mouse_release = false;
	int request_graph_print = 0;

	double mouse_x, mouse_y;
	double mouse_delta_x, mouse_delta_y;
	zest_uint last_fps;

	void Init();
	void Update(float ellapsed);
};

float LengthVec3NoSqr(tfx_vec3_t const *v) {
	return v->x * v->x + v->y * v->y + v->z * v->z;
}

float LengthVec3(tfx_vec3_t const *v) {
	return sqrtf(LengthVec3NoSqr(v));
}

tfx_vec3_t NormalizeVec3(tfx_vec3_t const *v) {
	float length = LengthVec3(v);
	return length > 0.f ? tfx_vec3_t(v->x / length, v->y / length, v->z / length) : *v;
}

//Allows us to cast a ray into the screen from the mouse position to place an effect where we click
tfx_vec3_t ScreenRay(zest_context context, float x, float y, float depth_offset, zest_vec3 &camera_position, zest_uniform_buffer_handle buffer) {
	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(buffer);
	tfx_uniform_buffer_data_t *data = (tfx_uniform_buffer_data_t *)zest_GetUniformBufferData(uniform_buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(context), zest_ScreenHeightf(context), &data->proj, &data->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

//Update the power up effect to follow the player
void UpdateGotPowerUpEffect(tfx_effect_manager pm, tfxEffectID effect_index) {
	VadersGame *game = static_cast<VadersGame*>(tfx_GetEffectUserData(pm, effect_index));
	tfx_SetEffectPositionVec3(pm, effect_index, game->player.position);
}

void UpdateMouse(VadersGame *game) {
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

//Helper function to load and add an image to an image collection
zest_atlas_region_t *AddGameSprite(zest_image_collection_t *collection, const char *path, int frames, int frame_width, int frame_height) {
	int width, height, channels;
	stbi_uc *pixels = stbi_load(path, &width, &height, &channels, 0);
	int size = width * height * channels;
	zest_atlas_region_t *region = nullptr;
	if (frames <= 1) {
		region = zest_AddImageAtlasPixels(collection, pixels, size, width, height, zest_format_r8g8b8a8_unorm);
	} else {
		region = zest_AddImageAtlasAnimationPixels(collection, pixels, size, width, height, (tfxU32)frame_width, frame_height, frames, zest_format_r8g8b8a8_unorm);
	}
	STBI_FREE(pixels);
	return region;
}

void zest_DrawBillboard(zest_layer layer, zest_atlas_region_t *image, float position[3], float angle, float sx, float sy) {
    ZEST_ASSERT_HANDLE(layer);	//Not a valid handle!
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_instance);        //Call zest_StartSpriteDrawing before calling this function

    zest_billboard_instance_t* billboard = (zest_billboard_instance_t*)zest_NextInstance(layer);

    billboard->scale_handle = zest_Pack16bit4SScaled(sx, sy, 0.5f, 0.5f, 256.f, 128.f);
    billboard->position = zest_Vec3Set(position[0], position[1], position[2]);
    billboard->rotations_stretch = zest_Vec4Set( 0.f, 0.f, angle, 0.f );
    billboard->uv = image->uv_packed;
    billboard->alignment = 16711680;        //x = 0.f, y = 1.f, z = 0.f;
    billboard->color = layer->current_color;
    billboard->intensity_texture_array = (image->layer_index << 24) + (zest_uint)(layer->intensity * 0.125f * 4194303.f);
}

//Initialise the game and set up the renderer
void VadersGame::Init() {
	zest_tfx_InitTimelineFXRenderResources(device, context, &tfx_rendering, "examples/assets/vaders/vadereffects.tfx");
	//Renderer specific - initialise the texture
	float max_radius = 0;
	//Add all of the images and animations into the texture
	game_sprites = zest_CreateImageAtlasCollection(zest_format_r8g8b8a8_unorm, 100);
	int width, height, channels;
	stbi_uc *pixels = stbi_load("examples/assets/vaders/player.png", &width, &height, &channels, 0);
	int size = width * height * channels;
	player_image = AddGameSprite(&game_sprites, "examples/assets/vaders/player.png", 1, 0, 0);
	vader_image1 = AddGameSprite(&game_sprites, "examples/assets/vaders/vader.png", 1, 0, 0);
	vader_image2 = AddGameSprite(&game_sprites, "examples/assets/vaders/vader2.png", 1, 0, 0);
	vader_image3 = AddGameSprite(&game_sprites, "examples/assets/vaders/vader3.png", 1, 0, 0);
	big_vader_image = AddGameSprite(&game_sprites, "examples/assets/vaders/big_vader.png", 1, 0, 0);
	vader_bullet_image = AddGameSprite(&game_sprites, "examples/assets/vaders/vader_bullet_64_64_16.png", 64, 16, 16);
	vader_bullet_glow_image = AddGameSprite(&game_sprites, "examples/assets/vaders/flare4.png", 1, 0, 0);

	sprite_texture = zest_CreateImageAtlas(context, &game_sprites, 1024, 1024, 0);
	zest_image sprite_image = zest_GetImage(sprite_texture);
	sprite_texture_index = zest_AcquireSampledImageIndex(device, sprite_image, zest_texture_array_binding);

	//Create a descriptor set for the texture that uses the 3d uniform buffer
	particle_ds_index = 0;

	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	sampler_handle = zest_CreateSampler(context, &sampler_info);
	zest_sampler sampler = zest_GetSampler(sampler_handle);
	sampler_index = zest_AcquireSamplerIndex(device, sampler);

	billboard_push.texture_index = sprite_texture_index;
	billboard_push.sampler_index = sampler_index;

	tfx_RandomReSeedTime(&random);

	//Load the effects library and pass the shape loader function pointer that you created earlier. Also pass this pointer to point to this object to give the shapeloader access to the texture we're loading the particle images into
	library = tfx_LoadEffectLibrary("examples/assets/vaders/vadereffects.tfx", zest_tfx_ShapeLoader, zest_tfx_GetUV, &tfx_rendering);
	//Renderer specific
	//Process the texture with all the particle shapes that we just added to
	tfx_rendering.particle_texture = zest_CreateImageAtlas(context, &tfx_rendering.particle_images, 1024, 1024, 0);
	zest_image particle_image = zest_GetImage(tfx_rendering.particle_texture);
	tfx_rendering.particle_texture_index = zest_AcquireSampledImageIndex(device, particle_image, zest_texture_array_binding);

	//Prepare all the Effect templates we need from the library
	player_bullet_effect = tfx_CreateEffectTemplate(library, "Player Bullet");
	vader_explosion_effect = tfx_CreateEffectTemplate(library, "Vader Explosion");
	big_explosion = tfx_CreateEffectTemplate(library, "Big Explosion");
	player_explosion = tfx_CreateEffectTemplate(library, "Player Explosion");
	background = tfx_CreateEffectTemplate(library, "Background");
	title = tfx_CreateEffectTemplate(library, "Title");
	laser = tfx_CreateEffectTemplate(library, "Laser");
	charge_up = tfx_CreateEffectTemplate(library, "Charge Up");
	got_power_up = tfx_CreateEffectTemplate(library, "Got Power Up");
	weapon_power_up = tfx_CreateEffectTemplate(library, "Power Up");
	damage = tfx_CreateEffectTemplate(library, "Damage");

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
	tfx_rendering.color_ramps_index = zest_AcquireSampledImageIndex(device, color_ramps_image, zest_texture_array_binding);
	//Now that the particle shapes have been setup in the renderer, we can call this function to update the shape data in the library
	//with the correct uv texture coords ready to upload to gpu. This buffer will be accessed in the vertex shader when rendering.
	tfx_UpdateLibraryGPUImageData(library);

	//Now upload the image data to the GPU and set up the shader resources ready for rendering
	zest_tfx_UpdateTimelineFXImageData(context, &tfx_rendering, library);

	tfxU32 layer_max_values[tfxLAYERS];
	memset(layer_max_values, 0, 16);
	layer_max_values[0] = 5000;
	layer_max_values[1] = 2500;
	layer_max_values[2] = 2500;
	layer_max_values[3] = 2500;
	//Initialise a particle manager. This manages effects, emitters and the particles that they spawn
	//Depending on your needs you can use as many particle managers as you need.
	//pm.InitFor3d(layer_max_values, 100, tfx_particle_manager_tMode_ordered_by_depth_guaranteed, 512);
	tfx_effect_manager_info_t background_pm_info = tfx_CreateEffectManagerInfo(tfxEffectManagerSetup_group_sprites_by_effect);
	background_pm = tfx_CreateEffectManager(background_pm_info);
	tfx_effect_manager_info_t game_pm_info = tfx_CreateEffectManagerInfo(tfxEffectManagerSetup_group_sprites_by_effect);
	game_pm = tfx_CreateEffectManager(game_pm_info);
	game_pm_info.max_effects = 10;
	title_pm = tfx_CreateEffectManager(game_pm_info);

	//Load a font we can draw text with
	if (!zest__file_exists("examples/assets/vaders/Anta-Regular.msdf")) {
		font = zest_CreateMSDF(context, "examples/assets/vaders/Anta-Regular.ttf", sampler_index, 64.f, 4.f);
		zest_SaveMSDF(&font, "examples/assets/vaders/Anta-Regular.msdf");
	} else {
		font = zest_LoadMSDF(context, "examples/assets/vaders/Anta-Regular.msdf", sampler_index);
	}

	//font.settings.gamma = .25f;
	//font.settings.smoothness = 1.f;
	font.settings.shadow_color.a = 0;

	//Player setup
	player.rate_of_fire = 4.f * UpdateFrequency;
	high_score = 0;

	//Set the user data in the got power up effect and the update callback so that we can position it each frame
	tfx_SetTemplateEffectUserData(got_power_up, this);
	tfx_SetTemplateEffectUpdateCallback(got_power_up, UpdateGotPowerUpEffect);

	//Initialise imgui
	zest_imgui_Initialise(context, &imgui, zest_implglfw_DestroyWindow);
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)zest_Window(context), true);

	//Set up the font in imgui
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

	io.Fonts->Clear();
	float font_size = 15.f;
	unsigned char *font_data;
	int tex_width, tex_height;
	ImFontConfig config;
	config.PixelSnapH = true;
	io.Fonts->AddFontFromFileTTF("examples/assets/Lato-Regular.ttf", font_size);
	io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

	zest_imgui_RebuildFontTexture(&imgui, tex_width, tex_height, font_data);

	//Update the uniform buffer so that the projection and view matrices are set for the screen ray functions below
	zest_tfx_UpdateUniformBuffer(context, &tfx_rendering);

	//Get the top left and bottom right screen positions in 3d space so we know when things leave the screen
	top_left_bound = ScreenRay(context, 0.f, 0.f, 10.f, tfx_rendering.camera.position, tfx_rendering.uniform_buffer);
	bottom_right_bound = ScreenRay(context, zest_ScreenWidthf(context), zest_ScreenHeightf(context), 10.f, tfx_rendering.camera.position, tfx_rendering.uniform_buffer);

	//Add the background effect and title effect to the particle manager and set their positions
	if (tfx_AddEffectTemplateToEffectManager(background_pm, background, &background_index)) {
		zest_vec3 position = zest_AddVec3(zest_ScaleVec3(tfx_rendering.camera.front, 12.f), tfx_rendering.camera.position);
		tfx_SetEffectPositionVec3(background_pm, background_index, { position.x, position.y, position.z });
	}
	if (tfx_AddEffectTemplateToEffectManager(title_pm, title, &title_index)) {
		tfx_SetEffectPositionVec3(title_pm, title_index, ScreenRay(context, zest_ScreenWidthf(context) * .5f, zest_ScreenHeightf(context) * .25f, 4.f, tfx_rendering.camera.position, tfx_rendering.uniform_buffer));
	}

	//Create and compile the shaders for our custom sprite pipeline
	billboard_frag_shader = zest_CreateShaderFromFile(device, "examples/assets/shaders/billboard.frag", "billboard_frag.spv", zest_fragment_shader, true);
	billboard_vert_shader = zest_CreateShaderFromFile(device, "examples/assets/shaders/billboard.vert", "billboard_vert.spv", zest_vertex_shader, true);

	//Create a pipeline that we can use to draw billboards
	billboard_pipeline = zest_BeginPipelineTemplate(device, "pipeline_billboard");
	zest_AddVertexInputBindingDescription(billboard_pipeline, 0, sizeof(zest_billboard_instance_t), zest_input_rate_instance);

	zest_AddVertexAttribute(billboard_pipeline, 0, 0, zest_format_r32g32b32_sfloat, offsetof(zest_billboard_instance_t, position));			    // Location 0: Position
	zest_AddVertexAttribute(billboard_pipeline, 0, 1, zest_format_r8g8b8_snorm, offsetof(zest_billboard_instance_t, alignment));		         	// Location 9: Alignment X, Y and Z
	zest_AddVertexAttribute(billboard_pipeline, 0, 2, zest_format_r32g32b32a32_sfloat, offsetof(zest_billboard_instance_t, rotations_stretch));	// Location 2: Rotations + stretch
	zest_AddVertexAttribute(billboard_pipeline, 0, 3, zest_format_r16g16b16a16_snorm, offsetof(zest_billboard_instance_t, uv));		    		// Location 1: uv_packed
	zest_AddVertexAttribute(billboard_pipeline, 0, 4, zest_format_r16g16b16a16_sscaled, offsetof(zest_billboard_instance_t, scale_handle));		// Location 4: Scale + Handle
	zest_AddVertexAttribute(billboard_pipeline, 0, 5, zest_format_r32_uint, offsetof(zest_billboard_instance_t, intensity_texture_array));		// Location 6: texture array index * intensity
	zest_AddVertexAttribute(billboard_pipeline, 0, 6, zest_format_r8g8b8a8_unorm, offsetof(zest_billboard_instance_t, color));			        // Location 7: Instance Color

	zest_SetPipelineVertShader(billboard_pipeline, billboard_vert_shader);
	zest_SetPipelineFragShader(billboard_pipeline, billboard_frag_shader);
	zest_SetPipelineDepthTest(billboard_pipeline, false, true);

	billboard_layer_handle = zest_CreateInstanceLayer(context, "billboards", sizeof(zest_billboard_instance_t), 10000);

	font_resources = zest_CreateFontResources(context, "shaders/font.vert", "shaders/font.frag");
	font_layer_handle = zest_CreateFontLayer(context, "MSDF Font", 500);

	zest_ForEachFrameInFlight(fif) {
		index_offset[fif] = 0;
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
	float radians = tfx_DegreesToRadians(angle);  // convert angle to radians
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

	float width = zest_ScreenWidthf(game->context);
	float height = zest_ScreenHeightf(game->context);
	game->margin_x = width * x_distance;
	game->margin_y = height * y_distance;
	game->spacing_x = width * x_spacing;
	game->spacing_y = height * y_spacing;

	for (float x = 0; x < cols; ++x) {
		for (float y = 0; y < rows; ++y) {
			Vader vader;
			vader.position = vader.captured = ScreenRay(game->context, game->spacing_x * x + game->margin_x, game->spacing_y * y + game->margin_y, 10.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer);
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
	float width = zest_ScreenWidthf(game->context);
	float height = zest_ScreenHeightf(game->context);
	game->margin_x = width * x_distance;
	game->margin_y = height * y_distance;

	Vader vader;
	vader.end_position = ScreenRay(game->context, zest_ScreenWidthf(game->context) * .5f, game->margin_y * .45f, 10.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer);
	vader.position = vader.captured = vader.start_position = ScreenRay(game->context, zest_ScreenWidthf(game->context) * .5f, -game->margin_y, 10.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer);;
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
			if (tfx_GetDistance(bullet.position.z, bullet.position.y, vader.position.z, vader.position.y) < 0.3f) {
				hit = true;
				bullet.remove = true;
				//Blow up the vader. Add teh vader_explosion_effect template to the particle manager
				tfxEffectID effect_index;
				if (tfx_AddEffectTemplateToEffectManager(game->game_pm, game->vader_explosion_effect, &effect_index)) {
					//Set the effect position
					tfx_SetEffectPositionVec3(game->game_pm, effect_index, vader.position);
					//Alter the effect scale
					tfx_SetEffectOveralScale(game->game_pm, effect_index, 2.5f);
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
			vader.direction += tfx_DegreesToRadians(180.f + (wave * 10.f)) * UpdateFrequency;
			if (vader.direction >= Rad270) {
				vader.direction = Rad270;
				vader.turning = 0;
			}
		}
		else if (vader.turning == 2) {
			vader.direction -= tfx_DegreesToRadians(180.f + (wave * 10.f)) * UpdateFrequency;
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
		if (game->state != GameState_game_over && tfx_RandomRangeZeroToMax(&game->random, 1.f) <= vader.chance_to_shoot) {
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
			if (tfx_GetDistance(bullet.position.z, bullet.position.y, vader.position.z, vader.position.y) < 0.3f) {
				bullet.remove = true;
				vader.health--;
				//Add the damage taken effect to the particle manager and set it's position
				tfxEffectID damage_index;
				if (tfx_AddEffectTemplateToEffectManager(game->game_pm, game->damage, &damage_index)) {
					tfx_SetEffectPositionVec3(game->game_pm, damage_index, bullet.position);
				}
				if (vader.health == 0) {
					dead = true;
					//blow up the big vader, add the big_explosion template to the particle manager and set position and scale
					tfxEffectID effect_index;
					if (tfx_AddEffectTemplateToEffectManager(game->game_pm, game->big_explosion, &effect_index)) {
						tfx_SetEffectPositionVec3(game->game_pm, effect_index, vader.position);
						tfx_SetEffectOveralScale(game->game_pm, effect_index, 2.5f);
					}
					game->score += 500;
					game->high_score = tfxMax(game->score, game->high_score);
					if (vader.flags & VaderFlags_firing_laser) {
						tfx_SoftExpireEffect(game->game_pm, vader.laser);
					}
					//Add the power up effect that floats downward to the particle manager and set its position and scale
					tfxEffectID power_up_index;
					if (tfx_AddEffectTemplateToEffectManager(game->game_pm, game->weapon_power_up, &power_up_index)) {
						tfx_SetEffectPositionVec3(game->game_pm, power_up_index, vader.position);
						tfx_SetEffectOveralScale(game->game_pm, power_up_index, 3.f);
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
			if (tfx_AddEffectTemplateToEffectManager(game->game_pm, game->charge_up, &effect_index)) {
				tfx_vec3_t laser_offset = RotatePoint({ 0.f, .4f, 0.f }, { 0.f, 0.f, 0.f }, vader.angle - 90.f);
				tfx_SetEffectPositionVec3(game->game_pm, effect_index, vader.position + laser_offset);
			}
			vader.flags |= VaderFlags_charging_up;
			vader.flags |= VaderFlags_in_position;
		}
		else if (vader.time > 3.f && vader.flags & VaderFlags_charging_up) {
			//Big vader has finished charging up so shoot the laser
			//Add the laser effect to the particle manager and set the position
			tfx_AddEffectTemplateToEffectManager(game->game_pm, game->laser, &vader.laser);
			tfx_vec3_t laser_offset = RotatePoint({ 0.f, .4f, 0.f }, { 0.f, 0.f, 0.f }, vader.angle - 90.f);
			tfx_SetEffectPositionVec3(game->game_pm, vader.laser, vader.position + laser_offset);
			tfx_SetEffectRoll(game->game_pm, vader.laser, -tfx_DegreesToRadians(vader.angle));
			tfx_SetEffectOveralScale(game->game_pm, vader.laser, 2.f);
			//Set the height of the laser which will change the length of it. We want it to extend off of the screen
			tfx_SetEffectHeightMultiplier(game->game_pm, vader.laser, 3.f);
			vader.flags |= VaderFlags_firing_laser;
			vader.flags &= ~VaderFlags_charging_up;
		}
		else if (vader.flags & VaderFlags_firing_laser) {
			//While the laser is firing, rotate the laser effect and update it's position to keep it at the tip of the vader
			float time = (vader.time - 3.f) * .25f;
			vader.angle = EaseInOutQuad(tfxMin(time * .5f, 1.f)) * (vader.end_angle - vader.start_angle) + vader.start_angle;
			tfx_vec3_t laser_offset = RotatePoint({ 0.f, .4f, 0.f }, { 0.f, 0.f, 0.f }, vader.angle - 90.f);
			tfx_SetEffectPositionVec3(game->game_pm, vader.laser, vader.position + laser_offset);
			//Rotate the laser
			tfx_SetEffectRoll(game->game_pm, vader.laser, -tfx_DegreesToRadians(vader.angle));
			if (vader.angle == vader.end_angle) {
				vader.time = 0;
				vader.end_angle = vader.start_angle;
				vader.start_angle = vader.angle;
				//Now that we've reached the final angle of the laser, soft expire the effect so that the effect stops spawning particles 
				//and any remaining particles expire naturally
				tfx_SoftExpireEffect(game->game_pm, vader.laser);
				vader.flags &= ~VaderFlags_firing_laser;
			}
			else {
				tfx_vec3_t laser_normal = NormalizeVec3(&laser_offset);
				//Check to see if the laser is colliding with the player
				if (game->state != GameState_game_over && IsLineCircleCollision(vader.position + laser_offset, vader.position + laser_offset + (laser_normal * 20.f), game->player.position, .3f)) {
					//Destroy the player. Add the player explosion to the particle manager and position/scale it.
					tfxEffectID effect_index;
					if (tfx_AddEffectTemplateToEffectManager(game->game_pm, game->player_explosion, &effect_index)) {
						tfx_SetEffectPositionVec3(game->game_pm, effect_index, game->player.position);
						tfx_SetEffectOveralScale(game->game_pm, effect_index, 1.5f);
					}
					game->state = GameState_game_over;
				}
			}
		}
		//push the big vader to the next buffer
		game->big_vaders[next_buffer].push_back(vader);
	}
	if (game->big_vaders[next_buffer].size()) {
		game->countdown_to_big_vader = tfx_RandomRangeFromTo(&game->random, 15.f, 35.f);
	}
}

void UpdatePowerUps(VadersGame *game) {
	int next_buffer = !game->current_buffer;
	game->power_ups[next_buffer].clear();
	for (auto &power_up : game->power_ups[game->current_buffer]) {
		tfx_vec3_t position;
		tfx_GetEffectPositionVec3(game->game_pm, power_up, &position.x);
		if (tfx_GetDistance(position.z, position.y, game->player.position.z, game->player.position.y) < 0.3f) {
			tfx_HardExpireEffect(game->game_pm, power_up);
			tfxEffectID effect_index;
			if (tfx_AddEffectTemplateToEffectManager(game->game_pm, game->got_power_up, &effect_index)) {
				tfx_SetEffectPositionVec3(game->game_pm, effect_index, game->player.position);
				tfx_SetEffectOveralScale(game->game_pm, effect_index, 2.5f);
			}
			game->player.rate_of_fire += 1 * UpdateFrequency;
			continue;
		}
		tfx_MoveEffect(game->game_pm, power_up, 0.f, -1.f * UpdateFrequency, 0.f);
		game->power_ups[next_buffer].push_back(power_up);
	}
}

void UpdatePlayerPosition(VadersGame *game, Player *player) {
	player->position = ScreenRay(game->context, (float)game->mouse_x, (float)game->mouse_y, 10.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer);
}

void UpdatePlayer(VadersGame *game, Player *player) {
	if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		player->fire_count += player->rate_of_fire;
		if (player->fire_count >= 1.f) {
			player->fire_count = 0;
			PlayerBullet new_bullet;
			if(tfx_AddEffectTemplateToEffectManager(game->game_pm, game->player_bullet_effect, &new_bullet.effect_index)){
				new_bullet.position = player->position;
				tfx_SetEffectPositionVec3(game->game_pm, new_bullet.effect_index, new_bullet.position);
				tfx_SetEffectBaseNoiseOffset(game->game_pm, new_bullet.effect_index, game->noise_offset);
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
	tfx_vec3_t top_left = ScreenRay(game->context, 0.f, 0.f, 10.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer);
	for (auto &bullet : game->player_bullets[game->current_buffer]) {
		if (bullet.remove) {
			tfx_SoftExpireEffect(game->game_pm, bullet.effect_index);
			continue;
		}
		bullet.position.y += bullet.speed * UpdateFrequency;
		if (bullet.position.y > top_left.y + 1.f) {
			//Expire the effect if it hits the top of the screen
			tfx_SoftExpireEffect(game->game_pm, bullet.effect_index);
		}
		else {
			//Update the bullet effect position
			tfx_SetEffectPositionVec3(game->game_pm, bullet.effect_index, bullet.position);
			game->player_bullets[next_buffer].push_back(bullet);
		}
	}
}

void UpdateVaderBullets(VadersGame *game) {
	int next_buffer = !game->current_buffer;
	game->vader_bullets[next_buffer].clear();
	for (auto &bullet : game->vader_bullets[game->current_buffer]) {
		//Does the vader bullet collide with the player?
		if (game->state != GameState_game_over && tfx_GetDistance(bullet.position.z, bullet.position.y, game->player.position.z, game->player.position.y) < 0.3f) {
			//Blow up the player, add the player_explosion effect to the particle manager and set it's position/scale
			tfxEffectID effect_index = 0;
			if (tfx_AddEffectTemplateToEffectManager(game->game_pm, game->player_explosion, &effect_index)) {
				tfx_SetEffectPositionVec3(game->game_pm, effect_index, bullet.position);
				tfx_SetEffectOveralScale(game->game_pm, effect_index, 1.5f);
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
		tfx_ScaleTemplateGlobalMultiplier(game->background, tfxEffect_global_amount_index, .25f);
		//Restart the title/background effects
		tfx_SoftExpireEffect(game->background_pm, game->background_index);
		tfx_HardExpireEffect(game->title_pm, game->title_index);
		tfx_DisableTemplateEmitter(game->player_bullet_effect, "Player Bullet/Flare");
		//Disable the flare emitter in the title effect
		tfx_DisableTemplateEmitter(game->title, "Title/Flare");
		//Set the single spawn amount of the emitters with the big explosion effect
		tfx_SetTemplateSingleSpawnAmount(game->big_explosion, "Big Explosion/Ring Stretch", 100);
		tfx_SetTemplateSingleSpawnAmount(game->big_explosion, "Big Explosion/Ring Stretch.1", 100);
		//Scale the base amount of the cloud burst expand emitter
		tfx_ScaleTemplateEmitterGraph(game->big_explosion, "Big Explosion/Cloud Burst Expand", tfxEmitter_base_amount_index, 0.25f);
		//Tone down the player explosion as well
		tfx_SetTemplateSingleSpawnAmount(game->player_explosion, "Player Explosion/Ring Stretch", 100);
		tfx_SetTemplateSingleSpawnAmount(game->player_explosion, "Player Explosion/Ring Stretch.1", 100);
		tfx_ScaleTemplateEmitterGraph(game->player_explosion, "Player Explosion/Cloud Burst", tfxEmitter_base_amount_index, 0.25f);
		tfx_SetTemplateSingleSpawnAmount(game->got_power_up, "Got Power Up/Delayed Ellipse", 100);
		tfx_ScaleTemplateGlobalMultiplier(game->vader_explosion_effect, tfxEffect_global_amount_index, .25f);
		//For the laser scale the global amount to adjust the amount of all emitters within the effect
		tfx_ScaleTemplateGlobalMultiplier(game->laser, tfxEffect_global_amount_index, .5f);
		tfx_DisableTemplateEmitter(game->laser, "Laser/Flare");

		if (tfx_AddEffectTemplateToEffectManager(game->background_pm, game->background, &game->background_index)) {
			zest_vec3 position = zest_AddVec3(zest_ScaleVec3(game->tfx_rendering.camera.front, 12.f), game->tfx_rendering.camera.position);
			tfx_SetEffectPositionVec3(game->background_pm, game->background_index, { position.x, position.y, position.z });
		}
		if (tfx_AddEffectTemplateToEffectManager(game->title_pm, game->title, &game->title_index)) {
			tfx_SetEffectPositionVec3(game->title_pm, game->title_index, ScreenRay(game->context, zest_ScreenWidthf(game->context) * .5f, zest_ScreenHeightf(game->context) * .25f, 4.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer));
		}
	}
	else if (game->particle_option == 1) {
		tfx_ScaleTemplateGlobalMultiplier(game->background, tfxEffect_global_amount_index, .65f);
		tfx_SoftExpireEffect(game->background_pm, game->background_index);
		tfx_HardExpireEffect(game->title_pm, game->title_index);
		tfx_EnableTemplateEmitter(game->player_bullet_effect, "Player Bullet/Flare");
		tfx_ScaleTemplateEmitterGraph(game->player_bullet_effect, "Player Bullet/Flare", tfxEmitter_base_amount_index, 0.5f);
		tfx_DisableTemplateEmitter(game->title, "Title/Flare");
		tfx_SetTemplateSingleSpawnAmount(game->big_explosion, "Big Explosion/Ring Stretch", 200);
		tfx_SetTemplateSingleSpawnAmount(game->big_explosion, "Big Explosion/Ring Stretch.1", 250);
		tfx_ScaleTemplateEmitterGraph(game->big_explosion, "Big Explosion/Cloud Burst Expand", tfxEmitter_base_amount_index, 0.65f);
		tfx_SetTemplateSingleSpawnAmount(game->player_explosion, "Player Explosion/Ring Stretch", 200);
		tfx_SetTemplateSingleSpawnAmount(game->player_explosion, "Player Explosion/Ring Stretch.1", 250);
		tfx_ScaleTemplateEmitterGraph(game->player_explosion, "Player Explosion/Cloud Burst", tfxEmitter_base_amount_index, 0.65f);
		tfx_ScaleTemplateGlobalMultiplier(game->vader_explosion_effect, tfxEffect_global_amount_index, .65f);
		tfx_SetTemplateSingleSpawnAmount(game->got_power_up, "Got Power Up/Delayed Ellipse", 250);
		tfx_ScaleTemplateGlobalMultiplier(game->laser, tfxEffect_global_amount_index, 1.f);
		tfx_DisableTemplateEmitter(game->laser, "Laser/Flare");

		if (tfx_AddEffectTemplateToEffectManager(game->background_pm, game->background, &game->background_index)) {
			zest_vec3 position = zest_AddVec3(zest_ScaleVec3(game->tfx_rendering.camera.front, 12.f), game->tfx_rendering.camera.position);
			tfx_SetEffectPositionVec3(game->background_pm, game->background_index, { position.x, position.y, position.z });
		}
		if (tfx_AddEffectTemplateToEffectManager(game->title_pm, game->title, &game->title_index)) {
			tfx_SetEffectPositionVec3(game->title_pm, game->title_index, ScreenRay(game->context, zest_ScreenWidthf(game->context) * .5f, zest_ScreenHeightf(game->context) * .25f, 4.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer));
		}
	}
	else if (game->particle_option == 2) {
		tfx_ScaleTemplateGlobalMultiplier(game->background, tfxEffect_global_amount_index, 1.f);
		tfx_SoftExpireEffect(game->background_pm, game->background_index);
		tfx_EnableTemplateEmitter(game->player_bullet_effect, "Player Bullet/Flare");
		tfx_ScaleTemplateEmitterGraph(game->player_bullet_effect, "Player Bullet/Flare", tfxEmitter_base_amount_index, 1.f);
		tfx_EnableTemplateEmitter(game->title, "Title/Flare");
		tfx_SetTemplateSingleSpawnAmount(game->big_explosion, "Big Explosion/Ring Stretch", 400);
		tfx_SetTemplateSingleSpawnAmount(game->big_explosion, "Big Explosion/Ring Stretch.1", 500);
		tfx_ScaleTemplateEmitterGraph(game->big_explosion, "Big Explosion/Cloud Burst Expand", tfxEmitter_base_amount_index, 1.f);
		tfx_SetTemplateSingleSpawnAmount(game->player_explosion, "Player Explosion/Ring Stretch", 400);
		tfx_SetTemplateSingleSpawnAmount(game->player_explosion, "Player Explosion/Ring Stretch.1", 500);
		tfx_ScaleTemplateEmitterGraph(game->player_explosion, "Player Explosion/Cloud Burst", tfxEmitter_base_amount_index, 1.f);
		tfx_ScaleTemplateGlobalMultiplier(game->vader_explosion_effect, tfxEffect_global_amount_index, 1.f);
		tfx_SetTemplateSingleSpawnAmount(game->got_power_up, "Got Power Up/Delayed Ellipse", 566);
		tfx_ScaleTemplateGlobalMultiplier(game->laser, tfxEffect_global_amount_index, 1.f);
		tfx_EnableTemplateEmitter(game->laser, "Laser/Flare");

		if (tfx_AddEffectTemplateToEffectManager(game->background_pm, game->background, &game->background_index)) {
			zest_vec3 position = zest_AddVec3(zest_ScaleVec3(game->tfx_rendering.camera.front, 12.f), game->tfx_rendering.camera.position);
			tfx_SetEffectPositionVec3(game->background_pm, game->background_index, { position.x, position.y, position.z });
		}
		tfx_HardExpireEffect(game->title_pm, game->title_index);
		if (tfx_AddEffectTemplateToEffectManager(game->title_pm, game->title, &game->title_index)) {
			tfx_SetEffectPositionVec3(game->title_pm, game->title_index, ScreenRay(game->context, zest_ScreenWidthf(game->context) * .5f, zest_ScreenHeightf(game->context) * .25f, 4.f, game->tfx_rendering.camera.position, game->tfx_rendering.uniform_buffer));
		}
	}
}

void BuildUI(VadersGame *game) {
	//Draw the imgui window
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	//if (ImGui::IsKeyDown(ImGuiKey_Space)) {
		ImGui::Begin("Effects");
		ImGui::Text("FPS: %i", game->last_fps);
		ImGui::Text("Game Particles: %i", tfx_ParticleCount(game->game_pm));
		ImGui::Text("Background Particles: %i", tfx_ParticleCount(game->background_pm));
		ImGui::Text("Title Particles: %i", tfx_ParticleCount(game->title_pm));
		ImGui::Text("Effects: %i", tfx_EffectCount(game->game_pm));
		ImGui::Text("Emitters: %i", tfx_EmitterCount(game->game_pm));
		ImGui::Text("Free Emitters: %i", game->game_pm->free_emitters.size());
		ImGui::Text("Position: %f, %f, %f", game->player.position.x, game->player.position.y, game->player.position.z);
		static bool filtering = false;
		static bool sync_fps = false;
		ImGui::Checkbox("Sync FPS", &sync_fps);
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			if (sync_fps) {
				zest_EnableVSync(game->context);
			} else {
				zest_DisableVSync(game->context);
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
		if (ImGui::Button("Print Render Graph")) {
			game->request_graph_print = 1;
		}
		ImGui::End();
	//}

	ImGui::Render();
	ImGui::UpdatePlatformWindows();
}

//Draw all the billboards for the game
void DrawPlayer(VadersGame *game, zest_layer layer) {
	zest_SetLayerColor(layer, 255, 255, 255, 255);
	zest_SetLayerIntensity(layer, 1.f);
	zest_DrawBillboard(layer, game->player_image, &game->player.position.x, 0.f, 1.f, 1.f);
}

tfx_vec3_t InterpolateVec3(float tween, tfx_vec3_t from, tfx_vec3_t to) {
	return to * tween + from * (1.f - tween);
}

void DrawVaders(VadersGame *game, zest_layer layer, float lerp) {
	zest_SetLayerColor(layer, 255, 255, 255, 255);
	zest_SetLayerIntensity(layer, 1.f);
	for (auto &vader : game->vaders[game->current_buffer]) {
		tfx_vec3_t tween = InterpolateVec3(lerp, vader.captured, vader.position);
		zest_DrawBillboard(layer, vader.image, &tween.x, tfx_DegreesToRadians(vader.angle), 0.5f, 0.5f);
	}
	for (auto &vader : game->big_vaders[game->current_buffer]) {
		tfx_vec3_t tween = InterpolateVec3(lerp, vader.captured, vader.position);
		zest_DrawBillboard(layer, vader.image, &tween.x, tfx_DegreesToRadians(vader.angle), 1.f, 1.f);
	}
}

void DrawVaderBullets(VadersGame *game, zest_layer layer, float lerp) {
	for (auto &bullet : game->vader_bullets[game->current_buffer]) {
		int frame_offset = (int)bullet.frame % 64;
		tfx_vec3_t tween = InterpolateVec3(lerp, bullet.captured, bullet.position);
		zest_SetLayerColor(layer, 255, 255, 255, 0);
		zest_SetLayerIntensity(layer, 1.2f);
		zest_DrawBillboard(layer, game->vader_bullet_image + frame_offset, &tween.x, 0.f, 0.25f, 0.25f);
		zest_SetLayerColor(layer, 255, 128, 64, 0);
		zest_SetLayerIntensity(layer, .5f);
		zest_DrawBillboard(layer, game->vader_bullet_glow_image, &tween.x, 0.f, 0.65f, 0.65f);
	}
}

void ResetGame(VadersGame *game) {
	tfx_ClearEffectManager(game->game_pm, false, false);
	game->vaders[game->current_buffer].clear();
	game->big_vaders[game->current_buffer].clear();
	game->player_bullets[game->current_buffer].clear();
	game->vader_bullets[game->current_buffer].clear();
	game->power_ups[game->current_buffer].clear();
	game->player.rate_of_fire = 4.f * UpdateFrequency;
	game->score = 0;
	game->current_wave = 0;
	game->countdown_to_big_vader = tfx_RandomRangeFromTo(&game->random, 15.f, 35.f);
}

//A simple example to render the particles. This is for when the particle manager has one single list of sprites rather than grouped by effect
void RenderParticles3d(tfx_effect_manager pm, VadersGame *game, zest_layer layer) {
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_SetInstanceDrawing(layer, game->tfx_rendering.pipeline);
	tfx_instance_t *billboards = tfx_GetInstanceBuffer(pm);
	zest_draw_buffer_result result = zest_DrawInstanceBuffer(layer, billboards, tfx_GetInstanceCount(pm));
	int fif = zest_GetLayerFrameInFlight(layer);
	game->index_offset[fif ^ 1] = zest_GetInstanceLayerCount(layer);
}

//Render the particles by effect
void RenderEffectParticles(tfx_effect_manager pm, VadersGame *game, zest_layer layer) {
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_SetInstanceDrawing(layer, game->tfx_rendering.pipeline);

	//Because we're drawing the background first without using per effect drawing, we need to send the starting offset of the sprite
	//instances in the layer so that the previous sprite lookup in the shader is aligned properly.
	tfx_push_constants_t *push = (tfx_push_constants_t *)zest_GetLayerPushConstants(layer);
	int fif = zest_GetLayerFrameInFlight(layer);
	push->index_offset += game->index_offset[fif];

	tfx_instance_t *billboards = nullptr;
	tfx_effect_instance_data_t *instance_data;
	tfxU32 instance_count = 0;
	while (tfx_GetNextInstanceBuffer(pm, &billboards, &instance_data, &instance_count)) {
		zest_draw_buffer_result result = zest_DrawInstanceBuffer(layer, billboards, instance_count);
	}
	tfx_ResetInstanceBufferLoopIndex(pm);
}

void UploadGameLayerData(const zest_command_list command_list, void *user_data) {
	VadersGame *game = (VadersGame*)user_data;

	zest_layer font_layer = zest_GetLayer(game->font_layer_handle);
	zest_layer billboard_layer = zest_GetLayer(game->billboard_layer_handle);
	zest_layer tfx_layer = zest_GetLayer(game->tfx_rendering.layer);
	zest_buffer billboard_staging = zest_GetLayerStagingVertexBuffer(billboard_layer);
	zest_buffer tfx_staging = zest_GetLayerStagingVertexBuffer(tfx_layer);
	zest_buffer font_staging = zest_GetLayerStagingVertexBuffer(font_layer);
	zest_buffer billboard_device = zest_GetLayerResourceBuffer(billboard_layer);
	zest_buffer tfx_device = zest_GetLayerResourceBuffer(tfx_layer);
	zest_buffer font_device = zest_GetLayerResourceBuffer(font_layer);

	zest_buffer_uploader_t billboard_upload = { 0, billboard_staging, billboard_device, 0 };
	zest_buffer_uploader_t font_upload = { 0, font_staging, font_device, 0 };
	zest_buffer_uploader_t tfx_upload = { 0, tfx_staging, tfx_device, 0 };

	zest_size memory_in_use = zest_GetLayerVertexMemoryInUse(billboard_layer);
	if (memory_in_use && billboard_device) {
		zest_AddCopyCommand(command_list->context, &billboard_upload, billboard_staging, billboard_device, memory_in_use);
	}

	memory_in_use = zest_GetLayerVertexMemoryInUse(font_layer);
	if (memory_in_use && font_device) {
		zest_AddCopyCommand(command_list->context, &font_upload, font_staging, font_device, memory_in_use);
	}

	memory_in_use = zest_GetLayerVertexMemoryInUse(tfx_layer);
	if (memory_in_use && tfx_device) {
		zest_AddCopyCommand(command_list->context, &tfx_upload, tfx_staging, tfx_device, memory_in_use);
	}

	zest_cmd_UploadBuffer(command_list, &billboard_upload);
	zest_cmd_UploadBuffer(command_list, &font_upload);
	zest_cmd_UploadBuffer(command_list, &tfx_upload);
}

void VadersGame::Update(float ellapsed) {
	zest_UpdateDevice(device);

	UpdatePlayerPosition(this, &player);

	UpdateMouse(this);

	zest_layer font_layer = zest_GetLayer(font_layer_handle);
	zest_layer billboard_layer = zest_GetLayer(billboard_layer_handle);
	zest_layer tfx_layer = zest_GetLayer(tfx_rendering.layer);

	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(tfx_rendering.uniform_buffer);
	billboard_push.uniform_index = zest_GetUniformBufferDescriptorIndex(uniform_buffer);

	if (zest_BeginFrame(context)) {

		zest_StartTimerLoop(tfx_rendering.timer) {
			//Render based on the current game state
			if (state == GameState_title) {
				//Update the background particle manager
				if (pending_ticks > 0) {
					tfx_UpdateEffectManager(background_pm, FrameLength * pending_ticks);
					//Update the title particle manager
					tfx_UpdateEffectManager(title_pm, FrameLength * pending_ticks);
					pending_ticks = 0;
				}
				if (!ImGui::IsKeyDown(ImGuiKey_Space)) {
					if (!wait_for_mouse_release && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
						ResetGame(this);
						vaders[current_buffer].clear();
						state = GameState_game;
						wait_for_mouse_release = true;
					} else if (wait_for_mouse_release && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
						wait_for_mouse_release = false;
					}
				}
			}
			else if (state == GameState_game) {
				if (!paused) {
					//Update the main game particle manager and the background particle manager
					if (pending_ticks > 0) {
						tfx_UpdateEffectManager(game_pm, FrameLength * pending_ticks);
						tfx_UpdateEffectManager(background_pm, FrameLength * pending_ticks);
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
					current_buffer = current_buffer ^ 1;
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
					tfx_UpdateEffectManager(game_pm, FrameLength * pending_ticks);
					tfx_UpdateEffectManager(background_pm, FrameLength * pending_ticks);
					pending_ticks = 0;
				}
				noise_offset += 1.f * UpdateFrequency;
				//Update all the game objects
				UpdateVaders(this);
				UpdateVaderBullets(this);
				UpdatePlayerBullets(this);
				UpdatePowerUps(this);
				current_buffer = current_buffer ^ 1;
				if (!ImGui::IsKeyDown(ImGuiKey_Space)) {
					if (!wait_for_mouse_release && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
						state = GameState_title;
						wait_for_mouse_release = true;
						//Clear out the title effect manager and restart the effect. This is so that when the shader tries to 
						//lerp with particles in the previous it won't be able to because they've already been replaced with other
						//sprite data while the game was playing. If you didn't want to do this then you could just give the title
						//effect manager it's own layer and sprite buffer to render particles with, but I just simlify things here
						//by restarting the effect instead.
						tfx_HardExpireEffect(title_pm, title_index);
						tfx_ClearEffectManager(title_pm, false, false);
						if (tfx_AddEffectTemplateToEffectManager(title_pm, title, &title_index)) {
							tfx_SetEffectPositionVec3(title_pm, title_index, ScreenRay(context, zest_ScreenWidthf(context) * .5f, zest_ScreenHeightf(context) * .25f, 4.f, tfx_rendering.camera.position, tfx_rendering.uniform_buffer));
						}
					} else if (wait_for_mouse_release && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
						wait_for_mouse_release = false;
					}
				}
			}
			//Draw the Imgui window
			BuildUI(this);
		} zest_EndTimerLoop(tfx_rendering.timer);

		zest_SetMSDFFontDrawing(font_layer, &font, &font_resources);
		zest_SetLayerColor(font_layer, 255, 255, 255, 255);
		//Do all the rendering outside of the update loop
		//Set the font drawing to the font we loaded in the Init function
		zest_SetMSDFFontDrawing(font_layer, &font, &font_resources);
		//Render the background particles
		if (zest_TimerUpdateWasRun(&tfx_rendering.timer)) {
			zest_ResetInstanceLayer(tfx_layer);
			RenderParticles3d(background_pm, this, tfx_layer);
		}

		if (state == GameState_title) {
			//If showing the title screen, render the title particles
			if (zest_TimerUpdateWasRun(&tfx_rendering.timer)) {
				RenderEffectParticles(title_pm, this, tfx_layer);
			}
			//Draw the start text
			zest_DrawMSDFText(font_layer, zest_ScreenWidthf(context) * .5f, zest_ScreenHeightf(context) * .5f, .5f, .5f, .5f, 0.f, "Press Button to Start");
			zest_SetInstanceDrawing(billboard_layer, billboard_pipeline);
			zest_SetLayerPushConstants(billboard_layer, &billboard_push, sizeof(billboard_push_constant_t));
			DrawPlayer(this, billboard_layer);
		} else if (state == GameState_game) {
			//If the game is in the play state draw all the game billboards
			//Set the billboard drawing to the sprite texture
			zest_SetInstanceDrawing(billboard_layer, billboard_pipeline);
			zest_SetLayerPushConstants(billboard_layer, &billboard_push, sizeof(billboard_push_constant_t));
			//Draw the player and vaders
			DrawPlayer(this, billboard_layer);
			DrawVaders(this, billboard_layer, (float)tfx_rendering.timer.lerp);
			//Render all of the game particles
			if (zest_TimerUpdateWasRun(&tfx_rendering.timer)) {
				RenderEffectParticles(game_pm, this, tfx_layer);
			}
			//Set the billboard drawing back to the sprite texture (after rendering the particles with the particle texture)
			//We want to draw the vader bullets over the top of the particles so that they're easier to see
			zest_SetInstanceDrawing(billboard_layer, billboard_pipeline);
			zest_SetLayerPushConstants(billboard_layer, &billboard_push, sizeof(billboard_push_constant_t));
			DrawVaderBullets(this, billboard_layer, (float)tfx_rendering.timer.lerp);
			//Draw some text for score and wave
			zest_DrawMSDFText(font_layer, zest_ScreenWidthf(context) * .5f, zest_ScreenHeightf(context) * .95f, .5f, .5f, .3f, 0.f, "%i", score);
			zest_DrawMSDFText(font_layer, zest_ScreenWidthf(context) * .05f, zest_ScreenHeightf(context) * .95f, 0.f, .5f, .3f, 0.f, "High Score: %i", high_score);
			zest_DrawMSDFText(font_layer, zest_ScreenWidthf(context) * .95f, zest_ScreenHeightf(context) * .95f, 1.f, .5f, .3f, 0.f, "Wave: %i", (int)current_wave);
		} else if (state == GameState_game_over) {
			//Game over but keep drawing vaders until the player presses the mouse again to go back to the title screen
			zest_SetInstanceDrawing(billboard_layer, billboard_pipeline);
			zest_SetLayerPushConstants(billboard_layer, &billboard_push, sizeof(billboard_push_constant_t));
			DrawVaders(this, billboard_layer, (float)tfx_rendering.timer.lerp);
			if (zest_TimerUpdateWasRun(&tfx_rendering.timer)) {
				RenderEffectParticles(game_pm, this, tfx_layer);
			}
			zest_SetInstanceDrawing(billboard_layer, billboard_pipeline);
			zest_SetLayerPushConstants(billboard_layer, &billboard_push, sizeof(billboard_push_constant_t));
			DrawVaderBullets(this, billboard_layer, (float)tfx_rendering.timer.lerp);
			zest_DrawMSDFText(font_layer, zest_ScreenWidthf(context) * .5f, zest_ScreenHeightf(context) * .5f, .5f, .5f, 1.f, 0.f, "GAME OVER");
		}

		cache_info.draw_imgui = zest_imgui_HasGuiToDraw(&imgui);
		cache_info.draw_timeline_fx = zest_GetLayerInstanceSize(tfx_layer) > 0;
		cache_info.draw_sprites = zest_GetLayerInstanceSize(billboard_layer) > 0;
		zest_frame_graph_cache_key_t cache_key = {};
		cache_key = zest_InitialiseCacheKey(context, &cache_info, sizeof(RenderCacheInfo));

		//To ensure that the imgui buffers are updated with the latest vertex data make sure you call it
		//after zest_BeginFrame every frame.
		zest_tfx_UpdateUniformBuffer(context, &tfx_rendering);
		zest_SetSwapchainClearColor(context, 0.f, 0.f, .2f, 1.f);

		zest_frame_graph frame_graph = zest_GetCachedFrameGraph(context, &cache_key);
		if (!frame_graph) {
			if (zest_BeginFrameGraph(context, "TimelineFX Render Graph", &cache_key)) {
				zest_image particle_image = zest_GetImage(tfx_rendering.particle_texture);
				zest_image color_ramps_image = zest_GetImage(tfx_rendering.color_ramps_texture);
				zest_image sprites_image = zest_GetImage(sprite_texture);
				//---------------------------------Resources-------------------------------------------------------
				zest_resource_node tfx_write_layer = zest_AddTransientLayerResource("Write Particle Buffer", tfx_layer, false);
				zest_resource_node tfx_read_layer = zest_AddTransientLayerResource("Read Particle Buffer", tfx_layer, true);
				zest_resource_node billboard_layer_resource = zest_AddTransientLayerResource("Billboards", billboard_layer, false);
				zest_resource_node font_layer_resources = zest_AddTransientLayerResource("Fonts", font_layer, false);
				zest_ImportSwapchainResource();
				//--------------------------------------------------------------------------------------------------

				//-------------------------Game Transfer Pass-------------------------------------------------
				zest_BeginTransferPass("Upload layers"); {
					zest_ConnectOutput(tfx_write_layer);
					zest_ConnectOutput(billboard_layer_resource);
					zest_ConnectOutput(font_layer_resources);
					zest_SetPassTask(UploadGameLayerData, this);
					zest_EndPass();
				}
				//--------------------------------------------------------------------------------------------------

				//------------------------ Particles Pass -----------------------------------------------------------
				zest_BeginRenderPass("Particles Pass"); {
					zest_ConnectInput(tfx_write_layer);
					zest_ConnectInput(tfx_read_layer);
					zest_ConnectSwapChainOutput();
					zest_SetPassTask(zest_tfx_DrawParticleLayer, &tfx_rendering);
					zest_EndPass();
				}

				//------------------------ Billboards Pass -----------------------------------------------------------
				zest_BeginRenderPass("Billboards Pass"); {
					zest_ConnectInput(billboard_layer_resource);
					zest_ConnectSwapChainOutput();
					zest_SetPassTask(zest_DrawInstanceLayer, billboard_layer);
					zest_EndPass();
				}

				//------------------------ Fonts Pass ----------------------------------------------------------------
				zest_BeginRenderPass("Fonts Pass"); {
					zest_ConnectInput(font_layer_resources);
					zest_ConnectSwapChainOutput();
					zest_SetPassTask(zest_DrawInstanceLayer, font_layer);
					zest_EndPass();
				}
				//----------------------------------------------------------------------------------------------------

				//------------------------ ImGui Pass ----------------------------------------------------------------
				//If there's imgui to draw then draw it
				zest_pass_node imgui_pass = zest_imgui_BeginPass(&imgui, imgui.main_viewport); {
					if (imgui_pass) {
						zest_ConnectSwapChainOutput();
						zest_EndPass();
					}
				}
				//----------------------------------------------------------------------------------------------------

				//Compile frame graph. 
				frame_graph = zest_EndFrameGraph();
			}
		}
		zest_QueueFrameGraphForExecution(context, frame_graph);
		zest_EndFrame(context);
		if (request_graph_print > 0) {
			//You can print out the render graph for debugging purposes
			zest_PrintCompiledFrameGraph(frame_graph);
			request_graph_print--;
		}
	}

	//Render any additional viewports
	ImGui::RenderPlatformWindowsDefault(NULL, &imgui);

	if (zest_SwapchainWasRecreated(context)) {
		top_left_bound = ScreenRay(context, 0.f, 0.f, 10.f, tfx_rendering.camera.position, tfx_rendering.uniform_buffer);
		bottom_right_bound = ScreenRay(context, zest_ScreenWidthf(context), zest_ScreenHeightf(context), 10.f, tfx_rendering.camera.position, tfx_rendering.uniform_buffer);
		zest_SetLayerViewPort(font_layer, 0, 0, zest_ScreenWidth(context), zest_ScreenHeight(context), zest_ScreenWidthf(context), zest_ScreenHeightf(context));
		zest_UpdateFontTransform(&font);
		zest_SetLayerViewPort(billboard_layer, 0, 0, zest_ScreenWidth(context), zest_ScreenHeight(context), zest_ScreenWidthf(context), zest_ScreenHeightf(context));
	}
}

//Update callback called from Zest
void MainLoop(VadersGame *game) {
	zest_microsecs running_time = zest_Microsecs();
	zest_microsecs frame_time = 0;
	zest_uint frame_count = 0;

	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(game->context))) {
		zest_microsecs current_frame_time = zest_Microsecs() - running_time;
		running_time = zest_Microsecs();
		frame_time += current_frame_time;
		frame_count += 1;
		if (frame_time >= ZEST_MICROSECS_SECOND) {
			frame_time -= ZEST_MICROSECS_SECOND;
			game->last_fps = frame_count;
			frame_count = 0;
		}

		float ellapsed = (float)current_frame_time;

		glfwPollEvents();
		game->Update(ellapsed);
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main() {
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	ZEST__FLAG(create_info.flags, zest_context_init_flag_enable_vsync);

	VadersGame game = { 0 };
	tfx_InitialiseTimelineFX(tfx_GetDefaultThreadCount(), tfxMegabyte(128));

	if (!glfwInit()) {
		return 0;
	}

	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);

	//Create the device that serves all vulkan based contexts
	game.device = zest_implglfw_CreateDevice(true);

	//Create a window using GLFW
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");
	//Initialise Zest
	game.context = zest_CreateContext(game.device, &window_handles, &create_info);

	game.Init();
	MainLoop(&game);

	for (int i = 0; i != 2; ++i) {
		game.player_bullets[i].free();
		game.vaders[i].free();
		game.big_vaders[i].free();
		game.vader_bullets[i].free();
		game.power_ups[i].free();
	}

	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Destroy(&game.imgui);
	zest_DestroyDevice(game.device);
	tfx_EndTimelineFX();

	return 0;
}
#else
int main(void) {
	zest_vec3 v = zest_Vec3Set(1.f, 0.f, 0.f);
	zest_uint packed = zest_Pack8bitx3(&v);
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	create_info.log_path = ".";
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	zest_implglfw_SetCallbacks(&create_info);

	VadersGame game;
	InitialiseTimelineFX(12);

	zest_CreateContext(&create_info);
	zest_SetUserData(&game);
	zest_SetUserUpdateCallback(UpdateTfxExample);
	game.Init();

	zest_Start();

	return 0;
}
#endif
