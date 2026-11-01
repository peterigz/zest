#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_ALL_UTILITIES_IMPLEMENTATION
#include <SDL.h>
#include "zest-timelinefx-particle-techniques.h"
#include "zest.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "examples/Common/sdl_controls.cpp"
#include "examples/Common/sdl_events.cpp"

#define MAX_INSTANCES 1024
#define MAX_SPRITES	250000
#define EFFECT_SPAWN_INTERVAL 25.f		//Milliseconds
#define TFX_LIBRARY_PATH "examples/assets/instanced_effects.tfx"

//Both the baked sprite data and the library contain these effects
static const char *effect_names[EFFECT_COUNT] = { "Big Explosion", "Star Burst Flash", "EmissionSingleShot", "Firework" };

#define BufferCast(struct_name, buffer_name) static_cast<struct_name*>(buffer_name.mapped)

//---Render specific functions---
//The following code will depend on the renderer that you're using

//Send the command to the GPU to draw all of the sprites for all of the animation instances that are currently in play
void RecordComputeSprites(zest_command_list command_list, void *user_data) {
	tfxParticleTechniquesExample *example = static_cast<tfxParticleTechniquesExample *>(user_data);

	zest_buffer billboard_buffer = zest_GetPassInputBuffer(command_list, "Billboard Instances");
	//Bind the buffer that contains the sprite instances to draw. These are updated by the compute shader on the GPU
	if (!billboard_buffer) return;
	zest_cmd_BindVertexBuffer(command_list, 0, 1, billboard_buffer);

	//Draw all the instance_data in the buffer
	tfx_sprite_sheet_push_t push{};
	push.color_ramp_texture_index = example->tfx_rendering.color_ramps_index;
	push.sampler_index = example->tfx_rendering.sampler_index;
	push.image_data_index = example->tfx_rendering.image_data_index;
	push.emitter_properties_index = example->tfx_rendering.emitter_properties_index;
	push.prev_billboards_index = 0;
	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(example->tfx_rendering.uniform_buffer);
	push.uniform_index = zest_GetUniformBufferDescriptorIndex(uniform_buffer);

	zest_layer layer = zest_GetLayer(example->tfx_rendering.layer);
	zest_cmd_LayerViewport(command_list, layer);
	
	zest_pipeline pipeline = zest_GetPipeline(example->tfx_rendering.particles.pipeline, command_list);
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_SendPushConstants(command_list, &push, sizeof(tfx_sprite_sheet_push_t));
	tfxU32 total_billboards = tfx_GetTotalSpritesThatNeedDrawing(example->animation_manager_3d);
	zest_cmd_Draw(command_list, 6, total_billboards, 0, 0);
}

//Prepare the compute shader that will be used to playback the effects in the animation manager
void PrepareComputeForEffectPlayback(tfxParticleTechniquesExample *example) {
	//Register a new compute shader
	example->playback_shader = CreateTimelineFXShader(example->shader_session, "tfx_sprite_playback", zest_compute_shader);

	//Finalise the compute set up by calling MakeCompute which creates all the boiler plate code to create the
	//compute pipeline in Vulkan
	example->compute_pipeline_3d = zest_CreateCompute(example->device, "TFX Playback", example->playback_shader);
}

//Every frame the compute shader needs to be dispatched which means that all the commands for the compute shader
//need to be added to the command buffer
void SpriteComputeFunction(zest_command_list command_list, void *user_data) {

	tfxParticleTechniquesExample *example = static_cast<tfxParticleTechniquesExample*>(user_data);

	zest_resource_node offsets_node = zest_GetPassInputResource(command_list, "Buffer Offsets");
	zest_resource_node animation_instances_node = zest_GetPassInputResource(command_list, "Animation Instances");
	zest_resource_node billboard_instances_node = zest_GetPassOutputResource(command_list, "Billboard Instances");

	if (!zest_ResourceBufferIsValid(offsets_node)) return;
	if (!zest_ResourceBufferIsValid(animation_instances_node)) return;

	tfx_animation_manager animation_manager = nullptr;
	animation_manager = example->animation_manager_3d;
	zest_compute compute = zest_GetCompute(example->compute_pipeline_3d);
	zest_cmd_BindComputePipeline(command_list, compute);

	//We'll need the animation metrics to tell the compute shader how many animation instances we're rendering this frame
	tfx_animation_buffer_metrics_t metrics = tfx_GetAnimationBufferMetrics(animation_manager);

	//Update the push constants with some metrics. These are referenced in the compute shader.
	//The total number of animation instances that need to be drawn
	tfx_sprite_data_push_t push;
	push.animation_instances_total = metrics.instances_size;
	//If any animation instance contains animated shapes then set to 1. the compute shader can use this to avoid some unecessary
	//computation if all particle shapes are not animated
	push.animated_shapes = tfx_HasAnimatedShapes(animation_manager);
	//Set the total number of instance_data that need to be processed by the shader
	push.billboards_total = metrics.total_sprites_to_draw;

	push.offsets_index = zest_GetTransientBufferBindlessIndex(command_list, offsets_node);
	push.animation_instances_index = zest_GetTransientBufferBindlessIndex(command_list, animation_instances_node);
	push.billboards_index = zest_GetTransientBufferBindlessIndex(command_list, billboard_instances_node);
	push.image_data_index = example->tfx_rendering.image_data_index;
	push.sprite_data_index = example->tfx_rendering.sprite_data_index;
	push.emitter_properties_index = example->tfx_rendering.emitter_properties_index;

	//Send the push constants in the compute object to the shader
	zest_cmd_SendPushConstants(command_list, &push, sizeof(tfx_sprite_data_push_t));

	//The 128 here refers to the local_size_x in the shader and is how many elements each group will work on
	//For example if there are 1024 instance_data, if we divide by 128 there will be 8 groups working on 128 instance_data each in parallel
	zest_cmd_DispatchCompute(command_list, (metrics.total_sprites_to_draw / 128) + 1, 1, 1);
}

//---Stage specific functions---
//Used when the effects are simulated live by a stage instead of played back from the baked sprite data.

//An effect gets about one in every EFFECT_COUNT spawns, doubled to cover runs of the same effect. Returns the pool capacity
//the stage needs for the rings of every Run on GPU emitter.
zest_uint SetGPUParticleCapacities(tfxParticleTechniquesExample *example) {
	zest_uint pool_capacity = 0;
	for (int effect_index = 0; effect_index != EFFECT_COUNT; ++effect_index) {
		tfx_effect_template effect_template = example->effect_templates[effect_index];
		zest_uint overlapping_locations = (zest_uint)(tfx_GetEffectLifetime(effect_template) * 2.f / (EFFECT_SPAWN_INTERVAL * EFFECT_COUNT)) + 1;
		pool_capacity += tfx_SetTemplateGPUCapacityForLocations(effect_template, overlapping_locations);
	}
	return pool_capacity;
}

//The effects spawn at user locations, so each one is added to the stage once and a location is added for every burst
void AddStageEffects(tfxParticleTechniquesExample *example) {
	for (int effect_index = 0; effect_index != EFFECT_COUNT; ++effect_index) {
		example->effect_ids[effect_index] = tfx_AddEffectTemplateToStage(example->stage, example->effect_templates[effect_index]);
	}
}

void InitStage(tfxParticleTechniquesExample *example, zest_shader_handle particles_frag_shader, zest_shader_handle ribbon_vert_shader, zest_shader_handle ribbon_frag_shader, zest_shader_handle ribbon_comp_shader) {
	//Live particles interpolate with the previous update so they need the vertex shader that reads the previous instances
	zest_shader_handle particles_vert_shader = CreateTimelineFXShader(example->shader_session, "tfx_billboard_vertex", zest_vertex_shader);
	zest_tfx_InitTimelineFXRenderResources(example->context, &example->stage_rendering, particles_vert_shader, particles_frag_shader, ribbon_vert_shader, ribbon_frag_shader, ribbon_comp_shader);

	example->library = zest_tfx_LoadLibrary(example->context, &example->stage_rendering, TFX_LIBRARY_PATH);
	for (int effect_index = 0; effect_index != EFFECT_COUNT; ++effect_index) {
		assert(tfx_IsValidDescriptorPath(example->library, effect_names[effect_index]));	//The effect is missing from the library
		example->effect_templates[effect_index] = tfx_CreateEffectTemplate(example->library, effect_names[effect_index]);
	}
	zest_tfx_FinaliseLibrary(example->context, &example->stage_rendering, example->library);

	//The GPU particle compute shader reads the graph lookups from the global buffers
	zest_tfx_CreateGlobalBuffers(example->context, &example->global_buffers);
	zest_tfx_InitialiseGlobalData(example->context, &example->global_buffers);

	//A gpu_particle_capacity is what turns on GPU simulation for the stage, without it Run on GPU emitters run on the CPU
	tfx_stage_info_t stage_info = tfx_CreateStageInfo(tfxStageSetup_none);
	stage_info.gpu_particle_capacity = SetGPUParticleCapacities(example);
	example->stage = tfx_CreateStage(stage_info);
	AddStageEffects(example);

	zest_shader_handle simulate_particles_shader = CreateTimelineFXShader(example->shader_session, "simulate_particles", zest_compute_shader);
	zest_tfx_CreateGPUParticleRenderer(example->context, &example->gpu_particles, &example->stage_rendering, &example->global_buffers, example->stage, simulate_particles_shader);
}

void ClearStage(tfxParticleTechniquesExample *example) {
	tfx_ClearStage(example->stage, false, false);
	AddStageEffects(example);
	zest_tfx_ResetGPUParticleDraws(&example->gpu_particles);
}

bool UsingStage(tfxParticleTechniquesExample *example) {
	return example->effect_mode != EffectMode_baked;
}

//Clears the effects of whichever mode is being left and starts the other one fresh
void ApplyRequestedEffectMode(tfxParticleTechniquesExample *example) {
	if (example->effect_mode == example->requested_effect_mode) {
		return;
	}
	if (!UsingStage(example)) {
		tfx_ClearAllAnimationInstances(example->animation_manager_3d);
		//Otherwise the time spent in baked mode would be caught up in one update
		zest_TimerReset(&example->stage_rendering.timer);
	}
	//Effects keep the simulation they were added with so the stage effects are added again after switching
	tfx_SetStageGPUSimulation(example->stage, example->requested_effect_mode == EffectMode_stage_gpu);
	ClearStage(example);
	example->effect_mode = example->requested_effect_mode;
}

void UpdateStage(tfxParticleTechniquesExample *example) {
	tfx_library_render_resources_t *resources = &example->stage_rendering;
	tfx_SetStageCamera(example->stage, &resources->camera.front.x, &resources->camera.position.x);

	bool stage_updated = false;
	zest_StartTimerLoop(resources->timer) {
		//One update for all the pending ticks so that the previous instance buffer is always the one that was drawn last
		if (pending_ticks > 0) {
			tfx_UpdateStage(example->stage, zest_TimerFrameLengthMillisecs(&resources->timer) * pending_ticks);
			pending_ticks = 0;
			stage_updated = true;
		}
	} zest_EndTimerLoop(resources->timer);

	//Only render after tfx_UpdateStage ran. Otherwise the last update's instances are drawn again with the new timer lerp
	if (stage_updated) {
		zest_tfx_RenderGPUParticleStage(&example->gpu_particles);
	}
}
//----End of render specific functions-----

//Initialise the example and create all the necessary buffers and objects for the compute shaders and load in the effects library
//and prepare the effect we'll use as an example.
void InitExample(tfxParticleTechniquesExample *example) {
	//Compile the shaders from timelinefx.slang in the timelinefxlib submodule. The baked vertex shader draws the billboards
	//that tfx_sprite_playback has already interpolated, so it doesn't need the previous frame's instances
	example->shader_session = CreateTimelineFXShaderSession(example->device);
	zest_shader_handle particles_frag_shader = CreateTimelineFXShader(example->shader_session, "tfx_billboard_fragment", zest_fragment_shader);
	zest_shader_handle particles_vert_shader = CreateTimelineFXShader(example->shader_session, "tfx_billboard_baked_vertex", zest_vertex_shader);
	zest_shader_handle ribbon_rendering_frag_shader = CreateTimelineFXShader(example->shader_session, "tfx_ribbon_fragment", zest_fragment_shader);
	zest_shader_handle ribbon_rendering_vert_shader = CreateTimelineFXShader(example->shader_session, "tfx_ribbon_vertex", zest_vertex_shader);
	zest_shader_handle ribbon_rendering_comp_shader = CreateTimelineFXShader(example->shader_session, "tfx_ribbon_compute", zest_compute_shader);
	zest_tfx_InitTimelineFXRenderResources(example->context, &example->tfx_rendering, particles_vert_shader, particles_frag_shader, ribbon_rendering_vert_shader, ribbon_rendering_frag_shader, ribbon_rendering_comp_shader);
	//The scene is laid out for a camera at the origin looking down +X rather than the editor's default camera
	example->tfx_rendering.camera = zest_CreateCamera();
	zest_CameraSetFoV(&example->tfx_rendering.camera, 60.f);
	zest_tfx_UpdateUniformBuffer(example->context, &example->tfx_rendering);
	//The baked vertex shader has no captured_index input to declare
	zest_ClearVertexAttributeDescriptions(example->tfx_rendering.particles.pipeline);
	zest_tfx_AddBillboardVertexAttributes(example->tfx_rendering.particles.pipeline, ZEST_FALSE);

	zest_sampler_info_t sampler = zest_CreateSamplerInfo();
	example->sampler = zest_CreateSampler(example->device, &sampler);
	example->sampler_index = zest_AcquireSamplerIndex(example->device, zest_GetSampler(example->sampler));

	//Initialise an animation manager. An animation manager maintains our animation instances for us and provides all the necessary metrics we'll
	//need to upload the buffers we need to both to initialise the compute shader and when we upload the offsets and instances buffer each frame.
	//Specify the maximum number of animation instances that you might want to play each frame
	example->animation_manager_3d = tfx_CreateAnimationManager(MAX_INSTANCES, MAX_SPRITES);
	//Here we set the callback that will be used to decide if an animation should be drawn or not. We use the bounding box to check if it's inside
	//the view frustum and cull it if it's not.
	tfx_SetAnimationManagerCullCallback(example->animation_manager_3d, CullAnimationInstancesCallback);
	//Set the user data to the tfxParticleTechniquesExample which we can use in the callback function
	tfx_SetAnimationManagerUserData(example->animation_manager_3d, example);

	//Create the handle where we will store gpu shapes for uploading to the gpu
	example->gpu_image_data = tfx_CreateGPUShapesList();

	//Record the effects we want and calculate the bounding boxes
	example->record_time = zest_Millisecs();		//See how long it takes to record.

	//Load prerecorded sprite data and create the particle image atlas
	tfxErrorFlags result = zest_tfx_LoadSpriteData(example->context, &example->tfx_rendering, "examples/assets/prerecorded_effects.tfxsd", example->animation_manager_3d);
	assert(result == tfxErrorCode_success);		//Unable to load the sprite data!

	//Finalise sprite data - upload color ramps, GPU image data, and create/upload sprite data and emitter properties buffers
	zest_tfx_FinaliseSpriteData(example->context, &example->tfx_rendering, example->animation_manager_3d, example->gpu_image_data);

	//Prepare the compute shaders for effect playback
	PrepareComputeForEffectPlayback(example);

	//Create the staging buffers that will stage the per frame instance and offset buffers
	zest_ForEachFrameInFlight(fif) {
		
		example->animation_instances_staging_buffer[fif] = zest_CreateStagingBuffer(example->device, tfx_CalculateAnimationInstanceBufferSize(MAX_INSTANCES), 0);
		example->offsets_staging_buffer[fif] = zest_CreateStagingBuffer(example->device, tfx_CalculateAnimationOffsetsBufferSize(MAX_INSTANCES), 0);
	}

	example->record_time = zest_Millisecs() - example->record_time;	//note how long the above took

	//Load the library the sprite data was recorded from so the same effects can be simulated live
	InitStage(example, particles_frag_shader, ribbon_rendering_vert_shader, ribbon_rendering_frag_shader, ribbon_rendering_comp_shader);

	//Initialise Dear ImGui
	zest_imgui_Initialise(example->context, &example->imgui, zest_implsdl2_DestroyWindow);
    ImGui_ImplSDL2_InitForVulkan((SDL_Window *)zest_Window(example->context));

	//Render specific - Set up the callback for updating the uniform buffers containing the model and view matrices
	zest_tfx_UpdateUniformBuffer(example->context, &example->tfx_rendering);

	//Set up a timer
	example->timer = zest_CreateTimer(60);
	example->random = tfx_CreateRandom(zest_Millisecs());
}

//Fuction to cast a ray from screen to world space.
zest_vec3 ScreenRay(tfxParticleTechniquesExample *example, float x, float y, float depth_offset, zest_vec3 &camera_position) {
	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(example->tfx_rendering.uniform_buffer);
	tfx_uniform_buffer_data_t *ubo_ptr = static_cast<tfx_uniform_buffer_data_t *>(zest_GetUniformBufferData(uniform_buffer));
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(example->context), zest_ScreenHeightf(example->context), &ubo_ptr->proj, &ubo_ptr->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

void BuildUI(tfxParticleTechniquesExample *example, zest_uint fps) {
	//Must call the imgui SDL2 implementation function
	ImGui_ImplSDL2_NewFrame();
	//Draw our imgui stuff
	ImGui::NewFrame();
	ImGui::Begin("Instanced Effects");
	ImGui::Text("FPS %i", fps);
	//Applied at the start of the next frame so that a stage update this frame still gets its GPU particles dispatched
	static const char *effect_mode_names[EffectMode_count] = { "Baked", "Stage simulated on the CPU", "Stage simulated on the GPU" };
	ImGui::Combo("Effects", &example->requested_effect_mode, effect_mode_names, EffectMode_count);
	ImGui::Separator();
	if (UsingStage(example)) {
		ImGui::Text("Effects: %u", tfx_GetEffectCount(example->stage));
		ImGui::Text("CPU Particles: %u", tfx_GetParticleCount(example->stage));
		if (example->effect_mode == EffectMode_stage_gpu) {
			if (example->gpu_particles.pool) {
				tfx_gpu_particle_stats_t stats = tfx_GetGPUParticleStats(example->stage);
				//Dead particles keep their slot until the longest lived particle spawned on the same tick dies
				ImGui::Text("GPU Particle Slots: %u", stats.estimated_live_particles);
				ImGui::Text("GPU Pool Allocated: %u / %u", stats.allocated, stats.capacity);
				ImGui::Text("GPU Rings: %u", stats.ring_count);
				ImGui::Text("Blocked Emitters: %u", stats.blocked_emitters);
				ImGui::Text("Dropped Spawns: %llu", (unsigned long long)stats.dropped_spawns);
			} else {
				ImGui::Text("No Run on GPU emitters in the library");
			}
		}
	} else {
		ImGui::Text("Record Time: %zims", example->record_time);
		ImGui::Text("Culled Instances: %i", tfx_GetTotalInstancesBeingUpdated(example->animation_manager_3d) - tfx_GetAnimationInstancesCount(example->animation_manager_3d));
		ImGui::Text("Instances In View: %i", tfx_GetAnimationInstancesCount(example->animation_manager_3d));
		ImGui::Text("Sprites Drawn: %i", tfx_GetTotalSpritesThatNeedDrawing(example->animation_manager_3d));
		ImGui::Text("Total Memory For Drawn Sprites: %ikb", tfx_GetTotalSpritesThatNeedDrawing(example->animation_manager_3d) / 1024);
		ImGui::Text("Total Memory For Sprite Data: %ikb", tfx_GetAnimationInstancesSizeInBytes(example->animation_manager_3d) / 1024);
	}
	ImGui::End();
	zest_imgui_DrawProfileWindow(example->context);
	ImGui::Render();
	//This function must be called after ImGui::Render to honor imguis lazily loaded atlas textures
	//for fonts. If this function is not called then you will hit an assert in imgui about no texture
	//being found.
	zest_imgui_UpdateTextures(&example->imgui);
}

//The callback function called for each instance that is being updated.
bool CullAnimationInstancesCallback(tfx_animation_manager animation_manager, tfx_float32x3_t position, float radius, void *user_data) {
	//Grab the tfxParticleTechniquesExample struct from the user data.
	tfxParticleTechniquesExample *example = static_cast<tfxParticleTechniquesExample *>(user_data);

	//Check if the radius of the bounding box is in view. Note that the planes are calculated in the UpdateUniform3d function.
	return (bool)zest_IsSphereInFrustum(example->tfx_rendering.planes, &position.x, radius);
}

void UploadAnimationData(const zest_command_list command_list, void *user_data) {
	tfxParticleTechniquesExample *example = static_cast<tfxParticleTechniquesExample *>(user_data);

	zest_resource_node offsets_node = zest_GetPassOutputResource(command_list, "Buffer Offsets");
	zest_resource_node animation_instances_node = zest_GetPassOutputResource(command_list, "Animation Instances");

	if (!zest_ResourceBufferIsValid(offsets_node)) return;
	if (!zest_ResourceBufferIsValid(animation_instances_node)) return;

	zest_buffer offsets_buffer = zest_GetResourceBuffer(offsets_node);
	zest_buffer animation_instances_buffer = zest_GetResourceBuffer(animation_instances_node);

	zest_uint fif = zest_CurrentFIF(zest_GetContext(command_list));

	zest_cmd_CopyBuffer(command_list, example->offsets_staging_buffer[fif], offsets_buffer, zest_BufferSize(offsets_buffer));
	zest_cmd_CopyBuffer(command_list, example->animation_instances_staging_buffer[fif], animation_instances_buffer, zest_BufferSize(animation_instances_buffer));
}

zest_buffer GetOffsetsBufferSize(zest_context context, zest_resource_node node) {
	tfxParticleTechniquesExample *example = static_cast<tfxParticleTechniquesExample *>(zest_GetResourceUserData(node));
	zest_SetResourceBufferSize(node, tfx_GetOffsetsSizeInBytes(example->animation_manager_3d));
    return NULL;
}

zest_buffer GetAnimationInstanceBufferSize(zest_context context, zest_resource_node node) {
	tfxParticleTechniquesExample *example = static_cast<tfxParticleTechniquesExample *>(zest_GetResourceUserData(node));
	zest_SetResourceBufferSize(node, tfx_GetAnimationInstancesSizeInBytes(example->animation_manager_3d));
    return NULL;
}

zest_buffer GetBillboardInstanceBufferSize(zest_context context, zest_resource_node node) {
	tfxParticleTechniquesExample *example = static_cast<tfxParticleTechniquesExample *>(zest_GetResourceUserData(node));
	zest_SetResourceBufferSize(node, tfx_GetTotalSpritesThatNeedDrawing(example->animation_manager_3d) * sizeof(tfx_instance_t));
    return NULL;
}

zest_vec3 RandomEffectPosition(tfxParticleTechniquesExample *example) {
	return { tfx_RandomRangeFromTo(&example->random, -10.f, 10.f), tfx_RandomRangeFromTo(&example->random, 8.f, 15.f), tfx_RandomRangeFromTo(&example->random, -10.f, 10.f) };
}

//Adds the effect to the stage when simulating live, otherwise plays its baked sprite data in the animation manager.
//The stage plays the effects at their authored scale.
void AddEffect(tfxParticleTechniquesExample *example, int effect_index, zest_vec3 position, float scale) {
	if (UsingStage(example)) {
		//The location removes itself once the effect has played out there
		tfx_AddSpawnLocation(example->stage, example->effect_ids[effect_index], &position.x, tfxSpawnLocationAdd_none);
		return;
	}
	tfxAnimationID anim_id = tfx_AddAnimationInstance(example->animation_manager_3d, effect_names[effect_index], 0);
	if (tfx_AnimationIDIsValid(anim_id)) {
		tfx_SetAnimationPosition(example->animation_manager_3d, anim_id, &position.x);
		tfx_SetAnimationScale(example->animation_manager_3d, anim_id, scale);
	}
}

void AddBakedPasses(tfxParticleTechniquesExample *example) {
	zest_buffer_resource_info_t offsets_buffer_info = { zest_resource_usage_hint_copy_dst, tfx_GetOffsetsSizeInBytes(example->animation_manager_3d) };
	zest_buffer_resource_info_t animation_instances_buffer_info = { zest_resource_usage_hint_copy_dst, tfx_GetAnimationInstancesSizeInBytes(example->animation_manager_3d) };
	zest_buffer_resource_info_t billboard_instance_buffer_info = { zest_resource_usage_hint_none, tfx_GetTotalSpritesThatNeedDrawing(example->animation_manager_3d) * sizeof(tfx_instance_t) };

	zest_resource_node offsets_buffer = zest_AddTransientBufferResource("Buffer Offsets", &offsets_buffer_info);
	zest_resource_node animation_instances = zest_AddTransientBufferResource("Animation Instances", &animation_instances_buffer_info);
	zest_resource_node billboard_instances = zest_AddTransientBufferResource("Billboard Instances", &billboard_instance_buffer_info);
	zest_SetResourceBufferProvider(offsets_buffer, GetOffsetsBufferSize);
	zest_SetResourceBufferProvider(animation_instances, GetAnimationInstanceBufferSize);
	zest_SetResourceBufferProvider(billboard_instances, GetBillboardInstanceBufferSize);
	zest_SetResourceUserData(offsets_buffer, example);
	zest_SetResourceUserData(animation_instances, example);
	zest_SetResourceUserData(billboard_instances, example);

	//------------------------ Sprite Data Tab -----------------------------------------------------------
	zest_BeginTransferPass("Transfer Animation Data"); {
		zest_ConnectOutput(offsets_buffer);
		zest_ConnectOutput(animation_instances);
		zest_SetPassTask(UploadAnimationData, example);
		zest_EndPass();
	}

	zest_BeginComputePass("Compute Sprite Data"); {
		zest_ConnectInput(offsets_buffer);
		zest_ConnectInput(animation_instances);
		zest_ConnectOutput(billboard_instances);
		zest_SetPassTask(SpriteComputeFunction, example);
		zest_EndPass();
	}

	zest_BeginRenderPass("Sprite Data Pass"); {
		zest_ConnectInput(billboard_instances);
		zest_ConnectSwapChainOutput();
		zest_SetPassTask(RecordComputeSprites, example);
		zest_EndPass();
	}
}

//Our main update function that's run every frame.
void MainLoop(tfxParticleTechniquesExample *example) {

	zest_microsecs running_time = zest_Microsecs();
	zest_microsecs frame_time = 0;
	zest_uint frame_count = 0;
	zest_uint fps = 0;
	int running = 1;
	SDL_Event event;

	while (running) {
		running = PollSDLEvents(example->context, &event);
		zest_microsecs current_frame_time = zest_Microsecs() - running_time;
		running_time = zest_Microsecs();
		frame_time += current_frame_time;
		frame_count += 1;
		if (frame_time >= ZEST_MICROSECS_SECOND) {
			frame_time -= ZEST_MICROSECS_SECOND;
			fps = frame_count;
			frame_count = 0;
		}
		zest_UpdateDevice(example->device);
		zest_tfx_UpdateLibraryGPUData(example->context, &example->stage_rendering, &example->global_buffers);

		if (zest_BeginFrame(example->context)) {
			UpdateMouse(example->context, &example->mouse, &example->tfx_rendering.camera);

			zest_tfx_UpdateUniformBuffer(example->context, &example->tfx_rendering);

			zest_uint fif = zest_CurrentFIF(example->context);

			ApplyRequestedEffectMode(example);

			int r = tfx_RandomRangeFromToInt(&example->random, 0, EFFECT_COUNT - 1);
			if (!example->left_mouse_clicked && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				//If the left mouse is clicked then spawn a random firework and raycasting it into the scene.
				zest_vec3 position = ScreenRay(example, (float)example->mouse.mouse_x, (float)example->mouse.mouse_y, 12.f, example->tfx_rendering.camera.position);
				AddEffect(example, r, position, tfx_RandomRangeFromTo(&example->random, 0.5f, 1.5f));
				example->left_mouse_clicked = true;
			} else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				example->left_mouse_clicked = false;
			}

			//Make a firework spawn randomly in the scene every 25 millisecs
			example->trigger_effect += current_frame_time;
			if (example->trigger_effect >= ZEST_MILLISECONDS_IN_MICROSECONDS(EFFECT_SPAWN_INTERVAL)) {
				AddEffect(example, r, RandomEffectPosition(example), tfx_RandomRangeFromTo(&example->random, 0.75f, 1.5f));
				example->trigger_effect = 0;
			}

			if (ImGui::IsKeyDown(ImGuiKey_Space)) {
				if (UsingStage(example)) {
					ClearStage(example);
				} else {
					tfx_ClearAllAnimationInstances(example->animation_manager_3d);
				}
			}

			//An example fixed rate update loop
			zest_StartTimerLoop(example->timer) {
				BuildUI(example, fps);

				UpdateCameraPosition(&example->timer, &example->new_camera_position, &example->old_camera_position, &example->tfx_rendering.camera, 5.f);
			} zest_EndTimerLoop(example->timer);

			example->tfx_rendering.camera.position = zest_LerpVec3(&example->old_camera_position, &example->new_camera_position, (float)zest_TimerLerp(&example->timer));

			if (UsingStage(example)) {
				//The stage has its own uniform buffer because the timer lerp it interpolates the particles with is its own
				example->stage_rendering.camera = example->tfx_rendering.camera;
				UpdateStage(example);
				zest_tfx_UpdateUniformBuffer(example->context, &example->stage_rendering);
			} else {
				tfx_UpdateAnimationManager(example->animation_manager_3d, (float)current_frame_time / 1000.f);

				//Copy the offsets and animation instances either to the staging buffers. The staging buffers will then be uploaded in the render pipeline.
				memcpy(zest_BufferData(example->offsets_staging_buffer[fif]), tfx_GetOffsetsBufferPointer(example->animation_manager_3d), tfx_GetOffsetsSizeInBytes(example->animation_manager_3d));
				memcpy(zest_BufferData(example->animation_instances_staging_buffer[fif]), tfx_GetAnimationInstancesBufferPointer(example->animation_manager_3d), tfx_GetAnimationInstancesSizeInBytes(example->animation_manager_3d));
			}

			RenderCacheInfo cache_info{};
			cache_info.draw_imgui = zest_imgui_HasGuiToDraw(&example->imgui);
			cache_info.draw_timeline_fx = true;
			cache_info.use_stage = UsingStage(example);
			zest_frame_graph_cache_key_t cache_key = {};
			cache_key = zest_InitialiseCacheKey(example->context, &cache_info, sizeof(RenderCacheInfo));

			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(example->context, &cache_key);
			if (!frame_graph) {
				if (zest_BeginFrameGraph(example->context, "TimelineFX Render Graph", &cache_key)) {
					zest_ImportSwapchainResource();
					if (UsingStage(example)) {
						zest_tfx_AddGPUParticlesToFrameGraph(&example->gpu_particles, 0);
					} else {
						AddBakedPasses(example);
					}

					zest_pass_node imgui_pass = zest_imgui_BeginPass(&example->imgui, example->imgui.main_viewport); {
						if (imgui_pass) {
							zest_ConnectSwapChainOutput();
							zest_EndPass();
						}
					}
				}
				frame_graph = zest_EndFrameGraph();
			}
			zest_EndFrame(example->context, frame_graph);
		}
	}
}

int main(int argc, char *argv[]) {
	if (!TimelineFXShadersAvailable("zest-timelinefx-particle-techniques")) return 0;

	zest_create_context_info_t create_info = zest_CreateContextInfo();
	ZEST__UNFLAG(create_info.flags, zest_context_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_context_init_flag_gpu_profiling);
	ZEST__FLAG(create_info.flags, zest_context_init_flag_cpu_profiling);

	tfxParticleTechniquesExample example{};
	//Initialise TimelineFX with however many threads you want. Each emitter is updated in it's own thread.
	tfx_BeginTimelineFX(tfx_GetDefaultThreadCount(), zloc__MEGABYTE(128));

	//Create a window using SDL2. We must do this before setting up the device as it's needed to get
	//the extensions info.
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "TimelineFX Particle Techniques");

	//Create the device that serves all vulkan based contexts
	example.device = zest_implsdl2_CreateVulkanDevice(&window_data, 0);

	//Initialise Zest
	example.context = zest_CreateContext(example.device, &window_data, &create_info);

	InitExample(&example);

	MainLoop(&example);

	ImGui_ImplSDL2_Shutdown();
	zest_imgui_Destroy(&example.imgui);
	FreeTimelineFXShaderSession(example.device, example.shader_session);
	zest_DestroyDevice(example.device);
	tfx_EndTimelineFX();

	return 0;
}
