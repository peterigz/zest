#include "zest-tests.h"

void zest_CreateResources(ZestTests *tests) {
	zest_image_info_t image_info = zest_CreateImageInfo(256, 256);
	image_info.flags = zest_image_preset_texture;
	zest_buffer_info_t vertex_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_gpu_only);
	StressResources *resources = &tests->stress_resources;
	for (int i = 0; i != MAX_TEST_RESOURCES; i++) {
		resources->images[i] = zest_CreateImage(tests->context, &image_info);
		resources->layers[i] = zest_CreateInstanceLayer(tests->context, "Stress Layer", 32, 1000);
		resources->buffers[i] = zest_CreateBuffer(tests->context, 1024 * 1024, &vertex_info);
	}
}

int test__stress_pass_chain(ZestTests *tests, Test *test) {
	zest_resource_node image_resources[MAX_TEST_RESOURCES];
	zest_resource_node buffer_resources[MAX_TEST_RESOURCES];
	StressResources *resources = &tests->stress_resources;
	zest_CreateResources(tests);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			for (int i = 0; i != MAX_TEST_RESOURCES; i++) {
				char buffer[32];
				snprintf(buffer, sizeof(buffer), "Image %i", i);
				image_resources[i] = zest_ImportImageResource(buffer, resources->images[i], 0);
				snprintf(buffer, sizeof(buffer), "Buffer %i", i);
				buffer_resources[i] = zest_ImportBufferResource(buffer, resources->buffers[i], 0);
			}
			zest_ImportSwapchainResource();

			for (int i = 0; i != MAX_TEST_RESOURCES; i++) {
				zest_BeginRenderPass("Pass");
				zest_ConnectInput(image_resources[i]);
				zest_ConnectInput(buffer_resources[i]);
				zest_ConnectSwapChainOutput();
				zest_SetPassTask(zest_EmptyRenderPass, 0);
				zest_EndPass();
			}

			frame_graph = zest_EndFrameGraph();
			ZEST_PRINT("Frame graph compiled in %llu microseconds", frame_graph->compile_time);
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
		}
		zest_EndFrame(tests->context);
		ZEST_PRINT("Frame graph executed in %llu microseconds", frame_graph->execute_time);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}