#pragma once

struct PushConstants {
	zest_vec4 blur;
	zest_vec2 texture_size;
} push;

struct RenderTargetExample {
	zest_pipeline blur_pipeline;
	zest_render_target top_target;
	zest_render_target base_target;
	zest_render_target final_blur;
	zest_command_queue command_queue;
	zest_layer base_layer;
	zest_layer top_layer;
	zest_layer font_layer;
	zest_texture texture;
	zest_image image;
	zest_image wabbit;
	zest_font font;
	PushConstants push_constants;

	struct wabbit_pos {
		float x, y;
		float vx, vy;
	} wabbit_pos;
};

void InitExample(RenderTargetExample * example);
void AddHorizontalBlur(zest_render_target_t *target, void *data);
void AddVerticalBlur(zest_render_target_t *target, void *data);