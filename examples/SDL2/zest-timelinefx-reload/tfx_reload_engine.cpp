/*
	engine.dll for the TimelineFX hot reload harness.

	TimelineFX is compiled into this module and exports nothing of its own. When
	platform.exe calls FreeLibrary every TimelineFX global, every address in this
	file, and every function pointer TimelineFX stored in its own state goes away.
	The state itself lives in a pool platform.exe owns and must come back untouched.

	Everything at file scope below is deliberately left as a plain global: it is
	zeroed on every reload, and engine_reattach rebuilding it from the saved state
	the platform handed back is the thing under test.

	zest is not linked here, not even in the rendered harness. Library data the
	renderer needs leaves through engine_library_blob and engine_render_data as a
	pointer and a length; the shape loader and uv lookup that consume it belong to
	platform.exe and arrive as arguments.

	timelinefx_internal.h is included for two reasons, both flagged in the report:
	  - tfx_CreateMemorySnapshot, for the flat-pool assertion.
	  - tfx_stage_s::effects, because the public API has no way to re-register an
	    update callback on an effect that is already live in a stage, and
	    tfx_SetContext nulls exactly that field.
*/
#ifndef TFX_RELOAD_ENGINE_EXPORTS
#define TFX_RELOAD_ENGINE_EXPORTS
#endif
#include "tfx_reload_engine.h"
#include "timelinefx_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static tfx_library         engine_library;
static tfx_stage           engine_stage;
static tfx_effect_template engine_effect_template;
static tfxU32              engine_effect_id;
static tfxU32              engine_callback_hits;
static tfxU32              engine_frame_index;
static void               *engine_pool_memory;
static size_t              engine_pool_size;
static tfx_uv_lookup       engine_uv_lookup;
static void               *engine_uv_user_data;
static int                 engine_trace;
static int                 engine_globals_were_zeroed = 1;

/* Set TFX_RELOAD_TRACE=1 to follow the engine through init and each reload. The
   negative case that is meant to crash gives up nothing else to look at. */
#define ENGINE_TRACE(...) do { if (engine_trace) { fprintf(stderr, "[engine] " __VA_ARGS__); fflush(stderr); } } while (0)

static void engine__read_trace_setting(void) {
	const char *trace = getenv("TFX_RELOAD_TRACE");
	engine_trace = (trace && trace[0] == '1');
}

/*
	Tier 2 callback. Its address lives in this module, so tfx_SetContext nulls it
	on every reload and engine_reattach has to put it back. Counting hits is how
	the harness proves it was re-registered (positive case) and how it proves the
	behaviour stops rather than crashes when it is not (negative case).
*/
static void engine__effect_update_callback(tfx_stage pm, tfxEffectID effect_index) {
	(void)pm;
	(void)effect_index;
	engine_callback_hits++;
}

static void engine__bind_callbacks(void) {
	if (engine_effect_template) {
		tfx_SetTemplateEffectUpdateCallback(engine_effect_template, engine__effect_update_callback);
	}
	/* uv_lookup is the caller's, so it comes back from the saved state rather than
	   from this module. tfx_SetContext nulls it either way. */
	if (engine_library && engine_uv_lookup) {
		engine_library->uv_lookup = engine_uv_lookup;
	}
	/* The live effect already in the stage. No public setter exists for this. */
	if (engine_stage && engine_effect_id < engine_stage->effects.size()) {
		engine_stage->effects[engine_effect_id].source_effect->update_callback = engine__effect_update_callback;
	}
}

static void engine__memory_snapshot(engine_sample_t *out) {
	tfx_allocator *allocator = tfxMemoryAllocator;
	out->pool_count = (int)tfxStore->memory_pool_count;
	for (tfxU32 i = 0; i != tfxStore->memory_pool_count; ++i) {
		tfx_pool *pool = (i == 0) ? tfx_GetPool(allocator) : (tfx_pool *)tfxStore->memory_pools[i];
		tfx_pool_stats_t stats = tfx_CreateMemorySnapshot(pool);
		out->used_blocks += stats.used_blocks;
		out->free_blocks += stats.free_blocks;
		out->used_size   += stats.used_size;
	}
}

static void engine__fill_saved_state(engine_saved_state_t *out_state) {
	if (!out_state) return;
	out_state->context         = tfx_GetContext();
	out_state->library         = engine_library;
	out_state->stage           = engine_stage;
	out_state->effect_template = engine_effect_template;
	out_state->effect_id       = engine_effect_id;
	out_state->callback_hits   = engine_callback_hits;
	out_state->frame_index     = engine_frame_index;
	out_state->pool_memory     = engine_pool_memory;
	out_state->pool_size       = engine_pool_size;
	out_state->uv_lookup       = engine_uv_lookup;
	out_state->uv_user_data    = engine_uv_user_data;
}

int engine_init(void *pool_memory, size_t pool_size, const tfx_allocation_callbacks_t *callbacks) {
	engine__read_trace_setting();
	engine_pool_memory = pool_memory;
	engine_pool_size   = pool_size;

	/* First load only. The pool is acquired through the platform's callbacks, so
	   every byte TimelineFX touches is platform.exe memory. */
	ENGINE_TRACE("tfx_BeginTimelineFX, %d threads, %zu byte pool\n", (int)tfx_GetDefaultThreadCount(), pool_size);
	return tfx_BeginTimelineFX(tfx_GetDefaultThreadCount(), pool_size, callbacks) != NULL;
}

int engine_shape_count_in_file(const char *path) {
	return tfx_GetShapeCountInLibrary(path);
}

int engine_load_library(const char *path, tfx_shape_loader shape_loader, tfx_uv_lookup uv_lookup, void *user_data, unsigned int *out_error_flags) {
	ENGINE_TRACE("loading %s\n", path);
	engine_uv_lookup    = uv_lookup;
	engine_uv_user_data = user_data;
	engine_library = tfx_LoadEffectLibrary(path, shape_loader, uv_lookup, user_data);
	if (!engine_library) return 0;
	if (out_error_flags) *out_error_flags = (unsigned int)tfx_GetLibraryErrorStatus(engine_library);
	ENGINE_TRACE("library %p, error flags 0x%X\n", (void *)engine_library, out_error_flags ? *out_error_flags : 0u);
	return 1;
}

int engine_start_effect(const char *effect_name, tfxU32 seed, engine_saved_state_t *out_state) {
	if (!engine_library) return 0;

	tfx_stage_info_t info = tfx_CreateStageInfo(tfxStageSetup_none);
	engine_stage = tfx_CreateStage(info);
	if (!engine_stage) return 0;
	tfx_SetStageSeed(engine_stage, seed);

	ENGINE_TRACE("stage %p, creating template for '%s'\n", (void *)engine_stage, effect_name);
	engine_effect_template = tfx_CreateEffectTemplate(engine_library, effect_name);
	if (!engine_effect_template) return 0;
	tfx_SetTemplateEffectUpdateCallback(engine_effect_template, engine__effect_update_callback);

	engine_effect_id = tfx_AddEffectTemplateToStage(engine_stage, engine_effect_template);
	if (!tfx_EffectIDIsValid(engine_effect_id)) return 0;
	tfx_SetEffectPosition(engine_stage, engine_effect_id, 0.f, 0.f, 0.f);
	engine__bind_callbacks();
	ENGINE_TRACE("effect id %u\n", engine_effect_id);

	engine__fill_saved_state(out_state);
	return 1;
}

void engine_finalise_library(void) {
	if (engine_library) tfx_UpdateLibraryGPUImageData(engine_library);
}

int engine_library_blob(int kind, int index, engine_blob_t *out) {
	memset(out, 0, sizeof(*out));
	if (!engine_library) return 0;
	switch ((engine_blob_kind)kind) {
	case engine_blob_gpu_shapes: {
		tfx_gpu_shapes shapes = tfx_GetLibraryGPUShapes(engine_library);
		if (!shapes) return 0;
		out->data      = tfx_GetGPUShapesArray(shapes);
		out->size      = tfx_GetGPUShapesSizeInBytes(shapes);
		out->available = 1;
		return out->data != NULL && out->size > 0;
	}
	case engine_blob_particle_properties:
		out->data      = tfx_GetParticlePropertiesBuffer(engine_library);
		out->size      = tfx_GetParticlePropertiesBufferSizeInBytes(engine_library);
		out->available = 1;
		return out->data != NULL && out->size > 0;
	case engine_blob_color_ramp_bitmap: {
		out->available = (int)tfx_GetColorRampBitmapCount(engine_library);
		if (index < 0 || index >= out->available) return 0;
		tfx_bitmap_t *bitmap = tfx_GetColorRampBitmap(engine_library, (tfxU32)index);
		if (!bitmap) return 0;
		out->data   = tfx_GetBitmapData(bitmap);
		out->size   = tfx_GetBitmapSize(bitmap);
		out->width  = (int)tfx_GetBitmapWidth(bitmap);
		out->height = (int)tfx_GetBitmapHeight(bitmap);
		return out->data != NULL;
	}
	default:
		return 0;
	}
}

void engine_image_info(tfx_image_data_t *image_data, engine_image_info_t *out) {
	out->width       = (int)tfx_GetImageWidth(image_data);
	out->height      = (int)tfx_GetImageHeight(image_data);
	out->frame_count = (int)tfx_GetImageFrameCount(image_data);
}

void engine_image_set_pointer(tfx_image_data_t *image_data, void *ptr) {
	tfx_SetImagePointer(image_data, ptr);
}

int engine_reattach(const engine_saved_state_t *state, const tfx_allocation_callbacks_t *callbacks, unsigned int flags) {
	engine__read_trace_setting();
	if (!state) return 0;

	/* Read before anything is restored. A fresh module image means a zeroed data
	   segment; if these came back with the previous generation's values the module
	   was never really unloaded and the whole run would be proving nothing. */
	engine_globals_were_zeroed = (engine_stage == NULL && engine_library == NULL &&
	                              engine_effect_template == NULL && engine_callback_hits == 0 &&
	                              engine_frame_index == 0);

	/* Adopt the context first. Nothing else here may touch TimelineFX before it:
	   this module's tfxCurrentContext is null until tfx_SetContext assigns it.
	   Skipping this is negative case 2. */
	if (flags & ENGINE_REATTACH_SET_CONTEXT) {
		ENGINE_TRACE("tfx_SetContext(%p)\n", (void *)state->context);
		if (!tfx_SetContext(state->context, callbacks)) return 0;
	}

	engine_library         = state->library;
	engine_stage           = state->stage;
	engine_effect_template = state->effect_template;
	engine_effect_id       = state->effect_id;
	engine_callback_hits   = state->callback_hits;
	engine_frame_index     = state->frame_index;
	engine_pool_memory     = state->pool_memory;
	engine_pool_size       = state->pool_size;
	engine_uv_lookup       = state->uv_lookup;
	engine_uv_user_data    = state->uv_user_data;

	/* tfx_SetContext nulled every Tier 2 callback because their addresses pointed
	   into the module that just went away. Skipping this is negative case 3. */
	if (flags & ENGINE_REATTACH_REBIND_CALLBACK) {
		engine__bind_callbacks();
	}

	ENGINE_TRACE("tfx_ResumeTimelineFX\n");
	tfx_ResumeTimelineFX();
	return 1;
}

void engine_detach(engine_saved_state_t *out_state, unsigned int flags) {
	/* Mandatory. After the first multithreaded frame there is a per-stage update
	   thread parked inside this module's code between frames, and the CRT holds a
	   module reference for every live thread, so FreeLibrary without this unloads
	   nothing at all. Skipping it is negative case 1. */
	if (flags & ENGINE_DETACH_SUSPEND) {
		ENGINE_TRACE("tfx_SuspendTimelineFX\n");
		tfx_SuspendTimelineFX();
	}
	engine__fill_saved_state(out_state);
}

void engine_update(double elapsed_ms) {
	tfx_UpdateStage(engine_stage, elapsed_ms);
	engine_frame_index++;
}

int engine_particle_count(void) {
	return (int)tfx_GetParticleCount(engine_stage);
}

void engine_set_camera(const float front[3], const float position[3]) {
	tfx_SetStageCamera(engine_stage, (float *)front, (float *)position);
}

void engine_render_data(engine_render_data_t *out) {
	out->particle_count = tfx_GetParticleCount(engine_stage);
	tfx_CompleteStageWork(engine_stage);
	out->instances      = tfx_GetInstanceBuffer(engine_stage);
	out->instance_count = tfx_GetInstanceCount(engine_stage);
}

void engine_sample(engine_sample_t *out) {
	memset(out, 0, sizeof(*out));
	out->particle_count = tfx_GetParticleCount(engine_stage);
	tfx_CompleteStageWork(engine_stage);
	tfx_instance_t *instances = tfx_GetInstanceBuffer(engine_stage);
	int count = tfx_GetInstanceCount(engine_stage);
	out->instance_count = count;

	double cx = 0.0, cy = 0.0, cz = 0.0;
	for (int i = 0; i != count; ++i) {
		cx += instances[i].position.x;
		cy += instances[i].position.y;
		cz += instances[i].position.z;
	}
	if (count > 0) {
		cx /= count; cy /= count; cz /= count;
		double sum = 0.0;
		for (int i = 0; i != count; ++i) {
			double dx = instances[i].position.x - cx;
			double dy = instances[i].position.y - cy;
			double dz = instances[i].position.z - cz;
			sum += dx * dx + dy * dy + dz * dz;
		}
		out->spread = (float)sqrt(sum / count);
	}
	out->centroid[0] = (float)cx;
	out->centroid[1] = (float)cy;
	out->centroid[2] = (float)cz;

	out->callback_hits       = engine_callback_hits;
	out->frame_index         = engine_frame_index;
	out->thread_count        = (int)tfxStore->thread_count;
	out->globals_were_zeroed = engine_globals_were_zeroed;

	/* Proves the state really is in the platform's block rather than a heap this
	   module owns - if it were, the reload could not work at all. */
	tfx_context context = tfx_GetContext();
	char *base = (char *)engine_pool_memory;
	out->context_in_pool = (base && (char *)context >= base && (char *)context < base + engine_pool_size) ? 1 : 0;

	engine__memory_snapshot(out);
}

void engine_sample_positions(float *out_xyz, int max_samples, int *out_count) {
	tfx_CompleteStageWork(engine_stage);
	tfx_instance_t *instances = tfx_GetInstanceBuffer(engine_stage);
	int count = tfx_GetInstanceCount(engine_stage);
	if (count > max_samples) count = max_samples;
	for (int i = 0; i != count; ++i) {
		out_xyz[i * 3 + 0] = instances[i].position.x;
		out_xyz[i * 3 + 1] = instances[i].position.y;
		out_xyz[i * 3 + 2] = instances[i].position.z;
	}
	*out_count = count;
}

void engine_shutdown(void) {
	if (engine_effect_template) tfx_FreeEffectTemplate(engine_effect_template);
	engine_effect_template = NULL;
	tfx_EndTimelineFX();
	engine_library = NULL;
	engine_stage = NULL;
}
