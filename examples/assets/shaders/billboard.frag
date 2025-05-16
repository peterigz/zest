#version 450

//----------------------
//2d/3d billboard fragment shader
//----------------------

layout(location = 0) in vec4 in_frag_color;
layout(location = 1) in vec3 in_tex_coord;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2DArray texture_sampler;

void main() {
    vec4 texel = texture(texture_sampler, in_tex_coord);
    //Pre multiply alpha
    outColor.rgb = texel.rgb * in_frag_color.rgb * texel.a;
    //If in_frag_color.a is 0 then color will be additive. The higher the value of a the more alpha blended the color will be.
    outColor.a = texel.a * in_frag_color.a;
}