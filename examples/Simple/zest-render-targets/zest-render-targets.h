#pragma once

struct BlurPushConstants {
	zest_uint storage_image_index;
	zest_uint src_mip_index;
	zest_uint downsampler_mip_index;
};

struct CompositePushConstants {
	zest_uint base_index;
	zest_uint bloom_index;
	float bloom_alpha;
};

struct BloomPushConstants {
	zest_vec4 settings;				
};

struct RenderTargetExample {
	zest_pipeline_template downsample_pipeline;	    //Handle to the pipeline template we will use for the downsampling part of the blur effect
	zest_pipeline_template upsample_pipeline;	    //Handle to the pipeline template we will use for the upsampling part of the blur effect
	zest_pipeline_template composite_pipeline;		//Handle to the pipeline template we will use to composite the base and blur render targets
	zest_pipeline_template bloom_pass_pipeline;		//Handle to the pipeline template we will use filter the base target to pick out a color threshold
	zest_shader_resources render_target_resources;	//Shader resources for drawing the render target to the screen
	zest_compute downsampler_compute;
	zest_compute upsampler_compute;
	zest_uint render_target_push;
	zest_sampler pass_through_sampler;
	zest_sampler mipped_sampler;
	zest_texture top_target;						//Render target to draw the result of the blur effect on top of the other layers
	zest_texture base_target;						//The base target to draw the initial images that will be blurred
	zest_texture final_blur;						//Render target where the final blur effect happens
	zest_texture downsampler;						//Render target where the bloom pass happens to filter out dark colors
	zest_texture upsampler;							//Render target where the bloom pass happens to filter out dark colors
	zest_texture compositor;						//Render target to combine the base target with the final blur
	zest_texture tonemap;							//Render target to tonemap the composted base and blur/bloom layers
	zest_command_queue command_queue;	            //Custom command queue that we'll build from scratch
	zest_layer base_layer;				            //Base layer for drawing to the base render target
	zest_layer top_layer;				            //Top layer for drawing to the top render target
	zest_layer font_layer;				            //Font layer for drawing some text to the top layer
	zest_texture texture;				            //A texture to store the images
	zest_shader_resources sprite_shader_resources;	//Shader resources for the sprite drawing
	zest_image image;					            //Handle to the statue image
	zest_image wabbit;					            //Handle to the rabbit image that we'll bounce around the screen
	zest_font font;						            //Handle to the font
	BlurPushConstants blur_push_constants;	        //The push constants containing the texture size and the direction of the blur
	BloomPushConstants bloom_constants;		        //The push constants containing the bloom thresholds
	CompositePushConstants composite_push_constants;//The push constants used with compositing the render targets to the swap chain

	struct wabbit_pos {
		float x, y;						            //Some variables to help bounce the rabbit around
		float vx, vy;
	} wabbit_pos;
};

void InitExample(RenderTargetExample * example);
