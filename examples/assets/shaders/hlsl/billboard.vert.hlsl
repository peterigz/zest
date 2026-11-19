cbuffer zest_push : register(b0, space0) {
	uint texture_index;
	uint sample_index;
	uint uniform_index;
};

struct view_t {
	float4x4 view;
	float4x4 proj;
	float2 screen_size;
	float timer_lerp;
	float update_time;
};

ConstantBuffer<view_t> zest_uniforms[] : register(b0, space107);

struct varyings {
	float4 position : SV_Position;
	float4 frag_color : COLOR0;
	float3 tex_coord : TEXCOORD0;
};

static const int indexes[6] = { 0, 1, 2, 2, 1, 3 };

//GLSL constructors fill columns and HLSL constructors fill rows, so matrices built in the shader go through this
float3x3 columns3(float3 column0, float3 column1, float3 column2) {
	return transpose(float3x3(column0, column1, column2));
}

//The view matrix is a rotation and a translation, so its inverse is the transposed rotation and minus that times the translation
float3 camera_position_from_view(float4x4 view) {
	float3x3 rotation = (float3x3)view;
	float3 translation = float3(view[0][3], view[1][3], view[2][3]);
	return -mul(transpose(rotation), translation);
}

//One instance per billboard, drawn as 6 vertices. Alignment is fetched as 4 snorm channels, of which 3 are read
varyings main(float3 position : POSITION0, float3 alignment : ALIGNMENT0, float4 rotations_stretch : ROTATIONS_STRETCH0, float4 uv : TEXCOORD0,
	int4 scale_handle : SCALE_HANDLE0, uint intensity_texture_array : INTENSITY_TEXTURE_ARRAY0, float4 in_color : COLOR0, uint vertex_index : SV_VertexID) {
	view_t ubo = zest_uniforms[NonUniformResourceIndex(uniform_index)];
	varyings output;

	//rotations_stretch: x = pitch, y = yaw, z = roll, w = stretch
	float intensity = float(intensity_texture_array & 0x003FFFFFu) / 524287.875;
	uint mode = (intensity_texture_array >> 22) & 3u;

	bool full_billboard = mode == 0u;
	bool axial_billboard = mode == 2u;
	bool vector_aligned = mode == 1u || mode == 2u;

	float2 scale = float2(scale_handle.xy) * (256.0 / 32767.0);
	float2 handle = float2(scale_handle.zw) * (128.0 / 32767.0);

	//Corner indexing: 0,1,2,2,1,3 maps to the quad corners
	int corner_index = indexes[vertex_index];
	float2 corner = float2(corner_index & 1, (corner_index >> 1) & 1);
	float2 local_position_2d = (corner - handle) * scale;

	float2 uvs[4] = { float2(uv.x, uv.y), float2(uv.z, uv.y), float2(uv.x, uv.w), float2(uv.z, uv.w) };
	output.tex_coord = float3(uvs[corner_index], float(intensity_texture_array >> 24));

	//Local offset, with y flipped so +y in the texture is up on screen
	float3 local_offset = float3(local_position_2d.x, -local_position_2d.y, 0.0);

	//Apply local rotations (roll, pitch, yaw)
	float3x3 rot = float3x3(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0);

	if (rotations_stretch.z != 0.0) {
		float a = rotations_stretch.z;
		float c = cos(a), s = sin(a);
		rot = mul(rot, columns3(float3(c, -s, 0), float3(s, c, 0), float3(0, 0, 1)));
	}
	if (rotations_stretch.x != 0.0 && !full_billboard) {
		float a = rotations_stretch.x;
		float c = cos(a), s = sin(a);
		rot = mul(rot, columns3(float3(1, 0, 0), float3(0, c, -s), float3(0, s, c)));
	}
	if (rotations_stretch.y != 0.0 && !full_billboard) {
		float a = rotations_stretch.y;
		float c = cos(a), s = sin(a);
		rot = mul(rot, columns3(float3(c, 0, s), float3(0, 1, 0), float3(-s, 0, c)));
	}

	local_offset = mul(rot, local_offset);

	//Billboarding
	float3 final_offset;

	if (vector_aligned) {
		float3 align_norm = normalize(alignment);
		float3 right = normalize(cross(align_norm, float3(0, 1, 0)));
		if (dot(right, right) < 0.01) right = float3(1, 0, 0);
		float3 up = cross(right, align_norm);

		if (axial_billboard) {
			//Cylindrical: project camera direction onto plane perpendicular to alignment
			float3 to_camera = normalize(camera_position_from_view(ubo.view) - position);
			float3 projected = normalize(to_camera - dot(to_camera, align_norm) * align_norm);
			float3 camera_right = projected;
			float3 camera_up = cross(camera_right, align_norm);
			final_offset = local_offset.x * camera_right + local_offset.y * camera_up;
		} else {
			final_offset = local_offset.x * right + local_offset.y * up;
		}
	} else {
		//Standard spherical billboard: the camera's right and up are the view matrix's first two rows
		float3 camera_right = float3(ubo.view[0][0], ubo.view[0][1], ubo.view[0][2]);
		float3 camera_up = float3(ubo.view[1][0], ubo.view[1][1], ubo.view[1][2]);

		final_offset = camera_right * local_offset.x + camera_up * local_offset.y;
	}

	//Optional stretch
	if (rotations_stretch.w != 0.0 && vector_aligned) {
		float3 direction = normalize(alignment);
		final_offset += direction * dot(local_offset, direction) * rotations_stretch.w;
	}

	float3 world_position = position + final_offset;
	output.position = mul(mul(ubo.proj, ubo.view), float4(world_position, 1.0));

	output.frag_color = in_color * intensity;
	return output;
}
