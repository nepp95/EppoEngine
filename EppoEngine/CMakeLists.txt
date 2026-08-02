project(Engine)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "Source/*.cpp" "Vendor/*.cpp")
add_library(EppoEngine STATIC ${SOURCES})

target_compile_definitions(EppoEngine PUBLIC ${EP_PLATFORM} GLM_FORCE_DEPTH_ZERO_TO_ONE)

if (${EP_PLATFORM} STREQUAL "EP_PLATFORM_WINDOWS")
    target_compile_options(EppoEngine PUBLIC /utf-8)
    target_include_directories(EppoEngine PUBLIC $ENV{VULKAN_SDK}/Include)
    target_link_directories(EppoEngine PUBLIC "$ENV{VULKAN_SDK}/Lib")
    target_link_libraries(EppoEngine PRIVATE $<$<CONFIG:Debug>:spirv-cross-cored> $<$<CONFIG:RelWithDebInfo>:spirv-cross-core> $<$<CONFIG:Release>:spirv-cross-core>)
elseif (${EP_PLATFORM} STREQUAL "EP_PLATFORM_LINUX")
    # DXC's WinAdapter.h uses __declspec(uuid(...)) unless __EMULATE_UUID is set;
    # the latter selects its cross-platform UUID path so the headers compile with
    # clang on Linux (see Platform/Vulkan/VulkanShader.cpp).
    target_compile_definitions(EppoEngine PUBLIC __EMULATE_UUID)
    target_include_directories(EppoEngine PUBLIC $ENV{VULKAN_SDK}/include)
    target_link_directories(EppoEngine PUBLIC "$ENV{VULKAN_SDK}/lib")
    target_link_libraries(EppoEngine PRIVATE spirv-cross-core)
    target_link_libraries(EppoEngine PUBLIC ${CMAKE_DL_LIBS}) # dlopen, dlsym
else()
    message(FATAL_ERROR "Unsupported platform '${CMAKE_SYSTEM_NAME}' detected!")
endif()

set(INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Vendor"
    ${TINYGLTF_INCLUDE_DIRS}
)

target_compile_definitions(EppoEngine PUBLIC
    $<$<CONFIG:Debug>:EP_DEBUG> $<$<CONFIG:RelWithDebInfo>:EP_RELEASE> $<$<CONFIG:Release>:EP_DIST>
    $<$<CONFIG:Debug>:TRACY_ENABLE> $<$<CONFIG:RelWithDebInfo>:TRACY_ENABLE>
)
target_include_directories(EppoEngine PUBLIC ${INCLUDE_DIRS})
target_link_libraries(EppoEngine PUBLIC box3d::box3d EnTT::EnTT glm::glm imgui::imgui imguizmo::imguizmo ImGuiFileDialog::ImGuiFileDialog nvrhi spdlog::spdlog Tracy::TracyClient Vulkan::Vulkan)
target_link_libraries(EppoEngine PRIVATE
    efsw::efsw
    glfw
    Vulkan::dxc_lib
)
target_precompile_headers(EppoEngine PRIVATE "Source/pch.h")

if (MSVC)
    target_compile_options(EppoEngine PRIVATE "/W3")
endif ()
