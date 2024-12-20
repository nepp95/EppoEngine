#pragma once

enum VkFormat;

namespace Eppo
{
	enum class ShaderDataType : uint8_t
	{
		None = 0, Float, Float2, Float3, Float4, Mat3, Mat4, Int, Int2, Int3, Int4, Bool
	};

	namespace Utils
	{
		uint32_t ShaderDataTypeSize(ShaderDataType type);
		VkFormat ShaderDataTypeToVkFormat(ShaderDataType type);
	}

	struct BufferElement
	{
		ShaderDataType Type;
		std::string Name;
		uint32_t Size;
		size_t Offset;
		bool Normalized;

		BufferElement() = default;
		BufferElement(const ShaderDataType type, std::string name, const bool normalized = false)
			: Type(type), Name(std::move(name)), Size(Utils::ShaderDataTypeSize(Type)), Offset(0), Normalized(normalized)
		{}

		[[nodiscard]] uint32_t GetComponentCount() const
		{
			switch (Type)
			{
				case ShaderDataType::Float:			return 1;
				case ShaderDataType::Float2:		return 2;
				case ShaderDataType::Float3:		return 3;
				case ShaderDataType::Float4:		return 4;
				case ShaderDataType::Mat3:			return 3;
				case ShaderDataType::Mat4:			return 4;
				case ShaderDataType::Int:			return 1;
				case ShaderDataType::Int2:			return 2;
				case ShaderDataType::Int3:			return 3;
				case ShaderDataType::Int4:			return 4;
				case ShaderDataType::Bool:			return 1;
			}

			EPPO_ASSERT(false)
			return 0;
		}
	};

	class VertexBufferLayout
	{
	public:
		VertexBufferLayout() = default;
		VertexBufferLayout(std::initializer_list<BufferElement> elements);

		uint32_t GetStride() const { return m_Stride; }
		const std::vector<BufferElement>& GetElements() const { return m_Elements; }

		std::vector<BufferElement>::iterator begin() { return m_Elements.begin(); }
		std::vector<BufferElement>::iterator end() { return m_Elements.end(); }
		std::vector<BufferElement>::const_iterator begin() const { return m_Elements.begin(); }
		std::vector<BufferElement>::const_iterator end() const { return m_Elements.end(); }

	private:
		void CalculateOffsetsAndStride();

	private:
		std::vector<BufferElement> m_Elements;
		uint32_t m_Stride = 0;
	};
}
