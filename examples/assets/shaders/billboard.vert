#version 450

layout(set = 0, binding = 0) uniform UboView {
    mat4 view;
    mat4 proj;
    vec2 screen_size;
    float timer_lerp;
    float update_time;
} uboView;

layout(push_constant) uniform Push {
    uint texture_index;
    uint sample_index;
} pc;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 alignment;
layout(location = 2) in vec4 rotations_stretch;   // x=pitch, y=yaw, z=roll, w=stretch
layout(location = 3) in vec4 uv;
layout(location = 4) in vec4 scale_handle;
layout(location = 5) in uint intensity_texture_array;
layout(location = 6) in vec4 in_color;

layout(location = 0) out vec4 out_frag_color;
layout(location = 1) out vec3 out_tex_coord;

const int indexes[6] = int[](0, 1, 2, 2, 1, 3);

void main()
{
    float intensity = float(intensity_texture_array & 0x003FFFFFu) / 524287.875;
    uint mode = (intensity_texture_array >> 22) & 3u;

    bool full_billboard  = mode == 0u;
    bool axial_billboard = mode == 2u;
    bool vector_aligned  = mode == 1u || mode == 2u;

    vec2 scale  = scale_handle.xy * (256.0 / 32767.0);
    vec2 handle = scale_handle.zw * (128.0 / 32767.0);

    // Correct corner indexing
    int cornerIndex = indexes[gl_VertexIndex];  // 0,1,2,2,1,3 → maps to quad corners
    vec2 corner = vec2(cornerIndex & 1, (cornerIndex >> 1) & 1);  // 0..1
    vec2 localPos2D = (corner - handle) * scale;

    // Fix UVs using proper index
    vec2 uvs[4] = vec2[](
        vec2(uv.x, uv.y),
        vec2(uv.z, uv.y),
        vec2(uv.x, uv.w),
        vec2(uv.z, uv.w)
    );
    int uvIndex = indexes[gl_VertexIndex];
    out_tex_coord = vec3(uvs[uvIndex], float(intensity_texture_array >> 24));

    // Local offset — flip Y here so +Y in texture = up on screen
    vec3 localOffset = vec3(localPos2D.x, -localPos2D.y, 0.0);  // ← THIS FIXES UPSIDE-DOWN

    // === Apply local rotations (roll/pitch/yaw) ===
    mat3 rot = mat3(1.0);

    if (rotations_stretch.z != 0.0) {
        float a = rotations_stretch.z;
        float c = cos(a), s = sin(a);
        rot *= mat3(c, -s, 0, s,  c, 0, 0, 0, 1);
    }
    if (rotations_stretch.x != 0.0 && !full_billboard) {
        float a = rotations_stretch.x;
        float c = cos(a), s = sin(a);
        rot *= mat3(1, 0, 0, 0, c, -s, 0, s, c);
    }
    if (rotations_stretch.y != 0.0 && !full_billboard) {
        float a = rotations_stretch.y;
        float c = cos(a), s = sin(a);
        rot *= mat3(c, 0, s, 0, 1, 0, -s, 0, c);
    }

    localOffset = rot * localOffset;

    // === BILLBOARDING ===
    vec3 finalOffset;

    if (vector_aligned) {
        vec3 alignNorm = normalize(alignment);
        vec3 right = normalize(cross(alignNorm, vec3(0,1,0)));
        if (dot(right, right) < 0.01) right = vec3(1,0,0);
        vec3 up = cross(right, alignNorm);

        if (axial_billboard) {
            // Cylindrical: project camera direction onto plane perpendicular to alignment
            vec3 toCam = normalize((inverse(uboView.view) * vec4(0,0,0,1)).xyz - position);
            vec3 proj = normalize(toCam - dot(toCam, alignNorm) * alignNorm);
            vec3 camRight = proj;
            vec3 camUp = cross(camRight, alignNorm);
            finalOffset = localOffset.x * camRight + localOffset.y * camUp;
        } else {
            finalOffset = localOffset.x * right + localOffset.y * up;
        }
    } else {
        // Standard spherical billboard
        vec3 camRight = vec3(uboView.view[0][0], uboView.view[1][0], uboView.view[2][0]);
        vec3 camUp    = vec3(uboView.view[0][1], uboView.view[1][1], uboView.view[2][1]);

        finalOffset = camRight * localOffset.x + camUp * localOffset.y;
    }

    // Optional stretch
    if (rotations_stretch.w != 0.0 && vector_aligned) {
        vec3 dir = normalize(alignment);
        finalOffset += dir * dot(localOffset, dir) * rotations_stretch.w;
    }

    vec3 worldPos = position + finalOffset;
    gl_Position = uboView.proj * uboView.view * vec4(worldPos, 1.0);

    out_frag_color = in_color * intensity;
}