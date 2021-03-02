#pragma once

#include <vector>
#include <iostream>
#include <functional>
#include <string>

#include <volk.h>

#include "Window.h"

#define PAUSE_AND_EXIT(x) std::cin.get(); std::exit(x);

struct Buffer
{
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
};

struct Image
{
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
};

struct ImageState
{
	VkAccessFlags accessFlags;
	VkPipelineStageFlags stageMask;
	VkImageLayout layout;
};

class RenderDevice
{
private:
	VkInstance m_instance = VK_NULL_HANDLE;
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkPhysicalDeviceMemoryProperties m_memProperties;
	uint32_t m_queueFamilyIndex = (uint32_t)-1;

	VkDevice m_device = VK_NULL_HANDLE;
	VkQueue m_queue = VK_NULL_HANDLE;
	VkCommandPool m_transientPool = VK_NULL_HANDLE;
public:
	RenderDevice() {}

	void createInstance(std::vector<const char*> extensions, std::vector<const char*> validationLayers, bool enableDebugMessenger);
	void createSurface(const Window& window);
	void choosePhysicalDevice();
	void getPhysicalDeviceFeatures(VkPhysicalDeviceFeatures* features, void* pNextChain) const;
	void getPhysicalDevicePropertes(VkPhysicalDeviceProperties* properties, void* pNextChain) const;
	void createLogicalDevice(std::vector<const char*> extensions, std::vector<const char*> validationLayers, void* pNextChain = nullptr, VkPhysicalDeviceFeatures* pFeatures = nullptr);

	VkCommandPool createCommandPool(VkCommandPoolCreateFlags flags = 0) const;
	void submit(const std::vector<VkCommandBuffer>& commandBuffers, const std::vector<std::pair<VkSemaphore, VkPipelineStageFlags>>& waitSemaphores, const std::vector<VkSemaphore> signalSemaphores = {}, VkFence signalFence = VK_NULL_HANDLE) const;

	Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) const;
	VkDeviceAddress getBufferAddress(VkBuffer buffer) const;
	void destroyBuffer(const Buffer& buffer) const;

	Image createImage2D(int width, int height, VkFormat format, int mipLevels, VkImageUsageFlags usage, VkMemoryPropertyFlags properties) const;
	void destroyImage(const Image& image) const;

	void executeCommands(int bufferCount, const std::function<void(VkCommandBuffer*)>& func) const;

	VkShaderModule compileShader(VkShaderStageFlagBits shaderType, const std::string& source) const;

	void destroy();

	inline VkInstance getInstance() const { return m_instance; }
	inline VkDevice getDevice() const { return m_device; }
	inline VkQueue getQueue() const { return m_queue; }

	inline VkSurfaceKHR getSurface() const { return m_surface; }
	inline VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
};

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