/*
	Stage 1 of the TimelineFX hot reload harness: headless.

	No window, no Vulkan, no zest. If a reload breaks here it is TimelineFX, and
	nothing else, that broke - which is the whole reason this executable exists
	separately from the rendered one.

	This is a correctness harness, not a demo. The assertions are about continuity
	of particle state across the reload boundary. "It did not crash" is not a pass.

	Usage, from the repository root:
	  zest-timelinefx-reload                     positive run, 50 reloads
	  zest-timelinefx-reload --negatives         run the three negative cases as children
	  zest-timelinefx-reload --case=<name>       run one negative case in this process
	                                             (no-suspend, no-setcontext, drop-callback)
	  --iterations=N --warmup=N --frames-between=N --effect=NAME --library=PATH --verbose
*/

#include "tfx_reload_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------------- */
/* Continuity metrics.                                                        */
/*                                                                            */
/* The instance buffer is rebuilt every frame and its order is not a stable    */
/* particle identity, so continuity is measured as a nearest-neighbour match   */
/* from a sample of the old cloud into the whole of the new one. If state      */
/* survived, every sampled particle is still there, one frame further along.   */
/* If state was lost the cloud restarts at the spawn point and every match     */
/* distance blows up.                                                          */
/* ------------------------------------------------------------------------- */

typedef struct cloud_t {
	float *positions;
	int    count;
} cloud_t;

static void cloud_create(cloud_t *cloud) {
	cloud->positions = (float *)malloc((size_t)TFX_RELOAD_MAX_POSITIONS * 3 * sizeof(float));
	cloud->count = 0;
}

static void cloud_destroy(cloud_t *cloud) {
	free(cloud->positions);
	cloud->positions = NULL;
}

static int compare_floats(const void *a, const void *b) {
	float fa = *(const float *)a, fb = *(const float *)b;
	return (fa < fb) ? -1 : (fa > fb) ? 1 : 0;
}

/* Median distance from a sample of `from` to its nearest point in `to`. */
static float cloud_match_distance(const cloud_t *from, const cloud_t *to, int samples, float *out_worst) {
	float distances[TFX_RELOAD_TRACKED_SAMPLES];
	if (out_worst) *out_worst = 0.f;
	if (from->count == 0 || to->count == 0) return -1.f;
	if (samples > TFX_RELOAD_TRACKED_SAMPLES) samples = TFX_RELOAD_TRACKED_SAMPLES;
	if (samples > from->count) samples = from->count;

	int stride = from->count / samples;
	if (stride < 1) stride = 1;
	for (int s = 0; s != samples; ++s) {
		const float *p = from->positions + (size_t)(s * stride) * 3;
		float best = 3.4e38f;
		for (int i = 0; i != to->count; ++i) {
			const float *q = to->positions + (size_t)i * 3;
			float dx = p[0] - q[0], dy = p[1] - q[1], dz = p[2] - q[2];
			float d2 = dx * dx + dy * dy + dz * dz;
			if (d2 < best) best = d2;
		}
		distances[s] = sqrtf(best);
		if (out_worst && distances[s] > *out_worst) *out_worst = distances[s];
	}
	qsort(distances, samples, sizeof(float), compare_floats);
	return distances[samples / 2];
}

static void capture_cloud(harness_t *harness, cloud_t *cloud) {
	harness->module.sample_positions(cloud->positions, TFX_RELOAD_MAX_POSITIONS, &cloud->count);
}

/* ------------------------------------------------------------------------- */
/* Options                                                                    */
/* ------------------------------------------------------------------------- */

typedef struct options_t {
	int         iterations;
	int         warmup_frames;
	int         frames_between;
	const char *library_path;
	const char *effect_name;
	const char *negative_case;
	int         run_negatives;
	int         verbose;
} options_t;

static int option_int(const char *arg, const char *name, int *out) {
	size_t len = strlen(name);
	if (strncmp(arg, name, len) == 0 && arg[len] == '=') {
		*out = atoi(arg + len + 1);
		return 1;
	}
	return 0;
}

static int option_string(const char *arg, const char *name, const char **out) {
	size_t len = strlen(name);
	if (strncmp(arg, name, len) == 0 && arg[len] == '=') {
		*out = arg + len + 1;
		return 1;
	}
	return 0;
}

static void options_parse(options_t *options, int argc, char **argv) {
	options->iterations     = 50;
	options->warmup_frames  = 300;
	options->frames_between = 30;
	options->library_path   = "examples/assets/vaders/vadereffects.tfx";
	options->effect_name    = "Background";
	options->negative_case  = NULL;
	options->run_negatives  = 0;
	options->verbose        = 0;
	for (int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		if (option_int(arg, "--iterations", &options->iterations)) continue;
		if (option_int(arg, "--warmup", &options->warmup_frames)) continue;
		if (option_int(arg, "--frames-between", &options->frames_between)) continue;
		if (option_string(arg, "--library", &options->library_path)) continue;
		if (option_string(arg, "--effect", &options->effect_name)) continue;
		if (option_string(arg, "--case", &options->negative_case)) continue;
		if (strcmp(arg, "--negatives") == 0) { options->run_negatives = 1; continue; }
		if (strcmp(arg, "--verbose") == 0) { options->verbose = 1; continue; }
		fprintf(stderr, "[platform] unknown option %s\n", arg);
	}
}

/* Headless uploads nothing, so both stubs are genuinely empty. Leaving
   image_data->ptr NULL is the supported way to say "no renderer handle": the
   library still records the shape, every emitter still gets a valid
   state_properties.image, and the load reports
   tfxErrorCode_some_images_loaded_without_user_ptr, which is asserted below. */
static void headless_shape_loader(const char *filename, tfx_image_data_t *image_data, void *raw_image_data, int image_size, void *user_data) {
	(void)filename; (void)image_data; (void)raw_image_data; (void)image_size; (void)user_data;
}

static void headless_uv_lookup(void *ptr, tfx_gpu_image_data_t *image_data, int offset) {
	(void)ptr; (void)image_data; (void)offset;
}

static unsigned int library_error_flags;

static int start_simulation(harness_t *harness, const options_t *options) {
	if (!harness_start(harness, (size_t)128 * 1024 * 1024, options->library_path, options->verbose)) return 0;
	if (!harness->module.load_library(options->library_path, headless_shape_loader, headless_uv_lookup,
	                                  harness, &library_error_flags)) {
		fprintf(stderr, "[platform] could not load %s\n", options->library_path);
		return 0;
	}
	if (!harness->module.start_effect(options->effect_name, TFX_RELOAD_SEED, &harness->saved)) {
		fprintf(stderr, "[platform] no effect named '%s' in %s\n", options->effect_name, options->library_path);
		return 0;
	}
	return 1;
}

/* ------------------------------------------------------------------------- */
/* The positive run                                                           */
/* ------------------------------------------------------------------------- */

static int run_positive(const options_t *options) {
	harness_t harness;
	if (!start_simulation(&harness, options)) return HARNESS_FAIL;

	cloud_t before, after, previous;
	cloud_create(&before);
	cloud_create(&after);
	cloud_create(&previous);

	printf("TimelineFX hot reload harness (stage 1, headless)\n");
	printf("  engine dll     : %s\n", harness.module.source_path);
	printf("  library        : %s\n", options->library_path);
	printf("  effect         : %s\n", options->effect_name);
	printf("  pool           : 128 MB of platform memory at %p\n", (void *)harness.pool.base);
	printf("  timestep       : %.5f ms fixed, seed %u\n", TFX_RELOAD_FIXED_TIMESTEP, TFX_RELOAD_SEED);
	printf("  reloads        : %d, %d frames between each\n\n", options->iterations, options->frames_between);

	/* --- Warm up, and measure what one frame of motion actually looks like --- */
	float frame_motions[512];
	int   frame_motion_count = 0;
	int   count_deltas[512];
	int   count_delta_count = 0;
	int   previous_count = 0;

	for (int frame = 0; frame != options->warmup_frames; ++frame) {
		harness.module.update(TFX_RELOAD_FIXED_TIMESTEP);
		capture_cloud(&harness, &after);
		int count = after.count;
		/* Only sample the steady state, once the population has settled. */
		if (frame > options->warmup_frames / 2 && previous.count > 0 && count > 0) {
			if (frame_motion_count < 512) {
				float motion = cloud_match_distance(&previous, &after, TFX_RELOAD_TRACKED_SAMPLES, NULL);
				if (motion >= 0.f) frame_motions[frame_motion_count++] = motion;
			}
			if (count_delta_count < 512) {
				int delta = count - previous_count;
				count_deltas[count_delta_count++] = delta < 0 ? -delta : delta;
			}
		}
		previous_count = count;
		float *swap = previous.positions; previous.positions = after.positions; after.positions = swap;
		previous.count = count;
	}

	if (frame_motion_count == 0) {
		fprintf(stderr, "[platform] no motion baseline could be measured - the effect produced no particles.\n");
		return HARNESS_FAIL;
	}
	qsort(frame_motions, frame_motion_count, sizeof(float), compare_floats);
	float typical_frame_motion = frame_motions[frame_motion_count / 2];
	float worst_frame_motion   = frame_motions[frame_motion_count - 1];

	int worst_count_delta = 0;
	for (int i = 0; i != count_delta_count; ++i) if (count_deltas[i] > worst_count_delta) worst_count_delta = count_deltas[i];

	engine_sample_t warm;
	harness.module.sample(&warm);

	printf("Baseline after %d warmup frames\n", options->warmup_frames);
	printf("  particles                     : %u\n", warm.particle_count);
	printf("  instances                     : %d\n", warm.instance_count);
	printf("  worker threads                : %d\n", warm.thread_count);
	printf("  per-frame motion (median/max) : %.4f / %.4f units\n", typical_frame_motion, worst_frame_motion);
	printf("  per-frame count swing (max)   : %d\n", worst_count_delta);
	printf("  pool blocks in use            : %d (%zu KB) across %d pool(s)\n\n",
	       warm.used_blocks, warm.used_size / 1024, warm.pool_count);

	printf("Preconditions\n");
	platform_check(warm.context_in_pool, "tfx context lives in platform memory", "inside the VirtualAlloc block");
	platform_check(warm.particle_count > 0, "population is alive before any reload", "%u particles", warm.particle_count);
	platform_check(warm.thread_count > 0, "TimelineFX is multithreaded", "%d workers parked between frames", warm.thread_count);
	platform_check(typical_frame_motion > 0.f, "particles are actually moving", "median %.4f units/frame", typical_frame_motion);
	platform_check(warm.pool_count == 1, "single pool after warmup", "%d pool(s)", warm.pool_count);
	/* The shape loader here sets no image pointer, which is legal but must be
	   reported rather than silently producing emitters with no image. */
	platform_check((library_error_flags & tfxErrorCode_some_images_loaded_without_user_ptr) != 0,
	               "load reported the shapes have no renderer handle", "error flags 0x%X", library_error_flags);

	/* Tolerances derived from the measured behaviour of this effect, not guessed. */
	const float motion_tolerance = worst_frame_motion * 8.f + typical_frame_motion * 2.f;
	const int   count_tolerance  = worst_count_delta * 6 + (int)(warm.particle_count / 10) + 8;

	printf("\nReloading %d times\n", options->iterations);
	printf("  tolerance: boundary motion <= %.4f units, count swing <= %d\n\n", motion_tolerance, count_tolerance);

	int     reload_failures        = 0;
	int     zero_population        = 0;
	float   worst_boundary_motion  = 0.f;
	int     worst_boundary_count   = 0;
	tfxU32  callback_hits_previous = warm.callback_hits;
	int     callback_stalls        = 0;
	int    *used_blocks_history    = (int *)malloc(sizeof(int) * options->iterations);
	size_t *used_size_history      = (size_t *)malloc(sizeof(size_t) * options->iterations);
	size_t  pool_cursor_after_init = harness.pool.cursor;
	int     extra_pools            = 0;
	int     unloads_refused        = 0;
	int     stale_globals          = 0;

	for (int iteration = 0; iteration != options->iterations; ++iteration) {
		engine_sample_t pre;
		harness.module.sample(&pre);
		capture_cloud(&harness, &before);

		if (pre.particle_count == 0) {
			zero_population++;
			printf("  iteration %2d: FAIL - population is zero at detach, this iteration proves nothing\n", iteration);
		}

		if (!harness_reload(&harness, ENGINE_DETACH_DEFAULT, ENGINE_REATTACH_DEFAULT)) {
			fprintf(stderr, "[platform] reload %d failed\n", iteration);
			return HARNESS_FAIL;
		}

		harness.module.update(TFX_RELOAD_FIXED_TIMESTEP);
		capture_cloud(&harness, &after);
		engine_sample_t post;
		harness.module.sample(&post);

		float worst = 0.f;
		float boundary_motion = cloud_match_distance(&before, &after, TFX_RELOAD_TRACKED_SAMPLES, &worst);
		int   count_delta = (int)post.particle_count - (int)pre.particle_count;
		if (count_delta < 0) count_delta = -count_delta;

		int ok = 1;
		if (boundary_motion < 0.f)                    ok = 0;   /* one side of the boundary was empty */
		if (boundary_motion > motion_tolerance)       ok = 0;
		if (count_delta > count_tolerance)            ok = 0;
		if (post.particle_count == 0)                 ok = 0;
		if (post.callback_hits <= callback_hits_previous) { ok = 0; callback_stalls++; }
		if (!post.context_in_pool)                    ok = 0;
		if (post.pool_count != 1)                     extra_pools++;
		/* Guards against the run passing because nothing actually reloaded. */
		if (harness.module.unload_left_it_mapped)     { ok = 0; unloads_refused++; }
		if (!post.globals_were_zeroed)                { ok = 0; stale_globals++; }

		if (boundary_motion > worst_boundary_motion) worst_boundary_motion = boundary_motion;
		if (count_delta > worst_boundary_count) worst_boundary_count = count_delta;
		if (!ok) reload_failures++;

		used_blocks_history[iteration] = post.used_blocks;
		used_size_history[iteration]   = post.used_size;
		callback_hits_previous         = post.callback_hits;

		if (options->verbose || !ok) {
			printf("  iteration %2d: %s motion %.4f (worst %.4f) count %u -> %u  blocks %d  gen %d\n",
			       iteration, ok ? "ok  " : "FAIL", boundary_motion, worst, pre.particle_count,
			       post.particle_count, post.used_blocks, harness.module.generation - 1);
		}

		for (int frame = 0; frame != options->frames_between; ++frame) {
			harness.module.update(TFX_RELOAD_FIXED_TIMESTEP);
		}
	}

	/* --- Pool drift: compare the settled half of the run against the early half --- */
	int    half = options->iterations / 2;
	double early_blocks = 0.0, late_blocks = 0.0;
	double early_size = 0.0, late_size = 0.0;
	int    early_n = 0, late_n = 0;
	for (int i = 0; i != options->iterations; ++i) {
		if (i < half) { early_blocks += used_blocks_history[i]; early_size += (double)used_size_history[i]; early_n++; }
		else          { late_blocks  += used_blocks_history[i]; late_size  += (double)used_size_history[i]; late_n++; }
	}
	if (early_n) { early_blocks /= early_n; early_size /= early_n; }
	if (late_n)  { late_blocks  /= late_n;  late_size  /= late_n; }
	double block_drift = late_blocks - early_blocks;
	double size_drift  = late_size - early_size;

	printf("\nResults over %d reloads\n", options->iterations);
	platform_check(unloads_refused == 0, "engine.dll really was unmapped every time",
	               "%d unloads left the image mapped", unloads_refused);
	platform_check(stale_globals == 0, "engine.dll came back with a zeroed data segment",
	               "%d reattaches saw the previous generation's globals", stale_globals);
	platform_check(reload_failures == 0, "every reload preserved particle state",
	               "%d of %d iterations failed", reload_failures, options->iterations);
	platform_check(zero_population == 0, "population never empty at detach", "%d empty detaches", zero_population);
	platform_check(worst_boundary_motion <= motion_tolerance, "boundary motion stayed within one frame",
	               "worst %.4f vs tolerance %.4f (typical frame %.4f)", worst_boundary_motion, motion_tolerance, typical_frame_motion);
	platform_check(worst_boundary_count <= count_tolerance, "particle count stayed in the same ballpark",
	               "worst swing %d vs tolerance %d", worst_boundary_count, count_tolerance);
	platform_check(callback_stalls == 0, "re-registered update callback kept firing", "%d stalls", callback_stalls);
	platform_check(extra_pools == 0, "TimelineFX never grew past its first pool",
	               "%d iterations reported extra pools", extra_pools);
	if (early_n > 0 && late_n > 0) {
		platform_check(block_drift <= 2.0, "pool block count is flat across reloads",
		               "mean %.1f early -> %.1f late (drift %+.1f blocks)", early_blocks, late_blocks, block_drift);
		platform_check(size_drift <= early_size * 0.05, "pool bytes in use are flat across reloads",
		               "mean %.0f KB early -> %.0f KB late", early_size / 1024.0, late_size / 1024.0);
	} else {
		printf("  [SKIP] pool drift needs at least 2 iterations to compare halves\n");
	}
	platform_check(harness.pool.cursor == pool_cursor_after_init, "platform never handed out a second pool",
	               "cursor %zu MB unchanged since init", harness.pool.cursor / (1024 * 1024));

	/* --- Clean shutdown --- */
	char shutdown_text[8192];
	int captured = harness_shutdown_and_capture(&harness, shutdown_text, sizeof(shutdown_text));
	int reported_success = captured && strstr(shutdown_text, "Successful shutdown of TimelineFX") != NULL;
	int reported_leak    = captured && strstr(shutdown_text, "memory leak") != NULL;

	printf("\nShutdown\n");
	platform_check(captured, "shutdown output captured", "%s", captured ? "ok" : "could not redirect stdout");
	platform_check(reported_success, "TimelineFX reported a clean shutdown", "%s",
	               reported_success ? "\"Successful shutdown of TimelineFX.\"" : "success message absent");
	platform_check(!reported_leak, "TimelineFX reported no leaked blocks", "%s",
	               reported_leak ? "leak report present" : "no leak report");
	platform_check(harness.pool.live_allocations == 0, "every platform pool block was returned",
	               "%d still outstanding of %d total", harness.pool.live_allocations, harness.pool.total_allocations);
	if (captured && (!reported_success || reported_leak)) {
		printf("\n--- engine shutdown output ---\n%s\n------------------------------\n", shutdown_text);
	}

	module_unload(&harness.module);
	RemoveDirectoryA(harness.module.scratch_dir);
	cloud_destroy(&before);
	cloud_destroy(&after);
	cloud_destroy(&previous);
	free(used_blocks_history);
	free(used_size_history);
	platform_pool_destroy(&harness.pool);

	printf("\n%s (%d failed check%s)\n", platform_failures ? "FAILED" : "PASSED",
	       platform_failures, platform_failures == 1 ? "" : "s");
	return platform_failures ? HARNESS_FAIL : HARNESS_PASS;
}

/* ------------------------------------------------------------------------- */
/* Negative cases. Each runs in its own process under --negatives, because one */
/* of the three is supposed to take the process down.                          */
/* ------------------------------------------------------------------------- */

static int run_negative_case(const options_t *options) {
	options_t local = *options;
	if (local.warmup_frames > 200) local.warmup_frames = 200;
	local.verbose = 1;   /* a case designed to fail should say everything it can first */

	harness_t harness;
	if (!start_simulation(&harness, &local)) return HARNESS_FAIL;

	for (int frame = 0; frame != local.warmup_frames; ++frame) harness.module.update(TFX_RELOAD_FIXED_TIMESTEP);

	engine_sample_t pre;
	harness.module.sample(&pre);
	printf("[%s] warmed up: %u particles, %d worker threads\n", options->negative_case, pre.particle_count, pre.thread_count);
	if (pre.particle_count == 0) {
		fprintf(stderr, "[%s] no particles - the case would prove nothing\n", options->negative_case);
		return HARNESS_FAIL;
	}

	unsigned int detach_flags   = ENGINE_DETACH_DEFAULT;
	unsigned int reattach_flags = ENGINE_REATTACH_DEFAULT;

	if (strcmp(options->negative_case, "no-suspend") == 0) {
		detach_flags &= ~ENGINE_DETACH_SUSPEND;
		printf("[no-suspend] calling FreeLibrary with the per-stage update thread still parked in engine.dll\n");
	} else if (strcmp(options->negative_case, "no-setcontext") == 0) {
		reattach_flags &= ~ENGINE_REATTACH_SET_CONTEXT;
		printf("[no-setcontext] reloading without adopting the context into the new module\n");
	} else if (strcmp(options->negative_case, "drop-callback") == 0) {
		reattach_flags &= ~ENGINE_REATTACH_REBIND_CALLBACK;
		printf("[drop-callback] reloading without re-registering the update callback tfx_SetContext nulled\n");
	} else {
		fprintf(stderr, "[platform] unknown case '%s'\n", options->negative_case);
		return HARNESS_FAIL;
	}
	fflush(stdout);

	if (!harness_reload(&harness, detach_flags, reattach_flags)) {
		printf("[%s] reload refused (engine_reattach returned false)\n", options->negative_case);
		return HARNESS_NEGATIVE_EXPECTED;
	}

	for (int frame = 0; frame != 30; ++frame) harness.module.update(TFX_RELOAD_FIXED_TIMESTEP);
	engine_sample_t post;
	harness.module.sample(&post);
	fflush(stdout);

	if (strcmp(options->negative_case, "no-suspend") == 0) {
		/*
			The roadmap expected a crash from the parked update thread. On Windows
			with the MSVC CRT it is worse than that and quieter: TimelineFX creates
			its threads with _beginthreadex, which holds a module reference for the
			lifetime of each thread, so FreeLibrary returns success and unloads
			nothing. The old image stays mapped, its globals keep their values, and
			the "reload" goes on running the previous generation's code. The failure
			is the refused unload, so that is what this asserts.
		*/
		printf("[no-suspend] old image after FreeLibrary: %s\n",
		       harness.module.unload_left_it_mapped ? "STILL MAPPED" : "unmapped");
		printf("[no-suspend] engine globals after reattach: %s\n",
		       post.globals_were_zeroed ? "zeroed (a real reload)" : "carried over (no reload happened)");
		if (harness.module.unload_left_it_mapped || !post.globals_were_zeroed) {
			printf("[no-suspend] the unload was silently refused - suspend is what releases the module\n");
			return HARNESS_NEGATIVE_EXPECTED;
		}
		printf("[no-suspend] the module unloaded and nothing went wrong - suspend appears not to be required\n");
		return HARNESS_NEGATIVE_SURVIVED;
	}

	if (strcmp(options->negative_case, "drop-callback") == 0) {
		/* Expected: the callback stops firing. Silent, not fatal. */
		int stopped = (post.callback_hits == pre.callback_hits);
		printf("[drop-callback] callback hits %u before, %u after 30 further frames\n", pre.callback_hits, post.callback_hits);
		if (stopped) {
			printf("[drop-callback] behaviour stopped without a crash, as documented\n");
			return HARNESS_NEGATIVE_EXPECTED;
		}
		printf("[drop-callback] callback kept firing - tfx_SetContext did not null it\n");
		return HARNESS_NEGATIVE_SURVIVED;
	}

	printf("[%s] survived the reload - this case was supposed to take the process down\n", options->negative_case);
	return HARNESS_NEGATIVE_SURVIVED;
}

static int spawn_case(const char *case_name, const options_t *options, DWORD *out_exit_code) {
	char exe_path[MAX_PATH];
	GetModuleFileNameA(NULL, exe_path, MAX_PATH);

	char command_line[MAX_PATH * 3];
	snprintf(command_line, sizeof(command_line), "\"%s\" --case=%s --library=%s --effect=%s",
	         exe_path, case_name, options->library_path, options->effect_name);

	STARTUPINFOA startup = { sizeof(startup) };
	PROCESS_INFORMATION process = { 0 };
	if (!CreateProcessA(NULL, command_line, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &process)) {
		fprintf(stderr, "[platform] could not spawn %s (error %lu)\n", case_name, GetLastError());
		return 0;
	}
	WaitForSingleObject(process.hProcess, 120000);
	GetExitCodeProcess(process.hProcess, out_exit_code);
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return 1;
}

static int run_negatives(const options_t *options) {
	static const char *cases[] = { "no-suspend", "no-setcontext", "drop-callback" };
	static const char *expectation[] = {
		"FreeLibrary silently refuses to unload - the CRT pins the module per live thread",
		"process dies - the first TimelineFX touch goes through a context this module does not have",
		"callback silently stops firing, no crash"
	};
	/* Only no-setcontext is expected to take the process down. See the no-suspend
	   branch in run_negative_case for why that one fails quietly instead. */
	static const int wants_crash[] = { 0, 1, 0 };

	printf("Negative cases - each runs in its own process\n");
	printf("A harness that passes for the wrong reason is worse than no harness, so each\n");
	printf("of these must fail in the documented way.\n\n");

	for (int i = 0; i != 3; ++i) {
		DWORD exit_code = 0;
		printf("--- %s ---\n  expected: %s\n", cases[i], expectation[i]);
		if (!spawn_case(cases[i], options, &exit_code)) { platform_failures++; continue; }

		int crashed = (exit_code != HARNESS_PASS && exit_code != HARNESS_FAIL &&
		               exit_code != HARNESS_NEGATIVE_EXPECTED && exit_code != HARNESS_NEGATIVE_SURVIVED);

		if (wants_crash[i]) {
			platform_check(crashed, cases[i], "exit code 0x%08lX%s", exit_code,
			               crashed ? " (abnormal termination, as expected)" : "");
		} else {
			platform_check(exit_code == HARNESS_NEGATIVE_EXPECTED, cases[i], "exit code %lu%s", exit_code,
			               exit_code == HARNESS_NEGATIVE_EXPECTED ? " (failed quietly, as expected)" : "");
		}
		printf("\n");
	}

	printf("%s (%d failed check%s)\n", platform_failures ? "FAILED" : "PASSED",
	       platform_failures, platform_failures == 1 ? "" : "s");
	return platform_failures ? HARNESS_FAIL : HARNESS_PASS;
}

int main(int argc, char **argv) {
	setvbuf(stdout, NULL, _IONBF, 0);
	/* Asserts and faults go to stderr, never to a modal dialog: one negative case is
	   supposed to take the process down and nothing may block on input. */
	platform_install_crash_reporting();

	options_t options;
	options_parse(&options, argc, argv);

	if (options.negative_case) return run_negative_case(&options);
	if (options.run_negatives) return run_negatives(&options);
	return run_positive(&options);
}
