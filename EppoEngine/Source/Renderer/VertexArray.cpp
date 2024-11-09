#include "pch.h"
#include "VertexArray.h"

#include <glad/glad.h>

namespace Eppo
{
	namespace Utils
	{
		static GLenum ShaderDataTypeToOpenGLType(ShaderDataType type)
		{
			switch (type)
			{
				case ShaderDataType::Float:		return GL_FLOAT;
				case ShaderDataType::Float2:	return GL_FLOAT;
				case ShaderDataType::Float3:	return GL_FLOAT;
				case ShaderDataType::Float4:	return GL_FLOAT;
				case ShaderDataType::Mat3:		return GL_FLOAT;
				case ShaderDataType::Mat4:		return GL_FLOAT;
				case ShaderDataType::Int:		return GL_INT;
				case ShaderDataType::Int2:		return GL_INT;
				case ShaderDataType::Int3:		return GL_INT;
				case ShaderDataType::Int4:		return GL_INT;
				case ShaderDataType::Bool:		return GL_BOOL;
			}

			EPPO_ASSERT(false);
			return 0;
		}
	}

	VertexArray::VertexArray(Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer)
		: m_VertexBuffer(vertexBuffer), m_IndexBuffer(indexBuffer)
	{
		glCreateVertexArrays(1, &m_RendererID);
	}

	VertexArray::~VertexArray()
	{
		glDeleteVertexArrays(1, &m_RendererID);
	}

	void VertexArray::Bind() const
	{
		glBindVertexArray(m_RendererID);
	}

	void VertexArray::Unbind() const
	{
		glBindVertexArray(0);
	}

	void VertexArray::SetLayout(const VertexBufferLayout& layout)
	{
		EPPO_PROFILE_FUNCTION("VertexArray::SetLayout");

		Bind();
		m_IndexBuffer->Bind();
		m_VertexBuffer->Bind();

		uint32_t index = 0;
		for (const auto& element : layout)
		{
			switch (element.Type)
			{
				case ShaderDataType::Float:
				case ShaderDataType::Float2:
				case ShaderDataType::Float3:
				case ShaderDataType::Float4:
				{
					glEnableVertexAttribArray(index);
					glVertexAttribPointer(index,
						element.GetComponentCount(),
						Utils::ShaderDataTypeToOpenGLType(element.Type),
						element.Normalized ? GL_TRUE : GL_FALSE,
						layout.GetStride(),
						(const void*)element.Offset
					);

					index++;

					break;
				}

				case ShaderDataType::Int:
				case ShaderDataType::Int2:
				case ShaderDataType::Int3:
				case ShaderDataType::Int4:
				case ShaderDataType::Bool:
				{
					glEnableVertexAttribArray(index),
					glVertexAttribIPointer(index,
						element.GetComponentCount(),
						Utils::ShaderDataTypeToOpenGLType(element.Type),
						layout.GetStride(),
						(const void*)element.Offset
					);

					index++;

					break;
				}

				case ShaderDataType::Mat3:
				case ShaderDataType::Mat4:
				{
					uint32_t count = element.GetComponentCount();

					for (uint32_t i = 0; i < count; i++)
					{
						glEnableVertexAttribArray(index);
						glVertexAttribPointer(index,
							count,
							Utils::ShaderDataTypeToOpenGLType(element.Type),
							element.Normalized ? GL_TRUE : GL_FALSE,
							layout.GetStride(),
							(const void*)(element.Offset + (sizeof(float) * count * i))
						);

						index++;
					}

					break;
				}
			}
		}
	}

	uint32_t VertexArray::GetIndexCount() const
	{
		return m_IndexBuffer->GetIndexCount();
	}
}
