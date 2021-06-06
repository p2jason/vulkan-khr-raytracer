#pragma once

#include "api/Window.h"
#include "api/RenderDevice.h"
#include "api/Swapchain.h"

#include "api/RaytracingPipeline.h"

class ScenePresenter
{
private:
	SwapchainFactory m_swapchainFactory;
	SurfaceProfile m_surfaceProfile;
	Swapchain m_swapchain;

	VkDescriptorSetLayout m_descLayout = VK_NULL_HANDLE;
	VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
	VkPipeline m_pipeline = VK_NULL_HANDLE;

	VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

	VkSampler m_sampler = VK_NULL_HANDLE;

	VkRenderPass m_renderPass = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> m_framebuffers;

	VkCommandPool m_commandPool = VK_NULL_HANDLE;
	VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
	VkQueryPool m_queryPool = VK_NULL_HANDLE;

	VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
	VkSemaphore m_renderCompleteSemaphore = VK_NULL_HANDLE;
	VkFence m_renderFinishedFence = VK_NULL_HANDLE;
	uint32_t m_imageIndex = (uint32_t)-1;

	float m_timestampPeriod = 0;
	double m_renderTime = 0;

	int m_width = 0;
	int m_height = 0;

	const RenderDevice* m_device = nullptr;
private:
	void createRenderPass();
	void createPipeline();

	void createSwapchain();
	void destroySwapchain();
	
	void createFramebuffers();
	void destroyFramebuffers();

	void createCommandBuffers();
	void destroyCommandBuffers();

	void initImGui(const Window& window);
	void destroyImGui();
public:
	ScenePresenter() {}

	void init(const RenderDevice* device, const Window& window, int initialWidth, int initialHeight);
	void destroy();

	void resize(int width, int height);

	VkCommandBuffer beginFrame();
	void endFrame(const RaytracingPipeline& pipeline, VkRect2D renderArea, ImageState prevImageState);

	inline std::vector<const char*> determineDeviceExtensions(VkPhysicalDevice physicalDevice) { return m_swapchainFactory.determineDeviceExtensions(physicalDevice); }
	inline std::vector<const char*> determineInstanceExtensions() { return m_swapchainFactory.determineInstanceExtensions(); }
};