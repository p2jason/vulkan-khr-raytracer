#include <iostream>

#include "api/Window.h"
#include "api/RenderDevice.h"

int main()
{
	Window window;
	if (!window.init("Vulkan KHR Raytracer", 1280, 720))
	{
		std::cin.get();
		return 0;
	}

	//Setup render device
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = window.getRequiredExtensions(glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	std::vector<const char*> validationLayers({ "VK_LAYER_LUNARG_standard_validation", "VK_LAYER_KHRONOS_validation" });

	RenderDevice renderDevice;
	renderDevice.createInstance(extensions, validationLayers, true);
	renderDevice.createSurface(window);
	renderDevice.choosePhysicalDevice();
	renderDevice.createLogicalDevice({ VK_KHR_SWAPCHAIN_EXTENSION_NAME }, validationLayers);


	while (!window.isCloseRequested())
	{
		window.pollEvents();
	}

	renderDevice.destroy();
	window.terminate();

	return 0;
}