#include "impl_slang.h"

zest_slang_info_t *zest_slang_Session() {
    ZEST_ASSERT(context->renderer->slang_info);  //Slang hasn't been initialise, call zest_slang_InitialiseSession
    return static_cast<zest_slang_info_t *>(context->renderer->slang_info);
}

void zest_slang_InitialiseSession() {
    void *memory = zest_AllocateMemory(sizeof(zest_slang_info_t));
    zest_slang_info_t *slang_info = new (memory) zest_slang_info_t();
    slang_info->magic = zest_INIT_MAGIC(zest_struct_type_slang_info);
    slang::createGlobalSession(slang_info->global_session.writeRef());
    context->renderer->slang_info = slang_info;
}

void zest_slang_Shutdown() {
    if (context->renderer->slang_info) {
        zest_slang_info_t *slang_info = static_cast<zest_slang_info_t *>(context->renderer->slang_info);

        // Deleting the C++ object will automatically trigger the ComPtr's
        // destructor, which correctly releases the global session.
        slang_info->~zest_slang_info_t();
        zest_FreeMemory(slang_info);

        context->renderer->slang_info = 0;
    }
}

int zest_slang_Compile( const char *shader_path, const char *entry_point_name, SlangStage stage, zest_slang_compiled_shader &out_compiled_shader, slang::ShaderReflection *&out_reflection) {
    zest_slang_info_t *slang_info = zest_slang_Session();
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
    diagnoseIfNeeded(diagnosticBlob, "Module Loading");
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
        diagnoseIfNeeded(diagnostics, "Program Composition");
        SLANG_RETURN_ON_FAIL(result);
    }

    // Now, get the compiled code (SPIR-V)
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        SlangResult result = composedProgram->getEntryPointCode(0, 0, out_compiled_shader.writeRef(), diagnostics.writeRef());
        diagnoseIfNeeded(diagnostics, "SPIR-V Generation");
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
        ZEST_APPEND_LOG(context->device->log_path.str, "------------------------------------");
    }
    */

    return 0;
}

SlangStage zest__slang_GetStage(shaderc_shader_kind kind) {
    switch (kind) {
    case shaderc_vertex_shader:   return SLANG_STAGE_VERTEX;
    case shaderc_fragment_shader: return SLANG_STAGE_FRAGMENT;
    case shaderc_compute_shader:  return SLANG_STAGE_COMPUTE;
    default: return SLANG_STAGE_NONE;
    }
}

zest_shader zest_slang_CreateShader(const char *shader_path, const char *name, const char *entry_point, shaderc_shader_kind type, bool disable_caching) {
    zest_slang_compiled_shader compiled_shader;
    slang::ShaderReflection *reflection_info = nullptr;

    SlangStage slang_stage = zest__slang_GetStage(type);
    if (slang_stage == SLANG_STAGE_NONE) {
        ZEST_APPEND_LOG(context->device->log_path.str, "Unsupported shader type for Slang compilation.");
        return nullptr;
    }

    const char *final_entry_point = entry_point;
    if (!final_entry_point) {
        switch (type) {
        case shaderc_vertex_shader: final_entry_point = "vertexMain"; break;
        case shaderc_fragment_shader: final_entry_point = "fragmentMain"; break;
        case shaderc_compute_shader: final_entry_point = "computeMain"; break;
        default: ZEST_APPEND_LOG(context->device->log_path.str, "No default entry point for shader type."); return nullptr;
        }
    }

    int result = zest_slang_Compile(shader_path, final_entry_point, slang_stage, compiled_shader, reflection_info);

    if (result != 0) {
        ZEST_APPEND_LOG(context->device->log_path.str, "Slang compilation failed for %s", shader_path);
        return nullptr;
    }

    zest_uint spv_size = (zest_uint)compiled_shader->getBufferSize();
    const void *spv_binary = compiled_shader->getBufferPointer();

    zest_shader shader = zest_AddShaderFromSPVMemory(name, spv_binary, spv_size, type);
    if (!shader) {
        return nullptr;
    }

    shader->stage = (VkShaderStageFlagBits)slang_stage; // Note: SlangStage maps directly to VkShaderStageFlagBits

    if (!disable_caching && ZestApp->create_info.flags & zest_init_flag_cache_shaders) {
        zest_CacheShader(shader);
    }

    return shader;
}
