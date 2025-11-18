function(buildExample EXAMPLE_NAME)
    SET(EXAMPLE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_NAME})

	include_directories(${CMAKE_SOURCE_DIR}/)
	include_directories(${CMAKE_SOURCE_DIR}/implementations) 
	include_directories(${CMAKE_SOURCE_DIR}/submodules/)

	SET(IMPLEMENTATIONS ${CMAKE_SOURCE_DIR}/implementations/)
    
    # Separate C and C++ source files
    file(GLOB C_SOURCE ${EXAMPLE_FOLDER}/*.c)
    file(GLOB CPP_SOURCE ${EXAMPLE_FOLDER}/*.cpp)
    file(GLOB HPP_SOURCE ${EXAMPLE_FOLDER}/*.hpp)
    file(GLOB HEADERS ${EXAMPLE_FOLDER}/*.h)
    
    if(EXISTS ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.h)
        SET(MAIN_HEADER ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.h)
    ENDIF()

	SET(TIMELINEFX ${CMAKE_SOURCE_DIR}/submodules/timelinefxlib/)
    
    if(${EXAMPLE_NAME} STREQUAL "zest-timelinefx-c")
        SET(MAIN ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.c)
        # Only use C sources for this project
        SET(SOURCE ${C_SOURCE})
		file(GLOB TIMELINEFX_SOURCE 
			${TIMELINEFX}timelinefx.h
			${TIMELINEFX}timelinefx.cpp
		)
		file(GLOB IMPLEMENTATIONS_SOURCE
			${IMPLEMENTATIONS}impl_timelinefx.c
		)
    elseif(${EXAMPLE_NAME} STREQUAL "zest-zloc")
        SET(MAIN ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.c)
        SET(SOURCE ${C_SOURCE} ${CPP_SOURCE} ${HPP_SOURCE})
        include_directories(${CMAKE_SOURCE_DIR}/submodules/zloc)
		file(GLOB IMPLEMENTATIONS_SOURCE
			${CMAKE_SOURCE_DIR}/submodules/zloc/zloc.h
		)
		SET(TIMELINEFX_SOURCE "")
    else()
        SET(MAIN ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.cpp)
        SET(SOURCE ${C_SOURCE} ${CPP_SOURCE} ${HPP_SOURCE})
		SET(TIMELINEFX_SOURCE "")
		SET(IMPLEMENTATIONS_SOURCE "")
    endif()

    if(${EXAMPLE_NAME} STREQUAL "zest-slang")
		file(GLOB SLANG_SOURCE
			${IMPLEMENTATIONS}impl_slang.cpp
		)
    else()
		SET(SLANG_SOURCE "")
    endif()
    
    set(SHADER_DIR "${EXAMPLE_NAME}/shaders")
    file(GLOB SHADERS "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag" "${SHADER_DIR}/*.comp")
    source_group("Shaders" FILES ${SHADERS})
    
    if(WIN32)
        add_executable(${EXAMPLE_NAME} ${MAIN} ${SOURCE} ${SLANG_SOURCE} ${IMPLEMENTATIONS_SOURCE} ${TIMELINEFX_SOURCE} ${MAIN_HEADER} ${SHADERS})
    else(WIN32)
        add_executable(${EXAMPLE_NAME} ${MAIN} ${SOURCE} ${MAIN_HEADER} ${SLANG_SOURCE} ${IMPLEMENTATIONS_SOURCE} ${TIMELINEFX_SOURCE} ${SHADERS})
    endif(WIN32)
    
    # Set C-specific properties for zest-timelinefx-c
    if(${EXAMPLE_NAME} STREQUAL "zest-timelinefx-c")
        set_target_properties(${EXAMPLE_NAME} PROPERTIES
            C_STANDARD 11  # or whatever C standard you want to use
            C_STANDARD_REQUIRED ON
            LINKER_LANGUAGE C
        )
        # Disable C++ features for this target
        target_compile_features(${EXAMPLE_NAME} PRIVATE c_std_11)  # or whatever C standard you want
		target_include_directories(${EXAMPLE_NAME} PUBLIC ${TIMELINEFX}) 
    elseif(${EXAMPLE_NAME} STREQUAL "zest-zloc")
        set_target_properties(${EXAMPLE_NAME} PROPERTIES
            C_STANDARD 11
            C_STANDARD_REQUIRED ON
            LINKER_LANGUAGE C
        )
        target_compile_features(${EXAMPLE_NAME} PRIVATE c_std_11)
        target_include_directories(${EXAMPLE_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/submodules/zloc)
    endif()
    
    if(WIN32)
        target_link_libraries(${EXAMPLE_NAME} zest-interface ${Vulkan_LIBRARY} ${WINLIBS})
    else(WIN32)
        target_link_libraries(${EXAMPLE_NAME} zest-interface ${Vulkan_LIBRARY})
    endif(WIN32)
    
    set_target_properties(${EXAMPLE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_target_properties(${EXAMPLE_NAME} PROPERTIES XCODE_GENERATE_SCHEME TRUE XCODE_SCHEME_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction(buildExample)

set(EXAMPLES
	zest-instance-layers
	zest-remove-images
	zest-lines
	zest-slang
	zest-zloc
)


buildExamples()