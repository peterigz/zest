#version 450
#extension GL_EXT_nonuniform_qualifier : require
//The debug overlay's fragment shader on Vulkan and Metal, which samples one slice of the font array per character

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in flat float in_layer;

layout(location = 0) out vec4 out_color;
layout(set = 0, binding = 0) uniform sampler samplers[];
layout(set = 0, binding = 1) uniform texture2D textures[];
layout(set = 0, binding = 3) uniform texture2DArray texture_arrays[];

layout(push_constant) uniform imgui_push
{
	vec4 transform;
	uint texture_index;
	uint sampler_index;
} pc;

void main()
{
	float alpha = texture(sampler2DArray(texture_arrays[pc.texture_index], samplers[pc.sampler_index]), vec3(in_uv, in_layer)).r;
	out_color = vec4(in_color.rgb, in_color.a * alpha);
}
