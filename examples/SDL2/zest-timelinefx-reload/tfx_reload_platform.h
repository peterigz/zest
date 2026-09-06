/*
	The persistent half of the TimelineFX hot reload harness, shared by the headless
	and the rendered executables.

	Everything here lives in platform.exe and never reloads: the memory pool, the
	allocate/deallocate callbacks, the module load/unload pair, and the one reload
	sequence. Nothing in this file calls a tfx_* function.
*/
#ifndef TFX_RELOAD_PLATFORM_H
#define TFX_RELOAD_PLATFORM_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>

#include "tfx_reload_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HARNESS_PASS              0
#define HARNESS_FAIL              1
#define HARNESS_NEGATIVE_EXPECTED 42   /* negative case behaved as documented, without crashing */
#define HARNESS_NEGATIVE_SURVIVED 43   /* negative case was supposed to fail and did not */

#define TFX_RELOAD_FIXED_TIMESTEP 16.66667
#define TFX_RELOAD_SEED           123456u

/*
	Platform owned pool. One VirtualAlloc region, bump allocated. The bump never
	reuses a byte, so any pool TimelineFX acquires after the first shows up
	immediately as cursor movement - the cheapest possible detector for a reload
	that leaked a whole pool.
*/
typedef struct platform_pool_t {
	unsigned char *base;
	size_t         reserved;
	size_t         cursor;
	int            live_allocations;
	int            total_allocations;
	size_t         live_bytes;
} platform_pool_t;

int  platform_pool_create(platform_pool_t *pool, size_t reserved);
void platform_pool_destroy(platform_pool_t *pool);

/* These two live in platform.exe on purpose. If they lived in engine.dll their
   addresses would dangle across the reload exactly like every other pointer, and
   passing them to tfx_SetContext would be load bearing rather than belt-and-braces. */
void *platform_allocate(void *user_data, size_t size, size_t alignment);
void  platform_deallocate(void *user_data, void *memory, size_t size, size_t alignment);

typedef struct engine_module_t {
	HMODULE                    handle;
	char                       source_path[MAX_PATH];
	char                       scratch_dir[MAX_PATH];
	char                       loaded_path[MAX_PATH];
	int                        generation;
	int                        verbose;
	int                        unload_left_it_mapped;   /* set by module_unload every time */
	engine_init_fn             init;
	engine_reattach_fn         reattach;
	engine_detach_fn           detach;
	engine_update_fn           update;
	engine_particle_count_fn   particle_count;
	engine_sample_fn           sample;
	engine_sample_positions_fn sample_positions;
	engine_shutdown_fn         shutdown;
	engine_render_data_fn      render_data;
	engine_load_library_fn     load_library;
	engine_library_blob_fn     library_blob;
	engine_set_camera_fn       set_camera;
	engine_shape_count_in_file_fn shape_count_in_file;
	engine_start_effect_fn     start_effect;
	engine_finalise_library_fn finalise_library;
	engine_image_info_fn       image_info;
	engine_image_set_pointer_fn image_set_pointer;
} engine_module_t;

int  module_resolve_paths(engine_module_t *module);
int  module_load(engine_module_t *module);
void module_unload(engine_module_t *module);

typedef struct harness_t {
	engine_module_t            module;
	tfx_allocation_callbacks_t callbacks;
	engine_saved_state_t       saved;
	platform_pool_t            pool;
	int                        verbose;
} harness_t;

/* The whole unload/load pair. Nothing else in the harness does it. */
int harness_reload(harness_t *harness, unsigned int detach_flags, unsigned int reattach_flags);

/* Reserves the pool, loads the first generation, and calls engine_init. Does not
   load an effects library - the caller does that, because the rendered harness has
   to supply zest-backed shape/uv callbacks and the headless one does not. */
int harness_start(harness_t *harness, size_t pool_size, const char *library_path, int verbose);

/* stdout of engine_shutdown, captured so the leak report can be asserted on. */
int harness_shutdown_and_capture(harness_t *harness, char *out_text, size_t out_size);

/* Asserts and faults to stderr, never to a modal dialog, plus a fault reporter that
   names the faulting module and RVA. Two negative cases depend on both. */
void platform_install_crash_reporting(void);

extern int platform_failures;
void platform_check(int condition, const char *what, const char *detail_format, ...);

#ifdef __cplusplus
}
#endif

#endif /* TFX_RELOAD_PLATFORM_H */
