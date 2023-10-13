#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec3 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(binding = 1) uniform sampler2DArray texture_sampler;

layout(push_constant) uniform quad_index
{
    mat4 model;
	vec4 parameters;
	vec4 shadow_parameters;
	vec4 shadow_color;
	vec4 camera;
	uint flags;
} font;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

vec4 blend(vec4 src, vec4 dst, float alpha) {
    // src OVER dst porter-duff blending
    float a = src.a + dst.a * (1.0 - src.a);
    vec3 rgb = (src.a * src.rgb + dst.a * dst.rgb * (1.0 - src.a)) / (a == 0.0 ? 1.0 : a);
    return vec4(rgb, a * alpha);
}

float linearstep(float a, float b, float x) {
    return clamp((x - a) / (b - a), 0.0, 1.0);
}

float get_uv_scale(vec2 uv) {
	vec2 dx = dFdx(uv);
	vec2 dy = dFdy(uv);
	return (length(dx) + length(dy)) * 0.5;
}

void main() {

	vec4 glyph = frag_color;
	float opacity;
	vec4 sampled = texture(texture_sampler, frag_tex_coord);

	vec2 texture_size = textureSize( texture_sampler, 0 ).xy;
	float scale = get_uv_scale(frag_tex_coord.xy * texture_size) * font.parameters.z;
	float d = (median(sampled.r, sampled.g, sampled.b) - 0.75) * font.parameters.x;
	float sdf = (d + font.parameters.w) / scale + 0.5 + font.parameters.y;
	float mask = clamp(sdf, 0.0, 1.0);
	glyph = vec4(glyph.rgb, glyph.a * mask);

	if (font.shadow_color.a > 0) {
		float sd = texture(texture_sampler, vec3(frag_tex_coord.xy - font.shadow_parameters.xy / texture_size.xy, 0)).a;
		float shadowAlpha = linearstep(0.5 - font.shadow_parameters.z, 0.5 + font.shadow_parameters.z, sd) * font.shadow_color.a;
		shadowAlpha *= 1.0 - mask * font.shadow_parameters.w;
		vec4 shadow = vec4(font.shadow_color.rgb, shadowAlpha);
		out_color = blend(blend(vec4(0), glyph, 1.0), shadow, frag_color.a);
		out_color.rgb = out_color.rgb * out_color.a;
	} else {
		out_color.rgb = glyph.rgb * glyph.a;
		out_color.a = glyph.a;
	}

}