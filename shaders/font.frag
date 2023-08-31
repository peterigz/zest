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

vec2 safeNormalize( in vec2 v )
{
   float len = length( v );
   len = ( len > 0.0 ) ? 1.0 / len : 0.0;
   return v * len;
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

const float font_weight = 0;

void main() {

	vec4 glyph = frag_color;
	float opacity;
	vec4 sampled = texture(texture_sampler, frag_tex_coord);

	float expand = font.parameters.w;
	float bleed = font.camera.x;
	float radius = font.parameters.x;	
	float scale = get_uv_scale(frag_tex_coord.xy * textureSize(texture_sampler, 0).xy) * font.parameters.z;
	
	float d = (median(sampled.r, sampled.g, sampled.b) - 0.75) * radius;
	float s = (d + expand / font.parameters.y) / scale + 0.5 + bleed;
	vec2 sdf = vec2(s, s);
	float mask = clamp(sdf.x, 0.0, 1.0);

	glyph = frag_color;


	if (font.shadow_color.a > 0) {
		vec3 u_textureSize = textureSize( texture_sampler, 0 );
		float sd = texture(texture_sampler, vec3(frag_tex_coord.xy - font.shadow_parameters.xy / u_textureSize.xy, 0)).a + font_weight;
		float shadowAlpha = linearstep(0.5 - font.shadow_parameters.z, 0.5 + font.shadow_parameters.z, sd) * font.shadow_color.a;
		shadowAlpha *= 1.0 - opacity * font.shadow_parameters.w;
		vec4 shadow = vec4(font.shadow_color.rgb, shadowAlpha);
		out_color = vec4(glyph.rgb * mask, glyph.a * mask);
		out_color = blend(blend(vec4(0), out_color, 1.0), shadow, frag_color.a);
		//out_color.rgb = out_color.rgb * out_color.a;
	} else {
		out_color = vec4(glyph.rgb * mask, glyph.a * mask);
	}

}