#include "RenderDevice.h"

#include <iostream>
#include <algorithm>

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	int minSeveritry = 2;

	switch (messageSeverity)
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		if (minSeveritry <= 0)
		{
			std::cout << "Vulkan Info: " << pCallbackData->pMessage << std::endl;
		}
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		if (minSeveritry <= 1)
		{
			if (!!strcmp(pCallbackData->pMessage, "Added messenger"))
			{
				std::cout << "Vulkan Verbose: " << pCallbackData->pMessage << std::endl;
			}
		}
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		if (minSeveritry <= 2)
		{
			std::cout << "Vulkan Warning: " << pCallbackData->pMessage << std::endl;
		}
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		if (minSeveritry <= 3)
		{
			std::cout << "Vulkan Error: " << pCallbackData->pMessage << std::endl;
		}
		break;
	}

	return VK_FALSE;
}

void RenderDevice::createInstance(std::vector<const char*> extensions, std::vector<const char*> validationLayers, bool enableDebugMessenger)
{
	if (enableDebugMessenger)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	//Check extenion support
	uint32_t extensionCount = 0;
	VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));

	VkExtensionProperties* instanceExtensions = new VkExtensionProperties[extensionCount];
	VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, instanceExtensions));

	for (size_t i = 0; i < extensions.size(); ++i)
	{
		bool found = false;

		for (uint32_t j = 0; j < extensionCount; ++j)
		{
			if (!strcmp(extensions[i], instanceExtensions[j].extensionName))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			std::cout << "Cannot find extension: " << extensions[i] << std::endl;
			PAUSE_AND_EXIT(-1)
		}
	}

	//Check layer support
	uint32_t layerCount = 0;
	VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));

	VkLayerProperties* instanceLayers = new VkLayerProperties[layerCount];
	VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, instanceLayers));

	for (size_t i = 0; i < validationLayers.size(); ++i)
	{
		bool found = false;

		for (uint32_t j = 0; j < layerCount; ++j)
		{
			if (!strcmp(validationLayers[i], instanceLayers[j].layerName))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			std::cout << "Cannot find layer: " << validationLayers[i] << std::endl;
			PAUSE_AND_EXIT(-1)
		}
	}

	//Create instance
	VkApplicationInfo applicationInfo = {};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pApplicationName = "Vulkan KHR Raytracer";
	applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.pEngineName = "Vulkan KHR Raytracer";
	applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.apiVersion = VK_API_VERSION_1_2;

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
	debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugCreateInfo.pfnUserCallback = debugCallback;
	debugCreateInfo.pUserData = nullptr;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext = enableDebugMessenger ? &debugCreateInfo : nullptr;
	createInfo.pApplicationInfo = &applicationInfo;
	createInfo.ppEnabledExtensionNames = extensions.data();
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.ppEnabledLayerNames = validationLayers.data();
	createInfo.enabledLayerCount = (uint32_t)validationLayers.size();

	VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance));

	volkLoadInstance(m_instance);
}

void RenderDevice::createSurface(const Window& window)
{
	m_surface = window.createSurface(m_instance);
}

void RenderDevice::choosePhysicalDevice()
{
	uint32_t count = 0;
	VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &count, nullptr));

	VkPhysicalDevice* physicalDevices = new VkPhysicalDevice[count];
	VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &count, physicalDevices));

	VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
	for (uint32_t i = 0; i < count; ++i)
	{
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(physicalDevices[i], &properties);

		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceFeatures(physicalDevices[i], &features);

		bestDevice = physicalDevices[i];
	}

	delete[] physicalDevices;

	if (bestDevice == VK_NULL_HANDLE)
	{
		std::cout << "Cannot find adequate GPU!" << std::endl;
		PAUSE_AND_EXIT(-1)
	}

	m_physicalDevice = bestDevice;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memProperties);

	//Choose swapchain present mode
	uint32_t presentModeCount = 0;
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr));

	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, presentModes.data()));

	VkPresentModeKHR scPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (VkPresentModeKHR mode : presentModes)
	{
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			scPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
	}

	m_presentMode = scPresentMode;

	//Choose swapchain format
	uint32_t formatCount = 0;
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr));

	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, surfaceFormats.data()));

	VkSurfaceFormatKHR scFormat = surfaceFormats[0];
	for (VkSurfaceFormatKHR format : surfaceFormats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			scFormat = format;
			break;
		}
	}

	m_swapchainFormat = scFormat;
}

void RenderDevice::createLogicalDevice(std::vector<const char*> extensions, std::vector<const char*> validationLayers)
{
	//Check extension support
	uint32_t extensionCount = 0;
	VK_CHECK(vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionCount, nullptr));

	VkExtensionProperties* instanceExtensions = new VkExtensionProperties[extensionCount];
	VK_CHECK(vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionCount, instanceExtensions));

	for (size_t i = 0; i < extensions.size(); ++i)
	{
		bool found = false;

		for (uint32_t j = 0; j < extensionCount; ++j)
		{
			if (!strcmp(extensions[i], instanceExtensions[j].extensionName))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			std::cout << "Cannot find extension: " << extensions[i] << std::endl;
			PAUSE_AND_EXIT(-1)
		}
	}

	//Check layer support
	uint32_t layerCount = 0;
	VK_CHECK(vkEnumerateDeviceLayerProperties(m_physicalDevice, &layerCount, nullptr));

	VkLayerProperties* deviceLayers = new VkLayerProperties[layerCount];
	VK_CHECK(vkEnumerateDeviceLayerProperties(m_physicalDevice, &layerCount, deviceLayers));

	for (size_t i = 0; i < validationLayers.size(); ++i)
	{
		bool found = false;

		for (uint32_t j = 0; j < layerCount; ++j)
		{
			if (!strcmp(validationLayers[i], deviceLayers[j].layerName))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			std::cout << "Cannot find layer: " << validationLayers[i] << std::endl;
			PAUSE_AND_EXIT(-1)
		}
	}

	//Choose queue family
	uint32_t queueCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, nullptr);

	VkQueueFamilyProperties* queueFamilies = new VkQueueFamilyProperties[queueCount];
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, queueFamilies);

	for (uint32_t i = 0; i < queueCount; ++i)
	{
		VkBool32 presentSupported = VK_FALSE;
		VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &presentSupported));
																			    
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && presentSupported == VK_TRUE)
		{
			m_queueFamilyIndex = i;
		}
	}

	delete[] queueFamilies;

	if (m_queueFamilyIndex == (uint32_t)-1)
	{
		std::cout << "Cannot find queue family that supports graphics operations!" << std::endl;
		PAUSE_AND_EXIT(-1)
	}

	float priority = 1.0f;

	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &priority;

	//Create logical device
	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = 1;
	createInfo.pQueueCreateInfos = &queueCreateInfo;
	createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
	createInfo.ppEnabledLayerNames = validationLayers.data();
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();
	createInfo.pEnabledFeatures = nullptr;

	VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

	volkLoadDevice(m_device);

	vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);
}

void RenderDevice::createSwapchain(int width, int height)
{
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &m_surfaceCapabilities));

	//Choose swapchain extent
	VkExtent2D scExtent = m_surfaceCapabilities.currentExtent;
	if (m_surfaceCapabilities.currentExtent.width == UINT32_MAX || m_surfaceCapabilities.currentExtent.height == UINT32_MAX)
	{
		scExtent.width = std::max(m_surfaceCapabilities.minImageExtent.width, std::min((uint32_t)width, m_surfaceCapabilities.maxImageExtent.width));
		scExtent.height = std::max(m_surfaceCapabilities.minImageExtent.height, std::min((uint32_t)height, m_surfaceCapabilities.maxImageExtent.height));
	}

	//Choose image count
	uint32_t imageCount = m_surfaceCapabilities.minImageCount + 1;
	if (m_surfaceCapabilities.maxImageCount > 0 && imageCount > m_surfaceCapabilities.maxImageCount)
	{
		imageCount = m_surfaceCapabilities.maxImageCount;
	}

	//Create swapchain
	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = m_swapchainFormat.format;
	createInfo.imageColorSpace = m_swapchainFormat.colorSpace;
	createInfo.imageExtent = scExtent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;
	createInfo.preTransform = m_surfaceCapabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = m_presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	VK_CHECK(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain));

	m_swapchainExtent = scExtent;

	//Pull swapchain images
	uint32_t swapchainImageCount = 0;
	VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchainImageCount, nullptr));

	m_swapchainImages.resize(swapchainImageCount);
	VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchainImageCount, m_swapchainImages.data()));

	//Create swapchain image views
	m_swapchainImageViews.resize(swapchainImageCount);

	for (size_t i = 0; i < swapchainImageCount; ++i)
	{
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = m_swapchainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = m_swapchainFormat.format;
		createInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		
		VkImageView imageView;
		VK_CHECK(vkCreateImageView(m_device, &createInfo, nullptr, &imageView));

		m_swapchainImageViews[i] = imageView;
	}
}

VkCommandPool RenderDevice::createCommandPool(VkCommandPoolCreateFlags flags) const
{
	VkCommandPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = m_queueFamilyIndex;
	createInfo.flags = flags;

	VkCommandPool commandPool;
	VK_CHECK(vkCreateCommandPool(m_device, &createInfo, nullptr, &commandPool));

	return commandPool;
}

uint32_t RenderDevice::acquireNextImage(VkSemaphore semaphore) const
{
	uint32_t imageIndex = 0;
	VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &imageIndex));

	return imageIndex;
}

void RenderDevice::submit(const std::vector<VkCommandBuffer>& commandBuffers, const std::vector<std::pair<VkSemaphore, VkPipelineStageFlags>>& waitSemaphores, const std::vector<VkSemaphore> signalSemaphores, VkFence signalFence) const
{
	std::vector<VkSemaphore> wSemaphores(waitSemaphores.size());
	std::vector<VkPipelineStageFlags> wStages(waitSemaphores.size());

	for (size_t i = 0; i < waitSemaphores.size(); ++i)
	{
		wSemaphores[i] = waitSemaphores[i].first;
		wStages[i] = waitSemaphores[i].second;
	}

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
	submitInfo.pWaitSemaphores = wSemaphores.data();
	submitInfo.pWaitDstStageMask = wStages.data();
	submitInfo.commandBufferCount = (uint32_t)commandBuffers.size();
	submitInfo.pCommandBuffers = commandBuffers.data();
	submitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
	submitInfo.pSignalSemaphores = signalSemaphores.data();

	VK_CHECK(vkQueueSubmit(m_queue, 1, &submitInfo, signalFence));
}

void RenderDevice::present(uint32_t imageIndex, const std::vector<VkSemaphore>& waitSemaphores) const
{
	VkResult result = VK_SUCCESS;

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
	presentInfo.pWaitSemaphores = waitSemaphores.data();
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = &result;

	VK_CHECK(vkQueuePresentKHR(m_queue, &presentInfo));
}

void RenderDevice::destroySwapchain()
{
	for (size_t i = 0; i < m_swapchainImageViews.size(); ++i)
	{
		vkDestroyImageView(m_device, m_swapchainImageViews[i], nullptr);
	}

	m_swapchainImageViews.clear();
	m_swapchainImages.clear();

	if (m_swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	}
}

void RenderDevice::destroy()
{
	destroySwapchain();

	if (m_device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(m_device, nullptr);
	}

	if (m_surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	}

	if (m_instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(m_instance, nullptr);
	}
}