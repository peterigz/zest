#version 450

layout(location = 0) in vec3 in_tex_coord;
layout(location = 1) in flat ivec3 in_color_ramp_coords;
layout(location = 2) in vec4 in_intensity_curved_alpha_map;

layout(location = 0) out vec4 out_color;
layout(set = 1, binding = 2) uniform sampler2DArray tex_sampler;
layout(set = 1, binding = 3) uniform sampler2DArray color_ramps;

void main() {
	vec4 texel = texture(tex_sampler, in_tex_coord);
	float texture_influence = in_intensity_curved_alpha_map.w;
	int ramp_x = int(min((texel.r) * texture_influence, 1) * 255);
	ivec3 ramp = ivec3(ramp_x, in_color_ramp_coords.x, in_color_ramp_coords.y);
	vec4 ramp_texel = texelFetch(color_ramps, ramp, 0) * in_intensity_curved_alpha_map.x;
	float curved_alpha = 1 - smoothstep(texel.a * in_intensity_curved_alpha_map.z, texel.a, 1 - in_intensity_curved_alpha_map.y);
	out_color.rgb = texel.rgb * ramp_texel.rgb * curved_alpha * texel.a;
	out_color.a = texel.a * ramp_texel.a * curved_alpha;
}