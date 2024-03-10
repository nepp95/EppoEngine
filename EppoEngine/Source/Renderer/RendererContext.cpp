#include "pch.h"
#include "RendererContext.h"

#include "Core/Application.h"
#include "Platform/OpenGL/OpenGLContext.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Renderer/RendererAPI.h"

namespace Eppo
{
	Ref<RendererContext> RendererContext::Get()
	{
		return Application::Get().GetWindow().GetRendererContext();
	}

	Ref<RendererContext> RendererContext::Create(void* windowHandle)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::OpenGL:
			{
				return Ref<OpenGLContext>::Create(static_cast<GLFWwindow*>(windowHandle)).As<RendererContext>();
			}

			case RendererAPIType::Vulkan:
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
