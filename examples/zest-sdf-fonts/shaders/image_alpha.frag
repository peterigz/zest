#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 in_frag_color;
layout(location = 1) in vec3 in_tex_coord;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2DArray texSampler;

layout(push_constant) uniform quad_index
{
    mat4 model;
    vec4 parameters1;
	vec4 parameters2;
	vec4 parameters3;
	vec4 camera;
	uint flags;
} pc;

float adjustSDF(float sdfValue) {
    // Zero level and scaling factor
    const float zeroLevel = pc.parameters1.x;
    const float scaleFactor = 0.5;
    
    // Adjust SDF value around the zero level
    float adjustedSDF = sdfValue - zeroLevel;
    
    // Add 0.5 to account for center pixel opacity
    adjustedSDF += scaleFactor;
    
    return adjustedSDF;
}

float get_uv_scale(vec2 uv) {
	vec2 dx = dFdx(uv);
	vec2 dy = dFdy(uv);
	return (length(dx) + length(dy)) * 0.5;
}

void main() {
	float texel = texture(texSampler, in_tex_coord).r;

    float expand = pc.parameters2.x;
    float bleed = pc.parameters2.y;
    float radius = pc.parameters1.x;	
	float scale = get_uv_scale(in_tex_coord.xy * textureSize(texSampler, 0).xy) * pc.parameters1.z;
    
    float d = (texel - 0.75) * radius;
    float s = (d + expand / pc.parameters1.y) / scale + 0.5 + bleed;
    vec2 sdf = vec2(s, s);
	float mask = clamp(sdf.x, 0.0, 1.0);

	vec4 color = in_frag_color;

	//if (mask == 0.0) { discard; }

	outColor = vec4(color.rgb * mask, color.a * mask);
}
