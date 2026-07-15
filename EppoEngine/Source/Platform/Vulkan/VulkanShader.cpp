#include "pch.h"
#include "Platform/Vulkan/VulkanShader.h"

#include "Renderer/DeviceManager.h"

#if defined(EP_PLATFORM_WINDOWS)
#include <atlbase.h>
#else
#include <dxc/WinAdapter.h>
#endif

#include <dxc/dxcapi.h>

#include <ranges>
#include <nvrhi/utils.h>
#include <spirv_cross/spirv_cross.hpp>

namespace Eppo
{
	namespace
	{
		auto NvrhiShaderTypeToSuffix(const nvrhi::ShaderType type) -> std::string
		{
			switch (type)
			{
				case nvrhi::ShaderType::Vertex:
					return "vert";
				case nvrhi::ShaderType::Pixel:
					return "frag";
			}

			EP_ASSERT(false);
			return "Unknown";
		}

		auto SpirvTypeToNvrhiType(const std::string& semantic, const spirv_cross::SPIRType& type) -> nvrhi::Format
		{
			using spirv_cross::SPIRType;

			bool packed = semantic.substr(0, 5) == "COLOR";

			switch (type.basetype)
			{
				case SPIRType::BaseType::Float:
				{
					if (type.vecsize == 1)
						return packed ? nvrhi::Format::R8_UNORM : nvrhi::Format::R32_FLOAT;
					if (type.vecsize == 2)
						return packed ? nvrhi::Format::RG8_UNORM : nvrhi::Format::RG32_FLOAT;;
					if (type.vecsize == 3)
						return nvrhi::Format::RGB32_FLOAT;
					if (type.vecsize == 4)
						return packed ? nvrhi::Format::RGBA8_UNORM : nvrhi::Format::RGBA32_FLOAT;

					EP_ASSERT(false);
					return nvrhi::Format::UNKNOWN;
				}

				case SPIRType::BaseType::UInt:
					return nvrhi::Format::R32_UINT;

				case SPIRType::BaseType::Boolean:
					return nvrhi::Format::R8_UINT;

				default:
				{
					EP_ASSERT(false);
					return nvrhi::Format::UNKNOWN;
				}
			}
		}
	}

	VulkanShader::VulkanShader(ShaderSpecification spec)
		: Shader(std::move(spec))
	{
		CompileOrGetCache();
		CreateShaderHandles();

		Log::Info("==================================");
		Log::Info("===== Shader Reflection Data =====");
		Log::Info("==================================");
		Log::Info("Name: {}", m_Specification.Name);

		for (const auto& [type, bytes] : m_ShaderBytes)
			Reflect(type);

		if (m_ShaderBytes.contains(nvrhi::ShaderType::Vertex))
			CreateInputLayout();

		Log::Info("==================================");

		CreateBindingLayout();

		// Release some unneeded memory
		m_ShaderSources.clear();
		m_ShaderBytes.clear();
	}

	auto VulkanShader::CompileOrGetCache() -> void
	{
		const auto& dm = DeviceManager::Get();

		const std::filesystem::path cacheDir = FS::GetShaderCacheDirectory();
		const std::filesystem::path vertPath = FS::GetResourcesDirectory() / "Shaders" / std::format("{}.vert", m_Specification.Name);
		const std::filesystem::path pixelPath = FS::GetResourcesDirectory() / "Shaders" / std::format("{}.frag", m_Specification.Name);
		m_ShaderSources[nvrhi::ShaderType::Vertex] = FS::ReadText(vertPath);
		m_ShaderSources[nvrhi::ShaderType::Pixel] = FS::ReadText(pixelPath);

		bool verified = true;
		for (const auto& [type, source] : m_ShaderSources)
		{
			const std::filesystem::path shaderBinaryPath = FS::GetShaderCacheDirectory() / std::format("{}.{}.spv", m_Specification.Name, NvrhiShaderTypeToSuffix(type));
			const std::filesystem::path shaderHashPath = FS::GetShaderCacheDirectory() / std::format("{}.{}.hash", m_Specification.Name, NvrhiShaderTypeToSuffix(type));
		
			if (FS::Exists(shaderBinaryPath) && FS::Exists(shaderHashPath))
			{
				std::string hash = std::to_string(Hash::GenerateFnv(source));
				std::string cacheHash = FS::ReadText(shaderHashPath);

				if (hash != cacheHash)
					verified = false;
			}
			else
			{
				verified = false;
			}
		}

		if (verified)
		{
			Log::Info("Loading shader cache for '{}'", m_Specification.Name);

			for (const auto& [type, source] : m_ShaderSources)
			{
				const std::filesystem::path shaderBinaryPath = FS::GetShaderCacheDirectory() / std::format("{}.{}.spv", m_Specification.Name, NvrhiShaderTypeToSuffix(type));
				m_ShaderBytes[type] = FS::ReadBytes(shaderBinaryPath);
			}
		}
		else
		{
			Log::Info("Compiling shader '{}'", m_Specification.Name);

			for (const auto& [type, bytes] : m_ShaderSources)
			{
				// Compile shader
				Compile(type);

				// Write shader hash
				const std::filesystem::path shaderHashPath = FS::GetShaderCacheDirectory() / std::format("{}.{}.hash", m_Specification.Name, NvrhiShaderTypeToSuffix(type));
				const std::string hash = std::to_string(Hash::GenerateFnv(m_ShaderSources.at(type)));
				FS::WriteText(shaderHashPath, hash, true);
			}
		}
	}

	auto VulkanShader::Compile(nvrhi::ShaderType type) -> void
	{
		// Create compiler
		CComPtr<IDxcUtils> utils;
		CComPtr<IDxcCompiler3> compiler;
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

		// Create include handler
		CComPtr<IDxcIncludeHandler> includeHandler;
		utils->CreateDefaultIncludeHandler(&includeHandler);

		// Command line args for compiler
		const std::wstring shaderPath = std::filesystem::path(FS::GetResourcesDirectory() / "Shaders" / std::format("{}.{}", m_Specification.Name, NvrhiShaderTypeToSuffix(type))).wstring();
		const std::wstring binaryPath = std::filesystem::path(FS::GetShaderCacheDirectory() / std::format("{}.{}.spv", m_Specification.Name, NvrhiShaderTypeToSuffix(type))).wstring();
		
		const bool isVertex = type == nvrhi::ShaderType::Vertex ? true : false;
		LPCWSTR args[] = {
			L"-E", L"Main",
			L"-T", isVertex ? L"vs_6_0" : L"ps_6_0",
			L"-spirv", L"-fvk-t-shift", L"0", L"0", L"-fvk-s-shift", L"128", L"0", L"-fvk-b-shift", L"256", L"0", L"-fvk-u-shift", L"384", L"0", L"-fspv-reflect",
			L"-D", L"TARGET_VULKAN",
			shaderPath.c_str(),
			L"-Fo", binaryPath.c_str()
		};

		DxcBuffer srcBuffer{
			.Ptr = m_ShaderSources.at(type).c_str(),
			.Size = m_ShaderSources.at(type).size(),
			.Encoding = DXC_CP_UTF8,
		};

		// Execute compiler
		CComPtr<IDxcResult> result;
		compiler->Compile(&srcBuffer, args, _countof(args), includeHandler, IID_PPV_ARGS(&result));

		CComPtr<IDxcBlobUtf8> errors = nullptr;
		result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);

		if (errors != nullptr && errors->GetStringLength() != 0)
		{
			Log::Error("Compiler returned with errors: \n{}", errors->GetStringPointer());

			HRESULT status;
			result->GetStatus(&status);
			if (FAILED(status))
			{
				Log::Error("Compiling failed due to errors!");
				EP_ASSERT(false);
				return;
			}
		}

		// Save shader binary
		CComPtr<IDxcBlob> binary = nullptr;
		CComPtr<IDxcBlobWide> binaryName = nullptr;
		result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&binary), &binaryName);

		if (binary != nullptr)
		{
			const char* pBinary = static_cast<const char*>(binary->GetBufferPointer());
			FS::WriteBytes(binaryPath, pBinary, binary->GetBufferSize(), true);
			m_ShaderBytes[type] = std::vector<char>(pBinary, pBinary + binary->GetBufferSize());
		}
	}

	auto VulkanShader::Reflect(nvrhi::ShaderType type) -> void
	{
		const spirv_cross::Compiler compiler(reinterpret_cast<uint32_t*>(m_ShaderBytes.at(type).data()), m_ShaderBytes.at(type).size() / 4);
		const spirv_cross::ShaderResources resources = compiler.get_shader_resources();
		nvrhi::VulkanBindingOffsets vulkanOffsets{};

		Log::Info("Stage: {}", nvrhi::utils::ShaderStageToString(type));

		if (!resources.stage_inputs.empty() && type == nvrhi::ShaderType::Vertex)
		{
			Log::Info("\tInputs:");

			for (const auto& resource : resources.stage_inputs)
			{
				const auto& bufferType = compiler.get_type(resource.base_type_id);
				const uint32_t location = compiler.get_decoration(resource.id, spv::DecorationLocation);
				const auto& semantic = compiler.get_decoration_string(resource.id, spv::DecorationHlslSemanticGOOGLE);

				auto& input = m_ShaderInputs.emplace_back();
				input.Name = resource.name;
				input.Location = location;
				input.Type = SpirvTypeToNvrhiType(semantic, bufferType);
				input.Offset = m_InputAttributeStride;

				m_InputAttributeStride += Utils::NvrhiFormatSize(input.Type);

				Log::Info("\t\tName: {}", input.Name);
				Log::Info("\t\tLocation: {}", input.Location);
				Log::Info("\t\tType: {}", nvrhi::utils::FormatToString(input.Type));
			}

			// Sort inputs by location
			std::ranges::sort(m_ShaderInputs, std::ranges::less{},
				[](const ShaderInputAttribute& input)
				{
					return input.Location;
				});
		}

		if (!resources.push_constant_buffers.empty())
		{
			const auto& resource = resources.push_constant_buffers[0];
			const auto& bufferType = compiler.get_type(resource.base_type_id);
			const size_t bufferSize = compiler.get_declared_struct_size(bufferType);

			m_PushConstants.Binding = 0;
			m_PushConstants.Size = static_cast<uint32_t>(bufferSize);
			m_PushConstants.Stage = m_HasPushConstants ? nvrhi::ShaderType::All : type;
			m_HasPushConstants = true;
		}

		if (!resources.uniform_buffers.empty())
		{
			Log::Info("Found {} uniform_buffers", resources.uniform_buffers.size());

			for (const auto& resource : resources.uniform_buffers)
			{
				const uint32_t set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				const uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding)  - vulkanOffsets.constantBuffer;

				bool bindingExists = false;
				if (m_ShaderResources.contains(set))
				{
					for (auto& setResource : m_ShaderResources.at(set))
					{
						if (resource.name == setResource.Name && binding == setResource.Binding)
						{
							setResource.Stage = (setResource.Stage | type);
							bindingExists = true;
							break;
						}
					}
				}
				
				if (!bindingExists)
				{
					ShaderResourceBinding& shaderResource = m_ShaderResources[set].emplace_back();
					shaderResource.Name = resource.name;
					shaderResource.Binding = binding;
					shaderResource.Stage = type;
					shaderResource.Type = nvrhi::ResourceType::ConstantBuffer;

					Log::Info("\t\tName: {}", shaderResource.Name);
					Log::Info("\t\tBinding: {} (set: {})", shaderResource.Binding, set);
					Log::Info("\t\tType: {}", nvrhi::utils::ResourceTypeToString(shaderResource.Type));
				}
			}
		}

		if (!resources.separate_images.empty())
		{
			Log::Info("Found {} separate_images", resources.separate_images.size());

			for (const auto& resource : resources.separate_images)
			{
				const uint32_t set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				const uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding) - vulkanOffsets.shaderResource;
				
				auto& spirvType = compiler.get_type(resource.type_id);
				uint32_t arraySize = 1;
				if (!spirvType.array.empty())
					arraySize = spirvType.array[0];

				bool bindingExists = false;
				if (m_ShaderResources.contains(set))
				{
					for (auto& setResource : m_ShaderResources.at(set))
					{
						if (resource.name == setResource.Name && binding == setResource.Binding)
						{
							setResource.Stage = (setResource.Stage | type);
							bindingExists = true;
							break;
						}
					}
				}

				if (!bindingExists)
				{
					ShaderResourceBinding& shaderResource = m_ShaderResources[set].emplace_back();
					shaderResource.Name = resource.name;
					shaderResource.Binding = binding;
					shaderResource.ArraySize = arraySize;
					shaderResource.Stage = type;
					shaderResource.Type = nvrhi::ResourceType::Texture_SRV;

					Log::Info("\t\tName: {}", shaderResource.Name);
					Log::Info("\t\tBinding: {} (set: {})", shaderResource.Binding, set);
					Log::Info("\t\tType: {}", nvrhi::utils::ResourceTypeToString(shaderResource.Type));
				}
			}
		}

		if (!resources.separate_samplers.empty())
		{
			Log::Info("Found {} separate_samplers", resources.separate_samplers.size());

			for (const auto& resource : resources.separate_samplers)
			{
				const uint32_t set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				const uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding) - vulkanOffsets.sampler;
			
				bool bindingExists = false;
				if (m_ShaderResources.contains(set))
				{
					for (auto& setResource : m_ShaderResources.at(set))
					{
						if (resource.name == setResource.Name && binding == setResource.Binding)
						{
							setResource.Stage = (setResource.Stage | type);
							bindingExists = true;
							break;
						}
					}
				}

				if (!bindingExists)
				{
					ShaderResourceBinding& shaderResource = m_ShaderResources[set].emplace_back();
					shaderResource.Name = resource.name;
					shaderResource.Binding = binding;
					shaderResource.Stage = type;
					shaderResource.Type = nvrhi::ResourceType::Sampler;

					Log::Info("\t\tName: {}", shaderResource.Name);
					Log::Info("\t\tBinding: {} (set: {})", shaderResource.Binding, set);
					Log::Info("\t\tType: {}", nvrhi::utils::ResourceTypeToString(shaderResource.Type));
				}
			}
		}

		if (!resources.storage_buffers.empty())
		{
			Log::Info("Found {} storage_buffers", resources.storage_buffers.size());
			
			for (const auto& resource : resources.storage_buffers)
			{
				const uint32_t set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				const uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding) - vulkanOffsets.shaderResource;
			
				bool bindingExists = false;
				if (m_ShaderResources.contains(set))
				{
					for (auto& setResource : m_ShaderResources.at(set))
					{
						if (resource.name == setResource.Name && binding == setResource.Binding)
						{
							setResource.Stage = (setResource.Stage | type);
							bindingExists = true;
							break;
						}
					}
				}

				if (!bindingExists)
				{
					ShaderResourceBinding& shaderResource = m_ShaderResources[set].emplace_back();
					shaderResource.Name = resource.name;
					shaderResource.Binding = binding;
					shaderResource.Stage = type;
					shaderResource.Type = nvrhi::ResourceType::StructuredBuffer_SRV;
				
					Log::Info("\t\tName: {}", shaderResource.Name);
					Log::Info("\t\tBinding: {} (set: {})", shaderResource.Binding, set);
					Log::Info("\t\tType: {}", nvrhi::utils::ResourceTypeToString(shaderResource.Type));
				}
			}
		}

		Log::Trace("Found {} sampled_images", resources.sampled_images.size());
		Log::Trace("Found {} storage_images", resources.storage_images.size());
	}
}