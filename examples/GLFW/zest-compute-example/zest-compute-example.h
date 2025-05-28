#pragma once

#include <zest.h>
#include "implementations/impl_imgui.h"
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui_glfw.h"
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
} ubo;

struct ImGuiApp {
	zest_imgui imgui_info;
	zest_index imgui_draw_routine_index;

	zest_texture particle_texture;
	zest_texture gradient_texture;
	zest_descriptor_set_layout descriptor_layout;
	zest_descriptor_set_t descriptor_set;
	zest_descriptor_pool descriptor_pool;
	zest_shader_resources shader_resources;

	zest_descriptor_buffer particle_buffer;
	zest_pipeline_template particle_pipeline;
	zest_vertex_input_descriptions vertice_attributes;
	zest_uniform_buffer compute_uniform_buffer[ZEST_MAX_FIF];
	zest_descriptor_set_layout uniform_layout;
	zest_descriptor_set_t uniform_set[ZEST_MAX_FIF];
	zest_descriptor_pool uniform_set_pool;
	zest_compute compute;
    VkDescriptorSet *draw_sets;

	zest_render_graph render_graph;

	zest_timer loop_timer;

	float frame_timer;
	float anim_start;
	float timer;
	bool attach_to_cursor;
	bool request_graph_print;
};

static inline float Radians(float degrees) { return degrees * 0.01745329251994329576923690768489f; }
static inline float Degrees(float radians) { return radians * 57.295779513082320876798154814105f; }
void InitImGuiApp(ImGuiApp *app);
void UpdateComputeUniformBuffers(ImGuiApp *app);
void RecordComputeCommands(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data);
int DrawComputeSpritesCondition(zest_draw_routine draw_routine);
void RecordComputeSprites(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data);
