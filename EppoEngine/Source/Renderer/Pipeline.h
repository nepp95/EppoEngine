#pragma once

#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Image.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexBufferLayout.h"

namespace Eppo
{
	struct ColorAttachment
	{
		ImageFormat Format;

		bool Clear = true;
		glm::vec4 ClearValue = glm::vec4(0.0f);

		ColorAttachment(ImageFormat format, bool clear = true, const glm::vec4& clearValue = glm::vec4(0.0f))
			: Format(format), Clear(clear), ClearValue(clearValue)
		{}
	};

	struct PipelineSpecification
	{
		Ref<Shader> Shader;
		VertexBufferLayout Layout;
		bool SwapchainTarget = false;

		uint32_t Width = 0;
		uint32_t Height = 0;

		// Input Assembly
		VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		// Rasterization
		VkPolygonMode PolygonMode = VK_POLYGON_MODE_FILL;
		VkCullModeFlags CullMode = VK_CULL_MODE_BACK_BIT;
		VkFrontFace CullFrontFace = VK_FRONT_FACE_CLOCKWISE;

		// Depth Stencil
		bool DepthTesting = false;
		bool DepthCubeMapImage = false;
		bool ClearDepthOnLoad = true;
		float ClearDepth = 1.0f;
		VkCompareOp DepthCompareOp = VK_COMPARE_OP_LESS;
		Ref<Image> DepthImage = nullptr;

		// Color Attachments
		std::vector<ColorAttachment> ColorAttachments;
		Ref<Image> ExistingImage;

		// Push Constants
		std::vector<VkPushConstantRange> PushConstantRanges;
	};

	class Pipeline
	{
	public:
		Pipeline(const PipelineSpecification& specification);
		~Pipeline();

		void RT_Bind(Ref<RenderCommandBuffer> renderCommandBuffer) const;
		void RT_BindDescriptorSets(Ref<RenderCommandBuffer> renderCommandBuffer, uint32_t start, uint32_t count);
		void RT_DrawIndexed(Ref<RenderCommandBuffer> renderCommandBuffer, uint32_t count);
		void RT_DrawIndexed(Ref<RenderCommandBuffer> renderCommandBuffer, const Primitive& primitive) const;

		void RT_SetViewport(Ref<RenderCommandBuffer> renderCommandBuffer) const;
		void RT_SetScissor(Ref<RenderCommandBuffer> renderCommandBuffer) const;
		void RT_SetPushConstants(Ref<RenderCommandBuffer> renderCommandBuffer, Buffer data) const;

		const PipelineSpecification& GetSpecification() const { return m_Specification; }
		PipelineSpecification& GetSpecification() { return m_Specification; }

		VkPipeline GetPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

		Ref<Image> GetImage(uint32_t index) const { return m_Images[index]; }
		Ref<Image> GetFinalImage() const { return m_Images[0]; }

		const std::vector<VkDescriptorSet>& GetDescriptorSets(uint32_t frameIndex) { return m_DescriptorSets[frameIndex]; }
		const std::vector<VkRenderingAttachmentInfo>& GetAttachmentInfos() const { return m_AttachmentInfos; }

	private:
		std::vector<VkFormat> GetVkColorAttachmentFormats() const;		

	private:
		PipelineSpecification m_Specification;

		VkPipeline m_Pipeline;
		VkPipelineLayout m_PipelineLayout;

		std::unordered_map<uint32_t, std::vector<VkDescriptorSet>> m_DescriptorSets;
		std::vector<VkRenderingAttachmentInfo> m_AttachmentInfos;
		std::vector<Ref<Image>> m_Images;
	};
}
