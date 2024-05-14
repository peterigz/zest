#version 450
#extension GL_ARB_separate_shader_objects : enable

//Quad indexes
const int indexes[6] = {0, 1, 2, 0, 2, 3};
const vec3 vertices[4] = {	{0, -.5, 0}, 
							{0, -.5, 1},
							{0,  .5, 1},
							{0,  .5, 0},
						};

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
} pc;

//Instance
layout(location = 0) in vec4 start;
layout(location = 1) in vec4 end;
layout(location = 2) in vec4 line_color;

layout(location = 0) out vec4 out_frag_color;
layout(location = 1) out vec4 p1;
layout(location = 2) out vec4 p2;
layout(location = 3) out vec3 out_end;
layout(location = 4) out float millisecs;
layout(location = 5) out float res;

void main() {
	vec4 clip0 = uboView.proj * uboView.view * pc.model * vec4(start.xyz, 1.0);
	vec4 clip1 = uboView.proj * uboView.view * pc.model * vec4(end.xyz, 1.0);

	vec2 screen0 = uboView.res * (0.5 * clip0.xy/clip0.w + 0.5);
	vec2 screen1 = uboView.res * (0.5 * clip1.xy/clip1.w + 0.5);

	int index = indexes[gl_VertexIndex];
	vec3 vertex = vertices[index];

	vec2 line = screen1 - screen0;
	vec2 normal = normalize(vec2(-line.y, line.x));
	vec2 pt0 = screen0 + (start.w * 1.25) * (vertex.x * line + vertex.y * normal);
	vec2 pt1 = screen1 + (end.w * 1.25) * (vertex.x * line + vertex.y * normal);
	vec2 vertex_position = mix(pt0, pt1, vertex.z);
	vec4 clip = mix(clip0, clip1, vertex.z);
	gl_Position = vec4(clip.w * ((2.0 * vertex_position) / uboView.res - 1.0), clip.z, clip.w);

	//----------------
	out_frag_color = line_color;
	p1 = vec4(screen0, 0,  start.w);
	p2 = vec4(screen1, 0,  end.w);
	out_end = p2.xyz;
	millisecs = uboView.millisecs;
	res = 1.0 / uboView.res.y;
}