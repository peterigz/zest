#version 450
#extension GL_ARB_separate_shader_objects : enable

//Quad indexes
const int indexes[6] = {0, 1, 2, 2, 1, 3};

layout (binding = 0) uniform UboView 
{
	mat4 view;
	mat4 proj;
	vec4 parameters1;
	vec4 parameters2;
	vec2 res;
	uint millisecs;
} uboView;

layout(push_constant) uniform quad_index
{
    mat4 model;
	mat4 view;
	mat4 proj;
    vec4 parameters1;
	vec4 parameters2;
	vec4 camera;
	uint flags;
} pc;

//Vertex
//layout(location = 0) in vec2 vertex_position;

//Quad vertex positions
layout(location = 0) in vec4 vertex;

//Line instance data
layout(location = 1) in vec4 start;
layout(location = 3) in vec4 start_color;
layout(location = 2) in vec4 end;
layout(location = 4) in vec4 end_color;

layout(location = 0) out vec4 out_frag_color;
layout(location = 1) out vec4 p1;
layout(location = 2) out vec4 p2;

void main() {

	vec2 line = end.xy - start.xy;
	vec2 normal = normalize(vec2(-line.y, line.x));
	vec2 vertex_position = start.xy + line * (vertex.x + .5) + normal * start.w * vertex.y;	

	mat4 modelView = uboView.view * pc.model;
	vec3 pos = vec3(vertex_position.x, vertex_position.y, 1);
	gl_Position = uboView.proj * modelView * vec4(pos, 1.0);

	//----------------
	out_frag_color = start_color;
	p1 = start;
	p2 = end;
}