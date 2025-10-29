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

struct render_target_app_t {
	zest_context context;
	zest_pipeline_template downsample_pipeline;	    //Handle to the pipeline template we will use for the downsampling part of the blur effect
	zest_pipeline_template upsample_pipeline;	    //Handle to the pipeline template we will use for the upsampling part of the blur effect
	zest_pipeline_template composite_pipeline;		//Handle to the pipeline template we will use to composite the base and blur render targets
	zest_pipeline_template bloom_pass_pipeline;		//Handle to the pipeline template we will use filter the base target to pick out a color threshold
	zest_shader_resources_handle render_target_resources;	//Shader resources for drawing the render target to the screen
	zest_compute_handle downsampler_compute;
	zest_compute_handle upsampler_compute;
	zest_uint render_target_push;
	zest_sampler_handle pass_through_sampler;
	zest_sampler_handle mipped_sampler;
	zest_uint pass_through_sampler_index;
	zest_uint mipped_sampler_index;
	zest_image_handle top_target;						//Render target to draw the result of the blur effect on top of the other layers
	zest_image_handle base_target;						//The base target to draw the initial images that will be blurred
	zest_image_handle final_blur;						//Render target where the final blur effect happens
	zest_image_handle downsampler;						//Render target where the bloom pass happens to filter out dark colors
	zest_image_handle upsampler;							//Render target where the bloom pass happens to filter out dark colors
	zest_image_handle compositor;						//Render target to combine the base target with the final blur
	zest_image_handle tonemap;							//Render target to tonemap the composted base and blur/bloom layers
	zest_layer_handle base_layer;				            //Base layer for drawing to the base render target
	zest_layer_handle top_layer;				            //Top layer for drawing to the top render target
	zest_layer_handle font_layer;				            //Font layer for drawing some text to the top layer
	zest_image_handle texture;				            //A texture to store the images
	zest_shader_resources_handle sprite_shader_resources;	//Shader resources for the sprite drawing
	zest_atlas_region image;					            //Handle to the statue image
	zest_atlas_region wabbit;					            //Handle to the rabbit image that we'll bounce around the screen
	zest_msdf_font_t font;					            //Handle to the font
	zest_font_resources_t font_resources;
	BlurPushConstants blur_push_constants;	        //The push constants containing the texture size and the direction of the blur
	BloomPushConstants bloom_constants;		        //The push constants containing the bloom thresholds
	CompositePushConstants composite_push_constants;//The push constants used with compositing the render targets to the swap chain
	zest_execution_timeline timeline;

	double mouse_x, mouse_y;
	double mouse_delta_x, mouse_delta_y;

	struct wabbit_pos {
		float x, y;						            //Some variables to help bounce the rabbit around
		float vx, vy;
	} wabbit_pos;
};

void InitExample(render_target_app_t * example);
