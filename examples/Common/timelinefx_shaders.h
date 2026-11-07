#pragma once

//Every TimelineFX shader is an entry point in the one Slang module that ships with the library, compiled here at start up.
//Include after zest.h in the file that defines ZEST_IMPLEMENTATION.

#include "impl_timelinefx.h"

#define TIMELINEFX_SHADER_SEARCH_PATH "submodules/timelinefxlib/Shaders"
#define TIMELINEFX_SHADER_MODULE "timelinefx"

#ifdef ZEST_ENABLE_SLANG
#include "impl_slang.hpp"

//The session caches the parsed module so each entry point after the first only costs code generation, and keeps the shaders hot reloadable
inline zest_slang_session CreateTimelineFXShaderSession(zest_device device) {
	zest_slang_InitialiseSession(device);
	const char *search_paths[] = { TIMELINEFX_SHADER_SEARCH_PATH };
	zest_slang_session_info_t info = zest_slang_DefaultSessionInfo();
	info.search_paths = search_paths;
	info.search_path_count = 1;
	info.optimization_level = zest_slang_optimization_high;
	zest_slang_session session = zest_slang_CreateSession(device, &info);
	if (!session) {
		ZEST_PRINT("Could not create a Slang session for the TimelineFX shaders: %s", zest_slang_GetCreateSessionError(device));
	}
	return session;
}

inline zest_shader_handle CreateTimelineFXShader(zest_slang_session session, const char *entry_point, zest_shader_type type) {
	zest_slang_entry_point_info_t info = {};
	info.module = TIMELINEFX_SHADER_MODULE;
	info.entry_point = entry_point;
	info.type = type;
	zest_shader_handle shader = zest_slang_CreateShaderFromSession(session, &info, entry_point);
	if (!shader.value) {
		ZEST_PRINT("Could not compile the TimelineFX shader %s: %s", entry_point, zest_slang_GetLastError(session));
	}
	return shader;
}

//Call before zest_DestroyDevice
inline void FreeTimelineFXShaderSession(zest_device device, zest_slang_session session) {
	zest_slang_FreeSession(session);
	zest_slang_Shutdown(device);
}
#else
//ZEST_SLANG_CONFIGS left Slang out of this configuration, so these only let the example compile and main exits before using them
typedef struct zest_slang_session_t *zest_slang_session;
inline zest_slang_session CreateTimelineFXShaderSession(zest_device device) { return NULL; }
inline zest_shader_handle CreateTimelineFXShader(zest_slang_session session, const char *entry_point, zest_shader_type type) { return zest_shader_handle{}; }
inline void FreeTimelineFXShaderSession(zest_device device, zest_slang_session session) {}
#endif

//The entry points zest_tfx_InitLayerPipelines draws layers that write depth and lit layers with
inline zest_tfx_layer_shaders_t CreateTimelineFXLayerShaders(zest_slang_session session) {
	zest_tfx_layer_shaders_t shaders = {};
	shaders.billboard_cutout_fragment = CreateTimelineFXShader(session, "tfx_billboard_cutout_fragment", zest_fragment_shader);
	shaders.billboard_lit_vertex = CreateTimelineFXShader(session, "tfx_billboard_lit_vertex", zest_vertex_shader);
	shaders.billboard_lit_fragment = CreateTimelineFXShader(session, "tfx_billboard_lit_fragment", zest_fragment_shader);
	shaders.billboard_lit_cutout_fragment = CreateTimelineFXShader(session, "tfx_billboard_lit_cutout_fragment", zest_fragment_shader);
	shaders.ribbon_cutout_fragment = CreateTimelineFXShader(session, "tfx_ribbon_cutout_fragment", zest_fragment_shader);
	shaders.ribbon_lit_vertex = CreateTimelineFXShader(session, "tfx_ribbon_lit_vertex", zest_vertex_shader);
	shaders.ribbon_lit_fragment = CreateTimelineFXShader(session, "tfx_ribbon_lit_fragment", zest_fragment_shader);
	shaders.ribbon_lit_cutout_fragment = CreateTimelineFXShader(session, "tfx_ribbon_lit_cutout_fragment", zest_fragment_shader);
	return shaders;
}

inline bool TimelineFXShadersAvailable(const char *example_name) {
#ifdef ZEST_ENABLE_SLANG
	return true;
#else
	ZEST_PRINT("%s compiles the TimelineFX shaders with Slang, which isn't enabled for this configuration (see ZEST_SLANG_CONFIGS).", example_name);
	return false;
#endif
}
