#version 450 core
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_samplerless_texture_functions : enable

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec3 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler samplers[];
layout(set = 1, binding = 3) uniform texture2DArray images[];

layout(push_constant) uniform quad_index
{
    float radius;
    float bleed;
    float aa_factor;
    float thickness;
    uint sampler_index;
    uint image_index;
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
    vec4 sampled = texture(sampler2DArray(images[font.image_index], samplers[font.sampler_index]), frag_tex_coord);

    vec2 texture_size = textureSize(images[font.image_index], 0).xy;
    float scale = get_uv_scale(frag_tex_coord.xy * texture_size) * font.aa_factor;
    float d = (median(sampled.r, sampled.g, sampled.b) - 0.75) * font.radius;
    float sdf = (d + font.thickness) / scale + 0.5 + font.bleed;
    float mask = clamp(sdf, 0.0, 1.0);
    glyph = vec4(glyph.rgb, glyph.a * mask);

	out_color.rgb = glyph.rgb * glyph.a;
	out_color.a = glyph.a;
	//out_color = vec4(1, 1, 1, 1);

}