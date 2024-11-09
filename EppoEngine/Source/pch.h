#pragma once

#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <random>
#include <string>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(TRACY_ENABLE)
	#include <tracy/Tracy.hpp>
	#include <glad/glad.h>
	#include <tracy/TracyOpenGL.hpp>
#endif

#include "Core/Base.h"
#include "Core/Log.h"
