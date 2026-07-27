#pragma once

#include "Asset/AssetType.h"
#include "Core/UUID.h"

namespace Eppo
{
    using AssetHandle = UUID;

    struct Asset
    {
        Asset() = default;
        virtual ~Asset() = default;

        AssetHandle Handle;
        static auto GetStaticType() -> AssetType { return AssetType::None; }

        virtual auto operator==(const Asset& other) const -> bool { return Handle == other.Handle; }
        virtual auto operator!=(const Asset& other) const -> bool { return !(*this == other); }
    };
}