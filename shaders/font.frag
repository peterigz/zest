#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec3 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(binding = 1) uniform sampler2DArray texture_sampler;

layout(push_constant) uniform quad_index
{
    mat4 model;
    vec4 parameters1;
	vec4 shadow_parameters;
	vec4 shadow_color;
	vec4 camera;
	uint flags;
} font;

/*
float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

float screenPxRange() {
    vec2 unitRange = vec2(font.parameters1.x) / vec2(textureSize(texture_sampler, 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(frag_tex_coord.xy);
    return max(0.5*dot(unitRange, screenTexSize), 1.0);
}

void main() {
    vec4 texel = texture(texture_sampler, frag_tex_coord);
    float dist = median(texel.r, texel.g, texel.b);

    float pxDist = screenPxRange() * (dist - 0.5);
    float opacity = clamp(pxDist + 0.5, 0.0, 1.0);

	vec4 glyph = vec4(frag_color.rgb, frag_color.a * opacity);
	out_color.rgb = glyph.rgb * glyph.a;
	out_color.a = glyph.a;
}
*/

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

const float font_weight = 0;

void main() {

	vec4 glyph;
	float opacity;
	vec3 sampled = texture(texture_sampler, frag_tex_coord).rgb;

	if(font.flags == 0) {
		//Simple rendering of the glyph
		// Bilinear sampling of the distance field
		// Acquiring the signed distance
		float d = median(sampled.r, sampled.g, sampled.b) + font_weight - 0.5;
		// The anti-aliased measure of how "inside" the fragment lies
		opacity = clamp(d/fwidth(d) + 0.5, 0.0, 1.0);
		// Combining the two colors
		glyph = vec4(frag_color.rgb, frag_color.a * opacity);
	} else if((font.flags & 1) > 0) {
		//More precise rendering of the glyph for smaller textures
		vec3 uv = frag_tex_coord * textureSize( texture_sampler, 0 );
		// Calculate derivates
		vec2 Jdx = dFdx( uv.xy );
		vec2 Jdy = dFdy( uv.xy );
		// calculate signed distance (in texels).
		float sig_dist = median( sampled.r, sampled.g, sampled.b ) - 0.5;
		// For proper anti-aliasing, we need to calculate signed distance in pixels. We do this using derivatives.
		vec2 gradDist = safeNormalize( vec2( dFdx( sig_dist ), dFdy( sig_dist ) ) );
		vec2 grad = vec2( gradDist.x * Jdx.x + gradDist.y * Jdy.x, gradDist.x * Jdx.y + gradDist.y * Jdy.y );
		// Apply anti-aliasing.
		const float kThickness = 0.125f;
		const float kNormalization = kThickness * .5 * sqrt( 2.0 );
		float afwidth = min( kNormalization * length( grad ), 0.5 );
		opacity = smoothstep( 0.0 - afwidth, 0.0 + afwidth, sig_dist );
		// Apply pre-multiplied alpha with gamma correction.
		//glyph.a = pow( frag_color.a * opacity, 1.0 / 2.2 );
		glyph = vec4(frag_color.rgb, frag_color.a * opacity);
	}

	if (font.shadow_color.a > 0) {
		vec3 u_textureSize = textureSize( texture_sampler, 0 );
		float sd = texture(texture_sampler, vec3(frag_tex_coord.xy - font.shadow_parameters.xy / u_textureSize.xy, 0)).a + font_weight;
		float shadowAlpha = linearstep(0.5 - font.shadow_parameters.z, 0.5 + font.shadow_parameters.z, sd) * font.shadow_color.a;
		shadowAlpha *= 1.0 - opacity * font.shadow_parameters.w;
		vec4 shadow = vec4(font.shadow_color.rgb, shadowAlpha);
		out_color = blend(blend(vec4(0), glyph, 1.0), shadow, frag_color.a);
		out_color.rgb = out_color.rgb * out_color.a;
	} else {
		out_color.rgb = glyph.rgb * glyph.a;
		out_color.a = glyph.a;
	}
}