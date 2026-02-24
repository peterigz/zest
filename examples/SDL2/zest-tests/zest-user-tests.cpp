#include "zest-tests.h"

//Test that various usage errors are gracefully handled and reported to the user with clear instructions
//on how to fix

//Test zest_BeginFrame zest_UpdateDevice omission.
int test__no_update_device(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
	if (zest_BeginFrame(tests->context)) {
		zest_EndFrame(tests->context, 0);
	}
	test->result |= (zest_GetValidationErrorCount(tests->context) != 1);
	test->frame_count++;
	return test->result;
}

//Test zest_BeginFrame zest_EndFrame omission.
int test__no_end_frame(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
	for(int i = 0; i != 2; i++) {
		zest_UpdateDevice(tests->device);
		if (zest_BeginFrame(tests->context)) {
		}
	}
	test->result |= (zest_GetValidationErrorCount(tests->context) != 1);
	test->frame_count++;
	return test->result;
}

//Test output to swap chain but didn't import
int test__no_swapchain_import(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			zest_BeginRenderPass("Draw Nothing"); {
				zest_ConnectSwapChainOutput();
				zest_SetPassTask(zest_EmptyRenderPass, 0);
				zest_EndPass();
			}
			frame_graph = zest_EndFrameGraph();
		}
		zest_EndFrame(tests->context, frame_graph);
	}
	test->result |= (zest_GetValidationErrorCount(tests->context) != 1);
	test->frame_count++;
	return test->result;
}

//Test that a frame graph still works with or without the zest_EndPass function. If zest_EndPass is omitted
//it should be called anyway inside a begin pass function
int test__no_end_pass(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};

	int passed_tests = 0;
	int total_tests = 0;
	{
		int phase_total = 0;
		int phase_passed = 0;
		zest_UpdateDevice(tests->device);
		if (zest_BeginFrame(tests->context)) {
			zest_frame_graph frame_graph = NULL;
			if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
				zest_ImportSwapchainResource();
				zest_BeginRenderPass("Draw Nothing"); {
					zest_ConnectSwapChainOutput();
					zest_SetPassTask(zest_EmptyRenderPass, 0);
				}
				frame_graph = zest_EndFrameGraph();
			}
			zest_EndFrame(tests->context, frame_graph);
		}
		phase_passed = (zest_GetValidationErrorCount(tests->context) == 0);
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
		PrintTestUpdate(test, 1, phase_passed == phase_total);
	}

	{
		int phase_total = 0;
		int phase_passed = 0;
		zest_UpdateDevice(tests->device);
		if (zest_BeginFrame(tests->context)) {
			zest_frame_graph frame_graph = NULL;
			if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
				zest_ImportSwapchainResource();
				zest_BeginRenderPass("Initial Pass"); {
					zest_ConnectSwapChainOutput();
					zest_SetPassTask(zest_EmptyRenderPass, 0);
				}
				zest_BeginRenderPass("Draw Nothing"); {
					zest_ConnectSwapChainOutput();
					zest_SetPassTask(zest_EmptyRenderPass, 0);
					zest_EndPass();
				}
				frame_graph = zest_EndFrameGraph();
			}
			zest_EndFrame(tests->context, frame_graph);
		}
		phase_passed = (zest_GetValidationErrorCount(tests->context) == 0);
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
		PrintTestUpdate(test, 2, phase_passed == phase_total);
	}

	test->result |= (total_tests != passed_tests);
	test->frame_count++;
	return test->result;
}

//Test bad ordering of frame graph functions
int test__bad_frame_graph_ordering(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
	int passed_tests = 0;
	int total_tests = 0;

	{
		//Phase 1: Connect swap chain must go inside a pass
		int phase_total = 0;
		int phase_passed = 0;
		zest_UpdateDevice(tests->device);
		if (zest_BeginFrame(tests->context)) {
			zest_frame_graph frame_graph = NULL;
			if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
				zest_ImportSwapchainResource();
				zest_ConnectSwapChainOutput();
				zest_BeginRenderPass("Draw Nothing"); {
					zest_SetPassTask(zest_EmptyRenderPass, 0);
					zest_EndPass();
				}
				frame_graph = zest_EndFrameGraph();
			}
			zest_EndFrame(tests->context, frame_graph);
		}
		phase_passed = (zest_GetValidationErrorCount(tests->context) == 1);
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
		PrintTestUpdate(test, 1, phase_passed == phase_total);
	}

	zest_ResetValidationErrors(tests->device);

	{
		//Phase 2: Connect ouput must go inside/after the transfer pass
		zest_buffer_resource_info_t buffer_info = {};
		buffer_info.size = sizeof(float) * 1024;
		int phase_total = 0;
		int phase_passed = 0;
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			zest_resource_node output_a = zest_AddTransientBufferResource("Output A", &buffer_info);

			zest_ConnectOutput(output_a);
			zest_BeginTransferPass("Transfer"); {
				zest_SetPassTask(zest_TransferBuffer, tests);
			}
			frame_graph = zest_EndFrameGraph();
			zest_FlushFrameGraph(frame_graph);
		}
		phase_passed = (zest_GetValidationErrorCount(tests->context) == 1);
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
		PrintTestUpdate(test, 2, phase_passed == phase_total);
	}

	zest_ResetValidationErrors(tests->device);

	{
		//Phase 3: Can only connect swapchain inside a zest_BeginFrame
		zest_buffer_resource_info_t buffer_info = {};
		buffer_info.size = sizeof(float) * 1024;
		int phase_total = 0;
		int phase_passed = 0;
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			zest_ImportSwapchainResource();
			zest_BeginRenderPass("Draw Nothing"); {
				zest_SetPassTask(zest_EmptyRenderPass, 0);
				zest_ConnectSwapChainOutput();
				zest_EndPass();
			}
			frame_graph = zest_EndFrameGraph();
			zest_FlushFrameGraph(frame_graph);
		}
		phase_passed = (zest_GetValidationErrorCount(tests->context) == 2);
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
		PrintTestUpdate(test, 3, phase_passed == phase_total);
	}

	zest_ResetValidationErrors(tests->device);

	{
		//Phase 4: Call zest_BeginFrame without a corresponding zest_EndFrame
		zest_buffer_resource_info_t buffer_info = {};
		buffer_info.size = sizeof(float) * 1024;
		int phase_total = 0;
		int phase_passed = 0;
		zest_frame_graph frame_graph = NULL;
		zest_UpdateDevice(tests->device);
		zest_BeginFrame(tests->context);
		zest_BeginFrame(tests->context);
		phase_passed = (zest_GetValidationErrorCount(tests->context) == 1);
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
		PrintTestUpdate(test, 4, phase_passed == phase_total);
	}

	zest_ResetValidationErrors(tests->device);

	{
		//Phase 5: Call zest_EndFrame without a corresponding zest_BeginFrame
		zest_buffer_resource_info_t buffer_info = {};
		buffer_info.size = sizeof(float) * 1024;
		int phase_total = 0;
		int phase_passed = 0;
		zest_frame_graph frame_graph = NULL;
		zest_UpdateDevice(tests->device);
		zest_EndFrame(tests->context, 0);
		zest_EndFrame(tests->context, 0);
		phase_passed = (zest_GetValidationErrorCount(tests->context) == 1);
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
		PrintTestUpdate(test, 5, phase_passed == phase_total);
	}

	{
		//Phase 6: Try and connect an invalid resource
		zest_buffer_resource_info_t buffer_info = {};
		buffer_info.size = sizeof(float) * 1024;
		int phase_total = 0;
		int phase_passed = 0;
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			zest_BeginRenderPass("Draw Nothing"); {
				zest_SetPassTask(zest_EmptyRenderPass, 0);
				zest_ConnectInput((zest_resource_node)&phase_total);
				zest_ConnectOutput((zest_resource_node)&phase_total);
				zest_EndPass();
			}
			frame_graph = zest_EndFrameGraph();
			zest_FlushFrameGraph(frame_graph);
		}
		phase_passed = (zest_GetValidationErrorCount(tests->context) == 2);
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
		PrintTestUpdate(test, 6, phase_passed == phase_total);
	}

	test->result |= (passed_tests != total_tests);
	test->frame_count++;
	return test->result;
}

//Test frame graph state errors: nested BeginFrameGraph, EndFrameGraph without Begin, pass outside scope
int test__frame_graph_state_errors(ZestTests *tests, Test *test) {
	int passed_tests = 0;
	int total_tests = 0;

	{
		//Phase 1: Call BeginFrameGraph twice without EndFrameGraph
		int phase_total = 0;
		int phase_passed = 0;
		zest_UpdateDevice(tests->device);
		if (zest_BeginFrame(tests->context)) {
			zest_BeginFrameGraph(tests->context, "First Graph", 0);
			zest_BeginFrameGraph(tests->context, "Second Graph", 0);
			zest_EndFrame(tests->context, 0);
		}
		phase_passed = (zest_GetValidationErrorCount(tests->context) >= 1);
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
		PrintTestUpdate(test, 1, phase_passed == phase_total);
	}

	zest_ResetValidationErrors(tests->device);

	{
		//Phase 2: Call EndFrameGraph without BeginFrameGraph
		int phase_total = 0;
		int phase_passed = 0;
		zest_UpdateDevice(tests->device);
		zest_frame_graph frame_graph = 0;
		if (zest_BeginFrame(tests->context)) {
			frame_graph = zest_EndFrameGraph();
			zest_EndFrame(tests->context, frame_graph);
		}
		phase_passed = !frame_graph;
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
		PrintTestUpdate(test, 2, phase_passed == phase_total);
	}

	zest_ResetValidationErrors(tests->device);

	{
		//Phase 3: Call BeginRenderPass outside frame graph scope
		int phase_total = 0;
		int phase_passed = 0;
		zest_UpdateDevice(tests->device);
		zest_pass_node pass = 0;
		if (zest_BeginFrame(tests->context)) {
			pass = zest_BeginRenderPass("Orphan Pass");
			zest_EndFrame(tests->context, 0);
		}
		phase_passed = !pass;
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
		PrintTestUpdate(test, 3, phase_passed == phase_total);
	}

	test->result |= (passed_tests != total_tests);
	test->frame_count++;
	return test->result;
}

//Test that a pass without a task callback is reported
int test__pass_without_task(ZestTests *tests, Test *test) {
	int passed_tests = 0;
	int total_tests = 0;

	{
		int phase_total = 0;
		int phase_passed = 0;
		zest_UpdateDevice(tests->device);
		if (zest_BeginFrame(tests->context)) {
			zest_frame_graph frame_graph = NULL;
			if (zest_BeginFrameGraph(tests->context, "Taskless Pass Test", 0)) {
				zest_ImportSwapchainResource();
				zest_BeginRenderPass("No Task Pass"); {
					zest_ConnectSwapChainOutput();
					//Deliberately omit zest_SetPassTask
					zest_EndPass();
				}
				frame_graph = zest_EndFrameGraph();
			}
			zest_EndFrame(tests->context, frame_graph);
		}
		zest_PrintReports(tests->context);
		phase_passed = (zest_ReportCount(tests->context) >= 3);
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
	}

	test->result |= (passed_tests != total_tests);
	test->frame_count++;
	return test->result;
}

//Test that an imported resource that is never connected is reported
int test__unused_imported_resource(ZestTests *tests, Test *test) {
	int passed_tests = 0;
	int total_tests = 0;

	{
		int phase_total = 0;
		int phase_passed = 0;
		zest_UpdateDevice(tests->device);
		if (zest_BeginFrame(tests->context)) {
			zest_frame_graph frame_graph = NULL;
			if (zest_BeginFrameGraph(tests->context, "Unused Resource Test", 0)) {
				zest_ImportSwapchainResource();
				//Create a transient image but never connect it
				zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
				info.width = 256;
				info.height = 256;
				zest_resource_node unused_resource = zest_AddTransientImageResource("Unused Image", &info);

				zest_BeginRenderPass("Draw Pass"); {
					zest_ConnectSwapChainOutput();
					zest_SetPassTask(zest_EmptyRenderPass, 0);
					zest_EndPass();
				}
				frame_graph = zest_EndFrameGraph();
			}
			zest_EndFrame(tests->context, frame_graph);
		}
		//Should report about the unused/culled resource
		phase_passed = (zest_ReportCount(tests->context) >= 1);
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
	}

	test->result |= (passed_tests != total_tests);
	test->frame_count++;
	return test->result;
}

//Test that importing swapchain but never outputting to it is reported
int test__unused_swapchain(ZestTests *tests, Test *test) {
	int passed_tests = 0;
	int total_tests = 0;

	{
		int phase_total = 0;
		int phase_passed = 0;
		zest_UpdateDevice(tests->device);
		if (zest_BeginFrame(tests->context)) {
			zest_frame_graph frame_graph = NULL;
			if (zest_BeginFrameGraph(tests->context, "Unused Swapchain Test", 0)) {
				zest_ImportSwapchainResource();
				//Import swapchain but never connect it as output
				zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
				info.width = 256;
				info.height = 256;
				zest_resource_node output = zest_AddTransientImageResource("Output Image", &info);
				zest_FlagResourceAsEssential(output);

				zest_BeginRenderPass("Draw Pass"); {
					zest_ConnectOutput(output);
					zest_SetPassTask(zest_EmptyRenderPass, 0);
					zest_EndPass();
				}
				frame_graph = zest_EndFrameGraph();
			}
			zest_EndFrame(tests->context, frame_graph);
		}
		//Should report about the unused swapchain
		zest_PrintReports(tests->context);
		phase_passed = (zest_ReportCount(tests->context) >= 3);
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
	}

	test->result |= (passed_tests != total_tests);
	test->frame_count++;
	return test->result;
}

//Test that buffer outputs in render passes are allowed (valid use case for storage buffers, etc)
int test__buffer_output_in_render_pass(ZestTests *tests, Test *test) {
	int passed_tests = 0;
	int total_tests = 0;

	{
		//Phase 1: Connect a buffer as output in a render pass - this should be allowed
		int phase_total = 0;
		int phase_passed = 0;
		zest_UpdateDevice(tests->device);
		if (zest_BeginFrame(tests->context)) {
			zest_frame_graph frame_graph = NULL;
			if (zest_BeginFrameGraph(tests->context, "Buffer Output Test", 0)) {
				zest_ImportSwapchainResource();
				zest_buffer_resource_info_t buffer_info = {};
				buffer_info.size = sizeof(float) * 1024;
				zest_resource_node buffer_resource = zest_AddTransientBufferResource("Buffer", &buffer_info);
				zest_FlagResourceAsEssential(buffer_resource);

				zest_BeginRenderPass("Draw Pass"); {
					zest_ConnectSwapChainOutput();
					//Buffer output in render pass is valid (e.g., storage buffer writes)
					zest_ConnectOutput(buffer_resource);
					zest_SetPassTask(zest_EmptyRenderPass, 0);
					zest_EndPass();
				}
				frame_graph = zest_EndFrameGraph();
			}
			zest_EndFrame(tests->context, frame_graph);
		}
		//Should compile and execute without errors
		phase_passed = (zest_GetValidationErrorCount(tests->context) == 0);
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
	}

	test->result |= (passed_tests != total_tests);
	test->frame_count++;
	return test->result;
}

//Test importing swapchain multiple times in one graph
int test__multiple_swapchain_imports(ZestTests *tests, Test *test) {
	int passed_tests = 0;
	int total_tests = 0;

	{
		int phase_total = 0;
		int phase_passed = 0;
		zest_UpdateDevice(tests->device);
		if (zest_BeginFrame(tests->context)) {
			zest_frame_graph frame_graph = NULL;
			if (zest_BeginFrameGraph(tests->context, "Multiple Swapchain Test", 0)) {
				zest_ImportSwapchainResource();
				zest_ImportSwapchainResource(); //Second import - should error

				zest_BeginRenderPass("Draw Pass"); {
					zest_ConnectSwapChainOutput();
					zest_SetPassTask(zest_EmptyRenderPass, 0);
					zest_EndPass();
				}
				frame_graph = zest_EndFrameGraph();
			}
			zest_EndFrame(tests->context, frame_graph);
		}
		phase_passed = (zest_GetValidationErrorCount(tests->context) >= 1);
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
	}

	test->result |= (passed_tests != total_tests);
	test->frame_count++;
	return test->result;
}

//Test that the frame graph compiler correctly reorders passes for transient resource dependencies
int test__transient_dependency_ordering(ZestTests *tests, Test *test) {
	int passed_tests = 0;
	int total_tests = 0;

	{
		int phase_total = 0;
		int phase_passed = 0;
		zest_UpdateDevice(tests->device);
		if (zest_BeginFrame(tests->context)) {
			zest_frame_graph frame_graph = NULL;
			if (zest_BeginFrameGraph(tests->context, "Dependency Ordering Test", 0)) {
				zest_ImportSwapchainResource();

				//Create transient image resource
				zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
				info.width = 256;
				info.height = 256;
				zest_resource_node intermediate = zest_AddTransientImageResource("Intermediate", &info);

				//Pass A: reads the intermediate (declared first, but should execute second)
				zest_BeginRenderPass("Pass A - Reader"); {
					zest_ConnectInput(intermediate);
					zest_ConnectSwapChainOutput();
					zest_SetPassTask(zest_EmptyRenderPass, 0);
					zest_EndPass();
				}

				//Pass B: writes the intermediate (declared second, but should execute first)
				zest_BeginRenderPass("Pass B - Writer"); {
					zest_ConnectOutput(intermediate);
					zest_SetPassTask(zest_EmptyRenderPass, 0);
					zest_EndPass();
				}

				frame_graph = zest_EndFrameGraph();
			}
			zest_EndFrame(tests->context, frame_graph);
		}
		//Should compile without errors - compiler should reorder correctly
		phase_passed = (zest_GetValidationErrorCount(tests->context) == 0);
		phase_total += 1;
		passed_tests += phase_passed;
		total_tests += phase_total;
	}

	test->result |= (passed_tests != total_tests);
	test->frame_count++;
	return test->result;
}

int test__resources_with_same_name(ZestTests *tests, Test *test) {
	int passed_tests = 0;
	int total_tests = 0;

	{
		zest_buffer_resource_info_t buffer_info = {};
		buffer_info.size = sizeof(float) * 1024;
		zest_image_resource_info_t image_info = {zest_format_r8g8b8a8_unorm};
		image_info.width = 256;
		image_info.height = 256;
		zest_UpdateDevice(tests->device);
		if (zest_BeginFrame(tests->context)) {
			zest_frame_graph frame_graph = NULL;
			if (zest_BeginFrameGraph(tests->context, "Multiple Resources with the same name test", 0)) {
				zest_ImportSwapchainResource();
				zest_resource_node buffer_a = zest_AddTransientBufferResource("Buffer", &buffer_info);
				zest_resource_node buffer_b = zest_AddTransientBufferResource("Buffer", &buffer_info);
				zest_resource_node image_a = zest_AddTransientImageResource("Image", &image_info);
				zest_resource_node image_b = zest_AddTransientImageResource("Image", &image_info);

				frame_graph = zest_EndFrameGraph();
			}
			zest_EndFrame(tests->context, frame_graph);
		}
		total_tests += 1;
	}

	zest_uint validation_count = zest_GetValidationErrorCount(tests->context);
	test->result |= (validation_count == 0);
	test->frame_count++;
	return test->result;
}

int test__add_2_resources_the_same(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->texture)) {
		zest_image_info_t image_info = zest_CreateImageInfo(256, 256);
		image_info.flags = zest_image_preset_storage;
		tests->texture = zest_CreateImage(tests->device, &image_info);
	}

	int passed_tests = 0;
	int total_tests = 0;

	{
		zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_gpu_only);
		zest_buffer buffer = zest_CreateBuffer(tests->device, 1024, &buffer_info);
		zest_image_resource_info_t image_info = {zest_format_r8g8b8a8_unorm};
		image_info.width = 256;
		image_info.height = 256;
		zest_UpdateDevice(tests->device);
		zest_image image = zest_GetImage(tests->texture);
		if (zest_BeginFrame(tests->context)) {
			zest_frame_graph frame_graph = NULL;
			if (zest_BeginFrameGraph(tests->context, "2 Resources the same test", 0)) {
				zest_ImportSwapchainResource();
				zest_resource_node buffer_a = zest_ImportBufferResource("Buffer a", buffer, 0);
				zest_resource_node buffer_b = zest_ImportBufferResource("Buffer b", buffer, 0);
				zest_resource_node image_a = zest_ImportImageResource("Image a", image, 0);
				zest_resource_node image_b = zest_ImportImageResource("Image b", image, 0);

				if (buffer_b == NULL && image_b == NULL) {
					passed_tests++;
				}

				frame_graph = zest_EndFrameGraph();
			}
			zest_EndFrame(tests->context, frame_graph);
		}
		total_tests += 1;
	}

	test->result |= (passed_tests != total_tests);
	test->frame_count++;
	return test->result;
}

int test__acquire_release_resources(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->texture)) {
		zest_image_info_t image_info = zest_CreateImageInfo(256, 256);
		image_info.flags = zest_image_preset_storage;
		tests->texture = zest_CreateImage(tests->device, &image_info);
	}

	zest_image image = zest_GetImage(tests->texture);

	zest_uint indexes[2];
	indexes[0] = zest_AcquireSampledImageIndex(tests->device, image, zest_texture_2d_binding);
	indexes[1] = zest_AcquireSampledImageIndex(tests->device, image, zest_texture_array_binding);

	zest_ReleaseImageIndex(tests->device, image, zest_texture_2d_binding);
	zest_ReleaseImageIndex(tests->device, image, zest_texture_array_binding);
	zest_ReleaseImageIndex(tests->device, image, zest_texture_2d_binding);
	zest_ReleaseImageIndex(tests->device, image, zest_texture_array_binding);

	zest_ReleaseBindlessIndex(tests->device, indexes[0], zest_texture_2d_binding);
	zest_ReleaseBindlessIndex(tests->device, indexes[1], zest_texture_array_binding);

	zest_ReleaseBindlessIndex(tests->device, 10, zest_texture_2d_binding);
	zest_ReleaseBindlessIndex(tests->device, 11, zest_texture_array_binding);

	zest_ReleaseBindlessIndex(tests->device, 10, zest_texture_2d_binding);
	zest_ReleaseBindlessIndex(tests->device, 11, zest_texture_array_binding);

	zest_uint report_count = zest_ReportCount(tests->context);

	test->result |= (report_count != 4);
	test->frame_count++;
	return test->result;
}
