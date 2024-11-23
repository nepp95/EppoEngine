#pragma once

namespace Eppo
{
	class UniformBuffer
	{
	public:
		virtual ~UniformBuffer() {};

		virtual void SetData(void* data, uint32_t size) = 0;
		virtual uint32_t GetBinding() const = 0;

		static Ref<UniformBuffer> Create(uint32_t size, uint32_t binding);
	};
}
