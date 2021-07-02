#pragma once

#include "api/Window.h"
#include "api/RenderDevice.h"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>

enum class CameraType
{
	PERSPECTIVE,
	ORTHOGRAPHIC
};

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

	CameraType m_cameraType;

	int m_renderTargetWidth = 0;
	int m_renderTargetHeigth = 0;
public:
	Camera(Window* window, CameraType type) :
		m_window(window), m_cameraType(type),
		m_position(), m_rotation(),
		m_speed(12), m_sensitivity(0.075f) {}

	virtual void init(const RenderDevice* device) {}
	virtual void destroy() {}

	virtual void onUpdated(glm::vec3 position, glm::quat rotation, int width, int heigth) {}

	virtual VkDescriptorBufferInfo getDescriptorInfo() const { return {}; }
	virtual std::string getCameraDefintions() const { return ""; }

	bool update(double delta);

	inline void setRenderTargetSize(int width, int height)
	{
		m_renderTargetWidth = width;
		m_renderTargetHeigth = height;

		onUpdated(m_position, m_rotation, m_renderTargetWidth, m_renderTargetHeigth);
	}

	inline void setPosition(glm::vec3 position)
	{
		m_position = position;

		onUpdated(m_position, m_rotation, m_renderTargetWidth, m_renderTargetHeigth);
	}

	inline void setRotation(glm::quat rotation)
	{
		m_rotation = rotation;

		onUpdated(m_position, m_rotation, m_renderTargetWidth, m_renderTargetHeigth);
	}

	inline glm::vec3 getPosition() const { return m_position; }
	inline glm::quat getRotation() const { return m_rotation; }

	inline CameraType getType() const { return m_cameraType; }
};