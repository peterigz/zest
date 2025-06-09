#pragma once

#include <zest.h>
#include "implementations/impl_imgui.h"
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui_glfw.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

struct zest_custom_sprite_instance_t {     //48 bytes - For 2 colored sprites. Color bands are stored in a texture that is sampled in the shader
	zest_vec4 position_rotation;                   //The position of the sprite with rotation in w and stretch in z
	zest_u64 size_handle;                          //Size of the sprite in pixels and the handle packed into a u64 (4 16bit floats)
	zest_u64 uv;                                   //The UV coords of the image in the texture packed into a u64 snorm (4 16bit floats)
	zest_uint alignment;                           //normalised alignment vector 2 floats packed into 16bits
	zest_uint intensity;                           //2 intensities for color and color hint
	zest_uint lerp_values;                         //Time interpolation value and the balance value to mix between the 2 colors (2 16bit unorm floats)
	zest_uint texture_indexes;                     //4 indexes: 2 y positions for the color band position and 2 texture array indexes
};

struct  extra_layer_data {
	float intensity2;
};

struct ImGuiApp {
	zest_index imgui_draw_routine_index;
	zest_texture imgui_font_texture;
	zest_texture test_texture;
	zest_texture color_ramps_texture;
	zest_image test_image;
	zest_image color_ramps_image;
	zest_bitmap_t color_ramps_bitmap;
	zest_set_layout custom_descriptor_set_layout;
	zest_descriptor_set_t custom_descriptor_set;
	zest_shader_resources shader_resources;

	zest_vertex_input_descriptions custom_sprite_vertice_attributes = 0;	//Must be zero'd
	zest_pipeline_template custom_pipeline;
	zest_shader custom_frag_shader;
	zest_shader custom_vert_shader;
	zest_layer custom_layer;
	float lerp_value;
	float mix_value;
	extra_layer_data layer_data;
};

void InitImGuiApp(ImGuiApp *app);
void zest_DrawCustomSprite(zest_layer layer, zest_image image, float x, float y, float r, float sx, float sy, float hx, float hy, zest_uint alignment, float stretch, zest_vec2 lerp_values);

static const char *custom_frag_shader = ZEST_GLSL(450 core,
const float intensity_max_value = 128.0 / 32767.0;
layout(location = 0) in vec3 in_tex_coord;
layout(location = 1) in float mix_value;
layout(location = 2) in flat ivec4 in_color_ramp_coords;
layout(location = 3) in vec2 intensity;
layout(location = 0) out vec4 outColor;
layout(set = 1, binding = 0) uniform sampler2DArray texture_sampler[];

layout(push_constant) uniform quad_index
{
	uint texture_index1;
	uint texture_index2;
	uint texture_index3;
	uint texture_index4;
	vec4 parameters1;
	vec4 parameters2;
	vec4 parameters3;
	vec4 camera;
} pc;

void main() {
	vec4 texel = texture(texture_sampler[pc.texture_index1], in_tex_coord);
	ivec3 ramp1 = ivec3(in_color_ramp_coords.x, 0, in_color_ramp_coords.w);
	ivec3 ramp2 = ivec3(in_color_ramp_coords.x, 1, in_color_ramp_coords.w);
	vec4 ramp_texel1 = texelFetch(texture_sampler[pc.texture_index2], ramp1, 0) * (intensity.x * intensity_max_value);
	vec4 ramp_texel2 = texelFetch(texture_sampler[pc.texture_index2], ramp2, 0) * (intensity.y * intensity_max_value);
	vec4 frag_color = mix(ramp_texel1, ramp_texel2, texel.a * mix_value);
	outColor.rgb = texel.rgb * frag_color.rgb * texel.a;
	outColor.a = texel.a * frag_color.a;
}
);

static const char *custom_vert_shader = ZEST_GLSL(450 core,
//Quad indexes
const int indexes[6] = int[6](0, 1, 2, 2, 1, 3);
const float scale_max_value = 4096.0 / 32767.0;
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
	uint texture_index1;
	uint texture_index2;
	uint texture_index3;
	uint texture_index4;
	vec4 parameters1;
	vec4 parameters2;
	vec4 parameters3;
	vec4 camera;
} pc;

//Vertex
//layout(location = 0) in vec2 vertex_position;

//Instance
layout(location = 0) in vec4 position_rotation;
layout(location = 1) in vec4 size_handle;
layout(location = 2) in vec4 uv;
layout(location = 3) in vec2 alignment;
layout(location = 4) in vec2 intensity;
layout(location = 5) in vec2 interpolation;
layout(location = 6) in uint texture_array_index;

layout(location = 0) out vec3 out_tex_coord;
layout(location = 1) out float out_slider;
layout(location = 2) out ivec4 out_color_ramp_coords;
layout(location = 3) out vec2 out_intensity;

void main() {
	vec2 alignment_normal = normalize(vec2(alignment.x, alignment.y + 0.000001)) * position_rotation.z;
    vec2 size = size_handle.xy * scale_max_value;
    vec2 handle = size_handle.zw * handle_max_value;

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

	mat4 modelView = uboView.view;
	vec3 pos = matrix * vec3(vertex_position.x, vertex_position.y, 1);
	pos.xy += alignment_normal * dot(pos.xy, alignment_normal);
	pos.xy += position_rotation.xy;
	gl_Position = uboView.proj * modelView * vec4(pos, 1.0);

	//----------------
	ivec4 texture_indexes = ivec4((texture_array_index & 0xFF000000) >> 24, (texture_array_index & 0x00FF0000) >> 16, (texture_array_index & 0x0000FF00) >> 8, texture_array_index & 0x000000FF);
	out_tex_coord = vec3(uvs[index], texture_indexes.x);
	out_color_ramp_coords = ivec4(int(interpolation.x * 255), texture_indexes.y, texture_indexes.z, texture_indexes.w);
	out_slider = interpolation.y;
	out_intensity = intensity;
}
);
