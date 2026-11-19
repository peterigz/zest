#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#ifdef ZEST_ENABLE_D3D12
#define ZEST_D3D12_IMPLEMENTATION
#endif
#include <SDL.h>
#include <zest.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "examples/Common/example_platform.h"

/**
	Clears the window to a slowly cycling colour on either backend.
	Arguments: --vulkan or --d3d12 selects the backend (D3D12 is the default when it's compiled in),
	--frames N exits after N frames (resizing the window at N / 2), --validate enables the validation
	layers and fails the run on any validation error. See examples/Common/example_platform.h.
 */

static void SetCycleColor(zest_context context, int frame) {
	float time = (float)frame * 0.005f;
	zest_SetSwapchainClearColor(context, 0.12f + 0.05f * sinf(time), 0.14f + 0.05f * sinf(time + 2.094f), 0.18f + 0.05f * sinf(time + 4.188f), 1.f);
}

int main(int argc, char *argv[]) {
	//--vulkan, --d3d12, --validate, --validate-sync and --frames N, see examples/Common/example_platform.h
	example_options_t options = ParseExampleOptions(argc, argv);

	zest_create_context_info_t create_info = zest_CreateContextInfo();
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "Clear Screen");

	zest_device device = CreateExampleDevice(&window_data, &options, 0);
	if (!device) {
		return 1;
	}
	zest_context context = zest_CreateContext(device, &window_data, &create_info);
	if (!context) {
		printf("Could not create the context\n");
		zest_DestroyDevice(device);
		return 1;
	}

	int frame = 0;
	bool running = true;
	while (running && !ExampleFrameLimitReached(&options, frame)) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				running = false;
			}
		}
		if (options.max_frames > 0 && frame == options.max_frames / 2) {
			SDL_SetWindowSize((SDL_Window*)zest_Window(context), 800, 600);
		}

		zest_UpdateDevice(device);
		zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(context, 0, 0);
		if (zest_BeginFrame(context)) {
			SetCycleColor(context, frame);
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(context, &cache_key);
			if (!frame_graph) {
				if (zest_BeginFrameGraph(context, "Clear Graph", &cache_key)) {
					zest_ImportSwapchainResource();
					zest_BeginRenderPass("Clear"); {
						zest_ConnectSwapChainOutput();
						zest_SetPassTask(zest_EmptyRenderPass, NULL);
						zest_EndPass();
					}
					frame_graph = zest_EndFrameGraph();
				}
			}
			zest_EndFrame(context, frame_graph);
			if (zest_SwapchainWasRecreated(context)) {
				SetCycleColor(context, frame);
			}
		}
		++frame;
	}

	return FinishExample(device, &options);
}
