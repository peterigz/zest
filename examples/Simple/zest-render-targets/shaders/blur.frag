#version 450

layout (set = 0, binding = 0) uniform sampler2D sampler_color;

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 out_color;

layout(push_constant) uniform frag_pushes
{
	vec4 settings;
	vec2 texture_size;
	vec2 padding;
} pc;

void main(void)
{
	vec2 src_texel_size = 1.0 / pc.texture_size;
	float x = src_texel_size.x;
	float y = src_texel_size.y;
	float lod = pc.settings.w;

	// Take 13 samples around current texel:
	// a - b - c
	// - j - k -
	// d - e - f
	// - l - m -
	// g - h - i
	// === ('e' is the current texel) ===
	vec3 a = textureLod(sampler_color, vec2(uv.x - 2*x, uv.y + 2*y), lod).rgb;
	vec3 b = textureLod(sampler_color, vec2(uv.x,       uv.y + 2*y), lod).rgb;
	vec3 c = textureLod(sampler_color, vec2(uv.x + 2*x, uv.y + 2*y), lod).rgb;

	vec3 d = textureLod(sampler_color, vec2(uv.x - 2*x, uv.y), lod).rgb;
	vec3 e = textureLod(sampler_color, vec2(uv.x,       uv.y), lod).rgb;
	vec3 f = textureLod(sampler_color, vec2(uv.x + 2*x, uv.y), lod).rgb;

	vec3 g = textureLod(sampler_color, vec2(uv.x - 2*x, uv.y - 2*y), lod).rgb;
	vec3 h = textureLod(sampler_color, vec2(uv.x,       uv.y - 2*y), lod).rgb;
	vec3 i = textureLod(sampler_color, vec2(uv.x + 2*x, uv.y - 2*y), lod).rgb;

	vec3 j = textureLod(sampler_color, vec2(uv.x - x, uv.y + y), lod).rgb;
	vec3 k = textureLod(sampler_color, vec2(uv.x + x, uv.y + y), lod).rgb;
	vec3 l = textureLod(sampler_color, vec2(uv.x - x, uv.y - y), lod).rgb;
	vec3 m = textureLod(sampler_color, vec2(uv.x + x, uv.y - y), lod).rgb;

	// Apply weighted distribution:
	// 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
	// a,b,d,e * 0.125
	// b,c,e,f * 0.125
	// d,e,g,h * 0.125
	// e,f,h,i * 0.125
	// j,k,l,m * 0.5
	// This shows 5 square areas that are being sampled. But some of them overlap,
	// so to have an energy preserving downsample we need to make some adjustments.
	// The weights are the distributed, so that the sum of j,k,l,m (e.g.)
	// contribute 0.5 to the final color output. The code below is written
	// to effectively yield this sum. We get:
	// 0.125*5 + 0.03125*4 + 0.0625*4 = 1
	vec3 downsample = e*0.125;
	downsample += (a+c+g+i)*0.03125;
	downsample += (b+d+f+h)*0.0625;
	downsample += (j+k+l+m)*0.125;
	out_color = vec4(downsample, 1.0);
}



