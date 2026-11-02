#pragma once

#include <zest.h>
#include <timelinefx.h>

//impl_timelinefx.c uses zest_utilities.h image loading, so define ZEST_IMAGES_IMPLEMENTATION where ZEST_IMPLEMENTATION is defined.
//The shaders are entry points in submodules/timelinefxlib/Shaders/timelinefx.slang. examples/Common/timelinefx_shaders.h compiles them.
//The shaders interpolate with tfx_library_render_resources_t::timer: run zest_StartTimerLoop/zest_EndTimerLoop on it and, once the loop
//has run, update each stage with tfx_UpdateStageSubSteps(stage, zest_TimerFrameLengthMillisecs(&timer), pending_ticks). The factor the
//shaders use is tfx_GetStageInterpolation with that timer's alpha, not zest_TimerLerp, so a catch-up frame doesn't jump.

typedef struct tfx_push_constants_s {
	tfxU32 color_ramp_texture_index;
	tfxU32 image_data_index;
	tfxU32 particle_properties_index;
	tfxU32 sampler_index;
	tfxU32 prev_billboards_index;
	tfxU32 index_offset;
	tfxU32 uniform_index;
} tfx_push_constants_t;

typedef struct tfx_uniform_buffer_data_t {
    zest_matrix4 view;
    zest_matrix4 proj;
    zest_vec2 screen_size;
	zest_uint millisecs;
    float timer_lerp;
    float update_time;
} tfx_uniform_buffer_data_t;

typedef struct tfx_ribbon_rendering_t {
	zest_pipeline_template pipeline;
	zest_shader_handle vert_shader;
	zest_shader_handle frag_shader;
	zest_shader_handle comp_shader;
	zest_compute_handle ribbon_compute;
} tfx_ribbon_rendering_t;

typedef struct tfx_particle_rendering_t {
	zest_shader_handle frag_shader;
	zest_shader_handle vert_shader;
	zest_pipeline_template pipeline;
} tfx_particle_rendering_t;

typedef struct tfx_global_library_buffers_t {
	zest_buffer lookup_buffer[ZEST_MAX_FIF];
	zest_uint lookup_index[ZEST_MAX_FIF];
	tfxU32 uploaded_lookups_version;		//tfx_GetGPUGraphLookupsVersion when the lookups were last uploaded
	//Every loaded library's data for stages with global_library_indexes, uploaded by zest_tfx_UpdateGlobalGPUData
	zest_buffer particle_properties;
	zest_buffer image_data;
	zest_image_handle color_ramps_texture;
	zest_uint particle_properties_index;
	zest_uint image_data_index;
	zest_uint color_ramps_index;
	zest_uint color_ramp_layer_capacity;
	tfxU32 *uploaded_color_ramp_layer_versions;	//One per layer of color_ramps_texture
	tfxU32 uploaded_particle_properties_version;
	tfxU32 uploaded_shapes_version;
	tfxU32 uploaded_color_ramps_version;
	zest_bool library_data_uploaded;
} tfx_global_library_buffers_t;

//One per particle shape. tfx holds a pointer to this for as long as the shape is in the library, so each
//record is allocated on its own and never moves; the shape_images array below holds only pointers and is
//free to grow or reorder. The frames of an animated shape are the array layers of the shape's one image,
//so an animation costs a single descriptor like any other shape.
typedef struct zest_tfx_shape_image_t {
	zest_image_handle image;
	zest_uint bindless_index;	//What the shaders sample the shape through
	zest_uint frames;			//Array layers in the image
	zest_uint frame_width;
	zest_uint frame_height;
	zest_format format;					//What the image is created with
	zest_component_mapping_t swizzle;
	zest_uint stored_mip_levels;		//Mip levels read from the file, 0 when the mips are generated after upload
	zest_bitmap_t pixels;		//The decoded sheet, or the stored mip levels back to back with the largest first. Alive only until the upload
	zest_bool unreadable;		//No image is made and the shape renders as the default image
} zest_tfx_shape_image_t;

//Holds one library's GPU data and one layer. Every stage drawn through these resources must only play effects from that
//library, because the draw passes send one set of color ramp, image data and property indexes for the whole stage, unless
//zest_tfx_UseGlobalLibraryData switched them to every library's data. Each GPU particle renderer needs its own render
//resources, since zest_tfx_RenderGPUParticleStage resets the layer.
typedef struct tfx_library_render_resources_s {
	zest_layer_handle layer;
	tfx_particle_rendering_t particles;
	tfx_ribbon_rendering_t ribbon_rendering;
	zest_image_collection_t color_ramps_collection;
	zest_image_handle color_ramps_texture;
	zest_sampler_handle sampler;
	zest_instruction_id layer_ids[tfxLAYERS];
	zest_uint color_ramps_index;
	zest_uint image_data_index;
	zest_uint sampler_index;
	zest_uint particle_properties_index;
	zest_uniform_buffer_handle uniform_buffer;
	zest_buffer particle_properties;
	zest_buffer image_data;
	zest_camera_t camera;
	zest_timer_t timer;
	zest_execution_timeline timeline;
	zest_vec4 planes[6];
	//Prerecorded sprite data buffers (used by animation manager workflow)
	zest_buffer sprite_data_buffer;
	zest_buffer emitter_properties_buffer;
	zest_uint sprite_data_index;
	zest_uint emitter_properties_index;
	zest_device device;
	//Every particle shape owns its image and its bindless index. The records are appended as the shape
	//loader runs and the last pending_count of them are the ones still waiting to be uploaded.
	zest_tfx_shape_image_t **shape_images;
	zest_uint shape_image_count;
	zest_uint shape_image_capacity;
	zest_uint pending_count;
	zest_uint images_removed;	//Counted by the shape remover across one refresh
	zest_uint unreadable_shapes;	//Shapes the loader could not decode, which render as the default image
	tfx_library library;		//Set by zest_tfx_LoadLibrary
	tfx_global_library_buffers_t *global_library_data;	//Set by zest_tfx_UseGlobalLibraryData, then the draws send its indexes
	tfxU32 uploaded_color_ramps_version;
	tfxU32 uploaded_particle_properties_version;
	zest_bool stale_data_reported;
} tfx_library_render_resources_t;

#define TFX_RIBBON_SEGMENT_BUFFER_NAME "Ribbon Segment Buffer"
#define TFX_RIBBON_INSTANCE_BUFFER_NAME "Ribbon Instance Buffer"
#define TFX_RIBBON_EMITTER_BUFFER_NAME "Emitter Buffer"
#define TFX_RIBBON_VERTEX_BUFFER_NAME "Ribbon Vertex Buffer"
#define TFX_RIBBON_INDEX_BUFFER_NAME "Ribbon Index Buffer"

#ifndef ZEST_TFX_MAX_RIBBON_STAGES
#define ZEST_TFX_MAX_RIBBON_STAGES 16
#endif

//Draws the ribbons of any number of stages with one upload, one compute pass and one draw pass. Pick the
//stages to draw each frame with zest_tfx_BeginRibbons / zest_tfx_AddRibbonStage. A cached frame graph reads
//the stage list every execution, so which stages are drawn doesn't need to be part of the cache key. That only holds
//while the renderer lives: freeing it flushes the context's cached frame graphs. The stages' ribbons must all come from
//the render resources' library, or from any library with zest_tfx_UseGlobalLibraryData.
typedef struct tfx_ribbon_renderer_t {
	zest_context context;
	tfx_library_render_resources_t *render_resources;
	tfx_global_library_buffers_t *global_buffers;
	zest_buffer segment_staging_buffer[ZEST_MAX_FIF];
	zest_buffer ribbon_staging_buffer[ZEST_MAX_FIF];
	zest_buffer emitter_staging_buffer[ZEST_MAX_FIF];
	tfx_ribbon_batch_t batch;
	tfx_stage stages[ZEST_TFX_MAX_RIBBON_STAGES];
	tfx_ribbon_batch_offsets_t stage_offsets[ZEST_TFX_MAX_RIBBON_STAGES];
	float stage_lerps[ZEST_TFX_MAX_RIBBON_STAGES];	//Each stage's tfx_GetStageInterpolation when it was added
} tfx_ribbon_renderer_t;

#define TFX_GPU_PARTICLE_UPLOAD_BUFFER_NAME "GPU Particle Upload"
#define TFX_GPU_PARTICLE_POOL_NAME "GPU Particle Pool"
#define TFX_GPU_PARTICLE_LAYER_NAME "GPU Particle Stage Instances"
#define TFX_GPU_PARTICLE_PREVIOUS_LAYER_NAME "GPU Particle Stage Previous Instances"

//Draws a stage whose Run on GPU emitters are simulated by the simulate_particles compute shader. The compute shader writes the
//GPU instances into the render resources' layer after the CPU instances. Each layer is drawn one sort key at a time
//(tfx_SetEffectSortKey), the key's CPU range then its GPU range. A stage without a gpu_particle_capacity still works, it just
//draws the CPU instances. Every range is drawn with the render resources' library, so a stage playing effects from two
//libraries needs zest_tfx_UseGlobalLibraryData on its render resources. One renderer per render resources.
typedef struct tfx_gpu_particle_renderer_t {
	zest_context context;
	tfx_library_render_resources_t *render_resources;	//Needs the tfx_billboard_vertex pipeline, which reads the previous instances
	tfx_global_library_buffers_t *global_buffers;		//The graph lookups the compute shader reads
	tfx_stage stage;
	zest_compute_handle compute;
	zest_buffer pool;									//Holds the particles between frames. Created zeroed and never cleared after that
	zest_uint pool_index;
	zest_buffer upload_staging_buffer[ZEST_MAX_FIF];
	tfx_gpu_particle_frame_t frame;
	tfx_gpu_particle_push_t push;
	zest_uint pending_dispatch_count;
	zest_bool layer_upload_pending;
	tfx_instance_key_range_t *key_ranges;				//Every layer's ranges in draw order
	zest_uint key_range_count;
	zest_uint key_range_capacity;
} tfx_gpu_particle_renderer_t;

#ifdef __cplusplus
extern "C" {
#endif

//-- High-level API (recommended) --

//Initialise the render resources needed for TimelineFX rendering including pipelines, shaders, layer and uniform buffer.
void zest_tfx_InitTimelineFXRenderResources(zest_context context, tfx_library_render_resources_t *render_resources, zest_shader_handle vert_shader, zest_shader_handle frag_shader, zest_shader_handle ribbon_vert, zest_shader_handle ribbon_frag, zest_shader_handle ribbon_comp);
//Free everything zest_tfx_InitTimelineFXRenderResources and the library loading functions created, including the library images.
//The shaders are left alone since they were passed in. Free the ribbon and GPU particle renderers using these resources first,
//and call it between frames, before the context is destroyed. Waits for the device to go idle if the pipelines were used.
//Flushes the context's cached frame graphs, since they hold resource nodes for the layer.
void zest_tfx_FreeRenderResources(zest_context context, tfx_library_render_resources_t *render_resources);

//Load a TimelineFX library, giving every particle shape its own image. Returns the library handle so you
//can create effect templates. Call zest_tfx_FinaliseLibrary next, or zest_tfx_UpdateGlobalGPUData for the global library data.
tfx_library zest_tfx_LoadLibrary(zest_context context, tfx_library_render_resources_t *resources, const char *library_path);

//Upload the library's color ramps, shape data and particle properties. Call once after zest_tfx_LoadLibrary. Templates can be
//created before or after it.
void zest_tfx_FinaliseLibrary(zest_context context, tfx_library_render_resources_t *resources, tfx_library library);

//Call once a frame before zest_BeginFrame. It compares the library's GPU data versions with the ones last uploaded, and when
//something changed (another library loaded, a template customised with tfx_ScaleTemplateEmitterGraph) it waits for the device
//to go idle and uploads it again. global_buffers may be null when the host has none. Returns true when it uploaded anything.
//The draw functions print a message once if they find the data changed and this wasn't called.
zest_bool zest_tfx_UpdateLibraryGPUData(zest_context context, tfx_library_render_resources_t *resources, tfx_global_library_buffers_t *global_buffers);

//Load prerecorded sprite data, giving every particle shape its own image.
//Returns the error flags from tfx_LoadSpriteData.
tfxErrorFlags zest_tfx_LoadSpriteData(zest_context context, tfx_library_render_resources_t *resources, const char *path, tfx_animation_manager animation_manager);

//Finalise prerecorded sprite data - uploads color ramps from the animation manager and GPU image data.
void zest_tfx_FinaliseSpriteData(zest_context context, tfx_library_render_resources_t *resources, tfx_animation_manager animation_manager, tfx_gpu_shapes gpu_image_data);

void zest_tfx_CreateGlobalBuffers(zest_context context, tfx_global_library_buffers_t *buffers);
//Free the buffers, image and descriptor indexes zest_tfx_CreateGlobalBuffers and zest_tfx_UpdateGlobalGPUData created. Free
//the ribbon and GPU particle renderers using them first.
void zest_tfx_FreeGlobalBuffers(zest_context context, tfx_global_library_buffers_t *buffers);

//-- Effects from more than one library --
//Load each library into its own zeroed tfx_library_render_resources_t with zest_tfx_LoadLibrary, which keeps that library's
//shape images (free them with zest_tfx_FreeLibraryImages after tfx_FreeLibrary). zest_tfx_FinaliseLibrary isn't needed.
//Draw the stages, which need global_library_indexes in their tfx_stage_info_t, through render resources set up with
//zest_tfx_UseGlobalLibraryData.

//Call once a frame before zest_BeginFrame in place of zest_tfx_UpdateLibraryGPUData, and straight after loading or freeing
//a library, before adding its effects. It runs tfx_UpdateGlobalGPUData, and when anything moved it waits for the device to
//go idle and uploads every library's properties and shapes, the color ramp layers that changed and the graph lookups.
//Returns false if a library didn't fit, see tfx_UpdateGlobalGPUData.
//Unloading a library while stages draw: between frames call tfx_FreeLibrary (it removes the library's effects from every stage),
//this, then zest_tfx_FreeLibraryImages. Until each stage updates again its instances still hold the freed library's particles
//and indexes into the old layout, so on that frame update every stage you draw before drawing it, with tfx_UpdateStage(stage, 0)
//if the timer didn't tick or the game is paused, and render it as on an update frame: zest_tfx_RenderGPUParticleStage, which
//also dispatches the GPU particles, or reset your layer and draw the instances again. zest_tfx_ResetGPUParticleDraws isn't
//needed, zest_tfx_RenderGPUParticleStage rebuilds the draw ranges. The image free is deferred, so it's safe before that update.
zest_bool zest_tfx_UpdateGlobalGPUData(zest_context context, tfx_global_library_buffers_t *global_buffers);
//Draw through these render resources with the data zest_tfx_UpdateGlobalGPUData uploads instead of their own library's, so
//their stages can play effects from any loaded library.
void zest_tfx_UseGlobalLibraryData(tfx_library_render_resources_t *resources, tfx_global_library_buffers_t *global_buffers);

//-- Ribbons --

void zest_tfx_CreateRibbonRenderer(zest_context context, tfx_ribbon_renderer_t *renderer, tfx_library_render_resources_t *resources, tfx_global_library_buffers_t *global_buffers);
//Call between frames, before the context is destroyed. Flushes the context's cached frame graphs, since they point at the renderer.
void zest_tfx_FreeRibbonRenderer(tfx_ribbon_renderer_t *renderer);
//Start this frame's list of stages to draw. Call after zest_BeginFrame and after the stages have been updated.
void zest_tfx_BeginRibbons(tfx_ribbon_renderer_t *renderer);
//Copy a stage's ribbons for drawing this frame. A stage that wasn't updated this frame still has to be added
//to be drawn. Call it after zest_EndTimerLoop on the render resources' timer, since it takes the stage's interpolation
//factor from that timer's alpha. Returns false if the stage couldn't be added.
zest_bool zest_tfx_AddRibbonStage(tfx_ribbon_renderer_t *renderer, tfx_stage stage);
zest_bool zest_tfx_HasRibbonsToDraw(tfx_ribbon_renderer_t *renderer);
//Add the upload, compute and draw passes for everything added this frame. Only needed when
//zest_tfx_HasRibbonsToDraw returns true. Draws to the swapchain when output_resource is null.
void zest_tfx_AddRibbonsToFrameGraph(tfx_ribbon_renderer_t *renderer, zest_resource_node output_resource);

//-- GPU particles --

//Size GPU rings for user spawn locations with tfx_SetTemplateGPUCapacityForLocations from the library.
//Use the renderer for every stage you draw, with or without GPU emitters. It creates the particle pool and the compute from the
//simulate_particles shader when the stage has a gpu_particle_capacity, see zest_tfx_SimulatesOnGPU.
void zest_tfx_CreateGPUParticleRenderer(zest_context context, tfx_gpu_particle_renderer_t *renderer, tfx_library_render_resources_t *resources, tfx_global_library_buffers_t *global_buffers, tfx_stage stage, zest_shader_handle simulate_shader);
//True when the stage had a gpu_particle_capacity, so its Run on GPU emitters are simulated by the compute shader
zest_bool zest_tfx_SimulatesOnGPU(const tfx_gpu_particle_renderer_t *renderer);
//Call between frames, before the context is destroyed. Flushes the context's cached frame graphs, since they hold resource nodes
//for the pool and point at the renderer.
void zest_tfx_FreeGPUParticleRenderer(tfx_gpu_particle_renderer_t *renderer);
//Call after zest_BeginFrame on every frame that tfx_UpdateStage ran, an update of 0 included, and only then. A frame where the timer
//ticked but the stage wasn't updated, such as while paused, would draw no GPU particles. It resets the render resources' layer, draws
//the CPU instances into it and stages the GPU particles simulated since the last call, which then have to be dispatched by the frame
//graph this frame.
void zest_tfx_RenderGPUParticleStage(tfx_gpu_particle_renderer_t *renderer);
//Call after every tfx_ClearStage. Until the next zest_tfx_RenderGPUParticleStage the renderer keeps drawing the last update's instances.
//Not needed after tfx_FreeLibrary if you follow the unload steps on zest_tfx_UpdateGlobalGPUData.
void zest_tfx_ResetGPUParticleDraws(tfx_gpu_particle_renderer_t *renderer);
//Add the upload, simulate and draw passes. Draws to the swapchain when output_resource is null. A cached frame graph picks up
//whatever was staged each frame, so it doesn't need to be part of the cache key. That only holds while the renderer and its render
//resources live: freeing either flushes the context's cached frame graphs.
void zest_tfx_AddGPUParticlesToFrameGraph(tfx_gpu_particle_renderer_t *renderer, zest_resource_node output_resource);

//What a call to zest_tfx_RefreshLibrary did. The tfx result's lists point into the library and stay valid
//until the next refresh of that library.
typedef struct tfx_library_refresh_t {
	tfx_refresh_result_t result;
	zest_uint images_added;			//Images created for shapes the refresh brought in
	zest_uint images_removed;		//Images freed for shapes the refresh took out
} tfx_library_refresh_t;

//Poll the library's file for changes and apply whatever can be applied in place, then bring the renderer's
//side up to date: new shapes are loaded through the shape loader and given images, removed shapes have their
//images and descriptor indexes freed, and the gpu shape and emitter property buffers are re-uploaded.
//Cheap to call regularly - it only reads the file's version until something actually changes.
//global_buffers may be null if the host has none, but then the global graph lookup table is left stale and
//any ribbon in the library will sample the wrong curves.
//Returns true when something was applied, in which case refresh describes it.
zest_bool zest_tfx_RefreshLibrary(zest_context context, tfx_library_render_resources_t *resources, tfx_library library, tfx_global_library_buffers_t *global_buffers, tfx_library_refresh_t *refresh);
//-- Uniform buffer and rendering --

//Call every frame after zest_BeginFrame and zest_EndTimerLoop. timer_lerp is tfx_GetStageInterpolation(stage) with the resources'
//timer. Every stage drawn through these resources shares it, so pass one that's updated with the same elapsed time as the others
//(stages on different update spans need their own render resources). Pass NULL for 1, when nothing interpolates.
void zest_tfx_UpdateUniformBuffer(zest_context context, tfx_library_render_resources_t *resources, tfx_stage stage);
void zest_tfx_RenderParticles(tfx_stage pm, tfx_library_render_resources_t *resources);
void zest_tfx_RenderParticlesByEffect(tfx_stage pm, tfx_library_render_resources_t *resources);
void zest_tfx_FreeLibraryImages(tfx_library_render_resources_t *resources);

//-- Frame graph pass callbacks --

void zest_tfx_DrawParticleLayer(const zest_command_list command_list, void *user_data);
void zest_tfx_UploadRibbonData(const zest_command_list command_list, void *user_data);
void zest_tfx_RibbonComputeFunction(const zest_command_list command_list, void *user_data);
void zest_tfx_RenderRibbons(const zest_command_list command_list, void *user_data);
void zest_tfx_UploadGPUParticleData(const zest_command_list command_list, void *user_data);
void zest_tfx_SimulateGPUParticles(const zest_command_list command_list, void *user_data);
void zest_tfx_DrawGPUParticleStage(const zest_command_list command_list, void *user_data);

//-- Low-level functions (for custom renderer integration) --

//Adds the tfx_instance_t vertex attributes at binding 0. Leave out the previous instances for a vertex shader that doesn't
//interpolate, like tfx_billboard_baked_vertex, which has no captured_index input.
void zest_tfx_AddBillboardVertexAttributes(zest_pipeline_template pipeline, zest_bool reads_previous_instances);
void zest_tfx_UpdateTimelineFXImageData(zest_context context, tfx_library_render_resources_t *tfx_rendering, tfx_gpu_shapes shapes);
void zest_tfx_UpdateTimelineFXParticleProperties(zest_context context, tfx_library_render_resources_t *tfx_rendering, tfx_library library);
void zest_tfx_InitialiseGlobalData(zest_context context, tfx_global_library_buffers_t *buffers);
void zest_tfx_GetUV(void *ptr, tfx_gpu_image_data_t *image_data, int offset);
void zest_tfx_ShapeLoader(const char *filename, tfx_image_data_t *image_data, void *raw_image_data, int image_memory_size, void *custom_data);
void zest_tfx_ShapeRemover(tfx_image_data_t *image_data, void *custom_data);

#ifdef __cplusplus
}
#endif