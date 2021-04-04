#include <iostream>
#include <thread>
#include <chrono>

#include "ProjectBase.h"

#include "api/Window.h"
#include "api/RenderDevice.h"
#include "api/RaytracingDevice.h"

#include "scene/ScenePresenter.h"
#include "scene/SceneLoader.h"

#include "scene/pipelines/SamplerZooPipeline.h"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
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
	std::string scenePath = std::string(DATA_DIRECTORY_PATH) + "/scenes/shadow_test.glb";
	std::shared_ptr<Scene> scene = SceneLoader::loadScene(&raytracingDevice, scenePath.c_str());

	SamplerZooPipeline pipeline;
	if (!pipeline.init(&raytracingDevice, VK_NULL_HANDLE, scene))
	{
		PAUSE_AND_EXIT(-1);
	}

	pipeline.createRenderTarget(1920, 1080);
	pipeline.setCameraData(scene->cameraPosition, scene->cameraRotation);

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

		VkCommandBuffer commandBuffer = presenter.beginFrame();

		pipeline.raytrace(commandBuffer);

		VkRect2D renderArea = { { 0, 0 }, { (uint32_t)viewportSize.x, (uint32_t)viewportSize.y } };
		ImageState previousImageState = { VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_IMAGE_LAYOUT_GENERAL };
		
		presenter.endFrame(pipeline, renderArea, previousImageState);

		using namespace std::chrono_literals;
		std::this_thread::sleep_for(1ms);
	}

	pipeline.destroyRenderTarget();
	pipeline.destroy();
	scene = nullptr;

	presenter.destroy();
	renderDevice.destroy();

	window.terminate();

	return 0;
}