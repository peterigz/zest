#version 450
#extension GL_ARB_separate_shader_objects : enable

//Quad indexes
const int indexes[6] = {0, 1, 2, 2, 1, 3};
const float scale_max_value = 4096.0 / 32767.0;
const float handle_max_value = 128.0 / 32767.0;

layout (binding = 0) uniform UboView 
{
	mat4 view;
	mat4 proj;
	vec4 parameters1;
	vec4 parameters2;
	vec2 res;
	uint millisecs;
} uboView;

//Vertex
//layout(location = 0) in vec2 vertex_position;

//Instance
layout(location = 0) in vec2 position;
layout(location = 1) in vec4 uv;
layout(location = 2) in vec4 scale_handle;
layout(location = 3) in float rotation;
layout(location = 4) in float intensity;
layout(location = 5) in vec4 in_color;
layout(location = 6) in uint texture_array_index;

layout(location = 0) out vec4 out_frag_color;
layout(location = 1) out vec3 out_tex_coord;

void main() {
    vec2 scale = scale_handle.xy * scale_max_value;
    vec2 handle = scale_handle.zw * handle_max_value;

	vec2 uvs[4];
	uvs[0].x = uv.x ; uvs[0].y = uv.y;
	uvs[1].x = uv.z ; uvs[1].y = uv.y;
	uvs[2].x = uv.x ; uvs[2].y = uv.w;
	uvs[3].x = uv.z ; uvs[3].y = uv.w;

	vec2 bounds[4];
	bounds[0].x = scale.x * (0 - handle.x);
	bounds[0].y = scale.y * (0 - handle.y);
	bounds[3].x = scale.x * (1 - handle.x);
	bounds[3].y = scale.y * (1 - handle.y);
	bounds[1].x = bounds[3].x;
	bounds[1].y = bounds[0].y;
	bounds[2].x = bounds[0].x;
	bounds[2].y = bounds[3].y;

	int index = indexes[gl_VertexIndex];

	vec2 vertex_position = bounds[index];

	mat3 matrix = mat3(1.0);
	float s = sin(rotation);
	float c = cos(rotation);

	matrix[0][0] = c;
	matrix[0][1] = s;
	matrix[1][0] = -s;
	matrix[1][1] = c;

	vec3 pos = matrix * vec3(vertex_position.x, vertex_position.y, 1);
	pos.xy += position;
	gl_Position = uboView.proj * uboView.view * vec4(pos, 1.0);

	//----------------
	out_tex_coord = vec3(uvs[index], texture_array_index);
	out_frag_color = in_color * intensity;
}