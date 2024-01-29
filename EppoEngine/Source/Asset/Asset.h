#pragma once

#include "Asset/AssetType.h"
#include "Core/UUID.h"

namespace Eppo
{
	using AssetHandle = UUID;

	struct Asset : public RefCounter
	{
		AssetHandle Handle;

		static AssetType GetStaticType() { return AssetType::None; }

		virtual bool operator==(const Asset& other) const
		{
			return Handle == other.Handle;
		}

		virtual bool operator!=(const Asset& other) const
		{
			return !(*this == other);
		}
	};
}
