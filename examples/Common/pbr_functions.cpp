/**
 * pbr_functions.cpp - PBR Image-Based Lighting (IBL) Setup Functions
 *
 * This file contains functions to generate the precomputed textures required for
 * physically-based rendering with image-based lighting. These textures enable
 * realistic metallic/roughness workflows with environment reflections.
 *
 * The three textures work together using the "split-sum approximation":
 *
 * 1. BRDF LUT (CreateBRDFLUT):
 *    Pre-integrates the specular BRDF. Lookup by (NdotV, roughness) returns
 *    scale/bias values for the Fresnel term. Same for all environments.
 *
 * 2. Irradiance Cubemap (CreateIrradianceCube):
 *    Pre-convolves the environment for diffuse lighting. Each texel stores
 *    the total incoming light from its hemisphere direction.
 *
 * 3. Prefiltered Environment (CreatePrefilteredCube):
 *    Pre-convolves the environment at different roughness levels stored in mips.
 *    Mip 0 = sharp reflections, higher mips = blurrier reflections.
 *
 * All three functions use Zest's "immediate mode" command buffer API to run
 * compute shaders outside the frame graph. This is appropriate for one-time
 * initialization that doesn't need to be part of the render loop.
 *
 * Usage in PBR shader:
 *   diffuse  = irradiance_cube.Sample(N) * albedo
 *   specular = prefiltered_cube.SampleLevel(R, roughness * maxMip) * (F0 * brdf.x + brdf.y)
 */

/**
 * CreateBRDFLUT - Generate a BRDF (Bidirectional Reflectance Distribution Function) Lookup Table
 *
 * This creates a 2D texture that pre-computes the split-sum approximation for the specular
 * part of the Cook-Torrance BRDF. The LUT is indexed by:
 *   - U axis: NdotV (cosine of angle between normal and view direction)
 *   - V axis: Roughness
 *
 * The output stores two values (R16G16 format):
 *   - R: Scale factor for F0 (base reflectivity)
 *   - G: Bias factor added to the result
 *
 * This LUT only needs to be generated once and can be reused across all PBR materials.
 */
zest_image_handle CreateBRDFLUT(zest_context context) {
	zest_device device = zest_GetContextDevice(context);

	// Load the compute shader that will generate the LUT
	// The 'true' parameter just tells the function not to cache the shader which is useful if you're just
	// developing the shader and need to recompile it each time
	zest_shader_handle brd_shader = zest_CreateShaderFromFile(device, "examples/Common/shaders/genbrdflut.comp", "genbrdflut_comp.spv", zest_compute_shader, true);

	// Create a 512x512 storage image to hold the LUT
	// R16G16 format gives us two 16-bit float channels for the scale and bias values
	zest_image_info_t image_info = zest_CreateImageInfo(512, 512);
	image_info.format = zest_format_r16g16_sfloat;
	image_info.flags = zest_image_preset_storage;  // Storage image so compute shader can write to it
	zest_image_handle brd_texture = zest_CreateImage(device, &image_info);
	zest_image brd_image = zest_GetImage(brd_texture);

	// Register the image with the bindless descriptor system
	// Storage binding: for the compute shader to write to
	// Sampled binding: for later use in the main loop frame graph for the PBR shader to read from
	zest_uint brd_bindless_texture_index = zest_AcquireStorageImageIndex(device, brd_image, zest_storage_image_binding);
	zest_AcquireSampledImageIndex(device, brd_image, zest_texture_2d_binding);

	// Create the compute pipeline from the shader
	zest_compute_handle brd_compute = zest_CreateCompute(device, "Brd Compute", brd_shader);
	zest_compute compute = zest_GetCompute(brd_compute);

	// Calculate dispatch dimensions - must match the local_size in the shader (8x8)
	const zest_uint local_size_x = 8;
	const zest_uint local_size_y = 8;
	zest_uint group_count_x = (512 + local_size_x - 1) / local_size_x;
	zest_uint group_count_y = (512 + local_size_y - 1) / local_size_y;

	// Push constant contains only the bindless index so the shader knows where to write
	zest_uint push;
	push = brd_bindless_texture_index;

	// Execute the compute shader using immediate mode commands
	// We use the graphics queue because we want to transition the image layout afterward
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_compute);
	zest_imm_SendPushConstants(queue, &push, sizeof(zest_uint));
	zest_imm_BindComputePipeline(queue, compute);
	zest_imm_DispatchCompute(queue, group_count_x, group_count_y, 6);
	// Transition image from storage layout to shader-read-only for sampling in render passes
	zest_imm_TransitionImage(queue, brd_image, zest_image_layout_shader_read_only_optimal, 0, 1, 0, 1);
	zest_imm_EndCommandBuffer(queue);

	// Clean up temporary resources - the texture itself is returned for use but we
	// don't need the storage image descriptor index anymore
	zest_ReleaseImageIndex(device, brd_image, zest_storage_image_binding);
	zest_FreeCompute(brd_compute);
	zest_FreeShader(brd_shader);
	return brd_texture;
}

/**
 * CreateIrradianceCube - Generate an irradiance cubemap for diffuse IBL (Image-Based Lighting)
 *
 * This function convolves the environment map (skybox) to create a low-frequency irradiance map.
 * Each texel in the output represents the total incoming light (irradiance) from the hemisphere
 * centered on that direction. This is used for the diffuse component of PBR lighting.
 *
 * The irradiance map is small (64x64 per face) because diffuse lighting is low-frequency -
 * it varies slowly across the surface and doesn't need high resolution.
 *
 * @param context           The Zest context
 * @param skybox_texture    The source environment cubemap to convolve
 * @param sampler_index     Bindless index of the sampler to use for reading the skybox
 */
zest_image_handle CreateIrradianceCube(zest_context context, zest_image_handle skybox_texture, zest_uint sampler_index) {
	zest_device device = zest_GetContextDevice(context);

	zest_shader_handle irr_shader = zest_CreateShaderFromFile(device, "examples/Common/shaders/irradiancecube.comp", "irradiancecube_comp.spv", zest_compute_shader, true);

	// Create a 64x64 cubemap - small because diffuse irradiance is low frequency
	// R32G32B32A32 format for high precision color values
	zest_image_info_t image_info = zest_CreateImageInfo(64, 64);
	image_info.format = zest_format_r32g32b32a32_sfloat;
	image_info.flags = zest_image_preset_storage_cubemap;
	image_info.layer_count = 6;  // 6 faces of the cubemap
	zest_image_handle irr_texture = zest_CreateImage(device, &image_info);
	zest_image irr_image = zest_GetImage(irr_texture);
	zest_image skybox_image = zest_GetImage(skybox_texture);

	// Register for bindless access - storage for compute writes, sampled for later reads in the main loop frame graph
	zest_uint irr_bindless_texture_index = zest_AcquireStorageImageIndex(device, irr_image, zest_storage_image_binding);
	zest_AcquireSampledImageIndex(device, irr_image, zest_texture_cube_binding);

	zest_compute_handle irr_compute = zest_CreateCompute(device, "irradiance compute", irr_shader);
	zest_compute compute = zest_GetCompute(irr_compute);

	// Set up push constants for the compute shader
	irr_push_constant_t push;
	push.source_env_index = zest_ImageDescriptorIndex(skybox_image, zest_texture_cube_binding);
	push.irr_index = irr_bindless_texture_index;
	push.sampler_index = sampler_index;

	// Delta values control the sampling density for hemisphere integration
	// delta_phi: azimuthal angle step (around the hemisphere)
	// delta_theta: polar angle step (from pole to equator)
	float delta_phi = (2.0f * float(ZEST_PI)) / 180.0f;
	float delta_theta = (0.5f * float(ZEST_PI)) / 64.0f;
	push.delta_phi = delta_phi;
	push.delta_theta = delta_theta;

	zest_uint local_size = 8;
	zest_uint group_count_x = (64 + local_size - 1) / local_size;
	zest_uint group_count_y = (64 + local_size - 1) / local_size;

	// Execute compute shader - dispatch 6 layers for all cubemap faces
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_compute);
	zest_imm_SendPushConstants(queue, &push, sizeof(irr_push_constant_t));
	zest_imm_BindComputePipeline(queue, compute);
	zest_imm_DispatchCompute(queue, group_count_x, group_count_y, 6);
	// Transition all 6 layers to shader-read-only layout
	zest_imm_TransitionImage(queue, irr_image, zest_image_layout_shader_read_only_optimal, 0, 1, 0, 6);
	zest_imm_EndCommandBuffer(queue);

	// Clean up - storage binding no longer needed, compute resources can be freed
	zest_ReleaseImageIndex(device, irr_image, zest_storage_image_binding);
	zest_FreeCompute(irr_compute);
	zest_FreeShader(irr_shader);
	return irr_texture;
}

/**
 * CreatePrefilteredCube - Generate a prefiltered environment cubemap for specular IBL
 *
 * This function creates a mipmapped cubemap where each mip level is pre-convolved with
 * increasing roughness values. This is the second part of the split-sum approximation
 * for specular IBL:
 *   - Mip 0: Roughness 0 (mirror-like reflections, sharp)
 *   - Higher mips: Increasing roughness (blurrier reflections)
 *
 * During rendering, the PBR shader samples from the appropriate mip level based on
 * material roughness, giving efficient glossy-to-rough reflections without runtime convolution.
 *
 * @param context                 The Zest context
 * @param skybox_texture          The source environment cubemap to prefilter
 * @param sampler_index           Bindless index of the sampler for reading the skybox
 * @param prefiltered_mip_indexes Output: array of bindless indexes for each mip level
 *                                (caller receives ownership, used during generation)
 */
zest_image_handle CreatePrefilteredCube(zest_context context, zest_image_handle skybox_texture, zest_uint sampler_index, zest_uint **prefiltered_mip_indexes) {
	zest_device device = zest_GetContextDevice(context);

	zest_shader_handle prefiltered_shader = zest_CreateShaderFromFile(device, "examples/Common/shaders/prefilterenvmap.comp", "prefilterenvmap_comp.spv", zest_compute_shader, true);

	// Create a 512x512 mipped cubemap - larger than irradiance because specular needs detail
	// R16G16B16A16 format balances precision with memory usage
	zest_image_info_t image_info = zest_CreateImageInfo(512, 512);
	image_info.format = zest_format_r16g16b16a16_sfloat;
	image_info.flags = zest_image_preset_storage_mipped_cubemap;  // Enables mipmap chain
	image_info.layer_count = 6;
	zest_image_handle prefiltered_texture = zest_CreateImage(device, &image_info);
	zest_image prefiltered_image = zest_GetImage(prefiltered_texture);
	zest_image skybox_image = zest_GetImage(skybox_texture);

	// Create separate image views for each mip level so compute shader can write to them individually
	// This is necessary because storage images can only write to a single mip level at a time
	zest_image_view_array_handle prefiltered_view_array_handle = zest_CreateImageViewsPerMip(device, prefiltered_image);
	zest_image_view_array prefiltered_view_array = zest_GetImageViewArray(prefiltered_view_array_handle);

	// Acquire bindless indexes for each mip level's view
	zest_uint prefiltered_bindless_texture_index = zest_AcquireStorageImageIndex(device, prefiltered_image, zest_storage_image_binding);
	*prefiltered_mip_indexes = zest_AcquireImageMipIndexes(device, prefiltered_image, prefiltered_view_array, zest_storage_image_binding, zest_descriptor_type_storage_image);
	zest_AcquireSampledImageIndex(device, prefiltered_image, zest_texture_cube_binding);

	zest_compute_handle prefiltered_compute = zest_CreateCompute(device, "prefiltered compute", prefiltered_shader);
	zest_compute compute = zest_GetCompute(prefiltered_compute);

	const zest_uint local_size = 8;
	prefiltered_push_constant_t push;
	push.source_env_index = zest_ImageDescriptorIndex(skybox_image, zest_texture_cube_binding);
	push.num_samples = 32;  // Number of importance samples for Monte Carlo integration
	push.sampler_index = sampler_index;

	const zest_image_info_t *prefiltered_image_info = zest_ImageInfo(prefiltered_image);

	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_compute);
	zest_imm_SendPushConstants(queue, &push, sizeof(irr_push_constant_t));
	zest_imm_BindComputePipeline(queue, compute);

	// Process each mip level with its corresponding roughness value
	// Mip 0 = roughness 0 (perfect mirror), highest mip = roughness 1 (fully diffuse)
	for (zest_uint m = 0; m < prefiltered_image_info->mip_levels; m++) {
		// Linear mapping from mip level to roughness
		push.roughness = (float)m / (float)(prefiltered_image_info->mip_levels - 1);
		// Each mip level has its own storage image bindless index
		push.prefiltered_index = (*prefiltered_mip_indexes)[m];

		zest_imm_SendPushConstants(queue, &push, sizeof(prefiltered_push_constant_t));

		// Calculate dimensions for this mip level (each mip is half the previous)
		float mip_width = static_cast<float>(512 * powf(0.5f, (float)m));
		float mip_height = static_cast<float>(512 * powf(0.5f, (float)m));
		zest_uint group_count_x = (zest_uint)ceilf(mip_width / local_size);
		zest_uint group_count_y = (zest_uint)ceilf(mip_height / local_size);

		// Dispatch the compute shader for all 6 cubemap faces at this mip level
		zest_imm_DispatchCompute(queue, group_count_x, group_count_y, 6);
	}

	// Transition all mip levels of all 6 faces to shader-read-only
	zest_imm_TransitionImage(queue, prefiltered_image, zest_image_layout_shader_read_only_optimal, 0, prefiltered_image_info->mip_levels, 0, 6);
	zest_imm_EndCommandBuffer(queue);

	// Clean up temporary resources
	// Note: The sampled image binding is kept for use in rendering
	zest_FreeCompute(prefiltered_compute);
	zest_FreeShader(prefiltered_shader);
	zest_FreeImageViewArray(prefiltered_view_array_handle);
	zest_ReleaseBindlessIndex(device, prefiltered_bindless_texture_index, zest_storage_image_binding);
	zest_ReleaseImageMipIndexes(device, prefiltered_image, zest_storage_image_binding);

	return prefiltered_texture;
}