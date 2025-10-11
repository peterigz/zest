#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define TINYKTX_IMPLEMENTATION
#define ZEST_TEST_MODE
#include "zest.h"
#include "zest-render-graph-tests.h"
#include "imgui_internal.h"

//Empty Graph: Compile and execute an empty render graph. It should do nothing and not crash.
int test__empty_graph(ZestTests *tests, Test *test) {
	if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
		zest_frame_graph frame_graph = zest_EndFrameGraphAndWait();
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Single Pass - No I/O: A graph with one pass that has no resource inputs or outputs. Should show status no work to do.
int test__single_pass(ZestTests *tests, Test *test) {
	if (zest_BeginFrameGraph(tests->context, "Single Pass Test", 0)) {
		zest_pass_node clear_pass = zest_BeginRenderPass("Empty Pass");
		zest_frame_graph frame_graph = zest_EndFrameGraphAndWait();
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Blank Screen: Compile and execute a blank screen. 
int test__blank_screen(ZestTests *tests, Test *test) {
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			zest_ImportSwapchainResource();
			zest_pass_node clear_pass = zest_BeginGraphicBlankScreen("Draw Nothing");
			zest_ConnectSwapChainOutput();
			zest_EndPass();
			frame_graph = zest_EndFrameGraph();
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
		}
		zest_EndFrame(tests->context);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Unused Pass Culling: Define two passes, A and B. Pass A writes to a resource, but that resource is never used as an input to Pass B or as a final
//output. Verify that Pass A is culled and never executed.
int test__pass_culling(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
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

			zest_BeginGraphicBlankScreen("Pass B");
			zest_ConnectSwapChainOutput();
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
		}
		zest_EndFrame(tests->context);
		test->result |= frame_graph ? zest_GetFrameGraphResult(frame_graph) : 1;
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Unused Resource Culling: Declare a resource that is never used by any pass. The graph should compile and run without trying to allocate memory for it.
int test__resource_culling(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Resource Culling", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);
			zest_BeginGraphicBlankScreen("Draw Nothing");
			zest_ConnectSwapChainOutput();
			zest_EndPass();
			zest_frame_graph frame_graph = zest_EndFrameGraph();
			test->result |= zest_GetFrameGraphCulledResourceCount(frame_graph) > 0 ? 1 : 0;
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
		}
		zest_EndFrame(tests->context);
		test->result |= frame_graph ? zest_GetFrameGraphResult(frame_graph) : 1;
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Chained Culling: Pass A writes to Resource X. Pass B reads from Resource X and writes to Resource Y. If Resource Y is not used as a final output, both
//Pass A and Pass B should be culled.
int test__chained_pass_culling(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
	
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
			zest_pass_node pass_b = zest_BeginGraphicBlankScreen("Pass B");
			zest_ConnectInput(output_x);
			zest_ConnectOutput(output_y);
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			zest_pass_node pass_c = zest_BeginGraphicBlankScreen("Pass C");
			zest_ConnectSwapChainOutput();
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
		}
		zest_EndFrame(tests->context);
		if (frame_graph && zest_GetFrameGraphCulledPassesCount(frame_graph) == 2) {
			test->result |= zest_GetFrameGraphResult(frame_graph);
		}
		test->result |= zest_GetValidationErrorCount(tests->context);
	}
	test->frame_count++;
	return test->result;
}

//Transient Resource Test: Pass A writes to a transient texture. Pass B reads from that texture. Verify the data is passed correctly. The graph should
//manage the creation and destruction of the transient texture in the appropriate passes.
int test__transient_image(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Transient Image", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);

			zest_pass_node pass_a = zest_BeginRenderPass("Pass A");
			zest_ConnectOutput(output_a);
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			zest_pass_node pass_b = zest_BeginGraphicBlankScreen("Pass B");
			zest_ConnectInput(output_a);
			zest_ConnectSwapChainOutput();
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
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
		zest_EndFrame(tests->context);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Input/Output Test: A pass that takes a pre-existing resource (e.g., a user-created buffer) as input and writes to a 
//final output resource (e.g., the swapchain).
int test__import_image(ZestTests *tests, Test *test) {
	if (!zest_IsValidImageHandle(tests->texture)) {
		zest_image_info_t image_info = zest_CreateImageInfo(256, 256);
		image_info.flags = zest_image_preset_storage;
		tests->texture = zest_CreateImage(tests->context, &image_info);
	}
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Import Image", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node imported_image = zest_ImportImageResource("Imported Texture", tests->texture, 0);

			zest_pass_node pass_a = zest_BeginGraphicBlankScreen("Pass B");
			zest_ConnectInput(imported_image);
			zest_ConnectSwapChainOutput();
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
			test->result |= zest_GetFrameGraphResult(frame_graph);
		}
		zest_EndFrame(tests->context);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
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
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Transient Image", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);

			zest_pass_node pass_a = zest_BeginRenderPass("Pass A");
			zest_ConnectOutput(output_a);
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			zest_pass_node pass_b = zest_BeginGraphicBlankScreen("Pass B");
			zest_ConnectInput(output_a);
			zest_ConnectSwapChainOutput();
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
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
						test->result += barriers->release_image_barriers[0].dst_access_mask == zest_access_none ? 0 : 1;
					}
				} else {
					test->result += 1;
				}
			} else {
				test->result += 1;
			}
		}
		zest_EndFrame(tests->context);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

void zest_WriteBufferCompute(const zest_command_list command_list, void *user_data) {
	ZestTests *tests = (ZestTests *)user_data;
	zest_resource_node write_buffer = zest_GetPassOutputResource(command_list, "Write Buffer");
	ZEST_ASSERT_HANDLE(write_buffer);


	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_descriptor_set sets[] = {
		zest_GetGlobalBindlessSet(tests->context)
	};

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, tests->compute_write, sets, 1);

	TestPushConstants push;

	// Update push constants for the current dispatch
	// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
	push.index1 = zest_GetTransientBufferBindlessIndex(command_list, write_buffer);

	zest_cmd_SendCustomComputePushConstants(command_list, tests->compute_write, &push);

	zest_uint group_count_x = (1000 + local_size_x - 1) / local_size_x;

	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, tests->compute_write, group_count_x, 1, 1);
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
		zest_GetGlobalBindlessSet(tests->context)
	};

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, tests->compute_verify, sets, 1);

	TestPushConstants push;

	// Update push constants for the current dispatch
	// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
	push.index1 = zest_GetTransientBufferBindlessIndex(command_list, write_buffer);
	push.index2 = zest_GetTransientBufferBindlessIndex(command_list, verify_buffer);

	zest_cmd_SendCustomComputePushConstants(command_list, tests->compute_verify, &push);

	zest_uint group_count_x = (1000 + local_size_x - 1) / local_size_x;

	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, tests->compute_verify, group_count_x, 1, 1);
}

/*
10. Buffer Write/Read:
* Pass A (Compute): Writes a specific data pattern into a storage buffer.
* Pass B (Compute): Reads from the buffer and verifies the data is correct (e.g., by writing a success/fail flag to another buffer that can be read by
  the CPU).
* Also tests executing and waiting to finish
*/
int test__buffer_read_write(ZestTests *tests, Test *test) {
	if (!zest_IsValidComputeHandle(tests->compute_write)) {
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->context, "examples/GLFW/zest-render-graph-tests/shaders/buffer_write.comp", "buffer_write.spv", shaderc_compute_shader, 1, compiler, 0);
		shaderc_compiler_release(compiler);
		zest_compute_builder_t builder = zest_BeginComputeBuilder(tests->context);
		zest_SetComputeBindlessLayout(&builder, zest_GetGlobalBindlessLayout(tests->context));
		zest_SetComputeUserData(&builder, tests);
		zest_AddComputeShader(&builder, shader);
		zest_SetComputePushConstantSize(&builder, sizeof(TestPushConstants));
		tests->compute_write = zest_FinishCompute(&builder, "Buffer Write");
		if (!zest_IsValidComputeHandle(tests->compute_write)) {
			test->frame_count++;
			test->result = -1;
			return test->result;
		}
	}
	if (!zest_IsValidComputeHandle(tests->compute_verify)) {
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->context, "examples/GLFW/zest-render-graph-tests/shaders/buffer_verify.comp", "buffer_verify.spv", shaderc_compute_shader, 1, compiler, 0);
		shaderc_compiler_release(compiler);
		zest_compute_builder_t builder = zest_BeginComputeBuilder(tests->context);
		zest_SetComputeBindlessLayout(&builder, zest_GetGlobalBindlessLayout(tests->context));
		zest_SetComputeUserData(&builder, tests);
		zest_AddComputeShader(&builder, shader);
		zest_SetComputePushConstantSize(&builder, sizeof(TestPushConstants));
		tests->compute_verify = zest_FinishCompute(&builder, "Buffer Verify");
		if (!zest_IsValidComputeHandle(tests->compute_verify)) {
			test->frame_count++;
			test->result = -1;
			return test->result;
		}
	}
	if (!tests->cpu_buffer) {
		zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_to_cpu);
		tests->cpu_buffer = zest_CreateBuffer(tests->context, sizeof(TestResults), &storage_buffer_info);
		tests->cpu_buffer_index = zest_AcquireGlobalStorageBufferIndex(tests->cpu_buffer);
			
	}
	zest_buffer_resource_info_t info = {};
	info.size = sizeof(TestData) * 1000;
	if (zest_BeginFrameGraph(tests->context, "Buffer Read/Write", 0)) {
		zest_resource_node write_buffer = zest_AddTransientBufferResource("Write Buffer", &info);
		zest_resource_node verify_buffer = zest_ImportBufferResource("Verify Buffer", tests->cpu_buffer, 0);

		zest_pass_node write_pass = zest_BeginComputePass(tests->compute_write, "Write Pass");
		zest_ConnectOutput(write_buffer);
		zest_SetPassTask(zest_WriteBufferCompute, tests);
		zest_EndPass();

		zest_pass_node verify_pass = zest_BeginComputePass(tests->compute_verify, "Verify Pass");
		zest_ConnectInput(write_buffer);
		zest_ConnectOutput(verify_buffer);
		zest_SetPassTask(zest_VerifyBufferCompute, tests);
		zest_EndPass();

		zest_frame_graph frame_graph = zest_EndFrameGraphAndWait();
		test->result |= zest_GetFrameGraphResult(frame_graph);
		TestData *test_data = (TestData *)zest_BufferData(tests->cpu_buffer);
		if (test_data->vec.x != 1.f) {
			test->result = 1;
		}
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

/*
Multi-Reader Barrier Test: Pass A writes to a resource. Passes B and C both read from that same resource. 
The graph should correctly synchronize this so B and C only execute after A is complete.
*/
int test__multi_reader_barrier(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_format_r8g8b8a8_unorm};
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Multi Reader Barrier", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);

			zest_pass_node pass_a = zest_BeginRenderPass("Pass A");
			zest_ConnectOutput(output_a);
			zest_SetPassTask(zest_EmptyRenderPass, NULL);
			zest_EndPass();

			zest_pass_node pass_b = zest_BeginGraphicBlankScreen("Pass B");
			zest_ConnectInput(output_a);
			zest_ConnectSwapChainOutput();
			zest_EndPass();

			zest_pass_node pass_c = zest_BeginGraphicBlankScreen("Pass C");
			zest_ConnectInput(output_a);
			zest_ConnectSwapChainOutput();
			zest_EndPass();
			frame_graph = zest_EndFrameGraph();
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
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
		zest_EndFrame(tests->context);
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
		zest_GetGlobalBindlessSet(tests->context)
	};

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, tests->compute_verify, sets, 1);

	TestPushConstants push;

	if (!tests->sampler.value) {
		tests->sampler = zest_CreateSampler(tests->context, &tests->sampler_info);
		tests->sampler_index = zest_AcquireGlobalSamplerIndex(tests->sampler, zest_sampler_binding);
	}

	// Update push constants for the current dispatch
	// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
	push.index1 = zest_GetTransientSampledImageBindlessIndex(command_list, read_image, zest_texture_2d_binding);
	push.index2 = tests->cpu_buffer_index;
	push.index3 = tests->sampler_index;
	push.index4 = zest_ScreenWidth(tests->context);
	push.index5 = zest_ScreenHeight(tests->context);

	zest_cmd_SendCustomComputePushConstants(command_list, tests->compute_verify, &push);

	zest_uint group_count_x = (zest_ScreenWidth(tests->context) + local_size_x - 1) / local_size_x;
	zest_uint group_count_y = (zest_ScreenHeight(tests->context) + local_size_y - 1) / local_size_y;

	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, tests->compute_verify, group_count_x, group_count_y, 1);
}


/*
Image Write / Read(Clear Color) :
* Pass A(Graphics) : Clears a transient image to a specific color.
* Pass B(Compute) : Reads the image pixels and verifies they match the clear color.
*/
int test__image_read_write(ZestTests *tests, Test *test) {
	if (!zest_IsValidComputeHandle(tests->compute_verify)) {
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->context, "examples/GLFW/zest-render-graph-tests/shaders/image_verify.comp", "image_verify.spv", shaderc_compute_shader, 1, compiler, 0);
		shaderc_compiler_release(compiler);
		zest_compute_builder_t builder = zest_BeginComputeBuilder(tests->context);
		zest_SetComputeBindlessLayout(&builder, zest_GetGlobalBindlessLayout(tests->context));
		zest_SetComputeUserData(&builder, tests);
		zest_AddComputeShader(&builder, shader);
		zest_SetComputePushConstantSize(&builder, sizeof(TestPushConstants));
		tests->compute_verify = zest_FinishCompute(&builder, "Image Verify");
		if (!zest_IsValidComputeHandle(tests->compute_verify)) {
			test->frame_count++;
			test->result = 1;
			return test->result;
		}
	}
	if (!tests->cpu_buffer) {
		zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_to_cpu);
		tests->cpu_buffer = zest_CreateBuffer(tests->context, sizeof(TestResults), &storage_buffer_info);
		tests->cpu_buffer_index = zest_AcquireGlobalStorageBufferIndex(tests->cpu_buffer);
			
	}
	zest_image_resource_info_t image_info = {zest_format_r8g8b8a8_unorm};
	if (zest_BeginFrameGraph(tests->context, "Image Read Write", 0)) {
		zest_resource_node write_buffer = zest_AddTransientImageResource("Write Buffer", &image_info);
		zest_resource_node verify_buffer = zest_ImportBufferResource("Verify Buffer", tests->cpu_buffer, 0);
		zest_SetResourceClearColor(write_buffer, 0.0f, 1.0f, 1.0f, 1.0f);

		zest_pass_node pass_a = zest_BeginRenderPass("Pass A");
		zest_ConnectOutput(write_buffer);
		zest_SetPassTask(zest_EmptyRenderPass, NULL);
		zest_EndPass();

		zest_pass_node verify_pass = zest_BeginComputePass(tests->compute_verify, "Pass B");
		zest_ConnectInput(write_buffer);
		zest_ConnectOutput(verify_buffer);
		zest_SetPassTask(zest_VerifyImageCompute, tests);
		zest_EndPass();

		zest_frame_graph frame_graph = zest_EndFrameGraphAndWait();

		TestData *test_data = (TestData *)zest_BufferData(tests->cpu_buffer);
		test->result |= zest_GetFrameGraphResult(frame_graph);
		if (test_data->vec.x == 0.f) {
			test->result = 1;
		}
		if (test_data->vec.y == 1.f) {
			test->result = 1;
		}
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
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
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Blank Screen", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node depth = zest_AddTransientImageResource("Depth Buffer", &depth_info);
			zest_FlagResourceAsEssential(depth);
			zest_pass_node clear_pass = zest_BeginGraphicBlankScreen("Draw Nothing");
			zest_SetSwapchainClearColor(tests->context, 0.0f, 0.1f, 0.2f, 1.0f);
			zest_ConnectSwapChainOutput();
			zest_ConnectOutput(depth);
			zest_EndPass();
			frame_graph = zest_EndFrameGraph();
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
		}
		zest_EndFrame(tests->context);
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
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

void zest_WriteImageCompute(const zest_command_list command_list, void *user_data) {
	ZestTests *tests = (ZestTests *)user_data;
	zest_resource_node write_buffer = zest_GetPassOutputResource(command_list, "Output A");
	ZEST_ASSERT_HANDLE(write_buffer);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_descriptor_set sets[] = {
		zest_GetGlobalBindlessSet(tests->context)
	};

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, tests->compute_write, sets, 1);

	TestPushConstants push;

	if (!tests->sampler.value) {
		tests->sampler = zest_CreateSampler(tests->context, &tests->sampler_info);
		tests->sampler_index = zest_AcquireGlobalSamplerIndex(tests->sampler, zest_sampler_binding);
	}

	// Update push constants for the current dispatch
	// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
	push.index1 = zest_GetTransientSampledImageBindlessIndex(command_list, write_buffer, zest_storage_image_binding);

	zest_cmd_SendCustomComputePushConstants(command_list, tests->compute_write, &push);

	zest_image_info_t image_desc = zest_GetResourceImageDescription(write_buffer);
	zest_uint group_count_x = (image_desc.extent.width + local_size_x - 1) / local_size_x;
	zest_uint group_count_y = (image_desc.extent.height + local_size_y - 1) / local_size_y;

	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, tests->compute_write, group_count_x, group_count_y, 1);
}


/*
Multi-Queue Synchronization:
* Pass A (Compute Queue): Processes data in a buffer.
* Pass B (Graphics Queue): Uses that buffer as a vertex buffer for rendering.
* The graph must handle the queue ownership transfer and synchronization (semaphores).
*/
int test__multi_queue_sync(ZestTests *tests, Test *test) {
	if (!zest_IsValidComputeHandle(tests->compute_write)) {
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->context, "examples/GLFW/zest-render-graph-tests/shaders/image_write2.comp", "image_write.spv", shaderc_compute_shader, 1, compiler, 0);
		shaderc_compiler_release(compiler);
		zest_compute_builder_t builder = zest_BeginComputeBuilder(tests->context);
		zest_SetComputeBindlessLayout(&builder, zest_GetGlobalBindlessLayout(tests->context));
		zest_SetComputeUserData(&builder, tests);
		zest_AddComputeShader(&builder, shader);
		zest_SetComputePushConstantSize(&builder, sizeof(TestPushConstants));
		tests->compute_write = zest_FinishCompute(&builder, "Buffer Write");
		if (!zest_IsValidComputeHandle(tests->compute_write)) {
			test->frame_count++;
			test->result = 1;
			return test->result;
		}
	}

	zest_SetSwapchainClearColor(tests->context, 0.f, .1f, .2f, 1.f);
	zest_image_resource_info_t info = { zest_format_r8g8b8a8_unorm };
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Multi Queue Sync", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);

			zest_pass_node pass_a = zest_BeginComputePass(tests->compute_write, "Pass A");
			zest_ConnectOutput(output_a);
			zest_SetPassTask(zest_WriteImageCompute, tests);
			zest_EndPass();

			zest_pass_node pass_b = zest_BeginGraphicBlankScreen("Pass B");
			zest_ConnectInput(output_a);
			zest_ConnectSwapChainOutput();
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
		}
		zest_EndFrame(tests->context);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

/*
Pass Grouping : Define two graphics passes that render to the same render target but have no dependency on each other. The graph compiler should ideally
group these into a single render pass with two subpasses for efficiency.
*/
int test__pass_grouping(ZestTests *tests, Test *test) {
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Pass Grouping", 0)) {
			zest_ImportSwapchainResource();

			zest_pass_node pass_a = zest_BeginGraphicBlankScreen("Draw Pass A");
			zest_ConnectSwapChainOutput();
			zest_EndPass();

			zest_pass_node pass_b = zest_BeginGraphicBlankScreen("Draw Pass B");
			zest_ConnectSwapChainOutput();
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
		}
		zest_EndFrame(tests->context);
		zest_uint final_pass_count = zest_GetFrameGraphFinalPassCount(frame_graph);
		test->result |= final_pass_count == 1 ? 0 : 1;
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

/*
Cyclic Dependency : Create a graph where Pass A depends on Pass B's output, and Pass B depends on Pass A's output.The graph compiler should detect this
cycle and return an error instead of crashing.
*/
int test__cyclic_dependency(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = { zest_format_r8g8b8a8_unorm };
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Cyclic Dependency", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);
			zest_resource_node output_b = zest_AddTransientImageResource("Output B", &info);

			zest_pass_node pass_a = zest_BeginGraphicBlankScreen("Draw Pass A");
			zest_ConnectInput(output_b);
			zest_ConnectOutput(output_a);
			zest_EndPass();

			zest_pass_node pass_b = zest_BeginGraphicBlankScreen("Draw Pass B");
			zest_ConnectInput(output_a);
			zest_ConnectOutput(output_b);
			zest_EndPass();

			zest_pass_node pass_c = zest_BeginGraphicBlankScreen("Draw Pass C");
			zest_ConnectSwapChainOutput();
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
		}
		zest_EndFrame(tests->context);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

/*
Re-executing Graph : Compile a graph once, then execute it for multiple consecutive frames.
Verify that no memory leaks or synchronization issues occur.
*/
int test__simple_caching(ZestTests *tests, Test *test) {
	zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(tests->context, 0, 0);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = zest_GetCachedFrameGraph(tests->context, &cache_key);
		if (!frame_graph) {
			if (zest_BeginFrameGraph(tests->context, "Blank Screen", &cache_key)) {
				zest_ImportSwapchainResource();
				zest_pass_node clear_pass = zest_BeginGraphicBlankScreen("Draw Nothing");
				zest_ConnectSwapChainOutput();
				zest_EndPass();
				frame_graph = zest_EndFrameGraph();
				zest_QueueFrameGraphForExecution(tests->context, frame_graph);
			}
		} else {
			zest_QueueFrameGraphForExecution(tests->context, frame_graph);
			test->cache_count++;
		}
		zest_EndFrame(tests->context);
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}
	test->result |= zest_GetValidationErrorCount(tests->context);
	if (test->frame_count > 0) {
		test->result |= test->cache_count == 0;
	}
	test->frame_count++;
	return test->result;
}


void InitialiseTests(ZestTests *tests) {

	tests->tests[0] = { "Empty Graph", test__empty_graph, 0, 0, zest_fgs_no_work_to_do, tests->simple_create_info};
	tests->tests[1] = { "Single Pass", test__single_pass, 0, 0, zest_fgs_no_work_to_do, tests->simple_create_info};
	tests->tests[2] = { "Blank Screen", test__blank_screen, 0, 0, 0, tests->simple_create_info };
	tests->tests[3] = { "Pass Culling", test__pass_culling, 0, 0, 0, tests->simple_create_info };
	tests->tests[4] = { "Resource Culling", test__resource_culling, 0, 0, 1, tests->simple_create_info };
	tests->tests[5] = { "Chained Pass Culling", test__chained_pass_culling, 0, 0, 0, tests->simple_create_info };
	tests->tests[6] = { "Transient Image", test__transient_image, 0, 0, 0, tests->simple_create_info };
	tests->tests[7] = { "Import Image", test__import_image, 0, 0, 0, tests->simple_create_info };
	tests->tests[8] = { "Image Barriers", test__image_barrier_tests, 0, 0, 0, tests->simple_create_info };
	tests->tests[9] = { "Buffer Read/Write", test__buffer_read_write, 0, 0, 0, tests->simple_create_info };
	tests->tests[10] = { "Multi Reader Barrier", test__multi_reader_barrier, 0, 0, 0, tests->simple_create_info };
	tests->tests[11] = { "Image Write/Read", test__image_read_write, 0, 0, 0, tests->simple_create_info };
	tests->tests[12] = { "Depth Attachment", test__depth_attachment, 0, 0, 0, tests->depth_create_info };
	tests->tests[13] = { "Multi Queue Sync", test__multi_queue_sync, 0, 0, 0, tests->simple_create_info };
	tests->tests[14] = { "Pass Grouping", test__pass_grouping, 0, 0, 0, tests->simple_create_info };
	tests->tests[15] = { "Cyclic Dependency", test__cyclic_dependency, 0, 0, zest_fgs_cyclic_dependency, tests->simple_create_info };
	tests->tests[16] = { "Simple Graph Cache", test__simple_caching, 0, 0, 0, tests->simple_create_info };

	tests->sampler_info = zest_CreateSamplerInfo();
	tests->mipped_sampler_info = zest_CreateMippedSamplerInfo(7);

	tests->current_test = 0;
    zest_ResetValidationErrors(tests->context);
}

void ResetTests(ZestTests *tests) {
	tests->texture = { 0 };
	tests->compute_verify = { 0 };
	tests->compute_write = { 0 };
	tests->cpu_buffer = { 0 };
	tests->sampler = { 0 };
	tests->mipped_sampler = { 0 };
}

void RunTests(ZestTests *tests) {

	while (1) {
		Test *current_test = &tests->tests[tests->current_test];
		int result = current_test->the_test(tests, current_test);

		if (current_test->frame_count == ZEST_MAX_FIF) {
			if (current_test->result != current_test->expected_result) {
				ZEST_PRINT("%s test failed", current_test->name);
			} else {
				ZEST_PRINT("%s test passed", current_test->name);
			}
			if (tests->current_test < TEST_COUNT - 1) {
				tests->current_test++;
				zest_SetCreateInfo(&tests->tests[tests->current_test].create_info);
				zest_ResetRenderer(tests->context);
				zest_ResetValidationErrors(tests->context);
				ResetTests(tests);
			} else {
				break;
			}
		}
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {

	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(zest_validation_flag_enable_sync | zest_validation_flag_best_practices);
//	zest_create_info_t create_info = zest_CreateInfo();
	//ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_memory);
	ZEST__UNFLAG(create_info.flags, zest_init_flag_cache_shaders);
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);

	ZestTests tests = {};
	tests.simple_create_info = create_info;
	tests.depth_create_info = create_info;

	if (!glfwInit()) {
		return 0;
	}
	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);

	//Create the device that serves all vulkan based context
	zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
	zest_AddDeviceBuilderExtensions(device_builder, glfw_extensions, count);
	zest_AddDeviceBuilderValidation(device_builder);
	zest_DeviceBuilderLogToConsole(device_builder);
	zest_device device = zest_EndDeviceBuilder(device_builder);

	//Create a window using GLFW
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");
	//Initialise Zest
	tests.context = zest_Initialise(device, window_handles, &create_info);

	InitialiseTests(&tests);

	//Set the Zest use data
	zest_SetUserData(&tests);
	RunTests(&tests);
	
	//Start the main loop
	zest_Shutdown(tests.context);

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	zest_implglfw_SetCallbacks(&create_info);
    ZEST__FLAG(create_info.flags, zest_init_flag_maximised);

	ImGuiApp imgui_app;

    create_info.log_path = ".";
	zest_Initialise(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#endif
