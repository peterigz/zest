#pragma once

#include <GLFW/glfw3.h>
#include <zest.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

#define PARTICLE_COUNT 256 * 1024

struct Particle {
	zest_vec2 pos;								// Particle position
	zest_vec2 vel;								// Particle velocity
	zest_vec4 gradient_pos;						// Texture coordinates for the gradient ramp map
};

struct ComputeUniformBuffer {					// Compute shader uniform block object
	float deltaT;								//		Frame delta time
	float dest_x;								//		x position of the attractor
	float dest_y;								//		y position of the attractor
	int32_t particleCount = PARTICLE_COUNT;
	int32_t particle_buffer_index = 0;
} ubo;

struct ParticleFragmentPush {
	int particle_index;
	int gradient_index;
	int sampler_index;
};

struct RenderCacheInfo {
	bool draw_imgui;
};

struct ComputeExample {
	zest_context context;
	zest_device device;
	zest_imgui_t imgui;

	zest_image_handle particle_image;
	zest_image_handle gradient_image;
	zest_uint particle_image_index;
	zest_uint gradient_image_index;

	zest_sampler_handle particle_sampler;
	zest_uint sampler_index;

	zest_buffer particle_buffer;
	zest_uint particle_buffer_index;
	zest_pipeline_template particle_pipeline;
	zest_uniform_buffer_handle compute_uniform_buffer;
	zest_compute_handle compute;

	zest_timer_t loop_timer;
	RenderCacheInfo cache_info;

	float frame_timer;
	float anim_start;
	float timer;
	bool attach_to_cursor;
	bool request_graph_print;
};

static inline float Radians(float degrees) { return degrees * 0.01745329251994329576923690768489f; }
static inline float Degrees(float radians) { return radians * 57.295779513082320876798154814105f; }
void InitComputeExample(ComputeExample *app);
void UpdateComputeUniformBuffers(ComputeExample *app);
void RecordComputeCommands(zest_command_list command_buffer, void *user_data);
void RecordComputeSprites(zest_command_list command_buffer, void *user_data);
void MainLoop(ComputeExample *app);
