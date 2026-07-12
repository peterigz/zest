#include "zest-tests.h"

//Layer tests. These cover the instance layer system: staging writes, instruction batching,
//automatic buffer growth, the upload path into a frame graph transient buffer, drawing with
//zest_DrawInstanceLayer and frame in flight handling. The instance struct is TestData (one vec4).

//Write a recognisable per-instance pattern so the GPU readback can validate exact placement
void SetLayerTestInstance(TestData *instance, zest_uint index, zest_uint batch) {
	instance->vec.x = (float)index;
	instance->vec.y = (float)(index * 2);
	instance->vec.z = (float)batch;
	instance->vec.w = 1.f;
}

int LayerTestInstanceMatches(TestData *instance, zest_uint index, zest_uint batch) {
	return instance->vec.x == (float)index && instance->vec.y == (float)(index * 2) &&
		instance->vec.z == (float)batch && instance->vec.w == 1.f;
}

//Copies the layer's transient device buffer into the imported readback buffer so the CPU can
//verify the upload landed correctly. Copy exactly the bytes the layer wrote: both buffers may be
//allocated larger than requested due to allocator alignment.
void zest_ReadbackLayerBuffer(const zest_command_list command_list, void *user_data) {
	zest_layer layer = (zest_layer)user_data;
	zest_buffer src = zest_GetPassInputBuffer(command_list, "Layer Data");
	zest_buffer dst = zest_GetPassOutputBuffer(command_list, "Readback");
	zest_size size = zest_GetLayerVertexMemoryInUse(layer);
	if (src && dst && size) {
		zest_cmd_CopyBuffer(command_list, src, dst, size);
	}
}

//Pipeline template that consumes TestData as a per-instance attribute and expands a full screen
//quad in the vertex shader, coloring every pixel with the instance data
zest_pipeline_template create_instance_pipeline_template(ZestTests *tests, const char *name) {
	zest_shader_handle vert_shader = zest_CreateShaderFromFile(tests->device, "examples/SDL2/zest-tests/shaders/instance.vert", "instance_vert.spv", zest_vertex_shader, NULL, true);
	zest_shader_handle frag_shader = zest_CreateShaderFromFile(tests->device, "examples/SDL2/zest-tests/shaders/instance.frag", "instance_frag.spv", zest_fragment_shader, NULL, true);

	zest_pipeline_template pipeline_template = zest_CreatePipelineTemplate(tests->device, name);
	zest_AddVertexInputBindingDescription(pipeline_template, 0, sizeof(TestData), zest_input_rate_instance);
	zest_AddVertexAttribute(pipeline_template, 0, 0, zest_format_r32g32b32a32_sfloat, offsetof(TestData, vec));
	zest_SetPipelineVertShader(pipeline_template, vert_shader);
	zest_SetPipelineFragShader(pipeline_template, frag_shader);
	return pipeline_template;
}

//Runs a frame graph that uploads the layer's staging data to its transient device buffer and
//copies that buffer back into a host visible buffer, then verifies every instance on the CPU.
//Returns 0 on success.
int VerifyLayerUpload(ZestTests *tests, zest_layer layer, zest_uint instance_count, zest_uint batch_count, const zest_uint *batch_sizes) {
	int result = 0;
	zest_size data_size = instance_count * sizeof(TestData);
	zest_buffer_info_t readback_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_to_cpu);
	zest_buffer readback = zest_CreateBuffer(tests->device, data_size, &readback_info);
	memset(zest_BufferData(readback), 0, data_size);

	//The timeline signal makes zest_FlushFrameGraph wait for the GPU to finish, without it the
	//flush returns as soon as the work is submitted and the readback data would race the GPU
	zest_execution_timeline timeline = zest_CreateExecutionTimeline(tests->device);
	if (zest_BeginCommandGraph(tests->context, "Layer Upload", 0)) {
		zest_resource_node layer_resource = zest_AddTransientLayerResource("Layer Data", layer, ZEST_FALSE);
		zest_resource_node readback_resource = zest_ImportBufferResource("Readback", readback, 0);

		zest_BeginTransferPass("Upload Layer");
		zest_ConnectOutput(layer_resource);
		zest_SetPassTask(zest_UploadInstanceLayerData, layer);
		zest_EndPass();

		zest_BeginTransferPass("Readback Layer");
		zest_ConnectInput(layer_resource);
		zest_ConnectOutput(readback_resource);
		zest_SetPassTask(zest_ReadbackLayerBuffer, layer);
		zest_EndPass();

		zest_SignalTimeline(timeline);
		zest_frame_graph frame_graph = zest_EndFrameGraph();
		zest_semaphore_status status = zest_FlushFrameGraph(frame_graph);
		if (status != zest_semaphore_status_success) {
			ZEST_PRINT("\tVerifyLayerUpload: flush status %i", (int)status);
			result = 1;
		}
		if (zest_GetFrameGraphResult(frame_graph) != 0) {
			ZEST_PRINT("\tVerifyLayerUpload: frame graph result %i", (int)zest_GetFrameGraphResult(frame_graph));
		}
		result |= zest_GetFrameGraphResult(frame_graph);

		TestData *data = (TestData *)zest_BufferData(readback);
		zest_uint index = 0;
		for (zest_uint batch = 0; batch != batch_count; ++batch) {
			for (zest_uint i = 0; i != batch_sizes[batch]; ++i) {
				if (!LayerTestInstanceMatches(&data[index], i, batch)) {
					ZEST_PRINT("\tVerifyLayerUpload: mismatch at index %u (batch %u, i %u): got %f %f %f %f", index, batch, i, data[index].vec.x, data[index].vec.y, data[index].vec.z, data[index].vec.w);
					result = 1;
					batch = batch_count - 1;
					break;
				}
				index++;
			}
		}
	} else {
		ZEST_PRINT("\tVerifyLayerUpload: BeginCommandGraph failed");
		result = 1;
	}
	zest_FreeBuffer(readback);
	return result;
}

/*
Instance Layer Upload: Write a known pattern of instances into a layer, upload it into the frame
graph's transient buffer and read the device buffer back to verify every byte arrived intact.
Also checks the CPU side counts and the single recorded instruction.
*/
int test__instance_layer_upload(ZestTests *tests, Test *test) {
	const zest_uint instance_count = 100;
	zest_layer_handle layer_handle = zest_CreateInstanceLayer(tests->context, "Upload Layer", sizeof(TestData), 128);
	zest_layer layer = zest_GetLayer(layer_handle);
	zest_pipeline_template pipeline = zest_CreatePipelineTemplate(tests->device, "Layer Upload Pipeline");

	zest_StartInstanceDrawing(layer, pipeline);
	for (zest_uint i = 0; i != instance_count; ++i) {
		TestData *instance = (TestData *)zest_NextInstance(layer);
		SetLayerTestInstance(instance, i, 0);
	}
	zest_EndInstanceInstructions(layer);

	if (zest_GetInstanceLayerCount(layer) != instance_count) {
		ZEST_PRINT("\tUpload: instance count %u, expected %u", zest_GetInstanceLayerCount(layer), instance_count);
		test->result = 1;
	}
	if (zest_GetLayerInstructionCount(layer) != 1) {
		ZEST_PRINT("\tUpload: instruction count %u, expected 1", zest_GetLayerInstructionCount(layer));
		test->result = 1;
	}
	zest_layer_instruction_t *instruction = zest_GetLayerInstruction(layer, 0);
	if (!instruction || instruction->total_instances != instance_count || instruction->start_index != 0) {
		ZEST_PRINT("\tUpload: bad instruction");
		test->result = 1;
	}

	zest_uint batch_sizes[1] = { instance_count };
	test->result |= VerifyLayerUpload(tests, layer, instance_count, 1, batch_sizes);

	zest_FreeLayer(layer_handle);
	zest_uint validation_errors = zest_GetValidationErrorCount(tests->device);
	if (validation_errors) {
		ZEST_PRINT("\tUpload: %u validation errors", validation_errors);
	}
	test->result |= validation_errors;
	test->frame_count++;
	return test->result;
}

/*
Instruction Batching: Record three draw batches with different pipelines and push constants and
verify each instruction records the correct start index, instance total, pipeline and push
constant data, then verify all three batches land contiguously in the device buffer.
*/
int test__instance_layer_batching(ZestTests *tests, Test *test) {
	const zest_uint batch_sizes[3] = { 10, 20, 5 };
	const zest_uint total_instances = 35;
	zest_layer_handle layer_handle = zest_CreateInstanceLayer(tests->context, "Batching Layer", sizeof(TestData), 64);
	zest_layer layer = zest_GetLayer(layer_handle);
	zest_pipeline_template pipeline_a = zest_CreatePipelineTemplate(tests->device, "Layer Batch Pipeline A");
	zest_pipeline_template pipeline_b = zest_CreatePipelineTemplate(tests->device, "Layer Batch Pipeline B");
	zest_pipeline_template expected_pipelines[3] = { pipeline_a, pipeline_b, pipeline_a };

	TestPushConstants pushes[3] = { { 1, 2, 3, 4, 5 }, { 6, 7, 8, 9, 10 }, { 11, 12, 13, 14, 15 } };

	for (zest_uint batch = 0; batch != 3; ++batch) {
		zest_instruction_id id = zest_StartInstanceDrawing(layer, expected_pipelines[batch]);
		if (id != batch) {
			test->result = 1;
		}
		zest_SetLayerPushConstants(layer, &pushes[batch], sizeof(TestPushConstants));
		for (zest_uint i = 0; i != batch_sizes[batch]; ++i) {
			TestData *instance = (TestData *)zest_NextInstance(layer);
			SetLayerTestInstance(instance, i, batch);
		}
	}
	zest_EndInstanceInstructions(layer);

	if (zest_GetInstanceLayerCount(layer) != total_instances) {
		test->result = 1;
	}
	if (zest_GetLayerInstructionCount(layer) != 3) {
		test->result = 1;
	}
	zest_uint expected_start = 0;
	for (zest_uint batch = 0; batch != 3; ++batch) {
		zest_layer_instruction_t *instruction = zest_GetLayerInstruction(layer, batch);
		if (!instruction) {
			test->result = 1;
			continue;
		}
		if (instruction->start_index != expected_start || instruction->total_instances != batch_sizes[batch]) {
			test->result = 1;
		}
		if (instruction->pipeline_template != expected_pipelines[batch]) {
			test->result = 1;
		}
		if (memcmp(instruction->push_constant, &pushes[batch], sizeof(TestPushConstants)) != 0) {
			test->result = 1;
		}
		expected_start += batch_sizes[batch];
	}

	test->result |= VerifyLayerUpload(tests, layer, total_instances, 3, batch_sizes);

	zest_FreeLayer(layer_handle);
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

/*
Buffer Growth: Create a layer with a small initial capacity and write far more instances than it
can hold, forcing the staging buffer to grow multiple times mid-write. Verify no data is lost
across the grows, both in the staging buffer and after uploading to the device buffer.
*/
int test__instance_layer_grow(ZestTests *tests, Test *test) {
	const zest_uint instance_count = 1000;
	zest_layer_handle layer_handle = zest_CreateInstanceLayer(tests->context, "Grow Layer", sizeof(TestData), 8);
	zest_layer layer = zest_GetLayer(layer_handle);
	zest_pipeline_template pipeline = zest_CreatePipelineTemplate(tests->device, "Layer Grow Pipeline");

	zest_StartInstanceDrawing(layer, pipeline);
	for (zest_uint i = 0; i != instance_count; ++i) {
		TestData *instance = (TestData *)zest_NextInstance(layer);
		SetLayerTestInstance(instance, i, 0);
	}
	zest_EndInstanceInstructions(layer);

	if (zest_GetInstanceLayerCount(layer) != instance_count) {
		test->result = 1;
	}
	zest_buffer staging = zest_GetLayerStagingVertexBuffer(layer);
	if (zest_GetBufferSize(staging) < instance_count * sizeof(TestData)) {
		test->result = 1;
	}
	//The staging buffer is host visible so growing must have preserved everything written so far
	TestData *staging_data = (TestData *)zest_BufferData(staging);
	for (zest_uint i = 0; i != instance_count; ++i) {
		if (!LayerTestInstanceMatches(&staging_data[i], i, 0)) {
			test->result = 1;
			break;
		}
	}

	zest_uint batch_sizes[1] = { instance_count };
	test->result |= VerifyLayerUpload(tests, layer, instance_count, 1, batch_sizes);

	zest_FreeLayer(layer_handle);
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

/*
Instance Draw: Full end to end draw through zest_DrawInstanceLayer. A single instance carrying
the color (0, 1, 1, 1) is uploaded and drawn as a full screen quad over a red clear color, then
the image is verified with the same compute shader the image tests use, which checks for exactly
that color. If the instance data never reached the vertex shader the red clear would fail the check.
*/
int test__instance_layer_draw(ZestTests *tests, Test *test) {
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
	memset(zest_BufferData(tests->cpu_buffer), 0, sizeof(TestResults));

	zest_layer_handle layer_handle = zest_CreateInstanceLayer(tests->context, "Draw Layer", sizeof(TestData), 8);
	zest_layer layer = zest_GetLayer(layer_handle);
	zest_pipeline_template pipeline = create_instance_pipeline_template(tests, "Layer Draw Pipeline");

	zest_StartInstanceDrawing(layer, pipeline);
	TestData *instance = (TestData *)zest_NextInstance(layer);
	//The color image_verify.comp expects
	instance->vec.x = 0.f;
	instance->vec.y = 1.f;
	instance->vec.z = 1.f;
	instance->vec.w = 1.f;
	zest_EndInstanceInstructions(layer);

	zest_image_resource_info_t image_info = { zest_format_r8g8b8a8_unorm };
	zest_execution_timeline timeline = zest_CreateExecutionTimeline(tests->device);
	if (zest_BeginCommandGraph(tests->context, "Layer Draw", 0)) {
		zest_resource_node layer_resource = zest_AddTransientLayerResource("Layer Data", layer, ZEST_FALSE);
		zest_resource_node render_target = zest_AddTransientImageResource("Write Buffer", &image_info);
		zest_resource_node verify_buffer = zest_ImportBufferResource("Verify Buffer", tests->cpu_buffer, 0);
		//Clear to red so the verify pass can only succeed if the layer actually drew over it
		zest_SetResourceClearColor(render_target, 1.f, 0.f, 0.f, 1.f);

		zest_BeginTransferPass("Upload Layer");
		zest_ConnectOutput(layer_resource);
		zest_SetPassTask(zest_UploadInstanceLayerData, layer);
		zest_EndPass();

		zest_BeginRenderPass("Draw Layer");
		zest_ConnectInput(layer_resource);
		zest_ConnectOutput(render_target);
		zest_SetPassTask(zest_DrawInstanceLayer, layer);
		zest_EndPass();

		zest_BeginComputePass("Verify Pass");
		zest_ConnectInput(render_target);
		zest_ConnectOutput(verify_buffer);
		zest_SetPassTask(zest_VerifyImageCompute, tests);
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
		if (test_data->vec.y == 1.f) {
			test->result = 1;
		}
	} else {
		test->result = 1;
	}

	zest_FreeLayer(layer_handle);
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

/*
Frame In Flight: Run the layer through real frames with zest_BeginFrame/zest_EndFrame for two
full frame in flight cycles, drawing to the swapchain each frame. Each frame writes a different
number of instances; if the per frame in flight rotation or the deferred reset of a previous
frame's data is broken then the instance count for the current frame won't match what was written.
*/
int test__instance_layer_fif(ZestTests *tests, Test *test) {
	zest_UpdateDevice(tests->device);
	if (zest_BeginFrame(tests->context)) {
		if (!tests->test_layer.value) {
			tests->test_layer = zest_CreateInstanceLayer(tests->context, "FIF Layer", sizeof(TestData), 64);
			tests->layer_pipeline = create_instance_pipeline_template(tests, "Layer FIF Pipeline");
		}
		zest_layer layer = zest_GetLayer(tests->test_layer);

		zest_uint expected_count = (zest_uint)test->frame_count + 1;
		zest_StartInstanceDrawing(layer, tests->layer_pipeline);
		for (zest_uint i = 0; i != expected_count; ++i) {
			TestData *instance = (TestData *)zest_NextInstance(layer);
			SetLayerTestInstance(instance, i, (zest_uint)test->frame_count);
		}
		zest_EndInstanceInstructions(layer);

		//The layer must have synced to the context's current frame in flight and reset any counts
		//left over from the previous frame that used this frame in flight slot
		if (zest_GetInstanceLayerCount(layer) != expected_count) {
			test->result |= 1;
		}
		if ((zest_uint)zest_GetLayerFrameInFlight(layer) != zest_CurrentFIF(tests->context)) {
			test->result |= 1;
		}
		if (zest_GetLayerInstructionCount(layer) != 1) {
			test->result |= 1;
		}


		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Layer FIF", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node layer_resource = zest_AddTransientLayerResource("Layer Data", layer, ZEST_FALSE);

			zest_BeginTransferPass("Upload Layer");
			zest_ConnectOutput(layer_resource);
			zest_SetPassTask(zest_UploadInstanceLayerData, layer);
			zest_EndPass();

			zest_BeginRenderPass("Draw Layer");
			zest_ConnectInput(layer_resource);
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_DrawInstanceLayer, layer);
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
