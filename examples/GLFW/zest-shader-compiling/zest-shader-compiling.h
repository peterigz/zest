#pragma once

#include <zest.h>
#include "implementations/impl_imgui.h"
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui_glfw.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

typedef struct {
	char filepath[256];
	time_t last_modified;
} FileMonitor;

struct ImGuiApp {
	zest_imgui_t imgui_layer_info;
	zest_index imgui_draw_routine_index;
	zest_texture imgui_font_texture;
	zest_texture test_texture;
	zest_image test_image;

	zest_pipeline custom_pipeline;
	zest_shader custom_frag_shader;
	zest_shader custom_vert_shader;
	zest_shader_resources shader_resources;
	zest_layer custom_layer;
	shaderc_compiler_t compiler;
	shaderc_compilation_result_t validation_result;
	float mix_value;
	float num_cells;
	zest_millisecs start_time;
	FileMonitor shader_file;
};

void InitImGuiApp(ImGuiApp *app);
int EditShaderCode(ImGuiInputTextCallbackData *data);
void init_file_monitor(FileMonitor *monitor, const char *filepath);
bool check_file_changed(FileMonitor *monitor);

static const char *custom_frag_shader = ZEST_GLSL(450 core,
layout(location = 0) in vec4 in_frag_color;
layout(location = 1) in vec3 in_tex_coord;
layout(location = 2) in float mix_value;
layout(location = 0) out vec4 outColor;
layout(set = 1, binding = 0) uniform sampler2DArray texSampler;

void main() {
	vec4 texel = texture(texSampler, in_tex_coord);
	vec3 color2 = vec3(1, 0, 1);
	vec3 frag_color = mix(color2.rgb, in_frag_color.rgb, texel.a * mix_value);
	outColor.rgb = texel.rgb * frag_color.rgb * texel.a;
	outColor.a = texel.a * in_frag_color.a;
}
);

static const char *custom_vert_shader = ZEST_GLSL(450 core,
//Quad indexes
const int indexes[6] = int[6](0, 1, 2, 2, 1, 3);
const float size_max_value = 4096.0 / 32767.0;
const float handle_max_value = 128.0 / 32767.0;

layout(set = 0, binding = 0) uniform UboView
{
	mat4 view;
	mat4 proj;
	vec4 parameters1;
	vec4 parameters2;
	vec2 res;
	uint millisecs;
} uboView;

layout(push_constant) uniform quad_index
{
	mat4 model;
	vec4 parameters1;
	vec4 parameters2;
	vec4 parameters3;
	vec4 camera;
} pc;

//Vertex
//layout(location = 0) in vec2 vertex_position;

//Instance
layout(location = 0) in vec4 uv;
layout(location = 1) in vec4 position_rotation;
layout(location = 2) in vec4 size_handle;
layout(location = 3) in vec2 alignment;
layout(location = 4) in vec4 in_color;
layout(location = 5) in uint intensity_texture_array;

layout(location = 0) out vec4 out_frag_color;
layout(location = 1) out vec3 out_tex_coord;
layout(location = 2) out float out_slider;

void main() {
	//Pluck out the intensity value from 
	float intensity = intensity_texture_array & uint(0x003fffff);
	intensity /= 524287.875;
	//Scale the size and handle to the correct values
	vec2 size = size_handle.xy * size_max_value;
	vec2 handle = size_handle.zw * handle_max_value;

	vec2 alignment_normal = normalize(vec2(alignment.x, alignment.y + 0.000001)) * position_rotation.z;

	vec2 uvs[4];
	uvs[0].x = uv.x; uvs[0].y = uv.y;
	uvs[1].x = uv.z; uvs[1].y = uv.y;
	uvs[2].x = uv.x; uvs[2].y = uv.w;
	uvs[3].x = uv.z; uvs[3].y = uv.w;

	vec2 bounds[4];
	bounds[0].x = size.x * (0 - handle.x);
	bounds[0].y = size.y * (0 - handle.y);
	bounds[3].x = size.x * (1 - handle.x);
	bounds[3].y = size.y * (1 - handle.y);
	bounds[1].x = bounds[3].x;
	bounds[1].y = bounds[0].y;
	bounds[2].x = bounds[0].x;
	bounds[2].y = bounds[3].y;

	int index = indexes[gl_VertexIndex];

	vec2 vertex_position = bounds[index];

	mat3 matrix = mat3(1.0);
	float s = sin(position_rotation.w);
	float c = cos(position_rotation.w);

	matrix[0][0] = c;
	matrix[0][1] = s;
	matrix[1][0] = -s;
	matrix[1][1] = c;

	mat4 modelView = uboView.view * pc.model;
	vec3 pos = matrix * vec3(vertex_position.x, vertex_position.y, 1);
	pos.xy += alignment_normal * dot(pos.xy, alignment_normal);
	pos.xy += position_rotation.xy;
	gl_Position = uboView.proj * modelView * vec4(pos, 1.0);

	//----------------
	out_tex_coord = vec3(uvs[index], intensity_texture_array);
	out_frag_color = in_color * intensity;
	out_slider = pc.parameters1.x;
}
);
