#pragma once

#include <new>
#include <ctype.h>
#if !defined(_WIN32)
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#endif

// X11/Xlib.h defines macros like "None", "Bool", "Status" that conflict with Slang's enums.
// Save and undefine them before including Slang headers, then restore after.
#ifdef None
#define _XLIB_NONE_SAVED None
#undef None
#endif

#ifdef Bool
#define _XLIB_BOOL_SAVED Bool
#undef Bool
#endif

#ifdef Status
#define _XLIB_STATUS_SAVED Status
#undef Status
#endif

#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include <slang/slang-com-helper.h>

// Restore X11 macros
#ifdef _XLIB_NONE_SAVED
#define None _XLIB_NONE_SAVED
#undef _XLIB_NONE_SAVED
#endif

#ifdef _XLIB_BOOL_SAVED
#define Bool _XLIB_BOOL_SAVED
#undef _XLIB_BOOL_SAVED
#endif

#ifdef _XLIB_STATUS_SAVED
#define Status _XLIB_STATUS_SAVED
#undef _XLIB_STATUS_SAVED
#endif

#include <zest.h>

//Not thread-safe: use a session, its shaders and zest_CheckShaderHotReload from one thread at a time.

#define ZEST_SLANG_MAX_LINK_MODULES 16
#define ZEST_SLANG_MAX_TYPE_ARGUMENTS 16

typedef struct zest_slang_session_t *zest_slang_session;
typedef struct zest_slang_shader_source_s zest_slang_shader_source_t;

typedef struct zest_slang_info_s {
    int magic;
    Slang::ComPtr<slang::IGlobalSession> global_session;
    zest_slang_session *sessions;                    //Sessions still alive, freed by zest_slang_Shutdown (vec)
    zest_text_t create_session_error;                //Why the last zest_slang_CreateSession returned NULL
} zest_slang_info_t;

typedef Slang::ComPtr<slang::IBlob> zest_slang_compiled_shader;

typedef struct zest_slang_macro_s {
    const char *name;
    const char *value;
} zest_slang_macro_t;

typedef enum zest_slang_optimization {
    zest_slang_optimization_default,                 //Slang's default level, also what a zero initialised session info gets
    zest_slang_optimization_none,
    zest_slang_optimization_high,
    zest_slang_optimization_maximal,
} zest_slang_optimization;

typedef struct zest_slang_session_info_s {
    const char **search_paths;                       //Module search paths for import resolution
    zest_uint search_path_count;
    const zest_slang_macro_t *macros;
    zest_uint macro_count;
    const char *profile;                             //Defaults to "spirv_1_5"
    zest_bool debug_info;                            //Emit SPIR-V source-level debug info (RenderDoc / Nsight). Best with zest_slang_optimization_none.
    zest_slang_optimization optimization_level;
    zest_bool scalar_block_layout;                   //Scalar (C-like) buffer layout. Needs zest_capability_scalar_block_layout on the device.
} zest_slang_session_info_t;

typedef enum zest_slang_result {
    zest_slang_result_success,
    zest_slang_result_module_load_failed,
    zest_slang_result_entry_point_not_found,
    zest_slang_result_specialization_failed,
    zest_slang_result_link_failed,
    zest_slang_result_code_generation_failed,
    zest_slang_result_type_not_found,
    zest_slang_result_invalid_arguments,
} zest_slang_result;

typedef struct zest_slang_entry_point_info_s {
    const char *module;                              //Module name or path, resolved through the search paths
    const char *entry_point;                         //Name of a [shader("...")] function
    zest_shader_type type;
    const char **type_arguments;                     //Generic entry point specialisation, type names in parameter order. Optional.
    zest_uint type_argument_count;
    const char **link_modules;                       //Extra modules composed into the program, e.g. the module that exports an extern type. Optional.
    zest_uint link_module_count;
} zest_slang_entry_point_info_t;

typedef struct zest_slang_blob_s {
    const void *data;
    zest_size size;
    void *internal;                                  //Owning IBlob
} zest_slang_blob_t;

typedef struct zest_slang_file_stamp_s {
    zest_text_t path;
    zest_u64 mtime;                                  //0 for a file that doesn't exist yet
} zest_slang_file_stamp_t;

zest_hash_map(zest_slang_file_stamp_t) zest_slang_file_stamps;    //Keyed by a hash of the path
zest_hash_map(zest_u64) zest_slang_mtimes;                        //Path hash to mtime, 0 if the file can't be read

inline void zest__slang_record_file(zest_slang_session session, zest_slang_file_stamps *dependencies, const char *path, zest_u64 mtime);

inline zest_bool zest__slang_same_uuid(const SlangUUID &first, const SlangUUID &second) {
    return memcmp(&first, &second, sizeof(SlangUUID)) == 0;
}

//One spelling per file: absolute, forward slashes, and lower case on Windows where paths ignore case
inline void zest__slang_canonical_path(const char *path, char *out_path, size_t out_size) {
#if defined(_WIN32)
    DWORD length = GetFullPathNameA(path, (DWORD)out_size, out_path, NULL);
    if (length == 0 || length >= out_size) {
        snprintf(out_path, out_size, "%s", path);
    }
    for (char *character = out_path; *character; ++character) {
        *character = *character == '\\' ? '/' : (char)tolower((unsigned char)*character);
    }
#else
    char resolved[PATH_MAX];
    if (realpath(path, resolved)) {
        snprintf(out_path, out_size, "%s", resolved);
        return;
    }
    //A file that doesn't exist yet: resolve its folder instead
    const char *last_slash = strrchr(path, '/');
    char directory[PATH_MAX];
    snprintf(directory, sizeof(directory), "%.*s", last_slash ? (int)(last_slash - path) : 1, last_slash ? path : ".");
    if (realpath(directory, resolved)) {
        snprintf(out_path, out_size, "%s/%s", resolved, last_slash ? last_slash + 1 : path);
    } else {
        snprintf(out_path, out_size, "%s", path);
    }
#endif
}

inline zest_bool zest__slang_blob_contains(slang::IBlob *blob, const char *text) {
    if (!blob) return ZEST_FALSE;
    const char *data = (const char *)blob->getBufferPointer();
    size_t size = blob->getBufferSize();
    size_t text_length = strlen(text);
    for (size_t index = 0; index + text_length <= size; ++index) {
        if (memcmp(data + index, text, text_length) == 0) return ZEST_TRUE;
    }
    return ZEST_FALSE;
}

//Owns the bytes of a file handed to Slang
class zest__slang_file_blob : public ISlangBlob {
public:
    zest__slang_file_blob(char *data, size_t size) : reference_count(0), data(data), size(size) {}
    virtual ~zest__slang_file_blob() { free(data); }
    SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(SlangUUID const &uuid, void **out_object) SLANG_OVERRIDE {
        if (zest__slang_same_uuid(uuid, ISlangUnknown::getTypeGuid()) || zest__slang_same_uuid(uuid, ISlangBlob::getTypeGuid())) {
            addRef();
            *out_object = static_cast<ISlangBlob *>(this);
            return SLANG_OK;
        }
        return SLANG_E_NO_INTERFACE;
    }
    SLANG_NO_THROW uint32_t SLANG_MCALL addRef() SLANG_OVERRIDE { return ++reference_count; }
    SLANG_NO_THROW uint32_t SLANG_MCALL release() SLANG_OVERRIDE {
        if (--reference_count == 0) {
            delete this;
            return 0;
        }
        return reference_count;
    }
    SLANG_NO_THROW void const *SLANG_MCALL getBufferPointer() SLANG_OVERRIDE { return data; }
    SLANG_NO_THROW size_t SLANG_MCALL getBufferSize() SLANG_OVERRIDE { return size; }
private:
    uint32_t reference_count;
    char *data;
    size_t size;
};

inline ISlangBlob *zest__slang_string_blob(const char *text, size_t length) {
    char *data = (char *)malloc(length + 1);
    memcpy(data, text, length);
    data[length] = 0;
    ISlangBlob *blob = new zest__slang_file_blob(data, length);
    blob->addRef();
    return blob;
}

inline zest_bool zest__slang_is_absolute_path(const char *path) {
    return path[0] == '/' || path[0] == '\\' || (path[0] && path[1] == ':');
}

//Paths are their own identity so Slang reports real dependency paths; records every path Slang reads or looks for
class zest__slang_file_system : public ISlangFileSystemExt {
public:
    zest_slang_session session;                      //NULL once the session is freed
    zest_slang_file_stamps *dependencies;            //Dependencies of the compile in progress, if any

    zest__slang_file_system(zest_slang_session session) : session(session), dependencies(nullptr), reference_count(0) {}
    virtual ~zest__slang_file_system() {}
    SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(SlangUUID const &uuid, void **out_object) SLANG_OVERRIDE {
        void *object = castAs(uuid);
        if (!object) return SLANG_E_NO_INTERFACE;
        addRef();
        *out_object = object;
        return SLANG_OK;
    }
    SLANG_NO_THROW uint32_t SLANG_MCALL addRef() SLANG_OVERRIDE { return ++reference_count; }
    SLANG_NO_THROW uint32_t SLANG_MCALL release() SLANG_OVERRIDE {
        if (--reference_count == 0) {
            delete this;
            return 0;
        }
        return reference_count;
    }
    SLANG_NO_THROW void *SLANG_MCALL castAs(const SlangUUID &uuid) SLANG_OVERRIDE {
        if (zest__slang_same_uuid(uuid, ISlangUnknown::getTypeGuid()) || zest__slang_same_uuid(uuid, ISlangCastable::getTypeGuid()) ||
            zest__slang_same_uuid(uuid, ISlangFileSystem::getTypeGuid()) || zest__slang_same_uuid(uuid, ISlangFileSystemExt::getTypeGuid())) {
            return static_cast<ISlangFileSystemExt *>(this);
        }
        return nullptr;
    }
    SLANG_NO_THROW SlangResult SLANG_MCALL loadFile(char const *path, ISlangBlob **out_blob) SLANG_OVERRIDE {
        *out_blob = nullptr;
        zest_u64 mtime = 0;
        zest_bool exists = zest_GetFileModifiedTime(path, &mtime);
        if (session) {
            zest__slang_record_file(session, dependencies, path, exists ? mtime : 0);
        }
        if (!exists) return SLANG_E_NOT_FOUND;
        FILE *file = zest__open_file(path, "rb");
        if (!file) return SLANG_E_CANNOT_OPEN;
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        char *data = (char *)malloc(size > 0 ? (size_t)size : 1);
        size_t read = size > 0 ? fread(data, 1, (size_t)size, file) : 0;
        fclose(file);
        if (size < 0 || (size > 0 && read != (size_t)size)) {
            free(data);
            return SLANG_FAIL;
        }
        *out_blob = new zest__slang_file_blob(data, (size_t)size);
        (*out_blob)->addRef();
        return SLANG_OK;
    }
    SLANG_NO_THROW SlangResult SLANG_MCALL getFileUniqueIdentity(const char *path, ISlangBlob **out_unique_identity) SLANG_OVERRIDE {
        char canonical[1024];
        zest__slang_canonical_path(path, canonical, sizeof(canonical));
        *out_unique_identity = zest__slang_string_blob(canonical, strlen(canonical));
        return SLANG_OK;
    }
    SLANG_NO_THROW SlangResult SLANG_MCALL calcCombinedPath(SlangPathType from_path_type, const char *from_path, const char *path, ISlangBlob **out_path) SLANG_OVERRIDE {
        size_t directory_length = strlen(from_path);
        if (from_path_type == SLANG_PATH_TYPE_FILE) {
            //Up to and including the last slash, so a file in the root keeps "/"
            directory_length = 0;
            for (size_t index = 0; from_path[index]; ++index) {
                if (from_path[index] == '/' || from_path[index] == '\\') directory_length = index + 1;
            }
        }
        if (zest__slang_is_absolute_path(path) || directory_length == 0) {
            *out_path = zest__slang_string_blob(path, strlen(path));
            return SLANG_OK;
        }
        zest_bool has_separator = from_path[directory_length - 1] == '/' || from_path[directory_length - 1] == '\\';
        size_t prefix_length = directory_length + (has_separator ? 0 : 1);
        size_t path_length = strlen(path);
        char *combined = (char *)malloc(prefix_length + path_length + 1);
        memcpy(combined, from_path, directory_length);
        combined[prefix_length - 1] = '/';
        memcpy(combined + prefix_length, path, path_length + 1);
        *out_path = new zest__slang_file_blob(combined, prefix_length + path_length);
        (*out_path)->addRef();
        return SLANG_OK;
    }
    //A file Slang looks for but can't find is recorded as missing so creating it later triggers a reload
    SLANG_NO_THROW SlangResult SLANG_MCALL getPathType(const char *path, SlangPathType *out_path_type) SLANG_OVERRIDE {
#if defined(_WIN32)
        DWORD attributes = GetFileAttributesA(path);
        zest_bool exists = attributes != INVALID_FILE_ATTRIBUTES;
        zest_bool directory = exists && (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
        struct stat status;
        zest_bool exists = stat(path, &status) == 0;
        zest_bool directory = exists && S_ISDIR(status.st_mode);
#endif
        if (!exists) {
            if (session) zest__slang_record_file(session, dependencies, path, 0);
            return SLANG_E_NOT_FOUND;
        }
        *out_path_type = directory ? SLANG_PATH_TYPE_DIRECTORY : SLANG_PATH_TYPE_FILE;
        return SLANG_OK;
    }
    SLANG_NO_THROW SlangResult SLANG_MCALL getPath(PathKind kind, const char *path, ISlangBlob **out_path) SLANG_OVERRIDE {
        if (kind == PathKind::Canonical) {
            return getFileUniqueIdentity(path, out_path);
        }
        *out_path = zest__slang_string_blob(path, strlen(path));
        return SLANG_OK;
    }
    SLANG_NO_THROW void SLANG_MCALL clearCache() SLANG_OVERRIDE {}
    SLANG_NO_THROW SlangResult SLANG_MCALL enumeratePathContents(const char *path, FileSystemContentsCallBack callback, void *user_data) SLANG_OVERRIDE {
        return SLANG_E_NOT_IMPLEMENTED;
    }
    SLANG_NO_THROW OSPathKind SLANG_MCALL getOSPathKind() SLANG_OVERRIDE { return OSPathKind::Direct; }
private:
    uint32_t reference_count;
};

typedef struct zest_slang_session_t {
    int magic;
    zest_device device;
    Slang::ComPtr<slang::ISession> session;
    zest__slang_file_system *file_system;            //Shared by every ISession this session creates, holds one reference
    zest_text_t *search_paths;                       //(vec)
    zest_text_t *macro_names;                        //(vec)
    zest_text_t *macro_values;                       //(vec)
    const char **search_path_pointers;               //Into search_paths, for slang::SessionDesc (vec)
    slang::PreprocessorMacroDesc *macro_descriptions;   //Into macro_names and macro_values, for slang::SessionDesc (vec)
    zest_text_t profile;
    zest_bool debug_info;
    zest_slang_optimization optimization_level;
    zest_bool scalar_block_layout;
    zest_bool needs_reset;                           //A module failed to load, so the ISession may hold a broken one and is rebuilt before the next compile
    zest_slang_file_stamps loaded_files;             //Every file the current ISession has read and its mtime from just before the read
    zest_uint check_index;                           //zest_CheckShaderHotReload pass that check_mtimes belongs to
    zest_slang_mtimes check_mtimes;                  //mtimes read during that pass, shared by the session's shaders
    zest_slang_shader_source_t **shader_sources;     //Shaders created from this session (vec)
    zest_slang_result last_result;
    zest_text_t last_error;
} zest_slang_session_t;

struct zest_slang_shader_source_s {
    zest_slang_session session;
    zest_text_t module;
    zest_text_t entry_point;
    zest_shader_type type;
    zest_text_t *type_arguments;                     //(vec)
    zest_text_t *link_modules;                       //(vec)
    zest_slang_file_stamps dependencies;             //Every source file a compile of this shader has read or looked for
};

inline void diagnoseIfNeeded(zest_device device, slang::IBlob *diagnosticsBlob, const char *stage) {
    if (diagnosticsBlob && diagnosticsBlob->getBufferSize()) {
        ZEST_APPEND_LOG(device->log_path.str, "Slang diagnostics during [%s]:\n%.*s", stage, (int)diagnosticsBlob->getBufferSize(), (const char *)diagnosticsBlob->getBufferPointer());
    }
}

inline zest_slang_info_t *zest_slang_Session(zest_device device) {
    ZEST_ASSERT(device->slang_info);  //Slang hasn't been initialise, call zest_slang_InitialiseSession
    return static_cast<zest_slang_info_t *>(device->slang_info);
}

inline void zest_slang_FreeSession(zest_slang_session session);

//Creates the global Slang session for the device. Call once before any other zest_slang_ function.
inline void zest_slang_InitialiseSession(zest_device device) {
    void *memory = zest_AllocateMemory(device, sizeof(zest_slang_info_t));
    zest_slang_info_t *slang_info = new (memory) zest_slang_info_t();
    slang_info->magic = zest_INIT_MAGIC(zest_struct_type_slang_info);
    slang::createGlobalSession(slang_info->global_session.writeRef());
    device->slang_info = slang_info;
}

//Free every session with zest_slang_FreeSession first. Asserts if any are left, and frees them when asserts are off.
inline void zest_slang_Shutdown(zest_device device) {
    if (device->slang_info) {
        zest_slang_info_t *slang_info = static_cast<zest_slang_info_t *>(device->slang_info);

        ZEST_ASSERT(!zest_vec_size(slang_info->sessions), "Free every Slang session with zest_slang_FreeSession before calling zest_slang_Shutdown.");
        while (zest_vec_size(slang_info->sessions)) {
            zest_slang_FreeSession(zest_vec_back(slang_info->sessions));
        }
        zest_vec_free(device->allocator, slang_info->sessions);
        zest_FreeText(device->allocator, &slang_info->create_session_error);

        // Deleting the C++ object will automatically trigger the ComPtr's
        // destructor, which correctly releases the global session.
        slang_info->~zest_slang_info_t();
        zest_FreeMemory(device, slang_info);

        device->slang_info = 0;
    }
}

//Compiles one entry point in a fresh ISession. Kept for back compatibility, prefer zest_slang_CreateSession and zest_slang_CompileToBinary.
inline int zest_slang_Compile(zest_device device, const char *shader_path, const char *entry_point_name, SlangStage stage, zest_slang_compiled_shader &out_compiled_shader, slang::ShaderReflection *&out_reflection) {
    zest_slang_info_t *slang_info = zest_slang_Session(device);
    Slang::ComPtr<slang::IGlobalSession> global_session = slang_info->global_session;

    Slang::ComPtr<slang::ISession> session;
    slang::SessionDesc sessionDesc = {};
    slang::TargetDesc targetDesc = {};

    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = global_session->findProfile("spirv_1_5");
    if (targetDesc.profile == SLANG_PROFILE_UNKNOWN) {
        ZEST_APPEND_LOG(device->log_path.str, "Slang error: Could not find spirv_1_5 profile.");
        return -1;
    }
    // This flag is recommended for modern Slang versions when targeting Vulkan
    targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;
    global_session->createSession(sessionDesc, session.writeRef());

    Slang::ComPtr<slang::IBlob> diagnosticBlob;
    slang::IModule *slangModule = session->loadModule(shader_path, diagnosticBlob.writeRef());
    diagnoseIfNeeded(device, diagnosticBlob, "Module Loading");
    if (!slangModule) {
        ZEST_APPEND_LOG(device->log_path.str, "Slang failed to load module: %s", shader_path);
        return -1;
    }

    Slang::ComPtr<slang::IEntryPoint> entryPoint;
    SlangResult findEntryPointResult = slangModule->findEntryPointByName(entry_point_name, entryPoint.writeRef());
    if (SLANG_FAILED(findEntryPointResult) || !entryPoint) {
        ZEST_APPEND_LOG(device->log_path.str, "Failed to find entry point '%s' in '%s'", entry_point_name, shader_path);
        return -1;
    }

    // Compose the final program from the module and the entry point
    slang::IComponentType *componentTypes[] = { slangModule, entryPoint };
    Slang::ComPtr<slang::IComponentType> composedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        SlangResult result = session->createCompositeComponentType(
            componentTypes,
            2,
            composedProgram.writeRef(),
            diagnostics.writeRef()
        );
        diagnoseIfNeeded(device, diagnostics, "Program Composition");
        SLANG_RETURN_ON_FAIL(result);
    }

    // Now, get the compiled code (SPIR-V)
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        SlangResult result = composedProgram->getEntryPointCode(0, 0, out_compiled_shader.writeRef(), diagnostics.writeRef());
        diagnoseIfNeeded(device, diagnostics, "SPIR-V Generation");
        SLANG_RETURN_ON_FAIL(result);
    }

    out_reflection = composedProgram->getLayout();

    /*
    if (out_reflection && stage == SLANG_STAGE_VERTEX) {
        ZEST_PRINT("--- Vertex Attributes for %s ---", shader_path);

        slang::ShaderReflection *layout = out_reflection;
        slang::EntryPointReflection *entryPointReflection = layout->getEntryPointByIndex(0);
        ZEST_PRINT("Entry point name: %s", entryPointReflection->getName());
        if (!entryPointReflection) {
            ZEST_PRINT("Could not get entry point reflection for %s", shader_path);
        } else {
            unsigned parameterCount = entryPointReflection->getParameterCount();

            for (unsigned i = 0; i < parameterCount; ++i) {
                slang::VariableLayoutReflection *param = entryPointReflection->getParameterByIndex(i);
                slang::VariableReflection *variable = param->getVariable();
                slang::TypeReflection *type = param->getType();

                ZEST_PRINT("Parameter name: %s", param->getName());
                ZEST_PRINT("Variable name: %s", variable->getName());
                ZEST_PRINT("Type name: %s", type->getName());

                zest_uint field_count = type->getFieldCount();

                for (int k = 0; k != type->getFieldCount(); ++k) {
                    slang::VariableReflection *field = type->getFieldByIndex(k);
					ZEST_PRINT("    %s", field->getName());
                    slang::Attribute *attribute = field->findUserAttributeByName(global_session, "vk_format");
                    for (int l = 0; l != field->getUserAttributeCount(); ++l) {
                        slang::Attribute *attribute = field->getUserAttributeByIndex(l);
                        zest_size out_size;
                        ZEST_PRINT("Attribute: %s, %s", attribute->getName(), attribute->getArgumentValueString(0, &out_size));
                    }
                }

            }
        }
        ZEST_APPEND_LOG(device->log_path.str, "------------------------------------\n");
    }
    */

    return 0;
}

inline const char *zest__slang_stage_name(SlangStage stage) {
    switch (stage) {
    case SLANG_STAGE_VERTEX:   return "vertex";
    case SLANG_STAGE_FRAGMENT: return "fragment";
    case SLANG_STAGE_COMPUTE:  return "compute";
    default: return "matching";
    }
}

inline SlangStage zest__slang_GetStage(zest_shader_type type) {
    switch (type) {
    case zest_vertex_shader:   return SLANG_STAGE_VERTEX;
    case zest_fragment_shader: return SLANG_STAGE_FRAGMENT;
    case zest_compute_shader:  return SLANG_STAGE_COMPUTE;
    default: return SLANG_STAGE_NONE;
    }
}

//Kept for back compatibility, prefer zest_slang_CreateShaderFromSession.
inline zest_shader_handle zest_slang_CreateShader(zest_device device, const char *shader_path, const char *name, const char *entry_point, zest_shader_type type, bool disable_caching) {
    zest_slang_compiled_shader compiled_shader;
    slang::ShaderReflection *reflection_info = nullptr;

    SlangStage slang_stage = zest__slang_GetStage(type);
    if (slang_stage == SLANG_STAGE_NONE) {
        ZEST_APPEND_LOG(device->log_path.str, "Unsupported shader type for Slang compilation.");
		return {};
    }

    const char *final_entry_point = entry_point;
    if (!final_entry_point) {
        switch (type) {
        case zest_vertex_shader: final_entry_point = "vertexMain"; break;
        case zest_fragment_shader: final_entry_point = "fragmentMain"; break;
        case zest_compute_shader: final_entry_point = "computeMain"; break;
			default: ZEST_APPEND_LOG(device->log_path.str, "No default entry point for shader type."); return {};
        }
    }

    int result = zest_slang_Compile(device, shader_path, final_entry_point, slang_stage, compiled_shader, reflection_info);

    if (result != 0) {
        ZEST_APPEND_LOG(device->log_path.str, "Slang compilation failed for %s", shader_path);
		return {};
    }

    zest_uint spv_size = (zest_uint)compiled_shader->getBufferSize();
    const void *spv_binary = compiled_shader->getBufferPointer();

    zest_shader_handle shader_handle = zest_CreateShaderFromBinary(device, name, spv_binary, spv_size, type);
    if (!shader_handle.value) {
		return {};
    }
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(shader_handle.store, shader_handle.value);

    if (!disable_caching && device->init_flags & zest_device_init_flag_cache_shaders) {
        zest__cache_shader(device, shader);
    }

    return shader_handle;
}

// -- Sessions

#define ZEST__SLANG_VALID_SESSION(session) ((session) && ZEST_VALID_HANDLE(session, zest_struct_type_slang_session))

inline zest_slang_session_info_t zest_slang_DefaultSessionInfo(void) {
    zest_slang_session_info_t info = {};
    info.profile = "spirv_1_5";
    return info;
}

inline void zest__slang_free_text_vec(zest_device device, zest_text_t *&texts) {
    zest_vec_foreach(index, texts) {
        zest_FreeText(device->allocator, &texts[index]);
    }
    zest_vec_free(device->allocator, texts);
}

inline void zest__slang_free_stamps(zest_device device, zest_slang_file_stamps &stamps) {
    zest_vec_foreach(index, stamps.data) {
        zest_FreeText(device->allocator, &stamps.data[index].path);
    }
    zest_map_free(device->allocator, stamps);
}

inline void zest__slang_push_text(zest_device device, zest_text_t *&texts, const char *value) {
    zest_text_t text = {};
    zest_SetText(device->allocator, &text, value ? value : "");
    zest_vec_push(device->allocator, texts, text);
}

inline void zest__slang_add_stamp(zest_device device, zest_slang_file_stamps &stamps, const char *path, zest_key key, zest_u64 mtime) {
    if (zest_map_valid_key(stamps, key)) return;
    zest_slang_file_stamp_t stamp = {};
    zest_SetText(device->allocator, &stamp.path, path);
    stamp.mtime = mtime;
    zest_map_insert_key(device->allocator, stamps, key, stamp);
}

//Called by the file system for every path Slang reads or looks for. mtime is 0 for a missing file, so creating it resets the session.
inline void zest__slang_record_file(zest_slang_session session, zest_slang_file_stamps *dependencies, const char *path, zest_u64 mtime) {
    char canonical[1024];
    zest__slang_canonical_path(path, canonical, sizeof(canonical));
    zest_key key = zest_Hash(canonical, strlen(canonical), ZEST_HASH_SEED);
    zest__slang_add_stamp(session->device, session->loaded_files, canonical, key, mtime);
    if (zest_map_valid_key(session->check_mtimes, key)) {
        *zest_map_at_key(session->check_mtimes, key) = mtime;
    }
    if (dependencies) {
        zest__slang_add_stamp(session->device, *dependencies, canonical, key, mtime);
    }
}

//Adds every file a module read, including transitive imports, which a module already cached by the session didn't read again
inline void zest__slang_track_module(zest_slang_session session, slang::IModule *module, zest_slang_file_stamps *dependencies) {
    if (!dependencies) return;
    SlangInt32 file_count = module->getDependencyFileCount();
    for (SlangInt32 file_index = 0; file_index != file_count; ++file_index) {
        const char *path = module->getDependencyFilePath(file_index);
        if (!path || !path[0]) continue;
        char canonical[1024];
        zest__slang_canonical_path(path, canonical, sizeof(canonical));
        zest_key key = zest_Hash(canonical, strlen(canonical), ZEST_HASH_SEED);
        zest_u64 mtime = 0;
        if (zest_map_valid_key(session->loaded_files, key)) {
            mtime = (zest_map_at_key(session->loaded_files, key))->mtime;
        }
        if (!mtime && !zest_GetFileModifiedTime(canonical, &mtime)) {
            continue;
        }
        zest__slang_add_stamp(session->device, *dependencies, canonical, key, mtime);
    }
}

inline const char *zest__slang_file_name(const char *path) {
    const char *name = path;
    for (const char *character = path; *character; ++character) {
        if (*character == '/' || *character == '\\') name = character + 1;
    }
    return name;
}

//Missing paths with the name of a file the shader uses, e.g. override/kernel.slang, which a shader built from cached modules wouldn't otherwise watch
inline void zest__slang_add_missing_files(zest_slang_session session, zest_slang_file_stamps &dependencies) {
    zest_map_foreach(index, session->loaded_files) {
        zest_slang_file_stamp_t *missing = &session->loaded_files.data[session->loaded_files.map[index].index];
        if (missing->mtime) continue;
        const char *missing_name = zest__slang_file_name(missing->path.str);
        zest_bool shadows_a_dependency = ZEST_FALSE;
        zest_vec_foreach(dependency_index, dependencies.data) {
            zest_slang_file_stamp_t *dependency = &dependencies.data[dependency_index];
            if (dependency->mtime && strcmp(zest__slang_file_name(dependency->path.str), missing_name) == 0) {
                shadows_a_dependency = ZEST_TRUE;
                break;
            }
        }
        if (shadows_a_dependency) {
            zest__slang_add_stamp(session->device, dependencies, missing->path.str, session->loaded_files.map[index].key, 0);
        }
    }
}

//Moves stamps for files target doesn't watch yet, so files read only by a failed compile are watched too
inline void zest__slang_merge_stamps(zest_device device, zest_slang_file_stamps &target, zest_slang_file_stamps &source) {
    zest_map_foreach(index, source) {
        zest_key key = source.map[index].key;
        zest_slang_file_stamp_t *stamp = &source.data[source.map[index].index];
        if (zest_map_valid_key(target, key)) {
            zest_FreeText(device->allocator, &stamp->path);
        } else {
            zest_map_insert_key(device->allocator, target, key, *stamp);
        }
    }
    zest_map_free(device->allocator, source);
}

inline zest_bool zest__slang_create_isession(zest_slang_session session) {
    zest_device device = session->device;
    zest_slang_info_t *slang_info = zest_slang_Session(device);

    slang::TargetDesc target_description = {};
    target_description.format = SLANG_SPIRV;
    target_description.profile = slang_info->global_session->findProfile(session->profile.str);
    if (target_description.profile == SLANG_PROFILE_UNKNOWN) {
        zest_SetTextf(device->allocator, &session->last_error, "Slang could not find the profile '%s'.", session->profile.str);
        ZEST_APPEND_LOG(device->log_path.str, "%s", session->last_error.str);
        return ZEST_FALSE;
    }
    target_description.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
    target_description.forceGLSLScalarBufferLayout = session->scalar_block_layout ? true : false;

    slang::CompilerOptionEntry options[2] = {};
    uint32_t option_count = 0;
    options[option_count].name = slang::CompilerOptionName::Optimization;
    options[option_count].value.kind = slang::CompilerOptionValueKind::Int;
    switch (session->optimization_level) {
    case zest_slang_optimization_none: options[option_count].value.intValue0 = SLANG_OPTIMIZATION_LEVEL_NONE; break;
    case zest_slang_optimization_high: options[option_count].value.intValue0 = SLANG_OPTIMIZATION_LEVEL_HIGH; break;
    case zest_slang_optimization_maximal: options[option_count].value.intValue0 = SLANG_OPTIMIZATION_LEVEL_MAXIMAL; break;
    default: options[option_count].value.intValue0 = SLANG_OPTIMIZATION_LEVEL_DEFAULT; break;
    }
    option_count++;
    if (session->debug_info) {
        options[option_count].name = slang::CompilerOptionName::DebugInformation;
        options[option_count].value.kind = slang::CompilerOptionValueKind::Int;
        options[option_count].value.intValue0 = SLANG_DEBUG_INFO_LEVEL_STANDARD;
        option_count++;
    }

    slang::SessionDesc session_description = {};
    session_description.targets = &target_description;
    session_description.targetCount = 1;
    session_description.searchPaths = session->search_path_pointers;
    session_description.searchPathCount = zest_vec_size(session->search_path_pointers);
    session_description.preprocessorMacros = session->macro_descriptions;
    session_description.preprocessorMacroCount = zest_vec_size(session->macro_descriptions);
    session_description.fileSystem = session->file_system;
    session_description.compilerOptionEntries = options;
    session_description.compilerOptionEntryCount = option_count;

    session->session = nullptr;
    SlangResult result = slang_info->global_session->createSession(session_description, session->session.writeRef());
    if (SLANG_FAILED(result) || !session->session) {
        zest_SetText(device->allocator, &session->last_error, "Slang failed to create a session.");
        ZEST_APPEND_LOG(device->log_path.str, "%s", session->last_error.str);
        return ZEST_FALSE;
    }
    return ZEST_TRUE;
}

//Returns NULL if the profile isn't known or the session couldn't be created, see zest_slang_GetCreateSessionError.
inline zest_slang_session zest_slang_CreateSession(zest_device device, const zest_slang_session_info_t *info) {
    zest_slang_info_t *slang_info = zest_slang_Session(device);
    zest_slang_session_info_t default_info = zest_slang_DefaultSessionInfo();
    if (!info) info = &default_info;

    void *memory = zest_AllocateMemory(device, sizeof(zest_slang_session_t));
    zest_slang_session session = new (memory) zest_slang_session_t();
    session->magic = zest_INIT_MAGIC(zest_struct_type_slang_session);
    session->device = device;
    session->file_system = new zest__slang_file_system(session);
    session->file_system->addRef();
    for (zest_uint index = 0; index != info->search_path_count; ++index) {
        zest__slang_push_text(device, session->search_paths, info->search_paths[index]);
    }
    for (zest_uint index = 0; index != info->macro_count; ++index) {
        zest__slang_push_text(device, session->macro_names, info->macros[index].name);
        zest__slang_push_text(device, session->macro_values, info->macros[index].value);
    }
    zest_vec_foreach(index, session->search_paths) {
        zest_vec_push(device->allocator, session->search_path_pointers, session->search_paths[index].str);
    }
    zest_vec_foreach(index, session->macro_names) {
        slang::PreprocessorMacroDesc macro_description;
        macro_description.name = session->macro_names[index].str;
        macro_description.value = session->macro_values[index].str;
        zest_vec_push(device->allocator, session->macro_descriptions, macro_description);
    }
    zest_SetText(device->allocator, &session->profile, info->profile ? info->profile : "spirv_1_5");
    session->debug_info = info->debug_info;
    session->optimization_level = info->optimization_level;
    session->scalar_block_layout = info->scalar_block_layout;

    zest_vec_push(device->allocator, slang_info->sessions, session);
    zest_bool scalar_layout_missing = session->scalar_block_layout && !zest_DeviceFeatureEnabled(device, zest_capability_scalar_block_layout);
    if (scalar_layout_missing) {
        zest_SetText(device->allocator, &session->last_error, "Slang session asked for scalar_block_layout but the device doesn't have zest_capability_scalar_block_layout enabled.");
        ZEST_APPEND_LOG(device->log_path.str, "%s", session->last_error.str);
    }
    if (scalar_layout_missing || !zest__slang_create_isession(session)) {
        zest_SetText(device->allocator, &slang_info->create_session_error, zest_TextLength(&session->last_error) ? session->last_error.str : "Slang failed to create a session.");
        zest_slang_FreeSession(session);
        return nullptr;
    }
    zest_FreeText(device->allocator, &slang_info->create_session_error);
    return session;
}

inline const char *zest_slang_GetCreateSessionError(zest_device device) {
    zest_slang_info_t *slang_info = zest_slang_Session(device);
    return zest_TextLength(&slang_info->create_session_error) ? slang_info->create_session_error.str : "";
}

inline void zest_slang_FreeSession(zest_slang_session session) {
    if (!session) return;
    ZEST_ASSERT(ZEST__SLANG_VALID_SESSION(session), "Not a valid session. It may have been freed already, zest_slang_Shutdown frees any sessions left.");
    if (!ZEST__SLANG_VALID_SESSION(session)) return;
    zest_device device = session->device;
    zest_vec_foreach(index, session->shader_sources) {
        session->shader_sources[index]->session = nullptr;
    }
    zest_vec_free(device->allocator, session->shader_sources);
    zest__slang_free_text_vec(device, session->search_paths);
    zest__slang_free_text_vec(device, session->macro_names);
    zest__slang_free_text_vec(device, session->macro_values);
    zest_vec_free(device->allocator, session->search_path_pointers);
    zest_vec_free(device->allocator, session->macro_descriptions);
    zest__slang_free_stamps(device, session->loaded_files);
    zest_map_free(device->allocator, session->check_mtimes);
    zest_FreeText(device->allocator, &session->profile);
    zest_FreeText(device->allocator, &session->last_error);

    zest_slang_info_t *slang_info = zest_slang_Session(device);
    zest_vec_foreach(index, slang_info->sessions) {
        if (slang_info->sessions[index] == session) {
            slang_info->sessions[index] = zest_vec_back(slang_info->sessions);
            zest_vec_clip(slang_info->sessions);
            break;
        }
    }

    //The ISession may still hold the file system, so stop it recording into freed memory
    session->file_system->session = nullptr;
    session->file_system->dependencies = nullptr;
    session->file_system->release();
    session->magic = 0;
    session->~zest_slang_session_t();
    zest_FreeMemory(device, session);
}

//Drops the session's cached modules so edited files are re-read on the next compile
inline void zest_slang_ResetSession(zest_slang_session session) {
    if (!ZEST__SLANG_VALID_SESSION(session)) return;
    zest__slang_free_stamps(session->device, session->loaded_files);
    session->needs_reset = ZEST_FALSE;
    zest__slang_create_isession(session);
}

//Each file is read once per zest_CheckShaderHotReload pass however many of the session's shaders depend on it
inline zest_u64 zest__slang_checked_mtime(zest_slang_session session, zest_uint check_index, const char *path, zest_key key) {
    if (session->check_index != check_index) {
        zest_map_clear(session->check_mtimes);
        session->check_index = check_index;
    }
    if (zest_map_valid_key(session->check_mtimes, key)) {
        return *zest_map_at_key(session->check_mtimes, key);
    }
    zest_u64 mtime = 0;
    zest_GetFileModifiedTime(path, &mtime);
    zest_map_insert_key(session->device->allocator, session->check_mtimes, key, mtime);
    return mtime;
}

//Resets the session if any file it read changed, so shaders sharing an edited import re-parse it once. check_index 0 means outside a reload pass.
inline void zest__slang_reset_if_stale(zest_slang_session session, zest_uint check_index) {
    zest_bool stale = session->needs_reset || !session->session;
    zest_map_foreach(index, session->loaded_files) {
        if (stale) break;
        zest_key key = session->loaded_files.map[index].key;
        zest_slang_file_stamp_t *stamp = &session->loaded_files.data[session->loaded_files.map[index].index];
        zest_u64 mtime = 0;
        if (check_index) {
            mtime = zest__slang_checked_mtime(session, check_index, stamp->path.str, key);
        } else {
            zest_GetFileModifiedTime(stamp->path.str, &mtime);
        }
        stale = mtime != stamp->mtime;
    }
    if (stale) {
        zest_slang_ResetSession(session);
    }
}

inline zest_slang_result zest_slang_GetLastResult(zest_slang_session session) {
    return ZEST__SLANG_VALID_SESSION(session) ? session->last_result : zest_slang_result_invalid_arguments;
}

//Diagnostics text of the last failed compile or type lookup, owned by the session and replaced by the next one
inline const char *zest_slang_GetLastError(zest_slang_session session) {
    if (!ZEST__SLANG_VALID_SESSION(session)) return "Not a valid Slang session.";
    return zest_TextLength(&session->last_error) ? session->last_error.str : "";
}

// -- Compiling

inline void zest__slang_clear_error(zest_slang_session session) {
    zest_FreeText(session->device->allocator, &session->last_error);
    session->last_result = zest_slang_result_success;
}

inline zest_slang_result zest__slang_set_error(zest_slang_session session, zest_slang_result result, const char *stage, slang::IBlob *diagnostics, const char *message) {
    zest_device device = session->device;
    if (diagnostics && diagnostics->getBufferSize()) {
        zest_SetTextf(device->allocator, &session->last_error, "Slang %s failed:\n%.*s", stage, (int)diagnostics->getBufferSize(), (const char *)diagnostics->getBufferPointer());
    } else {
        zest_SetTextf(device->allocator, &session->last_error, "Slang %s failed: %s", stage, message);
    }
    ZEST_APPEND_LOG(device->log_path.str, "%s", session->last_error.str);
    session->last_result = result;
    return result;
}

//A module that failed to load may leave a broken cache entry, so the session is rebuilt before the next compile
inline zest_slang_result zest__slang_fail(zest_slang_session session, zest_slang_result result, const char *stage, slang::IBlob *diagnostics, const char *message) {
    if (result == zest_slang_result_module_load_failed) {
        session->needs_reset = ZEST_TRUE;
    }
    return zest__slang_set_error(session, result, stage, diagnostics, message);
}

//Keeps the error from the failed session rebuild when there is one
inline zest_slang_result zest__slang_no_session(zest_slang_session session) {
    session->needs_reset = ZEST_TRUE;
    if (zest_TextLength(&session->last_error)) {
        session->last_result = zest_slang_result_module_load_failed;
        return session->last_result;
    }
    return zest__slang_set_error(session, zest_slang_result_module_load_failed, "session creation", nullptr, "No valid Slang session.");
}

inline zest_slang_result zest__slang_compile_program(zest_slang_session session, const zest_slang_entry_point_info_t *info, Slang::ComPtr<slang::IBlob> &out_code, zest_slang_file_stamps *dependencies) {
    if (!session->session) {
        return zest__slang_no_session(session);
    }
    zest_device device = session->device;
    SlangStage stage = zest__slang_GetStage(info->type);
    char message[512];

    //The main module, the link modules and the entry point
    slang::IComponentType *components[ZEST_SLANG_MAX_LINK_MODULES + 2];
    zest_uint module_count = 1 + info->link_module_count;
    slang::IModule *main_module = nullptr;
    for (zest_uint index = 0; index != module_count; ++index) {
        const char *module_name = index == 0 ? info->module : info->link_modules[index - 1];
        Slang::ComPtr<slang::IBlob> diagnostics;
        slang::IModule *module = session->session->loadModule(module_name, diagnostics.writeRef());
        if (!module) {
            snprintf(message, sizeof(message), "Could not load module '%s'.", module_name);
            return zest__slang_fail(session, zest_slang_result_module_load_failed, "module loading", diagnostics, message);
        }
        diagnoseIfNeeded(device, diagnostics, "module loading");
        zest__slang_track_module(session, module, dependencies);
        components[index] = module;
        if (index == 0) main_module = module;
    }

    Slang::ComPtr<slang::IEntryPoint> entry_point;
    Slang::ComPtr<slang::IBlob> entry_point_diagnostics;
    main_module->findEntryPointByName(info->entry_point, entry_point.writeRef());
    if (!entry_point && stage != SLANG_STAGE_NONE) {
        main_module->findAndCheckEntryPoint(info->entry_point, stage, entry_point.writeRef(), entry_point_diagnostics.writeRef());
    }
    if (!entry_point) {
        snprintf(message, sizeof(message), "Entry point '%s' not found in module '%s'.", info->entry_point, info->module);
        return zest__slang_fail(session, zest_slang_result_entry_point_not_found, "entry point lookup", entry_point_diagnostics, message);
    }

    Slang::ComPtr<slang::IComponentType> entry_component(entry_point.get());
    Slang::ComPtr<slang::IComponentType> modules_program;
    if (info->type_argument_count) {
        //Look the types up over every module so types exported by link modules resolve too.
        Slang::ComPtr<slang::IBlob> diagnostics;
        if (SLANG_FAILED(session->session->createCompositeComponentType(components, module_count, modules_program.writeRef(), diagnostics.writeRef()))) {
            return zest__slang_fail(session, zest_slang_result_specialization_failed, "specialisation", diagnostics, "Could not compose the modules.");
        }
        slang::ProgramLayout *layout = modules_program->getLayout();
        slang::SpecializationArg arguments[ZEST_SLANG_MAX_TYPE_ARGUMENTS];
        for (zest_uint index = 0; index != info->type_argument_count; ++index) {
            slang::TypeReflection *type = layout ? layout->findTypeByName(info->type_arguments[index]) : nullptr;
            if (!type) {
                snprintf(message, sizeof(message), "Type '%s' not found for entry point '%s'.", info->type_arguments[index], info->entry_point);
                return zest__slang_set_error(session, zest_slang_result_type_not_found, "type lookup", nullptr, message);
            }
            arguments[index] = slang::SpecializationArg::fromType(type);
        }
        Slang::ComPtr<slang::IComponentType> specialized;
        diagnostics = nullptr;
        if (SLANG_FAILED(entry_point->specialize(arguments, info->type_argument_count, specialized.writeRef(), diagnostics.writeRef())) || !specialized) {
            return zest__slang_fail(session, zest_slang_result_specialization_failed, "specialisation", diagnostics, "Could not specialise the entry point.");
        }
        diagnoseIfNeeded(device, diagnostics, "specialisation");
        entry_component = specialized;
        //Reuse the composed modules rather than composing them again
        components[0] = modules_program;
        module_count = 1;
    }
    components[module_count] = entry_component;

    Slang::ComPtr<slang::IComponentType> composed;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        if (SLANG_FAILED(session->session->createCompositeComponentType(components, module_count + 1, composed.writeRef(), diagnostics.writeRef()))) {
            return zest__slang_fail(session, zest_slang_result_link_failed, "composition", diagnostics, "Could not compose the program.");
        }
        diagnoseIfNeeded(device, diagnostics, "composition");
    }

    slang::ProgramLayout *program_layout = composed->getLayout();
    slang::EntryPointReflection *entry_point_reflection = program_layout ? program_layout->getEntryPointByIndex(0) : nullptr;
    if (stage != SLANG_STAGE_NONE && entry_point_reflection && entry_point_reflection->getStage() != stage) {
        snprintf(message, sizeof(message), "Entry point '%s' in module '%s' is not a %s shader.", info->entry_point, info->module, zest__slang_stage_name(stage));
        return zest__slang_fail(session, zest_slang_result_entry_point_not_found, "entry point lookup", nullptr, message);
    }

    Slang::ComPtr<slang::IComponentType> linked;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        if (SLANG_FAILED(composed->link(linked.writeRef(), diagnostics.writeRef())) || !linked) {
            return zest__slang_fail(session, zest_slang_result_link_failed, "linking", diagnostics, "Could not link the program.");
        }
        diagnoseIfNeeded(device, diagnostics, "linking");
    }

    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        out_code = nullptr;
        if (SLANG_FAILED(linked->getEntryPointCode(0, 0, out_code.writeRef(), diagnostics.writeRef())) || !out_code) {
            //Unresolved extern types are only found when the IR is linked during code generation.
            zest_slang_result result = zest_slang_result_code_generation_failed;
            if (zest__slang_blob_contains(diagnostics, "unresolved external symbol")) {
                result = zest_slang_result_link_failed;
            }
            return zest__slang_fail(session, result, "code generation", diagnostics, "Could not generate code.");
        }
        diagnoseIfNeeded(device, diagnostics, "code generation");
    }
    return zest_slang_result_success;
}

//check_index is the zest_CheckShaderHotReload pass the compile runs in, or 0
inline zest_slang_result zest__slang_compile(zest_slang_session session, const zest_slang_entry_point_info_t *info, Slang::ComPtr<slang::IBlob> &out_code, zest_slang_file_stamps *dependencies, zest_uint check_index) {
    if (!ZEST__SLANG_VALID_SESSION(session)) return zest_slang_result_invalid_arguments;
    zest__slang_clear_error(session);
    if (!info || !info->module || !info->entry_point || info->link_module_count > ZEST_SLANG_MAX_LINK_MODULES || info->type_argument_count > ZEST_SLANG_MAX_TYPE_ARGUMENTS ||
        (info->link_module_count && !info->link_modules) || (info->type_argument_count && !info->type_arguments)) {
        char message[256];
        snprintf(message, sizeof(message), "Entry point info needs a module and entry point, at most %u link modules and at most %u type arguments.", ZEST_SLANG_MAX_LINK_MODULES, ZEST_SLANG_MAX_TYPE_ARGUMENTS);
        return zest__slang_set_error(session, zest_slang_result_invalid_arguments, "argument check", nullptr, message);
    }
    zest__slang_reset_if_stale(session, check_index);
    session->file_system->dependencies = dependencies;
    zest_slang_result result = zest__slang_compile_program(session, info, out_code, dependencies);
    session->file_system->dependencies = nullptr;
    if (dependencies) {
        zest__slang_add_missing_files(session, *dependencies);
    }
    return result;
}

//Compile one entry point to SPIR-V. Free the blob with zest_slang_FreeBlob.
inline zest_bool zest_slang_CompileToBinary(zest_slang_session session, const zest_slang_entry_point_info_t *info, zest_slang_blob_t *out_blob) {
    if (!out_blob) return ZEST_FALSE;
    *out_blob = {};
    Slang::ComPtr<slang::IBlob> code;
    if (zest__slang_compile(session, info, code, nullptr, 0) != zest_slang_result_success) {
        return ZEST_FALSE;
    }
    out_blob->data = code->getBufferPointer();
    out_blob->size = (zest_size)code->getBufferSize();
    out_blob->internal = code.detach();
    return ZEST_TRUE;
}

inline void zest_slang_FreeBlob(zest_slang_blob_t *blob) {
    if (blob && blob->internal) {
        static_cast<slang::IBlob *>(blob->internal)->release();
    }
    if (blob) *blob = {};
}

// -- Shaders and hot reload

inline void zest__slang_free_shader_source(zest_shader shader, void *user_data) {
    zest_slang_shader_source_t *source = static_cast<zest_slang_shader_source_t *>(user_data);
    zest_device device = (zest_device)shader->handle.store->origin;
    zest_slang_session session = source->session;
    if (session) {
        zest_vec_foreach(index, session->shader_sources) {
            if (session->shader_sources[index] == source) {
                session->shader_sources[index] = zest_vec_back(session->shader_sources);
                zest_vec_clip(session->shader_sources);
                break;
            }
        }
    }
    zest_FreeText(device->allocator, &source->module);
    zest_FreeText(device->allocator, &source->entry_point);
    zest__slang_free_text_vec(device, source->type_arguments);
    zest__slang_free_text_vec(device, source->link_modules);
    zest__slang_free_stamps(device, source->dependencies);
    zest_FreeMemory(device, source);
}

inline zest_shader_reload_result zest__slang_reload_shader(zest_shader shader, zest_uint check_index, void *user_data) {
    zest_slang_shader_source_t *source = static_cast<zest_slang_shader_source_t *>(user_data);
    zest_slang_session session = source->session;
    if (!session) return zest_shader_reload_unchanged;

    //mtimes always advance so a broken file doesn't re-fire every check. 0 is a missing file, so creating or deleting one is a change.
    zest_bool changed = ZEST_FALSE;
    zest_map_foreach(index, source->dependencies) {
        zest_key key = source->dependencies.map[index].key;
        zest_slang_file_stamp_t *stamp = &source->dependencies.data[source->dependencies.map[index].index];
        zest_u64 mtime = zest__slang_checked_mtime(session, check_index, stamp->path.str, key);
        if (mtime == stamp->mtime) continue;
        stamp->mtime = mtime;
        changed = ZEST_TRUE;
    }
    if (!changed) return zest_shader_reload_unchanged;

    const char *type_arguments[ZEST_SLANG_MAX_TYPE_ARGUMENTS];
    const char *link_modules[ZEST_SLANG_MAX_LINK_MODULES];
    zest_slang_entry_point_info_t info = {};
    info.module = source->module.str;
    info.entry_point = source->entry_point.str;
    info.type = source->type;
    info.type_argument_count = zest_vec_size(source->type_arguments);
    info.link_module_count = zest_vec_size(source->link_modules);
    zest_vec_foreach(index, source->type_arguments) type_arguments[index] = source->type_arguments[index].str;
    zest_vec_foreach(index, source->link_modules) link_modules[index] = source->link_modules[index].str;
    info.type_arguments = type_arguments;
    info.link_modules = link_modules;

    zest_device device = session->device;
    Slang::ComPtr<slang::IBlob> code;
    zest_slang_file_stamps dependencies = {};
    if (zest__slang_compile(session, &info, code, &dependencies, check_index) != zest_slang_result_success) {
        zest__slang_merge_stamps(device, source->dependencies, dependencies);
        zest_SetText(device->allocator, &shader->last_error, zest_slang_GetLastError(session));
        return zest_shader_reload_failed;
    }

    zest_size size = (zest_size)code->getBufferSize();
    zest_vec_resize(device->allocator, shader->binary, (zest_uint)size);
    memcpy(shader->binary, code->getBufferPointer(), size);
    shader->binary_size = size;
    zest__slang_free_stamps(device, source->dependencies);
    source->dependencies = dependencies;
    return zest_shader_reload_success;
}

//Compile and create a shader that hot reloads when any file it depends on changes. Returns a zero handle on failure.
inline zest_shader_handle zest_slang_CreateShaderFromSession(zest_slang_session session, const zest_slang_entry_point_info_t *info, const char *name) {
    if (!ZEST__SLANG_VALID_SESSION(session)) return {};
    zest_device device = session->device;
    Slang::ComPtr<slang::IBlob> code;
    zest_slang_file_stamps dependencies = {};
    if (zest__slang_compile(session, info, code, &dependencies, 0) != zest_slang_result_success) {
        zest__slang_free_stamps(device, dependencies);
        return {};
    }

    zest_shader_handle shader_handle = zest_CreateShaderFromBinary(device, name, code->getBufferPointer(), (zest_uint)code->getBufferSize(), info->type);
    if (!shader_handle.value) {
        zest__slang_free_stamps(device, dependencies);
        return {};
    }

    void *memory = zest_AllocateMemory(device, sizeof(zest_slang_shader_source_t));
    zest_slang_shader_source_t *source = new (memory) zest_slang_shader_source_t();
    source->session = session;
    zest_SetText(device->allocator, &source->module, info->module);
    zest_SetText(device->allocator, &source->entry_point, info->entry_point);
    source->type = info->type;
    for (zest_uint index = 0; index != info->type_argument_count; ++index) {
        zest__slang_push_text(device, source->type_arguments, info->type_arguments[index]);
    }
    for (zest_uint index = 0; index != info->link_module_count; ++index) {
        zest__slang_push_text(device, source->link_modules, info->link_modules[index]);
    }
    source->dependencies = dependencies;
    zest_vec_push(device->allocator, session->shader_sources, source);
    zest_SetShaderReloadCallback(shader_handle, zest__slang_reload_shader, zest__slang_free_shader_source, source);
    return shader_handle;
}

// -- Reflection

inline zest_size zest__slang_type_size(zest_slang_session session, const char *module_name, const char *type_name) {
    char message[512];
    if (!session->session) {
        zest__slang_no_session(session);
        return 0;
    }
    Slang::ComPtr<slang::IBlob> diagnostics;
    slang::IModule *module = session->session->loadModule(module_name, diagnostics.writeRef());
    if (!module) {
        snprintf(message, sizeof(message), "Could not load module '%s'.", module_name);
        zest__slang_fail(session, zest_slang_result_module_load_failed, "module loading", diagnostics, message);
        return 0;
    }
    slang::ProgramLayout *layout = module->getLayout();
    slang::TypeReflection *type = layout ? layout->findTypeByName(type_name) : nullptr;
    slang::TypeReflection *container = type ? session->session->getContainerType(type, slang::ContainerType::StructuredBuffer) : nullptr;
    slang::TypeLayoutReflection *container_layout = container ? session->session->getTypeLayout(container) : nullptr;
    if (!container_layout || !container_layout->getElementTypeLayout()) {
        snprintf(message, sizeof(message), "Type '%s' not found in module '%s'.", type_name, module_name);
        zest__slang_set_error(session, zest_slang_result_type_not_found, "type lookup", nullptr, message);
        return 0;
    }
    return (zest_size)container_layout->getElementTypeLayout()->getStride();
}

//Stride of a struct in a StructuredBuffer (std430, or scalar with scalar_block_layout), or 0 if the type isn't found
inline zest_size zest_slang_GetTypeSize(zest_slang_session session, const char *module_name, const char *type_name) {
    if (!ZEST__SLANG_VALID_SESSION(session)) return 0;
    zest__slang_clear_error(session);
    if (!module_name || !type_name) {
        zest__slang_set_error(session, zest_slang_result_invalid_arguments, "argument check", nullptr, "A module and type name are required.");
        return 0;
    }
    zest__slang_reset_if_stale(session, 0);
    return zest__slang_type_size(session, module_name, type_name);
}
