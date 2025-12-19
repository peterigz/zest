#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec4 uv;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler samplers[];
layout(set = 1, binding = 1) uniform texture2D images[];

//The push constant struct for storing 
layout(push_constant) uniform push_constants
{
	uint sampler_index;
	uint texture_index;
} pc;

void main() {
	vec4 texel = texture(sampler2D(images[nonuniformEXT(pc.texture_index)], samplers[nonuniformEXT(pc.sampler_index)]), uv.xy);
	out_color.rgb = texel.rgb * color.rgb * uv.w * texel.a;
	out_color.a = 0;
	out_color.rgb = color.rgb * uv.w * texel.a;
	//out_color = vec4(1, 1, 1, 0);
}