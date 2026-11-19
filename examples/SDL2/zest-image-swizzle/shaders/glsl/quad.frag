#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout (binding = 0) uniform sampler samplers[];
layout (binding = 1) uniform texture2D textures_2d[];

layout (location = 0) in vec2 in_uv;

layout (location = 0) out vec4 out_color;

layout (push_constant) uniform push {
	vec4 rect;
	vec4 tint;
	uint sampler_index;
	uint image_index;
} pc;

//Both quads sample an r8_unorm image. The shader is identical for each - the only difference is
//the component mapping baked into the image view that pc.image_index points at.
void main() {
	vec4 texel = texture(sampler2D(textures_2d[pc.image_index], samplers[pc.sampler_index]), in_uv);
	//The pipeline blends premultiplied alpha, so scale rgb by the final alpha
	float alpha = texel.a * pc.tint.a;
	out_color = vec4(texel.rgb * pc.tint.rgb * alpha, alpha);
}
