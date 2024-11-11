#include "pch.h"
#include "ShaderIncluder.h"

#include "Core/Filesystem.h"

namespace Eppo
{
	shaderc_include_result* ShaderIncluder::GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth)
	{
		// Get requested file path
		std::filesystem::path requestingFile = requesting_source;
		std::filesystem::path requestedFile = requestingFile.parent_path() / std::filesystem::path(requested_source);

		std::string source = Filesystem::ReadText(requestedFile);

		auto dataContainer = new std::array<std::string, 2>;
		(*dataContainer)[0] = requestedFile.string();
		(*dataContainer)[1] = source;

		auto data = new shaderc_include_result;
		data->source_name = (*dataContainer)[0].data();
		data->source_name_length = (*dataContainer)[0].size();
		data->content = (*dataContainer)[1].data();
		data->content_length = (*dataContainer)[1].size();
		data->user_data = dataContainer;

		return data;
	}

	void ShaderIncluder::ReleaseInclude(shaderc_include_result* data)
	{
		delete static_cast<std::array<std::string, 2>*>(data->user_data);
		delete data;
	}
}
