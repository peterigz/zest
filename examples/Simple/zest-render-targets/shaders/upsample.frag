#version 450

layout (set = 0, binding = 0) uniform sampler2D upsample_source_image;	//this will be the mip_level + 1
layout (set = 0, binding = 1) uniform sampler2D downsampler;	//Use textureLod to fetch the mip we want to sample from

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 out_color;

layout(push_constant) uniform frag_pushes
{
	vec4 settings;      //downsampler mip level in w. Filter radius in x
	vec2 texture_size;
	vec2 padding;
} pc;

void main(void)
{
    // The filter kernel is applied with a radius, specified in texture
    // coordinates, so that the radius will vary across mip resolutions.
    float x_offset_uv = 1.0 / pc.texture_size.x;
    float y_offset_uv = 1.0 / pc.texture_size.y;

    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(upsample_source_image, vec2(uv.x - x_offset_uv, uv.y + y_offset_uv)).rgb;
    vec3 b = texture(upsample_source_image, vec2(uv.x,     uv.y + y_offset_uv)).rgb;
    vec3 c = texture(upsample_source_image, vec2(uv.x + x_offset_uv, uv.y + y_offset_uv)).rgb;

    vec3 d = texture(upsample_source_image, vec2(uv.x - x_offset_uv, uv.y)).rgb;
    vec3 e = texture(upsample_source_image, vec2(uv.x,     uv.y)).rgb;
    vec3 f = texture(upsample_source_image, vec2(uv.x + x_offset_uv, uv.y)).rgb;

    vec3 g = texture(upsample_source_image, vec2(uv.x - x_offset_uv, uv.y - y_offset_uv)).rgb;
    vec3 h = texture(upsample_source_image, vec2(uv.x,     uv.y - y_offset_uv)).rgb;
    vec3 i = texture(upsample_source_image, vec2(uv.x + x_offset_uv, uv.y - y_offset_uv)).rgb;

    vec4 downsample = textureLod(downsampler, uv, pc.settings.w);
    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    vec3 upsample = e*4.0;
    upsample += (b+d+f+h)*2.0;
    upsample += (a+c+g+i);
    upsample *= 1.0 / 16.0;
    out_color = vec4(upsample + downsample.rgb, 1.0);
}



