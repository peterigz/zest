# TimelineFX hot reload harness.
#
# Two targets, not one, so this example cannot go through buildExample():
#   zest-tfx-reload-engine   engine.dll. TimelineFX is compiled INTO this and
#                            nowhere else - two copies would each get their own
#                            tfxCurrentContext and the test would be meaningless.
#   zest-timelinefx-reload   platform.exe. Owns the pool and the reload sequence,
#                            never links timelinefx.cpp.
#
# TIMELINEFX_RELOAD deliberately points at a local TimelineFX working copy rather
# than submodules/timelinefxlib: the reload API (tfx_GetContext / tfx_SetContext /
# tfx_SuspendTimelineFX / tfx_ResumeTimelineFX) lives on the unpushed dll-context
# branch there. The shared TIMELINEFX variable is untouched, so every other
# TimelineFX example keeps building against the submodule copy on master.
set(TIMELINEFX_RELOAD "E:/Projects/C++/TimelineFX/TFXEditor/TimelineFXLib/" CACHE PATH
	"TimelineFX working copy carrying the hot reload API (branch dll-context)")

if(NOT WIN32)
	message(STATUS "zest-timelinefx-reload: skipped, the platform layer is LoadLibrary/FreeLibrary only for now")
	return()
endif()

if(NOT EXISTS "${TIMELINEFX_RELOAD}/timelinefx.cpp")
	message(WARNING "zest-timelinefx-reload: skipped, no timelinefx.cpp at ${TIMELINEFX_RELOAD}")
	return()
endif()

set(RELOAD_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# --- engine.dll ---------------------------------------------------------------
add_library(zest-tfx-reload-engine SHARED
	${RELOAD_DIR}/tfx_reload_engine.cpp
	${RELOAD_DIR}/tfx_reload_engine.h
	${TIMELINEFX_RELOAD}/timelinefx.cpp
	${TIMELINEFX_RELOAD}/timelinefx.h
	${TIMELINEFX_RELOAD}/timelinefx_internal.h
)
source_group("TimelineFX" FILES
	${TIMELINEFX_RELOAD}/timelinefx.cpp
	${TIMELINEFX_RELOAD}/timelinefx.h
	${TIMELINEFX_RELOAD}/timelinefx_internal.h
)
target_include_directories(zest-tfx-reload-engine PRIVATE ${TIMELINEFX_RELOAD} ${RELOAD_DIR})
# Tracy keeps its own globals and thread registrations inside the reloading
# module. Off for the baseline; turn it on separately once the baseline is green.
target_compile_definitions(zest-tfx-reload-engine PRIVATE TFX_RELOAD_ENGINE_EXPORTS)
set_target_properties(zest-tfx-reload-engine PROPERTIES CXX_STANDARD 17)
if(NOT MSVC)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i[3-6]86")
		target_compile_options(zest-tfx-reload-engine PRIVATE -mavx2 -mfma)
	endif()
	target_compile_options(zest-tfx-reload-engine PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fpermissive> -Wno-ignored-attributes)
endif()

# --- platform.exe -------------------------------------------------------------
add_executable(zest-timelinefx-reload
	${RELOAD_DIR}/zest-timelinefx-reload.cpp
	${RELOAD_DIR}/tfx_reload_platform.cpp
	${RELOAD_DIR}/tfx_reload_platform.h
	${RELOAD_DIR}/tfx_reload_engine.h
	${RELOAD_DIR}/README.md
)
target_include_directories(zest-timelinefx-reload PRIVATE ${TIMELINEFX_RELOAD} ${RELOAD_DIR})
add_dependencies(zest-timelinefx-reload zest-tfx-reload-engine)
set_target_properties(zest-timelinefx-reload PROPERTIES CXX_STANDARD 17)

# platform.exe locates engine.dll next to its own module path, so both targets
# have to land in the same directory. They do by default - neither sets
# RUNTIME_OUTPUT_DIRECTORY, so both go to this directory plus the config name.

# The effects library path is relative to the repository root.
set_target_properties(zest-timelinefx-reload PROPERTIES
	VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# --- platform.exe, rendered (stage 2) -----------------------------------------
# Same platform layer and same engine.dll. zest, SDL2 and every Vulkan object are
# linked here and nowhere else, so a broken reload cannot be blamed on zest.
if(SDL2_FOUND)
	add_executable(zest-timelinefx-reload-render
		${RELOAD_DIR}/zest-timelinefx-reload-render.cpp
		${RELOAD_DIR}/tfx_reload_platform.cpp
		${RELOAD_DIR}/tfx_reload_platform.h
		${RELOAD_DIR}/tfx_reload_engine.h
	)
	target_include_directories(zest-timelinefx-reload-render PRIVATE
		${TIMELINEFX_RELOAD}
		${RELOAD_DIR}
		${CMAKE_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/implementations
		${CMAKE_SOURCE_DIR}/examples/libs
		${SDL2_INCLUDE_DIRS}
	)
	target_link_libraries(zest-timelinefx-reload-render zest-interface ${SDL2_LIBRARIES} ${WINLIBS})
	add_dependencies(zest-timelinefx-reload-render zest-tfx-reload-engine)
	set_target_properties(zest-timelinefx-reload-render PROPERTIES
		CXX_STANDARD 17
		VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	# The rendered harness locates engine.dll next to its own module path, so it has
	# to sit in the same directory as the headless one and the engine.
else()
	message(STATUS "zest-timelinefx-reload-render: skipped, SDL2 not found")
endif()

# --- Tracy (off by default) ---------------------------------------------------
# The baseline runs with Tracy disabled on purpose: it keeps its own globals and
# thread registrations inside the reloading module, so leaving it on would confuse
# the first results. Turn it on to re-test that separately:
#   cmake -S . -B <build> -DZEST_TFX_RELOAD_ENABLE_TRACY=ON
# tfxTRACY is what makes timelinefx_internal.h map tfxPROFILE onto a Tracy zone;
# TRACY_ENABLE arms Tracy itself and arrives from the TracyClient target.
option(ZEST_TFX_RELOAD_ENABLE_TRACY "Instrument the reload harness engine.dll with Tracy" OFF)
set(ZEST_TFX_RELOAD_TRACY_DIR "E:/Projects/C++/TimelineFX/TFXEditor/submodules/tracy" CACHE PATH
	"Path to a Tracy checkout for the reload harness")

if(ZEST_TFX_RELOAD_ENABLE_TRACY)
	if(NOT EXISTS "${ZEST_TFX_RELOAD_TRACY_DIR}/CMakeLists.txt")
		message(FATAL_ERROR "ZEST_TFX_RELOAD_ENABLE_TRACY is ON but no Tracy checkout at ${ZEST_TFX_RELOAD_TRACY_DIR}")
	endif()
	set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)
	add_subdirectory("${ZEST_TFX_RELOAD_TRACY_DIR}" "${CMAKE_BINARY_DIR}/tracy" EXCLUDE_FROM_ALL)
	target_link_libraries(zest-tfx-reload-engine Tracy::TracyClient)
	target_compile_definitions(zest-tfx-reload-engine PRIVATE tfxTRACY)
	message(STATUS "zest-timelinefx-reload: Tracy enabled in engine.dll (${ZEST_TFX_RELOAD_TRACY_DIR})")
endif()
