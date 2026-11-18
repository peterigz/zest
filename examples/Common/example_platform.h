#pragma once

//Backend choice and validation for the examples that run on both Vulkan and D3D12.
//Include after zest.h in the file that defines the implementation, with ZEST_D3D12_IMPLEMENTATION defined under ZEST_ENABLE_D3D12.
//Arguments: --d3d12 selects the D3D12 backend, --validate enables the validation layers and fails the run on any validation error,
//--validate-sync adds Vulkan synchronisation validation or D3D12 GPU-based validation, --frames N exits after N frames.

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <imgui/backends/imgui_impl_sdl2.h>

typedef struct example_options_t {
	zest_bool use_d3d12;
	zest_bool validate;			//Validation layers on, and the exit code fails on any validation error
	zest_bool validate_sync;	//Vulkan synchronisation validation or D3D12 GPU-based validation, implies validate
	int max_frames;				//0 runs until the window is closed
} example_options_t;

//Ignores arguments it doesn't know, so an example can parse its own
example_options_t ParseExampleOptions(int argc, char *argv[]) {
	example_options_t options = {};
	for (int argument_index = 1; argument_index < argc; ++argument_index) {
		if (strcmp(argv[argument_index], "--d3d12") == 0) {
			options.use_d3d12 = ZEST_TRUE;
		} else if (strcmp(argv[argument_index], "--validate") == 0) {
			options.validate = ZEST_TRUE;
		} else if (strcmp(argv[argument_index], "--validate-sync") == 0) {
			options.validate = ZEST_TRUE;
			options.validate_sync = ZEST_TRUE;
		} else if (strcmp(argv[argument_index], "--frames") == 0 && argument_index + 1 < argc) {
			options.max_frames = atoi(argv[++argument_index]);
		}
	}
#ifndef ZEST_ENABLE_D3D12
	if (options.use_d3d12) {
		printf("D3D12 is not compiled into this build\n");
		exit(1);
	}
#endif
	return options;
}

//Returns NULL if the device couldn't be created
zest_device CreateExampleDevice(zest_window_data_t *window_data, const example_options_t *options, zest_device_init_flags flags) {
	if (options->validate) {
		flags |= zest_device_init_flag_enable_validation_layers | zest_device_init_flag_log_validation_errors_to_memory | zest_device_init_flag_log_validation_errors_to_console;
	}
	if (options->validate_sync) {
		flags |= zest_device_init_flag_enable_validation_layers_with_sync;
	}
	zest_device device = NULL;
#ifdef ZEST_ENABLE_D3D12
	if (options->use_d3d12) {
		device = zest_implsdl2_CreateD3D12Device(window_data, flags);
	} else
#endif
	{
		device = zest_implsdl2_CreateVulkanDevice(window_data, flags);
	}
	if (!device) {
		printf("Could not create the device\n");
		return NULL;
	}
	if (!options->use_d3d12) {
		//Vulkan loader warnings from third party layers are counted as validation errors
		printf("ignored %u validation messages from device creation\n", zest_GetValidationErrorCount(device));
		zest_ResetValidationErrors(device);
	}
	return device;
}

//Destroys the device and returns the exit code: 1 for any validation error, or on D3D12 for any COM reference the device left behind
int FinishExample(zest_device device, const example_options_t *options) {
	zest_uint validation_error_count = zest_GetValidationErrorCount(device);
	if (options->validate) {
		printf("validation errors: %u\n", validation_error_count);
	}
	zest_DestroyDevice(device);
	int exit_code = options->validate && validation_error_count > 0 ? 1 : 0;
#ifdef ZEST_ENABLE_D3D12
	if (options->validate && options->use_d3d12) {
		zest_uint release_count = zest__dx_last_device_release_count();
		printf("device release count: %u\n", release_count);
		if (release_count != 0) {
			exit_code = 1;
		}
	}
#endif
	return exit_code;
}

zest_bool ExampleFrameLimitReached(const example_options_t *options, int frame) {
	return options->max_frames != 0 && frame >= options->max_frames;
}

//InitForVulkan gives extra viewport windows SDL_WINDOW_VULKAN, which a machine without a Vulkan loader can't create
void InitImGuiSDL2ForDevice(zest_device device, SDL_Window *window) {
	if (zest_GetDevicePlatform(device) == zest_platform_d3d12) {
		ImGui_ImplSDL2_InitForD3D(window);
	} else {
		ImGui_ImplSDL2_InitForVulkan(window);
	}
}
