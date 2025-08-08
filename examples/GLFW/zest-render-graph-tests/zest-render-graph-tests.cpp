#include "zest-render-graph-tests.h"
#include "imgui_internal.h"

/*
  Core Functionality & Culling

  Resource Management & Barriers

  Data Integrity
  Complex Scenarios & Error Handling
   16. Re-executing Graph: Compile a graph once, then execute it for multiple consecutive frames. Verify that no memory leaks or synchronization issues occur.
*/

//Empty Graph: Compile and execute an empty render graph. It should do nothing and not crash.
int test__empty_graph(ZestTests *tests, Test *test) {
	if (zest_BeginRenderGraph("Blank Screen")) {
		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
	}
	test->result |= zest_GetValidationErrorCount();
	test->frame_count++;
	return test->result;
}

//Single Pass - No I/O: A graph with one pass that has no resource inputs or outputs. Should show status no work to do.
int test__single_pass(ZestTests *tests, Test *test) {
	if (zest_BeginRenderGraph("Single Pass Test")) {
		zest_pass_node clear_pass = zest_AddRenderPassNode("Empty Pass");
		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
	}
	test->result |= zest_GetValidationErrorCount();
	test->frame_count++;
	return test->result;
}

//Blank Screen: Compile and execute a blank screen. 
int test__blank_screen(ZestTests *tests, Test *test) {
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Blank Screen")) {
		zest_pass_node clear_pass = zest_AddGraphicBlankScreen("Draw Nothing");
		zest_ConnectSwapChainOutput(clear_pass);
		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
	}
	test->result |= zest_GetValidationErrorCount();
	test->frame_count++;
	return test->result;
}

//Unused Pass Culling: Define two passes, A and B. Pass A writes to a resource, but that resource is never used as an input to Pass B or as a final
//output. Verify that Pass A is culled and never executed.
int test__pass_culling(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_texture_format_rgba_unorm};
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Pass Culling")) {
		zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);

		//This pass should get culled
		zest_pass_node pass_a = zest_AddRenderPassNode("Pass A");
		zest_ConnectOutput(pass_a, output_a);
		zest_SetPassTask(pass_a, zest_EmptyRenderPass, NULL);

		zest_pass_node pass_b = zest_AddGraphicBlankScreen("Pass B");
		zest_ConnectSwapChainOutput(pass_b);

		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
	}
	test->result |= zest_GetValidationErrorCount();
	test->frame_count++;
	return test->result;
}

//Unused Resource Culling: Declare a resource that is never used by any pass. The graph should compile and run without trying to allocate memory for it.
int test__resource_culling(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_texture_format_rgba_unorm};
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Resource Culling")) {
		zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);
		zest_pass_node clear_pass = zest_AddGraphicBlankScreen("Draw Nothing");
		zest_ConnectSwapChainOutput(clear_pass);
		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
	}
	test->result |= zest_GetValidationErrorCount();
	test->frame_count++;
	return test->result;
}

//Chained Culling: Pass A writes to Resource X. Pass B reads from Resource X and writes to Resource Y. If Resource Y is not used as a final output, both
//Pass A and Pass B should be culled.
int test__chained_pass_culling(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_texture_format_rgba_unorm};
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Chained Pass Culling")) {
		zest_resource_node output_x = zest_AddTransientImageResource("Output X", &info);
		zest_resource_node output_y = zest_AddTransientImageResource("Output Y", &info);

		//This pass should get culled
		zest_pass_node pass_a = zest_AddRenderPassNode("Pass A");
		zest_ConnectOutput(pass_a, output_x);
		zest_SetPassTask(pass_a, zest_EmptyRenderPass, NULL);

		//This pass should also get culled
		zest_pass_node pass_b = zest_AddGraphicBlankScreen("Pass B");
		zest_ConnectInput(pass_b, output_x, 0);
		zest_ConnectOutput(pass_b, output_y);
		zest_SetPassTask(pass_b, zest_EmptyRenderPass, NULL);

		zest_pass_node pass_c = zest_AddGraphicBlankScreen("Pass C");
		zest_ConnectSwapChainOutput(pass_c);

		zest_render_graph render_graph = zest_EndRenderGraph();
		if (render_graph->culled_passes_count == 2) {
			test->result |= render_graph->error_status;
		}
	}
	test->result |= zest_GetValidationErrorCount();
	test->frame_count++;
	return test->result;
}

//Transient Resource Test: Pass A writes to a transient texture. Pass B reads from that texture. Verify the data is passed correctly. The graph should
//manage the creation and destruction of the transient texture in the appropriate passes.
int test__transient_image(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_texture_format_rgba_unorm};
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Transient Image")) {
		zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);

		zest_pass_node pass_a = zest_AddRenderPassNode("Pass A");
		zest_ConnectOutput(pass_a, output_a);
		zest_SetPassTask(pass_a, zest_EmptyRenderPass, NULL);

		zest_pass_node pass_b = zest_AddGraphicBlankScreen("Pass B");
		zest_ConnectInput(pass_b, output_a, 0);
		zest_ConnectSwapChainOutput(pass_b);

		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
		if (zest_map_size(render_graph->final_passes) == 2) {
			zest_pass_group_t *final_pass_a = zest_map_at_key(render_graph->final_passes, pass_a->output_key);
			zest_pass_group_t *final_pass_b = zest_map_at_key(render_graph->final_passes, pass_b->output_key);
			zest_uint create_size = zest_vec_size(final_pass_a->transient_resources_to_create);
			if (create_size != 1) {
				test->result = 1;
			}
			zest_uint free_size = zest_vec_size(final_pass_b->transient_resources_to_free);
			if (free_size != 1) {
				test->result = 1;
			}
		} else {
			test->result = 1;
		}
	}
	test->result |= zest_GetValidationErrorCount();
	test->frame_count++;
	return test->result;
}

//Input/Output Test: A pass that takes a pre-existing resource (e.g., a user-created buffer) as input and writes to a 
//final output resource (e.g., the swapchain).
int test__import_image(ZestTests *tests, Test *test) {
	if (!tests->texture) {
		tests->texture = zest_CreateTexturePacked("Sprite Texture", zest_texture_format_rgba_unorm);
		zest_image player_image = zest_AddTextureImageFile(tests->texture, "examples/assets/vaders/player.png");
		zest_ProcessTextureImages(tests->texture);
	}
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Import Image")) {
		zest_resource_node imported_image = zest_ImportImageResource("Imported Texture", tests->texture);

		zest_pass_node pass_a = zest_AddGraphicBlankScreen("Pass B");
		zest_ConnectInput(pass_a, imported_image, 0);
		zest_ConnectSwapChainOutput(pass_a);

		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
	}
	test->result |= zest_GetValidationErrorCount();
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
	zest_image_resource_info_t info = {zest_texture_format_rgba_unorm};
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Transient Image")) {
		zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);

		zest_pass_node pass_a = zest_AddRenderPassNode("Pass A");
		zest_ConnectOutput(pass_a, output_a);
		zest_SetPassTask(pass_a, zest_EmptyRenderPass, NULL);

		zest_pass_node pass_b = zest_AddGraphicBlankScreen("Pass B");
		zest_ConnectInput(pass_b, output_a, 0);
		zest_ConnectSwapChainOutput(pass_b);

		zest_render_graph render_graph = zest_EndRenderGraph();
		//zest_PrintCompiledRenderGraph(render_graph);
		test->result |= render_graph->error_status;
		if (zest_vec_size(render_graph->submissions) == 1) {
			zest_wave_submission_t *submission = &render_graph->submissions[0];
			if (zest_vec_size(submission->batches) == 1) {
				zest_submission_batch_t *batch = &submission->batches[0];
				zest_pass_group_t *grouped_pass = &render_graph->final_passes.data[batch->pass_indices[0]];
                zest_execution_details_t *exe_details = &grouped_pass->execution_details;
				zest_execution_barriers_t *barriers = &exe_details->barriers;
				zest_uint acquire_size = zest_vec_size(barriers->acquire_image_barriers);
				zest_uint release_size = zest_vec_size(barriers->release_image_barriers);
				if (acquire_size == 1 && release_size == 1) {
					test->result += barriers->acquire_image_barriers[0].oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : 1;
					test->result += barriers->acquire_image_barriers[0].newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ? 0 : 1;
					test->result += barriers->acquire_image_barriers[0].srcAccessMask == VK_ACCESS_NONE ? 0 : 1;
					test->result += barriers->acquire_image_barriers[0].dstAccessMask == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT ? 0 : 1;
					test->result += barriers->release_image_barriers[0].oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ? 0 : 1;
					test->result += barriers->release_image_barriers[0].newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ? 0 : 1;
					test->result += barriers->release_image_barriers[0].srcAccessMask == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT ? 0 : 1;
					test->result += barriers->release_image_barriers[0].dstAccessMask == VK_ACCESS_NONE ? 0 : 1;
				}
			} else {
				test->result += 1;
			}
		} else {
			test->result += 1;
		}
	}
	test->result |= zest_GetValidationErrorCount();
	test->frame_count++;
	return test->result;
}

void zest_WriteBufferCompute(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	ZestTests *tests = (ZestTests *)user_data;
	zest_resource_node write_buffer = zest_GetPassOutputResource(context->pass_node, "Write Buffer");
	ZEST_ASSERT_HANDLE(write_buffer);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	VkDescriptorSet sets[] = {
		ZestRenderer->global_set->vk_descriptor_set,
	};

	// Bind the pipeline once before the loop
	zest_BindComputePipeline(command_buffer, tests->compute_write, sets, 1);

	TestPushConstants push;

	// Update push constants for the current dispatch
	// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
	push.index1 = zest_GetTransientBufferBindlessIndex(context, write_buffer);

	zest_SendCustomComputePushConstants(command_buffer, tests->compute_write, &push);

	zest_uint group_count_x = (1000 + local_size_x - 1) / local_size_x;

	//Dispatch the compute shader
	zest_DispatchCompute(command_buffer, tests->compute_write, group_count_x, 1, 1);
}

void zest_VerifyBufferCompute(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	ZestTests *tests = (ZestTests *)user_data;
	zest_resource_node write_buffer = zest_GetPassInputResource(context->pass_node, "Write Buffer");
	zest_resource_node verify_buffer = zest_GetPassOutputResource(context->pass_node, "Verify Buffer");
	ZEST_ASSERT_HANDLE(write_buffer);
	ZEST_ASSERT_HANDLE(verify_buffer);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	VkDescriptorSet sets[] = {
		ZestRenderer->global_set->vk_descriptor_set,
	};

	// Bind the pipeline once before the loop
	zest_BindComputePipeline(command_buffer, tests->compute_verify, sets, 1);

	TestPushConstants push;

	// Update push constants for the current dispatch
	// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
	push.index1 = zest_GetTransientBufferBindlessIndex(context, write_buffer);
	push.index2 = zest_GetTransientBufferBindlessIndex(context, verify_buffer);

	zest_SendCustomComputePushConstants(command_buffer, tests->compute_verify, &push);

	zest_uint group_count_x = (1000 + local_size_x - 1) / local_size_x;

	//Dispatch the compute shader
	zest_DispatchCompute(command_buffer, tests->compute_verify, group_count_x, 1, 1);
}

/*
10. Buffer Write/Read:
* Pass A (Compute): Writes a specific data pattern into a storage buffer.
* Pass B (Compute): Reads from the buffer and verifies the data is correct (e.g., by writing a success/fail flag to another buffer that can be read by
  the CPU).
* Also tests executing and waiting to finish
*/
int test__buffer_read_write(ZestTests *tests, Test *test) {
	if (!tests->compute_write) {
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		zest_shader shader = zest_CreateShaderFromFile("examples/GLFW/zest-render-graph-tests/shaders/buffer_write.comp", "buffer_write.spv", shaderc_compute_shader, 1, compiler, 0);
		shaderc_compiler_release(compiler);
		zest_compute_builder_t builder = zest_BeginComputeBuilder();
		zest_SetComputeBindlessLayout(&builder, ZestRenderer->global_bindless_set_layout);
		zest_SetComputeUserData(&builder, tests);
		zest_AddComputeShader(&builder, shader);
		zest_SetComputePushConstantSize(&builder, sizeof(TestPushConstants));
		tests->compute_write = zest_FinishCompute(&builder, "Buffer Write");
	}
	if (!tests->compute_verify) {
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		zest_shader shader = zest_CreateShaderFromFile("examples/GLFW/zest-render-graph-tests/shaders/buffer_verify.comp", "buffer_verify.spv", shaderc_compute_shader, 1, compiler, 0);
		shaderc_compiler_release(compiler);
		zest_compute_builder_t builder = zest_BeginComputeBuilder();
		zest_SetComputeBindlessLayout(&builder, ZestRenderer->global_bindless_set_layout);
		zest_SetComputeUserData(&builder, tests);
		zest_AddComputeShader(&builder, shader);
		zest_SetComputePushConstantSize(&builder, sizeof(TestPushConstants));
		tests->compute_verify = zest_FinishCompute(&builder, "Buffer Verify");
	}
	if (!tests->cpu_buffer) {
		tests->cpu_buffer = zest_CreateCPUStorageBuffer(sizeof(TestResults), 0);
		tests->cpu_buffer_index = zest_AcquireGlobalStorageBufferIndex(tests->cpu_buffer);
			
	}
	zest_buffer_resource_info_t info = {};
	info.size = sizeof(TestData) * 1000;
	if (zest_BeginRenderGraph("Buffer Read/Write")) {
		zest_resource_node write_buffer = zest_AddTransientBufferResource("Write Buffer", &info);
		zest_resource_node verify_buffer = zest_ImportBufferResource("Verify Buffer", tests->cpu_buffer);

		zest_pass_node write_pass = zest_AddComputePassNode(tests->compute_write, "Write Pass");
		zest_ConnectOutput(write_pass, write_buffer);
		zest_SetPassTask(write_pass, zest_WriteBufferCompute, tests);

		zest_pass_node verify_pass = zest_AddComputePassNode(tests->compute_verify, "Verify Pass");
		zest_ConnectInput(verify_pass, write_buffer, 0);
		zest_ConnectOutput(verify_pass, verify_buffer);
		zest_SetPassTask(verify_pass, zest_VerifyBufferCompute, tests);

		zest_render_graph render_graph = zest_EndRenderGraphAndWait();
		test->result |= render_graph->error_status;
		TestData *test_data = (TestData *)tests->cpu_buffer->data;
		if (test_data->vec.x != 1.f) {
			test->result = 1;
		}
	}
	test->result |= zest_GetValidationErrorCount();
	test->frame_count++;
	return test->result;
}

/*
Multi-Reader Barrier Test: Pass A writes to a resource. Passes B and C both read from that same resource. 
The graph should correctly synchronize this so B and C only execute after A is complete.
*/
int test__multi_reader_barrier(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_texture_format_rgba_unorm};
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Multi Reader Barrier")) {
		zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);

		zest_pass_node pass_a = zest_AddRenderPassNode("Pass A");
		zest_ConnectOutput(pass_a, output_a);
		zest_SetPassTask(pass_a, zest_EmptyRenderPass, NULL);

		zest_pass_node pass_b = zest_AddGraphicBlankScreen("Pass B");
		zest_ConnectInput(pass_b, output_a, 0);
		zest_ConnectSwapChainOutput(pass_b);

		zest_pass_node pass_c = zest_AddGraphicBlankScreen("Pass C");
		zest_ConnectInput(pass_c, output_a, 0);
		zest_ConnectSwapChainOutput(pass_c);
		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
		int failed = 0;
		if (zest_vec_size(render_graph->submissions) == 1) {
			if (zest_vec_size(render_graph->submissions[0].batches) == 1) {
				if (zest_vec_size(render_graph->submissions[0].batches[0].pass_indices) == 2) {
					zest_pass_group_t *pass_group_a = &render_graph->final_passes.data[render_graph->submissions[0].batches[0].pass_indices[0]];
					zest_pass_group_t *pass_group_b = &render_graph->final_passes.data[render_graph->submissions[0].batches[0].pass_indices[1]];
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
	test->frame_count++;
	return test->result;
}

void zest_VerifyImageCompute(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	ZestTests *tests = (ZestTests *)user_data;
	zest_resource_node read_image = zest_GetPassInputResource(context->pass_node, "Write Buffer");
	zest_resource_node verify_buffer = zest_GetPassOutputResource(context->pass_node, "Verify Buffer");
	ZEST_ASSERT_HANDLE(read_image);
	ZEST_ASSERT_HANDLE(verify_buffer);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	VkDescriptorSet sets[] = {
		ZestRenderer->global_set->vk_descriptor_set,
	};

	// Bind the pipeline once before the loop
	zest_BindComputePipeline(command_buffer, tests->compute_verify, sets, 1);

	TestPushConstants push;

	// Update push constants for the current dispatch
	// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
	push.index1 = zest_GetTransientImageBindlessIndex(context, read_image, ZEST_TRUE, zest_binding_type_combined_image_sampler);
	push.index2 = tests->cpu_buffer_index;
	push.index3 = zest_ScreenWidth();
	push.index4 = zest_ScreenHeight();

	zest_SendCustomComputePushConstants(command_buffer, tests->compute_verify, &push);

	zest_uint group_count_x = (zest_ScreenWidth() + local_size_x - 1) / local_size_x;
	zest_uint group_count_y = (zest_ScreenHeight() + local_size_y - 1) / local_size_y;

	//Dispatch the compute shader
	zest_DispatchCompute(command_buffer, tests->compute_verify, group_count_x, group_count_y, 1);
}


/*
Image Write / Read(Clear Color) :
* Pass A(Graphics) : Clears a transient image to a specific color.
* Pass B(Compute) : Reads the image pixels and verifies they match the clear color.
*/
int test__image_read_write(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = {zest_texture_format_rgba_unorm};
	if (!tests->compute_verify) {
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		zest_shader shader = zest_CreateShaderFromFile("examples/GLFW/zest-render-graph-tests/shaders/image_verify.comp", "image_verify.spv", shaderc_compute_shader, 1, compiler, 0);
		shaderc_compiler_release(compiler);
		zest_compute_builder_t builder = zest_BeginComputeBuilder();
		zest_SetComputeBindlessLayout(&builder, ZestRenderer->global_bindless_set_layout);
		zest_SetComputeUserData(&builder, tests);
		zest_AddComputeShader(&builder, shader);
		zest_SetComputePushConstantSize(&builder, sizeof(TestPushConstants));
		tests->compute_verify = zest_FinishCompute(&builder, "Image Verify");
	}
	if (!tests->cpu_buffer) {
		tests->cpu_buffer = zest_CreateCPUStorageBuffer(sizeof(TestResults), 0);
		tests->cpu_buffer_index = zest_AcquireGlobalStorageBufferIndex(tests->cpu_buffer);
			
	}
	if (zest_BeginRenderGraph("Image Read Write")) {
		zest_resource_node write_buffer = zest_AddTransientImageResource("Write Buffer", &info);
		zest_resource_node verify_buffer = zest_ImportBufferResource("Verify Buffer", tests->cpu_buffer);
		zest_SetResourceClearColor(write_buffer, 0.0f, 1.0f, 1.0f, 1.0f);

		zest_pass_node pass_a = zest_AddRenderPassNode("Pass A");
		zest_ConnectOutput(pass_a, write_buffer);
		zest_SetPassTask(pass_a, zest_EmptyRenderPass, NULL);

		zest_pass_node verify_pass = zest_AddComputePassNode(tests->compute_verify, "Pass B");
		zest_ConnectInput(verify_pass, write_buffer, 0);
		zest_ConnectOutput(verify_pass, verify_buffer);
		zest_SetPassTask(verify_pass, zest_VerifyImageCompute, tests);

		zest_render_graph render_graph = zest_EndRenderGraphAndWait();

		TestData *test_data = (TestData *)tests->cpu_buffer->data;
		test->result |= render_graph->error_status;
		if (test_data->vec.x == 0.f) {
			test->result = 1;
		}
		if (test_data->vec.y == 1.f) {
			test->result = 1;
		}
	}
	test->result |= zest_GetValidationErrorCount();
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
	zest_image_resource_info_t info = { zest_texture_format_rgba_unorm };
	zest_image_resource_info_t depth_info = { zest_texture_format_depth };
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Blank Screen")) {
		zest_resource_node depth = zest_AddTransientImageResource("Depth Buffer", &depth_info);
		zest_FlagResourceAsEssential(depth);
		zest_pass_node clear_pass = zest_AddGraphicBlankScreen("Draw Nothing");
		zest_SetSwapchainClearColor(zest_GetMainWindowSwapchain(), 0.0f, 0.1f, 0.2f, 1.0f);
		zest_ConnectSwapChainOutput(clear_pass);
		zest_ConnectOutput(clear_pass, depth);
		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result = 1;
		if (zest_map_size(render_graph->final_passes) == 1) {
			zest_pass_group_t *pass_group = &render_graph->final_passes.data[0];
			if (zest_vec_size(pass_group->execution_details.attachment_resource_nodes) == 2) {
				zest_resource_node swapchain_resource = pass_group->execution_details.attachment_resource_nodes[0];
				zest_resource_node depth_resource = pass_group->execution_details.attachment_resource_nodes[1];
				if (swapchain_resource->type == zest_resource_type_swap_chain_image && depth_resource->type == zest_resource_type_depth) {
					test->result = 0;
				}
			}
		}
		test->result |= render_graph->error_status;
	}
	test->result |= zest_GetValidationErrorCount();
	test->frame_count++;
	return test->result;
}

void zest_WriteImageCompute(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	ZestTests *tests = (ZestTests *)user_data;
	zest_resource_node write_buffer = zest_GetPassOutputResource(context->pass_node, "Output A");
	ZEST_ASSERT_HANDLE(write_buffer);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	VkDescriptorSet sets[] = {
		ZestRenderer->global_set->vk_descriptor_set,
	};

	// Bind the pipeline once before the loop
	zest_BindComputePipeline(command_buffer, tests->compute_write, sets, 1);

	TestPushConstants push;

	// Update push constants for the current dispatch
	// Note: You may need to update the BlurPushConstants struct to remove dst_mip_index
	push.index1 = zest_GetTransientImageBindlessIndex(context, write_buffer, true, zest_binding_type_storage_image);

	zest_SendCustomComputePushConstants(command_buffer, tests->compute_write, &push);

	zest_uint group_count_x = (write_buffer->image_desc.width + local_size_x - 1) / local_size_x;
	zest_uint group_count_y = (write_buffer->image_desc.height + local_size_y - 1) / local_size_y;

	//Dispatch the compute shader
	zest_DispatchCompute(command_buffer, tests->compute_write, group_count_x, group_count_y, 1);
}


/*
Multi-Queue Synchronization:
* Pass A (Compute Queue): Processes data in a buffer.
* Pass B (Graphics Queue): Uses that buffer as a vertex buffer for rendering.
* The graph must handle the queue ownership transfer and synchronization (semaphores).
*/
int test__multi_queue_sync(ZestTests *tests, Test *test) {
	if (!tests->compute_write) {
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		zest_shader shader = zest_CreateShaderFromFile("examples/GLFW/zest-render-graph-tests/shaders/image_write2.comp", "image_write.spv", shaderc_compute_shader, 1, compiler, 0);
		shaderc_compiler_release(compiler);
		zest_compute_builder_t builder = zest_BeginComputeBuilder();
		zest_SetComputeBindlessLayout(&builder, ZestRenderer->global_bindless_set_layout);
		zest_SetComputeUserData(&builder, tests);
		zest_AddComputeShader(&builder, shader);
		zest_SetComputePushConstantSize(&builder, sizeof(TestPushConstants));
		tests->compute_write = zest_FinishCompute(&builder, "Buffer Write");
	}

	zest_SetSwapchainClearColor(zest_GetMainWindowSwapchain(), 0.f, .1f, .2f, 1.f);
	zest_image_resource_info_t info = { zest_texture_format_rgba_unorm };
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Multi Queue Sync")) {
		zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);

		zest_pass_node pass_a = zest_AddComputePassNode(tests->compute_write, "Pass A");
		zest_ConnectOutput(pass_a, output_a);
		zest_SetPassTask(pass_a, zest_WriteImageCompute, tests);

		zest_pass_node pass_b = zest_AddGraphicBlankScreen("Pass B");
		zest_ConnectInput(pass_b, output_a, 0);
		zest_ConnectSwapChainOutput(pass_b);

		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
	}
	test->result |= zest_GetValidationErrorCount();
	test->frame_count++;
	return test->result;
}

/*
Pass Grouping : Define two graphics passes that render to the same render target but have no dependency on each other. The graph compiler should ideally
group these into a single render pass with two subpasses for efficiency.
*/
int test__pass_grouping(ZestTests *tests, Test *test) {
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Pass Grouping")) {

		zest_pass_node pass_a = zest_AddGraphicBlankScreen("Draw Pass A");
		zest_ConnectSwapChainOutput(pass_a);

		zest_pass_node pass_b = zest_AddGraphicBlankScreen("Draw Pass B");
		zest_ConnectSwapChainOutput(pass_b);

		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
		zest_uint final_pass_count = zest_map_size(render_graph->final_passes);
		test->result |= final_pass_count == 1 ? 0 : 1;
	}
	test->result |= zest_GetValidationErrorCount();
	test->frame_count++;
	return test->result;
}

/*
Cyclic Dependency : Create a graph where Pass A depends on Pass B's output, and Pass B depends on Pass A's output.The graph compiler should detect this
cycle and return an error instead of crashing.
*/
int test__cyclic_dependency(ZestTests *tests, Test *test) {
	zest_image_resource_info_t info = { zest_texture_format_rgba_unorm };
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Cyclic Dependency")) {
		zest_resource_node output_a = zest_AddTransientImageResource("Output A", &info);
		zest_resource_node output_b = zest_AddTransientImageResource("Output B", &info);

		zest_pass_node pass_a = zest_AddGraphicBlankScreen("Draw Pass A");
		zest_ConnectInput(pass_a, output_b, 0);
		zest_ConnectOutput(pass_a, output_a);

		zest_pass_node pass_b = zest_AddGraphicBlankScreen("Draw Pass B");
		zest_ConnectInput(pass_b, output_a, 0);
		zest_ConnectOutput(pass_b, output_b);

		zest_pass_node pass_c = zest_AddGraphicBlankScreen("Draw Pass C");
		zest_ConnectSwapChainOutput(pass_c);

		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
	}
	test->result |= zest_GetValidationErrorCount();
	test->frame_count++;
	return test->result;
}

/*
Re-executing Graph : Compile a graph once, then execute it for multiple consecutive frames.
Verify that no memory leaks or synchronization issues occur.
*/


void InitialiseTests(ZestTests *tests) {

	tests->tests[0] = { "Empty Graph", test__empty_graph, 0, 0, zest_rgs_no_work_to_do, tests->simple_create_info};
	tests->tests[1] = { "Single Pass", test__single_pass, 0, 0, zest_rgs_no_work_to_do | zest_rgs_passes_were_culled, tests->simple_create_info};
	tests->tests[2] = { "Blank Screen", test__blank_screen, 0, 0, 0, tests->simple_create_info };
	tests->tests[3] = { "Pass Culling", test__pass_culling, 0, 0, zest_rgs_passes_were_culled, tests->simple_create_info };
	tests->tests[4] = { "Resource Culling", test__resource_culling, 0, 0, 0, tests->simple_create_info };
	tests->tests[5] = { "Chained Pass Culling", test__chained_pass_culling, 0, 0, zest_rgs_passes_were_culled, tests->simple_create_info };
	tests->tests[6] = { "Transient Image", test__transient_image, 0, 0, 0, tests->simple_create_info };
	tests->tests[7] = { "Import Image", test__import_image, 0, 0, 0, tests->simple_create_info };
	tests->tests[8] = { "Image Barriers", test__image_barrier_tests, 0, 0, 0, tests->simple_create_info };
	tests->tests[9] = { "Buffer Read/Write", test__buffer_read_write, 0, 0, 0, tests->simple_create_info };
	tests->tests[10] = { "Multi Reader Barrier", test__multi_reader_barrier, 0, 0, 0, tests->simple_create_info };
	tests->tests[11] = { "Image Write/Read", test__image_read_write, 0, 0, 0, tests->simple_create_info };
	tests->tests[12] = { "Depth Attachment", test__depth_attachment, 0, 0, 0, tests->depth_create_info };
	tests->tests[13] = { "Multi Queue Sync", test__multi_queue_sync, 0, 0, 0, tests->simple_create_info };
	tests->tests[14] = { "Pass Grouping", test__pass_grouping, 0, 0, 0, tests->simple_create_info };
	tests->tests[15] = { "Cyclic Dependency", test__cyclic_dependency, 0, 0, zest_rgs_cyclic_dependency, tests->simple_create_info };

	VkSamplerCreateInfo sampler_info = zest_CreateSamplerInfo();
	VkSamplerCreateInfo mipped_sampler_info = zest_CreateMippedSamplerInfo(7);
	tests->sampler_info = zest_CreateSamplerInfo();
	tests->mipped_sampler_info = zest_CreateMippedSamplerInfo(7);

	tests->current_test = 9;
    zest_ResetValidationErrors();
}

void ResetTests(ZestTests *tests) {
	tests->texture = NULL;
	tests->compute_verify = NULL;
	tests->compute_write = NULL;
	tests->cpu_buffer = NULL;
}

void UpdateCallback(zest_microsecs elapsed, void* user_data) {
	//The struct for this example app from the user data we set when initialising Zest
	ZestTests* tests = (ZestTests*)user_data;

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
			zest_ResetRenderer();
			zest_ResetValidationErrors();
			ResetTests(tests);
		}
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {

	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(0);
//	zest_create_info_t create_info = zest_CreateInfo();
	//ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_memory);
	ZEST__UNFLAG(create_info.flags, zest_init_flag_cache_shaders);
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	create_info.log_path = "./";
	create_info.thread_count = 0;

	ZestTests tests = {};
	tests.simple_create_info = create_info;
	tests.depth_create_info = create_info;

	//Initialise Zest
	zest_Initialise(&create_info);

	InitialiseTests(&tests);

	//Set the Zest use data
	zest_SetUserData(&tests);
	//Set the udpate callback to be called every frame
	zest_SetUserUpdateCallback(UpdateCallback);
	
	//Start the main loop
	zest_Start();

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
