#include "Window.h"

#include <iostream>

void errorCallback(int error, const char* description)
{
	std::cout << "GLFW Error: " << description << std::endl;
}

bool Window::init(const char* title, int width, int height)
{
	if (!glfwInit())
	{
		std::cout << "Failed to initialize GLFW!" << std::endl;
		return false;
	}

	if (!glfwVulkanSupported())
	{
		std::cout << "Vulkan not supported by GLFW!" << std::endl;
		return false;
	}

	volkInitialize();

	glfwSetErrorCallback(errorCallback);

	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);

	glfwShowWindow(m_window);

	return true;
}

void Window::pollEvents() const
{
	glfwPollEvents();
}

bool Window::isCloseRequested() const
{
	return glfwWindowShouldClose(m_window);
}

glm::ivec2 Window::getViewportSize() const
{
	int width;
	int height;
	glfwGetFramebufferSize(m_window, &width, &height);

	return glm::ivec2(width, height);
}

std::vector<const char*> Window::getRequiredExtensions() const
{
	uint32_t extensionCount = 0;
	const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);

	return std::vector<const char*>(extensions, extensions + extensionCount);
}

VkSurfaceKHR Window::createSurface(VkInstance instance) const
{
	VkSurfaceKHR surface;
	if (glfwCreateWindowSurface(instance, m_window, nullptr, &surface) != VK_SUCCESS)
	{
		std::cout << "Failed to create window surface!" << std::endl;
	}

	return surface;
}

void Window::terminate()
{
	glfwDestroyWindow(m_window);
	glfwTerminate();
}