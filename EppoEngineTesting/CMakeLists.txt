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

add_test(NAME EppoEngineTesting COMMAND EppoEngineTesting)
