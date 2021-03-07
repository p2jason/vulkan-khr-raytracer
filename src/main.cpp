#include <iostream>
#include <thread>
#include <chrono>

#include <volk.h>
#include <vulkan/vulkan.hpp>

#include <glm/gtx/transform.hpp>

#include "api/Window.h"
#include "api/RenderDevice.h"
#include "api/RaytracingDevice.h"

#include "scene/ScenePresenter.h"
#include "scene/SceneLoader.h"

#include "scene/SimpleRaytracingPipeling.h"

#include <glm/glm.hpp>

#include <glm/gtc/quaternion.hpp>

int main()
{
	Window window;
	if (!window.init("Vulkan KHR Raytracer", 2560, 1440))
	{
		std::cin.get();
		return 0;
	}

	//Setup render device
	ScenePresenter presenter;

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = window.getRequiredExtensions(glfwExtensionCount);

	std::vector<const char*> instanceExtensions = presenter.determineInstanceExtensions();
	instanceExtensions.insert(instanceExtensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);

	std::vector<const char*> validationLayers({ "VK_LAYER_KHRONOS_validation" });//"VK_LAYER_LUNARG_standard_validation"

	glm::ivec2 viewportSize = window.getViewportSize();

	RaytracingDevice raytracingDevice;

	std::vector<const char*> deviceExtensions = raytracingDevice.getRequiredExtensions();
	deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	RenderDevice renderDevice;
	renderDevice.createInstance(instanceExtensions, validationLayers, true);
	renderDevice.createSurface(window);
	renderDevice.choosePhysicalDevice();

	RaytracingDeviceFeatures* features = raytracingDevice.init(&renderDevice);

	std::vector<const char*> swapchainExtensions = presenter.determineDeviceExtensions(renderDevice.getPhysicalDevice());
	deviceExtensions.insert(deviceExtensions.end(), swapchainExtensions.begin(), swapchainExtensions.end());

	renderDevice.createLogicalDevice(deviceExtensions, validationLayers, features->pNext);

	delete features;

	presenter.init(&renderDevice, viewportSize.x, viewportSize.y);

	//Create command pool & buffer
	VkDevice device = renderDevice.getDevice();
	VkCommandPool commandPool = renderDevice.createCommandPool();

	VkCommandBuffer commandBuffer;
	{
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		
		VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));
	}

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderCompleteSemaphore;
	VkFence renderFinishedFence;

	{
		VkSemaphoreCreateInfo semaphoreCI = {};
		semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		
		VK_CHECK(vkCreateSemaphore(device, &semaphoreCI, nullptr, &imageAvailableSemaphore));
		VK_CHECK(vkCreateSemaphore(device, &semaphoreCI, nullptr, &renderCompleteSemaphore));

		VkFenceCreateInfo fenceCI = {};
		fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		
		VK_CHECK(vkCreateFence(device, &fenceCI, nullptr, &renderFinishedFence));
	}

	std::shared_ptr<Scene> scene = SceneLoader::loadScene(&raytracingDevice, "C:/Users/Jason/Downloads/sponza.glb");

	BasicRaytracingPipeline pipeline;
	if (!pipeline.init(&raytracingDevice, VK_NULL_HANDLE, scene))
	{
		PAUSE_AND_EXIT(-1);
	}

	pipeline.createRenderTarget(2560, 1440);
	pipeline.setCameraData(glm::vec3(0, 6, 0), glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0)));

	while (!window.isCloseRequested())
	{
		window.pollEvents();

		glm::ivec2 currentViewport = window.getViewportSize();

		//Check resize
		if (viewportSize != currentViewport && (currentViewport.x > 0 && currentViewport.y > 0))
		{
			viewportSize = currentViewport;
			presenter.resize(viewportSize.x, viewportSize.y);
		}

		//Render
		uint32_t imageIndex = presenter.acquireNextImage(imageAvailableSemaphore);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pInheritanceInfo = nullptr;

		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		pipeline.raytrace(commandBuffer);

		VkRect2D renderArea = { { 0, 0 }, { (uint32_t)viewportSize.x, (uint32_t)viewportSize.y } };
		ImageState previousImageState = { VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_IMAGE_LAYOUT_GENERAL };
		
		presenter.showRender(commandBuffer, pipeline, imageIndex, renderArea, previousImageState, VK_IMAGE_LAYOUT_GENERAL);
		
		VK_CHECK(vkEndCommandBuffer(commandBuffer));

		renderDevice.submit({ commandBuffer }, { { imageAvailableSemaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT } }, { renderCompleteSemaphore }, renderFinishedFence);

		presenter.present(renderDevice.getQueue(), imageIndex, { renderCompleteSemaphore });

		VK_CHECK(vkWaitForFences(device, 1, &renderFinishedFence, VK_TRUE, UINT64_MAX));
		VK_CHECK(vkResetFences(device, 1, &renderFinishedFence));
		VK_CHECK(vkResetCommandPool(device, commandPool, 0));

		using namespace std::chrono_literals;
		std::this_thread::sleep_for(1ms);
	}

	pipeline.destroyRenderTarget();
	pipeline.destroy();
	scene = nullptr;

	vkDestroyFence(device, renderFinishedFence, nullptr);
	vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(device, renderCompleteSemaphore, nullptr);

	vkDestroyCommandPool(device, commandPool, nullptr);

	presenter.destroy();
	renderDevice.destroy();

	window.terminate();

	return 0;
}