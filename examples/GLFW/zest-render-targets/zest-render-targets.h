#pragma once
#include <GLFW/glfw3.h>
#include <zest.h>

struct BlurPushConstants {
	zest_uint storage_image_index;
	zest_uint src_mip_index;
	zest_uint downsampler_mip_index;
	zest_uint sampler_index;
};

struct CompositePushConstants {
	zest_uint base_index;
	zest_uint bloom_index;
	zest_uint sampler_index;
	float bloom_alpha;
};

struct BloomPushConstants {
	zest_vec4 settings;				
};

struct RenderCacheInfo {
	bool draw_imgui;
	zest_image_handle test_texture;
};

struct render_target_app_t {
	zest_context context;
	zest_pipeline_template composite_pipeline;		//Handle to the pipeline template we will use to composite the base and blur render targets
	zest_shader_resources_handle render_target_resources;	//Shader resources for drawing the render target to the screen
	zest_compute_handle downsampler_compute;
	zest_compute_handle upsampler_compute;
	zest_uint render_target_push;
	zest_sampler_handle sampler;
	zest_uint sampler_index;
	zest_layer_handle font_layer;				            //Font layer for drawing some text to the top layer
	zest_msdf_font_t font;					            //Handle to the font
	zest_font_resources_t font_resources;
	BloomPushConstants bloom_constants;		        //The push constants containing the bloom thresholds
	RenderCacheInfo cache_info;
	zest_timer_handle timer;

	double mouse_x, mouse_y;
	double mouse_delta_x, mouse_delta_y;
};

void InitExample(render_target_app_t * example);
