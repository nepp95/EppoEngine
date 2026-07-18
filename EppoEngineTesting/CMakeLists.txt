project(EppoEngineTesting)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "Source/*.cpp")
set(INCLUDE_DIRS
    "Source"
)

add_executable(EppoEngineTesting ${SOURCES})
add_dependencies(EppoEngineTesting EppoScriptCore)
target_include_directories(EppoEngineTesting PRIVATE ${INCLUDE_DIRS})
target_link_libraries(EppoEngineTesting PRIVATE EppoEngine UnitTest++)

# EppoScriptCore.dll + runtimeconfig.json beside the test exe (same as the editor).
CopyBuildScripts(EppoEngineTesting)

# Copy linked runtime DLLs (nvrhi, glfw, Tracy, ...) next to the exe so it can
# launch. Without this the process fails to load (e.g. missing TracyClient.dll).
# Windows-only: TARGET_RUNTIME_DLLS is empty on ELF platforms (the loader finds
# .so deps via RPATH), and an empty `cmake -E copy` would error on Linux/CI.
if (WIN32)
    add_custom_command(TARGET EppoEngineTesting POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:EppoEngineTesting> $<TARGET_RUNTIME_DLLS:EppoEngineTesting>
        COMMAND_EXPAND_LISTS
    )

    add_custom_command(TARGET EppoEngineTesting POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$ENV{VULKAN_SDK}/Bin/dxcompiler.dll" $<TARGET_FILE_DIR:EppoEngineTesting>
    )
endif ()

# --- Test user-script assembly ---------------------------------------------
# Build the tiny user harness (EppoTesting.Scripts.dll) the Scripting suite loads,
# then deploy it (and a fresh managed core) beside the exe. EppoEngineTesting
# depends on this target, so a targeted `--target EppoEngineTesting` build (as CI
# runs) produces the assembly — an unattached ALL target would be skipped by CI.
# The deploy re-runs whenever the assemblies change, so managed-only edits don't
# leave stale DLLs (CopyBuildScripts' POST_BUILD only fires on a C++ relink).
# EppoScriptCore.dll is built at the root binary dir by the EppoScriptCore target.
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/harness/EppoTesting.Scripts.dll"
    COMMAND ${DOTNET_EXE} build "${CMAKE_CURRENT_SOURCE_DIR}/TestData/Scripts/HarnessScripts.csproj"
            -c $<CONFIG> -o "${CMAKE_CURRENT_BINARY_DIR}/harness"
            -p:CoreManagedDll="${CMAKE_BINARY_DIR}/EppoScriptCore.dll"
            -v m --nologo
    COMMENT "Building test harness user assembly"
    DEPENDS "${CMAKE_BINARY_DIR}/EppoScriptCore.dll"
            "${CMAKE_CURRENT_SOURCE_DIR}/TestData/Scripts/Source/HarnessScript.cs"
            "${CMAKE_CURRENT_SOURCE_DIR}/TestData/Scripts/HarnessScripts.csproj"
    VERBATIM
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/_HarnessDeploy"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/harness/EppoTesting.Scripts.dll"
            "${CMAKE_BINARY_DIR}/EppoScriptCore.dll"
    # The exe depends on this target, so it deploys before the exe is linked. On
    # multi-config generators (VS) the per-config output dir may not exist yet, so
    # create it before copying into it.
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:EppoEngineTesting>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/harness/EppoTesting.Scripts.dll"
            "${CMAKE_BINARY_DIR}/EppoScriptCore.dll"
            "${CMAKE_BINARY_DIR}/EppoScriptCore.pdb"
            "$<TARGET_FILE_DIR:EppoEngineTesting>"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/_HarnessDeploy"
    COMMENT "Deploying test harness + managed core beside the test exe"
    VERBATIM
)
add_custom_target(EppoTestingHarness DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/_HarnessDeploy")
add_dependencies(EppoTestingHarness EppoScriptCore)
add_dependencies(EppoEngineTesting EppoTestingHarness)

# Copy the editor's Resources/ + TestData/Scenes/ next to the exe (stamp-gated)
# so the graphical suites can boot the renderer and load harness.epscene.
set(EDITOR_RESOURCES_DIR "${CMAKE_SOURCE_DIR}/EppoEditor/Resources")
file(GLOB_RECURSE TEST_RESOURCE_FILES CONFIGURE_DEPENDS "${EDITOR_RESOURCES_DIR}/*")
file(GLOB TEST_SCENE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/TestData/Scenes/*")
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/_TestResourcesSync"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:EppoEngineTesting>/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${EDITOR_RESOURCES_DIR}" "$<TARGET_FILE_DIR:EppoEngineTesting>/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/TestData/Scenes" "$<TARGET_FILE_DIR:EppoEngineTesting>/TestData/Scenes"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/_TestResourcesSync"
    DEPENDS ${TEST_RESOURCE_FILES} ${TEST_SCENE_FILES}
    COMMENT "Copying Resources + scenario TestData (tests)"
    VERBATIM
)
add_custom_target(EppoTestResources DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/_TestResourcesSync")
add_dependencies(EppoEngineTesting EppoTestResources)

function(AddTestingSuite name label)
    add_test(NAME ${name}
        COMMAND EppoEngineTesting ${name}
        WORKING_DIRECTORY $<TARGET_FILE_DIR:EppoEngineTesting>)
    set_tests_properties(${name} PROPERTIES LABELS "${label}")
endfunction()

AddTestingSuite(Core core)
AddTestingSuite(Scene core)
AddTestingSuite(Physics core)
AddTestingSuite(Scene core)
AddTestingSuite(Scripting scripting)
AddTestingSuite(ScriptMarshalling scripting)

# Graphical suites: boot the real Application (need a display + GPU). Excluded on
# headless CI via `ctest --label-exclude graphical`.
AddTestingSuite(App graphical)
AddTestingSuite(Renderer graphical)
AddTestingSuite(Scenario graphical)
