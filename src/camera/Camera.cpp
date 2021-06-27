#include "Camera.h"

bool Camera::update(double delta)
{
	glm::vec3 currentPosition = m_position;
	glm::quat currentRotation = m_rotation;

	//Update state
	if (Window::windowInput.isKeyPressed(GLFW_KEY_ESCAPE))
	{
		if (!m_escapePressed)
		{
			m_isActive = !m_isActive;

			m_window->showCursor(!m_isActive);
		}

		m_escapePressed = true;
	}
	else
	{
		m_escapePressed = false;
	}

	if (!m_isActive)
	{
		return false;
	}

	//Update position
	float moveAmount = delta * m_speed;

	if (Window::windowInput.isKeyPressed(GLFW_KEY_W))
	{
		m_position += m_rotation * glm::vec3(0, 0, moveAmount);
	}

	if (Window::windowInput.isKeyPressed(GLFW_KEY_S))
	{
		m_position += m_rotation * glm::vec3(0, 0, -moveAmount);
	}

	if (Window::windowInput.isKeyPressed(GLFW_KEY_A))
	{
		m_position += m_rotation * glm::vec3(-moveAmount, 0, 0);
	}

	if (Window::windowInput.isKeyPressed(GLFW_KEY_D))
	{
		m_position += m_rotation * glm::vec3(moveAmount, 0, 0);
	}

	if (Window::windowInput.isKeyPressed(GLFW_KEY_SPACE))
	{
		m_position += glm::vec3(0, moveAmount, 0);
	}

	if (Window::windowInput.isKeyPressed(GLFW_KEY_LEFT_SHIFT))
	{
		m_position += glm::vec3(0, -moveAmount, 0);
	}

	//Update rotation
	float xRot = (float)(Window::windowInput.mouseDeltaY * m_sensitivity);
	float yRot = (float)(Window::windowInput.mouseDeltaX * m_sensitivity);

	m_rotation = glm::angleAxis(glm::radians(yRot), glm::vec3(0, 1, 0)) * m_rotation;
	m_rotation *= glm::angleAxis(glm::radians(xRot), glm::vec3(1, 0, 0));

	m_window->setMousePos(m_window->getViewportSize() / 2);

	return currentPosition != m_position || currentRotation != m_rotation;
}