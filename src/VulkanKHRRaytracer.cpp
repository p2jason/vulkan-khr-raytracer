#include "VulkanKHRRaytracer.h"

#include <chrono>
#include <thread>

#include <imgui.h>
#include <imgui_internal.h>

#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

#include "scene/pipelines/SamplerZooPipeline.h"
#include "scene/SimpleRaytracingPipeling.h"

#undef max
#undef min

typedef RaytracingPipeline* (*PipelineDefFunc)();

template<typename T>
RaytracingPipeline* createPipeline()
{
	return new T();
}

std::vector<const char*> s_validationLayers({ "VK_LAYER_KHRONOS_validation" });

const char* s_pipelineNames[] = {
	"Albedo + Shadow",
	"Sampler Zoo"
};

PipelineDefFunc s_pipelineFunctions[] = {
	&createPipeline<BasicRaytracingPipeline>,
	&createPipeline<SamplerZooPipeline>
};

VulkanKHRRaytracer::VulkanKHRRaytracer() :
	m_reloadScene(true), m_changedPipeline(false), m_selectedPipelineIndex(1),
	m_showMessageDialog(false)
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
	m_pipeline = s_pipelineFunctions[m_selectedPipelineIndex]();

	strcpy(m_scenePath, m_pipeline->getDefaultScene().c_str());

	printf("Starting renderer...\n");

	mainLoop();
}

void VulkanKHRRaytracer::handlePipelineChange()
{
	RaytracingPipeline* newPipeline = s_pipelineFunctions[m_selectedPipelineIndex]();
	if (!newPipeline->init(&m_raytracingDevice, VK_NULL_HANDLE, m_scene))
	{
		m_errorMessage = "Failed to load new raytracing pipeline";
		m_showMessageDialog = true;

		return;
	}

	m_pipeline->destroyRenderTarget();
	m_pipeline->destroy();

	delete m_pipeline;

	m_pipeline = newPipeline;
	m_pipeline->createRenderTarget(m_renderTargetWidth, m_renderTargetHeight);
	m_pipeline->setCameraData(m_scene->cameraPosition, m_scene->cameraRotation);
}

void VulkanKHRRaytracer::handleReloadScene()
{
	std::shared_ptr<Scene> newScene = SceneLoader::loadScene(&m_raytracingDevice, m_scenePath);

	if (!newScene)
	{
		m_errorMessage = "Failed to load new scene";
		m_showMessageDialog = true;

		return;
	}

	RaytracingPipeline* newPipeline = s_pipelineFunctions[m_selectedPipelineIndex]();
	if (!newPipeline->init(&m_raytracingDevice, VK_NULL_HANDLE, newScene))
	{
		m_errorMessage = "Failed to reload raytracing pipeline";
		m_showMessageDialog = true;

		return;
	}

	//Reload pipeline
	m_pipeline->destroyRenderTarget();
	m_pipeline->destroy();

	m_scene = newScene;

	m_pipeline = newPipeline;
	m_pipeline->createRenderTarget(m_renderTargetWidth, m_renderTargetHeight);
	m_pipeline->setCameraData(m_scene->cameraPosition, m_scene->cameraRotation);
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

		//Update pipeline
		if (m_changedPipeline)
		{
			printf("Changing rendering backend to: '%s'\n", s_pipelineNames[m_selectedPipelineIndex]);

			handlePipelineChange();

			m_changedPipeline = false;
		}

		//Update scene
		if (m_reloadScene)
		{
			printf("Changing scene to: '%s'\n", m_scenePath);

			handleReloadScene();

			m_reloadScene = false;
		}

		VkCommandBuffer commandBuffer = m_presenter.beginFrame();

		m_pipeline->raytrace(commandBuffer);

		VkRect2D renderArea = { { 0, 0 }, { (uint32_t)viewportSize.x, (uint32_t)viewportSize.y } };
		ImageState previousImageState = { VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_IMAGE_LAYOUT_GENERAL };

		m_presenter.endFrame(m_pipeline, renderArea, previousImageState);

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
		if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::InputText("Scene path", m_scenePath, sizeof(m_scenePath) / sizeof(m_scenePath[0]) - 1, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				m_reloadScene = true;
			}
		}

		if (ImGui::CollapsingHeader("Rendering Backend", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Button("Reload"))
			{
				m_changedPipeline = true;
			}

			ImGui::SameLine();
			ImGui::SetNextItemWidth(std::max(std::min(200.0f, ImGui::GetContentRegionAvail().x), 100.0f));
			if (ImGui::Combo("Backend", &m_selectedPipelineIndex, s_pipelineNames, sizeof(s_pipelineNames) / sizeof(s_pipelineNames[0])))
			{
				m_changedPipeline = true;
			}

			ImGui::Separator();

			ImGui::Text("Description:");
			ImGui::TextWrapped(m_pipeline->getDescription());
			
			ImGui::Separator();

			if (ImGui::TreeNode("Options"))
			{
				m_pipeline->drawOptionsUI();

				ImGui::TreePop();
			}

			ImGui::Spacing();
		}

		if (ImGui::CollapsingHeader("Camera Details", ImGuiTreeNodeFlags_DefaultOpen))
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
		}

		if (m_showMessageDialog)
		{
			ImGui::OpenPopup("Error Message");
			m_showMessageDialog = false;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(311, 0));//118

		if (ImGui::BeginPopupModal("Error Message", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextWrapped(m_errorMessage.c_str());

			ImGui::Separator();

			if (ImGui::Button("Close"))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
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

		delete m_pipeline;
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