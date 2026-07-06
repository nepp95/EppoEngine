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

# Graphical suites (App/Scenario) boot the real Application, which loads shaders
# and fonts from Resources/ next to the exe (FS::GetResourcesDirectory() resolves
# against the working directory = exe dir). Reuse the editor's Resources tree,
# copied incrementally via a stamp file so it only re-copies when files change.
set(EDITOR_RESOURCES_DIR "${CMAKE_SOURCE_DIR}/EppoEditor/Resources")
file(GLOB_RECURSE TEST_RESOURCE_FILES CONFIGURE_DEPENDS "${EDITOR_RESOURCES_DIR}/*")
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/_TestResourcesSync"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:EppoEngineTesting>/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${EDITOR_RESOURCES_DIR}" "$<TARGET_FILE_DIR:EppoEngineTesting>/Resources"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/_TestResourcesSync"
    DEPENDS ${TEST_RESOURCE_FILES}
    COMMENT "Copying Resources (tests)"
    VERBATIM)
add_custom_target(EppoTestResources ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/_TestResourcesSync")
add_dependencies(EppoEngineTesting EppoTestResources)

# Committed scenario data (e.g. the testing scene) copied next to the exe so
# scenarios can load it via FS::GetRootDirectory() (= working dir = exe dir).
add_custom_command(TARGET EppoEngineTesting POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/TestData/Scenes" "$<TARGET_FILE_DIR:EppoEngineTesting>/TestData/Scenes"
    COMMENT "Copying scenario TestData")

# Scripting suite provisioning (Windows only, mirrors EppoEditor): the hosted
# .NET runtime needs runtimeconfig.json + the managed core assembly (+ deps)
# next to the test exe. We rebuild the managed core here rather than reaching
# into the editor's output so the test target stands alone.
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(DOTNET_EXE "$ENV{DOTNET_ROOT}/dotnet.exe")

    file(GLOB_RECURSE MANAGED_CORE_SOURCES CONFIGURE_DEPENDS "${EppoScriptCore_MANAGED_SOURCE_DIR}/Source/*.cs")

    # The editor builds this same csproj; we can't redirect its intermediate dir
    # (the port's csproj uses default globbing, which then sweeps stale in-source
    # obj/*.cs into the compile), so instead we output to our own dir but serialize
    # after the editor's managed build to avoid a parallel-build race on the shared
    # obj/. EppoScriptCoreManaged is defined in EppoEditor (added before us).
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.Managed.dll"
        COMMAND ${DOTNET_EXE} build "${EppoScriptCore_MANAGED_SOURCE_DIR}/EppoScriptCore.Managed.csproj" -c $<CONFIG> -o ${CMAKE_CURRENT_BINARY_DIR} -v m --nologo
        COMMENT "Building EppoScriptCore.Managed (tests)"
        DEPENDS ${MANAGED_CORE_SOURCES} "${EppoScriptCore_MANAGED_SOURCE_DIR}/EppoScriptCore.Managed.csproj"
        VERBATIM)
    add_custom_target(EppoScriptCoreManagedTest ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.Managed.dll")
    if (TARGET EppoScriptCoreManaged)
        add_dependencies(EppoScriptCoreManagedTest EppoScriptCoreManaged)
    endif ()
    add_dependencies(EppoEngineTesting EppoScriptCoreManagedTest)

    # A tiny user script assembly (one concrete ScriptBehaviour) so the Scripting
    # suite can exercise user-assembly loading + class/field discovery. References
    # the core managed DLL by path (passed as an MSBuild property).
    set(TEST_SCRIPTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/TestData/Scripts")
    file(GLOB_RECURSE TEST_SCRIPT_SOURCES CONFIGURE_DEPENDS "${TEST_SCRIPTS_DIR}/Source/*.cs")

    # Intermediates go under the build tree (not the source tree next to the
    # csproj) so nothing generated leaks into git.
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/TestScripts/EppoTesting.Scripts.dll"
        COMMAND ${DOTNET_EXE} build "${TEST_SCRIPTS_DIR}/HarnessScripts.csproj" -c $<CONFIG>
            -o "${CMAKE_CURRENT_BINARY_DIR}/TestScripts"
            -p:BaseIntermediateOutputPath=${CMAKE_CURRENT_BINARY_DIR}/TestScripts/obj/
            -p:CoreManagedDll=${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.Managed.dll
            -v m --nologo
        COMMENT "Building test user script assembly"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.Managed.dll" ${TEST_SCRIPT_SOURCES} "${TEST_SCRIPTS_DIR}/HarnessScripts.csproj"
        VERBATIM)
    add_custom_target(EppoTestScripts ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/TestScripts/EppoTesting.Scripts.dll")
    add_dependencies(EppoTestScripts EppoScriptCoreManagedTest)
    add_dependencies(EppoEngineTesting EppoTestScripts)

    add_custom_command(TARGET EppoEngineTesting POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/EppoEditor/runtimeconfig.json" $<TARGET_FILE_DIR:EppoEngineTesting>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.Managed.dll" $<TARGET_FILE_DIR:EppoEngineTesting>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.Managed.pdb" $<TARGET_FILE_DIR:EppoEngineTesting>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/EppoScriptCore.Managed.deps.json" $<TARGET_FILE_DIR:EppoEngineTesting>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/TestScripts/EppoTesting.Scripts.dll" $<TARGET_FILE_DIR:EppoEngineTesting>
        COMMENT "Copying EppoScriptCore runtime + test scripts")
endif ()

# Register one CTest entry per UnitTest++ SUITE. The runner takes a suite name as
# its first argument and runs only that suite (see Source/main.cpp), giving
# per-suite pass/fail granularity. Labels group suites by cost/dependency so they
# can be filtered, e.g. `ctest -L unit`:
#   unit      — pure logic, no engine subsystems spun up
#   scene     — headless ECS + serialization
#   scripting — needs the hosted .NET runtime + managed assembly
function(eppo_add_suite suite)
    cmake_parse_arguments(ARG "" "LABEL" "" ${ARGN})
    # Run from the exe's own directory: the hosted .NET runtime resolves the
    # managed assemblies relative to the working directory, and the data files
    # (runtimeconfig.json, script DLLs) are copied next to the exe.
    add_test(NAME ${suite} COMMAND EppoEngineTesting ${suite}
        WORKING_DIRECTORY $<TARGET_FILE_DIR:EppoEngineTesting>)
    if (ARG_LABEL)
        set_tests_properties(${suite} PROPERTIES LABELS "${ARG_LABEL}")
    endif ()
endfunction()

eppo_add_suite(Core      LABEL unit)
eppo_add_suite(Input     LABEL unit)
eppo_add_suite(Scene     LABEL scene)
eppo_add_suite(Project   LABEL scene)
eppo_add_suite(Scripting LABEL scripting)
eppo_add_suite(App       LABEL graphical)
eppo_add_suite(Scenario  LABEL graphical)
