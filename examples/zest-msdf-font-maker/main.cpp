#include "header.h"
#include "imgui_internal.h"
#include <filesystem>
#include <fstream>
#include <ios>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void InitImGuiApp(ImGuiApp *app) {

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(zest_ScreenWidthf(), zest_ScreenHeightf());
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	int upload_size = width * height * 4 * sizeof(char);

	zest_bitmap font_bitmap = zest_CreateBitmapFromRawBuffer("font_bitmap", pixels, upload_size, width, height, 4);
	app->imgui_font_texture_index = zest_CreateTexture("imgui_font", zest_texture_storage_type_single, zest_texture_flag_none, zest_texture_format_rgba, 10);
	zest_texture *font_texture = zest_GetTextureByName("imgui_font");
	zest_index font_image_index = zest_AddTextureImageBitmap(font_texture, &font_bitmap);
	zest_ProcessTextureImages(font_texture);
	io.Fonts->SetTexID(zest_GetImageFromTexture(font_texture, font_image_index));
	ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)ZestApp->window->window_handle, true);

	app->imgui_layer_info.pipeline_index = zest_PipelineIndex("pipeline_imgui");
	zest_ModifyCommandQueue(ZestApp->default_command_queue_index);
	{
		zest_ModifyDrawCommands(ZestApp->default_render_commands_index);
		{
			app->imgui_layer_info.mesh_layer_index = zest_NewMeshLayer("imgui mesh layer", sizeof(ImDrawVert));
			zest_ContextDrawRoutine()->draw_callback = zest_imgui_DrawLayer;
			zest_ContextDrawRoutine()->data = &app->imgui_layer_info;
		}
		zest_FinishQueueSetup();
	}
	
	app->config.font_file = "";
}

static void SaveFont(std::string filename, std::map<const char, zest_font_character> *characters, float size, zest_bitmap *bitmap) {
	std::ofstream ofile(filename, std::ios::binary);

	uint32_t character_count = uint32_t(characters->size());
	char magic_number[] = "!TSEZ";
	ofile.write((char*)magic_number, 6);
	ofile.write((char*)&character_count, sizeof(uint32_t));

	for (const auto& c : *characters)
	{
		ofile.write((char*)&c.second.character, sizeof(zest_font_character));
	}

	ofile.write((char*)&size, sizeof(float));

	custom_stbi_mem_context context;
	context.last_pos = 0;
	std::vector<char> buffer;
	context.context = &buffer;
	int result = stbi_write_png_to_func(custom_stbi_write_mem, &context, bitmap->width, bitmap->height, bitmap->channels, bitmap->data, bitmap->stride);

	ofile.write((char*)&bitmap->size, sizeof(unsigned long));
	ofile.write(reinterpret_cast<char *>(buffer.data()), buffer.size());

	ofile.close();
}

bool GenerateAtlas(const char *fontFilename, const char *save_to, zest_font_configuration *config) {
	bool success = false;
	// Initialize instance of FreeType library
	if (msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype()) {
		// Load font file
		if (msdfgen::FontHandle *font = msdfgen::loadFont(ft, fontFilename)) {
			// Storage for glyph geometry and their coordinates in the atlas
			std::vector<GlyphGeometry> glyphs;
			// FontGeometry is a helper class that loads a set of glyphs from a single font.
			// It can also be used to get additional font metrics, kerning information, etc.
			FontGeometry font_geometry(&glyphs);
			// Load a set of character glyphs:
			// The second argument can be ignored unless you mix different font sizes in one atlas.
			// In the last argument, you can specify a charset other than ASCII.
			// To load specific glyph indices, use loadGlyphs instead.
			font_geometry.loadCharset(font, 1.0, createAsciiCharset());
			// Apply MSDF edge coloring. See edge-coloring.h for other coloring strategies.
			const double maxCornerAngle = 3.0;
			for (GlyphGeometry &glyph : glyphs)
				glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, maxCornerAngle, 0);
			// TightAtlasPacker class computes the layout of the atlas.
			TightAtlasPacker packer;
			// Set atlas parameters:
			// setDimensions or setDimensionsConstraint to find the best value
			packer.setDimensionsConstraint(TightAtlasPacker::DimensionsConstraint::SQUARE);
			// setScale for a fixed size or setMinimumScale to use the largest that fits
			packer.setMinimumScale(config->minimum_scale);
			// setPixelRange or setUnitRange
			packer.setPixelRange(config->px_range);
			packer.setMiterLimit(config->miter_limit);
			packer.setPadding(config->padding);
			// Compute atlas layout - pack glyphs
			packer.pack(glyphs.data(), (int)glyphs.size());
			// Get final atlas dimensions
			packer.getDimensions(config->width, config->height);
			// The ImmediateAtlasGenerator class facilitates the generation of the atlas bitmap.
			ImmediateAtlasGenerator<
				float, // pixel type of buffer for individual glyphs depends on generator function
				4, // number of atlas color channels
				&mtsdfGenerator, // function to generate bitmaps for individual glyphs
				BitmapAtlasStorage<byte, 4> // class that stores the atlas bitmap
				// For example, a custom atlas storage class that stores it in VRAM can be used.
			> generator(config->width, config->height);
			// GeneratorAttributes can be modified to change the generator's default settings.
			GeneratorAttributes attributes;
			generator.setAttributes(attributes);
			generator.setThreadCount(4);
			// Generate atlas bitmap
			generator.generate(glyphs.data(), (int)glyphs.size());
			// The atlas bitmap can now be retrieved via atlasStorage as a BitmapConstRef.
			// The glyphs array (or font_geometry) contains positioning data for typesetting text.
			//success = myProject::submitAtlasBitmapAndLayout(generator.atlasStorage(), glyphs);
			msdfgen::savePng(generator.atlasStorage(), "test.png");

			msdfgen::BitmapConstRef<unsigned char, 4> atlas = (msdfgen::BitmapConstRef<unsigned char, 4>) generator.atlasStorage();
			std::vector<byte> pixels(4 * config->width * config->height);
			byte *it = pixels.data();
			for (int y = config->height - 1; y >= 0; --y) {
				for (int x = 0; x < config->width; ++x) {
					*it++ = atlas(x, y)[2];
					*it++ = atlas(x, y)[1];
					*it++ = atlas(x, y)[0];
					*it++ = atlas(x, y)[3];
				}
			}

			zest_bitmap bitmap = zest_NewBitmap();
			zest_AllocateBitmap(&bitmap, config->width, config->height, 4, 0);

			memcpy(bitmap.data, pixels.data(), pixels.size());

			std::map<const char, zest_font_character> characters;
			float size = 0;
			for (auto &glyph : font_geometry.getGlyphs()) {

				double l, b, r, t;
				zest_font_character c;

				glyph.getQuadPlaneBounds(l, b, r, t);
				c.width = float(r - l);
				c.height = float(t - b);
				size = std::fmaxf(size, c.height);
				c.character[0] = (char)glyph.getIdentifier(font_geometry.getPreferredIdentifierType());
				c.xadvance = (float)glyph.getAdvance();
				c.xoffset = (float)l;
				c.yoffset = (float)-t;

				glyph.getQuadAtlasBounds(l, b, r, t);
				c.uv.x = (float)l / (float)config->width;
				c.uv.y = (float)t / (float)config->height;
				c.uv.z = (float)r / (float)config->width;
				c.uv.w = (float)b / (float)config->height;
				c.skip = glyph.isWhitespace() ? true : false;

				const char key = c.character[0];
				characters.insert(std::pair<const char, zest_font_character>(key, c));
			}

			SaveFont(save_to, &characters, size, &bitmap);
			zest_FreeBitmap(&bitmap);
			// Cleanup
			msdfgen::destroyFont(font);
		}
		msdfgen::deinitializeFreetype(ft);
	}
	return success;
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	//Don't forget to update the uniform buffer!
	zest_Update2dUniformBuffer();
	zest_SetActiveRenderQueue(0);
	ImGuiApp *app = (ImGuiApp*)user_data;
	zest_instance_layer *sprite_layer = zest_GetInstanceLayerByIndex(0);

	ImGui_ImplGlfw_NewFrame();

	ImGui::NewFrame();
	double minimum_size = 16.0;
	ImGui::Begin("Generate Font");
	if (ImGui::Button("Open Font")) {
		ImGuiFileDialog::Instance()->OpenDialog("load_font", "Choose File", ".ttf,.otf", ".", 1, nullptr, ImGuiFileDialogFlags_Modal);
	}
	ImGui::DragScalar("Pixel Range", ImGuiDataType_Double, &app->config.px_range, 0.1f);
	ImGui::DragScalar("Miter Limit", ImGuiDataType_Double, &app->config.miter_limit, 0.1f);
	ImGui::DragScalar("Minimim Scale", ImGuiDataType_Double, &app->config.minimum_scale, 0.1f, &minimum_size);
	ImGui::DragInt("Padding", &app->config.padding, 1, 0, 32);

	if (ImGuiFileDialog::Instance()->Display("load_font"))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			std::filesystem::path path = ImGuiFileDialog::Instance()->GetFilePathName();
			path.replace_extension(std::filesystem::path(".zft"));
			app->config.font_save_file = path.string();
			app->config.font_file = ImGuiFileDialog::Instance()->GetCurrentFileName();
			app->config.font_path = ImGuiFileDialog::Instance()->GetFilePathName();
			app->config.font_folder = ImGuiFileDialog::Instance()->GetCurrentPath();
		}
		ImGuiFileDialog::Instance()->Close();
	}

	if (app->config.font_folder.length()) {
		ImGui::Text("File: %s", app->config.font_file.c_str());
	}

	if (app->config.font_file.length() && ImGui::Button("Generate File")) {
		std::filesystem::path path = ImGuiFileDialog::Instance()->GetFilePathName();
		app->config.font_save_file = path.string();
		ImGuiFileDialog::Instance()->OpenDialog("save_font", "Choose File", ".zft", app->config.font_save_file, 1, nullptr, ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ConfirmOverwrite);
	}

	if (ImGuiFileDialog::Instance()->Display("save_font"))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			std::filesystem::path path = ImGuiFileDialog::Instance()->GetFilePathName();
			app->config.font_save_file = path.string();
			app->config.font_folder = ImGuiFileDialog::Instance()->GetCurrentPath();
			GenerateAtlas(app->config.font_path.c_str(), app->config.font_save_file.c_str(), &app->config);
		}
		ImGuiFileDialog::Instance()->Close();
	}

	ImGui::End();
	ImGui::Render();
	zest_imgui_CopyBuffers(app->imgui_layer_info.mesh_layer_index);
}

int main(void) {

	zest_create_info create_info = zest_CreateInfo();
	zest_implglfw_SetCallbacks(&create_info);

	ImGuiApp imgui_app;

	zest_Initialise(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}