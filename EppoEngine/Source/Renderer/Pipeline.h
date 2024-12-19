#pragma once

#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Image.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexBufferLayout.h"

namespace Eppo
{
	struct RenderAttachment
	{
		Ref<Image> RenderImage;
		bool Clear = true;

		union ClearValue
		{
			glm::vec4 Color;
			float Depth = 1.0f;
		} ClearValue;

		explicit RenderAttachment(const Ref<Image>& image, const bool clear = true, const glm::vec4& clearValue = glm::vec4(0.0f))
			: RenderImage(image), Clear(clear)
		{
			ClearValue.Color = clearValue;
		}

		explicit RenderAttachment(const Ref<Image>& image, const bool clear = true, const float clearValue = 1.0f)
			: RenderImage(image), Clear(clear)
		{
			ClearValue.Depth = clearValue;
		}
	};

	enum class PrimitiveTopology : uint8_t
	{
		Lines,
		Triangles
	};

	enum class PolygonMode : uint8_t
	{
		Fill,
		Line
	};

	enum class CullMode : uint8_t
	{
		Back,
		Front,
		FrontAndBack
	};

	enum class CullFrontFace : uint8_t
	{
		Clockwise,
		CounterClockwise
	};

	enum class DepthCompareOp : uint8_t
	{
		Less,
		Equal,
		LessOrEqual,
		Greater,
		NotEqual,
		GreaterOrEqual,

		Always,
		Never
	};

	struct PipelineSpecification
	{
		Ref<Shader> Shader;
		VertexBufferLayout Layout;
		bool SwapchainTarget = false;

		uint32_t Width = 0;
		uint32_t Height = 0;

		// Input Assembly
		PrimitiveTopology Topology = PrimitiveTopology::Triangles;

		// Rasterization
		PolygonMode PolygonMode = PolygonMode::Fill;
		CullMode CullMode = CullMode::Back;
		CullFrontFace CullFrontFace = CullFrontFace::Clockwise;

		// Depth Stencil
		bool CreateDepthImage = false;
		bool TestDepth = false;
		bool WriteDepth = false;
		bool ClearDepthOnLoad = true;
		float ClearDepth = 1.0f;
		DepthCompareOp DepthCompareOp = DepthCompareOp::Less;

		// Render Attachments
		std::vector<RenderAttachment> RenderAttachments;
		bool CubeMap = false;
	};

	class Pipeline
	{
	public:
		virtual ~Pipeline() = default;

		[[nodiscard]] virtual const PipelineSpecification& GetSpecification() const = 0;
		virtual PipelineSpecification& GetSpecification() = 0;

		[[nodiscard]] virtual Ref<Image> GetImage(uint32_t index) const = 0;
		[[nodiscard]] virtual Ref<Image> GetFinalImage() const = 0;

		static Ref<Pipeline> Create(const PipelineSpecification& specification);
	};
}
