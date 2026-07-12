#version 450

//Per instance data, the layer tests use TestData (a single vec4) as the instance struct
layout(location = 0) in vec4 instance_color;

layout(location = 0) out vec4 out_color;

//Two triangles covering the whole viewport in NDC, expanded from gl_VertexIndex so the
//instance data only has to carry the color that the verify shader checks for
vec2 positions[6] = vec2[](
	vec2(-1.0, -1.0), vec2( 1.0, -1.0), vec2(-1.0,  1.0),
	vec2( 1.0, -1.0), vec2( 1.0,  1.0), vec2(-1.0,  1.0)
);

void main() {
	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	out_color = instance_color;
}
