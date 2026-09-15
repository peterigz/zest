#version 450

layout (location = 0) out vec2 out_uv;

layout (push_constant) uniform push {
	vec4 rect;		//x, y, width, height in normalised device coordinates
	vec4 tint;
	uint sampler_index;
	uint image_index;
} pc;

//No vertex buffer is bound. A 4 vertex triangle strip draw builds the quad from gl_VertexIndex.
void main() {
	vec2 corner = vec2(float(gl_VertexIndex & 1), float(gl_VertexIndex >> 1));
	out_uv = corner;
	gl_Position = vec4(pc.rect.xy + corner * pc.rect.zw, 0.0, 1.0);
}
