#include "pch.h"
#include "VertexBufferLayout.h"

#include "Platform/Vulkan/Vulkan.h"

namespace Eppo
{
	namespace Utils
	{
		uint32_t ShaderDataTypeSize(ShaderDataType type)
		{
			switch (type)
			{
				case ShaderDataType::Float:			return 4;
				case ShaderDataType::Float2:		return 4 * 2;
				case ShaderDataType::Float3:		return 4 * 3;
				case ShaderDataType::Float4:		return 4 * 4;
				case ShaderDataType::Mat3:			return 4 * 3 * 3;
				case ShaderDataType::Mat4:			return 4 * 4 * 4;
				case ShaderDataType::Int:			return 4;
				case ShaderDataType::Int2:			return 4 * 2;
				case ShaderDataType::Int3:			return 4 * 3;
				case ShaderDataType::Int4:			return 4 * 4;
				case ShaderDataType::Bool:			return 1;
			}

			EPPO_ASSERT(false);
			return 0;
		}

		VkFormat ShaderDataTypeToVkFormat(ShaderDataType type)
		{
			switch (type)
			{
				case ShaderDataType::Float:     return VK_FORMAT_R32_SFLOAT;
				case ShaderDataType::Float2:    return VK_FORMAT_R32G32_SFLOAT;
				case ShaderDataType::Float3:    return VK_FORMAT_R32G32B32_SFLOAT;
				case ShaderDataType::Float4:    return VK_FORMAT_R32G32B32A32_SFLOAT;
					//case ShaderDataType::Mat3:      return 4 * 3 * 3;
					//case ShaderDataType::Mat4:      return 4 * 4 * 4;
				case ShaderDataType::Int:       return VK_FORMAT_R32_SINT;
				case ShaderDataType::Int2:      return VK_FORMAT_R32G32_SINT;
				case ShaderDataType::Int3:      return VK_FORMAT_R32G32B32_SINT;
				case ShaderDataType::Int4:      return VK_FORMAT_R32G32B32A32_SINT;
				case ShaderDataType::Bool:      return VK_FORMAT_R8_UINT;
			}

			EPPO_ASSERT(false);
			return VK_FORMAT_UNDEFINED;
		}
	}

	VertexBufferLayout::VertexBufferLayout(std::initializer_list<BufferElement> elements)
		: m_Elements(elements)
	{
		CalculateOffsetsAndStride();
	}

	void VertexBufferLayout::CalculateOffsetsAndStride()
	{
		size_t offset = 0;
		m_Stride = 0;

		for (auto& element : m_Elements)
		{
			element.Offset = offset;
			offset += element.Size;
			m_Stride += element.Size;
		}
	}
}
