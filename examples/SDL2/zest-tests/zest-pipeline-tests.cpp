#include "zest-tests.h"
#include <cstdio>

struct vertex {
	zest_vec4 position;
	zest_vec4 uv;
	zest_color_t color;
	zest_uint padding[3];
};

// Helper to create a command list with a default rendering info for pipeline tests
zest_command_list_t create_test_command_list(ZestTests *tests) {
	zest_command_list_t command_list = ZEST__ZERO_INIT(zest_command_list_t);
	command_list.context = tests->context;
	command_list.rendering_info.color_attachment_count = 1;
	command_list.rendering_info.color_attachment_formats[0] = zest_format_b8g8r8a8_unorm;
	command_list.device = tests->device;
	zest_CreateTestRenderPass(tests->device, &command_list);
	return command_list;
}

// Helper function to create a basic pipeline template
zest_pipeline_template create_basic_pipeline_template(ZestTests *tests, const char *name) {
	char shader_path[256];
	zest_shader_handle vert_shader = zest_CreateShaderFromFile(tests->device, TestShaderPath(tests->device, "examples/SDL2/zest-tests/shaders/glsl/vertex.vert", shader_path, sizeof(shader_path)), "vertex_vert.spv", zest_vertex_shader, NULL, true);
	zest_shader_handle frag_shader = zest_CreateShaderFromFile(tests->device, TestShaderPath(tests->device, "examples/SDL2/zest-tests/shaders/glsl/vertex.frag", shader_path, sizeof(shader_path)), "vertex_frag.spv", zest_fragment_shader, NULL, true);

	zest_pipeline_template pipeline_test = zest_CreatePipelineTemplate(tests->device, name);
	
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
	zest_command_list_t command_list = create_test_command_list(tests);
	// Test 1: Depth test enabled, write enabled
	zest_pipeline_template pipeline1 = create_basic_pipeline_template(tests, "Depth Pipeline - Test Write");
	zest_SetPipelineDepthTest(pipeline1, true, true);
	zest_pipeline p1 = zest_GetPipeline(pipeline1, &command_list);
	
	// Test 2: Depth test enabled, write disabled
	zest_pipeline_template pipeline2 = create_basic_pipeline_template(tests, "Depth Pipeline - Test Only");
	zest_SetPipelineDepthTest(pipeline2, true, false);
	zest_pipeline p2 = zest_GetPipeline(pipeline2, &command_list);
	
	// Test 3: Depth test disabled, write disabled
	zest_pipeline_template pipeline3 = create_basic_pipeline_template(tests, "Depth Pipeline - Disabled");
	zest_SetPipelineDepthTest(pipeline3, false, false);
	zest_pipeline p3 = zest_GetPipeline(pipeline3, &command_list);
	
	// Test 4: Depth test disabled, write enabled (should still work but no testing)
	zest_pipeline_template pipeline4 = create_basic_pipeline_template(tests, "Depth Pipeline - Write Only");
	zest_SetPipelineDepthTest(pipeline4, false, true);
	zest_pipeline p4 = zest_GetPipeline(pipeline4, &command_list);
	
	// All pipelines should be created successfully
	test->result = (p1 == NULL || p2 == NULL || p3 == NULL || p4 == NULL) ? 1 : 0;
	test->frame_count++;
	return test->result;
}

//Pipeline state blending - test all available blend states
int test__pipeline_state_blending(ZestTests *tests, Test *test) {
	// Test all 10 available blend states
	zest_color_blend_attachment_t blend_states[] = {
		zest_BlendStateNone(),
		zest_BlendStateOpaque(),
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
		"Blend State Opaque",
		"Blend State Additive",
		"Blend State Additive2", 
		"Blend State Alpha Only",
		"Blend State Alpha",
		"Blend State PreMultiply",
		"Blend State PreMultiply Swap",
		"Blend State Max Alpha",
		"Blend State ImGui"
	};
	
	zest_pipeline pipelines[10];
	int failed_count = 0;

	zest_command_list_t command_list = create_test_command_list(tests);
	
	for (int i = 0; i < 10; i++) {
		zest_pipeline_template pipeline = create_basic_pipeline_template(tests, blend_names[i]);
		zest_SetPipelineBlend(pipeline, blend_states[i]);
		pipelines[i] = zest_GetPipeline(pipeline, &command_list);
		
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
	zest_command_list_t command_list = create_test_command_list(tests);
	
	int name_index = 0;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 2; j++) {
			//Neither D3D12 nor Metal has a cull mode that culls both faces
			if ((tests->platform == zest_platform_d3d12 || tests->platform == zest_platform_metal) && cull_modes[i] == zest_cull_mode_front_and_back) {
				name_index++;
				continue;
			}
			zest_pipeline_template pipeline = create_basic_pipeline_template(tests, cull_names[name_index++]);
			zest_SetPipelineCullMode(pipeline, cull_modes[i]);
			zest_SetPipelineFrontFace(pipeline, front_faces[j]);
			pipelines[pipeline_index] = zest_GetPipeline(pipeline, &command_list);
			
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
		zest_topology_triangle_list,
#if !defined(__APPLE__)
		zest_topology_line_strip,
		zest_topology_triangle_fan,
		zest_topology_triangle_strip,
#endif
		// Note: adjacency topologies require geometry shaders
		// Note: patch topology requires tessellation shaders
	};

	const char* topology_names[] = {
		"Point List",
		"Line List",
		"Triangle List",
#if !defined(__APPLE__)
		"Line Strip",
		"Triangle Fan",
		"Triangle Strip",
#endif
	};

#if defined(__APPLE__)
	const int topology_count = 3; // Strips and Fans not available on MoltenVK
#else
	const int topology_count = 6;
#endif

	zest_pipeline pipelines[6];
	int failed_count = 0;
	zest_command_list_t command_list = create_test_command_list(tests);

	for (int i = 0; i < topology_count; i++) {
#ifdef ZEST_ENABLE_D3D12
		if (tests->platform == zest_platform_d3d12 && topologies[i] == zest_topology_triangle_fan && !ZEST_DX_BACKEND(device, tests->device)->support.triangle_fan) {
			ZEST_PRINT("\tTriangle fans were skipped, as this D3D12 driver doesn't support them");
			continue;
		}
#endif
		zest_pipeline_template pipeline = create_basic_pipeline_template(tests, topology_names[i]);
		zest_SetPipelineTopology(pipeline, topologies[i]);
		pipelines[i] = zest_GetPipeline(pipeline, &command_list);

		if (pipelines[i] == NULL) {
			failed_count++;
		}
	}
	
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->device);
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
	zest_command_list_t command_list = create_test_command_list(tests);
	
	for (int i = 0; i < 3; i++) {
		//D3D12 has no point fill mode
		if (tests->platform == zest_platform_d3d12 && polygon_modes[i] == zest_polygon_mode_point) continue;
#if defined(__APPLE__)
		//MoltenVK's portability subset has no point fill mode
		if (polygon_modes[i] == zest_polygon_mode_point) continue;
#endif
		zest_pipeline_template pipeline = create_basic_pipeline_template(tests, polygon_mode_names[i]);
		zest_SetPipelinePolygonFillMode(pipeline, polygon_modes[i]);
		pipelines[i] = zest_GetPipeline(pipeline, &command_list);
		
		if (pipelines[i] == NULL) {
			failed_count++;
		}
	}
	
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->device);
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
	zest_command_list_t command_list = create_test_command_list(tests);
	
	for (int i = 0; i < 2; i++) {
		zest_pipeline_template pipeline = create_basic_pipeline_template(tests, front_face_names[i]);
		zest_SetPipelineFrontFace(pipeline, front_faces[i]);
		pipelines[i] = zest_GetPipeline(pipeline, &command_list);
		
		if (pipelines[i] == NULL) {
			failed_count++;
		}
	}
	
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->device);
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
	zest_command_list_t command_list = create_test_command_list(tests);
	
	for (int i = 0; i < 4; i++) {
		//Three channel 16 bit formats have no DXGI equivalent
		if (tests->platform == zest_platform_d3d12 && position_formats[i] == zest_format_r16g16b16_sfloat) continue;
		char shader_path[256];
		zest_shader_handle vert_shader = zest_CreateShaderFromFile(tests->device, TestShaderPath(tests->device, "examples/SDL2/zest-tests/shaders/glsl/vertex.vert", shader_path, sizeof(shader_path)), "vertex_vert.spv", zest_vertex_shader, NULL, true);
		zest_shader_handle frag_shader = zest_CreateShaderFromFile(tests->device, TestShaderPath(tests->device, "examples/SDL2/zest-tests/shaders/glsl/vertex.frag", shader_path, sizeof(shader_path)), "vertex_frag.spv", zest_fragment_shader, NULL, true);

		zest_pipeline_template pipeline_test = zest_CreatePipelineTemplate(tests->device, format_names[i]);
		
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
		
		pipelines[i] = zest_GetPipeline(pipeline_test, &command_list);
		
		if (pipelines[i] == NULL) {
			failed_count++;
		}
	}
	
	// Test instance rate input
	zest_pipeline_template instance_pipeline = zest_CreatePipelineTemplate(tests->device, "Instance Rate Test");
	
	char shader_path[256];
	zest_shader_handle vert_shader = zest_CreateShaderFromFile(tests->device, TestShaderPath(tests->device, "examples/SDL2/zest-tests/shaders/glsl/vertex.vert", shader_path, sizeof(shader_path)), "vertex_vert.spv", zest_vertex_shader, NULL, true);
	zest_shader_handle frag_shader = zest_CreateShaderFromFile(tests->device, TestShaderPath(tests->device, "examples/SDL2/zest-tests/shaders/glsl/vertex.frag", shader_path, sizeof(shader_path)), "vertex_frag.spv", zest_fragment_shader, NULL, true);
	
	zest_AddVertexInputBindingDescription(instance_pipeline, 0, sizeof(vertex), zest_input_rate_instance);
	zest_AddVertexAttribute(instance_pipeline, 0, 0, zest_format_r32g32b32a32_sfloat, offsetof(vertex, position));	  
	zest_AddVertexAttribute(instance_pipeline, 0, 1, zest_format_r32g32b32a32_sfloat, offsetof(vertex, uv));	
	zest_AddVertexAttribute(instance_pipeline, 0, 2, zest_format_r8g8b8a8_unorm, offsetof(vertex, color));	
	
	zest_SetPipelineVertShader(instance_pipeline, vert_shader);
	zest_SetPipelineFragShader(instance_pipeline, frag_shader);
	zest_SetPipelineBlend(instance_pipeline, zest_PreMultiplyBlendState());
	zest_SetPipelineTopology(instance_pipeline, zest_topology_triangle_list);
	zest_SetPipelineCullMode(instance_pipeline, zest_cull_mode_back);
	
	zest_pipeline instance_result = zest_GetPipeline(instance_pipeline, &command_list);
	
	if (instance_result == NULL) {
		failed_count++;
	}
	
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->device);
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
	zest_command_list_t command_list = create_test_command_list(tests);
	
	for (int i = 0; i < 4; i++) {
		zest_pipeline_template pipeline = create_basic_pipeline_template(tests, custom_blend_names[i]);
		zest_SetPipelineBlend(pipeline, custom_blends[i]);
		pipelines[i] = zest_GetPipeline(pipeline, &command_list);
		
		if (pipelines[i] == NULL) {
			failed_count++;
		}
	}
	
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->device);
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
	zest_command_list_t command_list = create_test_command_list(tests);
	
	// Test fill mode with default settings
	zest_pipeline_template pipeline1 = create_basic_pipeline_template(tests, rasterization_names[0]);
	zest_SetPipelinePolygonFillMode(pipeline1, zest_polygon_mode_fill);
	pipelines[0] = zest_GetPipeline(pipeline1, &command_list);
	
	// Test line mode (note: line width would need to be set via internal struct access)
	zest_pipeline_template pipeline2 = create_basic_pipeline_template(tests, rasterization_names[1]);
	zest_SetPipelinePolygonFillMode(pipeline2, zest_polygon_mode_line);
	pipelines[1] = zest_GetPipeline(pipeline2, &command_list);
	
	// Test point mode, which D3D12 doesn't have
	zest_bool skip_point = tests->platform == zest_platform_d3d12;
#if defined(__APPLE__)
	skip_point = ZEST_TRUE;	//MoltenVK's portability subset has no point fill mode
#endif
	if (!skip_point) {
		zest_pipeline_template pipeline3 = create_basic_pipeline_template(tests, rasterization_names[3]);
		zest_SetPipelinePolygonFillMode(pipeline3, zest_polygon_mode_point);
		pipelines[3] = zest_GetPipeline(pipeline3, &command_list);
	}
	
	// Test different culling combinations
	zest_pipeline_template pipeline4 = create_basic_pipeline_template(tests, rasterization_names[4]);
	zest_SetPipelineCullMode(pipeline4, zest_cull_mode_none);
	zest_SetPipelinePolygonFillMode(pipeline4, zest_polygon_mode_fill);
	pipelines[4] = zest_GetPipeline(pipeline4, &command_list);
	
	zest_pipeline_template pipeline5 = create_basic_pipeline_template(tests, rasterization_names[5]);
	zest_SetPipelineCullMode(pipeline5, zest_cull_mode_front);
	zest_SetPipelinePolygonFillMode(pipeline5, zest_polygon_mode_line);
	pipelines[5] = zest_GetPipeline(pipeline5, &command_list);
	
	for (int i = 0; i < 6; i++) {
		if (i != 2 && !(i == 3 && skip_point)) { // Skip index 2 (thick line) as we didn't create it
			if (pipelines[i] == NULL) {
				failed_count++;
			}
		}
	}
	
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

//Clip positions (-1,-1), (1,-1) and (-1,1): top-left, top-right, bottom-left in framebuffer space, so clockwise
static const char *winding_vertex_source =
	"#version 450\n"
	"void main() {\n"
	"	vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);\n"
	"	gl_Position = vec4(corner - 1.0, 0.0, 1.0);\n"
	"}\n";
static const char *winding_fragment_source =
	"#version 450\n"
	"layout(location = 0) out vec4 out_colour;\n"
	"void main() { out_colour = vec4(1.0, 0.0, 0.0, 1.0); }\n";
static const char *winding_vertex_hlsl =
	"float4 main(uint vertex_index : SV_VertexID) : SV_Position {\n"
	"	float2 corner = float2((vertex_index << 1) & 2, vertex_index & 2);\n"
	"	return float4(corner - 1.0, 0.0, 1.0);\n"
	"}\n";
static const char *winding_fragment_hlsl =
	"float4 main() : SV_Target0 { return float4(1.0, 0.0, 0.0, 1.0); }\n";

struct WindingCoverage {
	zest_uint red_count;
	zest_uint red_inside_count;		//Red pixels with x + y < 15
	zest_uint other_count;			//Pixels neither red nor opaque black
};

//Classifies a 16x16 r8g8b8a8 readback against the triangle, which covers exactly the pixels with x + y < 15
static WindingCoverage winding_coverage(const zest_uint *pixels) {
	WindingCoverage coverage = {};
	for (zest_uint y = 0; y != 16; ++y) {
		for (zest_uint x = 0; x != 16; ++x) {
			const unsigned char *channels = (const unsigned char *)&pixels[y * 16 + x];
			if (channels[0] == 255 && channels[1] == 0 && channels[2] == 0 && channels[3] == 255) {
				coverage.red_count++;
				if (x + y < 15) coverage.red_inside_count++;
			} else if (channels[0] != 0 || channels[1] != 0 || channels[2] != 0 || channels[3] != 255) {
				coverage.other_count++;
			}
		}
	}
	return coverage;
}

static zest_bool winding_covers_triangle(WindingCoverage coverage) {
	return coverage.red_count == 120 && coverage.red_inside_count == 120 && coverage.other_count == 0;
}

static zest_bool winding_covers_nothing(WindingCoverage coverage) {
	return coverage.red_count == 0 && coverage.other_count == 0;
}

static void winding_draw_task(const zest_command_list command_list, void *user_data) {
	zest_pipeline_template pipeline_template = (zest_pipeline_template)user_data;
	zest_viewport_t viewport = zest_CreateViewport(0.f, 0.f, 16.f, 16.f, 0.f, 1.f);
	zest_scissor_rect_t scissor = zest_CreateRect2D(16, 16, 0, 0);
	zest_cmd_ViewPort(command_list, &viewport);
	zest_cmd_Scissor(command_list, &scissor);
	zest_pipeline pipeline = zest_GetPipeline(pipeline_template, command_list);
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_Draw(command_list, 3, 1, 0, 0);
}

static int draw_winding_triangle(ZestTests *tests, zest_image image, zest_shader_handle vertex_shader, zest_shader_handle fragment_shader, zest_front_face front_face, const char *name, WindingCoverage *coverage) {
	zest_pipeline_template pipeline_template = zest_CreatePipelineTemplate(tests->device, name);
	zest_SetPipelineShaders(pipeline_template, vertex_shader, fragment_shader);
	zest_SetPipelineDisableVertexInput(pipeline_template);
	zest_SetPipelineBlend(pipeline_template, zest_BlendStateOpaque());
	zest_SetPipelineCullMode(pipeline_template, zest_cull_mode_back);
	zest_SetPipelineFrontFace(pipeline_template, front_face);
	if (!zest_BeginCommandGraph(tests->context, "Winding Coverage")) return 1;
	zest_resource_node target = zest_ImportImageResource("Winding Target", image, 0);
	zest_SetResourceClearColor(target, 0.f, 0.f, 0.f, 1.f);
	zest_BeginRenderPass("Winding Draw");
	zest_ConnectOutput(target);
	zest_SetPassTask(winding_draw_task, pipeline_template);
	zest_EndPass();
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	int failed = zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
	zest_uint pixels[16 * 16];
	memset(pixels, 0, sizeof(pixels));
	failed |= !zest_CopyImageToBitmap(tests->device, image, pixels);
	*coverage = winding_coverage(pixels);
	ZEST_PRINT("\t%s: %u red pixels, %u of them inside x + y < 15, %u neither red nor black", name, coverage->red_count, coverage->red_inside_count, coverage->other_count);
	return failed;
}

//With back faces culled, only the clockwise front face keeps the framebuffer-space clockwise triangle
int test__pipeline_winding_coverage(ZestTests *tests, Test *test) {
	zest_bool hlsl = zest_GetDevicePlatform(tests->device) == zest_platform_d3d12;
	zest_shader_handle vertex_shader = zest_CreateShader(tests->device, hlsl ? winding_vertex_hlsl : winding_vertex_source, zest_vertex_shader, "winding_coverage_vert", NULL, ZEST_TRUE);
	zest_shader_handle fragment_shader = zest_CreateShader(tests->device, hlsl ? winding_fragment_hlsl : winding_fragment_source, zest_fragment_shader, "winding_coverage_frag", NULL, ZEST_TRUE);
	zest_image_info_t image_info = zest_CreateImageInfo(16, 16);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_color_attachment | zest_image_flag_transfer_src;
	zest_image_handle image_handle = zest_CreateImage(tests->device, &image_info);
	zest_image image = zest_GetImage(image_handle);
	int failed = image == NULL;
	if (!failed) {
		WindingCoverage clockwise = {};
		WindingCoverage counter_clockwise = {};
		failed |= draw_winding_triangle(tests, image, vertex_shader, fragment_shader, zest_front_face_clockwise, "Winding Clockwise", &clockwise);
		failed |= draw_winding_triangle(tests, image, vertex_shader, fragment_shader, zest_front_face_counter_clockwise, "Winding Counter Clockwise", &counter_clockwise);
		failed |= !winding_covers_triangle(clockwise) || !winding_covers_nothing(counter_clockwise);
		zest_FreeImageNow(image_handle);
	}
	test->result = failed;
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

static const char *vertex_draw_vertex_glsl =
	"#version 450\n"
	"layout(location = 0) in vec2 position;\n"
	"layout(location = 1) in vec4 color;\n"
	"layout(location = 0) out vec4 out_color;\n"
	"void main() {\n"
	"	gl_Position = vec4(position, 0.0, 1.0);\n"
	"	out_color = color;\n"
	"}\n";
static const char *vertex_draw_fragment_glsl =
	"#version 450\n"
	"layout(location = 0) in vec4 in_color;\n"
	"layout(location = 0) out vec4 out_color;\n"
	"layout(push_constant) uniform push_block { vec4 tint; } push;\n"
	"void main() { out_color = in_color * push.tint; }\n";
static const char *vertex_draw_vertex_hlsl =
	"struct vertex_input {\n"
	"	float2 position : POSITION0;\n"
	"	float4 color : COLOR0;\n"
	"};\n"
	"struct varyings {\n"
	"	float4 position : SV_Position;\n"
	"	float4 color : COLOR0;\n"
	"};\n"
	"varyings main(vertex_input input) {\n"
	"	varyings output;\n"
	"	output.position = float4(input.position, 0.0, 1.0);\n"
	"	output.color = input.color;\n"
	"	return output;\n"
	"}\n";
static const char *vertex_draw_fragment_hlsl =
	"cbuffer zest_push : register(b0, space0) { float4 tint; };\n"
	"struct varyings {\n"
	"	float4 position : SV_Position;\n"
	"	float4 color : COLOR0;\n"
	"};\n"
	"float4 main(varyings input) : SV_Target0 { return input.color * tint; }\n";

struct VertexDrawVertex {
	float x;
	float y;
	zest_uint color;		//r8g8b8a8_unorm
};

struct VertexDrawResources {
	zest_buffer vertex_buffer;
	zest_buffer index_buffer;
	zest_shader_handle vertex_shader;
	zest_shader_handle fragment_shader;
	zest_pipeline_template pipeline_template;
};

//A quad over the left half of clip space then one over the right, all colour (255, 128, 0, 255), and the indices of one quad
static zest_bool create_vertex_draw_resources(zest_device device, VertexDrawResources *out_resources) {
	zest_bool hlsl = zest_GetDevicePlatform(device) == zest_platform_d3d12;
	*out_resources = {};
	out_resources->vertex_shader = zest_CreateShader(device, hlsl ? vertex_draw_vertex_hlsl : vertex_draw_vertex_glsl, zest_vertex_shader, "vertex_draw_vert", NULL, ZEST_TRUE);
	out_resources->fragment_shader = zest_CreateShader(device, hlsl ? vertex_draw_fragment_hlsl : vertex_draw_fragment_glsl, zest_fragment_shader, "vertex_draw_frag", NULL, ZEST_TRUE);
	zest_buffer_info_t vertex_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_cpu_to_gpu);
	zest_buffer_info_t index_info = zest_CreateBufferInfo(zest_buffer_type_index, zest_memory_usage_cpu_to_gpu);
	out_resources->vertex_buffer = zest_CreateBuffer(device, sizeof(VertexDrawVertex) * 8, &vertex_info);
	out_resources->index_buffer = zest_CreateBuffer(device, sizeof(zest_uint) * 6, &index_info);
	if (!out_resources->vertex_buffer || !out_resources->index_buffer) return ZEST_FALSE;
	zest_uint color = 255u | (128u << 8) | (255u << 24);
	VertexDrawVertex vertices[8] = {
		{ -1.f, -1.f, color }, { 0.f, -1.f, color }, { 0.f, 1.f, color }, { -1.f, 1.f, color },
		{ 0.f, -1.f, color }, { 1.f, -1.f, color }, { 1.f, 1.f, color }, { 0.f, 1.f, color },
	};
	zest_uint indices[6] = { 0, 1, 2, 2, 3, 0 };
	memcpy(zest_BufferData(out_resources->vertex_buffer), vertices, sizeof(vertices));
	memcpy(zest_BufferData(out_resources->index_buffer), indices, sizeof(indices));
	zest_pipeline_template pipeline_template = zest_CreatePipelineTemplate(device, "Vertex Draw");
	zest_SetPipelineShaders(pipeline_template, out_resources->vertex_shader, out_resources->fragment_shader);
	zest_AddVertexInputBindingDescription(pipeline_template, 0, sizeof(VertexDrawVertex), zest_input_rate_vertex);
	zest_AddVertexAttribute(pipeline_template, 0, 0, zest_format_r32g32_sfloat, offsetof(VertexDrawVertex, x));
	zest_AddVertexAttribute(pipeline_template, 0, 1, zest_format_r8g8b8a8_unorm, offsetof(VertexDrawVertex, color));
	zest_SetPipelineCullMode(pipeline_template, zest_cull_mode_none);
	zest_SetPipelineBlend(pipeline_template, zest_BlendStateOpaque());
	out_resources->pipeline_template = pipeline_template;
	return ZEST_TRUE;
}

static void free_vertex_draw_resources(VertexDrawResources *resources) {
	if (resources->pipeline_template) zest_FreePipelineTemplate(resources->pipeline_template);
	if (resources->vertex_buffer) zest_FreeBufferNow(resources->vertex_buffer);
	if (resources->index_buffer) zest_FreeBufferNow(resources->index_buffer);
	zest_FreeShader(resources->vertex_shader);
	zest_FreeShader(resources->fragment_shader);
}

static void set_full_viewport(const zest_command_list command_list) {
	zest_viewport_t viewport = zest_CreateViewport(0.f, 0.f, 16.f, 16.f, 0.f, 1.f);
	zest_scissor_rect_t scissor = zest_CreateRect2D(16, 16, 0, 0);
	zest_cmd_ViewPort(command_list, &viewport);
	zest_cmd_Scissor(command_list, &scissor);
}

static void vertex_draw_task(const zest_command_list command_list, void *user_data) {
	VertexDrawResources *resources = (VertexDrawResources *)user_data;
	set_full_viewport(command_list);
	zest_cmd_BindVertexBuffer(command_list, 0, 1, resources->vertex_buffer);
	zest_cmd_BindIndexBuffer(command_list, resources->index_buffer);
	zest_pipeline pipeline = zest_GetPipeline(resources->pipeline_template, command_list);
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	float left_tint[4] = { 1.f, 0.5f, 1.f, 1.f };
	zest_cmd_SendPushConstants(command_list, left_tint, sizeof(left_tint));
	zest_cmd_DrawIndexed(command_list, 6, 1, 0, 0, 0);
	float right_tint[4] = { 0.f, 1.f, 0.f, 1.f };
	zest_cmd_SendPushConstants(command_list, right_tint, sizeof(right_tint));
	zest_cmd_DrawIndexed(command_list, 6, 1, 0, 4, 0);
}

static zest_image_handle create_draw_target(zest_device device) {
	zest_image_info_t image_info = zest_CreateImageInfo(16, 16);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_color_attachment | zest_image_flag_transfer_src;
	return zest_CreateImage(device, &image_info);
}

//Runs task in one render pass on image, cleared to opaque black, with a 16x16 depth buffer cleared to 1 when with_depth is set, then reads image back
static int render_draw_pass(zest_context context, const char *name, zest_image image, zest_bool with_depth, zest_fg_execution_callback task, void *user_data, zest_uint pixels[256]) {
	if (!zest_BeginCommandGraph(context, name)) return 1;
	zest_resource_node target = zest_ImportImageResource("Draw Target", image, 0);
	zest_SetResourceClearColor(target, 0.f, 0.f, 0.f, 1.f);
	zest_resource_node depth = NULL;
	if (with_depth) {
		zest_image_resource_info_t depth_info = { zest_format_depth };
		depth_info.width = 16;
		depth_info.height = 16;
		depth = zest_AddTransientImageResource("Draw Depth", &depth_info);
		zest_FlagResourceAsEssential(depth);
	}
	zest_BeginRenderPass(name);
	zest_ConnectOutput(target);
	if (depth) zest_ConnectOutput(depth);
	zest_SetPassTask(task, user_data);
	zest_EndPass();
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	int failed = frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
	memset(pixels, 0, sizeof(zest_uint) * 256);
	failed |= !zest_CopyImageToBitmap(context->device, image, pixels);
	return failed;
}

//Each pixel of columns first_column to first_column + column_count - 1 within 1 of expected, in r8g8b8a8 channel order
static zest_bool columns_hold(const zest_uint *pixels, zest_uint first_column, zest_uint column_count, const int expected[4]) {
	for (zest_uint y = 0; y != 16; ++y) {
		for (zest_uint x = first_column; x != first_column + column_count; ++x) {
			const unsigned char *channels = (const unsigned char *)&pixels[y * 16 + x];
			for (int channel = 0; channel != 4; ++channel) {
				int difference = (int)channels[channel] - expected[channel];
				if (difference > 1 || difference < -1) {
					ZEST_PRINT("\tPixel (%u, %u) is (%u, %u, %u, %u), expected (%i, %i, %i, %i)", x, y, channels[0], channels[1], channels[2], channels[3], expected[0], expected[1], expected[2], expected[3]);
					return ZEST_FALSE;
				}
			}
		}
	}
	return ZEST_TRUE;
}

static const int vertex_draw_left[4] = { 255, 64, 0, 255 };
static const int vertex_draw_right[4] = { 0, 128, 0, 255 };

//Vertex and index buffers, an indexed draw with a vertex offset, and a push block read by the fragment shader
int test__pipeline_vertex_draw(ZestTests *tests, Test *test) {
	VertexDrawResources resources;
	int failed = !create_vertex_draw_resources(tests->device, &resources);
	zest_image_handle image_handle = create_draw_target(tests->device);
	zest_image image = zest_GetImage(image_handle);
	failed |= image == NULL;
	if (!failed) {
		zest_uint pixels[256];
		failed |= render_draw_pass(tests->context, "Vertex Draw", image, ZEST_FALSE, vertex_draw_task, &resources, pixels);
		failed |= !columns_hold(pixels, 0, 8, vertex_draw_left) || !columns_hold(pixels, 8, 8, vertex_draw_right);
	}
	if (image) zest_FreeImageNow(image_handle);
	free_vertex_draw_resources(&resources);
	test->result = failed;
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

static const char *depth_bias_vertex_glsl =
	"#version 450\n"
	"void main() {\n"
	"	vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);\n"
	"	gl_Position = vec4(corner * 2.0 - 1.0, 0.5, 1.0);\n"
	"}\n";
static const char *depth_bias_fragment_glsl =
	"#version 450\n"
	"layout(location = 0) out vec4 out_color;\n"
	"layout(push_constant) uniform push_block { vec4 color; } push;\n"
	"void main() { out_color = push.color; }\n";
static const char *depth_bias_vertex_hlsl =
	"float4 main(uint vertex_index : SV_VertexID) : SV_Position {\n"
	"	float2 corner = float2((vertex_index << 1) & 2, vertex_index & 2);\n"
	"	return float4(corner * 2.0 - 1.0, 0.5, 1.0);\n"
	"}\n";
static const char *depth_bias_fragment_hlsl =
	"cbuffer zest_push : register(b0, space0) { float4 color; };\n"
	"float4 main() : SV_Target0 { return color; }\n";

struct DepthBiasDraws {
	zest_pipeline_template templates[3];		//Red writing depth, green failing an equal depth, blue biased towards the camera
	zest_bool set_depth_bias;					//Off, blue's bias is the one baked into its template, which only D3D12 applies
};

static void depth_bias_task(const zest_command_list command_list, void *user_data) {
	DepthBiasDraws *draws = (DepthBiasDraws *)user_data;
	const float colors[3][4] = { { 1.f, 0.f, 0.f, 1.f }, { 0.f, 1.f, 0.f, 1.f }, { 0.f, 0.f, 1.f, 1.f } };
	set_full_viewport(command_list);
	for (int draw = 0; draw != 3; ++draw) {
		zest_pipeline pipeline = zest_GetPipeline(draws->templates[draw], command_list);
		if (!pipeline) return;
		if (draw == 2) {
			zest_scissor_rect_t left_half = zest_CreateRect2D(8, 16, 0, 0);
			zest_cmd_Scissor(command_list, &left_half);
		}
		zest_cmd_BindPipeline(command_list, pipeline);
		//Vulkan forgets a dynamic bias when a pipeline without one is bound, so it's set after the bind
		if (draw == 2 && draws->set_depth_bias) zest_cmd_SetDepthBias(command_list, -16.f, 0.f, 0.f);
		zest_cmd_SendPushConstants(command_list, (void *)colors[draw], sizeof(colors[draw]));
		zest_cmd_Draw(command_list, 3, 1, 0, 0);
	}
}

//A depth bias pulls an equal depth in front of what's already there, set with zest_cmd_SetDepthBias or, on D3D12, baked into the pipeline
int test__pipeline_depth_bias(ZestTests *tests, Test *test) {
	zest_device device = tests->device;
	test->frame_count++;
	zest_bool set_bias_pass = ZEST_TRUE;
#ifdef ZEST_ENABLE_D3D12
	if (tests->platform == zest_platform_d3d12 && !ZEST_DX_BACKEND(device, device)->support.dynamic_depth_bias) {
		ZEST_PRINT("\tThe zest_cmd_SetDepthBias pass was skipped, as this D3D12 driver has no dynamic depth bias");
		set_bias_pass = ZEST_FALSE;
	}
#endif
	zest_bool hlsl = zest_GetDevicePlatform(device) == zest_platform_d3d12;
	zest_shader_handle vertex_shader = zest_CreateShader(device, hlsl ? depth_bias_vertex_hlsl : depth_bias_vertex_glsl, zest_vertex_shader, "depth_bias_vert", NULL, ZEST_TRUE);
	zest_shader_handle fragment_shader = zest_CreateShader(device, hlsl ? depth_bias_fragment_hlsl : depth_bias_fragment_glsl, zest_fragment_shader, "depth_bias_frag", NULL, ZEST_TRUE);
	DepthBiasDraws draws;
	const char *names[3] = { "Depth Bias Red", "Depth Bias Green", "Depth Bias Blue" };
	for (int draw = 0; draw != 3; ++draw) {
		zest_pipeline_template pipeline_template = zest_CreatePipelineTemplate(device, names[draw]);
		zest_SetPipelineShaders(pipeline_template, vertex_shader, fragment_shader);
		zest_SetPipelineDisableVertexInput(pipeline_template);
		zest_SetPipelineBlend(pipeline_template, zest_BlendStateOpaque());
		zest_SetPipelineDepthTest(pipeline_template, ZEST_TRUE, ZEST_TRUE);
		if (draw != 0) pipeline_template->depth_stencil.depth_compare_op = zest_compare_op_less;
		if (draw == 2) {
			zest_SetPipelineDepthBias(pipeline_template, ZEST_TRUE);
			pipeline_template->rasterization.depth_bias_constant_factor = -16.f;
		}
		draws.templates[draw] = pipeline_template;
	}
	zest_image_handle image_handle = create_draw_target(device);
	zest_image image = zest_GetImage(image_handle);
	int failed = image == NULL;
	if (!failed) {
		zest_uint pixels[256];
		const int blue[4] = { 0, 0, 255, 255 };
		const int red[4] = { 255, 0, 0, 255 };
		if (set_bias_pass) {
			draws.set_depth_bias = ZEST_TRUE;
			failed |= render_draw_pass(tests->context, "Depth Bias", image, ZEST_TRUE, depth_bias_task, &draws, pixels);
			failed |= !columns_hold(pixels, 0, 8, blue) || !columns_hold(pixels, 8, 8, red);
		}
		//A D3D12 list that never sets a bias keeps the pipeline's baked one, with or without dynamic depth bias
		if (tests->platform == zest_platform_d3d12) {
			draws.set_depth_bias = ZEST_FALSE;
			failed |= render_draw_pass(tests->context, "Depth Bias Baked", image, ZEST_TRUE, depth_bias_task, &draws, pixels);
			failed |= !columns_hold(pixels, 0, 8, blue) || !columns_hold(pixels, 8, 8, red);
		}
		zest_FreeImageNow(image_handle);
	}
	for (int draw = 0; draw != 3; ++draw) {
		zest_FreePipelineTemplate(draws.templates[draw]);
	}
	zest_FreeShader(vertex_shader);
	zest_FreeShader(fragment_shader);
	test->result = failed;
	test->result |= zest_GetValidationErrorCount(device);
	return test->result;
}

struct IndirectDraws {
	VertexDrawResources resources;
	zest_buffer indexed_commands;
	zest_buffer commands;
	zest_pipeline_template triangle_template;
};

static void indirect_indexed_task(const zest_command_list command_list, void *user_data) {
	IndirectDraws *draws = (IndirectDraws *)user_data;
	set_full_viewport(command_list);
	zest_cmd_BindVertexBuffer(command_list, 0, 1, draws->resources.vertex_buffer);
	zest_cmd_BindIndexBuffer(command_list, draws->resources.index_buffer);
	zest_pipeline pipeline = zest_GetPipeline(draws->resources.pipeline_template, command_list);
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	float tint[4] = { 1.f, 0.5f, 1.f, 1.f };
	zest_cmd_SendPushConstants(command_list, tint, sizeof(tint));
	//The offset is a draw index, so this skips the empty first command
	zest_cmd_DrawIndexedIndirect(command_list, draws->indexed_commands, 1, 2, sizeof(zest_draw_indexed_indirect_command_t));
}

static void indirect_triangle_task(const zest_command_list command_list, void *user_data) {
	IndirectDraws *draws = (IndirectDraws *)user_data;
	set_full_viewport(command_list);
	zest_pipeline pipeline = zest_GetPipeline(draws->triangle_template, command_list);
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	float color[4] = { 1.f, 1.f, 0.f, 1.f };
	zest_cmd_SendPushConstants(command_list, color, sizeof(color));
	zest_cmd_DrawIndirect(command_list, draws->commands, 1, 1, sizeof(zest_draw_indirect_command_t));
}

//Indexed and non-indexed indirect draws read from an offset given as a draw index
int test__pipeline_indirect_draw(ZestTests *tests, Test *test) {
	zest_device device = tests->device;
	test->frame_count++;
	IndirectDraws draws = {};
	int failed = !create_vertex_draw_resources(device, &draws.resources);
	zest_buffer_info_t indirect_info = zest_CreateBufferInfo(zest_buffer_type_indirect, zest_memory_usage_cpu_to_gpu);
	draws.indexed_commands = zest_CreateBuffer(device, sizeof(zest_draw_indexed_indirect_command_t) * 3, &indirect_info);
	draws.commands = zest_CreateBuffer(device, sizeof(zest_draw_indirect_command_t) * 2, &indirect_info);
	failed |= !draws.indexed_commands || !draws.commands;
	zest_bool hlsl = zest_GetDevicePlatform(device) == zest_platform_d3d12;
	zest_shader_handle vertex_shader = zest_CreateShader(device, hlsl ? depth_bias_vertex_hlsl : depth_bias_vertex_glsl, zest_vertex_shader, "indirect_triangle_vert", NULL, ZEST_TRUE);
	zest_shader_handle fragment_shader = zest_CreateShader(device, hlsl ? depth_bias_fragment_hlsl : depth_bias_fragment_glsl, zest_fragment_shader, "indirect_triangle_frag", NULL, ZEST_TRUE);
	draws.triangle_template = zest_CreatePipelineTemplate(device, "Indirect Triangle");
	zest_SetPipelineShaders(draws.triangle_template, vertex_shader, fragment_shader);
	zest_SetPipelineDisableVertexInput(draws.triangle_template);
	zest_SetPipelineBlend(draws.triangle_template, zest_BlendStateOpaque());
	zest_image_handle image_handle = create_draw_target(device);
	zest_image image = zest_GetImage(image_handle);
	failed |= image == NULL;
	if (!failed) {
		zest_draw_indexed_indirect_command_t indexed_commands[3] = { { 0, 0, 0, 0, 0 }, { 6, 1, 0, 0, 0 }, { 6, 1, 0, 4, 0 } };
		memcpy(zest_BufferData(draws.indexed_commands), indexed_commands, sizeof(indexed_commands));
		zest_draw_indirect_command_t commands[2] = { { 0, 0, 0, 0 }, { 3, 1, 0, 0 } };
		memcpy(zest_BufferData(draws.commands), commands, sizeof(commands));
		zest_uint pixels[256];
		failed |= render_draw_pass(tests->context, "Indirect Indexed", image, ZEST_FALSE, indirect_indexed_task, &draws, pixels);
		failed |= !columns_hold(pixels, 0, 16, vertex_draw_left);
		const int yellow[4] = { 255, 255, 0, 255 };
		failed |= render_draw_pass(tests->context, "Indirect Triangle", image, ZEST_FALSE, indirect_triangle_task, &draws, pixels);
		failed |= !columns_hold(pixels, 0, 16, yellow);
	}
	if (image) zest_FreeImageNow(image_handle);
	if (draws.indexed_commands) zest_FreeBufferNow(draws.indexed_commands);
	if (draws.commands) zest_FreeBufferNow(draws.commands);
	zest_FreePipelineTemplate(draws.triangle_template);
	zest_FreeShader(vertex_shader);
	zest_FreeShader(fragment_shader);
	free_vertex_draw_resources(&draws.resources);
	test->result = failed;
	test->result |= zest_GetValidationErrorCount(device);
	return test->result;
}

struct ImGuiShaderDraw {
	zest_buffer vertex_buffer;
	zest_buffer index_buffer;
	zest_pipeline_template pipeline_template;
	zest_imgui_push_t push;
};

static void imgui_shader_task(const zest_command_list command_list, void *user_data) {
	ImGuiShaderDraw *draw = (ImGuiShaderDraw *)user_data;
	set_full_viewport(command_list);
	zest_cmd_BindVertexBuffer(command_list, 0, 1, draw->vertex_buffer);
	zest_cmd_BindIndexBuffer(command_list, draw->index_buffer);
	zest_pipeline pipeline = zest_GetPipeline(draw->pipeline_template, command_list);
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_SendPushConstants(command_list, &draw->push, sizeof(draw->push));
	zest_cmd_DrawIndexed(command_list, 6, 1, 0, 0, 0);
}

//ImGui's shader pair in a pipeline built as zest_imgui_Initialise builds its own: a texture array slice times the vertex colour
int test__pipeline_imgui_shaders(ZestTests *tests, Test *test) {
	zest_device device = tests->device;
	test->frame_count++;
	zest_bool hlsl = zest_GetDevicePlatform(device) == zest_platform_d3d12;
	zest_shader_handle vertex_shader = zest_CreateShader(device, hlsl ? zest_shader_imgui_vert_hlsl : zest_shader_imgui_vert, zest_vertex_shader, "imgui_test_vert", NULL, ZEST_TRUE);
	zest_shader_handle fragment_shader = zest_CreateShader(device, hlsl ? zest_shader_imgui_frag_hlsl : zest_shader_imgui_frag, zest_fragment_shader, "imgui_test_frag", NULL, ZEST_TRUE);
	ImGuiShaderDraw draw = {};
	draw.pipeline_template = zest_CreatePipelineTemplate(device, "ImGui Shaders");
	zest_AddVertexInputBindingDescription(draw.pipeline_template, 0, sizeof(zest_ImDrawVert_t), zest_input_rate_vertex);
	zest_AddVertexAttribute(draw.pipeline_template, 0, 0, zest_format_r32g32_sfloat, offsetof(zest_ImDrawVert_t, pos));
	zest_AddVertexAttribute(draw.pipeline_template, 0, 1, zest_format_r32g32_sfloat, offsetof(zest_ImDrawVert_t, uv));
	zest_AddVertexAttribute(draw.pipeline_template, 0, 2, zest_format_r8g8b8a8_unorm, offsetof(zest_ImDrawVert_t, col));
	zest_SetPipelineShaders(draw.pipeline_template, vertex_shader, fragment_shader);
	zest_SetPipelineFrontFace(draw.pipeline_template, zest_front_face_counter_clockwise);
	zest_SetPipelineCullMode(draw.pipeline_template, zest_cull_mode_none);
	zest_SetPipelineTopology(draw.pipeline_template, zest_topology_triangle_list);
	zest_SetPipelineBlend(draw.pipeline_template, zest_ImGuiBlendState());
	zest_SetPipelineDepthTest(draw.pipeline_template, ZEST_FALSE, ZEST_FALSE);

	zest_uint texels[16];
	for (int texel = 0; texel != 16; ++texel) texels[texel] = 255u | (128u << 8) | (255u << 16) | (255u << 24);
	zest_image_info_t texture_info = zest_CreateImageInfo(4, 4);
	texture_info.format = zest_format_r8g8b8a8_unorm;
	texture_info.flags = zest_image_preset_texture | zest_image_flag_force_image_array;
	zest_image_handle texture_handle = zest_CreateImageWithPixels(device, texels, sizeof(texels), &texture_info);
	zest_image texture = zest_GetImage(texture_handle);
	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	zest_sampler_handle sampler_handle = zest_CreateSampler(device, &sampler_info);
	zest_sampler sampler = zest_GetSampler(sampler_handle);
	int failed = texture == NULL || sampler == NULL;
	if (!failed) {
		draw.push.font_texture_index = zest_AcquireSampledImageIndex(device, texture, zest_texture_array_binding);
		draw.push.font_sampler_index = zest_AcquireSamplerIndex(device, sampler);
	}
	//The transform zest_imgui_UploadImGuiPass sends for a 16x16 display at the origin
	draw.push.transform = zest_Vec4Set(2.f / 16.f, 2.f / 16.f, -1.f, -1.f);

	zest_buffer_info_t vertex_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_cpu_to_gpu);
	zest_buffer_info_t index_info = zest_CreateBufferInfo(zest_buffer_type_index, zest_memory_usage_cpu_to_gpu);
	draw.vertex_buffer = zest_CreateBuffer(device, sizeof(zest_ImDrawVert_t) * 4, &vertex_info);
	draw.index_buffer = zest_CreateBuffer(device, sizeof(zest_uint) * 6, &index_info);
	failed |= !draw.vertex_buffer || !draw.index_buffer;
	zest_image_handle image_handle = create_draw_target(device);
	zest_image image = zest_GetImage(image_handle);
	failed |= image == NULL;
	if (!failed) {
		zest_uint yellow = 255u | (255u << 8) | (255u << 24);
		zest_ImDrawVert_t vertices[4] = {
			{ { 0.f, 0.f }, { 0.f, 0.f }, yellow }, { { 16.f, 0.f }, { 1.f, 0.f }, yellow },
			{ { 16.f, 16.f }, { 1.f, 1.f }, yellow }, { { 0.f, 16.f }, { 0.f, 1.f }, yellow },
		};
		zest_uint indices[6] = { 0, 1, 2, 2, 3, 0 };
		memcpy(zest_BufferData(draw.vertex_buffer), vertices, sizeof(vertices));
		memcpy(zest_BufferData(draw.index_buffer), indices, sizeof(indices));
		zest_uint pixels[256];
		failed |= render_draw_pass(tests->context, "ImGui Shaders", image, ZEST_FALSE, imgui_shader_task, &draw, pixels);
		//ImGui's blend writes alpha as source alpha times one minus source alpha, so an opaque draw leaves 0
		const int expected[4] = { 255, 128, 0, 0 };
		failed |= !columns_hold(pixels, 0, 16, expected);
	}
	if (image) zest_FreeImageNow(image_handle);
	if (texture) zest_FreeImageNow(texture_handle);
	if (sampler) zest_FreeSamplerNow(sampler_handle);
	if (draw.vertex_buffer) zest_FreeBufferNow(draw.vertex_buffer);
	if (draw.index_buffer) zest_FreeBufferNow(draw.index_buffer);
	zest_FreePipelineTemplate(draw.pipeline_template);
	zest_FreeShader(vertex_shader);
	zest_FreeShader(fragment_shader);
	test->result = failed;
	test->result |= zest_GetValidationErrorCount(device);
	return test->result;
}

//Helper for the shader cache test: does a file exist on disk?
static bool cache_file_exists(const char *path) {
	FILE *file = fopen(path, "rb");
	if (!file) {
		return false;
	}
	fclose(file);
	return true;
}

//The shader cache is keyed on a hash of the source, not on the shader name. This guards the regression where
//editing a shader kept serving the binary compiled from the previous source.
int test__shader_cache_invalidation(ZestTests *tests, Test *test) {
	static const char *glsl_source_a =
		"#version 450\n"
		"layout(location = 0) out vec4 out_colour;\n"
		"void main() { out_colour = vec4(1.0, 0.0, 0.0, 1.0); }\n";
	static const char *glsl_source_b =
		"#version 450\n"
		"layout(location = 0) out vec4 out_colour;\n"
		"void main() { out_colour = vec4(0.0, 1.0, 0.0, 1.0); }\n";
	static const char *hlsl_source_a = "float4 main() : SV_Target0 { return float4(1.0, 0.0, 0.0, 1.0); }\n";
	static const char *hlsl_source_b = "float4 main() : SV_Target0 { return float4(0.0, 1.0, 0.0, 1.0); }\n";
	zest_bool hlsl = zest_GetDevicePlatform(tests->device) == zest_platform_d3d12;
	const char *source_a = hlsl ? hlsl_source_a : glsl_source_a;
	const char *source_b = hlsl ? hlsl_source_b : glsl_source_b;

	const char *shader_name = "zest_cache_invalidation_test";
	zest_device device = tests->device;
	zest_device_init_flags saved_flags = device->init_flags;
	device->init_flags |= zest_device_init_flag_cache_shaders;

	int failed_count = 0;

	//First compile of source A populates the cache.
	zest_shader_handle handle_a = zest_CreateShader(device, source_a, zest_fragment_shader, shader_name, NULL, ZEST_FALSE);
	zest_shader shader_a = zest_GetShader(handle_a);
	zest_key key_a = shader_a->cache_key;
	zest_text_t path_a = ZEST__ZERO_INIT(zest_text_t);
	zest_SetText(device->allocator, &path_a, shader_a->cache_path.str);

	//Keep a copy of A's binary; the shader store can move entries as more shaders are added.
	zest_size size_a = shader_a->binary_size;
	char *binary_a = (char *)malloc(size_a);
	memcpy(binary_a, shader_a->binary, size_a);

	if (key_a == 0 || size_a == 0 || !cache_file_exists(path_a.str)) {
		failed_count++;
	}

	//Recompiling the same source must land on the same key and produce the same binary.
	zest_shader_handle handle_a2 = zest_CreateShader(device, source_a, zest_fragment_shader, shader_name, NULL, ZEST_FALSE);
	zest_shader shader_a2 = zest_GetShader(handle_a2);
	if (shader_a2->cache_key != key_a || shader_a2->binary_size != size_a || memcmp(shader_a2->binary, binary_a, size_a) != 0) {
		failed_count++;
	}

	//The regression: same name, different source. This must compile fresh rather than return A's binary.
	zest_shader_handle handle_b = zest_CreateShader(device, source_b, zest_fragment_shader, shader_name, NULL, ZEST_FALSE);
	zest_shader shader_b = zest_GetShader(handle_b);
	zest_key key_b = shader_b->cache_key;
	zest_text_t path_b = ZEST__ZERO_INIT(zest_text_t);
	zest_SetText(device->allocator, &path_b, shader_b->cache_path.str);

	if (key_b == key_a || !cache_file_exists(path_b.str)) {
		failed_count++;
	}
	if (shader_b->binary_size == size_a && memcmp(shader_b->binary, binary_a, size_a) == 0) {
		failed_count++;
	}

	//Macro definitions change the compiled output, so they have to change the key too.
	zest_shader_options options = zest_CreateShaderOptions(device);
	zest_AddMacroDefinition(options, "ZEST_TEST_MACRO", "1");
	zest_key key_macro = zest__shader_cache_key(source_a, zest_fragment_shader, "main", options, device->shader_cache_salt);
	if (key_macro == key_a) {
		failed_count++;
	}
	zest_FreeShaderOptions(options);

	free(binary_a);
	remove(path_a.str);
	remove(path_b.str);
	zest_FreeText(device->allocator, &path_a);
	zest_FreeText(device->allocator, &path_b);
	zest_FreeShader(handle_a);
	zest_FreeShader(handle_a2);
	zest_FreeShader(handle_b);
	device->init_flags = saved_flags;

	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

static const char *glsl_empty_compute = "#version 450\nlayout(local_size_x = 1) in;\nvoid main() {}\n";
static const char *hlsl_empty_compute = "[numthreads(1, 1, 1)]\nvoid main() {}\n";

//Compiles source into an existing shader through zest_CompileShader, which returns false where zest_CreateShader would assert
static zest_bool compile_test_source(zest_device device, zest_shader_handle shader_handle, const char *source, zest_shader_options options) {
	zest_SetText(device->allocator, &zest_GetShader(shader_handle)->shader_code, source);
	return zest_CompileShader(shader_handle, options);
}

static zest_uint shader_binary_magic(zest_shader shader) {
	zest_uint magic = 0;
	if (shader->binary && shader->binary_size >= sizeof(magic)) memcpy(&magic, shader->binary, sizeof(magic));
	return magic;
}

//A binary in the other platform's format is rejected, and a cache file in it is compiled over
int test__shader_binary_platform_mismatch(ZestTests *tests, Test *test) {
	zest_device device = tests->device;
	zest_bool d3d12 = zest_GetDevicePlatform(device) == zest_platform_d3d12;
	const zest_uint spirv_magic = 0x07230203;
	const zest_uint dxil_magic = 0x43425844;
	zest_uint own_magic = d3d12 ? dxil_magic : spirv_magic;
	zest_uint other_magic = d3d12 ? spirv_magic : dxil_magic;
	int failed_count = 0;

	zest_uint binary[4] = { other_magic, 0, 0, 0 };
	zest_uint error_count = zest_GetValidationErrorCount(device);
	zest_shader_handle other_handle = zest_CreateShaderFromBinary(device, "zest_mismatch_other", binary, sizeof(binary), zest_fragment_shader);
	if (zest_IsValidHandle((void *)&other_handle) || zest_GetValidationErrorCount(device) != error_count + 1) {
		ZEST_PRINT("\tA binary in the other platform's format wasn't rejected with one validation error");
		failed_count++;
	}

	binary[0] = own_magic;
	zest_shader_handle own_handle = zest_CreateShaderFromBinary(device, "zest_mismatch_own", binary, sizeof(binary), zest_fragment_shader);
	if (zest_IsValidHandle((void *)&own_handle)) {
		zest_FreeShader(own_handle);
	} else {
		ZEST_PRINT("\tA binary in this platform's format was rejected");
		failed_count++;
	}

	zest_device_init_flags saved_flags = device->init_flags;
	device->init_flags |= zest_device_init_flag_cache_shaders;
	const char *source = d3d12 ? hlsl_empty_compute : glsl_empty_compute;
	const char *shader_name = "zest_platform_mismatch_test";
	zest_shader_handle cached_handle = zest_CreateShader(device, source, zest_compute_shader, shader_name, NULL, ZEST_FALSE);
	zest_text_t cache_path = ZEST__ZERO_INIT(zest_text_t);
	zest_SetText(device->allocator, &cache_path, zest_GetShader(cached_handle)->cache_path.str);
	zest_FreeShader(cached_handle);
	failed_count += !cache_file_exists(cache_path.str);
	binary[0] = other_magic;
	FILE *planted_file = fopen(cache_path.str, "wb");
	if (planted_file) {
		fwrite(binary, 1, sizeof(binary), planted_file);
		fclose(planted_file);
	} else {
		failed_count++;
	}
	zest_shader_handle recompiled_handle = zest_CreateShader(device, source, zest_compute_shader, shader_name, NULL, ZEST_FALSE);
	zest_shader recompiled = zest_GetShader(recompiled_handle);
	if (strcmp(recompiled->cache_path.str, cache_path.str) != 0 || shader_binary_magic(recompiled) != own_magic || recompiled->binary_size <= sizeof(binary)) {
		ZEST_PRINT("\tA cache file in the other platform's format was loaded instead of compiled over");
		failed_count++;
	}
	zest_FreeShader(recompiled_handle);
	remove(cache_path.str);
	zest_FreeText(device->allocator, &cache_path);
	device->init_flags = saved_flags;

	zest_ResetValidationErrors(device);
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(device);
	test->frame_count++;
	return test->result;
}

//MSL is refused before any other backend's compiler sees it, and Metal keeps the text as the shader's binary and out of the cache
int test__shader_msl_source_platform(ZestTests *tests, Test *test) {
	static const char *msl_red_fragment =
		"#include <metal_stdlib>\n"
		"using namespace metal;\n"
		"fragment float4 red_fragment() {\n"
		"	return float4(1.0, 0.0, 0.0, 1.0);\n"
		"}\n";
	zest_device device = tests->device;
	zest_bool metal = zest_GetDevicePlatform(device) == zest_platform_metal;
	int failed_count = 0;
	zest_shader_options options = zest_CreateShaderOptions(device);
	zest_SetShaderOptionsMSL(options, 0, 0, 0);
	zest_device_init_flags saved_flags = device->init_flags;
	device->init_flags |= zest_device_init_flag_cache_shaders;
	zest_uint error_count = zest_GetValidationErrorCount(device);
	zest_shader_handle shader_handle = zest_CreateShader(device, msl_red_fragment, zest_fragment_shader, "zest_msl_source_test", options, ZEST_FALSE);
	if (metal) {
		zest_shader shader = shader_handle.value ? zest_GetShader(shader_handle) : NULL;
		zest_size text_size = strlen(msl_red_fragment) + 1;
		if (!shader || shader->binary_size != text_size || memcmp(shader->binary, msl_red_fragment, (size_t)text_size) != 0) {
			ZEST_PRINT("\tThe Metal backend didn't keep the MSL text and its terminating zero as the shader's binary");
			failed_count++;
		}
		if (shader && cache_file_exists(shader->cache_path.str)) {
			ZEST_PRINT("\tThe MSL shader was written to the shader cache");
			failed_count++;
		}
		failed_count += zest_GetValidationErrorCount(device) != error_count;
	} else {
		if (shader_handle.value || zest_GetValidationErrorCount(device) != error_count + 1 || !validation_error_contains(device, "is MSL, which only the Metal backend takes")) {
			ZEST_PRINT("\tzest_CreateShader with MSL options wasn't refused with one validation error");
			failed_count++;
		}
		zest_shader_handle compiled_handle = zest_CreateShader(device, zest_GetDevicePlatform(device) == zest_platform_d3d12 ? hlsl_empty_compute : glsl_empty_compute, zest_compute_shader, "zest_msl_recompile_test", NULL, ZEST_TRUE);
		error_count = zest_GetValidationErrorCount(device);
		if (compile_test_source(device, compiled_handle, msl_red_fragment, options) || zest_GetValidationErrorCount(device) != error_count + 1) {
			ZEST_PRINT("\tzest_CompileShader with MSL options wasn't refused with one validation error");
			failed_count++;
		}
		zest_FreeShader(compiled_handle);
	}
	if (shader_handle.value) zest_FreeShader(shader_handle);
	zest_FreeShaderOptions(options);
	device->init_flags = saved_flags;

	zest_ResetValidationErrors(device);
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(device);
	test->frame_count++;
	return test->result;
}

//Each backend defines its own platform macro when it compiles, and no other backend's
int test__shader_platform_macro(ZestTests *tests, Test *test) {
	static const char *glsl_needs_own =
		"#version 450\n"
		"layout(local_size_x = 1) in;\n"
		"#ifndef ZEST_VULKAN\n"
		"#error ZEST_VULKAN is missing\n"
		"#endif\n"
		"void main() {}\n";
	static const char *glsl_rejects_other =
		"#version 450\n"
		"layout(local_size_x = 1) in;\n"
		"#ifdef ZEST_D3D12\n"
		"#error ZEST_D3D12 is defined\n"
		"#endif\n"
		"void main() {}\n";
	static const char *glsl_broken = "#version 450\nlayout(local_size_x = 1) in;\nvoid main() { int value = ; }\n";
	static const char *metal_needs_own =
		"#version 450\n"
		"layout(local_size_x = 1) in;\n"
		"#ifndef ZEST_METAL\n"
		"#error ZEST_METAL is missing\n"
		"#endif\n"
		"void main() {}\n";
	static const char *metal_rejects_other =
		"#version 450\n"
		"layout(local_size_x = 1) in;\n"
		"#if defined(ZEST_VULKAN) || defined(ZEST_D3D12)\n"
		"#error ZEST_VULKAN or ZEST_D3D12 is defined\n"
		"#endif\n"
		"void main() {}\n";
	static const char *hlsl_needs_own =
		"#ifndef ZEST_D3D12\n"
		"#error ZEST_D3D12 is missing\n"
		"#endif\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n";
	static const char *hlsl_rejects_other =
		"#ifdef ZEST_VULKAN\n"
		"#error ZEST_VULKAN is defined\n"
		"#endif\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n";
	static const char *hlsl_broken = "[numthreads(1, 1, 1)]\nvoid main() { int value = ; }\n";
	zest_device device = tests->device;
	zest_platform_type platform = zest_GetDevicePlatform(device);
	zest_bool hlsl = platform == zest_platform_d3d12;
	const char *needs_own = hlsl ? hlsl_needs_own : platform == zest_platform_metal ? metal_needs_own : glsl_needs_own;
	const char *rejects_other = hlsl ? hlsl_rejects_other : platform == zest_platform_metal ? metal_rejects_other : glsl_rejects_other;
	int failed_count = 0;

	zest_shader_handle shader_handle = zest_CreateShader(device, hlsl ? hlsl_empty_compute : glsl_empty_compute, zest_compute_shader, "zest_platform_macro_test", NULL, ZEST_TRUE);
	if (!compile_test_source(device, shader_handle, needs_own, NULL)) {
		ZEST_PRINT("\tThe platform's own macro isn't defined");
		failed_count++;
	}
	if (!compile_test_source(device, shader_handle, rejects_other, NULL)) {
		ZEST_PRINT("\tThe other platform's macro is defined");
		failed_count++;
	}
	zest_FreeShader(shader_handle);

	if (!zest_ValidateShader(device, needs_own, zest_compute_shader, "zest_platform_macro_validate")) {
		ZEST_PRINT("\tzest_ValidateShader doesn't define the platform's macro");
		failed_count++;
	}
	ZEST_PRINT("\tThe next shader compilation error is expected");
	if (zest_ValidateShader(device, hlsl ? hlsl_broken : glsl_broken, zest_compute_shader, "zest_platform_macro_broken")) {
		ZEST_PRINT("\tzest_ValidateShader passed a source with a syntax error");
		failed_count++;
	}

	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(device);
	test->frame_count++;
	return test->result;
}

//Rewrites the file until its mtime moves past before, so the reload check can't miss the edit
static zest_bool macro_test__write_file(const char *path, const char *text, zest_u64 before) {
	for (int attempt = 0; attempt != 100; ++attempt) {
		FILE *file = fopen(path, "wb");
		if (!file) return ZEST_FALSE;
		fputs(text, file);
		fclose(file);
		zest_u64 after = 0;
		if (zest_GetFileModifiedTime(path, &after) && after != before) return ZEST_TRUE;
		SDL_Delay(10);
	}
	return ZEST_FALSE;
}

//Hot reload recompiles with the macros the shader was created or last compiled with
int test__shader_hot_reload_keeps_macros(ZestTests *tests, Test *test) {
	static const char *glsl_source =
		"#version 450\n"
		"layout(local_size_x = 1) in;\n"
		"#ifndef ZEST_RELOAD_MACRO\n"
		"#error ZEST_RELOAD_MACRO is missing\n"
		"#endif\n"
		"void main() {}\n";
	static const char *glsl_source_edited =
		"#version 450\n"
		"layout(local_size_x = 1) in;\n"
		"#ifndef ZEST_RELOAD_MACRO\n"
		"#error ZEST_RELOAD_MACRO is missing\n"
		"#endif\n"
		"void main() {}\n"
		"//edited\n";
	static const char *glsl_source_second_macro =
		"#version 450\n"
		"layout(local_size_x = 1) in;\n"
		"#if !defined(ZEST_RELOAD_MACRO) || !defined(ZEST_SECOND_MACRO)\n"
		"#error a macro is missing\n"
		"#endif\n"
		"void main() {}\n";
	static const char *hlsl_source =
		"#ifndef ZEST_RELOAD_MACRO\n"
		"#error ZEST_RELOAD_MACRO is missing\n"
		"#endif\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n";
	static const char *hlsl_source_edited =
		"#ifndef ZEST_RELOAD_MACRO\n"
		"#error ZEST_RELOAD_MACRO is missing\n"
		"#endif\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n"
		"//edited\n";
	static const char *hlsl_source_second_macro =
		"#if !defined(ZEST_RELOAD_MACRO) || !defined(ZEST_SECOND_MACRO)\n"
		"#error a macro is missing\n"
		"#endif\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n";
	const char *path = "zest_macro_reload_test.comp";
	zest_device device = tests->device;
	zest_bool hlsl = zest_GetDevicePlatform(device) == zest_platform_d3d12;
	const char *source = hlsl ? hlsl_source : glsl_source;
	const char *source_edited = hlsl ? hlsl_source_edited : glsl_source_edited;
	const char *source_second_macro = hlsl ? hlsl_source_second_macro : glsl_source_second_macro;
	if (!macro_test__write_file(path, source, 0)) {
		ZEST_PRINT("\tCould not write %s", path);
		test->result = 1;
		test->frame_count++;
		return test->result;
	}
	int failed_count = 0;

	zest_shader_options options = zest_CreateShaderOptions(device);
	zest_AddMacroDefinition(options, "ZEST_RELOAD_MACRO", "1");
	zest_shader_handle shader_handle = zest_CreateShaderFromFile(device, path, "zest_macro_reload_test", zest_compute_shader, options, ZEST_TRUE);
	//The shader has to keep its own copy of the options
	zest_FreeShaderOptions(options);
	zest_SetShaderHotReload(shader_handle, ZEST_TRUE);

	if (!macro_test__write_file(path, source_edited, zest_GetShader(shader_handle)->last_mtime) || zest_CheckShaderHotReload(device) != 1 || zest_GetShaderLastError(shader_handle)[0]) {
		ZEST_PRINT("\tHot reload lost the shader's macros: %s", zest_GetShaderLastError(shader_handle));
		failed_count++;
	}

	//NULL options recompile with the stored ones instead of dropping them
	if (!zest_CompileShader(shader_handle, NULL)) {
		ZEST_PRINT("\tzest_CompileShader with NULL options dropped the shader's macros");
		failed_count++;
	}
	if (!macro_test__write_file(path, source, zest_GetShader(shader_handle)->last_mtime) || zest_CheckShaderHotReload(device) != 1 || zest_GetShaderLastError(shader_handle)[0]) {
		ZEST_PRINT("\tHot reload lost the macros after zest_CompileShader with NULL: %s", zest_GetShaderLastError(shader_handle));
		failed_count++;
	}

	//Compiling with new options makes them the ones hot reload uses
	zest_shader_options second_options = zest_CreateShaderOptions(device);
	zest_AddMacroDefinition(second_options, "ZEST_RELOAD_MACRO", "1");
	zest_AddMacroDefinition(second_options, "ZEST_SECOND_MACRO", "1");
	if (!zest_CompileShader(shader_handle, second_options)) {
		failed_count++;
	}
	zest_FreeShaderOptions(second_options);
	if (!macro_test__write_file(path, source_second_macro, zest_GetShader(shader_handle)->last_mtime) || zest_CheckShaderHotReload(device) != 1 || zest_GetShaderLastError(shader_handle)[0]) {
		ZEST_PRINT("\tHot reload lost the macros from zest_CompileShader: %s", zest_GetShaderLastError(shader_handle));
		failed_count++;
	}

	zest_FreeShader(shader_handle);
	remove(path);
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}
