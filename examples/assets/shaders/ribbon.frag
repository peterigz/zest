#version 450

layout(location = 0) in vec4 uv;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2DArray tex_sampler;

void main() {
	vec4 texel = texture(tex_sampler, uv.xyz);
	out_color.rgb = texel.rgb * color.rgb * uv.w * texel.a;
	out_color.a = 0;
	out_color.rgb = color.rgb * uv.w * texel.a;
	//out_color = vec4(1, 1, 1, 0);
}