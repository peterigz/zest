#include "zest-tests.h"
#include <cstdio>

struct vertex {
	zest_vec4 position;
	zest_vec4 uv;
	zest_color_t color;
	zest_uint padding[3];
};

// Helper function to create a basic pipeline template
zest_pipeline_template create_basic_pipeline_template(ZestTests *tests, const char *name) {
	zest_shader_handle vert_shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/vertex.vert", "vertex_vert.spv", zest_vertex_shader, true);
	zest_shader_handle frag_shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/vertex.frag", "vertex_frag.spv", zest_fragment_shader, true);

	zest_pipeline_template pipeline_test = zest_BeginPipelineTemplate(tests->device, name);
	
	//Set up the vertex attributes that will take in all of the billboard data stored in tfx_3d_instance_t objects
	zest_AddVertexInputBindingDescription(pipeline_test, 0, sizeof(vertex), zest_input_rate_vertex);
	zest_AddVertexAttribute(pipeline_test, 0, 0, zest_format_r32g32b32a32_sfloat, offsetof(vertex, position));	  
	zest_AddVertexAttribute(pipeline_test, 0, 1, zest_format_r32g32b32a32_sfloat, offsetof(vertex, uv));	
	zest_AddVertexAttribute(pipeline_test, 0, 2, zest_format_r8g8b8a8_unorm, offsetof(vertex, color));	
	//Set the shaders to our custom timelinefx shaders
	zest_SetPipelineVertShader(pipeline_test, vert_shader);
	zest_SetPipelineFragShader(pipeline_test, frag_shader);
	zest_SetPipelineBlend(pipeline_test, zest_PreMultiplyBlendState());
	zest_SetPipelineTopology(pipeline_test, zest_topology_triangle_list);
	zest_SetPipelineCullMode(pipeline_test, zest_cull_mode_back);
	
	return pipeline_test;
}

//Pipeline state depths - test multiple depth configurations
int test__pipeline_state_depth(ZestTests *tests, Test *test) {
	// Test 1: Depth test enabled, write enabled
	zest_pipeline_template pipeline1 = create_basic_pipeline_template(tests, "Depth Pipeline - Test Write");
	zest_SetPipelineDepthTest(pipeline1, true, true);
	zest_pipeline p1 = zest_CachePipeline(pipeline1, tests->context);
	
	// Test 2: Depth test enabled, write disabled
	zest_pipeline_template pipeline2 = create_basic_pipeline_template(tests, "Depth Pipeline - Test Only");
	zest_SetPipelineDepthTest(pipeline2, true, false);
	zest_pipeline p2 = zest_CachePipeline(pipeline2, tests->context);
	
	// Test 3: Depth test disabled, write disabled
	zest_pipeline_template pipeline3 = create_basic_pipeline_template(tests, "Depth Pipeline - Disabled");
	zest_SetPipelineDepthTest(pipeline3, false, false);
	zest_pipeline p3 = zest_CachePipeline(pipeline3, tests->context);
	
	// Test 4: Depth test disabled, write enabled (should still work but no testing)
	zest_pipeline_template pipeline4 = create_basic_pipeline_template(tests, "Depth Pipeline - Write Only");
	zest_SetPipelineDepthTest(pipeline4, false, true);
	zest_pipeline p4 = zest_CachePipeline(pipeline4, tests->context);
	
	// All pipelines should be created successfully
	test->result = (p1 == NULL || p2 == NULL || p3 == NULL || p4 == NULL) ? 1 : 0;
	test->frame_count++;
	return test->result;
}

//Pipeline state blending - test all available blend states
int test__pipeline_state_blending(ZestTests *tests, Test *test) {
	// Test all 9 available blend states
	zest_color_blend_attachment_t blend_states[] = {
		zest_BlendStateNone(),
		zest_AdditiveBlendState(),
		zest_AdditiveBlendState2(),
		zest_AlphaOnlyBlendState(),
		zest_AlphaBlendState(),
		zest_PreMultiplyBlendState(),
		zest_PreMultiplyBlendStateForSwap(),
		zest_MaxAlphaBlendState(),
		zest_ImGuiBlendState()
	};
	
	const char* blend_names[] = {
		"Blend State None",
		"Blend State Additive",
		"Blend State Additive2", 
		"Blend State Alpha Only",
		"Blend State Alpha",
		"Blend State PreMultiply",
		"Blend State PreMultiply Swap",
		"Blend State Max Alpha",
		"Blend State ImGui"
	};
	
	zest_pipeline pipelines[9];
	int failed_count = 0;
	
	for (int i = 0; i < 9; i++) {
		zest_pipeline_template pipeline = create_basic_pipeline_template(tests, blend_names[i]);
		zest_SetPipelineBlend(pipeline, blend_states[i]);
		pipelines[i] = zest_CachePipeline(pipeline, tests->context);
		
		if (pipelines[i] == NULL) {
			failed_count++;
		}
	}
	
	test->result = failed_count > 0 ? 1 : 0;
	test->frame_count++;
	return test->result;
}

//Pipeline state culling - test all culling modes with front face settings
int test__pipeline_state_culling(ZestTests *tests, Test *test) {
	zest_cull_mode cull_modes[] = {
		zest_cull_mode_none,
		zest_cull_mode_front,
		zest_cull_mode_back,
		zest_cull_mode_front_and_back
	};
	
	zest_front_face front_faces[] = {
		zest_front_face_counter_clockwise,
		zest_front_face_clockwise
	};
	
	const char* cull_names[] = {
		"Cull None - Counter Clockwise",
		"Cull None - Clockwise",
		"Cull Front - Counter Clockwise",
		"Cull Front - Clockwise",
		"Cull Back - Counter Clockwise",
		"Cull Back - Clockwise",
		"Cull Front and Back - Counter Clockwise",
		"Cull Front and Back - Clockwise",
		"Cull None - Counter Clockwise",
		"Cull None - Clockwise",
		"Cull Front - Counter Clockwise",
		"Cull Front - Clockwise",
		"Cull Back - Counter Clockwise",
		"Cull Back - Clockwise",
		"Cull Front and Back - Counter Clockwise",
		"Cull Front and Back - Clockwise"
	};

	zest_pipeline pipelines[8]; // 4 cull modes × 2 front faces
	int failed_count = 0;
	int pipeline_index = 0;
	
	int name_index = 0;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 2; j++) {
			zest_pipeline_template pipeline = create_basic_pipeline_template(tests, cull_names[name_index++]);
			zest_SetPipelineCullMode(pipeline, cull_modes[i]);
			zest_SetPipelineFrontFace(pipeline, front_faces[j]);
			pipelines[pipeline_index] = zest_CachePipeline(pipeline, tests->context);
			
			if (pipelines[pipeline_index] == NULL) {
				failed_count++;
			}
			pipeline_index++;
		}
	}
	
	test->result = failed_count > 0 ? 1 : 0;
	test->frame_count++;
	return test->result;
}

//Pipeline state topology - test all primitive topology types
int test__pipeline_state_topology(ZestTests *tests, Test *test) {
	// Test topologies that work with vertex+fragment shaders only
	// Skip adjacency and patch topologies as they require geometry/tessellation shaders
	zest_topology topologies[] = {
		zest_topology_point_list,
		zest_topology_line_list,
		zest_topology_line_strip,
		zest_topology_triangle_list,
		zest_topology_triangle_strip,
		zest_topology_triangle_fan
		// Note: adjacency topologies require geometry shaders
		// Note: patch topology requires tessellation shaders
	};
	
	const char* topology_names[] = {
		"Point List",
		"Line List",
		"Line Strip", 
		"Triangle List",
		"Triangle Strip",
		"Triangle Fan"
	};
	
	zest_pipeline pipelines[6];
	int failed_count = 0;
	
	for (int i = 0; i < 6; i++) {
		zest_pipeline_template pipeline = create_basic_pipeline_template(tests, topology_names[i]);
		zest_SetPipelineTopology(pipeline, topologies[i]);
		pipelines[i] = zest_CachePipeline(pipeline, tests->context);
		
		if (pipelines[i] == NULL) {
			failed_count++;
		}
	}
	
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Pipeline state polygon mode - test fill, line, and point modes
int test__pipeline_state_polygon_mode(ZestTests *tests, Test *test) {
	zest_polygon_mode polygon_modes[] = {
		zest_polygon_mode_fill,
		zest_polygon_mode_line,
		zest_polygon_mode_point
	};
	
	const char* polygon_mode_names[] = {
		"Fill Mode",
		"Line Mode",
		"Point Mode"
	};
	
	zest_pipeline pipelines[3];
	int failed_count = 0;
	
	for (int i = 0; i < 3; i++) {
		zest_pipeline_template pipeline = create_basic_pipeline_template(tests, polygon_mode_names[i]);
		zest_SetPipelinePolygonFillMode(pipeline, polygon_modes[i]);
		pipelines[i] = zest_CachePipeline(pipeline, tests->context);
		
		if (pipelines[i] == NULL) {
			failed_count++;
		}
	}
	
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Pipeline state front face - test clockwise vs counter-clockwise winding
int test__pipeline_state_front_face(ZestTests *tests, Test *test) {
	zest_front_face front_faces[] = {
		zest_front_face_counter_clockwise,
		zest_front_face_clockwise
	};
	
	const char* front_face_names[] = {
		"Front Face Counter Clockwise",
		"Front Face Clockwise"
	};
	
	zest_pipeline pipelines[2];
	int failed_count = 0;
	
	for (int i = 0; i < 2; i++) {
		zest_pipeline_template pipeline = create_basic_pipeline_template(tests, front_face_names[i]);
		zest_SetPipelineFrontFace(pipeline, front_faces[i]);
		pipelines[i] = zest_CachePipeline(pipeline, tests->context);
		
		if (pipelines[i] == NULL) {
			failed_count++;
		}
	}
	
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Pipeline state vertex input - test various vertex formats and input rates
int test__pipeline_state_vertex_input(ZestTests *tests, Test *test) {
	// Test different vertex formats for position attribute
	zest_format position_formats[] = {
		zest_format_r32g32b32_sfloat,
		zest_format_r32g32b32a32_sfloat,
		zest_format_r16g16b16_sfloat,
		zest_format_r16g16b16a16_sfloat
	};
	
	const char* format_names[] = {
		"RGB32 Float",
		"RGBA32 Float", 
		"RGB16 Float",
		"RGBA16 Float"
	};
	
	zest_pipeline pipelines[4];
	int failed_count = 0;
	
	for (int i = 0; i < 4; i++) {
		zest_shader_handle vert_shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/vertex.vert", "vertex_vert.spv", zest_vertex_shader, true);
		zest_shader_handle frag_shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/vertex.frag", "vertex_frag.spv", zest_fragment_shader, true);

		zest_pipeline_template pipeline_test = zest_BeginPipelineTemplate(tests->device, format_names[i]);
		
		// Test with different position format
		zest_AddVertexInputBindingDescription(pipeline_test, 0, sizeof(vertex), zest_input_rate_vertex);
		zest_AddVertexAttribute(pipeline_test, 0, 0, position_formats[i], offsetof(vertex, position));	  
		zest_AddVertexAttribute(pipeline_test, 0, 1, zest_format_r32g32b32a32_sfloat, offsetof(vertex, uv));	
		zest_AddVertexAttribute(pipeline_test, 0, 2, zest_format_r8g8b8a8_unorm, offsetof(vertex, color));	
		
		zest_SetPipelineVertShader(pipeline_test, vert_shader);
		zest_SetPipelineFragShader(pipeline_test, frag_shader);
		zest_SetPipelineBlend(pipeline_test, zest_PreMultiplyBlendState());
		zest_SetPipelineTopology(pipeline_test, zest_topology_triangle_list);
		zest_SetPipelineCullMode(pipeline_test, zest_cull_mode_back);
		
		pipelines[i] = zest_CachePipeline(pipeline_test, tests->context);
		
		if (pipelines[i] == NULL) {
			failed_count++;
		}
	}
	
	// Test instance rate input
	zest_pipeline_template instance_pipeline = zest_BeginPipelineTemplate(tests->device, "Instance Rate Test");
	
	zest_shader_handle vert_shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/vertex.vert", "vertex_vert.spv", zest_vertex_shader, true);
	zest_shader_handle frag_shader = zest_CreateShaderFromFile(tests->device, "examples/GLFW/zest-tests/shaders/vertex.frag", "vertex_frag.spv", zest_fragment_shader, true);
	
	zest_AddVertexInputBindingDescription(instance_pipeline, 0, sizeof(vertex), zest_input_rate_instance);
	zest_AddVertexAttribute(instance_pipeline, 0, 0, zest_format_r32g32b32a32_sfloat, offsetof(vertex, position));	  
	zest_AddVertexAttribute(instance_pipeline, 0, 1, zest_format_r32g32b32a32_sfloat, offsetof(vertex, uv));	
	zest_AddVertexAttribute(instance_pipeline, 0, 2, zest_format_r8g8b8a8_unorm, offsetof(vertex, color));	
	
	zest_SetPipelineVertShader(instance_pipeline, vert_shader);
	zest_SetPipelineFragShader(instance_pipeline, frag_shader);
	zest_SetPipelineBlend(instance_pipeline, zest_PreMultiplyBlendState());
	zest_SetPipelineTopology(instance_pipeline, zest_topology_triangle_list);
	zest_SetPipelineCullMode(instance_pipeline, zest_cull_mode_back);
	
	zest_pipeline instance_result = zest_CachePipeline(instance_pipeline, tests->context);
	
	if (instance_result == NULL) {
		failed_count++;
	}
	
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Pipeline state multiblend - test custom blend configurations
int test__pipeline_state_multiblend(ZestTests *tests, Test *test) {
	// Test custom blend configurations - initialize with assignments to avoid C++20 issues
	zest_color_blend_attachment_t custom_blends[4];
	
	// Multiply blend
	custom_blends[0].blend_enable = ZEST_TRUE;
	custom_blends[0].src_color_blend_factor = zest_blend_factor_dst_color;
	custom_blends[0].dst_color_blend_factor = zest_blend_factor_zero;
	custom_blends[0].color_blend_op = zest_blend_op_add;
	custom_blends[0].src_alpha_blend_factor = zest_blend_factor_dst_alpha;
	custom_blends[0].dst_alpha_blend_factor = zest_blend_factor_zero;
	custom_blends[0].alpha_blend_op = zest_blend_op_add;
	custom_blends[0].color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
	
	// Screen blend
	custom_blends[1].blend_enable = ZEST_TRUE;
	custom_blends[1].src_color_blend_factor = zest_blend_factor_one;
	custom_blends[1].dst_color_blend_factor = zest_blend_factor_one_minus_src_color;
	custom_blends[1].color_blend_op = zest_blend_op_add;
	custom_blends[1].src_alpha_blend_factor = zest_blend_factor_one;
	custom_blends[1].dst_alpha_blend_factor = zest_blend_factor_one_minus_src_alpha;
	custom_blends[1].alpha_blend_op = zest_blend_op_add;
	custom_blends[1].color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
	
	// Subtract blend  
	custom_blends[2].blend_enable = ZEST_TRUE;
	custom_blends[2].src_color_blend_factor = zest_blend_factor_one;
	custom_blends[2].dst_color_blend_factor = zest_blend_factor_one;
	custom_blends[2].color_blend_op = zest_blend_op_reverse_subtract;
	custom_blends[2].src_alpha_blend_factor = zest_blend_factor_one;
	custom_blends[2].dst_alpha_blend_factor = zest_blend_factor_one;
	custom_blends[2].alpha_blend_op = zest_blend_op_reverse_subtract;
	custom_blends[2].color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
	
	// Min blend
	custom_blends[3].blend_enable = ZEST_TRUE;
	custom_blends[3].src_color_blend_factor = zest_blend_factor_one;
	custom_blends[3].dst_color_blend_factor = zest_blend_factor_one;
	custom_blends[3].color_blend_op = zest_blend_op_min;
	custom_blends[3].src_alpha_blend_factor = zest_blend_factor_one;
	custom_blends[3].dst_alpha_blend_factor = zest_blend_factor_one;
	custom_blends[3].alpha_blend_op = zest_blend_op_min;
	custom_blends[3].color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
	
	const char* custom_blend_names[] = {
		"Custom Multiply Blend",
		"Custom Screen Blend",
		"Custom Subtract Blend", 
		"Custom Min Blend"
	};
	
	zest_pipeline pipelines[4];
	int failed_count = 0;
	
	for (int i = 0; i < 4; i++) {
		zest_pipeline_template pipeline = create_basic_pipeline_template(tests, custom_blend_names[i]);
		zest_SetPipelineBlend(pipeline, custom_blends[i]);
		pipelines[i] = zest_CachePipeline(pipeline, tests->context);
		
		if (pipelines[i] == NULL) {
			failed_count++;
		}
	}
	
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Pipeline state rasterization - test line width and other rasterization settings
int test__pipeline_state_rasterization(ZestTests *tests, Test *test) {
	// Test different polygon modes combined with different line widths
	zest_polygon_mode polygon_modes[] = {
		zest_polygon_mode_fill,
		zest_polygon_mode_line,
		zest_polygon_mode_point
	};
	
	const char* rasterization_names[] = {
		"Fill Mode Normal",
		"Line Mode Thin",
		"Line Mode Thick", 
		"Point Mode",
		"Fill Mode Cull None",
		"Line Mode Cull Front"
	};
	
	zest_pipeline pipelines[6];
	int failed_count = 0;
	
	// Test fill mode with default settings
	zest_pipeline_template pipeline1 = create_basic_pipeline_template(tests, rasterization_names[0]);
	zest_SetPipelinePolygonFillMode(pipeline1, zest_polygon_mode_fill);
	pipelines[0] = zest_CachePipeline(pipeline1, tests->context);
	
	// Test line mode (note: line width would need to be set via internal struct access)
	zest_pipeline_template pipeline2 = create_basic_pipeline_template(tests, rasterization_names[1]);
	zest_SetPipelinePolygonFillMode(pipeline2, zest_polygon_mode_line);
	pipelines[1] = zest_CachePipeline(pipeline2, tests->context);
	
	// Test point mode
	zest_pipeline_template pipeline3 = create_basic_pipeline_template(tests, rasterization_names[3]);
	zest_SetPipelinePolygonFillMode(pipeline3, zest_polygon_mode_point);
	pipelines[3] = zest_CachePipeline(pipeline3, tests->context);
	
	// Test different culling combinations
	zest_pipeline_template pipeline4 = create_basic_pipeline_template(tests, rasterization_names[4]);
	zest_SetPipelineCullMode(pipeline4, zest_cull_mode_none);
	zest_SetPipelinePolygonFillMode(pipeline4, zest_polygon_mode_fill);
	pipelines[4] = zest_CachePipeline(pipeline4, tests->context);
	
	zest_pipeline_template pipeline5 = create_basic_pipeline_template(tests, rasterization_names[5]);
	zest_SetPipelineCullMode(pipeline5, zest_cull_mode_front);
	zest_SetPipelinePolygonFillMode(pipeline5, zest_polygon_mode_line);
	pipelines[5] = zest_CachePipeline(pipeline5, tests->context);
	
	for (int i = 0; i < 6; i++) {
		if (i != 2) { // Skip index 2 (thick line) as we didn't create it
			if (pipelines[i] == NULL) {
				failed_count++;
			}
		}
	}
	
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}