#version 450
//This is an all in one billboard vertex shader which can handle both free align and align to camera.
//If you have an effect with multiple emitters using both align methods then you can use this shader to 
//draw the effect with one draw call. Otherwise if your effect only uses one or the other you can optimise by
//using the shaders that only handle whatever you need for the effect.

//Quad indexes
const int indexes[6] = int[6]( 0, 1, 2, 2, 1, 3 );

//We add an epsilon to avoid nans with the cross product
const vec3 up = vec3( 0, 1, 0.00001 );
const vec3 front = vec3( 0, 0, 1 );
const vec3 left = vec3( 1, 0, 0 );
const float size_max_value = 256.0 / 32767.0;
const float handle_max_value = 128.0 / 32767.0;
const float intensity_max_value = 128.0 / 32767.0;

struct ImageData {
	vec4 uv;
	uvec2 uv_packed;
	vec2 image_size;
	uint texture_array_index;
	float animation_frames;
	float max_radius;
};

struct BillboardInstance {					//56 bytes + padding to 64
	vec4 position;							//The position of the sprite, x, y - world, z, w = captured for interpolating
	vec3 rotations;				            //Rotations of the sprite
	uint alignment;	    					//normalised alignment vector 2 floats packed into 16bits or 3 8bit floats for 3d
	ivec2 size_handle;						//Size of the sprite in pixels and the handle packed into a u64 (4 16bit floats)
	uint intensity_life;					//Multiplier for the color and life of particle
	uint curved_alpha;						//Sharpness and dissolve amount value for fading the image 2 16bit floats packed
	uint indexes;							//[color ramp y index, color ramp texture array index, capture flag, image data index (1 bit << 15), billboard alignment (2 bits << 13), image data index max 8191 images]
	uint captured_index;					//Index to the sprite in the buffer from the previous frame for interpolation
    ivec2 padding;
};

layout(set = 0, binding = 0) uniform UboView
{
    mat4 view;
    mat4 proj;
    vec4 lerp;
    vec4 parameters2;
    vec2 res;
    uint millisecs;
} ub;

layout(set = 1, binding = 0) readonly buffer InImageData {
	ImageData image_data[];
};

layout(set = 1, binding = 1) readonly buffer InSpriteInstances {
	BillboardInstance prev_billboards[];
};

layout(push_constant) uniform quad_index
{
    mat4 model;
    vec4 parameters1;
    vec4 parameters2;
    vec4 parameters3;
    vec4 camera;
} pc;

//Vertex
//layout(location = 0) in vec2 vertex_position;

//Instance
layout(location = 0) in vec4 position;
layout(location = 1) in vec3 rotations;
layout(location = 2) in vec3 alignment;
layout(location = 3) in vec4 size_handle;
layout(location = 4) in vec2 intensity_life;
layout(location = 5) in vec2 curved_alpha;
layout(location = 6) in uint texture_indexes;
layout(location = 7) in uint captured_index;

layout(location = 0) out vec3 out_tex_coord;
layout(location = 1) out ivec3 out_texture_indexes;
layout(location = 2) out vec3 out_intensity_curved_alpha;

mat3 RotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    return mat3(oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s, oc * axis.z * axis.x + axis.y * s,
        oc * axis.x * axis.y + axis.z * s, oc * axis.y * axis.y + c, oc * axis.y * axis.z - axis.x * s,
        oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s, oc * axis.z * axis.z + c);
}

void main() {
    vec2 size = size_handle.xy * size_max_value;
    vec2 handle = size_handle.zw * handle_max_value;

	uint prev_index = captured_index & 0x0FFFFFFF;
	float first_frame = float((texture_indexes & 0x00008000) >> 15);

	uint prev_size_packed = prev_billboards[prev_index].size_handle.x;
	vec2 lerped_size = vec2(float(prev_size_packed & 0xFFFF) * size_max_value, float((prev_size_packed & 0xFFFF0000) >> 16) * size_max_value);
	lerped_size = mix(lerped_size, size, ub.lerp.x) * first_frame;
	vec3 lerped_position = mix(prev_billboards[prev_index].position.xyz, position.xyz, ub.lerp.x);
	vec3 lerped_rotation = mix(prev_billboards[prev_index].rotations, rotations, ub.lerp.x);

    //Info about how to align the billboard is stored in bits 22 and 23 of intensity_texture_array

    //Billboarding determines whether the quad will align to the camera or not. 0 means that it will 
    //align to the camera. This value is determined by the first bit: 01
    float billboarding = float((texture_indexes & uint(0x2000)) >> 13);

    //align_type is set to 1 when we want the quad to align to the vector stored in alignment.xyz with
    //no billboarding. Billboarding will always be set to 1 in this case, so in other words both bits
    //will be set: 11.
    float align_type = float((texture_indexes & uint(0x6000)) == 0x6000);

    //vector_align is set to 1 when we want the billboard to align to the camera and the vector
    //stored in alignment.xyz. billboarding and align_type will always be 0 if this is the case. the second
    //bit is the only bit set if this is the case: 10
    float vector_align = float((texture_indexes & uint(0x6000)) == 0x2000);

    //vec3 alignment_up_cross = dot(alignment.xyz, up) == 0 ? vec3(0, 1, 0) : normalize(cross(alignment.xyz, up));
    vec3 alignment_up_cross = normalize(cross(alignment, up));

    vec2 uvs[4];
	uint image_index = texture_indexes & 0x00001FFF;
	vec4 uv = image_data[image_index].uv;
    uvs[0].x = uv.x; uvs[0].y = uv.y;
    uvs[1].x = uv.z; uvs[1].y = uv.y;
    uvs[2].x = uv.x; uvs[2].y = uv.w;
    uvs[3].x = uv.z; uvs[3].y = uv.w;

    vec3 camera_relative_aligment = alignment * inverse(mat3(ub.view));
    float dp_up = dot(camera_relative_aligment.xy, -up.xy);
    float det = camera_relative_aligment.x * -up.y;
    float dp_angle = vector_align * atan(-det, -dp_up) + lerped_rotation.z;

    const vec3 identity_bounds[4] = vec3[4](
        vec3( lerped_size.x * (0 - handle.x), -lerped_size.y * (0 - handle.y), 0),
        vec3( lerped_size.x * (1 - handle.x), -lerped_size.y * (0 - handle.y), 0),
        vec3( lerped_size.x * (0 - handle.x), -lerped_size.y * (1 - handle.y), 0),
        vec3( lerped_size.x * (1 - handle.x), -lerped_size.y * (1 - handle.y), 0)
    );

    vec3 bounds[4];
    bounds[0] = align_type == 1 ? ((-lerped_size.y * alignment * (0 - handle.y)) + (lerped_size.x * alignment_up_cross * (0 - handle.x))) : identity_bounds[0];
    bounds[1] = align_type == 1 ? ((-lerped_size.y * alignment * (0 - handle.y)) + (lerped_size.x * alignment_up_cross * (1 - handle.x))) : identity_bounds[1];
    bounds[2] = align_type == 1 ? ((-lerped_size.y * alignment * (1 - handle.y)) + (lerped_size.x * alignment_up_cross * (0 - handle.x))) : identity_bounds[2];
    bounds[3] = align_type == 1 ? ((-lerped_size.y * alignment * (1 - handle.y)) + (lerped_size.x * alignment_up_cross * (1 - handle.x))) : identity_bounds[3];

    int index = indexes[gl_VertexIndex];

    vec3 vertex_position = bounds[index];

    vec3 surface_normal = cross(bounds[1] - bounds[0], bounds[2] - bounds[0]);
    mat3 matrix_roll = RotationMatrix(align_type * alignment + front * (1 - align_type), align_type * lerped_rotation.z + dp_angle * (1 - align_type));
    mat3 matrix_pitch = RotationMatrix(align_type * alignment_up_cross + left * (1 - align_type), lerped_rotation.x * billboarding);
    mat3 matrix_yaw = RotationMatrix(align_type * surface_normal + up * (1 - align_type), lerped_rotation.y * billboarding);

    mat4 model = mat4(1.0);
    model[3][0] = lerped_position.x;
    model[3][1] = lerped_position.y;
    model[3][2] = lerped_position.z;

    mat3 rot_mat = matrix_pitch * matrix_yaw * matrix_roll;

    mat4 modelView = ub.view * model;
    vec3 pos = rot_mat * vertex_position;
    //Stretch effect but negate if billboarding is not active
    pos += camera_relative_aligment * dot(pos, camera_relative_aligment) * position.w * (1 - billboarding);
    pos += alignment * dot(pos, alignment) * position.w * billboarding;

    //Billboarding. If billboarding = 0 then billboarding is active and the quad will always face the camera, 
    //otherwise the modelView matrix is used as it is.
    modelView[0][0] = (billboarding * modelView[0][0]) + 1 * (1 - billboarding);
    modelView[0][1] = (billboarding * modelView[0][1]);
    modelView[0][2] = (billboarding * modelView[0][2]);
    modelView[1][0] = (billboarding * modelView[1][0]);
    modelView[1][1] = (billboarding * modelView[1][1]) + 1 * (1 - billboarding);
    modelView[1][2] = (billboarding * modelView[1][2]);
    modelView[2][0] = (billboarding * modelView[2][0]);
    modelView[2][1] = (billboarding * modelView[2][1]);
    modelView[2][2] = (billboarding * modelView[2][2]) + 1 * (1 - billboarding);

    vec4 p = modelView * vec4(pos, 1.0);
    gl_Position = ub.proj * p;

    //----------------
	int life = int(intensity_life.y * intensity_max_value * 255);
	out_texture_indexes = ivec3((texture_indexes & 0xFF000000) >> 24, (texture_indexes & 0x00FF0000) >> 16, life);
	out_tex_coord = vec3(uvs[index], image_data[image_index].texture_array_index);
	out_intensity_curved_alpha = vec3(intensity_life.x * intensity_max_value, curved_alpha.x, curved_alpha.y);
}
