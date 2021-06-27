#include "Window.h"

#include <iostream>

void errorCallback(int error, const char* description)
{
	std::cout << "GLFW Error: " << description << std::endl;
}

WindowInput Window::windowInput = {};

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

	glfwSetWindowUserPointer(m_window, &windowInput);

	glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		WindowInput* input = (WindowInput*)glfwGetWindowUserPointer(window);

		if (action != GLFW_RELEASE)
		{
			input->pressedKeys.insert(key);
		}
		else
		{
			input->pressedKeys.erase(key);
		}
	});

	glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos)
	{
		WindowInput* input = (WindowInput*)glfwGetWindowUserPointer(window);
		input->mouseDeltaX = xpos - input->mouseX;
		input->mouseDeltaY = ypos - input->mouseY;

		input->mouseX = xpos;
		input->mouseY = ypos;
	});

	glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods)
	{
		WindowInput* input = (WindowInput*)glfwGetWindowUserPointer(window);

		if (action != GLFW_RELEASE)
		{
			input->pressedMouseButtons.insert(button);
		}
		else
		{
			input->pressedMouseButtons.erase(button);
		}
	});

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

void Window::showCursor(bool show)
{
	glfwSetInputMode(m_window, GLFW_CURSOR, show ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
}

void Window::setMousePos(glm::vec2 position)
{
	windowInput.mouseX = position.x;
	windowInput.mouseY = position.y;

	glfwSetCursorPos(m_window, position.x, position.y);
}

void Window::terminate()
{
	glfwDestroyWindow(m_window);
	glfwTerminate();
}