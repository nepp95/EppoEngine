#pragma once

#include "Renderer/Vulkan.h"

#include <vector>

namespace Eppo
{
	// Descriptor abstraction from https://vkguide.dev/docs/extra-chapter/abstracting_descriptors/
	struct DescriptorLayoutInfo
	{
		std::vector<VkDescriptorSetLayoutBinding> Bindings;

		bool operator==(const DescriptorLayoutInfo& other) const
		{
			if (Bindings.size() != other.Bindings.size())
				return false;

			for (size_t i = 0; i < Bindings.size(); i++)
			{
				if (Bindings[i].binding != other.Bindings[i].binding)
					return false;
				if (Bindings[i].descriptorCount != other.Bindings[i].descriptorCount)
					return false;
				if (Bindings[i].descriptorType != other.Bindings[i].descriptorType)
					return false;
				if (Bindings[i].stageFlags != other.Bindings[i].stageFlags)
					return false;
				// TODO: Check pImmutableSamplers?
			}

			return true;
		}

		size_t hash() const
		{
			size_t result = std::hash<size_t>()(Bindings.size());

			for (const auto& binding : Bindings)
			{
				size_t bindingHash = binding.binding | binding.descriptorType << 8 | binding.descriptorCount << 16 | binding.stageFlags << 24;
				result ^= std::hash<size_t>()(bindingHash);
			}

			return result;
		}
	};

	struct DescriptorLayoutHash
	{
		std::size_t operator()(const DescriptorLayoutInfo& key) const
		{
			return key.hash();
		}
	};
}
