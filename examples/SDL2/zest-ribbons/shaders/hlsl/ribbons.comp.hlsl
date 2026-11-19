cbuffer zest_push : register(b0, space0) {
	float4 camera_position;
	float texture_y_scale;
	float texture_y_offset;
	float width_scale_multiplier;
	uint segment_count;
	uint tessellation;
	uint index_offset;
	uint ribbon_count;
	uint segment_buffer_index;
	uint instance_buffer_index;
	uint vertex_buffer_index;
	uint index_buffer_index;
};

/*
This compute shader takes in a buffer of ribbon segments and builds a vertex and index buffer for consumption by the vertex
shader. The push constant contains data such as the number of individual ribbons and the segment count of each ribbon. All
ribbons are the same length passed into the push constant as segment_count
*/

ByteAddressBuffer zest_buffers[] : register(t0, space105);
RWByteAddressBuffer zest_rw_buffers[] : register(u0, space105);

//ribbon_segment: float4 position_and_width (xyz = position, w = width), uint color, 12 bytes of padding
static const uint SEGMENT_STRIDE = 32;
//ribbon: float global_width, uint start_index (the segments of each ribbon are a ring buffer)
static const uint INSTANCE_STRIDE = 8;
//ribbon_vertex: float4 position, float4 uv, uint color, 12 bytes of padding
static const uint VERTEX_STRIDE = 48;

float4 load_segment_position_and_width(uint segment_index) {
	return asfloat(zest_buffers[NonUniformResourceIndex(segment_buffer_index)].Load4(segment_index * SEGMENT_STRIDE));
}

uint load_segment_color(uint segment_index) {
	return zest_buffers[NonUniformResourceIndex(segment_buffer_index)].Load(segment_index * SEGMENT_STRIDE + 16);
}

float load_instance_global_width(uint ribbon_index) {
	return asfloat(zest_buffers[NonUniformResourceIndex(instance_buffer_index)].Load(ribbon_index * INSTANCE_STRIDE));
}

uint load_instance_start_index(uint ribbon_index) {
	return zest_buffers[NonUniformResourceIndex(instance_buffer_index)].Load(ribbon_index * INSTANCE_STRIDE + 4);
}

void store_vertex(uint vertex_index, float4 position, float4 uv, uint color) {
	uint vertex_offset = vertex_index * VERTEX_STRIDE;
	zest_rw_buffers[NonUniformResourceIndex(vertex_buffer_index)].Store4(vertex_offset, asuint(position));
	zest_rw_buffers[NonUniformResourceIndex(vertex_buffer_index)].Store4(vertex_offset + 16, asuint(uv));
	zest_rw_buffers[NonUniformResourceIndex(vertex_buffer_index)].Store(vertex_offset + 32, color);
}

void store_index(uint index, uint vertex_index) {
	zest_rw_buffers[NonUniformResourceIndex(index_buffer_index)].Store(index * 4, vertex_index);
}

[numthreads(128, 1, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
	uint global_index = thread_id.x;
	if (global_index >= segment_count * ribbon_count) return;

	uint ribbon_index = global_index / segment_count;
	//All ribbon segments are in a single array of segments, so we need the offset to the location in the buffer
	//for the current ribbon using the ribbon index
	uint global_offset = ribbon_index * segment_count;
	//Each set of ribbon segments are stored in a ring buffer, so the local segment index plus the ring's start index,
	//wrapped by the segment count, plus the ribbon's offset gives the index into the buffer
	uint start_index = load_instance_start_index(ribbon_index);
	uint local_segment_index = global_index % segment_count;
	uint global_segment_index = global_offset + (local_segment_index + start_index) % segment_count;

	//Now we have the global segment index, we can index into the ribbon_data to get the segment position, width and color
	float4 position_and_width = load_segment_position_and_width(global_segment_index);
	float3 current_pos = position_and_width.xyz;
	uint current_color = load_segment_color(global_segment_index);
	float width = position_and_width.w * width_scale_multiplier * load_instance_global_width(ribbon_index);

	//Get a normalized vector to the camera so that the vertices can be made to face the camera
	float3 to_camera = normalize(camera_position.xyz - current_pos.xyz);

	//The next and previous segments wrap around the ring buffer. The previous index wraps as a uint at 0, as in the GLSL
	uint next_segment_index = global_offset + ((local_segment_index + 1 + start_index) % segment_count);
	uint prev_segment_index = global_offset + ((local_segment_index - 1 + start_index) % segment_count);

	float3 forward = (local_segment_index < segment_count - 1)
		? normalize(load_segment_position_and_width(next_segment_index).xyz - current_pos)
		: normalize(current_pos - load_segment_position_and_width(prev_segment_index).xyz);

	//Calculate the bisector and use it to get a perpendicular vector from the segment direction and the camera position
	float3 bisector = normalize(normalize(to_camera) + forward);
	float3 right = normalize(cross(forward, bisector));

	float ribbon_lerp = float(local_segment_index) / float(segment_count - 1);
	float u_scaled = ribbon_lerp * texture_y_scale;

	//Calculate the number of vertices per segment and ribbon
	uint vertices_per_segment = (tessellation + 1) * 2;
	uint vertices_per_ribbon = segment_count * vertices_per_segment;
	//Calculate the current base offset in to the vertex buffer we're writing to
	uint vertex_base_index = (ribbon_index * vertices_per_ribbon) + (local_segment_index * vertices_per_segment);

	//Tessellation is the same across the dispatch, so this branch is uniform
	if (tessellation > 0) {
		//Calculate vertices per segment based on tessellation
		for (uint step_index = 0; step_index <= tessellation; step_index++) {
			float t = float(step_index) / float(tessellation);		//Interpolation factor
			float width_factor = lerp(0.5, 0.0, t);					//Interpolate from outer to inner

			uint vertex_index = vertex_base_index + step_index;
			store_vertex(vertex_index, float4(current_pos + right * width * width_factor, 1.0), float4(t * 0.5, u_scaled + texture_y_offset, 0.0, ribbon_lerp * 3), current_color);

			width_factor = lerp(0.0, -0.5, t);						//Interpolate from inner to outer
			vertex_index = vertex_base_index + (tessellation + 1) + step_index;
			store_vertex(vertex_index, float4(current_pos + right * width * width_factor, 1.0), float4(0.5 + t * 0.5, u_scaled + texture_y_offset, 1.0, ribbon_lerp * 3), current_color);
		}

		//Generate indices for tessellated segments
		if (local_segment_index < segment_count - 1) {
			uint indices_per_segment = tessellation * 12;
			uint indices_per_ribbon = segment_count * indices_per_segment - indices_per_segment;
			uint index_base = index_offset + (ribbon_index * indices_per_ribbon) + (local_segment_index * indices_per_segment);
			uint next_base_vertex = vertex_base_index + vertices_per_segment;

			for (uint quad_index = 0; quad_index < tessellation; quad_index++) {
				uint current_quad_index = index_base + (quad_index * 12);

				uint v0 = vertex_base_index + quad_index;
				uint v1 = vertex_base_index + quad_index + 1;
				uint v2 = next_base_vertex + quad_index;
				uint v3 = next_base_vertex + quad_index + 1;

				//First triangle
				store_index(current_quad_index + 0, v0);
				store_index(current_quad_index + 1, v1);
				store_index(current_quad_index + 2, v2);

				//Second triangle
				store_index(current_quad_index + 3, v2);
				store_index(current_quad_index + 4, v1);
				store_index(current_quad_index + 5, v3);

				uint next_t = tessellation + 1;
				v0 += next_t;
				v1 += next_t;
				v2 += next_t;
				v3 += next_t;

				//First triangle
				store_index(current_quad_index + 6, v0);
				store_index(current_quad_index + 7, v1);
				store_index(current_quad_index + 8, v2);

				//Second triangle
				store_index(current_quad_index + 9, v2);
				store_index(current_quad_index + 10, v1);
				store_index(current_quad_index + 11, v3);
			}
		}
	} else {
		//Non-tessellated version
		float width_factor = width * 0.5;
		store_vertex(vertex_base_index, float4(current_pos + right * width_factor, 1.0), float4(0.0, u_scaled + texture_y_offset, 0.0, ribbon_lerp * 3.0), current_color);
		store_vertex(vertex_base_index + 1, float4(current_pos - right * width_factor, 1.0), float4(1.0, u_scaled + texture_y_offset, 1.0, ribbon_lerp * 3.0), current_color);

		//Generate indices for non-tessellated segments
		if (local_segment_index < segment_count - 1) {
			uint index_base = index_offset + (global_segment_index * 6);
			uint v0 = vertex_base_index;
			uint v1 = vertex_base_index + 2;		//Next segment's first vertex
			uint v2 = vertex_base_index + 1;
			uint v3 = vertex_base_index + 3;		//Next segment's second vertex

			//First triangle
			store_index(index_base + 0, v0);
			store_index(index_base + 1, v1);
			store_index(index_base + 2, v2);

			//Second triangle
			store_index(index_base + 3, v2);
			store_index(index_base + 4, v1);
			store_index(index_base + 5, v3);
		}
	}
}
