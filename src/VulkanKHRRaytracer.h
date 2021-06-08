#pragma once

#include "api/Window.h"
#include "api/RenderDevice.h"
#include "api/RaytracingDevice.h"

#include "scene/SceneLoader.h"
#include "scene/ScenePresenter.h"

#include <mutex>

class VulkanKHRRaytracer
{
private:
	Window m_window;
	int m_startingWidth = 1920;
	int m_startingHeight = 1080;

	int m_renderTargetWidth = 1920;
	int m_renderTargetHeight = 1080;

	char m_scenePath[1024] = { 0 };

	RenderDevice m_device;
	RaytracingDevice m_raytracingDevice;

	ScenePresenter m_presenter;

	bool m_showMessageDialog;
	std::string m_errorMessage;

	bool m_changedPipeline = false;
	int m_selectedPipelineIndex = 0;
	RaytracingPipeline* m_pipeline = nullptr;

	bool m_reloadScene = false;
	std::shared_ptr<Scene> m_scene = nullptr;

	std::mutex m_frameLock;
	bool m_skipPipeline = false;
	bool m_showProgressDialog = false;
	std::shared_ptr<SceneLoadProgress> m_sceneProgessTracker = nullptr;
private:
	VulkanKHRRaytracer();

	void loadSceneDeffered();
	void handlePipelineChange();

	void mainLoop();
	void drawUI();
public:
	VulkanKHRRaytracer(const VulkanKHRRaytracer&) = delete;
	~VulkanKHRRaytracer() {}

	void start();
	void stop();

	VulkanKHRRaytracer& operator=(const VulkanKHRRaytracer&) = delete;

	static VulkanKHRRaytracer* create();
};