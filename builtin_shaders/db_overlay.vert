#version 450
#extension GL_EXT_nonuniform_qualifier : require
//The debug overlay's vertex shader on Vulkan and Metal

layout(push_constant) uniform imgui_push
{
	vec4 transform;
	uint font_index;
	uint sampler_index;
} pc;

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_tex_coord;
layout(location = 2) in vec4 in_color;
layout(location = 3) in uint in_character_index;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out flat float out_layer;

void main() {

	gl_Position = vec4(in_position * pc.transform.xy + pc.transform.zw, 0.0, 1.0);

	out_uv = in_tex_coord;
	out_layer = float(in_character_index);
	out_color = in_color;
}
