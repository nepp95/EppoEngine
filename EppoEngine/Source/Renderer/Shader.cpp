#include "pch.h"
#include "Shader.h"

#include "Core/Filesystem.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	namespace Utils
	{
		std::filesystem::path GetOrCreateCacheDirectory()
		{
			if (!Filesystem::Exists("Resources/Shaders/Cache"))
				std::filesystem::create_directories("Resources/Shaders/Cache");

			return "Resources/Shaders/Cache";
		}

		std::string ShaderStageToString(const ShaderStage stage)
		{
			switch (stage)
			{
				case ShaderStage::Vertex:	return "vert";
				case ShaderStage::Fragment:	return "frag";
			}

			EPPO_ASSERT(false)
			return "Invalid";
		}

		ShaderStage StringToShaderStage(const std::string_view stage)
		{
			if (stage == "vert")			return ShaderStage::Vertex;
			if (stage == "frag")			return ShaderStage::Fragment;

			EPPO_ASSERT(false)
			return ShaderStage::None;
		}
	}

	Shader::Shader(ShaderSpecification specification)
		: m_Specification(std::move(specification))
	{
		m_Name = m_Specification.Filepath.stem().string();
	}

	Ref<Shader> Shader::Create(const ShaderSpecification& specification)
	{
		switch (RendererContext::GetAPI())
		{
			case RendererAPI::Vulkan:	return CreateRef<VulkanShader>(specification);
		}

		EPPO_ASSERT(false)
		return nullptr;
	}
}
