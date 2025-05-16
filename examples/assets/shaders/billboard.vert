#version 450

//----------------------
//3d billboard vertex shader
//----------------------

//Quad indexes
const int indexes[6] = int[6]( 0, 1, 2, 2, 1, 3 );

//We add an epsilon to avoid nans with the cross product
const vec3 up = vec3( 0, 1, 0.00001 );
const vec3 front = vec3( 0, 0, 1 );
const vec3 left = vec3( 1, 0, 0 );
const float scale_max_value = 256.0 / 32767.0;
const float handle_max_value = 128.0 / 32767.0;

layout(set = 0, binding = 0) uniform UboView
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
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 alignment;
layout(location = 2) in vec4 rotations_stretch;
layout(location = 3) in vec4 uv;
layout(location = 4) in vec4 scale_handle;
layout(location = 5) in uint intensity_texture_array;
layout(location = 6) in vec4 in_color;

layout(location = 0) out vec4 out_frag_color;
layout(location = 1) out vec3 out_tex_coord;

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
    //Pluck out the intensity value from 
    float intensity = intensity_texture_array & uint(0x003fffff);
    intensity /= 524287.875;

    vec2 scale = scale_handle.xy * scale_max_value;
    vec2 handle = scale_handle.zw * handle_max_value;

    //Info about how to align the billboard is stored in bits 22 and 23 of intensity_texture_array

    //Billboarding determines whether the quad will align to the camera or not. 0 means that it will 
    //align to the camera. This value is determined by the first bit: 01
    float billboarding = float((intensity_texture_array & uint(0x00400000)) >> 22);

    //align_type is set to 1 when we want the quad to align to the vector stored in alignment.xyz with
    //no billboarding. Billboarding will always be set to 1 in this case, so in other words both bits
    //will be set: 11.
    float align_type = float((intensity_texture_array & uint(0x00C00000)) == 12582912);

    //vector_align is set to 1 when we want the billboard to align to the camera and the vector
    //stored in alignment.xyz. billboarding and align_type will always be 0 if this is the case. the second
    //bit is the only bit set if this is the case: 10
    float vector_align = float((intensity_texture_array & uint(0x00C00000)) == 8388608);

    //vec3 alignment_up_cross = dot(alignment.xyz, up) == 0 ? vec3(0, 1, 0) : normalize(cross(alignment.xyz, up));
    vec3 alignment_up_cross = normalize(cross(alignment, up));

    vec2 uvs[4];
    uvs[0].x = uv.x; uvs[0].y = uv.y;
    uvs[1].x = uv.z; uvs[1].y = uv.y;
    uvs[2].x = uv.x; uvs[2].y = uv.w;
    uvs[3].x = uv.z; uvs[3].y = uv.w;

    vec3 camera_relative_aligment = alignment * inverse(mat3(uboView.view));
    float dp_up = dot(camera_relative_aligment.xy, -up.xy);
    float det = camera_relative_aligment.x * -up.y;
    float dp_angle = vector_align * atan(-det, -dp_up) + rotations_stretch.z;

    const vec3 identity_bounds[4] = vec3[4](
        vec3( scale.x * (0 - handle.x), -scale.y * (0 - handle.y), 0),
        vec3( scale.x * (1 - handle.x), -scale.y * (0 - handle.y), 0),
        vec3( scale.x * (0 - handle.x), -scale.y * (1 - handle.y), 0),
        vec3( scale.x * (1 - handle.x), -scale.y * (1 - handle.y), 0)
    );

    vec3 bounds[4];
    bounds[0] = align_type == 1 ? ((-scale.y * alignment * (0 - handle.y)) + (scale.x * alignment_up_cross * (0 - handle.x))) : identity_bounds[0];
    bounds[1] = align_type == 1 ? ((-scale.y * alignment * (0 - handle.y)) + (scale.x * alignment_up_cross * (1 - handle.x))) : identity_bounds[1];
    bounds[2] = align_type == 1 ? ((-scale.y * alignment * (1 - handle.y)) + (scale.x * alignment_up_cross * (0 - handle.x))) : identity_bounds[2];
    bounds[3] = align_type == 1 ? ((-scale.y * alignment * (1 - handle.y)) + (scale.x * alignment_up_cross * (1 - handle.x))) : identity_bounds[3];

    int index = indexes[gl_VertexIndex];

    vec3 vertex_position = bounds[index];

    vec3 surface_normal = cross(bounds[1] - bounds[0], bounds[2] - bounds[0]);
    mat3 matrix_roll = RotationMatrix(align_type * alignment + front * (1 - align_type), align_type * rotations_stretch.z + dp_angle * (1 - align_type));
    mat3 matrix_pitch = RotationMatrix(align_type * alignment_up_cross + left * (1 - align_type), rotations_stretch.x * billboarding);
    mat3 matrix_yaw = RotationMatrix(align_type * surface_normal + up * (1 - align_type), rotations_stretch.y * billboarding);

    mat4 model = mat4(1.0);
    model[3][0] = position.x;
    model[3][1] = position.y;
    model[3][2] = position.z;

    mat3 rot_mat = matrix_pitch * matrix_yaw * matrix_roll;

    mat4 modelView = uboView.view * model;
    vec3 pos = rot_mat * vertex_position;
    //Stretch effect but negate if billboarding is not active
    pos += camera_relative_aligment * dot(pos, camera_relative_aligment) * rotations_stretch.w * (1 - billboarding);
    pos += alignment * dot(pos, alignment) * rotations_stretch.w * billboarding;

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
    gl_Position = uboView.proj * p;

    //----------------
    out_frag_color = in_color * intensity;
    out_tex_coord = vec3(uvs[index], (intensity_texture_array & uint(0xFF000000)) >> 24);
}