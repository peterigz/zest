cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(NAME Zest)
project(${NAME})

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Set C/C++ standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Add the zest library target
file(GLOB LIB_SRC "*.c" "*.h")
file(GLOB LIB_HEADERS "*.h")

add_library(zest-lib STATIC ${LIB_SRC})
target_include_directories(zest-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# --- [NEW] Optional Slang Integration ---
option(ZEST_ENABLE_SLANG "Enable Slang shader compiler support" OFF)

if(ZEST_ENABLE_SLANG)
    message(STATUS "Slang support enabled. Compiling zest-lib as C++.")

    # Add a preprocessor definition to use in your code (#ifdef ZEST_ENABLE_SLANG)
    target_compile_definitions(zest-lib PUBLIC ZEST_ENABLE_SLANG)

    # Find the Slang library within the Vulkan SDK directory
    if(NOT DEFINED ENV{VULKAN_SDK})
        message(FATAL_ERROR "ZEST_ENABLE_SLANG is ON, but the VULKAN_SDK environment variable is not set.")
    endif()

    find_library(SLANG_LIBRARY
        NAMES slang
        PATHS "$ENV{VULKAN_SDK}/Lib"
        DOC "Path to slang.lib"
    )

    if(NOT SLANG_LIBRARY)
        message(FATAL_ERROR "Could not find slang.lib under $ENV{VULKAN_SDK}/Lib. Please check your Vulkan SDK installation.")
    else()
        message(STATUS "Found Slang library: ${SLANG_LIBRARY}")
    endif()

else()
	SET(SLANG_LIBRARY "")
    message(STATUS "Slang support disabled. Compiling zest-lib as C.")
endif()
# --- End Slang Integration ---

option(ADD_MSDF_FONT_BUILDER "OPTIONAL: Add the msdf font builder project - you'll need to set up msdf libraries yourself and link them below")
FIND_LIBRARY(MSDF_ATLAS_LIB NAMES msdf-atlas-gen.lib)
FIND_LIBRARY(MSDF_GEN_CORE NAMES msdfgen-core.lib)
FIND_LIBRARY(MSDF_GEN_EXT NAMES msdfgen-ext.lib)

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

# Check if GLFW3 is available
find_package(GLFW3 QUIET)

# Check if SDL is available
find_package(SDL2 QUIET)

if (GLFW3_FOUND)
    # GLFW3 found, add the GLFW3 library and examples that require it
    add_library(GLFW3 INTERFACE)
    IF(APPLE)
    include_directories(${GLFW3_INCLUDE_DIR})
    ENDIF(APPLE)
    target_link_libraries(GLFW3 INTERFACE glfw)
else ()
    IF(APPLE)
        message(FATAL_ERROR "Mac builds currently require GLFW3 - not found.")
    ELSE(APPLE)
        message(STATUS "GLFW3 not found. Examples requiring GLFW3 will be skipped.")
    ENDIF(APPLE)
endif ()

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
    link_libraries(Vulkan::Vulkan Vulkan::shaderc_combined ${GLFW3_LIBRARY} "-framework AppKit" "-framework QuartzCore")
ELSE(WIN32)
#   link_libraries(${XCB_LIBRARIES} ${Vulkan_LIBRARY} ${DIRECTFB_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF(WIN32)

# Link zest-lib with Vulkan
        # Link Slang to zest-lib
if(ZEST_ENABLE_SLANG)
	target_link_libraries(zest-lib PUBLIC Vulkan::Vulkan Vulkan::shaderc_combined ${SLANG_LIBRARY})
else ()
	target_link_libraries(zest-lib Vulkan::Vulkan Vulkan::shaderc_combined)
endif ()
set_target_properties(zest-lib PROPERTIES LINK_FLAGS "/ignore:4099")
add_subdirectory(submodules/freetype)
# Add the remaining example projects
add_subdirectory(examples/Simple)
if (GLFW3_FOUND)
    add_subdirectory(examples/GLFW)
endif ()

if (SDL2_FOUND)
    add_subdirectory(examples/SDL2)
endif ()