#include <iostream>
#include <thread>
#include <chrono>

#include <glm/gtx/transform.hpp>

#include "api/Window.h"
#include "api/RenderDevice.h"
#include "api/RaytracingDevice.h"

#include "scene/ScenePresenter.h"
#include "scene/SceneLoader.h"

#include "scene/pipelines/SamplerZooPipeline.h"

#include <glm/glm.hpp>

#include <glm/gtc/quaternion.hpp>

std::vector<const char*> s_validationLayers({ "VK_LAYER_KHRONOS_validation" });//"VK_LAYER_LUNARG_standard_validation"

int main()
{
	Window window;
	if (!window.init("Vulkan KHR Raytracer", 1920, 1080))
	{
		std::cin.get();
		return 0;
	}

	ScenePresenter presenter;

	std::vector<const char*> glfwExtensions = window.getRequiredExtensions();
	std::vector<const char*> instanceExtensions = presenter.determineInstanceExtensions();
	instanceExtensions.insert(instanceExtensions.end(), glfwExtensions.begin(), glfwExtensions.end());

	//Create render device
	RenderDevice renderDevice;
	renderDevice.createInstance(instanceExtensions, s_validationLayers, true);
	renderDevice.createSurface(window);
	renderDevice.choosePhysicalDevice();

	//Create raytracing device
	RaytracingDevice raytracingDevice;

	std::vector<const char*> deviceExtensions = raytracingDevice.getRequiredExtensions();
	std::vector<const char*> swapchainExtensions = presenter.determineDeviceExtensions(renderDevice.getPhysicalDevice());
	deviceExtensions.insert(deviceExtensions.end(), swapchainExtensions.begin(), swapchainExtensions.end());

	//Create logical device
	RaytracingDeviceFeatures* features = raytracingDevice.init(&renderDevice);

	renderDevice.createLogicalDevice(deviceExtensions, s_validationLayers, features->pNext);

	delete features;

	//Initialize scene presenter
	glm::ivec2 viewportSize = window.getViewportSize();

	presenter.init(&renderDevice, window, viewportSize.x, viewportSize.y);

	//Load scene and created pipeline
	std::shared_ptr<Scene> scene = SceneLoader::loadScene(&raytracingDevice, "C:/Users/Jason/Downloads/shadow_test.glb");

	SamplerZooPipeline pipeline;
	if (!pipeline.init(&raytracingDevice, VK_NULL_HANDLE, scene))
	{
		PAUSE_AND_EXIT(-1);
	}

	pipeline.createRenderTarget(1920, 1080);
	pipeline.setCameraData(scene->cameraPosition, scene->cameraRotation);

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
		presenter.drawUI();

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