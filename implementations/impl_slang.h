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

zest_slang_info_t *zest_slang_Session(zest_context context);
void zest_slang_InitialiseSession(zest_context context);
void zest_slang_Shutdown(zest_context context);
SlangStage zest__slang_GetStage(shaderc_shader_kind kind);
int zest_slang_Compile(zest_context context, const char *shader_path, const char *entry_point_name, SlangStage stage, zest_slang_compiled_shader &out_compiled_shader, slang::ShaderReflection *&out_reflection);
zest_shader_handle zest_slang_CreateShader(zest_context context, const char *shader_path, const char *name, const char *entry_point, shaderc_shader_kind type, bool disable_caching);

