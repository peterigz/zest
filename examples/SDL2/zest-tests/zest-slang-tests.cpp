#ifdef ZEST_ENABLE_SLANG
#include "impl_slang.hpp"
#include <string>

#define SLANG_TEST_SHADER_PATH "examples/SDL2/zest-tests/shaders/slang"
#define SLANG_TEST_RELOAD_PATH "zest_slang_hot_reload_test"
#define SLANG_TEST_PARTICLE_COUNT 100

struct SlangTestParticle {
	float position[3];
	zest_uint flags;
	float velocity[3];
	float age;
};

static zest_slang_session slang_test__create_session(zest_device device, const char *search_path, zest_bool scalar_block_layout) {
	if (!device->slang_info) {
		zest_slang_InitialiseSession(device);
	}
	zest_slang_session_info_t info = zest_slang_DefaultSessionInfo();
	info.search_paths = &search_path;
	info.search_path_count = 1;
	info.scalar_block_layout = scalar_block_layout;
	return zest_slang_CreateSession(device, &info);
}

static zest_slang_entry_point_info_t slang_test__entry(const char *entry_point, const char **link_module) {
	zest_slang_entry_point_info_t info = {};
	info.module = "kernel";
	info.entry_point = entry_point;
	info.type = zest_compute_shader;
	info.link_modules = link_module;
	info.link_module_count = link_module ? 1 : 0;
	return info;
}

static zest_bool slang_test__entry_point_is_main(const void *data, zest_size size) {
	const zest_uint *words = (const zest_uint *)data;
	zest_size word_count = size / 4;
	if (word_count < 5 || words[0] != 0x07230203) return ZEST_FALSE;
	zest_uint entry_point_count = 0;
	zest_size index = 5;
	while (index < word_count) {
		zest_uint instruction_word_count = words[index] >> 16;
		zest_uint opcode = words[index] & 0xFFFF;
		if (instruction_word_count == 0) return ZEST_FALSE;
		//OpEntryPoint: execution model, function id, then the name
		if (opcode == 15) {
			if (strcmp((const char *)&words[index + 3], "main") != 0) return ZEST_FALSE;
			entry_point_count++;
		}
		index += instruction_word_count;
	}
	return entry_point_count > 0;
}

static zest_bool slang_test__dispatch(ZestTests *tests, zest_compute_handle compute_handle, SlangTestParticle *out_particles) {
	zest_size size = sizeof(SlangTestParticle) * SLANG_TEST_PARTICLE_COUNT;
	zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_to_cpu);
	zest_buffer buffer = zest_CreateBuffer(tests->device, size, &buffer_info);
	if (!buffer) return ZEST_FALSE;
	memset(zest_BufferData(buffer), 0, size);
	zest_uint buffer_index = zest_AcquireStorageBufferIndex(tests->device, buffer);
	zest_uint push[4] = { buffer_index, SLANG_TEST_PARTICLE_COUNT, 0, 0 };
	zest_bool dispatched = ZEST_FALSE;
	zest_queue queue = zest_imm_BeginCommandBuffer(tests->device, zest_queue_compute);
	if (queue) {
		zest_imm_BindComputePipeline(queue, zest_GetCompute(compute_handle));
		zest_imm_SendPushConstants(queue, push, sizeof(push));
		dispatched = zest_imm_DispatchCompute(queue, (SLANG_TEST_PARTICLE_COUNT + 63) / 64, 1, 1);
		dispatched &= zest_imm_EndCommandBuffer(queue);
	}
	memcpy(out_particles, zest_BufferData(buffer), size);
	zest_ReleaseBindlessIndex(tests->device, buffer_index, zest_storage_buffer_binding);
	zest_FreeBuffer(buffer);
	return dispatched;
}

//Mirrors simulate() in kernel.slang
static zest_bool slang_test__check_particles(const SlangTestParticle *particles, float motion_scale, zest_bool user_collider, zest_uint marker) {
	for (int index = 0; index != SLANG_TEST_PARTICLE_COUNT; ++index) {
		const SlangTestParticle *particle = &particles[index];
		float expected_x = (float)index + motion_scale;
		if (particle->position[0] != expected_x || particle->position[1] != 1.f + motion_scale || particle->position[2] != 2.f + motion_scale) return ZEST_FALSE;
		zest_uint collision = user_collider ? (zest_uint)expected_x + 100 : 1;
		if (particle->flags != collision + marker * 1000) return ZEST_FALSE;
		if (particle->velocity[0] != 1.f || particle->velocity[1] != 1.f || particle->velocity[2] != 1.f || particle->age != 0.5f) return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

static zest_bool slang_test__run_and_check(ZestTests *tests, zest_shader_handle shader, float motion_scale, zest_bool user_collider, zest_uint marker) {
	zest_compute_handle compute = zest_CreateCompute(tests->device, "Slang Test", shader);
	if (!compute.value) return ZEST_FALSE;
	SlangTestParticle particles[SLANG_TEST_PARTICLE_COUNT];
	zest_bool passed = slang_test__dispatch(tests, compute, particles) && slang_test__check_particles(particles, motion_scale, user_collider, marker);
	zest_FreeCompute(compute);
	return passed;
}

static void slang_test__shutdown(zest_device device, zest_slang_session session) {
	zest_slang_FreeSession(session);
	zest_slang_Shutdown(device);
}

//Variants, generic specialisation and colliders from one session: common.slang parsed once, all entry points named main
int test__slang_variants_and_linking(ZestTests *tests, Test *test) {
	zest_device device = tests->device;
	zest_slang_session session = slang_test__create_session(device, SLANG_TEST_SHADER_PATH, ZEST_FALSE);
	if (!session) {
		test->result = 1;
		test->frame_count++;
		return test->result;
	}

	const char *default_collider = "default_collider";
	const char *user_collider = "user_collider";
	const char *motion_a = "MotionA";
	zest_slang_entry_point_info_t wrapper_a = slang_test__entry("simulate_a", &default_collider);
	zest_slang_entry_point_info_t wrapper_b = slang_test__entry("simulate_b", &default_collider);
	zest_slang_entry_point_info_t generic_a = slang_test__entry("simulate_generic", &default_collider);
	generic_a.type_arguments = &motion_a;
	generic_a.type_argument_count = 1;
	zest_slang_entry_point_info_t wrapper_a_user = slang_test__entry("simulate_a", &user_collider);

	zest_shader_handle shader_a = zest_slang_CreateShaderFromSession(session, &wrapper_a, "slang_simulate_a");
	slang::IModule *common_module = session->session->loadModule("common");
	SlangInt loaded_module_count = session->session->getLoadedModuleCount();
	zest_shader_handle shader_b = zest_slang_CreateShaderFromSession(session, &wrapper_b, "slang_simulate_b");
	zest_shader_handle shader_generic = zest_slang_CreateShaderFromSession(session, &generic_a, "slang_simulate_generic");
	//The session must hand back the module it already parsed rather than loading anything again
	if (session->session->loadModule("common") != common_module || session->session->getLoadedModuleCount() != loaded_module_count) {
		ZEST_PRINT("\tcommon.slang was loaded more than once");
		test->result |= 1;
	}
	zest_shader_handle shader_user = zest_slang_CreateShaderFromSession(session, &wrapper_a_user, "slang_simulate_a_user");

	if (!shader_a.value || !shader_b.value || !shader_generic.value || !shader_user.value) {
		ZEST_PRINT("\tSlang compile failed: %s", zest_slang_GetLastError(session));
		test->result |= 1;
	} else {
		zest_shader_handle shaders[] = { shader_a, shader_b, shader_generic, shader_user };
		for (int index = 0; index != 4; ++index) {
			zest_shader shader = zest_GetShader(shaders[index]);
			if (!slang_test__entry_point_is_main(zest_GetCompiledShader(shader), zest_GetCompiledShaderSize(shader))) {
				ZEST_PRINT("\tEntry point of shader %i is not named main", index);
				test->result |= 1;
			}
		}

		zest_slang_blob_t blob = {};
		if (!zest_slang_CompileToBinary(session, &generic_a, &blob)) {
			test->result |= 1;
		} else {
			zest_shader shader = zest_GetShader(shader_generic);
			if (blob.size != zest_GetCompiledShaderSize(shader) || memcmp(blob.data, zest_GetCompiledShader(shader), blob.size) != 0) {
				ZEST_PRINT("\tzest_GetCompiledShader doesn't match zest_slang_CompileToBinary");
				test->result |= 1;
			}
		}
		zest_slang_FreeBlob(&blob);

		if (!slang_test__run_and_check(tests, shader_a, 1.f, ZEST_FALSE, 1)) { ZEST_PRINT("\tsimulate_a gave wrong results"); test->result |= 1; }
		if (!slang_test__run_and_check(tests, shader_b, 2.f, ZEST_FALSE, 1)) { ZEST_PRINT("\tsimulate_b gave wrong results"); test->result |= 1; }
		if (!slang_test__run_and_check(tests, shader_generic, 1.f, ZEST_FALSE, 1)) { ZEST_PRINT("\tsimulate_generic<MotionA> gave wrong results"); test->result |= 1; }
		if (!slang_test__run_and_check(tests, shader_user, 1.f, ZEST_TRUE, 1)) { ZEST_PRINT("\tsimulate_a with user_collider gave wrong results"); test->result |= 1; }
	}

	if (shader_a.value) zest_FreeShader(shader_a);
	if (shader_b.value) zest_FreeShader(shader_b);
	if (shader_generic.value) zest_FreeShader(shader_generic);
	if (shader_user.value) zest_FreeShader(shader_user);
	slang_test__shutdown(device, session);
	test->result |= zest_GetValidationErrorCount(device);
	test->frame_count++;
	return test->result;
}

//Each failure stage reports its own result and a readable error
int test__slang_compile_failures(ZestTests *tests, Test *test) {
	zest_device device = tests->device;
	zest_slang_session session = slang_test__create_session(device, SLANG_TEST_SHADER_PATH, ZEST_FALSE);
	if (!session) {
		test->result = 1;
		test->frame_count++;
		return test->result;
	}
	zest_slang_blob_t blob = {};

	zest_slang_entry_point_info_t no_collider = slang_test__entry("simulate_a", NULL);
	if (zest_slang_CompileToBinary(session, &no_collider, &blob) || zest_slang_GetLastResult(session) != zest_slang_result_link_failed || !strstr(zest_slang_GetLastError(session), "Collider")) {
		ZEST_PRINT("\tExpected a link failure, got result %i: %s", zest_slang_GetLastResult(session), zest_slang_GetLastError(session));
		test->result |= 1;
	}
	if (zest_slang_CreateShaderFromSession(session, &no_collider, "slang_no_collider").value) {
		test->result |= 1;
	}

	const char *default_collider = "default_collider";
	zest_slang_entry_point_info_t missing_entry = slang_test__entry("does_not_exist", &default_collider);
	if (zest_slang_CompileToBinary(session, &missing_entry, &blob) || zest_slang_GetLastResult(session) != zest_slang_result_entry_point_not_found || !zest_slang_GetLastError(session)[0]) {
		test->result |= 1;
	}

	const char *missing_type = "NoSuchMotion";
	zest_slang_entry_point_info_t bad_type = slang_test__entry("simulate_generic", &default_collider);
	bad_type.type_arguments = &missing_type;
	bad_type.type_argument_count = 1;
	if (zest_slang_CompileToBinary(session, &bad_type, &blob) || zest_slang_GetLastResult(session) != zest_slang_result_type_not_found || !strstr(zest_slang_GetLastError(session), "NoSuchMotion")) {
		test->result |= 1;
	}

	zest_slang_entry_point_info_t missing_module = slang_test__entry("simulate_a", &default_collider);
	missing_module.module = "no_such_module";
	if (zest_slang_CompileToBinary(session, &missing_module, &blob) || zest_slang_GetLastResult(session) != zest_slang_result_module_load_failed || !zest_slang_GetLastError(session)[0]) {
		test->result |= 1;
	}

	//A good compile after failures clears the error
	zest_slang_entry_point_info_t good = slang_test__entry("simulate_a", &default_collider);
	if (!zest_slang_CompileToBinary(session, &good, &blob) || zest_slang_GetLastResult(session) != zest_slang_result_success || zest_slang_GetLastError(session)[0]) {
		test->result |= 1;
	}
	zest_slang_FreeBlob(&blob);

	//A compute entry point asked for as a vertex shader fails rather than producing the wrong shader type
	zest_slang_entry_point_info_t wrong_stage = slang_test__entry("simulate_a", &default_collider);
	wrong_stage.type = zest_vertex_shader;
	if (zest_slang_CompileToBinary(session, &wrong_stage, &blob) || zest_slang_GetLastResult(session) != zest_slang_result_entry_point_not_found || !strstr(zest_slang_GetLastError(session), "vertex")) {
		ZEST_PRINT("\tWrong stage reported result %i: %s", zest_slang_GetLastResult(session), zest_slang_GetLastError(session));
		test->result |= 1;
	}

	//Too many link modules is rejected rather than overrunning the compile's arrays
	const char *too_many_links[ZEST_SLANG_MAX_LINK_MODULES + 1];
	for (int index = 0; index != ZEST_SLANG_MAX_LINK_MODULES + 1; ++index) too_many_links[index] = default_collider;
	zest_slang_entry_point_info_t too_many = slang_test__entry("simulate_a", too_many_links);
	too_many.link_module_count = ZEST_SLANG_MAX_LINK_MODULES + 1;
	if (zest_slang_CompileToBinary(session, &too_many, &blob) || zest_slang_GetLastResult(session) != zest_slang_result_invalid_arguments) {
		test->result |= 1;
	}

	//A NULL session fails instead of crashing
	if (zest_slang_CompileToBinary(NULL, &good, &blob) || zest_slang_CreateShaderFromSession(NULL, &good, "slang_null").value || zest_slang_GetTypeSize(NULL, "common", "TestParticle") != 0 || zest_slang_GetLastResult(NULL) != zest_slang_result_invalid_arguments) {
		test->result |= 1;
	}

	//A session that can't be created still says why
	zest_slang_session_info_t bad_profile = zest_slang_DefaultSessionInfo();
	bad_profile.profile = "not_a_profile";
	zest_slang_session bad_session = zest_slang_CreateSession(device, &bad_profile);
	if (bad_session || !strstr(zest_slang_GetCreateSessionError(device), "not_a_profile")) {
		ZEST_PRINT("\tBad profile error: '%s'", zest_slang_GetCreateSessionError(device));
		test->result |= 1;
	}
	zest_slang_FreeSession(bad_session);

	slang_test__shutdown(device, session);
	test->result |= zest_GetValidationErrorCount(device);
	test->frame_count++;
	return test->result;
}

//StructuredBuffer strides, std430 by default and scalar when the session asks for it
int test__slang_type_size(ZestTests *tests, Test *test) {
	zest_device device = tests->device;
	zest_slang_session session = slang_test__create_session(device, SLANG_TEST_SHADER_PATH, ZEST_FALSE);
	if (!session) {
		test->result = 1;
		test->frame_count++;
		return test->result;
	}
	if (zest_slang_GetTypeSize(session, "common", "TestParticle") != 32) {
		ZEST_PRINT("\tTestParticle size %zu, expected 32", (size_t)zest_slang_GetTypeSize(session, "common", "TestParticle"));
		test->result |= 1;
	}
	if (zest_slang_GetTypeSize(session, "common", "PackedPair") != 32) {
		ZEST_PRINT("\tPackedPair std430 size %zu, expected 32", (size_t)zest_slang_GetTypeSize(session, "common", "PackedPair"));
		test->result |= 1;
	}
	if (zest_slang_GetTypeSize(session, "common", "NoSuchType") != 0 || zest_slang_GetLastResult(session) != zest_slang_result_type_not_found || !strstr(zest_slang_GetLastError(session), "NoSuchType")) {
		ZEST_PRINT("\tMissing type reported result %i: %s", zest_slang_GetLastResult(session), zest_slang_GetLastError(session));
		test->result |= 1;
	}
	if (zest_slang_GetTypeSize(session, "common", "TestParticle") != 32 || zest_slang_GetLastResult(session) != zest_slang_result_success || zest_slang_GetLastError(session)[0]) {
		ZEST_PRINT("\tA successful type lookup did not clear the previous error");
		test->result |= 1;
	}
	zest_slang_FreeSession(session);

	if (zest_DeviceFeatureEnabled(device, zest_capability_scalar_block_layout)) {
		zest_slang_session scalar_session = slang_test__create_session(device, SLANG_TEST_SHADER_PATH, ZEST_TRUE);
		if (!scalar_session || zest_slang_GetTypeSize(scalar_session, "common", "PackedPair") != 24 || zest_slang_GetTypeSize(scalar_session, "common", "TestParticle") != 32) {
			ZEST_PRINT("\tPackedPair scalar size %zu, expected 24", scalar_session ? (size_t)zest_slang_GetTypeSize(scalar_session, "common", "PackedPair") : 0);
			test->result |= 1;
		}
		zest_slang_FreeSession(scalar_session);
	} else {
		ZEST_PRINT("\tscalar_block_layout not supported, skipping the scalar layout check");
	}

	//Scalar layout without the device feature is refused rather than producing SPIR-V the device can't run
	zest_capability_flags enabled_capabilities = device->capabilities.enabled;
	device->capabilities.enabled &= ~zest_capability_scalar_block_layout;
	zest_slang_session refused_session = slang_test__create_session(device, SLANG_TEST_SHADER_PATH, ZEST_TRUE);
	device->capabilities.enabled = enabled_capabilities;
	if (refused_session || !strstr(zest_slang_GetCreateSessionError(device), "scalar_block_layout")) {
		ZEST_PRINT("\tScalar layout without the device feature was not refused: '%s'", zest_slang_GetCreateSessionError(device));
		test->result |= 1;
	}
	zest_slang_FreeSession(refused_session);

	zest_slang_Shutdown(device);
	test->frame_count++;
	return test->result;
}

//Debug SPIR-V only: Slang 2025.23.2 debug info crashes pipeline creation on AMD driver 32.0.21045.5002
int test__slang_debug_info(ZestTests *tests, Test *test) {
	zest_device device = tests->device;
	if (!device->slang_info) {
		zest_slang_InitialiseSession(device);
	}
	const char *search_path = SLANG_TEST_SHADER_PATH;
	zest_slang_session_info_t info = zest_slang_DefaultSessionInfo();
	info.search_paths = &search_path;
	info.search_path_count = 1;
	info.debug_info = ZEST_TRUE;
	info.optimization_level = zest_slang_optimization_none;
	zest_slang_session session = zest_slang_CreateSession(device, &info);
	const char *default_collider = "default_collider";
	zest_slang_entry_point_info_t entry = slang_test__entry("simulate_a", &default_collider);
	zest_shader_handle shader_handle = session ? zest_slang_CreateShaderFromSession(session, &entry, "slang_debug_info") : zest_shader_handle{};
	if (!shader_handle.value) {
		test->result |= 1;
	} else {
		zest_shader shader = zest_GetShader(shader_handle);
		std::string binary((const char *)zest_GetCompiledShader(shader), zest_GetCompiledShaderSize(shader));
		if (binary.find("NonSemantic.Shader.DebugInfo") == std::string::npos || binary.find("kernel.slang") == std::string::npos) {
			ZEST_PRINT("\tSPIR-V has no source level debug info");
			test->result |= 1;
		}
		zest_FreeShader(shader_handle);
	}
	slang_test__shutdown(device, session);
	test->result |= zest_GetValidationErrorCount(device);
	test->frame_count++;
	return test->result;
}

static zest_bool slang_test__write_file(const char *path, const std::string &text) {
	FILE *file = fopen(path, "wb");
	if (!file) return ZEST_FALSE;
	fwrite(text.data(), 1, text.size(), file);
	fclose(file);
	return ZEST_TRUE;
}

static std::string slang_test__read_file(const char *path) {
	std::string text;
	FILE *file = fopen(path, "rb");
	if (!file) return text;
	char chunk[4096];
	size_t read = 0;
	while ((read = fread(chunk, 1, sizeof(chunk), file)) > 0) {
		text.append(chunk, read);
	}
	fclose(file);
	return text;
}

//Rewrites the file until its mtime moves, so the reload check can't miss the edit
static zest_bool slang_test__edit_file(const char *path, const std::string &text) {
	zest_u64 before = 0;
	zest_GetFileModifiedTime(path, &before);
	for (int attempt = 0; attempt != 100; ++attempt) {
		if (!slang_test__write_file(path, text)) return ZEST_FALSE;
		zest_u64 after = 0;
		if (zest_GetFileModifiedTime(path, &after) && after != before) return ZEST_TRUE;
		SDL_Delay(10);
	}
	return ZEST_FALSE;
}

static std::string slang_test__with_marker(const std::string &common_source, const char *marker) {
	std::string text = common_source;
	size_t position = text.find("COMMON_MARKER = 1;");
	if (position != std::string::npos) {
		text.replace(position, strlen("COMMON_MARKER = 1;"), std::string("COMMON_MARKER = ") + marker);
	}
	return text;
}

static zest_bool slang_test__blobs_equal(const zest_slang_blob_t *first, const zest_slang_blob_t *second) {
	return first->size == second->size && memcmp(first->data, second->data, first->size) == 0;
}

//Hot reload through imports, syntax errors, link failures and broken or missing new imports
int test__slang_hot_reload(ZestTests *tests, Test *test) {
	zest_device device = tests->device;
	const char *files[] = { "common.slang", "kernel.slang", "default_collider.slang", "extra.slang", "later.slang" };
	zest__create_folder(device, SLANG_TEST_RELOAD_PATH);
	std::string sources[3];
	for (int index = 0; index != 3; ++index) {
		sources[index] = slang_test__read_file((std::string(SLANG_TEST_SHADER_PATH "/") + files[index]).c_str());
		if (sources[index].empty() || !slang_test__write_file((std::string(SLANG_TEST_RELOAD_PATH "/") + files[index]).c_str(), sources[index])) {
			test->result |= 1;
		}
	}
	const std::string &common_source = sources[0];
	const char *common_path = SLANG_TEST_RELOAD_PATH "/common.slang";
	const char *kernel_path = SLANG_TEST_RELOAD_PATH "/kernel.slang";
	const char *collider_path = SLANG_TEST_RELOAD_PATH "/default_collider.slang";
	const char *extra_path = SLANG_TEST_RELOAD_PATH "/extra.slang";
	const char *later_path = SLANG_TEST_RELOAD_PATH "/later.slang";
	remove(later_path);

	zest_slang_session session = test->result ? NULL : slang_test__create_session(device, SLANG_TEST_RELOAD_PATH, ZEST_FALSE);
	const char *default_collider = "default_collider";
	zest_slang_entry_point_info_t entry = slang_test__entry("simulate_a", &default_collider);
	zest_shader_handle shader_handle = session ? zest_slang_CreateShaderFromSession(session, &entry, "slang_hot_reload") : zest_shader_handle{};
	zest_compute_handle compute = shader_handle.value ? zest_CreateCompute(device, "Slang Hot Reload", shader_handle) : zest_compute_handle{};
	if (!compute.value) {
		test->result |= 1;
	} else {
		zest_SetShaderHotReload(shader_handle, ZEST_TRUE);
		SlangTestParticle particles[SLANG_TEST_PARTICLE_COUNT];

		if (!slang_test__dispatch(tests, compute, particles) || !slang_test__check_particles(particles, 1.f, ZEST_FALSE, 1)) {
			ZEST_PRINT("\tInitial dispatch gave wrong results");
			test->result |= 1;
		}
		if (zest_CheckShaderHotReload(device) != 0) {
			ZEST_PRINT("\tShader reloaded without any edits");
			test->result |= 1;
		}

		//Edit the import. A direct compile must see the edit rather than the session's cached module.
		zest_slang_blob_t before_edit = {};
		zest_slang_blob_t after_edit = {};
		zest_slang_CompileToBinary(session, &entry, &before_edit);
		if (!slang_test__edit_file(common_path, slang_test__with_marker(common_source, "2;"))) {
			test->result |= 1;
		}
		if (!zest_slang_CompileToBinary(session, &entry, &after_edit) || slang_test__blobs_equal(&before_edit, &after_edit)) {
			ZEST_PRINT("\tzest_slang_CompileToBinary returned a stale result after an edit");
			test->result |= 1;
		}
		zest_slang_FreeBlob(&before_edit);
		zest_slang_FreeBlob(&after_edit);
		if (zest_CheckShaderHotReload(device) != 1) {
			ZEST_PRINT("\tEditing common.slang did not reload the shader");
			test->result |= 1;
		}
		if (zest_GetShaderLastError(shader_handle)[0] || !slang_test__dispatch(tests, compute, particles) || !slang_test__check_particles(particles, 1.f, ZEST_FALSE, 2)) {
			ZEST_PRINT("\tReloaded shader gave wrong results");
			test->result |= 1;
		}

		//Break it: the old binary keeps running and the error is reported
		zest_shader shader = zest_GetShader(shader_handle);
		std::string good_binary((const char *)zest_GetCompiledShader(shader), zest_GetCompiledShaderSize(shader));
		if (!slang_test__edit_file(common_path, slang_test__with_marker(common_source, ";")) || zest_CheckShaderHotReload(device) != 0) {
			test->result |= 1;
		}
		if (!zest_GetShaderLastError(shader_handle)[0]) {
			ZEST_PRINT("\tSyntax error did not set the shader's last error");
			test->result |= 1;
		}
		if (good_binary.size() != zest_GetCompiledShaderSize(shader) || memcmp(good_binary.data(), zest_GetCompiledShader(shader), good_binary.size()) != 0) {
			ZEST_PRINT("\tFailed reload changed the shader binary");
			test->result |= 1;
		}
		if (!slang_test__dispatch(tests, compute, particles) || !slang_test__check_particles(particles, 1.f, ZEST_FALSE, 2)) {
			ZEST_PRINT("\tPrevious binary stopped working after a failed reload");
			test->result |= 1;
		}

		//Fix it again
		if (!slang_test__edit_file(common_path, slang_test__with_marker(common_source, "3;")) || zest_CheckShaderHotReload(device) != 1) {
			ZEST_PRINT("\tFixing common.slang did not reload the shader: %s", zest_GetShaderLastError(shader_handle));
			test->result |= 1;
		}
		if (zest_GetShaderLastError(shader_handle)[0] || !slang_test__dispatch(tests, compute, particles) || !slang_test__check_particles(particles, 1.f, ZEST_FALSE, 3)) {
			ZEST_PRINT("\tShader gave wrong results after fixing the error");
			test->result |= 1;
		}

		//Remove the extern Collider export: modules still load, the link fails, and restoring it must reload
		std::string collider_source = sources[2];
		size_t export_position = collider_source.find("export struct Collider");
		if (export_position != std::string::npos) {
			collider_source.erase(export_position, collider_source.find(';', export_position) + 1 - export_position);
		}
		if (!slang_test__edit_file(collider_path, collider_source) || zest_CheckShaderHotReload(device) != 0 || !strstr(zest_GetShaderLastError(shader_handle), "Collider")) {
			ZEST_PRINT("\tRemoving the Collider export did not fail to link: %s", zest_GetShaderLastError(shader_handle));
			test->result |= 1;
		}
		if (!slang_test__edit_file(collider_path, sources[2]) || zest_CheckShaderHotReload(device) != 1) {
			ZEST_PRINT("\tRestoring the Collider export did not reload the shader: %s", zest_GetShaderLastError(shader_handle));
			test->result |= 1;
		}
		if (zest_GetShaderLastError(shader_handle)[0] || !slang_test__dispatch(tests, compute, particles) || !slang_test__check_particles(particles, 1.f, ZEST_FALSE, 3)) {
			ZEST_PRINT("\tShader gave wrong results after the link failure was fixed");
			test->result |= 1;
		}

		//Import a new module that doesn't parse, then fix only that module
		std::string kernel_with_import = sources[1];
		size_t import_position = kernel_with_import.find("import common;");
		if (import_position != std::string::npos) {
			kernel_with_import.insert(import_position + strlen("import common;"), "\nimport extra;");
		}
		if (!slang_test__write_file(extra_path, "static const uint EXTRA_VALUE = ;\n") || !slang_test__edit_file(kernel_path, kernel_with_import) || zest_CheckShaderHotReload(device) != 0 || !zest_GetShaderLastError(shader_handle)[0]) {
			ZEST_PRINT("\tA broken new import did not fail the reload");
			test->result |= 1;
		}
		if (!slang_test__edit_file(extra_path, "static const uint EXTRA_VALUE = 1;\n") || zest_CheckShaderHotReload(device) != 1) {
			ZEST_PRINT("\tFixing the new import did not reload the shader: %s", zest_GetShaderLastError(shader_handle));
			test->result |= 1;
		}
		if (zest_GetShaderLastError(shader_handle)[0] || !slang_test__dispatch(tests, compute, particles) || !slang_test__check_particles(particles, 1.f, ZEST_FALSE, 3)) {
			ZEST_PRINT("\tShader gave wrong results after the new import was fixed");
			test->result |= 1;
		}

		//Import a module that doesn't exist yet, then create it without touching the kernel
		std::string kernel_with_missing_import = kernel_with_import;
		kernel_with_missing_import.insert(kernel_with_missing_import.find("import extra;") + strlen("import extra;"), "\nimport later;");
		if (!slang_test__edit_file(kernel_path, kernel_with_missing_import) || zest_CheckShaderHotReload(device) != 0 || !zest_GetShaderLastError(shader_handle)[0]) {
			ZEST_PRINT("\tA missing new import did not fail the reload");
			test->result |= 1;
		}
		if (!slang_test__write_file(later_path, "static const uint LATER_VALUE = 1;\n") || zest_CheckShaderHotReload(device) != 1) {
			ZEST_PRINT("\tCreating the missing import did not reload the shader: %s", zest_GetShaderLastError(shader_handle));
			test->result |= 1;
		}
		if (zest_GetShaderLastError(shader_handle)[0] || !slang_test__dispatch(tests, compute, particles) || !slang_test__check_particles(particles, 1.f, ZEST_FALSE, 3)) {
			ZEST_PRINT("\tShader gave wrong results after the missing import was created");
			test->result |= 1;
		}
	}

	if (compute.value) zest_FreeCompute(compute);
	if (shader_handle.value) zest_FreeShader(shader_handle);
	if (session) zest_slang_FreeSession(session);
	if (device->slang_info) zest_slang_Shutdown(device);
	for (int index = 0; index != 5; ++index) {
		remove((std::string(SLANG_TEST_RELOAD_PATH "/") + files[index]).c_str());
	}
#ifdef _WIN32
	_rmdir(SLANG_TEST_RELOAD_PATH);
#else
	rmdir(SLANG_TEST_RELOAD_PATH);
#endif
	test->result |= zest_GetValidationErrorCount(device);
	test->frame_count++;
	return test->result;
}

#define SLANG_TEST_SHADOW_BASE_PATH "zest_slang_shadow_base"
#define SLANG_TEST_SHADOW_OVERRIDE_PATH "zest_slang_shadow_override"

//Creating a module earlier in the search paths than the one in use reloads the shader with the new module
int test__slang_shadowing_module(ZestTests *tests, Test *test) {
	zest_device device = tests->device;
	const char *files[] = { "common.slang", "kernel.slang", "default_collider.slang" };
	zest__create_folder(device, SLANG_TEST_SHADOW_BASE_PATH);
	zest__create_folder(device, SLANG_TEST_SHADOW_OVERRIDE_PATH);
	std::string kernel_source;
	for (int index = 0; index != 3; ++index) {
		std::string source = slang_test__read_file((std::string(SLANG_TEST_SHADER_PATH "/") + files[index]).c_str());
		if (index == 1) kernel_source = source;
		if (source.empty() || !slang_test__write_file((std::string(SLANG_TEST_SHADOW_BASE_PATH "/") + files[index]).c_str(), source)) {
			test->result |= 1;
		}
	}
	const char *override_kernel_path = SLANG_TEST_SHADOW_OVERRIDE_PATH "/kernel.slang";
	const char *override_extra_path = SLANG_TEST_SHADOW_OVERRIDE_PATH "/extra_module.slang";
	const char *base_extra_path = SLANG_TEST_SHADOW_BASE_PATH "/extra_module.slang";
	remove(override_kernel_path);
	remove(override_extra_path);

	if (!device->slang_info) {
		zest_slang_InitialiseSession(device);
	}
	const char *search_paths[] = { SLANG_TEST_SHADOW_OVERRIDE_PATH, SLANG_TEST_SHADOW_BASE_PATH };
	zest_slang_session_info_t info = zest_slang_DefaultSessionInfo();
	info.search_paths = search_paths;
	info.search_path_count = 2;
	zest_slang_session session = test->result ? NULL : zest_slang_CreateSession(device, &info);
	const char *default_collider = "default_collider";
	zest_slang_entry_point_info_t entry = slang_test__entry("simulate_a", &default_collider);
	zest_slang_entry_point_info_t entry_b = slang_test__entry("simulate_b", &default_collider);
	zest_shader_handle shader_handle = session ? zest_slang_CreateShaderFromSession(session, &entry, "slang_shadowing") : zest_shader_handle{};
	//Built from the module the session already cached, so its own compile never looks for the override
	zest_shader_handle shader_handle_b = session ? zest_slang_CreateShaderFromSession(session, &entry_b, "slang_shadowing_b") : zest_shader_handle{};
	zest_compute_handle compute = shader_handle.value ? zest_CreateCompute(device, "Slang Shadowing", shader_handle) : zest_compute_handle{};
	zest_compute_handle compute_b = shader_handle_b.value ? zest_CreateCompute(device, "Slang Shadowing B", shader_handle_b) : zest_compute_handle{};
	zest_shader_handle shader_handle_c = {};
	if (!compute.value || !compute_b.value) {
		test->result |= 1;
	} else {
		zest_SetShaderHotReload(shader_handle, ZEST_TRUE);
		zest_SetShaderHotReload(shader_handle_b, ZEST_TRUE);
		SlangTestParticle particles[SLANG_TEST_PARTICLE_COUNT];
		if (!slang_test__dispatch(tests, compute, particles) || !slang_test__check_particles(particles, 1.f, ZEST_FALSE, 1) ||
			!slang_test__dispatch(tests, compute_b, particles) || !slang_test__check_particles(particles, 2.f, ZEST_FALSE, 1)) {
			ZEST_PRINT("\tInitial dispatch gave wrong results");
			test->result |= 1;
		}

		//The override kernel doubles the marker
		std::string override_kernel = kernel_source;
		size_t marker_position = override_kernel.find("COMMON_MARKER * 1000");
		if (marker_position != std::string::npos) {
			override_kernel.replace(marker_position, strlen("COMMON_MARKER * 1000"), "COMMON_MARKER * 2000");
		}
		if (!slang_test__write_file(override_kernel_path, override_kernel) || zest_CheckShaderHotReload(device) != 2) {
			ZEST_PRINT("\tCreating an overriding kernel did not reload both shaders: %s", zest_GetShaderLastError(shader_handle_b));
			test->result |= 1;
		}
		if (!slang_test__dispatch(tests, compute, particles) || !slang_test__check_particles(particles, 1.f, ZEST_FALSE, 2) ||
			!slang_test__dispatch(tests, compute_b, particles) || !slang_test__check_particles(particles, 2.f, ZEST_FALSE, 2)) {
			ZEST_PRINT("\tA reloaded shader still uses the shadowed kernel");
			test->result |= 1;
		}

		//Deleting the override falls back to the kernel it shadowed
		remove(override_kernel_path);
		if (zest_CheckShaderHotReload(device) != 2) {
			ZEST_PRINT("\tDeleting the overriding kernel did not reload both shaders: %s", zest_GetShaderLastError(shader_handle));
			test->result |= 1;
		}
		if (!slang_test__dispatch(tests, compute, particles) || !slang_test__check_particles(particles, 1.f, ZEST_FALSE, 1) ||
			!slang_test__dispatch(tests, compute_b, particles) || !slang_test__check_particles(particles, 2.f, ZEST_FALSE, 1)) {
			ZEST_PRINT("\tA shader still uses the deleted overriding kernel");
			test->result |= 1;
		}

		//Shader C, compiled after the session looked for override/extra_module.slang, doesn't use it so creating it mustn't reload C
		if (!slang_test__write_file(base_extra_path, "struct ExtraType { float value; };\n") || zest_slang_GetTypeSize(session, "extra_module", "ExtraType") != 4) {
			ZEST_PRINT("\tCould not load extra_module: %s", zest_slang_GetLastError(session));
			test->result |= 1;
		}
		shader_handle_c = zest_slang_CreateShaderFromSession(session, &entry, "slang_shadowing_c");
		if (!shader_handle_c.value) {
			test->result |= 1;
		} else {
			zest_SetShaderHotReload(shader_handle_c, ZEST_TRUE);
		}
		if (!slang_test__write_file(override_extra_path, "struct ExtraType { float value; };\n") || zest_CheckShaderHotReload(device) != 0) {
			ZEST_PRINT("\tCreating an unrelated module reloaded the shaders");
			test->result |= 1;
		}
	}

	if (compute.value) zest_FreeCompute(compute);
	if (compute_b.value) zest_FreeCompute(compute_b);
	if (shader_handle.value) zest_FreeShader(shader_handle);
	if (shader_handle_b.value) zest_FreeShader(shader_handle_b);
	if (shader_handle_c.value) zest_FreeShader(shader_handle_c);
	if (session) zest_slang_FreeSession(session);
	if (device->slang_info) zest_slang_Shutdown(device);
	remove(override_kernel_path);
	remove(override_extra_path);
	remove(base_extra_path);
	for (int index = 0; index != 3; ++index) {
		remove((std::string(SLANG_TEST_SHADOW_BASE_PATH "/") + files[index]).c_str());
	}
#ifdef _WIN32
	_rmdir(SLANG_TEST_SHADOW_OVERRIDE_PATH);
	_rmdir(SLANG_TEST_SHADOW_BASE_PATH);
#else
	rmdir(SLANG_TEST_SHADOW_OVERRIDE_PATH);
	rmdir(SLANG_TEST_SHADOW_BASE_PATH);
#endif
	test->result |= zest_GetValidationErrorCount(device);
	test->frame_count++;
	return test->result;
}
#endif
