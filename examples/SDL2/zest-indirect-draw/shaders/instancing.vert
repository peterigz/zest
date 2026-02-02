#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 7) uniform UboView
{
    mat4 proj;
    mat4 view;
	vec4 planes[6];
	vec4 light_pos;
	float locSpeed;
	float globSpeed;
	uint visible;
} ubo[];

layout (push_constant) uniform push {
	uint sampler_index;
	uint planet_index;
	uint rock_index;
	uint skybox_index;
	uint ubo_index;
} pc;

// Vertex attributes
layout (location = 0) in vec3 inPos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec2 inUV;

// Instanced attributes
layout (location = 4) in vec3 instancePos;
layout (location = 5) in vec3 instanceRot;
layout (location = 6) in vec3 instanceScale;
layout (location = 7) in uint instanceTexIndex;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outUV;
layout (location = 3) out vec3 outViewVec;
layout (location = 4) out vec3 outLightVec;

void main() 
{
	outColor = inColor.rgb;
	outUV = vec3(inUV, float(instanceTexIndex));

	mat3 mx, my, mz;
	
	// rotate around x
	float s = sin(instanceRot.x + ubo[pc.ubo_index].locSpeed);
	float c = cos(instanceRot.x + ubo[pc.ubo_index].locSpeed);

	mx[0] = vec3(c, s, 0.0);
	mx[1] = vec3(-s, c, 0.0);
	mx[2] = vec3(0.0, 0.0, 1.0);
	
	// rotate around y
	s = sin(instanceRot.y + ubo[pc.ubo_index].locSpeed);
	c = cos(instanceRot.y + ubo[pc.ubo_index].locSpeed);

	my[0] = vec3(c, 0.0, s);
	my[1] = vec3(0.0, 1.0, 0.0);
	my[2] = vec3(-s, 0.0, c);
	
	// rot around z
	s = sin(instanceRot.z + ubo[pc.ubo_index].locSpeed);
	c = cos(instanceRot.z + ubo[pc.ubo_index].locSpeed);	
	
	mz[0] = vec3(1.0, 0.0, 0.0);
	mz[1] = vec3(0.0, c, s);
	mz[2] = vec3(0.0, -s, c);
	
	mat3 rotMat = mz * my * mx;

	/*
	mat4 gRotMat;
	s = sin(instanceRot.y + ubo[pc.ubo_index].globSpeed);
	c = cos(instanceRot.y + ubo[pc.ubo_index].globSpeed);
	gRotMat[0] = vec4(c, 0.0, s, 0.0);
	gRotMat[1] = vec4(0.0, 1.0, 0.0, 0.0);
	gRotMat[2] = vec4(-s, 0.0, c, 0.0);
	gRotMat[3] = vec4(0.0, 0.0, 0.0, 1.0);	
	*/
	
	vec4 locPos = vec4(inPos.xyz * rotMat, 1.0);
	vec4 pos = vec4((locPos.xyz * instanceScale) + instancePos, 1.0);

	//gl_Position = ubo[pc.ubo_index].proj * ubo[pc.ubo_index].view * gRotMat * pos;
	gl_Position = ubo[pc.ubo_index].proj * ubo[pc.ubo_index].view * pos;
	//outNormal = mat3(ubo[pc.ubo_index].view * gRotMat) * inverse(rotMat) * inNormal;
	outNormal = mat3(ubo[pc.ubo_index].view) * inverse(rotMat) * inNormal;

	pos = ubo[pc.ubo_index].view * vec4(inPos.xyz + instancePos, 1.0);
	vec3 lPos = mat3(ubo[pc.ubo_index].view) * ubo[pc.ubo_index].light_pos.xyz;
	outLightVec = lPos - pos.xyz;
	outViewVec = -pos.xyz;		
}
