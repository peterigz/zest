#version 450
layout(location = 0) in vec3 in_tex_coord;
layout(location = 1) in flat ivec3 in_color_ramp_coords;
layout(location = 2) in vec3 in_intensity_curved_alpha;

layout(location = 0) out vec4 out_color;
layout(set = 1, binding = 1) uniform sampler2DArray tex_sampler;
layout(set = 1, binding = 2) uniform sampler2DArray color_ramps;

void main() {
	vec4 texel = texture(tex_sampler, in_tex_coord);
	ivec3 ramp = ivec3(in_color_ramp_coords.z, in_color_ramp_coords.x, in_color_ramp_coords.y);
	vec4 ramp_texel = texelFetch(color_ramps, ramp, 0) * in_intensity_curved_alpha.x;
	float curved_alpha = 1 - smoothstep(texel.a * in_intensity_curved_alpha.z, texel.a, 1 - in_intensity_curved_alpha.y);
	out_color.rgb = texel.rgb * ramp_texel.rgb * curved_alpha * texel.a;
	out_color.a = texel.a * ramp_texel.a * curved_alpha;
}