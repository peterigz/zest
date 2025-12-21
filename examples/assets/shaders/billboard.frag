#version 450
#extension GL_EXT_nonuniform_qualifier : require

//----------------------
//2d/3d billboard fragment shader
//----------------------

layout(location = 0) in vec4 in_frag_color;
layout(location = 1) in vec3 in_tex_coord;
layout(location = 0) out vec4 outColor;

layout(binding = 3) uniform texture2DArray images[];
layout(binding = 0) uniform sampler samplers[];

layout(push_constant) uniform quad_index
{
    uint texture_index;
    uint sampler_index;
    uint uniform_index;
} pc;

void main() {
	vec4 texel = texture(sampler2DArray(images[nonuniformEXT(pc.texture_index)], samplers[nonuniformEXT(pc.sampler_index)]), in_tex_coord);
    //Pre multiply alpha
    outColor.rgb = texel.rgb * in_frag_color.rgb * texel.a;
    //If in_frag_color.a is 0 then color will be additive. The higher the value of a the more alpha blended the color will be.
    outColor.a = texel.a * in_frag_color.a;
}