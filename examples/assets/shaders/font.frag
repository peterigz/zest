#version 450 core
#extension GL_OES_standard_derivatives : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_samplerless_texture_functions : enable

precision mediump float;

layout(location = 0) in vec3 frag_tex_coord;
layout(location = 1) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler samplers[];
layout(set = 0, binding = 1) uniform texture2D textures[];
layout(set = 0, binding = 3) uniform texture2DArray images[];

layout(push_constant) uniform push_constants {
	vec4 transform;
    vec4 shadow_color;
    vec2 shadow_offset; // In screen pixels
	vec2 unit_range;
	float in_bias;
	float out_bias;
	float smoothness;
	float gamma;
	uint sampler_index;
	uint image_index;
	uint binding_number;  // 1 for texture2D, 3 for texture2DArray. Changes if the font texture is on more than one layer
} font;

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

vec3 sampleFontTexture(vec2 uv) {
    if (font.binding_number == 1) {
        return texture(sampler2D(textures[nonuniformEXT(font.image_index)],
                       samplers[nonuniformEXT(font.sampler_index)]), uv).rgb;
    } else {
        return texture(sampler2DArray(images[nonuniformEXT(font.image_index)],
                       samplers[nonuniformEXT(font.sampler_index)]),
                       vec3(uv, frag_tex_coord.z)).rgb;
    }
}

vec2 getTextureSize() {
    if (font.binding_number == 1) {
        return vec2(textureSize(textures[font.image_index], 0));
    } else {
        return textureSize(images[font.image_index], 0).xy;
    }
}

void main() {
    // 1. Calculate shadow alpha
    vec2 texture_size = getTextureSize();
    vec2 shadow_uv_offset = font.shadow_offset / texture_size;
    vec3 shadow_msd = sampleFontTexture(frag_tex_coord.xy - shadow_uv_offset);
    float shadow_sd = median(shadow_msd.r, shadow_msd.g, shadow_msd.b);
    float shadow_alpha = contour(shadow_sd) * font.shadow_color.a;

    // 2. Calculate fill alpha
    vec3 fill_msd = sampleFontTexture(frag_tex_coord.xy);
    float fill_sd = median(fill_msd.r, fill_msd.g, fill_msd.b);
    float fill_alpha = contour(fill_sd);

    // 3. Correctly composite the text (foreground) over the shadow (background)
    vec4 Fg = vec4(frag_color.rgb, fill_alpha);
    vec4 Bg = vec4(font.shadow_color.rgb, shadow_alpha);

    // Standard "A over B" alpha compositing
    float final_alpha = Fg.a + Bg.a * (1.0 - Fg.a);
    vec3 final_rgb = (Fg.rgb * Fg.a + Bg.rgb * Bg.a * (1.0 - Fg.a)) / final_alpha;
    
    // Avoid division by zero
    if (final_alpha == 0.0) {
        final_rgb = vec3(0.0);
    }

    // Apply gamma correction to the final combined shape
    final_alpha = pow(final_alpha, 1.0 / font.gamma);

    // Output pre-multiplied color for correct blending in the pipeline
    out_color = vec4(final_rgb * final_alpha, final_alpha);
}