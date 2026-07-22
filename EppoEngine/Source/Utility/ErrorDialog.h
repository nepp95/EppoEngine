#pragma once

#include <string_view>

namespace Eppo::ErrorDialog
{
	auto Show(std::string_view title, std::string_view message) -> void;
}
