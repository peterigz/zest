#version 450
#extension GL_ARB_separate_shader_objects : enable

//Quad indexes
const int indexes[6] = {0, 1, 2, 2, 1, 3};
const float scale_max_value = 4096.0 / 32767.0;

//Instance
layout(location = 0) in vec2 in_position;
layout(location = 1) in vec4 uv;
layout(location = 2) in vec4 color;
layout(location = 3) in vec2 scale;
layout(location = 4) in uint texture_array_index;

layout(location = 0) out vec3 out_tex_coord;
layout(location = 1) out vec4 out_color;

layout(push_constant) uniform push_constants {
	vec4 transform;
    vec4 shadow_color;
    vec2 shadow_offset; // In screen pixels
	vec2 unit_range;
	float in_bias;
	float out_bias;
	float smoothness;
	float gamma;
	uint sampler_index;
	uint image_index;
	uint binding_number;  // 1 for texture2D, 3 for texture2DArray
} font;

void main() {
    vec2 size = scale * scale_max_value;

	vec2 uvs[4];
	uvs[0].x = uv.x ; uvs[0].y = uv.y;
	uvs[1].x = uv.z ; uvs[1].y = uv.y;
	uvs[2].x = uv.x ; uvs[2].y = uv.w;
	uvs[3].x = uv.z ; uvs[3].y = uv.w;

	vec2 bounds[4];
	bounds[0].x = 0;
	bounds[0].y = 0;
	bounds[3].x = size.x;
	bounds[3].y = size.y;
	bounds[1].x = bounds[3].x;
	bounds[1].y = bounds[0].y;
	bounds[2].x = bounds[0].x;
	bounds[2].y = bounds[3].y;

	int index = indexes[gl_VertexIndex];

	vec2 vertex_position = bounds[index] + in_position;
	gl_Position = vec4(vertex_position * font.transform.xy + font.transform.zw, 0.0, 1.0);

	//----------------
	out_tex_coord = vec3(uvs[index], texture_array_index);
	out_color = color;
}