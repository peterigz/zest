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
	file(GLOB SHADERS "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag" "${SHADER_DIR}/*.comp")
	file(GLOB IMGUI_SOURCE
		${IMGUI}*.cpp 
		${IMGUI}backends/imgui_impl_glfw.cpp
		${IMGUI}misc/freetype/imgui_freetype.cpp
	)
	
	file(GLOB IMPLEMENTATIONS_SOURCE
		${IMPLEMENTATIONS}impl_glfw.cpp
		${IMPLEMENTATIONS}impl_imgui_glfw.cpp
	)

	if(${EXAMPLE_NAME} STREQUAL "zest-msdf-font-maker")
		file(GLOB IMGUI_FILE_DIALOG_SOURCE
			${IMGUI_FILE_DIALOG}ImGuiFileDialog.cpp
		)
	else()
		SET(IMGUI_FILE_DIALOG_SOURCE "")
	endif()

	if(${EXAMPLE_NAME} STREQUAL "zest-vaders" OR ${EXAMPLE_NAME} STREQUAL "zest-timelinefx" OR ${EXAMPLE_NAME} STREQUAL "zest-timelinefx-instanced-effects" OR ${EXAMPLE_NAME} STREQUAL "zest-timelinefx-prerecorded-effects")
		file(GLOB TIMELINEFX_SOURCE 
			${TIMELINEFX}timelinefx.h
			${TIMELINEFX}timelinefx.cpp
		)
	else()
		SET(TIMELINEFX_SOURCE "")
	endif()

	# Organize shaders into a source group
	source_group("Shaders" FILES ${SHADERS})
	source_group("ImGui" FILES ${IMGUI_SOURCE})
	source_group("Implementations" FILES ${IMPLEMENTATIONS_SOURCE})
	
	add_compile_definitions(IMGUI_USER_CONFIG="zest_imconfig.h")

	if(WIN32)
		add_executable(${EXAMPLE_NAME} WIN32 ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS} ${IMGUI_SOURCE} ${IMPLEMENTATIONS_SOURCE} ${TIMELINEFX_SOURCE} ${IMGUI_FILE_DIALOG_SOURCE})
		target_link_libraries(${EXAMPLE_NAME} zest-lib ${Vulkan_LIBRARY} ${GLFW3_LIBRARY} ${WINLIBS})
	else(WIN32)
		add_executable(${EXAMPLE_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS} ${IMGUI_SOURCE} ${IMPLEMENTATIONS_SOURCE} ${TIMELINEFX_SOURCE} ${IMGUI_FILE_DIALOG_SOURCE})
		target_link_libraries(${EXAMPLE_NAME} zest-lib ${Vulkan_LIBRARY} ${GLFW3_LIBRARY})
	endif(WIN32)

	if(${EXAMPLE_NAME} STREQUAL "zest-msdf-font-maker")
		target_link_libraries(${EXAMPLE_NAME} ${CMAKE_SOURCE_DIR}/examples/libs/freetype/win64/freetype.lib ${CMAKE_SOURCE_DIR}/examples/libs/libpng16/libpng16.lib ${MSDF_ATLAS_LIB} ${MSDF_GEN_CORE} ${MSDF_GEN_EXT})
		target_include_directories(${EXAMPLE_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/submodules/msdf_atlas_gen/) 
		target_include_directories(${EXAMPLE_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/submodules/msdf_atlas_gen/msdfgen) 
	endif()

	target_link_libraries(${EXAMPLE_NAME} ${CMAKE_SOURCE_DIR}/examples/libs/freetype/win64/freetype.lib)
	target_include_directories(${EXAMPLE_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/examples/libs/freetype/include) 

	if(${EXAMPLE_NAME} STREQUAL "zest-vaders" OR ${EXAMPLE_NAME} STREQUAL "zest-timelinefx" OR ${EXAMPLE_NAME} STREQUAL "zest-timelinefx-instanced-effects" OR ${EXAMPLE_NAME} STREQUAL "zest-timelinefx-prerecorded-effects-effects")
		target_include_directories(${EXAMPLE_NAME} PUBLIC ${TIMELINEFX}) 
	endif()

	target_include_directories(${EXAMPLE_NAME} PUBLIC ${EXAMPLE_FOLDER})

	set_target_properties(${EXAMPLE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	set_target_properties(${EXAMPLE_NAME} PROPERTIES XCODE_GENERATE_SCHEME TRUE XCODE_SCHEME_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction(buildExample)

set(EXAMPLES
	zest-compute-example
	zest-imgui-template
	zest-vaders
	zest-timelinefx
	zest-timelinefx-instanced-effects
	zest-timelinefx-prerecorded-effects
)

buildExamples()

if(${ADD_MSDF_FONT_BUILDER})
	set(EXTRA_EXAMPLES
		zest-msdf-font-maker
	)
	buildExtraExamples()
endif()


