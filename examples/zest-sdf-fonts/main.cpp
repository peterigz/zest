#include "header.h"
#include "imgui_internal.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

void InitImGuiApp(ImGuiApp *app) {

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(zest_ScreenWidthf(), zest_ScreenHeightf());
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	int upload_size = width * height * 4 * sizeof(char);

	zest_bitmap_t font_bitmap = zest_CreateBitmapFromRawBuffer("font_bitmap", pixels, upload_size, width, height, 4);
	app->imgui_font_texture = zest_CreateTexture("imgui_font", zest_texture_storage_type_single, zest_texture_flag_none, zest_texture_format_rgba, 10);
	zest_image font_image = zest_AddTextureImageBitmap(app->imgui_font_texture, &font_bitmap);
	zest_ProcessTextureImages(app->imgui_font_texture);
	io.Fonts->SetTexID(font_image);
	ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)ZestApp->window->window_handle, true);

	app->imgui_layer_info.pipeline_index = zest_PipelineIndex("pipeline_imgui");
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			app->imgui_layer_info.mesh_layer_index = zest_NewMeshLayer("imgui mesh layer", sizeof(ImDrawVert));
			zest_ContextDrawRoutine()->draw_callback = zest_imgui_DrawLayer;
			zest_ContextDrawRoutine()->data = &app->imgui_layer_info;
		}
		zest_FinishQueueSetup();
	}

	RasteriseFont("fonts/Lato-Regular.ttf", app);

	zest_pipeline_set_t *sprite_pipeline = zest_PipelineByName("pipeline_2d_sprites_alpha");
	zest_pipeline_template_create_info_t sdf_pipeline_template = sprite_pipeline->create_info;
	app->sdf_pipeline_index = zest_AddPipeline("sdf_fonts");
	zest_pipeline_set_t *sdf_pipeline = zest_Pipeline(app->sdf_pipeline_index);
	sdf_pipeline_template.vertShaderFile = "spv/sdf.spv";
	sdf_pipeline_template.fragShaderFile = "spv/sdf.spv";
	zest_MakePipelineTemplate(sdf_pipeline, zest_GetStandardRenderPass(), &sdf_pipeline_template);
	zest_BuildPipeline(sdf_pipeline);

}

void RasteriseFont(const char *name, ImGuiApp *app) {
	app->font_buffer = (unsigned char*)zest_ReadEntireFile(name, 0);
	stbtt_fontinfo font_info;
	stbtt_InitFont(&font_info, app->font_buffer, stbtt_GetFontOffsetForIndex(app->font_buffer, 0));
	float scale = stbtt_ScaleForPixelHeight(&font_info, 32);
	int padding = 5;
	unsigned char onedge_value = 180;
	float pixel_dist_scale = 180.f / 5.0f;
	int width, height, xoff, yoff;

	unsigned char *bitmap = stbtt_GetCodepointSDF(&font_info, scale, 'Z', padding, onedge_value, pixel_dist_scale, &width, &height, &xoff, &yoff);
	zest_bitmap_t font_bitmap = zest_CreateBitmapFromRawBuffer("Font Glyph", bitmap, width * height, width, height, 1);

	app->glyph_texture = zest_CreateTextureSingle("Glyph", zest_texture_format_alpha);
	zest_SetUseFiltering(app->glyph_texture, ZEST_FALSE);
	app->glyph_image = zest_AddTextureImageBitmap(app->glyph_texture, &font_bitmap);
	zest_ProcessTextureImages(app->glyph_texture);
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	//Don't forget to update the uniform buffer!
	zest_Update2dUniformBuffer();
	zest_SetActiveRenderQueue(ZestApp->default_command_queue);
	ImGuiApp *app = (ImGuiApp*)user_data;
	zest_instance_layer_t *sprite_layer = zest_GetInstanceLayerByIndex(0);

	static bool vsync = true;
	static float size = 100.f;
	static float expand = 2.f;
	static float bleed = 0.25f;
	static float detail = 0.5f;
	static float radius = 25.f;
	static float aa = 5.f;

	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Test Window");
	ImGui::Text("FPS %i", ZestApp->last_fps);
	ImGui::Checkbox("VSync", &vsync);
	ImGui::DragFloat("Size", &size, 1.f);
	ImGui::DragFloat("Radius", &radius, .01f);
	ImGui::DragFloat("Expand", &expand, .01f);
	ImGui::DragFloat("Bleed", &bleed, .01f);
	ImGui::DragFloat("Detail", &detail, .1f);
	ImGui::DragFloat("AA", &aa, .1f);

	float d = (1.f - -.75f) * radius;
	float s = (d + expand / detail) / 1.f + 0.5f + bleed;

	ImGui::Text("est sdf: %f", s);

	if (ImGui::IsItemDeactivatedAfterEdit()) {
		if (vsync) {
			zest_EnableVSync();
		}
		else {
			zest_DisnableVSync();
		}
	}
	ImGui::End();
	ImGui::Render();
	zest_imgui_CopyBuffers(app->imgui_layer_info.mesh_layer_index);

	zest_SetSpriteDrawing(sprite_layer, app->glyph_texture, zest_GetTextureDescriptorSetIndex(app->glyph_texture, "Default"), app->sdf_pipeline_index);
	sprite_layer->current_color.a = 0;
	sprite_layer->current_instance_instruction.push_constants.parameters2.x = expand;
	sprite_layer->current_instance_instruction.push_constants.parameters2.y = bleed;
	sprite_layer->current_instance_instruction.push_constants.parameters1.x = radius;
	sprite_layer->current_instance_instruction.push_constants.parameters1.y = detail;
	sprite_layer->current_instance_instruction.push_constants.parameters1.z = aa;
	zest_DrawSprite(sprite_layer, app->glyph_image, zest_ScreenWidthf() * .5f, zest_ScreenHeightf() * .5f, 0.f, size, size, 0.5f, 0.5f, 0, 0.f, 0);
}

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