#include "VulkanKHRRaytracer.h"

#include <chrono>
#include <thread>

#include <imgui.h>
#include <imgui_internal.h>

#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

#include "scene/pipelines/SamplerZooPipeline.h"
#include "scene/SimpleRaytracingPipeling.h"

std::vector<const char*> s_validationLayers({ "VK_LAYER_KHRONOS_validation" });

VulkanKHRRaytracer::VulkanKHRRaytracer() :
	m_reloadScene(true)
{

}

void VulkanKHRRaytracer::start()
{
	m_window.init("Vulkan KHR Raytracer", m_startingWidth, m_startingHeight);

	//Gather required instance extensions
	std::vector<const char*> instanceExtensions = m_window.getRequiredExtensions();

	std::vector<const char*> extraExtensions = m_presenter.determineInstanceExtensions();
	instanceExtensions.insert(instanceExtensions.end(), extraExtensions.begin(), extraExtensions.end());

	//Initialize device
	m_device.createInstance(instanceExtensions, s_validationLayers, true);
	m_device.createSurface(m_window);
	m_device.choosePhysicalDevice();

	//Gather device instance extensions
	std::vector<const char*> deviceExtensions = m_raytracingDevice.getRequiredExtensions();

	extraExtensions = m_presenter.determineDeviceExtensions(m_device.getPhysicalDevice());
	deviceExtensions.insert(deviceExtensions.end(), extraExtensions.begin(), extraExtensions.end());

	//Create logical device
	RaytracingDeviceFeatures* rtFeatures = m_raytracingDevice.init(&m_device);

	m_device.createLogicalDevice(deviceExtensions, s_validationLayers, rtFeatures->pNext);

	delete rtFeatures;

	//Create scene presenter
	m_presenter.init(&m_device, m_window, m_startingWidth, m_startingHeight);

	//Create pipeline
	m_pipeline = new SamplerZooPipeline();

	strcpy(m_scenePath, m_pipeline->getDefaultScene().c_str());

	printf("Starting renderer...\n");

	mainLoop();
}

void VulkanKHRRaytracer::mainLoop()
{
	glm::ivec2 viewportSize = m_window.getViewportSize();

	while (!m_window.isCloseRequested())
	{
		m_window.pollEvents();

		//Check window resize
		glm::ivec2 currentViewport = m_window.getViewportSize();

		if (viewportSize != currentViewport && (currentViewport.x > 0 && currentViewport.y > 0))
		{
			viewportSize = currentViewport;
			m_presenter.resize(viewportSize.x, viewportSize.y);
		}

		drawUI();

		//Update scene
		if (m_reloadScene)
		{
			m_pipeline->destroyRenderTarget();
			m_pipeline->destroy();

			m_scene = SceneLoader::loadScene(&m_raytracingDevice, m_scenePath);

			m_pipeline->init(&m_raytracingDevice, VK_NULL_HANDLE, m_scene);

			m_pipeline->createRenderTarget(m_renderTargetWidth, m_renderTargetHeight);
			m_pipeline->setCameraData(m_scene->cameraPosition, m_scene->cameraRotation);

			m_reloadScene = false;
		}

		VkCommandBuffer commandBuffer = m_presenter.beginFrame();

		m_pipeline->raytrace(commandBuffer);

		VkRect2D renderArea = { { 0, 0 }, { (uint32_t)viewportSize.x, (uint32_t)viewportSize.y } };
		ImageState previousImageState = { VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_IMAGE_LAYOUT_GENERAL };

		m_presenter.endFrame(*m_pipeline, renderArea, previousImageState);

		using namespace std::chrono_literals;
		std::this_thread::sleep_for(1ms);
	}
}

void VulkanKHRRaytracer::drawUI()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if (ImGui::Begin("Frame"))
	{
		//ImGui::SetWindowFontScale(2.0f);

		if (ImGui::CollapsingHeader("Camera Details", 0))
		{
			//Resolution input
			static int dimensions[2] = { m_renderTargetWidth, m_renderTargetHeight };

			ImGui::SetNextItemWidth(150);
			if (ImGui::InputInt2("Resolution", dimensions, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				m_renderTargetWidth = dimensions[0];
				m_renderTargetHeight = dimensions[1];

				m_pipeline->destroyRenderTarget();
				m_pipeline->createRenderTarget(m_renderTargetWidth, m_renderTargetHeight);
			}
			
			//
		}

		if (ImGui::CollapsingHeader("Scene", 0))
		{
			if (ImGui::InputText("Scene path", m_scenePath, sizeof(m_scenePath) / sizeof(m_scenePath[0]) - 1, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				m_reloadScene = true;
			}
		}

		ImGui::End();
	}

	//ImGui::ShowDemoWindow();

	ImGui::Render();
}

void VulkanKHRRaytracer::stop()
{
	if (m_pipeline)
	{
		m_pipeline->destroyRenderTarget();
		m_pipeline->destroy();
	}

	m_scene = nullptr;

	m_presenter.destroy();
	m_device.destroy();

	m_window.terminate();
}

VulkanKHRRaytracer* VulkanKHRRaytracer::create()
{
	return new VulkanKHRRaytracer();
}