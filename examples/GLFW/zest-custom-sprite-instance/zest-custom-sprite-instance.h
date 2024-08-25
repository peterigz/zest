#pragma once

#include <zest.h>
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui_glfw.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

typedef struct zest_custom_sprite_instance_t {     //48 bytes - For 2 colored sprites. Color bands are stored in a texture that is sampled in the shader
	zest_vec4 position_rotation;                   //The position of the sprite with rotation in w and stretch in z
	zest_u64 size_handle;                          //Size of the sprite in pixels and the handle packed into a u64 (4 16bit floats)
	zest_u64 uv;                                   //The UV coords of the image in the texture packed into a u64 snorm (4 16bit floats)
	zest_uint alignment;                           //normalised alignment vector 2 floats packed into 16bits
    zest_uint intensity;                           //2 intensities for color and color hint
	zest_uint lerp_values;                         //Time interpolation value and the balance value to mix between the 2 colors (2 16bit unorm floats)
    zest_uint texture_indexes;                     //4 indexes: 2 y positions for the color band position and 2 texture array indexes
} zest_custom_sprite_instance_t;

struct ImGuiApp {
	zest_imgui_layer_info imgui_layer_info;
	zest_index imgui_draw_routine_index;
	zest_texture imgui_font_texture;
	zest_texture test_texture;
	zest_texture color_ramps_texture;
	zest_image test_image;
	zest_image color_ramps_image;
	zest_bitmap_t color_ramps_bitmap;
	zest_descriptor_set_layout custom_descriptor_set_layout;
	zest_descriptor_set_t custom_descriptor_set;

	zest_vertex_input_descriptions custom_sprite_vertice_attributes = 0;	//Must be zero'd
	zest_pipeline custom_pipeline;
	zest_shader custom_frag_shader;
	zest_shader custom_vert_shader;
	zest_layer custom_layer;
	float mix_value;
};

void InitImGuiApp(ImGuiApp *app);

void DarkStyle2() {
	ImGui::GetStyle().FrameRounding = 4.0f;
	ImGui::GetStyle().GrabRounding = 4.0f;

	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(1.00f, 1.00f, 1.00f, 0.26f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_Border] = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.32f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.34f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.91f, 0.35f, 0.05f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.5f, 0.5f, 0.5f, .25f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_Separator] = ImVec4(0.00f, 0.00f, 0.00f, 0.22f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_Tab] = ImVec4(0.17f, 0.17f, 0.17f, 0.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.80f);
	colors[ImGuiCol_TabSelected] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_TabDimmed] = ImVec4(0.86f, 0.31f, 0.02f, 0.00f);
	colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.86f, 0.31f, 0.02f, 0.68f);
	colors[ImGuiCol_DockingPreview] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.59f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.2f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.00f, 0.00f, 0.00f, 0.1f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.00f, 0.00f, 0.00f, 0.11f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

	ImGuiStyle &style = ImGui::GetStyle();
	style.WindowPadding.x = 10.f;
	style.WindowPadding.y = 10.f;
	style.FramePadding.x = 6.f;
	style.FramePadding.y = 4.f;
	style.ItemSpacing.x = 10.f;
	style.ItemSpacing.y = 6.f;
	style.ItemInnerSpacing.x = 5.f;
	style.ItemInnerSpacing.y = 5.f;
	style.WindowRounding = 0.f;
	style.FrameRounding = 4.f;
	style.ScrollbarRounding = 1.f;
	style.GrabRounding = 1.f;
	style.TabRounding = 4.f;
	style.IndentSpacing = 20.f;
	style.FrameBorderSize = 1.0f;

	style.ChildRounding = 0.f;
	style.PopupRounding = 0.f;
	style.CellPadding.x = 4.f;
	style.CellPadding.y = 2.f;
	style.TouchExtraPadding.x = 0.f;
	style.TouchExtraPadding.y = 0.f;
	style.ColumnsMinSpacing = 6.f;
	style.ScrollbarSize = 14.f;
	style.ScrollbarRounding = 1.f;
	style.GrabMinSize = 10.f;
	style.TabMinWidthForCloseButton = 0.f;
	style.DisplayWindowPadding.x = 19.f;
	style.DisplayWindowPadding.y = 19.f;
	style.DisplaySafeAreaPadding.x = 3.f;
	style.DisplaySafeAreaPadding.y = 3.f;
	style.MouseCursorScale = 1.f;
	style.WindowMinSize.x = 32.f;
	style.WindowMinSize.y = 32.f;
	style.ChildBorderSize = 1.f;

}

static const char *custom_frag_shader = ZEST_GLSL(450 core,
layout(location = 0) in vec3 in_tex_coord;
layout(location = 1) in float mix_value;
layout(location = 2) in flat ivec3 in_color_ramp_coords;
layout(location = 3) in vec2 intensity;
layout(location = 0) out vec4 outColor;
layout(binding = 1) uniform sampler2DArray texSampler;
layout(binding = 2) uniform sampler2DArray color_ramps;

void main() {
	vec4 texel = texture(texSampler, in_tex_coord);
	vec4 ramp_texel = texelFetch(color_ramps, in_color_ramp_coords, 0) * intensity.x;
	vec3 color2 = vec3(1, 0, 1) * intensity.y;
	vec3 frag_color = mix(color2.rgb, ramp_texel.rgb, texel.a * mix_value);
	outColor.rgb = texel.rgb * frag_color.rgb * texel.a;
	outColor.a = texel.a * ramp_texel.a;
}
);

static const char *custom_vert_shader = ZEST_GLSL(450 core,
	//Quad indexes
	const int indexes[6] = int[6](0, 1, 2, 2, 1, 3);

layout(binding = 0) uniform UboView
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
layout(location = 0) in vec4 position_rotation;
layout(location = 1) in vec4 size_handle;
layout(location = 2) in vec4 uv;
layout(location = 3) in vec2 alignment;
layout(location = 4) in vec2 intensity;
layout(location = 5) in vec2 interpolation;
layout(location = 6) in uint texture_array_index;

layout(location = 0) out vec3 out_tex_coord;
layout(location = 1) out float out_slider;
layout(location = 2) out ivec3 out_color_ramp_coords;
layout(location = 3) out vec2 out_intensity;

void main() {
	vec2 alignment_normal = normalize(vec2(alignment.x, alignment.y + 0.000001)) * position_rotation.z;

	vec2 uvs[4];
	uvs[0].x = uv.x; uvs[0].y = uv.y;
	uvs[1].x = uv.z; uvs[1].y = uv.y;
	uvs[2].x = uv.x; uvs[2].y = uv.w;
	uvs[3].x = uv.z; uvs[3].y = uv.w;

	vec2 bounds[4];
	bounds[0].x = size_handle.x * (0 - size_handle.z);
	bounds[0].y = size_handle.y * (0 - size_handle.w);
	bounds[3].x = size_handle.x * (1 - size_handle.z);
	bounds[3].y = size_handle.y * (1 - size_handle.w);
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
	ivec4 texture_indexes = ivec4((texture_array_index & 0xFF000000) >> 24, (texture_array_index & 0x00FF0000) >> 16, (texture_array_index & 0x0000FF00) >> 8, texture_array_index & 0x000000FF);
	out_tex_coord = vec3(uvs[index], texture_indexes.x);
	out_color_ramp_coords = ivec3(int(interpolation.x * 255), texture_indexes.y, texture_indexes.z);
	out_slider = pc.parameters1.x;
	out_intensity = intensity;
}
);
