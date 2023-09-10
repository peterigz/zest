function(buildExample EXAMPLE_NAME)
	SET(EXAMPLE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_NAME})
	
	include_directories(${EXAMPLE_FOLDER})
	include_directories(${CMAKE_SOURCE_DIR}/submodules/)
	include_directories(${CMAKE_SOURCE_DIR}/submodules/imgui)
	include_directories(${CMAKE_SOURCE_DIR}/submodules/imgui/backends)
	include_directories(${CMAKE_SOURCE_DIR}/submodules/imgui_test_engine/imgui_test_engine)
	include_directories(${CMAKE_SOURCE_DIR}/implementations) 
	
	SET(IMGUI ${CMAKE_SOURCE_DIR}/submodules/imgui/)
	SET(IMPLEMENTATIONS ${CMAKE_SOURCE_DIR}/implementations/)

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
	include_directories(${GLFW3_INCLUDE_DIR})

	# Collect shader files
	file(GLOB SHADERS "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag" "${SHADER_DIR}/*.comp")
	file(GLOB IMGUI_SOURCE
		${IMGUI}*.cpp 
		${IMGUI}backends/imgui_impl_glfw.cpp
	)
	
	file(GLOB IMPLEMENTATIONS_SOURCE
		${IMPLEMENTATIONS}impl_glfw.cpp
		${IMPLEMENTATIONS}impl_imgui.cpp
	)

	# Organize shaders into a source group
	source_group("Shaders" FILES ${SHADERS})
	source_group("ImGui" FILES ${IMGUI_SOURCE})
	source_group("Implementations" FILES ${IMPLEMENTATIONS_SOURCE})
	
	add_compile_definitions(IMGUI_USER_CONFIG="zest_imconfig.h")

	if(WIN32)
		add_executable(${EXAMPLE_NAME} WIN32 ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS} ${IMGUI_SOURCE} ${IMPLEMENTATIONS_SOURCE})
		target_link_libraries(${EXAMPLE_NAME} zest-lib ${Vulkan_LIBRARY} ${GLFW3_LIBRARY} ${WINLIBS})
	else(WIN32)
		add_executable(${EXAMPLE_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS} ${IMGUI_SOURCE} ${IMPLEMENTATIONS_SOURCE})
		target_link_libraries(${EXAMPLE_NAME} zest-lib ${Vulkan_LIBRARY} ${GLFW3_LIBRARY})
	endif(WIN32)

	set_target_properties(${EXAMPLE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction(buildExample)

set(EXAMPLES
	zest-compute-example
	zest-imgui-template
	#zest-msdf-font-maker
)

buildExamples()
