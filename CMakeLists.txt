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

# --- [NEW] Optional Slang Integration ---
option(ZEST_ENABLE_SLANG "Enable Slang shader compiler support" OFF)

if(ZEST_ENABLE_SLANG)
    message(STATUS "Slang support enabled.")

    # Add a preprocessor definition to use in your code (#ifdef ZEST_ENABLE_SLANG)
    target_compile_definitions(zest-interface INTERFACE ZEST_ENABLE_SLANG)

    # Find the Slang library. Search the Vulkan SDK first (including macOS layout),
    # then fall back to system paths (e.g. /usr/local/lib from brew).
    set(SLANG_SEARCH_PATHS "")
    if(DEFINED ENV{VULKAN_SDK})
        list(APPEND SLANG_SEARCH_PATHS "$ENV{VULKAN_SDK}/lib" "$ENV{VULKAN_SDK}/macOS/lib")
    endif()

    find_library(SLANG_LIBRARY
        NAMES slang
        PATHS ${SLANG_SEARCH_PATHS}
        DOC "Path to slang library"
    )

    if(NOT SLANG_LIBRARY)
        message(FATAL_ERROR "Could not find Slang library. Set VULKAN_SDK or install slang (e.g. via brew).")
    else()
        message(STATUS "Found Slang library: ${SLANG_LIBRARY}")
    endif()

else()
	SET(SLANG_LIBRARY "")
    message(STATUS "Slang support disabled.")
endif()
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
		target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan Vulkan::shaderc_combined ${SLANG_LIBRARY})
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
			target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan Vulkan::shaderc_combined ${SHADERC_EXTRA_LIBS} ${SLANG_LIBRARY} ${MACOS_FRAMEWORKS})
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
				target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan ${SHADERC_LIBRARIES} ${SHADERC_EXTRA_LIBS} ${SLANG_LIBRARY} ${MACOS_FRAMEWORKS})
			else()
				target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan ${SHADERC_LIBRARIES} ${SHADERC_EXTRA_LIBS} ${MACOS_FRAMEWORKS})
			endif()
			target_include_directories(zest-interface INTERFACE ${SHADERC_INCLUDE_DIRS})
			target_link_directories(zest-interface INTERFACE ${SHADERC_LIBRARY_DIRS})
		else()
			message(WARNING "shaderc not found for non-MSVC build. Runtime shader compilation will not be available.")
			if(ZEST_ENABLE_SLANG)
				target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan ${SLANG_LIBRARY} ${MACOS_FRAMEWORKS})
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
		target_compile_definitions(zest PRIVATE ZEST_ENABLE_SLANG)
		target_link_libraries(zest PUBLIC Vulkan::Vulkan Vulkan::shaderc_combined ${SLANG_LIBRARY})
	else ()
		target_link_libraries(zest PUBLIC Vulkan::Vulkan Vulkan::shaderc_combined)
	endif ()
	target_link_options(zest PUBLIC "/ignore:4099")
else()
	if(ZEST_ENABLE_SLANG)
		target_compile_definitions(zest PRIVATE ZEST_ENABLE_SLANG)
	endif()
	# Try Vulkan SDK's shaderc first
	if(TARGET Vulkan::shaderc_combined)
		if(ZEST_ENABLE_SLANG)
			target_link_libraries(zest PUBLIC Vulkan::Vulkan Vulkan::shaderc_combined ${SHADERC_EXTRA_LIBS} ${SLANG_LIBRARY} ${MACOS_FRAMEWORKS})
		else()
			target_link_libraries(zest PUBLIC Vulkan::Vulkan Vulkan::shaderc_combined ${SHADERC_EXTRA_LIBS} ${MACOS_FRAMEWORKS})
		endif()
	elseif(SHADERC_FOUND)
		if(ZEST_ENABLE_SLANG)
			target_link_libraries(zest PUBLIC Vulkan::Vulkan ${SHADERC_LIBRARIES} ${SHADERC_EXTRA_LIBS} ${SLANG_LIBRARY} ${MACOS_FRAMEWORKS})
		else()
			target_link_libraries(zest PUBLIC Vulkan::Vulkan ${SHADERC_LIBRARIES} ${SHADERC_EXTRA_LIBS} ${MACOS_FRAMEWORKS})
		endif()
		target_include_directories(zest PUBLIC ${SHADERC_INCLUDE_DIRS})
		target_link_directories(zest PUBLIC ${SHADERC_LIBRARY_DIRS})
	else()
		if(ZEST_ENABLE_SLANG)
			target_link_libraries(zest PUBLIC Vulkan::Vulkan ${SLANG_LIBRARY} ${MACOS_FRAMEWORKS})
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
