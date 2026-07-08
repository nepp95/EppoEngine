#include "pch.h"
#include "Math/Math.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>

#include <cmath>

namespace Eppo::Math
{
	auto DecomposeTransform(const glm::mat4& transform, glm::vec3& outTranslation, glm::vec3& outRotation, glm::vec3& outScale) -> bool
	{
		// Adapted from glm::decompose (gtx/matrix_decompose) but returns rotation as
		// Euler angles rather than a quaternion, matching TransformComponent.
		using T = float;
		glm::mat4 m(transform);

		// A zero w breaks the affine assumption below; bail rather than divide by it.
		if (glm::epsilonEqual(m[3][3], T(0), glm::epsilon<T>()))
			return false;

		// Discard any perspective terms: treat the input as affine.
		m[0][3] = m[1][3] = m[2][3] = T(0);
		m[3][3] = T(1);

		outTranslation = glm::vec3(m[3]);
		m[3] = glm::vec4(0, 0, 0, m[3].w);

		glm::vec3 row[3];
		for (int i = 0; i < 3; ++i)
			row[i] = glm::vec3(m[i]);

		// Scale is the length of each basis row; normalise the rows before pulling
		// the rotation out of them.
		outScale.x = glm::length(row[0]);
		if (outScale.x != T(0)) row[0] /= outScale.x;
		outScale.y = glm::length(row[1]);
		if (outScale.y != T(0)) row[1] /= outScale.y;
		outScale.z = glm::length(row[2]);
		if (outScale.z != T(0)) row[2] /= outScale.z;

		// Extract Euler angles from the normalised rotation basis. clamp guards
		// asin against values nudged just outside [-1, 1] by rounding.
		outRotation.y = std::asin(-glm::clamp(row[0][2], T(-1), T(1)));
		if (std::cos(outRotation.y) != T(0))
		{
			outRotation.x = std::atan2(row[1][2], row[2][2]);
			outRotation.z = std::atan2(row[0][1], row[0][0]);
		}
		else
		{
			// Gimbal lock: pitch is +/-90 degrees, fold roll into yaw.
			outRotation.x = std::atan2(-row[2][0], row[1][1]);
			outRotation.z = T(0);
		}

		return true;
	}
}
