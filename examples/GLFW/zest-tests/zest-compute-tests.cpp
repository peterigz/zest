#include "zest-tests.h"

void zest_DispatchBRDSetup(const zest_command_list command_list, void *user_data) {
	ZestTests *tests = (ZestTests *)user_data;

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_compute brd_compute = zest_GetCompute(tests->brd_compute);

	// Bind the pipeline once before the loop
	zest_cmd_BindComputePipeline(command_list, brd_compute);

	zest_uint push;

	push = tests->brd_bindless_texture_index;

	zest_cmd_SendPushConstants(command_list, &push, sizeof(zest_uint));

	zest_uint group_count_x = (512 + local_size_x - 1) / local_size_x;
	zest_uint group_count_y = (512 + local_size_y - 1) / local_size_y;

	//Dispatch the compute shader
	zest_cmd_DispatchCompute(command_list, group_count_x, group_count_y, 1);
}

int test__frame_graph_and_execute(ZestTests *tests, Test *test) {
	zest_image_info_t image_info = zest_CreateImageInfo(512, 512);
	image_info.format = zest_format_r16g16_sfloat;
	image_info.flags = zest_image_preset_storage;
	tests->brd_texture = zest_CreateImage(tests->device, &image_info);
	zest_image brd_image = zest_GetImage(tests->brd_texture);

	tests->brd_bindless_texture_index = zest_AcquireStorageImageIndex(tests->device, brd_image, zest_storage_image_binding);
	zest_AcquireSampledImageIndex(tests->device, brd_image, zest_texture_2d_binding);

	zest_execution_timeline timeline = zest_CreateExecutionTimeline(tests->device);

	tests->brd_shader = zest_CreateShaderFromFile(tests->device, "examples/Common/shaders/genbrdflut.comp", "genbrdflut_comp.spv", zest_compute_shader, true);
	tests->brd_compute = zest_CreateCompute(tests->device, "Brd Compute", tests->brd_shader);

	zest_BeginFrameGraph(tests->context, "BRDFLUT", 0);
	zest_resource_node texture_resource = zest_ImportImageResource("Brd texture", brd_image, 0);

	zest_compute compute = zest_GetCompute(tests->brd_compute);
	zest_BeginComputePass(compute, "Brd compute");
	zest_ConnectOutput(texture_resource);
	zest_SetPassTask(zest_DispatchBRDSetup, tests);
	zest_EndPass();

	zest_SignalTimeline(timeline);
	zest_EndFrameGraphAndExecute();
	zest_semaphore_status status = zest_WaitForSignal(timeline, ZEST_SECONDS_IN_MICROSECONDS(1));
	zest_FreeExecutionTimeline(timeline);
	
	zest_FreeImageNow(tests->brd_texture);

	test->result |= (status != zest_semaphore_status_success);
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;

	return test->result;
}

/*
Test timeline wait external: Two separate frame graphs where second waits on first via timeline.
Graph A: compute work, signals timeline
Graph B: waits on timeline, then does more compute
Verifies cross-graph synchronization.
*/
int test__timeline_wait_external(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->compute_write)) {
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/buffer_write.comp", "buffer_write.spv", zest_compute_shader, 1);
		tests->compute_write = zest_CreateCompute(tests->device, "Buffer Write", shader);
		if (!zest_IsValidHandle((void*)&tests->compute_write)) {
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

	zest_buffer_resource_info_t info = {};
	info.size = sizeof(TestData) * 1000;

	// Create timeline for cross-graph synchronization
	zest_execution_timeline timeline = zest_CreateExecutionTimeline(tests->device);

	zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	zest_buffer storage_buffer = zest_CreateBuffer(tests->device, 1024, &storage_buffer_info);

	// Graph A: Write to buffer and signal timeline
	if (zest_BeginFrameGraph(tests->context, "Graph A - Write", 0)) {
		zest_resource_node write_buffer = zest_ImportBufferResource("Write Buffer", storage_buffer, 0);

		zest_compute compute_write = zest_GetCompute(tests->compute_write);

		zest_BeginComputePass(compute_write, "Write Pass");
		zest_ConnectOutput(write_buffer);
		zest_SetPassTask(zest_WriteBufferCompute, tests);
		zest_EndPass();

		zest_SignalTimeline(timeline);
		zest_frame_graph frame_graph_a = zest_EndFrameGraphAndExecute();
		test->result |= zest_GetFrameGraphResult(frame_graph_a);
	}

	// Graph B: Wait on timeline then do more work (just verifies synchronization)
	if (zest_BeginFrameGraph(tests->context, "Graph B - After Wait", 0)) {
		zest_resource_node write_buffer = zest_ImportBufferResource("Write Buffer", storage_buffer, 0);

		zest_compute compute_write = zest_GetCompute(tests->compute_write);

		zest_WaitOnTimeline(timeline);

		zest_BeginComputePass(compute_write, "Second Write Pass");
		zest_ConnectOutput(write_buffer);
		zest_SetPassTask(zest_WriteBufferCompute, tests);
		zest_EndPass();

		zest_SignalTimeline(timeline);
		zest_frame_graph frame_graph_b = zest_EndFrameGraphAndExecute();
		test->result |= zest_GetFrameGraphResult(frame_graph_b);
	}

	// Wait for everything to complete
	zest_semaphore_status status = zest_WaitForSignal(timeline, ZEST_SECONDS_IN_MICROSECONDS(1));
	if (status != zest_semaphore_status_success) {
		test->result = 1;
	}

	zest_FreeBuffer(storage_buffer);

	zest_FreeExecutionTimeline(timeline);
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

/*
Test timeline timeout: Test zest_WaitForSignal timeout behavior.
Create timeline, don't signal it, wait with short timeout.
Verify returns zest_semaphore_status_timeout.
*/
int test__timeline_timeout(ZestTests *tests, Test *test) {
	zest_execution_timeline timeline = zest_CreateExecutionTimeline(tests->device);

	// Increment the timeline value so we're waiting for a signal that won't come
	timeline->current_value++;

	// Wait with a short timeout (100ms) - should timeout since we never signal
	zest_semaphore_status status = zest_WaitForSignal(timeline, 100000); // 100ms in microseconds

	// We expect a timeout
	if (status != zest_semaphore_status_timeout) {
		test->result = 1;
	}

	zest_FreeExecutionTimeline(timeline);
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

/*
Test immediate execute with caching: Execute same immediate graph multiple times.
Verify cache is used and each execution completes properly.
*/
int test__immediate_execute_cached(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->brd_shader)) {
		tests->brd_shader = zest_CreateShaderFromFile(tests->device, "examples/Common/shaders/genbrdflut.comp", "genbrdflut_comp.spv", zest_compute_shader, true);
	}
	if (!zest_IsValidHandle((void*)&tests->brd_compute)) {
		tests->brd_compute = zest_CreateCompute(tests->device, "Brd Compute", tests->brd_shader);
	}

	zest_image_info_t image_info = zest_CreateImageInfo(512, 512);
	image_info.format = zest_format_r16g16_sfloat;
	image_info.flags = zest_image_preset_storage;
	tests->brd_texture = zest_CreateImage(tests->device, &image_info);
	zest_image brd_image = zest_GetImage(tests->brd_texture);
	tests->brd_bindless_texture_index = zest_AcquireStorageImageIndex(tests->device, brd_image, zest_storage_image_binding);

	zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(tests->context, 0, 0);

	// Execute the same graph 3 times
	for (int i = 0; i < 3; i++) {
		zest_execution_timeline timeline = zest_CreateExecutionTimeline(tests->device);

		zest_frame_graph cached_graph = zest_GetCachedFrameGraph(tests->context, &cache_key);
		if (cached_graph) {
			// Cache hit - just count it, but still need to rebuild for immediate execute
			test->cache_count++;
		}

		// For immediate execute, we always build but verify cache is populated
		if (zest_BeginFrameGraph(tests->context, "BRDFLUT Cached", &cache_key)) {
			zest_resource_node texture_resource = zest_ImportImageResource("Brd texture", brd_image, 0);

			zest_compute compute = zest_GetCompute(tests->brd_compute);
			zest_BeginComputePass(compute, "Brd compute");
			zest_ConnectOutput(texture_resource);
			zest_SetPassTask(zest_DispatchBRDSetup, tests);
			zest_EndPass();

			zest_SignalTimeline(timeline);
			zest_frame_graph frame_graph = zest_EndFrameGraphAndExecute();
			test->result |= zest_GetFrameGraphResult(frame_graph);
		}

		zest_semaphore_status status = zest_WaitForSignal(timeline, ZEST_SECONDS_IN_MICROSECONDS(1));
		if (status != zest_semaphore_status_success) {
			test->result = 1;
		}
		zest_FreeExecutionTimeline(timeline);
	}

	// Should have used cache at least twice (after first execution)
	if (test->cache_count < 2) {
		test->result = 1;
	}

	zest_FreeImageNow(tests->brd_texture);

	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

void zest_DispatchMipmapCompute(const zest_command_list command_list, void *user_data) {
	ZestTests *tests = (ZestTests *)user_data;
	zest_resource_node output_image = zest_GetPassOutputResource(command_list, "Mipmap Image");
	ZEST_ASSERT_HANDLE(output_image);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_compute compute = zest_GetCompute(tests->brd_compute);
	zest_cmd_BindComputePipeline(command_list, compute);

	zest_image_info_t image_desc = zest_GetResourceImageDescription(output_image);

	// Process each mip level
	zest_uint width = image_desc.extent.width;
	zest_uint height = image_desc.extent.height;

	for (zest_uint mip = 0; mip < image_desc.mip_levels; mip++) {
		zest_uint push = zest_GetTransientSampledImageBindlessIndex(command_list, output_image, zest_storage_image_binding);
		zest_cmd_SendPushConstants(command_list, &push, sizeof(zest_uint));

		zest_uint group_count_x = (width + local_size_x - 1) / local_size_x;
		zest_uint group_count_y = (height + local_size_y - 1) / local_size_y;

		zest_cmd_DispatchCompute(command_list, group_count_x, group_count_y, 1);

		// Insert barrier between mip levels (except after last)
		if (mip < image_desc.mip_levels - 1) {
			zest_cmd_InsertComputeImageBarrier(command_list, output_image, mip);
		}

		width = width > 1 ? width / 2 : 1;
		height = height > 1 ? height / 2 : 1;
	}
}

/*
Test compute mipmap chain: Test compute generating mipmap levels with barriers.
Create storage image with mipmaps, dispatch compute for each mip level,
use zest_cmd_InsertComputeImageBarrier between levels.
*/
int test__compute_mipmap_chain(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->brd_shader)) {
		tests->brd_shader = zest_CreateShaderFromFile(tests->device, "examples/Common/shaders/genbrdflut.comp", "genbrdflut_comp.spv", zest_compute_shader, true);
	}
	if (!zest_IsValidHandle((void*)&tests->brd_compute)) {
		tests->brd_compute = zest_CreateCompute(tests->device, "Brd Compute", tests->brd_shader);
	}

	zest_execution_timeline timeline = zest_CreateExecutionTimeline(tests->device);

	// Create image resource info with mipmaps
	zest_image_resource_info_t image_info = { zest_format_r16g16_sfloat };
	image_info.width = 256;
	image_info.height = 256;
	image_info.mip_levels = 5; // 256 -> 128 -> 64 -> 32 -> 16

	if (zest_BeginFrameGraph(tests->context, "Mipmap Chain", 0)) {
		zest_resource_node mipmap_image = zest_AddTransientImageResource("Mipmap Image", &image_info);
		zest_FlagResourceAsEssential(mipmap_image);

		zest_compute compute = zest_GetCompute(tests->brd_compute);
		zest_BeginComputePass(compute, "Mipmap Generation");
		zest_ConnectOutput(mipmap_image);
		zest_SetPassTask(zest_DispatchMipmapCompute, tests);
		zest_EndPass();

		zest_SignalTimeline(timeline);
		zest_frame_graph frame_graph = zest_EndFrameGraphAndExecute();
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}

	zest_PrintReports(tests->context);

	zest_semaphore_status status = zest_WaitForSignal(timeline, ZEST_SECONDS_IN_MICROSECONDS(1));
	if (status != zest_semaphore_status_success) {
		test->result = 1;
	}

	zest_FreeExecutionTimeline(timeline);
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

/*
Test multiple timeline signals: Signal multiple timelines from one graph.
Create two timelines, single graph signals both, verify both can be waited on.
*/
int test__multiple_timeline_signals(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->compute_write)) {
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/buffer_write.comp", "buffer_write.spv", zest_compute_shader, 1);
		tests->compute_write = zest_CreateCompute(tests->device, "Buffer Write", shader);
		if (!zest_IsValidHandle((void*)&tests->compute_write)) {
			test->frame_count++;
			test->result = 1;
			return test->result;
		}
	}

	zest_buffer_resource_info_t info = {};
	info.size = sizeof(TestData) * 1000;

	// Create two timelines
	zest_execution_timeline timeline_1 = zest_CreateExecutionTimeline(tests->device);

	zest_execution_timeline timeline_2 = zest_CreateExecutionTimeline(tests->device);

	// Single graph that signals both timelines
	if (zest_BeginFrameGraph(tests->context, "Multi Signal", 0)) {
		zest_resource_node write_buffer = zest_AddTransientBufferResource("Write Buffer", &info);
		zest_FlagResourceAsEssential(write_buffer);

		zest_compute compute_write = zest_GetCompute(tests->compute_write);

		zest_BeginComputePass(compute_write, "Write Pass");
		zest_ConnectOutput(write_buffer);
		zest_SetPassTask(zest_WriteBufferCompute, tests);
		zest_EndPass();

		// Signal both timelines
		zest_SignalTimeline(timeline_1);
		zest_SignalTimeline(timeline_2);

		zest_frame_graph frame_graph = zest_EndFrameGraphAndExecute();
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}

	// Wait on both timelines
	zest_semaphore_status status_1 = zest_WaitForSignal(timeline_1, ZEST_SECONDS_IN_MICROSECONDS(1));
	zest_semaphore_status status_2 = zest_WaitForSignal(timeline_2, ZEST_SECONDS_IN_MICROSECONDS(1));

	if (status_1 != zest_semaphore_status_success || status_2 != zest_semaphore_status_success) {
		test->result = 1;
	}

	zest_FreeExecutionTimeline(timeline_1);
	zest_FreeExecutionTimeline(timeline_2);
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

void zest_ReadModifyWriteCompute(const zest_command_list command_list, void *user_data) {
	ZestTests *tests = (ZestTests *)user_data;
	zest_resource_node buffer = zest_GetPassInputResource(command_list, "Write Buffer");
	ZEST_ASSERT_HANDLE(buffer);

	const zest_uint local_size_x = 8;

	zest_compute compute = zest_GetCompute(tests->compute_write);
	zest_cmd_BindComputePipeline(command_list, compute);

	TestPushConstants push;
	push.index1 = zest_GetTransientBufferBindlessIndex(command_list, buffer);

	zest_cmd_SendPushConstants(command_list, &push, sizeof(TestPushConstants));

	zest_uint group_count_x = (1000 + local_size_x - 1) / local_size_x;
	zest_cmd_DispatchCompute(command_list, group_count_x, 1, 1);
}

/*
Test compute read-modify-write: Ping-pong pattern - same buffer as input AND output.
Single compute pass with zest_ConnectInput and zest_ConnectOutput on same resource.
Common pattern for particle simulation.
*/
int test__compute_read_modify_write(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->compute_write)) {
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/buffer_write.comp", "buffer_write.spv", zest_compute_shader, 1);
		tests->compute_write = zest_CreateCompute(tests->device, "Buffer Write", shader);
		if (!zest_IsValidHandle((void*)&tests->compute_write)) {
			test->frame_count++;
			test->result = 1;
			return test->result;
		}
	}

	zest_buffer_resource_info_t info = {};
	info.size = sizeof(TestData) * 1000;

	zest_execution_timeline timeline = zest_CreateExecutionTimeline(tests->device);

	if (zest_BeginFrameGraph(tests->context, "Read Modify Write", 0)) {
		zest_resource_node rmw_buffer = zest_AddTransientBufferResource("Write Buffer", &info);
		zest_FlagResourceAsEssential(rmw_buffer);

		zest_compute compute_write = zest_GetCompute(tests->compute_write);

		// First pass: Initialize buffer
		zest_BeginComputePass(compute_write, "Init Pass");
		zest_ConnectOutput(rmw_buffer);
		zest_SetPassTask(zest_WriteBufferCompute, tests);
		zest_EndPass();

		// Second pass: Read and modify same buffer (read-modify-write pattern)
		zest_BeginComputePass(compute_write, "RMW Pass");
		zest_ConnectInput(rmw_buffer);
		zest_ConnectOutput(rmw_buffer);
		zest_SetPassTask(zest_ReadModifyWriteCompute, tests);
		zest_EndPass();

		zest_SignalTimeline(timeline);
		zest_frame_graph frame_graph = zest_EndFrameGraphAndExecute();
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}

	zest_semaphore_status status = zest_WaitForSignal(timeline, ZEST_SECONDS_IN_MICROSECONDS(1));
	if (status != zest_semaphore_status_success) {
		test->result = 1;
	}

	zest_FreeExecutionTimeline(timeline);
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

/*
Test compute-only graph: Graph with only compute passes (no render/swapchain).
Multiple compute passes, no graphics queue involvement.
Verify compiles and executes correctly.
*/
int test__compute_only_graph(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->compute_write)) {
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/buffer_write.comp", "buffer_write.spv", zest_compute_shader, 1);
		tests->compute_write = zest_CreateCompute(tests->device, "Buffer Write", shader);
		if (!zest_IsValidHandle((void*)&tests->compute_write)) {
			test->frame_count++;
			test->result = 1;
			return test->result;
		}
	}
	if (!zest_IsValidHandle((void*)&tests->compute_verify)) {
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/buffer_verify.comp", "buffer_verify.spv", zest_compute_shader, 1);
		tests->compute_verify = zest_CreateCompute(tests->device, "Buffer Verify", shader);
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

	zest_buffer_resource_info_t info = {};
	info.size = sizeof(TestData) * 1000;

	zest_execution_timeline timeline = zest_CreateExecutionTimeline(tests->device);

	// Graph with ONLY compute passes - no render passes, no swapchain
	if (zest_BeginFrameGraph(tests->context, "Compute Only", 0)) {
		zest_resource_node buffer_a = zest_AddTransientBufferResource("Write Buffer", &info);
		zest_resource_node buffer_b = zest_AddTransientBufferResource("Buffer B", &info);
		zest_resource_node verify_buffer = zest_ImportBufferResource("Verify Buffer", tests->cpu_buffer, 0);

		zest_compute compute_write = zest_GetCompute(tests->compute_write);
		zest_compute compute_verify = zest_GetCompute(tests->compute_verify);

		// Pass 1: Write to buffer A
		zest_BeginComputePass(compute_write, "Write A");
		zest_ConnectOutput(buffer_a);
		zest_SetPassTask(zest_WriteBufferCompute, tests);
		zest_EndPass();

		// Pass 2: Write to buffer B (This should get culled)
		zest_BeginComputePass(compute_write, "Write B");
		zest_ConnectOutput(buffer_b);
		zest_SetPassTask(zest_WriteBufferCompute, tests);
		zest_EndPass();

		// Pass 3: Verify buffer A
		zest_BeginComputePass(compute_verify, "Verify Pass");
		zest_ConnectInput(buffer_a);
		zest_ConnectOutput(verify_buffer);
		zest_SetPassTask(zest_VerifyBufferCompute, tests);
		zest_EndPass();

		zest_SignalTimeline(timeline);
		zest_frame_graph frame_graph = zest_EndFrameGraphAndExecute();
		test->result |= zest_GetFrameGraphResult(frame_graph);

		// Verify we had multiple passes
		if (zest_GetFrameGraphFinalPassCount(frame_graph) != 2) {
			test->result = 1;
		}
	}

	zest_semaphore_status status = zest_WaitForSignal(timeline, ZEST_SECONDS_IN_MICROSECONDS(1));
	if (status != zest_semaphore_status_success) {
		test->result = 1;
	}

	// Verify the compute results
	TestData *test_data = (TestData *)zest_BufferData(tests->cpu_buffer);
	if (test_data->vec.x != 1.f) {
		test->result = 1;
	}

	zest_FreeExecutionTimeline(timeline);
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

/*
Test immediate execute no wait: Fire-and-forget pattern.
Execute immediate graph but don't wait, execute another graph.
Verify no crashes/conflicts.
*/
int test__immediate_execute_no_wait(ZestTests *tests, Test *test) {
	if (!zest_IsValidHandle((void*)&tests->compute_write)) {
		zest_shader_handle shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/buffer_write.comp", "buffer_write.spv", zest_compute_shader, 1);
		tests->compute_write = zest_CreateCompute(tests->device, "Buffer Write", shader);
		if (!zest_IsValidHandle((void*)&tests->compute_write)) {
			test->frame_count++;
			test->result = 1;
			return test->result;
		}
	}

	zest_buffer_resource_info_t info = {};
	info.size = sizeof(TestData) * 1000;

	// Create timeline for final wait only
	zest_execution_timeline final_timeline = zest_CreateExecutionTimeline(tests->device);

	// Execute first graph without waiting
	if (zest_BeginFrameGraph(tests->context, "Fire and Forget 1", 0)) {
		zest_resource_node buffer = zest_AddTransientBufferResource("Write Buffer", &info);
		zest_FlagResourceAsEssential(buffer);

		zest_compute compute_write = zest_GetCompute(tests->compute_write);

		zest_BeginComputePass(compute_write, "Write Pass 1");
		zest_ConnectOutput(buffer);
		zest_SetPassTask(zest_WriteBufferCompute, tests);
		zest_EndPass();

		// Don't wait - just execute
		zest_frame_graph frame_graph = zest_EndFrameGraphAndExecute();
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}

	// Immediately execute second graph
	if (zest_BeginFrameGraph(tests->context, "Fire and Forget 2", 0)) {
		zest_resource_node buffer = zest_AddTransientBufferResource("Write Buffer", &info);
		zest_FlagResourceAsEssential(buffer);

		zest_compute compute_write = zest_GetCompute(tests->compute_write);

		zest_BeginComputePass(compute_write, "Write Pass 2");
		zest_ConnectOutput(buffer);
		zest_SetPassTask(zest_WriteBufferCompute, tests);
		zest_EndPass();

		// Don't wait here either
		zest_frame_graph frame_graph = zest_EndFrameGraphAndExecute();
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}

	// Execute third graph and wait for completion
	if (zest_BeginFrameGraph(tests->context, "Final Graph", 0)) {
		zest_resource_node buffer = zest_AddTransientBufferResource("Write Buffer", &info);
		zest_FlagResourceAsEssential(buffer);

		zest_compute compute_write = zest_GetCompute(tests->compute_write);

		zest_BeginComputePass(compute_write, "Write Pass 3");
		zest_ConnectOutput(buffer);
		zest_SetPassTask(zest_WriteBufferCompute, tests);
		zest_EndPass();

		zest_SignalTimeline(final_timeline);
		zest_frame_graph frame_graph = zest_EndFrameGraphAndExecute();
		test->result |= zest_GetFrameGraphResult(frame_graph);
	}

	// Wait for final graph to complete. Note: this does NOT guarantee the first two graphs
	// are complete since they don't signal any timeline. This test verifies that multiple
	// immediate executions can be submitted without explicit waits and don't crash.
	zest_semaphore_status status = zest_WaitForSignal(final_timeline, ZEST_SECONDS_IN_MICROSECONDS(2));
	if (status != zest_semaphore_status_success) {
		test->result = 1;
	}

	zest_FreeExecutionTimeline(final_timeline);
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}