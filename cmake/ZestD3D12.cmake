# D3D12 support without add_subdirectory(zest): set ZEST_ENABLE_D3D12, include this, call zest_copy_d3d12_runtime(<target>)

# Variables are set in the including scope on every include, the function only on the first
get_property(_zest_d3d12_included GLOBAL PROPERTY ZEST_D3D12_MODULE_INCLUDED)
set_property(GLOBAL PROPERTY ZEST_D3D12_MODULE_INCLUDED TRUE)

# Guarded so a host's set() before the include isn't discarded by option()/set(CACHE) under old policies
if(NOT DEFINED ZEST_ENABLE_D3D12)
    option(ZEST_ENABLE_D3D12 "Enable the D3D12 backend (Windows only)" OFF)
endif()
if(NOT DEFINED ZEST_D3D12_AGILITY_VERSION)
    set(ZEST_D3D12_AGILITY_VERSION "1.615.1" CACHE STRING "Microsoft.Direct3D.D3D12 NuGet package version")
endif()
# SHA512 of the .nupkg in hex, which nuget.org publishes base64 encoded as packageHash. Set it yourself when you change the version.
if(NOT DEFINED ZEST_D3D12_AGILITY_SHA512 AND ZEST_D3D12_AGILITY_VERSION STREQUAL "1.615.1")
    set(ZEST_D3D12_AGILITY_SHA512 "cee32540e7ffb7d29165974e2daf9bc067ff225fa201f58abd4bd841a3d6a5b6a90e98dddedc27b256168927bfb3b49a7a7963fd8e26cf136a25dbffb27e3f25")
endif()

set(ZEST_D3D12_ENABLED FALSE)
set(ZEST_D3D12_SDK_VERSION "")
set(ZEST_D3D12_INCLUDE_DIR "")
set(ZEST_D3D12_RUNTIME_FILES "")

if(ZEST_ENABLE_D3D12)
    if(NOT WIN32)
        message(FATAL_ERROR "ZEST_ENABLE_D3D12 is only supported on Windows.")
    endif()

    # The runtime DLLs have to match the architecture of the executable that loads them
    if(CMAKE_GENERATOR_PLATFORM)
        string(TOLOWER "${CMAKE_GENERATOR_PLATFORM}" _zest_d3d12_target)
    elseif(CMAKE_VS_PLATFORM_NAME)
        string(TOLOWER "${CMAKE_VS_PLATFORM_NAME}" _zest_d3d12_target)
    # Ninja and Makefile generators: the compiler's own target, because an x64 to ARM64 MSVC toolchain leaves the processor at the host's
    elseif(CMAKE_C_COMPILER_ARCHITECTURE_ID)
        string(TOLOWER "${CMAKE_C_COMPILER_ARCHITECTURE_ID}" _zest_d3d12_target)
    elseif(CMAKE_CXX_COMPILER_ARCHITECTURE_ID)
        string(TOLOWER "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" _zest_d3d12_target)
    elseif(MSVC_C_ARCHITECTURE_ID)
        string(TOLOWER "${MSVC_C_ARCHITECTURE_ID}" _zest_d3d12_target)
    elseif(MSVC_CXX_ARCHITECTURE_ID)
        string(TOLOWER "${MSVC_CXX_ARCHITECTURE_ID}" _zest_d3d12_target)
    else()
        # A cross-compiling toolchain file sets the target's processor, otherwise it's the host's
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _zest_d3d12_target)
        # A 32-bit toolchain on a 64-bit host reports the host's processor
        if(NOT CMAKE_CROSSCOMPILING AND CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(_zest_d3d12_target "x86")
        endif()
    endif()
    if(_zest_d3d12_target MATCHES "^(arm64|aarch64)$")
        set(_zest_d3d12_architecture "arm64")
    elseif(_zest_d3d12_target MATCHES "^(win32|x86|i[3-6]86)$")
        set(_zest_d3d12_architecture "win32")
    else()
        set(_zest_d3d12_architecture "x64")
    endif()

    set(_zest_d3d12_package_dir "${CMAKE_BINARY_DIR}/_deps/agility-${ZEST_D3D12_AGILITY_VERSION}")
    set(_zest_d3d12_bin_dir "${_zest_d3d12_package_dir}/build/native/bin/${_zest_d3d12_architecture}")

    if(NOT EXISTS "${_zest_d3d12_package_dir}/Microsoft.Direct3D.D3D12.nuspec")
        if(NOT ZEST_D3D12_AGILITY_SHA512)
            message(FATAL_ERROR "Set ZEST_D3D12_AGILITY_SHA512 to the SHA512 of the Agility SDK ${ZEST_D3D12_AGILITY_VERSION} package, as hex. nuget.org publishes it base64 encoded, as packageHash in the version's catalog entry, so decode it to hex first.")
        endif()
        set(_zest_d3d12_package_file "${CMAKE_BINARY_DIR}/_deps/agility-${ZEST_D3D12_AGILITY_VERSION}.nupkg")
        message(STATUS "Downloading the Agility SDK ${ZEST_D3D12_AGILITY_VERSION}")
        file(DOWNLOAD
            "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/${ZEST_D3D12_AGILITY_VERSION}"
            "${_zest_d3d12_package_file}"
            STATUS _zest_d3d12_download_status
            TLS_VERIFY ON
            EXPECTED_HASH SHA512=${ZEST_D3D12_AGILITY_SHA512}
        )
        list(GET _zest_d3d12_download_status 0 _zest_d3d12_download_code)
        if(NOT _zest_d3d12_download_code EQUAL 0)
            file(REMOVE "${_zest_d3d12_package_file}")
            message(FATAL_ERROR "Couldn't download the Agility SDK ${ZEST_D3D12_AGILITY_VERSION}: ${_zest_d3d12_download_status}")
        endif()
        file(MAKE_DIRECTORY "${_zest_d3d12_package_dir}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xf "${_zest_d3d12_package_file}"
            WORKING_DIRECTORY "${_zest_d3d12_package_dir}"
            RESULT_VARIABLE _zest_d3d12_extract_result
        )
        if(NOT _zest_d3d12_extract_result EQUAL 0)
            message(FATAL_ERROR "Couldn't extract ${_zest_d3d12_package_file}: ${_zest_d3d12_extract_result}")
        endif()
    endif()

    if(NOT IS_DIRECTORY "${_zest_d3d12_bin_dir}")
        message(FATAL_ERROR "The Agility SDK ${ZEST_D3D12_AGILITY_VERSION} package has no ${_zest_d3d12_architecture} runtime for the target '${_zest_d3d12_target}': ${_zest_d3d12_bin_dir} doesn't exist.")
    endif()
    if(NOT EXISTS "${_zest_d3d12_bin_dir}/D3D12Core.dll")
        message(FATAL_ERROR "The Agility SDK package doesn't contain ${_zest_d3d12_bin_dir}/D3D12Core.dll")
    endif()

    # 1.615.1 gives 615
    string(REPLACE "." ";" _zest_d3d12_version_parts "${ZEST_D3D12_AGILITY_VERSION}")
    list(GET _zest_d3d12_version_parts 1 ZEST_D3D12_SDK_VERSION)
    set(ZEST_D3D12_INCLUDE_DIR "${_zest_d3d12_package_dir}/build/native/include")
    set(ZEST_D3D12_RUNTIME_FILES
        "${_zest_d3d12_bin_dir}/D3D12Core.dll"
        "${_zest_d3d12_bin_dir}/d3d12SDKLayers.dll"
    )
    set(ZEST_D3D12_ENABLED TRUE)
    message(STATUS "D3D12 support enabled: Agility SDK ${ZEST_D3D12_AGILITY_VERSION}, SDK version ${ZEST_D3D12_SDK_VERSION}, ${_zest_d3d12_architecture} runtime")
endif()

if(NOT _zest_d3d12_included)
    # Copy the Agility runtime into D3D12/ next to a target after it builds
    function(zest_copy_d3d12_runtime TARGET_NAME)
        if(NOT ZEST_D3D12_ENABLED)
            return()
        endif()
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET_NAME}>/D3D12"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ZEST_D3D12_RUNTIME_FILES} "$<TARGET_FILE_DIR:${TARGET_NAME}>/D3D12"
            VERBATIM
        )
    endfunction()
endif()
