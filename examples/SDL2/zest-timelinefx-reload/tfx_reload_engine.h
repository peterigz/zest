/*
	Shared C interface between platform.exe and engine.dll for the TimelineFX
	hot reload harness.

	Both sides compile this header. Only engine.dll compiles timelinefx.cpp, so
	only engine.dll can call a tfx_* function. platform.exe includes timelinefx.h
	purely for declarations - tfx_context, tfx_allocation_callbacks_t, tfx_instance_t
	and friends - never for code.

	The rendered harness adds a second rule: zest lives entirely in platform.exe.
	So everything engine.dll hands the renderer crosses this boundary as a plain
	pointer and length, and everything the renderer hands back (the shape loader
	and uv lookup) crosses it as a platform-owned function pointer.
*/
#ifndef TFX_RELOAD_ENGINE_H
#define TFX_RELOAD_ENGINE_H

#include <stddef.h>
#include "timelinefx.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef TFX_RELOAD_ENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
#else
#define ENGINE_API __attribute__((visibility("default")))
#endif

/* Positions the platform pulls back per sample. Generous enough to hold a whole
   healthy population so the nearest-neighbour continuity match has something to
   match against. */
#define TFX_RELOAD_MAX_POSITIONS 16384
/* How many of those the platform tracks individually across the reload. */
#define TFX_RELOAD_TRACKED_SAMPLES 32

/*
	Everything engine.dll needs handed back after a reload.

	Every member is a handle into the platform owned pool, a platform.exe function
	pointer, or a plain integer, so all of it outlives the module. The engine's own
	globals are zeroed by the reload; this struct, held in platform.exe memory, is
	how it finds its way back to its state. A real engine.dll would do exactly this.
*/
typedef struct engine_saved_state_t {
	tfx_context         context;
	tfx_library         library;
	tfx_stage           stage;
	tfx_effect_template effect_template;
	tfxU32              effect_id;
	tfxU32              callback_hits;
	tfxU32              frame_index;
	void               *pool_memory;      /* the platform block, so the engine keeps asserting it */
	size_t              pool_size;
	/* Tier 2. tfx_SetContext nulls library->uv_lookup because it assumes the address
	   died with the module. In the rendered harness it did not - it belongs to
	   platform.exe - but it still has to be handed back and re-registered. */
	tfx_uv_lookup       uv_lookup;
	void               *uv_user_data;
} engine_saved_state_t;

typedef struct engine_sample_t {
	tfxU32 particle_count;
	int    instance_count;
	float  centroid[3];
	float  spread;              /* rms distance of the instances from the centroid */
	tfxU32 callback_hits;       /* times the effect update callback has fired, ever */
	tfxU32 frame_index;         /* fixed timestep frames simulated, ever */
	int    used_blocks;         /* summed over every pool, via tfx_CreateMemorySnapshot */
	int    free_blocks;
	size_t used_size;
	int    pool_count;          /* > 1 means TimelineFX grew the pool */
	int    context_in_pool;     /* 1 when tfx_GetContext() lands inside the platform block */
	int    thread_count;
	int    globals_were_zeroed; /* the engine data segment came back zeroed, so the reload was real */
} engine_sample_t;

/* What the renderer reads each frame. Both pointers are into the platform-owned
   pool, so the platform may read them directly. */
typedef struct engine_render_data_t {
	const tfx_instance_t *instances;
	int                   instance_count;
	tfxU32                particle_count;
} engine_render_data_t;

/*
	A shape_loader lives in platform.exe but is handed a tfx_image_data_t, which is
	opaque outside timelinefx_internal.h and whose accessors are real functions
	linked into engine.dll. These two exports are the whole of what a loader needs,
	so the boundary holds without the platform linking any TimelineFX code.
*/
typedef struct engine_image_info_t {
	int   width;
	int   height;
	int   frame_count;
} engine_image_info_t;

/* Blocks of library data the renderer has to upload. The engine hands over the
   pointer and length; every zest call that consumes them is platform side. */
typedef enum engine_blob_kind {
	engine_blob_gpu_shapes,             /* tfx_gpu_image_data_t array */
	engine_blob_particle_properties,    /* tfx_gpu_particle_properties_t array */
	engine_blob_color_ramp_bitmap       /* one colour ramp bitmap, selected by index */
} engine_blob_kind;

typedef struct engine_blob_t {
	const void *data;
	size_t      size;
	int         width;         /* colour ramp bitmaps only */
	int         height;
	int         available;     /* how many blobs of this kind exist */
} engine_blob_t;

/* engine_reattach flags. Clearing a bit is how the negative cases are driven. */
#define ENGINE_REATTACH_SET_CONTEXT     (1u << 0)   /* call tfx_SetContext */
#define ENGINE_REATTACH_REBIND_CALLBACK (1u << 1)   /* re-register the Tier 2 callbacks */
#define ENGINE_REATTACH_DEFAULT         (ENGINE_REATTACH_SET_CONTEXT | ENGINE_REATTACH_REBIND_CALLBACK)

/* engine_detach flags. */
#define ENGINE_DETACH_SUSPEND           (1u << 0)   /* call tfx_SuspendTimelineFX */
#define ENGINE_DETACH_DEFAULT           (ENGINE_DETACH_SUSPEND)

/* First load only. The pool is acquired through the platform's callbacks;
   pool_memory/pool_size describe the platform block so the engine can assert the
   context really did land inside it. */
ENGINE_API int  engine_init(void *pool_memory, size_t pool_size, const tfx_allocation_callbacks_t *callbacks);

/* Shapes present in a .tfx file, so the renderer can size its atlas before the
   load starts feeding shapes to shape_loader. */
ENGINE_API int  engine_shape_count_in_file(const char *path);

/* shape_loader and uv_lookup belong to the caller. Headless passes stubs; the
   rendered harness passes zest-backed ones that live in platform.exe. */
ENGINE_API int  engine_load_library(const char *path, tfx_shape_loader shape_loader, tfx_uv_lookup uv_lookup, void *user_data, unsigned int *out_error_flags);

/* Create the stage, seed it, build the template and put one instance in the stage. */
ENGINE_API int  engine_start_effect(const char *effect_name, tfxU32 seed, engine_saved_state_t *out_state);

/* Rebuild the library's GPU shape data. Must run after every effect template is
   created, and again after a reload if the uv lookup changed. */
ENGINE_API void engine_finalise_library(void);

ENGINE_API int  engine_library_blob(int kind, int index, engine_blob_t *out);

ENGINE_API void engine_image_info(tfx_image_data_t *image_data, engine_image_info_t *out);
ENGINE_API void engine_image_set_pointer(tfx_image_data_t *image_data, void *ptr);

/* Called on every subsequent load, with the state the platform stashed. */
ENGINE_API int  engine_reattach(const engine_saved_state_t *state, const tfx_allocation_callbacks_t *callbacks,
                                unsigned int flags);

/* Called before every unload. Fills out_state for the platform to hold. */
ENGINE_API void engine_detach(engine_saved_state_t *out_state, unsigned int flags);

ENGINE_API void engine_update(double elapsed_ms);
ENGINE_API int  engine_particle_count(void);
ENGINE_API void engine_sample(engine_sample_t *out);
ENGINE_API void engine_sample_positions(float *out_xyz, int max_samples, int *out_count);
ENGINE_API void engine_render_data(engine_render_data_t *out);
ENGINE_API void engine_set_camera(const float front[3], const float position[3]);
ENGINE_API void engine_shutdown(void);

typedef int  (*engine_init_fn)(void *, size_t, const tfx_allocation_callbacks_t *);
typedef int  (*engine_shape_count_in_file_fn)(const char *);
typedef int  (*engine_load_library_fn)(const char *, tfx_shape_loader, tfx_uv_lookup, void *, unsigned int *);
typedef int  (*engine_start_effect_fn)(const char *, tfxU32, engine_saved_state_t *);
typedef void (*engine_finalise_library_fn)(void);
typedef int  (*engine_library_blob_fn)(int, int, engine_blob_t *);
typedef void (*engine_image_info_fn)(tfx_image_data_t *, engine_image_info_t *);
typedef void (*engine_image_set_pointer_fn)(tfx_image_data_t *, void *);
typedef int  (*engine_reattach_fn)(const engine_saved_state_t *, const tfx_allocation_callbacks_t *, unsigned int);
typedef void (*engine_detach_fn)(engine_saved_state_t *, unsigned int);
typedef void (*engine_update_fn)(double);
typedef int  (*engine_particle_count_fn)(void);
typedef void (*engine_sample_fn)(engine_sample_t *);
typedef void (*engine_sample_positions_fn)(float *, int, int *);
typedef void (*engine_render_data_fn)(engine_render_data_t *);
typedef void (*engine_set_camera_fn)(const float *, const float *);
typedef void (*engine_shutdown_fn)(void);

#ifdef __cplusplus
}
#endif

#endif /* TFX_RELOAD_ENGINE_H */
