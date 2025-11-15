function(buildExample EXAMPLE_NAME)
	SET(EXAMPLE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_NAME})
	
	include_directories(${CMAKE_SOURCE_DIR}/submodules/)
	include_directories(${CMAKE_SOURCE_DIR}/submodules/imgui)
	include_directories(${CMAKE_SOURCE_DIR}/submodules/imgui/backends)
	include_directories(${CMAKE_SOURCE_DIR}/submodules/imgui_test_engine/)
	include_directories(${CMAKE_SOURCE_DIR}/submodules/imgui_test_engine/imgui_test_engine)
	include_directories(${CMAKE_SOURCE_DIR}/submodules/imgui_test_engine/imgui_test_suite)
	include_directories(${CMAKE_SOURCE_DIR}/implementations) 
	include_directories(${CMAKE_SOURCE_DIR}/examples/libs)
	
	SET(IMGUI ${CMAKE_SOURCE_DIR}/submodules/imgui/)
	SET(IMGUI_TEST_ENGINE ${CMAKE_SOURCE_DIR}/submodules/imgui_test_engine/imgui_test_engine/)
	SET(IMGUI_TEST_SUITE ${CMAKE_SOURCE_DIR}/submodules/imgui_test_engine/imgui_test_suite/)
	SET(IMGUI_FILE_DIALOG ${CMAKE_SOURCE_DIR}/submodules/imgui_file_dialog/)
	SET(IMPLEMENTATIONS ${CMAKE_SOURCE_DIR}/implementations/)
	SET(TIMELINEFX ${CMAKE_SOURCE_DIR}/submodules/timelinefxlib/)
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
	include_directories(${GLFW3_INCLUDE_DIR})

	# Collect shader files
	file(GLOB SHADERS "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag" "${SHADER_DIR}/*.comp" "${SHADER_DIR}/*.slang")
	file(GLOB IMGUI_SOURCE
		${IMGUI}*.cpp 
		${IMGUI}backends/imgui_impl_glfw.cpp
		${IMGUI}misc/freetype/imgui_freetype.cpp
	)

	if(${EXAMPLE_NAME} STREQUAL "zest-imgui-test-suite")
		file(GLOB IMGUI_TEST_ENGINE_SOURCE
			${IMGUI_TEST_ENGINE}*.cpp 
		)
		file(GLOB IMGUI_TEST_SUITE_SOURCE
			${IMGUI_TEST_SUITE}*.cpp 
		)
		source_group("ImGui" FILES ${IMGUI_SOURCE})
	else()
		SET(IMGUI_TEST_ENGINE_SOURCE "")
		SET(IMGUI_TEST_SUITE_SOURCE "")
	endif()

	if(${EXAMPLE_NAME} STREQUAL "zest-implot")
		file(GLOB IMPLOT_SOURCE
			${EXAMPLE_FOLDER}/implot.cpp
			${EXAMPLE_FOLDER}/implot_internal.cpp
			${EXAMPLE_FOLDER}/implot_items.cpp
			${EXAMPLE_FOLDER}/implot_demo.cpp
		)
		source_group("ImGui" FILES ${IMGUI_SOURCE})
	else()
		SET(IMPLOT_SOURCE "")
	endif()

	if(${EXAMPLE_NAME} STREQUAL "zest-compute-example")
		file(GLOB IMPLEMENTATIONS_SOURCE
			${IMPLEMENTATIONS}impl_imgui.cpp
			${IMPLEMENTATIONS}impl_slang.hpp
		)
		source_group("ImGui" FILES ${IMGUI_SOURCE})
	elseif(${EXAMPLE_NAME} STREQUAL "zest-vaders" OR 
		${EXAMPLE_NAME} STREQUAL "zest-timelinefx" OR 
		${EXAMPLE_NAME} STREQUAL "zest-timelinefx-instanced-effects" OR 
		${EXAMPLE_NAME} STREQUAL "zest-timelinefx-prerecorded-effects" OR 
		${EXAMPLE_NAME} STREQUAL "zest-timelinefx-tests")
		file(GLOB TIMELINEFX_SOURCE 
			${TIMELINEFX}timelinefx.h
			${TIMELINEFX}timelinefx.cpp
		)
		file(GLOB IMPLEMENTATIONS_SOURCE
			${IMPLEMENTATIONS}impl_imgui.cpp
			${IMPLEMENTATIONS}impl_timelinefx.c
		)
		source_group("ImGui" FILES ${IMGUI_SOURCE})
	elseif(${EXAMPLE_NAME} STREQUAL "zest-fonts" OR
		${EXAMPLE_NAME} STREQUAL "zest-imgui-template" OR
		${EXAMPLE_NAME} STREQUAL "zest-pbr")
		SET(TIMELINEFX_SOURCE "")
		file(GLOB IMPLEMENTATIONS_SOURCE
			${IMPLEMENTATIONS}impl_imgui.cpp
		)
		source_group("ImGui" FILES ${IMGUI_SOURCE})
	else()
		SET(IMPLEMENTATIONS_SOURCE "")
		SET(IMGUI_SOURCE "")
	endif()

	# Organize shaders into a source group
	source_group("Shaders" FILES ${SHADERS})

	source_group("Implementations" FILES ${IMPLEMENTATIONS_SOURCE})
	if(${EXAMPLE_NAME} STREQUAL "zest-imgui-test-suite")
		source_group("ImGuiTestEngine" FILES ${IMGUI_TEST_ENGINE_SOURCE})
		source_group("ImGuiTestSuite" FILES ${IMGUI_TEST_SUITE_SOURCE})
	endif()
	
	add_compile_definitions(IMGUI_USER_CONFIG="zest_imconfig.h")

	if(WIN32)
		add_executable(${EXAMPLE_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS} ${IMGUI_SOURCE} ${IMPLEMENTATIONS_SOURCE} ${IMGUI_TEST_ENGINE_SOURCE} ${IMGUI_TEST_SUITE_SOURCE} ${TIMELINEFX_SOURCE} ${IMGUI_FILE_DIALOG_SOURCE} ${IMPLOT_SOURCE})
		target_link_libraries(${EXAMPLE_NAME} zest-interface ${GLFW3_LIBRARY} ${WINLIBS})
	else(WIN32)
		add_executable(${EXAMPLE_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS} ${IMGUI_SOURCE} ${IMPLEMENTATIONS_SOURCE} ${TIMELINEFX_SOURCE} ${IMGUI_TEST_ENGINE_SOURCE} ${IMGUI_TEST_SUITE_SOURCE} ${IMGUI_FILE_DIALOG_SOURCE})
		target_link_libraries(${EXAMPLE_NAME} zest-interface ${GLFW3_LIBRARY})
	endif(WIN32)

	target_link_libraries(${EXAMPLE_NAME} freetype)
	target_include_directories(${EXAMPLE_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/submodules/freetype/include) 

	if(${EXAMPLE_NAME} STREQUAL "zest-vaders" OR ${EXAMPLE_NAME} STREQUAL "zest-timelinefx" OR ${EXAMPLE_NAME} STREQUAL "zest-timelinefx-instanced-effects" OR ${EXAMPLE_NAME} STREQUAL "zest-timelinefx-prerecorded-effects-effects" OR ${EXAMPLE_NAME} STREQUAL "zest-timelinefx-tests")
		target_include_directories(${EXAMPLE_NAME} PUBLIC ${TIMELINEFX}) 
	endif()

	target_include_directories(${EXAMPLE_NAME} PUBLIC ${EXAMPLE_FOLDER})

	set_target_properties(${EXAMPLE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	set_target_properties(${EXAMPLE_NAME} PROPERTIES XCODE_GENERATE_SCHEME TRUE XCODE_SCHEME_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction(buildExample)

set(EXAMPLES
	zest-minimal-template
	zest-compute-example
	zest-imgui-template
	zest-render-graph-tests
	zest-vaders
	zest-timelinefx
	zest-timelinefx-instanced-effects
	zest-timelinefx-prerecorded-effects
	zest-ribbons
	zest-implot
	zest-pbr
	zest-fonts
	zest-render-targets
)

buildExamples()


