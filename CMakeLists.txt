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

    # Find the Slang library within the Vulkan SDK directory
    if(NOT DEFINED ENV{VULKAN_SDK})
        message(FATAL_ERROR "ZEST_ENABLE_SLANG is ON, but the VULKAN_SDK environment variable is not set.")
    endif()

    find_library(SLANG_LIBRARY
        NAMES slang
        PATHS "$ENV{VULKAN_SDK}/lib"
        DOC "Path to slang.lib"
    )

    if(NOT SLANG_LIBRARY)
        message(FATAL_ERROR "Could not find Slang library under $ENV{VULKAN_SDK}/lib. Please check your Vulkan SDK installation.")
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

# Find SDL2 - prefer Vulkan SDK's cmake config
if(DEFINED ENV{VULKAN_SDK})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{VULKAN_SDK}/cmake")
endif()

find_package(SDL2 QUIET)

if(NOT SDL2_FOUND)
    message(STATUS "SDL2 not found. Examples will be skipped.")
    message(STATUS "Hint: Make sure you select the SDL component when installing the Vulkan SDK.")
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

IF(WIN32)
    # Nothing here (yet)
ELSEIF(APPLE)
    link_libraries(Vulkan::Vulkan Vulkan::shaderc_combined ${SDL2_LIBRARIES} "-framework AppKit" "-framework QuartzCore")
ELSE(WIN32)
#   link_libraries(${XCB_LIBRARIES} ${Vulkan_LIBRARY} ${DIRECTFB_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF(WIN32)

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
	# For non-MSVC, use shaderc shared library (static has too many transitive dependencies)
	# Users need shaderc DLL in PATH or next to executable
	find_package(PkgConfig QUIET)
	if(PkgConfig_FOUND)
		pkg_check_modules(SHADERC QUIET shaderc)
	endif()
	if(SHADERC_FOUND)
		if(ZEST_ENABLE_SLANG)
			target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan ${SHADERC_LIBRARIES} ${SLANG_LIBRARY})
		else()
			target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan ${SHADERC_LIBRARIES})
		endif()
		target_include_directories(zest-interface INTERFACE ${SHADERC_INCLUDE_DIRS})
		target_link_directories(zest-interface INTERFACE ${SHADERC_LIBRARY_DIRS})
	else()
		message(WARNING "shaderc not found for non-MSVC build. Runtime shader compilation will not be available.")
		if(ZEST_ENABLE_SLANG)
			target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan ${SLANG_LIBRARY})
		else()
			target_link_libraries(zest-interface INTERFACE Vulkan::Vulkan)
		endif()
		target_compile_definitions(zest-interface INTERFACE ZEST_NO_SHADERC)
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
	if(SHADERC_FOUND)
		if(ZEST_ENABLE_SLANG)
			target_link_libraries(zest PUBLIC Vulkan::Vulkan ${SHADERC_LIBRARIES} ${SLANG_LIBRARY})
		else()
			target_link_libraries(zest PUBLIC Vulkan::Vulkan ${SHADERC_LIBRARIES})
		endif()
		target_include_directories(zest PUBLIC ${SHADERC_INCLUDE_DIRS})
		target_link_directories(zest PUBLIC ${SHADERC_LIBRARY_DIRS})
	else()
		if(ZEST_ENABLE_SLANG)
			target_link_libraries(zest PUBLIC Vulkan::Vulkan ${SLANG_LIBRARY})
		else()
			target_link_libraries(zest PUBLIC Vulkan::Vulkan)
		endif()
		target_compile_definitions(zest PUBLIC ZEST_NO_SHADERC)
	endif()
endif()
add_subdirectory(submodules/freetype)
# Add the remaining example projects
if (SDL2_FOUND)
    add_subdirectory(examples/SDL2)
endif ()
