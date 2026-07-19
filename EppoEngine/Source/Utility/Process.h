#pragma once

#include <string>
#include <vector>

namespace Eppo
{
	// Launches an executable, blocks until it exits and returns its exit code, or
	// -1 if the process could not be started. Output is inherited by this process.
	auto RunProcess(const std::string& executable, const std::vector<std::string>& args) -> int;
}
