#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <zest.h>
#include <string>

typedef struct minimal_app_t {
	zest_device device;
	zest_context context;
} minimal_app_t;

void BlankScreen(const zest_command_list command_list, void *user_data) {
	//Usually you'd have zest_cmd_ commands like zest_cmd_Draw or zest_cmd_DrawIndexed
	//But with nothing here it will just be a blank screen.
}

void MainLoop(minimal_app_t *app) {
	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
		zest_UpdateDevice(app->device);
		glfwPollEvents();
		//Generate a cache key
		zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(app->context, 0, 0);
		if (zest_BeginFrame(app->context)) {
			//Try and fetch a cached frame graph using the key
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);
			if (!frame_graph) {
				//If no frame graph is cached then start recording a new frame graph
				//Specifying the cache_key will make the frame graph get cached after it's done recording
				if (zest_BeginFrameGraph(app->context, "Render Graph", &cache_key)) {
					//Import the swap chain as a resource
					zest_ImportSwapchainResource();
					//Create a new render pass to draw to the swapchain
					zest_BeginRenderPass("Draw Nothing"); {
						//Tell the render pass that we want to output to the swap chain
						zest_ConnectSwapChainOutput();
						//Set the callback that will record the command buffer that draws to the swap chain. In this
						//case though we're not drawing anything other then a blank screen.
						zest_SetPassTask(BlankScreen, 0);
						//Declare the end of the render pass
						zest_EndPass();
					}
					//End the frame graph. This will compile the frame graph ready to be executed.
					frame_graph = zest_EndFrameGraph();
				}
			}
			//When building a frame graph that is within a zest_BeginFrame and zest_EndFrame you can 
			//queue it for execution. This means that it will be executed inside zest_EndFrame and presented
			//to the window surface.
			zest_QueueFrameGraphForExecution(app->context, frame_graph);
			zest_EndFrame(app->context);
		}
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
int main(void) 
{


	//Make a config struct where you can configure zest with some options
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	ZEST__UNFLAG(create_info.flags, zest_context_init_flag_enable_vsync);

	if (!glfwInit()) {
		return 0;
	}

	minimal_app_t app = {};

	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);

	//Create the device that serves all vulkan based contexts
	zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
	zest_AddDeviceBuilderExtensions(device_builder, glfw_extensions, count);
	zest_AddDeviceBuilderValidation(device_builder);
	zest_DeviceBuilderLogToConsole(device_builder);
	app.device = zest_EndDeviceBuilder(device_builder);

	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "Minimal Example");
	//Initialise Zest
	app.context = zest_CreateContext(app.device, &window_handles, &create_info);

	/* I was testing a new sparse hash map
	zest_map_t test_map;
	zest_uint capacity = 262144;
	zest_uint insert_count = 131072;
	zest__initialise_map(app.context, &test_map, sizeof(zest_uint), capacity);

	zest_hash_map(zest_uint) test_storage_map;
	test_storage_map test_storage = {};

	zest_vec_reserve(app.context->allocator, test_storage.map, capacity);
	zest_vec_reserve(app.context->allocator, test_storage.data, capacity);

	zest_uint value = 0;

	char name[16];

	zest_microsecs time = zest_Microsecs();

	for (int i = 0; i != insert_count; i++) {
		zest_key key = rand();
		zest_snprintf(name, 16, "Test %i", i);
		zest__insert(&test_map, name, &value); value++;
	}
	zest_microsecs insert_time = zest_Microsecs() - time;
	time = zest_Microsecs();

	for(int i = 0; i != test_map.current_size; i++) {
		zest_uint index = test_map.filled_slots[i];
		zest_uint *value = (zest_uint*)zest__at_index(&test_map, index);
		ZEST_ASSERT(*value == (zest_uint)i);
	}
	zest_microsecs index_time = zest_Microsecs() - time;
	time = zest_Microsecs();

	for (int i = 0; i != insert_count; i++) {
		zest_snprintf(name, 16, "Test %i", i);
		zest_uint *value = (zest_uint*)zest__at(&test_map, name);
		ZEST_ASSERT(*value == (zest_uint)i);
	}
	zest_microsecs lookup_time = zest_Microsecs() - time;

	ZEST_PRINT("Collisions: %u", test_map.collisions);
	ZEST_PRINT("Map Insert time: %llu", insert_time);
	ZEST_PRINT("Map Index time : %llu", index_time);
	ZEST_PRINT("Map Lookup time: %llu", lookup_time);
	ZEST_PRINT("Map Total Time: %llu", lookup_time + index_time + insert_time);

	time = zest_Microsecs();
	for (int i = 0; i != insert_count; i++) {
		zest_key key = rand();
		zest_snprintf(name, 16, "Test %i", i);
		zest_map_insert(app.context->allocator, test_storage, name, value); value++;
	}
	insert_time = zest_Microsecs() - time;
	time = zest_Microsecs();

	for(int i = 0; i != zest_map_size(test_storage); i++) {
		zest_uint index = test_map.filled_slots[i];
		zest_uint *value = zest_map_at_index(test_storage, i);
		//ZEST_ASSERT(*value == (zest_uint)i);
	}
	index_time = zest_Microsecs() - time;
	time = zest_Microsecs();

	for (int i = 0; i != zest_map_size(test_storage); i++) {
		zest_snprintf(name, 16, "Test %i", i);
		zest_uint *value = zest_map_at(test_storage, name);
		//ZEST_ASSERT(*value == (zest_uint)i);
	}
	lookup_time = zest_Microsecs() - time;
	ZEST_PRINT("Storage Insert time: %llu", insert_time);
	ZEST_PRINT("Storage Index time : %llu", index_time);
	ZEST_PRINT("Storage Lookup time: %llu", lookup_time);
	ZEST_PRINT("Storage Total Time: %llu", lookup_time + index_time + insert_time);

	return 0;
	*/

	//Start the Zest main loop
	MainLoop(&app);
	zest_DestroyDevice(app.device);

	return 0;
}
#else
int main(void) {
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);

	zest_CreateContext(&create_info);
    zest_LogFPSToConsole(1);
	zest_SetUserUpdateCallback(UpdateCallback);

	zest_Start();

	return 0;
}
#endif
