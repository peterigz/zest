#version 450

layout(location = 0) in vec4 uv;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2DArray tex_sampler;

void main() {
	vec4 texel = texture(tex_sampler, uv.xyz);
	texel.rgb *= vec3(1., .8, .3);
	out_color.rgb = texel.rgb * uv.w * texel.a;
	out_color.a = 0;
//	out_color = vec4(1, 1, 1, 0);
}