#pragma once

struct PushConstants {
	zest_vec4 blur;				
	zest_vec2 texture_size;
} push;

struct RenderTargetExample {
	zest_pipeline blur_pipeline;		//Handle to the pipeline we will use for the blur effect
	zest_render_target top_target;		//Render target to draw the result of the blur effect on top of the other layers
	zest_render_target base_target;		//The base target to draw the initial images that will be blurred
	zest_render_target final_blur;		//Render target where the final blur effect happens
	zest_command_queue command_queue;	//Custom command queue that we'll build from scratch
	zest_layer base_layer;				//Base layer for drawing to the base render target
	zest_layer top_layer;				//Top layer for drawing to the top render target
	zest_layer font_layer;				//Font layer for drawing some text to the top layer
	zest_texture texture;				//A texture to store the images
	zest_shader blur_frag_shader;
	zest_shader blur_vert_shader;
	zest_shader_resources sprite_shader_resources;	//Shader resources for the sprite drawing
	zest_shader_resources rt_shader_resources;	//Shader resources for the render target drawing
	zest_image image;					//Handle to the statue image
	zest_image wabbit;					//Handle to the rabbit image that we'll bounce around the screen
	zest_font font;						//Handle to the font
	PushConstants push_constants;		//The push constants containing the texture size and the direction of the blur

	struct wabbit_pos {
		float x, y;						//Some variables to help bounce the rabbit around
		float vx, vy;
	} wabbit_pos;
};

void InitExample(RenderTargetExample * example);
void RecordHorizontalBlur(zest_render_target_t *target, void *data, zest_uint fif);
void RecordVerticalBlur(zest_render_target_t *target, void *data, zest_uint fif);
void AddHorizontalBlur(zest_render_target_t *target, void *data);
void AddRecordVerticalBlur(zest_render_target_t *target, void *data);
