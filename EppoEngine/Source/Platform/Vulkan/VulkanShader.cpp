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

        // Resolves #includes strictly from a packed game. A deployed runtime has no shader files on disk,
        // and must not acquire any: an include that is not in the pack fails the compile instead.
        class PackedIncludeHandler final : public IDxcIncludeHandler
        {
        public:
            PackedIncludeHandler(IDxcUtils* utils, const std::map<std::string, std::string>& includes)
                : m_Utils(utils), m_Includes(includes)
            {}

            auto STDMETHODCALLTYPE LoadSource(LPCWSTR filename, IDxcBlob** includeSource) -> HRESULT override
            {
                if (!includeSource)
                    return E_INVALIDARG;
                *includeSource = nullptr;

                const auto* source = Find(std::filesystem::path(filename).lexically_normal().generic_string());
                if (!source)
                {
                    Log::Error("Packed shader include '{}' is not in the game package.", std::filesystem::path(filename).generic_string());
                    return E_FAIL;
                }

                CComPtr<IDxcBlobEncoding> blob;
                if (FAILED(m_Utils->CreateBlob(source->data(), static_cast<uint32_t>(source->size()), DXC_CP_UTF8, &blob)))
                    return E_FAIL;

                *includeSource = blob.Detach();
                return S_OK;
            }

            auto STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) -> HRESULT override
            {
                if (!object)
                    return E_INVALIDARG;

                if (riid == __uuidof(IDxcIncludeHandler) || riid == __uuidof(IUnknown))
                {
                    *object = static_cast<IDxcIncludeHandler*>(this);
                    AddRef();
                    return S_OK;
                }

                *object = nullptr;
                return E_NOINTERFACE;
            }

            // Scoped to a single Compile call, so reference counting has nothing to manage.
            auto STDMETHODCALLTYPE AddRef() -> ULONG override { return 1; }
            auto STDMETHODCALLTYPE Release() -> ULONG override { return 1; }

        private:
            // DXC resolves an include against the includer's name, so it may arrive prefixed. Keys are relative
            // to Resources/Shaders; take the longest matching suffix, since a shorter one can match a different file.
            [[nodiscard]] auto Find(const std::string& requested) const -> const std::string*
            {
                if (const auto it = m_Includes.find(requested); it != m_Includes.end())
                    return &it->second;

                const std::string* match = nullptr;
                size_t matched = 0;
                for (const auto& [path, source] : m_Includes)
                {
                    if (requested.size() <= path.size() || !requested.ends_with(path) ||
                        requested.at(requested.size() - path.size() - 1) != '/')
                        continue;

                    if (path.size() > matched)
                    {
                        match = &source;
                        matched = path.size();
                    }
                }

                return match;
            }

            IDxcUtils* m_Utils = nullptr;
            const std::map<std::string, std::string>& m_Includes;
        };

        // Includes are part of a shader's compiled result, so the cache key has to cover them too. Lengths are
        // folded in as well, otherwise a boundary can shift between two entries without changing the hash.
        auto HashSource(const std::string& source, const std::map<std::string, std::string>& includes) -> std::string
        {
            std::string combined = std::format("{}:{}", source.size(), source);
            for (const auto& [path, includeSource] : includes)
                combined += std::format("{}:{}{}:{}", path.size(), path, includeSource.size(), includeSource);

            return std::to_string(Hash::GenerateFnv(combined));
        }

        // Kept in step with the include walk in ProjectExporter::Export: if the two sets diverge, the editor
        // and the deployed game hash the same shader differently and every shipped game recompiles on launch.
        auto ReadIncludesFromDisk() -> std::map<std::string, std::string>
        {
            const auto shadersDirectory = FS::GetResourcesDirectory() / "Shaders";
            if (!FS::Exists(shadersDirectory))
                return {};

            std::map<std::string, std::string> includes;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(shadersDirectory))
            {
                if (!entry.is_regular_file() || entry.path().extension() != ".hlsli")
                    continue;

                includes.emplace(std::filesystem::relative(entry.path(), shadersDirectory).generic_string(), FS::ReadText(entry.path()));
            }

            return includes;
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
                        return packed ? nvrhi::Format::RG8_UNORM : nvrhi::Format::RG32_FLOAT;
                    ;
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
        if (!CompileOrGetCache())
        {
            Log::Error("Shader '{}' could not be compiled!", m_Specification.Name);
            EP_ASSERT(false, "Shader compilation failed!");
            return;
        }

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
    }

    auto VulkanShader::CompileOrGetCache() -> bool
    {
        // Packed sources are all a packed shader may read, along with its packed includes; it must not reach
        // the filesystem for either. (The SPIR-V cache below is still on disk, but it is this shader's own
        // output, keyed by a hash of the packed text.)
        if (!m_Specification.Sources.empty())
        {
            m_ShaderSources = m_Specification.Sources;
        }
        else
        {
            const std::filesystem::path vertPath = FS::GetResourcesDirectory() / "Shaders" / std::format("{}.vert", m_Specification.Name);
            const std::filesystem::path pixelPath = FS::GetResourcesDirectory() / "Shaders" / std::format("{}.frag", m_Specification.Name);
            m_ShaderSources[nvrhi::ShaderType::Vertex] = FS::ReadText(vertPath);
            m_ShaderSources[nvrhi::ShaderType::Pixel] = FS::ReadText(pixelPath);
            m_Specification.Includes = ReadIncludesFromDisk();
        }

        bool verified = true;
        for (const auto& [type, source] : m_ShaderSources)
        {
            const std::filesystem::path shaderBinaryPath =
                FS::GetShaderCacheDirectory() / std::format("{}.{}.spv", m_Specification.Name, NvrhiShaderTypeToSuffix(type));
            const std::filesystem::path shaderHashPath =
                FS::GetShaderCacheDirectory() / std::format("{}.{}.hash", m_Specification.Name, NvrhiShaderTypeToSuffix(type));

            if (FS::Exists(shaderBinaryPath) && FS::Exists(shaderHashPath))
            {
                std::string hash = HashSource(source, m_Specification.Includes);
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

            for (const auto& type : m_ShaderSources | std::views::keys)
            {
                const std::filesystem::path shaderBinaryPath =
                    FS::GetShaderCacheDirectory() / std::format("{}.{}.spv", m_Specification.Name, NvrhiShaderTypeToSuffix(type));
                m_ShaderBytes[type] = FS::ReadBytes(shaderBinaryPath);
            }

            return true;
        }

        Log::Info("Compiling shader '{}'", m_Specification.Name);

        for (const auto& type : m_ShaderSources | std::views::keys)
        {
            // Compile shader
            if (!Compile(type))
                return false;

            // Write shader hash
            const std::filesystem::path shaderHashPath =
                FS::GetShaderCacheDirectory() / std::format("{}.{}.hash", m_Specification.Name, NvrhiShaderTypeToSuffix(type));
            const std::string hash = HashSource(m_ShaderSources.at(type), m_Specification.Includes);
            FS::WriteText(shaderHashPath, hash, true);
        }

        return true;
    }

    auto VulkanShader::Compile(const nvrhi::ShaderType type) -> bool
    {
        // Create compiler
        CComPtr<IDxcUtils> utils;
        CComPtr<IDxcCompiler3> compiler;
        if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))) ||
            FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))))
        {
            Log::Error("Could not create the DXC compiler; is dxcompiler.dll present next to the executable?");
            return false;
        }

        // Create include handler. A packed shader gets the pack-backed one and never the default:
        // the default reads from disk, which a deployed game has none of.
        const bool packed = !m_Specification.Sources.empty();
        PackedIncludeHandler packedIncludeHandler(utils, m_Specification.Includes);
        CComPtr<IDxcIncludeHandler> diskIncludeHandler;
        if (!packed)
            utils->CreateDefaultIncludeHandler(&diskIncludeHandler);
        IDxcIncludeHandler* includeHandler = packed ? static_cast<IDxcIncludeHandler*>(&packedIncludeHandler) : diskIncludeHandler.p;

        // Command line args for compiler. A packed shader is named relative to the virtual Resources/Shaders
        // root, so DXC hands its #include paths to the handler the way the pack keys them.
        const auto shaderFilename = std::format("{}.{}", m_Specification.Name, NvrhiShaderTypeToSuffix(type));
        const std::wstring shaderPath = packed ? std::filesystem::path(shaderFilename).wstring()
                                               : std::filesystem::path(FS::GetResourcesDirectory() / "Shaders" / shaderFilename).wstring();
        const std::wstring binaryPath =
            std::filesystem::path(
                FS::GetShaderCacheDirectory() / std::format("{}.{}.spv", m_Specification.Name, NvrhiShaderTypeToSuffix(type))
            )
                .wstring();

        const bool isVertex = type == nvrhi::ShaderType::Vertex ? true : false;
        LPCWSTR args[] = { L"-E",
                           L"Main",
                           L"-T",
                           isVertex ? L"vs_6_6" : L"ps_6_6",
                           L"-spirv",
                           L"-fspv-target-env=vulkan1.3",
                           L"-fvk-t-shift",
                           L"0",
                           L"0",
                           L"-fvk-s-shift",
                           L"128",
                           L"0",
                           L"-fvk-b-shift",
                           L"256",
                           L"0",
                           L"-fvk-u-shift",
                           L"384",
                           L"0",
                           L"-fspv-reflect",
                           L"-fvk-bind-resource-heap",
                           L"0",
                           L"1",
                           L"-fvk-bind-sampler-heap",
                           L"0",
                           L"2",
                           L"-D",
                           L"TARGET_VULKAN",
                           shaderPath.c_str(),
                           L"-Fo",
                           binaryPath.c_str() };

        DxcBuffer srcBuffer{
            .Ptr = m_ShaderSources.at(type).c_str(),
            .Size = m_ShaderSources.at(type).size(),
            .Encoding = DXC_CP_UTF8,
        };

        // Execute compiler
        CComPtr<IDxcResult> result;
        if (FAILED(compiler->Compile(&srcBuffer, args, _countof(args), includeHandler, IID_PPV_ARGS(&result))) || !result)
        {
            Log::Error("Invoking the compiler for shader '{}' failed!", m_Specification.Name);
            return false;
        }

        CComPtr<IDxcBlobUtf8> errors = nullptr;
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);

        if (errors != nullptr && errors->GetStringLength() != 0)
        {
            Log::Error("Compiler returned with errors: \n{}", errors->GetStringPointer());

            HRESULT status;
            result->GetStatus(&status);
            if (FAILED(status))
            {
                Log::Error("Compiling shader '{}' failed due to errors!", m_Specification.Name);
                return false;
            }
        }

        // Save shader binary
        CComPtr<IDxcBlob> binary = nullptr;
        CComPtr<IDxcBlobWide> binaryName = nullptr;
        result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&binary), &binaryName);

        if (binary == nullptr)
        {
            Log::Error("Compiling shader '{}' produced no binary!", m_Specification.Name);
            return false;
        }

        const char* pBinary = static_cast<const char*>(binary->GetBufferPointer());
        FS::WriteBytes(binaryPath, pBinary, binary->GetBufferSize(), true);
        m_ShaderBytes[type] = std::vector<char>(pBinary, pBinary + binary->GetBufferSize());

        return true;
    }

    auto VulkanShader::Reflect(nvrhi::ShaderType type) -> void
    {
        const spirv_cross::Compiler compiler(reinterpret_cast<uint32_t*>(m_ShaderBytes.at(type).data()), m_ShaderBytes.at(type).size() / 4);
        const spirv_cross::ShaderResources resources = compiler.get_shader_resources();
        constexpr nvrhi::VulkanBindingOffsets vulkanOffsets{};

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
            std::ranges::sort(
                m_ShaderInputs, std::ranges::less{},
                [](const ShaderInputAttribute& input)
                {
                    return input.Location;
                }
            );
        }

        if (!resources.push_constant_buffers.empty())
        {
            const auto& resource = resources.push_constant_buffers[0];
            const auto& bufferType = compiler.get_type(resource.base_type_id);
            const size_t bufferSize = compiler.get_declared_struct_size(bufferType);
            const auto pushConstantSize = static_cast<uint32_t>(bufferSize);

            m_PushConstants.Binding = 0;
            if (pushConstantSize > m_PushConstants.Size)
                m_PushConstants.Size = pushConstantSize;
            m_PushConstants.Stage = m_HasPushConstants ? nvrhi::ShaderType::All : type;
            m_HasPushConstants = true;
        }

        if (!resources.uniform_buffers.empty())
        {
            Log::Info("Found {} uniform_buffers", resources.uniform_buffers.size());

            for (const auto& resource : resources.uniform_buffers)
            {
                const uint32_t set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
                const uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding) - vulkanOffsets.constantBuffer;

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
