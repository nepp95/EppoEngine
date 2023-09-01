#include "pch.h"
#include "VertexBufferLayout.h"

namespace Eppo
{
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
