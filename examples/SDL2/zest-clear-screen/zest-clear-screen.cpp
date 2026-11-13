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

/**
	Clears the window to a slowly cycling colour on either backend.
	Arguments: --d3d12 selects the D3D12 backend, --frames N exits after N frames (resizing the window
	at N / 2), --validate enables the validation layers and fails the run on any validation error.
 */

static void SetCycleColor(zest_context context, int frame) {
	float time = (float)frame * 0.005f;
	zest_SetSwapchainClearColor(context, 0.12f + 0.05f * sinf(time), 0.14f + 0.05f * sinf(time + 2.094f), 0.18f + 0.05f * sinf(time + 4.188f), 1.f);
}

int main(int argc, char *argv[]) {
	bool use_d3d12 = false;
	bool validate = false;
	int max_frames = 0;
	for (int argument_index = 1; argument_index < argc; ++argument_index) {
		if (strcmp(argv[argument_index], "--d3d12") == 0) {
			use_d3d12 = true;
		} else if (strcmp(argv[argument_index], "--validate") == 0) {
			validate = true;
		} else if (strcmp(argv[argument_index], "--frames") == 0 && argument_index + 1 < argc) {
			max_frames = atoi(argv[++argument_index]);
		}
	}

#ifndef ZEST_ENABLE_D3D12
	if (use_d3d12) {
		printf("D3D12 is not compiled into this build\n");
		return 1;
	}
#endif

	zest_device_init_flags device_flags = 0;
	if (validate) {
		device_flags = zest_device_init_flag_enable_validation_layers | zest_device_init_flag_log_validation_errors_to_memory | zest_device_init_flag_log_validation_errors_to_console;
	}

	zest_create_context_info_t create_info = zest_CreateContextInfo();
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "Clear Screen");

	zest_device device = NULL;
#ifdef ZEST_ENABLE_D3D12
	if (use_d3d12) {
		device = zest_implsdl2_CreateD3D12Device(&window_data, device_flags);
	} else
#endif
	{
		device = zest_implsdl2_CreateVulkanDevice(&window_data, device_flags);
	}
	if (!device) {
		printf("Could not create the device\n");
		return 1;
	}
	if (!use_d3d12) {
		//Vulkan loader warnings from third party layers are counted as validation errors
		printf("ignored %u validation messages from device creation\n", zest_GetValidationErrorCount(device));
		zest_ResetValidationErrors(device);
	}
	zest_context context = zest_CreateContext(device, &window_data, &create_info);
	if (!context) {
		printf("Could not create the context\n");
		zest_DestroyDevice(device);
		return 1;
	}

	int frame = 0;
	bool running = true;
	while (running && (max_frames == 0 || frame < max_frames)) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				running = false;
			}
		}
		if (max_frames > 0 && frame == max_frames / 2) {
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

	zest_uint validation_error_count = zest_GetValidationErrorCount(device);
	printf("validation errors: %u\n", validation_error_count);
	zest_DestroyDevice(device);

	int exit_code = validation_error_count > 0 ? 1 : 0;
#ifdef ZEST_ENABLE_D3D12
	if (validate && use_d3d12) {
		zest_uint release_count = zest__dx_last_device_release_count();
		printf("device release count: %u\n", release_count);
		if (release_count != 0) {
			exit_code = 1;
		}
	}
#endif
	return exit_code;
}
