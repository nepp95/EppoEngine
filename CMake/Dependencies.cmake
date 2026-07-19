find_package(Vulkan REQUIRED COMPONENTS dxc)
if (NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan not found!")
endif ()

find_package(box3d CONFIG REQUIRED)
if (NOT box3d_FOUND)
    message(FATAL_ERROR "box3d not found!")
endif ()

find_package(efsw CONFIG REQUIRED)
if (NOT efsw_FOUND)
    message(FATAL_ERROR "efsw not found!")
endif ()

find_package(EnTT REQUIRED)
if (NOT EnTT_FOUND)
    message(FATAL_ERROR "entt not found!")
endif ()

find_package(glm REQUIRED)
if (NOT glm_FOUND)
    message(FATAL_ERROR "glm not found!")
endif ()

find_package(imgui REQUIRED)
if (NOT imgui_FOUND)
    message(FATAL_ERROR "imgui not found!")
endif ()

find_package(imguizmo CONFIG REQUIRED)
if (NOT imguizmo_FOUND)
    message(FATAL_ERROR "imguizmo not found!")
endif ()

find_package(nfd REQUIRED)
if (NOT nfd_FOUND)
    message(FATAL_ERROR "nfd not found!")
endif ()

find_package(nvrhi REQUIRED)
if (NOT nvrhi_FOUND)
    message(FATAL_ERROR "nvrhi not found!")
endif ()

find_package(spdlog REQUIRED)
if (NOT spdlog_FOUND)
    message(FATAL_ERROR "spdlog not found!")
endif ()

find_package(tracy REQUIRED NAMES Tracy)
if (NOT tracy_FOUND)
    message(FATAL_ERROR "tracy not found!")
endif ()

find_package(UnitTest++ CONFIG REQUIRED)
if (NOT UnitTest++_FOUND)
    message(FATAL_ERROR "tracy not found!")
endif ()

if (NOT DEFINED ENV{DOTNET_ROOT})
    message(FATAL_ERROR "Set the DOTNET_ROOT environment variable to the .net SDK")
endif ()