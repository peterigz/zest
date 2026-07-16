#include "zest-tests.h"

//Empty Graph: Compile and execute an empty render graph. It should do nothing and not crash.
int test__empty_graph(ZestTests *tests, Test *test) {
	if (zest_BeginCommandGraph(tests->context, "Blank Screen", 0)) {
		zest_frame_graph frame_graph = zest_EndFrameGraph();
		test->result |= zest_GetFrameGraphResult(frame_graph);
		zest_FlushFrameGraph(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

//Single Pass - No I/O: A graph with one pass that has no resource inputs or outputs. Should show status no work to do.
int test__single_pass(ZestTests *tests, Test *test) {
	if (zest_BeginCommandGraph(tests->context, "Single Pass Test", 0)) {
		zest_pass_node clear_pass = zest_BeginRenderPass("Empty Pass");
		zest_frame_graph frame_graph = zest_EndFrameGraph();
		test->result |= zest_GetFrameGraphResult(frame_graph);
		zest_FlushFrameGraph(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

//Blank Screen: Compile and execute a blank screen. 
int test__blank_screen(ZestTests *tests, Test *test) {
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			zest_ImportSwapchainResource();
			zest_BeginRenderPass("Draw Nothing");
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_EmptyRenderPass, 0);
			zest_EndPass();
			frame_graph = zest_EndFrameGraph();
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

//Unused Pass Culling: Define two passes, A and B. Pass A writes to a resource, but that resource is never used as an input to Pass B or as a final
//output. Verify that Pass A is culled and never executed.
int test__pass_culling(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Pass Culling", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);

			//This pass should get culled
			zest_BeginRenderPass("Pass A");
			zest_ConnectOutput(output_a);
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			zest_BeginRenderPass("Pass B");
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_ConnectSwapChainOutput();
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= frame_graph ? zest_GetFrameGraphResult(frame_graph) : 1;
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

//Unused Resource Culling: Declare a resource that is never used by any pass. The graph should compile and run without trying to allocate memory for it.
int test__resource_culling(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Resource Culling", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);
			zest_BeginRenderPass("Draw Nothing");
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_ConnectSwapChainOutput();
			zest_EndPass();
			zest_frame_graph frame_graph = zest_EndFrameGraph();
			test->result |= zest_GetFrameGraphCulledResourceCount(frame_graph) > 0 ? 1 : 0;
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= frame_graph ? zest_GetFrameGraphResult(frame_graph) : 1;
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

//Chained Culling: Pass A writes to Resource X. Pass B reads from Resource X and writes to Resource Y. If Resource Y is not used as a final output, both
//Pass A and Pass B should be culled.
int test__chained_pass_culling(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
	
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Chained Pass Culling", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node output_x = zest_AddTransientImageResource("Output X", &info);
			zest_resource_node output_y = zest_AddTransientImageResource("Output Y", &info);

			//This pass should get culled
			zest_pass_node pass_a = zest_BeginRenderPass("Pass A");
			zest_ConnectOutput(output_x);
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			//This pass should also get culled
			zest_BeginRenderPass("Pass B");
			zest_ConnectInput(output_x);
			zest_ConnectOutput(output_y);
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			zest_BeginRenderPass("Pass C");
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
		}
		zest_EndFrame(tests->context, frame_graph);
		if (frame_graph && zest_GetFrameGraphCulledPassesCount(frame_graph) == 2) {
			test->result |= zest_GetFrameGraphResult(frame_graph);
		}
		test->result |= zest_GetValidationErrorCount(tests->device);
	}
	test->frame_count++;
	return test->result;
}

//Transient Resource Test: Pass A writes to a transient texture. Pass B reads from that texture. Verify the data is passed correctly. The graph should
//manage the creation and destruction of the transient texture in the appropriate passes.
int test__transient_image(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Transient Image", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);

			zest_pass_node pass_a = zest_BeginRenderPass("Pass A");
			zest_ConnectOutput(output_a);
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			zest_pass_node pass_b = zest_BeginRenderPass("Pass B");
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_ConnectInput(output_a);
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
			test->result |= zest_GetFrameGraphResult(frame_graph);
			if (zest_GetFrameGraphFinalPassCount(frame_graph) == 2) {
				zest_uint create_size = zest_GetFrameGraphPassTransientCreateCount(frame_graph, zest_GetPassOutputKey(pass_a));
				if (create_size != 1) {
					test->result = 1;
				}
				zest_uint free_size = zest_GetFrameGraphPassTransientFreeCount(frame_graph, zest_GetPassOutputKey(pass_b));
				if (free_size != 1) {
					test->result = 1;
				}
			} else {
				test->result = 1;
			}
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

//Input/Output Test: A pass that takes a pre-existing resource (e.g., a user-created buffer) as input and writes to a 
//final output resource (e.g., the swapchain).
int test__import_image(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->texture)) {
		zest_image_info_t image_info = zest_CreateImageInfo(256, 256);
		image_info.flags = zest_image_preset_storage;
		tests->texture = zest_CreateImage(tests->device, &image_info);
	}
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Import Image", 0)) {
			zest_image image = zest_GetImage(tests->texture);
			zest_ImportSwapchainResource();
			zest_resource_node imported_image = zest_ImportImageResource("Imported Texture", image, 0);

			zest_BeginRenderPass("Pass B");
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_ConnectInput(imported_image);
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
			test->result |= zest_GetFrameGraphResult(frame_graph);
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

/*
Automatic Barrier Test(Layout Transition) :
* Pass A : Renders to a texture(Color Attachment Optimal layout).
* Pass B : Uses that same texture as a shader input(Shader Read - Only Optimal layout).
* The graph must insert the correct pipeline barrier to transition the texture layout between passes.
*/
int test__image_barrier_tests(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Transient Image", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);

			zest_pass_node pass_a = zest_BeginRenderPass("Pass A");
			zest_ConnectOutput(output_a);
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			zest_BeginRenderPass("Pass B");
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_ConnectInput(output_a);
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
			//zest_PrintCompiledRenderGraph(frame_graph);
			test->result |= zest_GetFrameGraphResult(frame_graph);
			if (zest_GetFrameGraphSubmissionCount(frame_graph)) {
				if (zest_GetFrameGraphSubmissionBatchCount(frame_graph, 0)) {
					const zest_submission_batch_t *batch = zest_GetFrameGraphSubmissionBatch(frame_graph, 0, 0);
					const zest_pass_group_t *grouped_pass = zest_GetFrameGraphFinalPass(frame_graph, batch->pass_indices[0]);
					const zest_execution_details_t *exe_details = &grouped_pass->execution_details;
					const zest_execution_barriers_t *barriers = &exe_details->barriers;
					zest_uint acquire_size = zest_vec_size(barriers->acquire_image_barriers);
					zest_uint release_size = zest_vec_size(barriers->release_image_barriers);
					if (acquire_size == 1 && release_size == 1) {
						test->result += barriers->acquire_image_barriers[0].old_layout == zest_image_layout_undefined ? 0 : 1;
						test->result += barriers->acquire_image_barriers[0].new_layout == zest_image_layout_color_attachment_optimal ? 0 : 1;
						test->result += barriers->acquire_image_barriers[0].src_access_mask == zest_access_none ? 0 : 1;
						test->result += barriers->acquire_image_barriers[0].dst_access_mask == zest_access_color_attachment_write_bit ? 0 : 1;
						test->result += barriers->release_image_barriers[0].old_layout == zest_image_layout_color_attachment_optimal ? 0 : 1;
						test->result += barriers->release_image_barriers[0].new_layout == zest_image_layout_shader_read_only_optimal ? 0 : 1;
						test->result += barriers->release_image_barriers[0].src_access_mask == zest_access_color_attachment_write_bit ? 0 : 1;
						test->result += barriers->release_image_barriers[0].dst_access_mask == zest_access_shader_read_bit ? 0 : 1;
					}
				} else {
					test->result += 1;
				}
			} else {
				test->result += 1;
			}
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

void zest_WriteBufferCompute(const zest_command_list command_list, void *user_data) {
	ZestTests *tests = (ZestTests *)user_data;
	zest_resource_node write_buffer = zest_GetPassOutputResource(command_list, "Write Buffer");
	ZEST_ASSERT_HANDLE(write_buffer);


	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_compute compute = zest_GetCompute(tests->compute_write);

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, compute);

	TestPushConstants push;

	// Update push constants for the current dispatch
	// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
	push.index1 = zest_GetTransientBufferBindlessIndex(command_list, write_buffer);

	zest_cmd_SendPushConstants(command_list, &push, sizeof(TestPushConstants));

	zest_uint group_count_x = (1000 + local_size_x - 1) / local_size_x;

	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, group_count_x, 1, 1);
}

void zest_VerifyBufferCompute(const zest_command_list command_list, void *user_data) {
	ZestTests *tests = (ZestTests *)user_data;
	zest_resource_node write_buffer = zest_GetPassInputResource(command_list, "Write Buffer");
	zest_resource_node verify_buffer = zest_GetPassOutputResource(command_list, "Verify Buffer");
	ZEST_ASSERT_HANDLE(write_buffer);
	ZEST_ASSERT_HANDLE(verify_buffer);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_descriptor_set sets[] = {
		zest_GetBindlessSet(tests->device)
	};

	zest_compute compute_verify = zest_GetCompute(tests->compute_verify);

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, compute_verify);

	TestPushConstants push;

	// Update push constants for the current dispatch
	// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
	push.index1 = zest_GetTransientBufferBindlessIndex(command_list, write_buffer);
	push.index2 = zest_GetTransientBufferBindlessIndex(command_list, verify_buffer);

	zest_cmd_SendPushConstants(command_list, &push, sizeof(TestPushConstants));

	zest_uint group_count_x = (1000 + local_size_x - 1) / local_size_x;

	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, group_count_x, 1, 1);
}

/*
10. Buffer Write/Read:
* Pass A (Compute): Writes a specific data pattern into a storage buffer.
* Pass B (Compute): Reads from the buffer and verifies the data is correct (e.g., by writing a success/fail flag to another buffer that can be read by
  the CPU).
* Also tests executing and waiting to finish
*/
int test__buffer_read_write(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->compute_write)) {
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->device, "examples/SDL2/zest-tests/shaders/buffer_write.comp", "buffer_write.spv", zest_compute_shader, NULL, 1);
		tests->compute_write = zest_CreateCompute(tests->device, "Buffer Write", shader);
		if (!zest_IsValidHandle((void*)&tests->compute_write)) {
			test->frame_count++;
			test->result = -1;
			return test->result;
		}
	}
	if (!zest_IsValidHandle((void*)&tests->compute_verify)) {
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->device, "examples/SDL2/zest-tests/shaders/buffer_verify.comp", "buffer_verify.spv", zest_compute_shader, NULL, 1);
		tests->compute_verify = zest_CreateCompute(tests->device, "Buffer Verify", shader);
		if (!zest_IsValidHandle((void*)&tests->compute_verify)) {
			test->frame_count++;
			test->result = -1;
			return test->result;
		}
	}
	if (!tests->cpu_buffer) {
		zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_to_cpu);
		tests->cpu_buffer = zest_CreateBuffer(tests->device, sizeof(TestResults), &storage_buffer_info);
		tests->cpu_buffer_index = zest_AcquireStorageBufferIndex(tests->device, tests->cpu_buffer);
	}
	zest_buffer_resource_info_t info = {};
	info.size = sizeof(TestData) * 1000;
	zest_execution_timeline timeline = zest_CreateExecutionTimeline(tests->device);
	if (zest_BeginCommandGraph(tests->context, "Buffer Read/Write", 0)) {
		zest_resource_node write_buffer = zest_AddTransientBufferResource("Write Buffer", &info);
		zest_resource_node verify_buffer = zest_ImportBufferResource("Verify Buffer", tests->cpu_buffer, 0);

		zest_compute compute_write = zest_GetCompute(tests->compute_write);
		zest_compute compute_verify = zest_GetCompute(tests->compute_verify);

		zest_BeginComputePass("Write Pass");
		zest_ConnectOutput(write_buffer);
		zest_SetPassTask(zest_WriteBufferCompute, tests);
		zest_EndPass();

		zest_BeginComputePass("Verify Pass");
		zest_ConnectInput(write_buffer);
		zest_ConnectOutput(verify_buffer);
		zest_SetPassTask(zest_VerifyBufferCompute, tests);
		zest_EndPass();

		zest_SignalTimeline(timeline);
		zest_frame_graph frame_graph = zest_EndFrameGraph();
		zest_semaphore_status status = zest_FlushFrameGraph(frame_graph);
		if (status != zest_semaphore_status_success) {
			test->result = 1;
		}
		test->result |= zest_GetFrameGraphResult(frame_graph);
		TestData *test_data = (TestData *)zest_BufferData(tests->cpu_buffer);
		if (test_data->vec.x != 1.f) {
			test->result = 1;
		}
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

/*
Multi-Reader Barrier Test: Pass A writes to a resource. Passes B and C both read from that same resource. 
The graph should correctly synchronize this so B and C only execute after A is complete.
*/
int test__multi_reader_barrier(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Multi Reader Barrier", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);

			zest_pass_node pass_a = zest_BeginRenderPass("Pass A");
			zest_ConnectOutput(output_a);
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			zest_pass_node pass_b = zest_BeginRenderPass("Pass B");
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_ConnectInput(output_a);
			zest_EndPass();

			zest_pass_node pass_c = zest_BeginRenderPass("Pass C");
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_ConnectInput(output_a);
			zest_EndPass();
			frame_graph = zest_EndFrameGraph();
			test->result |= zest_GetFrameGraphResult(frame_graph);
			int failed = 0;
			if (zest_GetFrameGraphSubmissionCount(frame_graph) == 1) {
				if (zest_GetFrameGraphSubmissionBatchCount(frame_graph, 0) == 1) {
					const zest_submission_batch_t *batch = zest_GetFrameGraphSubmissionBatch(frame_graph, 0, 0);
					if (zest_vec_size(batch->pass_indices) == 2) {
						const zest_pass_group_t *pass_group_a = zest_GetFrameGraphFinalPass(frame_graph, batch->pass_indices[0]);
						const zest_pass_group_t *pass_group_b = zest_GetFrameGraphFinalPass(frame_graph, batch->pass_indices[1]);
						if (zest_vec_size(pass_group_a->passes) == 1 && zest_vec_size(pass_group_b->passes) == 2) {
							int check = pass_group_a->passes[0] == pass_a ? 1 : 0;
							check += pass_group_b->passes[0] == pass_b ? 1 : 0;
							check += pass_group_b->passes[1] == pass_c ? 1 : 0;
							failed = check == 3 ? 0 : 1;
						}
					}
				}
			}
			test->result |= failed;
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->frame_count++;
	return test->result;
}

void zest_VerifyImageCompute(const zest_command_list command_list, void *user_data) {
	ZestTests *tests = (ZestTests *)user_data;
	zest_resource_node read_image = zest_GetPassInputResource(command_list, "Write Buffer");
	zest_resource_node verify_buffer = zest_GetPassOutputResource(command_list, "Verify Buffer");
	ZEST_ASSERT_HANDLE(read_image);
	ZEST_ASSERT_HANDLE(verify_buffer);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_descriptor_set sets[] = {
		zest_GetBindlessSet(tests->device)
	};

	zest_compute compute_verify = zest_GetCompute(tests->compute_verify);

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, compute_verify);

	TestPushConstants push;

	if (!tests->sampler.value) {
		tests->sampler = zest_CreateSampler(tests->device, &tests->sampler_info);
		zest_sampler sampler = zest_GetSampler(tests->sampler);
		tests->sampler_index = zest_AcquireSamplerIndex(tests->device, sampler);
	}

	// Update push constants for the current dispatch
	// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
	push.index1 = zest_GetTransientSampledImageBindlessIndex(command_list, read_image, zest_texture_2d_binding);
	push.index2 = tests->cpu_buffer_index;
	push.index3 = tests->sampler_index;
	push.index4 = zest_ScreenWidth(tests->context);
	push.index5 = zest_ScreenHeight(tests->context);

	zest_cmd_SendPushConstants(command_list, &push, sizeof(TestPushConstants));

	zest_uint group_count_x = (zest_ScreenWidth(tests->context) + local_size_x - 1) / local_size_x;
	zest_uint group_count_y = (zest_ScreenHeight(tests->context) + local_size_y - 1) / local_size_y;

	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, group_count_x, group_count_y, 1);
}


/*
Image Write / Read(Clear Color) :
* Pass A(Graphics) : Clears a transient image to a specific color.
* Pass B(Compute) : Reads the image pixels and verifies they match the clear color.
*/
int test__image_read_write(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->compute_verify)) {
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->device, "examples/SDL2/zest-tests/shaders/image_verify.comp", "image_verify.spv", zest_compute_shader, NULL, 1);
		tests->compute_verify = zest_CreateCompute(tests->device, "Image Verify", shader);
		if (!zest_IsValidHandle((void*)&tests->compute_verify)) {
			test->frame_count++;
			test->result = 1;
			return test->result;
		}
	}
	if (!tests->cpu_buffer) {
		zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_to_cpu);
		tests->cpu_buffer = zest_CreateBuffer(tests->device, sizeof(TestResults), &storage_buffer_info);
		tests->cpu_buffer_index = zest_AcquireStorageBufferIndex(tests->device, tests->cpu_buffer);
			
	}
	zest_image_resource_info_t image_info = {zest_format_r8g8b8a8_unorm};
	zest_execution_timeline timeline = zest_CreateExecutionTimeline(tests->device);
	if (zest_BeginCommandGraph(tests->context, "Image Read Write", 0)) {
		zest_resource_node write_buffer = zest_AddTransientImageResource("Write Buffer", &image_info);
		zest_resource_node verify_buffer = zest_ImportBufferResource("Verify Buffer", tests->cpu_buffer, 0);
		zest_SetResourceClearColor(write_buffer, 0.0f, 1.0f, 1.0f, 1.0f);

		zest_compute compute_verify = zest_GetCompute(tests->compute_verify);

		zest_pass_node pass_a = zest_BeginRenderPass("Pass A");
		zest_ConnectOutput(write_buffer);
		zest_SetPassTask(zest_EmptyRenderPass, NULL);
		zest_EndPass();

		zest_pass_node verify_pass = zest_BeginComputePass("Pass B");
		zest_ConnectInput(write_buffer);
		zest_ConnectOutput(verify_buffer);
		zest_SetPassTask(zest_VerifyImageCompute, tests);
		zest_EndPass();

		zest_SignalTimeline(timeline);
		zest_frame_graph frame_graph = zest_EndFrameGraph();
		zest_semaphore_status status = zest_FlushFrameGraph(frame_graph);
		if (status != zest_semaphore_status_success) {
			test->result = 1;
		}

		TestData *test_data = (TestData *)zest_BufferData(tests->cpu_buffer);
		test->result |= zest_GetFrameGraphResult(frame_graph);
		if (test_data->vec.x == 0.f) {
			test->result = 1;
		}
		if (test_data->vec.y == 1.f) {
			test->result = 1;
		}
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

/*
Depth Attachment Test:
* Pass A: Renders geometry with depth testing/writing enabled to a transient depth buffer.
* Pass B: Renders geometry that should be occluded by the objects from Pass A.
* Verify the final image shows correct occlusion.
*/
int test__depth_attachment(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = { zest_format_r8g8b8a8_unorm };
	zest_image_resource_info_t depth_info = { zest_format_depth };
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node depth = zest_AddTransientImageResource("Depth Buffer", &depth_info);
			zest_FlagResourceAsEssential(depth);
			zest_BeginRenderPass("Draw Nothing");
			zest_ConnectSwapChainOutput();
			zest_ConnectOutput(depth);
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_SetSwapchainClearColor(tests->context, 0.0f, 0.1f, 0.2f, 1.0f);
			zest_EndPass();
			frame_graph = zest_EndFrameGraph();
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result = 1;
		if (zest_GetFrameGraphFinalPassCount(frame_graph) == 1) {
			const zest_pass_group_t *pass_group = zest_GetFrameGraphFinalPass(frame_graph, 0);
			if (zest_vec_size(pass_group->execution_details.color_attachments) == 1) {
				zest_rendering_attachment_info_t swapchain_resource = pass_group->execution_details.color_attachments[0];
				zest_rendering_attachment_info_t depth_resource = pass_group->execution_details.depth_attachment;
				if (swapchain_resource.image_view && depth_resource.image_view) {
					test->result = 0;
				}
			}
		}
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

void zest_TransferBuffer(const zest_command_list command_list, void *user_data) {
	zest_resource_node write_buffer = zest_GetPassOutputResource(command_list, "Output B");
	float dummy_data[1024];
	for (int i = 0; i != 1024; i++) {
		dummy_data[i] = (float)i;
	}
	zest_buffer staging_buffer = zest_CreateStagingBuffer(command_list->device, sizeof(float) * 1024, dummy_data);
	zest_cmd_CopyBuffer(command_list, staging_buffer, write_buffer->storage_buffer, write_buffer->storage_buffer->size);
}

void zest_WriteImageCompute(const zest_command_list command_list, void *user_data) {
	ZestTests *tests = (ZestTests *)user_data;
	zest_resource_node write_buffer = zest_GetPassOutputResource(command_list, "Output A");
	ZEST_ASSERT_HANDLE(write_buffer);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_descriptor_set sets[] = {
		zest_GetBindlessSet(tests->device)
	};

	zest_compute compute_write = zest_GetCompute(tests->compute_write);

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, compute_write);

	TestPushConstants push;

	if (!tests->sampler.value) {
		tests->sampler = zest_CreateSampler(tests->device, &tests->sampler_info);
		zest_sampler sampler = zest_GetSampler(tests->sampler);
		tests->sampler_index = zest_AcquireSamplerIndex(tests->device, sampler);
	}

	// Update push constants for the current dispatch
	// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
	push.index1 = zest_GetTransientSampledImageBindlessIndex(command_list, write_buffer, zest_storage_image_binding);

	zest_cmd_SendPushConstants(command_list, &push, sizeof(TestPushConstants));

	zest_image_info_t image_desc = zest_GetResourceImageDescription(write_buffer);
	zest_uint group_count_x = (image_desc.extent.width + local_size_x - 1) / local_size_x;
	zest_uint group_count_y = (image_desc.extent.height + local_size_y - 1) / local_size_y;

	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, group_count_x, group_count_y, 1);
}


/*
Multi-Queue Synchronization:
* Pass A (Compute Queue): Processes data in a buffer
* Pass B (Transfer Queue): Transfers data to a buffer
* Pass C (Graphics Queue): Uses the compute buffer as a vertex buffer for rendering.
* The graph must handle the queue ownership transfer and synchronization (semaphores). It must also run fine
* if the only queue available is the graphics queue.
*/
int test__multi_queue_sync(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->compute_write)) {
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->device, "examples/SDL2/zest-tests/shaders/image_write2.comp", "image_write.spv", zest_compute_shader, NULL, 1);
		tests->compute_write = zest_CreateCompute(tests->device, "Buffer Write", shader);
		if (!zest_IsValidHandle((void*)&tests->compute_write)) {
			test->frame_count++;
			test->result = 1;
			return test->result;
		}
	}

	zest_SetSwapchainClearColor(tests->context, 0.f, .1f, .2f, 1.f);
	zest_image_resource_info_t image_info = { zest_format_r8g8b8a8_unorm };
	zest_buffer_resource_info_t buffer_info = {};
	buffer_info.size = sizeof(float) * 1024;
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Multi Queue Sync", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node output_a = zest_AddTransientImageResource("Output A", &image_info);
			zest_resource_node output_b = zest_AddTransientBufferResource("Output B", &buffer_info);

			zest_compute compute_write = zest_GetCompute(tests->compute_write);

			zest_BeginComputePass("Pass A");
			zest_ConnectOutput(output_a);
			zest_SetPassTask(zest_WriteImageCompute, tests);
			zest_EndPass();

			zest_BeginTransferPass("Pass B");
			zest_ConnectOutput(output_b);
			zest_SetPassTask(zest_TransferBuffer, tests);
			zest_EndPass();

			zest_BeginRenderPass("Pass C");
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_ConnectInput(output_a);
			zest_ConnectInput(output_b);
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
		}
		zest_EndFrame(tests->context, frame_graph);
		//zest_PrintCompiledFrameGraph(frame_graph);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

/*
Pass Grouping : Define two graphics passes that render to the same render target but have no dependency on each other. The graph compiler should ideally
group these into a single render pass with two subpasses for efficiency.
*/
int test__pass_grouping(ZestTests *tests, Test *test) {
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Pass Grouping", 0)) {
			zest_ImportSwapchainResource();

			zest_BeginRenderPass("Draw Pass A");
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			zest_BeginRenderPass("Draw Pass B");
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
		}
		zest_EndFrame(tests->context, frame_graph);
		zest_uint final_pass_count = zest_GetFrameGraphFinalPassCount(frame_graph);
		test->result |= final_pass_count == 1 ? 0 : 1;
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

/*
Compute Write/Write Barrier : Two compute passes write the same transient storage image. The second pass also reads it back
(read-modify-write feedback). They must NOT be merged into a single pass group - merging would drop the synchronisation between
them. Verify they end up in separate groups and that a barrier is emitted on the image so the second writer is ordered after the
first. Contrast with test__pass_grouping, where two graphics passes sharing a render target legitimately merge into one group.
*/
int test__compute_waw_barrier(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = { zest_format_r8g8b8a8_unorm };
	info.width = 64;
	info.height = 64;
	zest_execution_timeline timeline = zest_CreateExecutionTimeline(tests->device);
	if (zest_BeginCommandGraph(tests->context, "Compute WAW Barrier", 0)) {
		zest_resource_node feedback_image = zest_AddTransientImageResource("Feedback Image", &info);
		zest_FlagResourceAsEssential(feedback_image);

		//First writer
		zest_BeginComputePass("Compute A");
		zest_ConnectOutput(feedback_image);
		zest_SetPassTask(zest_EmptyRenderPass, NULL);
		zest_EndPass();

		//Second writer reads what A wrote and writes it back (feedback). Must be ordered after A.
		zest_BeginComputePass("Compute B");
		zest_ConnectInput(feedback_image);
		zest_ConnectOutput(feedback_image);
		zest_SetPassTask(zest_EmptyRenderPass, NULL);
		zest_EndPass();

		zest_SignalTimeline(timeline);
		zest_frame_graph frame_graph = zest_EndFrameGraph();
		test->result |= zest_GetFrameGraphResult(frame_graph);

		//The two compute writers must not merge - expect two separate groups.
		test->result |= zest_GetFrameGraphFinalPassCount(frame_graph) == 2 ? 0 : 1;

		//And a barrier must be emitted on the image between them so B waits on A's write.
		int found_barrier = 0;
		zest_uint final_count = zest_GetFrameGraphFinalPassCount(frame_graph);
		for (zest_uint i = 0; i < final_count; i++) {
			const zest_pass_group_t *group = zest_GetFrameGraphFinalPass(frame_graph, i);
			if (zest_vec_size(group->execution_details.barriers.acquire_image_barriers) > 0) {
				found_barrier = 1;
			}
		}
		test->result |= found_barrier ? 0 : 1;

		zest_FlushFrameGraph(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

/*
Cyclic Dependency : Create a graph where Pass A depends on Pass B's output, and Pass B depends on Pass A's output.The graph compiler should detect this
cycle and return an error instead of crashing.
*/
int test__cyclic_dependency(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = { zest_format_r8g8b8a8_unorm };
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Cyclic Dependency", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);
			zest_resource_node output_b = zest_AddTransientImageResource("Output B", &info);

			zest_BeginRenderPass("Draw Pass A");
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_ConnectInput(output_b);
			zest_ConnectOutput(output_a);
			zest_EndPass();

			zest_BeginRenderPass("Draw Pass B");
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_ConnectInput(output_a);
			zest_ConnectOutput(output_b);
			zest_EndPass();

			zest_BeginRenderPass("Draw Pass C");
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

/*
Re-executing Graph : Compile a graph once, then execute it for multiple consecutive frames.
Verify that no memory leaks or synchronization issues occur.
*/
int test__simple_caching(ZestTests *tests, Test *test) {
	zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(tests->context, 0, 0);
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = zest_GetCachedFrameGraph(tests->context, &cache_key);
		if (!frame_graph) {
			if (zest_BeginFrameGraph(tests->context, "Blank Screen", &cache_key)) {
				zest_ImportSwapchainResource();

				zest_BeginRenderPass("Draw Nothing");
				zest_ConnectSwapChainOutput();
				zest_SetPassTask(zest_EmptyRenderPass, NULL);
				zest_EndPass();
				frame_graph = zest_EndFrameGraph();
			}
		} else {
			test->cache_count++;
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	if (test->frame_count > 0) {
		test->result |= test->cache_count == 0;
	}
	test->frame_count++;
	return test->result;
}

/*
Cached Transient Persistence:
A cached frame graph's transient images should persist across executions - the whole point of the
arena placement is that when nothing about a transient changes, the image, view and binding stay
alive instead of being recreated every frame. This test caches a graph with two transient images:
one with a fixed size and one whose size is driven by an image provider (the pattern used for
variable-size render targets that track a UI panel). It runs for six frames, resizing the
provider-driven image once at frame 3, then verifies:
  - the fixed-size image keeps the same backend (i.e. the same API image) across all executions
    of the same frame-in-flight slot
  - the provider-driven image is recreated exactly when its size changes (per FIF slot, so one
    frame apart for the two slots) and then persists again at the new size
*/
struct CachedPersistenceCapture {
	void *stable_backend[8];
	void *resize_backend[8];
	zest_uint resize_width[8];
	int frame;
};

static zest_uint tst__resize_target_width = 128;
static zest_uint tst__resize_target_height = 128;

zest_image_view tst__resize_provider(zest_context context, zest_resource_node node) {
	zest_SetResourceWidth(node, tst__resize_target_width);
	zest_SetResourceHeight(node, tst__resize_target_height);
	return NULL;
}

void tst__persistence_capture(const zest_command_list command_list, void *user_data) {
	CachedPersistenceCapture *capture = (CachedPersistenceCapture *)user_data;
	zest_resource_node stable = zest_GetPassInputResource(command_list, "Stable Image");
	zest_resource_node resize = zest_GetPassInputResource(command_list, "Resize Target");
	if (stable && resize && capture->frame < 8) {
		capture->stable_backend[capture->frame] = (void *)stable->image.backend;
		capture->resize_backend[capture->frame] = (void *)resize->image.backend;
		capture->resize_width[capture->frame] = resize->image.info.extent.width;
		capture->frame++;
	}
}

int test__cached_transient_persistence(ZestTests *tests, Test *test) {
	static CachedPersistenceCapture capture;
	if (test->frame_count == 0) {
		memset(&capture, 0, sizeof(capture));
		tst__resize_target_width = 128;
		tst__resize_target_height = 128;
	}
	if (test->frame_count == 3) {
		//Resize the provider-driven target mid-run without recompiling the graph
		tst__resize_target_width = 320;
		tst__resize_target_height = 200;
	}
	zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(tests->context, 0, 0);
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = zest_GetCachedFrameGraph(tests->context, &cache_key);
		if (!frame_graph) {
			if (zest_BeginFrameGraph(tests->context, "Cached Transients", &cache_key)) {
				zest_ImportSwapchainResource();
				zest_image_resource_info_t stable_info = { zest_format_r8g8b8a8_unorm };
				stable_info.width = 256;
				stable_info.height = 256;
				zest_image_resource_info_t resize_info = { zest_format_r8g8b8a8_unorm };
				resize_info.width = 128;
				resize_info.height = 128;
				zest_resource_node stable = zest_AddTransientImageResource("Stable Image", &stable_info);
				zest_resource_node resize = zest_AddTransientImageResource("Resize Target", &resize_info);
				zest_SetResourceImageProvider(resize, tst__resize_provider);

				//The two images have different sizes so they must be rendered in separate passes
				//(color attachments of one render pass share the framebuffer dimensions)
				zest_BeginRenderPass("Write Stable");
				zest_ConnectOutput(stable);
				zest_SetPassTask(zest_EmptyRenderPass, NULL);
				zest_EndPass();

				zest_BeginRenderPass("Write Resize");
				zest_ConnectOutput(resize);
				zest_SetPassTask(zest_EmptyRenderPass, NULL);
				zest_EndPass();

				zest_BeginRenderPass("Read Images");
				zest_ConnectInput(stable);
				zest_ConnectInput(resize);
				zest_ConnectSwapChainOutput();
				zest_SetPassTask(tst__persistence_capture, &capture);
				zest_EndPass();

				frame_graph = zest_EndFrameGraph();
			}
		} else {
			test->cache_count++;
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	if (test->frame_count == test->run_count) {
		if (test->cache_count == 0) {
			test->result |= 2;   //The graph never came from the cache
		}
		if (capture.frame < 6) {
			ZEST_PRINT("Cached Transients: only %i executions were captured", capture.frame);
			test->result |= 4;
		} else {
			//Executions alternate frame-in-flight slots, so compare captures two frames apart.
			//The stable image must never be recreated:
			int stable_persisted =
				capture.stable_backend[2] == capture.stable_backend[0] &&
				capture.stable_backend[4] == capture.stable_backend[0] &&
				capture.stable_backend[3] == capture.stable_backend[1] &&
				capture.stable_backend[5] == capture.stable_backend[1];
			//The resized image must be recreated exactly once per FIF slot (executions 3 and 4)
			//and then persist at the new size:
			int resize_recreated =
				capture.resize_backend[2] == capture.resize_backend[0] &&   //stable before the resize
				capture.resize_backend[3] != capture.resize_backend[1] &&   //recreated when the size changed
				capture.resize_backend[4] != capture.resize_backend[2] &&   //and for the other FIF slot too
				capture.resize_backend[5] == capture.resize_backend[3];     //stable again after
			int resize_applied = capture.resize_width[5] == 320 && capture.resize_width[2] == 128;
			ZEST_PRINT("Cached Transients: stable persisted: %s | resize recreated: %s | resize applied: %s",
				stable_persisted ? "yes" : "NO", resize_recreated ? "yes" : "NO", resize_applied ? "yes" : "NO");
			test->result |= stable_persisted ? 0 : 8;
			test->result |= resize_recreated ? 0 : 16;
			test->result |= resize_applied ? 0 : 32;
		}
		//Settle frames so deferred releases don't disturb the next test
		for (int frame = 0; frame < ZEST_MAX_FIF + 1; frame++) {
			zest_UpdateDevice(tests->device);
			if (zest_BeginFrame(tests->context)) {
				zest_EndFrame(tests->context, 0);
			}
		}
	}
	return test->result;
}

/*
Cached Transient Placement: a cached frame graph with two transient buffers written by the same
transfer pass every frame. "Grow Buffer" has a buffer provider (which resets storage_buffer each
execution) and grows after the first execution; "Stale Buffer" has no provider. Transient buffers
are placed as suballocations of the persistent per-FIF arena backings, and the placement pass must
re-place every transient buffer on every execution of a cached graph: a resource that keeps the
proxy from a previous execution stays bound to that execution's FIF backing and offset, so it both
writes into the wrong frame-in-flight's memory and overlaps whatever transient was re-placed over
those bytes. Sync validation catches the overlap as a WRITE_AFTER_WRITE between the two copies
once the FIF slot of the first execution comes around again and the grown buffer covers the stale
one's bytes.
*/
struct StalePlacementState {
	int execution_count;
};

zest_buffer tst__grow_buffer_provider(zest_context context, zest_resource_node node) {
	StalePlacementState *state = (StalePlacementState *)zest_GetResourceUserData(node);
	//256 bytes on the first execution, 66000 from then on so the re-placed buffer grows over the
	//bytes the stale buffer was given on the first execution.
	zest_SetResourceBufferSize(node, state->execution_count == 0 ? 256 : 66000);
	return NULL;
}

void tst__transfer_to_arena_buffers(const zest_command_list command_list, void *user_data) {
	StalePlacementState *state = (StalePlacementState *)user_data;
	zest_resource_node grow_buffer = zest_GetPassOutputResource(command_list, "Grow Buffer");
	zest_resource_node stale_buffer = zest_GetPassOutputResource(command_list, "Stale Buffer");
	zest_size grow_size = state->execution_count == 0 ? 256 : 66000;
	zest_buffer grow_staging = zest_CreateStagingBuffer(command_list->device, grow_size, 0);
	zest_cmd_CopyBuffer(command_list, grow_staging, zest_GetResourceBuffer(grow_buffer), grow_size);
	zest_FreeBuffer(grow_staging);
	zest_buffer stale_staging = zest_CreateStagingBuffer(command_list->device, 512, 0);
	zest_cmd_CopyBuffer(command_list, stale_staging, zest_GetResourceBuffer(stale_buffer), 512);
	zest_FreeBuffer(stale_staging);
	state->execution_count++;
}

int test__cached_transient_placement(ZestTests *tests, Test *test) {
	static StalePlacementState state;
	if (test->frame_count == 0) {
		state.execution_count = 0;
	}
	zest_buffer_resource_info_t grow_info = {};
	grow_info.size = 256;
	zest_buffer_resource_info_t stale_info = {};
	stale_info.size = 512;
	zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(tests->context, 0, 0);
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = zest_GetCachedFrameGraph(tests->context, &cache_key);
		if (!frame_graph) {
			if (zest_BeginFrameGraph(tests->context, "Cached Transient Placement", &cache_key)) {
				zest_ImportSwapchainResource();
				zest_resource_node grow_buffer = zest_AddTransientBufferResource("Grow Buffer", &grow_info);
				zest_SetResourceUserData(grow_buffer, &state);
				zest_SetResourceBufferProvider(grow_buffer, tst__grow_buffer_provider);
				zest_resource_node stale_buffer = zest_AddTransientBufferResource("Stale Buffer", &stale_info);

				zest_BeginTransferPass("Upload Pass");
				zest_ConnectOutput(grow_buffer);
				zest_ConnectOutput(stale_buffer);
				zest_SetPassTask(tst__transfer_to_arena_buffers, &state);
				zest_EndPass();

				zest_BeginRenderPass("Read Pass");
				zest_ConnectInput(grow_buffer);
				zest_ConnectInput(stale_buffer);
				zest_ConnectSwapChainOutput();
				zest_SetPassTask(zest_EmptyRenderPass, NULL);
				zest_EndPass();

				frame_graph = zest_EndFrameGraph();
			}
		} else {
			test->cache_count++;
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	if (test->frame_count == test->run_count && test->cache_count == 0) {
		test->result |= 2;   //The graph never came from the cache so the stale-proxy path was not exercised
	}
	return test->result;
}

/*
Unbacked Transient Barrier: a cached frame graph with a transient buffer that has backing on the
first execution but resolves to zero size from then on - exactly what an instance/dynamic layer
does when it has nothing to draw (see zest__instance_layer_resource_provider, which returns NULL
and leaves buffer_desc.size at 0 for an empty layer). The transient placement pass skips a zero
sized buffer, so it reaches execution with storage_buffer == NULL while its producer->consumer
acquire/release barriers are still baked into the cached graph. Before the fix the barrier code
dereferenced storage_buffer unconditionally and crashed; the fix skips the barrier for any buffer
with no backing this frame (there is no memory to synchronise). A "Keep Buffer" that stays backed
sits alongside it so the barrier array contains a mix of backed and unbacked entries, exercising
the run-splitting submission path. In ZEST_TEST_MODE a *sized* resource with no buffer would be
recorded as a validation error instead of crashing; an empty (zero size) one must not, so the test
asserts a clean result across all executions.
*/
struct UnbackedBufferState {
	int execution_count;
};

zest_buffer tst__vanishing_buffer_provider(zest_context context, zest_resource_node node) {
	UnbackedBufferState *state = (UnbackedBufferState *)zest_GetResourceUserData(node);
	//Sized on the first execution so it gets a transient placement, then empty from then on so the
	//cached graph re-executes with a buffer resource that has no backing buffer this frame.
	zest_SetResourceBufferSize(node, state->execution_count == 0 ? 256 : 0);
	return NULL;
}

void tst__read_mixed_buffers(const zest_command_list command_list, void *user_data) {
	//"Keep Buffer" is always backed, so acquiring its bindless index is always safe.
	zest_resource_node keep_buffer = zest_GetPassInputResource(command_list, "Keep Buffer");
	zest_GetTransientBufferBindlessIndex(command_list, keep_buffer);
	//"Vanishing Buffer" has no backing buffer on empty executions. This is the recommended userland
	//pattern: check zest_ResourceBufferIsValid before touching the buffer / acquiring its index so
	//we skip the descriptor update that would otherwise dereference a null buffer.
	zest_resource_node vanishing_buffer = zest_GetPassInputResource(command_list, "Vanishing Buffer");
	if (zest_ResourceBufferIsValid(vanishing_buffer)) {
		zest_GetTransientBufferBindlessIndex(command_list, vanishing_buffer);
	}
}

void tst__transfer_to_mixed_buffers(const zest_command_list command_list, void *user_data) {
	UnbackedBufferState *state = (UnbackedBufferState *)user_data;
	zest_resource_node keep_buffer = zest_GetPassOutputResource(command_list, "Keep Buffer");
	zest_buffer keep_staging = zest_CreateStagingBuffer(command_list->device, 512, 0);
	zest_cmd_CopyBuffer(command_list, keep_staging, zest_GetResourceBuffer(keep_buffer), 512);
	zest_FreeBuffer(keep_staging);
	if (state->execution_count == 0) {
		//Only copy the vanishing buffer while it still has backing.
		zest_resource_node vanishing_buffer = zest_GetPassOutputResource(command_list, "Vanishing Buffer");
		zest_buffer vanishing_staging = zest_CreateStagingBuffer(command_list->device, 256, 0);
		zest_cmd_CopyBuffer(command_list, vanishing_staging, zest_GetResourceBuffer(vanishing_buffer), 256);
		zest_FreeBuffer(vanishing_staging);
	}
	state->execution_count++;
}

int test__unbacked_transient_barrier(ZestTests *tests, Test *test) {
	static UnbackedBufferState state;
	if (test->frame_count == 0) {
		state.execution_count = 0;
	}
	zest_buffer_resource_info_t vanishing_info = {};
	vanishing_info.size = 256;
	zest_buffer_resource_info_t keep_info = {};
	keep_info.size = 512;
	zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(tests->context, 0, 0);
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = zest_GetCachedFrameGraph(tests->context, &cache_key);
		if (!frame_graph) {
			if (zest_BeginFrameGraph(tests->context, "Unbacked Transient Barrier", &cache_key)) {
				zest_ImportSwapchainResource();
				zest_resource_node vanishing_buffer = zest_AddTransientBufferResource("Vanishing Buffer", &vanishing_info);
				zest_SetResourceUserData(vanishing_buffer, &state);
				zest_SetResourceBufferProvider(vanishing_buffer, tst__vanishing_buffer_provider);
				zest_resource_node keep_buffer = zest_AddTransientBufferResource("Keep Buffer", &keep_info);

				zest_BeginTransferPass("Upload Pass");
				zest_ConnectOutput(vanishing_buffer);
				zest_ConnectOutput(keep_buffer);
				zest_SetPassTask(tst__transfer_to_mixed_buffers, &state);
				zest_EndPass();

				zest_BeginRenderPass("Read Pass");
				zest_ConnectInput(vanishing_buffer);
				zest_ConnectInput(keep_buffer);
				zest_ConnectSwapChainOutput();
				zest_SetPassTask(tst__read_mixed_buffers, &state);
				zest_EndPass();

				frame_graph = zest_EndFrameGraph();
			}
		} else {
			test->cache_count++;
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	if (test->frame_count == test->run_count && test->cache_count == 0) {
		test->result |= 2;   //The graph never came from the cache so the unbacked re-execution path was not exercised
	}
	return test->result;
}

/*
Essential-but-unused transient: flag a transient image essential yet never connect it to any pass.
The transient-planning guard must still cull it (a flag alone is not "used") rather than indexing
pass_execution_order with its ZEST_INVALID first-usage. Guards the "used" half of the parenthesised
condition in Plan_transient_buffers (see item 2): the graph compiles, the resource is reported
culled, and nothing crashes.
*/
int test__essential_unused_transient(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = { zest_format_r8g8b8a8_unorm };
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Essential Unused Transient", 0)) {
			zest_ImportSwapchainResource();
			//Essential but connected to no pass - must be culled, not create/free planned.
			zest_resource_node orphan = zest_AddTransientImageResource("Orphan Output", &info);
			zest_FlagResourceAsEssential(orphan);

			zest_BeginRenderPass("Draw Nothing");
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
			test->result |= zest_GetFrameGraphResult(frame_graph);
			//The orphaned essential transient must have been culled.
			if (zest_GetFrameGraphCulledResourceCount(frame_graph) != 1) {
				test->result = 1;
			}
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= frame_graph ? zest_GetFrameGraphResult(frame_graph) : 1;
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

/*
Versioned transient: writing a transient a second time turns the second write into a read-modify-
write on an aliased version node that shares the original's memory (the compiler auto-adds the
original as an input to the second writer). A later read gives that version node a reference_count
> 0. This is the resource shape item 2 flags as the plausible trigger for the '||'/'&&' precedence
bug in Plan_transient_buffers. Written as a write -> overwrite -> read chain (a plain WAW second
write, so no reader of res feeds the second writer and name-based cycle detection is not tripped).
Regression guard: the versioned chain must compile and execute cleanly - no cyclic/critical error,
no validation errors, no crash - and the transient planning must not choke on the aliased version's
untouched lifetime.
*/
int test__versioned_transient(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = { zest_format_r8g8b8a8_unorm };
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Versioned Transient", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node res = zest_AddTransientImageResource("Versioned Res", &info);

			//Pass A: first write -> res version 0.
			zest_BeginRenderPass("Write A");
			zest_ConnectOutput(res);
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			//Pass B: second write to res -> creates aliased version 1 that shares v0's memory.
			zest_BeginRenderPass("Write B");
			zest_ConnectOutput(res);
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			//Pass C: read res (v1) -> gives the aliased version node reference_count > 0, then feed
			//the swapchain so the chain survives culling. Before the fix this is the read that made
			//Plan_transient_buffers dereference the version node's ZEST_INVALID first-usage index.
			zest_BeginRenderPass("Read C");
			zest_ConnectInput(res);
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
			//Must not be reported as cyclic or otherwise failed - a clean compile of the versioned chain.
			test->result |= zest_GetFrameGraphResult(frame_graph);
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= frame_graph ? zest_GetFrameGraphResult(frame_graph) : 1;
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

/*
Arena Memory Bound: the acceptance test for transient arena sharing. Build M distinct cached frame
graphs (the cache key varies by user state) and execute them one per frame, round robin, for
several cycles. Every graph places an identically sized transient image, so under arena sharing
the context's transient arena pool must stay at the live working set - at most ZEST_MAX_FIF arenas
for the one image category (a graph's return is deferred by one FIF cycle, so the pool ping-pongs
between two arenas) - no matter how many cache entries are resident. Before sharing, every
resident cache entry kept its arena checkout for the life of the entry, so the pool grew by one
arena set per cache key and the capacity scaled with M.
*/
#define ARENA_BOUND_KEY_COUNT 5

int test__arena_memory_bound(ZestTests *tests, Test *test) {
	static zest_size warm_capacity;
	static zest_uint warm_arena_count;
	if (test->frame_count == 0) {
		warm_capacity = 0;
		warm_arena_count = 0;
	}
	int key_index = test->frame_count % ARENA_BOUND_KEY_COUNT;
	zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(tests->context, &key_index, sizeof(int));
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = zest_GetCachedFrameGraph(tests->context, &cache_key);
		if (!frame_graph) {
			if (zest_BeginFrameGraph(tests->context, "Arena Memory Bound", &cache_key)) {
				zest_ImportSwapchainResource();
				zest_image_resource_info_t info = { zest_format_r8g8b8a8_unorm };
				info.width = 512;
				info.height = 512;
				zest_resource_node target = zest_AddTransientImageResource("Target", &info);

				zest_BeginRenderPass("Write Target");
				zest_ConnectOutput(target);
				zest_SetPassTask(zest_EmptyRenderPass, NULL);
				zest_EndPass();

				zest_BeginRenderPass("Read Target");
				zest_ConnectInput(target);
				zest_ConnectSwapChainOutput();
				zest_SetPassTask(zest_EmptyRenderPass, NULL);
				zest_EndPass();

				frame_graph = zest_EndFrameGraph();
			}
		} else {
			test->cache_count++;
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	if (test->frame_count == ARENA_BOUND_KEY_COUNT * 2) {
		//All cache keys are resident and the arena pool has cycled: record the warm working set.
		zest_memory_usage_t usage = zest_GetMemoryUsage(tests->context);
		warm_capacity = usage.gpu_transient_capacity;
		warm_arena_count = usage.gpu_transient_arena_count;
	}
	if (test->frame_count == test->run_count) {
		zest_memory_usage_t usage = zest_GetMemoryUsage(tests->context);
		if (test->cache_count == 0) {
			test->result |= 2;   //The graphs never came from the cache
		}
		//The pool must stay at the live working set: for one transient image category that is at
		//most ZEST_MAX_FIF arenas (the FIF-deferred return alternates between two), not one arena
		//set per resident cache entry.
		if (usage.gpu_transient_arena_count > ZEST_MAX_FIF) {
			ZEST_PRINT("Arena Memory Bound: %u transient arenas for %u cache keys, expected at most %u",
				usage.gpu_transient_arena_count, (zest_uint)ARENA_BOUND_KEY_COUNT, (zest_uint)ZEST_MAX_FIF);
			test->result |= 4;
		}
		//And it must not have grown after all the cache keys were already resident.
		if (usage.gpu_transient_capacity > warm_capacity || usage.gpu_transient_arena_count > warm_arena_count) {
			ZEST_PRINT("Arena Memory Bound: arena pool grew after warm-up (%llu -> %llu bytes, %u -> %u arenas)",
				(zest_ull)warm_capacity, (zest_ull)usage.gpu_transient_capacity,
				warm_arena_count, usage.gpu_transient_arena_count);
			test->result |= 8;
		}
	}
	return test->result;
}

/*
Arena Alternation: two cached frame graphs alternate on one context in two-frame phases
(A A B B A A ...), so each graph re-executes on a frame-in-flight slot that the other graph placed
into in between. The foreign checkout must change the arena's backing identity, which drives the
recreate path when the first graph returns - its persistent images must NOT be reused, because the
other graph's work occupied those bytes with no barriers between the two graphs. The test captures
the transient image backend per execution and asserts each graph's images were recreated after the
other graph ran (equal backends would mean stale reuse; sync validation stays clean either way
because it cannot see cross-handle aliasing, hence the explicit pointer check).
*/
struct ArenaAlternationCapture {
	void *backends[2][16];
	int counts[2];
};

struct ArenaAlternationTaskData {
	ArenaAlternationCapture *capture;
	int graph_index;
};

void tst__alternation_capture(const zest_command_list command_list, void *user_data) {
	ArenaAlternationTaskData *task_data = (ArenaAlternationTaskData *)user_data;
	ArenaAlternationCapture *capture = task_data->capture;
	zest_resource_node target = zest_GetPassInputResource(command_list, "Target");
	if (target && capture->counts[task_data->graph_index] < 16) {
		capture->backends[task_data->graph_index][capture->counts[task_data->graph_index]] = (void *)target->image.backend;
		capture->counts[task_data->graph_index]++;
	}
}

int test__arena_alternation(ZestTests *tests, Test *test) {
	static ArenaAlternationCapture capture;
	static int graph_ids[2] = { 0, 1 };
	static ArenaAlternationTaskData task_data[2] = { { &capture, 0 }, { &capture, 1 } };
	if (test->frame_count == 0) {
		memset(&capture, 0, sizeof(capture));
	}
	int graph_index = (test->frame_count / 2) % 2;
	//Distinct user state per graph gives each its own cache entry
	zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(tests->context, &graph_ids[graph_index], sizeof(int));
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = zest_GetCachedFrameGraph(tests->context, &cache_key);
		if (!frame_graph) {
			if (zest_BeginFrameGraph(tests->context, graph_index == 0 ? "Alternation A" : "Alternation B", &cache_key)) {
				zest_ImportSwapchainResource();
				zest_image_resource_info_t info = { zest_format_r8g8b8a8_unorm };
				info.width = 256;
				info.height = 256;
				zest_resource_node target = zest_AddTransientImageResource("Target", &info);

				zest_BeginRenderPass("Write Target");
				zest_ConnectOutput(target);
				zest_SetPassTask(zest_EmptyRenderPass, NULL);
				zest_EndPass();

				zest_BeginRenderPass("Read Target");
				zest_ConnectInput(target);
				zest_ConnectSwapChainOutput();
				zest_SetPassTask(tst__alternation_capture, &task_data[graph_index]);
				zest_EndPass();

				frame_graph = zest_EndFrameGraph();
			}
		} else {
			test->cache_count++;
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	if (test->frame_count == test->run_count) {
		if (test->cache_count == 0) {
			test->result |= 2;   //The graphs never came from the cache
		}
		if (capture.counts[0] < 4 || capture.counts[1] < 4) {
			ZEST_PRINT("Arena Alternation: only %i/%i executions captured", capture.counts[0], capture.counts[1]);
			test->result |= 4;
		} else {
			//Each graph runs two consecutive frames (both FIF slots), so executions 2 and 3 are the
			//same FIF slots as 0 and 1 but with the other graph's placement in between. The images
			//must have been recreated - reuse would alias the other graph's bytes.
			int recreated =
				capture.backends[0][2] != capture.backends[0][0] &&
				capture.backends[0][3] != capture.backends[0][1] &&
				capture.backends[1][2] != capture.backends[1][0] &&
				capture.backends[1][3] != capture.backends[1][1];
			ZEST_PRINT("Arena Alternation: images recreated on graph switch: %s", recreated ? "yes" : "NO");
			if (!recreated) {
				test->result |= 8;
			}
		}
	}
	return test->result;
}

/*
Intraframe Two Graphs: a command graph flushed (without a timeline wait) in the same frame as the
render graph, both placing a transient buffer of the same category. The command graph's arena
return is deferred by one FIF cycle precisely because its GPU work may still be in flight with no
synchronisation against the render graph, so the render graph must check out a DIFFERENT arena -
the two buffers must land in different device memory backings. The pass tasks capture each
transient buffer's memory pool and the test asserts they never share one.
*/
struct IntraframeArenaCapture {
	void *command_graph_pool;
	void *render_graph_pool;
	int mismatch_frames;
	int captured_frames;
};

void tst__intraframe_command_write(const zest_command_list command_list, void *user_data) {
	IntraframeArenaCapture *capture = (IntraframeArenaCapture *)user_data;
	zest_resource_node buffer = zest_GetPassOutputResource(command_list, "CG Buffer");
	zest_buffer staging = zest_CreateStagingBuffer(command_list->device, 1024, 0);
	zest_cmd_CopyBuffer(command_list, staging, zest_GetResourceBuffer(buffer), 1024);
	zest_FreeBuffer(staging);
	capture->command_graph_pool = (void *)zest_GetResourceBuffer(buffer)->memory_pool;
}

void tst__intraframe_render_write(const zest_command_list command_list, void *user_data) {
	IntraframeArenaCapture *capture = (IntraframeArenaCapture *)user_data;
	zest_resource_node buffer = zest_GetPassOutputResource(command_list, "RG Buffer");
	zest_buffer staging = zest_CreateStagingBuffer(command_list->device, 1024, 0);
	zest_cmd_CopyBuffer(command_list, staging, zest_GetResourceBuffer(buffer), 1024);
	zest_FreeBuffer(staging);
	capture->render_graph_pool = (void *)zest_GetResourceBuffer(buffer)->memory_pool;
	capture->captured_frames++;
	if (capture->command_graph_pool && capture->command_graph_pool != capture->render_graph_pool) {
		capture->mismatch_frames++;
	}
}

int test__intraframe_two_graphs(ZestTests *tests, Test *test) {
	static IntraframeArenaCapture capture;
	if (test->frame_count == 0) {
		memset(&capture, 0, sizeof(capture));
	}
	zest_buffer_resource_info_t info = {};
	info.size = 1024;
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		capture.command_graph_pool = NULL;
		capture.render_graph_pool = NULL;
		//A command graph in the same frame as the render graph: no timeline signal, so nothing
		//waits on its GPU work and its arena must stay unavailable until the FIF slot cycles.
		if (zest_BeginCommandGraph(tests->context, "Intraframe Command Graph", 0)) {
			zest_resource_node cg_buffer = zest_AddTransientBufferResource("CG Buffer", &info);
			zest_FlagResourceAsEssential(cg_buffer);

			zest_BeginTransferPass("CG Upload");
			zest_ConnectOutput(cg_buffer);
			zest_SetPassTask(tst__intraframe_command_write, &capture);
			zest_EndPass();

			zest_frame_graph command_graph = zest_EndFrameGraph();
			test->result |= zest_GetFrameGraphResult(command_graph);
			zest_FlushFrameGraph(command_graph);
		}
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Intraframe Render Graph", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node rg_buffer = zest_AddTransientBufferResource("RG Buffer", &info);

			zest_BeginTransferPass("RG Upload");
			zest_ConnectOutput(rg_buffer);
			zest_SetPassTask(tst__intraframe_render_write, &capture);
			zest_EndPass();

			zest_BeginRenderPass("Read Buffer");
			zest_ConnectInput(rg_buffer);
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
		}
		zest_EndFrame(tests->context, frame_graph);
		test->result |= frame_graph ? zest_GetFrameGraphResult(frame_graph) : 1;
		//A fire-and-forget command graph's command buffer stays in the pending state as far as
		//validation is concerned (nothing ever waits on it), so the next frame's per-FIF command
		//pool reset would trip VUID-vkResetCommandPool-commandPool-00040 - a pre-existing hazard
		//of unwaited command graphs, unrelated to arenas. Idling the device retires the command
		//buffers; the deferred arena returns still only drain on the FIF cycle, so the
		//distinct-backing assertion above stays meaningful.
		zest_WaitForIdleDevice(tests->device);
	}
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	if (test->frame_count == test->run_count) {
		if (capture.captured_frames == 0) {
			test->result |= 2;   //The graphs never executed their transfer passes
		} else if (capture.mismatch_frames != capture.captured_frames) {
			ZEST_PRINT("Intraframe Two Graphs: buffers shared a backing on %i of %i frames",
				capture.captured_frames - capture.mismatch_frames, capture.captured_frames);
			test->result |= 4;
		}
	}
	return test->result;
}


