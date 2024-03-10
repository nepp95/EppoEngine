#include "pch.h"
#include "OpenGLPipeline.h"

#include "Platform/OpenGL/OpenGLVertexBuffer.h"

#include <glad/glad.h>

namespace Eppo
{
	OpenGLPipeline::OpenGLPipeline(const PipelineSpecification& specification)
		: Pipeline(specification)
	{
		// Create vertex array and bind it
		glCreateVertexArrays(1, &m_RendererID);
		glBindVertexArray(m_RendererID);
	}

	OpenGLPipeline::~OpenGLPipeline()
	{
		glDeleteVertexArrays(1, &m_RendererID);
	}

	void OpenGLPipeline::AddVertexBuffer(Ref<VertexBuffer> vertexBuffer)
	{
		for (const auto& vb : m_VertexBuffers)
		{
			if (vb == vertexBuffer)
				return;
		}

		Bind();
		vertexBuffer.As<OpenGLVertexBuffer>()->Bind();

		// Point the elements in the vertex array to the right element
		for (const auto& element : m_Specification.Layout)
		{
			switch (element.Type)
			{
				case ShaderDataType::Float:
				case ShaderDataType::Float2:
				case ShaderDataType::Float3:
				case ShaderDataType::Float4:
				{
					glEnableVertexAttribArray(m_bufferIndex);
					glVertexAttribPointer(m_bufferIndex,
						element.GetComponentCount(),
						GL_FLOAT,
						element.Normalized ? GL_TRUE : GL_FALSE,
						m_Specification.Layout.GetStride(),
						(const void*)element.Offset
					);
					m_bufferIndex++;
					break;
				}

				case ShaderDataType::Int:
				case ShaderDataType::Int2:
				case ShaderDataType::Int3:
				case ShaderDataType::Int4:
				{
					glEnableVertexAttribArray(m_bufferIndex);
					glVertexAttribIPointer(m_bufferIndex,
						element.GetComponentCount(),
						GL_INT,
						m_Specification.Layout.GetStride(),
						(const void*)element.Offset
					);
					m_bufferIndex++;
					break;
				}

				case ShaderDataType::Bool:
				{
					glEnableVertexAttribArray(m_bufferIndex);
					glVertexAttribIPointer(m_bufferIndex,
						element.GetComponentCount(),
						GL_BOOL,
						m_Specification.Layout.GetStride(),
						(const void*)element.Offset
					);
					m_bufferIndex++;
					break;
				}
			}
		}

		m_VertexBuffers.push_back(vertexBuffer);
	}

	void OpenGLPipeline::Bind() const
	{
		glBindVertexArray(m_RendererID);
	}

	void OpenGLPipeline::UpdateUniforms(Ref<UniformBufferSet> uniformBufferSet)
	{

	}
}
