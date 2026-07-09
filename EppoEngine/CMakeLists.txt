project(Engine)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "Source/*.cpp" "Vendor/*.cpp")
add_library(EppoEngine STATIC ${SOURCES})

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Target platform: ${CMAKE_SYSTEM_NAME}")
    target_compile_definitions(EppoEngine PUBLIC EP_PLATFORM_WINDOWS)
    target_compile_options(EppoEngine PUBLIC /utf-8)
    target_include_directories(EppoEngine PUBLIC $ENV{VULKAN_SDK}/Include)
    target_link_directories(EppoEngine PUBLIC "$ENV{VULKAN_SDK}/Lib")
    target_link_libraries(EppoEngine PRIVATE $<$<CONFIG:Debug>:spirv-cross-cored> $<$<CONFIG:RelWithDebInfo>:spirv-cross-core> $<$<CONFIG:Dist>:spirv-cross-core>)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Target platform: ${CMAKE_SYSTEM_NAME}")
    # DXC's WinAdapter.h uses __declspec(uuid(...)) unless __EMULATE_UUID is set;
    # the latter selects its cross-platform UUID path so the headers compile with
    # clang on Linux (see Platform/Vulkan/VulkanShader.cpp).
    target_compile_definitions(EppoEngine PUBLIC EP_PLATFORM_LINUX __EMULATE_UUID)
    target_include_directories(EppoEngine PUBLIC $ENV{VULKAN_SDK}/include)
    target_link_directories(EppoEngine PUBLIC "$ENV{VULKAN_SDK}/lib")
    target_link_libraries(EppoEngine PRIVATE spirv-cross-core)
else()
    message(FATAL_ERROR "Unsupported platform '${CMAKE_SYSTEM_NAME}' detected!")
endif()

set(INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    ${TINYGLTF_INCLUDE_DIRS}
)

target_compile_definitions(EppoEngine PUBLIC
    $<$<CONFIG:Debug>:EP_DEBUG> $<$<CONFIG:RelWithDebInfo>:EP_RELEASE> $<$<CONFIG:Release>:EP_DIST>
    $<$<CONFIG:Debug>:TRACY_ENABLE> $<$<CONFIG:RelWithDebInfo>:TRACY_ENABLE>
)
target_include_directories(EppoEngine PUBLIC ${INCLUDE_DIRS})
target_link_libraries(EppoEngine PUBLIC EnTT::EnTT glm::glm imgui::imgui imguizmo::imguizmo nfd::nfd nvrhi spdlog::spdlog Tracy::TracyClient Vulkan::Vulkan EppoScriptCore::EppoScriptCore.Native)
target_link_libraries(EppoEngine PRIVATE
    glfw
    Vulkan::dxc_lib
)
target_precompile_headers(EppoEngine PRIVATE "Source/pch.h")

if (MSVC)
    target_compile_options(EppoEngine PRIVATE "/W3")
endif ()