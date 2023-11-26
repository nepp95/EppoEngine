#pragma once

#include "Renderer/Camera/Camera.h"

namespace Eppo
{
	class EditorCamera : public Camera
	{
	public:
		/*EditorCamera(float Fov, float aspectRatio, float nearClip = 0.1f, float farClip = 1000.0f)
			: Camera(Fov, aspectRatio, nearClip, farClip)
		{}*/
		EditorCamera() = default;

	private:

	};
}
