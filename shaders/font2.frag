#version 450 core
#extension GL_OES_standard_derivatives : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_samplerless_texture_functions : enable

precision mediump float;

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec3 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler samplers[];
layout(set = 1, binding = 3) uniform texture2DArray images[];

layout(push_constant) uniform quad_index {
	vec2 unit_range;
	float in_bias;
	float out_bias;
	float supersample;
	float smoothness;
	float gamma;
	uint sampler_index;
	uint image_index;
} font;

/* some code from instructions on https://github.com/Chlumsky/msdfgen */
float median(float r, float g, float b) {
  return max(min(r, g), min(max(r, g), b));
}

float screenPxRange() {
  vec2 screenTexSize =  vec2(1.0) / fwidth(frag_tex_coord.xy);
  return max(0.5 * dot(font.unit_range, screenTexSize), 1.0);
}

float contour(float distance) {
  float width = screenPxRange();
  float e = width * (distance - 0.5 + font.in_bias) + 0.5 + font.out_bias;
  return  mix(clamp(e, 0.0, 1.0),
              smoothstep(0.0, 1.0, e),
              font.smoothness);
}

float sample_font(vec3 uvw) {
	vec3 msd = texture(sampler2DArray(images[font.image_index], samplers[font.sampler_index]), uvw).rgb;
	float sd = median(msd.r, msd.g, msd.b);
	float opacity = contour(sd);
	return opacity;
}

void main() {
  float opacity = sample_font(frag_tex_coord);

  /* from https://www.reddit.com/r/gamedev/comments/2879jd/comment/cicatot/ */
  float dscale = 0.354;
  vec2 uv = frag_tex_coord.xy;
  vec2 duv = dscale * (dFdx(uv) + dFdy(uv));
  vec4 box = vec4(uv - duv, uv + duv);
  float asum = sample_font(vec3(box.xy, frag_tex_coord.z))
             + sample_font(vec3(box.zw, frag_tex_coord.z))
             + sample_font(vec3(box.xw, frag_tex_coord.z))
             + sample_font(vec3(box.zy, frag_tex_coord.z));
  opacity = mix(opacity, (opacity + 0.5 * asum) / 3.0, font.supersample);
  opacity = pow(opacity, 1.0 / font.gamma);

  out_color = frag_color * opacity;
}