#version 450

layout(location = 0) out vec3 out_color;

//No vertex buffer is bound. The position and color are looked up from constant
//arrays using the vertex index, so a 3 vertex draw is all that's needed.
void main() {
	vec2 positions[3] = vec2[3](vec2(0.0, -0.6), vec2(0.6, 0.6), vec2(-0.6, 0.6));
	vec3 colors[3] = vec3[3](vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));

	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	out_color = colors[gl_VertexIndex];
}
