#pragma once

#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Image.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexBufferLayout.h"

namespace Eppo
{
	struct ColorAttachment
	{
		ImageFormat Format;

		bool Clear = true;
		glm::vec4 ClearValue = glm::vec4(0.0f);

		ColorAttachment(ImageFormat format, bool clear = true, const glm::vec4& clearValue = glm::vec4(0.0f))
			: Format(format), Clear(clear), ClearValue(clearValue)
		{}
	};

	enum class PrimitiveTopology
	{
		Lines,
		Triangles
	};

	enum class PolygonMode
	{
		Fill,
		Line
	};

	enum class CullMode
	{
		Back,
		Front,
		FrontAndBack
	};

	enum class CullFrontFace
	{
		Clockwise,
		CounterClockwise
	};

	enum class DepthCompareOp
	{
		Less,
		Equal,
		LessOrEqual,
		Greater,
		NotEqual,
		GreaterOrEqual
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
		bool DepthTesting = false;
		bool DepthCubeMapImage = false;
		bool ClearDepthOnLoad = true;
		float ClearDepth = 1.0f;
		DepthCompareOp DepthCompareOp = DepthCompareOp::Less;
		Ref<Image> DepthImage = nullptr;

		// Color Attachments
		std::vector<ColorAttachment> ColorAttachments;
		Ref<Image> ExistingImage;
	};

	class Pipeline
	{
	public:
		virtual ~Pipeline() {};

		virtual const PipelineSpecification& GetSpecification() const = 0;
		virtual PipelineSpecification& GetSpecification() = 0;

		virtual Ref<Image> GetImage(uint32_t index) const = 0;
		virtual Ref<Image> GetFinalImage() const = 0;

		static Ref<Pipeline> Create(const PipelineSpecification& specification);
	};
}
