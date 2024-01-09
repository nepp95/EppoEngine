#include "pch.h"
#include "RendererContext.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Eppo
{
	RendererAPI RendererContext::s_API = RendererAPI::Vulkan;

	Scope<RendererContext> RendererContext::Create(void* windowHandle)
	{
		switch (s_API)
		{
			case RendererAPI::OpenGL:
			{
				EPPO_ASSERT(false);
				break;	
			}

			case RendererAPI::Vulkan:
			{
				return CreateScope<VulkanContext>(static_cast<GLFWwindow*>(windowHandle));
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}

	//void RendererContext::Init()
	//{
	//	// Profiler
	//	m_TracyContexts.resize(VulkanConfig::MaxFramesInFlight);
	//	for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
	//		m_TracyContexts[i] = TracyVkContext(m_PhysicalDevice->GetNativeDevice(), m_LogicalDevice->GetNativeDevice(), m_LogicalDevice->GetGraphicsQueue(), m_Swapchain->m_CommandBuffers[i]);

	//	SubmitResourceFree([=]()
	//	{
	//		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
	//			TracyVkDestroy(m_TracyContexts[i]);
	//	});
	//}

	//void RendererContext::WaitIdle()
	//{
	//	EPPO_PROFILE_FUNCTION("RendererContext::WaitIdle");

	//	VkDevice device = m_LogicalDevice->GetNativeDevice();
	//	vkDeviceWaitIdle(device);
	//}

	Ref<RendererContext> RendererContext::Get()
	{
		EPPO_PROFILE_FUNCTION("RendererContext::Get");

		return Application::Get().GetWindow().GetRendererContext();
	}
}
