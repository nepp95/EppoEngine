#pragma once

#include "Core/Buffer.h"
#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/CommandBuffer.h"
#include "Renderer/Image.h"
#include "Renderer/Pipeline.h"

namespace Eppo
{
	class VulkanPipeline : public Pipeline
	{
	public:
		VulkanPipeline(const PipelineSpecification& specification);
		virtual ~VulkanPipeline();

		void RT_Bind(Ref<CommandBuffer> renderCommandBuffer) const;
		void RT_BindDescriptorSets(Ref<CommandBuffer> renderCommandBuffer, uint32_t start, uint32_t count);
		void RT_DrawIndexed(Ref<CommandBuffer> renderCommandBuffer, uint32_t count);
		void RT_DrawIndexed(Ref<CommandBuffer> renderCommandBuffer, const Primitive& primitive) const;

		void RT_SetViewport(Ref<CommandBuffer> renderCommandBuffer) const;
		void RT_SetScissor(Ref<CommandBuffer> renderCommandBuffer) const;
		void RT_SetPushConstants(Ref<CommandBuffer> renderCommandBuffer, Buffer data) const;

		Ref<Image> GetImage(uint32_t index) const override { return m_Images[index]; }
		Ref<Image> GetFinalImage() const override { return m_Images[0]; }

		VkPipeline GetPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

		const std::vector<VkDescriptorSet>& GetDescriptorSets(uint32_t frameIndex) { return m_DescriptorSets[frameIndex]; }
		const std::vector<VkRenderingAttachmentInfo>& GetAttachmentInfos() const { return m_AttachmentInfos; }

		const PipelineSpecification& GetSpecification() const override { return m_Specification; }
		PipelineSpecification& GetSpecification() override { return m_Specification; }

	private:
		PipelineSpecification m_Specification;

		VkPipeline m_Pipeline;
		VkPipelineLayout m_PipelineLayout;

		std::unordered_map<uint32_t, std::vector<VkDescriptorSet>> m_DescriptorSets;
		std::vector<VkRenderingAttachmentInfo> m_AttachmentInfos;
		std::vector<Ref<Image>> m_Images;
	};
}
