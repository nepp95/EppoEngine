#include "pch.h"
#include "VulkanPipeline.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Platform/Vulkan/VulkanUniformBuffer.h"

namespace Eppo
{
	namespace Utils
	{
		static VkFormat ShaderDataTypeToVkFormat(ShaderDataType type)
		{
			switch (type)
			{
				case ShaderDataType::Float:			return VK_FORMAT_R32_SFLOAT;
				case ShaderDataType::Float2:		return VK_FORMAT_R32G32_SFLOAT;
				case ShaderDataType::Float3:		return VK_FORMAT_R32G32B32_SFLOAT;
				case ShaderDataType::Float4:		return VK_FORMAT_R32G32B32A32_SFLOAT;
					//case ShaderDataType::Mat3:			return VK_FORMAT_;
					//case ShaderDataType::Mat4:			return VK_FORMAT_R32_SFLOAT;
				case ShaderDataType::Int:			return VK_FORMAT_R32_SINT;
				case ShaderDataType::Int2:			return VK_FORMAT_R32G32_SINT;
				case ShaderDataType::Int3:			return VK_FORMAT_R32G32B32_SINT;
				case ShaderDataType::Int4:			return VK_FORMAT_R32G32B32A32_SINT;
				case ShaderDataType::Bool:			return VK_FORMAT_R8_UINT;
			}

			EPPO_ASSERT(false);
			return VK_FORMAT_UNDEFINED;
		}
	}

	VulkanPipeline::VulkanPipeline(const PipelineSpecification& specification)
		: Pipeline(specification)
	{
		EPPO_PROFILE_FUNCTION("VulkanPipeline::VulkanPipeline");

		Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		// Vertex input
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = m_Specification.Layout.GetStride();
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
		const auto& elements = m_Specification.Layout.GetElements();
		for (size_t i = 0; i < elements.size(); i++)
		{
			VkVertexInputAttributeDescription& attribute = attributeDescriptions.emplace_back();
			attribute.binding = 0;
			attribute.location = i;
			attribute.format = Utils::ShaderDataTypeToVkFormat(elements[i].Type);
			attribute.offset = elements[i].Offset;
		}

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		// Input assembly
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
		inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

		// Viewport
		VkPipelineViewportStateCreateInfo viewportStateInfo{};
		viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateInfo.viewportCount = 1;
		viewportStateInfo.scissorCount = 1;

		// Rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
		rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizerInfo.depthClampEnable = VK_FALSE;
		rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizerInfo.lineWidth = 1.0f;
		rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizerInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizerInfo.depthBiasEnable = VK_FALSE;
		rasterizerInfo.depthBiasConstantFactor = 0.0f;
		rasterizerInfo.depthBiasClamp = 0.0f;
		rasterizerInfo.depthBiasSlopeFactor = 0.0f;

		// Multisampling
		VkPipelineMultisampleStateCreateInfo multisamplingInfo{};
		multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisamplingInfo.sampleShadingEnable = VK_FALSE;
		multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisamplingInfo.minSampleShading = 1.0f;
		multisamplingInfo.pSampleMask = VK_NULL_HANDLE;
		multisamplingInfo.alphaToCoverageEnable = VK_FALSE;
		multisamplingInfo.alphaToOneEnable = VK_FALSE;

		// Depth testing
		VkPipelineDepthStencilStateCreateInfo depthInfo{};
		depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthInfo.depthTestEnable = m_Specification.DepthTesting ? VK_TRUE : VK_FALSE;
		depthInfo.depthWriteEnable = m_Specification.DepthTesting ? VK_TRUE : VK_FALSE;
		depthInfo.depthCompareOp = m_Specification.DepthTesting ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_ALWAYS;
		depthInfo.depthBoundsTestEnable = VK_FALSE;
		depthInfo.minDepthBounds = 0.0f;
		depthInfo.maxDepthBounds = 1.0f;
		depthInfo.stencilTestEnable = VK_FALSE;

		// Color blending
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
		colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendInfo.logicOpEnable = VK_FALSE;
		colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
		colorBlendInfo.attachmentCount = 1;
		colorBlendInfo.pAttachments = &colorBlendAttachment;
		colorBlendInfo.blendConstants[0] = 0.0f;
		colorBlendInfo.blendConstants[1] = 0.0f;
		colorBlendInfo.blendConstants[2] = 0.0f;
		colorBlendInfo.blendConstants[3] = 0.0f;

		// Dynamic states
		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
		dynamicState.pDynamicStates = dynamicStates.data();

		// Descriptor pool
		VkDescriptorPoolSize poolSizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 10 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10 },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 10 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 10 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 10 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 10 }
		};

		// from IM_ARRAYSIZE (imgui) = ((int)(sizeof(poolSizes) / sizeof(*(poolSizes))));
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = 0;
		poolInfo.poolSizeCount = ((int)(sizeof(poolSizes) / sizeof(*(poolSizes))));
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = 4 * ((int)(sizeof(poolSizes) / sizeof(*(poolSizes))));

		m_DescriptorPools.resize(VulkanConfig::MaxFramesInFlight);
		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPools[i]), "Failed to create descriptor pool!");

		// Descriptor sets
		Ref<VulkanShader> vulkanShader = m_Specification.Shader.As<VulkanShader>();
		const auto& descriptorSetLayouts = vulkanShader->GetDescriptorSetLayouts();

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.pNext = nullptr;

		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
		{
			allocInfo.descriptorPool = m_DescriptorPools[i];

			m_DescriptorSets[i].resize(4);
			for (uint32_t j = 0; j < 4; j++)
			{
				allocInfo.descriptorSetCount = 1;
				allocInfo.pSetLayouts = &descriptorSetLayouts[j];

				VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &m_DescriptorSets[i][j]), "Failed to allocate descriptor set!");
			}
		}

		// Pipeline layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = (uint32_t)m_Specification.PushConstants.size();
		pipelineLayoutInfo.pPushConstantRanges = m_Specification.PushConstants.data();

		VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout), "Failed to create pipeline layout!");

		// Pipeline
		const auto& shader = m_Specification.Shader;

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = (uint32_t)vulkanShader->GetPipelineShaderStageInfos().size();
		pipelineInfo.pStages = vulkanShader->GetPipelineShaderStageInfos().data();
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
		pipelineInfo.pViewportState = &viewportStateInfo;
		pipelineInfo.pRasterizationState = &rasterizerInfo;
		pipelineInfo.pMultisampleState = &multisamplingInfo;
		pipelineInfo.pDepthStencilState = &depthInfo;
		pipelineInfo.pColorBlendState = &colorBlendInfo;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = m_PipelineLayout;
		pipelineInfo.renderPass = m_Specification.Framebuffer.As<VulkanFramebuffer>()->GetRenderPass();
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline), "Failed to create graphics pipeline!");
	}

	VulkanPipeline::~VulkanPipeline()
	{
		EPPO_PROFILE_FUNCTION("VulkanPipeline::~VulkanPipeline");

		Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		vkDestroyPipeline(device, m_Pipeline, nullptr);
		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
	}

	void VulkanPipeline::UpdateDescriptors(uint32_t frameIndex, Ref<UniformBufferSet> uniformBufferSet)
	{
		std::vector<VkWriteDescriptorSet> writeDescriptors;
		{
			VkWriteDescriptorSet& writeDescriptor = writeDescriptors.emplace_back();
			writeDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptor.descriptorCount = 1;
			writeDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDescriptor.dstSet = m_DescriptorSets[frameIndex][0];
			writeDescriptor.dstBinding = 0;
			writeDescriptor.pBufferInfo = &uniformBufferSet->Get(frameIndex, 0, 0).As<VulkanUniformBuffer>()->GetDescriptorBufferInfo();
		}

		{
			VkWriteDescriptorSet& writeDescriptor = writeDescriptors.emplace_back();
			writeDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptor.descriptorCount = 1;
			writeDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDescriptor.dstSet = m_DescriptorSets[frameIndex][0];
			writeDescriptor.dstBinding = 1;
			writeDescriptor.pBufferInfo = &uniformBufferSet->Get(frameIndex, 0, 1).As<VulkanUniformBuffer>()->GetDescriptorBufferInfo();
		}

		Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		vkUpdateDescriptorSets(device, writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

		/*{
			Ref<Image> depthImage = m_ShadowPipeline->GetSpecification().Framebuffer->GetDepthImage();

			VkWriteDescriptorSet& writeDescriptor = writeDescriptors.emplace_back();
			writeDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptor.descriptorCount = 1;
			writeDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptor.dstSet = m_DescriptorSets[frameIndex][0];
			writeDescriptor.dstBinding = 2;
			writeDescriptor.pImageInfo = &depthImage->GetDescriptorImageInfo();
		}*/
	}
}
