cbuffer zest_push : register(b0, space0) {
	uint indirect_cmd_index;
	uint index_count;
	uint first_index;
	int vertex_offset;
};

RWByteAddressBuffer zest_rw_buffers[] : register(u0, space105);

//Resets the 20 byte draw command before the Frustum Cull pass counts the visible instances into it
[numthreads(1, 1, 1)]
void main() {
	zest_rw_buffers[NonUniformResourceIndex(indirect_cmd_index)].Store4(0, uint4(index_count, 0, first_index, asuint(vertex_offset)));
	zest_rw_buffers[NonUniformResourceIndex(indirect_cmd_index)].Store(16, 0);
}
