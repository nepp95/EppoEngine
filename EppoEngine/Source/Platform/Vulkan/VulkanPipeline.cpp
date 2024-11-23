#include "pch.h"
#include "VulkanPipeline.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanImage.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Renderer/Renderer.h"

namespace Eppo
{
	namespace Utils
	{
		static VkPrimitiveTopology TopologyToVkTopology(PrimitiveTopology topology)
		{
			switch (topology)
			{
				case PrimitiveTopology::Lines:		return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
				case PrimitiveTopology::Triangles:	return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			}

			EPPO_ASSERT(false);
			return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
		}

		static VkPolygonMode PolygonModeToVkPolygonMode(PolygonMode mode)
		{
			switch (mode)
			{
				case PolygonMode::Fill:	return VK_POLYGON_MODE_FILL;
				case PolygonMode::Line:	return VK_POLYGON_MODE_LINE;
			}

			EPPO_ASSERT(false);
			return VK_POLYGON_MODE_MAX_ENUM;
		}

		static VkCullModeFlags CullModeToVkCullMode(CullMode mode)
		{
			switch (mode)
			{
				case CullMode::Back:			return VK_CULL_MODE_BACK_BIT;
				case CullMode::Front:			return VK_CULL_MODE_FRONT_BIT;
				case CullMode::FrontAndBack:	return VK_CULL_MODE_FRONT_AND_BACK;
			}

			EPPO_ASSERT(false);
			return VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
		}

		static VkFrontFace CullFrontFaceToVkFrontFace(CullFrontFace frontFace)
		{
			switch (frontFace)
			{
				case CullFrontFace::Clockwise:			return VK_FRONT_FACE_CLOCKWISE;
				case CullFrontFace::CounterClockwise:	return VK_FRONT_FACE_COUNTER_CLOCKWISE;
			}

			EPPO_ASSERT(false);
			return VK_FRONT_FACE_MAX_ENUM;
		}

		static VkCompareOp DepthCompareOpToVkCompareOp(DepthCompareOp op)
		{
			switch (op)
			{
				case DepthCompareOp::Less:			return VK_COMPARE_OP_LESS;
				case DepthCompareOp::Equal:			return VK_COMPARE_OP_EQUAL;
				case DepthCompareOp::LessOrEqual:	return VK_COMPARE_OP_LESS_OR_EQUAL;
				case DepthCompareOp::Greater:		return VK_COMPARE_OP_GREATER;
				case DepthCompareOp::NotEqual:		return VK_COMPARE_OP_NOT_EQUAL;
				case DepthCompareOp::GreaterOrEqual:return VK_COMPARE_OP_GREATER_OR_EQUAL;
			}

			EPPO_ASSERT(false);
			return VK_COMPARE_OP_MAX_ENUM;
		}
	}

	VulkanPipeline::VulkanPipeline(const PipelineSpecification& specification)
		: m_Specification(specification)
	{
		Ref<VulkanContext> context = VulkanContext::Get();
		Ref<VulkanSwapchain> swapchain = context->GetSwapchain();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = m_Specification.Layout.GetStride();
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
		const auto& elements = m_Specification.Layout.GetElements();
		for (size_t i = 0; i < elements.size(); i++)
		{
			VkVertexInputAttributeDescription& attributeDescription = attributeDescriptions.emplace_back();
			attributeDescription.binding = 0;
			attributeDescription.location = static_cast<uint32_t>(i);
			attributeDescription.format = Utils::ShaderDataTypeToVkFormat(elements[i].Type);
			attributeDescription.offset = static_cast<uint32_t>(elements[i].Offset);
		}

		VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
		vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
		vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
		inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyStateCreateInfo.topology = Utils::TopologyToVkTopology(m_Specification.Topology);
		inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
		viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateCreateInfo.viewportCount = 1;
		viewportStateCreateInfo.scissorCount = 1;

		VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
		rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
		rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizationStateCreateInfo.polygonMode = Utils::PolygonModeToVkPolygonMode(m_Specification.PolygonMode);
		rasterizationStateCreateInfo.lineWidth = 1.0f;
		rasterizationStateCreateInfo.cullMode = Utils::CullModeToVkCullMode(m_Specification.CullMode);
		rasterizationStateCreateInfo.frontFace = Utils::CullFrontFaceToVkFrontFace(m_Specification.CullFrontFace);
		rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
		rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
		rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
		rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;

		VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};
		multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
		multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleStateCreateInfo.minSampleShading = 1.0f;
		multisampleStateCreateInfo.pSampleMask = VK_NULL_HANDLE;
		multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
		multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

		VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
		depthStencilStateCreateInfo.stencilTestEnable = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilStateCreateInfo.depthTestEnable = m_Specification.DepthTesting;
		depthStencilStateCreateInfo.depthWriteEnable = m_Specification.DepthTesting;
		depthStencilStateCreateInfo.depthCompareOp = Utils::DepthCompareOpToVkCompareOp(m_Specification.DepthCompareOp);
		depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilStateCreateInfo.minDepthBounds = 0.0f;
		depthStencilStateCreateInfo.maxDepthBounds = 1.0f;
		depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;

		std::vector<VkPipelineColorBlendAttachmentState> attachmentStates;
		for (size_t i = 0; i < m_Specification.ColorAttachments.size(); i++)
		{
			VkPipelineColorBlendAttachmentState& colorBlendAttachmentState = attachmentStates.emplace_back();
			colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachmentState.blendEnable = VK_FALSE;
			colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		}

		VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
		colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
		colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
		colorBlendStateCreateInfo.attachmentCount = static_cast<uint32_t>(attachmentStates.size());
		colorBlendStateCreateInfo.pAttachments = attachmentStates.data();
		colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
		colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
		colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
		colorBlendStateCreateInfo.blendConstants[3] = 0.0f;

		std::array<VkDynamicState, 2> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
		dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

		Ref<VulkanShader> shader = std::static_pointer_cast<VulkanShader>(m_Specification.Shader);
		const auto& descriptorSetLayouts = shader->GetDescriptorSetLayouts();

		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
		{
			m_DescriptorSets[i].resize(4);
			for (uint32_t j = 0; j < 4; j++)
				m_DescriptorSets[i][j] = Renderer::AllocateDescriptor(descriptorSetLayouts[j]);
		}

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
		pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<size_t>(shader->GetPushConstantRanges().size());
		pipelineLayoutCreateInfo.pPushConstantRanges = !shader->GetPushConstantRanges().empty() ? shader->GetPushConstantRanges().data() : nullptr;
	
		VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayout), "Failed to create pipeline layout!");

		const auto& shaderStageInfos = shader->GetPipelineShaderStageInfos();
	
		std::vector<VkFormat> formats;
		for (const auto& colorAttachment : m_Specification.ColorAttachments)
		{
			VkFormat& format = formats.emplace_back();
			format = Utils::ImageFormatToVkFormat(colorAttachment.Format);
		}

		VkPipelineRenderingCreateInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		renderingInfo.colorAttachmentCount = static_cast<uint32_t>(formats.size());
		renderingInfo.pColorAttachmentFormats = formats.data();
		renderingInfo.depthAttachmentFormat = m_Specification.DepthTesting ? Utils::ImageFormatToVkFormat(ImageFormat::Depth) : VK_FORMAT_UNDEFINED;

		if (m_Specification.DepthCubeMapImage)
			renderingInfo.viewMask = 0b111111;

		VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
		graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		graphicsPipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStageInfos.size());
		graphicsPipelineCreateInfo.pStages = shaderStageInfos.data();
		graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
		graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
		graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
		graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
		graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
		graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
		graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
		graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
		graphicsPipelineCreateInfo.layout = m_PipelineLayout;
		graphicsPipelineCreateInfo.renderPass = nullptr; // Dynamic rendering
		graphicsPipelineCreateInfo.subpass = 0;
		graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		graphicsPipelineCreateInfo.basePipelineIndex = -1;
		graphicsPipelineCreateInfo.pNext = &renderingInfo;

		VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_Pipeline), "Failed to create graphics pipeline!");

		// Create the images from the attachment info
		for (const auto& attachment : m_Specification.ColorAttachments)
		{
			if (!m_Specification.SwapchainTarget && !m_Specification.ExistingImage)
			{
				ImageSpecification imageSpec;
				imageSpec.Format = attachment.Format;
				imageSpec.Width = m_Specification.Width;
				imageSpec.Height = m_Specification.Height;
				imageSpec.Usage = ImageUsage::Attachment;

				Ref<Image> image = Image::Create(imageSpec);
				m_Images.emplace_back(image);
			}

			if (m_Specification.ExistingImage)
				m_Images.emplace_back(m_Specification.ExistingImage);
		}

		if (m_Specification.DepthTesting)
		{
			// We simply want a depth attachment to go with our color attachments
			ImageSpecification imageSpec;
			imageSpec.Format = ImageFormat::Depth;
			imageSpec.Width = m_Specification.Width;
			imageSpec.Height = m_Specification.Height;
			imageSpec.Usage = ImageUsage::Attachment;
			imageSpec.CubeMap = m_Specification.DepthCubeMapImage;

			m_Specification.DepthImage = Image::Create(imageSpec);

			VkCommandBuffer cmd = context->GetLogicalDevice()->GetCommandBuffer(true);
			VulkanImage::TransitionImage(cmd, std::static_pointer_cast<VulkanImage>(m_Specification.DepthImage)->GetImageInfo().Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
			context->GetLogicalDevice()->FlushCommandBuffer(cmd);
		}
	}

	VulkanPipeline::~VulkanPipeline()
	{
		VkDevice device = VulkanContext::Get()->GetLogicalDevice()->GetNativeDevice();

		EPPO_WARN("Releasing pipeline {}", (void*)m_Pipeline);
		vkDestroyPipeline(device, m_Pipeline, nullptr);

		EPPO_WARN("Releasing pipeline layout {}", (void*)m_PipelineLayout);
		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
	}

	void VulkanPipeline::RT_Bind(Ref<CommandBuffer> commandBuffer) const
	{
		Renderer::SubmitCommand([this, commandBuffer]()
		{
			auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
			VkCommandBuffer cb = cmd->GetCurrentCommandBuffer();
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
		});
	}

	void VulkanPipeline::RT_BindDescriptorSets(Ref<CommandBuffer> commandBuffer, uint32_t start, uint32_t count)
	{
		Renderer::SubmitCommand([this, commandBuffer, start, count]()
		{
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
			auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
			VkCommandBuffer cb = cmd->GetCurrentCommandBuffer();

			const auto& descriptorSets = GetDescriptorSets(frameIndex);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, start, count, descriptorSets.data(), 0, nullptr);
		});
	}

	void VulkanPipeline::RT_DrawIndexed(Ref<CommandBuffer> commandBuffer, uint32_t count)
	{
		Renderer::SubmitCommand([commandBuffer, count]()
		{
			auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
			VkCommandBuffer cb = cmd->GetCurrentCommandBuffer();

			vkCmdDrawIndexed(cb, count, 1, 0, 0, 0);
		});
	}

	void VulkanPipeline::RT_DrawIndexed(Ref<CommandBuffer> commandBuffer, const Primitive& primitive) const
	{
		Renderer::SubmitCommand([commandBuffer, primitive]()
		{
			auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
			VkCommandBuffer cb = cmd->GetCurrentCommandBuffer();

			vkCmdDrawIndexed(cb, primitive.IndexCount, 1, primitive.FirstIndex, primitive.FirstVertex, 0);
		});
	}

	void VulkanPipeline::RT_SetViewport(Ref<CommandBuffer> commandBuffer) const
	{
		Renderer::SubmitCommand([this, commandBuffer]()
		{
			auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
			VkCommandBuffer cb = cmd->GetCurrentCommandBuffer();

			const auto& spec = GetSpecification();

			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = static_cast<float>(spec.Width);
			viewport.height = static_cast<float>(spec.Height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			vkCmdSetViewport(cb, 0, 1, &viewport);
		});
	}

	void VulkanPipeline::RT_SetScissor(Ref<CommandBuffer> commandBuffer) const
	{
		Renderer::SubmitCommand([this, commandBuffer]()
		{
			auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
			VkCommandBuffer cb = cmd->GetCurrentCommandBuffer();

			const auto& spec = GetSpecification();

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = { spec.Width, spec.Height };

			vkCmdSetScissor(cb, 0, 1, &scissor);
		});
	}

	void VulkanPipeline::RT_SetPushConstants(Ref<CommandBuffer> commandBuffer, Buffer data) const
	{
		Renderer::SubmitCommand([this, commandBuffer, data]() mutable
		{
			auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
			VkCommandBuffer cb = cmd->GetCurrentCommandBuffer();

			vkCmdPushConstants(cb, GetPipelineLayout(), VK_SHADER_STAGE_ALL_GRAPHICS, 0, data.Size, data.Data);

			data.Release();
		});
	}
}
