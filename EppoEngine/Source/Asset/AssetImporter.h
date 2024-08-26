#pragma once

#include "Asset/AssetMetadata.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Texture.h"
#include "Scene/Scene.h"

namespace Eppo
{
	class AssetImporter
	{
	public:
		static Ref<Asset> ImportAsset(AssetHandle handle, const AssetMetadata& metadata);

		static Ref<Mesh> ImportMesh(AssetHandle handle, const AssetMetadata& metadata);
		static Ref<Scene> ImportScene(AssetHandle handle, const AssetMetadata& metadata);
		static Ref<Texture> ImportTexture(AssetHandle handle, const AssetMetadata& metadata);
	};
}
