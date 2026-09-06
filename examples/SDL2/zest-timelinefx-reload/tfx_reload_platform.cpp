/*
	The persistent half of the harness. See tfx_reload_platform.h.
*/
#include "tfx_reload_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <io.h>
#include <crtdbg.h>

int platform_failures;

/* ------------------------------------------------------------------------- */
/* Pool                                                                       */
/* ------------------------------------------------------------------------- */

void *platform_allocate(void *user_data, size_t size, size_t alignment) {
	platform_pool_t *pool = (platform_pool_t *)user_data;
	if (alignment < 16) alignment = 16;
	size_t offset = (pool->cursor + (alignment - 1)) & ~(alignment - 1);
	if (offset + size > pool->reserved) {
		fprintf(stderr, "[platform] pool exhausted: wanted %zu bytes at offset %zu of %zu\n", size, offset, pool->reserved);
		return NULL;
	}
	pool->cursor = offset + size;
	pool->live_allocations++;
	pool->total_allocations++;
	pool->live_bytes += size;
	return pool->base + offset;
}

void platform_deallocate(void *user_data, void *memory, size_t size, size_t alignment) {
	platform_pool_t *pool = (platform_pool_t *)user_data;
	(void)memory;
	(void)alignment;
	pool->live_allocations--;
	pool->live_bytes -= size;
}

int platform_pool_create(platform_pool_t *pool, size_t reserved) {
	memset(pool, 0, sizeof(*pool));
	pool->base = (unsigned char *)VirtualAlloc(NULL, reserved, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	pool->reserved = reserved;
	return pool->base != NULL;
}

void platform_pool_destroy(platform_pool_t *pool) {
	if (pool->base) VirtualFree(pool->base, 0, MEM_RELEASE);
	pool->base = NULL;
}

/* ------------------------------------------------------------------------- */
/* Module loading                                                             */
/*                                                                            */
/* Each generation is loaded from its own copy of the DLL. That forces a fresh */
/* image rather than letting Windows hand back the one it just unmapped, which */
/* is what a real iterate-and-rebuild loop would produce.                      */
/* ------------------------------------------------------------------------- */

int module_resolve_paths(engine_module_t *module) {
	char exe_path[MAX_PATH];
	if (!GetModuleFileNameA(NULL, exe_path, MAX_PATH)) return 0;
	char *slash = strrchr(exe_path, '\\');
	if (!slash) return 0;
	*slash = 0;
	snprintf(module->source_path, MAX_PATH, "%s\\zest-tfx-reload-engine.dll", exe_path);
	snprintf(module->scratch_dir, MAX_PATH, "%s\\tfx-reload-scratch-%lu", exe_path, GetCurrentProcessId());
	CreateDirectoryA(module->scratch_dir, NULL);
	if (GetFileAttributesA(module->source_path) == INVALID_FILE_ATTRIBUTES) {
		fprintf(stderr, "[platform] engine dll not found at %s\n", module->source_path);
		return 0;
	}
	return 1;
}

int module_load(engine_module_t *module) {
	char next_path[MAX_PATH];
	snprintf(next_path, MAX_PATH, "%s\\engine_gen%03d.dll", module->scratch_dir, module->generation);
	if (!CopyFileA(module->source_path, next_path, FALSE)) {
		fprintf(stderr, "[platform] could not copy engine dll to %s (error %lu)\n", next_path, GetLastError());
		return 0;
	}
	module->handle = LoadLibraryA(next_path);
	if (!module->handle) {
		fprintf(stderr, "[platform] LoadLibrary failed for %s (error %lu)\n", next_path, GetLastError());
		return 0;
	}
	strcpy(module->loaded_path, next_path);

	module->init             = (engine_init_fn)GetProcAddress(module->handle, "engine_init");
	module->reattach         = (engine_reattach_fn)GetProcAddress(module->handle, "engine_reattach");
	module->detach           = (engine_detach_fn)GetProcAddress(module->handle, "engine_detach");
	module->update           = (engine_update_fn)GetProcAddress(module->handle, "engine_update");
	module->particle_count   = (engine_particle_count_fn)GetProcAddress(module->handle, "engine_particle_count");
	module->sample           = (engine_sample_fn)GetProcAddress(module->handle, "engine_sample");
	module->sample_positions = (engine_sample_positions_fn)GetProcAddress(module->handle, "engine_sample_positions");
	module->shutdown         = (engine_shutdown_fn)GetProcAddress(module->handle, "engine_shutdown");
	module->render_data      = (engine_render_data_fn)GetProcAddress(module->handle, "engine_render_data");
	module->load_library     = (engine_load_library_fn)GetProcAddress(module->handle, "engine_load_library");
	module->library_blob     = (engine_library_blob_fn)GetProcAddress(module->handle, "engine_library_blob");
	module->set_camera       = (engine_set_camera_fn)GetProcAddress(module->handle, "engine_set_camera");
	module->shape_count_in_file = (engine_shape_count_in_file_fn)GetProcAddress(module->handle, "engine_shape_count_in_file");
	module->start_effect     = (engine_start_effect_fn)GetProcAddress(module->handle, "engine_start_effect");
	module->finalise_library = (engine_finalise_library_fn)GetProcAddress(module->handle, "engine_finalise_library");
	module->image_info       = (engine_image_info_fn)GetProcAddress(module->handle, "engine_image_info");
	module->image_set_pointer = (engine_image_set_pointer_fn)GetProcAddress(module->handle, "engine_image_set_pointer");

	if (!module->init || !module->reattach || !module->detach || !module->update ||
	    !module->particle_count || !module->sample || !module->sample_positions || !module->shutdown ||
	    !module->render_data || !module->load_library || !module->library_blob || !module->set_camera ||
	    !module->shape_count_in_file || !module->start_effect || !module->finalise_library ||
	    !module->image_info || !module->image_set_pointer) {
		fprintf(stderr, "[platform] engine dll is missing one or more exports\n");
		return 0;
	}
	module->generation++;
	return 1;
}

void module_unload(engine_module_t *module) {
	char previous[MAX_PATH];
	strcpy(previous, module->loaded_path);
	HMODULE unloading = module->handle;
	FreeLibrary(module->handle);
	/*
		The whole test hinges on the image really being gone, not merely refcounted
		down, so confirm it rather than assume it. FreeLibrary reports success either
		way: MSVC's _beginthreadex holds a module reference for the lifetime of every
		thread whose start routine lives in that module, so a TimelineFX instance that
		was not suspended keeps engine.dll pinned and the unload quietly does nothing.
	*/
	MEMORY_BASIC_INFORMATION info = { 0 };
	SIZE_T queried = VirtualQuery((LPCVOID)unloading, &info, sizeof(info));
	module->unload_left_it_mapped = (queried && info.State != MEM_FREE) ? 1 : 0;
	if (module->verbose) {
		printf("[platform] gen %d base %p after FreeLibrary: %s\n", module->generation - 1, (void *)unloading,
		       module->unload_left_it_mapped ? "STILL MAPPED" : "unmapped");
	}
	module->handle = NULL;
	module->init = NULL; module->reattach = NULL; module->detach = NULL; module->update = NULL;
	module->particle_count = NULL; module->sample = NULL; module->sample_positions = NULL;
	module->shutdown = NULL; module->render_data = NULL; module->load_library = NULL;
	module->library_blob = NULL; module->set_camera = NULL; module->shape_count_in_file = NULL;
	module->start_effect = NULL; module->finalise_library = NULL;
	module->image_info = NULL; module->image_set_pointer = NULL;
	DeleteFileA(previous);
	module->loaded_path[0] = 0;
}

/* ------------------------------------------------------------------------- */
/* The reload sequence                                                        */
/* ------------------------------------------------------------------------- */

int harness_reload(harness_t *harness, unsigned int detach_flags, unsigned int reattach_flags) {
	harness->module.detach(&harness->saved, detach_flags);
	module_unload(&harness->module);
	if (!module_load(&harness->module)) return 0;
	return harness->module.reattach(&harness->saved, &harness->callbacks, reattach_flags);
}

int harness_start(harness_t *harness, size_t pool_size, const char *library_path, int verbose) {
	memset(harness, 0, sizeof(*harness));
	harness->verbose = verbose;
	harness->module.verbose = verbose;

	if (GetFileAttributesA(library_path) == INVALID_FILE_ATTRIBUTES) {
		fprintf(stderr, "[platform] effects library not found: %s\n", library_path);
		fprintf(stderr, "[platform] run from the repository root, or pass --library=<path>\n");
		return 0;
	}
	/* Reserve more than the pool so a growth request is satisfiable and visible
	   rather than turning into an out-of-memory abort. */
	if (!platform_pool_create(&harness->pool, pool_size * 2)) {
		fprintf(stderr, "[platform] VirtualAlloc failed\n");
		return 0;
	}
	harness->callbacks.user_data  = &harness->pool;
	harness->callbacks.allocate   = platform_allocate;
	harness->callbacks.deallocate = platform_deallocate;

	if (!module_resolve_paths(&harness->module)) return 0;
	if (!module_load(&harness->module)) return 0;
	if (verbose) printf("[platform] loaded %s\n", harness->module.loaded_path);

	if (!harness->module.init(harness->pool.base, pool_size, &harness->callbacks)) {
		fprintf(stderr, "[platform] engine_init failed\n");
		return 0;
	}
	return 1;
}

/* tfx_EndTimelineFX prints its leak report to stdout from inside engine.dll, so
   the only way to assert on it is to read it back. */
int harness_shutdown_and_capture(harness_t *harness, char *out_text, size_t out_size) {
	char capture_path[MAX_PATH];
	snprintf(capture_path, MAX_PATH, "%s\\shutdown.txt", harness->module.scratch_dir);

	out_text[0] = 0;
	fflush(stdout);
	int saved_stdout = _dup(_fileno(stdout));
	FILE *capture = fopen(capture_path, "w+");
	if (!capture || saved_stdout < 0) {
		harness->module.shutdown();
		if (capture) fclose(capture);
		return 0;
	}
	_dup2(_fileno(capture), _fileno(stdout));

	harness->module.shutdown();

	fflush(stdout);
	_dup2(saved_stdout, _fileno(stdout));
	_close(saved_stdout);
	fclose(capture);

	FILE *read_back = fopen(capture_path, "rb");
	if (!read_back) return 0;
	size_t read = fread(out_text, 1, out_size - 1, read_back);
	out_text[read] = 0;
	fclose(read_back);
	DeleteFileA(capture_path);
	return 1;
}

/* ------------------------------------------------------------------------- */
/* Crash reporting and result reporting                                       */
/* ------------------------------------------------------------------------- */

/* A hardware fault here is a result, not an accident: one of the negative cases is
   supposed to produce one. Printing the faulting module, its RVA and the address
   that was touched is the difference between "it died" and knowing why. */
static LONG CALLBACK platform_report_fault(EXCEPTION_POINTERS *info) {
	DWORD code = info->ExceptionRecord->ExceptionCode;
	if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION &&
	    code != EXCEPTION_IN_PAGE_ERROR && code != EXCEPTION_PRIV_INSTRUCTION &&
	    code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_DATATYPE_MISALIGNMENT) {
		return EXCEPTION_CONTINUE_SEARCH;
	}
	void *address = info->ExceptionRecord->ExceptionAddress;
	HMODULE module = NULL;
	char module_name[MAX_PATH] = "<unknown>";
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                   (LPCSTR)address, &module);
	if (module) GetModuleFileNameA(module, module_name, MAX_PATH);
	fflush(stdout);
	fprintf(stderr, "\n[platform] fault 0x%08lX at %p", code, address);
	if (module) fprintf(stderr, " (%s + 0x%llX)", module_name, (unsigned long long)((char *)address - (char *)module));
	fprintf(stderr, "\n");
	if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR) {
		fprintf(stderr, "[platform] %s address %p\n",
		        info->ExceptionRecord->ExceptionInformation[0] ? "write to" : "read from",
		        (void *)info->ExceptionRecord->ExceptionInformation[1]);
	}
	fflush(stderr);
	return EXCEPTION_CONTINUE_SEARCH;
}

void platform_install_crash_reporting(void) {
	AddVectoredExceptionHandler(1, platform_report_fault);
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
}

void platform_check(int condition, const char *what, const char *detail_format, ...) {
	va_list args;
	printf("  [%s] %-48s ", condition ? "PASS" : "FAIL", what);
	va_start(args, detail_format);
	vprintf(detail_format, args);
	va_end(args);
	printf("\n");
	if (!condition) platform_failures++;
}
