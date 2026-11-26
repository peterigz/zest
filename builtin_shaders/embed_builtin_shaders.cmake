# Compiles Zest's built-in shaders and embeds each binary in its backend header as an array of 32-bit words,
# so creating them never starts shaderc or DXC. Run it after editing anything in builtin_shaders/:
#     cmake [-DGLSLC=<path to glslc>] [-DDXC=<path to dxc.exe>] -P builtin_shaders/embed_builtin_shaders.cmake
# glslc comes with the Vulkan SDK and with shaderc, and is searched for under VULKAN_SDK, then on PATH.
# dxc comes from the DXC NuGet package that ZestD3D12.cmake downloads, so the arrays match the version Zest pins. Without
# -DDXC it's searched for in the sibling Zest-Build folder's _deps. The Vulkan SDK's dxc is a different version.
# Every run compiles every array, so it needs Windows for dxc.exe.
# -DCHECK=ON compiles nothing, and fails if a source changed since its array was generated.
# Each header's arrays sit between "//Begin generated shaders" and "//End generated shaders", which this script replaces.

cmake_minimum_required(VERSION 3.20)

get_filename_component(ZEST_EMBED_REPO "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(ZEST_EMBED_HEADERS zest_vulkan.h zest_metal.h zest_dx12.h)
set(ZEST_EMBED_BEGIN_MARKER "//Begin generated shaders")
set(ZEST_EMBED_END_MARKER "//End generated shaders")

# One entry per array: source|compiler|glslc stage or dxc entry point|dxc profile|header|array
# glslc's defaults match the runtime's shaderc: -O0, a Vulkan 1.0 target and no debug info. Vulkan and Metal get the same SPIR-V.
set(ZEST_EMBED_ARRAYS
    "db_overlay.vert|glslc|vert|-|zest_vulkan.h|zest__vk_db_overlay_vertex_spv"
    "db_overlay.frag|glslc|frag|-|zest_vulkan.h|zest__vk_db_overlay_fragment_spv"
    "db_overlay.vert|glslc|vert|-|zest_metal.h|zest__mtl_db_overlay_vertex_spv"
    "db_overlay.frag|glslc|frag|-|zest_metal.h|zest__mtl_db_overlay_fragment_spv"
    "dx12_blit.hlsl|dxc|vertex_main|vs_6_0|zest_dx12.h|zest__dx_blit_vertex_dxil"
    "dx12_blit.hlsl|dxc|pixel_main|ps_6_0|zest_dx12.h|zest__dx_blit_pixel_dxil"
    "dx12_db_overlay.vert.hlsl|dxc|main|vs_6_0|zest_dx12.h|zest__dx_db_overlay_vertex_dxil"
    "dx12_db_overlay.frag.hlsl|dxc|main|ps_6_0|zest_dx12.h|zest__dx_db_overlay_pixel_dxil"
)

# The first 16 hex digits of the source's SHA256, taken with CRLF turned to LF so Windows and Linux checkouts agree
function(zest_embed_source_hash source out_hash)
    file(READ "${ZEST_EMBED_REPO}/builtin_shaders/${source}" source_text)
    string(REPLACE "\r\n" "\n" source_text "${source_text}")
    string(SHA256 source_hash "${source_text}")
    string(SUBSTRING "${source_hash}" 0 16 source_hash)
    set(${out_hash} "${source_hash}" PARENT_SCOPE)
endfunction()

if(CHECK)
    set(stale_arrays "")
    foreach(entry IN LISTS ZEST_EMBED_ARRAYS)
        string(REPLACE "|" ";" fields "${entry}")
        list(GET fields 0 source)
        list(GET fields 4 header)
        list(GET fields 5 array)
        zest_embed_source_hash("${source}" source_hash)
        file(READ "${ZEST_EMBED_REPO}/${header}" header_text)
        string(REPLACE "." "\\." source_pattern "${source}")
        string(REGEX MATCH "//builtin_shaders/${source_pattern}, source sha256 ([0-9a-f]+)\\.[^\n]*\nstatic const zest_uint ${array}\\[" recorded "${header_text}")
        if(NOT recorded)
            list(APPEND stale_arrays "${array} is missing from ${header}.")
        elseif(NOT CMAKE_MATCH_1 STREQUAL source_hash)
            list(APPEND stale_arrays "${array} in ${header} is stale: builtin_shaders/${source} changed since it was generated.")
        endif()
    endforeach()
    if(stale_arrays)
        list(JOIN stale_arrays "\n" stale_report)
        message(FATAL_ERROR "${stale_report}\nRun cmake -P builtin_shaders/embed_builtin_shaders.cmake to regenerate the arrays, and commit the headers with the sources.")
    endif()
    message(STATUS "Every embedded built-in shader matches its source.")
    return()
endif()

if(NOT GLSLC)
    find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
endif()
if(NOT GLSLC OR NOT EXISTS "${GLSLC}")
    message(FATAL_ERROR "Couldn't find glslc. Install the Vulkan SDK or shaderc, or pass -DGLSLC=<path to glslc>.")
endif()
if(NOT DXC)
    file(GLOB dxc_candidates "${ZEST_EMBED_REPO}/../Zest-Build/_deps/dxc-*/build/native/bin/x64/dxc.exe")
    if(dxc_candidates)
        list(SORT dxc_candidates COMPARE NATURAL ORDER DESCENDING)
        list(GET dxc_candidates 0 DXC)
    endif()
endif()
if(NOT DXC OR NOT EXISTS "${DXC}")
    message(FATAL_ERROR "Couldn't find dxc. Configure a Windows build with ZEST_ENABLE_D3D12, which downloads DXC, then pass -DDXC=<build folder>/_deps/dxc-<version>/build/native/bin/x64/dxc.exe.")
endif()
message(STATUS "glslc: ${GLSLC}")
message(STATUS "dxc: ${DXC}")

if(DEFINED ENV{TEMP})
    file(TO_CMAKE_PATH "$ENV{TEMP}" temp_root)
elseif(DEFINED ENV{TMPDIR})
    file(TO_CMAKE_PATH "$ENV{TMPDIR}" temp_root)
else()
    set(temp_root "/tmp")
endif()
set(binary_folder "${temp_root}/zest_embed_builtin_shaders")
file(REMOVE_RECURSE "${binary_folder}")
file(MAKE_DIRECTORY "${binary_folder}")

foreach(header IN LISTS ZEST_EMBED_HEADERS)
    file(READ "${ZEST_EMBED_REPO}/${header}" header_text)
    string(FIND "${header_text}" "\r\n" crlf_position)
    if(crlf_position GREATER_EQUAL 0)
        set(newline "\r\n")
    else()
        set(newline "\n")
    endif()

    set(region "")
    foreach(entry IN LISTS ZEST_EMBED_ARRAYS)
        string(REPLACE "|" ";" fields "${entry}")
        list(GET fields 4 entry_header)
        if(NOT entry_header STREQUAL header)
            continue()
        endif()
        list(GET fields 0 source)
        list(GET fields 1 compiler)
        list(GET fields 2 stage_or_entry)
        list(GET fields 3 profile)
        list(GET fields 5 array)

        # A binary that two headers embed is compiled once
        set(binary "${binary_folder}/${source}.${stage_or_entry}.bin")
        if(NOT EXISTS "${binary}")
            if(compiler STREQUAL "glslc")
                set(command "${GLSLC}" -fshader-stage=${stage_or_entry} -o "${binary}" "${ZEST_EMBED_REPO}/builtin_shaders/${source}")
            else()
                set(command "${DXC}" -E ${stage_or_entry} -T ${profile} -Fo "${binary}" "${ZEST_EMBED_REPO}/builtin_shaders/${source}")
            endif()
            execute_process(COMMAND ${command} RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_output ERROR_VARIABLE compile_output)
            if(NOT compile_result EQUAL 0 OR NOT EXISTS "${binary}")
                message(FATAL_ERROR "${compiler} couldn't compile builtin_shaders/${source} for ${array}:\n${compile_output}")
            endif()
            if(compile_output)
                message(WARNING "${compiler} reported this for builtin_shaders/${source}:\n${compile_output}")
            endif()
        endif()

        file(READ "${binary}" binary_hex HEX)
        string(LENGTH "${binary_hex}" hex_length)
        math(EXPR byte_count "${hex_length} / 2")
        math(EXPR byte_remainder "${byte_count} % 4")
        if(byte_count EQUAL 0 OR NOT byte_remainder EQUAL 0)
            message(FATAL_ERROR "${compiler} gave ${byte_count} bytes for ${array}, which isn't a whole number of 32-bit words. Check the compiler's output.")
        endif()
        math(EXPR word_count "${byte_count} / 4")
        # Both formats are little endian, so each word's 4 bytes are reversed into a hex literal
        string(REGEX REPLACE "(..)(..)(..)(..)" "0x\\4\\3\\2\\1;" words "${binary_hex}")
        string(REGEX REPLACE ";$" "" words "${words}")

        zest_embed_source_hash("${source}" source_hash)
        string(APPEND region "//builtin_shaders/${source}, source sha256 ${source_hash}. Generated by builtin_shaders/embed_builtin_shaders.cmake, don't edit.${newline}")
        string(APPEND region "static const zest_uint ${array}[${word_count}] = {${newline}")
        set(word_index 0)
        set(column 0)
        foreach(word IN LISTS words)
            math(EXPR word_index "${word_index} + 1")
            if(column EQUAL 0)
                string(APPEND region "\t")
            endif()
            string(APPEND region "${word}")
            if(word_index EQUAL word_count)
                string(APPEND region "${newline}")
            elseif(column EQUAL 7)
                string(APPEND region ",${newline}")
                set(column 0)
            else()
                string(APPEND region ", ")
                math(EXPR column "${column} + 1")
            endif()
        endforeach()
        string(APPEND region "};${newline}")
        message(STATUS "${array}: ${word_count} words in ${header}")
    endforeach()

    string(FIND "${header_text}" "${ZEST_EMBED_BEGIN_MARKER}" begin_position)
    string(FIND "${header_text}" "${ZEST_EMBED_END_MARKER}" end_position)
    if(begin_position LESS 0 OR end_position LESS begin_position)
        message(FATAL_ERROR "${header} needs a \"${ZEST_EMBED_BEGIN_MARKER}\" line followed by an \"${ZEST_EMBED_END_MARKER}\" line in its Shader_code section.")
    endif()
    string(LENGTH "${ZEST_EMBED_BEGIN_MARKER}" marker_length)
    math(EXPR prefix_length "${begin_position} + ${marker_length}")
    string(SUBSTRING "${header_text}" 0 ${prefix_length} prefix)
    string(SUBSTRING "${header_text}" ${end_position} -1 suffix)
    set(generated_text "${prefix}${newline}${region}${suffix}")
    if(generated_text STREQUAL header_text)
        message(STATUS "${header} is up to date.")
    else()
        file(WRITE "${ZEST_EMBED_REPO}/${header}" "${generated_text}")
        message(STATUS "${header} updated.")
    endif()
endforeach()

file(REMOVE_RECURSE "${binary_folder}")
