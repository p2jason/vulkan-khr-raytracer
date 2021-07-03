#pragma once

#include <volk.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#undef min
#undef max

#include <glm/glm.hpp>

#include <vector>
#include <unordered_set>
#include <cstdint>

#include "Common.h"

class WindowInput
{
public:
	double mouseX;
	double mouseY;

	double mouseDeltaX;
	double mouseDeltaY;

	std::unordered_set<int> pressedKeys;
	std::unordered_set<int> pressedMouseButtons;
public:
	inline bool isKeyPressed(int key) const { return pressedKeys.find(key) != pressedKeys.end(); }
	inline bool isMouseButton(int button) const { return pressedMouseButtons.find(button) != pressedMouseButtons.end(); }
};

class Window
{
private:
	GLFWwindow* m_window = nullptr;
public:
	static WindowInput windowInput;
public:
	Window() {}

	void init(const char* title, int width, int height);
	void terminate();

	void pollEvents() const;
	bool isCloseRequested() const;

	glm::ivec2 getViewportSize() const;

	VkSurfaceKHR createSurface(VkInstance instance) const;
	std::vector<const char*> getRequiredExtensions() const;

	void showCursor(bool show);
	void setMousePos(glm::vec2 position);

	inline GLFWwindow* getHandle() const { return m_window; }
};