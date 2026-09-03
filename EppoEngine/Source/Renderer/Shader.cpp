#include "pch.h"
#include "Renderer/Shader.h"

#include "Platform/ComPtr.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Renderer/DescriptorManager.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Renderer.h"

#if defined(EP_PLATFORM_WINDOWS)
    #include "Platform/DX12/DX12Shader.h"
#endif

#include <dxc/dxcapi.h>

namespace Eppo
{
    namespace
    {
        struct ShaderStage
        {
            nvrhi::ShaderType Type;
            const wchar_t* TargetProfile;
            const char* CacheSuffix;
        };

        constexpr std::array s_ShaderStages{
            ShaderStage{ .Type = nvrhi::ShaderType::Vertex, .TargetProfile = L"vs_6_6", .CacheSuffix = "vert" },
            ShaderStage{ .Type = nvrhi::ShaderType::Pixel,  .TargetProfile = L"ps_6_6", .CacheSuffix = "frag" },
        };

        auto FindStage(const nvrhi::ShaderType type) -> const ShaderStage&
        {
            const auto it = std::ranges::find(s_ShaderStages, type, &ShaderStage::Type);
            EP_ASSERT(it != s_ShaderStages.end());
            return *it;
        }

        auto ShaderStageSuffix(const nvrhi::ShaderType type) -> std::string
        {
            return FindStage(type).CacheSuffix;
        }

        [[nodiscard]] auto ShaderBinaryExtension(const RendererAPI api) -> const char*
        {
            switch (api)
            {
                case RendererAPI::DX12:
                    return "dxil";
                case RendererAPI::Vulkan:
                    return "spv";
                default:
                    EP_ASSERT(false, "No renderer API selected for shader compilation!");
                    return "";
            }
        }

        auto DetectStages(const std::string& source) -> std::vector<nvrhi::ShaderType>
        {
            std::vector<nvrhi::ShaderType> stages;
            for (const auto& stage : s_ShaderStages)
                if (source.find(Utils::ShaderEntryPoint(stage.Type)) != std::string::npos)
                    stages.push_back(stage.Type);
            return stages;
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

                ComPtr<IDxcBlobEncoding> blob;
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
    }

    namespace Utils
    {
        auto NvrhiFormatSize(nvrhi::Format format) -> uint32_t
        {
            switch (format)
            {
                case nvrhi::Format::R32_FLOAT:
                    return 4;
                case nvrhi::Format::RG32_FLOAT:
                    return 4 * 2;
                case nvrhi::Format::RGB32_FLOAT:
                    return 4 * 3;
                case nvrhi::Format::RGBA32_FLOAT:
                    return 4 * 4;
                case nvrhi::Format::R8_UNORM:
                case nvrhi::Format::R8_UINT:
                    return 1;
                case nvrhi::Format::RG8_UNORM:
                    return 1 * 2;
                case nvrhi::Format::RGBA8_UNORM:
                    return 1 * 4;
                case nvrhi::Format::R32_UINT:
                    return 4;

                default:
                {
                    EP_ASSERT(false);
                    return 0;
                }
            }
        }

        auto ShaderEntryPoint(const nvrhi::ShaderType type) -> const char*
        {
            switch (type)
            {
                case nvrhi::ShaderType::Vertex:
                    return "VSMain";
                case nvrhi::ShaderType::Pixel:
                    return "PSMain";
            }

            EP_ASSERT(false);
            return "Main";
        }
    }

    Shader::Shader(ShaderSpecification spec)
        : m_Specification(std::move(spec))
    {
        Log::Info("Loading shader '{}'", m_Specification.Name);

        EP_ASSERT(DeviceManager::Get()->GetParams().API != RendererAPI::None);
        EP_ASSERT(!m_Specification.IsCompute);
    }

    auto Shader::CompileOrGetCache() -> bool
    {
        // A packed source is all a packed shader may read, along with its packed includes; it must not reach
        // the filesystem for either. (The shader cache below is still on disk, but it is this shader's own
        // output, keyed by a hash of the source.)
        if (!m_Specification.Source.empty())
        {
            m_ShaderSource = m_Specification.Source;
        }
        else
        {
            const std::filesystem::path sourcePath = FS::GetResourcesDirectory() / "Shaders" / std::format("{}.hlsl", m_Specification.Name);
            m_ShaderSource = FS::ReadText(sourcePath);
            m_Specification.Includes = ReadIncludesFromDisk();
        }

        const std::vector<nvrhi::ShaderType> stages = DetectStages(m_ShaderSource);
        if (stages.empty())
        {
            Log::Error("Shader '{}' defines no known stage entry points.", m_Specification.Name);
            return false;
        }

        // One source compiles to several stage binaries, so a single hash of that source keys them all.
        const std::string hash = HashSource(m_ShaderSource, m_Specification.Includes);
        const auto extension = ShaderBinaryExtension(DeviceManager::Get()->GetParams().API);
        const std::filesystem::path shaderHashPath =
            FS::GetShaderCacheDirectory() / std::format("{}.{}.hash", m_Specification.Name, extension);

        bool verified = FS::Exists(shaderHashPath) && FS::ReadText(shaderHashPath) == hash;
        for (const auto type : stages)
        {
            const std::filesystem::path shaderBinaryPath =
                FS::GetShaderCacheDirectory() / std::format("{}.{}.{}", m_Specification.Name, ShaderStageSuffix(type), extension);
            if (!FS::Exists(shaderBinaryPath))
                verified = false;
        }

        if (verified)
        {
            Log::Info("Loading shader cache for '{}'", m_Specification.Name);

            for (const auto type : stages)
            {
                const std::filesystem::path shaderBinaryPath =
                    FS::GetShaderCacheDirectory() / std::format("{}.{}.{}", m_Specification.Name, ShaderStageSuffix(type), extension);
                m_ShaderBytes[type] = FS::ReadBytes(shaderBinaryPath);
            }

            return true;
        }

        Log::Info("Compiling shader '{}'", m_Specification.Name);

        for (const auto type : stages)
        {
            if (!Compile(type))
                return false;
        }

        FS::WriteText(shaderHashPath, hash, true);

        return true;
    }

    auto Shader::Compile(const nvrhi::ShaderType type) -> bool
    {
        // Create compiler
        ComPtr<IDxcUtils> utils;
        ComPtr<IDxcCompiler3> compiler;
        if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))) ||
            FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))))
        {
            Log::Error("Could not create the DXC compiler; is dxcompiler.dll present next to the executable?");
            return false;
        }

        // Create include handler. A packed shader gets the pack-backed one and never the default:
        // the default reads from disk, which a deployed game has none of.
        const bool packed = !m_Specification.Source.empty();
        PackedIncludeHandler packedIncludeHandler(utils.Get(), m_Specification.Includes);
        ComPtr<IDxcIncludeHandler> diskIncludeHandler;
        if (!packed)
            utils->CreateDefaultIncludeHandler(&diskIncludeHandler);
        IDxcIncludeHandler* includeHandler = packed ? static_cast<IDxcIncludeHandler*>(&packedIncludeHandler) : diskIncludeHandler.Get();

        // Command line args for compiler. A packed shader is named relative to the virtual Resources/Shaders
        // root, so DXC hands its #include paths to the handler the way the pack keys them.
        const auto shaderFilename = std::format("{}.hlsl", m_Specification.Name);
        const std::wstring shaderPath = packed ? std::filesystem::path(shaderFilename).wstring()
                                               : std::filesystem::path(FS::GetResourcesDirectory() / "Shaders" / shaderFilename).wstring();
        const auto extension = ShaderBinaryExtension(DeviceManager::Get()->GetParams().API);
        const std::wstring binaryPath =
            std::filesystem::path(
                FS::GetShaderCacheDirectory() / std::format("{}.{}.{}", m_Specification.Name, ShaderStageSuffix(type), extension)
            )
                .wstring();

        const std::string entryPointNarrow = Utils::ShaderEntryPoint(type);
        const std::wstring entryPoint(entryPointNarrow.begin(), entryPointNarrow.end());
        std::vector<LPCWSTR> args{ L"-E", entryPoint.c_str(), L"-T", FindStage(type).TargetProfile };
        switch (DeviceManager::Get()->GetParams().API)
        {
            case RendererAPI::Vulkan:
                args.insert(
                    args.end(),
                    {
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
                    }
                );
                break;
            case RendererAPI::DX12:
                args.insert(args.end(), { L"-D", L"TARGET_DX12" });
                break;
            default:
                EP_ASSERT(false, "No renderer api selected!");
                return false;
        }
        args.insert(args.end(), { shaderPath.c_str(), L"-Fo", binaryPath.c_str() });

        DxcBuffer srcBuffer{
            .Ptr = m_ShaderSource.c_str(),
            .Size = m_ShaderSource.size(),
            .Encoding = DXC_CP_UTF8,
        };

        // Execute compiler
        ComPtr<IDxcResult> result;
        if (FAILED(compiler->Compile(&srcBuffer, args.data(), static_cast<uint32_t>(args.size()), includeHandler, IID_PPV_ARGS(&result))) ||
            !result)
        {
            Log::Error("Invoking the compiler for shader '{}' failed!", m_Specification.Name);
            return false;
        }

        ComPtr<IDxcBlobUtf8> errors;
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
        ComPtr<IDxcBlob> binary;
        ComPtr<IDxcBlobWide> binaryName;
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

    auto Shader::GetShaderHandle(const nvrhi::ShaderType type) -> nvrhi::ShaderHandle
    {
        if (const auto it = m_ShaderHandles.find(type); it != m_ShaderHandles.end())
            return it->second;
        return nullptr;
    }

    auto Shader::Create(ShaderSpecification spec) -> Ref<Shader>
    {
        switch (const auto& dm = DeviceManager::Get(); dm->GetParams().API)
        {
#if defined(EP_PLATFORM_WINDOWS)
            case RendererAPI::DX12:
                return CreateRef<DX12Shader>(std::move(spec));
#endif

            case RendererAPI::Vulkan:
                return CreateRef<VulkanShader>(std::move(spec));

            default:
            {
                EP_ASSERT(false);
                return nullptr;
                break;
            }
        }
    }

    auto Shader::IsLoaded() const -> bool
    {
        return m_IsLoaded.load(std::memory_order_acquire);
    }

    auto Shader::CreateShaderHandles() -> void
    {
        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();

        nvrhi::ShaderDesc shaderDesc{};

        for (const auto& [type, bytes] : m_ShaderBytes)
        {
            shaderDesc.shaderType = type;
            shaderDesc.entryName = Utils::ShaderEntryPoint(type);
            m_ShaderHandles[type] = device->createShader(shaderDesc, m_ShaderBytes.at(type).data(), m_ShaderBytes.at(type).size());
        }
    }

    auto Shader::CreateInputLayout() -> void
    {
        const auto& dm = DeviceManager::Get();
        auto device = dm->GetDevice();

        std::vector<nvrhi::VertexAttributeDesc> attributeDescs(m_ShaderInputs.size());
        for (uint32_t i = 0; i < m_ShaderInputs.size(); i++)
        {
            auto& input = m_ShaderInputs.at(i);
            auto& attributeDesc = attributeDescs.at(i);

            attributeDesc.name = input.Name;
            attributeDesc.format = input.Type;
            attributeDesc.arraySize = 1;
            attributeDesc.bufferIndex = 0;
            attributeDesc.offset = input.Offset;
            attributeDesc.elementStride = m_InputAttributeStride;
            attributeDesc.isInstanced = false;
        }

        m_InputLayout = device->createInputLayout(
            attributeDescs.data(), static_cast<uint32_t>(attributeDescs.size()), m_ShaderHandles.at(nvrhi::ShaderType::Vertex)
        );
    }

    auto Shader::CreateBindingLayout() -> void
    {
        const auto& dm = DeviceManager::Get();
        auto device = dm->GetDevice();
        const auto& descriptorManager = dm->GetRenderer()->GetDescriptorManager();
        const auto isGlobalHeap = [this](const uint32_t set, const std::string_view name, const nvrhi::ResourceType type) -> bool
        {
            const auto it = m_ShaderResources.find(set);
            if (it == m_ShaderResources.end())
                return true;

            return it->second.size() == 1 && it->second.front().Name == name && it->second.front().Type == type;
        };

        if (!isGlobalHeap(1, "ResourceDescriptorHeap", nvrhi::ResourceType::Texture_SRV) ||
            !isGlobalHeap(2, "SamplerDescriptorHeap", nvrhi::ResourceType::Sampler))
        {
            Log::Error("Shader '{}' uses descriptor sets reserved for the global bindless heaps!", m_Specification.Name);
            EP_ASSERT(false);
            return;
        }

        for (const auto& [set, setResources] : m_ShaderResources)
        {
            if (set == 1 || set == 2)
                continue;

            nvrhi::BindingLayoutDesc bindingLayoutDesc{
                .visibility = nvrhi::ShaderType::All,
            };

            if (set == 0 && m_HasPushConstants)
                bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::PushConstants(m_PushConstants.Binding, m_PushConstants.Size));

            for (const auto& resource : setResources)
            {
                if (resource.ArraySize == 0)
                {
                    EP_ASSERT(false);
                    continue;
                }

                nvrhi::BindingLayoutItem item{
                    .slot = resource.Binding,
                    .type = resource.Type,
                    .size = static_cast<uint16_t>(resource.ArraySize),
                };

                bindingLayoutDesc.addItem(item);
            }

            if (!bindingLayoutDesc.bindings.empty())
                m_BindingLayouts[set] = device->createBindingLayout(bindingLayoutDesc);
        }

        if (!m_BindingLayouts.contains(0))
        {
            nvrhi::BindingLayoutDesc bindingLayoutDesc{
                .visibility = nvrhi::ShaderType::All,
            };
            if (m_HasPushConstants)
                bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::PushConstants(m_PushConstants.Binding, m_PushConstants.Size));
            m_BindingLayouts[0] = device->createBindingLayout(bindingLayoutDesc);
        }

        EP_ASSERT(!m_BindingLayouts.contains(1));
        EP_ASSERT(!m_BindingLayouts.contains(2));
        m_BindingLayouts[1] = descriptorManager->GetResourceHeap()->BindingLayout;
        m_BindingLayouts[2] = descriptorManager->GetSamplerHeap()->BindingLayout;
    }
}
