#pragma once

struct PushConstants {
	zest_vec4 blur;
	zest_vec2 texture_size;
} push;

struct RenderTargetExample {
	zest_index blur_pipeline_index;
	zest_index top_target_index;
	zest_index base_target_index;
	zest_index final_blur_index;
	zest_command_queue command_queue;
	zest_layer base_layer;
	zest_layer top_layer;
	zest_texture texture;
	zest_image image;
	PushConstants push_constants;
};

void InitExample(RenderTargetExample * example);
void AddHorizontalBlur(zest_render_target_t *target, void *data);
void AddVerticalBlur(zest_render_target_t *target, void *data);
