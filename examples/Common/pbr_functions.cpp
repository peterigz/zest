
zest_image_handle CreateBRDFLUT(zest_context context) {
	zest_device device = zest_GetContextDevice(context);
	zest_shader_handle brd_shader = zest_CreateShaderFromFile(device, "examples/Common/shaders/genbrdflut.comp", "genbrdflut_comp.spv", zest_compute_shader, true);
	zest_image_info_t image_info = zest_CreateImageInfo(512, 512);
	image_info.format = zest_format_r16g16_sfloat;
	image_info.flags = zest_image_preset_storage;
	zest_image_handle brd_texture = zest_CreateImage(device, &image_info);
	zest_image brd_image = zest_GetImage(brd_texture);

	zest_uint brd_bindless_texture_index = zest_AcquireStorageImageIndex(device, brd_image, zest_storage_image_binding);
	zest_AcquireSampledImageIndex(device, brd_image, zest_texture_2d_binding);

	zest_compute_handle brd_compute = zest_CreateCompute(device, "Brd Compute", brd_shader, 0);
	zest_compute compute = zest_GetCompute(brd_compute);

	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;

	zest_uint group_count_x = (512 + local_size_x - 1) / local_size_x;
	zest_uint group_count_y = (512 + local_size_y - 1) / local_size_y;

	zest_uint push;
	push = brd_bindless_texture_index;

	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_compute);
	zest_imm_SendPushConstants(queue, &push, sizeof(zest_uint));
	zest_imm_BindComputePipeline(queue, compute);
	zest_imm_DispatchCompute(queue, group_count_x, group_count_y, 6);
	zest_imm_EndCommandBuffer(queue);

	zest_ReleaseImageIndex(device, brd_image, zest_storage_image_binding);
	zest_FreeCompute(brd_compute);
	zest_FreeShader(brd_shader);
	return brd_texture;
}

zest_image_handle CreateIrradianceCube(zest_context context, zest_image_handle skybox_texture, zest_uint sampler_index) {
	zest_device device = zest_GetContextDevice(context);

	zest_shader_handle irr_shader = zest_CreateShaderFromFile(device, "examples/Common/shaders/irradiancecube.comp", "irradiancecube_comp.spv", zest_compute_shader, true);
	zest_image_info_t image_info = zest_CreateImageInfo(64, 64);
	image_info.format = zest_format_r32g32b32a32_sfloat;
	image_info.flags = zest_image_preset_storage_cubemap;
	image_info.layer_count = 6;
	zest_image_handle irr_texture = zest_CreateImage(device, &image_info);
	zest_image irr_image = zest_GetImage(irr_texture);
	zest_image skybox_image = zest_GetImage(skybox_texture);
	zest_uint irr_bindless_texture_index = zest_AcquireStorageImageIndex(device, irr_image, zest_storage_image_binding);
	zest_AcquireSampledImageIndex(device, irr_image, zest_texture_cube_binding);

	zest_compute_handle irr_compute = zest_CreateCompute(device, "irradiance compute", irr_shader, 0);
	zest_compute compute = zest_GetCompute(irr_compute);

	irr_push_constant_t push;
	push.source_env_index = zest_ImageDescriptorIndex(skybox_image, zest_texture_cube_binding);
	push.irr_index = irr_bindless_texture_index;
	push.sampler_index = sampler_index;
	float delta_phi = (2.0f * float(ZEST_PI)) / 180.0f;
	float delta_theta = (0.5f * float(ZEST_PI)) / 64.0f;
	push.delta_phi = delta_phi;
	push.delta_theta = delta_theta;
	zest_uint local_size = 8;
	zest_uint group_count_x = (64 + local_size - 1) / local_size;
	zest_uint group_count_y = (64 + local_size - 1) / local_size;

	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_compute);
	zest_imm_SendPushConstants(queue, &push, sizeof(irr_push_constant_t));
	zest_imm_BindComputePipeline(queue, compute);
	zest_imm_DispatchCompute(queue, group_count_x, group_count_y, 6);
	zest_imm_EndCommandBuffer(queue);

	zest_ReleaseImageIndex(device, irr_image, zest_storage_image_binding);
	zest_FreeCompute(irr_compute);
	zest_FreeShader(irr_shader);
	return irr_texture;
}

zest_image_handle CreatePrefilteredCube(zest_context context, zest_image_handle skybox_texture, zest_uint sampler_index, zest_uint **prefiltered_mip_indexes) {
	zest_device device = zest_GetContextDevice(context);

	zest_shader_handle prefiltered_shader = zest_CreateShaderFromFile(device, "examples/Common/shaders/prefilterenvmap.comp", "prefilterenvmap_comp.spv", zest_compute_shader, true);
	zest_image_info_t image_info = zest_CreateImageInfo(512, 512);
	image_info.format = zest_format_r16g16b16a16_sfloat;
	image_info.flags = zest_image_preset_storage_mipped_cubemap;
	image_info.layer_count = 6;
	zest_image_handle prefiltered_texture = zest_CreateImage(device, &image_info);
	zest_image prefiltered_image = zest_GetImage(prefiltered_texture);
	zest_image skybox_image = zest_GetImage(skybox_texture);

	zest_image_view_array_handle prefiltered_view_array_handle = zest_CreateImageViewsPerMip(device, prefiltered_image);
	zest_image_view_array prefiltered_view_array = zest_GetImageViewArray(prefiltered_view_array_handle);
	zest_uint prefiltered_bindless_texture_index = zest_AcquireStorageImageIndex(device, prefiltered_image, zest_storage_image_binding);
	*prefiltered_mip_indexes = zest_AcquireImageMipIndexes(device, prefiltered_image, prefiltered_view_array, zest_storage_image_binding, zest_descriptor_type_storage_image);
	zest_AcquireSampledImageIndex(device, prefiltered_image, zest_texture_cube_binding);

	zest_compute_handle prefiltered_compute = zest_CreateCompute(device, "prefiltered compute", prefiltered_shader, 0);
	zest_compute compute = zest_GetCompute(prefiltered_compute);

	const zest_uint local_size = 8;
	prefiltered_push_constant_t push;
	push.source_env_index = zest_ImageDescriptorIndex(skybox_image, zest_texture_cube_binding);
	push.num_samples = 32;
	push.sampler_index = sampler_index;

	const zest_image_info_t *prefiltered_image_info = zest_ImageInfo(prefiltered_image);

	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_compute);
	zest_imm_SendPushConstants(queue, &push, sizeof(irr_push_constant_t));
	zest_imm_BindComputePipeline(queue, compute);
	for (zest_uint m = 0; m < prefiltered_image_info->mip_levels; m++) {
		push.roughness = (float)m / (float)(prefiltered_image_info->mip_levels - 1);
		push.prefiltered_index = (*prefiltered_mip_indexes)[m];

		zest_imm_SendPushConstants(queue, &push, sizeof(prefiltered_push_constant_t));

		float mip_width = static_cast<float>(512 * powf(0.5f, (float)m));
		float mip_height = static_cast<float>(512 * powf(0.5f, (float)m));
		zest_uint group_count_x = (zest_uint)ceilf(mip_width / local_size);
		zest_uint group_count_y = (zest_uint)ceilf(mip_height / local_size);

		//Dispatch the compute shader
		zest_imm_DispatchCompute(queue, group_count_x, group_count_y, 6);
	}
	zest_imm_EndCommandBuffer(queue);

	zest_FreeCompute(prefiltered_compute);
	zest_FreeShader(prefiltered_shader);
	zest_FreeImageViewArray(prefiltered_view_array_handle);
	zest_ReleaseBindlessIndex(device, prefiltered_bindless_texture_index, zest_storage_image_binding);
	zest_ReleaseImageMipIndexes(device, prefiltered_image, zest_storage_image_binding);

	return prefiltered_texture;
}