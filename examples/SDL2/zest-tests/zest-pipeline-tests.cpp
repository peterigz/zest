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
	zest_shader_handle vert_shader = zest_CreateShaderFromFile(tests->device, "examples/SDL2/zest-tests/shaders/vertex.vert", "vertex_vert.spv", zest_vertex_shader, NULL, true);
	zest_shader_handle frag_shader = zest_CreateShaderFromFile(tests->device, "examples/SDL2/zest-tests/shaders/vertex.frag", "vertex_frag.spv", zest_fragment_shader, NULL, true);

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
		zest_shader_handle vert_shader = zest_CreateShaderFromFile(tests->device, "examples/SDL2/zest-tests/shaders/vertex.vert", "vertex_vert.spv", zest_vertex_shader, NULL, true);
		zest_shader_handle frag_shader = zest_CreateShaderFromFile(tests->device, "examples/SDL2/zest-tests/shaders/vertex.frag", "vertex_frag.spv", zest_fragment_shader, NULL, true);

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
	
	zest_shader_handle vert_shader = zest_CreateShaderFromFile(tests->device, "examples/SDL2/zest-tests/shaders/vertex.vert", "vertex_vert.spv", zest_vertex_shader, NULL, true);
	zest_shader_handle frag_shader = zest_CreateShaderFromFile(tests->device, "examples/SDL2/zest-tests/shaders/vertex.frag", "vertex_frag.spv", zest_fragment_shader, NULL, true);
	
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
	
	// Test point mode
	zest_pipeline_template pipeline3 = create_basic_pipeline_template(tests, rasterization_names[3]);
	zest_SetPipelinePolygonFillMode(pipeline3, zest_polygon_mode_point);
	pipelines[3] = zest_GetPipeline(pipeline3, &command_list);
	
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
		if (i != 2) { // Skip index 2 (thick line) as we didn't create it
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
	if (!zest_BeginCommandGraph(tests->context, "Winding Coverage", 0)) return 1;
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
	zest_shader_handle vertex_shader = zest_CreateShader(tests->device, winding_vertex_source, zest_vertex_shader, "winding_coverage_vert", NULL, ZEST_TRUE);
	zest_shader_handle fragment_shader = zest_CreateShader(tests->device, winding_fragment_source, zest_fragment_shader, "winding_coverage_frag", NULL, ZEST_TRUE);
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
	static const char *source_a =
		"#version 450\n"
		"layout(location = 0) out vec4 out_colour;\n"
		"void main() { out_colour = vec4(1.0, 0.0, 0.0, 1.0); }\n";
	static const char *source_b =
		"#version 450\n"
		"layout(location = 0) out vec4 out_colour;\n"
		"void main() { out_colour = vec4(0.0, 1.0, 0.0, 1.0); }\n";

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
	zest_key key_macro = zest__shader_cache_key(source_a, zest_fragment_shader, "main", options);
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
	static const char *source =
		"#version 450\n"
		"layout(local_size_x = 1) in;\n"
		"#ifndef ZEST_RELOAD_MACRO\n"
		"#error ZEST_RELOAD_MACRO is missing\n"
		"#endif\n"
		"void main() {}\n";
	static const char *source_edited =
		"#version 450\n"
		"layout(local_size_x = 1) in;\n"
		"#ifndef ZEST_RELOAD_MACRO\n"
		"#error ZEST_RELOAD_MACRO is missing\n"
		"#endif\n"
		"void main() {}\n"
		"//edited\n";
	static const char *source_second_macro =
		"#version 450\n"
		"layout(local_size_x = 1) in;\n"
		"#if !defined(ZEST_RELOAD_MACRO) || !defined(ZEST_SECOND_MACRO)\n"
		"#error a macro is missing\n"
		"#endif\n"
		"void main() {}\n";
	const char *path = "zest_macro_reload_test.comp";
	zest_device device = tests->device;
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
