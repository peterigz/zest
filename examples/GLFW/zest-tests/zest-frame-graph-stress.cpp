#include "zest-tests.h"

void zest_CreateImageResources(ZestTests *tests) {
	zest_image_info_t image_info = zest_CreateImageInfo(256, 256);
	image_info.flags = zest_image_preset_color_attachment;
	StressResources *resources = &tests->stress_resources;
	for (int i = resources->image_count; i != MAX_TEST_RESOURCES; i++) {
		resources->images[i] = zest_CreateImage(tests->context, &image_info);
		zest_image image = zest_GetImage(resources->images[i]);
		ZEST_ASSERT_HANDLE(image);
		resources->image_count++;
	}
}

void zest_CreateBufferResources(ZestTests *tests) {
	zest_buffer_info_t vertex_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_gpu_only);
	StressResources *resources = &tests->stress_resources;
	for (int i = resources->buffer_count; i != MAX_TEST_RESOURCES; i++) {
		resources->buffers[i] = zest_CreateBuffer(tests->context, 1024 * 1024, &vertex_info);
		resources->buffer_count++;
	}
}

int test__stress_simple(ZestTests *tests, Test *test) {
	zest_resource_node image_resources[MAX_TEST_RESOURCES];
	zest_resource_node buffer_resources[MAX_TEST_RESOURCES];
	StressResources *resources = &tests->stress_resources;
	zest_CreateImageResources(tests);
	zest_CreateBufferResources(tests);
	char name_string[MAX_TEST_RESOURCES][16];
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			for (int i = 0; i != MAX_TEST_RESOURCES; i++) {
				char buffer[32];
				snprintf(buffer, sizeof(buffer), "Image %i", i);
				image_resources[i] = zest_ImportImageResource(buffer, zest_GetImage(resources->images[i]), 0);
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

int test__stress_pass_dependencies(ZestTests *tests, Test *test) {
	zest_resource_node image_resources[MAX_TEST_RESOURCES];
	zest_resource_node buffer_resources[MAX_TEST_RESOURCES];
	StressResources *resources = &tests->stress_resources;
	zest_CreateImageResources(tests);
	zest_CreateBufferResources(tests);
	char name_string[MAX_TEST_RESOURCES * 2][16];
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			for (int i = 0; i != MAX_TEST_RESOURCES; i++) {
				snprintf(name_string[i], sizeof(name_string[i]), "Image %i", i);
				image_resources[i] = zest_ImportImageResource(name_string[i], zest_GetImage(resources->images[i]), 0);
				snprintf(name_string[i + MAX_TEST_RESOURCES], sizeof(name_string), "Buffer %i", i);
				buffer_resources[i] = zest_ImportBufferResource(name_string[i + MAX_TEST_RESOURCES], resources->buffers[i], 0);
			}
			zest_ImportSwapchainResource();

			int mid_point = MAX_TEST_RESOURCES / 2;

			for (int i = 0; i != mid_point; i++) {
				zest_BeginRenderPass("Pass A");
				zest_ConnectInput(image_resources[i]);
				zest_ConnectInput(buffer_resources[i]);
				zest_ConnectOutput(image_resources[i + mid_point]);
				zest_ConnectOutput(buffer_resources[i + mid_point]);
				zest_SetPassTask(zest_EmptyRenderPass, 0);
				zest_EndPass();
			}

			for (int i = mid_point; i != MAX_TEST_RESOURCES; i++) {
				zest_BeginRenderPass("Pass B");
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
		//zest_PrintCompiledFrameGraph(frame_graph);
		ZEST_PRINT("Frame graph executed in %llu microseconds", frame_graph->execute_time);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

int test__stress_pass_dependency_chain(ZestTests *tests, Test *test) {
	zest_resource_node image_resources[MAX_TEST_RESOURCES];
	zest_resource_node buffer_resources[MAX_TEST_RESOURCES];
	StressResources *resources = &tests->stress_resources;
	zest_CreateImageResources(tests);
	zest_CreateBufferResources(tests);
	char name_string[MAX_TEST_RESOURCES * 2][16];
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			for (int i = 0; i != MAX_TEST_RESOURCES; i++) {
				snprintf(name_string[i], 16, "Image %i", i);
				image_resources[i] = zest_ImportImageResource(name_string[i], zest_GetImage(resources->images[i]), 0);
				snprintf(name_string[i + MAX_TEST_RESOURCES], 16, "Buffer %i", i);
				buffer_resources[i] = zest_ImportBufferResource(name_string[i + MAX_TEST_RESOURCES], resources->buffers[i], 0);
			}
			zest_ImportSwapchainResource();

			int mid_point = MAX_TEST_RESOURCES / 2;

			for (int i = 0; i != MAX_TEST_RESOURCES; i++) {
				zest_BeginRenderPass("Pass A");
				zest_ConnectOutput(image_resources[i]);
				zest_ConnectOutput(buffer_resources[i]);
				zest_SetPassTask(zest_EmptyRenderPass, 0);
				zest_EndPass();

				zest_BeginRenderPass("Pass B");
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
		//zest_PrintCompiledFrameGraph(frame_graph);
		ZEST_PRINT("Frame graph executed in %llu microseconds", frame_graph->execute_time);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

void zest_WriteBufferStressCompute(const zest_command_list command_list, void *user_data) {
	zest_resource_node write_buffer = (zest_resource_node)user_data;
	ZEST_ASSERT_HANDLE(write_buffer);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_descriptor_set sets[] = {
		zest_GetBindlessSet(command_list->device)
	};

	zest_compute compute = zest_GetCompute(command_list->pass_node->compute->handle);

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, compute);

	TestPushConstants push;

	// Update push constants for the current dispatch
	// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
	push.index1 = zest_GetTransientBufferBindlessIndex(command_list, write_buffer);
	if (push.index1 == ZEST_INVALID) return;

	zest_cmd_SendPushConstants(command_list, &push, sizeof(TestPushConstants));

	zest_uint size = write_buffer->storage_buffer->size / sizeof(TestData);
	zest_uint group_count_x = (size + local_size_x - 1) / local_size_x;

	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, group_count_x, 1, 1);
}

int test__stress_transient_buffers(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->compute_write)) {
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/buffer_write.comp", "buffer_write.spv", zest_compute_shader, 1);
		tests->compute_write = zest_CreateCompute(tests->device, "Buffer Write", shader, tests);
		if (!zest_IsValidHandle((void*)&tests->compute_write)) {
			test->frame_count++;
			test->result = -1;
			return test->result;
		}
	}

	zest_resource_node transient_resources[MAX_TEST_RESOURCES];
	zest_resource_node buffer_resources[MAX_TEST_RESOURCES];
	StressResources *resources = &tests->stress_resources;
	zest_CreateImageResources(tests);
	zest_CreateBufferResources(tests);
	char name_string[MAX_TEST_RESOURCES * 2][16];
	zest_buffer_resource_info_t info = {};
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			for (int i = 0; i != MAX_TEST_RESOURCES; i++) {
				zest_size random_size = rand() % 100000 + 1000;
				info.size = sizeof(TestData) * random_size;
				snprintf(name_string[i], 16, "Buffer %i", i);
				buffer_resources[i] = zest_ImportBufferResource(name_string[i], resources->buffers[i], 0);
				snprintf(name_string[i + MAX_TEST_RESOURCES], 16, "Transient %i", i);
				transient_resources[i] = zest_AddTransientBufferResource(name_string[i + MAX_TEST_RESOURCES], &info);
			}
			zest_ImportSwapchainResource();

			int mid_point = MAX_TEST_RESOURCES / 2;

		zest_compute compute_write = zest_GetCompute(tests->compute_write);

			for (int i = 0; i != MAX_TEST_RESOURCES; i += 2) {
				zest_BeginComputePass(compute_write, "Pass A");
				zest_ConnectInput(buffer_resources[i]);
				zest_ConnectOutput(transient_resources[i]);
				zest_SetPassTask(zest_WriteBufferStressCompute, transient_resources[i]);
				zest_EndPass();

				zest_BeginComputePass(compute_write, "Pass B");
				zest_ConnectInput(transient_resources[i]);
				zest_ConnectOutput(transient_resources[i + 1]);
				zest_SetPassTask(zest_WriteBufferStressCompute, transient_resources[i + 1]);
				zest_EndPass();

				zest_BeginRenderPass("Pass C");
				zest_ConnectInput(transient_resources[i + 1]);
				zest_ConnectSwapChainOutput();
				zest_SetPassTask(zest_EmptyRenderPass, 0);
				zest_EndPass();
			}

			frame_graph = zest_EndFrameGraph();
			ZEST_PRINT("Frame graph compiled in %llu microseconds", frame_graph->compile_time);
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
		}
		zest_EndFrame(tests->context);
		//zest_PrintCompiledFrameGraph(frame_graph);
		ZEST_PRINT("Frame graph executed in %llu microseconds", frame_graph->execute_time);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

int test__stress_transient_images(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->compute_write)) {
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/buffer_write.comp", "buffer_write.spv", zest_compute_shader, 1);
		tests->compute_write = zest_CreateCompute(tests->device, "Buffer Write", shader, tests);
		if (!zest_IsValidHandle((void*)&tests->compute_write)) {
			test->frame_count++;
			test->result = -1;
			return test->result;
		}
	}

	zest_resource_node transient_resources[MAX_TEST_RESOURCES];
	zest_resource_node buffer_resources[MAX_TEST_RESOURCES];
	StressResources *resources = &tests->stress_resources;
	zest_CreateImageResources(tests);
	zest_CreateBufferResources(tests);
	char name_string[MAX_TEST_RESOURCES * 2][16];
	zest_buffer_resource_info_t info = {};
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			for (int i = 0; i != MAX_TEST_RESOURCES; i++) {
				zest_uint random_width = rand() % 1024 + 256;
				zest_uint random_height = rand() % 1024 + 256;
				zest_image_resource_info_t image_info = {
					zest_format_r8g8b8a8_uint,
					zest_resource_usage_hint_copyable,
					random_width, random_height, 1
				};
				snprintf(name_string[i], 16, "Image %i", i);
				buffer_resources[i] = zest_ImportBufferResource(name_string[i], resources->buffers[i], 0);
				snprintf(name_string[i + MAX_TEST_RESOURCES], 16, "Transient %i", i);
				transient_resources[i] = zest_AddTransientImageResource(name_string[i + MAX_TEST_RESOURCES], &image_info);
			}
			zest_ImportSwapchainResource();

			int mid_point = MAX_TEST_RESOURCES / 2;

			for (int i = 0; i != MAX_TEST_RESOURCES; i += 2) {
				zest_BeginRenderPass("Pass A");
				zest_ConnectInput(buffer_resources[i]);
				zest_ConnectOutput(transient_resources[i]);
				zest_SetPassTask(zest_EmptyRenderPass, transient_resources[i]);
				zest_EndPass();

				zest_BeginRenderPass("Pass B");
				zest_ConnectInput(transient_resources[i]);
				zest_ConnectOutput(transient_resources[i + 1]);
				zest_SetPassTask(zest_EmptyRenderPass, transient_resources[i + 1]);
				zest_EndPass();

				zest_BeginRenderPass("Pass C");
				zest_ConnectInput(transient_resources[i + 1]);
				zest_ConnectSwapChainOutput();
				zest_SetPassTask(zest_EmptyRenderPass, 0);
				zest_EndPass();
			}

			frame_graph = zest_EndFrameGraph();
			ZEST_PRINT("Frame graph compiled in %llu microseconds", frame_graph->compile_time);
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
		}
		zest_EndFrame(tests->context);
		//zest_PrintCompiledFrameGraph(frame_graph);
		ZEST_PRINT("Frame graph executed in %llu microseconds", frame_graph->execute_time);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

int test__stress_all_transients(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->compute_write)) {
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/buffer_write.comp", "buffer_write.spv", zest_compute_shader, 1);
		tests->compute_write = zest_CreateCompute(tests->device, "Buffer Write", shader, tests);
		if (!zest_IsValidHandle((void*)&tests->compute_write)) {
			test->frame_count++;
			test->result = -1;
			return test->result;
		}
	}

	zest_resource_node transient_images[MAX_TEST_RESOURCES];
	zest_resource_node transient_buffers[MAX_TEST_RESOURCES];
	zest_resource_node buffer_resources[MAX_TEST_RESOURCES];
	StressResources *resources = &tests->stress_resources;
	zest_CreateImageResources(tests);
	zest_CreateBufferResources(tests);
	char name_string[MAX_TEST_RESOURCES * 3][16];
	zest_buffer_resource_info_t info = {};
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			for (int i = 0; i != MAX_TEST_RESOURCES; i++) {
				zest_size random_size = rand() % 100000 + 1000;
				info.size = sizeof(TestData) * random_size;
				zest_uint random_width = rand() % 1024 + 256;
				zest_uint random_height = rand() % 1024 + 256;
				zest_image_resource_info_t image_info = {
					zest_format_r8g8b8a8_uint,
					zest_resource_usage_hint_copyable,
					random_width, random_height, 1
				};
				snprintf(name_string[i], sizeof(name_string), "Image %i", i);
				buffer_resources[i] = zest_ImportBufferResource(name_string[i], resources->buffers[i], 0);
				snprintf(name_string[i + MAX_TEST_RESOURCES], 16, "TransientI %i", i);
				transient_images[i] = zest_AddTransientImageResource(name_string[i + MAX_TEST_RESOURCES], &image_info);
				snprintf(name_string[i + MAX_TEST_RESOURCES * 2], 16, "TransientB %i", i);
				transient_buffers[i] = zest_AddTransientBufferResource(name_string[i + MAX_TEST_RESOURCES * 2], &info);
			}
			zest_ImportSwapchainResource();

			int mid_point = MAX_TEST_RESOURCES / 2;

			for (int i = 0; i != MAX_TEST_RESOURCES; i += 2) {
				zest_BeginRenderPass("Pass A");
				zest_ConnectInput(buffer_resources[i]);
				zest_ConnectOutput(transient_images[i]);
				zest_ConnectOutput(transient_buffers[i]);
				zest_SetPassTask(zest_EmptyRenderPass, transient_images[i]);
				zest_EndPass();

				zest_BeginRenderPass("Pass B");
				zest_ConnectInput(transient_images[i]);
				zest_ConnectInput(transient_buffers[i]);
				zest_ConnectOutput(transient_images[i + 1]);
				zest_ConnectOutput(transient_buffers[i + 1]);
				zest_SetPassTask(zest_EmptyRenderPass, transient_images[i + 1]);
				zest_EndPass();

				zest_BeginRenderPass("Pass C");
				zest_ConnectInput(transient_images[i + 1]);
				zest_ConnectInput(transient_buffers[i + 1]);
				zest_ConnectSwapChainOutput();
				zest_SetPassTask(zest_EmptyRenderPass, 0);
				zest_EndPass();
			}

			frame_graph = zest_EndFrameGraph();
			ZEST_PRINT("Frame graph compiled in %llu microseconds", frame_graph->compile_time);
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
		}
		zest_EndFrame(tests->context);
		//zest_PrintCompiledFrameGraph(frame_graph);
		ZEST_PRINT("Frame graph executed in %llu microseconds", frame_graph->execute_time);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

void zest_StressTransferBuffer(const zest_command_list command_list, void *user_data) {
	zest_resource_node write_buffer = (zest_resource_node)user_data;
	ZEST_ASSERT_HANDLE(write_buffer);
	zest_uint max_index = ZEST__MAX(write_buffer->storage_buffer->size / sizeof(float), 1024);
	float dummy_data[1024];
	for (int i = 0; i != 1024; i++) {
		dummy_data[i] = (float)i;
	}
	zest_buffer staging_buffer = zest_CreateStagingBuffer(command_list->context, sizeof(float) * 1024, dummy_data);
	zest_cmd_CopyBuffer(command_list, staging_buffer, write_buffer->storage_buffer, staging_buffer->size);
	zest_FreeBuffer(staging_buffer);
}

void zest_StressWriteImageCompute(const zest_command_list command_list, void *user_data) {
	zest_resource_node write_buffer = (zest_resource_node)user_data;
	ZEST_ASSERT_HANDLE(write_buffer);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_descriptor_set sets[] = {
		zest_GetBindlessSet(command_list->device)
	};

	zest_compute compute = zest_GetCompute(command_list->pass_node->compute->handle);

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, compute);

	TestPushConstants push;

	// Update push constants for the current dispatch
	// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
	push.index1 = zest_GetTransientSampledImageBindlessIndex(command_list, write_buffer, zest_storage_image_binding);
	if (push.index1 == ZEST_INVALID) return;

	zest_cmd_SendPushConstants(command_list, &push, sizeof(TestPushConstants));

	zest_image_info_t image_desc = zest_GetResourceImageDescription(write_buffer);
	zest_uint group_count_x = (image_desc.extent.width + local_size_x - 1) / local_size_x;
	zest_uint group_count_y = (image_desc.extent.height + local_size_y - 1) / local_size_y;

	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, group_count_x, group_count_y, 1);
}

/*
Multi-Queue Stress Synchronization:
Run multiple independent tasks in parallel on the graphics, compute and transfer queues if available.
All passes generate random sized transient buffers
Feed them all as input into a final pass so that they don't get culled.
*/
int test__stress_multi_queue_sync(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->compute_write)) {
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/image_write2.comp", "image_write.spv", zest_compute_shader, 1);
		tests->compute_write = zest_CreateCompute(tests->device, "Buffer Write", shader, tests);
		if (!zest_IsValidHandle((void*)&tests->compute_write)) {
			test->frame_count++;
			test->result = 1;
			return test->result;
		}
	}

	zest_SetSwapchainClearColor(tests->context, 0.f, .1f, .2f, 1.f);
	zest_image_resource_info_t image_info = { zest_format_r8g8b8a8_unorm };
	zest_buffer_resource_info_t buffer_info = {};
	zest_resource_node transient_images[MAX_TEST_RESOURCES];
	zest_resource_node transient_buffers[MAX_TEST_RESOURCES];
	char name_string[MAX_TEST_RESOURCES * 2][16];
	buffer_info.size = sizeof(float) * 1024;
	zest_buffer_resource_info_t info = {};
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Multi Queue Sync", 0)) {
			zest_ImportSwapchainResource();
			for (int i = 0; i != MAX_TEST_RESOURCES; i++) {
				zest_size random_size = rand() % 100000 + 1000;
				info.size = sizeof(TestData) * random_size;
				zest_uint random_width = rand() % 1024 + 256;
				zest_uint random_height = rand() % 1024 + 256;
				zest_image_resource_info_t image_info = {
					zest_format_r8g8b8a8_uint,
					zest_resource_usage_hint_copyable,
					random_width, random_height, 1
				};
				snprintf(name_string[i], 16, "TransientI %i", i);
				transient_images[i] = zest_AddTransientImageResource(name_string[i], &image_info);
				snprintf(name_string[i + MAX_TEST_RESOURCES], 16, "TransientB %i", i);
				transient_buffers[i] = zest_AddTransientBufferResource(name_string[i + MAX_TEST_RESOURCES], &info);
			}

			zest_compute compute_write = zest_GetCompute(tests->compute_write);

			for (int i = 0; i != MAX_TEST_RESOURCES; i += 2) {
				zest_BeginComputePass(compute_write, "Pass A");
				zest_ConnectOutput(transient_images[i]);
				zest_SetPassTask(zest_StressWriteImageCompute, transient_images[i]);
				zest_EndPass();

				zest_BeginTransferPass("Pass B");
				zest_ConnectOutput(transient_buffers[i]);
				zest_SetPassTask(zest_StressTransferBuffer, transient_buffers[i]);
				zest_EndPass();

				zest_BeginComputePass(compute_write, "Pass C");
				zest_ConnectOutput(transient_buffers[i + 1]);
				zest_SetPassTask(zest_EmptyRenderPass, 0);
				zest_EndPass();

				zest_BeginRenderPass("Pass D");
				zest_ConnectOutput(transient_images[i + 1]);
				zest_SetPassTask(zest_EmptyRenderPass, 0);
				zest_EndPass();

				zest_BeginRenderPass("Pass E");
				zest_SetPassTask(zest_EmptyRenderPass, NULL);
				zest_ConnectInput(transient_images[i]);
				zest_ConnectInput(transient_buffers[i]);
				zest_ConnectInput(transient_buffers[i + 1]);
				zest_ConnectInput(transient_images[i + 1]);
				zest_SetPassTask(zest_EmptyRenderPass, 0);
				zest_ConnectSwapChainOutput();
				zest_EndPass();

			}

			frame_graph = zest_EndFrameGraph();
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
		}
		zest_EndFrame(tests->context);
		//zest_PrintCompiledFrameGraph(frame_graph);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Next test, infinite loop of passes