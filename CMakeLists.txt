cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(NAME Zest)
project(${NAME})

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Set C/C++ standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Suppress CRT security warnings on MSVC (for stb and similar libraries)
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# Add the zest interface library target (for header-only usage)
add_library(zest-interface INTERFACE)
target_include_directories(zest-interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Add the zest static library target (compiles to .lib)
add_library(zest STATIC zest.cpp)
target_include_directories(zest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# --- Optional Slang Integration ---
option(ZEST_ENABLE_SLANG "Enable Slang shader compiler support" OFF)
set(ZEST_SLANG_CONFIGS "" CACHE STRING "Configurations to enable Slang in, e.g. Debug. Empty enables it in every configuration.")

if(ZEST_ENABLE_SLANG)
    message(STATUS "Slang support enabled.")

    # ZEST_SLANG_ENABLED_GENEX is true in the configurations Slang is enabled for
    if(ZEST_SLANG_CONFIGS)
        set(_zest_slang_config_checks "")
        foreach(_zest_slang_config IN LISTS ZEST_SLANG_CONFIGS)
            list(APPEND _zest_slang_config_checks "$<CONFIG:${_zest_slang_config}>")
        endforeach()
        string(REPLACE ";" "," _zest_slang_config_checks "${_zest_slang_config_checks}")
        set(ZEST_SLANG_ENABLED_GENEX "$<OR:${_zest_slang_config_checks}>")
        message(STATUS "Slang enabled for configurations: ${ZEST_SLANG_CONFIGS}")
        # $<CONFIG:...> ignores case, so compare upper case here too
        string(TOUPPER "${CMAKE_BUILD_TYPE}" _zest_build_type_upper)
        string(TOUPPER "${ZEST_SLANG_CONFIGS}" _zest_slang_configs_upper)
        if(NOT CMAKE_CONFIGURATION_TYPES AND NOT _zest_build_type_upper IN_LIST _zest_slang_configs_upper)
            message(WARNING "ZEST_SLANG_CONFIGS (${ZEST_SLANG_CONFIGS}) doesn't include CMAKE_BUILD_TYPE '${CMAKE_BUILD_TYPE}', so this build has no Slang. Set CMAKE_BUILD_TYPE to one of them or clear ZEST_SLANG_CONFIGS.")
        endif()
    else()
        set(ZEST_SLANG_ENABLED_GENEX "1")
    endif()

    # Add a preprocessor definition to use in your code (#ifdef ZEST_ENABLE_SLANG)
    target_compile_definitions(zest-interface INTERFACE $<${ZEST_SLANG_ENABLED_GENEX}:ZEST_ENABLE_SLANG>)

    # Find slang/slang.h first, then the library beside it, so another library called slang (such as S-Lang) isn't picked up
    set(SLANG_SEARCH_PATHS "")
    set(SLANG_INCLUDE_SEARCH_PATHS "")
    if(DEFINED ENV{VULKAN_SDK})
        list(APPEND SLANG_SEARCH_PATHS "$ENV{VULKAN_SDK}/lib" "$ENV{VULKAN_SDK}/macOS/lib")
        list(APPEND SLANG_INCLUDE_SEARCH_PATHS "$ENV{VULKAN_SDK}/Include" "$ENV{VULKAN_SDK}/include" "$ENV{VULKAN_SDK}/macOS/include")
    endif()
    list(APPEND SLANG_INCLUDE_SEARCH_PATHS "/opt/homebrew/include" "/usr/local/include")

    find_path(SLANG_INCLUDE_DIR
        NAMES slang/slang.h
        HINTS ${SLANG_INCLUDE_SEARCH_PATHS}
        DOC "Directory containing slang/slang.h"
    )
    if(SLANG_INCLUDE_DIR)
        list(INSERT SLANG_SEARCH_PATHS 0 "${SLANG_INCLUDE_DIR}/../lib" "${SLANG_INCLUDE_DIR}/../lib64")
    endif()

    find_library(SLANG_LIBRARY
        NAMES slang
        HINTS ${SLANG_SEARCH_PATHS}
        DOC "Path to slang library"
    )

    if(NOT SLANG_LIBRARY)
        message(FATAL_ERROR "Could not find Slang library. Set VULKAN_SDK or install slang (e.g. via brew).")
    else()
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
    if(SLANG_INCLUDE_DIR)
        target_include_directories(zest-interface INTERFACE ${SLANG_INCLUDE_DIR})
        target_include_directories(zest PUBLIC ${SLANG_INCLUDE_DIR})
    else()
        message(WARNING "Could not find slang/slang.h, the Slang include directory must be added by the host.")
    endif()

    # Runtime libraries that zest_copy_slang_runtime copies next to a target
    set(ZEST_SLANG_RUNTIME_FILES "")
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
else()
	SET(SLANG_LIBRARY_LINK "")
	set(ZEST_SLANG_RUNTIME_FILES "")
    message(STATUS "Slang support disabled.")
endif()

# Copy the Slang runtime libraries next to a target after it builds, in the configurations Slang is enabled for
function(zest_copy_slang_runtime TARGET_NAME)
    if(NOT ZEST_ENABLE_SLANG OR NOT ZEST_SLANG_RUNTIME_FILES)
        return()
    endif()
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E $<IF:${ZEST_SLANG_ENABLED_GENEX},copy_if_different,echo> "$<${ZEST_SLANG_ENABLED_GENEX}:${ZEST_SLANG_RUNTIME_FILES}>" "$<${ZEST_SLANG_ENABLED_GENEX}:$<TARGET_FILE_DIR:${TARGET_NAME}>>"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
endfunction()
# --- End Slang Integration ---

# Use FindVulkan module added with CMAKE 3.7
if (NOT CMAKE_VERSION VERSION_LESS 3.7.0)
    message(STATUS "Using module to find Vulkan")
    find_package(Vulkan COMPONENTS shaderc_combined)
endif()

IF(WIN32)
    IF (NOT Vulkan_FOUND)
        find_library(Vulkan_LIBRARY NAMES vulkan-1 vulkan PATHS ${CMAKE_SOURCE_DIR}/libs/vulkan)
        IF (Vulkan_LIBRARY)
            set(Vulkan_FOUND ON)
            MESSAGE("Using bundled Vulkan library version")
        ENDIF()
    ENDIF()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")
ELSEIF(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_MACOS_MVK -DVK_EXAMPLE_XCODE_GENERATED")
ENDIF(WIN32)

IF (NOT Vulkan_FOUND)
    message(FATAL_ERROR "Could not find Vulkan library!")
ELSE()
    message(STATUS ${Vulkan_LIBRARY})
ENDIF()

# Include directories for Vulkan
include_directories(${Vulkan_INCLUDE_DIRS})
message(STATUS ${Vulkan_INCLUDE_DIRS})

# Find SDL2
# On Windows, the Vulkan SDK includes SDL2. On macOS/Linux, install via package manager.
if(WIN32 AND DEFINED ENV{VULKAN_SDK})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{VULKAN_SDK}/cmake")
endif()

find_package(SDL2 QUIET)


if(NOT SDL2_FOUND)
    message(STATUS "SDL2 not found. Examples will be skipped.")
    message(STATUS "Hint: On Windows, select SDL when installing Vulkan SDK. On macOS: brew install sdl2. On Linux: apt install libsdl2-dev")
endif()

function(buildExamples)
    foreach(EXAMPLE ${EXAMPLES})
        buildExample(${EXAMPLE})
    endforeach(EXAMPLE)
endfunction(buildExamples)

function(buildExtraExamples)
    foreach(EXAMPLE ${EXTRA_EXAMPLES})
        buildExample(${EXAMPLE})
    endforeach(EXAMPLE)
endfunction(buildExtraExamples)

# Platform-specific linking is handled via target_link_libraries on zest-interface and zest targets

# Link zest-interface with Vulkan (for header-only usage)
# Note: Vulkan SDK's shaderc is MSVC-only. For MinGW, use MSYS2's mingw-w64-x86_64-shaderc package
if(MSVC)
	if(ZEST_ENABLE_SLANG)
		target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan Vulkan::shaderc_combined ${SLANG_LIBRARY_LINK})
	else ()
		target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan Vulkan::shaderc_combined)
	endif ()
	target_link_options(zest-interface INTERFACE "/ignore:4099")
else()
	# macOS-specific frameworks
	if(APPLE)
		set(MACOS_FRAMEWORKS "-framework AppKit" "-framework QuartzCore")
	else()
		set(MACOS_FRAMEWORKS "")
	endif()

	# On Linux, shaderc_combined from system packages needs its transitive dependencies linked explicitly.
	# When using the Vulkan SDK's bundled shaderc_combined (a fully static archive), these are not needed.
	set(SHADERC_EXTRA_LIBS "")
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		set(_shaderc_transitive_libs glslang SPIRV glslang-default-resource-limits SPVRemapper SPIRV-Tools-opt SPIRV-Tools)
		set(_all_found TRUE)
		foreach(_lib IN LISTS _shaderc_transitive_libs)
			find_library(_found_${_lib} NAMES ${_lib})
			if(NOT _found_${_lib})
				set(_all_found FALSE)
				break()
			endif()
		endforeach()
		if(_all_found)
			set(SHADERC_EXTRA_LIBS ${_shaderc_transitive_libs})
		endif()
	endif()

	# Try Vulkan SDK's shaderc first (works on macOS and Linux with Vulkan SDK)
	if(TARGET Vulkan::shaderc_combined)
		if(ZEST_ENABLE_SLANG)
			target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan Vulkan::shaderc_combined ${SHADERC_EXTRA_LIBS} ${SLANG_LIBRARY_LINK} ${MACOS_FRAMEWORKS})
		else()
			target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan Vulkan::shaderc_combined ${SHADERC_EXTRA_LIBS} ${MACOS_FRAMEWORKS})
		endif()
	else()
		# Fallback to pkg-config for shaderc
		find_package(PkgConfig QUIET)
		if(PkgConfig_FOUND)
			pkg_check_modules(SHADERC QUIET shaderc)
		endif()

		if(SHADERC_FOUND)
			if(ZEST_ENABLE_SLANG)
				target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan ${SHADERC_LIBRARIES} ${SHADERC_EXTRA_LIBS} ${SLANG_LIBRARY_LINK} ${MACOS_FRAMEWORKS})
			else()
				target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan ${SHADERC_LIBRARIES} ${SHADERC_EXTRA_LIBS} ${MACOS_FRAMEWORKS})
			endif()
			target_include_directories(zest-interface INTERFACE ${SHADERC_INCLUDE_DIRS})
			target_link_directories(zest-interface INTERFACE ${SHADERC_LIBRARY_DIRS})
		else()
			message(WARNING "shaderc not found for non-MSVC build. Runtime shader compilation will not be available.")
			if(ZEST_ENABLE_SLANG)
				target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan ${SLANG_LIBRARY_LINK} ${MACOS_FRAMEWORKS})
			else()
				target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan ${MACOS_FRAMEWORKS})
			endif()
			target_compile_definitions(zest-interface INTERFACE ZEST_NO_SHADERC)
		endif()
	endif()
endif()

# Link zest static library with Vulkan
if(MSVC)
	if(ZEST_ENABLE_SLANG)
		target_compile_definitions(zest PRIVATE $<${ZEST_SLANG_ENABLED_GENEX}:ZEST_ENABLE_SLANG>)
		target_link_libraries(zest PUBLIC Vulkan::Vulkan Vulkan::shaderc_combined ${SLANG_LIBRARY_LINK})
	else ()
		target_link_libraries(zest PUBLIC Vulkan::Vulkan Vulkan::shaderc_combined)
	endif ()
	target_link_options(zest PUBLIC "/ignore:4099")
else()
	if(ZEST_ENABLE_SLANG)
		target_compile_definitions(zest PRIVATE $<${ZEST_SLANG_ENABLED_GENEX}:ZEST_ENABLE_SLANG>)
	endif()
	# Try Vulkan SDK's shaderc first
	if(TARGET Vulkan::shaderc_combined)
		if(ZEST_ENABLE_SLANG)
			target_link_libraries(zest PUBLIC Vulkan::Vulkan Vulkan::shaderc_combined ${SHADERC_EXTRA_LIBS} ${SLANG_LIBRARY_LINK} ${MACOS_FRAMEWORKS})
		else()
			target_link_libraries(zest PUBLIC Vulkan::Vulkan Vulkan::shaderc_combined ${SHADERC_EXTRA_LIBS} ${MACOS_FRAMEWORKS})
		endif()
	elseif(SHADERC_FOUND)
		if(ZEST_ENABLE_SLANG)
			target_link_libraries(zest PUBLIC Vulkan::Vulkan ${SHADERC_LIBRARIES} ${SHADERC_EXTRA_LIBS} ${SLANG_LIBRARY_LINK} ${MACOS_FRAMEWORKS})
		else()
			target_link_libraries(zest PUBLIC Vulkan::Vulkan ${SHADERC_LIBRARIES} ${SHADERC_EXTRA_LIBS} ${MACOS_FRAMEWORKS})
		endif()
		target_include_directories(zest PUBLIC ${SHADERC_INCLUDE_DIRS})
		target_link_directories(zest PUBLIC ${SHADERC_LIBRARY_DIRS})
	else()
		if(ZEST_ENABLE_SLANG)
			target_link_libraries(zest PUBLIC Vulkan::Vulkan ${SLANG_LIBRARY_LINK} ${MACOS_FRAMEWORKS})
		else()
			target_link_libraries(zest PUBLIC Vulkan::Vulkan ${MACOS_FRAMEWORKS})
		endif()
		target_compile_definitions(zest PUBLIC ZEST_NO_SHADERC)
	endif()
endif()
add_subdirectory(submodules/freetype)
# Add the remaining example projects
if (SDL2_FOUND)
    add_subdirectory(examples/SDL2)
endif ()
