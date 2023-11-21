#version 450
#extension GL_ARB_separate_shader_objects : enable

//Quad indexes
const int indexes[6] = {0, 1, 2, 2, 1, 3};
const vec2 vertex[4] = { {-.5, -.5}, {-.5, .5}, {.5, -.5}, {.5, .5} };

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
    vec4 parameters1;
	vec4 parameters2;
	vec4 parameters3;
	vec4 camera;
	uint flags;
} pc;

//Line instance data
layout(location = 0) in vec4 rect;
layout(location = 1) in vec4 parameters;
layout(location = 2) in vec4 start_color;
layout(location = 3) in vec4 end_color;

layout(location = 0) out vec4 out_frag_color;
layout(location = 1) out vec4 p1;
layout(location = 2) out vec4 p2;
layout(location = 3) out float shape_type;

void main() {
	int index = indexes[gl_VertexIndex];
	shape_type = pc.parameters1.x;

	vec2 vertex_position;
	if(shape_type == 6) {
		//line drawing
		vec2 line = rect.zw - rect.xy;
		vec2 normal = normalize(vec2(-line.y, line.x));
		vertex_position = rect.xy + line * (vertex[index].x + .5) + normal * parameters.x * vertex[index].y;	
	} else if(shape_type == 7) {
		//Rect drawing
		vec2 size = rect.zw - rect.xy;
		vec2 position = size * .5 + rect.xy;
		vertex_position = size.xy * vertex[index] + position;	
	}

	mat4 modelView = uboView.view * pc.model;
	vec3 pos = vec3(vertex_position.x, vertex_position.y, 1);
	gl_Position = uboView.proj * modelView * vec4(pos, 1.0);

	//----------------
	out_frag_color = start_color;
	p1 = rect;
	p2 = parameters;
}