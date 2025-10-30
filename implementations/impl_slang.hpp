#pragma once

#include <zest.h>
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include <slang/slang-com-helper.h>

typedef struct zest_slang_info_s {
    int magic;
    Slang::ComPtr<slang::IGlobalSession> global_session;
} zest_slang_info_t;

typedef Slang::ComPtr<slang::IBlob> zest_slang_compiled_shader;

inline void diagnoseIfNeeded(zest_context context, slang::IBlob *diagnosticsBlob, const char *stage) {
    if (diagnosticsBlob != nullptr) {
        const char *diagnostics = (const char *)diagnosticsBlob->getBufferPointer();
        if (diagnostics && strlen(diagnostics) > 0) {
            ZEST_APPEND_LOG(context->device->log_path.str, "Slang diagnostics during [%s]:\n%s", stage, diagnostics);
        }
    }
}

inline zest_slang_info_t *zest_slang_Session(zest_context context) {
    ZEST_ASSERT(context->device->slang_info);  //Slang hasn't been initialise, call zest_slang_InitialiseSession
    return static_cast<zest_slang_info_t *>(context->device->slang_info);
}

inline void zest_slang_InitialiseSession(zest_context context) {
    void *memory = zest_AllocateMemory(context, sizeof(zest_slang_info_t));
    zest_slang_info_t *slang_info = new (memory) zest_slang_info_t();
    slang_info->magic = zest_INIT_MAGIC(zest_struct_type_slang_info);
    slang::createGlobalSession(slang_info->global_session.writeRef());
    context->device->slang_info = slang_info;
}

inline void zest_slang_Shutdown(zest_context context) {
    if (context->device->slang_info) {
        zest_slang_info_t *slang_info = static_cast<zest_slang_info_t *>(context->device->slang_info);

        // Deleting the C++ object will automatically trigger the ComPtr's
        // destructor, which correctly releases the global session.
        slang_info->~zest_slang_info_t();
        zest_FreeMemory(context, slang_info);

        context->device->slang_info = 0;
    }
}

inline int zest_slang_Compile(zest_context context, const char *shader_path, const char *entry_point_name, SlangStage stage, zest_slang_compiled_shader &out_compiled_shader, slang::ShaderReflection *&out_reflection) {
    zest_slang_info_t *slang_info = zest_slang_Session(context);
    Slang::ComPtr<slang::IGlobalSession> global_session = slang_info->global_session;

    Slang::ComPtr<slang::ISession> session;
    slang::SessionDesc sessionDesc = {};
    slang::TargetDesc targetDesc = {};

    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = global_session->findProfile("spirv_1_5");
    if (targetDesc.profile == SLANG_PROFILE_UNKNOWN) {
        ZEST_APPEND_LOG(context->device->log_path.str, "Slang error: Could not find spirv_1_5 profile.");
        return -1;
    }
    // This flag is recommended for modern Slang versions when targeting Vulkan
    targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;
    global_session->createSession(sessionDesc, session.writeRef());

    Slang::ComPtr<slang::IBlob> diagnosticBlob;
    slang::IModule *slangModule = session->loadModule(shader_path, diagnosticBlob.writeRef());
    diagnoseIfNeeded(context, diagnosticBlob, "Module Loading");
    if (!slangModule) {
        ZEST_APPEND_LOG(context->device->log_path.str, "Slang failed to load module: %s", shader_path);
        return -1;
    }

    Slang::ComPtr<slang::IEntryPoint> entryPoint;
    SlangResult findEntryPointResult = slangModule->findEntryPointByName(entry_point_name, entryPoint.writeRef());
    if (SLANG_FAILED(findEntryPointResult) || !entryPoint) {
        ZEST_APPEND_LOG(context->device->log_path.str, "Failed to find entry point '%s' in '%s'", entry_point_name, shader_path);
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
        diagnoseIfNeeded(context, diagnostics, "Program Composition");
        SLANG_RETURN_ON_FAIL(result);
    }

    // Now, get the compiled code (SPIR-V)
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        SlangResult result = composedProgram->getEntryPointCode(0, 0, out_compiled_shader.writeRef(), diagnostics.writeRef());
        diagnoseIfNeeded(context, diagnostics, "SPIR-V Generation");
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
        ZEST_APPEND_LOG(context->device->log_path.str, "------------------------------------\n");
    }
    */

    return 0;
}

inline SlangStage zest__slang_GetStage(zest_shader_type type) {
    switch (type) {
    case zest_vertex_shader:   return SLANG_STAGE_VERTEX;
    case zest_fragment_shader: return SLANG_STAGE_FRAGMENT;
    case zest_compute_shader:  return SLANG_STAGE_COMPUTE;
    default: return SLANG_STAGE_NONE;
    }
}

inline zest_shader_handle zest_slang_CreateShader(zest_context context, const char *shader_path, const char *name, const char *entry_point, zest_shader_type type, bool disable_caching) {
    zest_slang_compiled_shader compiled_shader;
    slang::ShaderReflection *reflection_info = nullptr;

    SlangStage slang_stage = zest__slang_GetStage(type);
    if (slang_stage == SLANG_STAGE_NONE) {
        ZEST_APPEND_LOG(context->device->log_path.str, "Unsupported shader type for Slang compilation.");
		return {};
    }

    const char *final_entry_point = entry_point;
    if (!final_entry_point) {
        switch (type) {
        case zest_vertex_shader: final_entry_point = "vertexMain"; break;
        case zest_fragment_shader: final_entry_point = "fragmentMain"; break;
        case zest_compute_shader: final_entry_point = "computeMain"; break;
			default: ZEST_APPEND_LOG(context->device->log_path.str, "No default entry point for shader type."); return {};
        }
    }

    int result = zest_slang_Compile(context, shader_path, final_entry_point, slang_stage, compiled_shader, reflection_info);

    if (result != 0) {
        ZEST_APPEND_LOG(context->device->log_path.str, "Slang compilation failed for %s", shader_path);
		return {};
    }

    zest_uint spv_size = (zest_uint)compiled_shader->getBufferSize();
    const void *spv_binary = compiled_shader->getBufferPointer();

    zest_shader_handle shader_handle = zest_AddShaderFromSPVMemory(context, name, spv_binary, spv_size, type);
    if (!shader_handle.value) {
		return {};
    }
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(context, shader_handle.value);

    if (!disable_caching && context->create_info.flags & zest_init_flag_cache_shaders) {
        zest__cache_shader(shader);
    }

    return shader_handle;
}
