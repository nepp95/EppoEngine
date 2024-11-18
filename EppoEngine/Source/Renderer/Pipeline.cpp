#include "pch.h"
#include "Pipeline.h"

#include "Renderer/Renderer.h"

namespace Eppo
{
	Pipeline::Pipeline(const PipelineSpecification& specification)
		: m_Specification(specification)
	{
		Ref<RendererContext> context = RendererContext::Get();
		Ref<Swapchain> swapchain = context->GetSwapchain();
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
		inputAssemblyStateCreateInfo.topology = m_Specification.Topology;
		inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
		viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateCreateInfo.viewportCount = 1;
		viewportStateCreateInfo.scissorCount = 1;

		VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
		rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
		rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizationStateCreateInfo.polygonMode = m_Specification.PolygonMode;
		rasterizationStateCreateInfo.lineWidth = 1.0f;
		rasterizationStateCreateInfo.cullMode = m_Specification.CullMode;
		rasterizationStateCreateInfo.frontFace = m_Specification.CullFrontFace;
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
		depthStencilStateCreateInfo.depthCompareOp = m_Specification.DepthCompareOp;
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

		const auto& descriptorSetLayouts = m_Specification.Shader->GetDescriptorSetLayouts();

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
		pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<size_t>(m_Specification.PushConstantRanges.size());
		pipelineLayoutCreateInfo.pPushConstantRanges = !m_Specification.PushConstantRanges.empty() ? m_Specification.PushConstantRanges.data() : nullptr;

		VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayout), "Failed to create pipeline layout!");

		const auto& shaderStageInfos = m_Specification.Shader->GetPipelineShaderStageInfos();

		std::vector<VkFormat> formats = GetVkColorAttachmentFormats();

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
			if (!m_Specification.SwapchainTarget)
			{
				ImageSpecification imageSpec;
				imageSpec.Format = attachment.Format;
				imageSpec.Width = m_Specification.Width;
				imageSpec.Height = m_Specification.Height;
				imageSpec.Usage = ImageUsage::Attachment;

				Ref<Image> image = CreateRef<Image>(imageSpec);
				m_Images.emplace_back(image);
			}
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

			m_Specification.DepthImage = CreateRef<Image>(imageSpec);

			VkCommandBuffer cmd = context->GetLogicalDevice()->GetCommandBuffer(true);
			Image::TransitionImage(cmd, m_Specification.DepthImage->GetImageInfo().Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
			context->GetLogicalDevice()->FlushCommandBuffer(cmd);
		}
	}

	Pipeline::~Pipeline()
	{
		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		EPPO_WARN("Releasing pipeline {}", (void*)m_Pipeline);
		vkDestroyPipeline(device, m_Pipeline, nullptr);

		EPPO_WARN("Releasing pipeline layout {}", (void*)m_PipelineLayout);
		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
	}

	void Pipeline::RT_Bind(Ref<RenderCommandBuffer> renderCommandBuffer) const
	{
		auto instance = this;

		Renderer::SubmitCommand([instance, renderCommandBuffer]()
		{
			VkCommandBuffer cb = renderCommandBuffer->GetCurrentCommandBuffer();
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, instance->m_Pipeline);
		});
	}

	void Pipeline::RT_BindDescriptorSets(Ref<RenderCommandBuffer> renderCommandBuffer, uint32_t start, uint32_t count)
	{
		auto instance = this;

		Renderer::SubmitCommand([instance, renderCommandBuffer, start, count]()
		{
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
			VkCommandBuffer cb = renderCommandBuffer->GetCurrentCommandBuffer();

			const auto& descriptorSets = instance->GetDescriptorSets(frameIndex);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, instance->m_PipelineLayout, start, count, descriptorSets.data(), 0, nullptr);
		});
	}

	void Pipeline::RT_Draw(Ref<RenderCommandBuffer> renderCommandBuffer, const Primitive& primitive) const
	{
		auto instance = this;

		Renderer::SubmitCommand([instance, renderCommandBuffer, primitive]()
		{
			VkCommandBuffer cb = renderCommandBuffer->GetCurrentCommandBuffer();

			vkCmdDrawIndexed(cb, primitive.IndexCount, 1, primitive.FirstIndex, primitive.FirstVertex, 0);
		});
	}

	void Pipeline::RT_SetViewport(Ref<RenderCommandBuffer> renderCommandBuffer) const
	{
		auto instance = this;

		Renderer::SubmitCommand([instance, renderCommandBuffer]()
		{
			VkCommandBuffer cb = renderCommandBuffer->GetCurrentCommandBuffer();

			const auto& spec = instance->GetSpecification();

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

	void Pipeline::RT_SetScissor(Ref<RenderCommandBuffer> renderCommandBuffer) const
	{
		auto instance = this;

		Renderer::SubmitCommand([instance, renderCommandBuffer]()
		{
			VkCommandBuffer cb = renderCommandBuffer->GetCurrentCommandBuffer();

			const auto& spec = instance->GetSpecification();

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = { spec.Width, spec.Height };

			vkCmdSetScissor(cb, 0, 1, &scissor);
		});
	}

	void Pipeline::RT_SetPushConstants(Ref<RenderCommandBuffer> renderCommandBuffer, Buffer data) const
	{
		auto instance = this;

		Renderer::SubmitCommand([instance, renderCommandBuffer, data]() mutable
		{
			VkCommandBuffer cb = renderCommandBuffer->GetCurrentCommandBuffer();

			vkCmdPushConstants(cb, instance->GetPipelineLayout(), VK_SHADER_STAGE_ALL_GRAPHICS, 0, data.Size, data.Data);

			data.Release();
		});
	}

	std::vector<VkFormat> Pipeline::GetVkColorAttachmentFormats() const
	{
		std::vector<VkFormat> vkFormats;

		for (const auto& colorAttachment : m_Specification.ColorAttachments)
		{
			VkFormat& vkFormat = vkFormats.emplace_back();
			vkFormat = Utils::ImageFormatToVkFormat(colorAttachment.Format);
		}

		return vkFormats;
	}
}
