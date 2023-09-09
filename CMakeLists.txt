cmake_minimum_required(VERSION 3.4 FATAL_ERROR)

set(NAME zest-pocket-renderer)
project(${NAME})

include_directories(submodules/)
include_directories(submodules/imgui)
include_directories(submodules/imgui_test_engine/imgui_test_engine)
include_directories(implementations)

# Set C/C++ standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Add the zest library target
file(GLOB LIB_SRC "*.c" "*.h")
file(GLOB LIB_HEADERS  "*.h")

add_library(zest-lib STATIC ${LIB_SRC})
target_include_directories(zest-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Use FindVulkan module added with CMAKE 3.7
if (NOT CMAKE_VERSION VERSION_LESS 3.7.0)
	message(STATUS "Using module to find Vulkan")
	find_package(Vulkan)
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
find_package(glfw3 QUIET)

if (glfw3_FOUND)
    # GLFW3 found, add the GLFW3 library and examples that require it
    add_library(glfw INTERFACE)
    target_link_libraries(glfw INTERFACE glfw)
    
    # Add the example projects that require GLFW3
    #add_subdirectory(examples/zest-compute-example)
    #add_subdirectory(examples/zest-imgui-template)
	#add_subdirectory(examples/zest-msdf-font-maker)
else ()
    message(STATUS "GLFW3 not found. Examples requiring GLFW3 will be skipped.")
endif ()

function(buildExamples)
	foreach(EXAMPLE ${EXAMPLES})
		buildExample(${EXAMPLE})
	endforeach(EXAMPLE)
endfunction(buildExamples)

IF(WIN32)
	# Nothing here (yet)
ELSEIF(APPLE)
	link_libraries(${Vulkan_LIBRARY} "-framework AppKit" "-framework QuartzCore")
ELSE(WIN32)
#	link_libraries(${XCB_LIBRARIES} ${Vulkan_LIBRARY} ${Vulkan_LIBRARY} ${DIRECTFB_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF(WIN32)

# Link zest-lib with Vulkan
target_link_libraries(zest-lib Vulkan::Vulkan)
# Add the remaining example projects
add_subdirectory(examples/Simple)
