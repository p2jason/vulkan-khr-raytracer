#pragma once

#include "api/Window.h"
#include "api/RenderDevice.h"
#include "api/RaytracingDevice.h"

#include "scene/SceneLoader.h"
#include "scene/ScenePresenter.h"

class VulkanKHRRaytracer
{
private:
	Window m_window;
	int m_startingWidth = 1920;
	int m_startingHeight = 1080;

	RenderDevice m_device;
	RaytracingDevice m_raytracingDevice;

	ScenePresenter m_presenter;

	RaytracingPipeline* m_pipeline = nullptr;
	std::shared_ptr<Scene> m_scene = nullptr;
private:
	VulkanKHRRaytracer();

	void mainLoop();
public:
	VulkanKHRRaytracer(const VulkanKHRRaytracer&) = delete;
	~VulkanKHRRaytracer() {}

	void start();
	void stop();

	VulkanKHRRaytracer& operator=(const VulkanKHRRaytracer&) = delete;

	static VulkanKHRRaytracer* create();
};