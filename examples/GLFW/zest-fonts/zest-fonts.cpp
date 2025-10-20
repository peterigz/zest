#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <zest.h>
#include <GLFW/glfw3.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>
#define ZEST_MSDF_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "zest_utilities.h"
#include <stdio.h>
#include <stdlib.h>

struct RenderCacheInfo {
	bool draw_imgui;
	zest_image_handle test_texture;
};

typedef struct zest_font_character_t {
    float x0, y0, x1, y1; // UV coordinates in the atlas
    float x_offset, y_offset;
    float x_advance;
} zest_font_character_t;

typedef struct zest_fonts_example {
	zest_layer_handle font_layer;
	zest_timer_handle timer;
	zest_imgui_t imgui;
	zest_context context;
	RenderCacheInfo cache_info;
    stbtt_fontinfo font_info;
    zest_font_character_t font_chars[95];
    zest_image_handle font_atlas;
	zest_image_collection_handle font_atlas_collection;
	zest_atlas_region test_character;
	zest_uint test_character_index;
} zest_fonts_example;

void* zest__msdf_allocation(size_t size, void* ctx) {
	zest_context context = (zest_context)ctx;
    return zest_AllocateMemory(context, size);
}

void zest__msdf_free(void* ptr, void* ctx) {
	zest_context context = (zest_context)ctx;
    zest_FreeMemory(context, ptr);
}

void InitExample(zest_fonts_example *app) {
	//Initialise Dear ImGui
	app->imgui = zest_imgui_Initialise(app->context);
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)zest_Window(app->context), true);

	//Implement a dark style
	zest_imgui_DarkStyle();
	
	//This is an exmaple of how to change the font that ImGui uses
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.Fonts->Clear();
	float font_size_imgui = 16.f;
	unsigned char *font_data_imgui;
	int tex_width, tex_height;
	ImFontConfig config;
	config.PixelSnapH = true;
	io.Fonts->AddFontFromFileTTF("examples/assets/Lato-Regular.ttf", font_size_imgui);
	io.Fonts->GetTexDataAsRGBA32(&font_data_imgui, &tex_width, &tex_height);

	//Rebuild the Zest font texture
	zest_imgui_RebuildFontTexture(&app->imgui, tex_width, tex_height, font_data_imgui);

	//We can use a timer to only update imgui 60 times per second
	app->timer = zest_CreateTimer(app->context, 60);

    // --- MSDF Font Atlas Generation ---
    unsigned char* font_buffer = (unsigned char*)zest_ReadEntireFile(app->context, "examples/assets/Lato-Regular.ttf", ZEST_FALSE);
    if (!font_buffer) {
        printf("Error loading font file\n");
        return;
    }

    if (!stbtt_InitFont(&app->font_info, font_buffer, 0)) {
        printf("Error initializing font\n");
		zest_FreeFile(app->context, (zest_file)font_buffer);
        return;
    }

    // Atlas properties
    int atlas_width = 1024;
    int atlas_height = 1024;
    int atlas_channels = 4; // RGB for MSDF
	app->font_atlas_collection = zest_CreateImageAtlasCollection(app->context, zest_format_r32g32b32a32_sfloat);

    int current_x = 0;
    int current_y = 0;
    int max_row_height = 0;

    float font_size = 64.0f;
    float scale = stbtt_ScaleForPixelHeight(&app->font_info, font_size);
    float range = 4.0f; // Range of the SDF in pixels

	msdf_allocation_context_t allocation_context;
	allocation_context.ctx = app->context;
	allocation_context.alloc = zest__msdf_allocation;
	allocation_context.free = zest__msdf_free;

    for (int char_code = 65; char_code < 66; ++char_code) {
        int glyph_index = stbtt_FindGlyphIndex(&app->font_info, char_code);
        if (glyph_index == 0) {
            continue;
        }

        msdf_result_t result;
        if (msdf_genGlyph(&result, &app->font_info, glyph_index, 4, scale, range, NULL)) {
            if (current_x + result.width > atlas_width) {
                current_x = 0;
                current_y += max_row_height;
                max_row_height = 0;
            }

			char character[2] = { (char)char_code, '\0' };
			int size = result.width * result.height * sizeof(float) * atlas_channels;
			zest_bitmap tmp_bitmap = zest_CreateBitmapFromRawBuffer(app->context, character, result.rgba, size, result.width, result.height, zest_format_r32g32b32a32_sfloat);
			app->test_character = zest_AddImageAtlasBitmap(app->font_atlas_collection, tmp_bitmap, character);

            zest_font_character_t* fc = &app->font_chars[char_code - 32];
            fc->x0 = (float)current_x / (float)atlas_width;
            fc->y0 = (float)current_y / (float)atlas_height;
            fc->x1 = (float)(current_x + result.width) / (float)atlas_width;
            fc->y1 = (float)(current_y + result.height) / (float)atlas_height;

            int advance, lsb;
            stbtt_GetGlyphHMetrics(&app->font_info, glyph_index, &advance, &lsb);
            fc->x_advance = scale * advance;

            int x0, y0, x1, y1;
            stbtt_GetGlyphBox(&app->font_info, glyph_index, &x0, &y0, &x1, &y1);
            fc->x_offset = scale * x0;
            fc->y_offset = scale * y0;


            current_x += result.width;
            if (result.height > max_row_height) {
                max_row_height = result.height;
            }
        }
    }

	zest_image_handle image_atlas = zest_CreateImageAtlas(app->font_atlas_collection, 1024, 1024, 0);
	app->test_character_index = zest_AcquireGlobalSampledImageIndex(image_atlas, zest_texture_2d_binding);
	zest_BindAtlasRegionToImage(app->test_character, app->imgui.font_sampler_binding_index, image_atlas, zest_texture_2d_binding);
    zest_FreeFile(app->context, (zest_file)font_buffer);

}

void MainLoop(zest_fonts_example *app) {
	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
		glfwPollEvents();
		//We can use a timer to only update the gui every 60 times a second (or whatever you decide). This
		//means that the buffers are uploaded less frequently and the command buffer is also re-recorded
		//less frequently.
		zest_StartTimerLoop(app->timer) {
			//Must call the imgui GLFW implementation function
			ImGui_ImplGlfw_NewFrame();
			//Draw our imgui stuff
			ImGui::NewFrame();
			ImGui::Begin("Test Window");

			zest_vec4 uv = zest_ImageUV(app->test_character);
			zest_imgui_DrawImage(app->test_character, (float)app->test_character->width, (float)app->test_character->height, 0, 0);

			ImGui::End();
			ImGui::Render();
			//An imgui layer is a manual layer, meaning that you need to let it know that the buffers need updating.
			//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
			zest_imgui_UpdateBuffers(&app->imgui);
		} zest_EndTimerLoop(app->timer);

		app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw();
		zest_frame_graph_cache_key_t cache_key = {};
		cache_key = zest_InitialiseCacheKey(app->context, &app->cache_info, sizeof(RenderCacheInfo));

		if (zest_BeginFrame(app->context)) {
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);
			//Begin the render graph with the command that acquires a swap chain image (zest_BeginFrameGraphSwapchain)
			//Use the render graph we created earlier. Will return false if a swap chain image could not be acquired. This will happen
			//if the window is resized for example.
			if (!frame_graph) {
				if (zest_BeginFrameGraph(app->context, "ImGui", &cache_key)) {
					zest_ImportSwapchainResource();
					//If there was no imgui data to render then zest_imgui_BeginPass will return false
					//Import our test texture with the Bunny sprite
					//zest_resource_node test_texture = zest_ImportImageResource("test texture", app->test_texture, 0);
					//------------------------ ImGui Pass ----------------------------------------------------------------
					//If there's imgui to draw then draw it
					zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui);
					if (imgui_pass) {
						//zest_ConnectInput(test_texture, 0);
						zest_ConnectSwapChainOutput();
					} else {
						//If there's no ImGui to render then just render a blank screen
						zest_pass_node blank_pass = zest_BeginGraphicBlankScreen("Draw Nothing");
						//Add the swap chain as an output to the imgui render pass. This is telling the render graph where it should render to.
						zest_ConnectSwapChainOutput();
					}
					zest_EndPass();
					//----------------------------------------------------------------------------------------------------
					//End the render graph and execute it. This will submit it to the GPU.
					zest_frame_graph render_graph = zest_EndFrameGraph();
				}
			} else {
				zest_QueueFrameGraphForExecution(app->context, frame_graph);
			}
			zest_EndFrame(app->context);
		}
	}

}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(zest_validation_flag_enable_sync);
	//zest_create_info_t create_info = zest_CreateInfo();

	if (!glfwInit()) {
		return 0;
	}

	zest_fonts_example fonts_app = {};

	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);

	//Create the device that serves all vulkan based contexts
	zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
	zest_AddDeviceBuilderExtensions(device_builder, glfw_extensions, count);
	zest_AddDeviceBuilderValidation(device_builder);
	zest_DeviceBuilderLogToConsole(device_builder);
	zest_device device = zest_EndDeviceBuilder(device_builder);

	//Create a window using GLFW
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");
	//Initialise Zest
	fonts_app.context = zest_CreateContext(device, &window_handles, &create_info);

	//Set the Zest use data
	zest_SetContextUserData(fonts_app.context, &fonts_app);
	//Initialise our example
	InitExample(&fonts_app);

	//Start the main loop
	MainLoop(&fonts_app);
	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Destroy(&fonts_app.imgui);
	zest_DestroyContext(fonts_app.context);

	return 0;
}
#else
int main(void) {

	zest_create_info_t create_info = zest_CreateInfo();

	zest_fonts_example example;
    
	zest_CreateContext(&create_info);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(UpdateCallback);

	InitExample(&example);

	zest_Start();

	return 0;
}
#endif
