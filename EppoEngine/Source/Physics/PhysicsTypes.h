#pragma once

#include <box3d/math_functions.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Eppo::Utils
{
	// glm <-> Box3D. Single precision here, so b3Pos == b3Vec3 and the vec3
	// overload doubles as the position converter. b3Quat is {v, s}, glm is {w,x,y,z}.
	inline auto ToB3(const glm::vec3& v) -> b3Vec3 { return b3Vec3{ v.x, v.y, v.z }; }
	inline auto FromB3(const b3Vec3& v) -> glm::vec3 { return { v.x, v.y, v.z }; }
	inline auto ToB3(const glm::quat& q) -> b3Quat { return b3Quat{ b3Vec3{ q.x, q.y, q.z }, q.w }; }
	inline auto FromB3(const b3Quat& q) -> glm::quat { return { q.s, q.v.x, q.v.y, q.v.z }; }
}
