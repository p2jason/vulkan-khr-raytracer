#pragma once

#include "api/RenderDevice.h"
#include "api/Swapchain.h"

class ScenePresenter
{
private:
	SwapchainFactory m_swapchainFactory;
	SurfaceProfile m_surfaceProfile;
	Swapchain m_swapchain;

	VkDescriptorSetLayout m_descLayout = VK_NULL_HANDLE;
	VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
	VkPipeline m_pipeline = VK_NULL_HANDLE;

	VkRenderPass m_renderPass = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> m_framebuffers;

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
public:
	ScenePresenter() {}

	void init(const RenderDevice* device, int initialWidth, int initialHeight);
	void destroy();

	void resize(int width, int height);

	inline uint32_t acquireNextImage(VkSemaphore semaphore) const { return m_swapchain.acquireNextImage(semaphore); }
	inline void present(VkQueue queue, uint32_t imageIndex, const std::vector<VkSemaphore>& waitSemaphores) const { m_swapchain.present(queue, imageIndex, waitSemaphores); }

	void beginRenderPass(VkCommandBuffer commandBuffer, uint32_t imageIndex, VkRect2D renderArea) const;
	void endRenderPass(VkCommandBuffer commandBuffer) const;
	
	void showRender(VkCommandBuffer commandBuffer) const;

	inline std::vector<const char*> determineDeviceExtensions(VkPhysicalDevice physicalDevice) { return m_swapchainFactory.determineDeviceExtensions(physicalDevice); }
	inline std::vector<const char*> determineInstanceExtensions() { return m_swapchainFactory.determineInstanceExtensions(); }
};