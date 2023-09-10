function(buildExample EXAMPLE_NAME)
	SET(EXAMPLE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_NAME})

	file(GLOB SOURCE *.cpp *.h)
	if(EXISTS ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.h)
		SET(MAIN_HEADER ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.h)
	ENDIF()

	SET(MAIN_CPP ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.cpp)

	set(SHADER_DIR "${EXAMPLE_NAME}/shaders")

	# Collect shader files
	file(GLOB SHADERS "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag" "${SHADER_DIR}/*.comp")

	# Organize shaders into a source group
	source_group("Shaders" FILES ${SHADERS})

	if(WIN32)
		add_executable(${EXAMPLE_NAME} WIN32 ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS})
		target_link_libraries(${EXAMPLE_NAME} zest-lib ${Vulkan_LIBRARY} ${glfw_LIBRARY} ${WINLIBS})
	else(WIN32)
		add_executable(${EXAMPLE_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS})
		target_link_libraries(${EXAMPLE_NAME} zest-lib ${Vulkan_LIBRARY} ${glfw_LIBRARY})
	endif(WIN32)

	set_target_properties(${EXAMPLE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction(buildExample)

set(EXAMPLES
	zest-compute-example
	zest-imgui-template
	zest-msdf-font-maker
)

buildExamples()
