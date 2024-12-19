#pragma once

namespace Eppo
{
	class UUID
	{
	public:
		UUID();
		UUID(uint64_t uuid);
		UUID(const UUID&) = default;

		operator uint64_t() const { return m_UUID; }

	private:
		uint64_t m_UUID;
	};
}

template<>
struct std::hash<Eppo::UUID>
{
	std::size_t operator()(const Eppo::UUID& uuid) const noexcept
	{
		return hash<uint64_t>()(uuid);
	}
};
