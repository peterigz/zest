#version 450 core
#extension GL_EXT_nonuniform_qualifier : enable


layout(location = 0) in vec4 in_color;
layout(location = 1) in vec3 in_uv;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 5) uniform texture2D textures[];
layout(set = 0, binding = 0) uniform sampler samplers[];

//Not used by default by can be used in custom imgui image shaders
layout(push_constant) uniform imgui_push
{
	vec4 transform;
	uint font_texture_index;
	uint font_sampler_index;
	uint image_layer;
} pc;

void main()
{
	out_color = in_color * texture(sampler2D(textures[pc.font_texture_index], samplers[pc.font_sampler_index]), in_uv.xy);
}
