#pragma once

namespace Eppo
{
	class OpenGLQuery
	{
	public:
		OpenGLQuery(uint32_t queryIndex);
		~OpenGLQuery();

		void Begin() const;
		void End() const;

		uint64_t GetQueryResults() const;
		uint32_t GetQueryIndex() const { return m_QueryIndex; }

	private:
		uint32_t m_RendererID;
		uint32_t m_QueryIndex;
	};
}
