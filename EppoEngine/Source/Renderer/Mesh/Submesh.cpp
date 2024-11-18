#include "pch.h"
#include "Submesh.h"

#include "Renderer/Renderer.h"

namespace Eppo
{
	Submesh::Submesh(const std::string& name, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<Primitive>& primitives, const glm::mat4& transform)
		: m_Name(name), m_LocalTransform(transform), m_Primitives(primitives)
	{
		m_VertexBuffer = CreateRef<VertexBuffer>((void*)vertices.data(), vertices.size() * sizeof(Vertex));
		m_IndexBuffer = CreateRef<IndexBuffer>((void*)indices.data(), indices.size() * sizeof(uint32_t));
	}

	void Submesh::RT_BindVertexBuffer(Ref<RenderCommandBuffer> renderCommandBuffer) const
	{
		auto instance = this;

		Renderer::SubmitCommand([instance, renderCommandBuffer]()
		{
			VkCommandBuffer commandBuffer = renderCommandBuffer->GetCurrentCommandBuffer();

			VkBuffer vb = { instance->m_VertexBuffer->GetBuffer() };
			VkDeviceSize offsets[] = { 0 };

			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vb, offsets);
		});
	}

	void Submesh::RT_BindIndexBuffer(Ref<RenderCommandBuffer> renderCommandBuffer) const
	{
		auto instance = this;

		Renderer::SubmitCommand([instance, renderCommandBuffer]()
		{
			VkCommandBuffer commandBuffer = renderCommandBuffer->GetCurrentCommandBuffer();
		
			vkCmdBindIndexBuffer(commandBuffer, instance->m_IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
		});
	}
}
