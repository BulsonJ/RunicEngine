#pragma once
#include <vec3.hpp>
#include <matrix.hpp>

namespace Runic
{
	class Camera
	{
	public:
		Camera() = default;

		glm::vec3 GetPosition() const { return m_pos; }

		glm::mat4 BuildViewMatrix() const;
		glm::mat4 BuildProjMatrix() const;

		void SetPitch(float amount)
		{
			m_pitch = std::min((float)amount, -90.f);
			m_pitch = std::min((float)amount, 90.f);
		}
	// Add process input when input subsystem setup
	//TODO: private:
		float m_yaw = 0.0f;
		float m_pitch = 0.0f;
		glm::vec3 m_pos {0,0,0};
	};
}