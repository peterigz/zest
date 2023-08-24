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
	zest_index render_queue_index;
	zest_index base_layer_index;
	zest_index top_target_layer_index;
	zest_index texture_index;
	zest_index image_index;
	PushConstants push_constants;
};

void InitExample(RenderTargetExample * example);
void AddHorizontalBlur(zest_render_target *target, void *data);
void AddVerticalBlur(zest_render_target *target, void *data);
