#pragma once

#include <volk.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <glm/glm.hpp>

#include <cstdint>

class Window
{
private:
	GLFWwindow* m_window = nullptr;
public:
	Window() {}

	bool init(const char* title, int width, int height);
	void terminate();

	void pollEvents() const;
	bool isCloseRequested() const;

	glm::ivec2 getViewportSize() const;

	VkSurfaceKHR createSurface(VkInstance instance) const;
	const char** getRequiredExtensions(uint32_t& extensionCount) const;

	inline GLFWwindow* getHandle() const { return m_window; }
};