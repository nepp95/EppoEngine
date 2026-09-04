#include "pch.h"
#include "Renderer/Swapchain.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/Renderer.h"

#include <GLFW/glfw3.h>

namespace Eppo
{
    Swapchain::Swapchain(GLFWwindow* window)
        : m_Window(window)
    {
        EP_ASSERT(window);
    }

    auto Swapchain::Resize(const uint32_t width, const uint32_t height) -> void
    {
        const auto device = DeviceManager::Get()->GetDevice();

        device->waitForIdle();
        DeviceManager::Get()->GetRenderer()->ReleaseSwapchainResources();
        device->runGarbageCollection();
        CreateSwapchain(width, height);
        m_ResizePending = false;
    }

    auto Swapchain::GetWindowFramebufferSize() const -> std::pair<uint32_t, uint32_t>
    {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_Window, &width, &height);

        return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    }
}
