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
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ZestSlang.cmake)
if(ZEST_ENABLE_SLANG)
    # Use #ifdef ZEST_ENABLE_SLANG in your code; it is only defined in the configurations Slang is enabled for
    target_compile_definitions(zest-interface INTERFACE $<${ZEST_SLANG_ENABLED_GENEX}:ZEST_ENABLE_SLANG>)
    target_compile_definitions(zest PRIVATE $<${ZEST_SLANG_ENABLED_GENEX}:ZEST_ENABLE_SLANG>)
    if(SLANG_INCLUDE_DIR)
        target_include_directories(zest-interface INTERFACE ${SLANG_INCLUDE_DIR})
        target_include_directories(zest PUBLIC ${SLANG_INCLUDE_DIR})
    endif()
    target_link_libraries(zest-interface INTERFACE ${SLANG_LIBRARY_LINK})
    target_link_libraries(zest PUBLIC ${SLANG_LIBRARY_LINK})
endif()
# --- End Slang Integration ---

# --- Optional D3D12 Integration ---
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ZestD3D12.cmake)
if(ZEST_D3D12_ENABLED)
    # Use #ifdef ZEST_ENABLE_D3D12 in your code
    target_compile_definitions(zest-interface INTERFACE ZEST_ENABLE_D3D12 ZEST_D3D12_SDK_VERSION=${ZEST_D3D12_SDK_VERSION})
    target_compile_definitions(zest PRIVATE ZEST_ENABLE_D3D12 ZEST_D3D12_SDK_VERSION=${ZEST_D3D12_SDK_VERSION})
    # BEFORE so the Agility headers win over the Windows SDK's
    target_include_directories(zest-interface BEFORE INTERFACE ${ZEST_D3D12_INCLUDE_DIR})
    target_include_directories(zest BEFORE PUBLIC ${ZEST_D3D12_INCLUDE_DIR})
    target_link_libraries(zest-interface INTERFACE d3d12 dxgi)
    target_link_libraries(zest PUBLIC d3d12 dxgi)
endif()
# --- End D3D12 Integration ---

# --- Optional Vulkan Integration ---
# Included before ZestMetal.cmake, so Metal knows whether Vulkan supplies shaderc
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ZestVulkan.cmake)
if(ZEST_VULKAN_ENABLED)
    # Use #ifdef ZEST_ENABLE_VULKAN in your code
    target_compile_definitions(zest-interface INTERFACE ZEST_ENABLE_VULKAN ${ZEST_VULKAN_DEFINITIONS})
    target_compile_definitions(zest PRIVATE ZEST_ENABLE_VULKAN PUBLIC ${ZEST_VULKAN_DEFINITIONS})
    target_include_directories(zest-interface INTERFACE ${ZEST_VULKAN_INCLUDE_DIRS})
    target_include_directories(zest PUBLIC ${ZEST_VULKAN_INCLUDE_DIRS})
    target_link_directories(zest-interface INTERFACE ${ZEST_VULKAN_LIBRARY_DIRS})
    target_link_directories(zest PUBLIC ${ZEST_VULKAN_LIBRARY_DIRS})
    target_link_libraries(zest-interface INTERFACE ${ZEST_VULKAN_LIBRARIES})
    target_link_libraries(zest PUBLIC ${ZEST_VULKAN_LIBRARIES})
    if(MSVC)
        target_link_options(zest-interface INTERFACE "/ignore:4099")
        target_link_options(zest PUBLIC "/ignore:4099")
    endif()
    if(WIN32)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")
    elseif(APPLE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_MACOS_MVK -DVK_EXAMPLE_XCODE_GENERATED")
    endif()
endif()
# --- End Vulkan Integration ---

# --- Optional Metal Integration ---
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ZestMetal.cmake)
if(ZEST_METAL_ENABLED)
    # Use #ifdef ZEST_ENABLE_METAL in your code
    target_compile_definitions(zest-interface INTERFACE ZEST_ENABLE_METAL)
    target_compile_definitions(zest PRIVATE ZEST_ENABLE_METAL)
    target_include_directories(zest-interface INTERFACE ${ZEST_METAL_SPIRV_CROSS_INCLUDE_DIR} ${ZEST_METAL_SHADERC_INCLUDE_DIR})
    target_include_directories(zest PUBLIC ${ZEST_METAL_SPIRV_CROSS_INCLUDE_DIR} ${ZEST_METAL_SHADERC_INCLUDE_DIR})
    # Frameworks first: Vulkan links QuartzCore too, and CMake links twice any static library between two copies of an item
    target_link_libraries(zest-interface INTERFACE ${ZEST_METAL_FRAMEWORKS} ${ZEST_METAL_SPIRV_CROSS_LIBRARIES} ${ZEST_METAL_SHADERC_LIBRARY})
    target_link_libraries(zest PUBLIC ${ZEST_METAL_FRAMEWORKS} ${ZEST_METAL_SPIRV_CROSS_LIBRARIES} ${ZEST_METAL_SHADERC_LIBRARY})
    zest_metal_implementation_source(zest ${CMAKE_CURRENT_SOURCE_DIR}/zest.cpp)
endif()
# --- End Metal Integration ---

if(NOT ZEST_VULKAN_ENABLED AND NOT ZEST_D3D12_ENABLED AND NOT ZEST_METAL_ENABLED)
    message(FATAL_ERROR "Zest has no backend to build. Install the Vulkan SDK (on Linux, your distribution's Vulkan and shaderc packages), or turn on D3D12 on Windows or Metal on Apple.")
endif()

# Find SDL2
# On Windows, the Vulkan SDK includes SDL2, and without it SDL2's VC package is downloaded. On macOS/Linux, install via package manager.
if(WIN32 AND DEFINED ENV{VULKAN_SDK})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{VULKAN_SDK}/cmake")
endif()

find_package(SDL2 QUIET)

set(ZEST_SDL2_DOWNLOADED FALSE)
if(WIN32)
    if(NOT DEFINED ZEST_SDL2_VERSION)
        set(ZEST_SDL2_VERSION "2.32.10" CACHE STRING "SDL2 version downloaded on Windows when CMake can't find SDL2")
    endif()
    # SHA256 of SDL2-devel-<version>-VC.zip, which GitHub publishes with the release. Set it yourself when you change the version.
    if(NOT DEFINED ZEST_SDL2_SHA256 AND ZEST_SDL2_VERSION STREQUAL "2.32.10")
        set(ZEST_SDL2_SHA256 "af347939395a58b365846aaea27391e69f9ec9d4dd650d6ac40802159b418a6e")
    endif()

    set(_zest_sdl2_package_dir "${CMAKE_BINARY_DIR}/_deps/SDL2-${ZEST_SDL2_VERSION}")
    if(NOT SDL2_FOUND)
        set(_zest_sdl2_package_file "${CMAKE_BINARY_DIR}/_deps/SDL2-devel-${ZEST_SDL2_VERSION}-VC.zip")
        set(_zest_sdl2_failure "")
        if(NOT EXISTS "${_zest_sdl2_package_dir}/cmake/sdl2-config.cmake")
            if(NOT ZEST_SDL2_SHA256)
                set(_zest_sdl2_failure "set ZEST_SDL2_SHA256 to the SHA256 of SDL2-devel-${ZEST_SDL2_VERSION}-VC.zip to download it")
            else()
                message(STATUS "Downloading SDL2 ${ZEST_SDL2_VERSION}")
                # The hash is checked below rather than with EXPECTED_HASH, which would stop the configure
                file(DOWNLOAD
                    "https://github.com/libsdl-org/SDL/releases/download/release-${ZEST_SDL2_VERSION}/SDL2-devel-${ZEST_SDL2_VERSION}-VC.zip"
                    "${_zest_sdl2_package_file}"
                    STATUS _zest_sdl2_download_status
                    TLS_VERIFY ON
                )
                list(GET _zest_sdl2_download_status 0 _zest_sdl2_download_code)
                if(NOT _zest_sdl2_download_code EQUAL 0)
                    set(_zest_sdl2_failure "the download failed with ${_zest_sdl2_download_status}")
                else()
                    file(SHA256 "${_zest_sdl2_package_file}" _zest_sdl2_hash)
                    string(TOLOWER "${ZEST_SDL2_SHA256}" _zest_sdl2_expected_hash)
                    if(NOT _zest_sdl2_hash STREQUAL _zest_sdl2_expected_hash)
                        set(_zest_sdl2_failure "its SHA256 is ${_zest_sdl2_hash}, not ZEST_SDL2_SHA256's ${_zest_sdl2_expected_hash}")
                    else()
                        execute_process(
                            COMMAND ${CMAKE_COMMAND} -E tar xf "${_zest_sdl2_package_file}"
                            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/_deps"
                            RESULT_VARIABLE _zest_sdl2_extract_result
                        )
                        if(NOT _zest_sdl2_extract_result EQUAL 0)
                            set(_zest_sdl2_failure "couldn't extract ${_zest_sdl2_package_file}: ${_zest_sdl2_extract_result}")
                        endif()
                    endif()
                endif()
            endif()
        endif()
        if(NOT _zest_sdl2_failure)
            find_package(SDL2 CONFIG QUIET PATHS "${_zest_sdl2_package_dir}/cmake" NO_DEFAULT_PATH)
            if(NOT SDL2_FOUND)
                set(_zest_sdl2_failure "the package has no build for this architecture")
            endif()
        endif()
        if(_zest_sdl2_failure)
            file(REMOVE "${_zest_sdl2_package_file}")
            message(WARNING "Couldn't get SDL2 ${ZEST_SDL2_VERSION}: ${_zest_sdl2_failure}. The examples will be skipped. Install the Vulkan SDK with SDL2, or set SDL2_DIR to an SDL2 install.")
        endif()
    endif()
    # A reconfigure finds the download through the cached SDL2_DIR
    if(SDL2_FOUND AND SDL2_DIR STREQUAL "${_zest_sdl2_package_dir}/cmake")
        set(ZEST_SDL2_DOWNLOADED TRUE)
        message(STATUS "Using the downloaded SDL2 ${ZEST_SDL2_VERSION}: ${_zest_sdl2_package_dir}")
    endif()
endif()

if(NOT SDL2_FOUND)
    message(STATUS "SDL2 not found. Examples will be skipped.")
    message(STATUS "Hint: On Windows, select SDL when installing Vulkan SDK, or see why the SDL2 download failed above. On macOS: brew install sdl2. On Linux: apt install libsdl2-dev")
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

add_subdirectory(submodules/freetype)
# Add the remaining example projects
if (SDL2_FOUND)
    add_subdirectory(examples/SDL2)
endif ()
