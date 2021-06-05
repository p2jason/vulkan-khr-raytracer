#include "Window.h"

#include <iostream>

void errorCallback(int error, const char* description)
{
	std::cout << "GLFW Error: " << description << std::endl;
}

void Window::init(const char* title, int width, int height)
{
	if (!glfwInit())
	{
		FATAL_ERROR("Failed to initialize GLFW!");
	}

	if (!glfwVulkanSupported())
	{
		FATAL_ERROR("Vulkan not supported by GLFW!");
	}

	volkInitialize();

	glfwSetErrorCallback(errorCallback);

	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);

	glfwShowWindow(m_window);
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
		FATAL_ERROR("Failed to create window surface!");
	}

	return surface;
}

void Window::terminate()
{
	glfwDestroyWindow(m_window);
	glfwTerminate();
}