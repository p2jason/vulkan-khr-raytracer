#pragma once

#include "api/Window.h"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>

class Camera
{
private:
	Window* m_window;

	glm::vec3 m_position;
	glm::quat m_rotation;

	float m_speed;
	float m_sensitivity;

	bool m_escapePressed = false;
	bool m_isActive = false;
public:
	Camera(Window* window) : m_window(window),
		m_position(), m_rotation(),
		m_speed(12), m_sensitivity(0.075f) {}

	bool update(double delta);

	inline void setPosition(glm::vec3 position) { m_position = position; }
	inline glm::vec3 getPosition() const { return m_position; }

	inline void setRotation(glm::quat rotation) { m_rotation = rotation; }
	inline glm::quat getRotation() const { return m_rotation; }
};