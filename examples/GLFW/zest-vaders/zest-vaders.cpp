#include <zest.h>
#include "imgui.h"
#include "impl_glfw.h"
#include "impl_imgui.h"
#include "timelinefx.h"

using namespace tfx;

#define x_distance 0.078f
#define y_distance 0.158f
#define x_spacing 0.040f
#define y_spacing 0.058f

#define Rad90 1.5708f
#define Rad270 4.71239f

typedef unsigned int u32;

struct Player {
	tfxVec3 position;
	float rate_of_fire;
	float fire_count = 1.f;
};

struct PlayerBullet {
	tfxVec3 position;
	float speed = 20.f;
	tfxEffectID effect_index;
	bool remove = false;
};

struct VaderBullet {
	tfxVec3 captured;
	tfxVec3 position;
	tfxVec3 velocity;
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
	tfxVec3 captured;
	tfxVec3 position;
	tfxVec3 start_position;
	tfxVec3 end_position;
	tfxVec3 velocity;
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

struct VadersGame {
	tfxRandom random;
	zest_timer timer;
	zest_camera_t camera;
	zest_layer render_layer;
	zest_texture particle_texture;
	zest_texture sprite_texture;
	zest_texture imgui_font_texture;
	bool paused = false;
	Player player;
	tfxvec<PlayerBullet> player_bullets[2];
	tfxvec<Vader> vaders[2];
	tfxvec<Vader> big_vaders[2];
	tfxvec<VaderBullet> vader_bullets[2];
	tfxvec<tfxEffectID> power_ups[2];
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

	tfxVec3 top_left_bound;
	tfxVec3 bottom_right_bound;

	tfxLibrary library;
	tfxParticleManager game_pm;
	tfxParticleManager background_pm;
	tfxParticleManager title_pm;

	tfxEffectTemplate player_bullet_effect;
	tfxEffectTemplate vader_explosion_effect;
	tfxEffectTemplate player_explosion;
	tfxEffectTemplate big_explosion;
	tfxEffectTemplate background;
	tfxEffectTemplate title;
	tfxEffectTemplate laser;
	tfxEffectTemplate charge_up;
	tfxEffectTemplate weapon_power_up;
	tfxEffectTemplate got_power_up;
	tfxEffectTemplate damage;

	tfxEffectID background_index;
	tfxEffectID title_index;
	zest_font font;
	zest_imgui_layer_info imgui_layer_info;
	zest_layer font_layer;
	zest_layer billboard_layer;
	zest_descriptor_buffer uniform_buffer_3d;
	zest_descriptor_set billboard_descriptor;
	zest_descriptor_set particle_descriptor;
	zest_pipeline billboard_pipeline;

	int particle_option = 2;

	GameState state = GameState_title;
	bool wait_for_mouse_release = false;

	void Init();
	void Update(float ellapsed);
};

void DarkStyle2() {
	ImGui::GetStyle().FrameRounding = 4.0f;
	ImGui::GetStyle().GrabRounding = 4.0f;

	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(1.00f, 1.00f, 1.00f, 0.26f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_Border] = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.32f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.34f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.91f, 0.35f, 0.05f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.5f, 0.5f, 0.5f, .25f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_Separator] = ImVec4(0.00f, 0.00f, 0.00f, 0.22f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_Tab] = ImVec4(0.17f, 0.17f, 0.17f, 0.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.80f);
	colors[ImGuiCol_TabActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.86f, 0.31f, 0.02f, 0.00f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.86f, 0.31f, 0.02f, 0.68f);
	colors[ImGuiCol_DockingPreview] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.59f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.2f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.00f, 0.00f, 0.00f, 0.1f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.00f, 0.00f, 0.00f, 0.11f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

	ImGuiStyle &style = ImGui::GetStyle();
	style.WindowPadding.x = 10.f;
	style.WindowPadding.y = 10.f;
	style.FramePadding.x = 6.f;
	style.FramePadding.y = 4.f;
	style.ItemSpacing.x = 10.f;
	style.ItemSpacing.y = 6.f;
	style.ItemInnerSpacing.x = 5.f;
	style.ItemInnerSpacing.y = 5.f;
	style.WindowRounding = 0.f;
	style.FrameRounding = 4.f;
	style.ScrollbarRounding = 1.f;
	style.GrabRounding = 1.f;
	style.TabRounding = 4.f;
	style.IndentSpacing = 20.f;
	style.FrameBorderSize = 1.0f;

	style.ChildRounding = 0.f;
	style.PopupRounding = 0.f;
	style.CellPadding.x = 4.f;
	style.CellPadding.y = 2.f;
	style.TouchExtraPadding.x = 0.f;
	style.TouchExtraPadding.y = 0.f;
	style.ColumnsMinSpacing = 6.f;
	style.ScrollbarSize = 14.f;
	style.ScrollbarRounding = 1.f;
	style.GrabMinSize = 10.f;
	style.TabMinWidthForCloseButton = 0.f;
	style.DisplayWindowPadding.x = 19.f;
	style.DisplayWindowPadding.y = 19.f;
	style.DisplaySafeAreaPadding.x = 3.f;
	style.DisplaySafeAreaPadding.y = 3.f;
	style.MouseCursorScale = 1.f;
	style.WindowMinSize.x = 32.f;
	style.WindowMinSize.y = 32.f;
	style.ChildBorderSize = 1.f;

}

void UpdateUniform3d(VadersGame *game) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(game->uniform_buffer_3d);
	buffer_3d->view = zest_LookAt(game->camera.position, zest_AddVec3(game->camera.position, game->camera.front), game->camera.up);
	buffer_3d->proj = zest_Perspective(game->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.1f, 10000.f);
	buffer_3d->proj.v[1].y *= -1.f;
	buffer_3d->screen_size.x = zest_ScreenWidthf();
	buffer_3d->screen_size.y = zest_ScreenHeightf();
	buffer_3d->millisecs = 0;
}

//Before you load an effects file, you will need to define a ShapeLoader function that passes the following parameters:
//const char* filename			- this will be the filename of the image being loaded from the library. You don't have to do anything with this if you don't need to.
//ImageData	&image_data			- A struct containing data about the image. You will have to set image_data.ptr to point to the texture in your renderer for later use in the Render function that you will create to render the particles
//void *raw_image_data			- The raw data of the image which you can use to load the image into graphics memory
//int image_memory_size			- The size in bytes of the raw_image_data
//void *custom_data				- This allows you to pass through an object you can use to access whatever is necessary to load the image into graphics memory, depending on the renderer that you're using
void ShapeLoader(const char* filename, tfxImageData &image_data, void *raw_image_data, int image_memory_size, void *custom_data) {
	//Cast your custom data, this can be anything you want
	VadersGame *game = static_cast<VadersGame*>(custom_data);

	//This shape loader example uses the STB image library to load the raw bitmap (png usually) data
	zest_bitmap_t bitmap;
	zest_LoadBitmapImageMemory(&bitmap, (unsigned char*)raw_image_data, image_memory_size, 0);
	//Convert the image to RGBA which is necessary for this particular renderer
	zest_ConvertBitmapToRGBA(&bitmap, 255);
	//The editor has the option to convert an bitmap to an alpha map. I will probably change this so that it gets baked into the saved effect so you won't need to apply the filter here.
	//Alpha map is where all color channels are set to 255
	if (image_data.import_filter)
		zest_ConvertBitmapToAlpha(&bitmap);

	//Get the texture where we're storing all the particle shapes
	//You'll probably need to load the image in such a way depending on whether or not it's an animation or not
	if (image_data.animation_frames > 1) {
		//Add the spritesheet to the texture in our renderer
		float max_radius = 0;
		image_data.ptr = zest_AddTextureAnimationBitmap(game->particle_texture, &bitmap, (u32)image_data.image_size.x, (u32)image_data.image_size.y, (u32)image_data.animation_frames, &max_radius, 1);
		//Important step: you need to point the ImageData.ptr to the appropriate handle in the renderer to point to the texture of the particle shape
		//You'll need to use this in your render function to tell your renderer which texture to use to draw the particle
	}
	else {
		//Add the image to the texture in our renderer
		image_data.ptr = zest_AddTextureImageBitmap(game->particle_texture, &bitmap);
		//Important step: you need to point the ImageData.ptr to the appropriate handle in the renderer to point to the texture of the particle shape
		//You'll need to use this in your render function to tell your renderer which texture to use to draw the particle
	}
}

tfxVec3 ScreenRay(float x, float y, float depth_offset, zest_vec3 &camera_position, zest_descriptor_buffer buffer) {
	zest_uniform_buffer_data_t *buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(&camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

void UpdateGotPowerUpEffect(tfxParticleManager *pm, tfxEffectID effect_index) {
	VadersGame *game = static_cast<VadersGame*>(GetEffectUserData(pm, effect_index));
	SetEffectPosition(pm, effect_index, game->player.position);
}

void VadersGame::Init() {
	//Renderer specific - initialise the texture
	float max_radius = 0;
	sprite_texture = zest_CreateTexturePacked("Sprite Texture", zest_texture_format_rgba);
	player_image = zest_AddTextureImageFile(sprite_texture, "examples/assets/vaders/player.png");
	vader_image1 = zest_AddTextureImageFile(sprite_texture, "examples/assets/vaders/vader.png");
	vader_image2 = zest_AddTextureImageFile(sprite_texture, "examples/assets/vaders/vader2.png");
	vader_image3 = zest_AddTextureImageFile(sprite_texture, "examples/assets/vaders/vader3.png");
	big_vader_image = zest_AddTextureImageFile(sprite_texture, "examples/assets/vaders/big_vader.png");
	vader_bullet_image = zest_AddTextureAnimationFile(sprite_texture, "examples/assets/vaders/vader_bullet_64_64_16.png", 16, 16, 64, &max_radius, 1);
	vader_bullet_glow_image = zest_AddTextureImageFile(sprite_texture, "examples/assets/vaders/flare4.png");
	zest_ProcessTextureImages(sprite_texture);
	uniform_buffer_3d = zest_CreateUniformBuffer("3d uniform", sizeof(zest_uniform_buffer_data_t));
	billboard_descriptor = zest_CreateTextureSpriteDescriptorSets(sprite_texture, "3d descriptor", "3d uniform");
	zest_RefreshTextureDescriptors(sprite_texture);
	billboard_pipeline = zest_Pipeline("pipeline_billboard");

	random.ReSeed();

	int shape_count = GetShapeCountInLibrary("examples/assets/vaders/vadereffects.tfx");
	particle_texture = zest_CreateTexture("Particle Texture", zest_texture_storage_type_packed, 0, zest_texture_format_rgba, shape_count);
	//Load the effects library and pass the shape loader function pointer that you created earlier. Also pass this pointer to point to this object to give the shapeloader access to the texture we're loading the particle images into
	LoadEffectLibraryPackage("examples/assets/vaders/vadereffects.tfx", library, ShapeLoader, this);
	//Renderer specific
	zest_ProcessTextureImages(particle_texture);
	particle_descriptor = zest_CreateTextureSpriteDescriptorSets(particle_texture, "3d descriptor", "3d uniform");
	zest_RefreshTextureDescriptors(particle_texture);

	//Application specific, set up a timer for the update loop
	timer = zest_CreateTimer(60);

	//Make sure that timelinefx udates effects at the same frequency as your update loop.
	tfx::SetUpdateFrequency(60);

	camera = zest_CreateCamera();
	zest_CameraSetFoV(&camera, 60.f);

	UpdateUniform3d(this);

	top_left_bound = ScreenRay(0.f, 0.f, 10.f, camera.position, uniform_buffer_3d);
	bottom_right_bound = ScreenRay(zest_ScreenWidthf(), zest_ScreenHeightf(), 10.f, camera.position, uniform_buffer_3d);

	tfxU32 layer_max_values[tfxLAYERS];
	memset(layer_max_values, 0, 16);
	layer_max_values[0] = 5000;
	layer_max_values[1] = 2500;
	layer_max_values[2] = 2500;
	layer_max_values[3] = 2500;
	//Initialise a particle manager. This manages effects, emitters and the particles that they spawn
	//Depending on your needs you can use as many particle managers as you need.
	//pm.InitFor3d(layer_max_values, 100, tfxParticleManagerMode_ordered_by_depth_guaranteed, 512);
	InitParticleManagerFor3d(&background_pm, &library, layer_max_values, 100, tfxParticleManagerMode_ordered_by_depth, true, false, 2048);
	InitParticleManagerFor3d(&game_pm, &library, layer_max_values, 1000, tfxParticleManagerMode_unordered, true, false, 2048);
	InitParticleManagerFor3d(&title_pm, &library, layer_max_values, 100, tfxParticleManagerMode_unordered, true, false, 2048);

	font = zest_LoadMSDFFont("examples/assets/SourceSansPro-Regular.zft");

	//Renderer specific
	zest_SetDrawCommandsClsColor(zest_GetCommandQueueDrawCommands("Default Draw Commands"), 0.f, 0.f, .2f, 1.f);

	//Player setup
	player.rate_of_fire = 4.f * tfxUPDATE_TIME;
	high_score = 0;

	//Effect templates
	assert(PrepareEffectTemplate(library, "Player Bullet", player_bullet_effect));
	assert(PrepareEffectTemplate(library, "Vader Explosion", vader_explosion_effect));
	assert(PrepareEffectTemplate(library, "Big Explosion", big_explosion));
	assert(PrepareEffectTemplate(library, "Player Explosion", player_explosion));
	assert(PrepareEffectTemplate(library, "Background", background));
	assert(PrepareEffectTemplate(library, "Title", title));
	assert(PrepareEffectTemplate(library, "Laser", laser));
	assert(PrepareEffectTemplate(library, "Charge Up", charge_up));
	assert(PrepareEffectTemplate(library, "Got Power Up", got_power_up));
	assert(PrepareEffectTemplate(library, "Power Up", weapon_power_up));
	assert(PrepareEffectTemplate(library, "Damage", damage));

	got_power_up.SetUserData(this);
	got_power_up.SetEffectUpdateCallback(UpdateGotPowerUpEffect);

	background_index = AddEffectToParticleManager(&background_pm, background);
	zest_vec3 position = zest_AddVec3(zest_ScaleVec3(&camera.front, 12.f), camera.position);
	SetEffectPosition(&background_pm, background_index, { position.x, position.y, position.z });
	title_index = AddEffectToParticleManager(&title_pm, title);
	SetEffectPosition(&title_pm, title_index, ScreenRay(zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .25f, 4.f, camera.position, uniform_buffer_3d));

	zest_imgui_Initialise(&imgui_layer_info);

	DarkStyle2();

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

	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			billboard_layer = zest_NewBuiltinLayerSetup("Billboards", zest_builtin_layer_billboards);
			font_layer = zest_NewBuiltinLayerSetup("Fonts", zest_builtin_layer_fonts);
			zest_imgui_CreateLayer(&imgui_layer_info);
		}
		zest_FinishQueueSetup();
	}
	zest_TimerReset(timer);
	zest_OutputMemoryUsage();
}

float EaseOutQuad(float t) {
	return t * (2 - t);
}

float EaseInOutQuad(float t) {
	return t < 0.5f ? 2 * t * t : t * (4 - 2 * t) - 1;
}

tfxVec3 RotatePoint(tfxVec3 p, tfxVec3 center, float angle) {
	float radians = tfxRadians(angle);  // convert angle to radians
	float cosa = cosf(radians);
	float sina = sinf(radians);
	float newz = center.z + (p.z - center.z) * cosa - (p.y - center.y) * sina;
	float newy = center.y + (p.z - center.z) * sina + (p.y - center.y) * cosa;
	return { center.x, newz, newy };
}

bool IsLineCircleCollision(tfxVec3 line_start, tfxVec3 line_end, tfxVec3 circleCenter, float radius) {
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
			vader.position = vader.captured = ScreenRay(game->spacing_x * x + game->margin_x, game->spacing_y * y + game->margin_y, 10.f, game->camera.position, game->uniform_buffer_3d);
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
	vader.end_position = ScreenRay(zest_ScreenWidthf() * .5f, game->margin_y * .45f, 10.f, game->camera.position, game->uniform_buffer_3d);
	vader.position = vader.captured = vader.start_position = ScreenRay(zest_ScreenWidthf() * .5f, -game->margin_y, 10.f, game->camera.position, game->uniform_buffer_3d);;
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
	for (auto &vader : game->vaders[game->current_buffer]) {
		//Naive bullet collision
		bool hit = false;
		for (auto &bullet : game->player_bullets[game->current_buffer]) {
			if (GetDistance(bullet.position.z, bullet.position.y, vader.position.z, vader.position.y) < 0.3f) {
				hit = true;
				bullet.remove = true;
				tfxEffectID effect_index = AddEffectToParticleManager(&game->game_pm, game->vader_explosion_effect);
				SetEffectPosition(&game->game_pm, effect_index, vader.position);
				SetEffectOveralScale(&game->game_pm, effect_index, 2.5f);
				game->score += 150;
				game->high_score = tfxMax(game->score, game->high_score);
				break;
			}
		}
		if (hit) continue;
		if (vader.position.y < game->bottom_right_bound.y) continue;
		float wave = game->current_wave - 1.f;
		if (vader.turning == 1) {
			vader.direction += tfxRadians(180.f + (wave * 10.f)) * tfxUPDATE_TIME;
			if (vader.direction >= Rad270) {
				vader.direction = Rad270;
				vader.turning = 0;
			}
		}
		else if (vader.turning == 2) {
			vader.direction -= tfxRadians(180.f + (wave * 10.f)) * tfxUPDATE_TIME;
			if (vader.direction <= Rad90) {
				vader.direction = Rad90;
				vader.turning = 0;
			}
		}
		vader.velocity.z = vader.speed * sinf(vader.direction);
		vader.velocity.y = vader.speed * cosf(vader.direction);
		vader.captured = vader.position;
		vader.position += vader.velocity * tfxUPDATE_TIME;
		if (vader.position.z < -8.f && vader.direction == Rad270) {
			vader.turning = 2;
		}
		else if (vader.position.z > 8.f && vader.direction == Rad90) {
			vader.turning = 1;
		}
		if (game->state != GameState_game_over && game->random.Range(1.f) <= vader.chance_to_shoot) {
			VaderBullet bullet;
			bullet.position = bullet.captured = vader.position;
			bullet.velocity.y = (-4.f + (wave * 0.1f)) * tfxUPDATE_TIME;
			bullet.frame = 0;
			game->vader_bullets[game->current_buffer].push_back(bullet);
		}
		game->vaders[next_buffer].push_back(vader);
	}

	for (auto &vader : game->big_vaders[game->current_buffer]) {
		bool dead = false;
		for (auto &bullet : game->player_bullets[game->current_buffer]) {
			if (GetDistance(bullet.position.z, bullet.position.y, vader.position.z, vader.position.y) < 0.3f) {
				bullet.remove = true;
				vader.health--;
				tfxEffectID damage_index = AddEffectToParticleManager(&game->game_pm, game->damage);
				SetEffectPosition(&game->game_pm, damage_index, bullet.position);
				if (vader.health == 0) {
					dead = true;
					tfxEffectID effect_index = AddEffectToParticleManager(&game->game_pm, game->big_explosion);
					SetEffectPosition(&game->game_pm, effect_index, vader.position);
					SetEffectOveralScale(&game->game_pm, effect_index, 2.5f);
					game->score += 500;
					game->high_score = tfxMax(game->score, game->high_score);
					if (vader.flags & VaderFlags_firing_laser) {
						SoftExpireEffect(&game->game_pm, vader.laser);
					}
					tfxEffectID power_up_index = AddEffectToParticleManager(&game->game_pm, game->weapon_power_up);
					SetEffectPosition(&game->game_pm, power_up_index, vader.position);
					SetEffectOveralScale(&game->game_pm, power_up_index, 3.f);
					game->power_ups[game->current_buffer].push_back(power_up_index);
					break;
				}
			}
		}
		if (dead) continue;
		vader.time += tfxUPDATE_TIME;
		vader.captured = vader.position;
		if (!(vader.flags & VaderFlags_in_position)) {
			vader.position.y = EaseOutQuad(tfxMin(vader.time * .5f, 1.f)) * (vader.end_position.y - vader.start_position.y) + vader.start_position.y;
		}
		if (vader.time > 2.f && !(vader.flags & VaderFlags_charging_up) && !(vader.flags & VaderFlags_firing_laser)) {
			tfxEffectID effect_index = AddEffectToParticleManager(&game->game_pm, game->charge_up);
			tfxVec3 laser_offset = RotatePoint({ 0.f, .4f, 0.f }, { 0.f, 0.f, 0.f }, vader.angle - 90.f);
			SetEffectPosition(&game->game_pm, effect_index, vader.position + laser_offset);
			vader.flags |= VaderFlags_charging_up;
			vader.flags |= VaderFlags_in_position;
		}
		else if (vader.time > 3.f && vader.flags & VaderFlags_charging_up) {
			vader.laser = AddEffectToParticleManager(&game->game_pm, game->laser);
			tfxVec3 laser_offset = RotatePoint({ 0.f, .4f, 0.f }, { 0.f, 0.f, 0.f }, vader.angle - 90.f);
			SetEffectPosition(&game->game_pm, vader.laser, vader.position + laser_offset);
			SetEffectPitch(&game->game_pm, vader.laser, tfxRadians(vader.angle));
			SetEffectOveralScale(&game->game_pm, vader.laser, 2.f);
			SetEffectHeightMultiplier(&game->game_pm, vader.laser, 3.f);
			vader.flags |= VaderFlags_firing_laser;
			vader.flags &= ~VaderFlags_charging_up;
		}
		else if (vader.flags & VaderFlags_firing_laser) {
			float time = (vader.time - 3.f) * .25f;
			vader.angle = EaseInOutQuad(tfxMin(time * .5f, 1.f)) * (vader.end_angle - vader.start_angle) + vader.start_angle;
			tfxVec3 laser_offset = RotatePoint({ 0.f, .4f, 0.f }, { 0.f, 0.f, 0.f }, vader.angle - 90.f);
			SetEffectPosition(&game->game_pm, vader.laser, vader.position + laser_offset);
			SetEffectPitch(&game->game_pm, vader.laser, tfxRadians(vader.angle));
			if (vader.angle == vader.end_angle) {
				vader.time = 0;
				vader.end_angle = vader.start_angle;
				vader.start_angle = vader.angle;
				SoftExpireEffect(&game->game_pm, vader.laser);
				vader.flags &= ~VaderFlags_firing_laser;
			}
			else {
				tfxVec3 laser_normal = NormalizeVec(laser_offset);
				if (game->state != GameState_game_over && IsLineCircleCollision(vader.position + laser_offset, vader.position + laser_offset + (laser_normal * 20.f), game->player.position, .3f)) {
					int index = AddEffectToParticleManager(&game->game_pm, game->player_explosion);
					SetEffectPosition(&game->game_pm, index, game->player.position);
					SetEffectOveralScale(&game->game_pm, index, 1.5f);
					game->state = GameState_game_over;
				}
			}
		}
		game->big_vaders[next_buffer].push_back(vader);
	}
	if (game->big_vaders[next_buffer].size()) {
		game->countdown_to_big_vader = game->random.Range(15.f, 35.f);
	}
}

void UpdatePowerUps(VadersGame *game) {
	int next_buffer = !game->current_buffer;
	game->power_ups[next_buffer].clear();
	for (auto &power_up : game->power_ups[game->current_buffer]) {
		tfxVec3 position = GetEffectPosition(&game->game_pm, power_up);
		if (GetDistance(position.z, position.y, game->player.position.z, game->player.position.y) < 0.3f) {
			HardExpireEffect(&game->game_pm, power_up);
			tfxEffectID effect_index = AddEffectToParticleManager(&game->game_pm, game->got_power_up);
			SetEffectPosition(&game->game_pm, effect_index, game->player.position);
			SetEffectOveralScale(&game->game_pm, effect_index, 2.5f);
			game->player.rate_of_fire += 1 * tfxUPDATE_TIME;
			continue;
		}
		MoveEffect(&game->game_pm, power_up, 0.f, -1.f * tfxUPDATE_TIME, 0.f);
		game->power_ups[next_buffer].push_back(power_up);
	}
}

void UpdatePlayerPosition(VadersGame *game, Player *player) {
	player->position = ScreenRay(ZestApp->mouse_x, ZestApp->mouse_y, 10.f, game->camera.position, game->uniform_buffer_3d);
}

void UpdatePlayer(VadersGame *game, Player *player) {
	if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		player->fire_count += player->rate_of_fire;
		if (player->fire_count >= 1.f) {
			player->fire_count = 0;
			PlayerBullet new_bullet;
			new_bullet.effect_index = AddEffectToParticleManager(&game->game_pm, game->player_bullet_effect);
			if (new_bullet.effect_index != tfxINVALID) {
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
	tfxVec3 top_left = ScreenRay(0.f, 0.f, 10.f, game->camera.position, game->uniform_buffer_3d);
	for (auto &bullet : game->player_bullets[game->current_buffer]) {
		if (bullet.remove) {
			SoftExpireEffect(&game->game_pm, bullet.effect_index);
			continue;
		}
		bullet.position.y += bullet.speed * tfxUPDATE_TIME;
		if (bullet.position.y > top_left.y + 1.f) {
			SoftExpireEffect(&game->game_pm, bullet.effect_index);
		}
		else {
			SetEffectPosition(&game->game_pm, bullet.effect_index, bullet.position);
			game->player_bullets[next_buffer].push_back(bullet);
		}
	}
}

void UpdateVaderBullets(VadersGame *game) {
	int next_buffer = !game->current_buffer;
	game->vader_bullets[next_buffer].clear();
	for (auto &bullet : game->vader_bullets[game->current_buffer]) {
		if (game->state != GameState_game_over && GetDistance(bullet.position.z, bullet.position.y, game->player.position.z, game->player.position.y) < 0.3f) {
			int index = AddEffectToParticleManager(&game->game_pm, game->player_explosion);
			SetEffectPosition(&game->game_pm, index, bullet.position);
			SetEffectOveralScale(&game->game_pm, index, 1.5f);
			game->state = GameState_game_over;
			continue;
		}
		if (bullet.remove) {
			continue;
		}
		bullet.captured = bullet.position;
		bullet.position += bullet.velocity;
		bullet.frame += 60.f * tfxUPDATE_TIME;
		if (bullet.position.y < game->top_left_bound.y + 1.f &&
			bullet.position.z > game->top_left_bound.z - 1.f &&
			bullet.position.y > game->bottom_right_bound.y - 1.f &&
			bullet.position.z < game->bottom_right_bound.z + 1.f) {
			game->vader_bullets[next_buffer].push_back(bullet);
		}
	}
}

void SetParticleOption(VadersGame *game) {
	if (game->particle_option == 0) {
		game->background.ScaleGlobalMultiplier(tfxGlobal_amount, .25f);
		SoftExpireEffect(&game->background_pm, game->background_index);
		HardExpireEffect(&game->title_pm, game->title_index);
		game->player_bullet_effect.DisableEmitter("Player Bullet/Flare");
		game->title.DisableEmitter("Title/Flare");
		game->big_explosion.SetSingleSpawnAmount("Big Explosion/Ring Stretch", 100);
		game->big_explosion.SetSingleSpawnAmount("Big Explosion/Ring Stretch.1", 100);
		game->big_explosion.ScaleEmitterGraph("Big Explosion/Cloud Burst Expand", tfxBase_amount, 0.25f);
		game->player_explosion.SetSingleSpawnAmount("Player Explosion/Ring Stretch", 100);
		game->player_explosion.SetSingleSpawnAmount("Player Explosion/Ring Stretch.1", 100);
		game->player_explosion.ScaleEmitterGraph("Player Explosion/Cloud Burst", tfxBase_amount, 0.25f);
		game->got_power_up.SetSingleSpawnAmount("Got Power Up/Delayed Ellipse", 100);
		game->vader_explosion_effect.ScaleGlobalMultiplier(tfxGlobal_amount, .25f);
		game->laser.ScaleGlobalMultiplier(tfxGlobal_amount, .5f);
		game->laser.DisableEmitter("Laser/Flare");

		game->background_index = AddEffectToParticleManager(&game->background_pm, game->background);
		zest_vec3 position = zest_AddVec3(zest_ScaleVec3(&game->camera.front, 12.f), game->camera.position);
		SetEffectPosition(&game->background_pm, game->background_index, { position.x, position.y, position.z });
		game->title_index = AddEffectToParticleManager(&game->title_pm, game->title);
		SetEffectPosition(&game->title_pm, game->title_index, ScreenRay(zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .25f, 4.f, game->camera.position, game->uniform_buffer_3d));
	}
	else if (game->particle_option == 1) {
		game->background.ScaleGlobalMultiplier(tfxGlobal_amount, .65f);
		SoftExpireEffect(&game->background_pm, game->background_index);
		HardExpireEffect(&game->title_pm, game->title_index);
		game->player_bullet_effect.EnableEmitter("Player Bullet/Flare");
		game->player_bullet_effect.ScaleEmitterGraph("Player Bullet/Flare", tfxBase_amount, 0.5f);
		game->title.DisableEmitter("Title/Flare");
		game->big_explosion.SetSingleSpawnAmount("Big Explosion/Ring Stretch", 200);
		game->big_explosion.SetSingleSpawnAmount("Big Explosion/Ring Stretch.1", 250);
		game->big_explosion.ScaleEmitterGraph("Big Explosion/Cloud Burst Expand", tfxBase_amount, 0.65f);
		game->player_explosion.SetSingleSpawnAmount("Player Explosion/Ring Stretch", 200);
		game->player_explosion.SetSingleSpawnAmount("Player Explosion/Ring Stretch.1", 250);
		game->player_explosion.ScaleEmitterGraph("Player Explosion/Cloud Burst", tfxBase_amount, 0.65f);
		game->vader_explosion_effect.ScaleGlobalMultiplier(tfxGlobal_amount, .65f);
		game->got_power_up.SetSingleSpawnAmount("Got Power Up/Delayed Ellipse", 250);
		game->laser.ScaleGlobalMultiplier(tfxGlobal_amount, 1.f);
		game->laser.DisableEmitter("Laser/Flare");

		game->background_index = AddEffectToParticleManager(&game->background_pm, game->background);
		zest_vec3 position = zest_AddVec3(zest_ScaleVec3(&game->camera.front, 12.f), game->camera.position);
		SetEffectPosition(&game->background_pm, game->background_index, { position.x, position.y, position.z });
		game->title_index = AddEffectToParticleManager(&game->title_pm, game->title);
		SetEffectPosition(&game->title_pm, game->title_index, ScreenRay(zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .25f, 4.f, game->camera.position, game->uniform_buffer_3d));
	}
	else if (game->particle_option == 2) {
		game->background.ScaleGlobalMultiplier(tfxGlobal_amount, 1.f);
		SoftExpireEffect(&game->background_pm, game->background_index);
		HardExpireEffect(&game->title_pm, game->title_index);
		game->player_bullet_effect.EnableEmitter("Player Bullet/Flare");
		game->player_bullet_effect.ScaleEmitterGraph("Player Bullet/Flare", tfxBase_amount, 1.f);
		game->title.EnableEmitter("Title/Flare");
		game->big_explosion.SetSingleSpawnAmount("Big Explosion/Ring Stretch", 400);
		game->big_explosion.SetSingleSpawnAmount("Big Explosion/Ring Stretch.1", 500);
		game->big_explosion.ScaleEmitterGraph("Big Explosion/Cloud Burst Expand", tfxBase_amount, 1.f);
		game->player_explosion.SetSingleSpawnAmount("Player Explosion/Ring Stretch", 400);
		game->player_explosion.SetSingleSpawnAmount("Player Explosion/Ring Stretch.1", 500);
		game->player_explosion.ScaleEmitterGraph("Player Explosion/Cloud Burst", tfxBase_amount, 1.f);
		game->vader_explosion_effect.ScaleGlobalMultiplier(tfxGlobal_amount, 1.f);
		game->got_power_up.SetSingleSpawnAmount("Got Power Up/Delayed Ellipse", 566);
		game->laser.ScaleGlobalMultiplier(tfxGlobal_amount, 1.f);
		game->laser.EnableEmitter("Laser/Flare");

		game->background_index = AddEffectToParticleManager(&game->background_pm, game->background);
		zest_vec3 position = zest_AddVec3(zest_ScaleVec3(&game->camera.front, 12.f), game->camera.position);
		SetEffectPosition(&game->background_pm, game->background_index, { position.x, position.y, position.z });
		game->title_index = AddEffectToParticleManager(&game->title_pm, game->title);
		SetEffectPosition(&game->title_pm, game->title_index, ScreenRay(zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .25f, 4.f, game->camera.position, game->uniform_buffer_3d));
	}
}

void BuildUI(VadersGame *game) {
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Effects");
	ImGui::Text("FPS: %i", ZestApp->last_fps);
	ImGui::Text("Game Particles: %i", game->game_pm.ParticleCount());
	ImGui::Text("Background Particles: %i", game->background_pm.ParticleCount());
	ImGui::Text("Title Particles: %i", game->title_pm.ParticleCount());
	ImGui::Text("Effects: %i", game->game_pm.GetEffectBuffer()->size());
	ImGui::Text("Emitters: %i", game->game_pm.GetEmitterBuffer()->size());
	ImGui::Text("Free Emitters: %i", game->game_pm.free_emitters.size());
	ImGui::Text("Position: %f, %f, %f", game->player.position.x, game->player.position.y, game->player.position.z);
	static bool filtering = false;
	ImGui::Checkbox("Texture Filtering", &filtering);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		zest_SetTextureUseFiltering(game->particle_texture, filtering);
		zest_ScheduleTextureReprocess(game->particle_texture);
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
	zest_imgui_CopyBuffers(game->imgui_layer_info.mesh_layer);
}

void DrawPlayer(VadersGame *game) {
	zest_SetLayerColor(game->billboard_layer, 255, 255, 255, 255);
	zest_SetLayerIntensity(game->billboard_layer, 1.f);
	zest_DrawBillboard(game->billboard_layer, game->player_image, &game->player.position.x, 0.f, 1.f, 1.f);
}

void DrawVaders(VadersGame *game, float lerp) {
	zest_SetLayerColor(game->billboard_layer, 255, 255, 255, 255);
	zest_SetLayerIntensity(game->billboard_layer, 1.f);
	for (auto &vader : game->vaders[game->current_buffer]) {
		tfxVec3 tween = Tween(lerp, vader.position, vader.captured);
		zest_DrawBillboard(game->billboard_layer, vader.image, &tween.x, tfxRadians(vader.angle), 0.5f, 0.5f);
	}
	for (auto &vader : game->big_vaders[game->current_buffer]) {
		tfxVec3 tween = Tween(lerp, vader.position, vader.captured);
		zest_DrawBillboard(game->billboard_layer, vader.image, &tween.x, tfxRadians(vader.angle), 1.f, 1.f);
	}
}

void DrawVaderBullets(VadersGame *game, float lerp) {
	for (auto &bullet : game->vader_bullets[game->current_buffer]) {
		int frame_offset = (int)bullet.frame % 64;
		tfxVec3 tween = Tween(lerp, bullet.position, bullet.captured);
		zest_SetLayerColor(game->billboard_layer, 255, 255, 255, 0);
		zest_SetLayerIntensity(game->billboard_layer, 1.2f);
		zest_DrawBillboard(game->billboard_layer, game->vader_bullet_image + frame_offset, &tween.x, 0.f, 0.25f, 0.25f);
		zest_SetLayerColor(game->billboard_layer, 255, 128, 64, 0);
		zest_SetLayerIntensity(game->billboard_layer, .5f);
		zest_DrawBillboard(game->billboard_layer, game->vader_bullet_glow_image, &tween.x, 0.f, 0.65f, 0.65f);
	}
}

void RenderParticles3d(tfxParticleManager &pm, float tween, VadersGame *game) {
	//Renderer specific, get the layer that we will draw on (there's only one layer in this example)
	tfxWideFloat lerp = tfxWideSetSingle(tween);
	zest_SetBillboardDrawing(game->billboard_layer, game->particle_texture, game->particle_descriptor, game->billboard_pipeline);
	for (unsigned int layer = 0; layer != tfxLAYERS; ++layer) {
		tfxSpriteSoA &sprites = pm.sprites[pm.current_sprite_buffer][layer];
		for (int i = 0; i != pm.sprite_buffer[pm.current_sprite_buffer][layer].current_size; ++i) {
			zest_SetLayerColor(game->billboard_layer, sprites.color[i].r, sprites.color[i].g, sprites.color[i].b, sprites.color[i].a);
			//const float &captured_intensity = pm.GetCapturedSprite3dIntensity(layer, sprites.captured_index[i]);
			//float lerped_intensity = Interpolatef(tween, captured_intensity, sprites.intensity[i]);
			zest_SetLayerIntensity(game->billboard_layer, sprites.intensity[i]);
			zest_image image = (zest_image)pm.library->emitter_properties.image[sprites.property_indexes[i] & 0x0000FFFF]->ptr;
			tfxVec2 handle = pm.library->emitter_properties.image_handle[sprites.property_indexes[i] & 0x0000FFFF];
			const tfxSpriteTransform3d &captured = pm.GetCapturedSprite3dTransform(layer, sprites.captured_index[i]);
			tfxWideLerpTransformResult lerped = InterpolateSpriteTransform(lerp, sprites.transform_3d[i], captured);
			zest_DrawBillboardComplex(game->billboard_layer, image + ((sprites.property_indexes[i] & 0x00FF0000) >> 16),
				lerped.position,
				sprites.alignment[i],
				lerped.rotations,
				&handle.x,
				sprites.stretch[i],
				(sprites.property_indexes[i] & 0xFF000000) >> 24,
				lerped.scale[0], lerped.scale[1]);
		}
	}
}

void ResetGame(VadersGame *game) {
	ClearParticleManager(&game->game_pm);
	game->vaders[game->current_buffer].clear();
	game->big_vaders[game->current_buffer].clear();
	game->player_bullets[game->current_buffer].clear();
	game->vader_bullets[game->current_buffer].clear();
	game->power_ups[game->current_buffer].clear();
	game->player.rate_of_fire = 4.f * tfxUPDATE_TIME;
	game->score = 0;
	game->current_wave = 0;
	game->countdown_to_big_vader = game->random.Range(15.f, 35.f);
}

void VadersGame::Update(float ellapsed) {
	//Application specific update loop
	zest_TimerAccumulate(timer);

	//Renderer specific
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);

	if (ImGui::IsKeyReleased(ImGuiKey_Space)) {
		paused = !paused;
	}

	UpdatePlayerPosition(this, &player);
	UpdateUniform3d(this);
	zest_Update2dUniformBuffer();

	while (zest_TimerDoUpdate(timer)) {

		if (state == GameState_title) {
			UpdateParticleManager(&background_pm, tfxFRAME_LENGTH);
			UpdateParticleManager(&title_pm, tfxFRAME_LENGTH);
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
				UpdateParticleManager(&game_pm, tfxFRAME_LENGTH);
				UpdateParticleManager(&background_pm, tfxFRAME_LENGTH);
				noise_offset += 1.f * tfxUPDATE_TIME;
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
					countdown_to_big_vader -= tfxUPDATE_TIME;
				}
			}
		}
		else if (state == GameState_game_over) {
			UpdateParticleManager(&game_pm, tfxFRAME_LENGTH);
			UpdateParticleManager(&background_pm, tfxFRAME_LENGTH);
			noise_offset += 1.f * tfxUPDATE_TIME;
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
		BuildUI(this);

		zest_TimerUnAccumulate(timer);
	}

	zest_TimerSet(timer);

	zest_SetMSDFFontDrawing(font_layer, font, font->descriptor_set, font->pipeline);
	RenderParticles3d(background_pm, (float)timer->lerp, this);
	if (state == GameState_title) {
		RenderParticles3d(title_pm, (float)timer->lerp, this);
		zest_DrawMSDFText(font_layer, "Press Button to Start", zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .5f, .5f, .5f, 30.f, 0.f, 1);
	}
	else if (state == GameState_game) {
		zest_SetBillboardDrawing(billboard_layer, sprite_texture, billboard_descriptor, billboard_pipeline);
		DrawPlayer(this);
		DrawVaders(this, (float)timer->lerp);
		RenderParticles3d(game_pm, (float)timer->lerp, this);
		zest_SetBillboardDrawing(billboard_layer, sprite_texture, billboard_descriptor, billboard_pipeline);
		DrawVaderBullets(this, (float)timer->lerp);
		tfxStr16 score_text;
		tfxStr32 high_score_text = "High Score: ";
		tfxStr16 wave_text = "Wave: ";
		score_text.Appendf("%i", score);
		high_score_text.Appendf("%i", high_score);
		wave_text.Appendf("%i", (int)current_wave);
		zest_DrawMSDFText(font_layer, score_text.c_str(), zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .95f, .5f, .5f, 20.f, 0.f, 1);
		zest_DrawMSDFText(font_layer, high_score_text.c_str(), zest_ScreenWidthf() * .05f, zest_ScreenHeightf() * .95f, 0.f, .5f, 20.f, 0.f, 1);
		zest_DrawMSDFText(font_layer, wave_text.c_str(), zest_ScreenWidthf() * .95f, zest_ScreenHeightf() * .95f, 1.f, .5f, 20.f, 0.f, 1);
	}
	else if (state == GameState_game_over) {
		zest_SetBillboardDrawing(billboard_layer, sprite_texture, billboard_descriptor, billboard_pipeline);
		DrawVaders(this, (float)timer->lerp);
		RenderParticles3d(game_pm, (float)timer->lerp, this);
		zest_SetBillboardDrawing(billboard_layer, sprite_texture, billboard_descriptor, billboard_pipeline);
		DrawVaderBullets(this, (float)timer->lerp);
		zest_DrawMSDFText(font_layer, "GAME OVER", zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .5f, .5f, .5f, 60.f, 0.f, 1);
	}
}

//Application specific, this just sets the function to call each render update
void UpdateTfxExample(zest_microsecs ellapsed, void *data) {
	VadersGame *game = static_cast<VadersGame*>(data);
	game->Update((float)ellapsed);
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
//int main() {
	zest_vec3 v = zest_Vec3Set(1.f, 0.f, 0.f);
	zest_uint packed = zest_Pack8bitx3(&v);
	zest_create_info_t create_info = zest_CreateInfo();
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
