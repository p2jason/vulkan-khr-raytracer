#include "VulkanKHRRaytracer.h"

#include <chrono>
#include <thread>
#include <future>

#include <imgui.h>
#include <imgui_internal.h>

#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

#undef max
#undef min

#include "api/Window.h"

#include "camera/PerspectiveCamera.h"

#include "scene/pipelines/SamplerZooPipeline.h"
#include "scene/pipelines/BasicRaytracingPipeline.h"

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
	m_showMessageDialog(false), m_reloadOptions(nullptr), m_skipPipeline(true)
{
	m_camera = std::make_shared<PerspectiveCamera>(&m_window);
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

	//Create pipeline cache
	VkPipelineCacheCreateInfo pipelineCacheCI = {};
	pipelineCacheCI.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	pipelineCacheCI.initialDataSize = 0;
	pipelineCacheCI.pInitialData = nullptr;

	VK_CHECK(vkCreatePipelineCache(m_device.getDevice(), &pipelineCacheCI, nullptr, &m_pipelineCache));

	//Create camera
	m_camera->init(&m_device);
	m_camera->setRenderTargetSize(m_renderTargetWidth, m_renderTargetHeight);

	//Create pipeline
	m_pipeline = s_pipelineFunctions[m_selectedPipelineIndex]();

	strcpy(m_scenePath, m_pipeline->getDefaultScene().c_str());

	printf("Starting renderer...\n");

	mainLoop();
}

void VulkanKHRRaytracer::handlePipelineChange()
{
	RaytracingPipeline* newPipeline = s_pipelineFunctions[m_selectedPipelineIndex]();
	if (!newPipeline->init(&m_raytracingDevice, m_pipelineCache, m_scene, m_camera, m_reloadOptions))
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
}

void VulkanKHRRaytracer::loadSceneDeferred()
{
	std::shared_ptr<Scene> newScene = SceneLoader::loadScene(&m_raytracingDevice, m_scenePath, m_sceneProgessTracker);

	std::lock_guard<std::mutex> guard(m_frameLock);
	
	if (!newScene)
	{
		m_errorMessage = "Failed to load new scene";
		m_showMessageDialog = true;

		return;
	}

	RaytracingPipeline* newPipeline = s_pipelineFunctions[m_selectedPipelineIndex]();
	if (!newPipeline->init(&m_raytracingDevice, m_pipelineCache, newScene, m_camera, m_reloadOptions))
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

	m_camera->setPosition(m_scene->cameraPosition);
	m_camera->setRotation(m_scene->cameraRotation);

	m_sceneProgessTracker = nullptr;
	m_skipPipeline = false;
}

void VulkanKHRRaytracer::mainLoop()
{
	glm::ivec2 viewportSize = m_window.getViewportSize();

	auto lastTime = std::chrono::high_resolution_clock::now();

	while (!m_window.isCloseRequested())
	{
		m_window.pollEvents();

		//Update camera
		auto currentTime = std::chrono::high_resolution_clock::now();
		double deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime).count() / 1000.0;

		if (!m_skipPipeline && m_camera->update(deltaTime))
		{
			m_pipeline->notifyCameraChange();
		}

		lastTime = currentTime;

		//Check window resize
		glm::ivec2 currentViewport = m_window.getViewportSize();

		if (viewportSize != currentViewport && (currentViewport.x > 0 && currentViewport.y > 0))
		{
			viewportSize = currentViewport;
			m_presenter.resize(viewportSize.x, viewportSize.y);
		}

		std::lock_guard<std::mutex> guard(m_frameLock);

		drawUI();

		//Check reloaded pipeline
		bool checkOutdated = !m_skipPipeline && m_autoReloadScene && m_pipeline->isOutOfDate();

		if (m_pipeline->shouldReload() || checkOutdated)
		{
			m_reloadOptions = m_pipeline->getReloadOptions();

			m_pipeline->notifyReloaded();
			m_changedPipeline = true;
		}

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

			m_sceneProgessTracker = std::make_shared<SceneLoadProgress>();
			m_skipPipeline = true;
			m_showProgressDialog = true;

			std::thread(&VulkanKHRRaytracer::loadSceneDeferred, this).detach();

			m_reloadScene = false;
		}

		VkCommandBuffer commandBuffer = m_presenter.beginFrame();

		if (!m_skipPipeline)
		{
			m_pipeline->raytrace(commandBuffer);
		}

		VkRect2D renderArea = { { 0, 0 }, { (uint32_t)viewportSize.x, (uint32_t)viewportSize.y } };
		ImageState previousImageState = { VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_IMAGE_LAYOUT_GENERAL };

		m_presenter.endFrame(!m_skipPipeline ? m_pipeline : nullptr, renderArea, previousImageState, m_renderTargetWidth, m_renderTargetHeight);
	}
}

void VulkanKHRRaytracer::drawUI()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowSize(ImVec2(342, 432), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Frame"))
	{
		if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::InputText("Scene path", m_scenePath, sizeof(m_scenePath) / sizeof(m_scenePath[0]) - 1, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				m_reloadScene = true;
			}

			bool disabled = m_pipeline == nullptr || m_pipeline->getDefaultScene().empty();

			if (disabled)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}

			if (ImGui::Button("Load default backend scene") && !disabled)
			{
				strncpy(m_scenePath, m_pipeline->getDefaultScene().c_str(), sizeof(m_scenePath) / sizeof(m_scenePath[0]));

				m_reloadScene = true;
			}

			if (disabled)
			{
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
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
				m_reloadOptions = nullptr;
				m_changedPipeline = true;
			}

			ImGui::Checkbox("Auto reload pipeline", &m_autoReloadScene);

			ImGui::Separator();

			ImGui::Text("Description:");
			ImGui::TextWrapped(m_pipeline->getDescription());

			ImGui::Separator();

			ImGui::Text("Options:");
			m_pipeline->drawOptionsUI();

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

				m_camera->setRenderTargetSize(m_renderTargetWidth, m_renderTargetHeight);

				m_pipeline->destroyRenderTarget();
				m_pipeline->createRenderTarget(m_renderTargetWidth, m_renderTargetHeight);
			}
		}

		//Message dialog
		if (m_showMessageDialog)
		{
			ImGui::OpenPopup("Error message");
			m_showMessageDialog = false;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(311, 0));

		if (ImGui::BeginPopupModal("Error message", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextWrapped(m_errorMessage.c_str());

			ImGui::Separator();

			if (ImGui::Button("Close"))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		//Progress dialog
		if (m_showProgressDialog)
		{
			ImGui::OpenPopup("Loading scene");
			m_showProgressDialog = false;
		}

		center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(311, 0));

		if (ImGui::BeginPopupModal("Loading scene", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (m_sceneProgessTracker)
			{
				std::lock_guard<std::mutex> guard(m_sceneProgessTracker->lock);

				ImGui::Text(m_sceneProgessTracker->stageDescription.c_str());
				ImGui::ProgressBar((float)m_sceneProgessTracker->progressStage / m_sceneProgessTracker->numStages);

				ImGui::Spacing();

				ImGui::ProgressBar(m_sceneProgessTracker->stageProgess);

				if (m_sceneProgessTracker->progressStage == m_sceneProgessTracker->numStages)
				{
					ImGui::CloseCurrentPopup();
				}
			}
			else
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	ImGui::End();

	showPerformancePopup();

	ImGui::Render();
}

void VulkanKHRRaytracer::showPerformancePopup()
{
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

	float padding = 10.0f;
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	
	ImVec2 work_pos = viewport->WorkPos;
	ImVec2 work_size = viewport->WorkSize;

	ImVec2 window_pos(work_pos.x + work_size.x - padding, work_pos.y + work_size.y - padding);
	ImVec2 window_pos_pivot(1.0f, 1.0f);

	ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
	ImGui::SetNextWindowBgAlpha(0.35f);

	bool alwaysTrue = true;

	if (ImGui::Begin("Example: Simple overlay", &alwaysTrue, window_flags))
	{
		ImGui::Text("Performance overview");
		ImGui::Separator();

		ImGui::Text("Frame Time: %.1fms", m_presenter.getRenderTime() * 1000.0f);
	}

	ImGui::End();
}

void VulkanKHRRaytracer::stop()
{
	if (m_pipelineCache != VK_NULL_HANDLE)
	{
		vkDestroyPipelineCache(m_device.getDevice(), m_pipelineCache, nullptr);
	}

	if (m_pipeline)
	{
		m_pipeline->destroyRenderTarget();
		m_pipeline->destroy();

		delete m_pipeline;
	}

	m_camera->destroy();

	m_scene = nullptr;

	m_presenter.destroy();
	m_device.destroy();

	m_window.terminate();
}

VulkanKHRRaytracer* VulkanKHRRaytracer::create()
{
	return new VulkanKHRRaytracer();
}