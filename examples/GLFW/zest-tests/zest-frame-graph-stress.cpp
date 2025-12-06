#include "zest-tests.h"

void zest_CreateImageResources(ZestTests *tests) {
	zest_image_info_t image_info = zest_CreateImageInfo(256, 256);
	image_info.flags = zest_image_preset_color_attachment;
	StressResources *resources = &tests->stress_resources;
	for (int i = resources->image_count; i != MAX_TEST_RESOURCES; i++) {
		resources->images[i] = zest_CreateImage(tests->context, &image_info);
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

int test__stress_pass_dependencies(ZestTests *tests, Test *test) {
	zest_resource_node image_resources[MAX_TEST_RESOURCES];
	zest_resource_node buffer_resources[MAX_TEST_RESOURCES];
	StressResources *resources = &tests->stress_resources;
	zest_CreateImageResources(tests);
	zest_CreateBufferResources(tests);
	char name_string[MAX_TEST_RESOURCES * 2][16];
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			for (int i = 0; i != MAX_TEST_RESOURCES; i++) {
				snprintf(name_string[i], sizeof(name_string[i]), "Image %i", i);
				image_resources[i] = zest_ImportImageResource(name_string[i], resources->images[i], 0);
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
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			for (int i = 0; i != MAX_TEST_RESOURCES; i++) {
				snprintf(name_string[i], sizeof(name_string[i]), "Image %i", i);
				image_resources[i] = zest_ImportImageResource(name_string[i], resources->images[i], 0);
				snprintf(name_string[i + MAX_TEST_RESOURCES], sizeof(name_string), "Buffer %i", i);
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
		zest_GetBindlessSet(command_list->context)
	};

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, command_list->pass_node->compute->handle, sets, 1);

	TestPushConstants push;

	// Update push constants for the current dispatch
	// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
	push.index1 = zest_GetTransientBufferBindlessIndex(command_list, write_buffer);
	if (push.index1 == ZEST_INVALID) return;

	zest_cmd_SendCustomComputePushConstants(command_list, command_list->pass_node->compute->handle, &push);

	zest_uint size = write_buffer->storage_buffer->size / sizeof(TestData);
	zest_uint group_count_x = (size + local_size_x - 1) / local_size_x;

	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, command_list->pass_node->compute->handle, group_count_x, 1, 1);
}

int test__stress_transient_buffers(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->compute_write)) {
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/buffer_write.comp", "buffer_write.spv", zest_compute_shader, 1);
		zest_compute_builder_t builder = zest_BeginComputeBuilder(tests->context);
		zest_SetComputeBindlessLayout(&builder, zest_GetBindlessLayout(tests->context));
		zest_SetComputeUserData(&builder, tests);
		zest_AddComputeShader(&builder, shader);
		zest_SetComputePushConstantSize(&builder, sizeof(TestPushConstants));
		tests->compute_write = zest_FinishCompute(&builder, "Buffer Write");
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
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			for (int i = 0; i != MAX_TEST_RESOURCES; i++) {
				zest_size random_size = rand() % 100000 + 1000;
				info.size = sizeof(TestData) * random_size;
				snprintf(name_string[i], sizeof(name_string), "Buffer %i", i);
				buffer_resources[i] = zest_ImportBufferResource(name_string[i], resources->buffers[i], 0);
				snprintf(name_string[i + MAX_TEST_RESOURCES], sizeof(name_string), "Transient %i", i);
				transient_resources[i] = zest_AddTransientBufferResource(name_string[i + MAX_TEST_RESOURCES], &info);
			}
			zest_ImportSwapchainResource();

			int mid_point = MAX_TEST_RESOURCES / 2;

			for (int i = 0; i != MAX_TEST_RESOURCES; i += 2) {
				zest_BeginComputePass(tests->compute_write, "Pass A");
				zest_ConnectInput(buffer_resources[i]);
				zest_ConnectOutput(transient_resources[i]);
				zest_SetPassTask(zest_WriteBufferStressCompute, transient_resources[i]);
				zest_EndPass();

				zest_BeginComputePass(tests->compute_write, "Pass B");
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
		zest_compute_builder_t builder = zest_BeginComputeBuilder(tests->context);
		zest_SetComputeBindlessLayout(&builder, zest_GetBindlessLayout(tests->context));
		zest_SetComputeUserData(&builder, tests);
		zest_AddComputeShader(&builder, shader);
		zest_SetComputePushConstantSize(&builder, sizeof(TestPushConstants));
		tests->compute_write = zest_FinishCompute(&builder, "Buffer Write");
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
				snprintf(name_string[i], sizeof(name_string), "Image %i", i);
				buffer_resources[i] = zest_ImportBufferResource(name_string[i], resources->buffers[i], 0);
				snprintf(name_string[i + MAX_TEST_RESOURCES], sizeof(name_string), "Transient %i", i);
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
		zest_compute_builder_t builder = zest_BeginComputeBuilder(tests->context);
		zest_SetComputeBindlessLayout(&builder, zest_GetBindlessLayout(tests->context));
		zest_SetComputeUserData(&builder, tests);
		zest_AddComputeShader(&builder, shader);
		zest_SetComputePushConstantSize(&builder, sizeof(TestPushConstants));
		tests->compute_write = zest_FinishCompute(&builder, "Buffer Write");
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
				snprintf(name_string[i + MAX_TEST_RESOURCES], sizeof(name_string), "TransientI %i", i);
				transient_images[i] = zest_AddTransientImageResource(name_string[i + MAX_TEST_RESOURCES], &image_info);
				snprintf(name_string[i + MAX_TEST_RESOURCES * 2], sizeof(name_string), "TransientB %i", i);
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

//Next test, infinite loop of passes