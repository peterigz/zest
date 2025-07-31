#include "zest-render-graph-tests.h"
#include "imgui_internal.h"

/*
  Core Functionality & Culling

  Resource Management & Barriers

   9. Multi-Reader Barrier Test: Pass A writes to a resource. Passes B and C both read from that same resource. The graph should correctly synchronize this so
	  B and C only execute after A is complete.

  Data Integrity
   10. Buffer Write/Read:
	   * Pass A (Compute): Writes a specific data pattern into a storage buffer.
	   * Pass B (Compute): Reads from the buffer and verifies the data is correct (e.g., by writing a success/fail flag to another buffer that can be read by
		 the CPU).
   11. Image Write/Read (Clear Color):
	   * Pass A (Graphics): Clears a transient image to a specific color.
	   * Pass B (Compute): Reads the image pixels and verifies they match the clear color.
   12. Depth Attachment Test:
	   * Pass A: Renders geometry with depth testing/writing enabled to a transient depth buffer.
	   * Pass B: Renders geometry that should be occluded by the objects from Pass A.
	   * Verify the final image shows correct occlusion.

  Complex Scenarios & Error Handling
   13. Multi-Queue Synchronization:
	   * Pass A (Compute Queue): Processes data in a buffer.
	   * Pass B (Graphics Queue): Uses that buffer as a vertex buffer for rendering.
	   * The graph must handle the queue ownership transfer and synchronization (semaphores).
   14. Pass Grouping: Define two graphics passes that render to the same render target but have no dependency on each other. The graph compiler should ideally
	   group these into a single render pass with two subpasses for efficiency.
   15. Cyclic Dependency: Create a graph where Pass A depends on Pass B's output, and Pass B depends on Pass A's output. The graph compiler should detect this
	   cycle and return an error instead of crashing.
   16. Re-executing Graph: Compile a graph once, then execute it for multiple consecutive frames. Verify that no memory leaks or synchronization issues occur.
*/

//Empty Graph: Compile and execute an empty render graph. It should do nothing and not crash.
int test__empty_graph(ZestTests *tests, Test *test) {
	if (zest_BeginRenderGraph("Blank Screen")) {
		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
	}
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
	test->frame_count++;
	return test->result;
}

//Blank Screen: Compile and execute a blank screen. 
int test__blank_screen(ZestTests *tests, Test *test) {
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Blank Screen")) {
		zest_pass_node clear_pass = zest_AddGraphicBlankScreen("Draw Nothing");
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		zest_ConnectSwapChainOutput(clear_pass, clear_color);
		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
	}
	test->frame_count++;
	return test->result;
}

//Unused Pass Culling: Define two passes, A and B. Pass A writes to a resource, but that resource is never used as an input to Pass B or as a final
//output. Verify that Pass A is culled and never executed.
int test__pass_culling(ZestTests *tests, Test *test) {
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Pass Culling")) {
		zest_resource_node output_a = zest_AddRenderTarget("Output A", zest_texture_format_rgba_unorm, NULL, false);

		//This pass should get culled
		zest_pass_node pass_a = zest_AddRenderPassNode("Pass A");
		zest_ConnectRenderTargetOutput(pass_a, output_a);
		zest_SetPassTask(pass_a, zest_EmptyRenderPass, NULL);

		zest_pass_node pass_b = zest_AddGraphicBlankScreen("Pass B");
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		zest_ConnectSwapChainOutput(pass_b, clear_color);

		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
	}
	test->frame_count++;
	return test->result;
}

//Unused Resource Culling: Declare a resource that is never used by any pass. The graph should compile and run without trying to allocate memory for it.
int test__resource_culling(ZestTests *tests, Test *test) {
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Resource Culling")) {
		zest_resource_node output_a = zest_AddRenderTarget("Output A", zest_texture_format_rgba_unorm, NULL, false);
		zest_pass_node clear_pass = zest_AddGraphicBlankScreen("Draw Nothing");
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		zest_ConnectSwapChainOutput(clear_pass, clear_color);
		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
	}
	test->frame_count++;
	return test->result;
}

//Chained Culling: Pass A writes to Resource X. Pass B reads from Resource X and writes to Resource Y. If Resource Y is not used as a final output, both
//Pass A and Pass B should be culled.
int test__chained_pass_culling(ZestTests *tests, Test *test) {
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Chained Pass Culling")) {
		zest_resource_node output_x = zest_AddRenderTarget("Output X", zest_texture_format_rgba_unorm, NULL, false);
		zest_resource_node output_y = zest_AddRenderTarget("Output Y", zest_texture_format_rgba_unorm, NULL, false);

		//This pass should get culled
		zest_pass_node pass_a = zest_AddRenderPassNode("Pass A");
		zest_ConnectRenderTargetOutput(pass_a, output_x);
		zest_SetPassTask(pass_a, zest_EmptyRenderPass, NULL);

		//This pass should also get culled
		zest_pass_node pass_b = zest_AddGraphicBlankScreen("Pass B");
		zest_ConnectSampledImageInput(pass_b, output_x);
		zest_ConnectRenderTargetOutput(pass_b, output_y);
		zest_SetPassTask(pass_b, zest_EmptyRenderPass, NULL);

		zest_pass_node pass_c = zest_AddGraphicBlankScreen("Pass C");
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		zest_ConnectSwapChainOutput(pass_c, clear_color);

		zest_render_graph render_graph = zest_EndRenderGraph();
		if (render_graph->culled_passes_count == 2) {
			test->result |= render_graph->error_status;
		}
	}
	test->frame_count++;
	return test->result;
}

//Transient Resource Test: Pass A writes to a transient texture. Pass B reads from that texture. Verify the data is passed correctly. The graph should
//manage the creation and destruction of the transient texture in the appropriate passes.
int test__transient_image(ZestTests *tests, Test *test) {
	zest_sampler sampler = zest_GetSampler(&tests->sampler_info);
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Transient Image")) {
		zest_resource_node output_a = zest_AddRenderTarget("Output A", zest_texture_format_rgba_unorm, sampler, false);

		zest_pass_node pass_a = zest_AddRenderPassNode("Pass A");
		zest_ConnectRenderTargetOutput(pass_a, output_a);
		zest_SetPassTask(pass_a, zest_EmptyRenderPass, NULL);

		zest_pass_node pass_b = zest_AddGraphicBlankScreen("Pass B");
		zest_ConnectSampledImageInput(pass_b, output_a);
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		zest_ConnectSwapChainOutput(pass_b, clear_color);

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
	test->frame_count++;
	return test->result;
}

//Input/Output Test: A pass that takes a pre-existing resource (e.g., a user-created buffer) as input and writes to a 
//final output resource (e.g., the swapchain).
int test__import_image(ZestTests *tests, Test *test) {
	zest_sampler sampler = zest_GetSampler(&tests->sampler_info);
	if (!tests->texture) {
		tests->texture = zest_CreateTexturePacked("Sprite Texture", zest_texture_format_rgba_unorm);
		zest_image player_image = zest_AddTextureImageFile(tests->texture, "examples/assets/vaders/player.png");
		zest_ProcessTextureImages(tests->texture);
	}
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Import Image")) {
		zest_resource_node imported_image = zest_ImportImageResourceReadOnly("Imported Texture", tests->texture);

		zest_pass_node pass_a = zest_AddGraphicBlankScreen("Pass B");
		zest_ConnectSampledImageInput(pass_a, imported_image);
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		zest_ConnectSwapChainOutput(pass_a, clear_color);

		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
	}
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
	zest_sampler sampler = zest_GetSampler(&tests->sampler_info);
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Transient Image")) {
		zest_resource_node output_a = zest_AddRenderTarget("Output A", zest_texture_format_rgba_unorm, sampler, false);

		zest_pass_node pass_a = zest_AddRenderPassNode("Pass A");
		zest_ConnectRenderTargetOutput(pass_a, output_a);
		zest_SetPassTask(pass_a, zest_EmptyRenderPass, NULL);

		zest_pass_node pass_b = zest_AddGraphicBlankScreen("Pass B");
		zest_ConnectSampledImageInput(pass_b, output_a);
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		zest_ConnectSwapChainOutput(pass_b, clear_color);

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
	test->frame_count++;
	return test->result;
}

void zest_WriteBufferCompute(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	ZestTests *tests = (ZestTests *)user_data;
	zest_resource_node write_buffer = zest_GetPassOutputResource(context->pass_node, "Write Buffer");
	ZEST_CHECK_HANDLE(write_buffer);

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
	push.index1 = write_buffer->bindless_index;

	zest_SendCustomComputePushConstants(command_buffer, tests->compute_write, &push);

	zest_uint group_count_x = (1000 + local_size_x - 1) / local_size_x;

	//Dispatch the compute shader
	zest_DispatchCompute(command_buffer, tests->compute_write, group_count_x, 1, 1);
}

void zest_VerifyBufferCompute(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	ZestTests *tests = (ZestTests *)user_data;
	zest_resource_node write_buffer = zest_GetPassInputResource(context->pass_node, "Write Buffer");
	zest_resource_node verify_buffer = zest_GetPassOutputResource(context->pass_node, "Verify Buffer");
	ZEST_CHECK_HANDLE(write_buffer);
	ZEST_CHECK_HANDLE(verify_buffer);

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
	push.index1 = write_buffer->bindless_index;
	push.index2 = tests->cpu_buffer_index;

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
	if (zest_BeginRenderGraph("Buffer Read/Write")) {
		zest_resource_node write_buffer = zest_AddTransientStorageBufferResource("Write Buffer", sizeof(TestData) * 1000, ZEST_TRUE);
		zest_resource_node verify_buffer = zest_ImportStorageBufferResource("Verify Buffer", tests->cpu_buffer);

		zest_pass_node write_pass = zest_AddComputePassNode(tests->compute_write, "Write Pass");
		zest_ConnectStorageBufferOutput(write_pass, write_buffer);
		zest_SetPassTask(write_pass, zest_WriteBufferCompute, tests);

		zest_pass_node verify_pass = zest_AddComputePassNode(tests->compute_verify, "Verify Pass");
		zest_ConnectStorageBufferInput(verify_pass, write_buffer);
		zest_ConnectStorageBufferOutput(verify_pass, verify_buffer);
		zest_SetPassTask(verify_pass, zest_VerifyBufferCompute, tests);

		zest_render_graph render_graph = zest_EndRenderGraphAndWait();
		test->result |= render_graph->error_status;
		TestData *test_data = (TestData *)tests->cpu_buffer->data;
		if (test_data->vec.x != 1.f) {
			test->result = 1;
		}
	}
	test->frame_count++;
	return test->result;
}

void InitialiseTests(ZestTests *tests) {

	tests->tests[0] = { "Empty Graph", test__empty_graph, 0, 0, zest_rgs_no_work_to_do};
	tests->tests[1] = { "Single Pass", test__single_pass, 0, 0, zest_rgs_no_work_to_do | zest_rgs_passes_were_culled};
	tests->tests[2] = { "Blank Screen", test__blank_screen, 0, 0, 0 };
	tests->tests[3] = { "Pass Culling", test__pass_culling, 0, 0, zest_rgs_passes_were_culled };
	tests->tests[4] = { "Resource Culling", test__resource_culling, 0, 0, 0 };
	tests->tests[5] = { "Chained Pass Culling", test__chained_pass_culling, 0, 0, zest_rgs_passes_were_culled };
	tests->tests[6] = { "Transient Image", test__transient_image, 0, 0, 0 };
	tests->tests[7] = { "Import Image", test__import_image, 0, 0, 0 };
	tests->tests[8] = { "Image Barriers", test__image_barrier_tests, 0, 0, 0 };
	tests->tests[9] = { "Buffer Read/Write", test__buffer_read_write, 0, 0, 0 };

	VkSamplerCreateInfo sampler_info = zest_CreateSamplerInfo();
	VkSamplerCreateInfo mipped_sampler_info = zest_CreateMippedSamplerInfo(7);
	tests->sampler_info = zest_CreateSamplerInfo();
	tests->mipped_sampler_info = zest_CreateMippedSamplerInfo(7);

	tests->current_test = 9;
}

void ResetTests(ZestTests *tests) {
	tests->texture = NULL;
	tests->compute_verify = NULL;
	tests->compute_write = NULL;
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
			zest_ResetRenderer();
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
	ZEST__UNFLAG(create_info.flags, zest_init_flag_cache_shaders);
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	create_info.log_path = "./";
	create_info.thread_count = 0;

	ZestTests tests = {};

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
