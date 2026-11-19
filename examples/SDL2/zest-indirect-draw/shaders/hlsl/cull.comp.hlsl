cbuffer zest_push : register(b0, space0) {
	uint all_instances_index;
	uint visible_instances_index;
	uint indirect_cmd_index;
	uint ubo_index;
	uint total_count;
	uint index_count;
	uint first_index;
	int vertex_offset;
	uint cull_enabled;
	float contract_frustum;
};

struct view_t {
	float4x4 proj;
	float4x4 view;
	float4 planes[6];
	float4 light_pos;
	float loc_speed;
	float glob_speed;
	uint visible;
};

ByteAddressBuffer zest_buffers[] : register(t0, space105);
RWByteAddressBuffer zest_rw_buffers[] : register(u0, space105);
ConstantBuffer<view_t> zest_uniforms[] : register(b0, space107);

//mesh_instance_t is 44 tightly packed bytes: position, color, rotation, scale and texture layer index
static const uint INSTANCE_STRIDE = 44;
//Byte offset of instanceCount in the indexed indirect draw command
static const uint INSTANCE_COUNT_OFFSET = 4;

float4 contract_plane(float4 plane, float distance) {
	return float4(plane.xyz, plane.w - distance);
}

void contract_frustum_planes(inout float4 planes[6], float distance) {
	for (int plane_index = 0; plane_index != 2; ++plane_index) {
		planes[plane_index] = contract_plane(planes[plane_index], distance);
	}
}

bool frustum_check(float4 position, float radius) {
	//Check sphere against frustum planes
	float4 planes[6] = zest_uniforms[NonUniformResourceIndex(ubo_index)].planes;
	contract_frustum_planes(planes, contract_frustum);

	for (int plane_index = 0; plane_index != 6; ++plane_index) {
		if (dot(position, planes[plane_index]) + radius < 0.0) {
			return false;
		}
	}
	return true;
}

[numthreads(256, 1, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
	uint index = thread_id.x;
	if (index >= total_count) return;

	//The Reset Indirect pass has already reset the draw command, so every workgroup counts into the same fresh command
	uint instance_offset = index * INSTANCE_STRIDE;
	uint4 position_color = zest_buffers[NonUniformResourceIndex(all_instances_index)].Load4(instance_offset);
	uint4 rotation_scale_x = zest_buffers[NonUniformResourceIndex(all_instances_index)].Load4(instance_offset + 16);
	uint3 scale_yz_texture_layer = zest_buffers[NonUniformResourceIndex(all_instances_index)].Load3(instance_offset + 32);

	//Bounding sphere radius based on max scale component
	float3 scale = asfloat(uint3(rotation_scale_x.w, scale_yz_texture_layer.xy));
	float radius = max(max(scale.x, scale.y), scale.z) * 2.0;

	bool visible = (cull_enabled == 0) || frustum_check(float4(asfloat(position_color.xyz), 1.0), radius);
	if (visible) {
		uint visible_index;
		zest_rw_buffers[NonUniformResourceIndex(indirect_cmd_index)].InterlockedAdd(INSTANCE_COUNT_OFFSET, 1, visible_index);
		uint visible_offset = visible_index * INSTANCE_STRIDE;
		zest_rw_buffers[NonUniformResourceIndex(visible_instances_index)].Store4(visible_offset, position_color);
		zest_rw_buffers[NonUniformResourceIndex(visible_instances_index)].Store4(visible_offset + 16, rotation_scale_x);
		zest_rw_buffers[NonUniformResourceIndex(visible_instances_index)].Store3(visible_offset + 32, scale_yz_texture_layer);
	}
}
