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
	zest_cmd_CopyBuffer(command_list, staging_buffer, write_buffer->storage_buffer, staging_buffer->size);
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



