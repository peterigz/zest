#version 450
#extension GL_EXT_nonuniform_qualifier : require
//This is an all in one billboard vertex shader which can handle both free align and align to camera.
//If you have an effect with multiple emitters using both align methods then you can use this shader to 
//draw the effect with one draw call. Otherwise if your effect only uses one or the other you can optimise by
//using the shaders that only handle whatever you need for the effect.

//#define LOW_UPDATE_RATE

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
    vec2 padding;
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
	uint intensity_gradient_map;			//Multiplier for the color and the gradient map value packed into 16bit scaled floats
	uint curved_alpha_life;					//Sharpness and dissolve amount value for fading the image plus the life of the partile packed into 3 unorms
	uint indexes;							//[color ramp y index, color ramp texture array index, capture flag, image data index (1 bit << 15), billboard alignment (2 bits << 13), image data index max 8191 images]
	uint captured_index;					//Index to the sprite in the buffer from the previous frame for interpolation
    ivec2 padding;
};

layout(set = 0, binding = 0) uniform UboView
{
    mat4 view;
    mat4 proj;
    vec2 screen_size;
    float timer_lerp;
    float update_time;
} ub;

layout (std430, set = 1, binding = 1) readonly buffer InBillboardInstances {
	BillboardInstance data[];
} in_prev_billboards[];

layout (std430, set = 1, binding = 1) readonly buffer InImageData {
	ImageData data[];
} in_image_data[];

layout (push_constant) uniform quad_index
{
    uint particle_texture_index;
    uint color_ramp_texture_index;
    uint image_data_index;
    uint prev_billboards_index;
    uint index_offset;
} pc;

//Vertex
//layout(location = 0) in vec2 vertex_position;

//Instance
layout(location = 0) in vec4 position;
layout(location = 1) in vec3 rotations;
layout(location = 2) in vec3 alignment;
layout(location = 3) in vec4 size_handle;
layout(location = 4) in vec2 intensity_gradient_map;
layout(location = 5) in vec3 curved_alpha_life;
layout(location = 6) in uint texture_indexes;
layout(location = 7) in uint captured_index;

layout(location = 0) out vec3 out_tex_coord;
layout(location = 1) out ivec3 out_texture_indexes;
layout(location = 2) out vec4 out_intensity_curved_alpha_map;

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

	uint prev_index = (captured_index & 0x0FFFFFFF) + pc.index_offset;  //Add on an offset if the are multiple draw instructions
    //We won't interpolate the position of the particle for a few possible reasons:
    //  It's the first frame of the particle and there's no previous frame to interpolate with
    //  We're looping back to the start of a path or line
    //  It maybe cancelled becuase too much time passed since the last frame
	float interpolate_is_active = float((texture_indexes & 0x00008000) >> 15);

	uint prev_size_packed = in_prev_billboards[pc.prev_billboards_index].data[prev_index].size_handle.x;

    #ifdef LOW_UPDATE_RATE
    //For updating particles at 30 fps or less you can improve the first frame of particles by doing the following ternary operations to effectively cancel the interpolation:
    //define LOW_UPDATE_RATE to compile with this instead.
	vec2 lerped_size = interpolate_is_active == 1 ? vec2(float(prev_size_packed & 0xFFFF) * size_max_value, float((prev_size_packed & 0xFFFF0000) >> 16) * size_max_value) : size;
	lerped_size = mix(lerped_size, size, ub.lerp.x);
	vec3 lerped_position = interpolate_is_active == 1 ? mix(in_prev_billboards[pc.prev_billboards_index].data[prev_index].position.xyz, position.xyz, ub.lerp.x) : position.xyz;
	vec3 lerped_rotation = interpolate_is_active == 1 ? mix(in_prev_billboards[pc.prev_billboards_index].data[prev_index].rotations, rotations, ub.lerp.x) : rotations;
    #else
    //Otherwise just hide the first frame of the particle which is a little more efficient:
	vec2 lerped_size = vec2(float(prev_size_packed & 0xFFFF) * size_max_value, float((prev_size_packed & 0xFFFF0000) >> 16) * size_max_value);
	lerped_size = mix(lerped_size, size, ub.timer_lerp) * interpolate_is_active;
	vec3 lerped_position = mix(in_prev_billboards[pc.prev_billboards_index].data[prev_index].position.xyz, position.xyz, ub.timer_lerp);
	vec3 lerped_rotation = mix(in_prev_billboards[pc.prev_billboards_index].data[prev_index].rotations, rotations, ub.timer_lerp);
    #endif

    vec3 motion = position.xyz - in_prev_billboards[pc.prev_billboards_index].data[prev_index].position.xyz;
    motion.z += 0.000001;
    float travel_distance = length(motion); // Calculate the actual distance traveled
	bool has_alignment = dot(alignment, alignment) > 0;
    float stretch_factor = position.w * interpolate_is_active;

    motion = normalize(motion); // Normalize for direction
    vec3 final_alignment = has_alignment ? alignment : motion; // Use normalized motion or specified alignment
    //----

    //Info about how to align the billboard is stored in bits 22 and 23 of intensity_texture_array

    //Billboarding determines whether the quad will align to the camera or not. 0 means that it will 
    //align to the camera. This value is determined by the first bit: 01
    bool billboarding = (texture_indexes & uint(0x2000)) > 0;

    //align_type is set to 1 when we want the quad to align to the vector stored in alignment.xyz with
    //no billboarding. Billboarding will always be set to 1 in this case, so in other words both bits
    //will be set: 11.
    bool align_type = (texture_indexes & uint(0x6000)) == 0x6000;

    //vector_align is set to 1 when we want the billboard to align to the camera and the vector
    //stored in alignment.xyz. billboarding and align_type will always be 0 if this is the case. the second
    //bit is the only bit set if this is the case: 10
    bool vector_align = (texture_indexes & uint(0x6000)) == 0x4000;

    //vec3 alignment_up_cross = dot(alignment.xyz, up) == 0 ? vec3(0, 1, 0) : normalize(cross(alignment.xyz, up));
    vec3 alignment_up_cross = normalize(cross(final_alignment, up));

    vec2 uvs[4];
	uint image_index = texture_indexes & 0x00001FFF;
	vec4 uv = in_image_data[pc.image_data_index].data[image_index].uv;
    uvs[0].x = uv.x; uvs[0].y = uv.y;
    uvs[1].x = uv.z; uvs[1].y = uv.y;
    uvs[2].x = uv.x; uvs[2].y = uv.w;
    uvs[3].x = uv.z; uvs[3].y = uv.w;

    //Calculate components needed for vector_align roll
    vec3 camera_relative_aligment = final_alignment * inverse(mat3(ub.view));
    // Use vec2 for dot/determinant calculation components
    vec2 align_xy = normalize(camera_relative_aligment.xy + vec2(0,0.00001)); // Add epsilon, normalize
    vec2 up_xy = vec2(0, 1); 

    //Before I was using atan to calculate the angle from the particle and the camera which is quite expensive.
    //Instead we use dot and cross product to get the sin and cos values
    //Cosine of the angle between align_xy and up_xy
    float c_vec = dot(align_xy, up_xy);
    // Sine of the angle (using 2D cross product magnitude / determinant)
    float s_vec = align_xy.x * up_xy.y - align_xy.y * up_xy.x; 

    // We have cos(angle_vec) = c_vec, sin(angle_vec) = s_vec without atan
    // Get sin/cos for the instance's Z rotation
    float c_inst = cos(lerped_rotation.z);
    float s_inst = sin(lerped_rotation.z);

    // Combine sin/cos using angle addition formulas if vector_align is 1
    // cos(A+B) = cosA*cosB - sinA*sinB
    // sin(A+B) = sinA*cosB + cosA*sinB
    float c_combined = c_vec * c_inst - s_vec * s_inst;
    float s_combined = s_vec * c_inst + c_vec * s_inst;

    // Select the final cos/sin for the roll based on vector_align flag
    float final_c_roll = !vector_align ? c_inst : c_combined;
    float final_s_roll = !vector_align ? s_inst : s_combined;

    const vec3 identity_bounds[4] = vec3[4](
        vec3( lerped_size.x * (0 - handle.x), -lerped_size.y * (0 - handle.y), 0),
        vec3( lerped_size.x * (1 - handle.x), -lerped_size.y * (0 - handle.y), 0),
        vec3( lerped_size.x * (0 - handle.x), -lerped_size.y * (1 - handle.y), 0),
        vec3( lerped_size.x * (1 - handle.x), -lerped_size.y * (1 - handle.y), 0)
    );

    vec3 bounds[4];
    bounds[0] = align_type ? ((-lerped_size.y * final_alignment * (0 - handle.y)) + (lerped_size.x * alignment_up_cross * (0 - handle.x))) : identity_bounds[0];
    bounds[1] = align_type ? ((-lerped_size.y * final_alignment * (0 - handle.y)) + (lerped_size.x * alignment_up_cross * (1 - handle.x))) : identity_bounds[1];
    bounds[2] = align_type ? ((-lerped_size.y * final_alignment * (1 - handle.y)) + (lerped_size.x * alignment_up_cross * (0 - handle.x))) : identity_bounds[2];
    bounds[3] = align_type ? ((-lerped_size.y * final_alignment * (1 - handle.y)) + (lerped_size.x * alignment_up_cross * (1 - handle.x))) : identity_bounds[3];

    int index = indexes[gl_VertexIndex];

    vec3 vertex_position = bounds[index];

    // --- Rotation Matrix Construction ---

    vec3 roll_axis  = !align_type ? front : final_alignment;
    vec3 pitch_axis = !align_type ? left : alignment_up_cross;

    //Do some profiling on the following. Normalize + cross is expensive so maybe the branch is worth it, but maybe not...
    //vec3 yaw_axis = mix(up, normalize(cross(bounds[1] - bounds[0], bounds[2] - bounds[0]), align_type); 
    // Yaw axis depends on whether we are world-aligned (using surface normal) or not (using world up)
    // Calculate surface normal ONLY if needed
    vec3 yaw_axis = up; // Default to world up
    if (align_type) {
        // Calculate surface normal from world-aligned bounds
        vec3 surface_normal = normalize(cross(bounds[1] - bounds[0], bounds[2] - bounds[0]));
        yaw_axis = surface_normal;
    }

    // Construct Roll matrix directly using final sin/cos
    mat3 matrix_roll;
    vec3 axis = normalize(roll_axis); // Ensure axis is normalized
    float c = final_c_roll;
    float s = final_s_roll;
    float oc = 1.0 - c;
    matrix_roll = mat3(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
                       oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
                       oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c);

    // Construct Pitch and Yaw matrices (apply only if billboarding is off)
    // Angle is scaled by billboarding flag (0 if camera-aligned, 1 otherwise)
	float pitch = billboarding ? lerped_rotation.x : 0;
	float yaw   = billboarding ? lerped_rotation.y : 0;
    mat3 matrix_pitch = RotationMatrix(pitch_axis, pitch);
    mat3 matrix_yaw   = RotationMatrix(yaw_axis,   yaw);

    // Combine rotations
    mat3 rot_mat = matrix_pitch * matrix_yaw * matrix_roll;

    mat4 model = mat4(1.0);
    model[3].xyz = lerped_position;

    mat4 modelView = ub.view * model;
    vec3 pos = rot_mat * vertex_position;

    //Calculate the amount to stretch the particles. A stretch value (set in the editor) of 1 means that the particle is stretched
    //by the amount travelled.

    //Get the direction to stretch based on the billboarding option
	vec3 stretch_direction = billboarding ? final_alignment : normalize(camera_relative_aligment); // Assume inputs allow normalization
    //Half the amount in order to apply half of the stretch in the forward direction and the other half behind
	float vertex_offset_magnitude = 0.5 * travel_distance * stretch_factor;
    //See which vertex this is: Front or back
	float front_back_sign = sign(dot(pos, stretch_direction));
    //Calculate the final stretch amount for this vertex
	vec3 potential_offset = stretch_direction * front_back_sign * vertex_offset_magnitude;

    //Apply the stretch if applicable
	bool apply_stretch = (travel_distance > 0.00001 && stretch_factor > 0.0);
	pos += apply_stretch ? potential_offset : vec3(0.0);

    //Billboarding. If billboarding = 0 then billboarding is active and the quad will always face the camera, 
    //otherwise the modelView matrix is used as it is.
    modelView[0].xyz = !billboarding ? vec3(1, 0, 0) : modelView[0].xyz;
    modelView[1].xyz = !billboarding ? vec3(0, 1, 0) : modelView[1].xyz;
    modelView[2].xyz = !billboarding ? vec3(0, 0, 1) : modelView[2].xyz;

    vec4 p = modelView * vec4(pos, 1.0);
    gl_Position = ub.proj * p;

    //----------------
	int life = int(curved_alpha_life.z * 255);
	out_texture_indexes = ivec3((texture_indexes & 0xFF000000) >> 24, (texture_indexes & 0x00FF0000) >> 16, life);
	out_tex_coord = vec3(uvs[index], in_image_data[pc.image_data_index].data[image_index].texture_array_index);
	out_intensity_curved_alpha_map = vec4(intensity_gradient_map.x * intensity_max_value, curved_alpha_life.x, curved_alpha_life.y, intensity_gradient_map.y * intensity_max_value);
}
