# Slang support without add_subdirectory(zest): set ZEST_ENABLE_SLANG (and optionally ZEST_SLANG_CONFIGS), include this, call zest_target_use_slang(<target>)

# IN_LIST, whatever the including project's minimum version
cmake_policy(SET CMP0057 NEW)

# Variables are set in the including scope on every include, the functions only on the first
get_property(_zest_slang_included GLOBAL PROPERTY ZEST_SLANG_MODULE_INCLUDED)
set_property(GLOBAL PROPERTY ZEST_SLANG_MODULE_INCLUDED TRUE)

# Guarded so a host's set() before the include isn't discarded by option()/set(CACHE) under old policies
if(NOT DEFINED ZEST_ENABLE_SLANG)
    option(ZEST_ENABLE_SLANG "Enable Slang shader compiler support" OFF)
endif()
if(NOT DEFINED ZEST_SLANG_CONFIGS)
    set(ZEST_SLANG_CONFIGS "" CACHE STRING "Configurations to enable Slang in, e.g. Debug. Empty enables it in every configuration.")
endif()

# Report the settings once, and again only if a later include sees different ones
get_property(_zest_slang_reported_settings GLOBAL PROPERTY ZEST_SLANG_REPORTED_SETTINGS)
set(_zest_slang_report FALSE)
if(NOT "${_zest_slang_reported_settings}" STREQUAL "${ZEST_ENABLE_SLANG}|${ZEST_SLANG_CONFIGS}")
    set(_zest_slang_report TRUE)
    set_property(GLOBAL PROPERTY ZEST_SLANG_REPORTED_SETTINGS "${ZEST_ENABLE_SLANG}|${ZEST_SLANG_CONFIGS}")
endif()

set(ZEST_SLANG_ENABLED_GENEX "0")
set(SLANG_LIBRARY_LINK "")
set(ZEST_SLANG_RUNTIME_FILES "")

if(ZEST_ENABLE_SLANG)
    if(_zest_slang_report)
        message(STATUS "Slang support enabled.")
    endif()

    # ZEST_SLANG_ENABLED_GENEX is true in the configurations Slang is enabled for
    if(ZEST_SLANG_CONFIGS)
        set(_zest_slang_config_checks "")
        foreach(_zest_slang_config IN LISTS ZEST_SLANG_CONFIGS)
            list(APPEND _zest_slang_config_checks "$<CONFIG:${_zest_slang_config}>")
        endforeach()
        string(REPLACE ";" "," _zest_slang_config_checks "${_zest_slang_config_checks}")
        set(ZEST_SLANG_ENABLED_GENEX "$<OR:${_zest_slang_config_checks}>")
        # $<CONFIG:...> ignores case, so compare upper case here too
        string(TOUPPER "${CMAKE_BUILD_TYPE}" _zest_build_type_upper)
        string(TOUPPER "${ZEST_SLANG_CONFIGS}" _zest_slang_configs_upper)
        if(_zest_slang_report)
            message(STATUS "Slang enabled for configurations: ${ZEST_SLANG_CONFIGS}")
            if(NOT CMAKE_CONFIGURATION_TYPES AND NOT _zest_build_type_upper IN_LIST _zest_slang_configs_upper)
                message(WARNING "ZEST_SLANG_CONFIGS (${ZEST_SLANG_CONFIGS}) doesn't include CMAKE_BUILD_TYPE '${CMAKE_BUILD_TYPE}', so this build has no Slang. Set CMAKE_BUILD_TYPE to one of them or clear ZEST_SLANG_CONFIGS.")
            endif()
        endif()
    else()
        set(ZEST_SLANG_ENABLED_GENEX "1")
    endif()

    # Find slang/slang.h first, then the library beside it, so another library called slang (such as S-Lang) isn't picked up
    set(_zest_slang_search_paths "")
    set(_zest_slang_include_search_paths "")
    if(DEFINED ENV{VULKAN_SDK})
        list(APPEND _zest_slang_search_paths "$ENV{VULKAN_SDK}/lib" "$ENV{VULKAN_SDK}/macOS/lib")
        list(APPEND _zest_slang_include_search_paths "$ENV{VULKAN_SDK}/Include" "$ENV{VULKAN_SDK}/include" "$ENV{VULKAN_SDK}/macOS/include")
    endif()
    list(APPEND _zest_slang_include_search_paths "/opt/homebrew/include" "/usr/local/include")

    find_path(SLANG_INCLUDE_DIR
        NAMES slang/slang.h
        HINTS ${_zest_slang_include_search_paths}
        DOC "Directory containing slang/slang.h"
    )
    if(SLANG_INCLUDE_DIR)
        list(INSERT _zest_slang_search_paths 0 "${SLANG_INCLUDE_DIR}/../lib" "${SLANG_INCLUDE_DIR}/../lib64")
    endif()

    find_library(SLANG_LIBRARY
        NAMES slang
        HINTS ${_zest_slang_search_paths}
        DOC "Path to slang library"
    )

    if(NOT SLANG_LIBRARY)
        message(FATAL_ERROR "Could not find Slang library. Set VULKAN_SDK or install slang (e.g. via brew).")
    elseif(_zest_slang_report)
        message(STATUS "Found Slang library: ${SLANG_LIBRARY}")
    endif()

    # The library must sit under the folder holding slang/slang.h and, off Windows, beside Slang companions that S-Lang lacks
    get_filename_component(_zest_slang_library_dir "${SLANG_LIBRARY}" DIRECTORY)
    set(_zest_slang_library_valid TRUE)
    if(SLANG_INCLUDE_DIR)
        get_filename_component(_zest_slang_include_root "${SLANG_INCLUDE_DIR}/.." ABSOLUTE)
        get_filename_component(_zest_slang_library_dir_absolute "${_zest_slang_library_dir}" ABSOLUTE)
        string(TOUPPER "${_zest_slang_include_root}/" _zest_slang_include_root)
        string(TOUPPER "${_zest_slang_library_dir_absolute}/" _zest_slang_library_dir_absolute)
        string(FIND "${_zest_slang_library_dir_absolute}" "${_zest_slang_include_root}" _zest_slang_root_position)
        if(NOT _zest_slang_root_position EQUAL 0)
            set(_zest_slang_library_valid FALSE)
        endif()
    elseif(_zest_slang_report)
        message(WARNING "Could not find slang/slang.h, the Slang include directory must be added by the host.")
    endif()
    if(NOT WIN32)
        file(GLOB _zest_slang_companions "${_zest_slang_library_dir}/libslang-glslang*" "${_zest_slang_library_dir}/libslang-compiler*")
        if(NOT _zest_slang_companions)
            set(_zest_slang_library_valid FALSE)
        endif()
    endif()
    if(NOT _zest_slang_library_valid)
        message(FATAL_ERROR "Found ${SLANG_LIBRARY} with headers in ${SLANG_INCLUDE_DIR}, but they don't look like one Slang install, so the library may be another one called slang such as S-Lang. Set SLANG_LIBRARY and SLANG_INCLUDE_DIR to the Slang install.")
    endif()

    # Runtime libraries that zest_copy_slang_runtime copies next to a target
    if(WIN32)
        # The DLLs sit in the bin folder beside the lib folder slang.lib was found in, so they match the linked version
        get_filename_component(_zest_slang_binary_dir "${_zest_slang_library_dir}/../bin" ABSOLUTE)
        # slang-compiler.dll does the work behind slang.dll, slang-glslang.dll runs spirv-opt, slang-glsl-module.dll is GLSL compatibility
        foreach(_zest_slang_dll slang.dll slang-compiler.dll slang-glslang.dll slang-glsl-module.dll)
            if(EXISTS "${_zest_slang_binary_dir}/${_zest_slang_dll}")
                list(APPEND ZEST_SLANG_RUNTIME_FILES "${_zest_slang_binary_dir}/${_zest_slang_dll}")
            endif()
        endforeach()
    else()
        # The found library and its companions by name, never libslang* which also matches S-Lang
        get_filename_component(_zest_slang_library_file "${SLANG_LIBRARY}" REALPATH)
        file(GLOB ZEST_SLANG_RUNTIME_FILES "${_zest_slang_library_dir}/libslang-compiler*" "${_zest_slang_library_dir}/libslang-glslang*" "${_zest_slang_library_dir}/libslang-glsl-module*")
        list(APPEND ZEST_SLANG_RUNTIME_FILES "${SLANG_LIBRARY}" "${_zest_slang_library_file}")
        list(REMOVE_DUPLICATES ZEST_SLANG_RUNTIME_FILES)
    endif()

    # Only link Slang in the configurations it is enabled for
    set(SLANG_LIBRARY_LINK "$<${ZEST_SLANG_ENABLED_GENEX}:${SLANG_LIBRARY}>")
elseif(_zest_slang_report)
    message(STATUS "Slang support disabled.")
endif()

if(NOT _zest_slang_included)
    # Copy the Slang runtime libraries next to a target after it builds, in the configurations Slang is enabled for
    function(zest_copy_slang_runtime TARGET_NAME)
        if(NOT ZEST_ENABLE_SLANG OR NOT ZEST_SLANG_RUNTIME_FILES)
            return()
        endif()
        get_target_property(_zest_target_type ${TARGET_NAME} TYPE)
        if(_zest_target_type STREQUAL "INTERFACE_LIBRARY")
            return()
        endif()
        # Once per target, however many times this is called for it
        get_target_property(_zest_runtime_copied ${TARGET_NAME} ZEST_SLANG_RUNTIME_COPIED)
        if(_zest_runtime_copied)
            return()
        endif()
        set_target_properties(${TARGET_NAME} PROPERTIES ZEST_SLANG_RUNTIME_COPIED TRUE)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E $<IF:${ZEST_SLANG_ENABLED_GENEX},copy_if_different,echo> "$<${ZEST_SLANG_ENABLED_GENEX}:${ZEST_SLANG_RUNTIME_FILES}>" "$<${ZEST_SLANG_ENABLED_GENEX}:$<TARGET_FILE_DIR:${TARGET_NAME}>>"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endfunction()

    # ZEST_ENABLE_SLANG, Slang headers, library and runtime in the enabled configurations; links with the plain signature
    function(zest_target_use_slang TARGET_NAME)
        if(NOT ZEST_ENABLE_SLANG)
            return()
        endif()
        get_target_property(_zest_target_type ${TARGET_NAME} TYPE)
        if(_zest_target_type STREQUAL "INTERFACE_LIBRARY")
            set(_zest_scope INTERFACE)
        else()
            set(_zest_scope PUBLIC)
        endif()
        target_compile_definitions(${TARGET_NAME} ${_zest_scope} $<${ZEST_SLANG_ENABLED_GENEX}:ZEST_ENABLE_SLANG>)
        if(SLANG_INCLUDE_DIR)
            target_include_directories(${TARGET_NAME} ${_zest_scope} ${SLANG_INCLUDE_DIR})
        endif()
        if(_zest_scope STREQUAL "INTERFACE")
            target_link_libraries(${TARGET_NAME} INTERFACE ${SLANG_LIBRARY_LINK})
        else()
            target_link_libraries(${TARGET_NAME} ${SLANG_LIBRARY_LINK})
        endif()
        zest_copy_slang_runtime(${TARGET_NAME})
    endfunction()
endif()
