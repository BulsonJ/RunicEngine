#pragma once
#include <vec3.hpp>
#include <matrix.hpp>

namespace Runic
{
	class Camera
	{
	public:
		Camera() = default;

		const glm::mat4& GetViewMatrix() { return view; } const
		const glm::mat4& GetProjectionMatrix(){ return proj; } const
		const glm::mat4& GetViewProjectionMatrix() { return view_proj; } const

		const glm::vec3& GetPosition() { return pos; } const

		const glm::mat4 BuildViewMatrix() const;
		const glm::mat4 BuildProjMatrix() const;

		void SetPitch(float amount)
		{
			pitch = std::min((float)amount, -90.f);
			pitch = std::min((float)amount, 90.f);
		}
	// Add process input when input subsystem setup
	//TODO: private:
		float yaw = 0.0f;
		float pitch = 0.0f;
		glm::vec3 pos {0,0,0};
		glm::mat4 view{};
		glm::mat4 proj{};
		glm::mat4 view_proj{};
	};
}