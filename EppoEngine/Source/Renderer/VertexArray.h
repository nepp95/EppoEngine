#pragma once

#include "Renderer/IndexBuffer.h"
#include "Renderer/VertexBuffer.h"
#include "Renderer/VertexBufferLayout.h"

namespace Eppo
{
	class VertexArray
	{
	public:
		VertexArray(Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer);
		~VertexArray();

		void Bind() const;
		void Unbind() const;

		void SetLayout(const VertexBufferLayout& layout);

		uint32_t GetIndexCount() const;

	private:
		uint32_t m_RendererID;

		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;
	};
}
