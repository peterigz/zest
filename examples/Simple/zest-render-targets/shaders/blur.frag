#version 450

layout (set = 0, binding = 0) uniform sampler2DArray samplerColor;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout(push_constant) uniform frag_pushes
{
    vec4 blur;
	vec2 texture_size;
} settings;

void main(void)
{
	float weight[5];
	weight[0] = 0.227027;
	weight[1] = 0.1945946;
	weight[2] = 0.1216216;
	weight[3] = 0.054054;
	weight[4] = 0.016216;

	vec3 uv = vec3(inUV, 0);
	vec3 tex_offset = vec3(1.0 / settings.texture_size * settings.blur.x, 0); // gets size of single texel
	vec3 result = texture(samplerColor, uv).rgb * weight[0]; // current fragment's contribution
	for(int i = 1; i < 5; ++i)
	{
		if (settings.blur.z == 0)
		{
			// H
			result += texture(samplerColor, uv + vec3(tex_offset.x * i, 0.0, 0.0)).rgb * weight[i] * settings.blur.y;
			result += texture(samplerColor, uv - vec3(tex_offset.x * i, 0.0, 0.0)).rgb * weight[i] * settings.blur.y;
		}
		else
		{
			// V
			result += texture(samplerColor, uv + vec3(0.0, tex_offset.y * i, 0.0)).rgb * weight[i] * settings.blur.y;
			result += texture(samplerColor, uv - vec3(0.0, tex_offset.y * i, 0.0)).rgb * weight[i] * settings.blur.y;
		}
	}
	outColor = vec4(result, 1.0);
}



