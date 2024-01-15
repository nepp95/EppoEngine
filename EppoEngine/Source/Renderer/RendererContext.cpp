#include "pch.h"
#include "RendererContext.h"

#include "Core/Application.h"
#include "Platform/Vulkan/VulkanContext.h"

namespace Eppo
{
	RendererAPI RendererContext::s_API = RendererAPI::Vulkan;

	Ref<RendererContext> RendererContext::Get()
	{
		return Application::Get().GetWindow().GetRendererContext();
	}

	Ref<RendererContext> RendererContext::Create(void* windowHandle)
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
				return Ref<VulkanContext>::Create(static_cast<GLFWwindow*>(windowHandle)).As<RendererContext>();
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
}
