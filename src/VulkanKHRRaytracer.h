#pragma once

#include "api/Window.h"
#include "api/RenderDevice.h"
#include "api/RaytracingDevice.h"

#include "camera/Camera.h"

#include "scene/SceneLoader.h"
#include "scene/ScenePresenter.h"

#include <mutex>

class VulkanKHRRaytracer
{
private:
	Window m_window;
	int m_startingWidth = 1280;
	int m_startingHeight = 720;

	std::shared_ptr<Camera> m_camera = nullptr;
	int m_renderTargetWidth = 1280;
	int m_renderTargetHeight = 720;

	char m_scenePath[1024] = { 0 };

	RenderDevice m_device;
	RaytracingDevice m_raytracingDevice;

	ScenePresenter m_presenter;

	bool m_showMessageDialog;
	std::string m_errorMessage;

	bool m_changedPipeline = false;
	int m_selectedPipelineIndex = 0;
	RaytracingPipeline* m_pipeline = nullptr;
	std::shared_ptr<void> m_reloadOptions = nullptr;

	VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;

	bool m_autoReloadScene = true;
	bool m_reloadScene = false;
	std::shared_ptr<Scene> m_scene = nullptr;

	std::mutex m_frameLock;
	bool m_skipPipeline = false;
	bool m_showProgressDialog = false;
	std::shared_ptr<SceneLoadProgress> m_sceneProgessTracker = nullptr;
private:
	VulkanKHRRaytracer();

	void loadSceneDeferred();
	void handlePipelineChange();

	void mainLoop();
	void drawUI();
	void showPerformancePopup();
public:
	VulkanKHRRaytracer(const VulkanKHRRaytracer&) = delete;
	~VulkanKHRRaytracer() {}

	void start();
	void stop();

	VulkanKHRRaytracer& operator=(const VulkanKHRRaytracer&) = delete;

	static VulkanKHRRaytracer* create();
};