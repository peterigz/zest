#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_MSDF_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <zest.h>
#include <stdio.h>
#include <stdlib.h>

struct RenderCacheInfo {
	bool draw_imgui;
	zest_image_handle test_texture;
};

typedef struct zest_fonts_example {
	zest_timer_handle timer;
	zest_imgui_t imgui;
	zest_context context;
	RenderCacheInfo cache_info;
	zest_msdf_font_t font;
	zest_font_resources_t font_resources;
	zest_layer_handle font_layer;
	float font_size;
} zest_fonts_example;

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
	
	if (!zest__file_exists("examples/assets/Lato-Regular.msdf")) {
		app->font = zest_CreateMSDF(app->context, "examples/assets/Lato-Regular.ttf", app->imgui.font_sampler_binding_index, 64.f, 4.f);
		zest_SaveMSDF(&app->font, "examples/assets/Lato-Regular.msdf");
	} else {
		app->font = zest_LoadMSDF(app->context, "examples/assets/Lato-Regular.msdf", app->imgui.font_sampler_binding_index);
	}

	app->font_resources = zest_CreateFontResources(app->context, "shaders/font.vert", "shaders/font.frag");
	app->font_layer = zest_CreateFontLayer(app->context, "MSDF Font Example Layer");
	app->font_size = 1.f;
}

void MainLoop(zest_fonts_example *app) {
	zest_microsecs running_time = zest_Microsecs();
	zest_microsecs frame_time = 0;
	zest_uint frame_count = 0;
	zest_uint fps = 0;

	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
		zest_microsecs current_frame_time = zest_Microsecs() - running_time;
		running_time = zest_Microsecs();
		frame_time += current_frame_time;
		frame_count += 1;
		if (frame_time >= ZEST_MICROSECS_SECOND) {
			frame_time -= ZEST_MICROSECS_SECOND;
			fps = frame_count;
			frame_count = 0;
		}

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

			ImGui::DragFloat("Font Size", &app->font_size, 0.01f);
			ImGui::DragFloat2("Unit range", &app->font.settings.unit_range.x, 0.001f);
			ImGui::DragFloat2("Shadow Offset", &app->font.settings.shadow_offset.x, 0.01f);
			ImGui::DragFloat4("Shadow Color", &app->font.settings.shadow_color.x, 0.01f);
			ImGui::DragFloat("In bias", &app->font.settings.in_bias, 0.01f);
			ImGui::DragFloat("Out bias", &app->font.settings.out_bias, 0.01f);
			ImGui::DragFloat("Smoothness", &app->font.settings.smoothness, 0.01f);
			ImGui::DragFloat("Gamma", &app->font.settings.gamma, 0.01f);

			ImGui::End();
			ImGui::Render();
			//An imgui layer is a manual layer, meaning that you need to let it know that the buffers need updating.
			//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
			zest_imgui_UpdateBuffers(&app->imgui);
		} zest_EndTimerLoop(app->timer);

		zest_SetMSDFFontDrawing(app->font_layer, &app->font, &app->font_resources);
		zest_SetLayerColor(app->font_layer, 255, 255, 255, 255);
		zest_DrawMSDFText(app->font_layer, 20.f, 150.f, .0f, 0.0f, app->font_size, 0.f, "This is a test %u !£$%^&", fps);

		app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw();
		zest_frame_graph_cache_key_t cache_key = {};
		cache_key = zest_InitialiseCacheKey(app->context, &app->cache_info, sizeof(RenderCacheInfo));

		if (zest_BeginFrame(app->context)) {
			zest_SetSwapchainClearColor(app->context, 0.f, 0.2f, 0.5f, 1.f);
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);
			//Begin the render graph with the command that acquires a swap chain image (zest_BeginFrameGraphSwapchain)
			//Use the render graph we created earlier. Will return false if a swap chain image could not be acquired. This will happen
			//if the window is resized for example.
			if (!frame_graph) {
				if (zest_BeginFrameGraph(app->context, "ImGui", &cache_key)) {
					zest_resource_node font_layer_resource = zest_AddTransientLayerResource("Font layer", app->font_layer, ZEST_FALSE);
					zest_ImportSwapchainResource();
					//If there was no imgui data to render then zest_imgui_BeginPass will return false
					//Import our test texture with the Bunny sprite
					//zest_resource_node test_texture = zest_ImportImageResource("test texture", app->test_texture, 0);

					//------------------------ Font Transfer Pass ----------------------------------------------------------------
					zest_BeginTransferPass("Upload font layer"); {
						zest_ConnectOutput(font_layer_resource);
						zest_SetPassTask(zest_UploadInstanceLayerData, &app->font_layer);
						zest_EndPass();
					}
					//----------------------------------------------------------------------------------------

					//------------------------ Font Pass ----------------------------------------------------------------
					zest_BeginRenderPass("Font pass"); {
						zest_ConnectInput(font_layer_resource);
						zest_ConnectSwapChainOutput();
						zest_SetPassTask(zest_DrawInstanceLayer, &app->font_layer);
						zest_EndPass();
					}
					//----------------------------------------------------------------------------------------

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
					frame_graph = zest_EndFrameGraph();
				}
			}
			zest_QueueFrameGraphForExecution(app->context, frame_graph);
			zest_EndFrame(app->context);
		}
		if (zest_SwapchainWasRecreated(app->context)) {
			zest_SetLayerSizeToSwapchain(app->font_layer);
			zest_UpdateFontTransform(&app->font);
		}
	}

}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);

	if (!glfwInit()) {
		return 0;
	}

	zest_fonts_example fonts_app = {};

	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);

	//Create the device that serves all vulkan based contexts
	zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
	zest_AddDeviceBuilderExtensions(device_builder, glfw_extensions, count);
	//zest_AddDeviceBuilderValidation(device_builder);
	//zest_DeviceBuilderLogToConsole(device_builder);
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
	zest_FreeFont(&fonts_app.font);
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
