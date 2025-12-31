#include "zest-tests.h"

//Test that various usage errors are gracefully handled and reported to the user with clear instructions
//on how to fix

//Test zest_BeginFrame zest_UpdateDevice omission.
int test__no_update_device(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
	if (zest_BeginFrame(tests->context)) {
		zest_EndFrame(tests->context);
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
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			zest_BeginRenderPass("Draw Nothing"); {
				zest_ConnectSwapChainOutput();
				zest_SetPassTask(zest_EmptyRenderPass, 0);
				zest_EndPass();
				frame_graph = zest_EndFrameGraph();
				zest_QueueFrameGraphForExecution(tests->context, frame_graph);
			}
		}
		zest_EndFrame(tests->context);
	}
	test->result |= (zest_GetValidationErrorCount(tests->context) != 1);
	test->frame_count++;
	return test->result;
}
