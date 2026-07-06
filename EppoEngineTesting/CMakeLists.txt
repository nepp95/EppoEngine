project(EppoEngineTesting)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "Source/*.cpp")

add_executable(EppoEngineTesting ${SOURCES})

target_include_directories(EppoEngineTesting PRIVATE "Source")
target_link_libraries(EppoEngineTesting PRIVATE EppoEngine UnitTest++)

if (MSVC)
    target_compile_options(EppoEngineTesting PRIVATE "/W3")
    target_link_options(EppoEngineTesting PRIVATE "/ignore:4099")
endif ()

# EppoEngine pulls in DLL deps (spdlog, imgui, ...) — copy them next to the exe
add_custom_command(TARGET EppoEngineTesting POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:EppoEngineTesting> $<TARGET_RUNTIME_DLLS:EppoEngineTesting>
    COMMAND_EXPAND_LISTS)

# Register one CTest entry per UnitTest++ SUITE. The runner takes a suite name as
# its first argument and runs only that suite (see Source/main.cpp), giving
# per-suite pass/fail granularity. Labels group suites by cost/dependency so they
# can be filtered, e.g. `ctest -L unit`:
#   unit      — pure logic, no engine subsystems spun up
#   scene     — headless ECS + serialization
#   scripting — needs the hosted .NET runtime + managed assembly
function(eppo_add_suite suite)
    cmake_parse_arguments(ARG "" "LABEL" "" ${ARGN})
    add_test(NAME ${suite} COMMAND EppoEngineTesting ${suite})
    if (ARG_LABEL)
        set_tests_properties(${suite} PROPERTIES LABELS "${ARG_LABEL}")
    endif ()
endfunction()

eppo_add_suite(Core      LABEL unit)
eppo_add_suite(Input     LABEL unit)
eppo_add_suite(Scene     LABEL scene)
eppo_add_suite(Scripting LABEL scripting)
