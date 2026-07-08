#pragma once

#include <glm/glm.hpp>

namespace Eppo::Math
{
	// Decomposes an affine transform matrix into translation, rotation (Euler
	// angles in radians) and scale. Rotation is returned as Euler angles to match
	// TransformComponent's storage, so this is the inverse of
	// TransformComponent::GetTransform for a pure translate/rotate/scale matrix.
	// Returns false if the matrix is degenerate (w == 0). Shear and perspective
	// terms are discarded. Used when reparenting an entity while preserving its
	// world transform, and by viewport gizmos.
	auto DecomposeTransform(const glm::mat4& transform, glm::vec3& outTranslation, glm::vec3& outRotation, glm::vec3& outScale) -> bool;
}
