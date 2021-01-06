#pragma once

#include <vector>
#include <iostream>

#include <volk.h>

#include "Window.h"

#define PAUSE_AND_EXIT(x) std::cin.get(); std::exit(x);

#define VK_CHECK(x) checkVK(x, __FILE__, __LINE__)

inline bool checkVK(VkResult result, const char* file, unsigned int line)
{
	if (result != VK_SUCCESS)
	{
		const char* resultName = nullptr;
		switch (result)
		{
		case VK_SUCCESS: resultName = "VK_SUCCESS"; break;
		case VK_NOT_READY: resultName = "VK_NOT_READY"; break;
		case VK_TIMEOUT: resultName = "VK_TIMEOUT"; break;
		case VK_EVENT_SET: resultName = "VK_EVENT_SET"; break;
		case VK_EVENT_RESET: resultName = "VK_EVENT_RESET"; break;
		case VK_INCOMPLETE: resultName = "VK_INCOMPLETE"; break;
		case VK_ERROR_OUT_OF_HOST_MEMORY: resultName = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
		case VK_ERROR_OUT_OF_DEVICE_MEMORY: resultName = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
		case VK_ERROR_INITIALIZATION_FAILED: resultName = "VK_ERROR_INITIALIZATION_FAILED"; break;
		case VK_ERROR_DEVICE_LOST: resultName = "VK_ERROR_DEVICE_LOST"; break;
		case VK_ERROR_MEMORY_MAP_FAILED: resultName = "VK_ERROR_MEMORY_MAP_FAILED"; break;
		case VK_ERROR_LAYER_NOT_PRESENT: resultName = "VK_ERROR_LAYER_NOT_PRESENT"; break;
		case VK_ERROR_EXTENSION_NOT_PRESENT: resultName = "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
		case VK_ERROR_FEATURE_NOT_PRESENT: resultName = "VK_ERROR_FEATURE_NOT_PRESENT"; break;
		case VK_ERROR_INCOMPATIBLE_DRIVER: resultName = "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
		case VK_ERROR_TOO_MANY_OBJECTS: resultName = "VK_ERROR_TOO_MANY_OBJECTS"; break;
		case VK_ERROR_FORMAT_NOT_SUPPORTED: resultName = "VK_ERROR_FORMAT_NOT_SUPPORTED"; break;
		case VK_ERROR_FRAGMENTED_POOL: resultName = "VK_ERROR_FRAGMENTED_POOL"; break;
		case VK_ERROR_UNKNOWN: resultName = "VK_ERROR_UNKNOWN"; break;
		case VK_ERROR_OUT_OF_POOL_MEMORY: resultName = "VK_ERROR_OUT_OF_POOL_MEMORY"; break;
		case VK_ERROR_INVALID_EXTERNAL_HANDLE: resultName = "VK_ERROR_INVALID_EXTERNAL_HANDLE"; break;
		case VK_ERROR_FRAGMENTATION: resultName = "VK_ERROR_FRAGMENTATION"; break;
		case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: resultName = "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS"; break;
		case VK_ERROR_SURFACE_LOST_KHR: resultName = "VK_ERROR_SURFACE_LOST_KHR"; break;
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: resultName = "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR"; break;
		case VK_SUBOPTIMAL_KHR: resultName = "VK_SUBOPTIMAL_KHR"; break;
		case VK_ERROR_OUT_OF_DATE_KHR: resultName = "VK_ERROR_OUT_OF_DATE_KHR"; break;
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: resultName = "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR"; break;
		case VK_ERROR_VALIDATION_FAILED_EXT: resultName = "VK_ERROR_VALIDATION_FAILED_EXT"; break;
		case VK_ERROR_INVALID_SHADER_NV: resultName = "VK_ERROR_INVALID_SHADER_NV"; break;
		case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: resultName = "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT"; break;
		case VK_ERROR_NOT_PERMITTED_EXT: resultName = "VK_ERROR_NOT_PERMITTED_EXT"; break;
		case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: resultName = "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT"; break;
		case VK_THREAD_IDLE_KHR: resultName = "VK_THREAD_IDLE_KHR"; break;
		case VK_THREAD_DONE_KHR: resultName = "VK_THREAD_DONE_KHR"; break;
		case VK_OPERATION_DEFERRED_KHR: resultName = "VK_OPERATION_DEFERRED_KHR"; break;
		case VK_OPERATION_NOT_DEFERRED_KHR: resultName = "VK_OPERATION_NOT_DEFERRED_KHR"; break;
		case VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT: resultName = "VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT"; break;
		default: resultName = "Unknown"; break;
		}

		std::cout << "Vulkan Error(" << resultName << "): " << file << " @ " << line << std::endl;

		return false;
	}

	return true;
}

class RenderDevice
{
private:
	VkInstance m_instance = VK_NULL_HANDLE;
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;

	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkPhysicalDeviceMemoryProperties m_memProperties;
	uint32_t m_queueFamilyIndex = (uint32_t)-1;

	VkSurfaceCapabilitiesKHR m_surfaceCapabilities = {};
	VkSurfaceFormatKHR m_swapchainFormat = {};
	VkPresentModeKHR m_presentMode = VK_PRESENT_MODE_FIFO_KHR;;
	VkExtent2D m_swapchainExtent = {};

	VkDevice m_device = VK_NULL_HANDLE;
	VkQueue m_queue = VK_NULL_HANDLE;

	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	std::vector<VkImage> m_swapchainImages;
	std::vector<VkImageView> m_swapchainImageViews;
public:
	RenderDevice() {}

	void createInstance(std::vector<const char*> extensions, std::vector<const char*> validationLayers, bool enableDebugMessenger);
	void createSurface(const Window& window);
	void choosePhysicalDevice();
	void createLogicalDevice(std::vector<const char*> extensions, std::vector<const char*> validationLayers);
	void createSwapchain(int width, int height);

	VkCommandPool createCommandPool(VkCommandPoolCreateFlags flags = 0) const;
	uint32_t acquireNextImage(VkSemaphore semaphore) const;
	void submit(const std::vector<VkCommandBuffer>& commandBuffers, const std::vector<std::pair<VkSemaphore, VkPipelineStageFlags>>& waitSemaphores, const std::vector<VkSemaphore> signalSemaphores = {}, VkFence signalFence = VK_NULL_HANDLE) const;
	void present(uint32_t imageIndex, const std::vector<VkSemaphore>& waitSemaphores) const;

	void destroySwapchain();
	void destroy();

	inline VkDevice getDevice() const { return m_device; }
	inline VkQueue getQueue() const { return m_queue; }
	inline VkSurfaceFormatKHR getSurfaceFormat() const { return m_swapchainFormat; }
	inline const std::vector<VkImage>& getSwapchainImages() const { return m_swapchainImages; }
	inline const std::vector<VkImageView>& getSwapchainImageViews() const { return m_swapchainImageViews; }
};