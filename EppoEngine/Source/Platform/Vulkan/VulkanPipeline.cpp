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
		static VkPrimitiveTopology TopologyToVkTopology(const PrimitiveTopology topology)
		{
			switch (topology)
			{
				case PrimitiveTopology::Lines:		return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
				case PrimitiveTopology::Triangles:	return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			}

			EPPO_ASSERT(false)
			return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
		}

		static VkPolygonMode PolygonModeToVkPolygonMode(const PolygonMode mode)
		{
			switch (mode)
			{
				case PolygonMode::Fill:	return VK_POLYGON_MODE_FILL;
				case PolygonMode::Line:	return VK_POLYGON_MODE_LINE;
			}

			EPPO_ASSERT(false)
			return VK_POLYGON_MODE_MAX_ENUM;
		}

		static VkCullModeFlags CullModeToVkCullMode(const CullMode mode)
		{
			switch (mode)
			{
				case CullMode::Back:			return VK_CULL_MODE_BACK_BIT;
				case CullMode::Front:			return VK_CULL_MODE_FRONT_BIT;
				case CullMode::FrontAndBack:	return VK_CULL_MODE_FRONT_AND_BACK;
			}

			EPPO_ASSERT(false)
			return VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
		}

		static VkFrontFace CullFrontFaceToVkFrontFace(const CullFrontFace frontFace)
		{
			switch (frontFace)
			{
				case CullFrontFace::Clockwise:			return VK_FRONT_FACE_CLOCKWISE;
				case CullFrontFace::CounterClockwise:	return VK_FRONT_FACE_COUNTER_CLOCKWISE;
			}

			EPPO_ASSERT(false)
			return VK_FRONT_FACE_MAX_ENUM;
		}

		static VkCompareOp DepthCompareOpToVkCompareOp(const DepthCompareOp op)
		{
			switch (op)
			{
				case DepthCompareOp::Less:			return VK_COMPARE_OP_LESS;
				case DepthCompareOp::Equal:			return VK_COMPARE_OP_EQUAL;
				case DepthCompareOp::LessOrEqual:	return VK_COMPARE_OP_LESS_OR_EQUAL;
				case DepthCompareOp::Greater:		return VK_COMPARE_OP_GREATER;
				case DepthCompareOp::NotEqual:		return VK_COMPARE_OP_NOT_EQUAL;
				case DepthCompareOp::GreaterOrEqual:return VK_COMPARE_OP_GREATER_OR_EQUAL;

				case DepthCompareOp::Always:		return VK_COMPARE_OP_ALWAYS;
				case DepthCompareOp::Never:			return VK_COMPARE_OP_NEVER;
			}

			EPPO_ASSERT(false)
			return VK_COMPARE_OP_MAX_ENUM;
		}
	}

	VulkanPipeline::VulkanPipeline(PipelineSpecification specification)
		: m_Specification(std::move(specification))
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
		vertexInputStateCreateInfo.vertexBindingDescriptionCount = elements.empty() ? 0 : 1;
		vertexInputStateCreateInfo.pVertexBindingDescriptions = elements.empty() ? nullptr : &bindingDescription;
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
		depthStencilStateCreateInfo.depthTestEnable = m_Specification.TestDepth;
		depthStencilStateCreateInfo.depthWriteEnable = m_Specification.WriteDepth;
		depthStencilStateCreateInfo.depthCompareOp = Utils::DepthCompareOpToVkCompareOp(m_Specification.DepthCompareOp);
		depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilStateCreateInfo.minDepthBounds = 0.0f;
		depthStencilStateCreateInfo.maxDepthBounds = 1.0f;
		depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;

		std::vector<VkPipelineColorBlendAttachmentState> attachmentStates;
		for (const auto& attachment : m_Specification.RenderAttachments)
		{
			if (attachment.RenderImage->GetSpecification().Format == ImageFormat::Depth)
				continue;

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

		// Allocate descriptor sets
		Ref<VulkanShader> shader = std::static_pointer_cast<VulkanShader>(m_Specification.Shader);
		const auto& descriptorSetLayouts = shader->GetDescriptorSetLayouts();

		// Create pipeline layout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
		pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(shader->GetPushConstantRanges().size());
		pipelineLayoutCreateInfo.pPushConstantRanges = !shader->GetPushConstantRanges().empty() ? shader->GetPushConstantRanges().data() : nullptr;
	
		VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayout), "Failed to create pipeline layout!")

		// Create pipeline rendering infos
		const auto& shaderStageInfos = shader->GetPipelineShaderStageInfos();
	
		std::vector<VkFormat> formats;
		for (const auto& colorAttachment : m_Specification.RenderAttachments)
		{
			if (const auto format = colorAttachment.RenderImage->GetSpecification().Format;
				format != ImageFormat::Depth)
			{
				VkFormat& vkFormat = formats.emplace_back();
				vkFormat = Utils::ImageFormatToVkFormat(format);
			}
		}

		VkPipelineRenderingCreateInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		renderingInfo.colorAttachmentCount = static_cast<uint32_t>(formats.size());
		renderingInfo.pColorAttachmentFormats = formats.data();
		renderingInfo.depthAttachmentFormat = m_Specification.TestDepth ? Utils::ImageFormatToVkFormat(ImageFormat::Depth) : VK_FORMAT_UNDEFINED;

		if (m_Specification.CubeMap)
			renderingInfo.viewMask = 0b111111;

		// Create pipeline
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

		VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_Pipeline), "Failed to create graphics pipeline!")

		if (m_Specification.CreateDepthImage)
		{
			// We simply want a depth attachment to go with our color attachments
			ImageSpecification imageSpec;
			imageSpec.Format = ImageFormat::Depth;
			imageSpec.Width = m_Specification.Width;
			imageSpec.Height = m_Specification.Height;
			imageSpec.Usage = ImageUsage::Attachment;

			Ref<Image> image = Image::Create(imageSpec);

			m_Specification.RenderAttachments.emplace_back(image, true, 1.0f);

			VkCommandBuffer cmd = context->GetLogicalDevice()->GetCommandBuffer(true);
			VulkanImage::TransitionImage(cmd, std::static_pointer_cast<VulkanImage>(image)->GetImageInfo().Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
			context->GetLogicalDevice()->FlushCommandBuffer(cmd);
		}
	}

	VulkanPipeline::~VulkanPipeline()
	{
		const VkDevice device = VulkanContext::Get()->GetLogicalDevice()->GetNativeDevice();

		EPPO_MEM_WARN("Releasing pipeline {}", static_cast<void*>(m_Pipeline));
		vkDestroyPipeline(device, m_Pipeline, nullptr);

		EPPO_MEM_WARN("Releasing pipeline layout {}", static_cast<void*>(m_PipelineLayout));
		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
	}
}
