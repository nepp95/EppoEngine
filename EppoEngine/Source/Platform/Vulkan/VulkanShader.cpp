#include "pch.h"
#include "Platform/Vulkan/VulkanShader.h"

#include <nvrhi/utils.h>
#include <spirv_cross/spirv_cross.hpp>

namespace Eppo
{
    namespace
    {
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

        m_IsLoaded.store(true, std::memory_order_release);
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
