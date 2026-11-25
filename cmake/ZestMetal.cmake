# Metal support without add_subdirectory(zest): set ZEST_ENABLE_METAL, include this, call zest_metal_implementation_source(<target> <file>)

# Variables are set in the including scope on every include, the function only on the first
get_property(_zest_metal_included GLOBAL PROPERTY ZEST_METAL_MODULE_INCLUDED)
set_property(GLOBAL PROPERTY ZEST_METAL_MODULE_INCLUDED TRUE)

# Guarded so a host's set() before the include isn't discarded by option() under old policies.
# On by default on Apple
if(NOT DEFINED ZEST_ENABLE_METAL)
    if(APPLE)
        set(_zest_metal_default ON)
    else()
        set(_zest_metal_default OFF)
    endif()
    option(ZEST_ENABLE_METAL "Enable the Metal backend (Apple only)" ${_zest_metal_default})
endif()

set(ZEST_METAL_ENABLED FALSE)
set(ZEST_METAL_SPIRV_CROSS_LIBRARIES "")
set(ZEST_METAL_FRAMEWORKS "")

if(ZEST_ENABLE_METAL)
    if(NOT APPLE)
        message(FATAL_ERROR "ZEST_ENABLE_METAL is only supported on Apple platforms.")
    endif()

    # The backend turns SPIR-V into MSL with SPIRV-Cross's C API, which the Vulkan SDK ships as static libraries
    set(_zest_metal_sdk_hints "")
    if(DEFINED ENV{VULKAN_SDK})
        list(APPEND _zest_metal_sdk_hints "$ENV{VULKAN_SDK}")
    endif()
    find_path(ZEST_METAL_SPIRV_CROSS_INCLUDE_DIR NAMES spirv_cross/spirv_cross_c.h HINTS ${_zest_metal_sdk_hints} PATH_SUFFIXES include)
    if(NOT ZEST_METAL_SPIRV_CROSS_INCLUDE_DIR)
        message(FATAL_ERROR "Couldn't find spirv_cross/spirv_cross_c.h. Install the Vulkan SDK and set VULKAN_SDK to its macOS folder. Configure with -DZEST_ENABLE_METAL=OFF to build without Metal.")
    endif()

    # The C wrapper references every SPIRV-Cross backend, so all of these are needed, in this order
    foreach(_zest_metal_component c msl hlsl cpp reflect glsl core)
        find_library(ZEST_METAL_SPIRV_CROSS_LIBRARY_${_zest_metal_component} NAMES spirv-cross-${_zest_metal_component} HINTS ${_zest_metal_sdk_hints} PATH_SUFFIXES lib)
        if(NOT ZEST_METAL_SPIRV_CROSS_LIBRARY_${_zest_metal_component})
            message(FATAL_ERROR "Couldn't find the spirv-cross-${_zest_metal_component} library. Install the Vulkan SDK and set VULKAN_SDK to its macOS folder. Configure with -DZEST_ENABLE_METAL=OFF to build without Metal.")
        endif()
        list(APPEND ZEST_METAL_SPIRV_CROSS_LIBRARIES "${ZEST_METAL_SPIRV_CROSS_LIBRARY_${_zest_metal_component}}")
    endforeach()

    set(ZEST_METAL_FRAMEWORKS "-framework Metal" "-framework QuartzCore" "-framework Foundation")

    # The backend needs macOS 15 for residency sets
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 15.0)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0")
    endif()

    set(ZEST_METAL_ENABLED TRUE)
    message(STATUS "Metal support enabled: SPIRV-Cross from ${ZEST_METAL_SPIRV_CROSS_INCLUDE_DIR}, deployment target ${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

if(NOT _zest_metal_included)
    # Compile the file that defines ZEST_METAL_IMPLEMENTATION as Objective-C or Objective-C++ without ARC, which zest_metal.h needs
    function(zest_metal_implementation_source TARGET_NAME SOURCE_FILE)
        if(NOT ZEST_METAL_ENABLED)
            return()
        endif()
        get_filename_component(_zest_metal_extension "${SOURCE_FILE}" EXT)
        if(_zest_metal_extension STREQUAL ".c")
            set(_zest_metal_flags " -x objective-c -fno-objc-arc")
        else()
            set(_zest_metal_flags " -x objective-c++ -fno-objc-arc")
        endif()
        # A source property only reaches a target defined in another directory when that directory is named
        if(CMAKE_VERSION VERSION_LESS 3.18)
            set_property(SOURCE "${SOURCE_FILE}" APPEND_STRING PROPERTY COMPILE_FLAGS "${_zest_metal_flags}")
        else()
            set_property(SOURCE "${SOURCE_FILE}" TARGET_DIRECTORY ${TARGET_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS "${_zest_metal_flags}")
        endif()
    endfunction()
endif()
