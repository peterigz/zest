function(buildExample EXAMPLE_NAME)
	SET(EXAMPLE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_NAME})
	
	include_directories(${CMAKE_SOURCE_DIR}/submodules/)
	include_directories(${CMAKE_SOURCE_DIR}/submodules/imgui)
	include_directories(${CMAKE_SOURCE_DIR}/submodules/imgui/backends)
	include_directories(${CMAKE_SOURCE_DIR}/submodules/imgui_test_engine/imgui_test_engine)
	include_directories(${CMAKE_SOURCE_DIR}/implementations) 
	
	SET(IMGUI ${CMAKE_SOURCE_DIR}/submodules/imgui/)
	SET(IMGUI_FILE_DIALOG ${CMAKE_SOURCE_DIR}/submodules/imgui_file_dialog/)
	SET(IMPLEMENTATIONS ${CMAKE_SOURCE_DIR}/implementations/)
	SET(MSDFATLAS ${CMAKE_SOURCE_DIR}/submodules/msdf-atlas-gen/)

	file(GLOB SOURCE 
		*.cpp 
		*.h 
		${EXAMPLE_FOLDER}/zest_imconfig.h
	)
	if(EXISTS ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.h)
		SET(MAIN_HEADER ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.h)
	ENDIF()

	SET(MAIN_CPP ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.cpp)

	set(SHADER_DIR "${EXAMPLE_NAME}/shaders")
	include_directories(${SDL2_INCLUDE_DIRS})

	# Collect shader files
	file(GLOB SHADERS "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag" "${SHADER_DIR}/*.comp")
	file(GLOB IMGUI_SOURCE
		${IMGUI}*.cpp 
		${IMGUI}backends/imgui_impl_sdl2.cpp
		${IMGUI}misc/freetype/imgui_freetype.cpp
	)
	
	file(GLOB IMPLEMENTATIONS_SOURCE
		${IMPLEMENTATIONS}impl_imgui.cpp
	)

	# Organize shaders into a source group
	source_group("Shaders" FILES ${SHADERS})
	source_group("ImGui" FILES ${IMGUI_SOURCE})
	source_group("Implementations" FILES ${IMPLEMENTATIONS_SOURCE})
	
	add_compile_definitions(IMGUI_USER_CONFIG="zest_imconfig.h")

	if(WIN32)
		add_executable(${EXAMPLE_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS} ${IMGUI_SOURCE} ${IMPLEMENTATIONS_SOURCE})
		target_link_libraries(${EXAMPLE_NAME} zest-interface ${SDL2_LIBRARIES} ${WINLIBS})
	else(WIN32)
		add_executable(${EXAMPLE_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS} ${IMGUI_SOURCE} ${IMPLEMENTATIONS_SOURCE})
		target_link_libraries(${EXAMPLE_NAME} zest-interface ${SDL2_LIBRARIES})
	endif(WIN32)

	target_link_libraries(${EXAMPLE_NAME} freetype)
	target_include_directories(${EXAMPLE_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/submodules/freetype/include) 

	target_include_directories(${EXAMPLE_NAME} PUBLIC ${EXAMPLE_FOLDER})

	set_target_properties(${EXAMPLE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	set_target_properties(${EXAMPLE_NAME} PROPERTIES XCODE_GENERATE_SCHEME TRUE XCODE_SCHEME_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	
	add_custom_command(TARGET ${EXAMPLE_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:${EXAMPLE_NAME}> $<TARGET_FILE_DIR:${EXAMPLE_NAME}>
		COMMAND_EXPAND_LISTS
		)
endfunction(buildExample)

set(EXAMPLES
	zest-sdl2-imgui-template
)

buildExamples()



